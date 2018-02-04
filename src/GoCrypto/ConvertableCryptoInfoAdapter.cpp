#include "ConvertableCryptoInfoAdapter.h"

ConvertableCryptoInfoAdapter::ConvertableCryptoInfoAdapter(const std::vector<int>& rawElmInfoOffsets) :CryptoBoardInfoDefaultAdapter(rawElmInfoOffsets){

}

ConvertableCryptoInfoAdapter::~ConvertableCryptoInfoAdapter() {
	for (auto it = _registerEvents.begin(); it != _registerEvents.end(); it++) {
		int eventId = it->second;
		it->first->removeTradeEventListener(eventId);
	}
}

bool ConvertableCryptoInfoAdapter::comparePrice(int i1, int i2) {
	auto& item1 = _convertedItems.at(i1);
	auto& item2 = _convertedItems.at(i2);

	return item1.price < item2.price;
}
bool ConvertableCryptoInfoAdapter::compareVol(int i1, int i2) {
	auto& item1 = _convertedItems.at(i1);
	auto& item2 = _convertedItems.at(i2);

	return std::abs(item1.volume) < std::abs(item2.volume);
}
bool ConvertableCryptoInfoAdapter::comparePricePeriod(int i1, int i2, int iOffset) {
	auto pValue1 = (double*)((char*)&_convertedItems.at(i1) + _rawElmInfoOffsets[iOffset]);
	auto pValue2 = (double*)((char*)&_convertedItems.at(i2) + _rawElmInfoOffsets[iOffset]);

	return *pValue1 < *pValue2;
}

bool ConvertableCryptoInfoAdapter::compareVolPeriod(int i1, int i2, int iOffset) {
	auto pValue1 = (VolumePeriod*)((char*)&_convertedItems.at(i1) + _rawElmInfoOffsets[iOffset]);
	auto pValue2 = (VolumePeriod*)((char*)&_convertedItems.at(i2) + _rawElmInfoOffsets[iOffset]);

	return (pValue1->bought + pValue1->sold) < (pValue2->bought + pValue2->sold);
}

std::string ConvertableCryptoInfoAdapter::convert2StringForPrice(int i) {
	char buffer[32];
	auto price = _convertedItems.at(i).price;
	if (IS_INVALID_PRICE(price)) {
		return "N/A";
	}

	sprintf(buffer, "%.8f", price);
	return buffer;
}
std::string ConvertableCryptoInfoAdapter::convert2StringForVol(int i) {
	char buffer[32];
	auto vol = std::abs(_convertedItems.at(i).volume);
	if (IS_INVALID_VOL(vol)) {
		return "N/A";
	}

	sprintf(buffer, "%.8f", vol);
	return buffer;
}
std::string ConvertableCryptoInfoAdapter::convert2StringForPricePeriod(int i, int iOffset) {

	char buffer[32];
	auto pValue = (double*)((char*)&_convertedItems.at(i) + _rawElmInfoOffsets[iOffset]);
	auto price = *pValue;
	if (IS_INVALID_PRICE(price)) {
		return "N/A";
	}

	sprintf(buffer, "%.8f", price);
	return buffer;
}
std::string ConvertableCryptoInfoAdapter::convert2StringForVolPeriod(int i, int iOffset) {
	char buffer[32];
	auto pValue = (VolumePeriod*)((char*)&_convertedItems.at(i) + _rawElmInfoOffsets[iOffset]);
	auto vol = pValue->bought + pValue->sold;
	if (IS_INVALID_VOL(vol)) {
		return "N/A";
	}

	sprintf(buffer, "%.8f", vol);
	return buffer;
}

bool ConvertableCryptoInfoAdapter::checkValidPrice(int i) {
	auto& item = _convertedItems.at(i);
	return !IS_INVALID_PRICE(item.price);
}
bool ConvertableCryptoInfoAdapter::checkValidVol(int i) {
	auto& item = _convertedItems.at(i);
	return !IS_INVALID_VOL(item.volume);
}
bool ConvertableCryptoInfoAdapter::checkValidPricePeriod(int i, int iOffset) {
	auto pValue = (double*)((char*)&_convertedItems.at(i) + _rawElmInfoOffsets[iOffset]);
	return !IS_INVALID_PRICE(*pValue);
}
bool ConvertableCryptoInfoAdapter::checkValidVolPeriod(int i, int iOffset) {
	auto pValue = (VolumePeriod*)((char*)&_convertedItems.at(i) + _rawElmInfoOffsets[iOffset]);
	return !IS_INVALID_VOL(pValue->bought + pValue->sold);
}

void ConvertableCryptoInfoAdapter::updateElement(int elmIdx) {
	double priceFactor;
	int i = elmIdx;

	auto& crytoInfo = _convertedItems[i];
	auto& originCrytoInfo = _fixedItems->at(i);
	auto& symbol = originCrytoInfo.symbol;

	if (!IS_INVALID_PRICE(originCrytoInfo.price) && convertPrice(symbol, originCrytoInfo.price, crytoInfo.price)) {
		priceFactor = crytoInfo.price / originCrytoInfo.price;
		crytoInfo.pricePeriods[0] = originCrytoInfo.pricePeriods[0] * priceFactor;
		crytoInfo.pricePeriods[1] = originCrytoInfo.pricePeriods[1] * priceFactor;
		crytoInfo.pricePeriods[2] = originCrytoInfo.pricePeriods[2] * priceFactor;
		crytoInfo.pricePeriods[3] = originCrytoInfo.pricePeriods[3] * priceFactor;

		auto& quoteCurency = getQuote(symbol);
		if (quoteCurency.empty()) {
			crytoInfo.volume = 0;
			VolumePeriod* pVol = crytoInfo.volPeriods;
			VolumePeriod* pEnd = (VolumePeriod*)((char*)pVol + sizeof(crytoInfo.volPeriods));
			for (; pVol < pEnd; pVol++) {
				*pVol = VolumePeriod();
			}
		}
		else {
			crytoInfo.volume = originCrytoInfo.volume * crytoInfo.price;

			VolumePeriod* pVol = crytoInfo.volPeriods;
			const VolumePeriod* pVolOriginal = originCrytoInfo.volPeriods;
			VolumePeriod* pEnd = (VolumePeriod*)((char*)pVol + sizeof(crytoInfo.volPeriods));
			for (; pVol < pEnd; pVol++, pVolOriginal++) {
				pVol->bought = pVolOriginal->bought * crytoInfo.price;
				pVol->sold = pVolOriginal->sold * crytoInfo.price;
			}
		}
	}
	else {
		crytoInfo.price = -1;
		crytoInfo.volume = 0;
		crytoInfo.pricePeriods[0] = -1;
		crytoInfo.pricePeriods[1] = -1;
		crytoInfo.pricePeriods[2] = -1;
		crytoInfo.pricePeriods[3] = -1;
		VolumePeriod* pVol = crytoInfo.volPeriods;
		VolumePeriod* pEnd = (VolumePeriod*)((char*)pVol + sizeof(crytoInfo.volPeriods));
		for (; pVol < pEnd; pVol++) {
			*pVol = VolumePeriod();
		}
	}
}

void ConvertableCryptoInfoAdapter::onTradeEvent(NAPMarketEventHandler* sender, TradeItem* tradeItem, int, bool) {
	auto pair = sender->getPair();
	auto it = _symbolIndexMap.find(pair);
	if (it == _symbolIndexMap.end()) {
		pushLog("Something wrong, cannot find trade item to update for %s", pair);
		return;
	}

	updateElement(it->second);
}

void ConvertableCryptoInfoAdapter::setItems(const std::vector<CryptoBoardElmInfo>* fixedItems) {
	CryptoBoardInfoDefaultAdapter::setItems(fixedItems);

	// reset map from symbol to index
	_symbolIndexMap.clear();

	if (fixedItems == nullptr) {
		_convertedItems.clear();
	}
	else {
		_convertedItems.resize(fixedItems->size());

		// initialize the map
		for (int i = 0; i < (int)_fixedItems->size(); i++) {
			auto& cryptoInfo = _fixedItems->at(i);
			_symbolIndexMap[cryptoInfo.symbol] = i;
		}
		// convert all element at the first time
		for (int i = 0; i < (int)_convertedItems.size(); i++) {
			updateElement(i);
		}
	}
}

void ConvertableCryptoInfoAdapter::setCurrency(const std::string& currency) {
	_currentCurrency = currency;
}

const std::string& ConvertableCryptoInfoAdapter::getCurrency() const {
	return _currentCurrency;
}

const std::string& ConvertableCryptoInfoAdapter::getQuote(const std::string& symbol) {
	auto& currencies = _engine->getCurrencies();
	for (auto it = currencies.begin(); it != currencies.end(); it++) {
		if (symbol.compare(symbol.size() - it->size(), it->size(), *it) == 0) {
			return *it;
		}
	}

	static std::string emptyStr;
	return emptyStr;
}

bool ConvertableCryptoInfoAdapter::convertPrice(const std::string& symbol, const double& price, double& convertedPrice) {
	auto& quoteCurency = getQuote(symbol);
	if (quoteCurency.empty()) {
		return false;
	}
	if (quoteCurency == _currentCurrency) {
		convertedPrice = price;
		return true;
	}

	auto newSymbol = quoteCurency + _currentCurrency;
	auto it = _symbolIndexMap.find(newSymbol);
	bool reverse = false;
	if (it == _symbolIndexMap.end()) {
		newSymbol = _currentCurrency + quoteCurency;
		it = _symbolIndexMap.find(newSymbol);
		if (it == _symbolIndexMap.end()) {
			return false;
		}
		reverse = true;
	}

	int i = it->second;
	auto& originCrytoInfo = _fixedItems->at(i);
	auto& newPrice = originCrytoInfo.price;
	if (reverse) {
		convertedPrice = price / newPrice;
	}
	else {
		convertedPrice = price * newPrice;
	}

	return true;
}

void ConvertableCryptoInfoAdapter::intialize(const std::vector<CryptoBoardElmInfo>* fixedItems, PlatformEngine* engine) {
	using namespace std::placeholders;
	_engine = engine;
	setItems(fixedItems);

	auto platform = _engine->getPlatform();
	int nHandler = platform->getHandlerCount();
	std::vector<MarketEventHandler*> handlers(nHandler);
	platform->getHandlers(handlers.data(), nHandler);

	for (auto it = handlers.begin(); it != handlers.end(); it++) {
		auto handler = (NAPMarketEventHandler*)*it;
		auto tradeEventHandler = std::bind(&ConvertableCryptoInfoAdapter::onTradeEvent, this, _1, _2, _3, _4);
		int eventId = handler->addTradeEventListener(tradeEventHandler);
		_registerEvents.emplace_back(std::make_pair(handler, eventId));
	}
}