#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

class MenuVehicleDataLink: public Menu
{
   public:
      MenuVehicleDataLink();
      virtual ~MenuVehicleDataLink();
      virtual void Render();
      virtual void onSelectItem();
      virtual void valuesToUI();

   private:
      MenuItemSelect* m_pItemsSelect[10];
      MenuItemSlider* m_pItemsSlider[10];
      MenuItemRange*  m_pItemsRange[10];

      int m_IndexPort;
      int m_IndexSpeed;
};