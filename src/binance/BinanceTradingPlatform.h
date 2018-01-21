#pragma once
#include "CPPRESTBaseTradingPlatformThreadSafe.h"

#include <string>
#include <mutex>
#include <functional>
#include <memory>

class BinanceTradingPlatform : public CPPRESTBaseTradingPlatformThreadSafe
{
private:
	std::mutex _eventMutex;
	//std::map<int, EventHandlerInfo> _chanelEventHandlerInfoMap;
	//std::map<std::string, int> _chanelEventMap;
	//std::map<std::string, BookIdGeneratorInfo> _bookCollectionMap;
	//int _autoClientId;
	long long _timeDiff;

private:
	void messageHandlerImpl(const web::websockets::client::websocket_incoming_message& msg);

	void invokeTickerEvent(MarketEventHandler* handler, web::json::value& value);
	void invokeBookEventUpdate(MarketEventHandler* handler, web::json::value& value);
	void invokeBookEventSnapshot(MarketEventHandler* handler, web::json::value& value);
	void invokeTradeEvent(MarketEventHandler* handler, web::json::value& value);
	void invokeCandleEvent(MarketEventHandler* handler, web::json::value& value);
protected:
	virtual SubcribeStatus subscribeTickerImpl(const char* pair);
	virtual SubcribeStatus subscribeBookImpl(const char* pair);
	virtual SubcribeStatus subscribeTradeImpl(const char* pair);
	virtual SubcribeStatus subscribeCandleImpl(const char* pair);
	virtual SubcribeStatus unsubscribeTickerImpl(const char* pair);
	virtual SubcribeStatus unsubscribeBookImpl(const char* pair);
	virtual SubcribeStatus unsubscribeTradeImpl(const char* pair);
	virtual SubcribeStatus unsubscribeCandleImpl(const char* pair);

	bool connectImpl();
	bool disconnectImpl();

public:
	BinanceTradingPlatform();
	virtual ~BinanceTradingPlatform();

	virtual void startServerTimeQuery(TIMESTAMP updateInterval);
	virtual TIMESTAMP getSyncTime(TIMESTAMP localTime);
	virtual bool isServerTimeReady();
	virtual bool addEventHandler(MarketEventHandler* handler, bool allowDelete = false);
	virtual void removeEventHandler(const char* pair);
	virtual char* getAllPairs();
};


