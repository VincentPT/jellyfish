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
	
private:
	bool _runFlag;
	HMODULE _hLib;
	TradingPlatform* _platform;
	std::future<void> _broadCastIntervalTask;
	std::future<void> _messageLoopTask;
	AccessEventDataFunc _tickerAnalyzer;
	std::vector<TriggerTimeBase> _triggers;
	SyncMessageQueue<InternalNotificationData> _messageQueue;
	std::string _platformName;
	TIMESTAMP _tickerInterval;
	TIMESTAMP _notifyDistance;
	std::list<UserListenerInfoRaw> _userRawListeners;
	std::list<UserListenerInfo> _userListeners;
	std::map<std::string, std::shared_ptr<std::list<UserListenerInfo*>>> _pairListenerMap;
	std::map<std::string, bool> _degbugMap;
	std::vector<CryptoBoardElmInfo> _symbolsStatistics;
private:
	void timeInterval();
	void pushMessageLoop();
	void measurePriceIncrement(const std::vector<NAPMarketEventHandler*>& handlers);
	void tickerAnalyze(NAPMarketEventHandler*);
	bool processTickerLevel(const char* pair, TIMESTAMP timeBase, std::list<SimpleTicker>::iterator begin, std::list<SimpleTicker>::iterator& end, char level);
	void updateSymbolStatistic(CryptoBoardElmInfo* info, NAPMarketEventHandler* sender, TradeItem* tradeItem);
public:
	PlatformEngine(const char* platformName);
	virtual ~PlatformEngine();

	void run();
	void stop();
	const std::vector<CryptoBoardElmInfo>& getSymbolsStatistics() const;
	TradingPlatform* getPlatform();
};

typedef std::shared_ptr<Notifier> TraderRef;

