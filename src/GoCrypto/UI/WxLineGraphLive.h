#pragma once
#include "WxLineGraph.h"
#include <mutex>
#include <functional>
#include "TradingPlatform.h"

class WxLineGraphLive :
	public WxLineGraph
{
protected:
	mutable std::mutex _mutex;
	float _lastestX;
	ci::vec2 _autoScaleRange;
public:
	WxLineGraphLive();
	virtual ~WxLineGraphLive();

	virtual void update();
	virtual void draw();
	//virtual void addPoint(const glm::vec2& point);
	void setLiveX(float x);
	float getLiveX() const;

	virtual void acessSharedData(const AccessSharedDataFunc& f);
	// non synchronous functions
	virtual void addPointAndConstruct(const glm::vec2& point);
	virtual void adjustTransform();
	virtual void adjustHorizontalTransform(const glm::vec2& point);
	virtual void adjustVerticalTransform(const glm::vec2& point);
	virtual void clearPoints();
	virtual void setInitalGraphRegion(const ci::Area& area);
	void setAutoScaleRange(float y1, float y2);
};

