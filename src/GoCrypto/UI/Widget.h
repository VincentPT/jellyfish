#pragma once
//#include <Windows.h>
//#define FUNCTON_LOG() OutputDebugStringA(__FUNCTION__ " begin\n"); std::unique_ptr<char, std::function<void(char*)>> logger(__FUNCTION__ " end\n", [](char* text) { OutputDebugStringA(text);})
//#define SCOPE_LOG(scopeId) OutputDebugStringA(#scopeId " begin\n"); std::unique_ptr<char, std::function<void(char*)>> scopeId(#scopeId " end\n", [](char* text) { OutputDebugStringA(text);})

#define FUNCTON_LOG()
#define SCOPE_LOG(scopeId)

class Widget
{
public:
	Widget();
	virtual ~Widget();

	virtual void update() = 0;
	virtual void draw() = 0;
	virtual void setSize(float w, float h) = 0;
	virtual void setPos(float x, float y) = 0;
	virtual float getWidth() = 0;
	virtual float getHeight() = 0;
	virtual float getX() = 0;
	virtual float getY() = 0;
};


#include <functional>
typedef std::function<void(Widget*)> ButtonClickEventHandler;
typedef std::function<void(Widget*)> MouseDoubleClickEventHandler;
typedef std::function<void(Widget*)> AccessSharedDataFunc;

