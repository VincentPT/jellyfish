#pragma once
#include "UI/WxCryptoBoardInfo.h"
#include "Engine/PlatformEngine.h"
#include <map>
#include <list>

class ConvertableCryptoInfoAdapter : public CryptoBoardInfoDefaultAdapter {
	std::string _currentCurrency;
	PlatformEngine* _engine;
	std::vector<CryptoBoardElmInfo> _convertedItems;
	std::map<std::string, int> _symbolIndexMap;
	std::list<std::pair<NAPMarketEventHandler*, int>> _registerEvents;
private:
	void onTradeEvent(NAPMarketEventHandler* sender, TradeItem* tradeItem, int, bool);
	void updateElement(int elmIdx);
protected:
	virtual void setItems(const std::vector<CryptoBoardElmInfo>* fixedItems);
public:
	ConvertableCryptoInfoAdapter(const std::vector<int>& rawElmInfoOffsets);
	virtual~ConvertableCryptoInfoAdapter();
	virtual bool comparePrice(int i1, int i2);
	virtual bool compareVol(int i1, int i2);
	virtual bool comparePricePeriod(int i1, int i2, int iOffset);
	virtual bool compareVolPeriod(int i1, int i2, int iOffset);

	virtual std::string convert2StringForPrice(int i);
	virtual std::string convert2StringForVol(int i);
	virtual std::string convert2StringForPricePeriod(int i, int iOffset);
	virtual std::string convert2StringForVolPeriod(int i, int iOffset);
	
	virtual bool checkValidPrice(int i);
	virtual bool checkValidVol(int i);
	virtual bool checkValidPricePeriod(int i, int iOffset);
	virtual bool checkValidVolPeriod(int i, int iOffset);
	
	void setCurrency(const std::string&);
	const std::string& getCurrency() const;
	bool convertPrice(const std::string& symbol, const double& price, double& convertedPrice);
	const std::string& getQuote(const std::string& symbol);
	void intialize(const std::vector<CryptoBoardElmInfo>* fixedItems, PlatformEngine* engine);
};