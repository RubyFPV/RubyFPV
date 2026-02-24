#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuSystemDevLogs: public Menu
{
   public:
      MenuSystemDevLogs();
      virtual void onShow();
      virtual void Render();
      virtual void valuesToUI();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();

   private:
      void exportAllLogs();
      void exportVehicleLogs(char* szVehicleFolder);

      MenuItemSelect* m_pItemsSelect[20];
      MenuItemSlider* m_pItemsSlider[15];

      int m_IndexLogServiceController;
      int m_IndexLogServiceVehicle;
      int m_IndexLogLevelVehicle;
      int m_IndexLogLevelController;
      int m_IndexEnableLiveLog;
      int m_IndexGetVehicleLogs;
      int m_IndexZipAllLogs;
      int m_IndexClearControllerLogs;
      int m_IndexClearVehicleLogs;
};