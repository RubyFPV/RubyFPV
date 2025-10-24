#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_checkbox.h"
#include "menu_item_text.h"
#include "menu_item_edit.h"

class MenuVehicleWiFiDirect: public Menu
{
   public:
      MenuVehicleWiFiDirect();
      virtual ~MenuVehicleWiFiDirect();
      virtual void onShow();
      virtual void Render();
      virtual void onSelectItem();
      virtual void valuesToUI();
            
   private:
      void addItems();
      void sendWiFiDirectSettings();
      void updateUIState();
      
      // Menu item indices
      int m_IndexEnabled;
      int m_IndexMode;
      int m_IndexSSID;
      int m_IndexPassword;
      int m_IndexChannel;
      int m_IndexIPAddress;
      int m_IndexNetmask;
      int m_IndexDHCP;
      int m_IndexDHCPStart;
      int m_IndexDHCPEnd;
      int m_IndexPort;
      int m_IndexVTXIP;
      int m_IndexPreferWiFi;
      int m_IndexAutoFailover;
      int m_IndexMulticast;
      int m_IndexStatus;
      int m_IndexScanNetworks;
      
      // Menu items
      MenuItemCheckbox* m_pItemEnabled;
      MenuItemSelect* m_pItemMode;
      MenuItemEdit* m_pItemSSID;
      MenuItemEdit* m_pItemPassword;
      MenuItemSelect* m_pItemChannel;
      MenuItemEdit* m_pItemIPAddress;
      MenuItemEdit* m_pItemNetmask;
      MenuItemCheckbox* m_pItemDHCP;
      MenuItemEdit* m_pItemDHCPStart;
      MenuItemEdit* m_pItemDHCPEnd;
      MenuItemEdit* m_pItemPort;
      MenuItemEdit* m_pItemVTXIP;
      MenuItemCheckbox* m_pItemPreferWiFi;
      MenuItemCheckbox* m_pItemAutoFailover;
      MenuItemCheckbox* m_pItemMulticast;
      MenuItemText* m_pItemStatus;
      
      // State
      bool m_bWaitingForScan;
      u32 m_uScanStartTime;
};
