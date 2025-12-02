#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuVehicleCPU_OIPC: public Menu
{
   public:
      MenuVehicleCPU_OIPC();
      virtual void onShow();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();
      
   private:
      int m_IndexCPUSpeed;
      int m_IndexGPUBoost;
      int m_IndexGPUFreqCore1;
      int m_IndexGPUFreqCore2;
      int m_IndexPriorities;
      
      MenuItemSlider* m_pItemsSlider[10];
      MenuItemSelect* m_pItemsSelect[10];
};