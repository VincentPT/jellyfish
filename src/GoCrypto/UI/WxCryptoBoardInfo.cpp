#include "WxCryptoBoardInfo.h"

inline bool IS_INVALID_PRICE(const double& price) {
	return price <= 0;
}

inline bool IS_INVALID_VOL(const double& vol) {
	return vol == 0;
}

inline double computeBuy(const VolumePeriod& vol) {
	return vol.bought / (vol.bought + vol.sold);
}

inline int comparePrice(const double& price1, const double& price2) {
	if (IS_INVALID_PRICE(price1) && IS_INVALID_PRICE(price2)) {
		if (IS_INVALID_PRICE(price1)) {
			return -1;
		}
		if (IS_INVALID_PRICE(price2)) {
			return 1;
		}
		return 0;
	}

	if (price2 == price1) {
		return 0;
	}
	else if (price1 > price2) {
		return 1;
	}

	return -1;
}

inline int compareVol(const double& vol1, const double& vol2) {
	if (IS_INVALID_VOL(vol1) || IS_INVALID_VOL(vol2)) {
		if (IS_INVALID_VOL(vol1)) {
			return -1;
		}
		if (IS_INVALID_VOL(vol2)) {
			return 1;
		}
		return 0;
	}

	if (vol1 == vol2) {
		return 0;
	}
	else if (vol1 > vol2) {
		return 1;
	}

	return -1;
}

bool WxCryptoBoardInfo::checkValidSymbol(int i) {
	auto& item = _fixedItems->at(i);
	static std::string NA = "NA";
	return item.symbol.length() > 0 && item.symbol != NA;
}
bool WxCryptoBoardInfo::checkValidPrice(int i) {
	auto& item = _fixedItems->at(i);
	return !IS_INVALID_PRICE(item.price);
}
bool WxCryptoBoardInfo::checkValidVol(int i) {
	auto& item = _fixedItems->at(i);
	return !IS_INVALID_VOL(item.volume);
}
bool WxCryptoBoardInfo::checkValidPricePeriod(int i, int iOffset) {
	auto pValue = (double*)((char*)&_fixedItems->at(i) + _rawElmInfoOffsets[iOffset]);
	return !IS_INVALID_PRICE(*pValue);
}
bool WxCryptoBoardInfo::checkValidVolPeriod(int i, int iOffset) {
	auto pValue = (VolumePeriod*)((char*)&_fixedItems->at(i) + _rawElmInfoOffsets[iOffset]);
	return !IS_INVALID_VOL(pValue->bought + pValue->sold);
}
bool WxCryptoBoardInfo::checkValidBPSh(int i, int iOffset) {
	auto pValue = (VolumePeriod*)((char*)&_fixedItems->at(i) + _rawElmInfoOffsets[iOffset]);
	return !IS_INVALID_VOL(pValue->bought + pValue->sold);
}

bool WxCryptoBoardInfo::compareSymbol(int i1, int i2) {

	auto& item1 = _fixedItems->at(i1);
	auto& item2 = _fixedItems->at(i2);

	return item1.symbol < item2.symbol;
}

bool WxCryptoBoardInfo::comparePrice(int i1, int i2) {

	auto& item1 = _fixedItems->at(i1);
	auto& item2 = _fixedItems->at(i2);

	return item1.price < item2.price;
}
bool WxCryptoBoardInfo::compareVol(int i1, int i2) {

	auto& item1 = _fixedItems->at(i1);
	auto& item2 = _fixedItems->at(i2);

	return std::abs(item1.volume) < std::abs(item2.volume);
}
bool WxCryptoBoardInfo::comparePricePeriod(int i1, int i2, int iOffset) {

	auto pValue1 = (double*)((char*)&_fixedItems->at(i1) + _rawElmInfoOffsets[iOffset]);
	auto pValue2 = (double*)((char*)&_fixedItems->at(i2) + _rawElmInfoOffsets[iOffset]);

	return *pValue1 < *pValue2;
}
bool WxCryptoBoardInfo::compareVolPeriod(int i1, int i2, int iOffset) {

	auto pValue1 = (VolumePeriod*)((char*)&_fixedItems->at(i1) + _rawElmInfoOffsets[iOffset]);
	auto pValue2 = (VolumePeriod*)((char*)&_fixedItems->at(i2) + _rawElmInfoOffsets[iOffset]);

	return (pValue1->bought + pValue1->sold) < (pValue2->bought + pValue2->sold);
}

bool WxCryptoBoardInfo::compareVolBPSh(int i1, int i2, int iOffset) {
	auto pValue1 = (VolumePeriod*)((char*)&_fixedItems->at(i1) + _rawElmInfoOffsets[iOffset]);
	auto pValue2 = (VolumePeriod*)((char*)&_fixedItems->at(i2) + _rawElmInfoOffsets[iOffset]);

	return computeBuy(*pValue1) < computeBuy(*pValue2);
}

std::string WxCryptoBoardInfo::convert2StringForSymbol(int i) {
	return _fixedItems->at(_dataIndexcies[i]).symbol;
}
std::string WxCryptoBoardInfo::convert2StringForPrice(int i) {
	char buffer[32];
	auto price = _fixedItems->at(_dataIndexcies[i]).price;
	if (IS_INVALID_PRICE(price)) {
		return "N/A";
	}

	sprintf(buffer, "%.8f", price);
	return buffer;
}
std::string WxCryptoBoardInfo::convert2StringForPricePeriod(int i, int iOffset) {
	char buffer[32];
	auto pValue = (double*)((char*)&_fixedItems->at(_dataIndexcies[i]) + _rawElmInfoOffsets[iOffset]);
	auto price = *pValue;
	if (IS_INVALID_PRICE(price)) {
		return "N/A";
	}

	sprintf(buffer, "%.8f", price);
	return buffer;
}
std::string WxCryptoBoardInfo::convert2StringForVol(int i) {
	char buffer[32];
	auto vol = std::abs(_fixedItems->at(_dataIndexcies[i]).volume);
	if (IS_INVALID_VOL(vol)) {
		return "N/A";
	}

	sprintf(buffer, "%.8f", vol);
	return buffer;
}
std::string WxCryptoBoardInfo::convert2StringForVolPeriod(int i, int iOffset) {
	char buffer[32];
	auto pValue = (VolumePeriod*)((char*)&_fixedItems->at(_dataIndexcies[i]) + _rawElmInfoOffsets[iOffset]);
	auto vol = pValue->bought + pValue->sold;
	if ( IS_INVALID_VOL(vol)) {
		return "N/A";
	}

	sprintf(buffer, "%.8f", vol);
	return buffer;
}
std::string WxCryptoBoardInfo::convert2StringForBPSh(int i, int iOffset) {
	char buffer[32];
	auto pValue = (VolumePeriod*)((char*)&_fixedItems->at(_dataIndexcies[i]) + _rawElmInfoOffsets[iOffset]);
	if (IS_INVALID_VOL(pValue->bought + pValue->sold)) {
		return "N/A";
	}

	sprintf(buffer, "%0.2f %%", (computeBuy(*pValue) * 100));
	return buffer;
}

WxCryptoBoardInfo::WxCryptoBoardInfo() : _selected(-1), _fixedItems(nullptr)
{
	_window_flags |= ImGuiWindowFlags_NoTitleBar;
	_window_flags |= ImGuiWindowFlags_NoMove;
	_window_flags |= ImGuiWindowFlags_NoResize;
	_window_flags |= ImGuiWindowFlags_NoCollapse;
	_window_flags |= ImGuiWindowFlags_NoScrollbar;

	_columns = {
		{ "symbol", 50, 0 },
		{ "price", 80, 1 },
		{ "vol", 70, 2 },
		{ "p 30m", 80, 3 },
		{ "p 1h", 80, 4 },
		{ "p 2h", 80, 5 },
		{ "p 3h", 80, 6 },
		{ "p 4h", 80, 7 },
		{ "vol 30", 70, 8 },
		{ "vol 1h", 70, 9 },
		{ "vol 2h", 70, 10 },
		{ "vol 3h", 70, 11 },
		{ "vol 4h", 70, 12 },
		{ "BPV 30", 70, 13 },
		{ "BPV 1h", 70, 14 },
		{ "BPV 2h", 70, 15 },
		{ "BPV 3h", 70, 16 },
		{ "BPV 4h", 70, 17 },
	};

	_rawElmInfoOffsets = {
		offsetof(CryptoBoardElmInfo, symbol),
		offsetof(CryptoBoardElmInfo, price),
		offsetof(CryptoBoardElmInfo, volume),
		offsetof(CryptoBoardElmInfo, pricePeriod1),
		offsetof(CryptoBoardElmInfo, pricePeriod2),
		offsetof(CryptoBoardElmInfo, pricePeriod3),
		offsetof(CryptoBoardElmInfo, pricePeriod4),
		offsetof(CryptoBoardElmInfo, pricePeriod5),
		offsetof(CryptoBoardElmInfo, volPeriod1),
		offsetof(CryptoBoardElmInfo, volPeriod2),
		offsetof(CryptoBoardElmInfo, volPeriod3),
		offsetof(CryptoBoardElmInfo, volPeriod4),
		offsetof(CryptoBoardElmInfo, volPeriod5),
	};
	using namespace std::placeholders;
	_columnAdditionalInfo = {
		{
			bind(&WxCryptoBoardInfo::compareSymbol, this, _1, _2),
			bind(&WxCryptoBoardInfo::convert2StringForSymbol, this, _1),
			bind(&WxCryptoBoardInfo::checkValidSymbol, this, _1),
			SortType::NotSort,
		},
		{
			bind(&WxCryptoBoardInfo::comparePrice, this, _1, _2),
			bind(&WxCryptoBoardInfo::convert2StringForPrice, this, _1),
			bind(&WxCryptoBoardInfo::checkValidPrice, this, _1),
			SortType::NotSort,
		},
		{
			bind(&WxCryptoBoardInfo::compareVol, this, _1, _2),
			bind(&WxCryptoBoardInfo::convert2StringForVol, this, _1),
			bind(&WxCryptoBoardInfo::checkValidVol, this, _1),
			SortType::NotSort,
		},
		{
			bind(&WxCryptoBoardInfo::comparePricePeriod, this, _1, _2, 3),
			bind(&WxCryptoBoardInfo::convert2StringForPricePeriod, this, _1, 3),
			bind(&WxCryptoBoardInfo::checkValidPricePeriod, this, _1, 3),
			SortType::NotSort,
		},
		{
			bind(&WxCryptoBoardInfo::comparePricePeriod, this, _1, _2, 4),
			bind(&WxCryptoBoardInfo::convert2StringForPricePeriod, this, _1, 4),
			bind(&WxCryptoBoardInfo::checkValidPricePeriod, this, _1, 4),
			SortType::NotSort,
		},
		{
			bind(&WxCryptoBoardInfo::comparePricePeriod, this, _1, _2, 5),
			bind(&WxCryptoBoardInfo::convert2StringForPricePeriod, this, _1, 5),
			bind(&WxCryptoBoardInfo::checkValidPricePeriod, this, _1, 5),
			SortType::NotSort,
		},
		{
			bind(&WxCryptoBoardInfo::comparePricePeriod, this, _1, _2, 6),
			bind(&WxCryptoBoardInfo::convert2StringForPricePeriod, this, _1, 6),
			bind(&WxCryptoBoardInfo::checkValidPricePeriod, this, _1, 6),
			SortType::NotSort,
		},
		{
			bind(&WxCryptoBoardInfo::comparePricePeriod, this, _1, _2, 7),
			bind(&WxCryptoBoardInfo::convert2StringForPricePeriod, this, _1, 7),
			bind(&WxCryptoBoardInfo::checkValidPricePeriod, this, _1, 7),
			SortType::NotSort,
		},
		{
			bind(&WxCryptoBoardInfo::compareVolPeriod, this, _1, _2, 8),
			bind(&WxCryptoBoardInfo::convert2StringForVolPeriod, this, _1, 8),
			bind(&WxCryptoBoardInfo::checkValidVolPeriod, this, _1, 8),
			SortType::NotSort,
		},
		{
			bind(&WxCryptoBoardInfo::compareVolPeriod, this, _1, _2, 9),
			bind(&WxCryptoBoardInfo::convert2StringForVolPeriod, this, _1, 9),
			bind(&WxCryptoBoardInfo::checkValidVolPeriod, this, _1, 9),
			SortType::NotSort,
		},
		{
			bind(&WxCryptoBoardInfo::compareVolPeriod, this, _1, _2, 10),
			bind(&WxCryptoBoardInfo::convert2StringForVolPeriod, this, _1, 10),
			bind(&WxCryptoBoardInfo::checkValidVolPeriod, this, _1, 10),
			SortType::NotSort,
		},
		{
			bind(&WxCryptoBoardInfo::compareVolPeriod, this, _1, _2, 11),
			bind(&WxCryptoBoardInfo::convert2StringForVolPeriod, this, _1, 11),
			bind(&WxCryptoBoardInfo::checkValidVolPeriod, this, _1, 11),
			SortType::NotSort,
		},
		{
			bind(&WxCryptoBoardInfo::compareVolPeriod, this, _1, _2, 12),
			bind(&WxCryptoBoardInfo::convert2StringForVolPeriod, this, _1, 12),
			bind(&WxCryptoBoardInfo::checkValidVolPeriod, this, _1, 12),
			SortType::NotSort,
		},
		{
			bind(&WxCryptoBoardInfo::compareVolBPSh, this, _1, _2, 8),
			bind(&WxCryptoBoardInfo::convert2StringForBPSh, this, _1, 8),
			bind(&WxCryptoBoardInfo::checkValidBPSh, this, _1, 8),
			SortType::NotSort,
		},
		{
			bind(&WxCryptoBoardInfo::compareVolBPSh, this, _1, _2, 9),
			bind(&WxCryptoBoardInfo::convert2StringForBPSh, this, _1, 9),
			bind(&WxCryptoBoardInfo::checkValidBPSh, this, _1, 9),
			SortType::NotSort,
		},
		{
			bind(&WxCryptoBoardInfo::compareVolBPSh, this, _1, _2, 10),
			bind(&WxCryptoBoardInfo::convert2StringForBPSh, this, _1, 10),
			bind(&WxCryptoBoardInfo::checkValidBPSh, this, _1, 10),
			SortType::NotSort,
		},
		{
			bind(&WxCryptoBoardInfo::compareVolBPSh, this, _1, _2, 11),
			bind(&WxCryptoBoardInfo::convert2StringForBPSh, this, _1, 11),
			bind(&WxCryptoBoardInfo::checkValidBPSh, this, _1, 11),
			SortType::NotSort,
		},
		{
			bind(&WxCryptoBoardInfo::compareVolBPSh, this, _1, _2, 12),
			bind(&WxCryptoBoardInfo::convert2StringForBPSh, this, _1, 12),
			bind(&WxCryptoBoardInfo::checkValidBPSh, this, _1, 12),
			SortType::NotSort,
		},
	};

	float totalSize = 0;
	for (auto& column : _columns) {
		totalSize += column.size;
	}

	auto& style = ImGui::GetStyle();
	//setSize((int)(_columnWidth.x * _columns.size() + (int)(2* style.WindowPadding.x)), 500);

	setSize(totalSize + 2 * style.WindowPadding.x, 0);
	setPos(0, 0);
}

void WxCryptoBoardInfo::onSort(int columnIdx) {
	// set all other cloumns's sort type is not sort
	for (size_t i = 0; i < _columns.size(); i++) {
		auto& columInfo = _columnAdditionalInfo[i];
		if (i != columnIdx) {
			columInfo.sortType = SortType::NotSort;
		}
		else {
			if (columInfo.sortType == SortType::NotSort) {
				columInfo.sortType = SortType::Accessding;
			}
			else if (columInfo.sortType == SortType::Accessding) {
				columInfo.sortType = SortType::Descending;
			}
			else if (columInfo.sortType == SortType::Descending) {
				columInfo.sortType = SortType::Accessding;
			}
		}
	}
	auto& columInfo = _columnAdditionalInfo[columnIdx];
	auto& compareFunc = columInfo.compare;
	auto& checkValid = columInfo.checkValid;
	auto sortType = columInfo.sortType;

	auto invalidPartion = std::partition(_dataIndexcies.begin(), _dataIndexcies.end(), [&checkValid](int i) {
		return checkValid(i);
	});

	if (sortType == SortType::Accessding) {
		std::sort(_dataIndexcies.begin(), invalidPartion, compareFunc);
	}
	else {
		std::sort(_dataIndexcies.begin(), invalidPartion, [compareFunc](int i1, int i2) {
			return compareFunc(i2, i1);
		});
	}
}


WxCryptoBoardInfo::~WxCryptoBoardInfo()
{
}

//extern ImGuiContext*           GImGui;

void WxCryptoBoardInfo::update() {
	ImGui::SetNextWindowSize(_window_size, ImGuiCond_Always);
	ImGui::SetNextWindowPos(_window_pos);

	if (!ImGui::Begin("Symbols's information", nullptr, _window_flags))
	{
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	//ImGui::SetNextWindowContentSize(ImVec2(0, 25));
	float itemHeight = ImGui::GetTextLineHeightWithSpacing();
	ImGui::BeginChild("##HeaderRegion", ImVec2(0, itemHeight), false, ImGuiWindowFlags_NoScrollbar);
	ImGui::Columns((int)_columns.size(), "symbols", true);
	ImGui::Separator();
	float offset = 0.0f;
	for (int i = 0; i < (int)_columns.size(); i++)
	{
		auto& column = _columns[i];
		//ImGui::Text(column.label);
		if (ImGui::Button(column.label, ImVec2(column.size, 0))) {
			pushLog("push button %s\n", column.label);
			onSort(column.additionalIdx);
		}
		ImGui::SetColumnOffset(-1, offset);
		offset += column.size;
		ImGui::NextColumn();
	}
	ImGui::Separator();
	ImGui::Columns(1);
	ImGui::EndChild();

	auto oldSelection = _selected;

	if (_fixedItems) {
		int ITEMS_COUNT = (int)_fixedItems->size();
		auto windowHeight = ImGui::GetWindowHeight() - itemHeight - 10;
		//ImGui::SetNextWindowContentSize(ImVec2(0, 0));
		ImGui::BeginChild("##ScrollingRegion", ImVec2(0, windowHeight), false, ImGuiWindowFlags_HorizontalScrollbar);
		ImGui::Columns(_columns.size());

		ImGuiListClipper clipper(ITEMS_COUNT);  // Also demonstrate using the clipper for large list

		while (clipper.Step()) {
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
				offset = 0.0f;
				int j = 0;
				auto& elmInfo = _fixedItems->at(_dataIndexcies[i]);
				
				// symbol id
				if (ImGui::Selectable(elmInfo.symbol.c_str(), i == _selected, ImGuiSelectableFlags_SpanAllColumns))
					_selected = i;
				
				ImGui::SetColumnOffset(j, offset);
				offset += _columns[j].size;
				ImGui::NextColumn();

				for (j++; j < (int)_columns.size(); j++) {
					auto& columnInfo = _columnAdditionalInfo[_columns[j].additionalIdx];
					auto str = (columnInfo.toString)(i);
					ImGui::Text("%s", str.c_str());
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
	//else {
	//	int ITEMS_COUNT = 200;
	//	auto windowHeight = ImGui::GetWindowHeight() - itemHeight - 10;
	//	//ImGui::SetNextWindowContentSize(ImVec2(0, 0));
	//	ImGui::BeginChild("##ScrollingRegion", ImVec2(0, windowHeight), false, ImGuiWindowFlags_HorizontalScrollbar);
	//	ImGui::Columns(_columns.size());

	//	ImGuiListClipper clipper(ITEMS_COUNT);  // Also demonstrate using the clipper for large list

	//	char label[32];
	//	while (clipper.Step()) {
	//		for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
	//			offset = 0.0f;
	//			int j = 0;
	//			//sprintf(label, "%04d", i);
	//			sprintf(label, "N/A");

	//			if (ImGui::Selectable(label, i == _selected, ImGuiSelectableFlags_SpanAllColumns)) {
	//				_selected = i;
	//			}

	//			ImGui::SetColumnOffset(j, offset);
	//			offset += _columns[j].size;
	//			ImGui::NextColumn();

	//			for (j++; j < (int)_columns.size(); j++) {
	//				//ImGui::Text("%d:%d", i, j);
	//				ImGui::Text("N/A", i, j);
	//				ImGui::SetColumnOffset(j, offset);
	//				offset += _columns[j].size;
	//				ImGui::NextColumn();
	//			}
	//			ImGui::Separator();
	//		}
	//	}
	//	ImGui::Columns(1);
	//	ImGui::EndChild();
	//}

	ImGui::End();

	if (oldSelection != _selected) {
		if (onItemSelectedChanged) {
			onItemSelectedChanged(this);
		}
	}
}

void WxCryptoBoardInfo::setItems(const std::vector<CryptoBoardElmInfo>* fixedItems) {
	_fixedItems = fixedItems;
	_dataIndexcies.resize(_fixedItems->size());

	for (int i = 0; i < (int)_dataIndexcies.size(); i++) {
		_dataIndexcies[i] = i;
	}
}

void WxCryptoBoardInfo::setItemSelectionChangedHandler(ItemSelecionChangedHandler&& handler) {
	onItemSelectedChanged = handler;
}

const char* WxCryptoBoardInfo::getSelectedSymbol() const {
	if (_selected < 0 || _fixedItems == nullptr || _selected >= _fixedItems->size()) {
		return nullptr;
	}

	return _fixedItems->at(_selected).symbol.c_str();
}