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
	ButtonClickEventHandler _notificationModeChangedHandler;
	ButtonClickEventHandler _graphLengthChangedHandler;
	int _checkedButton;
	const char* _currentPlatform;
	int _currentGraphLength;
	bool _pushToCloud = false;
	std::vector<std::string> _currencies;
	mutable std::mutex _mutex;
public:
	WxControlBoard();
	virtual ~WxControlBoard();

	virtual void update();

	void setOnStartButtonClickHandler(ButtonClickEventHandler&& handler);
	void setOnStopButtonClickHandler(ButtonClickEventHandler&& handler);
	void setOnExportButtonClickHandler(ButtonClickEventHandler&& handler);
	void setOnSelectedCurrencyChangedHandler(ButtonClickEventHandler&& handler);
	void setOnNotificationModeChangedHandler(ButtonClickEventHandler&& handler);
	void setOnGraphLengthChangedHandler(ButtonClickEventHandler&& handler);

	void accessSharedData(const AccessSharedDataFunc&);
	void setBaseCurrencies(const std::vector<std::string>& currencies);
	const std::string& getCurrentCurrency() const;
	const char* getCurrentPlatform() const;
	bool isPushToCloudEnable() const;
	unsigned int getCurrentGraphLengh() const;
};

