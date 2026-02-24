#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

class MenuVehicleInstrumentsGeneral: public Menu
{
   public:
      MenuVehicleInstrumentsGeneral();
      virtual ~MenuVehicleInstrumentsGeneral();
      virtual void Render();
      virtual void onSelectItem();
      virtual void onItemValueChanged(int itemIndex);
      virtual void valuesToUI();

   private:
      MenuItemSlider* m_pItemsSlider[15];
      MenuItemSelect* m_pItemsSelect[20];
      MenuItemRange* m_pItemsRange[15];

      int m_nOSDIndex;

      int m_IndexAHISize;
      int m_IndexAHIStrokeSize;

      int m_IndexAltitudeType, m_IndexLocalVertSpeed;
      int m_IndexRevertRoll, m_IndexRevertPitch;
      int m_IndexAHIShowAirSpeed;
      int m_IndexUnits, m_IndexUnitsHeight;
      int m_IndexTemp;
      int m_IndexBatteryCells;
      int m_iIndexFlashOSDOnTelemLost;
      int m_IndexFlightEndStats;
};