#pragma once
#include <string>
#include <memory>
#include <map>
#include <list>
#include <mutex>
#include <thread>
#include <functional>
#include <map>

#include "MarketEventHandler.h"

// maximum record count
#define MAX_RECORDS_COUNT 100000

struct PricePoint {
	double price;
	TIMESTAMP at;
};

struct SimpleTicker
{
	PricePoint firstPrice;
	PricePoint lastPrice;
	PricePoint low;
	PricePoint high;
	double averagePrice; // average price during the period
	double soldVolume; // volume of shelling of takers
	double boughtVolume;// volume of buying of takers
	char processLevel;
};

class NAPMarketEventHandler;
typedef std::function<void(NAPMarketEventHandler*)> AccessEventDataFunc;
typedef std::function<void(NAPMarketEventHandler*, TradeItem*)> TradeEventListener;

class NAPMarketEventHandler : public MarketEventHandler
{
private:
	std::mutex _m;
	std::list<SimpleTicker> _tradeHistory;
	int _autoId;
	std::map<int, TradeEventListener> _tradeEventListeners;
	void* _tag;
	TIMESTAMP _tickerDuration;
	void addEmptyTicker(TIMESTAMP t);
public:
	NAPMarketEventHandler(TIMESTAMP tickerDuration, const char* pair);
	virtual ~NAPMarketEventHandler();

	virtual void onTickerUpdate(Ticker& ticker);
	virtual void onBooksUpdate(BookItemEventArgs* books, bool snapShot);
	virtual void onTradesUpdate(TradeItem* trades, int count, bool snapShot);
	virtual void onCandlesUpdate(CandleItem* candles, int count, bool snapShot);

	void accessSharedData(const AccessEventDataFunc& f);
	virtual void onTimeUpdate(TIMESTAMP t);
	const std::list<SimpleTicker>& getTickerHistoriesNonSync() const;
	std::list<SimpleTicker>& getTickerHistoriesNonSync();
	void setTag(void*p);
	void* getTag();
	int addTradeEventListener(TradeEventListener&& eventListener);
	void removeTradeEventListener(int id);
};
