#include "WxCryptoBoardInfo.h"

CryptoBoardInfoDefaultAdapter::CryptoBoardInfoDefaultAdapter(const std::vector<int>& rawElmInfoOffsets) :_rawElmInfoOffsets(rawElmInfoOffsets) {

}
CryptoBoardInfoDefaultAdapter::~CryptoBoardInfoDefaultAdapter() {}

//////////////////////////////////////////////////////////////////////////
bool CryptoBoardInfoDefaultAdapter::checkValidSymbol(int i) {
	auto& item = _fixedItems->at(i);
	static std::string NA = "NA";
	return item->symbol && item->symbol != NA;
}
bool CryptoBoardInfoDefaultAdapter::checkValidPrice(int i) {
	auto& item = _fixedItems->at(i);
	return !IS_INVALID_PRICE(item->price);
}
bool CryptoBoardInfoDefaultAdapter::checkValidVol(int i) {
	auto& item = _fixedItems->at(i);
	return !IS_INVALID_VOL(item->volume);
}
bool CryptoBoardInfoDefaultAdapter::checkValidPricePeriod(int i, int iOffset) {
	auto pValue = (double*)((char*)_fixedItems->at(i) + _rawElmInfoOffsets[iOffset]);
	return !IS_INVALID_PRICE(*pValue);
}
bool CryptoBoardInfoDefaultAdapter::checkValidVolPeriod(int i, int iOffset) {
	auto pValue = (PeriodInfo*)((char*)_fixedItems->at(i) + _rawElmInfoOffsets[iOffset]);
	return !IS_INVALID_VOL(pValue->bought + pValue->sold);
}
bool CryptoBoardInfoDefaultAdapter::checkValidBPSh(int i, int iOffset) {
	auto pValue = (PeriodInfo*)((char*)_fixedItems->at(i) + _rawElmInfoOffsets[iOffset]);
	return !IS_INVALID_VOL(pValue->bought + pValue->sold);
}

bool CryptoBoardInfoDefaultAdapter::compareSymbol(int i1, int i2) {

	auto& item1 = _fixedItems->at(i1);
	auto& item2 = _fixedItems->at(i2);

	return strcmp(item1->symbol, item2->symbol) == -1;
}

bool CryptoBoardInfoDefaultAdapter::comparePrice(int i1, int i2) {

	auto& item1 = _fixedItems->at(i1);
	auto& item2 = _fixedItems->at(i2);

	return item1->price < item2->price;
}
bool CryptoBoardInfoDefaultAdapter::compareVol(int i1, int i2) {

	auto& item1 = _fixedItems->at(i1);
	auto& item2 = _fixedItems->at(i2);

	return std::abs(item1->volume) < std::abs(item2->volume);
}
bool CryptoBoardInfoDefaultAdapter::comparePricePeriod(int i1, int i2, int iOffset) {

	auto pValue1 = (double*)((char*)_fixedItems->at(i1) + _rawElmInfoOffsets[iOffset]);
	auto pValue2 = (double*)((char*)_fixedItems->at(i2) + _rawElmInfoOffsets[iOffset]);

	return *pValue1 < *pValue2;
}
bool CryptoBoardInfoDefaultAdapter::compareVolPeriod(int i1, int i2, int iOffset) {

	auto pValue1 = (PeriodInfo*)((char*)_fixedItems->at(i1) + _rawElmInfoOffsets[iOffset]);
	auto pValue2 = (PeriodInfo*)((char*)_fixedItems->at(i2) + _rawElmInfoOffsets[iOffset]);

	return (pValue1->bought + pValue1->sold) < (pValue2->bought + pValue2->sold);
}

bool CryptoBoardInfoDefaultAdapter::compareVolBPSh(int i1, int i2, int iOffset) {
	auto pValue1 = (PeriodInfo*)((char*)_fixedItems->at(i1) + _rawElmInfoOffsets[iOffset]);
	auto pValue2 = (PeriodInfo*)((char*)_fixedItems->at(i2) + _rawElmInfoOffsets[iOffset]);

	return computeBuy(*pValue1) < computeBuy(*pValue2);
}

void CryptoBoardInfoDefaultAdapter::updateCellBufferForPrice(char* buffer, size_t bufferSize, int i) {
	auto price = _fixedItems->at(i)->price;
	if (IS_INVALID_PRICE(price)) {
		strcpy_s(buffer, bufferSize, "N/A");
	}
	else {
		sprintf_s(buffer, bufferSize, "%.8f", price);
	}
}
void CryptoBoardInfoDefaultAdapter::updateCellBufferForPricePeriod(char* buffer, size_t bufferSize, int i, int iOffset) {
	auto pValue = (double*)((char*)_fixedItems->at(i) + _rawElmInfoOffsets[iOffset]);
	auto price = *pValue;
	if (IS_INVALID_PRICE(price)) {
		strcpy_s(buffer, bufferSize, "N/A");
	}
	else {
		sprintf_s(buffer, bufferSize, "%.8f", price);
	}
}
void CryptoBoardInfoDefaultAdapter::updateCellBufferForVol(char* buffer, size_t bufferSize, int i) {
	auto vol = std::abs(_fixedItems->at(i)->volume);
	if (IS_INVALID_VOL(vol)) {
		strcpy_s(buffer, bufferSize, "N/A");
	}
	else {
		sprintf_s(buffer, bufferSize, "%.8f", vol);
	}
}
void CryptoBoardInfoDefaultAdapter::updateCellBufferForVolPeriod(char* buffer, size_t bufferSize, int i, int iOffset) {
	auto pValue = (PeriodInfo*)((char*)_fixedItems->at(i) + _rawElmInfoOffsets[iOffset]);
	auto vol = pValue->bought + pValue->sold;
	if (IS_INVALID_VOL(vol)) {
		strcpy_s(buffer, bufferSize, "N/A");
	}
	else {
		sprintf_s(buffer, bufferSize, "%.8f", vol);
	}
}
void CryptoBoardInfoDefaultAdapter::updateCellBufferForBPSh(char* buffer, size_t bufferSize, int i, int iOffset) {
	auto pValue = (PeriodInfo*)((char*)_fixedItems->at(i) + _rawElmInfoOffsets[iOffset]);
	if (IS_INVALID_VOL(pValue->bought + pValue->sold)) {
		strcpy_s(buffer, bufferSize, "N/A");
	}
	else {
		sprintf_s(buffer, bufferSize, "%0.2f %%", (computeBuy(*pValue) * 100));
	}
}
void CryptoBoardInfoDefaultAdapter::updateData() {}

////////////////////////////////////////////////////////////////////////

inline RowBuffer* createRowBuffer(int nCell) {
	auto res = (RowBuffer*)malloc(sizeof(RowBuffer) - sizeof(RowBuffer::rowData) + nCell * sizeof(RowBuffer::rowData[0]));
	res->nCell = nCell;
	res->cached = false;
	return res;
}


bool WxCryptoBoardInfo::checkValidSymbol(int i) {
	return _cryptoBoardInfoAdapter->checkValidSymbol(i);
}
bool WxCryptoBoardInfo::checkValidPrice(int i) {
	return _cryptoBoardInfoAdapter->checkValidPrice(i);
}
bool WxCryptoBoardInfo::checkValidVol(int i) {
	return _cryptoBoardInfoAdapter->checkValidVol(i);
}
bool WxCryptoBoardInfo::checkValidPricePeriod(int i, int iOffset) {
	return _cryptoBoardInfoAdapter->checkValidPricePeriod(i, iOffset);
}
bool WxCryptoBoardInfo::checkValidVolPeriod(int i, int iOffset) {
	return _cryptoBoardInfoAdapter->checkValidVolPeriod(i, iOffset);
}
bool WxCryptoBoardInfo::checkValidBPSh(int i, int iOffset) {
	return _cryptoBoardInfoAdapter->checkValidBPSh(i, iOffset);
}

bool WxCryptoBoardInfo::compareSymbol(int i1, int i2) {
	return _cryptoBoardInfoAdapter->compareSymbol(i1, i2);
}

bool WxCryptoBoardInfo::comparePrice(int i1, int i2) {
	return _cryptoBoardInfoAdapter->comparePrice(i1, i2);
}
bool WxCryptoBoardInfo::compareVol(int i1, int i2) {
	return _cryptoBoardInfoAdapter->compareVol(i1, i2);
}
bool WxCryptoBoardInfo::comparePricePeriod(int i1, int i2, int iOffset) {
	return _cryptoBoardInfoAdapter->comparePricePeriod(i1, i2, iOffset);
}
bool WxCryptoBoardInfo::compareVolPeriod(int i1, int i2, int iOffset) {
	return _cryptoBoardInfoAdapter->compareVolPeriod(i1, i2, iOffset);
}

bool WxCryptoBoardInfo::compareVolBPSh(int i1, int i2, int iOffset) {
	return _cryptoBoardInfoAdapter->compareVolBPSh(i1, i2, iOffset);
}

void WxCryptoBoardInfo::updateCellBufferForPrice(char* buffer, size_t bufferSize, int i) {
	_cryptoBoardInfoAdapter->updateCellBufferForPrice(buffer, bufferSize,  i);
}
void WxCryptoBoardInfo::updateCellBufferForPricePeriod(char* buffer, size_t bufferSize, int i, int iOffset) {
	_cryptoBoardInfoAdapter->updateCellBufferForPricePeriod(buffer, bufferSize, i, iOffset);
}
void WxCryptoBoardInfo::updateCellBufferForVol(char* buffer, size_t bufferSize, int i) {
	_cryptoBoardInfoAdapter->updateCellBufferForVol(buffer, bufferSize, i);
}
void WxCryptoBoardInfo::updateCellBufferForVolPeriod(char* buffer, size_t bufferSize, int i, int iOffset) {
	_cryptoBoardInfoAdapter->updateCellBufferForVolPeriod(buffer, bufferSize, i, iOffset);
}
void WxCryptoBoardInfo::updateCellBufferForBPSh(char* buffer, size_t bufferSize, int i, int iOffset) {
	_cryptoBoardInfoAdapter->updateCellBufferForBPSh(buffer, bufferSize, i, iOffset);
}

WxCryptoBoardInfo::WxCryptoBoardInfo() : _selected(-1), _fixedItems(nullptr)
{
	_window_flags |= ImGuiWindowFlags_NoTitleBar;
	_window_flags |= ImGuiWindowFlags_NoMove;
	_window_flags |= ImGuiWindowFlags_NoResize;
	_window_flags |= ImGuiWindowFlags_NoCollapse;
	_window_flags |= ImGuiWindowFlags_NoScrollbar;

	setPeriod(_periods);
	setPos(0, 0);

	resetCryptoAdapterToDefault();
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
	destroy(_cellBuffers);
}

void WxCryptoBoardInfo::update() {
	FUNCTON_LOG();

	std::unique_lock<std::mutex> lk(_mutex);

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

	// for adapter prepares data before display
	_cryptoBoardInfoAdapter->updateData();

	for (int i = 0; i < (int)_columns.size(); i++)
	{
		auto& column = _columns[i];
		//ImGui::Text(column.label);
		if (ImGui::Button(column.label.c_str(), ImVec2(column.size, 0))) {
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
				int col;
				int itemIdx = _dataIndexcies[i];
				auto& elmInfo = _fixedItems->at(itemIdx);
				
				// symbol id
				if (ImGui::Selectable(elmInfo->symbol, itemIdx == _selected, ImGuiSelectableFlags_SpanAllColumns))
					_selected = itemIdx;
				
				ImGui::SetColumnOffset(j, offset);
				offset += _columns[j].size;
				ImGui::NextColumn();

				auto& rowBuffer = _cellBuffers[itemIdx];

				if (rowBuffer->cached == false) {
					for (col = 0; col < rowBuffer->nCell; col++) {
						auto& columnInfo = _columnAdditionalInfo[_columns[col + 1].additionalIdx];
						(columnInfo.updateBuffer)(rowBuffer->rowData[col].data, sizeof(rowBuffer->rowData[col].data), itemIdx);
					}

					rowBuffer->cached = true;
				}

				for (j++, col = 0; j < (int)_columns.size(); j++, col++) {
					auto& columnInfo = _columnAdditionalInfo[_columns[j].additionalIdx];
					ImGui::Text("%s", rowBuffer->rowData[col].data);
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

	ImGui::End();

	if (oldSelection != _selected) {
		if (onItemSelectedChanged) {
			onItemSelectedChanged(this);
		}
	}
}

void WxCryptoBoardInfo::accessSharedData(const AccessSharedDataFunc& f) {
	std::unique_lock<std::mutex> lk(_mutex);
	f(this);
}

void WxCryptoBoardInfo::setItems(const std::vector<CryptoBoardElmInfo*>* fixedItems) {
	destroy(_cellBuffers);

	if (fixedItems == nullptr) {
		_fixedItems = nullptr;
		_dataIndexcies.resize(0);
		_cellBuffers.resize(0);
	}
	else {
		_fixedItems = fixedItems;
		_dataIndexcies.resize(_fixedItems->size());
		_cellBuffers.resize(_fixedItems->size());

		for (int i = 0; i < (int)_dataIndexcies.size(); i++) {
			_dataIndexcies[i] = i;
			_cellBuffers[i] = createRowBuffer(3 + _periods.size() * 3 - 1);
		}
	}
	_cryptoBoardInfoAdapter->setItems(_fixedItems);
}

const std::vector<CryptoBoardElmInfo*>* WxCryptoBoardInfo::getItems() const {
	return _fixedItems;
}

void WxCryptoBoardInfo::setPeriod(const std::vector<Period>& periods) {
	std::unique_lock<std::mutex> lk(_mutex);

	_periods = periods;

	int nPeriod = (int)periods.size();

	_columns = {
		{ "symbol", 50, 0 },
		{ "price", 80, 1 },
		{ "vol", 70, 2 }
	};
	for (int i = 0; i < nPeriod; i++) {
		ColumnHeader header = { "p " + _periods[i].name, 80, 3 + i };
		_columns.push_back(header);
	}
	for (int i = 0; i < nPeriod; i++) {
		ColumnHeader header = { "vol " + _periods[i].name, 70,  3 + nPeriod + i };
		_columns.push_back(header);
	}
	for (int i = 0; i < nPeriod; i++) {
		ColumnHeader header = { "BPV " + _periods[i].name, 70, 3 + 2 * nPeriod + i };
		_columns.push_back(header);
	}

	constexpr auto volPeriodsOffset = offsetof(CryptoBoardElmInfo, periods);
	constexpr auto pricePeriodsOffset = offsetof(PeriodInfo, averagePrice);

	_rawElmInfoOffsets = {
		offsetof(CryptoBoardElmInfo, symbol),
		offsetof(CryptoBoardElmInfo, price),
		offsetof(CryptoBoardElmInfo, volume),
	};
	for (int i = 0; i < nPeriod; i++) {
		_rawElmInfoOffsets.push_back(volPeriodsOffset + sizeof(PeriodInfo) * i + pricePeriodsOffset);
	}
	for (int i = 0; i < nPeriod; i++) {
		_rawElmInfoOffsets.push_back(volPeriodsOffset + sizeof(PeriodInfo) * i);
	}

	using namespace std::placeholders;
	_columnAdditionalInfo = {
		{
			bind(&WxCryptoBoardInfo::compareSymbol, this, _1, _2),
			nullptr,
			bind(&WxCryptoBoardInfo::checkValidSymbol, this, _1),
			SortType::NotSort,
		},
		{
			bind(&WxCryptoBoardInfo::comparePrice, this, _1, _2),
			bind(&WxCryptoBoardInfo::updateCellBufferForPrice, this, _1, _2, _3),
			bind(&WxCryptoBoardInfo::checkValidPrice, this, _1),
			SortType::NotSort,
		},
		{
			bind(&WxCryptoBoardInfo::compareVol, this, _1, _2),
			bind(&WxCryptoBoardInfo::updateCellBufferForVol, this, _1, _2, _3),
			bind(&WxCryptoBoardInfo::checkValidVol, this, _1),
			SortType::NotSort,
		},
	};

	for (int i = 0; i < nPeriod; i++) {
		ColumnInfoExt columnInfo = {
			bind(&WxCryptoBoardInfo::comparePricePeriod, this, _1, _2, 3 + i),
			bind(&WxCryptoBoardInfo::updateCellBufferForPricePeriod, this, _1, _2, _3, 3 + i),
			bind(&WxCryptoBoardInfo::checkValidPricePeriod, this, _1, 3 + i),
			SortType::NotSort,
		};

		_columnAdditionalInfo.push_back(columnInfo);
	}
	for (int i = 0; i < nPeriod; i++) {
		ColumnInfoExt columnInfo = {
			bind(&WxCryptoBoardInfo::compareVolPeriod, this, _1, _2, 3 + nPeriod + i),
			bind(&WxCryptoBoardInfo::updateCellBufferForVolPeriod, this, _1, _2, _3, 3 + nPeriod + i),
			bind(&WxCryptoBoardInfo::checkValidVolPeriod, this, _1, 3 + nPeriod + i),
			SortType::NotSort,
		};

		_columnAdditionalInfo.push_back(columnInfo);
	}
	for (int i = 0; i < nPeriod; i++) {
		ColumnInfoExt columnInfo = {
			bind(&WxCryptoBoardInfo::compareVolBPSh, this, _1, _2, 3 + nPeriod + i),
			bind(&WxCryptoBoardInfo::updateCellBufferForBPSh, this, _1, _2, _3, 3 + nPeriod + i),
			bind(&WxCryptoBoardInfo::checkValidBPSh, this, _1, 3 + nPeriod + i),
			SortType::NotSort,
		};

		_columnAdditionalInfo.push_back(columnInfo);
	}

	//float totalSize = 0;
	//for (auto& column : _columns) {
	//	totalSize += column.size;
	//}
}
void WxCryptoBoardInfo::setItemSelectionChangedHandler(ItemSelecionChangedHandler&& handler) {
	onItemSelectedChanged = handler;
}

const char* WxCryptoBoardInfo::getSelectedSymbol() const {
	if (_selected < 0 || _fixedItems == nullptr || _selected >= _fixedItems->size()) {
		return nullptr;
	}
	return _fixedItems->at(_selected)->symbol;
}

int WxCryptoBoardInfo::getSelectedSymbolIndex() const {
	return _selected;
}

void WxCryptoBoardInfo::resetCryptoAdapterToDefault() {
	auto adapter = std::make_shared<CryptoBoardInfoDefaultAdapter>(_rawElmInfoOffsets);
	setAdapter(adapter);
}

const std::vector<int>& WxCryptoBoardInfo::getRawElemInfoOffsets() const {
	return _rawElmInfoOffsets;
}

void WxCryptoBoardInfo::setAdapter(std::shared_ptr<CryptoBoardInfoModeAdapterBase> adapter) {
	_cryptoBoardInfoAdapter = adapter;
	refreshCached(-1);
}

void WxCryptoBoardInfo::refreshCached(int symbolIndex) {
	if (symbolIndex < (int)_cellBuffers.size()) {
		if (symbolIndex < 0) {
			for (auto& rowBuffer : _cellBuffers) {
				rowBuffer->cached = false;
			}
		}
		else {
			auto& rowBuffer = _cellBuffers[symbolIndex];
			rowBuffer->cached = false;
		}
	}
}