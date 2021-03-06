#ifdef WIN32
#include <filesystem>
using namespace std::experimental;
#else
#include <filesystem>
using namespace std;
#endif

#include "PlatformEngine.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <cpprest/json.h>
#include <algorithm>
#include <string>
#include <cpprest/http_client.h>

#include "../common/Utility.h"

#define TICKER_INTERVAL_DEFAULT 5000

#ifdef min
#undef min
#endif

using namespace std::chrono;
using namespace std;
typedef  TradingPlatform* (*CreatePlatformInstanceF)();

string formatPrice(double price) {
	char buffer[32];
	sprintf(buffer, "%.8f", price);
	buffer[10] = 0;

	return buffer;
}

PlatformEngine::PlatformEngine(const char* configFile) : _runFlag(false), _hLib(nullptr), _stopLoopTask(true) {
	using namespace std::placeholders;

	_platform = nullptr;

	auto updateMarketdataHandler = std::bind(&PlatformEngine::onMarketData, this, _1, _2, _3);
	_marketDataEventId = addMarketDataEventListener(updateMarketdataHandler);

	filesystem::path p(configFile);
	auto pathFile = p.filename();
	auto& pathExt = pathFile.extension();
	auto fileName = pathFile.u8string();
	_platformName = fileName.substr(0, fileName.size() - pathExt.u8string().size());
	
	std::string moduleName;

	auto parseTrigger = [](web::json::object& value) {
		TriggerPriceBase trigger;
		trigger.startTime = value[U("startTime")].as_integer();
		trigger.endTime = value[U("endTime")].as_integer();
		trigger.priceChangePerMin = (float)value[U("changedPerMin")].as_double();
		return trigger;
	};

	bool loadedTriggers = false;
	try {
		ifstream in;
		in.open(configFile);
		if (in.is_open()) {
			auto settings = web::json::value::parse(in);

			moduleName = CPPREST_FROM_STRING(settings[U("module")].as_string());

			auto& triggers = settings[U("triggers")].as_array();
			for (auto it = triggers.begin(); it != triggers.end(); it++) {
				_triggers.push_back( parseTrigger(it->as_object()));
			}
			sortPriceTriggers();

			auto& volumeTriggers = settings[U("volumeTriggers")].as_array();
			for (auto it = volumeTriggers.begin(); it != volumeTriggers.end(); it++) {
				auto& volumeTrigger = it->as_object();
				TriggerVolumeBaseItem volumeBaseTrigger;

				volumeBaseTrigger.measureDuration = volumeTrigger[U("measureDuration")].as_integer();
				volumeBaseTrigger.volumeChangedThreshold = (float)volumeTrigger[U("volumeChangedThresholdInPercent")].as_double();
				volumeBaseTrigger.miniumVolumeInBTC = (float)volumeTrigger[U("miniumVolumeInBTC")].as_double();
				if (volumeTrigger.find(U("priceChangedThresholdInPercent")) != volumeTrigger.end()) {
					volumeBaseTrigger.priceChangedThreshold = (float)volumeTrigger[U("priceChangedThresholdInPercent")].as_double();
				}
				else {
					volumeBaseTrigger.priceChangedThreshold = -1;
				}
				if (volumeTrigger.find(U("symbolFilter")) != volumeTrigger.end()) {
					auto symbolFilter = CPPREST_FROM_STRING(volumeTrigger[U("symbolFilter")].as_string());
					std::transform(symbolFilter.begin(), symbolFilter.begin(), symbolFilter.begin(), ::toupper);

					memcpy(volumeBaseTrigger.symbolFilter, symbolFilter.c_str(),
						std::min(symbolFilter.size() + 1, sizeof(volumeBaseTrigger.symbolFilter))
					);
				}
				else {
					volumeBaseTrigger.symbolFilter[0] = 0;
				}

				_volumeBaseTriggers.push_back(volumeBaseTrigger);
			}
			sortVolumeTriggers();

			loadedTriggers = true;

			auto& users = settings[U("users")].as_array();
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

			auto& periods = settings[U("periods")].as_array();
			for (auto it = periods.begin(); it != periods.end(); it++) {
				Period period;
				period.name = CPPREST_FROM_STRING(it->at(U("name")).as_string());
				period.durationInSecs = it->at(U("duration")).as_integer();

				_periods.push_back(period);
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
		pushLog((int)LogLevel::Error, "Load setting file %s failed\n", configFile);
	}

	_hLib = LoadLibraryA(moduleName.c_str());
	if (_hLib == nullptr) {
		pushLog((int)LogLevel::Error, "cannot load runtime platform %s\n", moduleName.c_str());
	}
	else {
		CreatePlatformInstanceF createFunc = (CreatePlatformInstanceF)GetProcAddress(_hLib, "createPlatformInstance");
		if (createFunc != nullptr) {
			_platform = createFunc();
			if (_platform) {
				_platform->setConfigFilePath(configFile);
			}
			else {
				pushLog((int)LogLevel::Error, "Create platform failed\n");
			}
		}
		else {
			pushLog((int)LogLevel::Error, "runtime platform %s is not valid\n", moduleName.c_str());
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
	removeMarketDataEventListener(_marketDataEventId);

	if (_platform) {
		_platform->disconnect();
		delete _platform;
	}

	if (_hLib) {
		FreeLibrary(_hLib);
	}

	destroy(_symbolsStatistics);
}

void PlatformEngine::sortPriceTriggers() {
	sort(_triggers.begin(), _triggers.end(), [](TriggerPriceBase& elm1, TriggerPriceBase& elm2) {
		return elm1.endTime < elm2.endTime;
	});
}

void PlatformEngine::sortVolumeTriggers() {
	// sort the trigger base on duration first then volume changed threshold
	// this sorting need for further processing
	std::sort(_volumeBaseTriggers.begin(), _volumeBaseTriggers.end(), [](TriggerVolumeBaseItem& item1, TriggerVolumeBaseItem& item2) {
		if (item1.measureDuration == item2.measureDuration) {
			if (item1.volumeChangedThreshold == item2.volumeChangedThreshold) {
				if (item1.miniumVolumeInBTC == item2.volumeChangedThreshold) {
					return item1.priceChangedThreshold > item2.priceChangedThreshold;
				}
				return item1.miniumVolumeInBTC > item2.volumeChangedThreshold;
			}
			return item1.volumeChangedThreshold > item2.volumeChangedThreshold;
		}

		return item1.measureDuration < item2.measureDuration;
	});
}

void PlatformEngine::pushMessageLoop() {
	InternalNotificationData notification;
	Notifier* notifier = Notifier::getInstance();
	while (_runFlag) {
		if (_messageQueue.popMessage(notification, 1000) && _runFlag) {
			auto it = _pairListenerMap.find(notification.pair);
			if (it != _pairListenerMap.end()) {
				auto& users = it->second;
				auto& messageInfo = notification.message;

				// push message to log
				const std::string& title = messageInfo.title;
				const std::string& message = messageInfo.message;
				auto str = Utility::time2str(notification.trigerTime);
				pushLog((int)LogLevel::Info, "[%s] %s %s\n", str.c_str(), title.c_str(), message.c_str());

				if (notification.notificationType == NotificationType::Price && _priceNotificationEnable == false) {
					continue;
				}
				if (notification.notificationType == NotificationType::Volume && _volumeNotificationEnable == false) {
					continue;
				}

				for (auto uit = users->begin(); uit != users->end() && _runFlag; uit++) {
					messageInfo.target = *uit;
					
					for (int tryCount = 1; tryCount <= 2; tryCount++) {
						try
						{
							if (notifier->pushNotification(messageInfo)) {
								break;
							}
						}
						catch (const std::exception &e)
						{
							pushLog((int)LogLevel::Error, "push message failed:%s\n", e.what());
						}
						catch (...) {
							pushLog((int)LogLevel::Error, "push message failed: unknow error\n");
						}
						
						pushLog((int)LogLevel::Error, "sending message again(%d)\n", tryCount);
					}
				}
			}
		}
	}
}

void PlatformEngine::updateMarketData() {
	using namespace web::http;
	/*
	Request: https://api.coinmarketcap.com/v1/global/
	Response:
	{
	"total_market_cap_usd": 378038428531.0,
	"total_24h_volume_usd": 26418975872.0,
	"bitcoin_percentage_of_market_cap": 39.39,
	"active_currencies": 895,
	"active_assets": 689,
	"active_markets": 10641,
	"last_updated": 1524322472
	}
	*/

	TIMESTAMP t1, t2;
	TIMESTAMP processTime;
	unsigned int sleepTime;

	// don't request market data too much time per one minute
	TIMESTAMP interval = 30 * 1000;

	client::http_client_config config;
	// 5 seconds timeout
	std::chrono::duration<long long, std::milli> timeout((long long)5000);
	config.set_timeout(timeout);

	http_request httpRequest(methods::GET);
	auto& headers = httpRequest.headers();
	headers[U("Connection")] = U("keep-alive");

	utility::string_t url = U("https://api.coinmarketcap.com/v1/global/");
	auto httClient = make_shared<client::http_client>(url, config);

	bool temp, exceptionOccured;
	MarketData marketData;
	marketData.at = 0;

	do
	{
		t1 = getCurrentTimeStamp();
		exceptionOccured = false;
		try {
			pushLog((int)LogLevel::Verbose, "requesting market data...\n");
			auto task = httClient->request(httpRequest).then([this, &marketData](http_response& response) {
				if (response.status_code() == 200) {
					auto js = response.extract_json().get();
					if (js.is_object()) {
						auto& obj = js.as_object();
						auto it1 = obj.find(U("total_market_cap_usd"));
						auto it2 = obj.find(U("total_24h_volume_usd"));
						auto it3 = obj.find(U("last_updated"));

						if (it1 != obj.end() && it2 != obj.end() && it3 != obj.end()) {
							// last_updated is an unix time
							TIMESTAMP lastUpdate = it3->second.as_number().to_int64() * 1000;
							marketData.marketCapUSD = it1->second.as_number().to_int64();
							marketData.volume24h = it3->second.as_number().to_int64();
							marketData.at = lastUpdate;

							{
								std::unique_lock<std::mutex> lk(_marketDataEventListenersMutex);
								for (auto it = _marketDataEventListeners.begin(); it != _marketDataEventListeners.end(); it++) {
									(it->second)(&marketData, 1, false);
								}
							}
						}
						else {
							pushLog((int)LogLevel::Error, "unknow response format %s\n", CPPREST_FROM_STRING(js.as_string()).c_str());
						}
					}
					else {
						pushLog((int)LogLevel::Error, "unknow response format %s\n", CPPREST_FROM_STRING(js.as_string()).c_str());
					}
				}
			});

			task.wait();
		}
		catch (const std::exception&e) {
			pushLog((int)LogLevel::Error, "request market data failed: %s\n", e.what());
			exceptionOccured = true;
		}
		catch (...) {
			pushLog((int)LogLevel::Error, "request market data failed: unknown error\n");
			exceptionOccured = true;
		}

		if (exceptionOccured) {
			// make the new client if an exception occured
			httClient = make_shared<client::http_client>(url, config);
		}

		t2 = getCurrentTimeStamp();

		processTime = t2 - t1;
		if (processTime < interval) {
			sleepTime = (unsigned int)(interval - processTime);
		}
		else {
			sleepTime = 0;
		}
	} while (_stopLoopTask.waitSignal(temp, sleepTime) == false);
}

void PlatformEngine::sheduleQueryHistory() {
	LOG_SCOPE_ACCESS(_platform->getLogger(), __FUNCTION__);
	RequestEventHistoryMessage message;
	while (_runFlag)
	{
		if (_symbolQueue.popMessage(message, 1000) && _runFlag) {
			if (message.eventType == EventHistoryType::TradeHistory) {
				auto symbol = message.pair.c_str();
				auto hander = _platform->getHandler(symbol);
				if (hander) {
					TradingList tradeItems;

					pushLog((int)LogLevel::Debug, "requesting trade history for %s\n", symbol);
					_platform->getTradeHistory(symbol, message.duration, message.endTime, tradeItems);
					vector<TradeItem> tradeRawItems(tradeItems.size());
					TradeItem* pItem = tradeRawItems.data();
					for (auto iter = tradeItems.head(); iter; iter = iter->nextNode) {
						*pItem++ = iter->value;
					}

					pushLog((int)LogLevel::Debug, "received trade history for %s\n", symbol);
					hander->onTradesUpdate(tradeRawItems.data(), (int)tradeRawItems.size(), true);
				}
			}
			else if (message.eventType == EventHistoryType::CandleHistory) {
				auto symbol = message.pair.c_str();
				auto hander = _platform->getHandler(symbol);
				if (hander) {
					CandleList candleItems;
					pushLog((int)LogLevel::Debug, "requesting candle history for %s\n", symbol);
					_platform->getCandleHistory(symbol, message.duration, message.endTime, candleItems);
					vector<CandleItem> candleRawItems(candleItems.size());
					CandleItem* pItem = candleRawItems.data();
					for (auto iter = candleItems.head(); iter; iter = iter->nextNode) {
						*pItem++ = iter->value;
					}

					pushLog((int)LogLevel::Debug, "received candle history for %s\n", symbol);
					hander->onCandlesUpdate(candleRawItems.data(), (int)candleRawItems.size(), true);
				}
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
	double amount, amountABS;
	double cost;
	TIMESTAMP serverTimeNow = _platform->getSyncTime(getCurrentTimeStamp());

	auto pVolumePeriod = info->periods;

	TIMESTAMP periodDuration = 0;

	auto it = tickers.begin();
	for (auto pit = _periods.begin(); pit != _periods.end(); pit++) {
		pVolumePeriod->bought = 0;
		pVolumePeriod->sold = 0;
		amount = 0;
		cost = 0;
		periodDuration += pit->durationInSecs * 1000;
		for (; it != tickers.end(); it++) {
			auto duration = serverTimeNow - it->firstPrice.at;
			if (duration > periodDuration) {
				break;
			}
			pVolumePeriod->bought += it->boughtVolume;
			pVolumePeriod->sold += it->soldVolume;

			amountABS = (it->boughtVolume + it->soldVolume);
			cost += it->averagePrice * amountABS;
			amount += amountABS;
		}
		if (amount) {
			pVolumePeriod->averagePrice = cost / amount;
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
		// save the ticker that mark as processed
		list<TickerUI> markTickers;
		for (auto it = tickers.begin(); it != tickers.end(); it++) {
			if (it->processLevel >= 0) {
				markTickers.push_back(*it);
			}
		}
		// build tickers for all trade items of curent pair on snapshot event
		tickers.clear();
		if (trades.size()) {
			auto& lastTrade = trades.front();
			TIMESTAMP baseTime = lastTrade.timestamp - lastTrade.timestamp % TICKER_DURATION;

			typedef decltype(trades.rbegin()) ITERATOR;
			auto nextIter = [](ITERATOR& it) { it++; };

			buildFromEmptyTickers(trades.rbegin(), trades.rend(), nextIter, tickers);
		}

		// restore the mark ticker
		auto jt = tickers.begin();

		Range<TIMESTAMP> markRange;
		Range<TIMESTAMP> newRange;
		Range<TIMESTAMP> overlapped;

		auto f = [&markRange, &newRange, &overlapped](TickerUI& item) {
			newRange.start = item.firstPrice.at;
			newRange.end = item.lastPrice.at;
			mergeParts<TIMESTAMP>(markRange, newRange, nullptr, &overlapped);

			return (overlapped.start <= overlapped.end);
		};

		for (auto it = markTickers.begin(); it != markTickers.end() && jt != tickers.end(); it++) {
			markRange.start = it->firstPrice.at;
			markRange.end = it->lastPrice.at;

			jt = std::find_if(jt, tickers.end(), f);

			if (jt != tickers.end()) {
				jt->processLevel = it->processLevel;
				pushLog((int)LogLevel::Verbose, "restore process level %s\n", sender->getPair());

				jt++;
			}
		}
		//pushLog("built ticker as snapshot event %s\n", sender->getPair());
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

				pushLog((int)LogLevel::Verbose, "updated the last ticker\n");
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

						pushLog((int)LogLevel::Verbose, "merged tickers \n");
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
						pushLog((int)LogLevel::Error, "something wrong, the update trade event is not newest event\n");
					}
				}
			}
		}
	}

	CryptoBoardElmInfo* info = _symbolsStatistics[i];
	info->price = trades.front().price;
	info->volume = fabs(trades.front().amount);

	{
		std::unique_lock<std::mutex> lk(_symboRequestslMutex);
		// it should only request snapshot for trade when first trade history return
		// to ensure the trade historys in local is fully and continous
		// try to insert a request and mark this request is executed on trade event
		auto it = _sentTradeSnapshotRequest.insert(std::make_pair(sender->getPair(), 0));
		if (it.second) {
			TIMESTAMP tEnd = 0;
			TIMESTAMP duration = 0;

			for (auto it = _periods.begin(); it != _periods.end(); it++) {
				duration += it->durationInSecs * 1000;
			}
			if (trades.size()) {
				tEnd = trades.back().timestamp;
				duration -= trades.front().timestamp - tEnd;
			}
			RequestEventHistoryMessage message;
			message.pair = sender->getPair();
			message.duration = duration;
			message.endTime = tEnd;
			message.eventType = EventHistoryType::TradeHistory;

			_symbolQueue.pushMessage(message);
		}
		else if (it.first->second != 0) {
			auto requestedHistoryEndAt = it.first->second;
			if (requestedHistoryEndAt < pTradeItemStart->timestamp) {
				// request trade histories to fill gaps between manual trade request and last trade event
				RequestEventHistoryMessage message;
				message.pair = sender->getPair();
				message.endTime = pTradeItemStart->timestamp;
				message.duration = message.endTime - requestedHistoryEndAt;
				message.eventType = EventHistoryType::TradeHistory;

				_symbolQueue.pushMessage(message);
				pushLog((int)LogLevel::Info, "send request trade history to fill gap for %s\n", sender->getPair());
			}
			// mark this request is executed on trade event
			it.first->second = 0;
		}
	}
	
	//analyze tickers for notification
	analyzeTickerForNotification(sender, tickers);

	// update satistics info for the symbol
	updateSymbolStatistics(info, tickers);
	if (_onSymbolStatisticUpdated) {
		_onSymbolStatisticUpdated(i);
	}
}

bool PlatformEngine::measureVolumeInPeriod(
	TIMESTAMP duration, TIMESTAMP lastProcessingTime, 
	std::list<CandleItem>::const_iterator&it, const std::list<CandleItem>::const_iterator& end,
	double& volume, double& priceHigh, double& priceLow) {
	auto timePoint = it->timestamp;
	for (; it != end; it++) {
		if (it->timestamp <= lastProcessingTime) {
			return false;
		}
		if (timePoint - it->timestamp >= duration) {
			break;
		}
		volume += it->volume;

		if (priceLow > it->low) {
			priceLow = it->low;
		}
		if (priceHigh < it->high) {
			priceHigh = it->high;
		}
	}

	return true;
}

void PlatformEngine::onCandle(int i, NAPMarketEventHandler* sender, CandleItem* candleItems, int count, bool snapshot) {
	{
		std::unique_lock<std::mutex> lk(_symboRequestslMutex);
		// try to insert a request and mark this request is executed on trade event
		auto rit = _sentCandleSnapshotRequest.insert(std::make_pair(sender->getPair(), 0));
		if (rit.second) {
			auto& candleItems = sender->getCandleHistoriesNonSync();
			if (candleItems.size()) {
				//TIMESTAMP tEnd = candleItems.back().timestamp - 1;
				//TIMESTAMP duration = 24 * 3600 * 1000 - (candleItems.front().timestamp - tEnd);

				TIMESTAMP tEnd = candleItems.back().timestamp - 1;
				TIMESTAMP duration = 7 * 24 * 3600 * 1000;

				RequestEventHistoryMessage message;
				message.pair = sender->getPair();
				message.duration = duration;
				message.endTime = tEnd;
				message.eventType = EventHistoryType::CandleHistory;

				_symbolQueue.pushMessage(message);
			}
		}
		else if (rit.first->second != 0) {
			auto& candleItems = sender->getCandleHistoriesNonSync();
			if (candleItems.size() == 0) {
				return;
			}
			auto& lastItem = candleItems.back();

			auto requestedHistoryEndAt = rit.first->second;
			if (requestedHistoryEndAt < lastItem.timestamp) {
				// request trade histories to fill gaps between manual trade request and last trade event
				RequestEventHistoryMessage message;
				message.pair = sender->getPair();
				message.endTime = lastItem.timestamp;
				message.duration = message.endTime - requestedHistoryEndAt;
				message.eventType = EventHistoryType::CandleHistory;

				_symbolQueue.pushMessage(message);
				pushLog((int)LogLevel::Info, "send request trade history to fill gap for %s\n", sender->getPair());
			}
			// mark this request is executed on trade event
			rit.first->second = 0;
		}
	}

	// process notification for volume
	if (snapshot == false) {
		auto& lastProcessingTime = _notifyProcessingVolumeMap[i];
		auto& candleItems = sender->getCandleHistoriesNonSync();
		auto it = candleItems.begin();
		if (it == candleItems.end()) {
			return;
		}
		auto timePoint = it->timestamp;
		auto priceNow = it->close;
		auto itEnd = candleItems.end();
		double lastestVolume = 0, previousLow = FLT_MAX, previousHigh = FLT_MIN;
		auto itStep1Previous = it;
		TIMESTAMP step1PrevousDuration = 0;

		std::unique_lock<std::mutex> lk(_volumeTrigersMutex);
		for (auto triggerIt = _volumeBaseTriggers.begin(); triggerIt != _volumeBaseTriggers.end(); triggerIt++) {
			auto measureDuration = ((TIMESTAMP)(triggerIt->measureDuration)) * 1000;
			/// part 1 - caculate volumes and prices in two lastest period base on current trigger duration
			// caculate volume for first period at current duration
			// for optimization, we only calculate the part that not count at previous of time
			auto step1Duration = measureDuration - step1PrevousDuration;
			
			auto itTemp = itStep1Previous;
			if (measureVolumeInPeriod(step1Duration, lastProcessingTime, itStep1Previous, itEnd, lastestVolume, previousHigh, previousLow) == false) {
				break;
			}
			// check if the iterator does not move or it reach the end
			if (itStep1Previous == itEnd || itStep1Previous == itTemp) {
				break;
			}
			it = itStep1Previous;
			step1PrevousDuration = measureDuration;

			// caculate volume for second period at current duration
			double previousVolume = 0;
			previousLow = FLT_MAX;
			previousHigh = FLT_MIN;
			auto previousTimePoint = it->timestamp;
			decltype(timePoint) lastCheckTime;
			
			itTemp = it;
			if (measureVolumeInPeriod(measureDuration, lastProcessingTime, it, itEnd, previousVolume, previousHigh, previousLow) == false) {
				break;
			}
			// check if the iterator does not move
			if (it == itTemp) {
				break;
			}

			/// part 2 - judge the action, caculate volume changed, price changed
			// get the last checked time
			if (it == candleItems.end()) {
				lastCheckTime = candleItems.back().timestamp;
			}
			else {
				// revert iterator to previous position to get previous timepoint
				it--;
				lastCheckTime = it->timestamp;
				// make iterator at current position
				it++;
			}

			const char* action = nullptr;
			float volumeChange, timeChange;
			double smallerVolume, largerVolume;

			if (lastestVolume > previousVolume) {
				action = "jumped";
				smallerVolume = previousVolume;
				largerVolume = lastestVolume;
			}
			else {
				smallerVolume = lastestVolume;
				largerVolume = previousVolume;
				// for drop action we must wait enough time to judge the movement
				if (lastCheckTime - previousTimePoint >= measureDuration * 90 / 100) {
					action = "dropped";
				}
			}
			if (smallerVolume <= 0) {
				smallerVolume = 0.00001;
			}

			decltype(priceNow) priceFrom, priceTo;

			if (std::abs(previousHigh - priceNow) > std::abs(previousLow - priceNow)) {
				priceFrom = previousHigh;
				priceTo = priceNow;
			}
			else {
				priceFrom = previousLow;
				priceTo = priceNow;
			}

			auto priceChanged = (priceTo - priceFrom) / priceFrom * 100;
			volumeChange = (largerVolume - smallerVolume) / smallerVolume * 100;
			bool passedAllCondition = false;

			/// part 3 - check if all trigger's conditions are matched
			if (action) {
				auto& originCrytoInfo = _symbolsStatistics[i];
				auto& symbol = originCrytoInfo->symbol;
				static const std::string baseCurrency = "BTC";
				double convertedPrice = -1;
				
				// check all trigger that have same duration
				for (auto jt = triggerIt; jt != _volumeBaseTriggers.end(); jt++) {
					if (jt->measureDuration != triggerIt->measureDuration) {
						break;
					}
					triggerIt = jt;

					if (jt->symbolFilter[0] == 0 || strcmp(jt->symbolFilter, symbol) == 0) {
						if (volumeChange >= jt->volumeChangedThreshold) {
							if (jt->priceChangedThreshold <= 0 || std::abs(priceChanged) >= jt->priceChangedThreshold) {
								// convert price to BTC to check minium volume base on BTC on the period
								if (convertedPrice <= 0) {
									convertPrice(symbol, baseCurrency, originCrytoInfo->price, convertedPrice);
								}
								if (convertedPrice > 0) {
									// trading volume must larger than a specific amount of BTC to trigger notification
									if (largerVolume * convertedPrice >= jt->miniumVolumeInBTC) {
										passedAllCondition = true;
										break;
									}
								}
							}
						}
					}
				}
			}

			/// part 4 send notification
			if (passedAllCondition) {
				char buffer[192];
				timeChange = (float)((timePoint - lastCheckTime) / (1000.0f * 60.0f));

				char* priceAction = "jumped +";
				if (priceChanged < 0) {
					priceAction = "dropped ";
				}

				sprintf_s(buffer, sizeof(buffer), "%s's volume %s %.2f%% in %.2f min from %f to %f, price %s%.2f%% from %s to %s at %s",
					sender->getPair(), action, (float)volumeChange, timeChange, (float)previousVolume, (float)lastestVolume,
					priceAction, priceChanged,
					formatPrice(priceFrom).c_str(),
					formatPrice(priceTo).c_str(), Utility::time2shortStr(timePoint).c_str()
				);

				InternalNotificationData notification;
				notification.notificationType = NotificationType::Volume;
				notification.trigerTime = timePoint;
				notification.message.title = _platformName;
				notification.message.message = buffer;
				notification.pair = sender->getPair();
				_messageQueue.pushMessage(notification);

				lastProcessingTime = timePoint;
			}
		}
	}
}

void PlatformEngine::setSymbolRequestHitoriesHighPriority(const std::string& symbol) {
	std::unique_lock<std::mutex> lk(_symboRequestslMutex);

	auto currentTimeStamp = getCurrentTimeStamp();

	// try to insert a request and mark this request is executed on candle event
	auto rit = _sentCandleSnapshotRequest.insert(std::make_pair(symbol, currentTimeStamp));

	int sentCount = 0;

	// check if candle histories request was not sent
	if (rit.second) {
		TIMESTAMP tEnd = currentTimeStamp;
		TIMESTAMP duration = 7 * 24 * 3600 * 1000;

		RequestEventHistoryMessage message;
		message.pair = symbol;
		message.duration = duration;
		message.endTime = tEnd;
		message.eventType = EventHistoryType::CandleHistory;

		_symbolQueue.pushMessageFront(message);
		pushLog((int)LogLevel::Info, "candle request was pushed at the highest priority\n");
		sentCount++;
	}
	auto cit = _sentTradeSnapshotRequest.insert(std::make_pair(symbol, currentTimeStamp));
	// check if trade histories request was not sent
	if (cit.second) {
		TIMESTAMP duration = 0;
		for (auto it = _periods.begin(); it != _periods.end(); it++) {
			duration += it->durationInSecs * 1000;
		}

		RequestEventHistoryMessage message;
		message.pair = symbol;
		message.duration = duration;
		message.endTime = currentTimeStamp;
		message.eventType = EventHistoryType::TradeHistory;

		_symbolQueue.pushMessage(message);

		pushLog((int)LogLevel::Info, "trade request was pushed at the highest priority\n");
		sentCount++;
	}
	if (sentCount == 2) return;

	std::list<RequestEventHistoryMessage> requestMessages;
	// remove all candle request message for this symbol
	_symbolQueue.removeMessage([&requestMessages, &symbol](RequestEventHistoryMessage& requestMessage) {
		if (requestMessage.pair == symbol) {
			requestMessages.push_back(requestMessage);
			return true;
		}
		return false;
	});

	if (requestMessages.size()) {
		for (auto it = requestMessages.rbegin(); it != requestMessages.rend(); it++) {
			_symbolQueue.pushMessageFront(*it);
		}
		pushLog((int)LogLevel::Info,  "request for %s was re-arranged at the highest priority\n", symbol.c_str());
	}
	else {
		pushLog((int)LogLevel::Error, "request for %s is executing or was executed completed\n", symbol.c_str());
	}
}

void PlatformEngine::run() {
	using namespace std::placeholders;
	if (_platform == nullptr) return;

	LOG_SCOPE_ACCESS(_platform->getLogger(), __FUNCTION__);

	_runFlag = true;
	_stopLoopTask.resetState(false);

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
			pushLog((int)LogLevel::Debug, "query list symbol failed, error: %s\n", e.what());
			return;
		}
		catch (...) {
			pushLog((int)LogLevel::Debug, "query list symbol failed, unknown error\n");
			return;
		}
	}

	if (_pairListenerMap.size() == 0) {
		pushLog((int)LogLevel::Debug, "no symbol defined\n");
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

	destroy(_symbolsStatistics);
	_symbolsTickers.clear();
	_symbolIndexMap.clear();

	_symbolsStatistics.resize(_pairListenerMap.size());
	_symbolsTickers.resize(_symbolsStatistics.size());
	_notifyProcessingVolumeMap.resize(_symbolsStatistics.size());
	int i = 0;
	int nPeriod = (int)_periods.size();

	for (auto it = _pairListenerMap.begin(); it != _pairListenerMap.end(); it++, i++) {
		NAPMarketEventHandler* eventHandler = new NAPMarketEventHandler(it->first.c_str());
		eventHandler->useTicker(false);
		eventHandler->useOrderBook(false);
		eventHandler->useTrades(true);
		eventHandler->useCandles(true);
		_platform->addEventHandler(eventHandler, true);

		CryptoBoardElmInfo* elmInfo = createCrytpElm(nPeriod);
		elmInfo->symbol = it->first.c_str();

		_symbolsStatistics[i] = elmInfo;
		_notifyProcessingVolumeMap[i] = 0;
		// map symbol with index of its element info
		_symbolIndexMap[it->first] = i;

		auto tradeEventListener = 
			std::bind(&PlatformEngine::onTrade, this, i, _1 , _2, _3, _4);
		eventHandler->addTradeEventListener(tradeEventListener);

		auto candleEventListener =
			std::bind(&PlatformEngine::onCandle, this, i, _1, _2, _3, _4);
		eventHandler->addCandleEventListener(candleEventListener);
	}

	// start interval thread
	//_broadCastIntervalTask = std::async(std::launch::async, [this]() {timeInterval(); });
	_messageLoopTask = std::async(std::launch::async, [this]() {pushMessageLoop(); });
	_sendTradeHistoryRequestLoop = std::async(std::launch::async, [this]() {sheduleQueryHistory(); });
	_marketDataRequestLoop = std::async(std::launch::async, [this]() {updateMarketData(); });

	// starting query time from server
	_platform->startServerTimeQuery(5000);

	// reset requesting history message
	_sentTradeSnapshotRequest.clear();
	_sentCandleSnapshotRequest.clear();
	_messageQueue.clear();

	// connect to the platform
	try {
		_platform->connect();
	}
	catch (exception&e) {
		pushLog((int)LogLevel::Error, "failed to connect to server:%s\n", e.what());
		return;
	}
}

void PlatformEngine::stop() {
	LOG_SCOPE_ACCESS(_platform->getLogger(), __FUNCTION__);

	if (_runFlag == false) return;
	_runFlag = false;
	_stopLoopTask.sendSignal(true);

	_platform->disconnect();

	pushLog((int)LogLevel::Debug, "stopped!\n");
	pushLog((int)LogLevel::Debug, "stoping message loop task...\n");
	if (_messageLoopTask.valid()) {
		// push an empty message
		_messageQueue.pushMessage({});
		_messageLoopTask.wait();
	}
	if (_marketDataRequestLoop.valid()) {
		_marketDataRequestLoop.wait();
	}
	pushLog((int)LogLevel::Debug, "stopped!\n");
	pushLog((int)LogLevel::Debug, "stop querying trade history...\n");
	if (_sendTradeHistoryRequestLoop.valid()) {
		_sendTradeHistoryRequestLoop.wait();
	}

	pushLog((int)LogLevel::Debug, "stopped!\n");
}

void formatPriceChanged(char* buffer, size_t bufferSize, const char* pair, const PricePoint& lastPrice, const PricePoint& basePrice) {

	double priceChanged = lastPrice.price - basePrice.price;
	TIMESTAMP duration = lastPrice.at - basePrice.at;
	const char* movementStr = "jumped +";
	if (priceChanged < 0) {
		movementStr = "dropped ";
	}

	auto priceChangedPercent = priceChanged / basePrice.price * 100;

	sprintf_s(buffer, bufferSize, "%s's price %s%.2f%% in %.2f min from %s to %s at %s", pair, movementStr,
		(float)priceChangedPercent, (duration / (60 * 1000.0f)), formatPrice(basePrice.price).c_str(), formatPrice(lastPrice.price).c_str(), Utility::time2shortStr(lastPrice.at).c_str());
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

	std::unique_lock<std::mutex> lk(_priceTrigersMutex);
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
			if (trigger.startTime * 1000 <= duration && duration < trigger.endTime * 1000) {
				if (updateBestPricePoint(bestPricePoint, lastPrice, &(ticker.low))) {
					theTicker = &ticker;
					iter = jt;
				}
			}

			duration = lastPrice.at - ticker.high.at;
			if (trigger.startTime * 1000 <= duration && duration < trigger.endTime * 1000) {
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
				if (abs(priceChangePerMin) >= trigger.priceChangePerMin/100) {
					((TickerUI*)theTicker)->processLevel = level;
					formatPriceChanged(buffer, sizeof(buffer), handler->getPair(), lastPrice, *bestPricePoint);

					InternalNotificationData notification;
					notification.trigerTime = lastPrice.at;
					notification.notificationType = NotificationType::Price;
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

const std::vector<CryptoBoardElmInfo*>& PlatformEngine::getSymbolsStatistics() const {
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

const std::vector<Period>& PlatformEngine::getPeriods() const {
	return _periods;
}

void PlatformEngine::setSymbolStatisticUpdatedHandler(SymbolStatisticUpdatedHandler&& handler) {
	_onSymbolStatisticUpdated = handler;
}

void PlatformEngine::enablePriceNotification(bool enable) {
	_priceNotificationEnable = enable;
}

void PlatformEngine::enableVolumeNotification(bool enable) {
	_volumeNotificationEnable = enable;
}

const std::string& PlatformEngine::getQuote(const std::string& symbol) {
	auto& currencies = getCurrencies();
	for (auto it = currencies.begin(); it != currencies.end(); it++) {
		if (symbol.compare(symbol.size() - it->size(), it->size(), *it) == 0) {
			return *it;
		}
	}

	static std::string emptyStr;
	return emptyStr;
}

bool PlatformEngine::convertPrice(const std::string& symbol, const std::string& baseQuoteCurrency, const double& price, double& convertedPrice) {
	auto& quoteCurency = getQuote(symbol);
	if (quoteCurency.empty()) {
		return false;
	}
	if (quoteCurency == baseQuoteCurrency) {
		convertedPrice = price;
		return true;
	}

	auto newSymbol = quoteCurency + baseQuoteCurrency;
	auto it = _symbolIndexMap.find(newSymbol);
	bool reverse = false;
	if (it == _symbolIndexMap.end()) {
		newSymbol = baseQuoteCurrency + quoteCurency;
		it = _symbolIndexMap.find(newSymbol);
		if (it == _symbolIndexMap.end()) {
			return false;
		}
		reverse = true;
	}

	int i = it->second;
	auto& originCrytoInfo = _symbolsStatistics[i];
	auto& newPrice = originCrytoInfo->price;
	if (reverse) {
		convertedPrice = price / newPrice;
	}
	else {
		convertedPrice = price * newPrice;
	}

	return true;
}

int PlatformEngine::addMarketDataEventListener(MarketDataEventListener&& eventListener) {
	std::unique_lock<std::mutex> lk(_marketDataEventListenersMutex);

	MarketDataEventListener emptyFunc;
	auto emptyPair = std::make_pair(_autoId, emptyFunc);
	auto it = _marketDataEventListeners.insert(emptyPair);
	it.first->second = eventListener;

	_autoId++;

	return emptyPair.first;
}

void PlatformEngine::removeMarketDataEventListener(int id) {
	std::unique_lock<std::mutex> lk(_marketDataEventListenersMutex);
	_marketDataEventListeners.erase(id);
}

void PlatformEngine::onMarketData(MarketData* items, int n, bool snapShot) {
	if (n == 0 || items == nullptr) return;
	std::unique_lock<std::mutex> lk(_marketHistoriesMutex);
	if (snapShot) {
		pushLog((int)LogLevel::Info, "market data snapshot event has not been implemeted\n");
	}
	else if (n == 1) {
		if (_marketHistories.size() == 0 || _marketHistories.back().at != items->at) {
			_marketHistories.push_back(*items);
		}
	}
	else {
		pushLog((int)LogLevel::Info, "multiple market data update event has not been implemeted\n");
	}
}

void PlatformEngine::setPriceTriggers(const std::vector<TriggerPriceBase>& triggers) {
	std::unique_lock<std::mutex> lk(_priceTrigersMutex);
	_triggers = triggers;
	sortPriceTriggers();
}

const std::vector<TriggerPriceBase>& PlatformEngine::getPriceTriggers() const {
	return _triggers;
}

void PlatformEngine::setVolumeTriggers(const std::vector<TriggerVolumeBaseItem>& triggers) {
	std::unique_lock<std::mutex> lk(_volumeTrigersMutex);
	_volumeBaseTriggers = triggers;
	sortVolumeTriggers();
}

const std::vector<TriggerVolumeBaseItem>& PlatformEngine::getVolumeTriggers() const {
	return _volumeBaseTriggers;
}