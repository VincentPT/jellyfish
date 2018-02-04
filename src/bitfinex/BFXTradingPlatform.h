#pragma once
#include "CPPRESTBaseTradingPlatformThreadSafe.h"

#include <cpprest/ws_client.h>
#include <cpprest/json.h>
#include <string>
#include <functional>
#include <future>
#include <SyncMessageQueue.hpp>

class BFXTradingPlatform : public CPPRESTBaseTradingPlatformThreadSafe
{
	typedef void (BFXTradingPlatform::*SendMarketEventFunc)(MarketEventHandler*, web::json::value&);

	struct EventHandlerInfo {
		MarketEventHandler* handler;
		const char* eventName;
		SendMarketEventFunc invokefunction;
	};

private:
	web::websockets::client::websocket_client _pingServerClient;
	std::future<void> _pingServerTask;
	Signal<bool> _stopLoopTask;

	std::mutex _eventMutex;
	std::map<int, EventHandlerInfo> _chanelEventHandlerInfoMap;
	std::map<std::string, int> _chanelEventMap;
	std::map<std::string, Ticker> _lastickerMap;
	std::future<void> _maintenanceTask;
	Signal<bool> _restartEvents;
	TIMESTAMP _timeDiff;
	bool _serverTimeIsReady;
private:
	void messageHandlerImpl(const web::websockets::client::websocket_incoming_message& msg);

	void invokeTickerEvent(MarketEventHandler* handler, web::json::value& value);
	void invokeBookEvent(MarketEventHandler* handler, web::json::value& value);
	void invokeTradeEvent(MarketEventHandler* handler, web::json::value& value);
	void invokeCandleEvent(MarketEventHandler* handler, web::json::value& value);

	SubcribeStatus unsubcribeChanel(const std::string& eventName, const std::string& pair);

	void maintenanceConnectionWorker();
	void pingServerLoop();
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
	BFXTradingPlatform();
	virtual ~BFXTradingPlatform();

	virtual void startServerTimeQuery(TIMESTAMP updateInterval);
	virtual TIMESTAMP getSyncTime(TIMESTAMP localTime);
	virtual bool isServerTimeReady();
	virtual bool addEventHandler(MarketEventHandler* handler, bool allowDelete = false);
	virtual void removeEventHandler(const char* pair);
	virtual void getAllPairs(StringList& pairs);
	virtual void getTradeHistory(const char* pair, TIMESTAMP duration, TIMESTAMP endTime, TradingList& tradeItems);
};