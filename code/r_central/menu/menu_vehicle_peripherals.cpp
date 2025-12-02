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

#include "../../public/ruby_core_plugin.h"
#include "../render_commands.h"
#include "menu.h"
#include "menu_vehicle.h"
#include "menu_vehicle_general.h"
//#include "menu_vehicle_radio.h"
#include "menu_vehicle_video.h"
#include "menu_vehicle_camera.h"
#include "menu_vehicle_osd.h"
#include "menu_vehicle_expert.h"
#include "menu_confirmation.h"
#include "menu_vehicle_telemetry.h"
#include "menu_vehicle_peripherals.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_item_legend.h"

#include <ctype.h>

MenuVehiclePeripherals::MenuVehiclePeripherals(void)
:Menu(MENU_ID_VEHICLE_PERIPHERALS, "Vehicle Peripherals Settings", NULL)
{
   m_Width = 0.32;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.4;
   m_bWaitingForVehicleInfo = false;
   
   char szBuff[256];

   addMenuItem(new MenuItemSection("Serial Ports"));

   for( int i=0; i<g_pCurrentModel->hardwareInterfacesInfo.serial_port_count; i++ )
   {
      if ( i != 0 )
         addSeparator();
      u32 uUsage = g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & 0xFF;
      sprintf( szBuff, "%s Usage:", g_pCurrentModel->hardwareInterfacesInfo.serial_port_names[i] );
      
      m_pItemsSelect[i*2] = new MenuItemSelect(szBuff);
      if ( ! (g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & MODEL_SERIAL_PORT_BIT_SUPPORTED) )
      {
         m_pItemsSelect[i*2]->addSelection("Not Supported");
         m_pItemsSelect[i*2]->setEnabled(false);
      }
      else
      {
         if ( uUsage == SERIAL_PORT_USAGE_SIK_RADIO )
         {
            m_pItemsSelect[i*2]->addSelection("None");
            m_pItemsSelect[i*2]->addSelection("SiK Radio Interface");
         }
         else
         {
            m_pItemsSelect[i*2]->addSelection(str_get_serial_port_usage(SERIAL_PORT_USAGE_NONE));
            m_pItemsSelect[i*2]->addSelection(str_get_serial_port_usage(SERIAL_PORT_USAGE_TELEMETRY));
            m_pItemsSelect[i*2]->addSelection(str_get_serial_port_usage(SERIAL_PORT_USAGE_DATA_LINK));
            m_pItemsSelect[i*2]->addSelection(str_get_serial_port_usage(SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_433));
            m_pItemsSelect[i*2]->addSelection(str_get_serial_port_usage(SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_868));
            m_pItemsSelect[i*2]->addSelection(str_get_serial_port_usage(SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_915));
            m_pItemsSelect[i*2]->addSelection(str_get_serial_port_usage(SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_24));
            m_IndexStartPortUsagePluginsStartIndex[i] = 7;

            for( int n=0; n<g_iVehicleCorePluginsCount; n++ )
            {
               if ( g_listVehicleCorePlugins[n].uRequestedCapabilities & CORE_PLUGIN_CAPABILITY_HARDWARE_ACCESS_UART )
               {
                  char szOption[256];
                  snprintf(szOption, sizeof(szOption)/sizeof(szOption[0]), "Core Plugin %s", g_listVehicleCorePlugins[n].szName);
                  m_pItemsSelect[i*2]->addSelection(szOption);
               }
            }

            m_pItemsSelect[i*2]->setIsEditable();
         }
      }
      m_IndexSerialPortsUsage[i] = addMenuItem(m_pItemsSelect[i*2]);

      sprintf( szBuff, "%s Baudrate", g_pCurrentModel->hardwareInterfacesInfo.serial_port_names[i] );
      m_pItemsSelect[i*2+1] = new MenuItemSelect(szBuff, "Sets the baud rate of this serial port.");
      for( int n=0; n<hardware_serial_get_baud_rates_count(); n++ )
      {
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%d bps", hardware_serial_get_baud_rates()[n]);
         m_pItemsSelect[i*2+1]->addSelection(szBuff);
      }
      m_pItemsSelect[i*2+1]->setIsEditable();
      m_IndexSerialPortsBaudRate[i] = addMenuItem(m_pItemsSelect[i*2+1]);

      //if ( uUsage == SERIAL_PORT_USAGE_SIK_RADIO )
      //{
      //   sprintf(szBuff,"This serial port is used by a radio interface. Radio interfaces settings are configured from [Radio Configuration] menu.");
      //   addMenuItem( new MenuItemText(szBuff, true) );
      //}
   }
}

MenuVehiclePeripherals::~MenuVehiclePeripherals()
{
}

void MenuVehiclePeripherals::onShow()
{
   Menu::onShow();
}


void MenuVehiclePeripherals::valuesToUI()
{
   for( int i=0; i<g_pCurrentModel->hardwareInterfacesInfo.serial_port_count; i++ )
   {
      if ( ! (g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & MODEL_SERIAL_PORT_BIT_SUPPORTED) )
      {
         m_pItemsSelect[i*2]->setSelectedIndex(0);
         m_pItemsSelect[i*2]->setEnabled(false);
         continue;
      }

      m_pItemsSelect[i*2]->setEnabled(true);
      
      u32 uUsage = g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & 0xFF;
      if ( uUsage == SERIAL_PORT_USAGE_SIK_RADIO )
      {
         m_pItemsSelect[i*2]->setSelectedIndex(1);
         m_pItemsSelect[i*2]->setEnabled(false);
      }
      else if ( uUsage < SERIAL_PORT_USAGE_CORE_PLUGIN )
      {
         if ( uUsage == SERIAL_PORT_USAGE_NONE )
            m_pItemsSelect[i*2]->setSelectedIndex(0);
         if ( uUsage == SERIAL_PORT_USAGE_TELEMETRY )
            m_pItemsSelect[i*2]->setSelectedIndex(1);
         if ( uUsage == SERIAL_PORT_USAGE_DATA_LINK )
            m_pItemsSelect[i*2]->setSelectedIndex(2);
         if ( uUsage == SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_433 )
            m_pItemsSelect[i*2]->setSelectedIndex(3);
         if ( uUsage == SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_868 )
            m_pItemsSelect[i*2]->setSelectedIndex(4);
         if ( uUsage == SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_915 )
            m_pItemsSelect[i*2]->setSelectedIndex(5);
         if ( uUsage == SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_24 )
            m_pItemsSelect[i*2]->setSelectedIndex(6);
      }
      else if ( ((int)uUsage - 20) > g_iVehicleCorePluginsCount )
         m_pItemsSelect[i*2]->setSelectedIndex(0);
      else
      {
         int iCountUARTPlugins = 0;
         int iTargetPluginIndex = uUsage - 20;
         for( int n=0; n<g_iVehicleCorePluginsCount; n++ )
         {
            if ( g_listVehicleCorePlugins[n].uRequestedCapabilities & CORE_PLUGIN_CAPABILITY_HARDWARE_ACCESS_UART )
            {
               if ( iTargetPluginIndex == n )
               {
                  m_pItemsSelect[i*2]->setSelectedIndex(m_IndexStartPortUsagePluginsStartIndex[i] + iCountUARTPlugins);
                  break;
               }
               iCountUARTPlugins++;
            }
         }
      }

      bool bFoundSpeed = false;
      for(int n=0; n<m_pItemsSelect[i*2+1]->getSelectionsCount(); n++ )
      {
         if ( (uUsage == SERIAL_PORT_USAGE_SIK_RADIO) && (hardware_serial_get_baud_rates()[n] < 57000) )
            m_pItemsSelect[i*2+1]->setSelectionIndexDisabled(n);
         else
            m_pItemsSelect[i*2+1]->setSelectionIndexEnabled(n);

         if ( hardware_serial_get_baud_rates()[n] == g_pCurrentModel->hardwareInterfacesInfo.serial_port_speed[i] )
         {
            m_pItemsSelect[i*2+1]->setSelection(n);
            bFoundSpeed = true;
            break;
         }
      }
      if ( ! bFoundSpeed )
      {
         m_pItemsSelect[i*2+1]->setSelection(0);
      }
   }
}

void MenuVehiclePeripherals::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();   
   float y = yTop;
 
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehiclePeripherals::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   for( int i=0; i<g_pCurrentModel->hardwareInterfacesInfo.serial_port_count; i++ )
   {
      if ( m_IndexSerialPortsUsage[i] == m_SelectedIndex )
      {
         u32 newUsage = 0;

         if ( m_pItemsSelect[i*2]->getSelectedIndex() < m_IndexStartPortUsagePluginsStartIndex[i] )
         {
            if ( 0 == m_pItemsSelect[i*2]->getSelectedIndex() )
               newUsage = SERIAL_PORT_USAGE_NONE;
            if ( 1 == m_pItemsSelect[i*2]->getSelectedIndex() )
               newUsage = SERIAL_PORT_USAGE_TELEMETRY;
            if ( 2 == m_pItemsSelect[i*2]->getSelectedIndex() )
               newUsage = SERIAL_PORT_USAGE_DATA_LINK;
            if ( 3 == m_pItemsSelect[i*2]->getSelectedIndex() )
               newUsage = SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_433;
            if ( 4 == m_pItemsSelect[i*2]->getSelectedIndex() )
               newUsage = SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_868;
            if ( 5 == m_pItemsSelect[i*2]->getSelectedIndex() )
               newUsage = SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_915;
            if ( 6 == m_pItemsSelect[i*2]->getSelectedIndex() )
               newUsage = SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_24;
            if ( newUsage == (g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & 0xFF) )
               return;
         }
         else
         {
            int iCountUARTPlugins = 0;
            for( int n=0; n<g_iVehicleCorePluginsCount; n++ )
            {
               if ( g_listVehicleCorePlugins[n].uRequestedCapabilities & CORE_PLUGIN_CAPABILITY_HARDWARE_ACCESS_UART )
               {
                  if ( (m_pItemsSelect[i*2]->getSelectedIndex() - m_IndexStartPortUsagePluginsStartIndex[i]) == iCountUARTPlugins )
                  {
                     newUsage = SERIAL_PORT_USAGE_CORE_PLUGIN + n;
                     break;
                  }
                  iCountUARTPlugins++;
               }
            }
         }
         if ( newUsage == (g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & 0xFF) )
            return;

         type_vehicle_hardware_interfaces_info new_info;
         memcpy((u8*)&new_info, (u8*)&(g_pCurrentModel->hardwareInterfacesInfo), sizeof(type_vehicle_hardware_interfaces_info));
         new_info.serial_port_supported_and_usage[i] &= 0xFFFFFF00;
         new_info.serial_port_supported_and_usage[i] |= (u8)newUsage;

         vehicle_runtime_reset_has_received_fc_telemetry_info(g_pCurrentModel->uVehicleId);
         log_line("Sending new serial ports usage to vehicle.");
         if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_SERIAL_PORTS_INFO, 0, (u8*)&new_info, sizeof(type_vehicle_hardware_interfaces_info)) )
            valuesToUI();
         return;
      }

      if ( m_IndexSerialPortsBaudRate[i] == m_SelectedIndex )
      {
         long val = hardware_serial_get_baud_rates()[m_pItemsSelect[i*2+1]->getSelectedIndex()];
         if ( val == g_pCurrentModel->hardwareInterfacesInfo.serial_port_speed[i] )
            return;

         type_vehicle_hardware_interfaces_info new_info;
         memcpy((u8*)&new_info, (u8*)&(g_pCurrentModel->hardwareInterfacesInfo), sizeof(type_vehicle_hardware_interfaces_info));
         new_info.serial_port_speed[i] = val;
         vehicle_runtime_reset_has_received_fc_telemetry_info(g_pCurrentModel->uVehicleId);
         log_line("Sending new serial ports info to vehicle.");
         if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_SERIAL_PORTS_INFO, 0, (u8*)&new_info, sizeof(type_vehicle_hardware_interfaces_info)) )
            valuesToUI();
         return;
      }
   }
}
