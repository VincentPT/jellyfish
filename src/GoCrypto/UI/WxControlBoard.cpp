#include "WxControlBoard.h"

WxControlBoard::WxControlBoard()
{
	_window_flags |= ImGuiWindowFlags_NoTitleBar;
	_window_flags |= ImGuiWindowFlags_NoMove;
	_window_flags |= ImGuiWindowFlags_NoResize;
	_window_flags |= ImGuiWindowFlags_NoCollapse;
	_window_flags |= ImGuiWindowFlags_NoScrollbar;
}


WxControlBoard::~WxControlBoard()
{
}
void WxControlBoard::update() {
	ImGui::SetNextWindowSize(_window_size, ImGuiCond_Always);
	ImGui::SetNextWindowPos(_window_pos);

	if (!ImGui::Begin("Control board", nullptr, _window_flags))
	{
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	if (ImGui::Button("start", ImVec2(120, 35))) {
		if (_startButtonClickHandler) {
			_startButtonClickHandler(this);
		}
	}

	if (ImGui::Button("stop", ImVec2(120, 35))) {
		if (_stopButtonClickHandler) {
			_stopButtonClickHandler(this);
		}
	}
	
	ImGui::End();
}

void WxControlBoard::setOnStartButtonClickHandler(ButtonClickEventHandler&& handler) {
	_startButtonClickHandler = handler;
}

void WxControlBoard::setOnStopButtonClickHandler(ButtonClickEventHandler&& handler) {
	_stopButtonClickHandler = handler;
}