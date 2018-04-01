#pragma once
#include "CiWidget.h"
#include "cinder/gl/Fbo.h"
#include <list>
#include <functional>

typedef std::function<std::tuple<std::string, std::string>(const glm::vec2& point)> TranslateFunction;

enum class HorizontalIndicatorAlignment {
	None,
	Left,
	Right
};

enum class VerticalIndicatorAlignment {
	None,
	Top,
	Bottom
};

class WxPointBaseGraph :
	public CiWidget
{
protected:
	std::list<glm::vec2> _points;
	ci::Area _displayArea;
	ci::ColorA8u _lineColor;
	ci::ColorA8u _graphRegionColor;
	ci::vec2 _scale;
	ci::vec2 _translate;
	ci::vec2 _cursorLocation;
	TranslateFunction _translateFunction;
	HorizontalIndicatorAlignment _horizontalIndicator;
	VerticalIndicatorAlignment _verticalIndicator;
	bool _drawBackground;

protected:
	void drawPointAtCursor();

public:
	WxPointBaseGraph();
	virtual ~WxPointBaseGraph();

	virtual void addPoint(const glm::vec2& point);
	void import(std::list<glm::vec2>& points);
	virtual void clearPoints();
	const std::list<glm::vec2>& getPoints() const;
	std::list<glm::vec2>& getPoints();
	virtual size_t getPointCount() const;
	virtual void setInitalGraphRegion(const ci::Area& area);
	virtual const ci::Area& getGraphRegion() const;
	virtual void setLineColor(const ci::ColorA8u& color);
	virtual const ci::ColorA8u& getLineColor() const;
	virtual void setGraphRegionColor(const ci::ColorA8u& color);
	virtual const ci::ColorA8u& getGraphRegionColor() const;
	virtual void setSize(float w, float h);
	virtual void setPos(float w, float h);
	virtual void translate(float x, float y);
	virtual void setTranslate(const ci::vec2& v);
	virtual const ci::vec2& getTranslate();
	virtual void scale(float x, float y);
	virtual ci::vec2 pointToWindow(float x, float y);
	virtual ci::vec2 pointToLocal(float x, float y);
	virtual ci::vec2 windowToPoint(float x, float y);
	virtual ci::vec2 localToPoint(float x, float y);
	virtual void setCursorLocation(const ci::vec2& location);
	virtual void setPointToTextTranslateFunction(const TranslateFunction& f);

	virtual void setIndicateAligment(HorizontalIndicatorAlignment horizontal, VerticalIndicatorAlignment vertical);
	virtual void drawBackground(bool drawBackground);
};

