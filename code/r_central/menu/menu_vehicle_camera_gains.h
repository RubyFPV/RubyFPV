#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

class MenuVehicleCameraGains: public Menu
{
   public:
      MenuVehicleCameraGains();
      virtual ~MenuVehicleCameraGains();
      virtual void Render();
      virtual void onSelectItem();
      virtual void onItemValueChanged(int itemIndex);
      virtual void valuesToUI();

   private:
      void updateUIValues();

      MenuItemSlider* m_pItemsSlider[15];
      MenuItemSelect* m_pItemsSelect[20];
      MenuItemRange* m_pItemsRange[15];

      int m_IndexGain, m_IndexGainB, m_IndexGainR;
};