#include "NAPMarketEventHandler.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <sstream>
#include <limits>
#include "GoCrypto.h"
using namespace std;

NAPMarketEventHandler::NAPMarketEventHandler(TIMESTAMP tickerDuration, const char* pair) :
	MarketEventHandler(pair), _tag(nullptr), _tickerDuration(tickerDuration), _autoId(1)
{
}

NAPMarketEventHandler::~NAPMarketEventHandler()
{
}

void NAPMarketEventHandler::onTickerUpdate(Ticker& ticker) {}

void NAPMarketEventHandler::onBooksUpdate(BookItemEventArgs* books, bool snapShot) {}

void NAPMarketEventHandler::onCandlesUpdate(CandleItem* candles, int count, bool snapShot) {}

void NAPMarketEventHandler::onTradesUpdate(TradeItem* trades, int count, bool snapShot) {
	//{
	//	TradeItem* pItemEnd = trades + count;
	//	for (TradeItem* pItem = trades; pItem < pItemEnd; pItem++) {
	//		TradeItem& item = *pItem;
	//		pushLog(getPair());
	//		if (item.amount < 0) {
	//			pushLog(":shell ");
	//		}
	//		else {
	//			pushLog(":buy ");
	//		}
	//		pushLog("{ %lld , %s, %lf, %lf }\n", item.oderId, time2str(item.timestamp).c_str(), item.price, item.amount);
	//	}
	//}
	std::unique_lock<std::mutex> lk(_m);

	if (trades == nullptr) return;
	if (_tradeHistory.size() == 0) {
		addEmptyTicker((trades + count - 1)->timestamp);
	}

	SimpleTicker* currentTicker = &_tradeHistory.front();

	TradeItem* pItemEnd = trades + count;
	auto pItem = pItemEnd - 1;

	if (currentTicker->firstPrice.at > pItem->timestamp) {
		currentTicker->firstPrice.at = pItem->timestamp;
		if (_tradeHistory.size() >= 2) {
			// current ticker
			auto it = _tradeHistory.begin();
			// previous ticker
			it++;
			it->lastPrice.at = currentTicker->firstPrice.at - 1;
		}
	}

	auto timeBase = currentTicker->firstPrice.at;

	for (; pItem >= trades; pItem--) {
		TradeItem& item = *pItem;
		auto duration = item.timestamp - timeBase;
		if (duration > _tickerDuration) {
			addEmptyTicker(item.timestamp);
			currentTicker = &_tradeHistory.front();
			timeBase = item.timestamp;
		}

		auto& firstPrice = currentTicker->firstPrice;
		auto& lastPrice = currentTicker->lastPrice;
		auto& high = currentTicker->high;
		auto& low = currentTicker->low;

		if (item.amount > 0) {
			currentTicker->boughtVolume += item.amount;
		}
		else {
			currentTicker->soldVolume += abs(item.amount);
		}

		if (high.price < item.price) {
			high.price = item.price;
			high.at = item.timestamp;
		}
		if (low.price > item.price) {
			low.price = item.price;
			low.at = item.timestamp;
		}

		if (firstPrice.price <= 0) {
			firstPrice.price = item.price;
			firstPrice.at = item.timestamp;
			currentTicker->averagePrice = firstPrice.price;

			// update duration for previous ticker and also
			// update average price the ticker
			// this will not correct if the connection to server is delayed or interupted
			if (_tradeHistory.size() >= 2) {
				// current ticker
				auto it = _tradeHistory.begin();
				// previous ticker
				it++;
				auto& prevTicker = *it;
				auto& prevFirstPrice = prevTicker.firstPrice;
				auto& prevLastPrice = prevTicker.lastPrice;

				auto sumPriceBeforeLastPrice = prevTicker.averagePrice * (prevLastPrice.at - prevFirstPrice.at);
				auto lastPriceTolNow = prevLastPrice.price * (item.timestamp - prevLastPrice.at);

				prevTicker.averagePrice = (sumPriceBeforeLastPrice + lastPriceTolNow) / (item.timestamp - prevFirstPrice.at);
				// update time for last price last to current time before it change to current price
				prevLastPrice.at = item.timestamp;
			}
		}
		else {
			// update average price for current ticker
			if (item.timestamp > firstPrice.at) {
				auto sumPriceBeforeLastPrice = currentTicker->averagePrice * (lastPrice.at - firstPrice.at);
				auto lastPriceTolNow = lastPrice.price * (item.timestamp - lastPrice.at);

				currentTicker->averagePrice = (sumPriceBeforeLastPrice + lastPriceTolNow) / (item.timestamp - firstPrice.at);
			}
			else {
				currentTicker->averagePrice = firstPrice.price;
			}

			// leave the calculation average price for the current trade item for the next interval event
		}

		lastPrice.price = pItem->price;
		lastPrice.at = pItem->timestamp;
	}

	for (pItem = pItemEnd - 1; pItem >= trades; pItem--) {
		for (auto it = _tradeEventListeners.begin(); it != _tradeEventListeners.end(); it++) {
			(it->second) (this, pItem);
		}
	}
}

void NAPMarketEventHandler::accessSharedData(const AccessEventDataFunc& f) {
	std::unique_lock<std::mutex> lk(_m);
	f(this);
}

const std::list<SimpleTicker>& NAPMarketEventHandler::getTickerHistoriesNonSync() const {
	return _tradeHistory;
}

std::list<SimpleTicker>& NAPMarketEventHandler::getTickerHistoriesNonSync() {
	return _tradeHistory;
}

void NAPMarketEventHandler::addEmptyTicker(TIMESTAMP t) {
	double lastPrice = -1;
	if (_tradeHistory.size()) {
		auto& lastPricePoint = _tradeHistory.front().lastPrice;
		lastPrice = lastPricePoint.price;

		// update previous last price last to now
		lastPricePoint.at = t - 1;
	}

	SimpleTicker ticker;

	ticker.firstPrice.price = lastPrice;
	ticker.firstPrice.at = t;

	ticker.lastPrice.price = lastPrice;
	ticker.lastPrice.at = t;

	ticker.high.price = lastPrice;
	ticker.high.at = t;
	if (lastPrice > 0) {
		ticker.low.price = lastPrice;
	}
	else {
		ticker.low.price = std::numeric_limits<double>::max();
	}
	ticker.low.at = t;

	ticker.averagePrice = lastPrice;
	ticker.boughtVolume = 0;
	ticker.soldVolume = 0;
	ticker.processLevel = -1;

	if (_tradeHistory.size() == MAX_RECORDS_COUNT) {
		_tradeHistory.pop_back();
	}

	_tradeHistory.emplace_front(ticker);

	//pushLog("{ ticker rew new at %s }\n", time2str(t).c_str());
}

void NAPMarketEventHandler::onTimeUpdate(TIMESTAMP t) {
	std::unique_lock<std::mutex> lk(_m);
	addEmptyTicker(t);
}

void NAPMarketEventHandler::setTag(void*p) {
	_tag = p;
}
void* NAPMarketEventHandler::getTag() {
	return _tag;
}

int NAPMarketEventHandler::addTradeEventListener(TradeEventListener&& eventListener) {
	std::unique_lock<std::mutex> lk(_m);
	TradeEventListener emptyFunc;
	auto emptyPair = std::make_pair(_autoId, emptyFunc);
	auto it = _tradeEventListeners.insert(emptyPair);
	it.first->second = eventListener;

	_autoId++;

	return emptyPair.first;
}

void NAPMarketEventHandler::removeTradeEventListener(int id) {
	std::unique_lock<std::mutex> lk(_m);
	_tradeEventListeners.erase(id);
}