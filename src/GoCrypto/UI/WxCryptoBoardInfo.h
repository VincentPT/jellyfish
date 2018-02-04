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

class CryptoBoardInfoModeAdapterBase {
protected:
	const std::vector<CryptoBoardElmInfo>* _fixedItems;
public:
	virtual bool compareSymbol(int i1, int i2) = 0;
	virtual bool comparePrice(int i1, int i2) = 0;
	virtual bool compareVol(int i1, int i2) = 0;
	virtual bool comparePricePeriod(int i1, int i2, int iOffset) = 0;
	virtual bool compareVolPeriod(int i1, int i2, int iOffset) = 0;
	virtual bool compareVolBPSh(int i1, int i2, int iOffset) = 0;

	virtual std::string convert2StringForSymbol(int i) = 0;
	virtual std::string convert2StringForPrice(int i) = 0;
	virtual std::string convert2StringForVol(int i) = 0;
	virtual std::string convert2StringForPricePeriod(int i, int iOffset) = 0;
	virtual std::string convert2StringForVolPeriod(int i, int iOffset) = 0;
	virtual std::string convert2StringForBPSh(int i, int iOffset) = 0;

	virtual bool checkValidSymbol(int i) = 0;
	virtual bool checkValidPrice(int i) = 0;
	virtual bool checkValidVol(int i) = 0;
	virtual bool checkValidPricePeriod(int i, int iOffset) = 0;
	virtual bool checkValidVolPeriod(int i, int iOffset) = 0;
	virtual bool checkValidBPSh(int i, int iOffset) = 0;
	virtual void updateData() = 0;
	virtual void setItems(const std::vector<CryptoBoardElmInfo>* fixedItems) {
		_fixedItems = fixedItems;
	}
};

class CryptoBoardInfoDefaultAdapter : public CryptoBoardInfoModeAdapterBase {
protected:
	const std::vector<int>& _rawElmInfoOffsets;
public:
	CryptoBoardInfoDefaultAdapter(const std::vector<int>& rawElmInfoOffsets);
	virtual~CryptoBoardInfoDefaultAdapter();
	virtual bool compareSymbol(int i1, int i2);
	virtual bool comparePrice(int i1, int i2);
	virtual bool compareVol(int i1, int i2);
	virtual bool comparePricePeriod(int i1, int i2, int iOffset);
	virtual bool compareVolPeriod(int i1, int i2, int iOffset);
	virtual bool compareVolBPSh(int i1, int i2, int iOffset);

	virtual std::string convert2StringForSymbol(int i);
	virtual std::string convert2StringForPrice(int i);
	virtual std::string convert2StringForVol(int i);
	virtual std::string convert2StringForPricePeriod(int i, int iOffset);
	virtual std::string convert2StringForVolPeriod(int i, int iOffset);
	virtual std::string convert2StringForBPSh(int i, int iOffset);

	virtual bool checkValidSymbol(int i);
	virtual bool checkValidPrice(int i);
	virtual bool checkValidVol(int i);
	virtual bool checkValidPricePeriod(int i, int iOffset);
	virtual bool checkValidVolPeriod(int i, int iOffset);
	virtual bool checkValidBPSh(int i, int iOffset);

	virtual void updateData();
};


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



class WxCryptoBoardInfo :
	public ImWidget
{
	std::vector<ColumnHeader> _columns;
	std::vector<ColumnInfoExt> _columnAdditionalInfo;
	const std::vector<CryptoBoardElmInfo>* _fixedItems;
	std::vector<int> _dataIndexcies;
	int _selected;

	std::shared_ptr<CryptoBoardInfoModeAdapterBase> _cryptoBoardInfoAdapter;

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
	virtual const std::vector<CryptoBoardElmInfo>* getItems() const;
	virtual void setItemSelectionChangedHandler(ItemSelecionChangedHandler&& handler);
	const char* getSelectedSymbol() const;
	void resetCryptoAdapterToDefault();
	const std::vector<int>& getRawElemInfoOffsets() const;
	void setAdapter(std::shared_ptr<CryptoBoardInfoModeAdapterBase> adapter);
};

