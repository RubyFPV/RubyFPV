#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"

class MenuButtons: public Menu
{
   public:
      MenuButtons();
      virtual void onShow();
      virtual void Render();
      virtual void onSelectItem();
      virtual void valuesToUI();

   private:
      MenuItemSelect* m_pItemsSelect[25];
      int m_IndexRotaryEncoder;
      int m_IndexRotarySpeed;
      int m_IndexRotaryEncoder2;
      int m_IndexRotarySpeed2;
};