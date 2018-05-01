#include "WxPriceTriggers.h"
#include <sstream>

WxPriceTriggers::WxPriceTriggers() : ImPopup("Volume triggers editor") {
	_columns = {
		{ "start time(*)", 120, 0 },
		{ "end time(*)", 120, 1 },
		{ "price changed/min(*)", 180, 2 },
	};

	// there is a bug if scrollbar is enable
	// Luckily,for this window, we don't need scrollbars.
	_window_flags |= ImGuiWindowFlags_NoScrollbar;
	_window_flags |= ImGuiWindowFlags_NoResize;
	_cellBuffer = createRowBuffer((int)_columns.size());
}

WxPriceTriggers::~WxPriceTriggers() {
	free(_cellBuffer);
}

void WxPriceTriggers::setTriggers(const std::vector<TriggerPriceBase>& triggers) {
	std::unique_lock<std::mutex> lk(_mutex);
	_triggers = triggers;
	_selectedItemIdx = -1;
}

const std::vector<TriggerPriceBase>& WxPriceTriggers::getTriggers() const {
	return _triggers;
}

void WxPriceTriggers::clearTriggers() {
	_triggers.clear();
}

void WxPriceTriggers::updateContent()
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

				sprintf(rowBuffer->rowData[0].data, "%d", trigger.startTime);
				sprintf(rowBuffer->rowData[1].data, "%d", trigger.endTime);
				sprintf(rowBuffer->rowData[2].data, "%.4f", trigger.priceChangePerMin);

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

	TriggerPriceBase* pItem = nullptr;
	int selectedItem = getSelectedItemIndex();
	int flags = 0;
	if (selectedItem >= 0 && selectedItem < (int)_triggers.size()) {
		pItem = &_triggers.at(selectedItem);
	}

	if (pItem) {
		ImGui::InputInt(_columns[0].label.c_str(), &pItem->startTime, 0, 0, flags);
		ImGui::Separator();
		ImGui::InputInt(_columns[1].label.c_str(), &pItem->endTime, 0, 0, flags);
		ImGui::Separator();
		ImGui::InputFloat(_columns[2].label.c_str(), &pItem->priceChangePerMin, 0, 0, 4, flags);
		ImGui::Separator();
	}
	else {
		ImGui::LabelText(_columns[0].label.c_str(), "");
		ImGui::Separator();
		ImGui::LabelText(_columns[1].label.c_str(), "");
		ImGui::Separator();
		ImGui::LabelText(_columns[2].label.c_str(), "");
		ImGui::Separator();
	}

	if (ImGui::Button("add", ImVec2(100, 20))) {
		TriggerPriceBase editingItem;
		editingItem.startTime = 60;
		editingItem.endTime = 120;
		editingItem.priceChangePerMin = 2.0f;

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
		_triggers.clear() ;
		_selectedItemIdx = -1;
	}
	ImGui::SameLine();
	if (ImGui::Button("apply&close", ImVec2(100, 20))) {
		// check all triggers is valid
		int i;
		std::stringstream ss;
		for (i = 0; i < (int)_triggers.size(); i++) {
			auto& trigger = _triggers[i];
			if (trigger.startTime <= 0) {
				ss << "row #" << (i + 1) << ", " << _columns[0].label.c_str() << " must be a positive value";
				break;
			}
			if (trigger.endTime <= 0) {
				ss << "row #" << (i + 1) << ", " << _columns[1].label.c_str() << " must be a positive value";
				break;
			}
			if (trigger.priceChangePerMin <= 0) {
				ss << "row #" << (i + 1) << ", " << _columns[2].label.c_str() << " must be a positive value";
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

int WxPriceTriggers::getSelectedItemIndex() const {
	return _selectedItemIdx;
}

void WxPriceTriggers::setOnApplyButtonClickHandler(ButtonClickEventHandler&& handler) {
	_applyClickHandler = handler;
}