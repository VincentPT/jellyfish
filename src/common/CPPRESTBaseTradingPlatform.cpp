#include "CPPRESTBaseTradingPlatform.h"
#include "Utility.h"


using namespace std;

using namespace web;
using namespace web::websockets::client;

template<class T>
void sendJsonContent(
	T& client,
	web::json::value& obj) {
	websocket_outgoing_message msg;
	std::string u8str = CPPREST_FROM_STRING(obj.serialize());

	msg.set_utf8_message(u8str);
	client.send(msg).wait();
}

void sendJsonContent(
	web::websockets::client::websocket_client& client,
	web::json::value& obj) {
	sendJsonContent<websocket_client>(client, obj);
}

void sendJsonContent(
	web::websockets::client::websocket_callback_client& client,
	web::json::value& obj) {
	sendJsonContent<websocket_callback_client>(client, obj);
}


CPPRESTBaseTradingPlatform::CPPRESTBaseTradingPlatform() {

}
CPPRESTBaseTradingPlatform::~CPPRESTBaseTradingPlatform() {

}
void CPPRESTBaseTradingPlatform::sendJsonContent(web::json::value& obj) {
	::sendJsonContent(*_client, obj);
}