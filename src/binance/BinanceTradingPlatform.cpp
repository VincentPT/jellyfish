#include "BinanceTradingPlatform.h"

#include "Utility.h"

#include <string>
#include <locale>
#include <codecvt>

#include <iostream>
using namespace std;
using namespace web;
using namespace web::websockets::client;


BinanceTradingPlatform::BinanceTradingPlatform() : _timeDiff(0)
{
	resetClient();
}

BinanceTradingPlatform::~BinanceTradingPlatform()
{
	disconnect();
	cout << "deleted BinanceTradingPlatform object" << endl;
}

void BinanceTradingPlatform::messageHandlerImpl(const websocket_incoming_message& msg) {
	auto bodyText = msg.extract_string().get();
	//std::cout << "draw message: " << bodyText << std::endl;

	stringstream sstream(bodyText);
	std::error_code errorCode;

	//{"stream":"<streamName>", "data" : <rawPayload>}
	auto eventStream = json::value::parse(sstream, errorCode);

	if (errorCode.value()) {
		cout << "Parse response error" << endl;
		return;
	}

	if (eventStream.is_object() == false) {
		cout << "wrong format" << endl;
		return;
	}
	if (!eventStream.has_field(U("stream")) || !eventStream.has_field(U("data"))) {
		cout << "unknown event:" << bodyText << endl;
		return;
	}

	auto value = eventStream[U("data")];

	if (value.is_object() == false) {
		cout << "wrong format" << endl;
		return;
	}

	if (!value.has_field(U("e")) || !value.has_field(U("s"))) {
		cout << "unknown event:" << bodyText << endl;
		return;
	}
	auto e = value[U("e")];
	auto s = value[U("s")];
	if (!e.is_string() || !s.is_string()) {
		cout << "wrong format" << endl;
		return;
	}

	auto pair = CPPREST_FROM_STRING(s.as_string());
	auto handler = getHandler(pair.c_str());

	if (handler) {
		auto& eventType = e.as_string();
		if (eventType == U("trade")) {
			invokeTradeEvent(handler, value);
		}
		else if (eventType == U("kline")) {
			invokeCandleEvent(handler, value);
		}
		else if (eventType == U("24hrTicker")) {
			invokeTickerEvent(handler, value);
		}
		else if (eventType == U("depthUpdate")) {
			invokeBookEventUpdate(handler, value);
		}
	}
	else {
		cout << "Cannot find handler for the symbol " << pair << endl;
	}
}

bool BinanceTradingPlatform::connectImpl() {
	//_client->connect(U("wss://stream.binance.com:9443/ws/ethusdt@aggTrade")).wait();
	//_client->connect(U("wss://stream.binance.com:9443/stream?streams=ethusdt@aggTrade/ethbtc@aggTrade/ethusdt@ticker")).wait();

	int nHandler = getHandlerCount();
	vector<MarketEventHandler*> handlers(nHandler);
	getHandlers(handlers.data(), nHandler);


	utility::string_t streams;

	for (auto it = handlers.begin(); it != handlers.end(); it++) {
		auto handler = *it;;
		utility::string_t tPair = CPPREST_TO_STRING(handler->getPair());
		std::transform(tPair.begin(), tPair.end(), tPair.begin(),tolower);

		if (handler->useTicker()) {
			streams += tPair;
			streams += U("@ticker/");
		}
		if (handler->useOrderBook()) {
			streams += tPair;
			streams += U("@depth/");
		}
		if (handler->useTrades()) {
			streams += tPair;
			streams += U("@trade/");
		}
		if (handler->useCandles()) {
			streams += tPair;
			streams += U("@kline_1m/");
		}
	}

	if (streams.size()) {
		streams.erase(streams.size() - 1);
	}
	_client->connect(U("wss://stream.binance.com:9443/stream?streams=") + streams).wait();
	return true;
}

bool BinanceTradingPlatform::disconnectImpl() {
	_client->close().wait();

	_stopPingTask.sendSignal(false);
	_pingServerTask.wait();
	return true;
}

bool BinanceTradingPlatform::addEventHandler(MarketEventHandler* handler, bool allowDelete) {
	if (getConnectionStatus()) {
		cout << "event handler must be added before connect to the platform server" << endl;
		return false;
	}
	std::unique_lock<std::mutex> lk(_eventMutex);
	TradingPlatform::addEventHandler(handler, allowDelete);
	return true;
}

void BinanceTradingPlatform::removeEventHandler(const char* pair) {
	std::unique_lock<std::mutex> lk(_eventMutex);
	TradingPlatform::removeEventHandler(pair);
}

SubcribeStatus BinanceTradingPlatform::subscribeTickerImpl(const char* pair) {
	std::cout << "BinanceTradingPlatform is not support this method, use MarketEventHandler properites instead" << std::endl;
	return SubcribeStatus::NA;
}

SubcribeStatus BinanceTradingPlatform::subscribeBookImpl(const char* pair) {
	std::cout << "BinanceTradingPlatform is not support this method, use MarketEventHandler properites instead" << std::endl;
	return SubcribeStatus::NA;
}

SubcribeStatus BinanceTradingPlatform::subscribeTradeImpl(const char* pair) {
	std::cout << "BinanceTradingPlatform is not support this method, use MarketEventHandler properites instead" << std::endl;
	return SubcribeStatus::NA;
}

SubcribeStatus BinanceTradingPlatform::subscribeCandleImpl(const char* pair) {
	std::cout << "BinanceTradingPlatform is not support this method, use MarketEventHandler properites instead" << std::endl;
	return SubcribeStatus::NA;
}

SubcribeStatus BinanceTradingPlatform::unsubscribeTickerImpl(const char* pair) {
	std::cout << "BinanceTradingPlatform is not support unsubcribe method, try using MarketEventHandler properites and reconnect instead" << std::endl;
	return SubcribeStatus::NA;
}
SubcribeStatus BinanceTradingPlatform::unsubscribeBookImpl(const char* pair) {
	std::cout << "BinanceTradingPlatform is not support unsubcribe method, try using MarketEventHandler properites and reconnect instead" << std::endl;
	return SubcribeStatus::NA;
}
SubcribeStatus BinanceTradingPlatform::unsubscribeTradeImpl(const char* pair) {
	std::cout << "BinanceTradingPlatform is not support unsubcribe method, try using MarketEventHandler properites and reconnect instead" << std::endl;
	return SubcribeStatus::NA;
}
SubcribeStatus BinanceTradingPlatform::unsubscribeCandleImpl(const char* pair) {
	std::cout << "BinanceTradingPlatform is not support unsubcribe method, try using MarketEventHandler properites and reconnect instead" << std::endl;
	return SubcribeStatus::NA;
}

Ticker parseTicker(web::json::value& value) {
	/*
	{
	"e": "24hrTicker",  // Event type
	"E": 123456789,     // Event time
	"s": "BNBBTC",      // Symbol
	"p": "0.0015",      // Price change
	"P": "250.00",      // Price change percent
	"w": "0.0018",      // Weighted average price
	"x": "0.0009",      // Previous day's close price
	"c": "0.0025",      // Current day's close price
	"Q": "10",          // Close trade's quantity
	"b": "0.0024",      // Best bid price
	"B": "10",          // Bid bid quantity
	"a": "0.0026",      // Best ask price
	"A": "100",         // Best ask quantity
	"o": "0.0010",      // Open price
	"h": "0.0025",      // High price
	"l": "0.0010",      // Low price
	"v": "10000",       // Total traded base asset volume
	"q": "18",          // Total traded quote asset volume
	"O": 0,             // Statistics open time
	"C": 86400000,      // Statistics close time
	"F": 0,             // First trade ID
	"L": 18150,         // Last trade Id
	"n": 18151          // Total number of trades
	}
	*/
	Ticker ticker;
	ticker.ask = _ttod(value[U("a")].as_string().c_str());
	ticker.bid = _ttod(value[U("b")].as_string().c_str());
	ticker.high = _ttod(value[U("h")].as_string().c_str());
	ticker.low = _ttod(value[U("l")].as_string().c_str());
	ticker.lastPrice = _ttod(value[U("c")].as_string().c_str());
	ticker.volume = _ttod(value[U("v")].as_string().c_str());
	ticker.timestamp = value[U("E")].as_number().to_uint64();

	return ticker;
}

void BinanceTradingPlatform::invokeTickerEvent(MarketEventHandler* handler, web::json::value& value) {
	Ticker ticker = parseTicker(value);
	handler->onTickerUpdate(ticker);
}

void BinanceTradingPlatform::invokeBookEventSnapshot(MarketEventHandler* handler, web::json::value& value) {
	wcout << value.serialize() << endl;
	cout << "parsing book event has been not completed yet" << endl;
}

void BinanceTradingPlatform::invokeBookEventUpdate(MarketEventHandler* handler, web::json::value& value) {
	wcout << value.serialize() << endl;
	cout << "parsing book event has been not completed yet" << endl;
}


void BinanceTradingPlatform::invokeTradeEvent(MarketEventHandler* handler, web::json::value& value) {
	/*
	{
	"e": "trade",     // Event type
	"E": 123456789,   // Event time
	"s": "BNBBTC",    // Symbol
	"t": 12345,       // Trade ID
	"p": "0.001",     // Price
	"q": "100",       // Quantity
	"b": 88,          // Buyer order Id
	"a": 50,          // Seller order Id
	"T": 123456785,   // Trade time
	"m": true,        // Is the buyer the market maker?
	"M": true         // Ignore.
	}
	*/
	TradeItem tradeItem;
	tradeItem.amount = _ttod(value[U("q")].as_string().c_str());
	tradeItem.oderId = value[U("t")].as_number().to_uint64();
	tradeItem.price = _ttod(value[U("p")].as_string().c_str());
	tradeItem.timestamp = value[U("T")].as_number().to_uint64();

	if (value[U("m")].as_bool() == false) {
		tradeItem.amount = -tradeItem.amount;
	}

	handler->onTradesUpdate(&tradeItem, 1, false);
}

void BinanceTradingPlatform::invokeCandleEvent(MarketEventHandler* handler, web::json::value& value) {
	/*
	{
	"e": "kline",     // Event type
	"E": 123456789,   // Event time
	"s": "BNBBTC",    // Symbol
	"k": {
	"t": 123400000, // Kline start time
	"T": 123460000, // Kline close time
	"s": "BNBBTC",  // Symbol
	"i": "1m",      // Interval
	"f": 100,       // First trade ID
	"L": 200,       // Last trade ID
	"o": "0.0010",  // Open price
	"c": "0.0020",  // Close price
	"h": "0.0025",  // High price
	"l": "0.0015",  // Low price
	"v": "1000",    // Base asset volume
	"n": 100,       // Number of trades
	"x": false,     // Is this kline closed?
	"q": "1.0000",  // Quote asset volume
	"V": "500",     // Taker buy base asset volume
	"Q": "0.500",   // Taker buy quote asset volume
	"B": "123456"   // Ignore
	}
	}
	*/

	web::json::value kLine = value[U("k")];

	CandleItem candleItem;
	candleItem.close = _ttod(kLine[U("c")].as_string().c_str());
	candleItem.high = _ttod(kLine[U("h")].as_string().c_str());
	candleItem.low = _ttod(kLine[U("l")].as_string().c_str());
	candleItem.open = _ttod(kLine[U("o")].as_string().c_str());
	candleItem.volume = _ttod(kLine[U("q")].as_string().c_str());
	candleItem.timestamp = kLine[U("t")].as_number().to_uint64();

	handler->onCandlesUpdate(&candleItem, 1, false);
}

using namespace web::http;                  // Common HTTP functionality
using namespace web::http::client;          // HTTP client features

char* BinanceTradingPlatform::getAllPairs() {
	/// https://api.binance.com
	/// api/v3/ticker/
	// Create http_client to send the request.
	http_client client(U("https://api.binance.com/api/v3/ticker/price"));
	auto task = client.request(methods::GET);
	task.wait();
	auto response = task.get();
	auto code = response.status_code();
	if (code == status_codes::OK) {
		auto task = response.extract_json(true);
		task.wait();
		auto value = task.get();

		if (value.is_object() && value.has_field(U("code")) && value.has_field(U("msg"))) {

			auto& msg = value[U("msg")].as_string();

			string erroreMsg("API error: ");
			erroreMsg.append(CPPREST_FROM_STRING(msg));
			throw std::runtime_error(erroreMsg);
		}
		if (value.is_array()) {
			auto& prices = value.as_array();
			list<string> pairs;
			size_t bufferSize = 0;
			for (auto it = prices.begin(); it != prices.end(); it++) {
				auto& elm = *it;
				if (!elm.is_object()) {
					cout << "price element is not an object" << endl;
					continue;
				}
				
				if (!elm.has_field(U("symbol"))) {
					cout << "price element is wrong format" << endl;
					continue;
				}

				auto symbol = elm[U("symbol")];
				if (!symbol.is_string()) {
					cout << "price element is wrong format" << endl;
					continue;
				}

				pairs.emplace_back(CPPREST_FROM_STRING(symbol.as_string()));

				bufferSize += pairs.back().size() + 1;
			}

			char* bufferPairs = (char*)malloc(bufferSize + 1);
			char* c = bufferPairs;
			for (auto it = pairs.begin(); it != pairs.end(); it++) {
				memcpy_s(c, it->size(), it->data(), it->size());
				c += it->size();
				*c++ = ';';
			}
			*(c-1) = 0;

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

void BinanceTradingPlatform::pingServerLoop() {
	bool temp;
	bool sent;

	TIMESTAMP t1, t2;
	TIMESTAMP processTime;
	TIMESTAMP interval = getQueryTimeInterval();
	unsigned int bonusTime, sleepTime = 0;

	/*
	Request: https://api.binance.com/api/v1/time
	Response:
	{
	"serverTime": 1499827319559
	}
	*/

	http_client_config config;
	// 5 seconds timeout
	std::chrono::duration<long long, std::milli> timeout((long long)5000);
	config.set_timeout(timeout);

	http_request httpRequest(methods::GET);
	auto& headers = httpRequest.headers();
	headers[U("Connection")] = U("keep-alive");

	_restClient = make_shared<http_client>(U("https://api.binance.com/api/v1/time"), config);
	
	do
	{
		t1 = getCurrentTimeStamp();
		bonusTime = 0;
		sent = false;
		try {
			auto task = _restClient->request(httpRequest).then([&t1, &t2, &bonusTime, this](http_response response) {
				t2 = getCurrentTimeStamp();

				//asume that sending time from client to server equal to
				//response time from server to client
				// then we can calculate coressponding time of server and client
				auto localTime = t1 + (t2 - t1) / 2;

				if (response.status_code() == 200) {
					auto js = response.extract_json().get();
					if (js.has_field(U("serverTime")) && js[U("serverTime")].is_integer()) {

						auto serverTime = js[U("serverTime")].as_number().to_int64();

						_timeDiff = serverTime - localTime;
						_serverTimeIsReady = true;
					}
					else {
						pushLogV("unknow response format %s\n", CPPREST_FROM_STRING(js.as_string()).c_str());
					}
				}
				else if (response.status_code() == 429) {
					pushLog("The API has reach the limit, pause to the next minutes\n");
					bonusTime = 60000;
				}
				else if (response.status_code() == 418) {
					pushLog("The client's IP has been banned by Binance\n");
				}
				else {
					pushLog("Ping server failed\n");
				}
			});

			task.wait();
		}
		catch (const std::exception&e) {
			pushLogV("ping server failed: %s\n", e.what());
		}
		catch (...) {
			pushLog("ping server failed: unknown error\n");
		}
		
		t2 = getCurrentTimeStamp();

		sleepTime = 0;
		processTime = t2 - t1;
		if (processTime < interval) {
			sleepTime = (unsigned int)(interval - processTime);
		}
	} while (_stopPingTask.waitSignal(temp, sleepTime + bonusTime) == false);
}

void BinanceTradingPlatform::startServerTimeQuery(TIMESTAMP updateInterval) {
	setQueryTimeInterval(updateInterval);
	_pingServerTask = std::async(std::launch::async, [this]() { pingServerLoop(); });
}

TIMESTAMP BinanceTradingPlatform::getSyncTime(TIMESTAMP localTime) {
	return localTime + _timeDiff;
}

bool BinanceTradingPlatform::isServerTimeReady() {
	return _serverTimeIsReady;
}

extern "C" {
	__declspec(dllexport) TradingPlatform* createPlatformInstance() {
		return new BinanceTradingPlatform();
	}
	__declspec(dllexport) void deletePlatformInstance(TradingPlatform* platform) {
		delete platform;
	}
}