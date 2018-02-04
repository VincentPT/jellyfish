#pragma once
#include "ImWidget.h"
#include "GoCrypto.h"
#include <functional>
#include <mutex>

struct ColumnHeader
{
	const char* label;
	float size;
	int additionalIdx;
}; 

typedef std::function<void(Widget* sender)> ItemSelecionChangedHandler;
typedef std::function<int(int i1, int i2)> CompareFunction;
typedef std::function<bool(int i)> CheckDataValidFunction;
typedef std::function<std::string(int i)> ToStringFunc;

enum class SortType {
	NotSort = 0,
	Accessding,
	Descending,
};

struct ColumnInfoExt {
	CompareFunction compare;
	ToStringFunc toString;
	CheckDataValidFunction checkValid;
	SortType sortType;
};

class WxCryptoBoardInfo :
	public ImWidget
{
	std::vector<ColumnHeader> _columns;
	std::vector<ColumnInfoExt> _columnAdditionalInfo;
	const std::vector<CryptoBoardElmInfo>* _fixedItems;
	std::vector<int> _dataIndexcies;
	int _selected;

	std::vector<int> _rawElmInfoOffsets;
	std::mutex _mutex;
private:
	ItemSelecionChangedHandler onItemSelectedChanged;
private:
	bool compareSymbol(int i1, int i2);
	bool comparePrice(int i1, int i2);
	bool compareVol(int i1, int i2);
	bool comparePricePeriod(int i1, int i2, int iOffset);
	bool compareVolPeriod(int i1, int i2, int iOffset);
	bool compareVolBPSh(int i1, int i2, int iOffset);

	std::string convert2StringForSymbol(int i);
	std::string convert2StringForPrice(int i);
	std::string convert2StringForVol(int i);
	std::string convert2StringForPricePeriod(int i, int iOffset);
	std::string convert2StringForVolPeriod(int i, int iOffset);
	std::string convert2StringForBPSh(int i, int iOffset);

	bool checkValidSymbol(int i);
	bool checkValidPrice(int i);
	bool checkValidVol(int i);
	bool checkValidPricePeriod(int i, int iOffset);
	bool checkValidVolPeriod(int i, int iOffset);
	bool checkValidBPSh(int i, int iOffset);

	void onSort(int columnIdx);
public:
	WxCryptoBoardInfo();
	virtual ~WxCryptoBoardInfo();

	virtual void update();

	void accessSharedData(const AccessSharedDataFunc&);
	virtual void setItems(const std::vector<CryptoBoardElmInfo>* fixedItems);
	virtual void setItemSelectionChangedHandler(ItemSelecionChangedHandler&& handler);
	const char* getSelectedSymbol() const;
};

