#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

class MenuVehicleOSD: public Menu
{
   public:
      MenuVehicleOSD();
      virtual ~MenuVehicleOSD();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onSelectItem();
            
   private:
      int m_IndexSettings;
      int m_IndexAlarms;
      int m_IndexPlugins;
      int m_IndexOSDLayout;
      int m_IndexOSDEnabled;
      int m_IndexOSDFontSize, m_IndexOSDTransparency, m_IndexBgOnTexts;
      int m_IndexOSDElements;
      int m_IndexOSDInstruments;
      int m_IndexOSDStats;
      MenuItemSlider* m_pItemsSlider[3];
      MenuItemSelect* m_pItemsSelect[10];
      MenuItemRange* m_pItemsRange[10];
};