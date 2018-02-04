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
	ButtonClickEventHandler _selectedCurrencyChangedHandler;
	int _checkedButton;
	std::vector<std::string> _currencies;
	mutable std::mutex _mutex;
public:
	WxControlBoard();
	virtual ~WxControlBoard();

	virtual void update();

	void setOnStartButtonClickHandler(ButtonClickEventHandler&& handler);
	void setOnStopButtonClickHandler(ButtonClickEventHandler&& handler);
	void setOnSelectedCurrencyChangedHandler(ButtonClickEventHandler&& handler);

	void accessSharedData(const AccessSharedDataFunc&);
	void setBaseCurrencies(const std::vector<std::string>& currencies);
	const std::string& getCurrentCurrency() const;
};

