#include "Notifier.h"
#include <cpprest/http_client.h>
#include <cpprest/json.h>
#include <sstream>
#include <chrono>
#include "../common/Utility.h"
#include "GoCrypto.h"

#include <chrono>
#include <fstream>
#include "MarketEventHandler.h"
using namespace std::chrono;

using namespace std;
using namespace web::http;                  // Common HTTP functionality
using namespace web::http::client;          // HTTP client features

Notifier::Notifier() {}

Notifier::~Notifier() {}

Notifier* Notifier::getInstance() {
	static Notifier notifier;
	return &notifier;
}

bool Notifier::pushNotification(const Notification& notification) {
	const UserListenerInfo& userInfo = *(notification.target);
	http_client_config config;
	//std::chrono::duration<long long, std::milli> timeout((long long)3000);
	//config.set_timeout(timeout);
	http_client client(U("https://api.pushover.net/1/messages.json"), config);

	const std::string& title = notification.title;
	const std::string& message = notification.message;

	bool res = true;
	//ofstream out;
	//try {
	//	string fileName(platformName);
	//	fileName.append(".txt");
	//	out.open(fileName, ios_base::out | ios_base::app);
	//	if (out.is_open()) {
	//		out << str << " " << title << " " << message << endl;
	//		res = true;
	//	}
	//}
	//catch (const std::exception &e)
	//{
	//	printf("Error exception:%s\n", e.what());
	//}

	web::json::value jsonBody;
	jsonBody[U("title")] = web::json::value::string(CPPREST_TO_STRING(title));
	jsonBody[U("message")] = web::json::value::string(CPPREST_TO_STRING(message));
	jsonBody[U("token")] = web::json::value::string(CPPREST_TO_STRING(userInfo.applicationKey));
	jsonBody[U("user")] = web::json::value::string(CPPREST_TO_STRING(userInfo.userKey));

	http_request request(methods::POST);
	request.set_body(jsonBody);

	//bool res = false;
	res = false;
	auto task = client.request(request).then([&res](http_response response) {
		auto code = response.status_code();
		if (code == status_codes::OK) {
			res = true;
		}
		else {
			string errorMsg("server response error:");
			errorMsg.append(to_string(code));
			pushLog((int)LogLevel::Error, "push message failed:%s\n", errorMsg.c_str());
		}
	});
	task.wait();
	return res;
}