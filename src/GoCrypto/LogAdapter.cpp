#include "LogAdapter.h"
#include "UI/WxAppLog.h"

void pushLog(const char* fmt, ...);

LogAdapter::LogAdapter(WxAppLog* appLog) : _appLog(appLog) {}

void LogAdapter::log(const char* message) {
	_appLog->addLog(message);
}

 void LogAdapter::logV(const char* fmt, ...) {
	 va_list args;
	 va_start(args, fmt);
	 _appLog->addLogV(fmt, args);
	 va_end(args);
}