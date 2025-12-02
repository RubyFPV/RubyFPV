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
#include "../base/models_list.h"
#include "../base/commands.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../base/radio_utils.h"
#include "../base/ruby_ipc.h"
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "../radio/radiolink.h"
#include "../radio/radio_rx.h"
#include "../radio/radio_tx.h"

#include "ruby_rt_vehicle.h"
#include "packets_utils.h"
#include "processor_tx_video.h"
#include "process_received_ruby_messages.h"
#include "launchers_vehicle.h"
#include "processor_relay.h"
#include "shared_vars.h"
#include "timers.h"
#include "adaptive_video.h"

fd_set s_ReadSetRXRadio;

bool s_bRCLinkDetected = false;

u32 s_uLastCommandChangeDataRateTime = 0;
u32 s_uLastCommandChangeDataRateCounter = MAX_U32;

u32 s_uTotalBadPacketsReceived = 0;

void _mark_link_from_controller_present(int iRadioInterface)
{
   g_bHadEverLinkToController = true;

   if ( hardware_radio_index_is_wifi_radio(iRadioInterface) )
      g_TimeLastReceivedFastRadioPacketFromController = g_TimeNow;
   else
      g_TimeLastReceivedSlowRadioPacketFromController = g_TimeNow;

   if ( (! g_bHasFastUplinkFromController) && hardware_radio_index_is_wifi_radio(iRadioInterface) )
   {
      log_line("[Router] Fast link from controller recovered (received fast packets from controller).");
      if ( 0 == g_LastTimeLostFastLinkFromController )
         log_line("[Router] Never have had lost the fast link before.");
      else
         log_line("[Router] Last time the fast link was lost was %u ms ago", g_TimeNow - g_LastTimeLostFastLinkFromController);

      //if ( g_pCurrentModel->osd_params.osd_preferences[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_PREFERENCES_BIT_FLAG_SHOW_CONTROLLER_LINK_LOST_ALARM )
         send_alarm_to_controller(ALARM_ID_LINK_TO_CONTROLLER_RECOVERED, hardware_radio_index_is_wifi_radio(iRadioInterface), 1-hardware_radio_index_is_wifi_radio(iRadioInterface), 10);
   }

   if ( (! g_bHasSlowUplinkFromController) && (! hardware_radio_index_is_wifi_radio(iRadioInterface)) )
   {
      log_line("[Router] Slow link from controller recovered (received slow packets from controller).");
      if ( 0 == g_LastTimeLostSlowLinkFromController )
         log_line("[Router] Never have had lost the slow link before.");
      else
         log_line("[Router] Last time the slow link was lost was %u ms ago", g_TimeNow - g_LastTimeLostSlowLinkFromController);

      //if ( g_pCurrentModel->osd_params.osd_preferences[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_PREFERENCES_BIT_FLAG_SHOW_CONTROLLER_LINK_LOST_ALARM )
         send_alarm_to_controller(ALARM_ID_LINK_TO_CONTROLLER_RECOVERED, hardware_radio_index_is_wifi_radio(iRadioInterface), 1-hardware_radio_index_is_wifi_radio(iRadioInterface), 10);
   }

   if ( (! g_bHasFastUplinkFromController) && (!g_bHasFastUplinkFromController) )
      adaptive_video_on_uplink_recovered();

   if ( hardware_radio_index_is_wifi_radio(iRadioInterface) )
      g_bHasFastUplinkFromController = true;
   else
      g_bHasSlowUplinkFromController = true;
}

void _check_update_atheros_datarates(u32 linkIndex, int datarateVideoBPS)
{
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioInfo )
         continue;
      if ( (pRadioInfo->iRadioType != RADIO_TYPE_ATHEROS) &&
           (pRadioInfo->iRadioType != RADIO_TYPE_RALINK) )
         continue;
      if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] != (int)linkIndex )
         continue;
      if ( datarateVideoBPS == pRadioInfo->iCurrentDataRateBPS )
         continue;

      radio_rx_pause_interface(i, "Check update Atheros datarate");
      radio_tx_pause_radio_interface(i, "Check update Atheros datarate");

      bool bWasOpenedForWrite = false;
      bool bWasOpenedForRead = false;
      log_line("Vehicle has Atheros cards. Setting the datarates for it.");
      if ( pRadioInfo->openedForWrite )
      {
         bWasOpenedForWrite = true;
         radio_close_interface_for_write(i);
      }
      if ( pRadioInfo->openedForRead )
      {
         bWasOpenedForRead = true;
         radio_close_interface_for_read(i);
      }
      g_TimeNow = get_current_timestamp_ms();
      if ( NULL != g_pProcessStats )
      {
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }

      radio_utils_set_datarate_atheros(g_pCurrentModel, i, datarateVideoBPS, 0);
      
      pRadioInfo->iCurrentDataRateBPS = datarateVideoBPS;

      g_TimeNow = get_current_timestamp_ms();
      if ( NULL != g_pProcessStats )
      {
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }

      if ( bWasOpenedForRead )
         radio_open_interface_for_read(i, RADIO_PORT_ROUTER_UPLINK);

      if ( bWasOpenedForWrite )
         radio_open_interface_for_write(i);

      radio_rx_resume_interface(i);
      radio_tx_resume_radio_interface(i);
      
      g_TimeNow = get_current_timestamp_ms();
      if ( NULL != g_pProcessStats )
      {
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }
   }
}

// returns the packet length if it was reconstructed
int _handle_received_packet_error(int iInterfaceIndex, u8* pData, int nDataLength, int* pCRCOk)
{
   s_uTotalBadPacketsReceived++;

   int iRadioError = get_last_processing_error_code();

   // Try reconstruction
   if ( iRadioError == RADIO_PROCESSING_ERROR_CODE_INVALID_CRC_RECEIVED )
   {
      t_packet_header* pPH = (t_packet_header*)pData;
      pPH->vehicle_id_src = 0;
      int nPacketLength = packet_process_and_check(iInterfaceIndex, pData, nDataLength, pCRCOk);

      if ( nPacketLength > 0 )
      {
         if ( g_TimeNow > g_UplinkInfoRxStats[iInterfaceIndex].timeLastLogWrongRxPacket + 10000 )
         {
            char szBuff[256];
            alarms_to_string(ALARM_ID_RECEIVED_INVALID_RADIO_PACKET_RECONSTRUCTED, 0,0, szBuff);
            log_line("Reconstructed invalid radio packet received. Sending alarm to controller. Alarms: %s, repeat count: %d", szBuff, (int)s_uTotalBadPacketsReceived);
            send_alarm_to_controller(ALARM_ID_RECEIVED_INVALID_RADIO_PACKET_RECONSTRUCTED, 0, 0, 1);
            g_UplinkInfoRxStats[iInterfaceIndex].timeLastLogWrongRxPacket = g_TimeNow;
            s_uTotalBadPacketsReceived = 0;
         }
         return nPacketLength;
      }
   }

   if ( iRadioError == RADIO_PROCESSING_ERROR_CODE_INVALID_CRC_RECEIVED )
   if ( g_TimeNow > g_UplinkInfoRxStats[iInterfaceIndex].timeLastLogWrongRxPacket + 10000 )
   {
      char szBuff[256];
      alarms_to_string(ALARM_ID_RECEIVED_INVALID_RADIO_PACKET, 0,0, szBuff);
      log_line("Invalid radio packet received (bad CRC) (error: %d). Sending alarm to controller. Alarms: %s, repeat count: %d", iRadioError, szBuff, (int)s_uTotalBadPacketsReceived);
      send_alarm_to_controller(ALARM_ID_RECEIVED_INVALID_RADIO_PACKET, 0, 0, 1);
      s_uTotalBadPacketsReceived = 0;
   }

   if ( g_TimeNow > g_UplinkInfoRxStats[iInterfaceIndex].timeLastLogWrongRxPacket + 10000 )
   {
      g_UplinkInfoRxStats[iInterfaceIndex].timeLastLogWrongRxPacket = g_TimeNow;
      log_softerror_and_alarm("Received %d invalid packet(s). Error: %d", s_uTotalBadPacketsReceived, iRadioError );
      s_uTotalBadPacketsReceived = 0;
   } 
   return 0;
}

void process_received_single_radio_packet(int iRadioInterface, u8* pData, int dataLength )
{
   t_packet_header* pPH = (t_packet_header*)pData;
   u32 uVehicleIdSrc = pPH->vehicle_id_src;
   u32 uVehicleIdDest = pPH->vehicle_id_dest;
   u8 uPacketType = pPH->packet_type;
   u8 uPacketFlags = pPH->packet_flags;

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastRadioRxTime = g_TimeNow;

   int iRadioLinkId = g_pCurrentModel->radioInterfacesParams.interface_link_id[iRadioInterface];
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterface);

   g_UplinkInfoRxStats[iRadioInterface].lastReceivedDBM = 1000;
   g_UplinkInfoRxStats[iRadioInterface].lastReceivedDBMNoise = 1000;
   g_UplinkInfoRxStats[iRadioInterface].lastReceivedSNR = 1000;
   g_UplinkInfoRxStats[iRadioInterface].uTimeLastCapture = pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.uLastTimeCapture;

   if ( pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iDbmLast < 500 )
   if ( pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iDbmLast > -500 )
      g_UplinkInfoRxStats[iRadioInterface].lastReceivedDBM = pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iDbmLast;
   
   if ( pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iDbmNoiseLast < 500 )
   if ( pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iDbmNoiseLast > -500 )
      g_UplinkInfoRxStats[iRadioInterface].lastReceivedDBMNoise = pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iDbmNoiseLast;

   if ( pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iSNRLast < 500 )
   if ( pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iSNRLast > -500 )
      g_UplinkInfoRxStats[iRadioInterface].lastReceivedSNR = pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll.iSNRLast;

   g_UplinkInfoRxStats[iRadioInterface].lastReceivedDataRate = pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.nDataRateBPSMCS;

   if ( dataLength <= 0 )
   {
      int bCRCOk = 0;
      dataLength = _handle_received_packet_error(iRadioInterface, pData, dataLength, &bCRCOk);
      if ( dataLength <= 0 )
         return;
   }

   bool bIsPacketFromRelayRadioLink = false;
   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == iRadioLinkId )
      bIsPacketFromRelayRadioLink = true;

   if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterface] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      bIsPacketFromRelayRadioLink = true;

   // Detect if it's a relayed packet from relayed vehicle to controller

   if ( bIsPacketFromRelayRadioLink )
   {
      static bool s_bFirstTimeReceivedDataOnRelayLink = true;

      if ( s_bFirstTimeReceivedDataOnRelayLink )
      {
         log_line("[Relay-RadioIn] Started for first time to receive data on the relay link (radio link %d, expected relayed VID: %u), from VID: %u, packet type: %s, current relay mode: %s",
            iRadioLinkId+1, g_pCurrentModel->relay_params.uRelayedVehicleId,
            uVehicleIdSrc, str_get_packet_type(uPacketType), str_format_relay_mode(g_pCurrentModel->relay_params.uCurrentRelayMode));
         s_bFirstTimeReceivedDataOnRelayLink = false;
      }

      if ( uVehicleIdSrc != g_pCurrentModel->relay_params.uRelayedVehicleId )
         return;

      // Process packets received on relay links separately
      relay_process_received_radio_packet_from_relayed_vehicle(iRadioLinkId, iRadioInterface, pData, dataLength);
      return;
   }

   // Detect if it's a relayed packet from controller to relayed vehicle
   
   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0) )
   if ( uVehicleIdDest == g_pCurrentModel->relay_params.uRelayedVehicleId )
   {
      _mark_link_from_controller_present(iRadioInterface);
  
      relay_process_received_single_radio_packet_from_controller_to_relayed_vehicle(iRadioInterface, pData, pPH->total_length);
      return;
   }

   if ( (0 == uVehicleIdSrc) || (MAX_U32 == uVehicleIdSrc) )
   {
      log_error_and_alarm("Received invalid radio packet: Invalid source vehicle id: %u (vehicle id dest: %u, packet type: %s, %d bytes, %d total bytes, component: %d)",
         uVehicleIdSrc, uVehicleIdDest, str_get_packet_type(uPacketType), dataLength, pPH->total_length, pPH->packet_flags & PACKET_FLAGS_MASK_MODULE);
      return;
   }
   if ( uVehicleIdSrc == g_uControllerId )
      _mark_link_from_controller_present(iRadioInterface);

   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RC )
   {
      if ( ! s_bRCLinkDetected )
      {
         char szBuff[128];
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "touch %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_RC_DETECTED);
         hw_execute_bash_command(szBuff, NULL);
      }
      s_bRCLinkDetected = true;
   }

   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RUBY )
   {
      if ( uVehicleIdDest == g_pCurrentModel->uVehicleId )
         process_received_ruby_message_from_controller(iRadioInterface, pData);
      return;
   }

   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
   {
      if ( uPacketType == PACKET_TYPE_COMMAND )
      {
         int iParamsLength = pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_command);
         t_packet_header_command* pPHC = (t_packet_header_command*)(pData + sizeof(t_packet_header));
         if ( pPHC->command_type == COMMAND_ID_UPLOAD_SW_TO_VEHICLE63 )
            g_uTimeLastCommandSoftwareUpload = g_TimeNow;

         if ( pPHC->command_type == COMMAND_ID_SET_RADIO_LINK_FLAGS )
         if ( pPHC->command_counter != s_uLastCommandChangeDataRateCounter || (g_TimeNow > s_uLastCommandChangeDataRateTime + 4000) )
         if ( iParamsLength == (int)(2*sizeof(u32)+2*sizeof(int)) )
         {
            s_uLastCommandChangeDataRateTime = g_TimeNow;
            s_uLastCommandChangeDataRateCounter = pPHC->command_counter;
            g_TimeLastSetRadioFlagsCommandReceived = g_TimeNow;
            u32* pInfo = (u32*)(pData + sizeof(t_packet_header)+sizeof(t_packet_header_command));
            u32 linkIndex = *pInfo;
            pInfo++;
            u32 linkFlags = *pInfo;
            pInfo++;
            int* piInfo = (int*)pInfo;
            int datarateVideo = *piInfo;
            piInfo++;
            int datarateData = *piInfo;
            log_line("Received new Set Radio Links Flags command. Link %d, Link flags: %s, Datarate %d/%d", linkIndex+1, str_get_radio_frame_flags_description2(linkFlags), datarateVideo, datarateData);
            if ( datarateVideo != g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[linkIndex] ) 
               _check_update_atheros_datarates(linkIndex, datarateVideo);
         }
      }
      //log_line("Received a command: packet type: %d, module: %d, length: %d", pPH->packet_type, pPH->packet_flags & PACKET_FLAGS_MASK_MODULE, pPH->total_length);
      ruby_ipc_channel_send_message(s_fIPCRouterToCommands, pData, dataLength);
      return;
   }

   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
   {
      //log_line("Received an uplink telemetry packet: packet type: %d, module: %d, length: %d", pPH->packet_type, pPH->packet_flags & PACKET_FLAGS_MASK_MODULE, pPH->total_length);
      ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, pData, dataLength);
      return;
   }

   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RC )
   {
      if ( g_pCurrentModel->rc_params.rc_enabled )
         ruby_ipc_channel_send_message(s_fIPCRouterToRC, pData, dataLength);
      return;
   }

   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
   {
      if ( g_pCurrentModel->hasCamera() )
         process_data_tx_video_command(iRadioInterface, pData);
   }
}
