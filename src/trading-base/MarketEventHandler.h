#pragma once
#include "TradingBase.h"

TRADING_PLATFORM_API void formatTime(char* bufffer, size_t bufferSize, TIMESTAMP t);
TRADING_PLATFORM_API TIMESTAMP getCurrentTimeStamp();

class TRADING_PLATFORM_API MarketEventHandler
{
protected:
	char* _pair;
	bool _useTicker;
	bool _useOrderBooks;
	bool _usedTrade;
	bool _useCandle;
	//std::map<int, BookItem> _buyOrders;
	//std::map<int, BookItem> _shellOrders;
public:
	MarketEventHandler(const char* pair);
	virtual ~MarketEventHandler();

	const char* getPair() const;
	virtual void onTickerUpdate(Ticker& ticker);
	virtual void onBooksUpdate(BookItemEventArgs* books, bool snapShot);
	virtual void onTradesUpdate(TradeItem* trades, int count, bool snapShot);
	virtual void onCandlesUpdate(CandleItem* candles, int count, bool snapShot);
	virtual void onTimeUpdate(TIMESTAMP t);

	virtual bool useTicker();
	virtual bool useOrderBook();
	virtual bool useTrades();
	virtual bool useCandles();

	virtual void useTicker(bool);
	virtual void useOrderBook(bool);
	virtual void useTrades(bool);
	virtual void useCandles(bool);
};
