#pragma once
#include "TradingBase.h"

typedef unsigned long long ORDER_ID;
typedef long long TIMESTAMP; //time stamp in mili seconds.

struct BookItem {
	ORDER_ID oderId;
	double price;
	double amount;
};

struct BookItemEventArgs {
	BookItem* askItems;
	BookItem* bidItems;
	int askCount;
	int bidCount;
};

struct Ticker
{
	double bid; // bid
	double ask; // ask
	double lastPrice; // The price at which the last order executed
	double low; // Lowest trade price of the last 24 hours
	double high; // Highest trade price of the last 24 hours;
	double volume; // Trading volume of the last 24 hours
	TIMESTAMP timestamp; //The timestamp at which this information was valid
};

struct TradeItem {
	ORDER_ID oderId;
	TIMESTAMP timestamp; 
	double price;
	double amount;
};

struct CandleItem {
	TIMESTAMP timestamp;
	double open;
	double close;
	double high;
	double low;
	double volume;
};

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
