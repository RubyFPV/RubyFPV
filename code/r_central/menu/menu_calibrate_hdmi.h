#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

class MenuCalibrateHDMI: public Menu
{
   public:
      MenuCalibrateHDMI();
      virtual ~MenuCalibrateHDMI();
      virtual void Render();
      virtual int onBack();
      virtual void onSelectItem();
      virtual void valuesToUI();

   private:

      MenuItemSlider* m_pItemsSlider[15];
      MenuItemSelect* m_pItemsSelect[20];
      MenuItemRange* m_pItemsRange[15];
};