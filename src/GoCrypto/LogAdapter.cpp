#include "LogAdapter.h"
#include "UI/WxAppLog.h"

LogAdapter::LogAdapter(WxAppLog* appLog) : _appLog(appLog) {}

void LogAdapter::log(LogLevel logLevel, const char* message) {
	_appLog->addLog((WxAppLog::LogLevel)logLevel, message);
}

 void LogAdapter::logV(LogLevel logLevel, const char* fmt, ...) {
	 va_list args;
	 va_start(args, fmt);
	 _appLog->addLogV((WxAppLog::LogLevel)logLevel, fmt, args);
	 va_end(args);
}