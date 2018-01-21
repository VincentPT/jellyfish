#pragma once
#include "ImWidget.h"

class WxAppLog : public ImWidget
{
    ImGuiTextBuffer     Buf;
    ImGuiTextFilter     Filter;
    ImVector<int>       LineOffsets;        // Index to lines offset
    bool                ScrollToBottom;
	
public:
	WxAppLog();
	~WxAppLog();
    void    clear()     { Buf.clear(); LineOffsets.clear(); }

	void    addLog(const char* fmt, ...) IM_FMTARGS(2);
	void    addLogV(const char* fmt, va_list args);
	void    update();
};