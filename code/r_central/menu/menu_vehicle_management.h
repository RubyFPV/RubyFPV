#pragma once
#include "menu_objects.h"

#define MAX_SETTINGS_IMPORT_FILES 256

class MenuVehicleManagement: public Menu
{
   public:
      MenuVehicleManagement();
      virtual ~MenuVehicleManagement();
      virtual void Render();
      virtual void onShow();
      virtual bool periodicLoop();
      virtual void onSelectItem();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);

   protected:
      int m_IndexUpdate, m_IndexHWInfo, m_IndexConfig, m_IndexModules, m_IndexReset, m_IndexDelete, m_IndexReboot;
      int m_IndexExport, m_IndexImport, m_IndexPlugins;
      int m_IndexFactoryReset;

      bool m_bWaitingForVehicleInfo;
};