#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

class MenuVehicleOSDStats: public Menu
{
   public:
      MenuVehicleOSDStats();
      virtual ~MenuVehicleOSDStats();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onSelectItem();
            
   private:
      int m_IndexFontSize, m_IndexTransparency;
      int m_IndexStatsRadioLinks, m_IndexStatsRadioInterfaces, m_IndexVehicleRadioRxStats, m_IndexStatsDecode;
      int m_IndexRadioInterfacesCompact;
      int m_IndexRadioRefreshInterval, m_IndexVideoRefreshInterval, m_IndexSnapshot, m_IndexSnapshotTimeout;
      int m_IndexStatsVideoStreamInfo;
      int m_IndexShowControllerAdaptiveInfoStats;
      int m_IndexStatsVideoExtended, m_IndexStatsAdaptiveVideoGraph, m_IndexStatsEff, m_IndexStatsRC;
      int m_IndexPanelsDirection;

      int m_IndexDevStatsVideo, m_IndexDevStatsVehicleTx, m_IndexDevVehicleVideoBitrateHistory, m_IndexDevStatsRadio, m_IndexDevFullRXStats, m_IndexDevStatsVehicleVideo, m_IndexDevStatsVehicleVideoGraphs;
      MenuItemSlider* m_pItemsSlider[30];
      MenuItemSelect* m_pItemsSelect[30];
      MenuItemRange* m_pItemsRange[35];
};