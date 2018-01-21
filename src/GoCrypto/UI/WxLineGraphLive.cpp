#include "WxLineGraphLive.h"
#include "cinder/app/App.h"

using namespace ci;
using namespace std;

WxLineGraphLive::WxLineGraphLive() : _timeScale(1), _generatedIdx(-2), _lastUpdate(-1) {
}

WxLineGraphLive::~WxLineGraphLive() {

}

void WxLineGraphLive::adjustTransform() {
	if (_lines.size() == 0) {
		return;
	}
	const glm::vec2& point = _lines.back();

	auto pointInWindow = pointToLocal(point.x, point.y);

	auto right = _displayArea.x2;
	auto top = _displayArea.y1;
	auto bottom = _displayArea.y2;

	float translateX = 0;
	float translateY = 0;

	if (pointInWindow.x >= right) {
		translateX = right - pointInWindow.x - _displayArea.getWidth() / 20;
	}

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

	if ((yBellow.y - yTop.y) > _displayArea.getHeight()/* || (yBellow.y - yTop.y) < _displayArea.getHeight() * 4 / 5*/) {
		auto scaleY = _displayArea.getHeight() / (yBellow.y - yTop.y) - 0.1f;
		scale(1.0f, scaleY);

		//translateY = ((yBellow.y + yTop.y) - _displayArea.getHeight()) / 2;

		auto newLocalPoint = pointToLocal(point.x, point.y);
		if (newLocalPoint.y < top) {
			translateY = newLocalPoint.y - top;
		}
		else if (newLocalPoint.y > bottom) {
			translateY = newLocalPoint.y - bottom;
		}
	}
	else if (yTop.y < top) {
		translateY = yTop.y - top;
	}
	else if (yBellow.y > bottom) {
		translateY = yBellow.y - bottom;
	}

	if (translateX || translateY) {
		translate(translateX, translateY);
	}
}

void WxLineGraphLive::addPoint(const glm::vec2& point) {
	std::unique_lock<std::mutex> lk(_mutex);
	if (_lines.size() && (int)_lines.size() - 1 == _generatedIdx) {
		auto& lastPoint = _lines.back();
		//correct auto generate point due to delay time of reponse
		// from server to client
		if (lastPoint.x > point.x) {
			lastPoint.x = point.x;
		}
	}

	WxLineGraph::addPoint(point);
	_lastUpdate = (float)ci::app::App::get()->getElapsedSeconds();
	adjustTransform();
}

void WxLineGraphLive::addPointAndConstruct(const glm::vec2& point) {
	std::unique_lock<std::mutex> lk(_mutex);
	if (_lines.size() == 0) {
		WxLineGraph::addPoint(point);
		_lastUpdate = (float)ci::app::App::get()->getElapsedSeconds();
	}
	else {
		auto constructPoint = _lines.back();
		constructPoint.x = point.x;
		WxLineGraph::addPoint(constructPoint);
		WxLineGraph::addPoint(point);
	}
}

void WxLineGraphLive::clearPoints() {
	std::unique_lock<std::mutex> lk(_mutex);
	WxLineGraph::clearPoints();
	_lastUpdate = -2;
}

//size_t WxLineGraphLive::getPointCount() const {
//	std::unique_lock<std::mutex> lk(_mutex);
//	return WxLineGraph::getPointCount();
//}

void WxLineGraphLive::update() {
	std::unique_lock<std::mutex> lk(_mutex);
	if (_lines.size() == 0) return;

	auto nowFromStart = (float)ci::app::App::get()->getElapsedSeconds();
	float duration = 0;
	if (_lastUpdate >= 0) {
		duration = (nowFromStart - _lastUpdate) * 1000;
	}

	auto& lastPoint = _lines.back();
	auto newPoint = lastPoint;

	newPoint.x += duration * _timeScale;
	auto pointInWindow1 = pointToLocal(lastPoint.x, lastPoint.y);
	auto pointInWindow2 = pointToLocal(newPoint.x, newPoint.y);

	if ((int)pointInWindow1.x == (int)pointInWindow2.x) {
		return;
	}

	//auto right = _displayArea.x2;
	//auto top = _displayArea.y1;
	//auto bottom = _displayArea.y2;

	//float translateX = 0;

	//if ((int)pointInWindow2.x >= right) {
	//	translateX = right - pointInWindow2.x - _displayArea.getWidth() / 20;
	//}
	//if (translateX) {
	//	translate(translateX, 0);
	//}
	//
	//if (_lines.size() && (int)_lines.size() - 1 == _generatedIdx) {
	//	lastPoint.x = newPoint.x;
	//}
	//else {
	//	_generatedIdx = (int)getPointCount();
	//	WxLineGraph::addPoint(newPoint);
	//}


	if (_lines.size() && (int)_lines.size() - 1 == _generatedIdx) {
		lastPoint.x = newPoint.x;
	}
	else {
		_generatedIdx = (int)getPointCount();
		WxLineGraph::addPoint(newPoint);
	}
	adjustTransform();

	_lastUpdate = nowFromStart;
}

void WxLineGraphLive::draw() {
	std::unique_lock<std::mutex> lk(_mutex);
	WxLineGraph::draw();
}

void WxLineGraphLive::setTimeScale(float scale) {
	_timeScale = scale;
}

float WxLineGraphLive::getTimeScale() const {
	return _timeScale;
}