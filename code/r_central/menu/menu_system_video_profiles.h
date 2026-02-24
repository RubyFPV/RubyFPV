#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuSystemVideoProfiles: public Menu
{
   public:
      MenuSystemVideoProfiles();
      virtual void onShow();
      virtual void Render();
      virtual void valuesToUI();
      virtual void onSelectItem();

   private:

      MenuItemSelect* m_pItemsSelect[200];
      MenuItemSlider* m_pItemsSlider[200];
};