#include "WxControlBoard.h"

struct BarGUIItem {
	char* name;
	int timeLength;
};

struct GraphLengthItem {
	char* name;
	unsigned int timeLength;
	int barCount;
	int defaultSelection;
	BarGUIItem* bars;
};

BarGUIItem barLengths7d[] = {
	{ "1d", 24 * 3600 },
	{ "12h", 12 * 3600 },
	{ "4h", 4 * 3600 },
	{ "1h", 1 * 3600 },
	{ "30m", 30 * 60 },
};

BarGUIItem barLengths1d[] = {
	{ "4h", 4 * 3600 },
	{ "1h", 1 * 3600 },
	{ "30m", 30 * 60 },
	{ "5m", 5 * 60 },
	{ "2m", 2 * 60 },
};

BarGUIItem barLengths12h[] = {
	{ "1h", 1 * 3600 },
	{ "30m", 30 * 60 },
	{ "5m", 5 * 60 },
	{ "2m", 2 * 60 },
	{ "1m", 1 * 60 },
};

BarGUIItem barLengths4h[] = {
	{ "30m", 30 * 60 },
	{ "5m", 5 * 60 },
	{ "2m", 2 * 60 },
	{ "1m", 1 * 60 },
	{ "30s", 30 },
};

BarGUIItem barLengths1h[] = {
	{ "5m", 5 * 60 },
	{ "2m", 2 * 60 },
	{ "1m", 1 * 60 },
	{ "30s", 30 },
	{ "15s", 30 },
	{ "5s", 5 },
};

BarGUIItem barLengths30m[] = {
	{ "2m", 2 * 60 },
	{ "1m", 1 * 60 },
	{ "30s", 30 },
	{ "15s", 30 },
	{ "5s", 5 },
};

BarGUIItem barLengths5m[] = {
	{ "30s", 30 },
	{ "15s", 15 },
	{ "5s", 5 },
};

BarGUIItem barLengths1m[] = {
	{ "5s", 5 },
};

GraphLengthItem graphLengths[] = {
	{ "7d", 7 * 24 * 3600, IM_ARRAYSIZE(barLengths7d), 4, barLengths7d },
	{ "1d", 1 * 24 * 3600, IM_ARRAYSIZE(barLengths1d), 4, barLengths1d },
	{ "12h", 12 * 3600, IM_ARRAYSIZE(barLengths12h), 3, barLengths12h },
	{ "4h", 4 * 3600, IM_ARRAYSIZE(barLengths4h), 2, barLengths4h },
	{ "1h", 1 * 3600, IM_ARRAYSIZE(barLengths1h), 2, barLengths1h },
	{ "30m", 30 * 60, IM_ARRAYSIZE(barLengths30m), 1, barLengths30m },
	{ "5m", 5 * 60, IM_ARRAYSIZE(barLengths5m), 1, barLengths5m },
	{ "1m", 1 * 60, IM_ARRAYSIZE(barLengths1m), 0, barLengths1m },
};


WxControlBoard::WxControlBoard(const std::vector<std::string>& platforms) : _checkedButton(-1), _currentGraphLength(1), _barLengthIdx(-1)
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

	_filterBuffer[0] = 0;
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
		bool oldVal = _notifyPriceMovement;
		ImGui::Checkbox("Price movement", &_notifyPriceMovement);
		if (oldVal != _notifyPriceMovement && _priceNotificationChangedHandler) {
			_priceNotificationChangedHandler(this);
		}
		oldVal = _notifyVolumeMovement;
		ImGui::Checkbox("Volume movement", &_notifyVolumeMovement);
		if (oldVal != _notifyVolumeMovement && _volumeNotificationChangedHandler) {
			_volumeNotificationChangedHandler(this);
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
	if (ImGui::CollapsingHeader("Graph Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
		{
			ImGui::ScopedItemWidth filterWidthScope(70);
			bool changed = false;
			if (ImGui::BeginCombo("time", graphLengths[_currentGraphLength].name))
			{
				int oldSelection = _currentGraphLength;
				for (int n = 0; n < IM_ARRAYSIZE(graphLengths); n++)
				{
					bool is_selected = _currentGraphLength == n;
					if (ImGui::Selectable(graphLengths[n].name, is_selected))
						_currentGraphLength = n;
					if (is_selected)
						ImGui::SetItemDefaultFocus();   // Set the initial focus when opening the combo (scrolling + for keyboard navigation support in the upcoming navigation branch)
				}
				ImGui::EndCombo();

				changed = oldSelection != _currentGraphLength;
			}
			auto& graphLengItem = graphLengths[_currentGraphLength];
			if (_barLengthIdx < 0 || _barLengthIdx >= graphLengItem.barCount || changed) {
				_barLengthIdx = graphLengItem.defaultSelection;
			}

			auto barLengths = graphLengItem.bars;

			if (ImGui::BeginCombo("bar", barLengths[_barLengthIdx].name))
			{
				int oldSelection = _barLengthIdx;
				for (int n = 0; n < graphLengItem.barCount; n++)
				{
					bool is_selected = _barLengthIdx == n;
					if (ImGui::Selectable(barLengths[n].name, is_selected))
						_barLengthIdx = n;
					if (is_selected)
						ImGui::SetItemDefaultFocus();   // Set the initial focus when opening the combo (scrolling + for keyboard navigation support in the upcoming navigation branch)
				}
				ImGui::EndCombo();
				if (changed || oldSelection != _barLengthIdx) {
					changed = true;
				}
			}
			if (changed && _graphLengthChangedHandler) {
				_graphLengthChangedHandler(this);
			}
		}
	}
	if (ImGui::CollapsingHeader("Symbol filter", ImGuiTreeNodeFlags_DefaultOpen)) {
		{
			ImGui::ScopedItemWidth filterWidthScope(70);

			if (ImGui::InputText("",
				&_filterBuffer[0], sizeof(_filterBuffer),
				ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CharsUppercase)) {
				if (_filterChangedHandler) {
					_filterChangedHandler(this);
				}
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear")) {
			_filterBuffer[0] = 0;
			if (_filterChangedHandler) {
				_filterChangedHandler(this);
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

void WxControlBoard::setOnPriceNotificationChangedHandler(ButtonClickEventHandler&& handler) {
	_priceNotificationChangedHandler = handler;
}

void WxControlBoard::setOnVolumeNotificationChangedHandler(ButtonClickEventHandler&& handler) {
	_volumeNotificationChangedHandler = handler;
}

void WxControlBoard::setOnGraphLengthChangedHandler(ButtonClickEventHandler&& handler) {
	_graphLengthChangedHandler = handler;
}

void WxControlBoard::setFilterChanedHandler(FilterActiveChanged&& handler) {
	_filterChangedHandler = handler;
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

bool WxControlBoard::isPriceNotificationEnable() const {
	return _notifyPriceMovement;
}

bool WxControlBoard::isVolumeNotificationEnable() const {
	return _notifyVolumeMovement;
}

unsigned int WxControlBoard::getCurrentGraphLengh() const {
	return graphLengths[_currentGraphLength].timeLength;
}

int WxControlBoard::getCurrentBarTime() const {
	auto& graphLengItem = graphLengths[_currentGraphLength];
	auto barLengths = graphLengItem.bars;

	return barLengths[_barLengthIdx].timeLength;
}

const char* WxControlBoard::getFilterText() const {
	return &_filterBuffer[0];
}