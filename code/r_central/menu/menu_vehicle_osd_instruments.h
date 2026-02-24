#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

class MenuVehicleOSDInstruments: public Menu
{
   public:
      MenuVehicleOSDInstruments();
      virtual ~MenuVehicleOSDInstruments();
      virtual void Render();
      virtual void onShow();
      virtual void onSelectItem();
      virtual void onItemValueChanged(int itemIndex);
      virtual void valuesToUI();

   private:
      MenuItemSlider* m_pItemsSlider[15];
      MenuItemSelect* m_pItemsSelect[50];
      MenuItemRange* m_pItemsRange[15];

      int m_IndexAHIToSides;
      int m_IndexAHIShowSpeedAlt;
      int m_IndexAHIShowHeading;
      int m_IndexAHIShowAltGraph;
      int m_IndexPlugins[50];
      int m_IndexPluginsConfigure[50];
};