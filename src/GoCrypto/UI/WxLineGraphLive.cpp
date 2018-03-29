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

	if (currHeight * _scale.y > _displayArea.getHeight()) {
		_scale.y = _displayArea.getHeight() / currHeight;
	}
	else if (currHeight > 0 && _scale.y * currHeight < _displayArea.getHeight() * 2.0f / 3) {
		_scale.y = _displayArea.getHeight() * 2.0f / (3 * currHeight);
	}
	
	//if (yTop.y < 0) {
	//	translate(0, -yTop.y);
	//}
	//else if (yBellow.y > _displayArea.getHeight()) {
	//	translate(0, _displayArea.getHeight() - yBellow.y);
	//}

	yTop = pointToLocal(0, maxY);
	yBellow = pointToLocal(0, minY);
	if (yTop.y < 0) {
		translate(0, -yTop.y);
	}
	if (yBellow.y > _displayArea.getHeight()) {
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
	FUNCTON_LOG();
	std::unique_lock<std::mutex> lk(_mutex);

	if (_points.size()) {
		auto localPoint = pointToWindow(_points.back().x, _points.back().y);
		vec2 constructPoint = pointToWindow(_lastestX, _points.back().y);

		WxLineGraph::draw();

		gl::color(_lineColor);
		gl::drawLine(localPoint, constructPoint);

		ColorAf color(1, 1, 1);
		gl::color(color);

		if (_translateFunction) {
			auto pointStr = _translateFunction(_points.back());
			
			ci::Font font("Arial", 20);

			float x1 = _displayArea.x1 + getX();
			float x2  = _displayArea.x1 + getX();

			if (_horizontalIndicator == HorizontalIndicatorAlignment::Right) {
				gl::drawStringCentered(std::get<1>(pointStr), glm::vec2(x2 - 20, constructPoint.y), color, font);
			}
			else if (_horizontalIndicator == HorizontalIndicatorAlignment::Left) {
				gl::drawStringCentered(std::get<1>(pointStr), glm::vec2(x1 + 30, constructPoint.y), color, font);
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
	_lastestX = x;
	adjustHorizontalTransform(glm::vec2(_lastestX, 0));
}