#include "CPPRESTBaseTradingPlatformThreadSafe.h"
#include "Utility.h"

using namespace web;
using namespace web::websockets::client;

CPPRESTBaseTradingPlatformThreadSafe::CPPRESTBaseTradingPlatformThreadSafe() : _blConnected(false) {

}
CPPRESTBaseTradingPlatformThreadSafe::~CPPRESTBaseTradingPlatformThreadSafe() {
}

void CPPRESTBaseTradingPlatformThreadSafe::subscribeTicker(const char* pair) {
	std::unique_lock<std::mutex> lk(_m);
	TradingPlatform::subscribeTicker(pair);
}

void CPPRESTBaseTradingPlatformThreadSafe::subscribeBook(const char* pair) {
	std::unique_lock<std::mutex> lk(_m);
	TradingPlatform::subscribeBook(pair);
}
void CPPRESTBaseTradingPlatformThreadSafe::subscribeTrade(const char* pair) {
	std::unique_lock<std::mutex> lk(_m);
	TradingPlatform::subscribeTrade(pair);
}

void CPPRESTBaseTradingPlatformThreadSafe::subscribeCandle(const char* pair) {
	std::unique_lock<std::mutex> lk(_m);
	TradingPlatform::subscribeCandle(pair);
}

void CPPRESTBaseTradingPlatformThreadSafe::unsubscribeTicker(const char* pair) {
	std::unique_lock<std::mutex> lk(_m);
	TradingPlatform::unsubscribeTicker(pair);
}
void CPPRESTBaseTradingPlatformThreadSafe::unsubscribeBook(const char* pair) {
	std::unique_lock<std::mutex> lk(_m);
	TradingPlatform::unsubscribeBook(pair);
}
void CPPRESTBaseTradingPlatformThreadSafe::unsubscribeTrade(const char* pair) {
	std::unique_lock<std::mutex> lk(_m);
	TradingPlatform::unsubscribeTrade(pair);
}
void CPPRESTBaseTradingPlatformThreadSafe::unsubscribeCandle(const char* pair) {
	std::unique_lock<std::mutex> lk(_m);
	TradingPlatform::unsubscribeCandle(pair);
}

void CPPRESTBaseTradingPlatformThreadSafe::connect() {
	LOG_SCOPE_ACCESS(getLogger(), __FUNCTION__);
	std::unique_lock<std::mutex> lk(_connectionMutex);
	if (_blConnected == true) {
		std::cout << "client instance is already connected" << std::endl;
		return;
	}

	_blConnected = connectImpl();
}

void CPPRESTBaseTradingPlatformThreadSafe::disconnect() {
	LOG_SCOPE_ACCESS(getLogger(), __FUNCTION__);
	std::unique_lock<std::mutex> lk(_connectionMutex);
	if (_blConnected == false) {
		std::cout << "client instance is already disconnected" << std::endl;
		return;
	}

	if (disconnectImpl()) {
		_blConnected = false;
	}
}

void CPPRESTBaseTradingPlatformThreadSafe::resetClient() {
	std::unique_lock<std::mutex> lk(_connectionMutex);
	resetClientNonSync();
}

void CPPRESTBaseTradingPlatformThreadSafe::resetClientNonSync() {
	setConnectionStatus(false);
	_client = std::make_shared<websocket_callback_client>();
	resetClientMessageHandlerNonSync();
}

void CPPRESTBaseTradingPlatformThreadSafe::resetClientMessageHandlerNonSync() {
	// set receive handler
	_client->set_message_handler(std::bind(&CPPRESTBaseTradingPlatformThreadSafe::messageHandler, this, std::placeholders::_1));
}

void CPPRESTBaseTradingPlatformThreadSafe::setConnectionStatus(bool flag) {
	_blConnected = flag;
}

bool CPPRESTBaseTradingPlatformThreadSafe::getConnectionStatus() const {
	return _blConnected;
}

void CPPRESTBaseTradingPlatformThreadSafe::messageHandler(const web::websockets::client::websocket_incoming_message& msg) {
	std::unique_lock<std::mutex> lk(_connectionMutex);
	if (_blConnected == false) {
		return;
	}

	messageHandlerImpl(msg);
}