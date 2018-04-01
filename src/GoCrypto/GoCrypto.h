#pragma once
#include <vector>
#include <string>

struct Period
{
	std::string name;
	int durationInSecs;
};

struct PeriodInfo
{
	double bought = 0;
	double sold = 0;
	double averagePrice = -1;
	//PeriodInfo() : PeriodInfo(0,0) {}
	//PeriodInfo(const double& b, const double& s) : bought(b), sold(s) {}
};

struct CryptoBoardElmInfo {
	const char* symbol;
	double price = -1;
	double volume = 0;
	int nPeriod;
	PeriodInfo periods[1];
};

template <class T>
inline void destroy(std::vector<T*>& elms) {
	for (auto it = elms.begin(); it != elms.end(); it++) {
		free(*it);
	}
	elms.clear();
}

inline CryptoBoardElmInfo* createCrytpElm(int nPeriod) {
	auto res = (CryptoBoardElmInfo*)malloc(sizeof(CryptoBoardElmInfo) - sizeof(CryptoBoardElmInfo::periods) + nPeriod * sizeof(CryptoBoardElmInfo::periods[0]));
	res->symbol = nullptr;
	res->price = -1;
	res->volume = 0;
	res->nPeriod = nPeriod;

	auto pPeriod = res->periods;
	auto pEnd = pPeriod + nPeriod;
	for (; pPeriod != pEnd; pPeriod++) {
		*pPeriod = PeriodInfo();
	}

	return res;
}

void pushLog(int logLevel, const char* fmt, ...);

#define DEFAULT_CURRENCY "As is"
