#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuVehicleDev: public Menu
{
   public:
      MenuVehicleDev();
      virtual void onShow(); 
      virtual void Render();
      virtual void valuesToUI();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();

   private:
      void addItems();
      MenuItemSelect* m_pItemsSelect[20];
      MenuItemSlider* m_pItemsSlider[15];

      int m_IndexDevStats;
      int m_IndexPCAPRadioTx;
      int m_IndexBypassSocketBuffers;
      int m_IndexClockSyncType;
      int m_IndexRadioSilence;
      int m_IndexRxLoopTimeout;
      int m_IndexRetransmissionWindow;
      int m_IndexInjectMinorVideoFaults;
      int m_IndexInjectVideoFaults;
      int m_IndexTestAdaptive;
      int m_IndexInsertVideoDbgTimings;
      int m_IndexResetDev;
};