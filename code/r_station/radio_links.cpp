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

#include "radio_links.h"
#include "timers.h"
#include "shared_vars.h"
#include "../base/ctrl_interfaces.h"
#include "../base/ctrl_preferences.h"
#include "../base/hardware_radio.h"
#include "../base/hardware_radio_sik.h"
#include "../base/hardware_radio_serial.h"
#include "../base/hardware_procs.h"
#include "../base/radio_utils.h"
#include "../common/string_utils.h"
#include "../common/radio_stats.h"
#include "../radio/radio_rx.h"
#include "../radio/radio_tx.h"
#include "../utils/utils_controller.h"

#include "packets_utils.h"
#include "ruby_rt_station.h"

int s_iFailedInitRadioInterface = -1;
u32 s_uTimeLastCheckedAuxiliaryLinks = 0;
fd_set s_RadioAuxiliaryRxReadSet;

u32 s_uTimeLastSetRadioLinksMonitorMode = 0;

int radio_links_has_failed_interfaces()
{
   return s_iFailedInitRadioInterface;
}

void radio_links_reinit_radio_interfaces()
{
   char szComm[256];
   
   radio_links_close_rxtx_radio_interfaces();
   
   send_alarm_to_central(ALARM_ID_GENERIC_STATUS_UPDATE, ALARM_FLAG_GENERIC_STATUS_RECONFIGURING_RADIO_INTERFACE, 0);

   hardware_radio_remove_stored_config();
   
   hw_execute_bash_command("/etc/init.d/udev restart", NULL);
   hardware_sleep_ms(200);
   hw_execute_bash_command("sudo systemctl restart networking", NULL);
   hardware_sleep_ms(200);
   //hw_execute_bash_command("sudo ifconfig -a", NULL);
   hw_execute_bash_command("sudo ip link", NULL);

   hardware_sleep_ms(50);

   hw_execute_bash_command("sudo systemctl stop networking", NULL);
   hardware_sleep_ms(200);
   //hw_execute_bash_command("sudo ifconfig -a", NULL);
   hw_execute_bash_command("sudo ip link", NULL);

   hardware_sleep_ms(50);

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

   char szOutput[4096];
   szOutput[0] = 0;
   //hw_execute_bash_command_raw("sudo ifconfig -a | grep wlan", szOutput);
   hw_execute_bash_command_raw("sudo ip link | grep wlan", NULL);

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

   log_line("Reinitializing radio interfaces: found interfaces on ip link: [%s]", szOutput);
   hardware_radio_remove_stored_config();
   
   //hw_execute_bash_command("ifconfig wlan0 down", NULL);
   //hw_execute_bash_command("ifconfig wlan1 down", NULL);
   //hw_execute_bash_command("ifconfig wlan2 down", NULL);
   //hw_execute_bash_command("ifconfig wlan3 down", NULL);
   hw_execute_bash_command("ip link set dev wlan0 down", NULL);
   hw_execute_bash_command("ip link set dev wlan1 down", NULL);
   hw_execute_bash_command("ip link set dev wlan2 down", NULL);
   hw_execute_bash_command("ip link set dev wlan3 down", NULL);
   hardware_sleep_ms(200);

   //hw_execute_bash_command("ifconfig wlan0 up", NULL);
   //hw_execute_bash_command("ifconfig wlan1 up", NULL);
   //hw_execute_bash_command("ifconfig wlan2 up", NULL);
   //hw_execute_bash_command("ifconfig wlan3 up", NULL);
   hw_execute_bash_command("ip link set dev wlan0 up", NULL);
   hw_execute_bash_command("ip link set dev wlan1 up", NULL);
   hw_execute_bash_command("ip link set dev wlan2 up", NULL);
   hw_execute_bash_command("ip link set dev wlan3 up", NULL);
   
   sprintf(szComm, "rm -rf %s%s", FOLDER_CONFIG, FILE_CONFIG_CURRENT_RADIO_HW_CONFIG);
   hw_execute_bash_command(szComm, NULL);
   
   // Remove radio initialize file flag
   sprintf(szComm, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_RADIOS_CONFIGURED);
   hw_execute_bash_command(szComm, NULL);

   radio_links_set_monitor_mode();

   char szCommRadioParams[64];
   strcpy(szCommRadioParams, "-initradio");
   for ( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( (g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[i] & 0xFF) == RADIO_TYPE_ATHEROS )
      if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] >= 0 )
      if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] < g_pCurrentModel->radioLinksParams.links_count )
      {
         int dataRateMb = g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[g_pCurrentModel->radioInterfacesParams.interface_link_id[i]];
         if ( dataRateMb > 0 )
            dataRateMb = dataRateMb / 1000 / 1000;
         if ( dataRateMb > 0 )
         {
            sprintf(szCommRadioParams, "-initradio %d", dataRateMb);
            break;
         }
      }
   }

   hw_execute_ruby_process_wait(NULL, "ruby_start", szCommRadioParams, NULL, 1);
   
   hardware_sleep_ms(100);
   hardware_reset_radio_enumerated_flag();
   hardware_enumerate_radio_interfaces();

   hardware_save_radio_info();
   hardware_sleep_ms(100);
 
   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

   log_line("=================================================================");
   log_line("Detected hardware radio interfaces:");
   hardware_log_radio_info(NULL, 0);

   radio_links_open_rxtx_radio_interfaces();

   send_alarm_to_central(ALARM_ID_GENERIC_STATUS_UPDATE, ALARM_FLAG_GENERIC_STATUS_RECONFIGURED_RADIO_INTERFACE, 0);
}

void radio_links_close_rxtx_radio_interfaces()
{
   log_line("Closing all radio interfaces (rx/tx).");

   radio_tx_mark_quit();
   hardware_sleep_ms(10);
   radio_tx_stop_tx_thread();

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
         hardware_radio_sik_close(i);
      else if ( hardware_radio_is_serial_radio(pRadioHWInfo) )
         hardware_radio_serial_close(i);
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( pRadioHWInfo->openedForWrite )
         radio_close_interface_for_write(i);
   }

   radio_close_interfaces_for_read();

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      g_SM_RadioStats.radio_interfaces[i].openedForRead = 0;
      g_SM_RadioStats.radio_interfaces[i].openedForWrite = 0;
   }
   if ( NULL != g_pSM_RadioStats )
      memcpy((u8*)g_pSM_RadioStats, (u8*)&g_SM_RadioStats, sizeof(shared_mem_radio_stats));
   log_line("Closed all radio interfaces (rx/tx)."); 
}


void radio_links_open_rxtx_radio_interfaces_for_search( u32 uSearchFreq )
{
   log_line("");
   log_line("OPEN RADIO INTERFACES START =========================================================");
   log_line("Opening RX radio interfaces for search (%s), firmware: %s ...", str_format_frequency(uSearchFreq), str_format_firmware_type(g_uAcceptedFirmwareType));

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      g_SM_RadioStats.radio_interfaces[i].openedForRead = 0;
      g_SM_RadioStats.radio_interfaces[i].openedForWrite = 0;
   }

   radio_links_set_monitor_mode();

   s_iFailedInitRadioInterface = -1;

   int iCountOpenRead = 0;
   int iCountSikInterfacesOpened = 0;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      u32 flags = controllerGetCardFlags(pRadioHWInfo->szMAC);
      if ( (flags & RADIO_HW_CAPABILITY_FLAG_DISABLED) || controllerIsCardDisabled(pRadioHWInfo->szMAC) )
         continue;

      if ( ! hardware_radio_is_elrs_radio(pRadioHWInfo) )
      if ( 0 == hardware_radio_supports_frequency(pRadioHWInfo, uSearchFreq ) )
         continue;

      if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
      {
         if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
         {
            if ( hardware_radio_sik_open_for_read_write(i) <= 0 )
               s_iFailedInitRadioInterface = i;
            else
            {
               g_SM_RadioStats.radio_interfaces[i].openedForRead = 1;
               iCountOpenRead++;
               iCountSikInterfacesOpened++;
            }
         }
         else if ( hardware_radio_is_elrs_radio(pRadioHWInfo) )
         {
            if ( hardware_radio_serial_open_for_read_write(i) <= 0 )
               s_iFailedInitRadioInterface = i;
            else
            {
               g_SM_RadioStats.radio_interfaces[i].openedForRead = 1;
               iCountOpenRead++;
               iCountSikInterfacesOpened++;
            }          
         }
         else
         {
            int iRes = radio_open_interface_for_read(i, RADIO_PORT_ROUTER_DOWNLINK);
              
            if ( iRes > 0 )
            {
               log_line("Opened radio interface %d for read: USB port %s %s %s", i+1, pRadioHWInfo->szUSBPort, str_get_radio_type_description(pRadioHWInfo->iRadioType), pRadioHWInfo->szMAC);
               g_SM_RadioStats.radio_interfaces[i].openedForRead = 1;
               iCountOpenRead++;
            }
            else
               s_iFailedInitRadioInterface = i;
         }
      }
   }
   
   if ( 0 < iCountSikInterfacesOpened )
   {
      radio_tx_set_sik_packet_size(g_pCurrentModel->radioLinksParams.iSiKPacketSize);
      radio_tx_start_tx_thread();
   }

   // While searching, all cards are on same frequency, so a single radio link assigned to them.
   int iRadioLink = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_SM_RadioStats.radio_interfaces[i].openedForRead )
      {
         g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId = iRadioLink;
         g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId = iRadioLink;
         //iRadioLink++;
      }
   }
   
   if ( NULL != g_pSM_RadioStats )
      memcpy((u8*)g_pSM_RadioStats, (u8*)&g_SM_RadioStats, sizeof(shared_mem_radio_stats));
   log_line("Opening RX radio interfaces for search complete. %d interfaces opened for RX:", iCountOpenRead);
   
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_SM_RadioStats.radio_interfaces[i].openedForRead )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
            log_line("   * Radio interface %d, name: %s opened for read.", i+1, "N/A");
         else
            log_line("   * Radio interface %d, name: %s opened for read.", i+1, pRadioHWInfo->szName);
      }
   }
   log_line("OPEN RADIO INTERFACES END =====================================================================");
   log_line("");
   radio_links_set_monitor_mode();
}

void radio_links_open_rxtx_radio_interfaces()
{
   log_line("");
   log_line("OPEN RADIO INTERFACES START =========================================================");

   if ( g_bSearching || (NULL == g_pCurrentModel) )
   {
      log_error_and_alarm("Invalid parameters for opening radio interfaces");
      return;
   }

   radio_links_set_monitor_mode();

   log_line("Opening RX/TX radio interfaces for current vehicle (firmware: %s)...", str_format_firmware_type(g_pCurrentModel->getVehicleFirmwareType()));

   int totalCountForRead = 0;
   int totalCountForWrite = 0;
   s_iFailedInitRadioInterface = -1;

   int countOpenedForReadForRadioLink[MAX_RADIO_INTERFACES];
   int countOpenedForWriteForRadioLink[MAX_RADIO_INTERFACES];
   int iCountSikInterfacesOpened = 0;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      countOpenedForReadForRadioLink[i] = 0;
      countOpenedForWriteForRadioLink[i] = 0;
      g_SM_RadioStats.radio_interfaces[i].openedForRead = 0;
      g_SM_RadioStats.radio_interfaces[i].openedForWrite = 0;
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);

      if ( (NULL == pRadioHWInfo) || controllerIsCardDisabled(pRadioHWInfo->szMAC) )
         continue;

      int nVehicleRadioLinkId = g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId;
      if ( nVehicleRadioLinkId < 0 || nVehicleRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count )
         continue;

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[nVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;

      // Ignore vehicle's relay radio links
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[nVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         continue;

     
      if ( (pRadioHWInfo->iRadioType == RADIO_TYPE_ATHEROS) ||
           (pRadioHWInfo->iRadioType == RADIO_TYPE_RALINK) )
      {
         int nRateTx = DEFAULT_RADIO_DATARATE_LOWEST;
         if ( NULL != g_pCurrentModel )
         {
            nRateTx = g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[nVehicleRadioLinkId];

            if ( (0 == nRateTx) || (-100 == nRateTx) )
            {
               if ( g_pCurrentModel->radioLinksParams.link_radio_flags[nVehicleRadioLinkId] & RADIO_FLAGS_USE_MCS_DATARATES )
                  nRateTx = -1;
               else
                  nRateTx = DEFAULT_RADIO_DATARATE_LOWEST;
            }

            log_line("Current model uplink radio datarate for vehicle radio link %d (%s): %d, %u, uplink rate type: %d",
               nVehicleRadioLinkId+1, pRadioHWInfo->szName, nRateTx, getRealDataRateFromRadioDataRate(nRateTx, g_pCurrentModel->radioLinksParams.link_radio_flags[nVehicleRadioLinkId], 0),
               g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[nVehicleRadioLinkId]);
         }
         Preferences* pP = get_Preferences();
         radio_utils_set_datarate_atheros(NULL, i, nRateTx, pP->iDebugWiFiChangeDelay);
      }

      u32 cardFlags = controllerGetCardFlags(pRadioHWInfo->szMAC);

      if ( cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      if ( (cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) ||
           (cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
      {
         if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
         {
            if ( hardware_radio_sik_open_for_read_write(i) <= 0 )
               s_iFailedInitRadioInterface = i;
            else
            {
               g_SM_RadioStats.radio_interfaces[i].openedForRead = 1;
               countOpenedForReadForRadioLink[nVehicleRadioLinkId]++;
               totalCountForRead++;

               g_SM_RadioStats.radio_interfaces[i].openedForWrite = 1;
               countOpenedForWriteForRadioLink[nVehicleRadioLinkId]++;
               totalCountForWrite++;
               iCountSikInterfacesOpened++;
            }
         }
         else if ( hardware_radio_is_elrs_radio(pRadioHWInfo) )
         {
            if ( hardware_radio_serial_open_for_read_write(i) <= 0 )
               s_iFailedInitRadioInterface = i;
            else
            {
               g_SM_RadioStats.radio_interfaces[i].openedForRead = 1;
               countOpenedForReadForRadioLink[nVehicleRadioLinkId]++;
               totalCountForRead++;

               g_SM_RadioStats.radio_interfaces[i].openedForWrite = 1;
               countOpenedForWriteForRadioLink[nVehicleRadioLinkId]++;
               totalCountForWrite++;
               iCountSikInterfacesOpened++;
            }
            radio_tx_set_serial_packet_size(i, DEFAULT_RADIO_SERIAL_AIR_PACKET_SIZE);
         }
         else
         {
            int iRes = radio_open_interface_for_read(i, RADIO_PORT_ROUTER_DOWNLINK);

            if ( iRes > 0 )
            {
               g_SM_RadioStats.radio_interfaces[i].openedForRead = 1;
               countOpenedForReadForRadioLink[nVehicleRadioLinkId]++;
               totalCountForRead++;
            }
            else
               s_iFailedInitRadioInterface = i;
         }
      }

      if ( g_pCurrentModel->getVehicleFirmwareType() == MODEL_FIRMWARE_TYPE_RUBY )
      if ( cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      if ( (cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) ||
           (cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
      if ( ! hardware_radio_is_serial_radio(pRadioHWInfo) )
      if ( ! hardware_radio_is_sik_radio(pRadioHWInfo) )
      {
         if ( radio_open_interface_for_write(i) > 0 )
         {
            g_SM_RadioStats.radio_interfaces[i].openedForWrite = 1;
            countOpenedForWriteForRadioLink[nVehicleRadioLinkId]++;
            totalCountForWrite++;
         }
         else
            s_iFailedInitRadioInterface = i;
      }
   }

   if ( NULL != g_pSM_RadioStats )
      memcpy((u8*)g_pSM_RadioStats, (u8*)&g_SM_RadioStats, sizeof(shared_mem_radio_stats));
   log_line("Opening RX/TX radio interfaces complete. %d interfaces opened for RX, %d interfaces opened for TX:", totalCountForRead, totalCountForWrite);

   if ( totalCountForRead == 0 )
   {
      log_error_and_alarm("Failed to find or open any RX interface for receiving data.");
      radio_links_close_rxtx_radio_interfaces();
      return;
   }

   if ( 0 == totalCountForWrite )
   if ( g_pCurrentModel->getVehicleFirmwareType() == MODEL_FIRMWARE_TYPE_RUBY )
   {
      log_error_and_alarm("Can't find any TX interfaces for sending data.");
      radio_links_close_rxtx_radio_interfaces();
      return;
   }

   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      
      // Ignore vehicle's relay radio links
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         continue;

      if ( 0 == countOpenedForReadForRadioLink[i] )
         log_error_and_alarm("Failed to find or open any RX interface for receiving data on vehicle's radio link %d.", i+1);
      if ( 0 == countOpenedForWriteForRadioLink[i] )
      if ( g_pCurrentModel->getVehicleFirmwareType() == MODEL_FIRMWARE_TYPE_RUBY )
         log_error_and_alarm("Failed to find or open any TX interface for sending data on vehicle's radio link %d.", i+1);

      //if ( 0 == countOpenedForReadForRadioLink[i] && 0 == countOpenedForWriteForRadioLink[i] )
      //   send_alarm_to_central(ALARM_ID_CONTROLLER_NO_INTERFACES_FOR_RADIO_LINK,i, 0);
   }

   log_line("Opened radio interfaces:");
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
   
      int nVehicleRadioLinkId = g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId;      
      int nLocalRadioLinkId = g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId;      
      char szFlags[128];
      szFlags[0] = 0;
      u32 uFlags = controllerGetCardFlags(pRadioHWInfo->szMAC);
      str_get_radio_capabilities_description(uFlags, szFlags);

      char szType[128];
      strcpy(szType, pRadioHWInfo->szDriver);
      if ( NULL != pCardInfo )
         strcpy(szType, str_get_radio_card_model_string(pCardInfo->cardModel));

      if ( pRadioHWInfo->openedForRead && pRadioHWInfo->openedForWrite )
         log_line(" * Radio Interface %d, %s, %s, %s, local radio link %d, vehicle radio link %d, opened for read/write, flags: %s", i+1, pRadioHWInfo->szName, szType, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), nLocalRadioLinkId+1, nVehicleRadioLinkId+1, szFlags );
      else if ( pRadioHWInfo->openedForRead )
         log_line(" * Radio Interface %d, %s, %s, %s, local radio link %d, vehicle radio link %d, opened for read, flags: %s", i+1, pRadioHWInfo->szName, szType, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), nLocalRadioLinkId+1, nVehicleRadioLinkId+1, szFlags );
      else if ( pRadioHWInfo->openedForWrite )
         log_line(" * Radio Interface %d, %s, %s, %s, local radio link %d, vehicle radio link %d, opened for write, flags: %s", i+1, pRadioHWInfo->szName, szType, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), nLocalRadioLinkId+1, nVehicleRadioLinkId+1, szFlags );
      else
         log_line(" * Radio Interface %d, %s, %s, %s not used. Flags: %s", i+1, pRadioHWInfo->szName, szType, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), szFlags );
   }

   if ( 0 < iCountSikInterfacesOpened )
   {
      radio_tx_set_sik_packet_size(g_pCurrentModel->radioLinksParams.iSiKPacketSize);
      radio_tx_start_tx_thread();
   }

   if ( NULL != g_pSM_RadioStats )
      memcpy((u8*)g_pSM_RadioStats, (u8*)&g_SM_RadioStats, sizeof(shared_mem_radio_stats));
   log_line("Finished opening RX/TX radio interfaces.");

   radio_links_set_monitor_mode();
   log_line("OPEN RADIO INTERFACES END ===========================================================");
   log_line("");
}


bool radio_links_apply_settings(Model* pModel, int iRadioLink, type_radio_links_parameters* pRadioLinkParamsOld, type_radio_links_parameters* pRadioLinkParamsNew)
{
   if ( (NULL == pModel) || (NULL == pRadioLinkParamsNew) )
      return false;
   if ( (iRadioLink < 0) || (iRadioLink >= pModel->radioLinksParams.links_count) )
      return false;

   // Update frequencies if needed
   // Update HT20/HT40 if needed

   bool bUpdateFreq = false;
   if ( pRadioLinkParamsOld->link_frequency_khz[iRadioLink] != pRadioLinkParamsNew->link_frequency_khz[iRadioLink] )
      bUpdateFreq = true;
   if ( (pRadioLinkParamsOld->link_radio_flags[iRadioLink] & RADIO_FLAG_HT40_CONTROLLER) != 
        (pRadioLinkParamsNew->link_radio_flags[iRadioLink] & RADIO_FLAG_HT40_CONTROLLER) )
      bUpdateFreq = true;

   if ( bUpdateFreq )
   {
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iRadioLink )
            continue;
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
            continue;

         if ( ! hardware_radioindex_supports_frequency(i, pRadioLinkParamsNew->link_frequency_khz[iRadioLink]) )
            continue;
         radio_utils_set_interface_frequency(pModel, i, iRadioLink, pRadioLinkParamsNew->link_frequency_khz[iRadioLink], g_pProcessStats, 0);
         radio_stats_set_card_current_frequency(&g_SM_RadioStats, i, pRadioLinkParamsNew->link_frequency_khz[iRadioLink]);
      }

      hardware_save_radio_info();
      if ( NULL != g_pSM_RadioStats )
         memcpy((u8*)g_pSM_RadioStats, (u8*)&g_SM_RadioStats, sizeof(shared_mem_radio_stats));
   }

   // Apply data rates
   // If uplink data rate for an Atheros card has changed, update it.
      
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iRadioLink )
         continue;
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( (pRadioHWInfo->iRadioType != RADIO_TYPE_ATHEROS) &&
           (pRadioHWInfo->iRadioType != RADIO_TYPE_RALINK) )
         continue;

      int nRateTx = DEFAULT_RADIO_DATARATE_LOWEST;
      if ( NULL != pModel )
      {
         nRateTx = pModel->radioLinksParams.uplink_datarate_data_bps[iRadioLink];
         if ( (0 == nRateTx) || (-100 == nRateTx) )
         {
            if ( pModel->radioLinksParams.link_radio_flags[iRadioLink] & RADIO_FLAGS_USE_MCS_DATARATES )
               nRateTx = -1;
            else
               nRateTx = DEFAULT_RADIO_DATARATE_LOWEST;
         }

         update_atheros_card_datarate(pModel, i, nRateTx, g_pProcessStats);
         g_TimeNow = get_current_timestamp_ms();
      }
   }

   // Radio flags are applied on the fly, when sending each radio packet
   
   return true;
}


void radio_links_set_monitor_mode()
{
   log_line("Set monitor mode on all radio interfaces...");
   s_uTimeLastSetRadioLinksMonitorMode = g_TimeNow;
   u32 uDelayMS = 20;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( ! hardware_radio_is_wifi_radio(pRadioHWInfo) )
         continue;

      #ifdef HW_PLATFORM_RADXA
      char szComm[128];
      //sprintf(szComm, "iwconfig %s mode monitor 2>&1", pRadioHWInfo->szName );
      //hw_execute_bash_command(szComm, NULL);
      //hardware_sleep_ms(uDelayMS);

      sprintf(szComm, "iw dev %s set monitor none 2>&1", pRadioHWInfo->szName);
      hw_execute_bash_command(szComm, NULL);
      hardware_sleep_ms(uDelayMS);

      sprintf(szComm, "iw dev %s set monitor fcsfail 2>&1", pRadioHWInfo->szName);
      hw_execute_bash_command(szComm, NULL);
      hardware_sleep_ms(uDelayMS);
      #endif

      #ifdef HW_PLATFORM_RASPBERRY
      char szComm[128];
      sprintf(szComm, "iw dev %s set monitor none 2>&1", pRadioHWInfo->szName);
      hw_execute_bash_command(szComm, NULL);
      hardware_sleep_ms(uDelayMS);

      sprintf(szComm, "iw dev %s set monitor fcsfail 2>&1", pRadioHWInfo->szName);
      hw_execute_bash_command(szComm, NULL);
      hardware_sleep_ms(uDelayMS);
      #endif
   }
}

u32 radio_links_get_last_set_monitor_time()
{
   return s_uTimeLastSetRadioLinksMonitorMode;
}
