#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "menu_item_text.h"


class MenuControllerPeripherals: public Menu
{
   public:
      MenuControllerPeripherals();
      virtual void onShow(); 
      virtual bool periodicLoop();
      virtual void Render();
      virtual void valuesToUI();
      virtual void onSelectItem();

   private:
      void addItems();
      void addI2CDevices();
      MenuItemSelect* m_pItemsSelect[100];
      MenuItemSlider* m_pItemsSlider[20];
      MenuItemRange* m_pItemsRange[14];

      int m_IndexSerialType[10], m_IndexSerialSpeed[10];
      int m_IndexJoysticks[10];
      int m_IndexI2CDevices[20];
      MenuItemText* m_pItemWait;
      int m_IndexWait;
      int m_nSearchI2CDeviceAddress;
      bool m_bShownUSBWarning;

      int m_iSerialBuiltInOptionsCount;
};