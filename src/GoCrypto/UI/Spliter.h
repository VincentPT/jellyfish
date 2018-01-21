#pragma once
#include "CiWidget.h"
#include <memory>

enum class FixedPanel
{
	Auto = 0,
	Panel1,
	Panel2,
};


class Spliter : public CiWidget
{
protected:
	std::shared_ptr<Widget> _child1;
	std::shared_ptr<Widget> _child2;
	FixedPanel _fixedPanel;
	bool _horizontal;
	float _relativeSize;
	float _panelSize;

	void updateRelativeSize();
	void updateChildrenrGeometrics();
public:
	Spliter();
	virtual ~Spliter();

	void update();
	void draw();
	void setSize(float w, float h);
	void setPos(float x, float y);

	void setChild1(const std::shared_ptr<Widget>& child1);
	void setChild2(const std::shared_ptr<Widget>& child2);

	void setFixPanel(FixedPanel fixedPanel);
	void setVertical(bool);
	void setFixedPanelSize(float size);
};

