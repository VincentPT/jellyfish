#pragma once
#include "ImWidget.h"
#include "GoCrypto.h"
#include <functional>
#include <vector>
#include <mutex>

typedef std::function<void(Widget*)> ButtonClickEventHandler;

class WxControlBoard :
	public ImWidget
{
	ButtonClickEventHandler _startButtonClickHandler;
	ButtonClickEventHandler _stopButtonClickHandler;
	int _checkedButton;
	std::vector<std::string> _currencies;
	std::mutex _mutex;
public:
	WxControlBoard();
	virtual ~WxControlBoard();

	virtual void update();

	void setOnStartButtonClickHandler(ButtonClickEventHandler&& handler);
	void setOnStopButtonClickHandler(ButtonClickEventHandler&& handler);

	void setBaseCurrencies(const std::vector<std::string>& currencies);
};

