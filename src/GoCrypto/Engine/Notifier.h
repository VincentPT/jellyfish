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
public:
	Notifier();
	~Notifier();
	bool pushNotification(const Notification& notification);
};
