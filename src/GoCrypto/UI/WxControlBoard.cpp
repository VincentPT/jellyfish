#include "WxControlBoard.h"

const char* platforms[] = { "bitfinex", "binance" };

WxControlBoard::WxControlBoard() : _checkedButton(-1), _currentPlatform(platforms[0]), _currentGraphLength(1)
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
	FUNCTON_LOG();

	ImGui::SetNextWindowSize(_window_size, ImGuiCond_Always);
	ImGui::SetNextWindowPos(_window_pos);

	if (!ImGui::Begin("Control board", nullptr, _window_flags))
	{
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	if (ImGui::BeginCombo(" ", _currentPlatform))
	{
		for (int n = 0; n < IM_ARRAYSIZE(platforms); n++)
		{
			bool is_selected = (_currentPlatform == platforms[n]);
			if (ImGui::Selectable(platforms[n], is_selected))
				_currentPlatform = platforms[n];
			if (is_selected)
				ImGui::SetItemDefaultFocus();   // Set the initial focus when opening the combo (scrolling + for keyboard navigation support in the upcoming navigation branch)
		}
		ImGui::EndCombo();
	}
	ImGui::Separator();

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
		ImGui::Checkbox("Push message", &_pushToCloud);
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
	return _currentPlatform;
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