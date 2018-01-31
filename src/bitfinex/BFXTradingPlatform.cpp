#include "BFXTradingPlatform.h"

#include "Utility.h"

#include <string>
#include <iostream>
#include <chrono>

#include <cpprest/http_client.h>
using namespace std;

using namespace web;
using namespace web::websockets::client;
using namespace web::http;                  // Common HTTP functionality
using namespace web::http::client;          // HTTP client features


BFXTradingPlatform::BFXTradingPlatform() : _timeDiff(0), _serverTimeIsReady(false) {
	resetClient();
}

BFXTradingPlatform::~BFXTradingPlatform()
{
	disconnect();
	cout << "deleted BFXTradingPlatform object" << endl;
}

void BFXTradingPlatform::maintenanceConnectionWorker() {
	using namespace std::chrono_literals;
	bool runFlag = true;
	while (runFlag)
	{
		_restartEvents.waitSignal(runFlag);
		if (runFlag == false) {
			break;
		}

		// after the wait, we own the lock.
		pushLog("Received reconnect signal\n");
		// sleep for 3 second and try to reconnect client
		std::this_thread::sleep_for(3s);
		resetClientNonSync();
		pushLog("reconnecting...\n");
		connect();
		pushLog("executed reconnection\n");
	}
}

void BFXTradingPlatform::messageHandlerImpl(const websocket_incoming_message& msg) {
	auto bodyText = msg.extract_string().get();

	stringstream sstream(bodyText);
	std::error_code errorCode;
	auto value = json::value::parse(sstream, errorCode);

	if (errorCode.value()) {
		cout << "Parse response error" << endl;
		return;
	}
	if (value.is_object())
	{
		std::cout << "response message: " << bodyText << std::endl;

		if (value.has_field(U("event"))) {
			auto eventValue = value[U("event")].as_string();
			if (eventValue == U("info")) {
				if (value.has_field(U("code")) && value.has_field(U("msg"))) {
					auto code = value[U("code")].as_integer();

					if (code == 20051) {
						pushLog("receive reconnect signal\n");
						pushLog("sending reconnect signal...\n");
						_restartEvents.sendSignal(true);
						
					}
					else if (code == 20060) {
						pushLog("Entering in Maintenance mode.(it should take 120 seconds at most).\n");
					}
					else if (code == 20060) {
						pushLog("Maintenance mode end\n");
					}
				}
				else if(value.has_field(U("version"))){
					setConnectionStatus(true);
					resetClientMessageHandlerNonSync();
				}
			}
			else if (value.has_field(U("channel"))) {
				utility::string_t tpair;
				if (value.has_field(U("pair"))) {
					tpair = value[U("pair")].as_string();
				}
				else if (value.has_field(U("key"))) {
					tpair = value[U("key")].as_string();
					auto pos = tpair.find_last_of(U(':'));
					if (pos == utility::string_t::npos || pos == tpair.size() - 1) {
						tpair = U("");
					}
					else {
						// ignore ':' and 't'
						tpair = tpair.substr(pos + 2);
					}
				}
				if (tpair.empty()) {
					cout << "Event is not supported" << endl;
					return;
				}
				auto key = CPPREST_FROM_STRING(tpair);
				std::unique_lock<std::mutex> lk(_eventMutex);

				auto handler = getHandler(key.c_str());
				if (handler) {
					SendMarketEventFunc marketEventHandler;
					const char* eventName;

					auto chanel = value[U("channel")].as_string();
						
					if (chanel == U("book")) {
						marketEventHandler = &BFXTradingPlatform::invokeBookEvent;
						eventName = MARKET_EVENT_BOOK;
					}
					else if (chanel == U("ticker")) {
						marketEventHandler = &BFXTradingPlatform::invokeTickerEvent;
						eventName = MARKET_EVENT_TICKER;
					}
					else if (chanel == U("trades")) {
						marketEventHandler = &BFXTradingPlatform::invokeTradeEvent;
						eventName = MARKET_EVENT_TRADE;
					}
					else if (chanel == U("candles")) {
						marketEventHandler = &BFXTradingPlatform::invokeCandleEvent;
						eventName = MARKET_EVENT_CANDLE;
					}
					else {
						cout << "chanel " << Utility::toString(chanel) << " is not support" << endl;
					}
					if (marketEventHandler) {
						if (value.has_field(U("chanId"))) {
							auto chanelid = value[U("chanId")].as_integer();

							EventHandlerInfo eventHandlerInfo;
							eventHandlerInfo.eventName = eventName;
							eventHandlerInfo.handler = handler;
							eventHandlerInfo.invokefunction = marketEventHandler;

							_chanelEventHandlerInfoMap[chanelid] = eventHandlerInfo;

							setEventSubscribed(eventName, key.c_str(), SubcribeStatus::Subcribed);

							// combine event name and pair to the key
							key.insert(0, eventName);
							_chanelEventMap[key] = chanelid;
						}
						else {
							setEventSubscribed(eventName, key.c_str(), SubcribeStatus::NA);
						}
					}
				}
				else {
					cout << "No event handler for the market event found" << endl;
				}
			}
			else if (eventValue == U("unsubscribed") && value.has_field(U("chanId"))) {
				auto chanelid = value[U("chanId")].as_integer();
				auto it = _chanelEventHandlerInfoMap.find(chanelid);
				if (it != _chanelEventHandlerInfoMap.end()) {
					auto& eventHandlerInfo = it->second;
					setEventSubscribed(eventHandlerInfo.eventName, eventHandlerInfo.handler->getPair(), SubcribeStatus::Unsubcribed);
				}
				else {
					cout << "unknown chanel was unsubcribed" << endl;
				}
			}
		}
	}
	else if (value.is_array()) {
		std::unique_lock<std::mutex> lk(_eventMutex);
		auto chanelid = value.at(0).as_integer();
		auto it = _chanelEventHandlerInfoMap.find(chanelid);

		if (it != _chanelEventHandlerInfoMap.end()) {
			auto& marketEventHandler = it->second;
			auto invokeMethod = marketEventHandler.invokefunction;
			(this->*invokeMethod)(marketEventHandler.handler, value);
		}
	}
}

bool BFXTradingPlatform::connectImpl() {
	using namespace std::chrono_literals;

	_client->set_message_handler(std::bind(&BFXTradingPlatform::messageHandlerImpl, this, std::placeholders::_1));
	_client->connect(U("wss://api.bitfinex.com/ws/2")).wait();

	bool needSet = true;

	if (_maintenanceTask.valid()) {
		auto status = _maintenanceTask.wait_for(0ms);
		needSet = (status == std::future_status::ready);
	}
	if (needSet) {
		_maintenanceTask = std::async(std::launch::async, [this]() {maintenanceConnectionWorker(); });
	}

	// subcribe events for already exits handlers
	int nHandler = getHandlerCount();
	vector<MarketEventHandler*> handlers(nHandler);
	getHandlers(handlers.data(), nHandler);

	for (auto it = handlers.begin(); it != handlers.end(); it++) {
		subcribeEventForHandler(*it);
	}

	return getConnectionStatus();
}

bool BFXTradingPlatform::disconnectImpl() {
	_restartEvents.sendSignal(false);
	_maintenanceTask.wait();

	_stopLoopTask.sendSignal(false);
	_pingServerTask.wait();

	_client->close().wait();
	return true;
}

bool BFXTradingPlatform::addEventHandler(MarketEventHandler* handler, bool allowDelete) {
	std::unique_lock<std::mutex> lk(_eventMutex);

	if (TradingPlatform::addEventHandler(handler, allowDelete)) {
		Ticker dummy;
		dummy.lastPrice = -1;
		_lastickerMap[handler->getPair()] = dummy;
		if (getConnectionStatus()) {
			subcribeEventForHandler(handler);
		}
	}

	return true;
}

void BFXTradingPlatform::removeEventHandler(const char* pair) {
	std::unique_lock<std::mutex> lk(_eventMutex);
	TradingPlatform::removeEventHandler(pair);
}

SubcribeStatus BFXTradingPlatform::subscribeTickerImpl(const char* pair) {
	json::value obj;
	utility::string_t tpair = CPPREST_TO_STRING(pair);

	obj[L"event"] = json::value::string(U("subscribe"));
	obj[L"channel"] = json::value::string(U("ticker"));
	obj[L"symbol"] = json::value::string(U("t") + tpair);

	sendJsonContent(obj);

	return SubcribeStatus::Subcribing;
}
SubcribeStatus BFXTradingPlatform::subscribeBookImpl(const char* pair) {
	json::value obj;
	utility::string_t tpair = CPPREST_TO_STRING(pair);

	obj[L"event"] = json::value::string(U("subscribe"));
	obj[L"channel"] = json::value::string(U("book"));
	obj[L"prec"] = json::value::string(L"R0");
	obj[L"symbol"] = json::value::string(U("t") + tpair);
	obj[L"len"] = json::value(100);

	sendJsonContent(obj);
	return SubcribeStatus::Subcribing;
}
SubcribeStatus BFXTradingPlatform::subscribeTradeImpl(const char* pair) {
	json::value obj;
	utility::string_t tpair = CPPREST_TO_STRING(pair);

	obj[L"event"] = json::value::string(U("subscribe"));
	obj[L"channel"] = json::value::string(U("trades"));
	obj[L"symbol"] = json::value::string(U("t") + tpair);

	sendJsonContent(obj);
	return SubcribeStatus::Subcribing;
}
SubcribeStatus BFXTradingPlatform::subscribeCandleImpl(const char* pair) {
	json::value obj;
	utility::string_t tpair = CPPREST_TO_STRING(pair);

	obj[L"event"] = json::value::string(U("subscribe"));
	obj[L"channel"] = json::value::string(U("candles"));
	obj[L"key"] = json::value::string(U("trade:1m:t") + tpair);// request candle for timeframe 1 min

	sendJsonContent(obj);
	return SubcribeStatus::Subcribing;
}

SubcribeStatus BFXTradingPlatform::unsubcribeChanel(const std::string& eventName, const std::string& pair) {
	auto key = eventName + pair;
	int chanelId = 0;

	std::unique_lock<std::mutex> lk(_eventMutex);
	auto it = _chanelEventMap.find(key);
	if (it == _chanelEventMap.end()) {
		return isEventSubscribed(eventName.c_str(), pair.c_str());
	}

	chanelId = it->second;

	json::value obj;
	obj[L"event"] = json::value::string(U("unsubscribe"));
	obj[L"chanId"] = json::value(chanelId);

	sendJsonContent(obj);
	return SubcribeStatus::Unsubcribing;
}

SubcribeStatus BFXTradingPlatform::unsubscribeTickerImpl(const char* pair) {
	return unsubcribeChanel(MARKET_EVENT_TICKER, pair);
}
SubcribeStatus BFXTradingPlatform::unsubscribeBookImpl(const char* pair) {
	return unsubcribeChanel(MARKET_EVENT_BOOK, pair);
}
SubcribeStatus BFXTradingPlatform::unsubscribeTradeImpl(const char* pair) {
	return unsubcribeChanel(MARKET_EVENT_TRADE, pair);
}
SubcribeStatus BFXTradingPlatform::unsubscribeCandleImpl(const char* pair) {
	return unsubcribeChanel(MARKET_EVENT_CANDLE, pair);
}

void BFXTradingPlatform::invokeTickerEvent(MarketEventHandler* handler, web::json::value& value) {
	auto& secondElm = value.at(1);
	auto it = _lastickerMap.find(handler->getPair());
	if (it == _lastickerMap.end()) {
		cout << "internal error" << endl;
		return;
	}

	Ticker& ticker = it->second;

	if (secondElm.is_array() == false) {
		if (secondElm.is_string() == false) {
			cout << "Expected result is an array" << endl;
			return;
		}
		if (secondElm.as_string() != U("hb")) {
			cout << "unknown ticker response" << endl;
			return;
		}
	}
	else {
		auto& jsarray = secondElm.as_array();
		ticker.bid = jsarray.at(0).as_double();
		ticker.ask = jsarray.at(2).as_double();
		ticker.lastPrice = jsarray.at(6).as_double();
		ticker.volume = jsarray.at(7).as_double();
		ticker.high = jsarray.at(8).as_double();
		ticker.low = jsarray.at(9).as_double();
	}
	ticker.timestamp = getSyncTime(getCurrentTimeStamp());

	handler->onTickerUpdate(ticker);
}

void BFXTradingPlatform::invokeBookEvent(MarketEventHandler* handler, web::json::value& value) {
	auto& secondElm = value.at(1);
	list<BookItem> askItems;
	list<BookItem> bidItems;

	if (secondElm.is_array() == false) {
		if (secondElm.is_string() == false) {
			cout << "Expected result is an array" << endl;
			return;
		}
		if (secondElm.as_string() != U("hb")) {
			cout << "unknown ticker response" << endl;
			return;
		}
		handler->onBooksUpdate(nullptr, false);
		return;
	}
	
	if (secondElm.size() > 0 && secondElm.at(0).is_array() == false) {
		auto& itemElm = secondElm.as_array();

		BookItem bookItem;
		bookItem.oderId = itemElm[0].as_number().to_uint64();
		bookItem.price = itemElm[1].as_double();
		bookItem.amount = itemElm[2].as_double();

		BookItemEventArgs bookEventArgs;
		if (bookItem.amount < 0) {
			bookItem.amount = -bookItem.amount;
			bookEventArgs.askItems = &bookItem;
			bookEventArgs.askCount = 1;

			bookEventArgs.bidCount = 0;
			bookEventArgs.bidItems = nullptr;
		}
		else {
			bookEventArgs.bidItems = &bookItem;
			bookEventArgs.bidCount = 1;

			bookEventArgs.askCount = 0;
			bookEventArgs.askItems = nullptr;
		}

		handler->onBooksUpdate(&bookEventArgs, false);
		return;
	}

	for (int i = 0; i < (int)secondElm.size(); i++) {
		auto& bookElm = secondElm.at(i);
		if (bookElm.is_array() == false) {
			cout << "Expected result is an array" << endl;
			continue;
		}

		auto& arr = bookElm.as_array();
		if (arr.size() != 3) {
			cout << "book item must contain atleast 3 elements" << endl;
			continue;
		}
		BookItem bookItem;
		bookItem.oderId = arr[0].as_number().to_uint64();
		bookItem.price = arr[1].as_double();
		bookItem.amount = arr[2].as_double();

		if (bookItem.amount < 0) {
			bookItem.amount = -bookItem.amount;
			askItems.push_back(bookItem);
		}
		else {
			bidItems.push_back(bookItem);
		}
	}

	vector<BookItem> askRawItems(askItems.size());
	vector<BookItem> bidRawItems(bidItems.size());

	askRawItems.assign(askItems.begin(), askItems.end());
	bidRawItems.assign(bidItems.begin(), bidItems.end());

	BookItemEventArgs bookEventArgs;
	bookEventArgs.askCount = (int)askRawItems.size();
	bookEventArgs.askItems = askRawItems.data();
	bookEventArgs.bidCount = (int)bidRawItems.size();
	bookEventArgs.bidItems = bidRawItems.data();

	handler->onBooksUpdate(&bookEventArgs, true);
}

void BFXTradingPlatform::invokeTradeEvent(MarketEventHandler* handler, web::json::value& value) {
	auto& secondElm = value.at(1);
	list<TradeItem> trades;

	if (secondElm.is_array() == false) {
		if (secondElm.is_string() == false) {
			cout << "Expected result is an array" << endl;
			return;
		}
		auto valueStr = secondElm.as_string();
		// process only for te message
		if (valueStr == U("te") && value.at(2).is_array()) {

			auto& itemElm = value.at(2).as_array();
			
			TradeItem tradeItem;
			tradeItem.oderId = itemElm[0].as_number().to_uint64();
			tradeItem.timestamp = itemElm[1].as_number().to_uint64();
			tradeItem.amount = itemElm[2].as_double();
			tradeItem.price = itemElm[3].as_double();

			handler->onTradesUpdate(&tradeItem, 1, false);
		}
		return;
	}

	if (secondElm.size() <= 0 || secondElm.at(0).is_array() == false) {
		cout << "Expected result is an array" << endl;
		return;
	}

	for (int i = 0; i < (int)secondElm.size(); i++) {
		auto& itemElm = secondElm.at(i);
		if (itemElm.is_array() == false) {
			cout << "Expected result is an array" << endl;
			continue;
		}

		auto& arr = itemElm.as_array();
		if (arr.size() != 4) {
			cout << "trade item must contain atleast 4 elements" << endl;
			continue;
		}
		TradeItem tradeItem;
		tradeItem.oderId = itemElm[0].as_number().to_uint64();
		tradeItem.timestamp = itemElm[1].as_number().to_uint64();
		tradeItem.amount = itemElm[2].as_double();
		tradeItem.price = itemElm[3].as_double();

		trades.push_back(tradeItem);
	}

	vector<TradeItem> itemarr(trades.size());
	itemarr.assign(trades.begin(), trades.end());

	handler->onTradesUpdate(itemarr.data(), (int)itemarr.size(), true);
}


void BFXTradingPlatform::invokeCandleEvent(MarketEventHandler* handler, web::json::value& value) {
	auto& secondElm = value.at(1);
	list<CandleItem> candles;

	if (secondElm.is_array() == false) {
		if (secondElm.is_string() == false) {
			cout << "Expected result is an array" << endl;
			return;
		}
		if (secondElm.as_string() != U("hb")) {
			cout << "unknown ticker response" << endl;
			return;
		}
		handler->onCandlesUpdate(nullptr, 0, false);
		return;
	}

	if (secondElm.size() > 0 && secondElm.at(0).is_array() == false) {
		auto& itemElm = secondElm.as_array();
		if(itemElm.size() != 6) {
			cout << "unknown candle response" << endl;
			return;
		}
		CandleItem item;
		item.timestamp = itemElm[0].as_number().to_uint64();
		item.open = itemElm[1].as_double();
		item.close = itemElm[2].as_double();
		item.high = itemElm[3].as_double();
		item.low = itemElm[4].as_double();
		item.volume = itemElm[5].as_double();
		handler->onCandlesUpdate(&item, 1, false);

		return;
	}

	for (int i = 0; i < (int)secondElm.size(); i++) {
		auto& bookElm = secondElm.at(i);
		if (bookElm.is_array() == false) {
			cout << "Expected result is an array" << endl;
			continue;
		}

		auto& itemElm = bookElm.as_array();
		if (itemElm.size() != 6) {
			cout << "book item must contain atleast 6 elements" << endl;
			continue;
		}
		CandleItem item;
		item.timestamp = itemElm[0].as_number().to_uint64();
		item.open = itemElm[1].as_double();
		item.close = itemElm[2].as_double();
		item.high = itemElm[3].as_double();
		item.low = itemElm[4].as_double();
		item.volume = itemElm[5].as_double();
		candles.push_back(item);
	}

	vector<CandleItem> itemarr(candles.size());
	itemarr.assign(candles.begin(), candles.end());

	//wcout << value.to_string() << endl;
	handler->onCandlesUpdate(itemarr.data(), (int)itemarr.size(), true);
}

char* BFXTradingPlatform::getAllPairs() {
	http_client client(U("https://api.bitfinex.com/v1/symbols"));
	auto task = client.request(methods::GET);
	task.wait();
	auto response = task.get();
	auto code = response.status_code();
	if (code == status_codes::OK) {
		auto task = response.extract_json(true);
		task.wait();
		auto value = task.get();

		if (value.is_array()) {
			auto& symbols = value.as_array();
			list<string> pairs;
			size_t bufferSize = 0;
			for (auto it = symbols.begin(); it != symbols.end(); it++) {
				auto& elm = *it;
				if (!elm.is_string()) {
					cout << "element is not a string" << endl;
					continue;
				}

				auto symbol = elm.as_string();
				std::transform(symbol.begin(), symbol.end(), symbol.begin(), toupper);

				pairs.emplace_back(CPPREST_FROM_STRING(symbol));

				bufferSize += pairs.back().size() + 1;
			}

			char* bufferPairs = (char*)malloc(bufferSize + 1);
			char* c = bufferPairs;
			for (auto it = pairs.begin(); it != pairs.end(); it++) {
				memcpy_s(c, it->size(), it->data(), it->size());
				c += it->size();
				*c++ = ';';
			}
			*c = 0;

			return bufferPairs;
		}
		else {
			string erroreMsg("API error: unknow response format");
			throw std::runtime_error(erroreMsg);
		}
	}
	string erroreMsg("server response error:");
	erroreMsg.append(to_string(code));
	throw std::runtime_error(erroreMsg);
}

void BFXTradingPlatform::pingServerLoop() {
	bool temp;
	bool sent;

	TIMESTAMP t1, t2;
	TIMESTAMP processTime;
	TIMESTAMP interval = getQueryTimeInterval();
	unsigned int sleepTime = 0;
	/*
	// request
	{
	"event":"ping",
	"cid": 1234
	}
	*/
	int chanelId = 1234;
	json::value obj;
	obj[L"event"] = json::value::string(U("ping"));
	obj[L"cid"] = json::value(chanelId);

	do
	{
		t1 = getCurrentTimeStamp();
		sent = false;
		try {
			::sendJsonContent(_pingServerClient, obj);
			sent = true;
		}
		catch (const std::exception&e) {
			pushLogV("ping server failed: %s\n", e.what());
		}
		catch (...) {
			pushLog("ping server failed: unknown error\n");
		}
		if(sent) {
			while (true) {
				auto task = _pingServerClient.receive();
				task.wait();
				t2 = getCurrentTimeStamp();

				auto msg = task.get();

				auto bodyText = msg.extract_string().get();

				stringstream sstream(bodyText);
				std::error_code errorCode;
				auto value = json::value::parse(sstream, errorCode);

				if (errorCode.value()) {
					pushLog("Parse response error\n");
				}
				if (value.is_object()) {
					if (value.has_field(U("event"))) {
						auto& eventVal = value[U("event")];
						if (eventVal.is_string() && eventVal.as_string() == U("pong")) {

							//asume that sending time from client to server equal to
							//response time from server to client
							// then we can calculate coressponding time of server and client
							auto localTime = t1 + (t2 - t1) / 2;
							auto serverTime = value[U("ts")].as_number().to_int64();

							_timeDiff = serverTime - localTime;
							_serverTimeIsReady = true;

							//char buffer[64];
							//formatTime(buffer, sizeof(buffer), serverTime);
							//pushLog(buffer);
							//pushLog("\n");

							//formatTime(buffer, sizeof(buffer), localTime);
							//pushLog(buffer);
							//pushLog("\n");
							
							break;
						}
					}
				}
			}
		}
		t2 = getCurrentTimeStamp();

		sleepTime = 0;
		processTime = t2 - t1;
		if (processTime < interval) {
			sleepTime = (unsigned int)(interval - processTime);
		}
	} while (_stopLoopTask.waitSignal(temp, sleepTime) == false);
}

void BFXTradingPlatform::startServerTimeQuery(TIMESTAMP updateInterval) {
	setQueryTimeInterval(updateInterval);

	_pingServerClient.connect(U("wss://api.bitfinex.com/ws/2")).wait();
	_pingServerTask = std::async(std::launch::async, [this]() { pingServerLoop(); });
}

TIMESTAMP BFXTradingPlatform::getSyncTime(TIMESTAMP localTime) {
	return (localTime + _timeDiff);
}

bool BFXTradingPlatform::isServerTimeReady() {
	return _serverTimeIsReady;
}

inline bool isMaitenanceMode(const json::array& response) {
	if (response.size() != 3) {
		return false;
	}

	if (response.at(0).is_string() && response.at(1).is_integer() && response.at(2).is_string()) {
		if (response.at(0).as_string() == U("error") && response.at(1).as_integer() == 20060) {
			return true;
		}
	}

	return false;
}

void BFXTradingPlatform::getTradeHistory(const char* pair, TIMESTAMP duration, TIMESTAMP endTime, TradingList& tradeItems) {
	bool temp;

	TIMESTAMP t1, t2, additionalTime;
	TIMESTAMP processTime;
	TIMESTAMP interval = 4*1000;
	unsigned int sleepTime = 0;
	bool shouldStop = false;

	std::map<TRADE_ID, bool> existingTrades;

	auto parseTrades = [this, &tradeItems, &duration, &existingTrades, &shouldStop, &additionalTime](http_response response) {
		additionalTime = 0;
		if (response.status_code() == 200) {
			auto js = response.extract_json().get();
			if (js.is_array()) {
				auto& items = js.as_array();

				if (!isMaitenanceMode(items)) {

					TIMESTAMP lastTimeStamp = 0;
					if (tradeItems.size()) {
						lastTimeStamp = tradeItems.back().timestamp;
					}
					
					for (auto it = items.begin(); it != items.end(); it++) {

						auto& itemElm = it->as_array();

						TradeItem tradeItem;
						tradeItem.oderId = itemElm[0].as_number().to_uint64();
						tradeItem.timestamp = itemElm[1].as_number().to_uint64();
						// skip existing trade items
						if (lastTimeStamp == tradeItem.timestamp && existingTrades.find(tradeItem.oderId) != existingTrades.end() ){
							//pushLogV("skip trade item %lld\n", tradeItem.oderId);
							if (items.size() == 1) {
								//pushLog("there is only one item\n");
								shouldStop = true;
							}
							continue;
						}

						tradeItem.amount = itemElm[2].as_double();
						tradeItem.price = itemElm[3].as_double();

						tradeItems.push_back(tradeItem);

						if (tradeItems.size() > 1) {
							if (tradeItems.front().timestamp - tradeItems.back().timestamp >= duration) {
								return;
							}
						}
					}
				}
			}
			else {
				pushLogV("unknow response format %s\n", CPPREST_FROM_STRING(js.as_string()).c_str());
			}
		}
		else {
			pushLog("query trade history failed, try again in one minute\n");
			additionalTime = 60 * 1000;
		}
	};

	TIMESTAMP tStart, tEnd, durationLeft = duration;
	utility::string_t parameters;

	if (endTime > 0) {
		tEnd = endTime;
		parameters = U("&end=");
		parameters += TO_STRING_T(tEnd);
	}

	utility::string_t baseURL = U("https://api.bitfinex.com/v2/trades/t");
	baseURL.append(CPPREST_TO_STRING(pair));
	baseURL.append(U("/hist?limit=1000"));

	while(durationLeft > 0)
	{
		t1 = getCurrentTimeStamp();
		try {
			http_client client(baseURL + parameters);
			auto task = client.request(methods::GET).then(parseTrades);
			task.wait();
		}
		catch (const std::exception&e) {
			pushLogV("ping server failed: %s\n", e.what());
		}
		catch (...) {
			pushLog("ping server failed: unknown error\n");
		}
		
		if (tradeItems.size() == 0 || shouldStop) {
			return;
		}

		tStart = tradeItems.back().timestamp;
		tEnd = tradeItems.front().timestamp;
		durationLeft = duration - (tEnd - tStart);

		tEnd = tStart;
		tStart = tEnd - durationLeft;

		parameters = U("&start=");
		parameters += TO_STRING_T(tStart);
		parameters += U("&end=");
		parameters += TO_STRING_T(tEnd);

		//pushLogV("request trade from %s to %s\n", Utility::time2str(tStart).c_str(), Utility::time2str(tEnd).c_str());

		t2 = getCurrentTimeStamp();

		sleepTime = 0;
		processTime = t2 - t1;
		if (processTime < interval) {
			sleepTime = (unsigned int)(interval - processTime);
		}
		if (_stopLoopTask.waitSignal(temp, sleepTime + (unsigned int)additionalTime)) {
			break;
		}

		existingTrades.clear();
		t2 = tradeItems.back().timestamp;
		for (auto it = tradeItems.head(); it; it = it->nextNode) {
			if (it->value.timestamp == t2) {
				existingTrades[it->value.oderId] = true;
			}
		}
	}
}

extern "C" {
	__declspec(dllexport) TradingPlatform* createPlatformInstance() {
		return new BFXTradingPlatform();
	}
	__declspec(dllexport) void deletePlatformInstance(TradingPlatform* platform) {
		delete platform;
	}
}