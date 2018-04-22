#pragma once
#include "ImWidget.h"

class ImPopup : public ImWidget
{
private:
	bool _isOpen;
	std::string _popUpId;
	static ImGuiID _autoIdGenerator;
protected:
	virtual void updateContent() = 0;
public:
	ImPopup(const std::string& name);
	~ImPopup();
	virtual void update();
	virtual void showWindow(bool show);
};