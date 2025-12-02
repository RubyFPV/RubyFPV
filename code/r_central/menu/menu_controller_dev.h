#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuControllerDev: public Menu
{
   public:
      MenuControllerDev();
      virtual void onShow(); 
      virtual void Render();
      virtual void valuesToUI();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();

   private:
      void addItems();
      MenuItemSelect* m_pItemsSelect[20];
      MenuItemSlider* m_pItemsSlider[15];

      int m_IndexMaxPacketSize;
      int m_IndexPCAPRadioTx;
      int m_IndexBypassSocketBuffers;
      int m_IndexPingClockSpeed;
      int m_IndexWiFiChangeDelay;
      int m_IndexRxLoopTimeout;
      int m_IndexDebugRTStatsGraphs;
      int m_IndexDebugRTStatsConfig;
      int m_IndexMPPBuffers;
      int m_IndexRenderOSDFSP;
      int m_IndexCPULoad;
      int m_IndexFreezeOSD;
      int m_IndexStreamerMode;
      int m_IndexWaitFullFrame;
      int m_IndexResetDev;
      int m_IndexExit;
};