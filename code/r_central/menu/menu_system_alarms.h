#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuSystemAlarms: public Menu
{
   public:
      MenuSystemAlarms();
      virtual void onShow();
      virtual void Render();
      virtual void valuesToUI();
      virtual void onSelectItem();

   private:
      MenuItemSelect* m_pItemsSelect[15];
      MenuItemSlider* m_pItemsSlider[10];

      int m_IndexAllAlarms;
      int m_IndexAlarmControllerIOErrors;
      int m_IndexAlarmInvalidRadioPackets;
      int m_IndexAlarmControllerLink;
      int m_IndexAlarmVideoDataOverload;
      int m_IndexAlarmVehicleCPUOverload;
      int m_IndexAlarmVehicleRxTimeout;
      int m_IndexAlarmControllerCPUOverload;
      int m_IndexAlarmControllerRxTimeout;
      int m_IndexAlarmControllerRecording;
      int m_IndexAlarmsDev;

      bool m_bMenuSystemAlarmsIsOnCustomOption;
};