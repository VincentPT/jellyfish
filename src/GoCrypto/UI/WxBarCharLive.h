#pragma once
#include <mutex>
#include <functional>
#include "WxPointBaseGraph.h"
#include "TradingPlatform.h"


class WxBarCharLive :
	public WxPointBaseGraph
{
protected:
	mutable std::mutex _mutex;
	ci::ColorA8u _barColor;
	float _currentX;
	float _barWidth;
public:
	WxBarCharLive();
	virtual ~WxBarCharLive();

	virtual void update();
	virtual void draw();
	virtual void adjustTransform();
	void acessSharedData(const AccessSharedDataFunc& f);
	virtual void setBarColor(const ci::ColorA8u& color);
	virtual const ci::ColorA8u& getBarColor() const;
	void setLiveX(float x);
	void setBarWidth(float barWidth);
};

