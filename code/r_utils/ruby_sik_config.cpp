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

#include <stdlib.h>
#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hardware_serial.h"
#include "../base/hardware_radio.h"
#include "../base/hardware_radio_sik.h"
#include "../base/hardware_procs.h"
#include "../base/radio_utils.h"
#include "../base/models.h"
#include "../base/ctrl_interfaces.h"
#include "../base/ctrl_settings.h"
#include "../common/string_utils.h"

void write_result(bool bSucceeded)
{
   char szFile[128];
   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_TEMP_SIK_CONFIG_FINISHED);
   FILE* fd = fopen(szFile, "wb");
   if ( NULL != fd )
   {
      fprintf(fd, "%d\n", (int)bSucceeded);
      fclose(fd);
   }
}

int _setTxPower(int iTxPower)
{
   log_line("Setting tx power to %d...", iTxPower);
   
   bool bFailed = false;

   for( int i=0; i<hardware_serial_get_ports_count(); i++ )
   {
      hw_serial_port_info_t* pSerialPortInfo = hardware_get_serial_port_info(i);
      if ( NULL == pSerialPortInfo )
         continue;
      if ( pSerialPortInfo->iPortUsage != SERIAL_PORT_USAGE_SIK_RADIO )
         continue;
      
      for( int k=0; k<hardware_get_radio_interfaces_count(); k++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(k);
         if ( NULL == pRadioHWInfo )
            continue;
         if ( ! hardware_radio_is_sik_radio(pRadioHWInfo) )
            continue;
         if ( 0 != strcmp(pRadioHWInfo->szDriver, pSerialPortInfo->szPortDeviceName) )
            continue;

         log_line("Setting tx power %d on serial port %d [%s] used on SiK radio interface %d [%s, MAC: %s]...",
             iTxPower, i+1, pSerialPortInfo->szPortDeviceName, k+1, pRadioHWInfo->szName, pRadioHWInfo->szMAC);

         int iSerialPort = hardware_open_serial_port(pSerialPortInfo->szPortDeviceName, pSerialPortInfo->lPortSpeed);
         if ( iSerialPort <= 0 )
         {
            log_error_and_alarm("Failed to open serial port for SiK radio %s at %d bps.", pSerialPortInfo->szPortDeviceName, pSerialPortInfo->lPortSpeed);
            bFailed = true;
            continue;
         }

         // Enter AT mode
         
         if ( ! hardware_radio_sik_enter_command_mode(iSerialPort, pSerialPortInfo->lPortSpeed, NULL) )
         if ( ! hardware_radio_sik_enter_command_mode(iSerialPort, pSerialPortInfo->lPortSpeed, NULL) )
         {
            log_softerror_and_alarm("Failed to enter SiK radio %s into command mode.", pSerialPortInfo->szPortDeviceName);
            close(iSerialPort);
            bFailed = true;
            continue;
         }

         bool bMustSaveFlash = false;
         char szComm[256];
         u8 bufferResponse[1024];
   
         sprintf(szComm, "ATS%d=%d", 4, iTxPower);

         if ( hardware_radio_sik_send_command(iSerialPort, szComm, bufferResponse, 255) )
         {
            //u32 uParam = atoi((char*)bufferResponse);
            //log_line("Did set param %s to value %u", szComm, uParam);
            bMustSaveFlash = true;
         }
         else
         {
            log_softerror_and_alarm("Failed to set SiK %s tx power to %d, response: [%s]",
               pSerialPortInfo->szPortDeviceName, iTxPower, bufferResponse);
            bFailed = true;
         }

         // Exit AT command mode
         
         if ( bMustSaveFlash )
         {
            log_line("Saving SiK params to flash and restarting SiK radio %s...", pSerialPortInfo->szPortDeviceName);
            hardware_radio_sik_save_settings_to_flash(iSerialPort);
         }

         close(iSerialPort);

         break;
      }
   }

   if ( ! bFailed )
   {
      log_line("Ruby SiK configuration completed without errors.");
      write_result(false);
      return 0;
   }

   log_line("Ruby SiK configuration completed with errors.");
   write_result(true);
   return 0;
}

int main(int argc, char *argv[])
{
   char szBuff[256];
   sprintf(szBuff, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_SIK_CONFIG_FINISHED);
   hw_execute_bash_command(szBuff, NULL);

   if ( strcmp(argv[argc-1], "-ver") == 0 )
   {
      printf("%d.%d (b-%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR, SYSTEM_SW_BUILD_NUMBER);
      write_result(false);
      return 0;
   }
   
   if ( argc < 5 )
   {
      printf("Usage: ruby_sik_config [local_serial_port] [local_serial_baudrate] [command+param]\n");
      printf("    command and param can be:\n");
      printf("    -show all / -serialspeed [speed] / -freq [freq] / -power [nb] / -vehicle all)\n");
      write_result(false);
      return 0;
   }

   log_init("RubySiKConfig");

   hardware_serial_reload_ports_settings();
   hardware_enumerate_radio_interfaces();
   hardware_radio_sik_load_configuration();

   bool bFailed = false;
   bool bMustSaveFlash = false;
   bool bSaveSerialPorts = false;
   bool bSaveRadioConfig = false;
   char szCommand[256];
   char szSerialPort[256];
   int iSerialSpeed = 0;
   int iParam1 = 0;

   strcpy(szSerialPort, argv[1]);
   iSerialSpeed = atoi(argv[2]);

   strcpy(szCommand, argv[3]);
   iParam1 = atoi(argv[4]);

   if ( 0 == strcmp(szCommand, "-show") )
      log_enable_stdout();

   if ( 0 == strcmp(szCommand, "-power") )
      return _setTxPower(iParam1);

   int iSerialPort = hardware_open_serial_port(szSerialPort, iSerialSpeed);
   if ( iSerialPort <= 0 )
   {
      log_error_and_alarm("Failed to open serial port for SiK radio %s at %d bps.", szSerialPort, iSerialSpeed);
      write_result(false);
      return 0;
   }

   // Enter AT mode
   
   if ( ! hardware_radio_sik_enter_command_mode(iSerialPort, iSerialSpeed, NULL) )
   if ( ! hardware_radio_sik_enter_command_mode(iSerialPort, iSerialSpeed, NULL) )
   {
      log_softerror_and_alarm("Failed to enter SiK radio into command mode.");
      close(iSerialPort);
      write_result(false);
      return 0;
   }

   u8 bufferResponse[1024];

   // Send params

   char szComm[32];
   
   if ( 0 == strcmp(szCommand, "-show") )
   {
      if ( hardware_radio_sik_send_command(iSerialPort, "ATI", bufferResponse, 255) )
      {
         bufferResponse[62] = 0;
         log_line("ATI: [%s]", (char*)bufferResponse);
      }
      else
         log_softerror_and_alarm("Failed to send ATI command.");

      int iMaxParam = 15;
      if ( iMaxParam > MAX_RADIO_HW_PARAMS )
         iMaxParam = MAX_RADIO_HW_PARAMS;

      for( int i=0; i<iMaxParam; i++ )
      {
         sprintf(szComm, "ATS%d?", i);
         if ( hardware_radio_sik_send_command(iSerialPort, szComm, bufferResponse, 255) )
         {
            u32 uParam = atoi((char*)bufferResponse);
            log_line("Did got param %s, value %u", szComm, uParam);
         }
         else
            log_softerror_and_alarm("Failed to get param: %s", szComm); 
      }
   }

   if ( 0 == strcmp(szCommand, "-serialspeed") )
   {
      sprintf(szComm, "ATS%d=%d", 1, hardware_radio_sik_get_encoded_serial_baudrate(iParam1));
      bufferResponse[0] = 0;

      if ( hardware_radio_sik_send_command(iSerialPort, szComm, bufferResponse, 255) )
      {
         if ( (NULL != strstr((char*)bufferResponse, "ERROR")) ||
              ((NULL == strstr((char*)bufferResponse, "OK")) &&
               (NULL == strstr((char*)bufferResponse, "ok")) &&
               (NULL == strstr((char*)bufferResponse, "Ok")) &&
               (NULL == strstr((char*)bufferResponse, "oK"))
               ) )
         {
             log_softerror_and_alarm("Failed to set serial port speed on the SiK radio.");
             bFailed = true;
         }
         else
         {
            bMustSaveFlash = true;
            hw_serial_port_info_t* pSerialPort = hardware_get_serial_port_info_from_serial_port_name(szSerialPort);
            if ( NULL != pSerialPort )
            {
               pSerialPort->lPortSpeed = (long) iParam1;
               bSaveSerialPorts = true;
            }
            else
            {
                log_softerror_and_alarm("Failed to get serial port hardware info for serial port [%s].", szSerialPort);
                bFailed = true;
            }

            radio_hw_info_t* pRadioInfo = hardware_radio_sik_get_from_serial_port(szSerialPort);
            if ( NULL != pRadioInfo )
            {
               pRadioInfo->uHardwareParamsList[1] = (u32)iParam1;
               bSaveRadioConfig = true;
            }
            else
            {
               log_softerror_and_alarm("Failed to get SiK radio info for serial port [%s].", szSerialPort);
               bFailed = true;
            }
         }
      }
      else
      {
         log_softerror_and_alarm("Failed to set SiK %s to %d, response: [%s]", szComm, iParam1, bufferResponse);
         bFailed = true;
      }
   }

   // Exit AT command mode
   
   if ( bMustSaveFlash )
   {
      log_line("Saving SiK params to flash and restarting SiK radio...");
      hardware_radio_sik_save_settings_to_flash(iSerialPort);
   }

   close(iSerialPort);

   if ( bSaveSerialPorts )
      hardware_serial_save_configuration();

   if ( bSaveRadioConfig )
   {
      hardware_radio_sik_save_configuration();
      hardware_save_radio_info();
   }

   if ( ! bFailed )
   {
      log_line("Ruby SiK configuration completed without errors.");
      write_result(false);
      return 0;
   }

   log_line("Ruby SiK configuration completed with errors.");
   write_result(true);
   return 0;
} 