#include "WxLineGraph.h"

using namespace ci;
using namespace std;

WxLineGraph::WxLineGraph() {

}

WxLineGraph::~WxLineGraph() {

}

void WxLineGraph::draw() {
	FUNCTON_LOG();
	if (_displayArea.getWidth() == 0 || _displayArea.getHeight() == 0) {
		return;
	}

	auto tl_x = getX();
	auto tl_y = getY();
	if(_drawBackground)
	{
		ci::Rectf graphRect(_displayArea.x1 + tl_x, _displayArea.y1 + tl_y, _displayArea.x2 + tl_x, _displayArea.y2 + tl_y);
		gl::color(_graphRegionColor);
		gl::drawSolidRect(graphRect);

		//gl::color(_lineColor);
		//gl::drawStrokedRect(graphRect);
	}
	{
		gl::ScopedColor color(_lineColor);
		gl::begin(GL_LINE_STRIP);

		auto leftPoint = localToPoint(0, 0);
		auto rightPoint = localToPoint(_displayArea.getWidth(), 0);
		auto it = _points.rbegin();
		for (; it != _points.rend(); it++) {
			if (it->x <= rightPoint.x) {
				break;
			}
		}

		for (; it != _points.rend(); it++) {
			vec2 pos = pointToWindow(it->x, it->y);
			gl::vertex(pos);
			if (it->x < leftPoint.x) {
				break;
			}
		}
		gl::end();
	}
	drawPointAtCursor();
}