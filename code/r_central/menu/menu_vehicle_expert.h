#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuVehicleExpert: public Menu
{
   public:
      MenuVehicleExpert();
      virtual void onShow();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();
      
   private:
      int m_IndexReboot;
      int m_IndexDHCP;
      int m_IndexCPUEnabled;
      int m_IndexGPUEnabled;
      int m_IndexVoltageEnabled;
      int m_IndexCPUSpeed;
      int m_IndexGPUSpeed;
      int m_IndexVoltage;
      int m_IndexReset;
      int m_IndexPriorities;
      
      MenuItemSlider* m_pItemsSlider[20];
      MenuItemSelect* m_pItemsSelect[20];

      void addTopInfo();

};