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
      virtual void onShow();
      virtual void Render();
      virtual void onSelectItem();
            
   private:
      int m_IndexFontSize, m_IndexTransparency, m_IndexFitWidgets;
      int m_IndexStatsRadioLinks, m_IndexStatsRadioInterfaces, m_IndexVehicleRadioRxStats, m_IndexStatsDecode;
      int m_IndexRadioRxHistoryController;
      int m_IndexRadioRxHistoryControllerBig;
      int m_IndexRadioRxHistoryVehicle;
      int m_IndexRadioRxHistoryVehicleBig;
      int m_IndexRadioRxDbgPingStats;
      int m_IndexAudioDecodeStats;
      int m_IndexVehicleDevStats;
      int m_IndexVideoFramesStats;
      int m_IndexRadioRefreshInterval, m_IndexVideoRefreshInterval, m_IndexSnapshot, m_IndexSnapshotTimeout;
      int m_IndexStatsVideoH264FramesInfo, m_IndexRefreshIntervalVideoBitrateHistory;
      int m_IndexTelemetryStats;
      int m_IndexShowControllerAdaptiveInfoStats;
      int m_IndexStatsVideoExtended, m_IndexStatsEff, m_IndexStatsRC;
      int m_IndexPanelsDirection;

      int m_IndexDevStatsVideo, m_IndexDevStatsVehicleTx, m_IndexDevVehicleVideoBitrateHistory, m_IndexDevStatsRadio, m_IndexDevFullRXStats;
      MenuItemSlider* m_pItemsSlider[40];
      MenuItemSelect* m_pItemsSelect[40];
      MenuItemRange* m_pItemsRange[40];
};