#include "WxAppLog.h"

WxAppLog::WxAppLog() {
	_window_flags |= ImGuiWindowFlags_NoTitleBar;
	_window_flags |= ImGuiWindowFlags_NoMove;
	_window_flags |= ImGuiWindowFlags_NoResize;
	_window_flags |= ImGuiWindowFlags_NoCollapse;
	_window_flags |= ImGuiWindowFlags_NoScrollbar;
}

WxAppLog::~WxAppLog() {

}

void WxAppLog::addLog(const char* fmt, ...) IM_FMTARGS(2)
{
    va_list args;
    va_start(args, fmt);
	addLogV(fmt, args);
    va_end(args);
}

void WxAppLog::addLogV(const char* fmt, va_list args) {
	int old_size = Buf.size();
	Buf.appendfv(fmt, args);
	for (int new_size = Buf.size(); old_size < new_size; old_size++)
		if (Buf[old_size] == '\n')
			LineOffsets.push_back(old_size);
	ScrollToBottom = true;
}

void WxAppLog::draw()
{	
	FUNCTON_LOG();
    ImGui::SetNextWindowSize(_window_size, ImGuiCond_Always);
	ImGui::SetNextWindowPos(_window_pos);

    ImGui::Begin("application log", nullptr, _window_flags);
    if (ImGui::Button("Clear")) clear();

    ImGui::SameLine();
    bool copy = ImGui::Button("Copy");

	ImGui::SameLine();
	Filter.Draw("Filter", -200.0f);

	ImGui::SameLine();
	static int _logLevel = 0;
	const char* logLevels[] = { "Debug", "Info", "Error"};
	if (ImGui::BeginCombo("Log level", logLevels[_logLevel]))
	{
		int oldSelection = _logLevel;
		for (int n = 0; n < IM_ARRAYSIZE(logLevels); n++)
		{
			bool is_selected = _logLevel == n;
			if (ImGui::Selectable(logLevels[n], is_selected))
				_logLevel = n;
			if (is_selected)
				ImGui::SetItemDefaultFocus();   // Set the initial focus when opening the combo (scrolling + for keyboard navigation support in the upcoming navigation branch)
		}
		ImGui::EndCombo();
		//if (oldSelection != _logLevel && _graphLengthChangedHandler) {
		//	_graphLengthChangedHandler(this);
		//}
	}

    ImGui::Separator();
    ImGui::BeginChild("scrolling", ImVec2(0,0), false, ImGuiWindowFlags_HorizontalScrollbar);
    if (copy) ImGui::LogToClipboard();

    if (Filter.IsActive())
    {
        const char* buf_begin = Buf.begin();
        const char* line = buf_begin;
        for (int line_no = 0; line != NULL; line_no++)
        {
            const char* line_end = (line_no < LineOffsets.Size) ? buf_begin + LineOffsets[line_no] : NULL;
            if (Filter.PassFilter(line, line_end))
                ImGui::TextUnformatted(line, line_end);
            line = line_end && line_end[1] ? line_end + 1 : NULL;
        }
    }
    else
    {
        ImGui::TextUnformatted(Buf.begin());
    }

    if (ScrollToBottom)
        ImGui::SetScrollHere(1.0f);
    ScrollToBottom = false;
    ImGui::EndChild();
    ImGui::End();
}