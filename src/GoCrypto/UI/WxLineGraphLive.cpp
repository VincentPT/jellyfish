#include "WxLineGraphLive.h"
#include "cinder/app/App.h"

using namespace ci;
using namespace std;

WxLineGraphLive::WxLineGraphLive() : _lastestX(0) {
}

WxLineGraphLive::~WxLineGraphLive() {

}

void WxLineGraphLive::adjustTransform() {
	if (_points.size() == 0) {
		return;
	}
	const glm::vec2& point = _points.back();

	//adjustHorizontalTransform(point);
	adjustVerticalTransform(point);
}

void WxLineGraphLive::adjustVerticalTransform(const glm::vec2& point) {
	if (_points.size() == 0) {
		return;
	}
	float translateY = 0;

	auto left = localToPoint(0, 0);
	auto right = localToPoint(_displayArea.getWidth(), 0);
	auto it = _points.rbegin();
	for (; it != _points.rend(); it++) {
		if (it->x <= right.x) {
			break;
		}
	}
	if (it == _points.rend()) return;

	float minY = it->y;
	float maxY = minY;

	for (; it != _points.rend() && it->x >= left.x; it++) {
		if (maxY < it->y) {
			maxY = it->y;
		}
		if (minY > it->y) {
			minY = it->y;
		}
	}

	auto yTop = pointToLocal(0, maxY);
	auto yBellow = pointToLocal(0, minY);
	auto currHeight = maxY - minY;

	auto autoScaleRangeY1 = _autoScaleRange.x - _displayArea.y1;
	auto autoScaleRangeY2 = _autoScaleRange.y - _displayArea.y1;
	auto autoScaleHeight = autoScaleRangeY2 - autoScaleRangeY1;

	if (currHeight > 0 && currHeight * _scale.y != autoScaleHeight) {
		_scale.y = autoScaleHeight / currHeight;
	}

	yTop = pointToLocal(0, maxY);
	yBellow = pointToLocal(0, minY);
	if (yTop.y < autoScaleRangeY1) {
		translate(0, autoScaleRangeY1 - yTop.y);
	}
	if (yBellow.y > autoScaleRangeY2) {
		translate(0, autoScaleRangeY2 - yBellow.y);
	}
}

inline auto getCurrentExternalTime(const double& baseTime) {
	auto currentTime = ci::app::App::get()->getElapsedSeconds();
	return  0 + (currentTime - baseTime) * 1000;
}

void WxLineGraphLive::adjustHorizontalTransform(const glm::vec2& point) {
	auto localPoint = pointToLocal(point.x, point.y);

	if (localPoint.x > _displayArea.getWidth()) {
		auto translateX = _displayArea.getWidth() - localPoint.x;
		translate(translateX, 0);
	}
}

//void WxLineGraphLive::addPoint(const glm::vec2& point) {
//	WxLineGraph::addPoint(point);
//	_lastestX = point.x;
//	adjustTransform();
//}

void WxLineGraphLive::acessSharedData(const AccessSharedDataFunc& f) {
	std::unique_lock<std::mutex> lk(_mutex);
	f(this);
}

void WxLineGraphLive::addPointAndConstruct(const glm::vec2& point) {
	if (_points.size() == 0) {
		WxLineGraph::addPoint(point);
		//_lastestX = point.x - 1;
	}
	else {
		auto constructPoint = _points.back();
		constructPoint.x = point.x;
		WxLineGraph::addPoint(constructPoint);
		WxLineGraph::addPoint(point);
	}
	//if (_lastestX < point.x) {
		_lastestX = point.x;
	//	adjustHorizontalTransform(point);
	//}
}

void WxLineGraphLive::clearPoints() {
	WxLineGraph::clearPoints();
}

//size_t WxLineGraphLive::getPointCount() const {
//	std::unique_lock<std::mutex> lk(_mutex);
//	return WxLineGraph::getPointCount();
//}

void WxLineGraphLive::update() {
}

void WxLineGraphLive::draw() {
	FUNCTON_LOG();
	std::unique_lock<std::mutex> lk(_mutex);

	if (_points.size()) {
		auto localPoint = pointToWindow(_points.back().x, _points.back().y);
		vec2 constructPoint = pointToWindow(_lastestX, _points.back().y);

		WxLineGraph::draw();

		gl::color(_lineColor);
		gl::drawLine(localPoint, constructPoint);

		ColorAf color(1, 1, 0);
		gl::color(color);

		if (_translateFunction) {
			auto pointStr = _translateFunction(_points.back());
			
			ci::Font font("Arial", 20);

			float x1 = _displayArea.x1 + getX();
			float x2  = _displayArea.x1 + getX();

			if (_horizontalIndicator == HorizontalIndicatorAlignment::Right) {
				gl::drawStringRight(std::get<1>(pointStr), glm::vec2(x2, constructPoint.y), color, font);
			}
			else if (_horizontalIndicator == HorizontalIndicatorAlignment::Left) {
				gl::drawString(std::get<1>(pointStr), glm::vec2(x1, constructPoint.y), color, font);
			}
			
			if (_horizontalIndicator == HorizontalIndicatorAlignment::Right) {
				gl::drawLine(constructPoint, glm::vec2(x2, constructPoint.y));
			}
			else if (_horizontalIndicator == HorizontalIndicatorAlignment::Left) {
				gl::drawLine(constructPoint, glm::vec2(x1, constructPoint.y));
			}
		}

		gl::drawSolidCircle(constructPoint, 3);
	}
	else {
		WxLineGraph::draw();
	}
}

void WxLineGraphLive::setLiveX(float x) {
	if (x > _lastestX) {
		_lastestX = x;
		adjustHorizontalTransform(glm::vec2(_lastestX, 0));
	}
}

float WxLineGraphLive::getLiveX() const {
	return _lastestX;
}

void WxLineGraphLive::setInitalGraphRegion(const ci::Area& area) {
	WxLineGraph::setInitalGraphRegion(area);
	setAutoScaleRange((float)area.y1, (float)area.y2);
}

void WxLineGraphLive::setAutoScaleRange(float y1, float y2) {
	_autoScaleRange.x = y1;
	_autoScaleRange.y = y2;
}