#include "NAPMarketEventHandler.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <sstream>
#include <limits>
#include "GoCrypto.h"
#include "../common/Utility.h"
#include <algorithm>
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
#if 0
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
#else
	std::unique_lock<std::mutex> lk(_m);
	if (trades == nullptr) return;
	if (_tradeHistory.size() == 0) {
		TradeItem* pItemEnd = trades + count;
		for (auto pItem = trades; pItem < pItemEnd; pItem++) {
			_tradeHistory.push_back(*pItem);
		}
		//pushLog("snapshot trade event\n");
	}
	else if (count > 1) {
		auto& oldestItem = _tradeHistory.back();
		auto& newestItem = _tradeHistory.front();
		auto& incomOldestItem = *(trades + count - 1);
		auto& incomNewestItem = *trades;

		Range<TIMESTAMP> currentRange{ oldestItem.timestamp, newestItem.timestamp };
		Range<TIMESTAMP> newRange{ incomOldestItem.timestamp, incomNewestItem.timestamp };

		Range<TIMESTAMP> overlapped;
		mergeParts<TIMESTAMP>(currentRange, newRange, nullptr, &overlapped);

		// check if two part are not overlap
		if (overlapped.start > overlapped.end) {
			// check if oldest existence trade item occurs later than newest trade item on the snapshot event
			if (oldestItem.timestamp > incomNewestItem.timestamp) {
				TradeItem* pItemEnd = trades + count;
				for (auto pItem = trades; pItem < pItemEnd; pItem++) {
					_tradeHistory.push_back(*pItem);
				}
				pushLog("added trade event back\n");
			}
			else {
				TradeItem* pItemEnd = trades;
				for (auto pItem = trades + count - 1; pItem >= pItemEnd; pItem--) {
					_tradeHistory.push_front(*pItem);
				}
				pushLog("added trade event front\n");
			}
		}
		else if (overlapped.start == overlapped.end) {
			// now, the two time ranges have start and the end are same
			// check whether what range is newer
			if (newestItem.timestamp > incomNewestItem.timestamp) {
				// now the existance range is newer
				// then move item from

				// collect trade item at the overlapped time
				map<TRADE_ID, bool> itemsSameTime;
				for (auto it = _tradeHistory.rbegin(); it != _tradeHistory.rend() && it->timestamp == overlapped.start; it++) {
					itemsSameTime[it->oderId] = true;
				}

				// add items that occurs at the same time
				// so we need to avoid add duplicated item by check trade id
				TradeItem* pItemEnd = trades + count;
				auto pItem = trades;
				for (; pItem < pItemEnd && pItem->timestamp == overlapped.start; pItem++) {
					// check incoming trade item is not exist in existance trade history
					if (itemsSameTime.find(pItem->oderId) == itemsSameTime.end()) {
						_tradeHistory.push_back(*pItem);
					}
				}
				// add lhe left items that not exist in existance trade history
				for (; pItem < pItemEnd; pItem++) {
					_tradeHistory.push_back(*pItem);
				}

				//pushLog("merge trade event at the common time back\n");
			}
			else {
				// now the incoming range is newer
				// then move item from
				
				// collect trade item at the overlapped time
				map<TRADE_ID, bool> itemsSameTime;
				for (auto it = _tradeHistory.begin(); it != _tradeHistory.end() && it->timestamp == overlapped.start; it++) {
					itemsSameTime[it->oderId] = true;
				}

				// add items that occurs at the same time
				// so we need to avoid add duplicated item by check trade id
				TradeItem* pItemEnd = trades;
				auto pItem = trades + count - 1;
				for (; pItem >= pItemEnd && pItem->timestamp == overlapped.start; pItem--) {
					// check incoming trade item is not exist in existance trade history
					if (itemsSameTime.find(pItem->oderId) == itemsSameTime.end()) {
						_tradeHistory.push_front(*pItem);
					}
				}
				// add lhe left items that not exist in existance trade history
				for (; pItem >= pItemEnd; pItem--) {
					_tradeHistory.push_front(*pItem);
				}

				pushLog("merge trade event at the common time front\n");
			}
		}
		else {
			//if (newestItem.timestamp > overlapped.end) {

			//}
			//else if (oldestItem.timestamp < overlapped.start) {

			//}
			//else if (newestItem.timestamp == overlapped.end) {
			//	// collect trade item at the overlapped time
			//	map<TRADE_ID, bool> itemsSameTime;
			//	for (auto it = _tradeHistory.begin(); it != _tradeHistory.end() && it->timestamp == overlapped.end; it++) {
			//		itemsSameTime[it->oderId] = true;
			//	}

			//	// add items that occurs at the same time
			//	// so we need to avoid add duplicated item by check trade id
			//	TradeItem* pItemEnd = trades + count;
			//	auto pItem = trades;
			//	for (; pItem < pItemEnd && pItem->timestamp == overlapped.end; pItem++) {
			//		// check incoming trade item is not exist in existance trade history
			//		if (itemsSameTime.find(pItem->oderId) == itemsSameTime.end()) {
			//			_tradeHistory.push_front(*pItem);
			//		}
			//	}

			//	itemsSameTime.clear();
			//	for (auto it = _tradeHistory.rbegin(); it != _tradeHistory.rend() && it->timestamp == overlapped.end; it++) {
			//		itemsSameTime[it->oderId] = true;
			//	}

			//	// add lhe left items that not exist in existance trade history
			//	for (; pItem < pItemEnd; pItem++) {
			//		_tradeHistory.push_back(*pItem);
			//	}
			//}
			//else if (oldestItem.timestamp == overlapped.start) {

			//}
			//else if (newestItem.timestamp > incomNewestItem.timestamp) {
			//	// if the existance range cover incoming range entirely
			//	// then we don't need to update the range
			//}
			//else {
			//	// if the incomming range cover existance range entirely
			//	// then we use the whole incomming range instead existance range
			//	_tradeHistory.clear();
			//	TradeItem* pItemEnd = trades + count;
			//	for (auto pItem = trades; pItem < pItemEnd; pItem++) {
			//		_tradeHistory.push_back(*pItem);
			//	}
			//}

			// for simply implementation but decrease performance
			map<TRADE_ID, TradeItem*> itemsMap;

			// add non duplicate trade id for the two list
			for (auto it = _tradeHistory.begin(); it != _tradeHistory.end() && it->timestamp == overlapped.end; it++) {
				itemsMap[it->oderId] = &*it;
			}

			TradeItem* pItemEnd = trades + count;
			auto pItem = trades;
			for (; pItem < pItemEnd; pItem++) {
				itemsMap[pItem->oderId] = pItem;
			}

			// merge two list into one
			list<TradeItem> newList;
			for (auto it = itemsMap.begin(); it != itemsMap.end(); it++) {
				newList.push_back(*it->second);
			}

			// sort by timestap
			newList.sort([](TradeItem& item1, TradeItem& item2) {
				return item1.timestamp > item2.timestamp;
			});

			// clear existance trade history
			_tradeHistory.clear();

			// move new merged history to the trade history
			_tradeHistory.splice(_tradeHistory.begin(), newList, newList.begin(), newList.end());

			pushLog("merge trade event using general method\n");
		}
	}
	else {
		if (trades->timestamp == _tradeHistory.back().timestamp) {
			auto it = _tradeHistory.rbegin();
			for (; it != _tradeHistory.rend() && it->timestamp == trades->timestamp; it++) {
				if (it->oderId == trades->oderId) {
					break;
				}
			}
			if (it == _tradeHistory.rend() || it->timestamp != trades->timestamp) {
				_tradeHistory.push_back(*trades);
			}

			pushLog("update trade event 1\n");
		}
		else if (trades->timestamp == _tradeHistory.front().timestamp) {
			auto it = _tradeHistory.begin();
			for (; it != _tradeHistory.end() && it->timestamp == trades->timestamp; it++) {
				if (it->oderId == trades->oderId) {
					break;
				}
			}
			if (it == _tradeHistory.end() || it->timestamp != trades->timestamp) {
				_tradeHistory.push_front(*trades);
			}
			//pushLog("update trade event 2\n");
		}
		else if (trades->timestamp < _tradeHistory.back().timestamp) {
			_tradeHistory.push_back(*trades);
			pushLog("update trade event 3\n");
		}
		else if (trades->timestamp > _tradeHistory.front().timestamp) {
			_tradeHistory.push_front(*trades);
			//pushLog("update trade event 4\n");
		}
		else {
			pushLog("no need to update trade event\n");
		}
	}
#endif
	for (auto it = _tradeEventListeners.begin(); it != _tradeEventListeners.end(); it++) {
		(it->second) (this, trades, count, snapShot);
	}
}



void NAPMarketEventHandler::accessSharedData(const AccessEventDataFunc& f) {
	std::unique_lock<std::mutex> lk(_m);
	f(this);
}

const std::list<TradeItem>& NAPMarketEventHandler::getTradeHistoriesNonSync() const {
	return _tradeHistory;
}

std::list<TradeItem>& NAPMarketEventHandler::getTradeHistoriesNonSync() {
	return _tradeHistory;
}

//void NAPMarketEventHandler::addEmptyTicker(TIMESTAMP t) {
//	double lastPrice = -1;
//	if (_tradeHistory.size()) {
//		auto& lastPricePoint = _tradeHistory.front().lastPrice;
//		lastPrice = lastPricePoint.price;
//
//		// update previous last price last to now
//		lastPricePoint.at = t - 1;
//	}
//
//	SimpleTicker ticker;
//
//	ticker.firstPrice.price = lastPrice;
//	ticker.firstPrice.at = t;
//
//	ticker.lastPrice.price = lastPrice;
//	ticker.lastPrice.at = t;
//
//	ticker.high.price = lastPrice;
//	ticker.high.at = t;
//	if (lastPrice > 0) {
//		ticker.low.price = lastPrice;
//	}
//	else {
//		ticker.low.price = std::numeric_limits<double>::max();
//	}
//	ticker.low.at = t;
//
//	ticker.averagePrice = lastPrice;
//	ticker.boughtVolume = 0;
//	ticker.soldVolume = 0;
//	ticker.processLevel = -1;
//
//	if (_tradeHistory.size() == MAX_RECORDS_COUNT) {
//		_tradeHistory.pop_back();
//	}
//
//	_tradeHistory.emplace_front(ticker);
//
//	//pushLog("{ ticker rew new at %s }\n", time2str(t).c_str());
//}

//void NAPMarketEventHandler::onTimeUpdate(TIMESTAMP t) {
//	std::unique_lock<std::mutex> lk(_m);
//	addEmptyTicker(t);
//}

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