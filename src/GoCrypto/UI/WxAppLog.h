#pragma once
#include "ImWidget.h"

class WxAppLog : public ImWidget
{
public:
	enum class LogLevel : int
	{
		Verbose = 0,
		Debug,
		Info,
		Error,
	};
private:
    ImGuiTextBuffer     Buf;
    ImGuiTextFilter     Filter;
    ImVector<int>       LineOffsets;        // Index to lines offset
	LogLevel _logLevel;
    bool                ScrollToBottom;
	MouseDoubleClickEventHandler _doubleClickHandler;
public:
	WxAppLog();
	~WxAppLog();
    void    clear()     { Buf.clear(); LineOffsets.clear(); }

	void    addLog(LogLevel logLevel, const char* fmt, ...) IM_FMTARGS(2);
	void    addLogV(LogLevel logLevel, const char* fmt, va_list args);
	void    update();
	void setLogLevel(LogLevel logLevel);
	void setDoubleClickHandler(MouseDoubleClickEventHandler&& handler);
};