#include "WxLineGraph.h"

using namespace ci;
using namespace std;

WxLineGraph::WxLineGraph() : _displayArea(0,0,0,0), _scale(1,1), _translate(0,0) {
	setLineColor(ColorA8u(0,0,0,255));
	setGraphRegionColor(ColorA8u(255, 255, 255, 255));
	//_transform = glm::translate(vec3(0, 0, 0));
}

WxLineGraph::~WxLineGraph() {

}

void WxLineGraph::draw() {
	if (_displayArea.getWidth() == 0 || _displayArea.getHeight() == 0) {
		return;
	}

	auto tl_x = getX();
	auto tl_y = getY();
	ci::Rectf graphRect(_displayArea.x1 + tl_x, _displayArea.y1 + tl_y, _displayArea.x2 + tl_x, _displayArea.y2 + tl_y);
	{
		gl::ScopedColor color(_graphRegionColor);
		gl::drawSolidRect(graphRect);
	}
	{
		gl::ScopedMatrices modelMatrix;
		gl::ScopedColor color(_lineColor);
		//gl::ScopedViewport(_displayArea.x1 + tl_x, _displayArea.y1 + tl_y, _displayArea.x2 + tl_x, _displayArea.y2 + tl_y);
		//gl::translate(_translate.x, _translate.y);
		//gl::scale(_scale.x, _scale.y);
		//gl::setModelMatrix(_transform);
		gl::begin(GL_LINE_STRIP);
		//gl::setModelMatrix()

		auto leftPoint = localToPoint(_displayArea.x1, 0);

		if (_lines.size()) {
			vec2 pos = pointToWindow(_lines.back().x, _lines.back().y);
			gl::drawSolidCircle(pos, 3);
		}

		for (auto it = _lines.rbegin(); it != _lines.rend(); it++) {
			if (it->x < leftPoint.x) {
				break;
			}
			vec2 pos = pointToWindow(it->x, it->y);
			gl::vertex(pos);
		}
		gl::end();
	}
}

void WxLineGraph::addPoint(const glm::vec2& point) {
	if (_lines.size() > 100000) {
		_lines.pop_front();
	}
	_lines.push_back(point);
}

void WxLineGraph::import(std::list<glm::vec2>& points) {
	_lines.clear();
	_lines.splice(_lines.begin(), points, points.begin(), points.end());
}


void WxLineGraph::clearPoints() {
	_lines.clear();
}

size_t WxLineGraph::getPointCount() const {
	return _lines.size();
}

void WxLineGraph::setInitalGraphRegion(const ci::Area& area) {
	_displayArea = area;
	_scale = vec2(1, 1);
	_translate = vec2(0, 0);
	translate(_displayArea.x1 + getX(), _displayArea.y1 + getY());
}

const ci::Area& WxLineGraph::getGraphRegion() const {
	return _displayArea;
}

void WxLineGraph::setLineColor(const ci::ColorA8u& color) {
	_lineColor = color;
}

const ci::ColorA8u& WxLineGraph::getLineColor()  const {
	return _lineColor;
}

void WxLineGraph::setGraphRegionColor(const ci::ColorA8u& color) {
	_graphRegionColor = color;
}

const ci::ColorA8u& WxLineGraph::getGraphRegionColor()  const {
	return _graphRegionColor;
}

void WxLineGraph::setSize(float w, float h) {
	float scaleX = 1.0f;
	float scaleY = 1.0f;
	float paddingX = getWidth() - _displayArea.x2;
	float paddingY = getHeight() - _displayArea.y2;

	auto oldW = _displayArea.getWidth();
	_displayArea.x2 = w - paddingX;
	if (oldW) {
		scaleX = _displayArea.getWidth() * 1.0f / oldW;
	}
	auto oldH = _displayArea.getHeight();

	_displayArea.y2 = h - paddingY;

	if (oldH) {
		scaleY = _displayArea.getHeight() * 1.0f / oldH;
	}
	if (scaleX != 1 || scaleY != 1) {
		scale(scaleX, scaleY);
	}

	CiWidget::setSize(w, h);
}

void WxLineGraph::setPos(float x, float y) {
	float deltaX = x - getX();
	float deltaY = y - getY();

	CiWidget::setPos(x, y);
	translate(deltaX, deltaY);
}

void WxLineGraph::translate(float x, float y) {
	//pushLog("translate %f, %f\n", x, y);

	_translate.x += x;
	_translate.y += y;

	//_transform *= glm::translate(vec3(x, y, 0));
}

void WxLineGraph::scale(float x, float y) {
	//pushLog("scale %f, %f\n", x, y);

	_scale.x *= x;
	_scale.y *= y;

	//_transform *= glm::scale(vec3(x, y, 0));
}

ci::vec2 WxLineGraph::pointToWindow(float x, float y) {
	y = y*_scale.y + _translate.y;
	y = _displayArea.getHeight() - y;
	return ci::vec2 (x*_scale.x + _translate.x, y);
}

ci::vec2 WxLineGraph::pointToLocal(float x, float y) {
	auto pos = pointToWindow(x, y);
	pos.x -= getX();
	pos.y -= getY();

	return pos;
}

ci::vec2 WxLineGraph::windowToPoint(float x, float y) {
	x -= getX();
	y -= getY();
	y += _displayArea.getHeight();
	return ci::vec2(x/_scale.x - _translate.x, y/_scale.y - _translate.y);
}

ci::vec2 WxLineGraph::localToPoint(float x, float y) {
	x += getX();
	y += getY();
	y += _displayArea.getHeight();
	return ci::vec2(x / _scale.x - _translate.x, y / _scale.y - _translate.y);
}