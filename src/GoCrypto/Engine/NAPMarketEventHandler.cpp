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

NAPMarketEventHandler::NAPMarketEventHandler(const char* pair) :
	MarketEventHandler(pair), _tag(nullptr), _autoId(1)
{
}

NAPMarketEventHandler::~NAPMarketEventHandler()
{
}

void NAPMarketEventHandler::onTickerUpdate(Ticker& ticker) {
	pushLog((int)LogLevel::Debug, "%s's ticker: {%lf, %lf, %lf, %lf, %lf, %lf, %s}\n", _pair, ticker.bid, ticker.ask, ticker.lastPrice, ticker.volume, ticker.high, ticker.low, Utility::time2str(ticker.timestamp).c_str());
}

void NAPMarketEventHandler::onBooksUpdate(BookItemEventArgs* books, bool snapShot) {}

void NAPMarketEventHandler::onCandlesUpdate(CandleItem* candles, int count, bool snapShot) {
	if (candles == nullptr) return;

	std::unique_lock<std::mutex> lk(_m);
	CandleItem* pItemEnd = candles + count;

	if (_candleHistory.size() == 0) {
		for (CandleItem* pItem = candles; pItem < pItemEnd; pItem++) {
			CandleItem& item = *pItem;
			_candleHistory.push_back(item);
		}
	}
	else if (count > 1) {
		auto& oldestItem = _candleHistory.back();
		auto& newestItem = _candleHistory.front();
		auto& incomOldestItem = *(candles + count - 1);
		auto& incomNewestItem = *candles;

		Range<TIMESTAMP> currentRange{ oldestItem.timestamp, newestItem.timestamp };
		Range<TIMESTAMP> newRange{ incomOldestItem.timestamp, incomNewestItem.timestamp };

		Range<TIMESTAMP> overlapped;
		mergeParts<TIMESTAMP>(currentRange, newRange, nullptr, &overlapped);

		// check if two part are not overlap
		if (overlapped.start > overlapped.end) {
			// check if oldest existence trade item occurs later than newest trade item on the snapshot event
			if (oldestItem.timestamp > incomNewestItem.timestamp) {
				for (auto pItem = candles; pItem < pItemEnd; pItem++) {
					_candleHistory.push_back(*pItem);
				}
				pushLog((int)LogLevel::Verbose, "added trade event back\n");
			}
			else {
				auto pItemEnd = candles;
				for (auto pItem = candles + count - 1; pItem >= pItemEnd; pItem--) {
					_candleHistory.push_front(*pItem);
				}
				pushLog((int)LogLevel::Verbose, "added candle event front\n");
			}
		}
		else if (overlapped.start == overlapped.end) {
			// now, the two time ranges have start and the end are same
			// check whether what range is newer
			if (newestItem.timestamp > incomNewestItem.timestamp) {
				// now the existance range is newer
				// so we ingnore the item of incomming event that at the overlapped point (at the beginning)
				auto pItem = candles;
				for (pItem++; pItem < pItemEnd; pItem++) {
					_candleHistory.push_back(*pItem);
				}
				pushLog((int)LogLevel::Verbose, "merge trade event at the common time back\n");
			}
			else {
				// now the incoming range is newer
				// so we ingnore the item of incomming event that at the overlapped point (at the end)
				auto pItem = pItemEnd;
				for (pItem--; pItem >= candles; pItem--) {
					_candleHistory.push_front(*pItem);
				}

				pushLog((int)LogLevel::Verbose, "merge trade event at the common time front\n");
			}
		}
		else {
			// now two range is overlapped
			// for simply implementation but decrease performance
			// we using map to ignore candles that has timestamp exist in candle history
			map<TIMESTAMP, bool> duplicatedCheckMap;

			// add non duplicate trade id for the two lists
			// first build trade id map for the first list
			for (auto it = _candleHistory.begin(); it != _candleHistory.end(); it++) {
				duplicatedCheckMap[it->timestamp] = true;
			}
			// copy item non duplicated from the second list to the first list.
			for (auto pItem = candles; pItem < pItemEnd; pItem++) {
				if (duplicatedCheckMap.find(pItem->timestamp) == duplicatedCheckMap.end()) {
					_candleHistory.push_back(*pItem);
				}
			}

			// sort by timestap
			_candleHistory.sort([](CandleItem& item1, CandleItem& item2) {
				return item1.timestamp > item2.timestamp;
			});

			pushLog((int)LogLevel::Verbose, "merge trade event using general method\n");
		}
	}
	else if (count == 1) {
		CandleItem& item = *candles;
		if (_candleHistory.size()) {
			if (_candleHistory.front().timestamp == item.timestamp) {
				_candleHistory.front() = item;
			}
			else if (_candleHistory.front().timestamp < item.timestamp) {
				_candleHistory.push_front(item);
			}
		}
		else {
			_candleHistory.push_front(item);
		}
	}

	for (auto it = _candleEventListeners.begin(); it != _candleEventListeners.end(); it++) {
		(it->second) (this, candles, count, snapShot);
	}
}

void NAPMarketEventHandler::onTradesUpdate(TradeItem* trades, int count, bool snapShot) {

	std::unique_lock<std::mutex> lk(_m);
	if (trades == nullptr) return;
	if (_tradeHistory.size() == 0) {
		TradeItem* pItemEnd = trades + count;
		for (auto pItem = trades; pItem < pItemEnd; pItem++) {
			_tradeHistory.push_back(*pItem);
		}
		pushLog((int)LogLevel::Verbose, "snapshot trade event\n");
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
				pushLog((int)LogLevel::Verbose, "added trade event back\n");
			}
			else {
				TradeItem* pItemEnd = trades;
				for (auto pItem = trades + count - 1; pItem >= pItemEnd; pItem--) {
					_tradeHistory.push_front(*pItem);
				}
				pushLog((int)LogLevel::Verbose, "added trade event front\n");
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

				pushLog((int)LogLevel::Verbose, "merge trade event at the common time back\n");
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

				pushLog((int)LogLevel::Verbose, "merge trade event at the common time front\n");
			}
		}
		else {
			// for simply implementation but decrease performance
			map<TRADE_ID, bool> duplicatedCheckMap;

			// add non duplicate trade id for the two lists
			// first build trade id map for the first list
			for (auto it = _tradeHistory.begin(); it != _tradeHistory.end(); it++) {
				duplicatedCheckMap[it->oderId] = true;
			}
			// copy item non duplicated from the second list to the first list.
			TradeItem* pItemEnd = trades + count;
			auto pItem = trades;
			for (; pItem < pItemEnd; pItem++) {
				if (duplicatedCheckMap.find(pItem->oderId) == duplicatedCheckMap.end()) {
					_tradeHistory.push_back(*pItem);
				}
			}

			// sort by timestap
			_tradeHistory.sort([](TradeItem& item1, TradeItem& item2) {
				return item1.timestamp > item2.timestamp;
			});

			pushLog((int)LogLevel::Verbose, "merge trade event using general method\n");
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

			pushLog((int)LogLevel::Verbose, "update trade event 1\n");
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
			pushLog((int)LogLevel::Verbose, "update trade event 2\n");
		}
		else if (trades->timestamp < _tradeHistory.back().timestamp) {
			_tradeHistory.push_back(*trades);
			pushLog((int)LogLevel::Verbose, "update trade event 3\n");
		}
		else if (trades->timestamp > _tradeHistory.front().timestamp) {
			_tradeHistory.push_front(*trades);
			pushLog((int)LogLevel::Verbose, "update trade event 4\n");
		}
		else {
			pushLog((int)LogLevel::Verbose, "merge update event into trade history\n");
			auto it = std::find_if(_tradeHistory.begin(), _tradeHistory.end(), [trades](const TradeItem& item) {
				return trades->timestamp > item.timestamp;
			});
			_tradeHistory.insert(it, *trades);
		}
	}

	if (_tradeHistory.size() > 100000) {
		_tradeHistory.resize(100000);
	}

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

const std::list<CandleItem>& NAPMarketEventHandler::getCandleHistoriesNonSync() {
	return _candleHistory;
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

int NAPMarketEventHandler::addCandleEventListener(CandleEventListener&& eventListener) {
	std::unique_lock<std::mutex> lk(_m);

	CandleEventListener emptyFunc;
	auto emptyPair = std::make_pair(_autoId, emptyFunc);
	auto it = _candleEventListeners.insert(emptyPair);
	it.first->second = eventListener;

	_autoId++;

	return emptyPair.first;
}

void NAPMarketEventHandler::removeCandleEventListener(int id) {
	std::unique_lock<std::mutex> lk(_m);
	_candleEventListeners.erase(id);
}