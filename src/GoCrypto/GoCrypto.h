#pragma once

struct VolumePeriod
{
	double bought = 0;
	double sold = 0;
};

struct CryptoBoardElmInfo {
	std::string symbol;
	double price = -1;
	double volume = 0;
	double pricePeriods[4] = { -1 };
	VolumePeriod volPeriods[5];
};

void pushLog(const char* fmt, ...);

#define DEFAULT_CURRENCY "As is"
