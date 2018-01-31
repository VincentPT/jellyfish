#pragma once

#include <string>
#include "TradingPlatform.h"


template <class T>
struct Range {
	T start;
	T end;
};

///////////////////////////////////////////////////////////////////////////
/// compute new range and overlapped range after merge two range
/// the two input rangse must have end >= start
/// 
///
/// the overlapped range
/// member 'start' is beginning of overlapped range
///
/// member 'end' is terminating of overlapped range
/// if start smaller than end means two ranges has overlapped region began at
/// start and terminated at end
///
/// if start greater than end means two ranges has missing region began at
/// start and terminated at end
///
/// if start equal to end means terminate point of a range is beginning point of the other range	
///
///////////////////////////////////////////////////////////////////////////
template <class T>
void mergeParts(const Range<T>& part1, const Range<T>& part2,
	Range<T>* newRange = nullptr, Range<T>* overlap = nullptr) {

	auto& t1 = part1.start;
	auto& t2 = part1.end;
	auto& t3 = part2.start;
	auto& t4 = part2.end;

	if (newRange) {
		newRange->start = std::min(t1, t3);
		newRange->end = std::max(t2, t4);
	}
	
	if (overlap) {
		overlap->start = std::max(t1, t3);
		overlap->end = std::min(t2, t4);
	}
}

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
#define TO_STRING_T(a) std::to_wstring(a)
#else
#define CPPREST_TO_STRING(str) (str)
#define CPPREST_FROM_STRING(str) (str)
#define TO_STRING_T(a) std::to_string(a)
#endif // _WIN32



