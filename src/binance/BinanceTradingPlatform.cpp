#include "BinanceTradingPlatform.h"

#include "Utility.h"

#include <string>
#include <locale>
#include <codecvt>

#include <iostream>
using namespace std;
using namespace web;
using namespace web::websockets::client;


BinanceTradingPlatform::BinanceTradingPlatform() : _timeDiff(0), _stopLoopTask(true)
{
	resetClient();
}

BinanceTradingPlatform::~BinanceTradingPlatform()
{
	disconnect();
}

void BinanceTradingPlatform::messageHandlerImpl(const websocket_incoming_message& msg) {
	auto bodyText = msg.extract_string().get();

	stringstream sstream(bodyText);
	std::error_code errorCode;

	//{"stream":"<streamName>", "data" : <rawPayload>}
	auto eventStream = json::value::parse(sstream, errorCode);

	if (errorCode.value()) {
		pushLog(LogLevel::Debug, "Parse response error\n");
		return;
	}

	if (eventStream.is_object() == false) {
		pushLog(LogLevel::Debug, "wrong format\n");
		return;
	}
	if (!eventStream.has_field(U("stream")) || !eventStream.has_field(U("data"))) {
		pushLogV(LogLevel::Debug, "unknown event %s\n", bodyText.c_str());
		return;
	}

	auto value = eventStream[U("data")];

	if (value.is_object() == false) {
		pushLog(LogLevel::Debug, "wrong format\n");
		return;
	}

	if (!value.has_field(U("e")) || !value.has_field(U("s"))) {
		pushLogV(LogLevel::Debug, "unknown event %s\n", bodyText.c_str());
		return;
	}
	auto e = value[U("e")];
	auto s = value[U("s")];
	if (!e.is_string() || !s.is_string()) {
		pushLog(LogLevel::Debug, "wrong format\n");
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
		pushLogV(LogLevel::Debug, "Cannot find handler for the symbol %s\n", pair.c_str());
	}
}

bool BinanceTradingPlatform::connectImpl() {
	//_client->connect(U("wss://stream.binance.com:9443/ws/ethusdt@aggTrade")).wait();
	//_client->connect(U("wss://stream.binance.com:9443/stream?streams=ethusdt@aggTrade/ethbtc@aggTrade/ethusdt@ticker")).wait();

	int nHandler = getHandlerCount();
	vector<MarketEventHandler*> handlers(nHandler);
	getHandlers(handlers.data(), nHandler);

	list<utility::string_t> streams;
	int sc = 0;

	auto addStream = [&streams, &sc](const utility::string_t& stream) {
		if (sc % 200 == 0) {
			streams.push_back(stream);
		}
		else {
			streams.back() += stream;
		}
		sc++;
	};

	for (auto it = handlers.begin(); it != handlers.end(); it++) {
		auto handler = *it;;
		utility::string_t tPair = CPPREST_TO_STRING(handler->getPair());
		std::transform(tPair.begin(), tPair.end(), tPair.begin(),tolower);

		if (handler->useTicker()) {
			addStream(tPair + U("@ticker/"));
		}
		if (handler->useOrderBook()) {
			addStream(tPair + U("@depth/"));
		}
		if (handler->useTrades()) {
			addStream(tPair + U("@trade/"));
		}
		if (handler->useCandles()) {
			addStream(tPair + U("@kline_1m/"));
		}
	}

	auto settingFile = getConfigFilePath();
	try {
		ifstream in;
		in.open(settingFile);
		if (in.is_open()) {
			auto settings = web::json::value::parse(in);
			_apiKey = CPPREST_FROM_STRING(settings[U("apiKey")].as_string());

			if (settings.has_field(U("timeToRestart"))) {
				_timeToRestart = settings[U("timeToRestart")].as_integer();
			}
			else {
				// default time to restart is 18 hours
				_timeToRestart = 18 * 3600;
			}
		}
		else {
			pushLogV(LogLevel::Error, "load setting file %s failed\n", settingFile);
		}
	}
	catch (...) {
		pushLogV(LogLevel::Error, "load setting file %s failed\n", settingFile);
	}

	if (_apiKey.empty()) {
		pushLogV(LogLevel::Error, "trade history request won't work due to missing api key in setting file %s\n", settingFile);
	}

	auto sit = streams.begin();

	_client->connect(U("wss://stream.binance.com:9443/stream?streams=") + *sit).wait();
	
	_additionalClients.clear();
	for (sit++; sit != streams.end(); sit++) {
		auto client = std::make_shared<websocket_callback_client>();
		client->set_message_handler(std::bind(&BinanceTradingPlatform::messageHandler, this, std::placeholders::_1));
		_additionalClients.push_back(client);

		client->connect(U("wss://stream.binance.com:9443/stream?streams=") + *sit).wait();
	}

	_maintenanceConnectionTask = std::async(std::launch::async, [this]() { maintanceConnection(); });

	return true;
}

bool BinanceTradingPlatform::disconnectImpl() {
	_client->close();

	for (auto it = _additionalClients.begin(); it != _additionalClients.end(); it++) {
		auto client = *it;
		client->close();
	}

	_stopLoopTask.sendSignal(true);
	_pingServerTask.wait();
	_maintenanceConnectionTask.wait();
	return true;
}

//void BinanceTradingPlatform::resetClientNonSync() {
//	CPPRESTBaseTradingPlatformThreadSafe::resetClientNonSync();
//}
//
//void BinanceTradingPlatform::resetClientMessageHandlerNonSync() {
//	CPPRESTBaseTradingPlatformThreadSafe::resetClientMessageHandlerNonSync();
//}


bool BinanceTradingPlatform::addEventHandler(MarketEventHandler* handler, bool allowDelete) {
	if (getConnectionStatus()) {
		pushLog(LogLevel::Debug, "event handler must be added before connect to the platform server\n");
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
	pushLog(LogLevel::Debug, "BinanceTradingPlatform is not support this method, use MarketEventHandler properites instead\n");
	return SubcribeStatus::NA;
}

SubcribeStatus BinanceTradingPlatform::subscribeBookImpl(const char* pair) {
	pushLog(LogLevel::Debug, "BinanceTradingPlatform is not support this method, use MarketEventHandler properites instead\n");
	return SubcribeStatus::NA;
}

SubcribeStatus BinanceTradingPlatform::subscribeTradeImpl(const char* pair) {
	pushLog(LogLevel::Debug, "BinanceTradingPlatform is not support this method, use MarketEventHandler properites instead\n");
	return SubcribeStatus::NA;
}

SubcribeStatus BinanceTradingPlatform::subscribeCandleImpl(const char* pair) {
	pushLog(LogLevel::Debug, "BinanceTradingPlatform is not support this method, use MarketEventHandler properites instead\n");
	return SubcribeStatus::NA;
}

SubcribeStatus BinanceTradingPlatform::unsubscribeTickerImpl(const char* pair) {
	pushLog(LogLevel::Debug, "BinanceTradingPlatform is not support unsubcribe method, try using MarketEventHandler properites and reconnect instead\n");
	return SubcribeStatus::NA;
}
SubcribeStatus BinanceTradingPlatform::unsubscribeBookImpl(const char* pair) {
	pushLog(LogLevel::Debug, "BinanceTradingPlatform is not support unsubcribe method, try using MarketEventHandler properites and reconnect instead\n");
	return SubcribeStatus::NA;
}
SubcribeStatus BinanceTradingPlatform::unsubscribeTradeImpl(const char* pair) {
	pushLog(LogLevel::Debug, "BinanceTradingPlatform is not support unsubcribe method, try using MarketEventHandler properites and reconnect instead\n");
	return SubcribeStatus::NA;
}
SubcribeStatus BinanceTradingPlatform::unsubscribeCandleImpl(const char* pair) {
	pushLog(LogLevel::Debug, "BinanceTradingPlatform is not support unsubcribe method, try using MarketEventHandler properites and reconnect instead\n");
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
	pushLog(LogLevel::Debug, "parsing book event has been not completed yet\n");
}

void BinanceTradingPlatform::invokeBookEventUpdate(MarketEventHandler* handler, web::json::value& value) {
	pushLog(LogLevel::Debug, "parsing book event has been not completed yet\n");
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
	candleItem.volume = _ttod(kLine[U("v")].as_string().c_str());
	candleItem.timestamp = kLine[U("t")].as_number().to_uint64();
	// candle duration was set on subscribe's request
	candleItem.duration = 60;

	handler->onCandlesUpdate(&candleItem, 1, false);
}

using namespace web::http;                  // Common HTTP functionality
using namespace web::http::client;          // HTTP client features

void BinanceTradingPlatform::getAllPairs(StringList& pairs) {
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
			for (auto it = prices.begin(); it != prices.end(); it++) {
				auto& elm = *it;
				if (!elm.is_object()) {
					pushLog(LogLevel::Debug, "price element is not an object\n");
					continue;
				}
				
				if (!elm.has_field(U("symbol"))) {
					pushLog(LogLevel::Debug, "price element is wrong format\n");
					continue;
				}

				auto symbolVal = elm[U("symbol")];
				if (!symbolVal.is_string()) {
					pushLog(LogLevel::Debug, "price element is wrong format\n");
					continue;
				}

				auto symbol_t = elm[U("symbol")].as_string();
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

void BinanceTradingPlatform::getBaseCurrencies(StringList& currencies) {
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
	
	TIMESTAMP timeDiffTotal = 0;
	int pingCount = 0;

	do
	{
		t1 = getCurrentTimeStamp();
		if (pingCount >= 300) {
			// take 300 ping is enough to have a corrected time diff
			break;
		}

		bonusTime = 0;
		sent = false;
		try {
			auto task = _restClient->request(httpRequest).then([&t1, &t2, &bonusTime, &timeDiffTotal, &pingCount, this](http_response& response) {
				t2 = getCurrentTimeStamp();

				//asume that sending time from client to server equal to
				//response time from server to client
				// then we can calculate coressponding time of server and client
				auto localTime = t1 + (t2 - t1) / 2;

				if (response.status_code() == 200) {
					auto js = response.extract_json().get();
					if (js.has_field(U("serverTime")) && js[U("serverTime")].is_integer()) {
						pingCount++;

						auto serverTime = js[U("serverTime")].as_number().to_int64();
						timeDiffTotal += serverTime - localTime;
						_timeDiff = (TIMESTAMP)(timeDiffTotal * 1.0 / pingCount);

						_serverTimeIsReady = true;
					}
					else {
						pushLogV(LogLevel::Error, "unknow response format %s\n", CPPREST_FROM_STRING(js.as_string()).c_str());
					}
				}
				else if (response.status_code() == 429) {
					pushLog(LogLevel::Error, "The API has reached the limit, pause to the next minutes\n");
					bonusTime = 60000;
				}
				else if (response.status_code() == 418) {
					pushLog(LogLevel::Error, "The client's IP has been banned by Binance\n");
				}
				else {
					pushLog(LogLevel::Error, "Ping server failed\n");
				}
			});

			task.wait();
		}
		catch (const std::exception&e) {
			pushLogV(LogLevel::Error, "ping server failed: %s\n", e.what());
		}
		catch (...) {
			pushLog(LogLevel::Error, "ping server failed: unknown error\n");
		}
		
		t2 = getCurrentTimeStamp();

		sleepTime = 0;
		processTime = t2 - t1;
		if (processTime < interval) {
			sleepTime = (unsigned int)(interval - processTime);
		}
	} while (_stopLoopTask.waitSignal(temp, sleepTime + bonusTime) == false);
}

void BinanceTradingPlatform::maintanceConnection() {
	LOG_SCOPE_ACCESS(getLogger(), __FUNCTION__);
	auto timeToRestartInMilisecond = (unsigned int)_timeToRestart * 1000;
	// wait 18h to restart connection due to a requirement from binance.
	// A single connection to stream.binance.com is only valid for 24 hours
	bool signal;
	bool waitRes = _stopLoopTask.waitSignal(signal, timeToRestartInMilisecond);

	if (_restartServerTask.valid()) {
		pushLog(LogLevel::Info, "waiting to previous restart task to finish...\n");
		_restartServerTask.wait();
		pushLog(LogLevel::Info, "waiting to previous restart task to finish completed!\n");
	}

	// check event is time out
	// it means no restart event during start the thread and now it's time to restart the connections
	if (waitRes == false) {
		pushLog(LogLevel::Info, "raise restart events\n");
		_restartServerTask = std::async(std::launch::async, [this]() {
			pushLog(LogLevel::Info, "excuting restart events\n");
			// store all current connection, because they are going to be closed and re-initialzed
			// but re-innitialized on a closing connection may make program hang up
			_inactiveClients.push_back(_client);
			for (auto it = _additionalClients.begin(); it != _additionalClients.end(); it++) {
				_inactiveClients.push_back(*it);
			}

			pushLog(LogLevel::Info, "excuting disconnect\n");
			// disconnect connections only
			// dont use disconnect because it will reset other state
			_client->close();
			for (auto it = _additionalClients.begin(); it != _additionalClients.end(); it++) {
				auto client = *it;
				client->close();
			}
			// wait the maintenance task completed
			// because it will be reset in connectImpl method
			_maintenanceConnectionTask.wait();

			// create new web socket client and set event handlers
			resetClientNonSync();

			pushLog(LogLevel::Info, "excuting reconnect\n");
			// reconnect connections
			connect();
		});
	}
}

void BinanceTradingPlatform::startServerTimeQuery(TIMESTAMP updateInterval) {
	setQueryTimeInterval(updateInterval);
	_pingServerTask = std::async(std::launch::async, [this]() { pingServerLoop(); });
}

void BinanceTradingPlatform::safeRequest(web::http::client::http_client& client, const std::function<void(web::http::http_response&)>& f) {
	client.request(methods::GET).then([&f, this](http_response& response) {
		if (response.status_code() == 200) {
			f(response);
		}
		else if (response.status_code() == 429) {
			pushLog(LogLevel::Error, "The API has reached the limit, pause to the next minutes\n");
		}
		else if (response.status_code() == 418) {
			pushLog(LogLevel::Error, "The client's IP has been banned by Binance\n");
		}
		else {
			pushLog(LogLevel::Error, "request failed\n");
		}
	});
}

void BinanceTradingPlatform::getTradeHistory(const char* pair, TIMESTAMP duration, TIMESTAMP endTime, TradingList& tradeItems) {
	auto tEnd = endTime;
	auto startTime = (endTime - duration);

	bool stopSignal = false;
	TIMESTAMP t1, t2;
	TIMESTAMP processTime;
	unsigned int bonusTime = 0, sleepTime = 0;

	ORDER_ID lastTradeId = 0;
	auto parseAggTrade = [this, &lastTradeId, &bonusTime](http_response& response) {
		bonusTime = 0;
		if (response.status_code() == 200) {
			auto js = response.extract_json().get();
			if (js.is_array() == false) {
				pushLog(LogLevel::Error, "unexpected trade history format\n");
			}
			else {
				auto& jsArray = js.as_array();
				if (jsArray.size()) {
					auto& firstElm = jsArray.at(0);
					if (firstElm.is_object() == false || firstElm.has_field(U("l")) == false) {
						pushLog(LogLevel::Error, "unexpected trade history format\n");
					}
					else {
						lastTradeId = firstElm[U("l")].as_number().to_uint64();
					}
				}
			}
		}
		else if (response.status_code() == 429) {
			pushLog(LogLevel::Error, "The API has reached the limit, pause to the next minutes\n");
			bonusTime = 60 * 1000;
		}
		else if (response.status_code() == 418) {
			pushLog(LogLevel::Error, "The client's IP has been banned by Binance\n");
		}
		else {
			pushLog(LogLevel::Error, "request failed\n");
		}
	};

	utility::string_t baseURL = U("https://api.binance.com/api/v1/aggTrades");
	utility::string_t parameters;

	// check trade history for each 30 minutes
	constexpr TIMESTAMP period = 30 * 60 * 1000;
	decltype(tEnd) tStart;

	http_request marketDataRequest(methods::GET);
	auto& headers = marketDataRequest.headers();
	headers.add(U("X-MBX-APIKEY"), _apiKey.c_str());

	do
	{
		tStart = tEnd - period;

		parameters = U("?symbol=");
		parameters += CPPREST_TO_STRING(pair);
		parameters += U("&startTime=");
		parameters += TO_STRING_T(tStart);
		parameters += U("&endTime=");
		parameters += TO_STRING_T(tEnd);
		parameters += U("&limit=1");

		bonusTime = 0;

		try {
			http_client client(baseURL + parameters);
			client.request(marketDataRequest).then(parseAggTrade).wait();
		}
		catch (const std::exception&e) {
			pushLogV(LogLevel::Error, "get trade history failed: %s\n", e.what());
		}
		catch (...) {
			pushLog(LogLevel::Error, "get trade history failed: unknown error\n");
		}

		tEnd = tStart;
	} while (lastTradeId > 0 && tEnd > startTime  && _stopLoopTask.waitSignal(stopSignal, 100 + bonusTime));

	if (stopSignal == true) {
		return;
	}

	if (lastTradeId == 0) {
		pushLog(LogLevel::Error, "get trade history failed: no trade history found\n");
		return;
	}

	constexpr int requestLimit = 1200;
	constexpr int weight = 5;
	constexpr int numberOfRequestPerMinMax = requestLimit / weight;

	baseURL = U("https://api.binance.com/api/v1/historicalTrades");

	auto toId = lastTradeId;
	auto limit = 500;
	bool shouldStop = false;

	auto parseTrade = [this, &tradeItems, startTime, &bonusTime, &shouldStop](http_response& response) {
		if (response.status_code() == 200) {
			auto js = response.extract_json().get();
			if (js.is_array() == false) {
				pushLog(LogLevel::Error, "unexpected trade history format\n");
			}
			else {
				auto& jsArray = js.as_array();
				for (auto it = jsArray.rbegin(); it != jsArray.rend(); it++) {
					auto& firstElm = *it;
					if (firstElm.is_object() == false) {
						pushLog(LogLevel::Error, "unexpected trade history format\n");
					}
					else {
						/*
						[
							{
								"id": 28457,
								"price": "4.00000100",
								"qty": "12.00000000",
								"time": 1499865549590,
								"isBuyerMaker": true,
								"isBestMatch": true
							}
						]
						*/
						TradeItem tradeItem;
						tradeItem.oderId = firstElm[U("id")].as_number().to_uint64();
						tradeItem.price = _ttod(firstElm[U("price")].as_string().c_str());
						tradeItem.amount = _ttod(firstElm[U("qty")].as_string().c_str());
						tradeItem.timestamp = firstElm[U("time")].as_number().to_int64();

						// only get trade item from start time to end time
						if (tradeItem.timestamp < startTime) {
							shouldStop = true;
							break;
						}

						bool isBuyMaker = firstElm[U("isBuyerMaker")].as_bool();
						if (!isBuyMaker) {
							tradeItem.amount = -tradeItem.amount;
						}

						tradeItems.push_back(tradeItem);
					}
				}
				if (jsArray.size() == 0) {
					shouldStop = true;
				}
			}
		}
		else if (response.status_code() == 429) {
			pushLog(LogLevel::Error, "The API has reached the limit, pause to the next minutes\n");
			bonusTime = 60 * 1000;
		}
		else if (response.status_code() == 418) {
			pushLog(LogLevel::Error, "The client's IP has been banned by Binance\n");
			shouldStop = true;
		}
		else {
			pushLog(LogLevel::Error, "request failed\n");
		}
	};

	TIMESTAMP interval = 60 * 1000 / (numberOfRequestPerMinMax / 2);

	do
	{
		t1 = getCurrentTimeStamp();
		bonusTime = 0;

		auto fromId = toId - limit - 1;

		parameters = U("?symbol=");
		parameters += CPPREST_TO_STRING(pair);
		parameters += U("&fromId=");
		parameters += TO_STRING_T(fromId);
		parameters += U("&limit=");
		parameters += TO_STRING_T(limit);

		try {
			http_client client(baseURL + parameters);
			client.request(marketDataRequest).then(parseTrade).wait();
		}
		catch (const std::exception&e) {
			pushLogV(LogLevel::Error, "get trade history failed: %s\n", e.what());
		}
		catch (...) {
			pushLog(LogLevel::Error, "get trade history failed: unknown error\n");
		}

		toId = fromId - 1;
		t2 = getCurrentTimeStamp();

		sleepTime = 0;
		processTime = t2 - t1;
		if (processTime < interval) {
			sleepTime = (unsigned int)(interval - processTime);
		}

	} while (shouldStop == false && _stopLoopTask.waitSignal(stopSignal, sleepTime + bonusTime) == false);
}

void BinanceTradingPlatform::getCandleHistory(const char* pair, TIMESTAMP duration, TIMESTAMP endTime, CandleList& candleItems) {
	if (duration <= 0) {
		pushLog(LogLevel::Error, "no need to request candle history\n");
		return;
	}
	auto startTime = (endTime - duration);

	bool stopSignal = false;
	TIMESTAMP t1, t2;
	TIMESTAMP processTime;
	unsigned int bonusTime = 0, sleepTime = 0;

	http_request marketDataRequest(methods::GET);
	auto& headers = marketDataRequest.headers();
	headers.add(U("X-MBX-APIKEY"), _apiKey.c_str());

	constexpr int requestLimit = 1200;
	constexpr int weight = 1;
	constexpr int numberOfRequestPerMinMax = requestLimit / weight;

	utility::string_t baseURL = U("https://api.binance.com/api/v1/klines");

	auto limit = 500;
	bool shouldStop = false;
	bool tryAgain = false;
	constexpr int candleDuration = 60;

	auto parseCandle = [this, &tryAgain, &candleItems, candleDuration, &shouldStop, &bonusTime](http_response& response) {
		if (response.status_code() == 200) {
			auto js = response.extract_json().get();
			if (js.is_array() == false) {
				pushLog(LogLevel::Error, "unexpected trade history format\n");
				tryAgain = true;
			}
			else {
				auto& jsArray = js.as_array();
				for (auto it = jsArray.begin(); it != jsArray.end(); it++) {
					auto& elm = *it;
					if (elm.is_array() == false) {
						pushLog(LogLevel::Error, "unexpected trade history format\n");
					}
					else {
						/*
						[
						  [
							1499040000000,      // Open time
							"0.01634790",       // Open
							"0.80000000",       // High
							"0.01575800",       // Low
							"0.01577100",       // Close
							"148976.11427815",  // Volume
							1499644799999,      // Close time
							"2434.19055334",    // Quote asset volume
							308,                // Number of trades
							"1756.87402397",    // Taker buy base asset volume
							"28.46694368",      // Taker buy quote asset volume
							"17928899.62484339" // Ignore
						  ]
						]
						*/

					 	auto& candleProperties = elm.as_array();

						CandleItem candleItem;
						candleItem.timestamp = candleProperties[0].as_number().to_uint64();
						candleItem.open = _ttod(candleProperties[1].as_string().c_str());
						candleItem.high = _ttod(candleProperties[2].as_string().c_str());
						candleItem.low = _ttod(candleProperties[3].as_string().c_str());
						candleItem.close = _ttod(candleProperties[4].as_string().c_str());
						candleItem.volume = _ttod(candleProperties[5].as_string().c_str());
						// candle duration was set on request
						candleItem.duration = candleDuration;

						candleItems.push_front(candleItem);
					}
				}
				if (jsArray.size() == 0) {
					shouldStop = true;
				}
			}
		}
		else if (response.status_code() == 429) {
			pushLog(LogLevel::Error, "The API has reached the limit, pause to the next minutes\n");
			bonusTime = 60 * 1000;
			tryAgain = true;
		}
		else if (response.status_code() == 418) {
			pushLog(LogLevel::Error, "The client's IP has been banned by Binance\n");
			shouldStop = true;
		}
		else {
			pushLog(LogLevel::Error, "request failed\n");
			tryAgain = true;
		}
	};

	TIMESTAMP interval = 60 * 1000 / (numberOfRequestPerMinMax / 2);
	utility::string_t parameters;

	startTime = startTime / (candleDuration * 1000) * (candleDuration * 1000);

	do
	{
		t1 = getCurrentTimeStamp();

		parameters = U("?symbol=");
		parameters += CPPREST_TO_STRING(pair);
		parameters += U("&interval=1m");
		parameters += U("&limit=");
		parameters += TO_STRING_T(limit);
		parameters += U("&startTime=");
		parameters += TO_STRING_T(startTime);
		parameters += U("&endTime=");
		parameters += TO_STRING_T(endTime);

		bonusTime = 0;
		tryAgain = false;

		try {
			http_client client(baseURL + parameters);
			client.request(marketDataRequest).then(parseCandle).wait();
		}
		catch (const std::exception&e) {
			pushLogV(LogLevel::Error, "get trade history failed: %s\n", e.what());
		}
		catch (...) {
			pushLog(LogLevel::Error, "get trade history failed: unknown error\n");
		}
		if (candleItems.size() && tryAgain == false) {
			startTime = candleItems.front().timestamp + candleDuration * 1000;
		}

		t2 = getCurrentTimeStamp();

		sleepTime = 0;
		processTime = t2 - t1;
		if (processTime < interval) {
			sleepTime = (unsigned int)(interval - processTime);
		}

	} while (shouldStop == false && endTime > startTime && _stopLoopTask.waitSignal(stopSignal, sleepTime + bonusTime) == false);
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