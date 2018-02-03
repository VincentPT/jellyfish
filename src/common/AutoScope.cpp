#include "AutoScope.h"

AutoScope::AutoScope(std::function<void(bool)>&& scopeAccess) : _scopeAccess(scopeAccess)
{
	_scopeAccess(true);
}

AutoScope::~AutoScope()
{
	_scopeAccess(false);
}
