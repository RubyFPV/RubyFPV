#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

class MenuVehicleRadioRuntimeCapabilities: public Menu
{
   public:
      MenuVehicleRadioRuntimeCapabilities();
      virtual ~MenuVehicleRadioRuntimeCapabilities();
      virtual void onShow();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onSelectItem();

   private:
      MenuItemSelect* m_pItemsSelect[10];
      MenuItemSlider* m_pItemsSlider[10];
      MenuItemRange*  m_pItemsRange[10];
};