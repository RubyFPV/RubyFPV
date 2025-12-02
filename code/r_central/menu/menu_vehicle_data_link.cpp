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
#include "menu_vehicle_data_link.h"
#include "menu_confirmation.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_item_legend.h"


MenuVehicleDataLink::MenuVehicleDataLink(void)
:Menu(MENU_ID_VEHICLE_DATA_LINK, "Auxiliary Data Link", NULL)
{
   m_Width = 0.28;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.24;

   addTopLine("Enable an auxiliary general purpose data link to be used for arbitrary data transport between the vehicle and the controller.");

   m_pItemsSelect[0] = new MenuItemSelect("Vehicle Serial Port", "The vehicle serial port to use for the auxiliary data link.");
   m_pItemsSelect[0]->addSelection("None");
   for( int i=0; i<g_pCurrentModel->hardwareInterfacesInfo.serial_port_count; i++ )
      m_pItemsSelect[0]->addSelection(g_pCurrentModel->hardwareInterfacesInfo.serial_port_names[i]);

   m_pItemsSelect[0]->setIsEditable();
   m_IndexPort = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[1] = new MenuItemSelect("Vehicle Serial Baudrate", "Sets the baud rate on the vehicle serial port for the auxiliary data link.");
   for( int i=0; i<hardware_serial_get_baud_rates_count(); i++ )
   {
      char szBuff[32];
      sprintf(szBuff, "%d bps", hardware_serial_get_baud_rates()[i]);
      m_pItemsSelect[1]->addSelection(szBuff);
   }
   m_pItemsSelect[1]->setIsEditable();
   m_IndexSpeed = addMenuItem(m_pItemsSelect[1]);

   addMenuItem(new MenuItemText("Note: Don't forget to assign a serial port on the controller too, to be used for the auxiliary data link."));
}

MenuVehicleDataLink::~MenuVehicleDataLink()
{
}

void MenuVehicleDataLink::valuesToUI()
{
   int iCurrentSerialPortIndex = -1;
   u32 uCurrentSerialPortSpeed = 0;
   for( int i=0; i<g_pCurrentModel->hardwareInterfacesInfo.serial_port_count; i++ )
   {
       
       if ( g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & MODEL_SERIAL_PORT_BIT_SUPPORTED )
       if ( (g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & 0xFF) == SERIAL_PORT_USAGE_DATA_LINK )
       {
          iCurrentSerialPortIndex = i;
          uCurrentSerialPortSpeed = g_pCurrentModel->hardwareInterfacesInfo.serial_port_speed[i];
          break;
       }
   }

   if ( -1 == iCurrentSerialPortIndex || 0 == uCurrentSerialPortSpeed )
   {
      m_pItemsSelect[0]->setSelectedIndex(0);
      m_pItemsSelect[1]->setEnabled(false);
   }
   else
   {
      m_pItemsSelect[0]->setSelectedIndex( 1 + iCurrentSerialPortIndex );
      m_pItemsSelect[1]->setEnabled(true);
   }
   bool bSpeedFound = false;
   for(int i=0; i<m_pItemsSelect[1]->getSelectionsCount(); i++ )
   {
      if ( hardware_serial_get_baud_rates()[i] == (int)uCurrentSerialPortSpeed )
      {
         m_pItemsSelect[1]->setSelection(i);
         bSpeedFound = true;
         break;
      }
   }
   if ( ! bSpeedFound )
   {
      char szBuff[256];
      sprintf(szBuff, "Info: You are using a custom baud rate (%d) for the auxiliary data link.", (int)uCurrentSerialPortSpeed);
      addTopLine(szBuff);
   }
}

void MenuVehicleDataLink::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuVehicleDataLink::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   int iCurrentSerialPortIndex = -1;

   for( int i=0; i<g_pCurrentModel->hardwareInterfacesInfo.serial_port_count; i++ )
   {
       
       if ( g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & MODEL_SERIAL_PORT_BIT_SUPPORTED )
       if ( (g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & 0xFF) == SERIAL_PORT_USAGE_DATA_LINK )
       {
          iCurrentSerialPortIndex = i;
          break;
       }
   }


   if ( m_IndexPort == m_SelectedIndex )
   {
      int iSerialPort = m_pItemsSelect[0]->getSelectedIndex();
      if ( iSerialPort != 0 )
      if ( (g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[iSerialPort-1] & 0xFF) == SERIAL_PORT_USAGE_SIK_RADIO )
      {
         MenuConfirmation* pMC = new MenuConfirmation("Can't use serial port", "The serial port you selected can't be used for a custom data link as it's used for a SiK radio interface.",1, true);
         pMC->m_yPos = 0.3;
         add_menu_to_stack(pMC);
         valuesToUI();
         return;
      }

      if ( 0 == iSerialPort )
      {
         if ( iCurrentSerialPortIndex != -1 )
         {
            type_vehicle_hardware_interfaces_info new_info;
            memcpy((u8*)&new_info, (u8*)&(g_pCurrentModel->hardwareInterfacesInfo), sizeof(type_vehicle_hardware_interfaces_info));
            new_info.serial_port_supported_and_usage[iCurrentSerialPortIndex] &= 0xFFFFFF00;
            new_info.serial_port_supported_and_usage[iCurrentSerialPortIndex] |= SERIAL_PORT_USAGE_NONE;
            
            log_line("Sending disabling data link serial port selection to vehicle.");
            if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_SERIAL_PORTS_INFO, 0, (u8*)&new_info, sizeof(type_vehicle_hardware_interfaces_info)) )
               valuesToUI();

            return;
         }
      }
      else
      {
         type_vehicle_hardware_interfaces_info new_info;
         memcpy((u8*)&new_info, (u8*)&(g_pCurrentModel->hardwareInterfacesInfo), sizeof(type_vehicle_hardware_interfaces_info));

         u8 uCurrentUsage = new_info.serial_port_supported_and_usage[iSerialPort-1] & 0xFF;
         if ( uCurrentUsage != SERIAL_PORT_USAGE_DATA_LINK )
         {
            if ( uCurrentUsage == SERIAL_PORT_USAGE_TELEMETRY )
            {
               MenuConfirmation* pMC = new MenuConfirmation("Telemetry Link Disabled", "The serial port was used by your telemetry link. It was reasigned to the auxiliary data link.",1, true);
               pMC->m_yPos = 0.3;
               add_menu_to_stack(pMC);      
            }
         }

         if ( iCurrentSerialPortIndex != -1 )
         {
            new_info.serial_port_supported_and_usage[iCurrentSerialPortIndex] &= 0xFFFFFF00;
            new_info.serial_port_supported_and_usage[iCurrentSerialPortIndex] |= SERIAL_PORT_USAGE_NONE;
         }
         new_info.serial_port_supported_and_usage[iSerialPort-1] &= 0xFFFFFF00;
         new_info.serial_port_supported_and_usage[iSerialPort-1] |= SERIAL_PORT_USAGE_DATA_LINK;
         
         log_line("Sending new serial port to be used for data link to vehicle.");
         if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_SERIAL_PORTS_INFO, 0, (u8*)&new_info, sizeof(type_vehicle_hardware_interfaces_info)) )
            valuesToUI();
         return;
      }
   }

   if ( m_IndexSpeed == m_SelectedIndex )
   {
      if ( -1 == iCurrentSerialPortIndex )
         return;

      long val = hardware_serial_get_baud_rates()[m_pItemsSelect[1]->getSelectedIndex()];
      if ( val == g_pCurrentModel->hardwareInterfacesInfo.serial_port_speed[iCurrentSerialPortIndex] )
         return;

      type_vehicle_hardware_interfaces_info new_info;
      memcpy((u8*)&new_info, (u8*)&(g_pCurrentModel->hardwareInterfacesInfo), sizeof(type_vehicle_hardware_interfaces_info));
      new_info.serial_port_speed[iCurrentSerialPortIndex] = (u32)val;
      log_line("Sending new serial port speed for data link to vehicle.");
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_SERIAL_PORTS_INFO, 0, (u8*)&new_info, sizeof(type_vehicle_hardware_interfaces_info)) )
         valuesToUI();
      return;
   }
}
