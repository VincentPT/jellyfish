#pragma once
#include "ImPopup.h"
#include "GoCrypto.h"

class WxVolumeTriggers : public ImPopup
{
private:
	mutable std::mutex _mutex;
	std::vector<ColumnHeader> _columns;
	std::vector<TriggerVolumeBaseItem> _triggers;
	RowBuffer* _cellBuffer;
	TriggerVolumeBaseItem _editingItem;
	int _selectedItemIdx = -1;
	std::string _errorString;
	float _firstShowErrorAt = 0;
	ButtonClickEventHandler _applyClickHandler;
protected:
	void updateContent();
public:
	WxVolumeTriggers();
	~WxVolumeTriggers();

	void setTriggers(const std::vector<TriggerVolumeBaseItem>& triggers);
	const std::vector<TriggerVolumeBaseItem>& getTriggers() const;
	void clearTriggers();
	int getSelectedItemIndex() const;

	void setOnApplyButtonClickHandler(ButtonClickEventHandler&& handler);
};