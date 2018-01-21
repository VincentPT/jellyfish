#include "HitBTCTradingPlatform.h"

#include "Utility.h"

#include <string>
#include <locale>
#include <codecvt>

#include <iostream>
#include <cpprest/http_client.h>

using namespace std;

using namespace web;
using namespace web::websockets::client;
using namespace web::http;                  // Common HTTP functionality
using namespace web::http::client;          // HTTP client features


HitBTCTradingPlatform::HitBTCTradingPlatform() : _autoClientId(0)
{
	resetClient();
}

HitBTCTradingPlatform::~HitBTCTradingPlatform()
{
	disconnect();
	cout << "deleted HitBTCTradingPlatform object" << endl;
}

void HitBTCTradingPlatform::messageHandlerImpl(const websocket_incoming_message& msg) {
	auto bodyText = msg.extract_string().get();
	//std::cout << "draw message: " << bodyText << std::endl;

	stringstream sstream(bodyText);
	std::error_code errorCode;
	auto value = json::value::parse(sstream, errorCode);

	if (errorCode.value()) {
		cout << "Parse response error" << endl;
		return;
	}

	if (value.is_object() && value.has_field(U("jsonrpc")) && value[U("jsonrpc")].as_string() == U("2.0")) {
		// "jsonrpc":"2.0","method":"ticker","params"
		if (value.has_field(U("result")) && value.has_field(U("id"))) {
			std::cout << "draw message: " << bodyText << std::endl;

			auto result = value[U("result")].as_bool();
			auto chanelId = value[U("id")].as_integer();
			
			std::unique_lock<std::mutex> lk(_eventMutex);

			auto it = _chanelEventHandlerInfoMap.find(chanelId);
			if (it != _chanelEventHandlerInfoMap.end()) {
				auto& eventHandlerInfo = it->second;
				auto status = isEventSubscribed(eventHandlerInfo.eventName, eventHandlerInfo.handler->getPair());
				if (status == SubcribeStatus::NA || status == SubcribeStatus::Subcribing) {
					setEventSubscribed(eventHandlerInfo.eventName, eventHandlerInfo.handler->getPair(),
						result ? SubcribeStatus::Subcribed : SubcribeStatus::NA);
				}
				else if (status == SubcribeStatus::Subcribed || status == SubcribeStatus::Unsubcribing) {
					setEventSubscribed(eventHandlerInfo.eventName, eventHandlerInfo.handler->getPair(),
						result ? SubcribeStatus::Unsubcribed : SubcribeStatus::Subcribed);
				}
			}
			else {
				cout << "Cannot find associate event for the result" << endl;
			}
		}
		else if(value.has_field(U("method")) && value.has_field(U("params"))) {
			auto params = value[U("params")];
			auto method = value[U("method")].as_string();
		
			if (params.is_object() && params.has_field(U("symbol"))) {
				auto pair = CPPREST_FROM_STRING(params[U("symbol")].as_string());

				std::unique_lock<std::mutex> lk(_eventMutex);
				auto handler = getHandler(pair.c_str());
				if (handler == nullptr) {
					cout << "No handler found for event ";
					wcout << method << endl;
				}

				if (method == U("ticker")) {
					invokeTickerEvent(handler, params);
				}
				else if (method == U("snapshotOrderbook")) {
					invokeBookEventSnapshot(handler, params);
				}
				else if (method == U("updateOrderbook")) {
					invokeBookEventUpdate(handler, params);
				}
				else if (method == U("snapshotTrades")) {
					invokeTradeEvent(handler, params, true);
				}
				else if (method == U("updateTrades")) {
					invokeTradeEvent(handler, params, false);
				}
				else if (method == U("snapshotCandles") ) {
					invokeCandleEvent(handler, params, true);
				}
				else if (method == U("updateCandles")) {
					invokeCandleEvent(handler, params, false);
				}
			}
		}
	}
}

bool HitBTCTradingPlatform::connectImpl() {
	_client->connect(U("wss://api.hitbtc.com/api/2/ws")).wait();

	// subcribe events for already exits handlers
	int nHandler = getHandlerCount();
	vector<MarketEventHandler*> handlers(nHandler);
	getHandlers(handlers.data(), nHandler);

	for (auto it = handlers.begin(); it != handlers.end(); it++) {
		subcribeEventForHandler(*it);
	}

	return true;
}

bool HitBTCTradingPlatform::disconnectImpl() {
	_client->close().wait();
	return true;
}

bool HitBTCTradingPlatform::addEventHandler(MarketEventHandler* handler, bool allowDelete) {
	std::unique_lock<std::mutex> lk(_eventMutex);

	if (TradingPlatform::addEventHandler(handler, allowDelete)) {
		if (getConnectionStatus()) {
			subcribeEventForHandler(handler);
		}
	}

	return true;
}

void HitBTCTradingPlatform::removeEventHandler(const char* pair) {
	std::unique_lock<std::mutex> lk(_eventMutex);
	TradingPlatform::removeEventHandler(pair);
}

SubcribeStatus HitBTCTradingPlatform::subscribeEventImpl(const char* eventName, const char* pair, const utility::string_t& method, bool isCandle) {
	json::value obj;
	utility::string_t tpair = CPPREST_TO_STRING(pair);

	json::value params;
	params[U("symbol")] = json::value::string(tpair);
	if (isCandle) {
		// request update candle for each 1 minute
		params[U("period")] = json::value::string(U("M1"));
	}

	obj[U("method")] = json::value::string(method);
	obj[U("params")] = params;
	obj[U("id")] = json::value(++_autoClientId);

	sendJsonContent(obj);

	string key(eventName);
	key += pair;
	_chanelEventMap[key] = _autoClientId;

	auto handler = getHandler(pair);
	if (handler) {
		EventHandlerInfo eventHandlerInfo;
		eventHandlerInfo.eventName = eventName;
		eventHandlerInfo.handler = handler;

		_chanelEventHandlerInfoMap[_autoClientId] = eventHandlerInfo;
	}
	else {
		cout << "warning! cannot find handler for pair" << pair << endl;
	}

	return SubcribeStatus::Subcribing;
}

SubcribeStatus HitBTCTradingPlatform::unsubscribeEventImpl(const char* eventName, const char* pair, const utility::string_t& method, bool isCandle) {
	string key(eventName);
	key += pair;
	auto it = _chanelEventMap.find(key);
	if (it == _chanelEventMap.end()) {
		return isEventSubscribed(eventName, pair);
	}

	int chanelId = it->second;

	json::value obj;
	utility::string_t tpair = CPPREST_TO_STRING(pair);

	json::value params;
	params[U("symbol")] = json::value::string(tpair);
	if (isCandle) {
		// request update candle for each 1 minute
		params[U("period")] = json::value::string(U("M1"));
	}

	obj[U("method")] = json::value::string(method);
	obj[U("params")] = params;
	obj[U("id")] = json::value(chanelId);

	sendJsonContent(obj);

	return SubcribeStatus::Unsubcribing;
}

SubcribeStatus HitBTCTradingPlatform::subscribeTickerImpl(const char* pair) {
	return subscribeEventImpl(MARKET_EVENT_TICKER, pair, U("subscribeTicker"));
}

SubcribeStatus HitBTCTradingPlatform::subscribeBookImpl(const char* pair) {
	return subscribeEventImpl(MARKET_EVENT_BOOK, pair, U("subscribeOrderbook"));
}

SubcribeStatus HitBTCTradingPlatform::subscribeTradeImpl(const char* pair) {
	return subscribeEventImpl(MARKET_EVENT_TRADE, pair, U("subscribeTrades"));
}

SubcribeStatus HitBTCTradingPlatform::subscribeCandleImpl(const char* pair) {
	return subscribeEventImpl(MARKET_EVENT_CANDLE, pair, U("subscribeCandles"), true);
}

SubcribeStatus HitBTCTradingPlatform::unsubscribeTickerImpl(const char* pair) {
	return unsubscribeEventImpl(MARKET_EVENT_TICKER, pair, U("unsubscribeTicker"));
}
SubcribeStatus HitBTCTradingPlatform::unsubscribeBookImpl(const char* pair) {
	return unsubscribeEventImpl(MARKET_EVENT_BOOK, pair, U("unsubscribeOrderbook"));
}
SubcribeStatus HitBTCTradingPlatform::unsubscribeTradeImpl(const char* pair) {
	return unsubscribeEventImpl(MARKET_EVENT_TRADE, pair, U("unsubscribeTrades"));
}
SubcribeStatus HitBTCTradingPlatform::unsubscribeCandleImpl(const char* pair) {
	return unsubscribeEventImpl(MARKET_EVENT_CANDLE, pair, U("unsubscribeCandles"), true);
}

TIMESTAMP TString2Timestamp(const utility::string_t& tstr) {
	//2017-12-31T10:23:00.007Z
	int year, month, day, hour, min, second, milisecond;
	tm time;
	_tscans(tstr.c_str(), U("%4d-%2d-%2dT%2d:%2d:%2d.%3d"),&year,&month,&day,&hour,&min,&second,&milisecond);
	
	time.tm_year = year - 1900;
	time.tm_mday = day;
	time.tm_mon = month - 1;
	time.tm_hour = hour;
	time.tm_min = min;
	time.tm_sec = second;

	time_t t = _mkgmtime(&time);

	TIMESTAMP ts = t;
	ts *= 1000;
	ts += milisecond;

	return ts;
}

void HitBTCTradingPlatform::invokeTickerEvent(MarketEventHandler* handler, web::json::value& value) {
	//"params":{"ask":"690.07","bid":"689.49","last":"689.96","open":"685.79","low":"643.50","high":"707.42","volume":"15724.575","volumeQuote":"10764839.65278","timestamp":"2017-12-31T10:23:00.007Z","symbol":"ETHUSD"}
	Ticker ticker;
	ticker.ask = _ttod(value[U("ask")].as_string().c_str());
	ticker.bid = _ttod(value[U("bid")].as_string().c_str());
	ticker.lastPrice = _ttod(value[U("last")].as_string().c_str());
	ticker.high = _ttod(value[U("high")].as_string().c_str());
	ticker.low = _ttod(value[U("low")].as_string().c_str());
	ticker.volume = _ttod(value[U("volume")].as_string().c_str());
	ticker.timestamp = TString2Timestamp(value[U("timestamp")].as_string());

	handler->onTickerUpdate(ticker);
}

inline BookItem parseBookItemNode(web::json::value& value, const utility::string_t** priceStr) {
	BookItem bookItem;
	auto& priceRefStr = value[U("price")].as_string();
	bookItem.price = _ttod(priceRefStr.c_str());
	bookItem.amount = _ttod(value[U("size")].as_string().c_str());

	*priceStr = &priceRefStr;
	return bookItem;
}

void HitBTCTradingPlatform::invokeBookEventSnapshot(MarketEventHandler* handler, web::json::value& value) {
	string collectionKey = handler->getPair();
	BookIdGeneratorInfo dummy;
	BookIdGeneratorInfo* bookIdGeneratoriInfo;
	auto it = _bookCollectionMap.insert(make_pair(collectionKey, dummy));
	if (it.second == true) {
		dummy.askContainer = make_shared<PriceOrderMap>();
		dummy.bidContainer = make_shared<PriceOrderMap>();
		it.first->second = dummy;
		bookIdGeneratoriInfo = &(it.first->second);
	}
	else {
		bookIdGeneratoriInfo = &(it.first->second);
		bookIdGeneratoriInfo->askContainer->clear();
		bookIdGeneratoriInfo->bidContainer->clear();
	}

	bookIdGeneratoriInfo->nextId = 1;

	ORDER_ID& orderId = bookIdGeneratoriInfo->nextId;
	PriceOrderMap* askContainer = bookIdGeneratoriInfo->askContainer.get();
	PriceOrderMap* bidContainer = bookIdGeneratoriInfo->bidContainer.get();

	if (!value.has_field(U("ask")) || !value.has_field(U("bid"))) {
		cout << "unknown format of order book" << endl;
		return;
	}

	auto ask = value[U("ask")];
	auto bid = value[U("bid")];

	if (!ask.is_array() || !bid.is_array()) {
		cout << "unknown format of order book" << endl;
		return;
	}

	auto& askItems = ask.as_array();
	auto& bidItems = bid.as_array();

	int idx1 = 0;
	int idx2 = 0;
	vector<BookItem> askRawItems(askItems.size());
	vector<BookItem> bidRawItems(bidItems.size());

	const utility::string_t* priceStr;

	for (auto it = askItems.begin(); it != askItems.end(); it++) {
		auto& elm = *it;
		if (elm.is_object() == false) {
			continue;
		}
		auto bookItem = parseBookItemNode(elm, &priceStr);
		bookItem.oderId = orderId;

		askRawItems[idx1++] = bookItem;

		askContainer->insert(make_pair(*priceStr, orderId));
		orderId++;
	}

	for (auto it = bidItems.begin(); it != bidItems.end(); it++) {
		auto& elm = *it;
		if (elm.is_object() == false) {
			continue;
		}
		auto bookItem = parseBookItemNode(elm, &priceStr);
		bookItem.oderId = orderId;

		bidRawItems[idx2++] = bookItem;

		bidContainer->insert(make_pair(*priceStr, orderId));
		orderId++;
	}

	BookItemEventArgs bookEventArgs;
	bookEventArgs.askCount = idx1;
	bookEventArgs.askItems = askRawItems.data();
	bookEventArgs.bidCount = idx2;
	bookEventArgs.bidItems = bidRawItems.data();

	handler->onBooksUpdate(&bookEventArgs, true);
}

void HitBTCTradingPlatform::invokeBookEventUpdate(MarketEventHandler* handler, web::json::value& value) {
	string collectionKey = handler->getPair();
	BookIdGeneratorInfo* bookIdGeneratoriInfo;
	auto it = _bookCollectionMap.find(collectionKey);
	if (it == _bookCollectionMap.end()) {
		cout << "something wrong, order update event occurs before order snapshot event" << endl;
		return;
	}
	
	bookIdGeneratoriInfo = &it->second;

	ORDER_ID& orderId = bookIdGeneratoriInfo->nextId;
	PriceOrderMap* askContainer = bookIdGeneratoriInfo->askContainer.get();
	PriceOrderMap* bidContainer = bookIdGeneratoriInfo->bidContainer.get();

	if (!value.has_field(U("ask")) || !value.has_field(U("bid"))) {
		cout << "unknown format of order book" << endl;
		return;
	}

	auto ask = value[U("ask")];
	auto bid = value[U("bid")];

	if (!ask.is_array() || !bid.is_array()) {
		cout << "unknown format of order book" << endl;
		return;
	}

	auto askItems = ask.as_array();
	auto bidItems = bid.as_array();

	int idx1 = 0;
	int idx2 = 0;
	vector<BookItem> askRawItems(askItems.size());
	vector<BookItem> bidRawItems(bidItems.size());

	BookItem bookItem;
	auto UpdateBookItem =[&orderId,&bookItem](PriceOrderMap* container, web::json::value& elm) -> bool {
		const utility::string_t* priceStr;
		bookItem = parseBookItemNode(elm,&priceStr);
		if (bookItem.amount == 0.0) {
			auto jt = container->find(*priceStr);
			if (jt == container->end()) {
				cout << "update event's parameter is invalid" << endl;
				return false;
			}
			bookItem.oderId = jt->second;
			bookItem.price = 0;
			container->erase(jt);
		}
		else {
			// try to insert new book item
			auto jt = container->insert(make_pair(*priceStr, orderId));
			// check if the item with same price is exist...
			if (jt.second == false) {
				// ...update existing book order
				bookItem.oderId = jt.first->second;
			}
			else {
				// ...then create new one with new id
				bookItem.oderId = orderId;
				jt.first->second = orderId;
				orderId++;
			}
		}

		return true;
	};

	for (auto it = askItems.begin(); it != askItems.end(); it++) {
		auto& elm = *it;
		if (elm.is_object() == false) {
			continue;
		}
		if (UpdateBookItem(askContainer, elm)) {
			askRawItems[idx1++] = bookItem;
		}
	}

	for (auto it = bidItems.begin(); it != bidItems.end(); it++) {
		auto& elm = *it;
		if (elm.is_object() == false) {
			continue;
		}
		if (UpdateBookItem(bidContainer, elm)) {
			bidRawItems[idx2++] = bookItem;
		}
	}

	BookItemEventArgs bookEventArgs;
	bookEventArgs.askCount = idx1;
	bookEventArgs.askItems = askRawItems.data();
	bookEventArgs.bidCount = idx2;
	bookEventArgs.bidItems = bidRawItems.data();

	handler->onBooksUpdate(&bookEventArgs, false);
}


void HitBTCTradingPlatform::invokeTradeEvent(MarketEventHandler* handler, web::json::value& value, bool snapshot) {
	if (value.has_field(U("data")) == false) {
		cout << "unknow format of trade event" << endl;
		return;
	}
	auto data = value[U("data")];
	if(data.is_array() == false) {
		cout << "unknow format of trade event" << endl;
		return;
	}
	auto& tradeArray = data.as_array();

	int idx = 0;
	vector<TradeItem> tradeRawItems(tradeArray.size());
	for (auto it = tradeArray.begin(); it != tradeArray.end(); it++) {
		//{"id":128008449,"price":"690.54","quantity":"0.032","side":"sell","timestamp":"2017-12-31T10:25:40.506Z"}
		auto& elm = *it;
		if (!elm.is_object()) {
			cout << "unknown trade item format" << endl;
			continue;
		}
		TradeItem& tradeItem = tradeRawItems[idx++];
		
		tradeItem.oderId = elm[U("id")].as_number().to_uint64();
		tradeItem.price = _ttod(elm[U("price")].as_string().c_str());
		tradeItem.amount = _ttod(elm[U("quantity")].as_string().c_str());
		tradeItem.timestamp = TString2Timestamp(elm[U("timestamp")].as_string());

		if (elm[U("side")].as_string() == U("sell")) {
			tradeItem.amount = -tradeItem.amount;
		}
	}

	handler->onTradesUpdate(tradeRawItems.data(), (int)tradeRawItems.size(), snapshot);
}

void HitBTCTradingPlatform::invokeCandleEvent(MarketEventHandler* handler, web::json::value& value, bool snapShot) {
	if (value.has_field(U("data")) == false) {
		cout << "unknow format of trade event" << endl;
		return;
	}
	auto data = value[U("data")];
	if (data.is_array() == false) {
		cout << "unknow format of trade event" << endl;
		return;
	}
	auto& candleArray = data.as_array();

	int idx = 0;
	vector<CandleItem> candleRawItems(candleArray.size());

	for (auto it = candleArray.begin(); it != candleArray.end(); it++) {
		auto& elm = *it;
		if (!elm.is_object()) {
			cout << "unknown trade item format" << endl;
			continue;
		}

		//{"timestamp":"2017-12-31T10:00:00.000Z","open":"685.43","close":"690.24","min":"683.81","max":"691.42","volume":"192.536","volumeQuote":"132289.09752"}
		CandleItem& candleItem = candleRawItems[idx++];
		candleItem.open = _ttod(elm[U("open")].as_string().c_str());
		candleItem.close = _ttod(elm[U("close")].as_string().c_str());
		candleItem.high = _ttod(elm[U("max")].as_string().c_str());
		candleItem.low = _ttod(elm[U("min")].as_string().c_str());
		candleItem.volume = _ttod(elm[U("volume")].as_string().c_str());
		candleItem.timestamp = TString2Timestamp(elm[U("timestamp")].as_string());
	}

	handler->onCandlesUpdate(candleRawItems.data(), (int)candleRawItems.size(), snapShot);
}

char* HitBTCTradingPlatform::getAllPairs() {
	http_client client(U("https://api.hitbtc.com/api/2/public/symbol"));
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
				if (!elm.is_object() || !elm.has_field(U("id"))) {
					cout << "Unknow symbol format" << endl;
					continue;
				}

				auto symbolId = elm[U("id")].as_string();
				std::transform(symbolId.begin(), symbolId.end(), symbolId.begin(), toupper);

				pairs.emplace_back(CPPREST_FROM_STRING(symbolId));

				bufferSize += pairs.back().size() + 1;

				if (pairs.size() == 2) {
					break;
				}
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

void HitBTCTradingPlatform::startServerTimeQuery(TIMESTAMP updateInterval) {

}

TIMESTAMP HitBTCTradingPlatform::getSyncTime(TIMESTAMP localTime) {
	return localTime;
}

bool HitBTCTradingPlatform::isServerTimeReady() {
	return false;
}

extern "C" {
	__declspec(dllexport) TradingPlatform* createPlatformInstance() {
		return new HitBTCTradingPlatform();
	}
	__declspec(dllexport) void deletePlatformInstance(TradingPlatform* platform) {
		delete platform;
	}
}