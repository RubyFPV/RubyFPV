#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"

class MenuControllerRecording: public Menu
{
   public:
      MenuControllerRecording();
      virtual void onShow();     
      virtual void Render();
      virtual void onSelectItem();
      virtual void valuesToUI();
      
   private:
      MenuItemSelect* m_pItemsSelect[30];
      MenuItemSlider* m_pItemsSlider[5];
      int m_IndexRecordIndicator, m_IndexRecordArm, m_IndexRecordDisarm, m_IndexRecordButton, m_IndexRecordLED;
      int m_IndexVideoDestination;
      int m_iIndexStopOnLinkLost;
      int m_iIndexStopOnLinkLostTime;

      int m_iIndexAddOSDToScreenshots;
      int m_iIndexRecordOSD;
      int m_iIndexRecordSTR;
      int m_iIndexSTRFramerate;
      int m_iIndexSTRTime;
      int m_iIndexSTRHome;
      int m_iIndexSTRGPS;
      int m_iIndexSTRAlt;
      int m_iIndexSTRRSSI;
      int m_iIndexSTRVoltage;
      int m_iIndexSTRBitrate;
};