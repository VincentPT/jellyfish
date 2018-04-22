#pragma once
#include "ImPopup.h"
#include "GoCrypto.h"

class WxPriceTriggers : public ImPopup
{
private:
	mutable std::mutex _mutex;
	std::vector<ColumnHeader> _columns;
	std::vector<TriggerPriceBase> _triggers;
	RowBuffer* _cellBuffer;
	int _selectedItemIdx = -1;
	std::string _errorString;
	float _firstShowErrorAt = 0;
	ButtonClickEventHandler _applyClickHandler;
protected:
	void updateContent();
public:
	WxPriceTriggers();
	~WxPriceTriggers();

	void setTriggers(const std::vector<TriggerPriceBase>& triggers);
	const std::vector<TriggerPriceBase>& getTriggers() const;
	void clearTriggers();
	int getSelectedItemIndex() const;

	void setOnApplyButtonClickHandler(ButtonClickEventHandler&& handler);
};