#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"

class MenuAbout: public Menu
{
   public:
      MenuAbout();
      virtual void Render();
      virtual void valuesToUI();
      virtual bool periodicLoop();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();

   private:
      MenuItemSelect* m_pItemsSelect[15];
      MenuItemSlider* m_pItemsSlider[10];

      int m_IndexOK;
};