#include "WxVolumeTriggers.h"
#include <sstream>

WxVolumeTriggers::WxVolumeTriggers() : ImPopup("Volume triggers editor") {
	_columns = {
		{ "duration(*)", 80, 0 },
		{ "volume changed threhold(*)", 190, 1 },
		{ "price changed threshold", 180, 2 },
		{ "minium volume(BTC)(*)", 155, 3 },
		{ "apply to", 90, 4 },
	};

	// there is a bug if scrollbar is enable
	// Luckily,for this window, we don't need scrollbars.
	_window_flags |= ImGuiWindowFlags_NoScrollbar;
	_window_flags |= ImGuiWindowFlags_NoResize;
	_cellBuffer = createRowBuffer((int)_columns.size());
}

WxVolumeTriggers::~WxVolumeTriggers() {
	free(_cellBuffer);
}

void WxVolumeTriggers::setTriggers(const std::vector<TriggerVolumeBaseItem>& triggers) {
	std::unique_lock<std::mutex> lk(_mutex);
	_triggers = triggers;
	_selectedItemIdx = -1;
}

const std::vector<TriggerVolumeBaseItem>& WxVolumeTriggers::getTriggers() const {
	return _triggers;
}

void WxVolumeTriggers::clearTriggers() {
	_triggers.clear();
}


void WxVolumeTriggers::updateContent()
{
	std::unique_lock<std::mutex> lk(_mutex);
	float offset = 0.0f;

	float itemHeight = ImGui::GetTextLineHeightWithSpacing();
	if (ImGui::BeginChild("##VolumeTriggers", ImVec2(0, itemHeight), false, ImGuiWindowFlags_NoScrollbar)) {
		ImGui::Columns((int)_columns.size());
		ImGui::Separator();

		for (int i = 0; i < (int)_columns.size(); i++)
		{
			auto& column = _columns[i];
			ImGui::Text("%s", column.label.c_str());

			ImGui::SetColumnOffset(-1, offset);
			offset += column.size;
			ImGui::NextColumn();
		}
		ImGui::Columns(1);
		ImGui::EndChild();
		ImGui::Separator();
	}

	int ITEMS_COUNT = (int)_triggers.size();
	auto dataGridHeight = 190.0f;

	if (ImGui::BeginChild("##ScrollingRegion", ImVec2(0, (int)dataGridHeight), false, ImGuiWindowFlags_HorizontalScrollbar)) {
		ImGui::Columns((int)_columns.size());

		ImGuiListClipper clipper(ITEMS_COUNT);
		while (clipper.Step()) {
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
				int j = 0;
				int itemIdx = i;
				auto& elmInfo = _triggers.at(itemIdx);

				auto rowBuffer = _cellBuffer;
				rowBuffer->cached = true;
				auto& trigger = _triggers[itemIdx];

				sprintf(rowBuffer->rowData[0].data, "%d", trigger.measureDuration);
				sprintf(rowBuffer->rowData[1].data, "%.0f", trigger.volumeChangedThreshold);
				sprintf(rowBuffer->rowData[2].data, "%.1f", trigger.priceChangedThreshold);
				sprintf(rowBuffer->rowData[3].data, "%.2f", trigger.miniumVolumeInBTC);
				if (trigger.symbolFilter[0] == 0) {
					sprintf(rowBuffer->rowData[4].data, "any");
				}
				else {
					sprintf(rowBuffer->rowData[4].data, "%s", &trigger.symbolFilter[0]);
				}

				offset = 0.0f;
				std::string selectableLabel = rowBuffer->rowData[j].data;
				selectableLabel.append("##");
				selectableLabel.append(std::to_string(itemIdx));
				if (ImGui::Selectable(selectableLabel.c_str(), itemIdx == _selectedItemIdx, ImGuiSelectableFlags_SpanAllColumns))
					_selectedItemIdx = itemIdx;

				ImGui::SetColumnOffset(j, offset);
				offset += _columns[j].size;
				ImGui::NextColumn();

				for (j++; j < (int)_columns.size(); j++) {
					ImGui::Text("%s", rowBuffer->rowData[j].data);
					ImGui::SetColumnOffset(j, offset);
					offset += _columns[j].size;
					ImGui::NextColumn();
				}
				ImGui::Separator();
			}
		}
		ImGui::Columns(1);
		ImGui::EndChild();
	}

	TriggerVolumeBaseItem* pItem = nullptr;
	int selectedItem = getSelectedItemIndex();
	int flags = 0;
	if (selectedItem >= 0 && selectedItem < (int)_triggers.size()) {
		pItem = &_triggers.at(selectedItem);
	}

	if (pItem) {
		ImGui::InputInt(_columns[0].label.c_str(), &pItem->measureDuration, 0, 0, flags);
		ImGui::Separator();
		ImGui::InputFloat(_columns[1].label.c_str(), &pItem->volumeChangedThreshold, 0, 0, 0, flags);
		ImGui::Separator();
		ImGui::InputFloat(_columns[2].label.c_str(), &pItem->priceChangedThreshold, 0, 0, 1, flags);
		ImGui::Separator();
		ImGui::InputFloat(_columns[3].label.c_str(), &pItem->miniumVolumeInBTC, 0, 0, 2, flags);
		ImGui::Separator();
		ImGui::InputText(_columns[4].label.c_str(), pItem->symbolFilter, sizeof(TriggerVolumeBaseItem::symbolFilter), ImGuiInputTextFlags_CharsUppercase);
		ImGui::Separator();
	}
	else {
		ImGui::LabelText(_columns[0].label.c_str(), "");
		ImGui::Separator();
		ImGui::LabelText(_columns[1].label.c_str(), "");
		ImGui::Separator();
		ImGui::LabelText(_columns[2].label.c_str(), "");
		ImGui::Separator();
		ImGui::LabelText(_columns[3].label.c_str(), "");
		ImGui::Separator();
		ImGui::LabelText(_columns[4].label.c_str(), "");
		ImGui::Separator();
	}

	if (ImGui::Button("add", ImVec2(100, 20))) {
		TriggerVolumeBaseItem editingItem;
		editingItem.measureDuration = 120;
		editingItem.miniumVolumeInBTC = 2;
		editingItem.priceChangedThreshold = -1;
		editingItem.volumeChangedThreshold = 600;
		editingItem.symbolFilter[0] = 0;

		_triggers.push_back(editingItem);
		_selectedItemIdx = (int)_triggers.size() - 1;
	}
	ImGui::SameLine();
	if (ImGui::Button("delete", ImVec2(100, 20))) {
		if (selectedItem >= 0 && selectedItem < (int)_triggers.size()) {
			if (selectedItem == (int)_triggers.size() - 1) {
				_selectedItemIdx--;
			}
			_triggers.erase(_triggers.begin() + selectedItem);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("clear", ImVec2(100, 20))) {
		_triggers.clear();
		_selectedItemIdx = -1;
	}
	ImGui::SameLine();
	if (ImGui::Button("apply&close", ImVec2(100, 20))) {
		// check all triggers is valid
		int i;
		std::stringstream ss;
		for (i = 0; i < (int)_triggers.size(); i++) {
			auto& trigger = _triggers[i];
			if (trigger.measureDuration <= 0) {
				ss << "row #" << (i + 1) << ", " << _columns[0].label.c_str() << " must be a positive value";
				break;
			}
			if (trigger.measureDuration % 60) {
				ss << "row #" << (i + 1) << ", " << _columns[0].label.c_str() << " must be a multiple of 60";
				break;
			}
			if (trigger.volumeChangedThreshold <= 0) {
				ss << "row #" << (i + 1) << ", " << _columns[1].label.c_str() << " must be a positive value";
				break;
			}
			if (trigger.miniumVolumeInBTC <= 0) {
				ss << "row #" << (i + 1) << ", " << _columns[3].label.c_str() << " must be a positive value";
				break;
			}
		}
		if (i == (int)_triggers.size()) {
			_errorString.clear();
			ImGui::CloseCurrentPopup();
			showWindow(false);
			if (_applyClickHandler) {
				_applyClickHandler(this);
			}
		}
		else {
			_firstShowErrorAt = ImGui::GetTime();
			_errorString = ss.str();
		}
		ImGui::GetTime();
	}
	ImGui::SameLine();
	if (ImGui::Button("close", ImVec2(100, 20))) {
		ImGui::CloseCurrentPopup();
		showWindow(false);
	}

	constexpr float errorDisplayTime = 5;
	constexpr float errorDisapearTime = 1.5;
	if (!_errorString.empty()) {
		auto currentTime = ImGui::GetTime();
		auto diffTime = currentTime - _firstShowErrorAt;
		if (diffTime < errorDisplayTime + errorDisapearTime) {
			float alpha = diffTime < errorDisplayTime ? 1.0f : 1.0f - (diffTime - errorDisplayTime) / errorDisapearTime;
			ImVec4 color(1.0f, 0, 0, alpha);
			ImGui::TextColored(color, "%s", _errorString.c_str());
		}
	}
}

int WxVolumeTriggers::getSelectedItemIndex() const {
	return _selectedItemIdx;
}

void WxVolumeTriggers::setOnApplyButtonClickHandler(ButtonClickEventHandler&& handler) {
	_applyClickHandler = handler;
}