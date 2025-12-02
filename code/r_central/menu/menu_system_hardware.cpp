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
#include "menu_text.h"
#include "menu_system_hardware.h"
#include "../../radio/radiolink.h"
#include "../launchers_controller.h"

#include <sys/types.h>
#include <dirent.h>
#include <string.h>

static bool s_bFirstTimeI2CDetectionMenuSystemDone = false;
static int s_nMenuSystemI2CDevices[128];

MenuSystemHardware::MenuSystemHardware(void)
:Menu(MENU_ID_SYSTEM_HARDWARE, "All System Devices and Peripherals (controller + vehicle)", NULL)
{
   m_Width = 0.64;
   m_Height = 0.7;
   m_xPos = 0.12;
   m_yPos = 0.14;
   m_nSearchI2CDeviceAddress = 0;
   s_nMenuSystemI2CDevices[0] = 0;

   load_ControllerInterfacesSettings();
   load_ControllerSettings();
   controllerInterfacesEnumJoysticks();

   setColumnsCount(2);
   enableColumnSelection(false);

   m_IndexGetControllerUSBInfo = addMenuItem(new MenuItem("Get controller USB info", "Get info from controller about all USB radio interfaces that where found on the controller."));
   m_pMenuItems[m_IndexGetControllerUSBInfo]->showArrow();

   m_IndexGetVehicleUSBInfo = addMenuItem(new MenuItem("Get vehicle USB info", "Get info from vehicle about all USB radio interfaces that where found on the vehicle."));
   m_pMenuItems[m_IndexGetVehicleUSBInfo]->showArrow();
}

void MenuSystemHardware::onShow()
{
   Menu::onShow();
}


bool MenuSystemHardware::periodicLoop()
{
   if ( s_bFirstTimeI2CDetectionMenuSystemDone )
      return false;

   if ( m_nSearchI2CDeviceAddress >= 128 )
      return false;
   if ( m_bIsAnimationInProgress )
      return false;

   for( int i=0; i<10; i++ )
   {
      if ( m_nSearchI2CDeviceAddress >= 128 )
         break;

      if ( hardware_is_known_i2c_device(m_nSearchI2CDeviceAddress) )
      if ( hardware_i2c_has_device_id(m_nSearchI2CDeviceAddress) )
      {
         s_nMenuSystemI2CDevices[m_nSearchI2CDeviceAddress] = m_nSearchI2CDeviceAddress;
         hardware_i2c_get_device_settings((u8)m_nSearchI2CDeviceAddress);
      }
      m_nSearchI2CDeviceAddress++;
   }
   
   if ( m_nSearchI2CDeviceAddress >= 128 )
   {
      hardware_i2c_save_device_settings();
      invalidate();
      s_bFirstTimeI2CDetectionMenuSystemDone = true;
   }
   return true;
}

void MenuSystemHardware::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   RenderItem(0,y);
   y += RenderItem(1, y, getUsableWidth()*0.9 + 4*m_sfMenuPaddingX);

   char szBuff[128];

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu); 

   y += height_text;
   float fMargin = m_sfMenuPaddingX;
   float xPos = m_xPos+fMargin;
   float yPos = y;
   float usableWidth = m_RenderWidth-2*fMargin;
   
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText(), 0.3);

   if ( ! s_bFirstTimeI2CDetectionMenuSystemDone )
   {
      yPos += height_text;
      int percent = m_nSearchI2CDeviceAddress*100/128;
      if ( percent > 100 ) percent = 100;
      sprintf(szBuff, "Enumerating I2C devices %d%%", percent);   
      g_pRenderEngine->drawText(xPos, yPos, g_idFontMenu, szBuff);
      yPos += height_text*1.2;
      g_pRenderEngine->drawText(xPos, yPos, g_idFontMenu, "Please wait...");
      yPos += 1.9*height_text;
      g_pRenderEngine->setColors(get_Color_MenuText());
      return;
   }

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(1.01);

   float yPosCol2 = yPos;
   float yStLine = yPos;   
   
   yPos += renderControllerInfo(xPos, yPos, usableWidth*0.5 - fMargin );
   yPosCol2 += renderVehicleInfo(xPos + usableWidth*0.5 + fMargin, yPosCol2, usableWidth*0.5 - fMargin );

   float yMax = yPosCol2;
   if ( yPos > yMax )
      yMax = yPos;

   g_pRenderEngine->setStrokeSize(1.01);
   g_pRenderEngine->drawLine(xPos+ usableWidth*0.5 - 0.01/g_pRenderEngine->getAspectRatio(), yStLine, xPos+usableWidth*0.5 - 0.01/g_pRenderEngine->getAspectRatio(), yMax);

   RenderEnd(yTop);
}


float MenuSystemHardware::renderVehicleInfo(float xPos, float yPos, float width)
{
   char szBuff[256];
   float y0 = yPos;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float xPad = 0.02*m_sfScaleFactor;

   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, "VEHICLE:", MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += 3.0*MENU_TEXTLINE_SPACING * height_text;

   if ( NULL == g_pCurrentModel )
   {
      yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, "Not connected to a vehicle.\nNo vehicle info to show.", MENU_TEXTLINE_SPACING, width, g_idFontMenu);
      yPos += MENU_TEXTLINE_SPACING * height_text;

      g_pRenderEngine->setColors(get_Color_MenuText());
      yPos += MENU_TEXTLINE_SPACING * height_text;
      return yPos-y0;
   }

   //--------------------------------------
   // Radio interfaces

   sprintf(szBuff, "Radio Interfaces: %d found", g_pCurrentModel->radioInterfacesParams.interfaces_count);
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      char szBands[128];
      str_get_supported_bands_string(g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i], szBands);
       
      sprintf(szBuff, "%d. Port %s, Type: %s, %s", i+1, g_pCurrentModel->radioInterfacesParams.interface_szPort[i], str_get_radio_card_model_string(g_pCurrentModel->radioInterfacesParams.interface_card_model[i]), szBands);
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         strcat(szBuff, " [RELAY LINK]");

      yPos += g_pRenderEngine->drawMessageLines(xPos+xPad, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
      yPos += MENU_TEXTLINE_SPACING * height_text;
   }

   yPos += MENU_TEXTLINE_SPACING * height_text;
   g_pRenderEngine->drawLine(xPos, yPos, xPos+width - xPad, yPos);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   // ----------------------
   // Serial ports

   sprintf(szBuff, "Serial Ports: %d found", g_pCurrentModel->hardwareInterfacesInfo.serial_port_count);
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   for( int i=0; i<g_pCurrentModel->hardwareInterfacesInfo.serial_port_count; i++ )
   {
      sprintf( szBuff, "%d. %s, Usage: %s, %d bps", i+1, g_pCurrentModel->hardwareInterfacesInfo.serial_port_names[i], str_get_serial_port_usage((int)(g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & 0xFF)), g_pCurrentModel->hardwareInterfacesInfo.serial_port_speed[i]);
      if ( ( g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & MODEL_SERIAL_PORT_BIT_SUPPORTED ) == 0 )
         sprintf( szBuff, "%d. %s, Unsupported!", i+1, g_pCurrentModel->hardwareInterfacesInfo.serial_port_names[i]);
      yPos += g_pRenderEngine->drawMessageLines(xPos+xPad, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
      yPos += MENU_TEXTLINE_SPACING * height_text;
   }

   yPos += MENU_TEXTLINE_SPACING * height_text;
   g_pRenderEngine->drawLine(xPos, yPos, xPos+width - xPad, yPos);
   yPos += MENU_TEXTLINE_SPACING * height_text;


   //--------------------------------------
   // I2C

   sprintf(szBuff, "I2C Devices and Busses:");
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   for( int i=0; i<g_pCurrentModel->hardwareInterfacesInfo.i2c_bus_count; i++ )
   {
      int nBusNumber = g_pCurrentModel->hardwareInterfacesInfo.i2c_bus_numbers[i];
      int count = 0;
      for( int k=0; k<g_pCurrentModel->hardwareInterfacesInfo.i2c_device_count; k++ )
      {
         if ( g_pCurrentModel->hardwareInterfacesInfo.i2c_devices_bus[k] != nBusNumber )
            continue;
         count++;
      }

      sprintf(szBuff, "I2C Bus %d:", nBusNumber);
      if ( 0 == count )
         sprintf(szBuff, "I2C Bus %d: No devices on this bus.", nBusNumber);

      yPos += g_pRenderEngine->drawMessageLines(xPos+xPad, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
      yPos += MENU_TEXTLINE_SPACING * height_text;
      
      count = 0;
      for( int k=0; k<g_pCurrentModel->hardwareInterfacesInfo.i2c_device_count; k++ )
      {
         if ( g_pCurrentModel->hardwareInterfacesInfo.i2c_devices_bus[k] != nBusNumber )
            continue;
         char szName[128];
         szName[0] = 0;
         hardware_i2c_get_device_name(g_pCurrentModel->hardwareInterfacesInfo.i2c_devices_address[k], szName);
         if ( hardware_is_known_i2c_device(g_pCurrentModel->hardwareInterfacesInfo.i2c_devices_address[k]) )
            sprintf(szBuff, "Address 0x%02X - %s", g_pCurrentModel->hardwareInterfacesInfo.i2c_devices_address[k], szName);
         else
            sprintf(szBuff, "Address 0x%02X - Unknown", g_pCurrentModel->hardwareInterfacesInfo.i2c_devices_address[k]);
         yPos += g_pRenderEngine->drawMessageLines(xPos+2*xPad, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
         count++;
      }
      yPos += MENU_TEXTLINE_SPACING * height_text;
   }
   yPos += MENU_TEXTLINE_SPACING * height_text;

   g_pRenderEngine->setColors(get_Color_MenuText());

   yPos += MENU_TEXTLINE_SPACING * height_text;
   return yPos-y0;
}

float MenuSystemHardware::renderControllerInfo(float xPos, float yPos, float width)
{
   float y0 = yPos;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float xPad = 0.02*m_sfScaleFactor;

   char szBuff[256];
   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();

   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, "CONTROLLER:", MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   //-----------------------------------
   // Radio cards


   sprintf(szBuff, "Radio Interfaces: %d found", hardware_get_radio_interfaces_count());
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioInfo )
      {
         sprintf(szBuff, "%d. Can't get radio info.", i+1);
         yPos += g_pRenderEngine->drawMessageLines(xPos+xPad, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
         yPos += MENU_TEXTLINE_SPACING * height_text;
         continue;
      }

      char szBands[128];
      str_get_supported_bands_string(pRadioInfo->supportedBands, szBands);
       
      sprintf(szBuff, "%d. Port %s, Type: %s, %s", i+1, pRadioInfo->szUSBPort, str_get_radio_card_model_string(pRadioInfo->iCardModel), szBands);
       
      yPos += g_pRenderEngine->drawMessageLines(xPos+xPad, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
      yPos += MENU_TEXTLINE_SPACING * height_text;
   }

   yPos += MENU_TEXTLINE_SPACING * height_text;
   g_pRenderEngine->drawLine(xPos, yPos, xPos+width - xPad, yPos);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   //-----------------------------------------
   // Serial ports

   sprintf(szBuff, "Serial Ports: %d found", hardware_serial_get_ports_count());
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   for( int i=0; i<hardware_serial_get_ports_count(); i++ )
   {
      hw_serial_port_info_t* pInfo = hardware_get_serial_port_info(i);
      if ( NULL == pInfo )
         sprintf(szBuff, "%d. Serial Port: Failed to get info.", i+1);
      else
      {
         sprintf( szBuff, "%d. %s, Usage: %s, %d bps", i+1, pInfo->szName, str_get_serial_port_usage(pInfo->iPortUsage), (int)pInfo->lPortSpeed);
         if ( pInfo->iSupported == 0 )
            sprintf( szBuff, "%d. %s, Unsupported!", i+1, pInfo->szName);
      }
      yPos += g_pRenderEngine->drawMessageLines(xPos+xPad, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
      yPos += MENU_TEXTLINE_SPACING * height_text;
   }
   yPos += MENU_TEXTLINE_SPACING * height_text;
   g_pRenderEngine->drawLine(xPos, yPos, xPos+width - xPad, yPos);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   //----------------------------------------
   // I2C

   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, "I2C Devices and Busses:", MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   for( int i=0; i<hardware_i2c_get_busses_count(); i++ )
   {
      hw_i2c_bus_info_t* pBus = hardware_i2c_get_bus_info(i);

      int count = 0;
      for( int k=1; k<128; k++ )
      {
         if ( ! hardware_i2c_has_device_id(k) )
            continue;
         if ( hardware_i2c_get_device_bus_number(k) != pBus->nBusNumber )
            continue;
         count++;
      }

      sprintf(szBuff, "I2C Bus %d, %s:", pBus->nBusNumber, pBus->szName);
      if ( 0 == count )
         sprintf(szBuff, "I2C Bus %d, %s: No devices on this bus.", pBus->nBusNumber, pBus->szName);
      yPos += g_pRenderEngine->drawMessageLines(xPos+xPad, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
      yPos += MENU_TEXTLINE_SPACING * height_text;

      count = 0;
      for( int k=1; k<128; k++ )
      {
         if ( ! hardware_i2c_has_device_id(k) )
            continue;
         if ( hardware_i2c_get_device_bus_number(k) != pBus->nBusNumber )
            continue;
         t_i2c_device_settings* pInfo = hardware_i2c_get_device_settings((u8)k);
         if ( NULL != pInfo )
            sprintf(szBuff, "Address 0x%02X - %s", k, pInfo->szDeviceName);
         else
            sprintf(szBuff, "Address 0x%02X - Unknown", k);
         yPos += g_pRenderEngine->drawMessageLines(xPos+2*xPad, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
         count++;
      }
      yPos += MENU_TEXTLINE_SPACING * height_text;
   }

   yPos += MENU_TEXTLINE_SPACING * height_text;
   g_pRenderEngine->drawLine(xPos, yPos, xPos+width - xPad, yPos);
   yPos += MENU_TEXTLINE_SPACING * height_text;
   sprintf(szBuff, "HID (Joystick) Interfaces: %d", pCI->inputInterfacesCount);
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   for( int i=0; i<pCI->inputInterfacesCount; i++ )
   {
      t_ControllerInputInterface* pCII = controllerInterfacesGetAt(i);
      if ( NULL == pCII )
         continue;
      sprintf( szBuff, "%d. Name: %s", i+1, pCII->szInterfaceName );
      yPos += g_pRenderEngine->drawMessageLines(xPos+xPad, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
      yPos += MENU_TEXTLINE_SPACING * height_text;
   }
   if ( 0 == pCI->inputInterfacesCount )
   {
      yPos += g_pRenderEngine->drawMessageLines(xPos+xPad, yPos, "No RC input devices detected.", MENU_TEXTLINE_SPACING, width, g_idFontMenu);
      yPos += MENU_TEXTLINE_SPACING * height_text;
   }    

   g_pRenderEngine->setColors(get_Color_MenuText());

   yPos += 2.0*MENU_TEXTLINE_SPACING * height_text;
   return yPos - y0;
}

void MenuSystemHardware::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( m_IndexGetVehicleUSBInfo == m_SelectedIndex )
   {
      handle_commands_send_to_vehicle(COMMAND_ID_GET_USB_INFO, 0, NULL, 0);
      return;
   }

   if ( m_IndexGetControllerUSBInfo == m_SelectedIndex )
   {
      Menu* pMenu = new Menu(0, "Controller USB Radio Interfaces Info",NULL);
      pMenu->m_xPos = 0.18;
      pMenu->m_yPos = 0.15;
      pMenu->m_Width = 0.56;
      add_menu_to_stack(pMenu);

      char szBuff[6000];
      char szBuff2[3000];
      szBuff[0] = 0;
      szBuff2[0] = 0;
      hw_execute_bash_command_raw("lsusb", szBuff);
      hw_execute_bash_command_raw("lsusb -t", szBuff2);
      int iLen1 = strlen(szBuff);
      if ( iLen1 > 3000 )
         iLen1 = 3000;
      strcat(szBuff, "\nTree:\n");
      strcat(szBuff, szBuff2);
      iLen1 = strlen(szBuff);


      int iPos = 0;
      int iStartLine = iPos;
      while (iPos < iLen1 )
      {
         if ( szBuff[iPos] == 10 || szBuff[iPos] == 13 || szBuff[iPos] == 0 )
         {
            szBuff[iPos] = 0;
            if ( iPos > iStartLine + 2 )
               pMenu->addTopLine(szBuff+iStartLine);
            iStartLine = iPos+1;
         }
         iPos++;
      }
      szBuff[iPos] = 0;
      if ( iPos > iStartLine + 2 )
        pMenu->addTopLine(szBuff+iStartLine);


      // Second part

      DIR *d;
      struct dirent *dir;
      char szFile[1024];
      char szComm[1024];
      char szOutput[1024];
      szBuff[0] = 0;

      d = opendir("/sys/bus/usb/devices/");
      if (d)
      {
         while ((dir = readdir(d)) != NULL)
         {
            int iLen = strlen(dir->d_name);
            if ( iLen < 3 )
               continue;
            snprintf(szFile, 1023, "/sys/bus/usb/devices/%s", dir->d_name);
            snprintf(szComm, 1023, "cat /sys/bus/usb/devices/%s/uevent | grep DRIVER", dir->d_name);
            szOutput[0] = 0;
            hw_execute_bash_command(szComm, szOutput);
            strcat(szBuff, dir->d_name);
            strcat(szBuff, " :  ");
            strcat(szBuff, szOutput);

            for( int i=0; i<hardware_get_radio_interfaces_count()+1; i++ )
            {
               szOutput[0] = 0;
               sprintf(szComm, "cat /sys/bus/usb/devices/%s/net/wlan%d/uevent 2>/dev/null | grep DEVTYPE=wlan", dir->d_name, i);
               hw_execute_bash_command_silent(szComm, szOutput);
               if ( strlen(szOutput) > 0 )
               {
                  log_line("Accessed %s", szComm);
                  szOutput[0] = 0;
                  sprintf(szComm, "cat /sys/bus/usb/devices/%s/net/wlan%d/uevent 2>/dev/null | grep INTERFACE", dir->d_name, i);
                  hw_execute_bash_command_silent(szComm, szOutput);
                  if ( strlen(szOutput) > 0 )
                  {
                     strcat(szBuff, ", ");
                     strcat(szBuff, szOutput);
                     iLen = strlen(szBuff);
                     if ( szBuff[iLen-1] == 10 || szBuff[iLen-1] == 13 )
                        szBuff[iLen-1] = 0;
                     iLen = strlen(szBuff);
                     if ( szBuff[iLen-1] == 10 || szBuff[iLen-1] == 13 )
                        szBuff[iLen-1] = 0;
                  }
                  szOutput[0] = 0;
                  sprintf(szComm, "cat /sys/bus/usb/devices/%s/net/wlan%d/uevent 2>/dev/null | grep IFINDEX", dir->d_name, i);
                  hw_execute_bash_command_silent(szComm, szOutput);
                  if ( strlen(szOutput) > 0 )
                  {
                     strcat(szBuff, ", ");
                     strcat(szBuff, szOutput);
                     iLen = strlen(szBuff);
                     if ( szBuff[iLen-1] == 10 || szBuff[iLen-1] == 13 )
                        szBuff[iLen-1] = 0;
                     iLen = strlen(szBuff);
                     if ( szBuff[iLen-1] == 10 || szBuff[iLen-1] == 13 )
                        szBuff[iLen-1] = 0;
                  }

                  szOutput[0] = 0;
                  sprintf(szComm, "cat /sys/bus/usb/devices/%s/uevent 2>/dev/null | grep PRODUCT", dir->d_name );
                  hw_execute_bash_command_silent(szComm, szOutput);
                  if ( strlen(szOutput) > 0 )
                  {
                     strcat(szBuff, ", ");
                     strcat(szBuff, szOutput);
                     iLen = strlen(szBuff);
                     if ( szBuff[iLen-1] == 10 || szBuff[iLen-1] == 13 )
                        szBuff[iLen-1] = 0;
                     iLen = strlen(szBuff);
                     if ( szBuff[iLen-1] == 10 || szBuff[iLen-1] == 13 )
                        szBuff[iLen-1] = 0;
                  }
                  break;
               }
            }

            iLen = strlen(szBuff);
            if ( iLen > 0 )
            if ( szBuff[iLen-1] == 10 || szBuff[iLen-1] == 13 )
               szBuff[iLen-1] = 0;

            iLen = strlen(szBuff);
            if ( iLen > 0 )
            if ( szBuff[iLen-1] == 10 || szBuff[iLen-1] == 13 )
               szBuff[iLen-1] = 0;

            strcat(szBuff, "\n");
         }
         closedir(d);
      }

      if ( 0 == strlen(szBuff) )
      {
         pMenu->addTopLine("No info extended available.");
         return;
      }
      pMenu->addTopLine("List:");
      int iLength = strlen(szBuff);

      iPos = 0;
      iStartLine = iPos;
      while (iPos < iLength )
      {
         if ( szBuff[iPos] == 10 || szBuff[iPos] == 13 || szBuff[iPos] == 0 )
         {
            szBuff[iPos] = 0;
            if ( iPos > iStartLine + 2 )
               pMenu->addTopLine(szBuff+iStartLine);
            iStartLine = iPos+1;
         }
         iPos++;
      }
      szBuff[iPos] = 0;
      if ( iPos > iStartLine + 2 )
        pMenu->addTopLine(szBuff+iStartLine);

      return;
   }
}

