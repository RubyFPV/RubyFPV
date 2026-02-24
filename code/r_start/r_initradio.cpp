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
#include "r_initradio.h"
#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hardware_radio.h"
#include "../base/hardware_radio_txpower.h"
#include "../base/hardware_procs.h"
#include "../base/radio_utils.h"
#if defined (HW_PLATFORM_RASPBERRY) || defined (HW_PLATFORM_RADXA)
#include "../base/ctrl_interfaces.h"
#include "../base/ctrl_settings.h"
#endif
#include "../common/string_utils.h"

bool s_bIsStation = false;
bool configure_radios_succeeded = false;
int giDataRateMbAtheros = 0;

void _set_radio_region()
{
   //hw_execute_bash_command("iw reg set DE", NULL);
   //system("iw reg set BO");

   char szOutput[256];
   hw_execute_bash_command_raw("iw reg set 00", szOutput);
}

int init_Radios()
{
   char szComm[128];
   char szOutput[2048];
   configure_radios_succeeded = false;

   log_line("=================================================================");
   log_line("Configuring radios: START.");

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_TEMP_RADIOS_CONFIGURED);
   if( access( szFile, R_OK ) != -1 )
   {
      log_line("Radios already configured. Do nothing.");
      log_line("=================================================================");
      configure_radios_succeeded = true;
      return 1;
   }

   u32 uDelayMS = DEFAULT_DELAY_WIFI_CHANGE;
   s_bIsStation = hardware_is_station();

   #if defined (HW_PLATFORM_RASPBERRY) || defined (HW_PLATFORM_RADXA)

   if ( s_bIsStation )
   {
      log_line("Configuring radios: we are station.");
      load_Preferences();
      load_ControllerInterfacesSettings();

      Preferences* pP = get_Preferences();
      if ( NULL != pP )
         uDelayMS = (u32) pP->iDebugWiFiChangeDelay;
      if ( (uDelayMS < 1) || (uDelayMS > 200) )
         uDelayMS = DEFAULT_DELAY_WIFI_CHANGE;
   }
   else
   {
      log_line("Configuring radios: we are vehicle.");
      reset_ControllerInterfacesSettings();
   }

   #endif

   _set_radio_region();

   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      log_error_and_alarm("No network cards found. Nothing to configure!");
      log_line("=================================================================");
      return 0;
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
      {
         log_error_and_alarm("Failed to get radio info for interface %d.", i+1);
         continue;
      }
      if ( ! pRadioHWInfo->isConfigurable )
      {
         log_line("Configuring radio interface %d (%s): radio interface is not configurable. Skipping it.", i+1, pRadioHWInfo->szName);
         continue;
      }
      if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
      {
         log_line("Configuring radio interface %d (%s): radio interface is SiK radio. Skipping it.", i+1, pRadioHWInfo->szName);
         continue;
      }
      if ( ! hardware_radio_is_wifi_radio(pRadioHWInfo) )
      {
         log_softerror_and_alarm("Radio interface %d is not a wifi radio type. Skipping it.", i+1);
         continue;
      }

      log_line( "Configuring wifi radio interface %d: %s ...", i+1, pRadioHWInfo->szName );
      if ( ! pRadioHWInfo->isSupported )
      {
         log_softerror_and_alarm("Found unsupported wifi radio: %s, skipping.",pRadioHWInfo->szName);
         continue;
      }

      hardware_initialize_radio_interface(i, uDelayMS);

      // Set a default minimum tx power
      if ( hardware_radio_driver_is_rtl8812au_card(pRadioHWInfo->iRadioDriver) )
         hardware_radio_set_txpower_raw_rtl8812au(i, 10);
      if ( hardware_radio_driver_is_rtl8812eu_card(pRadioHWInfo->iRadioDriver) )
         hardware_radio_set_txpower_raw_rtl8812eu(i, 10);

      //if ( s_bIsStation && controllerIsCardDisabled(pRadioHWInfo->szMAC) )
      //{
      //   sprintf(szComm, "ifconfig %s down", pRadioHWInfo->szName );
      //   execute_bash_command(szComm, NULL);
      //}
      hardware_sleep_ms(2*uDelayMS);

      for( int k=0; k<4; k++ )
      {
         //sprintf(szComm, "ifconfig | grep %s", pRadioHWInfo->szName);
         sprintf(szComm, "ip link | grep %s", pRadioHWInfo->szName);
         hw_execute_bash_command(szComm, szOutput);
         removeNewLines(szOutput);
         if ( 0 != szOutput[0] )
         {
            log_line("Radio interface %s state: [%s]", pRadioHWInfo->szName, szOutput);
            break;
         }
         hardware_sleep_ms(50);
      }
   }

   _set_radio_region();

   FILE* fd = fopen(szFile, "w");
   fprintf(fd, "done");
   fclose(fd);

   hardware_save_radio_info();

   log_line("Configuring radios COMPLETED.");
   log_line("=================================================================");
   log_line("Radio interfaces and frequencies assigned:");
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      log_line("   * Radio interface %d: %s on port %s, %s %s %s: %s", i+1, str_get_radio_card_model_string(pRadioHWInfo->iCardModel), pRadioHWInfo->szUSBPort, pRadioHWInfo->szName, pRadioHWInfo->szMAC, pRadioHWInfo->szDescription, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz));
   }
   log_line("=================================================================");
   configure_radios_succeeded = true;
   return 1;
}


int r_initradio(int argc, char *argv[])
{
   if ( argc >= 2 )
      giDataRateMbAtheros = atoi(argv[argc-1]);

   log_init("RubyRadioInit");
   log_arguments(argc, argv);

   hardware_detectBoardAndSystemType();

   char szOutput[1024];
   hw_execute_bash_command_raw("ls /sys/class/net/", szOutput);
   log_line("Network devices found: [%s]", szOutput);

   hardware_enumerate_radio_interfaces();

   int retry = 0;
   while ( 0 == hardware_get_radio_interfaces_count() && retry < 10 )
   {
      hardware_sleep_ms(200);
      hardware_radio_remove_stored_config();
      hardware_reset_radio_enumerated_flag();
      hardware_enumerate_radio_interfaces();
      retry++;
   }

   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      log_error_and_alarm("There are no radio interfaces (2.4/5.8 wlans) on this device.");
      return -1;
   }

   init_Radios();

   log_line("Ruby Init Radio process completed.");
   return (0);
}