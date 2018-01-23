#include <Windows.h>

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Rand.h"

#include "UI/WxCryptoBoardInfo.h"
#include "UI/WxAppLog.h"
#include "UI/Spliter.h"
#include "UI/WxLineGraphLive.h"

#include "Engine/PlatformEngine.h"
#include "LogAdapter.h"

using namespace ci;
using namespace ci::app;
using namespace std;

const char* platformName = "dummy.txt";

class BasicApp : public App {
	Spliter _spliter;
	shared_ptr<WxAppLog> _applog;
	shared_ptr<WxLineGraphLive> _graph;
	PlatformEngine* _platformRunner;
	bool _runFlag;
	thread _liveGraphWorker;
	LogAdapter* _logAdapter;
	TIMESTAMP _baseTime;

	int _lastEventId;
	NAPMarketEventHandler* _lastSelectedHandler;
public:
	BasicApp();
	~BasicApp();
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void update() override;
	void draw() override;

	void addLog(const char* fmt, va_list args) {
		if (_applog) {
			_applog->addLogV(fmt, args);
		}
	}

	void onSelectedTradeEvent(NAPMarketEventHandler*, TradeItem*);

	static void intializeApp(App::Settings* settings);
};

void pushLog(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	((BasicApp*)app::App::get())->addLog(fmt, args);
	va_end(args);
}

void BasicApp::intializeApp(App::Settings* settings) {
	auto size = Display::getMainDisplay()->getSize();
	size.x -= 100;
	size.y -= 100;
	settings->setWindowSize(size);
}

BasicApp::BasicApp() :
	_runFlag(true), _platformRunner(nullptr),
	_logAdapter(nullptr), _lastEventId(-1), 
	_lastSelectedHandler(nullptr) {
}

BasicApp::~BasicApp() {
	if (_liveGraphWorker.joinable()) {
		_liveGraphWorker.join();
	}
	if (_platformRunner) {
		_platformRunner->getPlatform()->setLogger(nullptr);
		_platformRunner->stop();
		delete _platformRunner;
	}
	if (_logAdapter) {
		delete _logAdapter;
	}
}

void BasicApp::setup()
{
	_baseTime = getCurrentTimeStamp();

	ui::Options uiOptions;
	ui::initialize(uiOptions);

	getWindow()->getSignalResize().connect([this]() {
		int w = getWindow()->getWidth();
		int h = getWindow()->getHeight();
		_spliter.setSize((float)w, (float)h);
	});

	int w = getWindow()->getWidth();
	int h = getWindow()->getHeight();
	_spliter.setSize((float)w, (float)h);
	_spliter.setPos(0, 0);

	auto cryptoBoard = std::make_shared<WxCryptoBoardInfo>();
	auto bottomSpliter = std::make_shared<Spliter>();
	_applog = std::make_shared<WxAppLog>();
	auto graphLine = std::make_shared<WxLineGraphLive>();

	_logAdapter = new LogAdapter(_applog.get());

	bottomSpliter->setVertical(true);
	bottomSpliter->setFixedPanelSize(800);
	bottomSpliter->setFixPanel(FixedPanel::Panel1);
	bottomSpliter->setChild1(graphLine);
	bottomSpliter->setChild2(_applog);

	_spliter.setFixedPanelSize(300);
	_spliter.setFixPanel(FixedPanel::Panel2);
	_spliter.setChild1(cryptoBoard);
	_spliter.setChild2(bottomSpliter);

	graphLine->setInitalGraphRegion(Area(20, 20, graphLine->getWidth() - 20, graphLine->getHeight() - 20));
	graphLine->setGraphRegionColor(ColorA8u(0, 0, 0, 255));
	graphLine->setLineColor(ColorA8u(255, 255, 0, 255));

	getWindow()->getSignalClose().connect([this]() { _runFlag = false; });

	_graph = graphLine;

	cryptoBoard->setItemSelectionChangedHandler([this, graphLine](Widget* sender) {
		auto selectedSymbol = ((WxCryptoBoardInfo*)sender)->getSelectedSymbol();
		if (selectedSymbol == nullptr) {
			return;
		}

		auto platform = _platformRunner->getPlatform();
		if (platform == nullptr) {
			return;
		}
		auto handler = platform->getHandler(selectedSymbol);
		if (handler == nullptr) {
			return;
		}

		// reset the graph
		graphLine->clearPoints();
		graphLine->setInitalGraphRegion(graphLine->getGraphRegion());

		// remove last trade event handler
		if (_lastSelectedHandler) {
			_lastSelectedHandler->removeTradeEventListener(_lastEventId);
		}
		// add new event handler
		_lastSelectedHandler = ((NAPMarketEventHandler*)handler);

		_lastSelectedHandler->accessSharedData([this](NAPMarketEventHandler* handler) {
			auto& tickers = handler->getTickerHistoriesNonSync();
			if (tickers.size()) {
				float timeScale = 1.0f / 500;
				float xScale;

				// import the first n - 1 points
				int firstPointsCount = (int)(tickers.size() - 1);
				for (auto it = tickers.rbegin(); firstPointsCount > 0; firstPointsCount--, it++) {
					auto& lastPricePoint = it->lastPrice;
					xScale = (float)((lastPricePoint.at - _baseTime) * timeScale);
					vec2 point(xScale, (float)lastPricePoint.price);
					_graph->addPointAndConstruct(point);
				}

				// add last point and adjust the transform
				auto& lastTicker = tickers.front();
				auto& lastPricePoint = lastTicker.lastPrice;

				auto& area = _graph->getGraphRegion();
				_graph->scale(1.0f, (float)(area.getHeight() / (lastPricePoint.price / 200)));
				_graph->setTimeScale(timeScale);

				xScale = (float)((lastPricePoint.at- _baseTime) * timeScale);

				vec2 point(xScale, (float)lastPricePoint.price);
				_graph->addPoint(point);
			}
		});

		auto eventListener = std::bind(&BasicApp::onSelectedTradeEvent, this, std::placeholders::_1, std::placeholders::_2);
		_lastEventId = _lastSelectedHandler->addTradeEventListener(eventListener);
	});

	//_liveGraphWorker = std::thread([this, graphLine]() {
	//	using namespace std::chrono_literals;
	//	time_t t;
	//	std::time(&t);
	//	
	//	Rand randomMan( (uint32_t) t);
	//	vec2 randomPoint;

	//	std::chrono::duration<float, std::milli> interval;

	//	auto& area = graphLine->getGraphRegion();
	//	auto w = area.getWidth();
	//	auto h = area.getHeight();
	//	randomPoint.x = randomMan.nextFloat(0, 100000);
	//	randomPoint.y = h;

	//	auto maxY = 1000000;
	//	int count = 0;

	//	//if (maxY / area.getHeight() < 20) {
	//	//	graphLine->scale(1.0f, area.getHeight()/ maxY);
	//	//}
	//	bool firstTime = true;

	//	while (_runFlag) {
	//		randomPoint.y = randomMan.nextFloat(0, maxY);
	//		if (firstTime) {
	//			graphLine->scale(1.0f, area.getHeight() / 2 / randomPoint.y);
	//			firstTime = false;
	//		}

	//		graphLine->addPoint(randomPoint);
	//		interval = std::chrono::duration<float, std::milli>(randomMan.nextFloat(200.0f, 500.0f));
	//		std::this_thread::sleep_for(interval);

	//		randomPoint.x += interval.count()/20;
	//		count++;
	//		if (count % 6 == 0) {
	//			maxY += maxY * 0.05f;
	//		}
	//	}
	//});

	getWindow()->getSignalKeyDown().connect([this](KeyEvent& event) {
		if (event.getCode() == KeyEvent::KEY_ESCAPE) {
			pushLog("pressed escaped\n");
			_runFlag = false;
		}
	});

	_platformRunner = new PlatformEngine("binance");
	_platformRunner->getPlatform()->setLogger(_logAdapter);
	_platformRunner->run();
	auto& items = _platformRunner->getSymbolsStatistics();
	cryptoBoard->setItems(&items);
}

void BasicApp::onSelectedTradeEvent(NAPMarketEventHandler* handler, TradeItem* item) {
	auto timeScale = _graph->getTimeScale();
	float xScale = (float)( (item->timestamp - _baseTime) * timeScale);

	vec2 point(xScale, (float)item->price);
	_graph->addPoint(point);
}

void BasicApp::mouseDown( MouseEvent event )
{
}

void BasicApp::update()
{
	_spliter.update();
}

void BasicApp::draw()
{
	//static float gray = 0.65f;
	gl::clear( ColorA::black() );
	
	// any widget added without a window will be added
	// in the default "debug" window
	//ui::DragFloat( "Gray", &gray, 0.01f, 0.0f, 1.0f );

	_spliter.draw();
}

CINDER_APP(BasicApp, RendererGl( RendererGl::Options().msaa( 8 ) ), BasicApp::intializeApp)
