/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "menu.h"
#include "menu_objects.h"
#include "menu_controller_network.h"
#include "menu_text.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_item_edit.h"
#include "menu_confirmation.h"

#include "../../base/hardware_files.h"
#include "../../base/config_file_names.h"
#include "../process_router_messages.h"

#include <time.h>
#include <sys/resource.h>

MenuControllerNetwork::MenuControllerNetwork(void)
:Menu(MENU_ID_CONTROLLER_NETWORK, "Controller Local Network Settings", NULL)
{
   m_Width = 0.29;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.2;
   
   ControllerSettings* pCS = get_ControllerSettings();
   char szBuff[128];
   char szOutput[1024];
   szOutput[0] = 0;
   hw_execute_bash_command_raw("hostname -I", szOutput);
   int k = strlen(szOutput)-1;
   while ( k > 0 )
   {
      if ( (szOutput[k] == 10) || (szOutput[k] == 13) )
      {
         szOutput[k] = 0;
         k--;
      }
      else
         break;
   }
   snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Current IP: %s", szOutput);
   addMenuItem(new MenuItemText(szBuff));

   m_pItemsSelect[0] = new MenuItemSelect("Enable ETH and DHCP", "Enables the wired network connection.");
   m_pItemsSelect[0]->addSelection("No");
   m_pItemsSelect[0]->addSelection("Yes");
   m_pItemsSelect[0]->setIsEditable();
   m_IndexDHCP = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[1] = new MenuItemSelect("Use a fixed IP", "Use a fixed IP on the controller, for point to point ETH connections.");
   m_pItemsSelect[1]->addSelection("No");
   m_pItemsSelect[1]->addSelection("Yes");
   m_pItemsSelect[1]->setIsEditable();
   m_IndexFixedIP = addMenuItem(m_pItemsSelect[1]);

   m_pItemsRange[0] = new MenuItemRange("IP", "Sets the fixed IP to assign to the controller.", 0, 255, 20, 1 );
   snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%d.%d.%d.", pCS->uFixedIP >> 24, (pCS->uFixedIP >> 16 ) & 0xFF, (pCS->uFixedIP >> 8 ) & 0xFF );
   m_pItemsRange[0]->setPrefix(szBuff);
   m_IndexIP = addMenuItem(m_pItemsRange[0]);
   addMenuItem(new MenuItemText("Note: Changing network settings requires a restart."));

   m_IndexSSH = addMenuItem( new MenuItem("Enable SSH", "Enables SSH login to this controller"));

   #if defined(HW_PLATFORM_RADXA)
   addMenuItem(new MenuItemSection("Internal WiFi"));

   m_iWifiEnabled = 0;
   m_szWifiSSID[0] = 0;
   m_szWifiPassword[0] = 0;
   _load_wifi_config();

   m_pItemsSelect[2] = new MenuItemSelect("Enable Internal WiFi", "Use the built-in WiFi to connect to a local network for SSH and updates. Auto-disables when paired with a vehicle.");
   m_pItemsSelect[2]->addSelection("No");
   m_pItemsSelect[2]->addSelection("Yes");
   m_pItemsSelect[2]->setIsEditable();
   m_IndexWifiEnable = addMenuItem(m_pItemsSelect[2]);

   m_IndexWifiSSID = addMenuItem(new MenuItemEdit("WiFi SSID", "Name of the WiFi network to connect to.", m_szWifiSSID));
   m_IndexWifiPassword = addMenuItem(new MenuItemEdit("WiFi Password", "Password for the WiFi network.", m_szWifiPassword));

   // Show WiFi IP if connected
   char szWifiIP[256];
   szWifiIP[0] = 0;
   hw_execute_bash_command_raw("ip addr show intwifi 2>/dev/null | grep 'inet ' | awk '{print $2}' | cut -d/ -f1", szWifiIP);
   removeTrailingNewLines(szWifiIP);
   if ( 0 != szWifiIP[0] )
   {
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "WiFi IP: %s", szWifiIP);
      addMenuItem(new MenuItemText(szBuff));
   }
   else
   {
      addMenuItem(new MenuItemText("WiFi: Not connected"));
   }
   #endif
}

void MenuControllerNetwork::_load_wifi_config()
{
   m_iWifiEnabled = 0;
   m_szWifiSSID[0] = 0;
   m_szWifiPassword[0] = 0;

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_WIFI_CONNECT);

   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
      return;

   char szLine[256];
   if ( NULL != fgets(szLine, sizeof(szLine), fd) )
   {
      removeTrailingNewLines(szLine);
      m_iWifiEnabled = atoi(szLine);
   }
   if ( NULL != fgets(m_szWifiSSID, sizeof(m_szWifiSSID), fd) )
      removeTrailingNewLines(m_szWifiSSID);
   if ( NULL != fgets(m_szWifiPassword, sizeof(m_szWifiPassword), fd) )
      removeTrailingNewLines(m_szWifiPassword);

   fclose(fd);
}

void MenuControllerNetwork::_save_wifi_config()
{
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_WIFI_CONNECT);

   FILE* fd = fopen(szFile, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to save WiFi config to %s", szFile);
      return;
   }
   fprintf(fd, "%d\n%s\n%s\n", m_iWifiEnabled, m_szWifiSSID, m_szWifiPassword);
   fclose(fd);
   log_line("Saved WiFi config: enabled=%d, SSID=%s", m_iWifiEnabled, m_szWifiSSID);
}

void MenuControllerNetwork::valuesToUI()
{
   ControllerSettings* pCS = get_ControllerSettings();

   if( access( "/boot/nodhcp", R_OK ) == -1 )
      m_pItemsSelect[0]->setSelection(1);
   else
      m_pItemsSelect[0]->setSelection(0);

   m_pItemsSelect[1]->setSelectedIndex(pCS->nUseFixedIP);

   m_pItemsRange[0]->setCurrentValue(pCS->uFixedIP & 0xFF);
   if ( pCS->nUseFixedIP )
      m_pItemsRange[0]->setEnabled(true);
   else
      m_pItemsRange[0]->setEnabled(false);

   #if defined(HW_PLATFORM_RADXA)
   m_pItemsSelect[2]->setSelectedIndex(m_iWifiEnabled);
   #endif
}

void MenuControllerNetwork::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuControllerNetwork::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   ControllerSettings* pCS = get_ControllerSettings();
   if ( NULL == pCS )
   {
      log_softerror_and_alarm("Failed to get pointer to controller settings structure");
      return;
   }

   Preferences* pp = get_Preferences();
   if ( NULL == pp )
   {
      log_softerror_and_alarm("Failed to get pointer to preferences structure");
      return;
   }


   if ( m_IndexDHCP == m_SelectedIndex )
   {
      hardware_mount_boot();
      hardware_sleep_ms(50);
      if ( 1 == m_pItemsSelect[0]->getSelectedIndex() )
      {
         log_line("Enabling DHCP");
         hw_execute_bash_command("rm -f /boot/nodhcp", NULL);
         pCS->nUseFixedIP = 0;
         save_ControllerSettings();
      }
      else
      {
         log_line("Disabling DHCP");
         hw_execute_bash_command("echo '1' > /boot/nodhcp", NULL);
      }
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);       
      return;
   }  

   if ( m_IndexFixedIP == m_SelectedIndex )
   {
      pCS->nUseFixedIP = m_pItemsSelect[1]->getSelectedIndex();
      save_ControllerSettings();
      if ( 1 == pCS->nUseFixedIP )
         hw_execute_bash_command("echo '1' > /boot/nodhcp", NULL);
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);       
      return;
   }

   if ( m_IndexIP == m_SelectedIndex )
   {
      m_pItemsRange[0]->invalidate();
      pCS->uFixedIP &= ~0xFF;
      pCS->uFixedIP |= ((int)m_pItemsRange[0]->getCurrentValue()) & 0xFF;
      save_ControllerSettings();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);       
      return;
   }

   if ( m_IndexSSH == m_SelectedIndex )
   {
      #if defined(HW_PLATFORM_RASPBERRY)
      hw_execute_bash_command("touch /boot/ssh", NULL);
      #endif

      #if defined(HW_PLATFORM_RADXA)
      hw_execute_bash_command("sudo systemctl enable ssh", NULL);
      #endif

      menu_discard_all();
      popups_remove_all();
      addMessage("SSH enabled. Controller will reboot now.");

      for( int i=0; i<5; i++ )
      {
         ruby_processing_loop(true);
         g_TimeNow = get_current_timestamp_ms();
         render_all(g_TimeNow);
         ruby_signal_alive();
         hardware_sleep_ms(200);
      }
      onEventReboot();
      hardware_reboot();
   }

   #if defined(HW_PLATFORM_RADXA)
   if ( m_IndexWifiEnable == m_SelectedIndex )
   {
      m_iWifiEnabled = m_pItemsSelect[2]->getSelectedIndex();
      _save_wifi_config();

      if ( m_iWifiEnabled )
      {
         if ( 0 == m_szWifiSSID[0] )
         {
            addMessage("Please set WiFi SSID first.");
            return;
         }
         hw_execute_bash_command("ruby_init_wifi 2>/dev/null &", NULL);
         addMessage("Connecting to WiFi...");
      }
      else
      {
         hw_execute_bash_command("ruby_init_wifi -disconnect 2>/dev/null &", NULL);
         addMessage("WiFi disconnected.");
      }
      valuesToUI();
      return;
   }

   if ( m_IndexWifiSSID == m_SelectedIndex )
   {
      MenuItemEdit* pEdit = (MenuItemEdit*)m_pMenuItems[m_IndexWifiSSID];
      strncpy(m_szWifiSSID, pEdit->getCurrentValue(), sizeof(m_szWifiSSID)-1);
      m_szWifiSSID[sizeof(m_szWifiSSID)-1] = 0;
      _save_wifi_config();
      return;
   }

   if ( m_IndexWifiPassword == m_SelectedIndex )
   {
      MenuItemEdit* pEdit = (MenuItemEdit*)m_pMenuItems[m_IndexWifiPassword];
      strncpy(m_szWifiPassword, pEdit->getCurrentValue(), sizeof(m_szWifiPassword)-1);
      m_szWifiPassword[sizeof(m_szWifiPassword)-1] = 0;
      _save_wifi_config();
      return;
   }
   #endif
}
