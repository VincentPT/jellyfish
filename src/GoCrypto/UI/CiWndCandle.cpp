#include "CiWndCandle.h"

using namespace ci;
using namespace ci::app;

CiWndCandle::CiWndCandle() {
	_windowUIRef = ci::app::App::get()->createWindow();
	_windowUIRef->setUserData(this);
}

void CiWndCandle::createWindow() {
	// just create an window data for Cinder's Window
	// the window data will be automatically destroyed when the Cinder's window is destroyed
	new CiWndCandle();
}

CiWndCandle::~CiWndCandle() {

}

void CiWndCandle::update() {

}

void CiWndCandle::draw() {
	gl::ScopedColor colorScoped(1, 1, 1);
	gl::drawSolidCircle(_windowUIRef->getCenter(), 100);
}