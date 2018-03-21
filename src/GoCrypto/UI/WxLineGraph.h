#pragma once
#include "WxPointBaseGraph.h"
#include "cinder/gl/Fbo.h"
#include <list>
#include <functional>

class WxLineGraph :
	public WxPointBaseGraph
{
protected:

public:
	WxLineGraph();
	virtual ~WxLineGraph();

	virtual void draw();
};

