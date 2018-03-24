#include "WxPointBaseGraph.h"

using namespace ci;
using namespace std;

WxPointBaseGraph::WxPointBaseGraph() : _displayArea(0,0,0,0), _scale(1,1), _translate(0,0),
_horizontalIndicator(HorizontalIndicatorAlignment::Right),
_verticalIndicator(VerticalIndicatorAlignment::Bottom),
_drawBackground(true)
{
	setLineColor(ColorA8u(0,0,0,255));
	setGraphRegionColor(ColorA8u(255, 255, 255, 255));
	//_transform = glm::translate(vec3(0, 0, 0));

	//_pointsData = gl::Vbo::create()
}

WxPointBaseGraph::~WxPointBaseGraph() {

}

void WxPointBaseGraph::addPoint(const glm::vec2& point) {
	if (_points.size() > 100000) {
		_points.pop_front();
	}
	_points.push_back(point);
}

void WxPointBaseGraph::import(std::list<glm::vec2>& points) {
	_points.clear();
	_points.splice(_points.begin(), points, points.begin(), points.end());
}


void WxPointBaseGraph::clearPoints() {
	_points.clear();
}

size_t WxPointBaseGraph::getPointCount() const {
	return _points.size();
}

const std::list<glm::vec2>& WxPointBaseGraph::getPoints() const {
	return _points;
}

std::list<glm::vec2>& WxPointBaseGraph::getPoints() {
	return _points;
}

void WxPointBaseGraph::setInitalGraphRegion(const ci::Area& area) {
	_displayArea = area;
	_scale = vec2(1, 1);
	_translate = vec2(0, 0);
	//translate(_displayArea.x1 + getX(), _displayArea.y1 + getY());
}

const ci::Area& WxPointBaseGraph::getGraphRegion() const {
	return _displayArea;
}

void WxPointBaseGraph::setLineColor(const ci::ColorA8u& color) {
	_lineColor = color;
}

const ci::ColorA8u& WxPointBaseGraph::getLineColor()  const {
	return _lineColor;
}

void WxPointBaseGraph::setGraphRegionColor(const ci::ColorA8u& color) {
	_graphRegionColor = color;
}

const ci::ColorA8u& WxPointBaseGraph::getGraphRegionColor()  const {
	return _graphRegionColor;
}

void WxPointBaseGraph::setSize(float w, float h) {
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

void WxPointBaseGraph::setPos(float x, float y) {
	float deltaX = x - getX();
	float deltaY = y - getY();

	CiWidget::setPos(x, y);
	translate(deltaX, deltaY);
}

void WxPointBaseGraph::translate(float x, float y) {
	//pushLog("translate %f, %f\n", x, y);

	_translate.x += x;
	_translate.y += y;

	//_transform *= glm::translate(vec3(x, y, 0));
}

void WxPointBaseGraph::scale(float x, float y) {
	//pushLog("scale %f, %f\n", x, y);

	_scale.x *= x;
	_scale.y *= y;

	//_transform *= glm::scale(vec3(x, y, 0));
}

ci::vec2 WxPointBaseGraph::pointToWindow(float x, float y) {
	auto localPoint = pointToLocal(x, y);
	localPoint.x += _displayArea.x1 + getX();
	localPoint.y += _displayArea.y1 + getY();

	return localPoint;
}

ci::vec2 WxPointBaseGraph::pointToLocal(float x, float y) {
	x *= _scale.x;
	y *= _scale.y;
	y = _displayArea.getHeight() - y;
	x += _translate.x;
	y += _translate.y;

	return ci::vec2(x, y);;
}

ci::vec2 WxPointBaseGraph::windowToPoint(float x, float y) {
	x -= _displayArea.x1 + getX();
	y -= _displayArea.y1 + getY();
	return localToPoint(x, y);
}

ci::vec2 WxPointBaseGraph::localToPoint(float x, float y) {
	x -= _translate.x;
	y -= _translate.y;
	y = _displayArea.getHeight() - y;
	y /= _scale.y;
	x /= _scale.x;

	return ci::vec2(x, y);
}

void WxPointBaseGraph::setCursorLocation(const ci::vec2& location) {
	_cursorLocation.x = location.x - getX() - _displayArea.getX1();
	_cursorLocation.y = location.y - getY() - _displayArea.getY1();
}

void WxPointBaseGraph::setPointToTextTranslateFunction(const TranslateFunction& f) {
	_translateFunction = f;
}

void WxPointBaseGraph::drawPointAtCursor() {
	FUNCTON_LOG();

	auto tl_x = getX();
	auto tl_y = getY();
	ci::Rectf graphRect(_displayArea.x1 + tl_x, _displayArea.y1 + tl_y, _displayArea.x2 + tl_x, _displayArea.y2 + tl_y);
	if (_translateFunction &&
		(_verticalIndicator != VerticalIndicatorAlignment::None || _horizontalIndicator != HorizontalIndicatorAlignment::None)
		) {
		if (_cursorLocation.x >= 0 && _cursorLocation.x < _displayArea.getWidth() &&
			_cursorLocation.y + _displayArea.y1 >= 0 && _cursorLocation.y < _displayArea.getWidth()) {

			SCOPE_LOG(scope1);

			auto point = localToPoint(_cursorLocation.x, _cursorLocation.y);
			auto pointStr = _translateFunction(point);

			SCOPE_LOG(scope2);
			glm::vec2 windowCursorLocation(_cursorLocation.x + graphRect.x1, _cursorLocation.y + graphRect.y1);

			ColorAf color(1, 0, 0);
			ci::Font font("Arial", 20);
			gl::color(color);

			SCOPE_LOG(scope3);
			if (_horizontalIndicator == HorizontalIndicatorAlignment::Right) {
				gl::drawStringCentered(std::get<1>(pointStr), glm::vec2(graphRect.x2 - 20, windowCursorLocation.y), color, font);
			}
			else if (_horizontalIndicator == HorizontalIndicatorAlignment::Left) {
				gl::drawStringCentered(std::get<1>(pointStr), glm::vec2(graphRect.x1 + 30, windowCursorLocation.y), color, font);
			}
			if (_verticalIndicator == VerticalIndicatorAlignment::Bottom) {
				gl::drawStringCentered(std::get<0>(pointStr), glm::vec2(windowCursorLocation.x - 20, graphRect.y2), color, font);
			}
			else if (_verticalIndicator == VerticalIndicatorAlignment::Top) {
				gl::drawStringCentered(std::get<0>(pointStr), glm::vec2(windowCursorLocation.x - 20, graphRect.y1), color, font);
			}

			if (_horizontalIndicator == HorizontalIndicatorAlignment::Right) {
				gl::drawLine(windowCursorLocation, glm::vec2(graphRect.x2, windowCursorLocation.y));
			}
			else if (_horizontalIndicator == HorizontalIndicatorAlignment::Left) {
				gl::drawLine(windowCursorLocation, glm::vec2(graphRect.x1, windowCursorLocation.y));
			}
			
			if (_verticalIndicator == VerticalIndicatorAlignment::Bottom) {
				gl::drawLine(windowCursorLocation, glm::vec2(windowCursorLocation.x, graphRect.y2));
			}
			else if (_verticalIndicator == VerticalIndicatorAlignment::Top) {
				gl::drawLine(windowCursorLocation, glm::vec2(windowCursorLocation.x, graphRect.y1));
			}
		}
	}
}

void WxPointBaseGraph::setIndicateAligment(HorizontalIndicatorAlignment horizontal, VerticalIndicatorAlignment vertical) {
	_horizontalIndicator = horizontal;
	_verticalIndicator = vertical;
}

void WxPointBaseGraph::drawBackground(bool drawBackground) {
	_drawBackground = drawBackground;
}