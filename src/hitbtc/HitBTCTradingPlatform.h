#pragma once
#include "CPPRESTBaseTradingPlatformThreadSafe.h"

#include <string>
#include <mutex>
#include <functional>
#include <memory>

class HitBTCTradingPlatform : public CPPRESTBaseTradingPlatformThreadSafe
{
	struct EventHandlerInfo {
		MarketEventHandler* handler;
		const char* eventName;
	};
	typedef std::map<utility::string_t, ORDER_ID> PriceOrderMap;
	typedef std::shared_ptr<PriceOrderMap> PriceOrderMapRef;

	struct BookIdGeneratorInfo {
		PriceOrderMapRef askContainer;
		PriceOrderMapRef bidContainer;
		ORDER_ID nextId;
	};
private:
	std::mutex _eventMutex;
	std::map<int, EventHandlerInfo> _chanelEventHandlerInfoMap;
	std::map<std::string, int> _chanelEventMap;
	std::map<std::string, BookIdGeneratorInfo> _bookCollectionMap;
	int _autoClientId;

private:
	void messageHandlerImpl(const web::websockets::client::websocket_incoming_message& msg);

	void invokeTickerEvent(MarketEventHandler* handler, web::json::value& value);
	void invokeBookEventUpdate(MarketEventHandler* handler, web::json::value& value);
	void invokeBookEventSnapshot(MarketEventHandler* handler, web::json::value& value);
	void invokeTradeEvent(MarketEventHandler* handler, web::json::value& value, bool snapshot);
	void invokeCandleEvent(MarketEventHandler* handler, web::json::value& value, bool snapShot);

	SubcribeStatus subscribeEventImpl(const char* eventName, const char* pair, const utility::string_t& method, bool isCandle = false);
	SubcribeStatus unsubscribeEventImpl(const char* eventName, const char* pair, const utility::string_t& method, bool isCandle = false);
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
	HitBTCTradingPlatform();
	virtual ~HitBTCTradingPlatform();

	virtual void startServerTimeQuery(TIMESTAMP updateInterval);
	virtual TIMESTAMP getSyncTime(TIMESTAMP localTime);
	virtual bool isServerTimeReady();
	virtual bool addEventHandler(MarketEventHandler* handler, bool allowDelete = false);
	virtual void removeEventHandler(const char* pair);
	virtual void getAllPairs(StringList& pairs);
	virtual void getTradeHistory(const char* pair, TIMESTAMP duration, TIMESTAMP endTime, TradingList& tradeItems);
};


