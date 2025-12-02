#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuControllerDevStatsConfig: public Menu
{
   public:
      MenuControllerDevStatsConfig();
      virtual void onShow(); 
      virtual void Render();
      virtual void valuesToUI();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();

   private:
      void addItems();
      MenuItemSelect* m_pItemsSelect[30];
      MenuItemSlider* m_pItemsSlider[15];

      int m_IndexQAButton;
      int m_IndexShowRXTXPackets;
      int m_IndexShowRXAirGaps;
      int m_IndexShowOutputVideoFrames;
      int m_IndexShowRxDBM;
      int m_IndexShowRxSNR;
      int m_IndexShowRxMissingPackets;
      int m_IndexShowRxMissingPacketsMaxGap;
      int m_IndexShowRxConsumedPackets;
      int m_IndexShowTxHighRegPackets;
      int m_IndexShowMinMaxAckTime;
      int m_IndexShowAckTimeHist;
      int m_IndexShowRxMaxECUsed;
      int m_IndexShowUnrecoverableVideoBlocks;
      int m_IndexShowVideoProfileChanges;
      int m_IndexShowVideoRetransmissions;
      int m_IndexShowVideoFramesSizes;
      int m_IndexShowVideoFramesProcTimes;
      int m_IndexShowVideoFramesSendTimes;
      int m_IndexShowVideoFramesRecvTimes;
      int m_IndexShowVideoFramesCaptureJitter;
};