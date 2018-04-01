#include "WxControlBoard.h"

const char* platforms[] = { "bitfinex", "binance" };

WxControlBoard::WxControlBoard(const std::vector<std::string>& platforms) : _checkedButton(-1), _currentGraphLength(1)
{
	_window_flags |= ImGuiWindowFlags_NoTitleBar;
	_window_flags |= ImGuiWindowFlags_NoMove;
	_window_flags |= ImGuiWindowFlags_NoResize;
	_window_flags |= ImGuiWindowFlags_NoCollapse;
	_window_flags |= ImGuiWindowFlags_NoScrollbar;

	_platforms = platforms;
	if (_platforms.size()) {
		_currentPlatform = 0;
	}
	else {
		_currentPlatform = -1;
	}
}


WxControlBoard::~WxControlBoard()
{
}
void WxControlBoard::update() {
	FUNCTON_LOG();

	ImGui::SetNextWindowSize(_window_size, ImGuiCond_Always);
	ImGui::SetNextWindowPos(_window_pos);

	if (!ImGui::Begin("Control board", nullptr, _window_flags))
	{
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	if (_platforms.size()) {
		if (ImGui::BeginCombo(" ", _platforms[_currentPlatform].c_str()))
		{
			for (int n = 0; n < (int)_platforms.size(); n++)
			{
				bool is_selected = (_currentPlatform == n);
				if (ImGui::Selectable(_platforms[n].c_str(), is_selected))
					_currentPlatform = n;
				if (is_selected)
					ImGui::SetItemDefaultFocus();   // Set the initial focus when opening the combo (scrolling + for keyboard navigation support in the upcoming navigation branch)
			}
			ImGui::EndCombo();
		}
		ImGui::Separator();
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

	if (ImGui::Button("export", ImVec2(120, 35))) {
		if (_exportButtonClickHandler) {
			_exportButtonClickHandler(this);
		}
	}
	ImGui::Separator();
	if (ImGui::CollapsingHeader("Notification", ImGuiTreeNodeFlags_DefaultOpen)) {
		bool oldVal = _pushToCloud;
		ImGui::Checkbox("Push message", &_pushToCloud);
		if (oldVal != _pushToCloud && _notificationModeChangedHandler) {
			_notificationModeChangedHandler(this);
		}
	}

	ImGui::Separator();
	if (ImGui::CollapsingHeader("Currencies", ImGuiTreeNodeFlags_DefaultOpen)) {
		std::unique_lock<std::mutex> lk(_mutex);
		int oldSelected = _checkedButton;

		ImGui::BeginGroup();

		for (int i = 0; i < _currencies.size(); i++) {
			ImGui::RadioButton(_currencies[i].c_str(), &_checkedButton, i);
		}
		ImGui::EndGroup();

		if (oldSelected != _checkedButton && _selectedCurrencyChangedHandler) {
			_selectedCurrencyChangedHandler(this);
		}
	}
	ImGui::Separator();
	if (ImGui::CollapsingHeader("Graph mode", ImGuiTreeNodeFlags_DefaultOpen)) {
		const char* graphLengths[] = {"7d", "1d", "12h", "4h", "1h", "30m", "5m", "1m" };
		if (ImGui::BeginCombo("  ", graphLengths[_currentGraphLength]))
		{
			int oldSelection = _currentGraphLength;
			for (int n = 0; n < IM_ARRAYSIZE(graphLengths); n++)
			{
				bool is_selected = _currentGraphLength == n;
				if (ImGui::Selectable(graphLengths[n], is_selected))
					_currentGraphLength = n;
				if (is_selected)
					ImGui::SetItemDefaultFocus();   // Set the initial focus when opening the combo (scrolling + for keyboard navigation support in the upcoming navigation branch)
			}
			ImGui::EndCombo();
			if (oldSelection != _currentGraphLength && _graphLengthChangedHandler) {
				_graphLengthChangedHandler(this);
			}
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

void WxControlBoard::setOnExportButtonClickHandler(ButtonClickEventHandler&& handler) {
	_exportButtonClickHandler = handler;
}

void WxControlBoard::setOnSelectedCurrencyChangedHandler(ButtonClickEventHandler&& handler) {
	_selectedCurrencyChangedHandler = handler;
}

void WxControlBoard::setOnNotificationModeChangedHandler(ButtonClickEventHandler&& handler) {
	_notificationModeChangedHandler = handler;
}

void WxControlBoard::setOnGraphLengthChangedHandler(ButtonClickEventHandler&& handler) {
	_graphLengthChangedHandler = handler;
}

void WxControlBoard::accessSharedData(const AccessSharedDataFunc& f) {
	std::unique_lock<std::mutex> lk(_mutex);
	f(this);
}

void WxControlBoard::setBaseCurrencies(const std::vector<std::string>& currencies) {
	_currencies = currencies;
	_currencies.push_back(DEFAULT_CURRENCY);
	_checkedButton = (int)(_currencies.size() - 1);
}

const std::string& WxControlBoard::getCurrentCurrency() const {
	return _currencies[_checkedButton];
}

const char* WxControlBoard::getCurrentPlatform() const {
	if (_currentPlatform < 0) return nullptr;
	return _platforms[_currentPlatform].c_str();
}

bool WxControlBoard::isPushToCloudEnable() const {
	return _pushToCloud;
}

unsigned int WxControlBoard::getCurrentGraphLengh() const {
	static unsigned int graphLengths[] = {
		7 * 24 * 3600,
		1 * 24 * 3600,
		12 * 3600,
		4 * 3600,
		1 * 3600,
		30 * 60,
		5 * 60,
		60,
	};

	return graphLengths[_currentGraphLength];
}