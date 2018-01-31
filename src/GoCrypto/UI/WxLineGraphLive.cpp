#include "WxLineGraphLive.h"
#include "cinder/app/App.h"

using namespace ci;
using namespace std;

WxLineGraphLive::WxLineGraphLive() : _pixelPerMicroSeconds(0.001f), _generatedIdx(-2), _xAtZero(0), _lastestX(0){
}

WxLineGraphLive::~WxLineGraphLive() {

}

void WxLineGraphLive::adjustTransform() {
	if (_lines.size() == 0) {
		return;
	}
	const glm::vec2& point = _lines.back();

	adjustHorizontalTransform(point);
	adjustVerticalTransform(point);
}

void WxLineGraphLive::adjustVerticalTransform(const glm::vec2& point) {
	if (_lines.size() == 0) {
		return;
	}

	auto top = _displayArea.y1;
	auto bottom = _displayArea.y2;

	float translateY = 0;

	auto leftPoint = localToPoint(_displayArea.x1, 0);
	float minY = FLT_MAX;
	float maxY = FLT_MIN;
	for (auto it = _lines.rbegin(); it != _lines.rend(); it++) {
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
	auto yBellow = pointToLocal(0, minY);
	auto yTop = pointToLocal(0, maxY);
	if (yTop.y > yBellow.y) {
		std::swap(yBellow, yTop);
	}

	if ((yBellow.y - yTop.y) > _displayArea.getHeight()) {
		auto scaleY = _displayArea.getHeight() / (yBellow.y - yTop.y) - 0.1f;
		scale(1.0f, scaleY);

		auto newLocalPoint = pointToLocal(point.x, point.y);
		if (newLocalPoint.y < top) {
			translateY = newLocalPoint.y - top;
		}
		else if (newLocalPoint.y > bottom) {
			translateY = newLocalPoint.y - bottom;
		}
	}
	else if ((yBellow.y - yTop.y) < _displayArea.getHeight() * 4.0f / 5) {
		auto scaleY = _displayArea.getHeight() / (yBellow.y - yTop.y) - 0.1f;
		scale(1.0f, scaleY);

		auto yBellow = pointToLocal(0, minY);
		auto yTop = pointToLocal(0, maxY);
		auto yMiddle = (yBellow.y + yTop.y) / 2;

		translateY = yMiddle - _displayArea.getHeight() / 2.0f;
	}
	else if (yTop.y < top) {
		translateY = yTop.y - top;
	}
	else if (yBellow.y > bottom) {
		translateY = yBellow.y - bottom;
	}

	if (translateY) {
		translate(0, translateY);
	}
}

void WxLineGraphLive::adjustHorizontalTransform(const glm::vec2& point) {
	auto localPoint = pointToLocal(point.x, point.y);

	auto actualXForPoint = localPoint.x;
	auto expectedX = _xAtZero;
	auto translateX = expectedX - actualXForPoint;

	if (translateX >= 0) {
		auto lastestPoint = point;

		auto currentTime = ci::app::App::get()->getElapsedSeconds();
		auto currentExternalTime = 0 + (currentTime - _baseTime) * 1000;
		auto actualExternalTime = lastestPoint.x / _pixelPerMicroSeconds;
		auto paddingTime = currentExternalTime - actualExternalTime;
		
		auto expectXForPoint = expectedX - paddingTime * _pixelPerMicroSeconds;
		translateX = expectXForPoint - actualXForPoint;
	}

	translate(translateX, 0);
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
	if (_lines.size() == 0) {
		WxLineGraph::addPoint(point);
	}
	else {
		auto constructPoint = _lines.back();
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
	if (_lines.size()) {
		auto points = _lines;
		//auto lastestPoint = points.back();

		auto currentTime = ci::app::App::get()->getElapsedSeconds();
		auto currentExternalTime = 0 + (currentTime - _baseTime) * 1000;
		auto actualExternalTime = _lastestX / _pixelPerMicroSeconds;
		auto paddingTime = currentExternalTime - actualExternalTime;

		_lastestX += paddingTime * _pixelPerMicroSeconds;
		adjustHorizontalTransform(vec2(_lastestX,0));

		auto localPoint = pointToWindow(points.back().x, points.back().y);
		vec2 constructPoint(_xAtZero, localPoint.y);

		WxLineGraph::draw();
		gl::drawLine(localPoint, constructPoint);
		gl::drawSolidCircle(constructPoint, 3);
	}
	else {
		WxLineGraph::draw();
	}
}

void WxLineGraphLive::setTimeScale(float scale) {
	_pixelPerMicroSeconds = scale;
}

float WxLineGraphLive::getTimeScale() const {
	return _pixelPerMicroSeconds;
}

void WxLineGraphLive::mapZeroTime(float x) {
	_xAtZero = x;
}


void WxLineGraphLive::baseTime() {
	_baseTime = ci::app::App::get()->getElapsedSeconds();
}