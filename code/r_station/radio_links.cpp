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
#include "radio_links_sik.h"
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
#include "../common/models_connect_frequencies.h"
#include "../radio/radio_rx.h"
#include "../radio/radio_tx.h"
#include "../utils/utils_controller.h"

#include "packets_utils.h"
#include "ruby_rt_station.h"

int s_iLastAutoAssignmentRadioLink = -1;
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

void radio_links_compute_auto_radio_interfaces_assignment(int iVehicleRadioLink)
{
   log_line("------------------------------------------------------------------");

   s_iLastAutoAssignmentRadioLink = iVehicleRadioLink;

   if ( g_bSearching || (NULL == g_pCurrentModel) )
   {
      log_error_and_alarm("Invalid parameters for assigning radio interfaces");
      return;
   }

   g_SM_RadioStats.countVehicleRadioLinks = 0;
   g_SM_RadioStats.countVehicleRadioLinks = g_pCurrentModel->radioLinksParams.links_count;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId = -1;
      g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId = -1;
      g_SM_RadioStats.radio_links[i].matchingVehicleRadioLinkId = -1;
   }

   //---------------------------------------------------------------
   // See how many active radio links the vehicle has

   u32 uStoredMainFrequencyForModel = get_model_main_connect_frequency(g_pCurrentModel->uVehicleId);
   int iStoredMainRadioLinkForModel = -1;

   int iCountAssignedVehicleRadioLinks = 0;

   int iCountVehicleActiveUsableRadioLinks = 0;
   u32 uConnectFirstUsableFrequency = 0;
   int iConnectFirstUsableRadioLinkId = 0;

   if ( (iVehicleRadioLink >= 0) && (iVehicleRadioLink < g_pCurrentModel->radioLinksParams.links_count) )
      log_line("Computing local radio interfaces (%d interfaces) assignment to single vehicle radio link %d (%s)...", hardware_get_radio_interfaces_count(), iVehicleRadioLink+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iVehicleRadioLink]));
   else
      log_line("Computing local radio interfaces (%d interfaces) assignment to auto vehicle radio links...", hardware_get_radio_interfaces_count());

   log_line("Vehicle (%u, %s) main 'connect to' frequency: %s, vehicle has a total of %d radio links.",
      g_pCurrentModel->uVehicleId, g_pCurrentModel->getLongName(), str_format_frequency(uStoredMainFrequencyForModel), g_pCurrentModel->radioLinksParams.links_count);

   if ( (iVehicleRadioLink >= 0) && (iVehicleRadioLink < g_pCurrentModel->radioLinksParams.links_count) )
      log_line("Force assignment to vehicle radio link %d only (%s)", iVehicleRadioLink+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iVehicleRadioLink]));

   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      {
         log_line("Vehicle's radio link %d is disabled. Skipping it.", i+1);
         continue;
      }

      if ( g_pCurrentModel->radioLinksParams.link_frequency_khz[i] == uStoredMainFrequencyForModel )
      {
         iStoredMainRadioLinkForModel = i;
         log_line("Vehicle's radio link %d is the main connect to radio link.", i+1);
      }

      // Ignore vehicle's relay radio links
      if ( iVehicleRadioLink == -1 )
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      {
         log_line("Vehicle's radio link %d is used for relaying. Skipping it.", i+1);
         continue;
      }

      log_line("Vehicle's radio link %d is usable, current frequency: %s", i+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[i]));
      iCountVehicleActiveUsableRadioLinks++;

      if ( (iVehicleRadioLink >= 0) && (iVehicleRadioLink < g_pCurrentModel->radioLinksParams.links_count) )
      {
         if ( i == iVehicleRadioLink )
         {
            uConnectFirstUsableFrequency = g_pCurrentModel->radioLinksParams.link_frequency_khz[i];
            iConnectFirstUsableRadioLinkId = i;
         }
         continue;
      }
      if ( 0 == uConnectFirstUsableFrequency )
      {
         uConnectFirstUsableFrequency = g_pCurrentModel->radioLinksParams.link_frequency_khz[i];
         iConnectFirstUsableRadioLinkId = i;
      }
   }

   log_line("Vehicle has %d active (enabled and not relay) radio links (out of %d radio links)", iCountVehicleActiveUsableRadioLinks, g_pCurrentModel->radioLinksParams.links_count);
   if ( -1 == iStoredMainRadioLinkForModel )
      log_line("Could not find vehicle's main connect radio link for vehicle, main connect frequency is: %s", str_format_frequency(uStoredMainFrequencyForModel));
   else
      log_line("Found vehicle's main connect radio link for frequency %s: vehicle radio link %d", str_format_frequency(uStoredMainFrequencyForModel), iStoredMainRadioLinkForModel+1);

   if ( 0 == iCountVehicleActiveUsableRadioLinks )
   {
      log_error_and_alarm("Vehicle has no active (enabled and not relay) radio links (out of %d radio links)", g_pCurrentModel->radioLinksParams.links_count);
      return;
   }

   //--------------------------------------------------------------------------
   // Begin - Check what vehicle radio links are supported by each radio interface.

   bool bInterfaceSupportsVehicleLink[MAX_RADIO_INTERFACES][MAX_RADIO_INTERFACES];
   bool bInterfaceSupportsMainConnectLink[MAX_RADIO_INTERFACES];
   int iInterfaceSupportedLinksCount[MAX_RADIO_INTERFACES];

   bool bCtrlInterfaceWasAssigned[MAX_RADIO_INTERFACES];
   bool bVehicleLinkWasAssigned[MAX_RADIO_INTERFACES];
   int  iVehicleLinkWasAssignedToControllerLinkIndex[MAX_RADIO_INTERFACES];

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      bCtrlInterfaceWasAssigned[i] = false;
      bVehicleLinkWasAssigned[i] = false;
      iVehicleLinkWasAssignedToControllerLinkIndex[i] = -1;

      bInterfaceSupportsMainConnectLink[i] = false;
      iInterfaceSupportedLinksCount[i] = 0;
      for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
         bInterfaceSupportsVehicleLink[i][k] = false;
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
      {
         log_softerror_and_alarm("Failed to get controller's radio interface %d hardware info. Skipping it.", i+1);
         continue;
      }
      if ( controllerIsCardDisabled(pRadioHWInfo->szMAC) )
      {
         log_line("Controller's radio interface %d is disabled. Skipping it.", i+1);
         continue;
      }

      u32 cardFlags = controllerGetCardFlags(pRadioHWInfo->szMAC);

      for( int iRadioLink=0; iRadioLink<g_pCurrentModel->radioLinksParams.links_count; iRadioLink++ )
      {
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            continue;

         if ( iVehicleRadioLink == -1 )
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
            continue;

         if ( (iVehicleRadioLink >= 0) && (iVehicleRadioLink < g_pCurrentModel->radioLinksParams.links_count) )
         if ( iRadioLink != iVehicleRadioLink )
            continue;

         // Uplink type radio link and RX only radio interface

         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
         if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         if ( ! (cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
            continue;

         // Downlink type radio link and TX only radio interface

         if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
         if ( ! (cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
            continue;

         bool bDoesMatch = false;

         // Match ELRS serial radio links
         if ( (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK_ELRS ) ||
              (pRadioHWInfo->iCardModel == CARD_MODEL_SERIAL_RADIO_ELRS) )
         {
            if ( (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK_ELRS ) &&
                 (pRadioHWInfo->iCardModel == CARD_MODEL_SERIAL_RADIO_ELRS) )
               bDoesMatch = true;
         }
         else if ( hardware_radio_supports_frequency(pRadioHWInfo, g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLink]) )
            bDoesMatch = true;

         if ( bDoesMatch )
         {
            log_line("Controller's radio interface %d does support vehicle's radio link %d (%s).", i+1, iRadioLink+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLink]));
            bInterfaceSupportsVehicleLink[i][iRadioLink] = true;
            iInterfaceSupportedLinksCount[i]++;

            if ( uStoredMainFrequencyForModel == g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLink] )
               bInterfaceSupportsMainConnectLink[i] = true;
         }
      }
   }

   // End - Check what vehicle radio links are supported by each radio interface.
   //--------------------------------------------------------------------------

   //---------------------------------------------------------------
   // Begin - Model with a single active radio link

   if ( iCountVehicleActiveUsableRadioLinks > 0 )
   if ( (iVehicleRadioLink >= 0) && (iVehicleRadioLink < g_pCurrentModel->radioLinksParams.links_count) )
   {
      iConnectFirstUsableRadioLinkId = iVehicleRadioLink;
      iCountVehicleActiveUsableRadioLinks = 1;
      log_line("Use a single vehicle radio link to connect to: vehicle radio link %d", iConnectFirstUsableRadioLinkId+1);
   }

   if ( 1 == iCountVehicleActiveUsableRadioLinks )
   {
      log_line("Computing controller's radio interfaces assignment to a single vehicle's radio link %d (vehicle has a single active (enabled and not relay) radio link on %s)", iConnectFirstUsableRadioLinkId+1, str_format_frequency(uConnectFirstUsableFrequency));

      int iCountInterfacesAssigned = 0;

      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( controllerIsCardDisabled(pRadioHWInfo->szMAC) )
         {
            log_line("  * Radio interface %d is disabled, do not assign it.", i+1);
            continue;
         }
         if ( ! hardware_radio_supports_frequency(pRadioHWInfo, uConnectFirstUsableFrequency) )
         {
            log_line("  * Radio interface %d does not support %s, do not assign it.", i+1, str_format_frequency(uConnectFirstUsableFrequency));
            continue;
         }
         g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId = 0;
         g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId = iConnectFirstUsableRadioLinkId;
         g_SM_RadioStats.radio_links[0].matchingVehicleRadioLinkId = iConnectFirstUsableRadioLinkId;
         iCountInterfacesAssigned++;
         t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
         if ( NULL != pCardInfo )
            log_line("  * Assigned radio interface %d (%s) to vehicle's radio link %d", i+1, str_get_radio_card_model_string(pCardInfo->cardModel), iConnectFirstUsableRadioLinkId+1);
         else
            log_line("  * Assigned radio interface %d (%s) to vehicle's radio link %d", i+1, "Unknown Type", iConnectFirstUsableRadioLinkId+1);
      }
      iCountAssignedVehicleRadioLinks = 1;
      g_SM_RadioStats.countLocalRadioLinks = 1;
      if ( NULL != g_pSM_RadioStats )
         memcpy((u8*)g_pSM_RadioStats, (u8*)&g_SM_RadioStats, sizeof(shared_mem_radio_stats));
      if ( 0 == iCountInterfacesAssigned )
         send_alarm_to_central(ALARM_ID_CONTROLLER_NO_INTERFACES_FOR_RADIO_LINK,iConnectFirstUsableRadioLinkId, 0);

      log_line("Controller will have %d radio links active/connected to vehicle.", iCountAssignedVehicleRadioLinks);
      log_line("Done computing radio interfaces assignment to radio links.");
      log_line("------------------------------------------------------------------");
      return;
   }

   // End - Model with a single active radio link
   //---------------------------------------------------------------

   //---------------------------------------------------------------
   // Begin - Model with a multiple active radio links

   log_line("Computing controller's radio interfaces assignment to vehicle's radio links (vehicle has %d active radio links (enabled and not relay), main controller connect frequency is: %s)", iCountVehicleActiveUsableRadioLinks, str_format_frequency(uStoredMainFrequencyForModel));

   //---------------------------------------------------------------
   // Begin - First, assign the radio interfaces that supports only a single radio link

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( (NULL == pRadioHWInfo) || (controllerIsCardDisabled(pRadioHWInfo->szMAC)) )
         continue;
      if ( iInterfaceSupportedLinksCount[i] != 1 )
         continue;

      int iSupportedVehicleLinkByInterface = -1;
      for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
      {
         if ( bInterfaceSupportsVehicleLink[i][k] )
         {
            iSupportedVehicleLinkByInterface = k;
            break;
         }
      }
      if ( (-1 == iSupportedVehicleLinkByInterface) || (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iSupportedVehicleLinkByInterface] & RADIO_HW_CAPABILITY_FLAG_DISABLED) )
         continue;

      if ( ! bVehicleLinkWasAssigned[iSupportedVehicleLinkByInterface] )
      {
         iVehicleLinkWasAssignedToControllerLinkIndex[iSupportedVehicleLinkByInterface] = iCountAssignedVehicleRadioLinks;
         iCountAssignedVehicleRadioLinks++;
      }
      bVehicleLinkWasAssigned[iSupportedVehicleLinkByInterface] = true;
      bCtrlInterfaceWasAssigned[i] = true;

      g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId = iVehicleLinkWasAssignedToControllerLinkIndex[iSupportedVehicleLinkByInterface];
      g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId = iSupportedVehicleLinkByInterface;
      g_SM_RadioStats.radio_links[iVehicleLinkWasAssignedToControllerLinkIndex[iSupportedVehicleLinkByInterface]].matchingVehicleRadioLinkId = iSupportedVehicleLinkByInterface;

      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
      if ( NULL != pCardInfo )
         log_line("  * Step A) Assigned controller's radio interface %d (%s) to controller local radio link %d, vehicle's radio link %d, %s, as it supports a single radio link from vehicle.", i+1, str_get_radio_card_model_string(pCardInfo->cardModel), g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId+1, iSupportedVehicleLinkByInterface+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iSupportedVehicleLinkByInterface]));
      else
         log_line("  * Step A) Assigned controller's radio interface %d (%s) to controller local radio link %d, vehicle's radio link %d, %s, as it supports a single radio link from vehicle.", i+1, "Unknown Type", g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId+1, iSupportedVehicleLinkByInterface+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iSupportedVehicleLinkByInterface]));
   }

   //---------------------------------------------------------------
   // End - First, assign the radio interfaces that supports only a single radio link

   //---------------------------------------------------------------
   // Assign at least one radio interface to the main connect radio link

   if ( (iStoredMainRadioLinkForModel != -1) && (! bVehicleLinkWasAssigned[iStoredMainRadioLinkForModel]) )
   {
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo || controllerIsCardDisabled(pRadioHWInfo->szMAC) )
            continue;
         if ( bCtrlInterfaceWasAssigned[i] )
            continue;
         if ( ! bInterfaceSupportsMainConnectLink[i] )
            continue;

         if ( ! bVehicleLinkWasAssigned[iStoredMainRadioLinkForModel] )
         {
            iVehicleLinkWasAssignedToControllerLinkIndex[iStoredMainRadioLinkForModel] = iCountAssignedVehicleRadioLinks;
            iCountAssignedVehicleRadioLinks++;
         }
         bVehicleLinkWasAssigned[iStoredMainRadioLinkForModel] = true;
         bCtrlInterfaceWasAssigned[i] = true;

         g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId = iVehicleLinkWasAssignedToControllerLinkIndex[iStoredMainRadioLinkForModel];
         g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId = iStoredMainRadioLinkForModel;
         g_SM_RadioStats.radio_links[iVehicleLinkWasAssignedToControllerLinkIndex[iStoredMainRadioLinkForModel]].matchingVehicleRadioLinkId = iStoredMainRadioLinkForModel;

         t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
         if ( NULL != pCardInfo )
            log_line("  * Step B) Assigned controller's radio interface %d (%s) to controller local radio link %d, vehicle's main connect radio link %d, %s", i+1, str_get_radio_card_model_string(pCardInfo->cardModel), g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId+1, iStoredMainRadioLinkForModel+1, str_format_frequency(uStoredMainFrequencyForModel));
         else
            log_line("  * Step B) Assigned controller's radio interface %d (%s) to controller local radio link %d, vehicle's main connect radio link %d, %s", i+1, "Unknown Type", g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId+1, iStoredMainRadioLinkForModel+1, str_format_frequency(uStoredMainFrequencyForModel));
         break;
      }
   }

   //---------------------------------------------------------------
   // Assign alternativelly each remaining radio interfaces to one radio link

   // Assign to the first vehicle's radio link that has no cards assigned to

   int iVehicleRadioLinkIdToAssign = 0;
   int iSafeCounter = 10;
   while ( true && (iSafeCounter > 0) )
   {
      iSafeCounter--;
      if ( ! bVehicleLinkWasAssigned[iVehicleRadioLinkIdToAssign] )
         break;

      iVehicleRadioLinkIdToAssign++;
      if ( iVehicleRadioLinkIdToAssign >= g_pCurrentModel->radioLinksParams.links_count )
      {
         iVehicleRadioLinkIdToAssign = 0;
         break;
      }
   }

   log_line("Compute local radio interface to assign to first unasigned vehicle radio link: vehicle radio link %d", iVehicleRadioLinkIdToAssign+1);

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo || controllerIsCardDisabled(pRadioHWInfo->szMAC) )
         continue;
      if ( iInterfaceSupportedLinksCount[i] < 2 )
         continue;
      if ( bCtrlInterfaceWasAssigned[i] )
         continue;

      int k=0;
      do
      {
         if ( bInterfaceSupportsVehicleLink[i][iVehicleRadioLinkIdToAssign] )
         {
            if ( ! bVehicleLinkWasAssigned[iVehicleRadioLinkIdToAssign] )
            {
               iVehicleLinkWasAssignedToControllerLinkIndex[iVehicleRadioLinkIdToAssign] = iCountAssignedVehicleRadioLinks;
               iCountAssignedVehicleRadioLinks++;
            }
            bVehicleLinkWasAssigned[iVehicleRadioLinkIdToAssign] = true;
            bCtrlInterfaceWasAssigned[i] = true;

            g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId = iVehicleLinkWasAssignedToControllerLinkIndex[iVehicleRadioLinkIdToAssign];
            g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId = iVehicleRadioLinkIdToAssign;
            g_SM_RadioStats.radio_links[iVehicleLinkWasAssignedToControllerLinkIndex[iVehicleRadioLinkIdToAssign]].matchingVehicleRadioLinkId = iVehicleRadioLinkIdToAssign;

            t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
            if ( NULL != pCardInfo )
               log_line("  * C) Assigned controller's radio interface %d (%s) to controller local radio link %d, radio link %d, %s", i+1, str_get_radio_card_model_string(pCardInfo->cardModel), g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId+1, iVehicleRadioLinkIdToAssign+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iVehicleRadioLinkIdToAssign]));
            else
               log_line("  * C) Assigned controller's radio interface %d (%s) to controller local radio link %d, radio link %d, %s", i+1, "Unknown Type", g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId+1, iVehicleRadioLinkIdToAssign+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iVehicleRadioLinkIdToAssign]));
         }
         k++;
         iVehicleRadioLinkIdToAssign++;
         if ( iVehicleRadioLinkIdToAssign >= g_pCurrentModel->radioLinksParams.links_count )
            iVehicleRadioLinkIdToAssign = 0;
      }
      while ( (! bCtrlInterfaceWasAssigned[i]) && (k <= MAX_RADIO_INTERFACES) );
   }

   g_SM_RadioStats.countLocalRadioLinks = iCountAssignedVehicleRadioLinks;
   log_line("Assigned %d controller local radio links to vehicle's radio links (vehicle has %d active radio links)", iCountAssignedVehicleRadioLinks, iCountVehicleActiveUsableRadioLinks);

   if ( NULL != g_pSM_RadioStats )
      memcpy((u8*)g_pSM_RadioStats, (u8*)&g_SM_RadioStats, sizeof(shared_mem_radio_stats));

   //---------------------------------------------------------------
   // Log errors

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo || controllerIsCardDisabled(pRadioHWInfo->szMAC) )
      {
         log_line("  * Radio interface %d is disabled. It was not assigned to any radio link.", i+1 );
         continue;
      }
      if ( iInterfaceSupportedLinksCount[i] == 0 )
      {
         log_line("  * Radio interface %d does not support any radio links.", i+1 );
         continue;
      }
   }

   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;

      // Ignore vehicle's relay radio links
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         continue;

      if ( ! bVehicleLinkWasAssigned[i] )
      {
         log_softerror_and_alarm("  * No controller radio interfaces where assigned to vehicle's radio link %d !", i+1);
         int iCountAssignableRadioInterfaces = controller_count_asignable_radio_interfaces_to_vehicle_radio_link(g_pCurrentModel, i);
         if ( 0 == iCountAssignableRadioInterfaces )
            send_alarm_to_central(ALARM_ID_CONTROLLER_NO_INTERFACES_FOR_RADIO_LINK, (u32)i, 0);
      }
   }
   log_line("Radio links mapping (from controller's %d local radio link(s) to vehicle's %d radio link(s):", g_SM_RadioStats.countLocalRadioLinks, g_pCurrentModel->radioLinksParams.links_count);
   for( int i=0; i<g_SM_RadioStats.countLocalRadioLinks; i++ )
      log_line("* Local radio link %d mapped to vehicle's radio link %d;", i+1, g_SM_RadioStats.radio_links[i].matchingVehicleRadioLinkId+1);

   log_line("Done computing radio interfaces assignment to radio links.");
   log_line("------------------------------------------------------------------");
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
               if ( g_pCurrentModel->radioLinksParams.link_radio_flags_rx[nVehicleRadioLinkId] & RADIO_FLAGS_USE_MCS_DATARATES )
                  nRateTx = -1;
               else
                  nRateTx = DEFAULT_RADIO_DATARATE_LOWEST;
            }

            log_line("Current model uplink radio datarate for vehicle radio link %d (%s): %d, %u, uplink rate type: %d",
               nVehicleRadioLinkId+1, pRadioHWInfo->szName, nRateTx, getRealDataRateFromRadioDataRate(nRateTx, g_pCurrentModel->radioLinksParams.link_radio_flags_rx[nVehicleRadioLinkId], 0),
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

      if ( (0 == countOpenedForReadForRadioLink[i]) )
      if ( (s_iLastAutoAssignmentRadioLink == -1) || (s_iLastAutoAssignmentRadioLink == i) )
         log_error_and_alarm("Failed to find or open any RX interface for receiving data on vehicle's radio link %d.", i+1);

      if ( 0 == countOpenedForWriteForRadioLink[i] )
      if ( (s_iLastAutoAssignmentRadioLink == -1) || (s_iLastAutoAssignmentRadioLink == i) )
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

bool radio_links_set_cards_frequencies_and_params(int iVehicleLinkId)
{
   if ( g_bSearching || (NULL == g_pCurrentModel) )
   {
      log_error_and_alarm("Invalid parameters for setting radio interfaces frequencies");
      return false;
   }

   if ( (iVehicleLinkId < 0) || (iVehicleLinkId >= hardware_get_radio_interfaces_count()) )
      log_line("Links: Setting all cards frequencies and params according to vehicle radio links...");
   else
      log_line("Links: Setting cards frequencies and params only for vehicle radio link %d", iVehicleLinkId+1);

   // Begin - Update atheros first
   for( int iRadioLink=0; iRadioLink<g_pCurrentModel->radioLinksParams.links_count; iRadioLink++ )
   {
      if ( (iVehicleLinkId != -1) && (iRadioLink != iVehicleLinkId) )
         continue;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            continue;
         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iRadioLink )
            continue;
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
            continue;
         if ( ! pRadioHWInfo->isConfigurable )
            continue;
         if ( (pRadioHWInfo->iRadioType != RADIO_TYPE_ATHEROS) &&
              (pRadioHWInfo->iRadioType != RADIO_TYPE_RALINK) )
            continue;

         int nRateTx = DEFAULT_RADIO_DATARATE_LOWEST;
         nRateTx = g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[iRadioLink];
         if ( (0 == nRateTx) || (-100 == nRateTx) )
         {
            if ( g_pCurrentModel->radioLinksParams.link_radio_flags_rx[iRadioLink] & RADIO_FLAGS_USE_MCS_DATARATES )
               nRateTx = -1;
            else
               nRateTx = DEFAULT_RADIO_DATARATE_LOWEST;
         }
         update_atheros_card_datarate(g_pCurrentModel, i, nRateTx, g_pProcessStats);
         g_TimeNow = get_current_timestamp_ms();
      }
   }
   // End - Update atheros first

   Preferences* pP = get_Preferences();

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;

      if ( ! pRadioHWInfo->isConfigurable )
      {
         radio_stats_set_card_current_frequency(&g_SM_RadioStats, i, pRadioHWInfo->uCurrentFrequencyKhz);
         log_line("Links: Radio interface %d is not configurable. Skipping it.", i+1);
         continue;
      }
      if ( controllerIsCardDisabled(pRadioHWInfo->szMAC) )
      {
         log_line("Links: Radio interface %d is disabled. Skipping it.", i+1);
         continue;
      }

      int nAssignedVehicleRadioLinkId = g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId;
      if ( (nAssignedVehicleRadioLinkId < 0) || (nAssignedVehicleRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count) )
      {
         log_line("Links: Radio interface %d is not assigned to any vehicle radio link. Skipping it.", i+1);
         continue;
      }

      if ( ( iVehicleLinkId >= 0 ) && (nAssignedVehicleRadioLinkId != iVehicleLinkId) )
         continue;

      if ( 0 == hardware_radio_supports_frequency(pRadioHWInfo, g_pCurrentModel->radioLinksParams.link_frequency_khz[nAssignedVehicleRadioLinkId] ) )
      {
         log_line("Links: Radio interface %d does not support vehicle radio link %d frequency %s. Skipping it.", i+1, nAssignedVehicleRadioLinkId+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[nAssignedVehicleRadioLinkId]));
         continue;
      }

      if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
      {
         if ( iVehicleLinkId >= 0 )
            radio_links_flag_update_sik_interface(i);
         else
         {
            u32 uFreqKhz = g_pCurrentModel->radioLinksParams.link_frequency_khz[nAssignedVehicleRadioLinkId];
            u32 uDataRate = g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[nAssignedVehicleRadioLinkId];
            u32 uTxPower = DEFAULT_RADIO_SIK_TX_POWER;
            t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
            if ( NULL != pCRII )
               uTxPower = pCRII->iRawPowerLevel;
            u32 uECC = (g_pCurrentModel->radioLinksParams.link_radio_flags_rx[nAssignedVehicleRadioLinkId] & RADIO_FLAGS_SIK_ECC)? 1:0;
            u32 uLBT = (g_pCurrentModel->radioLinksParams.link_radio_flags_rx[nAssignedVehicleRadioLinkId] & RADIO_FLAGS_SIK_LBT)? 1:0;
            u32 uMCSTR = (g_pCurrentModel->radioLinksParams.link_radio_flags_rx[nAssignedVehicleRadioLinkId] & RADIO_FLAGS_SIK_MCSTR)? 1:0;

            bool bDataRateOk = false;
            for( int k=0; k<getSiKAirDataRatesCount(); k++ )
            {
               if ( (int)uDataRate == getSiKAirDataRates()[k] )
               {
                  bDataRateOk = true;
                  break;
               }
            }

            if ( ! bDataRateOk )
            {
               log_softerror_and_alarm("Invalid radio datarate for SiK radio: %d bps. Revert to %d bps.", uDataRate, DEFAULT_RADIO_DATARATE_SIK_AIR);
               uDataRate = DEFAULT_RADIO_DATARATE_SIK_AIR;
            }

            int iRetry = 0;
            while ( iRetry < 2 )
            {
               int iRes = hardware_radio_sik_set_params(pRadioHWInfo,
                      uFreqKhz,
                      DEFAULT_RADIO_SIK_FREQ_SPREAD, DEFAULT_RADIO_SIK_CHANNELS,
                      DEFAULT_RADIO_SIK_NETID,
                      uDataRate, uTxPower,
                      uECC, uLBT, uMCSTR,
                      NULL);
               if ( iRes != 1 )
               {
                  log_softerror_and_alarm("Failed to configure SiK radio interface %d", i+1);
                  iRetry++;
               }
               else
               {
                  log_line("Updated successfully SiK radio interface %d to txpower %d, airrate: %d bps, ECC/LBT/MCSTR: %d/%d/%d",
                     i+1, uTxPower, uDataRate, uECC, uLBT, uMCSTR);
                  radio_stats_set_card_current_frequency(&g_SM_RadioStats, i, g_pCurrentModel->radioLinksParams.link_frequency_khz[nAssignedVehicleRadioLinkId]);
                  break;
               }
            }
         }
      }
      else
      {
         if ( radio_utils_set_interface_frequency(g_pCurrentModel, i, iVehicleLinkId, g_pCurrentModel->radioLinksParams.link_frequency_khz[nAssignedVehicleRadioLinkId], g_pProcessStats, pP->iDebugWiFiChangeDelay) )
            radio_stats_set_card_current_frequency(&g_SM_RadioStats, i, g_pCurrentModel->radioLinksParams.link_frequency_khz[nAssignedVehicleRadioLinkId]);
      }
   }

   if ( NULL != g_pSM_RadioStats )
      memcpy((u8*)g_pSM_RadioStats, (u8*)&g_SM_RadioStats, sizeof(shared_mem_radio_stats));

   hardware_save_radio_info();

   if ( (iVehicleLinkId < 0) || (iVehicleLinkId >= hardware_get_radio_interfaces_count()) )
      log_line("Links: Done setting all cards frequencies and params according to vehicle radio links.");
   else
      log_line("Links: Done setting cards frequencies and params only for vehicle radio link %d", iVehicleLinkId+1);

   return true;
}

bool radio_links_set_cards_frequencies_for_search(u32 uSearchFreq, bool bSiKSearch, int iAirDataRate, int iECC, int iLBT, int iMCSTR)
{
   log_line("Links: Set all cards frequencies for search mode to %s", str_format_frequency(uSearchFreq));
   if ( bSiKSearch )
      log_line("Search SiK mode update. Change all cards frequencies and update SiK params: Airrate: %d bps, ECC/LBT/MCSTR: %d/%d/%d",
         iAirDataRate, iECC, iLBT, iMCSTR);
   else
      log_line("No SiK mode update. Just change all interfaces frequencies");

   Preferences* pP = get_Preferences();

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;

      u32 flags = controllerGetCardFlags(pRadioHWInfo->szMAC);
      char szFlags[128];
      szFlags[0] = 0;
      str_get_radio_capabilities_description(flags, szFlags);

      log_line("Checking controller radio interface %d (%s) settings: MAC: [%s], flags: %s",
            i+1, pRadioHWInfo->szName, pRadioHWInfo->szMAC, szFlags );

      if ( controllerIsCardDisabled(pRadioHWInfo->szMAC) )
      {
         log_line("Links: Radio interface %d is disabled. Skipping it.", i+1);
         continue;
      }

      if ( ! pRadioHWInfo->isConfigurable )
      {
         radio_stats_set_card_current_frequency(&g_SM_RadioStats, i, pRadioHWInfo->uCurrentFrequencyKhz);
         log_line("Links: Radio interface %d is not configurable. Skipping it.", i+1);
         continue;
      }

      if ( 0 == hardware_radio_supports_frequency(pRadioHWInfo, uSearchFreq ) )
      {
         log_line("Links: Radio interface %d does not support search frequency %s. Skipping it.", i+1, str_format_frequency(uSearchFreq));
         continue;
      }

      if ( ! (flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
      {
         log_line("Links: Radio interface %d can't Rx. Skipping it.", i+1);
         continue;
      }

      if ( ! (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
      {
         log_line("Links: Radio interface %d can't be used for data Rx. Skipping it.", i+1);
         continue;
      }

      if ( bSiKSearch && hardware_radio_is_sik_radio(pRadioHWInfo) )
      {
         t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
         u32 uFreqKhz = uSearchFreq;
         u32 uDataRate = iAirDataRate;
         u32 uTxPower = DEFAULT_RADIO_SIK_TX_POWER;
         if ( NULL != pCRII )
            uTxPower = pCRII->iRawPowerLevel;
         u32 uECC = iECC;
         u32 uLBT = iLBT;
         u32 uMCSTR = iMCSTR;

         bool bDataRateOk = false;
         for( int k=0; k<getSiKAirDataRatesCount(); k++ )
         {
            if ( (int)uDataRate == getSiKAirDataRates()[k] )
            {
               bDataRateOk = true;
               break;
            }
         }

         if ( ! bDataRateOk )
         {
            log_softerror_and_alarm("Invalid radio datarate for SiK radio: %d bps. Revert to %d bps.", uDataRate, DEFAULT_RADIO_DATARATE_SIK_AIR);
            uDataRate = DEFAULT_RADIO_DATARATE_SIK_AIR;
         }

         int iRetry = 0;
         while ( iRetry < 2 )
         {
            int iRes = hardware_radio_sik_set_params(pRadioHWInfo,
                   uFreqKhz,
                   DEFAULT_RADIO_SIK_FREQ_SPREAD, DEFAULT_RADIO_SIK_CHANNELS,
                   DEFAULT_RADIO_SIK_NETID,
                   uDataRate, uTxPower,
                   uECC, uLBT, uMCSTR,
                   NULL);
            if ( iRes != 1 )
            {
               log_softerror_and_alarm("Failed to configure SiK radio interface %d", i+1);
               iRetry++;
            }
            else
            {
               log_line("Updated successfully SiK radio interface %d to txpower %d, airrate: %d bps, ECC/LBT/MCSTR: %d/%d/%d",
                   i+1, uTxPower, uDataRate, uECC, uLBT, uMCSTR);
               radio_stats_set_card_current_frequency(&g_SM_RadioStats, i, uSearchFreq);
               break;
            }
         }
      }
      else
      {
         if ( radio_utils_set_interface_frequency(NULL, i, -1, uSearchFreq, g_pProcessStats, pP->iDebugWiFiChangeDelay) )
            radio_stats_set_card_current_frequency(&g_SM_RadioStats, i, uSearchFreq);
      }
   }

   if ( NULL != g_pSM_RadioStats )
      memcpy((u8*)g_pSM_RadioStats, (u8*)&g_SM_RadioStats, sizeof(shared_mem_radio_stats));
   log_line("Links: Set all cards frequencies for search mode to %s. Completed.", str_format_frequency(uSearchFreq));
   return true;
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
   if ( (pRadioLinkParamsOld->link_radio_flags_rx[iRadioLink] & RADIO_FLAG_HT40) !=
        (pRadioLinkParamsNew->link_radio_flags_rx[iRadioLink] & RADIO_FLAG_HT40) )
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
            if ( pModel->radioLinksParams.link_radio_flags_rx[iRadioLink] & RADIO_FLAGS_USE_MCS_DATARATES )
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
