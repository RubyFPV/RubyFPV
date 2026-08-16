#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

class MenuControllerNetwork: public Menu
{
   public:
      MenuControllerNetwork();
      virtual void Render();
      virtual void valuesToUI();
      virtual void onSelectItem();

   private:
      MenuItemSelect* m_pItemsSelect[15];
      MenuItemSlider* m_pItemsSlider[10];
      MenuItemRange* m_pItemsRange[14];

      int m_IndexSSH;
      int m_IndexDHCP;
      int m_IndexFixedIP;
      int m_IndexIP;
      int m_IndexWifiEnable;
      int m_IndexWifiSSID;
      int m_IndexWifiPassword;

      void _load_wifi_config();
      void _save_wifi_config();

      int m_iWifiEnabled;
      char m_szWifiSSID[128];
      char m_szWifiPassword[128];
};