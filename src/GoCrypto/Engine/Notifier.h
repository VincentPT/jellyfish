#pragma once
#include <string>

struct UserListenerInfo {
	std::string userKey;
	std::string applicationKey;
};

struct Notification
{
	std::string title;
	std::string message;
	UserListenerInfo* target;
};

class NotifierImpl;

class Notifier
{
protected:
	Notifier();
public:
	static Notifier* getInstance();
	~Notifier();
	bool pushNotification(const Notification& notification);
};
