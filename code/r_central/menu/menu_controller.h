#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuController: public Menu
{
   public:
      MenuController();
      virtual void onShow(); 
      virtual void Render();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);  
      virtual void onSelectItem();

   private:
      void addItems();
      MenuItemSelect* m_pItemsSelect[15];
      MenuItemSlider* m_pItemsSlider[10];

      int m_IndexCPU, m_IndexPorts;
      int m_IndexButtons, m_IndexQuickMenu, m_IndexPreferences, m_IndexPreferencesUI, m_IndexPlugins, m_IndexReboot;
      int m_IndexUpdate;
      //int m_IndexShowCPUInfo, m_IndexShowVoltage;
      int m_IndexNetwork, m_IndexEncryption, m_IndexRadio;
      int m_IndexVideo, m_IndexTelemetry, m_IndexRecording;
      bool m_bShownHDMIChangeNotif;
      int m_IndexDeveloper;
};
