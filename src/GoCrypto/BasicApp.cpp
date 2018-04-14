#ifdef WIN32
#include <Windows.h>
#include <filesystem>
using namespace std::experimental;
#else
#include <unistd.h>
#include <filesystem>
using namespace std;
#endif

#include <stdio.h>
#include <io.h>
#include <fcntl.h>

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
#include <cpprest/json.h>

using namespace ci;
using namespace ci::app;
using namespace std;

const char* enableGUIComand = "show gui";

class BasicApp : public App {

	typedef std::function<void()> Task;

	Spliter _spliter;
	shared_ptr<Panel> _panel;
	Widget* _topCotrol;
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
	int _barTimeLength = 15 * 60 * 1000;
	bool _liveMode = true;

	mutex _serviceControlMutex;
	mutex _chartMutex;

	thread _consoleThread;
	bool _isConsoleThreadRunning = false;
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

	void addLog(LogLevel logLevel, const char* fmt, va_list args) {
		if (_applog) {
			_applog->addLogV( (WxAppLog::LogLevel) logLevel, fmt, args);
		}
	}

	void onSelectedTradeEvent(NAPMarketEventHandler*, TradeItem* tradeItem, int, bool);
	void onCandle(NAPMarketEventHandler* sender, CandleItem* candleItems, int count, bool snapshot);

	float convertRealTimeToDisplayTime(TIMESTAMP t);
	TIMESTAMP convertXToTime(float x);

	static void intializeApp(App::Settings* settings);

	void createCandleWindow();

	void onSelectedSymbolChanged(Widget*);

	void initBarchart(const std::list<CandleItem>& candles);
	void initBarchart(const std::list<TradeItem>& items);
	void onWindowSizeChanged();
	bool isGraphDataCandle();
	void initChart();
	TIMESTAMP computeBarTimeLength();
	void restoreTransformAfterInitOnly(function<void()>&& initFunction);
	TIMESTAMP getLiveTime();
	void enalbeServerMode(bool serverMode);
	void initializeConsole();
	void uninitializeConsole();
};

void pushLog(int logLevel, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	((BasicApp*)app::App::get())->addLog((LogLevel)logLevel, fmt, args);
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
	_lastSelectedHandler(nullptr), _pixelPerTime(1000) {
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

void BasicApp::onWindowSizeChanged() {
	// update root control size
	int w = getWindow()->getWidth();
	int h = getWindow()->getHeight();
	_topCotrol->setSize((float)w, (float)h);
	_topCotrol->setPos(0, 0);
	if (_topCotrol == _applog.get()) {
		return;
	}

	std::unique_lock<std::mutex> lk(_chartMutex);
	// back up transform
	static TIMESTAMP tAtZero;
	if (_liveMode == false) {
		auto xAtZero = _graph->localToPoint(0, 0).x;
		tAtZero = convertXToTime(xAtZero);
	}

	int graphX1 = 20;
	int graphX2 = (int)_barChart->getWidth() - 20;
	_graph->setInitalGraphRegion(Area(graphX1, 20, graphX2, (int)_barChart->getHeight() - 20));
	_barChart->setInitalGraphRegion(Area(graphX1, (int)_barChart->getHeight() / 2 - 10, graphX2, (int)_barChart->getHeight() - 20));

	auto& area = _graph->getGraphRegion();
	_graph->setAutoScaleRange((float)area.y1, area.y1 + area.getHeight() / 2.0f);

	if (_lastSelectedHandler) {
		initChart();
		if (_liveMode == false) {
			auto xAtZero = convertRealTimeToDisplayTime(tAtZero);
			auto newPointOfX = _graph->pointToLocal(xAtZero, 0);

			_barChart->setLiveX(_graph->getLiveX());

			_graph->translate(-newPointOfX.x, 0);
			auto t = _graph->getTranslate();
			t.y = _barChart->getTranslate().y;
			_barChart->setTranslate(t);

			_graph->adjustTransform();
			_barChart->adjustTransform();
		}
	}
	else {
		_barChart->adjustTransform();
		_graph->adjustTransform();
	}
}

bool BasicApp::isGraphDataCandle() {
	return _controlBoard->getCurrentGraphLengh() >= 12 * 3600;
}

std::string getExecutableAbsolutePath() {
	char buff[256];
#ifdef WIN32
	GetModuleFileNameA(nullptr, buff, sizeof(buff));
#else
	readlink("/proc/self/exe", buff, sizeof(buff));
#endif // WIN32
	return buff;
}

vector<string> listPlatformConfigs() {
	filesystem::path path = getExecutableAbsolutePath();
	path = path.parent_path();
	path /= "platforms";

	vector<string> configs;
	for (auto & p : filesystem::directory_iterator(path)) {
		if (p.path().filename().extension() == ".json") {
			auto configPath = p.path().u8string();
			ifstream in;
			in.open(configPath);

			auto fileName = p.path().filename().u8string();
			auto platformName = fileName.substr(0, fileName.size() - 5);

			if (in.is_open()) {
				try {
					web::json::value::parse(in);
					configs.emplace_back(platformName);
				}
				catch (std::exception&e) {
					pushLog((int)LogLevel::Error, "file %s is not a valid config file. error = %s", configPath.c_str(), e.what());
				}
				catch(...) {}
				in.close();
			}
			else {
				pushLog((int)LogLevel::Error, "file %s is unable to read", configPath.c_str());
			}
		}
	}

	return configs;
}

void setupConsole() {
	//AllocConsole();

	//HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
	//int hCrt = _open_osfhandle((intptr_t)handle_out, _O_TEXT);
	//FILE* hf_out = _fdopen(hCrt, "w");
	//setvbuf(hf_out, NULL, _IONBF, 1);
	//*stdout = *hf_out;

	//HANDLE handle_in = GetStdHandle(STD_INPUT_HANDLE);
	//hCrt = _open_osfhandle((intptr_t)handle_in, _O_TEXT);
	//FILE* hf_in = _fdopen(hCrt, "r");
	//setvbuf(hf_in, NULL, _IONBF, 128);
	//*stdin = *hf_in;

	AllocConsole();
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
}

void BasicApp::initializeConsole() {
	::setupConsole();

	// read input command from console
	_consoleThread = std::thread([this]() {
		std::string command;
		while (true)
		{
			std::getline(cin, command);
			std::transform(command.begin(), command.end(), command.begin(), ::tolower);
			
			if (command == enableGUIComand) {
				enalbeServerMode(false);
			}
			else {
				cout << "unnkown command '" << command << "'" << endl;
			}
		}
		_isConsoleThreadRunning = false;
	});
	_isConsoleThreadRunning = true;
}

void BasicApp::uninitializeConsole() {
	if (_consoleThread.joinable()) {
		if (_isConsoleThreadRunning) {
#ifdef WIN32
			HANDLE hThread = (HANDLE)_consoleThread.native_handle();
			::TerminateThread(hThread, 0);
#endif // WIN32
		}
		_consoleThread.join();
	}
}

void BasicApp::enalbeServerMode(bool serverMode) {
#ifdef WIN32
	ShowWindow(GetConsoleWindow(), serverMode ? SW_SHOW : SW_HIDE);
#else
	// should implement for other OS
#endif // WIN32

	if (serverMode == true) {
		getWindow()->hide();
		pushLog((int)LogLevel::Info, "server mode is enable, type '%s' to show GUI\n", enableGUIComand);
	}
	else {
		getWindow()->show();
		pushLog((int)LogLevel::Info, "gui mode is enable, double click in log window to enable server mode\n");
	}
}

void BasicApp::setup()
{
	FUNCTON_LOG();
	using namespace std::placeholders;
	ui::Options uiOptions;
	ui::initialize(uiOptions);

	getWindow()->setTitle("Go Crypto");
	getWindow()->getSignalResize().connect(std::bind(&BasicApp::onWindowSizeChanged, this));

	int w = getWindow()->getWidth();
	int h = getWindow()->getHeight();
	_spliter.setSize((float)w, (float)h);
	_spliter.setPos(0, 0);
	_topCotrol = &_spliter;

	_applog = std::make_shared<WxAppLog>();

	auto platforms = listPlatformConfigs();

	_cryptoBoard = std::make_shared<WxCryptoBoardInfo>();
	auto bottomSpliter = std::make_shared<Spliter>();
	auto topSpliter = std::make_shared<Spliter>();
	
	auto graphLine = std::make_shared<WxLineGraphLive>();
	_controlBoard = std::make_shared<WxControlBoard>(platforms);
	_barChart = std::make_shared<WxBarCharLive>();
	_panel = std::make_shared<Panel>();
	_panel->addChild(_barChart);
	_panel->addChild(graphLine);

	_logAdapter = new LogAdapter(_applog.get());

	topSpliter->setVertical(true);
	topSpliter->setFixedPanelSize(130);
	topSpliter->setFixPanel(FixedPanel::Panel2);
	topSpliter->setChild1(_cryptoBoard);
	topSpliter->setChild2(_controlBoard);

	bottomSpliter->setVertical(true);
	bottomSpliter->setFixedPanelSize(800);
	bottomSpliter->setFixPanel(FixedPanel::Panel1);
	bottomSpliter->setChild1(_panel);
	bottomSpliter->setChild2(_applog);

	_spliter.setFixedPanelSize(300);
	_spliter.setFixPanel(FixedPanel::Panel2);
	_spliter.setChild1(topSpliter);
	_spliter.setChild2(bottomSpliter);

	graphLine->setGraphRegionColor(ColorA8u(0, 0, 0, 255));
	graphLine->setLineColor(ColorA8u(165, 42, 42, 255));
	_barChart->setGraphRegionColor(ColorA8u(0, 0, 0, 255));
	_barChart->setLineColor(ColorA8u(0, 0, 255, 255));
	_barChart->setBarColor(ColorA8u(69, 69, 69, 255));

	graphLine->setIndicateAligment(HorizontalIndicatorAlignment::Left, VerticalIndicatorAlignment::Bottom);
	_barChart->setIndicateAligment(HorizontalIndicatorAlignment::Right, VerticalIndicatorAlignment::None);

	graphLine->drawBackground(false);

	getWindow()->getSignalClose().connect([this]() {
		_runFlag = false;
		uninitializeConsole();
	});

	// handle mouse up and mouse down to emit mouse double click
	static double timeDown;
	static double* pTimeDown = nullptr;
	static int clickCount = 0;
	static glm::ivec2 prePos;
	static bool dragging = false;

	getWindow()->getSignalMouseDown().connect([this](MouseEvent& me) {
		auto& pos = me.getPos();
		prePos = pos;
		if (pos.x >= _graph->getX() && pos.x < _graph->getX() + _graph->getWidth() &&
			pos.y >= _graph->getY() && pos.y < _graph->getY() + _graph->getHeight()) {
			clickCount++;
			if (pTimeDown == nullptr) {
				pTimeDown = &timeDown;
				timeDown = getElapsedSeconds();
			}
			else {
				auto timeDown2 = getElapsedSeconds();
				auto doubleClickTime = Utility::getDoubleClickTime();
				auto clickTime = (unsigned int)(timeDown2 - timeDown) * 1000;
				if (clickTime > doubleClickTime) {
					clickCount = 1;
					timeDown = timeDown2;
				}
			}
		}
		else {
			clickCount = 0;
			pTimeDown = nullptr;
		}
	});
	getWindow()->getSignalMouseUp().connect([this](MouseEvent& me) {
		auto& pos = me.getPos();
		auto timeUp = getElapsedSeconds();
		auto doubleClickTime = Utility::getDoubleClickTime();
		auto clickTime = (unsigned int)(timeUp - timeDown) * 1000;
		if (clickCount == 2 && pTimeDown && std::abs(prePos.x - pos.x) <= 3 && std::abs(prePos.y - pos.y) <= 3) {
			clickCount = 0;
			pTimeDown = nullptr;
			if (clickTime <= doubleClickTime) {
				if (_topCotrol == &_spliter) {
					_topCotrol = _panel.get();	
				}
				else {
					_topCotrol = &_spliter;
				}
				// reset chats by using method onWindowSizeChanged
				onWindowSizeChanged();
			}
		}
		if (clickTime > doubleClickTime) {
			clickCount = 0;
			pTimeDown = nullptr;
		}
		if (dragging) {
			std::unique_lock<std::mutex> lk(_chartMutex);
			dragging = false;
			_graph->adjustTransform();
			_barChart->adjustTransform();
			auto lastX = _graph->getLiveX();
			auto pointX = _graph->pointToLocal(lastX, 0);
			if (pointX.x <= _graph->getGraphRegion().x2) {
				_liveMode = true;
			}
		}
	});

	_graph = graphLine;

	auto translateTime = [this](float x) -> std::string {
		TIMESTAMP t = convertXToTime(x);
		char buffer[32];
		time_t tst = (time_t)(t / 1000);
		struct tm * timeinfo;
		timeinfo = localtime(&tst);
		if (timeinfo == nullptr) {
			buffer[0] = 0;
		}
		else {
			strftime(buffer, sizeof(buffer), "%b %d %T", timeinfo);
		}
		return buffer;
	};

	auto fPriceTranslate = [this, translateTime](const glm::vec2& point) -> std::tuple<std::string, std::string> {
		string xStr = translateTime(point.x);
		auto adapter = dynamic_cast<ConvertableCryptoInfoAdapter*>(_cryptoBoard->getAdapter().get());
		auto selectedSymbol = _cryptoBoard->getSelectedSymbol();

		string yStr;
		double price;
		if (adapter && selectedSymbol) {
			if (adapter->convertPrice(selectedSymbol, (double)point.y, price)) {
				yStr = std::to_string((float)price);
			}
			else {
				yStr = "N/A-" + std::to_string(point.y);
			}
		}
		else { 
			yStr = std::to_string(point.y);
		}

		std::tuple<std::string, std::string> pointStr(xStr, yStr);
		return pointStr;
	};

	auto fVolumeTranslate = [this, translateTime](const glm::vec2& point) -> std::tuple<std::string, std::string> {
		string xStr = translateTime(point.x);
		string yStr;

		auto adapter = dynamic_cast<ConvertableCryptoInfoAdapter*>(_cryptoBoard->getAdapter().get());
		auto items = _cryptoBoard->getItems();
		auto selectedItemIdx = _cryptoBoard->getSelectedSymbolIndex();

		if (adapter && items && selectedItemIdx >= 0 && items->size() > 0) {
			auto& originCrytoInfo = items->at(selectedItemIdx);
			auto selectedSymbol = originCrytoInfo->symbol;
			double price;
			if (adapter->convertPrice(selectedSymbol, originCrytoInfo->price, price)) {
				auto& quoteCurency = adapter->getQuote(selectedSymbol);
				if (quoteCurency.empty()) {
					yStr = "N/A-" + std::to_string(point.y);
				}
				else {
					yStr = std::to_string((float)(point.y * price));
				}
			}
			else {
				yStr = "N/A-" + std::to_string(point.y);
			}
		}
		else {
			yStr = std::to_string(point.y);
		}

		std::tuple<std::string, std::string> pointStr(xStr, yStr);
		return pointStr;
	};

	_graph->setPointToTextTranslateFunction(fPriceTranslate);
	_barChart->setPointToTextTranslateFunction(fVolumeTranslate);

	getWindow()->getSignalMouseMove().connect([this](MouseEvent& me) {
		_graph->setCursorLocation(glm::vec2(me.getX(), me.getY()));
		_barChart->setCursorLocation(glm::vec2(me.getX(), me.getY()));
	});

	getWindow()->getSignalMouseDrag().connect([this](MouseEvent& me) {
		std::unique_lock<std::mutex> lk(_chartMutex);
		auto & pos = me.getPos();
		if (pos.x >= _graph->getX() && pos.x < _graph->getX() + _graph->getWidth() &&
			pos.y >= _graph->getY() && pos.y < _graph->getY() + _graph->getHeight()) {

			auto translateX = me.getX() - prePos.x;
			_graph->translate((float)translateX, 0);
			auto t = _graph->getTranslate();
			t.y = _barChart->getTranslate().y;
			_barChart->setTranslate(t);
			_liveMode = false;
			prePos = me.getPos();
			_graph->setCursorLocation(prePos);
			_barChart->setCursorLocation(prePos);

			dragging = true;
		}
	});

	_baseTime = getCurrentTimeStamp();
	_cryptoBoard->setItemSelectionChangedHandler( std::bind(&BasicApp::onSelectedSymbolChanged, this, _1) );

	getWindow()->getSignalKeyDown().connect([this](KeyEvent& event) {
		if (event.getCode() == KeyEvent::KEY_ESCAPE) {
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

	_controlBoard->setOnNotificationModeChangedHandler([this](Widget*) {
		Notifier::getInstance()->enablePushToCloud(_controlBoard->isPushToCloudEnable());
	});

	_controlBoard->setOnGraphLengthChangedHandler([this](Widget*) {
		std::unique_lock<std::mutex> lk(_chartMutex);
		initChart();
	});

	_applog->setDoubleClickHandler([this](Widget*) {
		// enable server mode
		//_topCotrol = _applog.get();
		//_applog->addLog(WxAppLog::LogLevel::Info, "Server mode is enable\n");
		enalbeServerMode(true);
		//onWindowSizeChanged();
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

	initializeConsole();
	enalbeServerMode(false);
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
	{
		std::unique_lock<std::mutex> lk(_chartMutex);
		initChart();
	}

	auto eventListener = std::bind(&BasicApp::onSelectedTradeEvent, this, _1, _2, _3, _4);
	auto candleEventListener = std::bind(&BasicApp::onCandle, this, _1, _2, _3, _4);
	_lastEventId = _lastSelectedHandler->addTradeEventListener(eventListener);
	_lastCandleEventId = _lastSelectedHandler->addCandleEventListener(candleEventListener);
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
	pushLog((int)LogLevel::Info, "starting services...\n");
	if (_platformRunner) {
		pushLog((int)LogLevel::Error, "services have been already started\n");
		return;
	}

	auto currentPlatform = _controlBoard->getCurrentPlatform();
	if (currentPlatform == nullptr) {
		pushLog((int)LogLevel::Error, "No available platform found\n");
		return;
	}


	filesystem::path configPath = getExecutableAbsolutePath();
	configPath = configPath.parent_path();
	configPath /= "platforms";
	configPath /= currentPlatform;
	auto configFile = configPath.u8string() + ".json";

	_liveMode = true;
	{
		std::unique_lock<std::mutex> lk(_serviceControlMutex);
		_platformRunner = new PlatformEngine(configFile.c_str());
	}
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

	pushLog((int)LogLevel::Info, "services have been started\n");
}

void BasicApp::stopServices() {
	LOG_SCOPE_ACCESS(_logAdapter, __FUNCTION__);
	pushLog((int)LogLevel::Info, "stopping services...\n");
	if (!_platformRunner) {
		pushLog((int)LogLevel::Error, "services are not running\n");
		return;
	}
	_lastSelectedHandler = nullptr;

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
	{
		std::unique_lock<std::mutex> lk(_chartMutex);
		_graph->clearPoints();
		_barChart->clearPoints();
	}

	{
		std::unique_lock<std::mutex> lk(_serviceControlMutex);
		delete _platformRunner;
		_platformRunner = nullptr;
	}
	pushLog((int)LogLevel::Info, "services have been stopped\n");
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


//////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T, class FVolume, class FPrice>
void initBarChart(WxBarCharLive* barChart, WxLineGraphLive* lineChart,
	const std::list<T>& items, TIMESTAMP barTimeLength, TIMESTAMP graphLength,
	const FVolume& fVolume, const FPrice& fPrice) {

	float alignX;
	auto app = (BasicApp*)App::get();

	barChart->acessSharedData([barChart, app, &items, graphLength, barTimeLength, &alignX, &fVolume](Widget*) {
		barChart->clearPoints();
		barChart->setInitalGraphRegion(barChart->getGraphRegion());
		if (items.size()) {
			auto it = items.rbegin();

			TIMESTAMP beginTime = it->timestamp;
			TIMESTAMP alignTime = roundUp(beginTime, barTimeLength);

			// import the first n - 1 points
			for (; it != items.rend(); it++) {
				if (it->timestamp >= alignTime) {
					break;
				}
			}
			float volume = 0;
			for (; it != items.rend(); it++) {
				if ((it->timestamp - alignTime) < barTimeLength) {
					volume += fVolume(*it);
				}
				else {
					alignTime = roundDown(it->timestamp, barTimeLength);
					float x = app->convertRealTimeToDisplayTime(alignTime);
					barChart->addPoint(glm::vec2(x, volume));
					volume = fVolume(*it);
				}
			}

			// align current time
			alignTime = roundUp(getCurrentTimeStamp(), barTimeLength);

			// begin time of graph should be
			beginTime = alignTime - graphLength;
			alignX = app->convertRealTimeToDisplayTime(beginTime);

			// begin time should be display at begin of the graph
			barChart->translate(-alignX, 0);
			barChart->adjustTransform();
		}
	});

	lineChart->acessSharedData([app, lineChart, &items, &alignX, &fPrice](Widget*) {
		// reset the graph
		lineChart->clearPoints();
		auto& area = lineChart->getGraphRegion();
		lineChart->setInitalGraphRegion(area);
		lineChart->setAutoScaleRange((float)area.y1, area.y1 + area.getHeight() / 2.0f);
		if (items.size()) {
			float y;
			for (auto it = items.rbegin(); it != items.rend(); it++) {
				float x = app->convertRealTimeToDisplayTime(it->timestamp);
				y = fPrice(*it);
				lineChart->addPointAndConstruct(glm::vec2(x, y));
			}
			// align x for line chart to same as bar chart
			lineChart->translate(-alignX, 0);
			lineChart->adjustTransform();
		}
	});

	//lineChart->acessSharedData([app, lineChart, &items, &alignX, &fVolume](Widget*) {
	//	// reset the graph
	//	lineChart->clearPoints();
	//	auto& area = lineChart->getGraphRegion();
	//	lineChart->setInitalGraphRegion(area);
	//	lineChart->setAutoScaleRange((float)area.y1, area.y1 + area.getHeight() / 2.0f);

	//	buildVolumeLineChartForSnapshot(lineChart, items, fVolume);

	//	// align x for line chart to same as bar chart
	//	lineChart->translate(-alignX, 0);
	//	lineChart->adjustTransform();
	//});
}

template <class T, class FVolume>
void buildVolumeLineChartForSnapshot(WxLineGraphLive* lineChart, const std::list<T>& items, const FVolume& fVolume) {
	auto app = (BasicApp*)App::get();

	TIMESTAMP duration = 60 * 60 * 1000;
	float initializeVolume = 0;

	TIMESTAMP liveTime = app->getLiveTime();
	TIMESTAMP timeEnd = liveTime - duration;

	auto it = items.begin();
	auto jt = it;
	for (; jt != items.end(); jt++) {
		if (jt->timestamp <= timeEnd) {
			break;
		}
		initializeVolume += fVolume(*jt);
	}
	// application must have atleast one hour of data to draw this chart
	if (jt->timestamp > timeEnd || jt == items.end()) {
		return;
	}

	list<vec2> points;
	float x = app->convertRealTimeToDisplayTime(it->timestamp);
	float y = initializeVolume;
	points.push_back(glm::vec2(x, y));

	initializeVolume -= fVolume(*it);
	if (initializeVolume < 0) {
		initializeVolume = 0;
	}
	it++;
	timeEnd = it->timestamp - duration;

	for (jt++; jt != items.end();) {
		if (jt->timestamp > timeEnd) {
			initializeVolume += fVolume(*jt);
			jt++;
		}
		else {
			x = app->convertRealTimeToDisplayTime(it->timestamp);
			y = initializeVolume;
			points.push_back(glm::vec2(x, y));

			initializeVolume -= fVolume(*it);
			if (initializeVolume < 0) {
				initializeVolume = 0;
			}
			it++;
			timeEnd = it->timestamp - duration;
		}
	}

	for (auto pt = points.rbegin(); pt != points.rend(); pt++) {
		lineChart->addPointAndConstruct(*pt);
	}
}

template <class T, class FVolume, class FPrice>
void renewCharts(WxBarCharLive* barChart, WxLineGraphLive* lineChart, const std::list<T>& items, const FVolume& fVolume, const FPrice& fPrice) {
	auto app = (BasicApp*)App::get();
	auto barTimeLength = app->computeBarTimeLength();

	barChart->acessSharedData([barChart, app, &items, barTimeLength, &fVolume](Widget*) {
		barChart->clearPoints();

		auto it = items.rbegin();

		TIMESTAMP beginTime = it->timestamp;
		TIMESTAMP alignTime = roundUp(beginTime, barTimeLength);

		// import the first n - 1 points
		for (; it != items.rend(); it++) {
			if (it->timestamp >= alignTime) {
				break;
			}
		}
		float volume = 0;
		for (; it != items.rend(); it++) {
			if ((it->timestamp - alignTime) < barTimeLength) {
				volume += fVolume(*it);
			}
			else {
				alignTime = roundDown(it->timestamp, barTimeLength);
				float x = app->convertRealTimeToDisplayTime(alignTime);
				barChart->addPoint(glm::vec2(x, volume));
				volume = fVolume(*it);
			}
		}
		barChart->adjustTransform();
	});

	lineChart->acessSharedData([app, lineChart, &items, &fPrice](Widget*) {
		// reset the graph
		lineChart->clearPoints();

		float y;
		for (auto it = items.rbegin(); it != items.rend(); it++) {
			float x = app->convertRealTimeToDisplayTime(it->timestamp);
			y = fPrice(*it);
			lineChart->addPointAndConstruct(glm::vec2(x, y));
		}
		lineChart->adjustTransform();
	});

	//lineChart->acessSharedData([app, lineChart, &items, &fVolume](Widget*) {
	//	// reset the graph
	//	lineChart->clearPoints();
	//	buildVolumeLineChartForSnapshot(lineChart, items, fVolume);
	//	lineChart->adjustTransform();
	//});
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////

void BasicApp::onSelectedTradeEvent(NAPMarketEventHandler* handler, TradeItem* items, int count, bool snapShot) {
	if (isGraphDataCandle()) {
		return;
	}

	if(snapShot) {
		std::unique_lock<std::mutex> lk(_chartMutex);
		auto& items = handler->getTradeHistoriesNonSync();
		renewCharts(_barChart.get(), _graph.get(), items,
			[](const TradeItem& item) { return (float)abs(item.amount); },
			[](const TradeItem& item) { return (float)item.price; });
	}
	else {
		std::unique_lock<std::mutex> lk(_chartMutex);
		{
			TIMESTAMP barTimeLength = _barTimeLength;
			auto lastAlignedTime = roundDown(items->timestamp, barTimeLength);

			auto& points = _barChart->getPoints();
			if (points.size()) {
				auto& lastPoint = points.back();
				auto lastTimeInChart = convertXToTime(lastPoint.x);
				auto lastAnlignedTimeInChart = roundDown(lastTimeInChart, barTimeLength);

				if (lastAlignedTime == lastAnlignedTimeInChart) {
					// update last bar
					lastPoint.y += (float)abs(items->amount);
				}
				else if (lastAlignedTime > lastAnlignedTimeInChart) {
					// add new bar
					glm::vec2 newBarPoint(convertRealTimeToDisplayTime(lastAlignedTime), abs(items->amount));
					_barChart->addPoint(newBarPoint);
				}
				else {
					// some trade event item come to client late
					// that is not lastest event
					// so, we need to find the item which the event belong to
					for (auto it = points.begin(); it != points.end(); it++) {
						lastTimeInChart = convertXToTime(it->x);
						lastAnlignedTimeInChart = roundDown(lastTimeInChart, barTimeLength);
						if (lastAlignedTime == lastAnlignedTimeInChart) {
							// update last bar
							it->y += (float)abs(items->amount);
						}
					}
				}
			}
			else {
				// add new bar
				glm::vec2 newBarPoint(convertRealTimeToDisplayTime(lastAlignedTime), abs(items->amount));
				_barChart->addPoint(newBarPoint);
			}
			_barChart->adjustTransform();
		}

		if (items) {
			auto& points = _graph->getPoints();
			auto x = convertRealTimeToDisplayTime(items->timestamp);
			auto y = items->price;
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
	}
}

void BasicApp::restoreTransformAfterInitOnly(function<void()>&& initFunction) {
	std::unique_lock<std::mutex> lk(_chartMutex);
	static float xAtZero;
	if (_liveMode == false) {
		xAtZero = _graph->localToPoint(0, 0).x;
	}

	initFunction();

	if (_liveMode == false) {
		auto newPointOfX = _graph->pointToLocal(xAtZero, 0);
		_graph->translate(-newPointOfX.x, 0);
		auto t = _graph->getTranslate();
		t.y = _barChart->getTranslate().y;
		_barChart->setTranslate(t);
		_graph->adjustTransform();
		_barChart->adjustTransform();
	}
}

void BasicApp::onCandle(NAPMarketEventHandler* sender, CandleItem* candleItems, int count, bool snapshot) {
	if (isGraphDataCandle() == false) {
		return;
	}
	if (snapshot) {
		std::unique_lock<std::mutex> lk(_chartMutex);
		auto& candles = sender->getCandleHistoriesNonSync();
		renewCharts(_barChart.get(), _graph.get(), candles,
			[](const CandleItem& item) { return (float)item.volume; },
			[](const CandleItem& item) { return (float)((item.low + item.high) / 2); });
	}
	else {
		std::unique_lock<std::mutex> lk(_chartMutex);
		{
			TIMESTAMP barTimeLength = _barTimeLength;
			auto lastAlignedTime = roundDown(candleItems->timestamp, barTimeLength);
			auto& candles = sender->getCandleHistoriesNonSync();
			if (candles.size()) {
				// only calculate last hour
				float lastTotalVolume = 0;
				for (auto it = candles.begin(); it != candles.end() && it->timestamp >= lastAlignedTime; it++) {
					lastTotalVolume += (float)it->volume;
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
			}
		}

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
	}
}

void BasicApp::initChart() {
	if (_lastSelectedHandler == nullptr) return;
	_lastSelectedHandler->accessSharedData([this](NAPMarketEventHandler* handler) {
		if (isGraphDataCandle()) {
			auto& candles = handler->getCandleHistoriesNonSync();
			initBarchart(candles);
		}
		else {
			auto& items = handler->getTradeHistoriesNonSync();
			initBarchart(items);
		}
	});
}

TIMESTAMP BasicApp::computeBarTimeLength() {
	auto graphLength = _controlBoard->getCurrentGraphLengh();
	int barCount = _controlBoard->getCurrentBarCount();

	graphLength *= 1000;
	_pixelPerTime = _barChart->getGraphRegion().getWidth() * 1.0f / graphLength;
	float barWith = (float)_barChart->getGraphRegion().getWidth() / barCount;
	_barChart->setBarWidth(barWith);

	return (TIMESTAMP)(graphLength / barCount);
}


void BasicApp::initBarchart(const std::list<CandleItem>& candles) {
	_barTimeLength = (int)computeBarTimeLength();
	auto graphLength = _controlBoard->getCurrentGraphLengh() * 1000;
	//_liveMode = true;

	initBarChart(_barChart.get(), _graph.get(), candles, _barTimeLength, graphLength,
		[](const CandleItem& item) { return (float)item.volume; },
		[](const CandleItem& item) { return (float)((item.low + item.high) / 2); });
}

void BasicApp::initBarchart(const std::list<TradeItem>& items) {
	_barTimeLength = (int)computeBarTimeLength();
	auto graphLength = _controlBoard->getCurrentGraphLengh() * 1000;
	//_liveMode = true;

	initBarChart(_barChart.get(), _graph.get(), items, _barTimeLength, graphLength,
		[](const TradeItem& item) { return (float)abs(item.amount); },
		[](const TradeItem& item) { return (float)item.price; });
}

float BasicApp::convertRealTimeToDisplayTime(TIMESTAMP t) {
	return (float)((t - _baseTime) *_pixelPerTime);
}

TIMESTAMP BasicApp::convertXToTime(float x) {
	return _baseTime + (TIMESTAMP)( x * 1.0 / _pixelPerTime + 0.5);
}

void BasicApp::mouseDown( MouseEvent event )
{
}

TIMESTAMP BasicApp::getLiveTime() {
	TIMESTAMP liveTime = 0;
	if (_serviceControlMutex.try_lock()) {
		if (_platformRunner) {
			if (_platformRunner->getPlatform()->isServerTimeReady()) {
				liveTime = _platformRunner->getPlatform()->getSyncTime(getCurrentTimeStamp());
			}
		}
		_serviceControlMutex.unlock();
	}
	if (liveTime == 0) {
		liveTime = getCurrentTimeStamp();
	}

	return liveTime;
}

void BasicApp::update()
{
	FUNCTON_LOG();
	if (getWindow()->isHidden()) return;

	_topCotrol->update();
	if (_ciWndCandle) {
		_ciWndCandle->update();
	}
	if (_liveMode) {
		TIMESTAMP liveTime = getLiveTime();
		{
			std::unique_lock<std::mutex> lk(_chartMutex);
			auto liveX = convertRealTimeToDisplayTime(liveTime);
			_barChart->setLiveX(liveX);
			_graph->setLiveX(liveX);

			auto t = _graph->getTranslate();
			t.y = _barChart->getTranslate().y;
			_barChart->setTranslate(t);
		}
	}
}

void BasicApp::draw()
{
	FUNCTON_LOG();
	if (getWindow()->isHidden()) return;

	std::unique_lock<std::mutex> lk(_chartMutex);
	//auto wndCandle = getWindow()->getUserData<CiWndCandle>();
	//if (wndCandle) {
	//	wndCandle->draw();
	//}
	//else {
		gl::clear(ColorA::black());
		_topCotrol->draw();

		if (_barChart) {
			_barChart->drawPointAtCursor();
		}
		if (_graph) {
			_graph->drawPointAtCursor();
		}
	//}
}

CINDER_APP(BasicApp, RendererGl( RendererGl::Options().msaa( 8 ) ), BasicApp::intializeApp)
