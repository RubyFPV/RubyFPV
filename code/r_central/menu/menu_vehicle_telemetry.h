#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

class MenuVehicleTelemetry: public Menu
{
   public:
      MenuVehicleTelemetry();
      virtual ~MenuVehicleTelemetry();
      virtual void onShow();
      virtual void Render();
      virtual void onSelectItem();
      virtual void valuesToUI();

   private:
      void addItems();
      MenuItemSelect* m_pItemsSelect[15];
      MenuItemSlider* m_pItemsSlider[10];
      MenuItemRange*  m_pItemsRange[10];

      int m_IndexAutodetect;
      int m_IndexTelemetryAnySystem;
      int m_IndexTelemetryNoFCMessages;
      int m_IndexTelemetryRequestStreams;
      int m_IndexTelemetryControllerSysId;
      int m_IndexAlwaysArmed;
      int m_IndexInfoSysId;
      int m_IndexRemoveDuplicateMsg;
      int m_IndexRUpdateRate;
      int m_IndexVTelemetryType, m_IndexVSerialPort, m_IndexVBaudRate;
      int m_IndexSpectator;
      int m_IndexFullTelemetry;
      int m_IndexGPS;
};