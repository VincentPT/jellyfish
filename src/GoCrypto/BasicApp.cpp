#include <Windows.h>

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Rand.h"

#include "UI/WxCryptoBoardInfo.h"
#include "UI/WxAppLog.h"
#include "UI/Spliter.h"
#include "UI/WxLineGraphLive.h"
#include "UI/WxBarCharLive.h"
#include "UI/WxControlBoard.h"
#include "UI/Panel.h"

#include "Engine/PlatformEngine.h"
#include "LogAdapter.h"
#include "../common/Utility.h"
#include <ConvertableCryptoInfoAdapter.h>
#include "UI/CiWndCandle.h"

using namespace ci;
using namespace ci::app;
using namespace std;

const char* platformName = "dummy.txt";

class BasicApp : public App {

	typedef std::function<void()> Task;

	Spliter _spliter;
	shared_ptr<WxAppLog> _applog;
	shared_ptr<WxLineGraphLive> _graph;
	shared_ptr<WxBarCharLive> _barChart;
	shared_ptr<WxCryptoBoardInfo> _cryptoBoard;
	shared_ptr<WxControlBoard> _controlBoard;
	SyncMessageQueue<Task> _tasks;
	CiWndCandle* _ciWndCandle = nullptr;
	
	PlatformEngine* _platformRunner;
	bool _runFlag;
	//thread _liveGraphWorker;
	thread _asynTasksWorkder;
	LogAdapter* _logAdapter;
	TIMESTAMP _baseTime;
	float _pixelPerTime;

	int _lastEventId;
	int _lastCandleEventId;
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
	void onCandle(NAPMarketEventHandler* sender, CandleItem* candleItems, int count, bool snapshot);

	float convertRealTimeToDisplayTime(TIMESTAMP t);
	TIMESTAMP convertXToTime(float x);

	static void intializeApp(App::Settings* settings);

	void createCandleWindow();

	void onSelectedSymbolChanged(Widget*);
	void onSelectedGraphModeChanged(Widget*);
	void initBarchart(const std::list<CandleItem>& candles);
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
	_logAdapter(nullptr), _lastEventId(-1), _lastCandleEventId(-1),
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
	_barChart = std::make_shared<WxBarCharLive>();
	auto panel = std::make_shared<Panel>();
	panel->addChild(_barChart);
	panel->addChild(graphLine);

	_logAdapter = new LogAdapter(_applog.get());

	topSpliter->setVertical(true);
	topSpliter->setFixedPanelSize(130);
	topSpliter->setFixPanel(FixedPanel::Panel2);
	topSpliter->setChild1(_cryptoBoard);
	topSpliter->setChild2(_controlBoard);

	bottomSpliter->setVertical(true);
	bottomSpliter->setFixedPanelSize(800);
	bottomSpliter->setFixPanel(FixedPanel::Panel1);
	bottomSpliter->setChild1(panel);
	bottomSpliter->setChild2(_applog);

	_spliter.setFixedPanelSize(300);
	_spliter.setFixPanel(FixedPanel::Panel2);
	_spliter.setChild1(topSpliter);
	_spliter.setChild2(bottomSpliter);

	graphLine->setInitalGraphRegion(Area(20, 20, (int)graphLine->getWidth() - 20, (int)graphLine->getHeight() - 20));
	graphLine->setGraphRegionColor(ColorA8u(0, 0, 0, 255));
	graphLine->setLineColor(ColorA8u(255, 255, 0, 255));

	_barChart->setInitalGraphRegion(Area(20, 20, (int)_barChart->getWidth() - 20, (int)_barChart->getHeight() - 20));
	_barChart->setGraphRegionColor(ColorA8u(0, 0, 0, 255));
	_barChart->setLineColor(ColorA8u(255, 255, 0, 255));
	_barChart->setBarColor(ColorA8u(69, 69, 69, 255));

	graphLine->setIndicateAligment(HorizontalIndicatorAlignment::Left, VerticalIndicatorAlignment::None);
	_barChart->setIndicateAligment(HorizontalIndicatorAlignment::Right, VerticalIndicatorAlignment::Bottom);

	graphLine->drawBackground(false);

	getWindow()->getSignalClose().connect([this]() {
		_runFlag = false;
	});
	
	_graph = graphLine;

	auto fTextTranslate = [this](const glm::vec2& point) -> std::tuple<std::string, std::string> {
		TIMESTAMP t = convertXToTime(point.x);
		char buffer[32];
		time_t tst = (time_t)(t / 1000);
		struct tm * timeinfo;
		timeinfo = localtime(&tst);
		strftime(buffer, sizeof(buffer), "%b %d %T", timeinfo);

		std::tuple<std::string, std::string> pointStr(buffer, std::to_string(point.y));
		return pointStr;
	};

	_graph->setPointToTextTranslateFunction(fTextTranslate);
	_barChart->setPointToTextTranslateFunction(fTextTranslate);

	getWindow()->getSignalMouseMove().connect([this](MouseEvent& me) {
		auto graphMode = _controlBoard->getCurrentGraphMode();
		_graph->setCursorLocation(glm::vec2(me.getX(), me.getY()));
		_barChart->setCursorLocation(glm::vec2(me.getX(), me.getY()));
	});

	_baseTime = getCurrentTimeStamp();
	_cryptoBoard->setItemSelectionChangedHandler( std::bind(&BasicApp::onSelectedSymbolChanged, this, _1) );

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

	_controlBoard->setOnExportButtonClickHandler([this](Widget*) {
		if (_platformRunner == nullptr) return;
		auto& symbolsTickers = _platformRunner->getSymbolsTickers();
		int symbolIdx = _cryptoBoard->getSelectedSymbolIndex();
		const char* symbol = _cryptoBoard->getSelectedSymbol();
		if (symbolIdx >= 0 && symbolIdx < (int)symbolsTickers.size()) {
			auto tickers = symbolsTickers[symbolIdx];

			ofstream fs;
			string fileName(symbol);
			fileName.append(".csv");
			fs.open(fileName, std::ios::out);
			if (fs.is_open()) {

				fs << "time,buy,sell,average price" << std::endl;
				for (auto it = tickers.begin(); it != tickers.end(); it++) {
					auto& ticker = *it;

					fs << Utility::time2str(ticker.firstPrice.at) << ",";
					fs << ticker.boughtVolume << ",";
					fs << ticker.soldVolume << ",";
					fs << ticker.averagePrice << std::endl;
				}
			}

			// export candles
			auto handler  = (NAPMarketEventHandler*) _platformRunner->getPlatform()->getHandler(symbol);
			if (handler) {
				handler->accessSharedData([](NAPMarketEventHandler* sender) {
					auto& candles = sender->getCandleHistoriesNonSync();

					ofstream fs;
					string fileName(sender->getPair());
					fileName.append("_candles.txt");
					fs.open(fileName, std::ios::out);
					if (fs.is_open()) {
						for (auto it = candles.begin(); it != candles.end(); it++) {
							auto& item = *it;

							fs << "{" << Utility::time2str(item.timestamp) << ", " << item.open << ", " << item.close << ", " << item.high << ", " << item.low << ", " << item.volume << "}" << endl;
						}
					}
				});
			}
		}
	});

	_controlBoard->setOnSelectedCurrencyChangedHandler([this](Widget*) {
		applySelectedCurrency(_controlBoard->getCurrentCurrency());
	});

	//_controlBoard->setOnSelectedGraphModeChangedHandler(std::bind(&BasicApp::onSelectedGraphModeChanged, this, _1));

	//_controlBoard->setOnCandleButtonClickHandler([this](Widget*) {
	//	createCandleWindow();
	//});

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

void BasicApp::onSelectedSymbolChanged(Widget*) {
	using namespace std::placeholders;

	auto selectedSymbol = _cryptoBoard->getSelectedSymbol();
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

	// remove last trade event handler
	if (_lastSelectedHandler) {
		_lastSelectedHandler->removeTradeEventListener(_lastEventId);
		_lastSelectedHandler->removeCandleEventListener(_lastCandleEventId);
	}
	// add new event handler
	_lastSelectedHandler = ((NAPMarketEventHandler*)handler);

	_lastSelectedHandler->accessSharedData([this](NAPMarketEventHandler* handler) {
		auto graphMode = _controlBoard->getCurrentGraphMode();
		if (graphMode == GraphMode::Price) {
			auto& tickers = handler->getTradeHistoriesNonSync();
			if (tickers.size()) {
				_graph->acessSharedData([this, &tickers](Widget* sender) {
					WxLineGraphLive* graph = (WxLineGraphLive*)sender;

					// reset the graph
					_graph->clearPoints();
					_graph->setInitalGraphRegion(_graph->getGraphRegion());


					TIMESTAMP timeLength = 4 * 60 * 60 * 1000;
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
					float expectedXAtBeginTime = area.getWidth() * 95 / 100;

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
		}
		else if (graphMode == GraphMode::Volume) {
			auto& candles = handler->getCandleHistoriesNonSync();
			if (candles.size()) {
				initBarchart(candles);
			}
		}
	});

	//auto eventListener = std::bind(&BasicApp::onSelectedTradeEvent, this, _1, _2, _3, _4);
	auto candleEventListener = std::bind(&BasicApp::onCandle, this, _1, _2, _3, _4);
	//_lastEventId = _lastSelectedHandler->addTradeEventListener(eventListener);
	_lastCandleEventId = _lastSelectedHandler->addCandleEventListener(candleEventListener);
}

void BasicApp::onSelectedGraphModeChanged(Widget*) {
	auto bottomSpilter = (Spliter*)_spliter.getChild2().get();
	if (_controlBoard->getCurrentGraphMode() == GraphMode::Volume) {
		static bool initGraph = false;
		bottomSpilter->setChild1(_barChart);
		if (initGraph == false) {
			_barChart->setInitalGraphRegion(Area(20, 20, (int)_barChart->getWidth() - 20, (int)_barChart->getHeight() - 20));
			_barChart->setGraphRegionColor(ColorA8u(0, 0, 0, 255));
			_barChart->setLineColor(ColorA8u(255, 255, 0, 255));
			_barChart->setBarColor(ColorA8u(69, 69, 69, 255));
		}
	}
	else {
		bottomSpilter->setChild1(_graph);
	}

	onSelectedSymbolChanged(nullptr);
}

void BasicApp::createCandleWindow() {
	if (_ciWndCandle == nullptr) {
		auto wndRef = CiWndCandle::createWindow();
		_ciWndCandle = wndRef->getUserData<CiWndCandle>();

		wndRef->getSignalClose().connect([this]() {
			_ciWndCandle = nullptr;
		});
	}
}

void BasicApp::startServices() {
	LOG_SCOPE_ACCESS(_logAdapter, __FUNCTION__);
	if (_platformRunner) {
		pushLog("services have been already started\n");
		return;
	}

	_platformRunner = new PlatformEngine(_controlBoard->getCurrentPlatform());
	_platformRunner->getPlatform()->setLogger(_logAdapter);
	_platformRunner->setSymbolStatisticUpdatedHandler([this](int i) {
		_cryptoBoard->refreshCached(i);
	}
	);

	_cryptoBoard->setPeriod(_platformRunner->getPeriods());

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
	_graph->acessSharedData([](Widget* sender) {
		((WxLineGraph*)sender)->clearPoints();
	});

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
	if (_controlBoard->getCurrentGraphMode() != GraphMode::Price) {
		return;
	}

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

void BasicApp::onCandle(NAPMarketEventHandler* sender, CandleItem* candleItems, int count, bool snapshot) {
	if (_controlBoard->getCurrentGraphMode() != GraphMode::Volume) {
		return;
	}

	if (snapshot) {
		auto& candles = sender->getCandleHistoriesNonSync();
		initBarchart(candles);
	}
	else {
		_barChart->acessSharedData([this, candleItems, sender](Widget*) {
			constexpr int minLength = 15;
			TIMESTAMP barTimeLength = minLength * 60 * 1000;
			auto lastAlignedTime = roundDown(candleItems->timestamp, barTimeLength);
			auto& candles = sender->getCandleHistoriesNonSync();
			if (candles.size() == 0)
				return;

			// only calculate last hour
			auto lastTotalVolume = 0;
			for (auto it = candles.begin(); it != candles.end() && it->timestamp >= lastAlignedTime; it++) {
				lastTotalVolume += it->volume;
			}

			auto& points = _barChart->getPoints();
			if (points.size()) {
				auto& lastPoint = points.back();
				auto lastTimeInChart = convertXToTime(lastPoint.x);
				auto lastAnlignedTimeInChart = roundDown(lastTimeInChart, barTimeLength);

				if (lastAlignedTime == lastAnlignedTimeInChart) {
					// update last bar
					lastPoint.y = lastTotalVolume;
				}
				else {
					// add new bar
					glm::vec2 newBarPoint(convertRealTimeToDisplayTime(lastAlignedTime), lastTotalVolume);
					_barChart->addPoint(newBarPoint);
				}
			}
			else {
				// add new bar
				glm::vec2 newBarPoint(convertRealTimeToDisplayTime(lastAlignedTime), lastTotalVolume);
				_barChart->addPoint(newBarPoint);
			}
			_barChart->adjustTransform();
		});

		_graph->acessSharedData([this, candleItems, sender](Widget*) {
			if (candleItems) {
				auto& points = _graph->getPoints();
				auto x = convertRealTimeToDisplayTime(candleItems->timestamp);
				auto y = (candleItems->high + candleItems->low) / 2;
				if (points.size()) {
					if (points.back().y != y) {
						_graph->addPointAndConstruct(glm::vec2(x, y));
						_graph->adjustTransform();
					}
				}
				else {
					_graph->addPoint(glm::vec2(x, y));
					_graph->adjustTransform();
				}
			}
		});
	}
}

void BasicApp::initBarchart(const std::list<CandleItem>& candles) {
	constexpr int minLength = 15;
	TIMESTAMP barTimeLength = minLength * 60 * 1000;
	int barCount = 24 * 60 / minLength;
	float barWith = _barChart->getGraphRegion().getWidth() / barCount;
	_pixelPerTime = (float)(barWith / barTimeLength);

	float alignX;

	_barChart->acessSharedData([this, &candles, barTimeLength, barCount, &alignX](Widget*) {
		_barChart->clearPoints();
		_barChart->setInitalGraphRegion(_barChart->getGraphRegion());

		auto it = candles.rbegin();

		TIMESTAMP beginTime = it->timestamp;
		TIMESTAMP alignTime = roundUp(beginTime, barTimeLength);

		// import the first n - 1 points
		for (; it != candles.rend(); it++) {
			if (it->timestamp >= alignTime) {
				break;
			}
		}
		float volume = 0;
		for (; it != candles.rend(); it++) {
			if ((it->timestamp - alignTime) < barTimeLength) {
				volume += it->volume;
			}
			else {
				float x = convertRealTimeToDisplayTime(it->timestamp);
				_barChart->addPoint(glm::vec2(x, volume));
				alignTime = it->timestamp;
				volume = it->volume;
			}
		}

		// align current time
		alignTime = roundUp(getCurrentTimeStamp(), barTimeLength);

		// begin time of graph should be
		beginTime = alignTime - barCount * barTimeLength;
		alignX = convertRealTimeToDisplayTime(beginTime);

		// begin time should be display at begin of the graph
		_barChart->translate(-alignX, 0);
		_barChart->adjustTransform();
	});

	_graph->acessSharedData([this, &candles, &alignX](Widget* sender) {
		WxLineGraphLive* graph = (WxLineGraphLive*)sender;

		// reset the graph
		_graph->clearPoints();
		auto& area = _graph->getGraphRegion();
		_graph->setInitalGraphRegion(area);

		for (auto it = candles.rbegin(); it != candles.rend(); it++) {
			float x = convertRealTimeToDisplayTime(it->timestamp);
			_graph->addPointAndConstruct(glm::vec2(x, (it->high + it->low)/2));
		}
		auto& points = _graph->getPoints();
		if (points.size()) {
			_graph->scale(1.0f, (float)(area.getHeight() / (points.back().y / 200)));
		}

		_graph->translate(-alignX, 0);
		_graph->adjustTransform();
	});
}

float BasicApp::convertRealTimeToDisplayTime(TIMESTAMP t) {
	return (float)((t - _baseTime) *_pixelPerTime);
}

TIMESTAMP BasicApp::convertXToTime(float x) {
	return _baseTime + (TIMESTAMP)(((double)x) / _pixelPerTime);
}

void BasicApp::mouseDown( MouseEvent event )
{
}

void BasicApp::update()
{
	_spliter.update();
	if (_ciWndCandle) {
		_ciWndCandle->update();
	}
	TIMESTAMP liveTime = 0;
	//if (_platformRunner) {
	//	if (_platformRunner->getPlatform()->isServerTimeReady()) {
	//		liveTime = _platformRunner->getPlatform()->getSyncTime(getCurrentTimeStamp());
	//	}
	//}
	if (liveTime == 0) {
		liveTime = getCurrentTimeStamp();
	}
	auto liveX = convertRealTimeToDisplayTime(liveTime);
	_barChart->setLiveX(liveX);
	_graph->setLiveX(liveX);
}

void BasicApp::draw()
{
	auto wndCandle = getWindow()->getUserData<CiWndCandle>();
	if (wndCandle) {
		wndCandle->draw();
	}
	else {
		gl::clear(ColorA::black());
		_spliter.draw();
	}
}

CINDER_APP(BasicApp, RendererGl( RendererGl::Options().msaa( 8 ) ), BasicApp::intializeApp)
