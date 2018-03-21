#include "WxBarCharLive.h"
#include "cinder/app/App.h"

using namespace ci;
using namespace std;

WxBarCharLive::WxBarCharLive() : _currentX(0) {
}

WxBarCharLive::~WxBarCharLive() {

}

void WxBarCharLive::adjustTransform() {
	auto left = localToPoint(0, 0);
	auto it = _points.begin();
	for (; it != _points.end(); it++) {
		if (it->x >= left.x) {
			break;
		}
	}
	if (it != _points.end()) {
		float max_y = it->y;

		for (; it != _points.end(); it++) {
			if (max_y < it->y) {
				max_y = it->y;
			}
		}

		if (_scale.y * max_y > (float)_displayArea.getHeight()) {
			_scale.y = _displayArea.getHeight() / max_y;
		}

		auto bottom = pointToLocal(0, 0);
		if (bottom.y != _displayArea.getHeight()) {
			translate(0, _displayArea.getHeight() - bottom.y);
		}
	}
}

void WxBarCharLive::update() {
}

void WxBarCharLive::draw() {
	std::unique_lock<std::mutex> lk(_mutex);

	if (_displayArea.getWidth() == 0 || _displayArea.getHeight() == 0) {
		return;
	}

	auto tl_x = getX();
	auto tl_y = getY();
	if (_drawBackground)
	{
		ci::Rectf graphRect(_displayArea.x1 + tl_x, _displayArea.y1 + tl_y, _displayArea.x2 + tl_x, _displayArea.y2 + tl_y);
		gl::ScopedColor color(_graphRegionColor);
		gl::drawSolidRect(graphRect);
	}

	glm::vec2 leftMostPoint = localToPoint(0, 0);
	// skip out of range bars
	auto it = _points.begin();
	for (; it != _points.end(); it++) {
		if (it->x >= leftMostPoint.x) {
			break;
		}
	}
	if (it == _points.end()) return;
	if (it != _points.begin()) {
		it--;
	}

	auto basePoint = pointToWindow(0, 0);

	glm::vec2 point1 = pointToWindow(it->x, it->y);
	glm::vec2 point2;

	for (it++; true ; it++) {
		if (it == _points.end()) {
			point2 = pointToWindow(_currentX, 0);
		}
		else {
			point2 = pointToWindow(it->x, it->y);
		}
		Rectf barRect(point1.x, point1.y, point2.x, basePoint.y);

		gl::color(_barColor);
		gl::drawSolidRect(barRect);

		gl::color(_lineColor);
		gl::drawStrokedRect(barRect);

		point1 = point2;
		if (it == _points.end()) break;
	}

	drawPointAtCursor();
}

void WxBarCharLive::acessSharedData(const AccessSharedDataFunc& f) {
	std::unique_lock<std::mutex> lk(_mutex);
	f(this);
}

void WxBarCharLive::setBarColor(const ci::ColorA8u& color) {
	_barColor = color;
}

const ci::ColorA8u& WxBarCharLive::getBarColor() const {
	return _barColor;
}

void WxBarCharLive::setLiveX(float x) {
	_currentX = x;
	auto rightMostPoint = pointToLocal(_currentX, 0);
	if (rightMostPoint.x > _displayArea.x2) {
		translate(rightMostPoint.x - _displayArea.x2, 0);
	}
}