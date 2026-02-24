#pragma once
#include "menu_objects.h"
#include "../popup.h"

class MenuVehicleManagePlugins: public Menu
{
   public:
      MenuVehicleManagePlugins();
      virtual ~MenuVehicleManagePlugins();
      virtual void Render();
      virtual void onShow();
      virtual bool periodicLoop();
      virtual int  onBack();
      virtual void onSelectItem();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);

   protected:
      void populateInfo();
      int m_IndexSync;
      Popup* m_pPopup;
      bool m_bWaitingForVehicleInfo;

      MenuItemSelect* m_pItemsSelect[50];
      int m_IndexPlugins[20];
      int m_iCountPlugins;
      int m_IndexSelectedPlugin;
};