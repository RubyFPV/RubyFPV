#include "menu_vehicle_wifi_direct.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"
#include "../osd/osd_common.h"

MenuVehicleWiFiDirect::MenuVehicleWiFiDirect()
:Menu(MENU_ID_VEHICLE_WIFI_DIRECT, "WiFi Direct Link Settings", NULL)
{
   m_Width = 0.42;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.18;
   m_bWaitingForScan = false;
   m_uScanStartTime = 0;
   
   addItems();
}

MenuVehicleWiFiDirect::~MenuVehicleWiFiDirect()
{
}

void MenuVehicleWiFiDirect::addItems()
{
   int c = 0;
   float fTitleHeight = 0.0;
   
   // Enable/Disable WiFi Direct
   m_pItemEnabled = new MenuItemCheckbox("Enable WiFi Direct Link");
   m_pItemEnabled->setTooltip("Enable or disable WiFi Direct video streaming");
   m_IndexEnabled = addMenuItem(m_pItemEnabled);
   
   // WiFi Mode
   m_pItemMode = new MenuItemSelect("WiFi Mode");
   m_pItemMode->addSelection("None");
   m_pItemMode->addSelection("Access Point (VTX)");
   m_pItemMode->addSelection("Client (VRX)");
   m_pItemMode->setTooltip("Access Point mode creates a WiFi network on VTX. Client mode connects to existing network.");
   m_IndexMode = addMenuItem(m_pItemMode);
   
   addMenuItem(new MenuItemSection("Network Settings"));
   
   // SSID
   m_pItemSSID = new MenuItemEdit("Network Name (SSID)", "RubyFPV_Direct");
   m_pItemSSID->setMaxLength(32);
   m_IndexSSID = addMenuItem(m_pItemSSID);
   
   // Password
   m_pItemPassword = new MenuItemEdit("Password", "rubyfpv123");
   m_pItemPassword->setMaxLength(64);
   m_pItemPassword->setTooltip("WPA2 password (8-64 characters). Leave empty for open network.");
   m_IndexPassword = addMenuItem(m_pItemPassword);
   
   // Channel
   m_pItemChannel = new MenuItemSelect("WiFi Channel");
   for (int i = 1; i <= 14; i++) {
      char szBuff[32];
      sprintf(szBuff, "Channel %d", i);
      m_pItemChannel->addSelection(szBuff);
   }
   m_pItemChannel->setTooltip("2.4GHz WiFi channel (1-11 for US, 1-13 for EU, 1-14 for JP)");
   m_IndexChannel = addMenuItem(m_pItemChannel);
   
   addMenuItem(new MenuItemSection("IP Configuration"));
   
   // IP Address (AP mode)
   m_pItemIPAddress = new MenuItemEdit("AP IP Address", "192.168.42.1");
   m_pItemIPAddress->setMaxLength(15);
   m_pItemIPAddress->setTooltip("IP address for Access Point mode");
   m_IndexIPAddress = addMenuItem(m_pItemIPAddress);
   
   // Netmask
   m_pItemNetmask = new MenuItemEdit("Netmask", "255.255.255.0");
   m_pItemNetmask->setMaxLength(15);
   m_IndexNetmask = addMenuItem(m_pItemNetmask);
   
   // DHCP Enable
   m_pItemDHCP = new MenuItemCheckbox("Enable DHCP Server");
   m_pItemDHCP->setTooltip("Enable DHCP server in Access Point mode");
   m_IndexDHCP = addMenuItem(m_pItemDHCP);
   
   // DHCP Range Start
   m_pItemDHCPStart = new MenuItemEdit("DHCP Start IP", "192.168.42.100");
   m_pItemDHCPStart->setMaxLength(15);
   m_IndexDHCPStart = addMenuItem(m_pItemDHCPStart);
   
   // DHCP Range End
   m_pItemDHCPEnd = new MenuItemEdit("DHCP End IP", "192.168.42.200");
   m_pItemDHCPEnd->setMaxLength(15);
   m_IndexDHCPEnd = addMenuItem(m_pItemDHCPEnd);
   
   addMenuItem(new MenuItemSection("Video Stream Settings"));
   
   // UDP Port
   m_pItemPort = new MenuItemEdit("UDP Port", "5600");
   m_pItemPort->setMaxLength(5);
   m_pItemPort->setTooltip("UDP port for video stream");
   m_IndexPort = addMenuItem(m_pItemPort);
   
   // VTX IP (Client mode)
   m_pItemVTXIP = new MenuItemEdit("VTX IP Address", "192.168.42.1");
   m_pItemVTXIP->setMaxLength(15);
   m_pItemVTXIP->setTooltip("VTX IP address to receive video from (Client mode)");
   m_IndexVTXIP = addMenuItem(m_pItemVTXIP);
   
   addMenuItem(new MenuItemSection("Advanced Options"));
   
   // Prefer WiFi
   m_pItemPreferWiFi = new MenuItemCheckbox("Prefer WiFi over Radio");
   m_pItemPreferWiFi->setTooltip("Use WiFi Direct when available instead of radio links");
   m_IndexPreferWiFi = addMenuItem(m_pItemPreferWiFi);
   
   // Auto Failover
   m_pItemAutoFailover = new MenuItemCheckbox("Auto WiFi/Radio Failover");
   m_pItemAutoFailover->setTooltip("Automatically switch between WiFi and radio based on signal quality");
   m_IndexAutoFailover = addMenuItem(m_pItemAutoFailover);
   
   // Multicast
   m_pItemMulticast = new MenuItemCheckbox("Use Multicast");
   m_pItemMulticast->setTooltip("Use multicast for video streaming (allows multiple receivers)");
   m_IndexMulticast = addMenuItem(m_pItemMulticast);
   
   addMenuItem(new MenuItemSection("Status"));
   
   // Status text
   m_pItemStatus = new MenuItemText("Status: Not configured");
   m_IndexStatus = addMenuItem(m_pItemStatus);
   
   // Scan Networks (Client mode)
   m_IndexScanNetworks = addMenuItem(new MenuItem("Scan WiFi Networks"));
}

void MenuVehicleWiFiDirect::valuesToUI()
{
   if (!g_pCurrentModel) {
      return;
   }
   
   // Set values from model
   m_pItemEnabled->setChecked(g_pCurrentModel->wifi_direct_params.uFlags & WIFI_DIRECT_FLAG_ENABLED);
   m_pItemMode->setSelectedIndex(g_pCurrentModel->wifi_direct_params.iMode);
   
   m_pItemSSID->setValue(g_pCurrentModel->wifi_direct_params.szSSID);
   m_pItemPassword->setValue(g_pCurrentModel->wifi_direct_params.szPassword);
   m_pItemChannel->setSelectedIndex(g_pCurrentModel->wifi_direct_params.iChannel - 1);
   
   m_pItemIPAddress->setValue(g_pCurrentModel->wifi_direct_params.szIPAddress);
   m_pItemNetmask->setValue(g_pCurrentModel->wifi_direct_params.szNetmask);
   m_pItemDHCP->setChecked(g_pCurrentModel->wifi_direct_params.iDHCPEnabled);
   m_pItemDHCPStart->setValue(g_pCurrentModel->wifi_direct_params.szDHCPStart);
   m_pItemDHCPEnd->setValue(g_pCurrentModel->wifi_direct_params.szDHCPEnd);
   
   char szPort[16];
   sprintf(szPort, "%d", g_pCurrentModel->wifi_direct_params.iPort);
   m_pItemPort->setValue(szPort);
   
   m_pItemVTXIP->setValue(g_pCurrentModel->wifi_direct_params.szVTXIP);
   
   m_pItemPreferWiFi->setChecked(g_pCurrentModel->wifi_direct_params.uFlags & WIFI_DIRECT_FLAG_PREFER_WIFI);
   m_pItemAutoFailover->setChecked(g_pCurrentModel->wifi_direct_params.uFlags & WIFI_DIRECT_FLAG_AUTO_FAILOVER);
   m_pItemMulticast->setChecked(g_pCurrentModel->wifi_direct_params.uFlags & WIFI_DIRECT_FLAG_MULTICAST);
   
   updateUIState();
}

void MenuVehicleWiFiDirect::updateUIState()
{
   bool bEnabled = m_pItemEnabled->isChecked();
   int iMode = m_pItemMode->getSelectedIndex();
   
   // Enable/disable items based on enabled state
   m_pItemMode->setEnabled(bEnabled);
   m_pItemSSID->setEnabled(bEnabled);
   m_pItemPassword->setEnabled(bEnabled);
   m_pItemChannel->setEnabled(bEnabled);
   m_pItemPort->setEnabled(bEnabled);
   m_pItemPreferWiFi->setEnabled(bEnabled);
   m_pItemAutoFailover->setEnabled(bEnabled);
   m_pItemMulticast->setEnabled(bEnabled);
   
   // Enable/disable items based on mode
   bool bAPMode = (iMode == 1); // Access Point mode
   bool bClientMode = (iMode == 2); // Client mode
   
   m_pItemIPAddress->setEnabled(bEnabled && bAPMode);
   m_pItemNetmask->setEnabled(bEnabled && bAPMode);
   m_pItemDHCP->setEnabled(bEnabled && bAPMode);
   m_pItemDHCPStart->setEnabled(bEnabled && bAPMode && m_pItemDHCP->isChecked());
   m_pItemDHCPEnd->setEnabled(bEnabled && bAPMode && m_pItemDHCP->isChecked());
   
   m_pItemVTXIP->setEnabled(bEnabled && bClientMode);
   m_pMenuItems[m_IndexScanNetworks]->setEnabled(bEnabled && bClientMode);
   
   // Update status text
   if (!bEnabled) {
      m_pItemStatus->setValue("Status: Disabled");
   } else if (iMode == 0) {
      m_pItemStatus->setValue("Status: Mode not selected");
   } else if (iMode == 1) {
      char szStatus[128];
      sprintf(szStatus, "Status: AP Mode - %s @ %s", 
              g_pCurrentModel->wifi_direct_params.szSSID,
              g_pCurrentModel->wifi_direct_params.szIPAddress);
      m_pItemStatus->setValue(szStatus);
   } else if (iMode == 2) {
      char szStatus[128];
      sprintf(szStatus, "Status: Client Mode - Connect to %s", 
              g_pCurrentModel->wifi_direct_params.szSSID);
      m_pItemStatus->setValue(szStatus);
   }
}

void MenuVehicleWiFiDirect::onShow()
{
   valuesToUI();
   Menu::onShow();
}

void MenuVehicleWiFiDirect::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   
   for (int i = 0; i < m_ItemsCount; i++) {
      y += RenderItem(i, y);
   }
   
   RenderEnd(yTop);
}

void MenuVehicleWiFiDirect::onSelectItem()
{
   if (!g_pCurrentModel) {
      return;
   }
   
   Menu::onSelectItem();
   
   if (handle_commands_is_command_in_progress()) {
      handle_commands_show_popup_progress();
      return;
   }
   
   if (m_IndexEnabled == m_SelectedIndex ||
       m_IndexMode == m_SelectedIndex ||
       m_IndexDHCP == m_SelectedIndex ||
       m_IndexPreferWiFi == m_SelectedIndex ||
       m_IndexAutoFailover == m_SelectedIndex ||
       m_IndexMulticast == m_SelectedIndex)
   {
      updateUIState();
      sendWiFiDirectSettings();
      return;
   }
   
   if (m_IndexScanNetworks == m_SelectedIndex) {
      // TODO: Implement network scanning
      MenuConfirmation* pMC = new MenuConfirmation("WiFi Network Scan", "Network scanning not yet implemented.", 0, true);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      return;
   }
   
   // For all other items (text edits), send settings
   sendWiFiDirectSettings();
}

void MenuVehicleWiFiDirect::sendWiFiDirectSettings()
{
   if (!g_pCurrentModel) {
      return;
   }
   
   // Build new WiFi Direct parameters
   type_wifi_direct_parameters params;
   memcpy(&params, &g_pCurrentModel->wifi_direct_params, sizeof(type_wifi_direct_parameters));
   
   // Update flags
   params.uFlags = 0;
   if (m_pItemEnabled->isChecked()) {
      params.uFlags |= WIFI_DIRECT_FLAG_ENABLED;
   }
   if (m_pItemPreferWiFi->isChecked()) {
      params.uFlags |= WIFI_DIRECT_FLAG_PREFER_WIFI;
   }
   if (m_pItemAutoFailover->isChecked()) {
      params.uFlags |= WIFI_DIRECT_FLAG_AUTO_FAILOVER;
   }
   if (m_pItemMulticast->isChecked()) {
      params.uFlags |= WIFI_DIRECT_FLAG_MULTICAST;
   }
   
   // Update mode
   params.iMode = m_pItemMode->getSelectedIndex();
   
   // Update network settings
   strcpy(params.szSSID, m_pItemSSID->getValue());
   strcpy(params.szPassword, m_pItemPassword->getValue());
   params.iChannel = m_pItemChannel->getSelectedIndex() + 1;
   
   // Update IP settings
   strcpy(params.szIPAddress, m_pItemIPAddress->getValue());
   strcpy(params.szNetmask, m_pItemNetmask->getValue());
   params.iDHCPEnabled = m_pItemDHCP->isChecked() ? 1 : 0;
   strcpy(params.szDHCPStart, m_pItemDHCPStart->getValue());
   strcpy(params.szDHCPEnd, m_pItemDHCPEnd->getValue());
   
   // Update port
   params.iPort = atoi(m_pItemPort->getValue());
   if (params.iPort <= 0 || params.iPort > 65535) {
      params.iPort = 5600; // Default
   }
   
   // Update VTX IP
   strcpy(params.szVTXIP, m_pItemVTXIP->getValue());
   
   // Send command to vehicle
   if (!handle_commands_send_to_vehicle(COMMAND_ID_SET_WIFI_DIRECT_PARAMS, 0, (u8*)&params, sizeof(params))) {
      addMessage("Failed to send WiFi Direct settings to vehicle");
      return;
   }
   
   // Update local model
   memcpy(&g_pCurrentModel->wifi_direct_params, &params, sizeof(type_wifi_direct_parameters));
   saveControllerModel(g_pCurrentModel);
}
