#pragma once


#ifdef _WIN32
#ifdef EXPORT_TRADING_BASE
#define TRADING_PLATFORM_API __declspec(dllexport)
#else
#define TRADING_PLATFORM_API __declspec(dllimport)
#endif // EXPORT_TRADING_BASE
#else
#define TRADING_PLATFORM_API
#endif // _WIN32
