#include <Windows.h>

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Rand.h"

#include "UI/WxCryptoBoardInfo.h"
#include "UI/WxAppLog.h"
#include "UI/Spliter.h"
#include "UI/WxLineGraphLive.h"
#include "UI/WxControlBoard.h"

#include "Engine/PlatformEngine.h"
#include "LogAdapter.h"
#include "../common/Utility.h"
#include <ConvertableCryptoInfoAdapter.h>

using namespace ci;
using namespace ci::app;
using namespace std;

const char* platformName = "dummy.txt";

class BasicApp : public App {

	typedef std::function<void()> Task;

	Spliter _spliter;
	shared_ptr<WxAppLog> _applog;
	shared_ptr<WxLineGraphLive> _graph;
	shared_ptr<WxCryptoBoardInfo> _cryptoBoard;
	shared_ptr<WxControlBoard> _controlBoard;
	SyncMessageQueue<Task> _tasks;

	
	PlatformEngine* _platformRunner;
	bool _runFlag;
	//thread _liveGraphWorker;
	thread _asynTasksWorkder;
	LogAdapter* _logAdapter;
	TIMESTAMP _baseTime;
	float _pixelPerTime;

	int _lastEventId;
	NAPMarketEventHandler* _lastSelectedHandler;
public:
	BasicApp();
	~BasicApp();
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void update() override;
	void draw() override;
	void cleanup();
	void startServices();
	void stopServices();
	void applySelectedCurrency(const std::string& currency);

	void addLog(const char* fmt, va_list args) {
		if (_applog) {
			_applog->addLogV(fmt, args);
		}
	}

	void onSelectedTradeEvent(NAPMarketEventHandler*, TradeItem* tradeItem, int, bool);

	float convertRealTimeToDisplayTime(TIMESTAMP t);

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
	//if (_liveGraphWorker.joinable()) {
	//	_liveGraphWorker.join();
	//}
	stopServices();

	_runFlag = false;
	if (_asynTasksWorkder.joinable()) {
		_asynTasksWorkder.join();
	}
	if (_logAdapter) {
		delete _logAdapter;
	}
}

void BasicApp::cleanup() {
}

void BasicApp::setup()
{
	using namespace std::placeholders;
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

	_cryptoBoard = std::make_shared<WxCryptoBoardInfo>();
	auto bottomSpliter = std::make_shared<Spliter>();
	auto topSpliter = std::make_shared<Spliter>();
	_applog = std::make_shared<WxAppLog>();
	auto graphLine = std::make_shared<WxLineGraphLive>();
	_controlBoard = std::make_shared<WxControlBoard>();

	_logAdapter = new LogAdapter(_applog.get());

	topSpliter->setVertical(true);
	topSpliter->setFixedPanelSize(130);
	topSpliter->setFixPanel(FixedPanel::Panel2);
	topSpliter->setChild1(_cryptoBoard);
	topSpliter->setChild2(_controlBoard);

	bottomSpliter->setVertical(true);
	bottomSpliter->setFixedPanelSize(800);
	bottomSpliter->setFixPanel(FixedPanel::Panel1);
	bottomSpliter->setChild1(graphLine);
	bottomSpliter->setChild2(_applog);

	_spliter.setFixedPanelSize(300);
	_spliter.setFixPanel(FixedPanel::Panel2);
	_spliter.setChild1(topSpliter);
	_spliter.setChild2(bottomSpliter);

	graphLine->setInitalGraphRegion(Area(20, 20, (int)graphLine->getWidth() - 20, (int)graphLine->getHeight() - 20));
	graphLine->setGraphRegionColor(ColorA8u(0, 0, 0, 255));
	graphLine->setLineColor(ColorA8u(255, 255, 0, 255));

	getWindow()->getSignalClose().connect([this]() {
		_runFlag = false;
	});

	_graph = graphLine;

	_baseTime = getCurrentTimeStamp();
	_graph->baseTime();

	_cryptoBoard->setItemSelectionChangedHandler([this, graphLine](Widget* sender) {
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
			auto& tickers = handler->getTradeHistoriesNonSync();
			if (tickers.size()) {

				_graph->acessSharedData([this, &tickers](Widget* sender) {
					WxLineGraphLive* graph = (WxLineGraphLive*)sender;

					TIMESTAMP timeLength = 60 * 60 * 1000;
					_pixelPerTime = (float)(graph->getWidth() / timeLength);
					float timeScale;

					// import the first n - 1 points
					for (auto it = tickers.rbegin(); it != tickers.rend(); it++) {
						timeScale = convertRealTimeToDisplayTime(it->timestamp);
						vec2 point(timeScale, (float)it->price);
						graph->addPointAndConstruct(point);
					}

					// add last point and adjust the transform
					auto& lastTicker = tickers.front();

					auto& area = graph->getGraphRegion();
					graph->scale(1.0f, (float)(area.getHeight() / (lastTicker.price / 200)));

					// at time zero, the point will display at x = graph->getWidth() * 4 / 5
					float expectedXAtBeginTime = graph->getWidth() * 4 / 5;
					graph->mapZeroTime(expectedXAtBeginTime);
					graph->adjustTransform();
					graph->setTimeScale(_pixelPerTime);

					auto t = convertRealTimeToDisplayTime(lastTicker.timestamp);
					auto actualPointAtT = graph->pointToLocal(t, (float)lastTicker.price);

					auto expectedXAtT = expectedXAtBeginTime + t;
					auto correctionX = expectedXAtT - actualPointAtT.x;
					if (correctionX > 0) {
						graph->translate(correctionX, 0);
					}
					graph->adjustTransform();
				});
			}
		});

		auto eventListener = std::bind(&BasicApp::onSelectedTradeEvent, this, _1, _2, _3, _4);
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

	_controlBoard->setOnStartButtonClickHandler([this](Widget*) {
		Task task = std::bind(&BasicApp::startServices, this);
		_tasks.pushMessage(task);
	});
	_controlBoard->setOnStopButtonClickHandler([this](Widget*) {
		Task task = std::bind(&BasicApp::stopServices, this);
		_tasks.pushMessage(task);
	});
	_controlBoard->setOnSelectedCurrencyChangedHandler([this](Widget*) {
		applySelectedCurrency(_controlBoard->getCurrentCurrency());
	});

	_asynTasksWorkder = std::thread([this]() {
		while (_runFlag)
		{
			BasicApp::Task task;
			if (_tasks.popMessage(task, 1000)) {
				task();
			}
		}
	});
}

void BasicApp::startServices() {
	LOG_SCOPE_ACCESS(_logAdapter, __FUNCTION__);
	if (_platformRunner) {
		pushLog("services have been already started\n");
		return;
	}

	_platformRunner = new PlatformEngine("bitfinex");
	_platformRunner->getPlatform()->setLogger(_logAdapter);
	_platformRunner->run();

	_cryptoBoard->accessSharedData([this](Widget*) {
		auto& items = _platformRunner->getSymbolsStatistics();
		_cryptoBoard->setItems(&items);
	});
	_controlBoard->accessSharedData([this](Widget*) {
		_controlBoard->setBaseCurrencies(_platformRunner->getCurrencies());
	});
}

void BasicApp::stopServices() {
	LOG_SCOPE_ACCESS(_logAdapter, __FUNCTION__);
	if (!_platformRunner) {
		pushLog("services are not running\n");
		return;
	}
	
	_platformRunner->stop();
	_cryptoBoard->accessSharedData([this](Widget*) {
		_cryptoBoard->resetCryptoAdapterToDefault();
		_cryptoBoard->setItems(nullptr);
	});
	_controlBoard->accessSharedData([this](Widget*) {
		// reset currencies to empty
		_controlBoard->setBaseCurrencies({});
	});
	
	_platformRunner->getPlatform()->setLogger(nullptr);
	delete _platformRunner;
	_platformRunner = nullptr;
}

void BasicApp::applySelectedCurrency(const std::string& currency) {
	if (currency == DEFAULT_CURRENCY) {
		_cryptoBoard->accessSharedData([this](Widget* sender) {
			((WxCryptoBoardInfo*)sender)->resetCryptoAdapterToDefault();
			auto& items = _platformRunner->getSymbolsStatistics();
			((WxCryptoBoardInfo*)sender)->setItems(&items);
		});
	}
	else {
		_cryptoBoard->accessSharedData([&currency, this](Widget* sender) {
			auto crytoBoard = (WxCryptoBoardInfo*)sender;
			auto& elmInfoOffsets = crytoBoard->getRawElemInfoOffsets();
			auto adapter = make_shared<ConvertableCryptoInfoAdapter>(elmInfoOffsets);
			adapter->setCurrency(currency);
			adapter->intialize(crytoBoard->getItems(), _platformRunner);

			crytoBoard->setAdapter(adapter);
		});
	}
}

void BasicApp::onSelectedTradeEvent(NAPMarketEventHandler* handler, TradeItem* item, int count, bool) {
	_graph->acessSharedData([this, item, count, handler](Widget* sender) {
		WxLineGraphLive* graph = (WxLineGraphLive*)sender;
		graph->clearPoints();

		auto& tradeItems = handler->getTradeHistoriesNonSync();
		auto platform = _platformRunner->getPlatform();
		TIMESTAMP timeDiff = 0;
		if (platform->isServerTimeReady()) {
			auto localTime = getCurrentTimeStamp();
			auto serverTime = platform->getSyncTime(localTime);
			timeDiff = localTime - serverTime;
		}

		for (auto it = tradeItems.rbegin(); it != tradeItems.rend(); it++) {
			float timeScale = convertRealTimeToDisplayTime(it->timestamp + timeDiff);

			vec2 point(timeScale, (float)it->price);
			graph->addPointAndConstruct(point);
		}
		
		graph->adjustTransform();
	});
}

float BasicApp::convertRealTimeToDisplayTime(TIMESTAMP t) {
	return (float)((t - _baseTime) *_pixelPerTime);
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
