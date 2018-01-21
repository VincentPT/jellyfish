#pragma once

#include <string>
#include "TradingPlatform.h"

class Utility
{
public:
	Utility();
	~Utility();

	static std::wstring toWideString(const std::string& str);
	static std::string toString(const std::wstring& str);
	static std::string toMultibytes(const std::wstring& str);
	static std::string time2str(TIMESTAMP t);
};

#ifdef _WIN32
#define CPPREST_TO_STRING(str) Utility::toWideString(str)
#define CPPREST_FROM_STRING(str) Utility::toMultibytes(str)
#else
#define CPPREST_TO_STRING(str) (str)
#define CPPREST_FROM_STRING(str) (str)
#endif // _WIN32



