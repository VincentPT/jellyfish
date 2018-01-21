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