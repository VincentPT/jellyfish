#pragma once
#include "WxLineGraph.h"
#include <mutex>

class WxLineGraphLive :
	public WxLineGraph
{
protected:
	mutable std::mutex _mutex;
	float _timeScale;
	int _generatedIdx;
	float _lastUpdate;
public:
	WxLineGraphLive();
	virtual ~WxLineGraphLive();

	virtual void update();
	virtual void draw();
	virtual void addPoint(const glm::vec2& point);
	virtual void addPointAndConstruct(const glm::vec2& point);
	virtual void adjustTransform();
	virtual void clearPoints();
	////////////////////////////////////////////////////////////////////////////////
	///
	/// Set time scale for auto generate new point when time is update
	///
	////////////////////////////////////////////////////////////////////////////////
	virtual void setTimeScale(float scale);
	virtual float getTimeScale() const;
	//virtual size_t getPointCount() const;
};

