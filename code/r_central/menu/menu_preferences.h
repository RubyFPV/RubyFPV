#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"

class MenuPreferences: public Menu
{
   public:
      MenuPreferences();
      virtual void onShow();
      virtual void Render();
      virtual void onSelectItem();
      virtual void valuesToUI();

   private:
      MenuItemSelect* m_pItemsSelect[25];
      int m_IndexUnits;
      int m_IndexPersistentMessages;
      int m_IndexLogWindow;
};