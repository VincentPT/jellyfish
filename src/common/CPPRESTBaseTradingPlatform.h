#pragma once

#include "TradingPlatform.h"

#include <cpprest/ws_client.h>
#include <cpprest/json.h>

#ifdef _UTF16_STRINGS
#define _ttod(s) _wtof(s)
#define _ttoll(s) _wtoll(s)
#define _tscans swscanf_s
#else
#define _ttod(s) atof(s)
#define _ttoll(s) atoll(s)
#define _tscans sscanf_s
#endif

void sendJsonContent(
	web::websockets::client::websocket_client& client,
	web::json::value& obj);

void sendJsonContent(
	web::websockets::client::websocket_callback_client& client,
	web::json::value& obj);


class CPPRESTBaseTradingPlatform : public TradingPlatform
{
protected:
	std::shared_ptr<web::websockets::client::websocket_callback_client> _client;
protected:
	void sendJsonContent(web::json::value& value);
public:
	CPPRESTBaseTradingPlatform();
	virtual ~CPPRESTBaseTradingPlatform();
};