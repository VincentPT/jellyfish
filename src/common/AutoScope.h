#pragma once
#include <functional>

class AutoScope
{
	std::function<void(bool)> _scopeAccess;
public:
	AutoScope(std::function<void(bool)>&& scopeAccess);
	~AutoScope();
};

