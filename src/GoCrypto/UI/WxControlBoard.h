#pragma once
#include "ImWidget.h"
#include "GoCrypto.h"
#include <functional>
#include <vector>
#include <mutex>

typedef ButtonClickEventHandler FilterActiveChanged;

class WxControlBoard :
	public ImWidget
{
	ButtonClickEventHandler _startButtonClickHandler;
	ButtonClickEventHandler _forceLoadButtonClickHandler;
	ButtonClickEventHandler _exportButtonClickHandler;
	ButtonClickEventHandler _selectedCurrencyChangedHandler;
	ButtonClickEventHandler _priceNotificationChangedHandler;
	ButtonClickEventHandler _volumeNotificationChangedHandler;
	ButtonClickEventHandler _graphLengthChangedHandler;
	ButtonClickEventHandler _volumeTriggerClickHandler;
	ButtonClickEventHandler _priceTriggerClickHandler;
	FilterActiveChanged _filterChangedHandler;
	int _checkedButton;
	int _currentPlatform;
	std::vector<std::string> _platforms;
	int _currentGraphLength;
	int _barLengthIdx;
	bool _notifyPriceMovement = false;
	bool _notifyVolumeMovement = false;
	std::vector<std::string> _currencies;
	mutable std::mutex _mutex;
	char _filterBuffer[16];

	mutable std::mutex _starStopButtonStrMutex;
	std::string _starStopButtonStr;

	const MarketData* _pMarketData;
public:
	WxControlBoard(const std::vector<std::string>& platforms);
	virtual ~WxControlBoard();

	virtual void update();

	void setOnStartStopButtonClickHandler(ButtonClickEventHandler&& handler);
	void setOnForceLoadClickHandler(ButtonClickEventHandler&& handler);
	void setOnExportButtonClickHandler(ButtonClickEventHandler&& handler);
	void setOnPriceTriggersButtonClickHandler(ButtonClickEventHandler&& handler);
	void setOnVolumeTriggersButtonClickHandler(ButtonClickEventHandler&& handler);
	void setOnSelectedCurrencyChangedHandler(ButtonClickEventHandler&& handler);
	void setOnPriceNotificationChangedHandler(ButtonClickEventHandler&& handler);
	void setOnVolumeNotificationChangedHandler(ButtonClickEventHandler&& handler);
	void setOnGraphLengthChangedHandler(ButtonClickEventHandler&& handler);
	void setFilterChanedHandler(FilterActiveChanged&& handler);

	void accessSharedData(const AccessSharedDataFunc&);
	void setBaseCurrencies(const std::vector<std::string>& currencies);
	const std::string& getCurrentCurrency() const;
	const char* getCurrentPlatform() const;
	bool isPriceNotificationEnable() const;
	bool isVolumeNotificationEnable() const;
	unsigned int getCurrentGraphLengh() const;
	int getCurrentBarTime() const;
	const char* getFilterText() const;
	void setMarketData(const MarketData* data);
	void setStarStopButtonText(const char* buttonText);
};

