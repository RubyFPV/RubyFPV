#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

class MenuVehicleAlarms: public Menu
{
   public:
      MenuVehicleAlarms();
      virtual ~MenuVehicleAlarms();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onSelectItem();

   private:
      MenuItemSlider* m_pItemsSlider[4];
      MenuItemSelect* m_pItemsSelect[14];
      MenuItemRange* m_pItemsRange[14];

      int m_IndexOverload;
      int m_IndexLinkLost;
      int m_IndexAlarmMotorCurrent;
      int m_iIndexFlashOSDOnTelemLost;
      int m_iIndexTemperature;
};