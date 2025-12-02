#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

class MenuControllerVideo: public Menu
{
   public:
      MenuControllerVideo();
      virtual ~MenuControllerVideo();
      virtual void Render();
      virtual void valuesToUI();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual int onBack();
      virtual void onSelectItem();

   private:
      MenuItemSelect* m_pItemsSelect[15];
      MenuItemSlider* m_pItemsSlider[10];
      MenuItemRange* m_pItemsRange[14];

      int m_IndexHDMIBoost;
      int m_IndexHDMIRes;
      int m_IndexHDMIRefreshRate;
      int m_IndexHDMIOverscan;
      int m_IndexHDMIVSync;

      int m_IndexVideoUSBForward;
      int m_IndexVideoUSBPort;
      int m_IndexVideoUSBPacket;

      int m_IndexVideoETHForward;
      int m_IndexVideoETHPort;
      int m_IndexVideoETHPacket;
      int m_IndexCalibrateHDMI;

      int m_IndexStreamerMode;
      int m_IndexMPPBuffers;
      int m_IndexWaitFullFrame;

      int m_IndexAudioVolume;
      int m_IndexAudioTest;
      
      int m_hdmigroupOrg;
      int m_hdmimodeOrg;
};