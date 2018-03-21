#pragma once
#include "ImWidget.h"
#include "GoCrypto.h"
#include <functional>
#include <vector>
#include <mutex>

enum class GraphMode : int {
	Price = 0,
	Volume
};

class WxControlBoard :
	public ImWidget
{
	ButtonClickEventHandler _startButtonClickHandler;
	ButtonClickEventHandler _stopButtonClickHandler;
	ButtonClickEventHandler _exportButtonClickHandler;
	ButtonClickEventHandler _selectedCurrencyChangedHandler;
	ButtonClickEventHandler _selectedGraphModeChangedHandler;
	int _checkedButton;
	GraphMode _graphMode;
	const char* _currentPlatform;
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
	void setOnSelectedGraphModeChangedHandler(ButtonClickEventHandler&& handler);

	void accessSharedData(const AccessSharedDataFunc&);
	void setBaseCurrencies(const std::vector<std::string>& currencies);
	const std::string& getCurrentCurrency() const;
	const char* getCurrentPlatform() const;
	GraphMode getCurrentGraphMode() const;
};

