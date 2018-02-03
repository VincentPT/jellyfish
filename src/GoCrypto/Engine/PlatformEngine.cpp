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
		//if (_platform->isServerTimeReady()) {
		//	_platform->broadcastServerTime(_platform->getSyncTime(getCurrentTimeStamp()));
		//}
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
		if (_messageQueue.popMessage(notification, 1000) && _runFlag) {
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

void PlatformEngine::sheduleQueryHistory() {
	RequestTradeHistoryMessage message;
	while (_runFlag)
	{
		if (_symbolQueue.popMessage(message, 1000) && _runFlag) {
			auto symbol = message.pair.c_str();
			auto hander = _platform->getHandler(symbol);
			if (hander) {
				TradingList tradeItems;

				pushLog("requesting trade snapshot for %s\n", symbol);
				_platform->getTradeHistory(symbol, message.duration, message.endTime, tradeItems);
				vector<TradeItem> tradeRawItems(tradeItems.size());
				TradeItem* pItem = tradeRawItems.data();
				for (auto iter = tradeItems.head(); iter; iter = iter->nextNode) {
					*pItem++ = iter->value;
				}

				pushLog("received trade snapshot for %s\n", symbol);
				hander->onTradesUpdate(tradeRawItems.data(), (int)tradeRawItems.size(), true);
			}
		}
	}
}

void PlatformEngine::updateSymbolStatistic(CryptoBoardElmInfo* info, NAPMarketEventHandler* sender, TradeItem* tradeItem, int, bool) {
	info->price = tradeItem->price;
	info->volume = fabs(tradeItem->amount);

	auto& trades = sender->getTradeHistoriesNonSync();
	if (_sentTradeSnapshotRequest.find(sender->getPair()) == _sentTradeSnapshotRequest.end()) {
		TIMESTAMP tEnd = 0;
		TIMESTAMP duration = 4 * 3600 * 1000;
		if (trades.size()) {
			tEnd = trades.back().timestamp;

			duration -= trades.front().timestamp - tEnd;
		}
		RequestTradeHistoryMessage message;
		message.pair = sender->getPair();
		message.duration = duration;
		message.endTime = tEnd;

		_sentTradeSnapshotRequest[sender->getPair()] = true;
		_symbolQueue.pushMessage(message);

		//pushLog("send request trade snapshot for %s\n", sender->getPair());
	}


	static TIMESTAMP pricePeriods[] = {
		1 * 60 * 60 * 1000,
		2 * 60 * 60 * 1000,
		3 * 60 * 60 * 1000,
		4 * 60 * 60 * 1000,
	};

	static TIMESTAMP volPeriods[] = {
		1 * 60 * 60 * 1000,
		2 * 60 * 60 * 1000,
		3 * 60 * 60 * 1000,
		4 * 60 * 60 * 1000,
	};

	auto it = trades.begin();
	auto jt = it;

	auto& ticker = *it;
	double amount, amountABS;
	double cost;
	double* pValue = &info->pricePeriod1;

	TIMESTAMP serverTimeNow = _platform->getSyncTime(getCurrentTimeStamp());

	TIMESTAMP prevPeriod = 0;
	for (auto& period : pricePeriods) {
		amount = 0;
		cost = 0;
		for (; it != trades.end(); it++) {
			auto duration = serverTimeNow - it->timestamp;
			if (duration > period) {
				break;
			}
			amountABS = abs(it->amount);

			cost += it->price * amountABS;
			amount += amountABS;
		}

		if (amount) {
			*pValue = cost / amount;
		}
		pValue++;
	}

	auto pVolumePeriod = &info->volPeriod1;

	it = jt;
	for (auto& period : volPeriods) {
		amount = 0;
		for (; it != trades.end(); it++) {
			auto duration = serverTimeNow - it->timestamp;
			if (duration > period) {
				break;
			}
			if (it->amount > 0) {
				pVolumePeriod->bought += it->amount;
			}
			else if (it->amount < 0) {
				pVolumePeriod->sold -= it->amount;
			}
		}
		pVolumePeriod++;
	}
}

void PlatformEngine::run() {
	using namespace std::placeholders;
	if (_platform == nullptr) return;

	LOG_SCOPE_ACCESS(_platform->getLogger(), __FUNCTION__);

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

		auto eventListener = 
			std::bind(&PlatformEngine::updateSymbolStatistic, this, pSymbolStatisticsinfo, _1 , _2, _3, _4);
		eventHandler->addTradeEventListener(eventListener);

		pSymbolStatisticsinfo++;
	}

	// start interval thread
	_broadCastIntervalTask = std::async(std::launch::async, [this]() {timeInterval(); });
	_messageLoopTask = std::async(std::launch::async, [this]() {pushMessageLoop(); });
	_sendTradeHistoryRequestLoop = std::async(std::launch::async, [this]() {sheduleQueryHistory(); });

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
	LOG_SCOPE_ACCESS(_platform->getLogger(), __FUNCTION__);

	_runFlag = false;

	_platform->disconnect();

	pushLog("stoping interval task...\n");
	if (_broadCastIntervalTask.valid()) {
		_broadCastIntervalTask.wait();
	}
	pushLog("stopped!\n");
	pushLog("stoping message loop task...\n");
	if (_messageLoopTask.valid()) {
		// push an empty message
		_messageQueue.pushMessage({});
		_messageLoopTask.wait();
	}
	pushLog("stopped!\n");
	pushLog("stop querying trade history...\n");
	if (_sendTradeHistoryRequestLoop.valid()) {
		_sendTradeHistoryRequestLoop.wait();
	}
	pushLog("stopped!\n");
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

#define PROCESS_LEVEL(trade) ((levelIt = tradeLevelMap->find((trade).oderId)) != tradeLevelMap->end() ? levelIt->second : -1)
#define SET_PROCESS_LEVEL(trade, level) tradeLevelMap->operator[]((trade).oderId) = (level)

bool PlatformEngine::processTradesLevel(const char* pair, TIMESTAMP timeBase, std::list<TradeItem>::iterator it, std::list<TradeItem>::iterator& end, char level) {
	auto& trigger = _triggers[level];

	TradeLevelMap* tradeLevelMap;
	TradeLevelMap::iterator levelIt;
	auto levelMapIt = _processLevelMap.find(pair);
	if (levelMapIt == _processLevelMap.end()) {
		auto emptyLevelMapRef = make_shared<TradeLevelMap>();
		_processLevelMap[pair] = emptyLevelMapRef;
		tradeLevelMap = emptyLevelMapRef.get();
	}
	else {
		tradeLevelMap = levelMapIt->second.get();
	}

	auto& lastTrade = *it;
	TIMESTAMP duration;
	for (;it != end; end--)
	{
		duration = timeBase - end->timestamp;
		if (duration <= trigger.endTime) {
			break;
		}
	}

	if (PROCESS_LEVEL(lastTrade) == level) {
		return false;
	}

	double priceChanged;
	TradeItem* bestTradePoint = nullptr;
	double bestPriceChanged = 0;
	for (; it != end; it++) {
		auto& trade = *it;
		
		// prevent process for a same processed level
		if (PROCESS_LEVEL(trade) == level) {
			bestTradePoint = nullptr;
			break;
		}
		duration = timeBase - trade.timestamp;
		// don't send notify again if the time distance is too short
		if ( PROCESS_LEVEL(trade) >= 0 && duration < _notifyDistance) {
			bestTradePoint = nullptr;
			break;
		}

		priceChanged = (lastTrade.price - trade.price) / trade.price;

		if (bestTradePoint == nullptr || abs(bestPriceChanged) < abs(priceChanged)) {
			bestPriceChanged = priceChanged;
			bestTradePoint = &trade;
		}
		else if (abs(bestPriceChanged) == abs(priceChanged) && bestTradePoint->timestamp < trade.timestamp) {
			bestPriceChanged = priceChanged;
			bestTradePoint = &trade;
		}
	}

	char buffer[256];
	if (bestTradePoint) {
		duration = timeBase - bestTradePoint->timestamp;
		if (duration >= trigger.startTime) {
			auto priceChangePerMin = bestPriceChanged * 60 * 1000 / duration;
			if (abs(priceChangePerMin) >= trigger.priceChangePerMin) {
				// mark the trade was process for this level
				SET_PROCESS_LEVEL(lastTrade, level);
				//if (_degbugMap.find(pair) != _degbugMap.end()) {
				//	priceChanged = 0;
				//}

				PricePoint lastPricePoint;
				PricePoint bestPricePoint;

				lastPricePoint.price = lastTrade.price;
				lastPricePoint.at = lastTrade.timestamp;
				bestPricePoint.price = bestTradePoint->price;
				bestPricePoint.at = bestTradePoint->timestamp;

				formatPriceChanged(buffer, sizeof(buffer), pair, lastPricePoint, bestPricePoint);

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
	auto& trades = eventHandler->getTradeHistoriesNonSync();
	if (trades.size() == 0) return;

	auto it = trades.begin();
	auto end = trades.end();
	end--;

	TIMESTAMP serverTimeNow = _platform->getSyncTime(getCurrentTimeStamp());

	for (char level = (char)(_triggers.size() - 1); level >= 0; level--) {
		if (processTradesLevel(eventHandler->getPair(), serverTimeNow, it, end, level)) {
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