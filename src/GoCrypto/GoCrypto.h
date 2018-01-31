#pragma once

struct VolumePeriod
{
	double bought;
	double sold;

	VolumePeriod() : bought(0), sold(0)
	{}
};

struct CryptoBoardElmInfo {
	std::string symbol;
	double price;
	double volume;
	double pricePeriod1;
	double pricePeriod2;
	double pricePeriod3;
	double pricePeriod4;
	VolumePeriod volPeriod1;
	VolumePeriod volPeriod2;
	VolumePeriod volPeriod3;
	VolumePeriod volPeriod4;

	CryptoBoardElmInfo() :
		price(-1),
		volume(0),
		pricePeriod1(-1),
		pricePeriod2(-1),
		pricePeriod3(-1),
		pricePeriod4(-1) {

	}
};

void pushLog(const char* fmt, ...);
