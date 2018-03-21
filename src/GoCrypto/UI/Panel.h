#pragma once
#include "CiWidget.h"
#include <memory>

class Panel : public CiWidget
{
protected:
	std::list<std::shared_ptr<Widget>> _children;

	void updateChildrenrGeometrics();
public:
	Panel();
	virtual ~Panel();

	void update();
	void draw();
	void setSize(float w, float h);
	void setPos(float x, float y);

	void addChild(const std::shared_ptr<Widget>& child);
	const std::list<std::shared_ptr<Widget>>& getChildren() const;
};

