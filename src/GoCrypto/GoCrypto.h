#pragma once

struct CryptoBoardElmInfo {
	std::string symbol;
	double price;
	double volume;
	double pricePeriod1;
	double pricePeriod2;
	double pricePeriod3;
	double pricePeriod4;
	double pricePeriod5;
	double volPeriod1;
	double volPeriod2;
	double volPeriod3;
	double volPeriod4;
	double volPeriod5;

	CryptoBoardElmInfo() :
		price(-1),
		volume(0),
		pricePeriod1(-1),
		pricePeriod2(-1),
		pricePeriod3(-1),
		pricePeriod4(-1),
		pricePeriod5(-1),
		volPeriod1(0),
		volPeriod2(0),
		volPeriod3(0),
		volPeriod4(0),
		volPeriod5(0) {

	}
};

void pushLog(const char* fmt, ...);
