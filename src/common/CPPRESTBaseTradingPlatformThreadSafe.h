#pragma once

#include "CPPRESTBaseTradingPlatform.h"

#include <cpprest/ws_client.h>
#include <cpprest/json.h>
#include <mutex>

class CPPRESTBaseTradingPlatformThreadSafe : public CPPRESTBaseTradingPlatform
{
private:
	std::mutex _m;
	std::mutex _connectionMutex;
	bool _blConnected;
protected:
	virtual bool connectImpl() = 0;
	virtual bool disconnectImpl() = 0;
	virtual void messageHandlerImpl(const web::websockets::client::websocket_incoming_message& msg) = 0;
	virtual void setConnectionStatus(bool);
	virtual bool getConnectionStatus()const;
	virtual void resetClient();
	virtual void resetClientNonSync();
	virtual void resetClientMessageHandlerNonSync();
	void messageHandler(const web::websockets::client::websocket_incoming_message& msg);
public:
	CPPRESTBaseTradingPlatformThreadSafe();
	virtual ~CPPRESTBaseTradingPlatformThreadSafe();

	virtual void subscribeTicker(const char* pair);
	virtual void subscribeBook(const char* pair);
	virtual void subscribeTrade(const char* pair);
	virtual void subscribeCandle(const char* pair);

	virtual void unsubscribeTicker(const char* pair);
	virtual void unsubscribeBook(const char* pair);
	virtual void unsubscribeTrade(const char* pair);
	virtual void unsubscribeCandle(const char* pair);

	virtual void connect();
	virtual void disconnect();
};