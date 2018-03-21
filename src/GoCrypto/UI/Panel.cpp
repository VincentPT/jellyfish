#include "Panel.h"

void Panel::updateChildrenrGeometrics() {
	for (auto it = _children.begin(); it != _children.end(); it++) {
		(*it)->setPos(getX(), getY());
		(*it)->setSize(getWidth(), getHeight());
	}
}

Panel::Panel() {
	CiWidget::setPos(0, 0);
	CiWidget::setSize(300, 200);
}
Panel::~Panel(){}

void Panel::update() {
	for (auto it = _children.begin(); it != _children.end(); it++) {
		(*it)->update();
	}
}

void Panel::draw() {
	for (auto it = _children.begin(); it != _children.end(); it++) {
		(*it)->draw();
	}
}

void Panel::setSize(float w, float h) {
	CiWidget::setSize(w, h);
	updateChildrenrGeometrics();
}

void Panel::setPos(float x, float y) {
	CiWidget::setPos(x, y);
	updateChildrenrGeometrics();
}

void Panel::addChild(const std::shared_ptr<Widget>& child) {
	_children.push_back(child);
	updateChildrenrGeometrics();
}

const std::list<std::shared_ptr<Widget>>& Panel::getChildren() const {
	return _children;
}