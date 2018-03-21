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

	adjustHorizontalTransform(point);
	adjustVerticalTransform(point);
}

void WxLineGraphLive::adjustVerticalTransform(const glm::vec2& point) {
	if (_points.size() == 0) {
		return;
	}
	float translateY = 0;

	auto leftPoint = localToPoint(0, 0);
	float minY = FLT_MAX;
	float maxY = FLT_MIN;
	for (auto it = _points.rbegin(); it != _points.rend(); it++) {
		if (it->x < leftPoint.x) {
			break;
		}

		if (minY > it->y) {
			minY = it->y;
		}
		if (maxY < it->y) {
			maxY = it->y;
		}
	}
	auto yTop = pointToLocal(0, maxY);
	auto yBellow = pointToLocal(0, minY);
	auto currHeight = std::abs(yBellow.y - yTop.y);

	if (currHeight > _displayArea.getHeight()) {
		auto scaleY = _displayArea.getHeight() / currHeight;
		scale(1.0f, scaleY);

		yTop = pointToLocal(0, maxY);
		yBellow = pointToLocal(0, minY);
	}
	
	if (yTop.y < 0) {
		translate(0, -yTop.y);
	}
	else if (yBellow.y > _displayArea.getHeight()) {
		translate(0, _displayArea.getHeight() - yBellow.y);
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

void WxLineGraphLive::addPoint(const glm::vec2& point) {
	WxLineGraph::addPoint(point);
	_lastestX = point.x;
	adjustTransform();
}

void WxLineGraphLive::acessSharedData(const AccessSharedDataFunc& f) {
	std::unique_lock<std::mutex> lk(_mutex);
	f(this);
}

void WxLineGraphLive::addPointAndConstruct(const glm::vec2& point) {
	if (_points.size() == 0) {
		WxLineGraph::addPoint(point);
	}
	else {
		auto constructPoint = _points.back();
		constructPoint.x = point.x;
		WxLineGraph::addPoint(constructPoint);
		WxLineGraph::addPoint(point);
	}
	_lastestX = point.x;
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
	std::unique_lock<std::mutex> lk(_mutex);

	gl::ScopedColor color(_lineColor);
	if (_points.size()) {
		auto points = _points;

		auto localPoint = pointToWindow(points.back().x, points.back().y);
		vec2 constructPoint = pointToWindow(_lastestX, points.back().y);

		WxLineGraph::draw();
		gl::drawLine(localPoint, constructPoint);
		gl::drawSolidCircle(constructPoint, 3);
	}
	else {
		WxLineGraph::draw();
	}
}

void WxLineGraphLive::setLiveX(float x) {
	_lastestX = x;
	adjustHorizontalTransform(glm::vec2(_lastestX, 0));
}