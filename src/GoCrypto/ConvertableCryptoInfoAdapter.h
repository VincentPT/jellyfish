#pragma once
#include "UI\WxCryptoBoardInfo.h"

class ConvertableCryptoInfoAdapter : public CryptoBoardInfoDefaultAdapter {
public:
	ConvertableCryptoInfoAdapter(const std::vector<int>& rawElmInfoOffsets);
	virtual~ConvertableCryptoInfoAdapter();
	virtual bool comparePrice(int i1, int i2);
	virtual bool compareVol(int i1, int i2);
	virtual bool comparePricePeriod(int i1, int i2, int iOffset);
	virtual bool compareVolPeriod(int i1, int i2, int iOffset);

	virtual std::string convert2StringForSymbol(int i);
	virtual std::string convert2StringForPrice(int i);
	virtual std::string convert2StringForVol(int i);
	virtual std::string convert2StringForPricePeriod(int i, int iOffset);
	virtual std::string convert2StringForVolPeriod(int i, int iOffset);
};