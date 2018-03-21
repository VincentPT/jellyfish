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

class NAPMarketEventHandler;
typedef std::function<void(NAPMarketEventHandler*)> AccessEventDataFunc;
typedef std::function<void(NAPMarketEventHandler*, TradeItem*, int, bool)> TradeEventListener;
typedef std::function<void(NAPMarketEventHandler*, CandleItem*, int, bool)> CandleEventListener;

class NAPMarketEventHandler : public MarketEventHandler
{
private:
	std::mutex _m;
	std::list<TradeItem> _tradeHistory;
	std::list<CandleItem> _candleHistory;
	int _autoId;
	std::map<int, TradeEventListener> _tradeEventListeners;
	std::map<int, CandleEventListener> _candleEventListeners;
	void* _tag;
public:
	NAPMarketEventHandler(const char* pair);
	virtual ~NAPMarketEventHandler();

	virtual void onTickerUpdate(Ticker& ticker);
	virtual void onBooksUpdate(BookItemEventArgs* books, bool snapShot);
	virtual void onTradesUpdate(TradeItem* trades, int count, bool snapShot);
	virtual void onCandlesUpdate(CandleItem* candles, int count, bool snapShot);

	void accessSharedData(const AccessEventDataFunc& f);
	//virtual void onTimeUpdate(TIMESTAMP t);
	const std::list<TradeItem>& getTradeHistoriesNonSync() const;
	std::list<TradeItem>& getTradeHistoriesNonSync();
	const std::list<CandleItem>& getCandleHistoriesNonSync();
	void setTag(void*p);
	void* getTag();
	int addTradeEventListener(TradeEventListener&& eventListener);
	void removeTradeEventListener(int id);
	int addCandleEventListener(CandleEventListener&& eventListener);
	void removeCandleEventListener(int id);
};
