#include "CiWidget.h"

CiWidget::CiWidget()
{
}


CiWidget::~CiWidget()
{
}


void CiWidget::update() {}
void CiWidget::draw() {}

void CiWidget::setSize(float w, float h) {
	_window_size.x = w;
	_window_size.y = h;
}

float CiWidget::getWidth() {
	return _window_size.x;
}

float CiWidget::getHeight() {
	return _window_size.y;
}

void CiWidget::setPos(float x, float y) {
	_window_pos.x = x;
	_window_pos.y = y;
}

float CiWidget::getX() {
	return _window_pos.x;
}

float CiWidget::getY() {
	return _window_pos.y;
}
