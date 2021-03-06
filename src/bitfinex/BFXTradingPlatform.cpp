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


BFXTradingPlatform::BFXTradingPlatform() : _timeDiff(0), _serverTimeIsReady(false), _stopLoopTask(true), _restartEvents(false) {
	resetClient();
}

BFXTradingPlatform::~BFXTradingPlatform()
{
	disconnect();
	pushLog(LogLevel::Debug, "deleted BFXTradingPlatform object\n");
}

void BFXTradingPlatform::maintenanceConnectionWorker() {
	using namespace std::chrono_literals;
	bool runFlag = true;

	list<shared_ptr<websocket_callback_client>> inactiveClients;

	while (runFlag)
	{
		_restartEvents.waitSignal(runFlag);
		if (runFlag == false) {
			break;
		}

		// after the wait, we own the lock.
		pushLog(LogLevel::Debug, "Received reconnect signal\n");
		// sleep for 3 second and try to reconnect client
		std::this_thread::sleep_for(3s);

		// close current web socket client
		_client->close();

		// cannot unsubscribe events because connection was closed in server side
		// but we need to set event handler to unsubcribe to able to restart the connection of this platform
		int nHandler = getHandlerCount();
		vector<MarketEventHandler*> handlers(nHandler);
		getHandlers(handlers.data(), nHandler);

		pushLog(LogLevel::Debug, "reset event handlers status\n");
		SubcribeStatus status = SubcribeStatus::Unsubcribed;
		for (auto it = handlers.begin(); it != handlers.end(); it++) {
			setEventSubscribed(MARKET_EVENT_TICKER, (*it)->getPair(), status);
			setEventSubscribed(MARKET_EVENT_BOOK, (*it)->getPair(), status);
			setEventSubscribed(MARKET_EVENT_TRADE, (*it)->getPair(), status);
			setEventSubscribed(MARKET_EVENT_CANDLE, (*it)->getPair(), status);
		}

		// store it to temporary list because close operation may need time to complete
		inactiveClients.push_back(_client);
		// create new web socket client and set event handlers
		resetClientNonSync();
		pushLog(LogLevel::Debug, "reconnecting...\n");

		// connect web socket and subcrible market events
		connect();
		pushLog(LogLevel::Debug, "executed reconnection\n");
	}
}

void BFXTradingPlatform::messageHandlerImpl(const websocket_incoming_message& msg) {
	auto bodyText = msg.extract_string().get();

	stringstream sstream(bodyText);
	std::error_code errorCode;
	auto value = json::value::parse(sstream, errorCode);

	if (errorCode.value()) {
		pushLog(LogLevel::Error, "Parse response error\n");
		return;
	}
	if (value.is_object())
	{
		//pushLogV("response message: %s \n", bodyText.c_str());

		if (value.has_field(U("event"))) {
			auto eventValue = value[U("event")].as_string();
			if (eventValue == U("info")) {
				if (value.has_field(U("code")) && value.has_field(U("msg"))) {
					auto code = value[U("code")].as_integer();

					if (code == 20051) {
						pushLog(LogLevel::Debug, "receive reconnect signal\n");
						pushLog(LogLevel::Debug, "sending reconnect signal...\n");
						_restartEvents.sendSignal(true);
					}
					else if (code == 20060) {
						pushLog(LogLevel::Debug, "Entering in Maintenance mode.(it should take 120 seconds at most).\n");
					}
					else if (code == 20061) {
						pushLog(LogLevel::Debug, "Maintenance mode end\n");
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
					pushLog(LogLevel::Debug, "Event is not supported\n");
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
						pushLogV(LogLevel::Debug, "chanel %s is not support\n", Utility::toString(chanel).c_str());
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
					pushLog(LogLevel::Debug, "No event handler for the market event found\n");
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
					pushLog(LogLevel::Debug, "unknown chanel was unsubcribed\n");
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
	LOG_SCOPE_ACCESS(getLogger(), __FUNCTION__);
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
	LOG_SCOPE_ACCESS(getLogger(), __FUNCTION__);
	_restartEvents.sendSignal(false);
	_maintenanceTask.wait();
	pushLog(LogLevel::Debug, "maintenance task stopped\n");

	_stopLoopTask.sendSignal(false);
	_pingServerTask.wait();
	pushLog(LogLevel::Debug, "ping server task stopped\n");

	_client->close();
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
		pushLog(LogLevel::Error, "internal error\n");
		return;
	}

	Ticker& ticker = it->second;

	if (secondElm.is_array() == false) {
		if (secondElm.is_string() == false) {
			pushLog(LogLevel::Error, "Expected result is an array\n");
			return;
		}
		if (secondElm.as_string() != U("hb")) {
			pushLog(LogLevel::Error, "unknown ticker response\n");
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
			pushLog(LogLevel::Error, "Expected result is an array\n");
			return;
		}
		if (secondElm.as_string() != U("hb")) {
			pushLog(LogLevel::Error, "unknown ticker response\n");
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
			pushLog(LogLevel::Error, "Expected result is an array\n");
			continue;
		}

		auto& arr = bookElm.as_array();
		if (arr.size() != 3) {
			pushLog(LogLevel::Error, "book item must contain atleast 3 elements\n");
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
			pushLog(LogLevel::Error, "Expected result is an array\n");
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
		pushLog(LogLevel::Error, "Expected result is an array\n");
		return;
	}

	for (int i = 0; i < (int)secondElm.size(); i++) {
		auto& itemElm = secondElm.at(i);
		if (itemElm.is_array() == false) {
			pushLog(LogLevel::Error, "Expected result is an array\n");
			continue;
		}

		auto& arr = itemElm.as_array();
		if (arr.size() != 4) {
			pushLog(LogLevel::Error, "trade item must contain atleast 4 elements\n");
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
			pushLog(LogLevel::Error, "Expected result is an array\n");
			return;
		}
		if (secondElm.as_string() != U("hb")) {
			pushLog(LogLevel::Error, "unknown ticker response\n");
			return;
		}
		handler->onCandlesUpdate(nullptr, 0, false);
		return;
	}

	if (secondElm.size() > 0 && secondElm.at(0).is_array() == false) {
		auto& itemElm = secondElm.as_array();
		if(itemElm.size() != 6) {
			pushLog(LogLevel::Error, "unknown candle response\n");
			return;
		}
		CandleItem item;
		item.timestamp = itemElm[0].as_number().to_uint64();
		item.open = itemElm[1].as_double();
		item.close = itemElm[2].as_double();
		item.high = itemElm[3].as_double();
		item.low = itemElm[4].as_double();
		item.volume = itemElm[5].as_double();
		// candle duration was set on subscribe's request
		item.duration = 60;
		handler->onCandlesUpdate(&item, 1, false);

		return;
	}

	for (int i = 0; i < (int)secondElm.size(); i++) {
		auto& bookElm = secondElm.at(i);
		if (bookElm.is_array() == false) {
			pushLog(LogLevel::Error, "Expected result is an array\n");
			continue;
		}

		auto& itemElm = bookElm.as_array();
		if (itemElm.size() != 6) {
			pushLog(LogLevel::Error, "book item must contain atleast 6 elements\n");
			continue;
		}
		CandleItem item;
		item.timestamp = itemElm[0].as_number().to_uint64();
		item.open = itemElm[1].as_double();
		item.close = itemElm[2].as_double();
		item.high = itemElm[3].as_double();
		item.low = itemElm[4].as_double();
		item.volume = itemElm[5].as_double();
		// candle duration was set on subscribe's request
		item.duration = 60;
		candles.push_back(item);
	}

	vector<CandleItem> itemarr(candles.size());
	itemarr.assign(candles.begin(), candles.end());

	//wcout << value.to_string() << endl;
	handler->onCandlesUpdate(itemarr.data(), (int)itemarr.size(), true);
}

void BFXTradingPlatform::getAllPairs(StringList& pairs) {
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
			for (auto it = symbols.begin(); it != symbols.end(); it++) {
				auto& elm = *it;
				if (!elm.is_string()) {
					pushLog(LogLevel::Error, "element is not a string\n");
					continue;
				}

				auto symbol_t = elm.as_string();
				std::transform(symbol_t.begin(), symbol_t.end(), symbol_t.begin(), toupper);

				auto symbol = CPPREST_FROM_STRING(symbol_t);
				char* uSymbol = (char*)malloc(symbol.size() + 1);
				memcpy_s(uSymbol, symbol.size(), symbol.c_str(), symbol.size());
				uSymbol[symbol.size()] = 0;

				pairs.push_back(uSymbol);
			}


			return;
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

void BFXTradingPlatform::getBaseCurrencies(StringList& currencies) {
	currencies.push_back("USDT");
	currencies.push_back("BTC");
	currencies.push_back("ETH");
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


	auto pingServerClient = make_unique<websocket_client>();
	pingServerClient->connect(U("wss://api.bitfinex.com/ws/2")).wait();

	TIMESTAMP timeDiffTotal = 0;
	int pingCount = 0;

	do
	{
		t1 = getCurrentTimeStamp();
		sent = false;

		if (pingCount >= 300) {
			// take 300 ping is enough to have a corrected time diff
			break;
		}

		while (sent == false)
		{
			try {
				::sendJsonContent(*pingServerClient, obj);
				sent = true;
			}
			catch (const std::exception&e) {
				pushLogV(LogLevel::Error, "ping server failed: %s\n", e.what());
			}
			catch (...) {
				pushLog(LogLevel::Error, "ping server failed: unknown error\n");
			}
			if (sent == false) {
				pingServerClient = make_unique<websocket_client>();
				pingServerClient->connect(U("wss://api.bitfinex.com/ws/2")).wait();
				pushLog(LogLevel::Error, "attemp pinging server again\n");
			}
		}
		
		if(sent) {
			while (true) {
				try {
					auto task = pingServerClient->receive();
					task.wait();

					t2 = getCurrentTimeStamp();
					auto msg = task.get();

					auto bodyText = msg.extract_string().get();

					stringstream sstream(bodyText);
					std::error_code errorCode;
					auto value = json::value::parse(sstream, errorCode);

					if (errorCode.value()) {
						pushLog(LogLevel::Error, "Parse response error\n");
					}
					if (value.is_object()) {
						if (value.has_field(U("event"))) {
							auto& eventVal = value[U("event")];
							if (eventVal.is_string() && eventVal.as_string() == U("pong")) {

								pingCount++;

								//asume that sending time from client to server equal to
								//response time from server to client
								// then we can calculate coressponding time of server and client
								auto localTime = t1 + (t2 - t1) / 2;
								auto serverTime = value[U("ts")].as_number().to_int64();

								timeDiffTotal += serverTime - localTime;

								_timeDiff = (TIMESTAMP)(timeDiffTotal * 1.0 / pingCount);
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
				catch (const std::exception&e) {
					pushLogV(LogLevel::Error, "ping server failed: %s\n", e.what());
					break;
				}
				catch (...) {
					pushLog(LogLevel::Error, "ping server failed: unknown error\n");
					break;
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
	pushLog(LogLevel::Debug, "ping server stopped!\n");
}

void BFXTradingPlatform::startServerTimeQuery(TIMESTAMP updateInterval) {
	setQueryTimeInterval(updateInterval);

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
	bool tryAgain = false;
	auto parseTrades = [this, &tryAgain, &tradeItems, &duration, &existingTrades, &shouldStop, &additionalTime](http_response& response) {
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
					size_t skippedItem = 0;
					for (auto it = items.begin(); it != items.end(); it++) {

						auto& itemElm = it->as_array();

						TradeItem tradeItem;
						tradeItem.oderId = itemElm[0].as_number().to_uint64();
						tradeItem.timestamp = itemElm[1].as_number().to_uint64();
						// skip existing trade items
						if (lastTimeStamp == tradeItem.timestamp && existingTrades.find(tradeItem.oderId) != existingTrades.end() ){
							skippedItem++;
							//pushLogV("skip trade item %lld\n", tradeItem.oderId);
							if (items.size() == skippedItem) {
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
				pushLogV(LogLevel::Error, "unknow response format %s\n", CPPREST_FROM_STRING(js.as_string()).c_str());
			}
		}
		else {
			pushLog(LogLevel::Error, "query trade history failed, try again in one minute\n");
			additionalTime = 60 * 1000;
			tryAgain = true;
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
		additionalTime = 0;
		tryAgain = false;

		try {
			http_client client(baseURL + parameters);
			auto task = client.request(methods::GET).then(parseTrades);
			task.wait();
		}
		catch (const std::exception&e) {
			pushLogV(LogLevel::Error, "get trade history failed: %s\n", e.what());
		}
		catch (...) {
			pushLog(LogLevel::Error, "get trade history failed: unknown error\n");
		}
		
		if (shouldStop) {
			break;
		}

		if (tradeItems.size() && tryAgain == false) {
			tStart = tradeItems.back().timestamp;
			tEnd = tradeItems.front().timestamp;
			durationLeft = duration - (tEnd - tStart);

			tEnd = tStart;
			tStart = tEnd - durationLeft;

			parameters = U("&start=");
			parameters += TO_STRING_T(tStart);
			parameters += U("&end=");
			parameters += TO_STRING_T(tEnd);
		}
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

		if (tradeItems.size() && tryAgain == false) {
			existingTrades.clear();
			t2 = tradeItems.back().timestamp;
			for (auto it = tradeItems.head(); it; it = it->nextNode) {
				if (it->value.timestamp == t2) {
					existingTrades[it->value.oderId] = true;
				}
			}
		}
	}

	// for testing about restart event only
	//pushLog("sending testing reconnect signal...\n");
	//_restartEvents.sendSignal(true);
}

void BFXTradingPlatform::getCandleHistory(const char* pair, TIMESTAMP duration, TIMESTAMP endTime, CandleList& candleItems) {
	if (duration <= 0) {
		pushLog(LogLevel::Debug, "no need to request candle history\n");
		return;
	}
	bool temp;

	TIMESTAMP t1, t2, additionalTime;
	TIMESTAMP processTime;
	TIMESTAMP interval = 4 * 1000;
	unsigned int sleepTime = 0;
	bool shouldStop = false;

	constexpr int candleDuration = 60;

	std::map<TRADE_ID, bool> existingTrades;
	bool tryAgain = false;
	auto parseTrades = [this, &tryAgain, &candleItems, &duration, &existingTrades, &shouldStop, &additionalTime, candleDuration](http_response& response) {
		additionalTime = 0;
		if (response.status_code() == 200) {
			auto js = response.extract_json().get();
			if (js.is_array()) {
				auto& items = js.as_array();

				if (!isMaitenanceMode(items)) {
					if (items.size()) {
						for (auto it = items.begin(); it != items.end(); it++) {
							auto& itemElm = it->as_array();
							/*
							// response with Section = "hist"
							[
							[ MTS, OPEN, CLOSE, HIGH, LOW, VOLUME ],
							...
							]
							*/
							CandleItem item;
							item.timestamp = itemElm[0].as_number().to_uint64();
							item.open = itemElm[1].as_double();
							item.close = itemElm[2].as_double();
							item.high = itemElm[3].as_double();
							item.low = itemElm[4].as_double();
							item.volume = itemElm[5].as_double();
							// candle duration was set on request
							item.duration = candleDuration;

							candleItems.push_back(item);
						}
					}
					else {
						shouldStop = true;
					}
				}
				else {
					pushLog(LogLevel::Error, "Enter maintenance mode, try again in one minute\n");
					additionalTime = 60 * 1000;
					tryAgain = true;
				}
			}
			else {
				pushLogV(LogLevel::Error, "unknow response format %s\n", CPPREST_FROM_STRING(js.as_string()).c_str());
			}
		}
		else {
			pushLog(LogLevel::Error, "query candle history failed, try again in one minute\n");
			additionalTime = 60 * 1000;
			tryAgain = true;
		}
	};

	TIMESTAMP tStart, tEnd;
	utility::string_t parameters;

	utility::string_t baseURL = U("https://api.bitfinex.com/v2/candles/trade:1m:t");
	baseURL.append(CPPREST_TO_STRING(pair));
	baseURL.append(U("/hist"));

	tEnd = endTime;
	tStart = (endTime - duration) / (candleDuration * 1000) * (candleDuration * 1000);

	do
	{
		t1 = getCurrentTimeStamp();

		parameters = U("?start=");
		parameters += TO_STRING_T(tStart);
		parameters += U("&end=");
		parameters += TO_STRING_T(tEnd);

		additionalTime = 0;
		tryAgain = false;

		try {
			http_client client(baseURL + parameters);
			auto task = client.request(methods::GET).then(parseTrades);
			task.wait();
		}
		catch (const std::exception&e) {
			pushLogV(LogLevel::Error, "get candle history failed: %s\n", e.what());
		}
		catch (...) {
			pushLog(LogLevel::Error, "get candle history failed: unknown error\n");
		}

		if (shouldStop) {
			break;
		}
		if (candleItems.size() && tryAgain == false) {
			tEnd = candleItems.back().timestamp - (candleDuration * 1000);
		}
		t2 = getCurrentTimeStamp();

		sleepTime = 0;
		processTime = t2 - t1;
		if (processTime < interval) {
			sleepTime = (unsigned int)(interval - processTime);
		}
	} while ( tStart < tEnd && _stopLoopTask.waitSignal(temp, sleepTime + (unsigned int)additionalTime) == false);
}

extern "C" {
	__declspec(dllexport) TradingPlatform* createPlatformInstance() {
		return new BFXTradingPlatform();
	}
	__declspec(dllexport) void deletePlatformInstance(TradingPlatform* platform) {
		delete platform;
	}
}