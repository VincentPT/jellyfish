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

typedef std::function<void(MarketData*, int, bool)> MarketDataEventListener;

#define TICKER_DURATION 60000

class PlatformEngine
{
	struct UserListenerInfoRaw {
		UserListenerInfo* userInfo;
		std::list<std::string> pairs;
		bool all;
	};

	enum class NotificationType {
		Price,
		Volume,
	};

	struct InternalNotificationData {
		NotificationType notificationType;
		TIMESTAMP trigerTime;
		Notification message;
		std::string pair;
	};

	enum class EventHistoryType {
		TradeHistory,
		CandleHistory
	};

	struct RequestEventHistoryMessage {
		std::string pair;
		TIMESTAMP endTime;
		TIMESTAMP duration;
		EventHistoryType eventType;
	};
	
private:
	bool _runFlag;
	HMODULE _hLib;
	TradingPlatform* _platform;
	std::future<void> _broadCastIntervalTask;
	std::future<void> _messageLoopTask;
	AccessEventDataFunc _tickerAnalyzer;
	std::mutex _priceTrigersMutex;
	std::vector<TriggerPriceBase> _triggers;
	SyncMessageQueue<InternalNotificationData> _messageQueue;
	SyncMessageQueue<RequestEventHistoryMessage> _symbolQueue;
	std::string _platformName;
	std::list<UserListenerInfoRaw> _userRawListeners;
	std::list<UserListenerInfo> _userListeners;
	std::map<std::string, std::shared_ptr<std::list<UserListenerInfo*>>> _pairListenerMap;
	std::vector<CryptoBoardElmInfo*> _symbolsStatistics;
	std::map<std::string, int> _symbolIndexMap;
	std::vector<TIMESTAMP> _notifyProcessingVolumeMap;
	std::vector<std::list<TickerUI>> _symbolsTickers;
	std::vector<std::string> _currencies;
	std::vector<Period> _periods;

	typedef std::map<TRADE_ID, char> TradeLevelMap;
	std::map<std::string, std::shared_ptr<TradeLevelMap>> _processLevelMap;
	std::mutex _symboRequestslMutex;
	std::map<std::string, TIMESTAMP> _sentTradeSnapshotRequest;
	std::map<std::string, TIMESTAMP> _sentCandleSnapshotRequest;
	std::future<void> _sendTradeHistoryRequestLoop;
	SymbolStatisticUpdatedHandler _onSymbolStatisticUpdated;
	bool _priceNotificationEnable = false;
	bool _volumeNotificationEnable = false;
	std::mutex _volumeTrigersMutex;
	std::vector<TriggerVolumeBaseItem> _volumeBaseTriggers;
	std::mutex _marketHistoriesMutex;
	std::list<MarketData> _marketHistories;
	std::future<void> _marketDataRequestLoop;
	int _marketDataEventId;

	int _autoId = 0;
	Signal<bool> _stopLoopTask;
	std::mutex _marketDataEventListenersMutex;
	std::map<int, MarketDataEventListener> _marketDataEventListeners;
private:
	void pushMessageLoop();
	void sheduleQueryHistory();
	void updateMarketData();
	void updateSymbolStatistics(CryptoBoardElmInfo* info, const std::list<TickerUI>&);
	void analyzeTickerForNotification(NAPMarketEventHandler* handler, const std::list<TickerUI>&);
	void onTrade(int i, NAPMarketEventHandler* sender, TradeItem* tradeItem, int, bool);
	void onCandle(int i, NAPMarketEventHandler* sender, CandleItem* candleItems, int, bool);
	bool measureVolumeInPeriod(TIMESTAMP duration, TIMESTAMP lastProcessingTime,
		std::list<CandleItem>::const_iterator &it, const std::list<CandleItem>::const_iterator& end,
		double& volume, double& priceHigh, double& priceLow);

	void onMarketData(MarketData*, int, bool);
	void sortPriceTriggers();
	void sortVolumeTriggers();
public:
	PlatformEngine(const char* configFile);
	virtual ~PlatformEngine();

	void run();
	void stop();
	const std::vector<CryptoBoardElmInfo*>& getSymbolsStatistics() const;
	const std::vector<std::list<TickerUI>>& getSymbolsTickers();
	TradingPlatform* getPlatform();
	const std::vector<Period>& getPeriods() const;
	const std::vector<std::string>& getCurrencies() const;
	void setSymbolStatisticUpdatedHandler(SymbolStatisticUpdatedHandler&& handler);
	void enablePriceNotification(bool enable);
	void enableVolumeNotification(bool enable);
	const std::string& getQuote(const std::string& symbol);
	bool convertPrice(const std::string& symbol, const std::string& baseQuoteCurrency, const double& price, double& convertedPrice);
	int addMarketDataEventListener(MarketDataEventListener&& eventListener);
	void removeMarketDataEventListener(int id);

	void setPriceTriggers(const std::vector<TriggerPriceBase>& triggers);
	const std::vector<TriggerPriceBase>& getPriceTriggers() const;

	void setVolumeTriggers(const std::vector<TriggerVolumeBaseItem>& triggers);
	const std::vector<TriggerVolumeBaseItem>& getVolumeTriggers() const;
	void setSymbolRequestHitoriesHighPriority(const std::string& symbol);
};

typedef std::shared_ptr<Notifier> TraderRef;

