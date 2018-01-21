#include "Engine/PlatformEngine.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <cpprest/json.h>
#include <algorithm>

#include "../common/Utility.h"

#define TICKER_INTERVAL_DEFAULT 5000

using namespace std::chrono;
using namespace std;
typedef  TradingPlatform* (*CreatePlatformInstanceF)();


PlatformEngine::PlatformEngine(const char* platformName) : _runFlag(false), _hLib(nullptr), _platformName(platformName) {

	_platform = nullptr;
	_tickerInterval = TICKER_INTERVAL_DEFAULT;
	_notifyDistance = 30 * 60 * 1000;

	std::string moduleName(platformName);
	std::string settingFile(platformName);
	settingFile.append(".json");

	auto parseTrigger = [](web::json::object& value) {
		TriggerTimeBase trigger;
		trigger.startTime = value[U("startTime")].as_integer() * 1000;
		trigger.endTime = value[U("endTime")].as_integer() * 1000;
		trigger.priceChangePerMin = value[U("changedPerMin")].as_double()/100;
		return trigger;
	};

	bool loadedTriggers = false;
	try {

		ifstream in;
		in.open(settingFile);
		if (in.is_open()) {
			auto settings = web::json::value::parse(in);

			moduleName = CPPREST_FROM_STRING(settings[U("module")].as_string());
			_notifyDistance = settings[U("notifyTimeDistance")].as_integer() * 1000;
			_tickerInterval = settings[U("tickerInterval")].as_integer() * 1000;

			auto triggers = settings[U("triggers")].as_array();
			for (auto it = triggers.begin(); it != triggers.end(); it++) {
				_triggers.push_back( parseTrigger(it->as_object()));
			}

			sort(_triggers.begin(), _triggers.end(), [](TriggerTimeBase& elm1, TriggerTimeBase& elm2) {
				return elm1.endTime < elm2.endTime;
			});

			loadedTriggers = true;

			auto users = settings[U("users")].as_array();
			for (auto it = users.begin(); it != users.end(); it++) {
				UserListenerInfo userLitenserInfo;
				userLitenserInfo.applicationKey = CPPREST_FROM_STRING(it->at(U("applicationKey")).as_string());
				userLitenserInfo.userKey = CPPREST_FROM_STRING(it->at(U("userKey")).as_string());
				_userListeners.push_back(userLitenserInfo);

				UserListenerInfoRaw raw;
				raw.userInfo = &_userListeners.back();
				raw.all = false;
				auto elm = it->at(U("interest"));
				if (elm.is_array()) {
					auto& pairs = elm.as_array();

					for (auto pairIter = pairs.begin(); pairIter != pairs.end(); pairIter++) {
						raw.pairs.push_back( CPPREST_FROM_STRING(pairIter->as_string()));
					}
				}
				else if (elm.is_string() && elm.as_string() == U("all")) {
					raw.all = true;
				}

				_userRawListeners.push_back(raw);
			}
		}
	}
	catch(...) {
		cout << "Load setting file " << settingFile <<  " failed" << endl;
	}

	_hLib = LoadLibraryA(moduleName.c_str());
	if (_hLib == nullptr) {
		cout << "cannot load runtime platform" << endl;
	}
	else {
		CreatePlatformInstanceF createFunc = (CreatePlatformInstanceF)GetProcAddress(_hLib, "createPlatformInstance");
		if (createFunc != nullptr) {
			_platform = createFunc();
		}
	}

	_tickerAnalyzer = std::bind(&PlatformEngine::tickerAnalyze, this, std::placeholders::_1);

	if (!loadedTriggers) {
		_triggers = {
			{ 50 * 1000, 1800 * 1000, 0.303f/100 }, // change atleast 10% in 30 minutes
			{ 1780 * 1000, 28800 * 1000, 0.04167f / 100 }, // change atleast 20% in 8 hours
		};
	}
}

PlatformEngine::~PlatformEngine() {
	if (_platform) {
		_platform->disconnect();
		delete _platform;
	}

	if (_hLib) {
		FreeLibrary(_hLib);
	}
}

void PlatformEngine::timeInterval() {
	int nHandler = _platform->getHandlerCount();
	vector<NAPMarketEventHandler*> handlers(nHandler);
	_platform->getHandlers((MarketEventHandler**)handlers.data(), nHandler);

	TIMESTAMP t1, t2;
	TIMESTAMP processTime;
	while (_runFlag) {
		t1 = getCurrentTimeStamp();
		if (_platform->isServerTimeReady()) {
			_platform->broadcastServerTime(_platform->getSyncTime(getCurrentTimeStamp()));
		}
		measurePriceIncrement(handlers);

		t2 = getCurrentTimeStamp();
		processTime = (t2 - t1);
		//cout << "broadcast local ticker event(" << processTime.count() << " ms)" << endl;
		if (!_runFlag) {
			break;
		}

		if (processTime < _tickerInterval) {
			std::this_thread::sleep_for(duration<TIMESTAMP, std::milli>(_tickerInterval - processTime));
		}
	}
}

void PlatformEngine::pushMessageLoop() {
	InternalNotificationData notification;
	Notifier notifier;
	while (_runFlag) {
		_messageQueue.popMessage(notification);
		if (_runFlag) {
			auto it = _pairListenerMap.find(notification.pair);
			if (it != _pairListenerMap.end()) {
				auto& users = it->second;
				auto& messageInfo = notification.message;
				for (auto uit = users->begin(); uit != users->end() && _runFlag; uit++) {
					messageInfo.target = *uit;
					for (int tryCount = 1; tryCount <= 2; tryCount++) {
						// Wait for all the outstanding I/O to complete and handle any exceptions
						try
						{
							if (notifier.pushNotification(messageInfo)) {
								break;
							}
						}
						catch (const std::exception &e)
						{
							pushLog("push message failed:%s\n", e.what());
						}
						catch (...) {
							pushLog("push message failed: unknow error\n");
						}
						
						pushLog("sending message again(%d)\n", tryCount);
					}
				}
			}
		}
	}
}

void PlatformEngine::updateSymbolStatistic(CryptoBoardElmInfo* info, NAPMarketEventHandler* sender, TradeItem* tradeItem) {
	info->price = tradeItem->price;
	info->volume = fabs(tradeItem->amount);

	auto& tickers = sender->getTickerHistoriesNonSync();

	static TIMESTAMP pricePeriods[] = {
		30 * 60 * 1000,
		1 * 60 * 60 * 1000,
		2 * 60 * 60 * 1000,
		3 * 60 * 60 * 1000,
		4 * 60 * 60 * 1000,
	};

	static TIMESTAMP volPeriods[] = {
		30 * 60 * 1000,
		1 * 60 * 60 * 1000,
		2 * 60 * 60 * 1000,
		3 * 60 * 60 * 1000,
		4 * 60 * 60 * 1000,
	};

	auto it = tickers.begin();
	for (; it != tickers.end(); it++) {
		if (it->firstPrice.price >= 0) {
			break;
		}
	}
	if (it == tickers.end()) return;

	auto jt = it;

	auto& ticker = *it;
	TIMESTAMP totalTime;
	double sumPrice;
	double* pValue = &info->pricePeriod1;

	PricePoint* prevPricePoint = nullptr;
	TIMESTAMP serverTimeNow = _platform->getSyncTime(getCurrentTimeStamp());

	TIMESTAMP prevPeriod = 0;
	for (auto& period : pricePeriods) {
		totalTime = 0;
		sumPrice = 0;
		for (; it != tickers.end(); it++) {
			if (it->lastPrice.price < 0 || (it->boughtVolume <= 0 && it->soldVolume <= 0)) {
				continue;
			}

			auto duration = serverTimeNow - it->firstPrice.at;
			if (duration > period) {
				break;
			}

			duration = it->lastPrice.at - it->firstPrice.at;
			if (duration < 0) {
				pushLog("time synchronizing error %lld\n", duration);
			}
			sumPrice += it->averagePrice * duration;
			totalTime += (it->lastPrice.at - it->firstPrice.at);

			prevPricePoint = &it->lastPrice;
		}

		if (totalTime) {
			*pValue = sumPrice / totalTime;
		}
		pValue++;
	}

	auto pVolumePeriod = &info->volPeriod1;

	it = jt;
	double sumVol;
	for (auto& period : volPeriods) {
		sumVol = 0;
		for (; it != tickers.end(); it++) {
			if (it->lastPrice.price < 0 || (it->boughtVolume <= 0 && it->soldVolume <= 0)) {
				continue;
			}
			auto duration = serverTimeNow - it->firstPrice.at;
			if (duration > period) {
				break;
			}

			pVolumePeriod->bought += it->boughtVolume;
			pVolumePeriod->sold += it->soldVolume;
		}
		pVolumePeriod++;
	}
}

void PlatformEngine::run() {
	if (_platform == nullptr) return;
	_runFlag = true;

	bool needGetAll = false;
	for (auto it = _userRawListeners.begin(); it != _userRawListeners.end(); it++) {
		if (it->all) {
			needGetAll = true;
			break;
		}
		auto& pairs = it->pairs;
		for (auto jt = pairs.begin(); jt != pairs.end(); jt++) {
			_pairListenerMap[*jt] = nullptr;
		}
	}
	if (needGetAll) {
		_pairListenerMap.clear();
		// get symbols and regist event handler for each symbol
		try {
			auto pairs = _platform->getAllPairs();
			if (pairs) {
				auto c = pairs;
				while (*c)
				{
					auto s = c;
					c = strchr(c, ';');
					if (c) {
						*c = 0;
					}

					_pairListenerMap[s] = nullptr;

					if (c == nullptr) { break; }
					c++;
				}
				//_pairListenerMap["ETHBTC"] = nullptr;
				::free(pairs);
			}
		}
		catch (std::exception& e) {
			cout << e.what() << endl;
			return;
		}
		catch (...) {
			cout << "Unknow error" << endl;
			return;
		}
	}

	if (_pairListenerMap.size() == 0) {
		cout << "no symbol defined" << endl;
		return;
	}

	// initialize lists
	for (auto it = _pairListenerMap.begin(); it != _pairListenerMap.end(); it++) {
		it->second = make_shared<list<UserListenerInfo*>>();
	}

	for (auto jt = _userRawListeners.begin(); jt != _userRawListeners.end(); jt++) {
		if (jt->all) {
			for (auto it = _pairListenerMap.begin(); it != _pairListenerMap.end(); it++) {
				it->second->push_back(jt->userInfo);
			}
		}
		else {
			auto& pairs = jt->pairs;
			for (auto kt = pairs.begin(); kt != pairs.end(); kt++) {
				auto it = _pairListenerMap.find(*kt);
				if (it != _pairListenerMap.end()) {
					it->second->push_back(jt->userInfo);
				}
			}
		}
	}

	_symbolsStatistics.resize(_pairListenerMap.size());
	CryptoBoardElmInfo* pSymbolStatisticsinfo = _symbolsStatistics.data();
	for (auto it = _pairListenerMap.begin(); it != _pairListenerMap.end(); it++) {
		NAPMarketEventHandler* eventHandler = new NAPMarketEventHandler(_tickerInterval, it->first.c_str());
		eventHandler->useTicker(false);
		eventHandler->useOrderBook(false);
		eventHandler->useTrades(true);
		eventHandler->useCandles(false);
		_platform->addEventHandler(eventHandler, true);

		pSymbolStatisticsinfo->symbol = it->first;

		auto eventListener = std::bind(&PlatformEngine::updateSymbolStatistic, this, pSymbolStatisticsinfo, std::placeholders::_1 , std::placeholders::_2);
		eventHandler->addTradeEventListener(eventListener);

		pSymbolStatisticsinfo++;
	}

	// start interval thread
	_broadCastIntervalTask = std::async(std::launch::async, [this]() {timeInterval(); });
	_messageLoopTask = std::async(std::launch::async, [this]() {pushMessageLoop(); });

	// starting query time from server
	_platform->startServerTimeQuery(5000);

	// connect to the platform
	try {
		_platform->connect();
	}
	catch (exception&e) {
		cout << "error:" << e.what() << endl;
		return;
	}
}

void PlatformEngine::stop() {
	cout << "exiting..." << endl;
	_runFlag = false;	
	if (_broadCastIntervalTask.valid()) {
		_broadCastIntervalTask.wait();
	}
	if (_messageLoopTask.valid()) {
		// push an empty message
		_messageQueue.pushMessage({});
		_messageLoopTask.wait();
	}
}

void formatPriceChanged(char* buffer, size_t bufferSize, const char* pair, const PricePoint& lastPrice, const PricePoint& basePrice) {

	double priceChanged = lastPrice.price - basePrice.price;
	TIMESTAMP duration = lastPrice.at - basePrice.at;
	const char* movementStr = "jumped";
	if (priceChanged < 0) {
		movementStr = "dropped";
	}

	auto priceChangedPercent = priceChanged / basePrice.price * 100;

	sprintf_s(buffer, bufferSize, "%s %s %.2f%% in %.2f min from %lf to %lf", pair, movementStr,
		(float)priceChangedPercent, (duration / (60 * 1000.0f)), basePrice.price, lastPrice.price);
}

bool PlatformEngine::processTickerLevel(const char* pair, TIMESTAMP timeBase, std::list<SimpleTicker>::iterator it, std::list<SimpleTicker>::iterator& end, char level) {
	auto& trigger = _triggers[level];

	auto& lastTicker = *it;
	auto& lastPricePoint = lastTicker.lastPrice;
	TIMESTAMP duration;
	for (;it != end; end--)
	{
		auto& ticker = *end;
		duration = timeBase - ticker.firstPrice.at;
		if (duration <= trigger.endTime) {
			break;
		}
	}

	if (lastTicker.processLevel == level) {
		return false;
	}

	double priceChanged;
	decltype(lastTicker.lastPrice)* pricePoint;

	auto updatePriceChangedVariables = [&lastPricePoint, &priceChanged, &pricePoint](SimpleTicker& ticker) {
		auto priceChanged1 = (lastPricePoint.price - ticker.low.price) / ticker.low.price;
		auto priceChanged2 = (lastPricePoint.price - ticker.high.price) / ticker.high.price;
		if (abs(priceChanged1) > abs(priceChanged2)) {
			priceChanged = priceChanged1;
			pricePoint = &ticker.low;
		}
		else {
			priceChanged = priceChanged2;
			pricePoint = &ticker.high;
		}
	};

	decltype(lastTicker.lastPrice)* bestPricePoint = nullptr;
	double bestPriceChanged = 0;
	for (; it != end; it++) {
		auto& ticker = *it;
		if (ticker.lastPrice.price < 0 || (ticker.boughtVolume <= 0 && ticker.soldVolume <= 0)) {
			continue;
		}

		// prevent process for a same processed level
		if (ticker.processLevel == level) {
			bestPricePoint = nullptr;
			break;
		}
		duration = timeBase - ticker.firstPrice.at;
		// don't send notify again if the time distance is too short
		if (ticker.processLevel >= 0 && duration < _notifyDistance) {
			bestPricePoint = nullptr;
			break;
		}

		updatePriceChangedVariables(ticker);

		if (bestPricePoint == nullptr || abs(bestPriceChanged) < abs(priceChanged)) {
			bestPriceChanged = priceChanged;
			bestPricePoint = pricePoint;
		}
		else if (abs(bestPriceChanged) == abs(priceChanged) && bestPricePoint->at < pricePoint->at) {
			bestPriceChanged = priceChanged;
			bestPricePoint = pricePoint;
		}
	}

	char buffer[256];
	if (bestPricePoint) {
		duration = timeBase - bestPricePoint->at;
		if (duration >= trigger.startTime) {
			auto priceChangePerMin = bestPriceChanged * 60 * 1000 / duration;
			if (abs(priceChangePerMin) >= trigger.priceChangePerMin) {
				// mark the ticker was process for this level
				lastTicker.processLevel = level;
				//if (_degbugMap.find(pair) != _degbugMap.end()) {
				//	priceChanged = 0;
				//}

				formatPriceChanged(buffer, sizeof(buffer), pair, lastPricePoint, *bestPricePoint);

				InternalNotificationData notification;
				notification.message.title = _platformName;
				notification.message.message = buffer;
				notification.pair = pair;
				_messageQueue.pushMessage(notification);

				_degbugMap[pair] = true;

				return true;
			}
		}
	}

	return false;
}

void PlatformEngine::tickerAnalyze(NAPMarketEventHandler* eventHandler) {
	auto& tickers = eventHandler->getTickerHistoriesNonSync();

	auto it = tickers.begin();
	for (; it != tickers.end(); it++) {
		if (it->lastPrice.price > 0 && (it->boughtVolume > 0 || it->soldVolume > 0)) {
			break;
		}
	}
	if (it == tickers.end()) {
		return;
	}

	auto end = tickers.end();
	end--;

	high_resolution_clock::time_point t = high_resolution_clock::now();
	TIMESTAMP serverTimeNow = _platform->getSyncTime(t.time_since_epoch().count());

	for (char level = (char)(_triggers.size() - 1); level >= 0; level--) {
		if (processTickerLevel(eventHandler->getPair(), serverTimeNow, it, end, level)) {
			break;
		}
	}
}

void PlatformEngine::measurePriceIncrement(const std::vector<NAPMarketEventHandler*>& handlers) {
	for (auto it = handlers.begin(); it != handlers.end() && _runFlag; it++) {
		(*it)->accessSharedData(_tickerAnalyzer);
	}
}

const std::vector<CryptoBoardElmInfo>& PlatformEngine::getSymbolsStatistics() const {
	return _symbolsStatistics;
}

TradingPlatform* PlatformEngine::getPlatform() {
	return _platform;
}