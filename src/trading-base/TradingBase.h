#pragma once


#ifdef _WIN32
	#define TEMPLATE_EXTERN
	#ifdef EXPORT_TRADING_BASE
		#define TRADING_PLATFORM_API __declspec(dllexport)
	#else
		#define TRADING_PLATFORM_API __declspec(dllimport)
	#endif // EXPORT_TRADING_BASE
#else
	#define TEMPLATE_EXTERN extern
	#define TRADING_PLATFORM_API
#endif // _WIN32


typedef unsigned long long ORDER_ID;
typedef unsigned long long TRADE_ID;
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
	TRADE_ID oderId;
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

#include "SingleList.h"

TEMPLATE_EXTERN template class TRADING_PLATFORM_API SingleList<TradeItem>;
typedef SingleList<TradeItem> TradingList;