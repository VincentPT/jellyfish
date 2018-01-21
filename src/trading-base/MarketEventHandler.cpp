#include <Windows.h>
#include "MarketEventHandler.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <sstream>
using namespace std;

void formatTime(char* buffer, size_t bufferSize, TIMESTAMP t) {
	time_t tst = (time_t)(t / 1000);
	time_t miliseconds = t % 1000;

	struct tm * timeinfo;
	timeinfo = localtime(&tst);

	// print format of time equivalent to Thu Aug 23 14:55:02
	strftime(buffer, bufferSize, "%a %b %d %T.", timeinfo);
	auto len = strlen(buffer);
	char* end = &buffer[len];
	if (bufferSize > len + 4) {
		sprintf_s(end, 4, "%03d", (int)miliseconds);
		if (bufferSize > len + 10) {
			sprintf_s(end + 3, 6, " %04d", (int)(timeinfo->tm_year + 1900));
		}
	}
}

string time2str(TIMESTAMP t) {
	char buffer[64];
	formatTime(buffer, sizeof(buffer), t);
	return buffer;
}

TIMESTAMP getCurrentTimeStamp() {
	using namespace std::chrono;
	auto now = system_clock::now();
	auto now_ms = time_point_cast<milliseconds>(now);
	return now_ms.time_since_epoch().count();
}

MarketEventHandler::MarketEventHandler(const char* pair) :
	_useTicker(true), _useOrderBooks(false),
	_usedTrade(false), _useCandle(false)
{
	auto len = strlen(pair);
	_pair = (char*)malloc(len + 1);
	memcpy_s(_pair, len + 1, pair, len + 1);
}


MarketEventHandler::~MarketEventHandler()
{
	free(_pair);
}

const char* MarketEventHandler::getPair() const {
	return _pair;
}

bool MarketEventHandler::useTicker() {
	return _useTicker;
}
bool MarketEventHandler::useOrderBook() {
	return _useOrderBooks;
}
bool MarketEventHandler::useTrades() {
	return _usedTrade;
}
bool MarketEventHandler::useCandles() {
	return _useCandle;
}

void MarketEventHandler::useTicker(bool flag) {
	_useTicker = flag;
}
void MarketEventHandler::useOrderBook(bool flag) {
	_useOrderBooks = flag;
}
void MarketEventHandler::useTrades(bool flag) {
	_usedTrade = flag;
}
void MarketEventHandler::useCandles(bool flag) {
	_useCandle = flag;
}

void MarketEventHandler::onTickerUpdate(Ticker& ticker) {
	cout << _pair << "'s ticker: {" << ticker.bid << ", " << ticker.ask << ", " << ticker.lastPrice << ", " << ticker.volume << ", " << ticker.high << ", " << ticker.low << "," << time2str(ticker.timestamp) << "}"<< endl;
}

void printBookItems(const char* pair, const char* sideName, BookItem* bookItems, int count) {
	BookItem* pItemEnd = bookItems + count;
	if (count > 1) {
		cout << pair << "'s "<<sideName<< " begin " << endl;
		for (BookItem* pItem = bookItems; pItem < pItemEnd; pItem++) {
			BookItem& item = *pItem;
			cout << "{" << item.oderId << ", " << item.price << ", " << item.amount << "}" << endl;
		}
		cout << pair << "'s " << sideName << " end " << endl;
	}
	else if (count == 1) {
		BookItem& item = *bookItems;
		cout << pair << "'s " <<sideName << ": {" << item.oderId << ", " << item.price << ", " << item.amount << "}" << endl;
	}
}

void MarketEventHandler::onBooksUpdate(BookItemEventArgs* books, bool snapShot) {
	if (books == nullptr) return;
	if (snapShot) {
		cout << "books snapshot" << endl;
	}
	else {
		cout << "books update" << endl;
	}
	printBookItems(_pair, "ask", books->askItems, books->askCount);
	printBookItems(_pair, "bid", books->bidItems, books->bidCount);
}

void MarketEventHandler::onTradesUpdate(TradeItem* trades, int count, bool snapShot) {
	if (snapShot) {
		cout << "trades snapshot" << endl;
	}
	else {
		cout << "trades update" << endl;
	}
	if (trades == nullptr) return;

	TradeItem* pItemEnd = trades + count;
	
	for (TradeItem* pItem = trades; pItem < pItemEnd; pItem++) {
		TradeItem& item = *pItem;
		cout << getPair() << ": ";
		if (item.amount < 0) {
			cout << "shell ";
		}
		else {
			cout << "buy ";
		}
		cout << "{" << item.oderId << ", " << time2str(item.timestamp) << ", " << item.price << ", " << item.amount << "}" << endl;
	}
}

void MarketEventHandler::onCandlesUpdate(CandleItem* candles, int count, bool snapShot) {
	if (snapShot) {
		cout << "candles snapshot" << endl;
	}
	else {
		cout << "candles update" << endl;
	}
	if (candles == nullptr) return;

	CandleItem* pItemEnd = candles + count;
	if (count > 1) {
		cout << _pair << "'s candles " << endl;
		for (CandleItem* pItem = candles; pItem < pItemEnd; pItem++) {
			CandleItem& item = *pItem;
			cout << "{" << time2str(item.timestamp) << ", " << item.open << ", " << item.close << ", " << item.high << ", " << item.low << ", " << item.volume << "}" << endl;
		}
		cout << _pair << "'s candles end " << endl;
	}
	else if (count == 1) {
		CandleItem& item = *candles;
		cout << _pair << "'s candles: {" << time2str(item.timestamp) << ", " << item.open << ", " << item.close << ", " << item.high << ", " << item.low << ", " << item.volume << "}" << endl;
	}
}

void MarketEventHandler::onTimeUpdate(TIMESTAMP t) {
	cout << "time:" << time2str(t) << endl;
}