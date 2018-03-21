#include "CiWndCandle.h"
#include "UI/WxCryptoBoardInfo.h"

using namespace ci;
using namespace ci::app;

CiWndCandle::CiWndCandle() {
	_windowUIRef = ci::app::App::get()->createWindow();
	_windowUIRef->setUserData(this);

	_windowUIRef->getSignalResize().connect([this]() {
		int w = _windowUIRef->getWidth();
		int h = _windowUIRef->getHeight();
		_spliter.setSize((float)w, (float)h);
	});

	_windowUIRef->getRenderer()->makeCurrentContext();

	int w = getWindow()->getWidth();
	int h = getWindow()->getHeight();
	_spliter.setSize((float)w, (float)h);
	_spliter.setPos(0, 0);

	_spliter.setVertical(true);
	_spliter.setFixedPanelSize(130);
	_spliter.setFixPanel(FixedPanel::Panel2);

	auto cryptoBoard = std::make_shared<WxCryptoBoardInfo>();
	_spliter.setChild2(cryptoBoard);
}

ci::app::WindowRef CiWndCandle::createWindow() {
	// just create an window data for Cinder's Window
	// the window data will be automatically destroyed when the Cinder's window is destroyed
	auto ciWnd = new CiWndCandle();
	return ciWnd->_windowUIRef;
}

CiWndCandle::~CiWndCandle() {

}

void CiWndCandle::update() {
	_spliter.update();
}

void CiWndCandle::draw() {
	
}