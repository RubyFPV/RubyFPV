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

#include "../base/base.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/hardware_procs.h"

#include "forward_watch.h"
#include "shared_vars.h"
#include "warnings.h"
#include "ruby_central.h"
#include "timers.h"

bool s_bForwardPairingStarted = false;
bool s_bForwardHasUSBDevice = false;

u32 s_ForwardLastTimeCheck = 0;

void _forward_on_usb_device_detected()
{
   log_line("USB device detected");

   ruby_pause_watchdog("on new usb device");

   char szSysType[128];
   char szBuff[1024];
   char szOutput[2048];

   if ( hardware_is_vehicle() )
      strcpy(szSysType, "VEHICLE");
   else
      strcpy(szSysType, "STATION");

   hw_stop_process("pump");

   sprintf(szBuff, "pump -i usb0 --no-ntp -h Ruby%s_FWD", szSysType);
   //sprintf(szBuff, "dhclient usb0", szSysType);
   hw_execute_bash_command_raw(szBuff, NULL);

   hardware_sleep_ms(200);
   szOutput[0] = 0;
   hw_execute_bash_command_raw("ip route show 0.0.0.0/0 dev usb0", szOutput);
   //log_line("Output: [%s]", szOutput);
   int len = strlen(szOutput);
   char szIP[32];
   szIP[0] = 0;
   if ( 0 < len )
   {
      char* pIP = szOutput;
      while ( ((*pIP) != 0 ) && ((*pIP) < '0' || (*pIP) > '9') )
         pIP++;
      if ( *pIP != 0 )
         strcpy(szIP, pIP);
      if ( strlen(szIP) > 0 )
      {
         pIP = &szIP[0];
         while ( (*pIP) == '.' || (*pIP >= '0' && (*pIP) <= '9') )
            pIP++;
         *pIP = 0;
      }
   }
   log_line("USB Device IP Address: %s", szIP);

   bool bHasETH = ((hardware_has_eth() != NULL)?true:false);

   if ( ! bHasETH )
      log_line("No ETH port. No further action on USB tethering start.");
   else if ( ( access( "/boot/nodhcp", R_OK ) != -1 ) )
      log_line("DHCP is disabled. No further action on USB tethering start.");
   else
   {
      log_line("DHCP is enabled. Reneable the ETH routing on USB tethering start.");
      sprintf(szBuff, "nice pump -i eth0 --no-ntp -h Ruby%s &", szSysType);
      hw_execute_bash_command(szBuff, NULL);
   }


   sprintf(szBuff, "touch %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_USB_TETHERING_DEVICE);
   hw_execute_bash_command(szBuff, NULL);
   sprintf(szBuff, "echo \"%s\" > %s%s", szIP, FOLDER_RUBY_TEMP, FILE_TEMP_USB_TETHERING_DEVICE);
   hw_execute_bash_command(szBuff, NULL);
   log_line("Done configuring USB tethering device connection.");
   ruby_resume_watchdog("on new usb device");
}

void _forward_on_usb_device_unplugged()
{
   char szSysType[128];
   char szBuff[1024];

   ruby_pause_watchdog("on usb device unplugged");

   if ( hardware_is_vehicle() )
      strcpy(szSysType, "VEHICLE");
   else
      strcpy(szSysType, "STATION");

   bool bHasETH = ((hardware_has_eth() != NULL)?true:false);

   hw_stop_process("pump");

   if ( ! bHasETH )
      log_line("No ETH port. No further action on USB tethering stop.");
   else if ( ( access( "/boot/nodhcp", R_OK ) != -1 ) )
      log_line("DHCP is disabled. No further action on USB tethering stop.");
   else
   {
      log_line("DHCP is enabled. Reneable the ETH routing on USB tethering stop.");
      sprintf(szBuff, "nice pump -i eth0 --no-ntp -h Ruby%s &", szSysType);
      hw_execute_bash_command(szBuff, NULL);
   }
   ruby_resume_watchdog("on usb device unplugged");
}

bool forward_streams_on_pairing_start()
{
   s_bForwardPairingStarted = true;
   s_bForwardHasUSBDevice = false;
   return true;
}

bool forward_streams_on_pairing_stop()
{
   s_bForwardPairingStarted = false;
   s_bForwardHasUSBDevice = false;
   return true;
}

bool forward_streams_is_USB_device_connected()
{
   return s_bForwardHasUSBDevice;
}

void forward_streams_loop()
{
   ControllerSettings* pCS = get_ControllerSettings();
   if ( NULL == pCS )
      return;


   if ( g_TimeNow > s_ForwardLastTimeCheck + 1000 )
   {
      s_ForwardLastTimeCheck = g_TimeNow;

      if ( ! s_bForwardHasUSBDevice )
      {
         if ( access("/sys/class/net/usb0", R_OK ) != -1 )
         {
            warnings_add(0, "USB tethering device detected. Configuring, please wait...");
            render_all(g_TimeNow);
            hardware_sleep_ms(50);
            render_all(g_TimeNow);
            _forward_on_usb_device_detected();
            if ( pCS->iVideoForwardUSBType != 0 )
               warnings_add(0, "Started Video Forward to USB.");
            s_bForwardHasUSBDevice = true;
         }
      }
      else
      {
         if ( access("/sys/class/net/usb0", R_OK ) == -1 )
         {
            warnings_add(0, "USB tethering device unplugged. Please wait...");
            render_all(g_TimeNow);
            hardware_sleep_ms(50);
            render_all(g_TimeNow);
            if ( pCS->iVideoForwardUSBType != 0 )
               warnings_add(0, "Stopped Video Forward to USB.");
            log_line("USB tethering device unplugged.");
            _forward_on_usb_device_unplugged();
            char szBuff[256];
            sprintf(szBuff, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_USB_TETHERING_DEVICE);
            hw_execute_bash_command(szBuff, NULL);
            s_bForwardHasUSBDevice = false;
         }
      }
   }
}

