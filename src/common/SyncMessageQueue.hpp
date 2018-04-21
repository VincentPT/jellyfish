#pragma once
#include <mutex>
#include <list>

template <class T>
class SyncMessageQueue
{
	std::list<T> _messageQueue;
	std::mutex _messageQueueMutex;
	std::condition_variable _hasMessageCV;
public:
	SyncMessageQueue() {}
	virtual ~SyncMessageQueue() {}

	void pushMessage(const T& message) {
		std::lock_guard<std::mutex> lk(_messageQueueMutex);
		_messageQueue.push_back(message);
		_hasMessageCV.notify_one();
	}

	void popMessage(T& message) {
		std::unique_lock<std::mutex> lk(_messageQueueMutex);
		_hasMessageCV.wait(lk, [this, &message] {
			if (_messageQueue.size() > 0) {
				message = _messageQueue.front();
				_messageQueue.pop_front();
				return true;
			}

			return false;
		});
	}

	bool popMessage(T& message, unsigned int waitTimeMiliSeconds) {
		std::unique_lock<std::mutex> lk(_messageQueueMutex);

		std::chrono::duration<decltype(waitTimeMiliSeconds), std::milli> timeout(waitTimeMiliSeconds);
		bool res = _hasMessageCV.wait_for(lk, timeout, [this, &message] {
			if (_messageQueue.size() > 0) {
				message = _messageQueue.front();
				_messageQueue.pop_front();
				return true;
			}

			return false;
		});

		return res;
	}

	void clear() {
		std::unique_lock<std::mutex> lk(_messageQueueMutex);
		_messageQueue.clear();
	}
};

template <class T>
class Signal
{
	T _signal;
	bool _notifyAll;
	bool _signalReceived;
	std::mutex _messageQueueMutex;
	std::condition_variable _hasMessageCV;
public:
	Signal(bool notifyToAll) : _notifyAll(notifyToAll), _signalReceived(false) {}
	virtual ~Signal() {}

	void resetState(const T& resetSignalVale) {
		std::lock_guard<std::mutex> lk(_messageQueueMutex);
		_signal = resetSignalVale;
		_signalReceived = false;
	}

	void sendSignal(const T& signal) {
		std::lock_guard<std::mutex> lk(_messageQueueMutex);
		_signal = signal;
		_signalReceived = true;
		if (_notifyAll) {
			_hasMessageCV.notify_all();
		}
		else {
			_hasMessageCV.notify_one();
		}
	}

	bool waitSignal(T& signal, unsigned int miliseconds) {
		std::unique_lock<std::mutex> lk(_messageQueueMutex);

		std::chrono::duration<decltype(miliseconds), std::milli> timeout(miliseconds);

		bool res = _hasMessageCV.wait_for(lk, timeout, [this] {
			if (_signalReceived) {
				if (!_notifyAll) _signalReceived = false;
				return true;
			}

			return false;
		});

		if (res) {
			signal = _signal;
		}

		return res;
	}

	void waitSignal(T& signal) {
		std::unique_lock<std::mutex> lk(_messageQueueMutex);
		_hasMessageCV.wait(lk, [this]() {
			if (_signalReceived) {
				if(!_notifyAll) _signalReceived = false;
				return true;
			}

			return false;
		});

		signal = _signal;
	}
};


