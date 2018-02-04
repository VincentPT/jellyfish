#include "ConvertableCryptoInfoAdapter.h"

ConvertableCryptoInfoAdapter::ConvertableCryptoInfoAdapter(const std::vector<int>& rawElmInfoOffsets) :CryptoBoardInfoDefaultAdapter(rawElmInfoOffsets){

}

ConvertableCryptoInfoAdapter::~ConvertableCryptoInfoAdapter() {

}

bool ConvertableCryptoInfoAdapter::comparePrice(int i1, int i2) {
	return false;
}
bool ConvertableCryptoInfoAdapter::compareVol(int i1, int i2) {
	return false;
}
bool ConvertableCryptoInfoAdapter::comparePricePeriod(int i1, int i2, int iOffset) {
	return false;
}

bool ConvertableCryptoInfoAdapter::compareVolPeriod(int i1, int i2, int iOffset) {
	return false;
}

std::string ConvertableCryptoInfoAdapter::convert2StringForSymbol(int i) {
	return "N/A";
}
std::string ConvertableCryptoInfoAdapter::convert2StringForPrice(int i) {
	return "N/A";
}
std::string ConvertableCryptoInfoAdapter::convert2StringForVol(int i) {
	return "N/A";
}
std::string ConvertableCryptoInfoAdapter::convert2StringForPricePeriod(int i, int iOffset) {
	return "N/A";
}
std::string ConvertableCryptoInfoAdapter::convert2StringForVolPeriod(int i, int iOffset) {
	return "N/A";
}