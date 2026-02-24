#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "menu_item_section.h"
#include "menu_item_radio.h"
#include "menu_item_checkbox.h"

class MenuVehicleRCInput: public Menu
{
   public:
      MenuVehicleRCInput();
      virtual ~MenuVehicleRCInput();
      virtual void Render();
      virtual void onShow();
      virtual void onSelectItem();
      virtual void valuesToUI();

   private:
      MenuItemSelect* m_pItemsSelect[10];
      MenuItemSlider* m_pItemsSlider[10];
      MenuItemRadio* m_pItemsRadio[10];

      int m_IndexInputType;
};