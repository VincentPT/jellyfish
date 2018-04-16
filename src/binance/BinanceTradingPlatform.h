#pragma once
#include "CPPRESTBaseTradingPlatformThreadSafe.h"
#include <cpprest/http_client.h>

#include <string>
#include <mutex>
#include <functional>
#include <memory>
#include <SyncMessageQueue.hpp>
#include <future>

class BinanceTradingPlatform : public CPPRESTBaseTradingPlatformThreadSafe
{
private:
	std::mutex _eventMutex;

	std::future<void> _pingServerTask;
	std::future<void> _restartServerTask;
	std::future<void> _maintenanceConnectionTask;
	std::shared_ptr<web::http::client::http_client> _restClient;
	std::list<std::shared_ptr<web::websockets::client::websocket_callback_client>> _additionalClients;
	std::list<std::shared_ptr<web::websockets::client::websocket_callback_client>> _inactiveClients;
	Signal<bool> _stopLoopTask;
	TIMESTAMP _timeDiff;
	bool _serverTimeIsReady;
	std::string _apiKey;
	int _timeToRestart = 60;

private:
	void messageHandlerImpl(const web::websockets::client::websocket_incoming_message& msg);

	void invokeTickerEvent(MarketEventHandler* handler, web::json::value& value);
	void invokeBookEventUpdate(MarketEventHandler* handler, web::json::value& value);
	void invokeBookEventSnapshot(MarketEventHandler* handler, web::json::value& value);
	void invokeTradeEvent(MarketEventHandler* handler, web::json::value& value);
	void invokeCandleEvent(MarketEventHandler* handler, web::json::value& value);
	void pingServerLoop();
	void maintanceConnection();
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
	void safeRequest(web::http::client::http_client& client, const std::function<void(web::http::http_response&)>& f);

	//virtual void resetClientNonSync();
	//virtual void resetClientMessageHandlerNonSync();
public:
	BinanceTradingPlatform();
	virtual ~BinanceTradingPlatform();

	virtual void startServerTimeQuery(TIMESTAMP updateInterval);
	virtual TIMESTAMP getSyncTime(TIMESTAMP localTime);
	virtual bool isServerTimeReady();
	virtual bool addEventHandler(MarketEventHandler* handler, bool allowDelete = false);
	virtual void removeEventHandler(const char* pair);
	virtual void getAllPairs(StringList& pairs);
	virtual void getBaseCurrencies(StringList& currencies);
	virtual void getTradeHistory(const char* pair, TIMESTAMP duration, TIMESTAMP endTime, TradingList& tradeItems);
	virtual void getCandleHistory(const char* pair, TIMESTAMP duration, TIMESTAMP endTime, CandleList& candleItems);
};


