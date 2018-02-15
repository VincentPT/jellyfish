#pragma once
#include "MarketEventHandler.h"
#include <stdarg.h>
class TradingPlatformImpl;

#define MARKET_EVENT_TICKER "ticker"
#define MARKET_EVENT_BOOK "books"
#define MARKET_EVENT_TRADE "trades"
#define MARKET_EVENT_CANDLE "candles"

enum class SubcribeStatus : unsigned char
{
	NA = 0,
	Subcribing,
	Unsubcribing,
	Subcribed,
	Unsubcribed,
};

#define IS_STATUS_NOT_SUBSCRIBED(status) ((status) == SubcribeStatus::Unsubcribed || (status) == SubcribeStatus::Unsubcribing || (status) == SubcribeStatus::NA)

class TRADING_PLATFORM_API ILogger {
public:
	virtual void log(const char* message) = 0;
	virtual void logV(const char* fmt,...);
	virtual void logVA(const char* fmt, va_list args);
};

class TRADING_PLATFORM_API TradingPlatform
{
protected:
	TradingPlatformImpl* _tradingPlatformImpl;
	TIMESTAMP _updateInterval;
protected:
	void subcribeEventForHandler(MarketEventHandler* handler);
	void unsubcribeEventForHandler(MarketEventHandler* handler);
	virtual SubcribeStatus subscribeTickerImpl(const char* pair) = 0;
	virtual SubcribeStatus subscribeBookImpl(const char* pair) = 0;
	virtual SubcribeStatus subscribeTradeImpl(const char* pair) = 0;
	virtual SubcribeStatus subscribeCandleImpl(const char* pair) = 0;
	virtual SubcribeStatus unsubscribeTickerImpl(const char* pair) = 0;
	virtual SubcribeStatus unsubscribeBookImpl(const char* pair) = 0;
	virtual SubcribeStatus unsubscribeTradeImpl(const char* pair) = 0;
	virtual SubcribeStatus unsubscribeCandleImpl(const char* pair) = 0;

	virtual void setEventSubscribed(const char* eventName, const char* pair, SubcribeStatus status);
	virtual void setQueryTimeInterval(TIMESTAMP);
public:
	TradingPlatform();
	virtual ~TradingPlatform();

	virtual void connect() = 0;
	virtual void disconnect() = 0;
	virtual void startServerTimeQuery(TIMESTAMP updateInterval) = 0;
	virtual TIMESTAMP getSyncTime(TIMESTAMP localTime) = 0;
	virtual bool isServerTimeReady() = 0;
	virtual void getTradeHistory(const char* pair, TIMESTAMP duration, TIMESTAMP endTime, TradingList& tradeItems) = 0;
	// return pairs seperate by ';', caller must using free to free the string buffer after use
	virtual void getAllPairs(StringList& pairs) = 0;
	virtual void getBaseCurrencies(StringList& currencies) = 0;
	virtual void subscribeTicker(const char* pair);
	virtual void subscribeBook(const char* pair);
	virtual void subscribeTrade(const char* pair);
	virtual void subscribeCandle(const char* pair);
	virtual void unsubscribeTicker(const char* pair);
	virtual void unsubscribeBook(const char* pair);
	virtual void unsubscribeTrade(const char* pair);
	virtual void unsubscribeCandle(const char* pair);

	virtual SubcribeStatus isEventSubscribed(const char* eventName, const char* pair);

	virtual bool addEventHandler(MarketEventHandler* handler, bool allowDelete = false);
	virtual void removeEventHandler(const char* pair);
	virtual void removeAllHandlers();
	virtual int getHandlerCount() const;
	virtual TIMESTAMP getQueryTimeInterval() const;
	virtual void broadcastServerTime(TIMESTAMP t);
	/////////////////////////////////////////////////////////////////////////////////
	/// fill handlers to the buffer
	/// return number of valid handler in the buffer
	/////////////////////////////////////////////////////////////////////////////////
	virtual int getHandlers(MarketEventHandler** handlersBuffer, int maxCount) const;
	virtual MarketEventHandler* getHandler(const char* pair);
	virtual void setConfigFilePath(const char* path);
	virtual const char* getConfigFilePath() const;
	//////////////////////////////////////////////////////////////////////////////////////////
	///
	/// remove unused data
	///
	//////////////////////////////////////////////////////////////////////////////////////////
	virtual void cleanup();

	virtual ILogger* getLogger() const;
	virtual void setLogger(ILogger* logger);
	virtual void pushLog(const char* message);
	virtual void pushLogV(const char* fmt, ...);
	virtual void pushLogVA(const char* fmt, va_list args);
};

