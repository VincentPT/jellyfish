#pragma once
#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

#include "UI/Spliter.h"

class CiWndCandle
{
private:
	ci::app::WindowRef _windowUIRef;
	Spliter _spliter;
	bool _drawn = false;
protected:
	CiWndCandle();
public:
	static ci::app::WindowRef createWindow();
	virtual ~CiWndCandle();

	virtual void update();
	virtual void draw();
};

