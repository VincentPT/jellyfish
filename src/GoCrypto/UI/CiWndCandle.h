#pragma once
#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

class CiWndCandle
{
private:
	ci::app::WindowRef _windowUIRef;
protected:
	CiWndCandle();
public:
	static void createWindow();
	virtual ~CiWndCandle();

	virtual void update();
	virtual void draw();
};

