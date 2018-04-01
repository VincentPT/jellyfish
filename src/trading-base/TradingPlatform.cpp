#include "TradingPlatform.h"

#include <string>
#include <locale>
#include <codecvt>
#include <list>
#include <map>
#include <iostream>
#include <memory>
#include <algorithm>
#include <stdarg.h>

using namespace std;

void ILogger::logV(LogLevel logLevel, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	this->logVA(logLevel, fmt, args);
	va_end(args);
}

void ILogger::logVA(LogLevel logLevel, const char* fmt, va_list args) {
	char buffer[1024];
	vsprintf(buffer, fmt, args);
	this->log(logLevel, buffer);
}

class TradingPlatformImpl {
	std::map<std::string, MarketEventHandler*> _pairTraderMap;
	std::map<std::string, SubcribeStatus> _subscriptionMap;
	list<shared_ptr<MarketEventHandler>> _container;
	ILogger* _logger = nullptr;
	string _configFilePath;
public:
	bool addEventHandler(MarketEventHandler* handler, bool allowDelete) {
		auto pair = handler->getPair();
		auto it = _pairTraderMap.insert(std::make_pair(pair, handler));

		if (it.second == false) {
			pushLogV("Symbol %s  is already exist\n", pair);
			return false;
		}

		if (allowDelete) {
			_container.push_back(shared_ptr<MarketEventHandler>(handler));
		}
		return true;
	}

	void removeEventHandler(const std::string& pair) {
		_pairTraderMap.erase(pair);
	}

	void cleanup() {
		for (auto it = _container.begin(); it != _container.end();) {
			if (_pairTraderMap.find((*it)->getPair()) == _pairTraderMap.end()) {
				auto jt = it;
				it++;
				_container.erase(jt);
			}
			else {
				it++;
			}
		}
	}

	MarketEventHandler* getHandler(const string& pair) {
		auto it = _pairTraderMap.find(pair);
		if (it != _pairTraderMap.end()) {
			return it->second;
		}

		return nullptr;
	}

	SubcribeStatus isEventSubscribed(const string& eventName, const string& pair) {
		auto key = eventName + pair;
		auto it = _subscriptionMap.find(key);
		if (it != _subscriptionMap.end()) {
			return it->second;
		}

		return SubcribeStatus::NA;
	}

	void setEventSubscribed(const string& eventName, const string& pair, SubcribeStatus status) {
		auto key = eventName + pair;
		_subscriptionMap[key] = status;
		if (status == SubcribeStatus::Subcribed) {
			cout << eventName << pair << " is subscribed" << endl;
		}
		else if (status == SubcribeStatus::Unsubcribed) {
			cout << eventName << pair << " is unsubscribed" << endl;
		}
		else if (status == SubcribeStatus::Subcribing) {
			cout << eventName << pair << " is subcribing" << endl;
		}
		else if (status == SubcribeStatus::Unsubcribing) {
			cout << eventName << pair << " is unsubcribing" << endl;
		}
		else {
			cout << eventName << pair << "'s subscription status is reset" << endl;
		}
	}

	void removeAllHandlers() {
		_pairTraderMap.clear();
		_container.clear();
	}
	int getHandlerCount() const {
		return (int)_pairTraderMap.size();
	}
	/////////////////////////////////////////////////////////////////////////////////
	/// fill handlers to the buffer
	/// return number of valid handler in the buffer
	/////////////////////////////////////////////////////////////////////////////////
	int getHandlers(MarketEventHandler** handlersBuffer, int maxCount) const {
		int nValid = std::min(maxCount, getHandlerCount());
		MarketEventHandler** p = handlersBuffer;
		auto end = p + nValid;
		for (auto it = _pairTraderMap.begin(); it != _pairTraderMap.end() && p < end; it++) {
			*p++ = it->second;
		}

		return nValid;
	}

	void broadcastServerTime(TIMESTAMP t) {
		for (auto it = _pairTraderMap.begin(); it != _pairTraderMap.end(); it++) {
			it->second->onTimeUpdate(t);
		}
	}


	ILogger* getLogger() const {
		return _logger;
	}

	void setLogger(ILogger* logger) {
		_logger = logger;
	}

	void pushLog(LogLevel logLevel, const char* message) {
		if(_logger) _logger->log(logLevel, message);
	}

	void pushLogV(LogLevel logLevel, const char* fmt, ...) {
		if (_logger) {
			va_list args;
			va_start(args, fmt);
			_logger->logVA(logLevel, fmt, args);
			va_end(args);
		}
	}
	void pushLogVA(LogLevel logLevel, const char* fmt, va_list args) {
		if (_logger) _logger->logVA(logLevel, fmt, args);
	}

	void setConfigFilePath(const char* path) {
		_configFilePath = path;
	}
	const char* getConfigFilePath() const {
		return _configFilePath.c_str();
	}
};


TradingPlatform::TradingPlatform() : _updateInterval(1000)
{
	_tradingPlatformImpl = new TradingPlatformImpl();
}

TradingPlatform::~TradingPlatform()
{
	delete _tradingPlatformImpl;
}

bool TradingPlatform::addEventHandler(MarketEventHandler* handler, bool allowDelete) {
	return _tradingPlatformImpl->addEventHandler(handler, allowDelete);
}

void TradingPlatform::subcribeEventForHandler(MarketEventHandler* handler) {
	if (handler->useTicker()) subscribeTicker(handler->getPair());
	if (handler->useOrderBook()) subscribeBook(handler->getPair());
	if (handler->useTrades()) subscribeTrade(handler->getPair());
	if (handler->useCandles()) subscribeCandle(handler->getPair());
}

void TradingPlatform::unsubcribeEventForHandler(MarketEventHandler* handler) {
	if (handler->useTicker()) unsubscribeTicker(handler->getPair());
	if (handler->useOrderBook()) unsubscribeBook(handler->getPair());
	if (handler->useTrades()) unsubscribeTrade(handler->getPair());
	if (handler->useCandles()) unsubscribeCandle(handler->getPair());
}

MarketEventHandler* TradingPlatform::getHandler(const char* pair) {
	return _tradingPlatformImpl->getHandler(pair);
}

void TradingPlatform::removeEventHandler(const char* pair) {
	_tradingPlatformImpl->removeEventHandler(pair);
}

void TradingPlatform::cleanup() {
	_tradingPlatformImpl->cleanup();
}

void TradingPlatform::subscribeTicker(const char* pair) {
	auto status = isEventSubscribed(MARKET_EVENT_TICKER, pair);
	if (IS_STATUS_NOT_SUBSCRIBED(status)) {
		status = subscribeTickerImpl(pair);
		setEventSubscribed(MARKET_EVENT_TICKER, pair, status);
	}
}
void TradingPlatform::subscribeBook(const char* pair) {
	auto status = isEventSubscribed(MARKET_EVENT_BOOK, pair);
	if (IS_STATUS_NOT_SUBSCRIBED(status)) {
		status = subscribeBookImpl(pair);
		setEventSubscribed(MARKET_EVENT_BOOK, pair, status);
	}
}
void TradingPlatform::subscribeTrade(const char* pair) {
	auto status = isEventSubscribed(MARKET_EVENT_TRADE, pair);
	if (IS_STATUS_NOT_SUBSCRIBED(status)) {
		status = subscribeTradeImpl(pair);
		setEventSubscribed(MARKET_EVENT_TRADE, pair, status);
	}
}
void TradingPlatform::subscribeCandle(const char* pair) {
	auto status = isEventSubscribed(MARKET_EVENT_CANDLE, pair);
	if (IS_STATUS_NOT_SUBSCRIBED(status)) {
		status = subscribeCandleImpl(pair);
		setEventSubscribed(MARKET_EVENT_CANDLE, pair, status);
	}
}

void TradingPlatform::unsubscribeTicker(const char* pair) {
	auto status = isEventSubscribed(MARKET_EVENT_TICKER, pair);
	if (!IS_STATUS_NOT_SUBSCRIBED(status)) {
		status = unsubscribeTickerImpl(pair);
		setEventSubscribed(MARKET_EVENT_TICKER, pair, status);
	}
}
void TradingPlatform::unsubscribeBook(const char* pair) {
	auto status = isEventSubscribed(MARKET_EVENT_BOOK, pair);
	if (!IS_STATUS_NOT_SUBSCRIBED(status)) {
		status = unsubscribeBookImpl(pair);
		setEventSubscribed(MARKET_EVENT_BOOK, pair, status);
	}
}
void TradingPlatform::unsubscribeTrade(const char* pair) {
	auto status = isEventSubscribed(MARKET_EVENT_TRADE, pair);
	if (!IS_STATUS_NOT_SUBSCRIBED(status)) {
		status = unsubscribeTradeImpl(pair);
		setEventSubscribed(MARKET_EVENT_TRADE, pair, status);
	}
}
void TradingPlatform::unsubscribeCandle(const char* pair) {
	auto status = isEventSubscribed(MARKET_EVENT_CANDLE, pair);
	if (!IS_STATUS_NOT_SUBSCRIBED(status)) {
		status = unsubscribeCandleImpl(pair);
		setEventSubscribed(MARKET_EVENT_CANDLE, pair, status);
	}
}

SubcribeStatus TradingPlatform::isEventSubscribed(const char* eventName, const char* pair) {
	return _tradingPlatformImpl->isEventSubscribed(eventName, pair);
}

void TradingPlatform::setEventSubscribed(const char* eventName, const char* pair, SubcribeStatus status) {
	_tradingPlatformImpl->setEventSubscribed(eventName, pair, status);
}

void TradingPlatform::removeAllHandlers() {
	_tradingPlatformImpl->removeAllHandlers();
}

int TradingPlatform::getHandlerCount() const {
	return _tradingPlatformImpl->getHandlerCount();
}
/////////////////////////////////////////////////////////////////////////////////
/// fill handlers to the buffer
/// return number of valid handler in the buffer
/////////////////////////////////////////////////////////////////////////////////
int TradingPlatform::getHandlers(MarketEventHandler** handlersBuffer, int maxCount) const {
	return _tradingPlatformImpl->getHandlers(handlersBuffer, maxCount);
}

void TradingPlatform::broadcastServerTime(TIMESTAMP t) {
	_tradingPlatformImpl->broadcastServerTime(t);
}

ILogger* TradingPlatform::getLogger() const {
	return _tradingPlatformImpl->getLogger();
}

void TradingPlatform::setLogger(ILogger* logger) {
	_tradingPlatformImpl->setLogger(logger);
}

void TradingPlatform::setQueryTimeInterval(TIMESTAMP t) {
	_updateInterval = t;
}

TIMESTAMP TradingPlatform::getQueryTimeInterval() const {
	return _updateInterval;
}

void TradingPlatform::setConfigFilePath(const char* path) {
	_tradingPlatformImpl->setConfigFilePath(path);
}

const char* TradingPlatform::getConfigFilePath() const {
	return _tradingPlatformImpl->getConfigFilePath();
}

void TradingPlatform::pushLog(LogLevel logLevel, const char* message) {
	_tradingPlatformImpl->pushLog(logLevel, message);
}

void TradingPlatform::pushLogV(LogLevel logLevel, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	_tradingPlatformImpl->pushLogVA(logLevel, fmt, args);
	va_end(args);
}

void TradingPlatform::pushLogVA(LogLevel logLevel, const char* fmt, va_list args) {
	_tradingPlatformImpl->pushLogVA(logLevel, fmt, args);
}