#pragma once
#include "TradingPlatform.h"

class WxAppLog;
class LogAdapter : public ILogger {
	WxAppLog* _appLog;
public:
	LogAdapter(WxAppLog* appLog);
	virtual void log(LogLevel logLevel, const char* message);
	virtual void logV(LogLevel logLevel, const char* fmt, ...);
};