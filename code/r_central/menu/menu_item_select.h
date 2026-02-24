#pragma once
#include "menu_item_select_base.h"

class MenuItemSelect: public MenuItemSelectBase
{
   public:

     MenuItemSelect(const char* title);
     MenuItemSelect(const char* title, const char* tooltip);
     virtual ~MenuItemSelect();

     virtual void onKeyUp(bool bIgnoreReversion);
     virtual void onKeyDown(bool bIgnoreReversion);
     virtual void Render(float xPos, float yPos, bool bSelected, float fWidthSelection);

     void setUseMultiViewLayout();
     void setUsePopupSelector();
     void setPopupSelectorToRight();
     void disablePopupSelector();
     void setTooltipForSelection(int selectionIndex, const char* szTooltip);
     void addSeparator();

   protected:
     void RenderPopupSelections(float xPos, float yPos, bool bSelected, float fWidthSelection);

     bool m_bUseMultipleViewLayout;
     bool m_bUsePopupSelector;
     bool m_bPopupSelectorCentered;
     int m_iCountSeparators;
     int m_SeparatorIndexes[20];

     char* m_szSelectionsTooltips[MAX_MENU_ITEM_SELECTIONS];

};
