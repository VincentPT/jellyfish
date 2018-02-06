#pragma once
#include <Windows.h>

#include "Notifier.h"
#include "NAPMarketEventHandler.h"
#include "TradingPlatform.h"
#include "../common/SyncMessageQueue.hpp"
#include "GoCrypto.h"

#include <future>
#include <string>
#include <mutex>

#include <functional>

typedef std::function<void(int)> SymbolStatisticUpdatedHandler;

struct PricePoint {
	double price;
	TIMESTAMP at;
};

struct TickerUI {
	PricePoint firstPrice;
	PricePoint lastPrice;
	PricePoint low;
	PricePoint high;
	double averagePrice; // average price during the period
	double soldVolume; // volume of shelling of takers
	double boughtVolume;// volume of buying of takers
	char processLevel;
};

#define TICKER_DURATION 60000

class PlatformEngine
{
	struct TriggerTimeBase {
		TIMESTAMP startTime; // start time
		TIMESTAMP endTime; // start time
		double priceChangePerMin; // price change per one minute
	};

	struct UserListenerInfoRaw {
		UserListenerInfo* userInfo;
		std::list<std::string> pairs;
		bool all;
	};

	struct InternalNotificationData {
		Notification message;
		std::string pair;
	};

	struct RequestTradeHistoryMessage {
		std::string pair;
		TIMESTAMP endTime;
		TIMESTAMP duration;
	};
	
private:
	bool _runFlag;
	HMODULE _hLib;
	TradingPlatform* _platform;
	std::future<void> _broadCastIntervalTask;
	std::future<void> _messageLoopTask;
	AccessEventDataFunc _tickerAnalyzer;
	std::vector<TriggerTimeBase> _triggers;
	SyncMessageQueue<InternalNotificationData> _messageQueue;
	SyncMessageQueue<RequestTradeHistoryMessage> _symbolQueue;
	std::string _platformName;
	TIMESTAMP _tickerInterval;
	TIMESTAMP _notifyDistance;
	std::list<UserListenerInfoRaw> _userRawListeners;
	std::list<UserListenerInfo> _userListeners;
	std::map<std::string, std::shared_ptr<std::list<UserListenerInfo*>>> _pairListenerMap;
	std::vector<CryptoBoardElmInfo> _symbolsStatistics;
	std::vector<std::list<TickerUI>> _symbolsTickers;
	std::vector<std::string> _currencies;

	typedef std::map<TRADE_ID, char> TradeLevelMap;
	std::map<std::string, std::shared_ptr<TradeLevelMap>> _processLevelMap;
	std::map<std::string, bool> _sentTradeSnapshotRequest;
	std::future<void> _sendTradeHistoryRequestLoop;
	SymbolStatisticUpdatedHandler _onSymbolStatisticUpdated;
private:
	void timeInterval();
	void pushMessageLoop();
	void sheduleQueryHistory();
	void measurePriceIncrement(const std::vector<NAPMarketEventHandler*>& handlers);
	void tickerAnalyze(NAPMarketEventHandler*);
	bool processTradesLevel(const char* pair, TIMESTAMP timeBase, std::list<TradeItem>::iterator begin, std::list<TradeItem>::iterator& end, char level);
	void updateSymbolStatistic(int i, NAPMarketEventHandler* sender, TradeItem* tradeItem, int, bool);
public:
	PlatformEngine(const char* platformName);
	virtual ~PlatformEngine();

	void run();
	void stop();
	const std::vector<CryptoBoardElmInfo>& getSymbolsStatistics() const;
	TradingPlatform* getPlatform();
	const std::vector<std::string>& getCurrencies() const;
	void setSymbolStatisticUpdatedHandler(SymbolStatisticUpdatedHandler&& handler);
};

typedef std::shared_ptr<Notifier> TraderRef;

