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
	int _generatedIdx;
	double _baseTime;
	float _pixelPerMicroSeconds;
	float _xAtZero;
	float _lastestX;
public:
	WxLineGraphLive();
	virtual ~WxLineGraphLive();

	virtual void update();
	virtual void draw();
	virtual void addPoint(const glm::vec2& point);
	////////////////////////////////////////////////////////////////////////////////
	///
	/// Set time scale for auto generate new point when time is update
	///
	////////////////////////////////////////////////////////////////////////////////
	virtual void setTimeScale(float scale);
	virtual float getTimeScale() const;
	//virtual size_t getPointCount() const;

	virtual void acessSharedData(const AccessSharedDataFunc& f);
	// non synchronous functions
	virtual void addPointAndConstruct(const glm::vec2& point);
	virtual void adjustTransform();
	virtual void adjustHorizontalTransform(const glm::vec2& point);
	virtual void adjustVerticalTransform(const glm::vec2& point);
	virtual void clearPoints();
	virtual void mapZeroTime(float x);
	virtual void baseTime();
};

