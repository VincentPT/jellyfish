#ifdef WIN32
#include <Windows.h>
#endif // WIN32


#include "Utility.h"
#include <algorithm>
#include <string>
#include <locale>
#include <codecvt>

Utility::Utility()
{
}


Utility::~Utility()
{
}
 
std::wstring Utility::toWideString(const std::string& str) {
	std::wstring ws(str.begin(), str.end());
	return ws;
}

std::string Utility::toString(const std::wstring& wstr) {
	std::string s(wstr.begin(), wstr.end());
	return s;
}

std::string Utility::toMultibytes(const std::wstring& wstr) {
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	return converter.to_bytes(wstr);
}

std::string Utility::time2str(TIMESTAMP t) {
	char buffer[64];
	formatTime(buffer, sizeof(buffer), t);
	return buffer;
}

std::string Utility::time2shortStr(TIMESTAMP t) {
	char buffer[64];
	time_t tst = (time_t)(t / 1000);

	struct tm * timeinfo;
	timeinfo = localtime(&tst);

	// print format of time equivalent to Thu Aug 23 14:55:02
	strftime(buffer, sizeof(buffer), "%T", timeinfo);
	
	return buffer;
}

void Utility::logScopeAccess(ILogger* logger, const char* functionName, bool access) {
	if (logger) {
		if (access) {
			logger->logV(LogLevel::Debug, ">>> %s\n", functionName);
		}
		else {
			logger->logV(LogLevel::Debug, "<<< %s\n", functionName);
		}
	}
}

unsigned int Utility::getDoubleClickTime() {
#ifdef WIN32
	return GetDoubleClickTime();
#endif
}