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
			if (settings.has_field(U("currencies"))) {
				auto currenciesVal = settings[U("currencies")];
				if (currenciesVal.is_array()) {
					auto& currencies = currenciesVal.as_array();
					for (auto cit = currencies.begin(); cit != currencies.end(); cit++) {
					   _currencies.push_back(CPPREST_FROM_STRING(cit->as_string()));
					}
				}
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
			if (_platform) {
				_platform->setConfigFilePath(settingFile.c_str());
			}
		}
	}

	//_tickerAnalyzer = std::bind(&PlatformEngine::tickerAnalyze, this, std::placeholders::_1);

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

				pushLog("requesting trade history for %s\n", symbol);
				_platform->getTradeHistory(symbol, message.duration, message.endTime, tradeItems);
				vector<TradeItem> tradeRawItems(tradeItems.size());
				TradeItem* pItem = tradeRawItems.data();
				for (auto iter = tradeItems.head(); iter; iter = iter->nextNode) {
					*pItem++ = iter->value;
				}

				pushLog("received trade history for %s\n", symbol);
				hander->onTradesUpdate(tradeRawItems.data(), (int)tradeRawItems.size(), true);
			}
		}
	}
}

template <class Iter>
inline void updateTicker(Iter item, double& cost, double& sold, double& bought, PricePoint& price, PricePoint& low, PricePoint& high) {
	price.at = item->timestamp;
	price.price = item->price;

	if (high.price < price.price) {
		high = price;
	}
	if (low.price > price.price) {
		low = price;
	}

	if (item->amount < 0) {
		sold += -item->amount;
		cost -= price.price * item->amount;
	}
	else {
		bought += item->amount;
		cost += price.price * item->amount;
	}
}

template <class Iter, class NextIterFunc>
void buildFromEmptyTickers(Iter begin, Iter end, const NextIterFunc& nextIter, list<TickerUI>& tickers) {
	auto it = begin;
	TIMESTAMP baseTime = (it->timestamp / TICKER_DURATION + 1) * TICKER_DURATION;
	TickerUI* pLastTicker = nullptr;
	// build a new tickers
	for (auto it = begin; it != end;) {
		TickerUI ticker;
		// check if the current trade is belong to current time frame
		if (it->timestamp < baseTime) {
			ticker.firstPrice.at = it->timestamp;
			ticker.firstPrice.price = it->price;

			auto& lastPrice = ticker.lastPrice;
			auto& high = ticker.high;
			auto& low = ticker.low;

			// initialze last price, low price, high price as fisrt trade in current time frame
			lastPrice = ticker.firstPrice;
			high = lastPrice;
			low = lastPrice;

			// set bought or sold value
			if (it->amount < 0) {
				ticker.boughtVolume = 0;
				ticker.soldVolume = -it->amount;
			}
			else {
				ticker.boughtVolume = it->amount;
				ticker.soldVolume = 0;
			}
			// default the ticker is not processed
			ticker.processLevel = -1;

			// update first price, low price, high price base on the next trade events
			double cost = lastPrice.price * (ticker.boughtVolume + ticker.soldVolume);
			for (nextIter(it); it != end && it->timestamp < baseTime; nextIter(it)) {
				updateTicker(it, cost, ticker.soldVolume, ticker.boughtVolume, lastPrice, low, high);
			}

			ticker.averagePrice = cost / (ticker.boughtVolume + ticker.soldVolume);
		}
		else {
			ticker.averagePrice = -1;
			ticker.processLevel = -1;
			ticker.boughtVolume = 0;
			ticker.soldVolume = 0;

			ticker.firstPrice.at = baseTime;
			ticker.firstPrice.price = pLastTicker->lastPrice.price;

			ticker.lastPrice.at = ticker.firstPrice.at + TICKER_DURATION - 1;
			ticker.lastPrice.price = ticker.firstPrice.price;

			ticker.high = ticker.firstPrice;
			ticker.low = ticker.firstPrice;
		}
		tickers.push_front(ticker);
		pLastTicker = &tickers.front();
		baseTime += TICKER_DURATION;
	}
}

void PlatformEngine::updateSymbolStatistics(CryptoBoardElmInfo* info, const std::list<TickerUI>& tickers) {
	static TIMESTAMP pricePeriods[] = {
		1 * 60 * 60 * 1000,
		2 * 60 * 60 * 1000,
		3 * 60 * 60 * 1000,
		4 * 60 * 60 * 1000,
	};

	static TIMESTAMP volPeriods[] = {
		1 * 60 * 1000,
		1 * 60 * 60 * 1000,
		2 * 60 * 60 * 1000,
		3 * 60 * 60 * 1000,
		4 * 60 * 60 * 1000,
	};

	double amount, amountABS;
	double cost;
	double* pValue = info->pricePeriods;

	TIMESTAMP serverTimeNow = _platform->getSyncTime(getCurrentTimeStamp());

	auto it = tickers.begin();
	for (auto& period : pricePeriods) {
		amount = 0;
		cost = 0;
		for (; it != tickers.end(); it++) {
			auto duration = serverTimeNow - it->firstPrice.at;
			if (duration > period) {
				break;
			}
			amountABS = (it->boughtVolume + it->soldVolume);
			cost += it->averagePrice * amountABS;
			amount += amountABS;
		}

		if (amount) {
			*pValue = cost / amount;
		}
		pValue++;
	}

	auto pVolumePeriod = info->volPeriods;

	it = tickers.begin();
	for (auto& period : volPeriods) {
		pVolumePeriod->bought = 0;
		pVolumePeriod->sold = 0;
		for (; it != tickers.end(); it++) {
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

void PlatformEngine::onTrade(int i, NAPMarketEventHandler* sender, TradeItem* incommingTrades, int count, bool snapshot) {
	auto& trades = sender->getTradeHistoriesNonSync();

	if (incommingTrades == nullptr || count == 0) return;

	auto& tickers = _symbolsTickers[i];

	auto pTradeItemStart = incommingTrades + count - 1;
	auto pTradeItemEnd = incommingTrades - count;

	// check if it is a snapshot event or update event not the lastest item in the trade history list
	if (snapshot || incommingTrades->oderId != trades.front().oderId) {
		// build tickers for all trade items of curent pair on snapshot event
		tickers.clear();
		if (trades.size()) {
			auto& lastTrade = trades.front();
			TIMESTAMP baseTime = lastTrade.timestamp - lastTrade.timestamp % TICKER_DURATION;

			typedef decltype(trades.rbegin()) ITERATOR;
			auto nextIter = [](ITERATOR& it) { it++; };

			buildFromEmptyTickers(trades.rbegin(), trades.rend(), nextIter, tickers);
		}
		//pushLog("built ticker as snapshot event\n");
	}
	else if(incommingTrades && count > 0) {
		// update tickers for the last trade events
		if (tickers.size() == 0) {

			typedef decltype(incommingTrades) ITERATOR;
			auto nextIter = [](ITERATOR& it) { it--; };

			buildFromEmptyTickers(pTradeItemStart, pTradeItemEnd, nextIter, tickers);
		}
		else {
			auto pTicker = &tickers.front();
			auto timeRangeBase = pTicker->firstPrice.at - pTicker->firstPrice.at % TICKER_DURATION;

			// check if the new trade event is belong to last exist time frame
			if (timeRangeBase < pTradeItemStart->timestamp && incommingTrades->timestamp < timeRangeBase + TICKER_DURATION) {
				auto& lastPrice = pTicker->lastPrice;
				auto& high = pTicker->high;
				auto& low = pTicker->low;

				auto cost = pTicker->averagePrice * (pTicker->soldVolume + pTicker->boughtVolume);
				for (auto it = pTradeItemStart; it != pTradeItemEnd; it--) {
					updateTicker(it, cost, pTicker->soldVolume, pTicker->boughtVolume, lastPrice, low, high);
				}

				pTicker->averagePrice = cost / (pTicker->soldVolume + pTicker->boughtVolume);

				//pushLog("updated the last ticker\n");
			}
			else {
				typedef decltype(incommingTrades) ITERATOR;
				auto nextIter = [](ITERATOR& it) { it--; };

				list<TickerUI> newTickers;
				buildFromEmptyTickers(pTradeItemStart, pTradeItemEnd, nextIter, newTickers);
				if (newTickers.size()) {

					auto& oldLastTicker = tickers.front();
					auto newOldestTicker = newTickers.back();

					TIMESTAMP oldBaseTime = oldLastTicker.firstPrice.at - oldLastTicker.firstPrice.at % TICKER_DURATION;
					TIMESTAMP newBaseTime = newOldestTicker.firstPrice.at - newOldestTicker.firstPrice.at % TICKER_DURATION;
					if (oldBaseTime == newBaseTime) {
						// merge ticker
						// fisrt pop the last ticker of new tickers
						newTickers.pop_back();

						// merge volumes and average price
						auto oldCost = oldLastTicker.averagePrice * (oldLastTicker.boughtVolume + oldLastTicker.soldVolume);
						auto newCost = newOldestTicker.averagePrice * (newOldestTicker.boughtVolume + newOldestTicker.soldVolume);

						oldLastTicker.boughtVolume += newOldestTicker.boughtVolume;
						oldLastTicker.soldVolume += newOldestTicker.soldVolume;
						oldLastTicker.averagePrice = (oldCost + newCost) / (oldLastTicker.boughtVolume + oldLastTicker.soldVolume);

						// re-calculate high, low
						if (oldLastTicker.high.price < newOldestTicker.high.price) {
							oldLastTicker.high = newOldestTicker.high;
						}
						if (oldLastTicker.low.price > newOldestTicker.low.price) {
							oldLastTicker.low = newOldestTicker.low;
						}

						// update last price
						oldLastTicker.lastPrice = newOldestTicker.lastPrice;

						// move other left tickers from the new to the existing list
						for (auto it = newTickers.rbegin(); it != newTickers.rend(); it++) {
							tickers.push_front(*it);
						}

						//pushLog("merged tickers \n");
					}
					else if (newBaseTime > oldBaseTime) {
						// add empty tickers until reach the new time frame
						auto pLastTicker = &tickers.front();
						TIMESTAMP baseTime = oldBaseTime + TICKER_DURATION;
						while (baseTime < newBaseTime) {
							TickerUI ticker;
							ticker.averagePrice = pLastTicker->lastPrice.price;
							ticker.processLevel = -1;
							ticker.boughtVolume = 0;
							ticker.soldVolume = 0;

							ticker.firstPrice.at = baseTime;
							ticker.firstPrice.price = ticker.averagePrice;

							ticker.lastPrice.at = ticker.firstPrice.at + TICKER_DURATION - 1;
							ticker.lastPrice.price = ticker.averagePrice;

							ticker.high = ticker.firstPrice;
							ticker.low = ticker.firstPrice;

							tickers.push_front(ticker);

							baseTime += TICKER_DURATION;
						}

						// move other left tickers from the new to the existing list
						for (auto it = newTickers.rbegin(); it != newTickers.rend(); it++) {
							tickers.push_front(*it);
						}

						//pushLog("add tickers\n");
					}
					else {
						pushLog("something wrong, the update trade event is not newest event\n");
					}
				}
			}
		}
	}

	CryptoBoardElmInfo* info = &_symbolsStatistics[i];
	info->price = trades.front().price;
	info->volume = fabs(trades.front().amount);
	// only request snapshot for trade when first trade history return
	// to ensure the trade historys in local is fully and continous
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
	}
	
	//analyze tickers for notification
	analyzeTickerForNotification(sender, tickers);

	// update satistics info for the symbol
	updateSymbolStatistics(info, tickers);
	if (_onSymbolStatisticUpdated) {
		_onSymbolStatisticUpdated(i);
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
			StringList pairs;
			_platform->getAllPairs(pairs);
			for (auto it = pairs.head(); it; it = it->nextNode)
			{
				auto s = it->value;
				_pairListenerMap[s] = nullptr;
				free(s);
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

	_symbolsStatistics.clear();
	_symbolsTickers.clear();

	_symbolsStatistics.resize(_pairListenerMap.size());
	_symbolsTickers.resize(_symbolsStatistics.size());
	int i = 0;
	CryptoBoardElmInfo* pSymbolStatisticsinfo = _symbolsStatistics.data();
	for (auto it = _pairListenerMap.begin(); it != _pairListenerMap.end(); it++, i++) {
		NAPMarketEventHandler* eventHandler = new NAPMarketEventHandler(it->first.c_str());
		eventHandler->useTicker(false);
		eventHandler->useOrderBook(false);
		eventHandler->useTrades(true);
		eventHandler->useCandles(false);
		_platform->addEventHandler(eventHandler, true);

		_symbolsStatistics[i].symbol = it->first;

		auto eventListener = 
			std::bind(&PlatformEngine::onTrade, this, i, _1 , _2, _3, _4);
		eventHandler->addTradeEventListener(eventListener);
	}

	// start interval thread
	//_broadCastIntervalTask = std::async(std::launch::async, [this]() {timeInterval(); });
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

	//pushLog("stoping interval task...\n");
	//if (_broadCastIntervalTask.valid()) {
	//	_broadCastIntervalTask.wait();
	//}
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

inline bool updateBestPricePoint(const PricePoint*& bestPricePoint, const PricePoint& basePrice, const PricePoint* checkPrice) {
	auto priceChanged = basePrice.price - checkPrice->price;

	if (bestPricePoint == nullptr || abs(basePrice.price - bestPricePoint->price) < abs(priceChanged)) {
		bestPricePoint = checkPrice;
		return true;
	}
	return false;
}

void PlatformEngine::analyzeTickerForNotification(NAPMarketEventHandler* handler, const std::list<TickerUI>& tickers) {
	auto it = tickers.begin();

	auto& lastTicker = *it;
	auto& lastPrice = lastTicker.lastPrice;
	auto priceMoveUp = lastPrice.price - lastTicker.low.price;
	auto priceMoveDown = lastTicker.high.price - lastPrice.price;

	TIMESTAMP duration;
	double priceChanged;
	const PricePoint* bestPricePoint;
	const TickerUI* theTicker;
	auto iter = it;
	char buffer[256];

	for (char level = 0; level < (char)_triggers.size(); level++) {
		auto& trigger = _triggers[level];
		bestPricePoint = nullptr;
		for (auto jt = it; jt != tickers.end(); jt++) {
			auto& ticker = *jt;

			if (ticker.processLevel >= 0) {
				bestPricePoint = nullptr;
				break;
			}

			duration = lastPrice.at - ticker.low.at;
			if (trigger.startTime <= duration && duration < trigger.endTime) {
				if (updateBestPricePoint(bestPricePoint, lastPrice, &(ticker.low))) {
					theTicker = &ticker;
					iter = jt;
				}
			}

			duration = lastPrice.at - ticker.high.at;
			if (trigger.startTime <= duration && duration < trigger.endTime) {
				if (updateBestPricePoint(bestPricePoint, lastPrice, &(ticker.high))) {
					theTicker = &ticker;
					iter = jt;
				}
			}
		}

		if (bestPricePoint) {
			priceChanged = (lastPrice.price - bestPricePoint->price);
			duration = lastPrice.at - bestPricePoint->at;
			// check if the largest change in the period has 

			TIMESTAMP checkRangeEnd = lastPrice.at - duration * 10 / 100;
			auto jt = it;
			for (; jt != iter; jt++) {
				if (jt->lastPrice.at <= checkRangeEnd) {
					break;
				}
			}
			// find the best changed in the left of period
			bool detectedAnotherBestPrice = false;
			for (; jt != iter; jt++) {
				auto& ticker = *jt;
				if (abs(ticker.high.price - bestPricePoint->price) >= abs(priceChanged)) {
					detectedAnotherBestPrice = true;
					break;
				}
				if (abs(ticker.low.price - bestPricePoint->price) >= abs(priceChanged)) {
					detectedAnotherBestPrice = true;
					break;
				}
			}

			if (!detectedAnotherBestPrice) {
				priceChanged /= bestPricePoint->price;
				auto priceChangePerMin = priceChanged * 60 * 1000 / duration;
				if (abs(priceChangePerMin) >= trigger.priceChangePerMin) {
					((TickerUI*)theTicker)->processLevel = level;
					formatPriceChanged(buffer, sizeof(buffer), handler->getPair(), lastPrice, *bestPricePoint);

					InternalNotificationData notification;
					notification.message.title = _platformName;
					notification.message.message = buffer;
					notification.pair = handler->getPair();
					_messageQueue.pushMessage(notification);
					break;
				}
			}
		}
	}
}

const std::vector<CryptoBoardElmInfo>& PlatformEngine::getSymbolsStatistics() const {
	return _symbolsStatistics;
}

const std::vector<std::list<TickerUI>>& PlatformEngine::getSymbolsTickers() {
	return _symbolsTickers;
}

TradingPlatform* PlatformEngine::getPlatform() {
	return _platform;
}

const std::vector<string>& PlatformEngine::getCurrencies() const {
	return _currencies;
}

void PlatformEngine::setSymbolStatisticUpdatedHandler(SymbolStatisticUpdatedHandler&& handler) {
	_onSymbolStatisticUpdated = handler;
}