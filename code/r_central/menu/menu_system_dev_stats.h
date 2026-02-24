#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuSystemDevStats: public Menu
{
   public:
      MenuSystemDevStats();
      virtual void onShow();
      virtual void Render();
      virtual void valuesToUI();
      virtual void onSelectItem();

   private:
      MenuItemSelect* m_pItemsSelect[20];
      MenuItemSlider* m_pItemsSlider[15];

      int m_IndexDevStatsVideo;
      int m_IndexDevStatsRadio;
      int m_IndexDevStatsVehicle;
      int m_IndexDevStatsVehicleTx;
      int m_IndexDevFullRXStats;
      int m_IndexDevVehicleVideoBitrateHistory;
      int m_IndexShowControllerAdaptiveInfoStats;
};