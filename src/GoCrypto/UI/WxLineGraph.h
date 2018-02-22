#pragma once
#include "CiWidget.h"
#include "cinder/gl/Fbo.h"
#include <list>

class WxLineGraph :
	public CiWidget
{
protected:
	std::list<glm::vec2> _lines;
	ci::Area _displayArea;
	ci::ColorA8u _lineColor;
	ci::ColorA8u _graphRegionColor;
	ci::vec2 _scale;
	ci::vec2 _translate;
	ci::gl::FboRef _drawingFBO;
	//glm::mat4x4 _transform;

	//ci::gl::VboRef _pointsData;

public:
	WxLineGraph();
	virtual ~WxLineGraph();

	virtual void draw();
	virtual void addPoint(const glm::vec2& point);
	void import(std::list<glm::vec2>& points);
	virtual void clearPoints();
	const std::list<glm::vec2>& getPoints();
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
	virtual void scale(float x, float y);
	virtual ci::vec2 pointToWindow(float x, float y);
	virtual ci::vec2 pointToLocal(float x, float y);
	virtual ci::vec2 windowToPoint(float x, float y);
	virtual ci::vec2 localToPoint(float x, float y);
};

