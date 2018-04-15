#pragma once
#include "ImWidget.h"
#include "GoCrypto.h"
#include <functional>
#include <vector>
#include <mutex>

class WxControlBoard :
	public ImWidget
{
	ButtonClickEventHandler _startButtonClickHandler;
	ButtonClickEventHandler _stopButtonClickHandler;
	ButtonClickEventHandler _exportButtonClickHandler;
	ButtonClickEventHandler _selectedCurrencyChangedHandler;
	ButtonClickEventHandler _priceNotificationChangedHandler;
	ButtonClickEventHandler _volumeNotificationChangedHandler;
	ButtonClickEventHandler _graphLengthChangedHandler;
	int _checkedButton;
	int _currentPlatform;
	std::vector<std::string> _platforms;
	int _currentGraphLength;
	bool _notifyPriceMovement = false;
	bool _notifyVolumeMovement = false;
	std::vector<std::string> _currencies;
	mutable std::mutex _mutex;
public:
	WxControlBoard(const std::vector<std::string>& platforms);
	virtual ~WxControlBoard();

	virtual void update();

	void setOnStartButtonClickHandler(ButtonClickEventHandler&& handler);
	void setOnStopButtonClickHandler(ButtonClickEventHandler&& handler);
	void setOnExportButtonClickHandler(ButtonClickEventHandler&& handler);
	void setOnSelectedCurrencyChangedHandler(ButtonClickEventHandler&& handler);
	void setOnPriceNotificationChangedHandler(ButtonClickEventHandler&& handler);
	void setOnVolumeNotificationChangedHandler(ButtonClickEventHandler&& handler);
	void setOnGraphLengthChangedHandler(ButtonClickEventHandler&& handler);

	void accessSharedData(const AccessSharedDataFunc&);
	void setBaseCurrencies(const std::vector<std::string>& currencies);
	const std::string& getCurrentCurrency() const;
	const char* getCurrentPlatform() const;
	bool isPriceNotificationEnable() const;
	bool isVolumeNotificationEnable() const;
	unsigned int getCurrentGraphLengh() const;
	int getCurrentBarCount() const;
};

