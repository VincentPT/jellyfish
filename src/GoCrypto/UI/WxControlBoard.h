#pragma once
#include "ImWidget.h"
#include "GoCrypto.h"
#include <functional>

typedef std::function<void(Widget*)> ButtonClickEventHandler;

class WxControlBoard :
	public ImWidget
{
	ButtonClickEventHandler _startButtonClickHandler;
	ButtonClickEventHandler _stopButtonClickHandler;
public:
	WxControlBoard();
	virtual ~WxControlBoard();

	virtual void update();

	void setOnStartButtonClickHandler(ButtonClickEventHandler&& handler);
	void setOnStopButtonClickHandler(ButtonClickEventHandler&& handler);
};

