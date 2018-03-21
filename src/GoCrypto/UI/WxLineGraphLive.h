#pragma once
#include "WxLineGraph.h"
#include <mutex>
#include <functional>
#include "TradingPlatform.h"

class WxLineGraphLive;

class WxLineGraphLive :
	public WxLineGraph
{
protected:
	mutable std::mutex _mutex;
	float _lastestX;
public:
	WxLineGraphLive();
	virtual ~WxLineGraphLive();

	virtual void update();
	virtual void draw();
	virtual void addPoint(const glm::vec2& point);
	void setLiveX(float x);

	virtual void acessSharedData(const AccessSharedDataFunc& f);
	// non synchronous functions
	virtual void addPointAndConstruct(const glm::vec2& point);
	virtual void adjustTransform();
	virtual void adjustHorizontalTransform(const glm::vec2& point);
	virtual void adjustVerticalTransform(const glm::vec2& point);
	virtual void clearPoints();
};

