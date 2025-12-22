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

#include "process_radio_in_packets.h"
#include "processor_rx_audio.h"
#include "processor_rx_video.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../base/radio_utils.h"
#include "../base/ctrl_interfaces.h"
#include "../base/ctrl_settings.h"
#include "../base/encr.h"
#include "../base/msp.h"
#include "../base/models_list.h"
#include "../base/commands.h"
#include "../base/ruby_ipc.h"
#include "../base/parser_h264.h"
#include "../base/camera_utils.h"
#include "../common/string_utils.h"
#include "../common/radio_stats.h"
#include "../common/models_connect_frequencies.h"
#include "../common/relay_utils.h"
#include "../radio/radiolink.h"
#include "../radio/radio_duplicate_det.h"
#include "../radio/radio_tx.h"
#include "../radio/radio_rx.h"
#include "ruby_rt_station.h"
#include "relay_rx.h"
#include "test_link_params.h"
#include "shared_vars.h"
#include "timers.h"
#include "process_video_packets.h"
#include "adaptive_video.h"

#define MAX_PACKETS_IN_ID_HISTORY 6

fd_set s_ReadSetRXRadio;
u32 s_uRadioRxReadTimeoutCount = 0;

u32 s_uTotalBadPacketsReceived = 0;

u32 s_TimeLastLoggedSearchingRubyTelemetry = 0;
u32 s_TimeLastLoggedSearchingRubyTelemetryVehicleId = 0;

ParserH264 s_ParserH264RadioInput;

#define MAX_ALARMS_HISTORY 50

u32 s_uLastReceivedAlarmsIndexes[MAX_ALARMS_HISTORY];
u32 s_uTimeLastReceivedAlarm = 0;

void init_radio_rx_structures()
{
   for( int i=0; i<MAX_ALARMS_HISTORY; i++ )
      s_uLastReceivedAlarmsIndexes[i] = MAX_U32;

   s_ParserH264RadioInput.init();
}

int _process_received_ruby_message(int iRuntimeIndex, int iInterfaceIndex, u8* pPacketBuffer)
{
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   
   u8 uPacketType = pPH->packet_type;
   int iTotalLength = pPH->total_length;
   u32 uVehicleIdSrc = pPH->vehicle_id_src;
   u32 uVehicleIdDest = pPH->vehicle_id_dest;
   Model* pModel = findModelWithId(pPH->vehicle_id_src, 380);

   if ( NULL == pModel )
   {
      log_softerror_and_alarm("Received Ruby message for unknown model.");
      return 0;
   }   
   if ( (uPacketType == PACKET_TYPE_SIK_CONFIG) ||
        (uPacketType == PACKET_TYPE_OTA_UPDATE_STATUS) ||
        (uPacketType == PACKET_TYPE_RUBY_RELAY_RADIO_INFO) ||
        (uPacketType == PACKET_TYPE_RUBY_MODEL_SETTINGS) ||
        (uPacketType == PACKET_TYPE_RUBY_MESSAGE) )
   {
      if ( -1 != g_fIPCToCentral )
         ruby_ipc_channel_send_message(g_fIPCToCentral, (u8*)pPH, iTotalLength);
      else
         log_softerror_and_alarm("Received %s message (%d bytes without PH) but there is not channel to central to notify it.", str_get_packet_type(uPacketType), iTotalLength - sizeof(t_packet_header));

      return 0;
   }

   if ( uPacketType == PACKET_TYPE_TEST_RADIO_LINK )
   {
      test_link_process_received_message(iInterfaceIndex, pPacketBuffer);
      return 0;
   }

   if ( uPacketType == PACKET_TYPE_RUBY_RADIO_CONFIG_UPDATED )
   {
      log_line("Received vehicle's current radio configuration from vehicle uid %u, packet size: %d bytes.", uVehicleIdSrc, iTotalLength);
      if ( ! is_sw_version_atleast(pModel, 11, 7) )
      {
         log_line("Vehicle SW version is too old (%d.%d). Ignore this vehicle radio config update.", get_sw_version_major(pModel), get_sw_version_minor(pModel));
         return 0;
      }
      if ( iTotalLength < (int)(sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters) + sizeof(type_radio_links_parameters)) )
      {
         log_softerror_and_alarm("Received vehicle's current radio configuration: invalid packet size. Ignoring.");
         return 0;
      }
      if ( NULL == g_pCurrentModel )
         return 0;
      bool bRadioConfigChangedForCurentModel = false;
      if ( g_pCurrentModel->uVehicleId == uVehicleIdSrc )
      {
         log_line("Received vehicle's radio config is for current vehicle. Update radio config.");
         type_radio_interfaces_parameters radioInt;
         type_radio_links_parameters radioLinks;
         memcpy(&radioInt, pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters), sizeof(type_radio_interfaces_parameters));
         memcpy(&radioLinks, pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters), sizeof(type_radio_links_parameters));

         if ( iTotalLength >= (int)(sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters) + sizeof(type_radio_links_parameters) + sizeof(type_radio_runtime_capabilities_parameters)) )
            memcpy(&g_pCurrentModel->radioRuntimeCapabilities, pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters) + sizeof(type_radio_links_parameters), sizeof(type_radio_runtime_capabilities_parameters));
      
         bRadioConfigChangedForCurentModel = IsModelRadioConfigChanged(&(g_pCurrentModel->radioLinksParams), &(g_pCurrentModel->radioInterfacesParams),
                  &radioLinks, &radioInt);
         log_line("Vehicle's radio configuration %s", bRadioConfigChangedForCurentModel?"has changed.":"is unchanged.");
         if ( bRadioConfigChangedForCurentModel )
            log_line("The current vehicle's radio config for current model and radio links count or radio interfaces count has changed on the vehicle side.");
      }
      else
         log_line("Received vehicle's current radio configuration is for a different vehicle (VID %u) than the current vehicle (VID %u)",
            uVehicleIdSrc, g_pCurrentModel->uVehicleId );
      memcpy(&(pModel->relay_params), pPacketBuffer + sizeof(t_packet_header), sizeof(type_relay_parameters));
      memcpy(&(pModel->radioInterfacesParams), pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters), sizeof(type_radio_interfaces_parameters));
      memcpy(&(pModel->radioLinksParams), pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters), sizeof(type_radio_links_parameters));
      if ( iTotalLength >= (int)(sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters) + sizeof(type_radio_links_parameters) + sizeof(type_radio_runtime_capabilities_parameters)) )
         memcpy(&pModel->radioRuntimeCapabilities, pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters) + sizeof(type_radio_links_parameters), sizeof(type_radio_runtime_capabilities_parameters));
      pModel->validateRadioSettings();
      saveControllerModel(pModel);

      pPH->packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
      ruby_ipc_channel_send_message(g_fIPCToCentral, (u8*)pPH, pPH->total_length);
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

      if ( bRadioConfigChangedForCurentModel )
      {
         log_line("Current vehicle radio config was updated. Reassigning radio links...");
         reasign_radio_links(false);
      }
      log_line("Done processing received vehicle's radio config.");
      return 0;
   }

   if ( uPacketType == PACKET_TYPE_RUBY_PAIRING_CONFIRMATION )
   {      
      u32 uResendCount = 0;
      u16 uVehicleSoftwareVersion = 0;
      if ( iTotalLength >= (int)(sizeof(t_packet_header) + sizeof(u32)) )
         memcpy(&uResendCount, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));
      if ( iTotalLength >= (int)(sizeof(t_packet_header) + sizeof(u32) + sizeof(u16)) )
         memcpy(&uVehicleSoftwareVersion, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32), sizeof(u16));

      log_line("Received pairing confirmation from vehicle (received vehicle resend counter: %u). VID: %u, CID: %u, vehicle SW version: %d.%d", uResendCount, uVehicleIdSrc, uVehicleIdDest, uVehicleSoftwareVersion >> 8, uVehicleSoftwareVersion & 0xFF);

      if ( ! g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsPairingDone )
      {
         ruby_ipc_channel_send_message(g_fIPCToCentral, pPacketBuffer, iTotalLength);
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

         send_alarm_to_central(ALARM_ID_CONTROLLER_PAIRING_COMPLETED, uVehicleIdSrc, uVehicleSoftwareVersion);
         g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsPairingDone = true;
      }
      return 0;
   }

   if ( uPacketType == PACKET_TYPE_RUBY_RADIO_REINITIALIZED )
   {
      log_line("Received message that vehicle radio interfaces where reinitialized.");
      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_BROADCAST_RADIO_REINITIALIZED, STREAM_ID_DATA);
      PH.total_length = sizeof(t_packet_header);
      radio_packet_compute_crc((u8*)&PH, PH.total_length);
      ruby_ipc_channel_send_message(g_fIPCToCentral, (u8*)&PH, PH.total_length);
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      return 0;
   }

   if ( uPacketType == PACKET_TYPE_RUBY_ALARM )
   {
      u32 uAlarmIndex = 0;
      u32 uAlarm = 0;
      u32 uFlags1 = 0;
      u32 uFlags2 = 0;
      char szBuff[256];
      if ( iTotalLength == ((int)(sizeof(t_packet_header) + 3 * sizeof(u32))) )
      {
         memcpy(&uAlarm, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));
         memcpy(&uFlags1, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32), sizeof(u32));
         memcpy(&uFlags2, pPacketBuffer + sizeof(t_packet_header) + 2*sizeof(u32), sizeof(u32));
         alarms_to_string(uAlarm, uFlags1, uFlags2, szBuff);
         log_line("Received alarm from vehicle (old format). Alarm: %s, alarm index: %u", szBuff, uAlarmIndex);
      }
      // New format, version 7.5
      else if ( iTotalLength == ((int)(sizeof(t_packet_header) + 4 * sizeof(u32))) )
      {
         memcpy(&uAlarmIndex, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));
         memcpy(&uAlarm, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32), sizeof(u32));
         memcpy(&uFlags1, pPacketBuffer + sizeof(t_packet_header) + 2*sizeof(u32), sizeof(u32));
         memcpy(&uFlags2, pPacketBuffer + sizeof(t_packet_header) + 3*sizeof(u32), sizeof(u32));
         alarms_to_string(uAlarm, uFlags1, uFlags2, szBuff);
         log_line("Received alarm from vehicle. Alarm: %s, alarm index: %u", szBuff, uAlarmIndex);
      
         bool bDuplicateAlarm = false;
         if ( g_TimeNow > s_uTimeLastReceivedAlarm + 5000 )
             bDuplicateAlarm = false;
         else
         {
            for( int i=0; i<MAX_ALARMS_HISTORY; i++ )
            {
               if ( s_uLastReceivedAlarmsIndexes[i] == uAlarmIndex )
               {
                  bDuplicateAlarm = true;
                  break;
               }
            }  
         }
         if ( bDuplicateAlarm )
         {
            log_line("Duplicate alarm. Ignoring it.");
            return 0;
         }
         
         for( int i=MAX_ALARMS_HISTORY-1; i>0; i-- )
            s_uLastReceivedAlarmsIndexes[i] = s_uLastReceivedAlarmsIndexes[i-1];
         s_uLastReceivedAlarmsIndexes[0] = uAlarmIndex;
      }
      else
      {
         log_softerror_and_alarm("Received invalid alarm from vehicle. Received %d bytes, expected %d bytes", iTotalLength, (int)sizeof(t_packet_header) + 4 * (int)sizeof(u32));
         return 0;
      }

      s_uTimeLastReceivedAlarm = g_TimeNow;
      
      if ( uAlarm & ALARM_ID_LINK_TO_CONTROLLER_LOST )
      {
         if ( uFlags1 )
            g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleFastUplinkFromControllerLost = true;
         if ( uFlags2 )
            g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleSlowUplinkFromControllerLost = true;

         if ( uFlags1 && uFlags2 )
            log_line("Received alarm that vehicle lost both slow and fast connections from controller.");
         else if ( uFlags1 )
            log_line("Received alarm that vehicle lost the fast connection from controller.");
         else if ( uFlags2 )
            log_line("Received alarm that vehicle lost the slow connection from controller.");
         else
            log_line("Received alarm that vehicle lost [unknown] connections from controller.");
      }
      if ( uAlarm & ALARM_ID_LINK_TO_CONTROLLER_RECOVERED )
      {
         if ( uFlags1 )
            g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleFastUplinkFromControllerLost = false;
         if ( uFlags2 )
            g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleSlowUplinkFromControllerLost = false;

         if ( uFlags1 && uFlags2 )
            log_line("Received alarm that vehicle recovered both slow and fast connections from controller.");
         else if ( uFlags1 )
            log_line("Received alarm that vehicle recovered the fast connection from controller.");
         else if ( uFlags2 )
            log_line("Received alarm that vehicle recovered the slow connection from controller.");
         else
            log_line("Received alarm that vehicle recovered [unknown] connections from controller.");
      }
      
      log_line("Sending the alarm to central...");
      ruby_ipc_channel_send_message(g_fIPCToCentral, pPacketBuffer, iTotalLength);
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      log_line("Sent alarm to central");
      return 0;
   }

   if ( uPacketType == PACKET_TYPE_RUBY_PING_CLOCK_REPLY )
   if ( iTotalLength >= (int)(sizeof(t_packet_header) + 2*sizeof(u8) + sizeof(u32)) )
   {
      u8 uPingId = 0;
      u32 uVehicleLocalTimeMs = 0;
      u8 uOriginalLocalRadioLinkId = 0;
      u8 uReplyVehicleLocalRadioLinkId = 0;

      memcpy(&uPingId, pPacketBuffer + sizeof(t_packet_header), sizeof(u8));
      memcpy(&uVehicleLocalTimeMs, pPacketBuffer + sizeof(t_packet_header)+sizeof(u8), sizeof(u32));
      memcpy(&uOriginalLocalRadioLinkId, pPacketBuffer + sizeof(t_packet_header)+sizeof(u8)+sizeof(u32), sizeof(u8));
      if ( pPH->total_length > sizeof(t_packet_header) + 2*sizeof(u8) + sizeof(u32) )
         memcpy(&uReplyVehicleLocalRadioLinkId, pPacketBuffer + sizeof(t_packet_header)+2*sizeof(u8) + sizeof(u32), sizeof(u8));

      g_TimeNow = get_current_timestamp_ms();
      u32 uRoundtripMilis = g_TimeNow - g_State.vehiclesRuntimeInfo[iRuntimeIndex].uTimeLastPingSentToVehicleOnLocalRadioLinks[uOriginalLocalRadioLinkId];
      if ( uPingId == g_State.vehiclesRuntimeInfo[iRuntimeIndex].uLastPingIdSentToVehicleOnLocalRadioLinks[uOriginalLocalRadioLinkId] )
      {
         log_line("Recv ping reply id %d for vehicle link %d, from VID %u, in %u ms RT", uPingId, uOriginalLocalRadioLinkId+1, pPH->vehicle_id_src, uRoundtripMilis);
         controller_rt_info_update_ack_rt_time(&g_SMControllerRTInfo, pPH->vehicle_id_src, g_SM_RadioStats.radio_interfaces[iInterfaceIndex].assignedLocalRadioLinkId, uRoundtripMilis, 0x01);
         if ( uPingId != g_State.vehiclesRuntimeInfo[iRuntimeIndex].uLastPingIdReceivedFromVehicleOnLocalRadioLinks[uOriginalLocalRadioLinkId] )
         {
            g_State.vehiclesRuntimeInfo[iRuntimeIndex].uLastPingIdReceivedFromVehicleOnLocalRadioLinks[uOriginalLocalRadioLinkId] = uPingId;
            g_State.vehiclesRuntimeInfo[iRuntimeIndex].uTimeLastPingReplyReceivedFromVehicleOnLocalRadioLinks[uOriginalLocalRadioLinkId] = g_TimeNow;
            g_State.vehiclesRuntimeInfo[iRuntimeIndex].uPingRoundtripTimeOnLocalRadioLinks[uOriginalLocalRadioLinkId] = uRoundtripMilis;
            if ( uRoundtripMilis < g_State.vehiclesRuntimeInfo[iRuntimeIndex].uMinimumPingTimeMilisec )
            {
               g_State.vehiclesRuntimeInfo[iRuntimeIndex].uMinimumPingTimeMilisec = uRoundtripMilis;
               adjustLinkClockDeltasForVehicleRuntimeIndex(iRuntimeIndex, uRoundtripMilis, uVehicleLocalTimeMs);
            }
         }
         if ( g_pControllerSettings->iDbgPingGraphs )
         {
            int iIndex = g_SMDbgPingStats.iCurrentDataPointIndex[uOriginalLocalRadioLinkId];
            g_SMDbgPingStats.uRTTime[uOriginalLocalRadioLinkId][iIndex] = uRoundtripMilis;
            
            iIndex = (iIndex + 1) % MAX_DBG_PING_DATAPOINTS;
            g_SMDbgPingStats.iCurrentDataPointIndex[uOriginalLocalRadioLinkId] = iIndex;

            if ( NULL == g_pSMDbgPingStats )
               g_pSMDbgPingStats = shared_mem_rctrl_ping_stats_info_open_for_write();
            if ( NULL != g_pSMDbgPingStats )
               memcpy((u8*)g_pSMDbgPingStats, (u8*)&g_SMDbgPingStats, sizeof(shared_mem_ctrl_ping_stats));
         }
      }
      else
         log_line("Recv old ping reply id %d (last sent was %d) for vehicle link %d, from VID %u, in %u ms RT", uPingId, g_State.vehiclesRuntimeInfo[iRuntimeIndex].uLastPingIdSentToVehicleOnLocalRadioLinks[uOriginalLocalRadioLinkId], uOriginalLocalRadioLinkId+1, pPH->vehicle_id_src, uRoundtripMilis);
      return 0;
   }

   if ( uPacketType == PACKET_TYPE_RUBY_LOG_FILE_SEGMENT )
   if ( iTotalLength >= (int)sizeof(t_packet_header) )
   {
      t_packet_header_file_segment* pPHFS = (t_packet_header_file_segment*)(pPacketBuffer + sizeof(t_packet_header));
      log_line("Received a log file segment: file id: %d, segment size: %d, segment index: %d", pPHFS->file_id, (int)pPHFS->segment_size, (int)pPHFS->segment_index);
      if ( pPHFS->file_id == FILE_ID_VEHICLE_LOG )
      {
         char szFile[128];
         snprintf(szFile, sizeof(szFile)/sizeof(szFile[0]), LOG_FILE_VEHICLE, g_pCurrentModel->getShortName());      
         char szPath[256];
         strcpy(szPath, FOLDER_LOGS);
         strcat(szPath, szFile);

         FILE* fd = fopen(szPath, "ab");
         if ( NULL != fd )
         {
            u8* pData = pPacketBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_file_segment);
            fwrite(pData, 1, pPHFS->segment_size, fd);
            fclose(fd);
         }
         // Forward the message as a local control message to ruby_central
         pPH->packet_flags &= (~PACKET_FLAGS_MASK_MODULE);
         pPH->packet_flags |= PACKET_COMPONENT_LOCAL_CONTROL;
         pPH->packet_type = PACKET_TYPE_LOCAL_CONTROL_RECEIVED_VEHICLE_LOG_SEGMENT;
         
         ruby_ipc_channel_send_message(g_fIPCToCentral, pPacketBuffer, pPH->total_length);

         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
         return 0;
      }
      return 0;
   }

   if ( uPacketType == PACKET_TYPE_NEGOCIATE_RADIO_LINKS )
   if ( iTotalLength >= (int)sizeof(t_packet_header) + 2*(int)sizeof(u8) )
   {
      g_uTimeLastNegociateRadioPacket = g_TimeNow;
      // Skip keep alive message type
      u8 uCommand = pPacketBuffer[sizeof(t_packet_header) + sizeof(u8)];

      if ( uCommand != NEGOCIATE_RADIO_KEEP_ALIVE )
      if ( iTotalLength > (int)sizeof(t_packet_header) + 2*(int)sizeof(u8) )
      {
         radio_stats_reset_interfaces_rx_info(&g_SM_RadioStats, "Received a negociate radio links Ack");
         ruby_ipc_channel_send_message(g_fIPCToCentral, pPacketBuffer, iTotalLength);
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      }
      return 0;
   }
   return 0;
}

void _process_received_single_packet_while_searching(int interfaceIndex, u8* pData, int length)
{
   t_packet_header* pPH = (t_packet_header*)pData;
      
   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
   {
      static u32 s_uTimeLastNotifVideoOnSearchToCentral = 0;
      if ( g_TimeNow >= s_uTimeLastNotifVideoOnSearchToCentral + 50 )
      {
         s_uTimeLastNotifVideoOnSearchToCentral = g_TimeNow;
         // Notify central
         t_packet_header PH;
         radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROLL_VIDEO_DETECTED_ON_SEARCH, STREAM_ID_DATA);
         PH.vehicle_id_src = g_uSearchFrequency;
         PH.vehicle_id_dest = 0;
         PH.total_length = sizeof(t_packet_header);

         u8 packet[MAX_PACKET_TOTAL_SIZE];
         memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
         radio_packet_compute_crc(packet, PH.total_length);

         ruby_ipc_channel_send_message(g_fIPCToCentral, packet, PH.total_length);
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      }
      return;
   }

   // Ruby telemetry is always sent to central and rx telemetry (to populate Ruby telemetry shared object)
   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED )
   {
      // v3,v4,v5 ruby telemetry
      bool bIsV3 = false;
      bool bIsV4 = false;
      bool bIsV5 = false;
      bool bIsV6 = false;

      if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v3)))
         bIsV3 = true;
      if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v3) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info)))
         bIsV3 = true;
      if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v3) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)))
         bIsV3 = true;
      if ( bIsV3 )
      {
         t_packet_header_ruby_telemetry_extended_v3* pPHRTE = (t_packet_header_ruby_telemetry_extended_v3*)(pData + sizeof(t_packet_header));
         if ( (pPHRTE->rubyVersion >> 4) > 10 )
            bIsV3 = false;
         if ( (pPHRTE->rubyVersion >> 4) == 10 )
         if ( (pPHRTE->rubyVersion & 0x0F) > 4 )
            bIsV3 = false;
      }

      if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v4)))
         bIsV4 = true;
      if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v4) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info)))
         bIsV4 = true;
      if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v4) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)))
         bIsV4 = true;
      if ( bIsV4 )
      {
         t_packet_header_ruby_telemetry_extended_v4* pPHRTE = (t_packet_header_ruby_telemetry_extended_v4*)(pData + sizeof(t_packet_header));
         if ( (pPHRTE->rubyVersion >> 4) < 10 )
            bIsV4 = false;
         if ( (pPHRTE->rubyVersion >> 4) == 10 )
         if ( (pPHRTE->rubyVersion & 0x0F) < 4 )
            bIsV4 = false;
      }

      if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v5)))
         bIsV5 = true;
      if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v5) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info)))
         bIsV5 = true;
      if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v5) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)))
         bIsV5 = true;
      if ( bIsV5 )
      {
         t_packet_header_ruby_telemetry_extended_v5* pPHRTE = (t_packet_header_ruby_telemetry_extended_v5*)(pData + sizeof(t_packet_header));
         if ( (pPHRTE->rubyVersion >> 4) < 11 )
            bIsV5 = false;
         if ( (pPHRTE->rubyVersion >> 4) == 11 )
         if ( (pPHRTE->rubyVersion & 0x0F) < 2 )
            bIsV5 = false;
      }

      if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v6)))
         bIsV6 = true;
      if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v6) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info)))
         bIsV6 = true;
      if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v6) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)))
         bIsV6 = true;
      if ( bIsV6 )
      {
         t_packet_header_ruby_telemetry_extended_v6* pPHRTE = (t_packet_header_ruby_telemetry_extended_v6*)(pData + sizeof(t_packet_header));
         if ( (pPHRTE->rubyVersion >> 4) < 11 )
            bIsV6 = false;
         if ( (pPHRTE->rubyVersion >> 4) == 11 )
         if ( (pPHRTE->rubyVersion & 0x0F) < 5 )
            bIsV6 = false;
      }

      log_line("Received telemetry while searching. V3=%s V4=%s V5=%s V6=%s", bIsV3?"yes":"no", bIsV4?"yes":"no", bIsV5?"yes":"no", bIsV6?"yes":"no");
      if ( (!bIsV3) && (!bIsV4) && (!bIsV5) && (!bIsV6) )
      {
         t_packet_header_ruby_telemetry_extended_v3* pPHRTE = (t_packet_header_ruby_telemetry_extended_v3*)(pData + sizeof(t_packet_header)); 
         log_softerror_and_alarm("Received unknown telemetry version while searching (on %d Mhz). VID: %u, Version: %d.%d, Size: %d bytes",
            g_uSearchFrequency/1000, pPH->vehicle_id_src, pPHRTE->rubyVersion >> 4, pPHRTE->rubyVersion & 0x0F, pPH->total_length);
         log_softerror_and_alarm("PH: %d bytes, TelemV3: %d bytes, TelemV4: %d bytes, TelemV5: %d bytes, TelemV6: %d bytes, Extra info: %d bytes, Extra info retr: %d bytes",
           sizeof(t_packet_header), sizeof(t_packet_header_ruby_telemetry_extended_v3),
           sizeof(t_packet_header_ruby_telemetry_extended_v4),
           sizeof(t_packet_header_ruby_telemetry_extended_v5),
           sizeof(t_packet_header_ruby_telemetry_extended_v6),
           sizeof(t_packet_header_ruby_telemetry_extended_extra_info),
           sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions));
         log_softerror_and_alarm("Expected PH + PTelemExt + Extra + Retr: %d bytes",
             sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v6) + sizeof(t_packet_header_ruby_telemetry_extended_extra_info) + sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions));
      }
      else
      {
         t_packet_header_ruby_telemetry_extended_v3* pPHRTE = (t_packet_header_ruby_telemetry_extended_v3*)(pData + sizeof(t_packet_header)); 
         log_line("Received telemetry while searching (on %d Mhz). VID: %u, Version: %d.%d, Size: %d bytes",
            g_uSearchFrequency/1000, pPH->vehicle_id_src, pPHRTE->rubyVersion >> 4, pPHRTE->rubyVersion & 0x0F, pPH->total_length);
         log_line("PH: %d bytes, TelemV3: %d bytes, TelemV4: %d bytes, TelemV5: %d bytes, TelemV6: %d bytes, Extra info: %d bytes, Extra info retr: %d bytes",
           sizeof(t_packet_header), sizeof(t_packet_header_ruby_telemetry_extended_v3),
           sizeof(t_packet_header_ruby_telemetry_extended_v4),
           sizeof(t_packet_header_ruby_telemetry_extended_v5),
           sizeof(t_packet_header_ruby_telemetry_extended_v6),
           sizeof(t_packet_header_ruby_telemetry_extended_extra_info),
           sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions));
         log_line("Expected PH + PTelemExt + Extra + Retr: %d bytes",
             sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v6) + sizeof(t_packet_header_ruby_telemetry_extended_extra_info) + sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions));
      }
      if ( bIsV3 )
      {
         t_packet_header_ruby_telemetry_extended_v3* pPHRTE = (t_packet_header_ruby_telemetry_extended_v3*)(pData + sizeof(t_packet_header));
         u8 vMaj = (pPHRTE->rubyVersion) >> 4;
         u8 vMin = (pPHRTE->rubyVersion) & 0x0F;
         if ( (g_TimeNow >= s_TimeLastLoggedSearchingRubyTelemetry + 200) || (s_TimeLastLoggedSearchingRubyTelemetryVehicleId != pPH->vehicle_id_src) )
         {
            s_TimeLastLoggedSearchingRubyTelemetry = g_TimeNow;
            s_TimeLastLoggedSearchingRubyTelemetryVehicleId = pPH->vehicle_id_src;
            char szFreq1[64];
            char szFreq2[64];
            char szFreq3[64];
            strcpy(szFreq1, str_format_frequency(pPHRTE->uRadioFrequenciesKhz[0]));
            strcpy(szFreq2, str_format_frequency(pPHRTE->uRadioFrequenciesKhz[1]));
            strcpy(szFreq3, str_format_frequency(pPHRTE->uRadioFrequenciesKhz[2]));
            log_line("Received a Ruby telemetry packet (version 3) while searching: vehicle ID: %u, version: %d.%d, radio links (%d): %s, %s, %s",
             pPHRTE->uVehicleId, vMaj, vMin, pPHRTE->radio_links_count, 
             szFreq1, szFreq2, szFreq3 );
         }
         if ( -1 != g_fIPCToCentral )
            ruby_ipc_channel_send_message(g_fIPCToCentral, pData, length);
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      }
      if ( bIsV4 )
      {
         t_packet_header_ruby_telemetry_extended_v4* pPHRTE = (t_packet_header_ruby_telemetry_extended_v4*)(pData + sizeof(t_packet_header));
         u8 vMaj = pPHRTE->rubyVersion;
         u8 vMin = pPHRTE->rubyVersion;
         vMaj = vMaj >> 4;
         vMin = vMin & 0x0F;
         if ( (g_TimeNow >= s_TimeLastLoggedSearchingRubyTelemetry + 200) || (s_TimeLastLoggedSearchingRubyTelemetryVehicleId != pPH->vehicle_id_src) )
         {
            s_TimeLastLoggedSearchingRubyTelemetry = g_TimeNow;
            s_TimeLastLoggedSearchingRubyTelemetryVehicleId = pPH->vehicle_id_src;
            char szFreq1[64];
            char szFreq2[64];
            char szFreq3[64];
            strcpy(szFreq1, str_format_frequency(pPHRTE->uRadioFrequenciesKhz[0]));
            strcpy(szFreq2, str_format_frequency(pPHRTE->uRadioFrequenciesKhz[1]));
            strcpy(szFreq3, str_format_frequency(pPHRTE->uRadioFrequenciesKhz[2]));
            log_line("Received a Ruby telemetry packet (version 4) while searching: vehicle ID: %u, version: %d.%d, radio links (%d): %s, %s, %s",
             pPHRTE->uVehicleId, vMaj, vMin, pPHRTE->radio_links_count, 
             szFreq1, szFreq2, szFreq3 );
         }
         if ( -1 != g_fIPCToCentral )
            ruby_ipc_channel_send_message(g_fIPCToCentral, pData, length);
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      }

      if ( bIsV5 )
      {
         t_packet_header_ruby_telemetry_extended_v5* pPHRTE = (t_packet_header_ruby_telemetry_extended_v5*)(pData + sizeof(t_packet_header));
         u8 vMaj = pPHRTE->rubyVersion;
         u8 vMin = pPHRTE->rubyVersion;
         vMaj = vMaj >> 4;
         vMin = vMin & 0x0F;
         if ( (g_TimeNow >= s_TimeLastLoggedSearchingRubyTelemetry + 200) || (s_TimeLastLoggedSearchingRubyTelemetryVehicleId != pPH->vehicle_id_src) )
         {
            s_TimeLastLoggedSearchingRubyTelemetry = g_TimeNow;
            s_TimeLastLoggedSearchingRubyTelemetryVehicleId = pPH->vehicle_id_src;
            char szFreq1[64];
            char szFreq2[64];
            char szFreq3[64];
            strcpy(szFreq1, str_format_frequency(pPHRTE->uRadioFrequenciesKhz[0]));
            strcpy(szFreq2, str_format_frequency(pPHRTE->uRadioFrequenciesKhz[1]));
            strcpy(szFreq3, str_format_frequency(pPHRTE->uRadioFrequenciesKhz[2]));
            log_line("Received a Ruby telemetry packet (version 5) while searching: vehicle ID: %u, version: %d.%d, radio links (%d): %s, %s, %s",
             pPHRTE->uVehicleId, vMaj, vMin, pPHRTE->radio_links_count, 
             szFreq1, szFreq2, szFreq3 );
         }
         if ( -1 != g_fIPCToCentral )
            ruby_ipc_channel_send_message(g_fIPCToCentral, pData, length);
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      }

      if ( bIsV6 )
      {
         t_packet_header_ruby_telemetry_extended_v6* pPHRTE = (t_packet_header_ruby_telemetry_extended_v6*)(pData + sizeof(t_packet_header));
         u8 vMaj = pPHRTE->rubyVersion;
         u8 vMin = pPHRTE->rubyVersion;
         vMaj = vMaj >> 4;
         vMin = vMin & 0x0F;
         if ( (g_TimeNow >= s_TimeLastLoggedSearchingRubyTelemetry + 200) || (s_TimeLastLoggedSearchingRubyTelemetryVehicleId != pPH->vehicle_id_src) )
         {
            s_TimeLastLoggedSearchingRubyTelemetry = g_TimeNow;
            s_TimeLastLoggedSearchingRubyTelemetryVehicleId = pPH->vehicle_id_src;
            char szFreq1[64];
            char szFreq2[64];
            char szFreq3[64];
            strcpy(szFreq1, str_format_frequency(pPHRTE->uRadioFrequenciesKhz[0]));
            strcpy(szFreq2, str_format_frequency(pPHRTE->uRadioFrequenciesKhz[1]));
            strcpy(szFreq3, str_format_frequency(pPHRTE->uRadioFrequenciesKhz[2]));
            log_line("Received a Ruby telemetry packet (version 6) while searching on %s: vehicle ID: %u, version: %d.%d, radio links (%d): %s, %s, %s",
             str_format_frequency(g_uSearchFrequency),
             pPHRTE->uVehicleId, vMaj, vMin, pPHRTE->radio_links_count, 
             szFreq1, szFreq2, szFreq3 );
         }
         if ( -1 != g_fIPCToCentral )
            ruby_ipc_channel_send_message(g_fIPCToCentral, pData, length);
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      }
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_SHORT )
   {
      if ( (g_TimeNow >= s_TimeLastLoggedSearchingRubyTelemetry + 2000) || (s_TimeLastLoggedSearchingRubyTelemetryVehicleId != pPH->vehicle_id_src) )
      {
         s_TimeLastLoggedSearchingRubyTelemetry = g_TimeNow;
         s_TimeLastLoggedSearchingRubyTelemetryVehicleId = pPH->vehicle_id_src;
         log_line("Received a Ruby telemetry short packet while searching: vehicle ID: %u", pPH->vehicle_id_src );
      }
      if ( -1 != g_fIPCToCentral )
         ruby_ipc_channel_send_message(g_fIPCToCentral, pData, length);
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }   
}


// Returns false if packet should not be further processed (first pairing is not done and this is not a telemetry packet)
bool _check_update_first_pairing_done_if_needed(int iInterfaceIndex, u8* pPacketData)
{
   if ( g_bFirstModelPairingDone || g_bSearching || (NULL == pPacketData) )
      return true;

   t_packet_header* pPH = (t_packet_header*)pPacketData;
   if ( pPH->packet_type != PACKET_TYPE_RUBY_TELEMETRY_EXTENDED )
      return false;

   int iTelemetryVersion = 0;
   if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v3)) )
      iTelemetryVersion = 3;
   if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v3) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info)) )
      iTelemetryVersion = 3;
   if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v3) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)) )
      iTelemetryVersion = 3;

   if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v4)) )
      iTelemetryVersion = 4;
   if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v4) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info)) )
      iTelemetryVersion = 4;
   if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v4) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)) )
      iTelemetryVersion = 4;

   if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v5)) )
      iTelemetryVersion = 5;
   if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v5) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info)) )
      iTelemetryVersion = 5;
   if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v5) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)) )
      iTelemetryVersion = 5;

   if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v6)) )
      iTelemetryVersion = 6;
   if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v6) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info)) )
      iTelemetryVersion = 6;
   if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v6) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)) )
      iTelemetryVersion = 6;

   if ( 0 == iTelemetryVersion )
      return false;

   u8 uRubyVersion = 0;
   if ( 3 == iTelemetryVersion )
   {
      t_packet_header_ruby_telemetry_extended_v3* pPHRTE = (t_packet_header_ruby_telemetry_extended_v3*)(pPacketData+sizeof(t_packet_header));
      uRubyVersion = pPHRTE->rubyVersion;
   }
   if ( 4 == iTelemetryVersion )
   {
      t_packet_header_ruby_telemetry_extended_v4* pPHRTE = (t_packet_header_ruby_telemetry_extended_v4*)(pPacketData+sizeof(t_packet_header));
      uRubyVersion = pPHRTE->rubyVersion;
   }
   if ( 5 == iTelemetryVersion )
   {
      t_packet_header_ruby_telemetry_extended_v5* pPHRTE = (t_packet_header_ruby_telemetry_extended_v5*)(pPacketData+sizeof(t_packet_header));
      uRubyVersion = pPHRTE->rubyVersion;
   }
   if ( 6 == iTelemetryVersion )
   {
      t_packet_header_ruby_telemetry_extended_v6* pPHRTE = (t_packet_header_ruby_telemetry_extended_v6*)(pPacketData+sizeof(t_packet_header));
      uRubyVersion = pPHRTE->rubyVersion;
   }

   if ( 0 == uRubyVersion )
      return false;

   u32 uStreamPacketIndex = pPH->stream_packet_idx;
   u32 uVehicleIdSrc = pPH->vehicle_id_src;
   
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
   u32 uCurrentFrequencyKhz = 0;
   if ( NULL != pRadioHWInfo )
   {
      uCurrentFrequencyKhz = pRadioHWInfo->uCurrentFrequencyKhz;
      log_line("Received first radio packet (packet index %u) (from VID %u) on radio interface %d, freq: %s, and first pairing was not done. Do first pairing now.",
       uStreamPacketIndex & PACKET_FLAGS_MASK_STREAM_PACKET_IDX, uVehicleIdSrc, iInterfaceIndex+1, str_format_frequency(uCurrentFrequencyKhz));
   }
   else
      log_line("Received first radio packet (packet index %u) (from VID %u) on radio interface %d and first pairing was not done. Do first pairing now.", uStreamPacketIndex & PACKET_FLAGS_MASK_STREAM_PACKET_IDX, uVehicleIdSrc, iInterfaceIndex+1);

   log_line("Current router local model VID: %u, software ver: %d.%d, b-%d, ptr: %X, models current model: VID: %u, ptr: %X",
      g_pCurrentModel->uVehicleId, get_sw_version_major(g_pCurrentModel), get_sw_version_minor(g_pCurrentModel), get_sw_version_build(g_pCurrentModel), g_pCurrentModel,
      getCurrentModel()->uVehicleId, getCurrentModel());
   g_bFirstModelPairingDone = true;
   g_pCurrentModel->uVehicleId = uVehicleIdSrc;
   g_pCurrentModel->b_mustSyncFromVehicle = true;
   g_pCurrentModel->is_spectator = false;
   deleteAllModels();
   addNewModel(pPH->vehicle_id_src, (uRubyVersion>>4) &0x0F, uRubyVersion & 0x0F);
   replaceModel(0, g_pCurrentModel);
   saveControllerModel(g_pCurrentModel);
   logControllerModels();
   set_model_main_connect_frequency(g_pCurrentModel->uVehicleId, uCurrentFrequencyKhz);
   ruby_set_is_first_pairing_done();

   resetVehicleRuntimeInfo(0);
   g_State.vehiclesRuntimeInfo[0].uVehicleId = uVehicleIdSrc;
   reset_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, uVehicleIdSrc);

   // Notify central
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_FIRST_PAIRING_DONE, STREAM_ID_DATA);
   PH.vehicle_id_src = 0;
   PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
   PH.total_length = sizeof(t_packet_header);

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   radio_packet_compute_crc(packet, PH.total_length);
   if ( ruby_ipc_channel_send_message(g_fIPCToCentral, packet, PH.total_length) )
      log_line("Sent notification to central that first parining was done.");
   else
      log_softerror_and_alarm("Failed to send notification to central that first parining was done.");
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   return true;
}

void _check_update_bidirectional_link_state(int iInterfaceIndex, int iRuntimeIndex, u8 uPacketType, u8 uPacketFlags, u8* pPacketBuffer)
{
   bool bPacketIsAck = false;
   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
   if ( uPacketType == PACKET_TYPE_COMMAND_RESPONSE )
      bPacketIsAck = true;

   if ( (uPacketType == PACKET_TYPE_RUBY_PAIRING_CONFIRMATION) ||
        (uPacketType == PACKET_TYPE_RUBY_PING_CLOCK_REPLY) ||
        (uPacketType == PACKET_TYPE_VIDEO_ADAPTIVE_VIDEO_PARAMS_ACK) ||
        (uPacketType == PACKET_TYPE_TEST_RADIO_LINK) )
      bPacketIsAck = true;

   if ( uPacketType == PACKET_TYPE_NEGOCIATE_RADIO_LINKS )
   {
      u8 uCommand = pPacketBuffer[sizeof(t_packet_header) + sizeof(u8)];
      if ( uCommand != NEGOCIATE_RADIO_KEEP_ALIVE )
         bPacketIsAck = true;
   }

   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
   if ( uPacketFlags & PACKET_FLAGS_BIT_RETRANSMITED )
      bPacketIsAck = true;

   if ( bPacketIsAck )
   {
      g_SM_RadioStats.uLastTimeReceivedAckFromAVehicle = g_TimeNow;
      g_State.vehiclesRuntimeInfo[iRuntimeIndex].uLastTimeReceivedAckFromVehicle = g_TimeNow;

      if ( hardware_radio_index_is_wifi_radio(iInterfaceIndex) )
         g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleFastUplinkFromControllerLost = false;
      else
         g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleSlowUplinkFromControllerLost = false;
   }
}

void _parse_received_msp_packet(type_global_state_vehicle_runtime_info* pRuntimeInfo, u8* pData, int iDataLength)
{
   t_packet_header* pPH = (t_packet_header*)pData;
   t_packet_header_telemetry_msp* pPHMSP = (t_packet_header_telemetry_msp*)(pData + sizeof(t_packet_header));

   if ( (pPHMSP->uSegmentIdAndExtraInfo & 0xFFFF) <= (pRuntimeInfo->mspState.headerTelemetryMSP.uSegmentIdAndExtraInfo & 0xFFFF) )
   {
      if ( (pRuntimeInfo->mspState.headerTelemetryMSP.uSegmentIdAndExtraInfo & 0xFFFF) - (pPHMSP->uSegmentIdAndExtraInfo & 0xFFFF) < 50 )
         return;
   }
   memcpy(&(pRuntimeInfo->mspState.headerTelemetryMSP), pPHMSP, sizeof(t_packet_header_telemetry_msp));

   parse_msp_incoming_data(&(pRuntimeInfo->mspState), pData + sizeof(t_packet_header) + sizeof(t_packet_header_telemetry_msp), pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_telemetry_msp), true);
}

void process_received_single_radio_packet(int iInterfaceIndex, u8* pData, int iDataLength)
{
   t_packet_header* pPH = (t_packet_header*)pData;
   
   u32 uStreamPacketIndex = pPH->stream_packet_idx;
   u32 uVehicleIdSrc = pPH->vehicle_id_src;
   u32 uVehicleIdDest = pPH->vehicle_id_dest;
   u8 uPacketType = pPH->packet_type;
   int iPacketLength = pPH->total_length;
   u8 uPacketFlags = pPH->packet_flags;

   if ( uPacketType == PACKET_TYPE_VIDEO_DATA )
      {
       /*
      t_packet_header_video_segment* pPHVS = (t_packet_header_video_segment*) (pData+sizeof(t_packet_header));
      int iDbgDR = (int) pPH->uCRC;
      log_line("DBG cons %c%d [%u/%02d of %02d] sch %d/%d, framep %d/%d, EOF in %d+%d, %u ms from now, NAL %s%s-%s%s%s, eof?%d DR: %d", 
          (pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED)?'r':'f',
          pPHVS->uH264FrameIndex, pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex,
          pPHVS->uCurrentBlockDataPackets + pPHVS->uCurrentBlockECPackets,
          pPHVS->uCurrentBlockDataPackets, pPHVS->uCurrentBlockECPackets,
          pPHVS->uFramePacketsInfo & 0xFF, pPHVS->uFramePacketsInfo >> 8,
          pPHVS->uVideoStatusFlags2 & 0xFF,
          (pPHVS->uVideoStatusFlags2 >> 16) & 0xFF,
          radio_rx_get_current_frame_end_time() - g_TimeNow,
          (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_START)?"s":"",
          (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_END)?"e":"",
          (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_I)?"i":"",
          (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_P)?"p":"",
          (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_O)?"o":"",
          (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_END_OF_FRAME)?1:0,
          iDbgDR);
      /**/
      }

   if ( g_bFirstModelPairingDone && (!g_bSearching) )
   if ( g_pControllerSettings->iEnableDebugStats )
   {
      if ( 0 == g_SMControllerRTInfo.iRecvVideoDataRate[g_SMControllerRTInfo.iCurrentIndex][iInterfaceIndex] )
         g_SMControllerRTInfo.iRecvVideoDataRate[g_SMControllerRTInfo.iCurrentIndex][iInterfaceIndex] = g_SM_RadioStats.radio_interfaces[iInterfaceIndex].lastRecvDataRateVideo;
      
      if ( g_SM_RadioStats.radio_interfaces[iInterfaceIndex].lastRecvDataRateVideo < 0 )
      if ( g_SM_RadioStats.radio_interfaces[iInterfaceIndex].lastRecvDataRateVideo > g_SMControllerRTInfo.iRecvVideoDataRate[g_SMControllerRTInfo.iCurrentIndex][iInterfaceIndex] )
         g_SMControllerRTInfo.iRecvVideoDataRate[g_SMControllerRTInfo.iCurrentIndex][iInterfaceIndex] = g_SM_RadioStats.radio_interfaces[iInterfaceIndex].lastRecvDataRateVideo;

      if ( g_SM_RadioStats.radio_interfaces[iInterfaceIndex].lastRecvDataRateVideo > 0 )
      if ( g_SM_RadioStats.radio_interfaces[iInterfaceIndex].lastRecvDataRateVideo < g_SMControllerRTInfo.iRecvVideoDataRate[g_SMControllerRTInfo.iCurrentIndex][iInterfaceIndex] )
         g_SMControllerRTInfo.iRecvVideoDataRate[g_SMControllerRTInfo.iCurrentIndex][iInterfaceIndex] = g_SM_RadioStats.radio_interfaces[iInterfaceIndex].lastRecvDataRateVideo;
   }
   if ( NULL != g_pProcessStats )
   {
      // Added in radio_rx thread too
      u32 uGap = g_TimeNow - g_pProcessStats->lastRadioRxTime;
      if ( uGap > 255 )
         uGap = 255;
      if ( uGap > g_SMControllerRTInfo.uRxMaxAirgapSlots2[g_SMControllerRTInfo.iCurrentIndex] )
         g_SMControllerRTInfo.uRxMaxAirgapSlots2[g_SMControllerRTInfo.iCurrentIndex] = uGap;
      g_pProcessStats->lastRadioRxTime = g_TimeNow;
   }

   if ( (!g_bFirstModelPairingDone) && (!g_bSearching) )
   if ( ! _check_update_first_pairing_done_if_needed(iInterfaceIndex, pData) )
      return;

   // Searching ?
   
   if ( g_bSearching )
   {
      _process_received_single_packet_while_searching(iInterfaceIndex, pData, iDataLength);
      return;
   }
   
   if ( (0 == uVehicleIdSrc) || (MAX_U32 == uVehicleIdSrc) )
   {
      log_softerror_and_alarm("Received invalid radio packet: Invalid source vehicle id: %u (vehicle id dest: %u, packet type: %s, %d bytes, %d total bytes, component: %d)",
         uVehicleIdSrc, uVehicleIdDest, str_get_packet_type(uPacketType), iDataLength, pPH->total_length, pPH->packet_flags & PACKET_FLAGS_MASK_MODULE);
      return;
   }

   Model* pModel = findModelWithId(uVehicleIdSrc, 355);
   if ( NULL == pModel )
   {
      log_softerror_and_alarm("Received radio packet from unknown vehicle while not searching.");
      logCurrentVehiclesRuntimeInfo();
      return;
   }

   bool bNewVehicleId = true;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( 0 == g_State.vehiclesRuntimeInfo[i].uVehicleId )
         break;
      if ( g_State.vehiclesRuntimeInfo[i].uVehicleId == uVehicleIdSrc )
      {
         bNewVehicleId = false;
         break;
      }
   }

   if ( bNewVehicleId )
   {
      log_line("Start receiving radio packets from new VID: %u (name: %s, sw version: %d.%d, b-%d)", uVehicleIdSrc,
         ((NULL != pModel)?pModel->getLongName():"N/A"),
         ((NULL != pModel)?get_sw_version_major(pModel):0),
         ((NULL != pModel)?get_sw_version_minor(pModel):0),
         ((NULL != pModel)?get_sw_version_build(pModel):0));
      int iCountUsed = 0;
      int iFirstFree = -1;
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( g_State.vehiclesRuntimeInfo[i].uVehicleId != 0 )
            iCountUsed++;

         if ( g_State.vehiclesRuntimeInfo[i].uVehicleId == 0 )
         if ( -1 == iFirstFree )
            iFirstFree = i;
      }

      logCurrentVehiclesRuntimeInfo();

      if ( (iCountUsed >= MAX_CONCURENT_VEHICLES) || (-1 == iFirstFree) )
      {
         log_softerror_and_alarm("Radio in: No more room in vehicles runtime info structure for new vehicle VID %u", uVehicleIdSrc);
         char szTmp[256];
         szTmp[0] = 0;
         for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
         {
            char szT[32];
            sprintf(szT, "%u", g_State.vehiclesRuntimeInfo[i].uVehicleId);
            if ( 0 != i )
               strcat(szTmp, ", ");
            strcat(szTmp, szT);
         }
         log_softerror_and_alarm("Radio in: Current vehicles in vehicles runtime info: [%s]", szTmp);                   
      }
      else
      {
         resetVehicleRuntimeInfo(iFirstFree);
         g_State.vehiclesRuntimeInfo[iFirstFree].uVehicleId = uVehicleIdSrc;
         adaptive_video_on_new_vehicle(iFirstFree);
      }
      logCurrentVehiclesRuntimeInfo();
   }

   int iRuntimeIndex = getVehicleRuntimeIndex(uVehicleIdSrc);
   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(uVehicleIdSrc);

   if ( (-1 == iRuntimeIndex) || (NULL == pRuntimeInfo) )
   {
      log_softerror_and_alarm("Received radio packet from unknown runtime index while not searching.");
      logCurrentVehiclesRuntimeInfo();
      return;
   }

   
   if ( ! g_State.vehiclesRuntimeInfo[iRuntimeIndex].bReceivedAnyData )
   {
      g_State.vehiclesRuntimeInfo[iRuntimeIndex].bReceivedAnyData = true;
      if ( NULL == pModel )
         log_line("Start receiving radio packets from VID %u, runtime index %d, (NULL model)", uVehicleIdSrc, iRuntimeIndex);
      else
         log_line("Start receiving radio packets from VID %u, runtime index %d, (name: %s, sw version: b-%d)", uVehicleIdSrc, iRuntimeIndex, pModel->getLongName(), get_sw_version_build(pModel));
   }
   _check_update_bidirectional_link_state(iInterfaceIndex, iRuntimeIndex, uPacketType, uPacketFlags, pData);

   // Detect vehicle restart (stream packets indexes are starting again from zero or low value )

   u32 uStreamId = (uStreamPacketIndex)>>PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   if ( uStreamId >= MAX_RADIO_STREAMS )
      uStreamId = 0;

   if ( radio_dup_detection_is_vehicle_restarted(uVehicleIdSrc) )
   {
      log_line("RX thread detected vehicle restart (received stream packet index %u for stream %d, on interface index %d; Max received stream packet index was at %u for stream %d",
       (uStreamPacketIndex & PACKET_FLAGS_MASK_STREAM_PACKET_IDX), uStreamId, iInterfaceIndex+1, radio_dup_detection_get_max_received_packet_index_for_stream(uVehicleIdSrc, uStreamId), uStreamId );
      g_pProcessStats->alarmFlags = PROCESS_ALARM_RADIO_STREAM_RESTARTED;
      g_pProcessStats->alarmTime = g_TimeNow;

      // Reset pairing info so that pairing is done again with this vehicle
      resetPairingStateForVehicleRuntimeInfo(iRuntimeIndex);
      adaptive_video_reset_state(uVehicleIdSrc);

      for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
      {
         if ( NULL == g_pVideoProcessorRxList[i] )
            break;
         if ( g_pVideoProcessorRxList[i]->m_uVehicleId == uVehicleIdSrc )
         {
            g_pVideoProcessorRxList[i]->fullResetState("vehicle restart detected on Rx radio");
            break;
         }
      }
      radio_dup_detection_reset_vehicle_restarted_flag(uVehicleIdSrc);
      log_line("Done processing RX thread detected vehicle restart.");
   }
   
   // For data streams, discard packets that are too older than the newest packet received on that stream for that vehicle
   
   u32 uMaxStreamPacketIndex = radio_dup_detection_get_max_received_packet_index_for_stream(uVehicleIdSrc, uStreamId);
   
   if ( (uStreamId < STREAM_ID_VIDEO_1) && (uMaxStreamPacketIndex > 100) )
   if ( (uStreamPacketIndex & PACKET_FLAGS_MASK_STREAM_PACKET_IDX) < uMaxStreamPacketIndex-100 )
   {
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
      {
         t_packet_header_command_response* pPHCR = (t_packet_header_command_response*)(pData + sizeof(t_packet_header));

         if ( pPHCR->origin_command_type == COMMAND_ID_GET_ALL_PARAMS_ZIP )
         {
            log_line("Discarding received command response for get zip model settings, command counter %d, command retry counter %d, on radio interface %d. Discard because this packet index is %u and last received packet index for this data stream (stream %d) is %u",
                pPHCR->origin_command_counter, pPHCR->origin_command_resend_counter,
                iInterfaceIndex+1,
                (pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX), uStreamId, uMaxStreamPacketIndex);
         }
      }
      return;
   }

   bool bIsRelayedPacket = false;

   // Received packet from different vehicle ?

   if ( (NULL != g_pCurrentModel) && (uVehicleIdSrc != g_pCurrentModel->uVehicleId) )
   {
      // Relayed packet?
      if ( !g_bSearching )
      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
      if ( (g_pCurrentModel->relay_params.uRelayedVehicleId != 0) && (g_pCurrentModel->relay_params.uRelayedVehicleId != MAX_U32) )
      if ( uVehicleIdSrc == g_pCurrentModel->relay_params.uRelayedVehicleId )
         bIsRelayedPacket = true;

      /*
      if ( bIsRelayedPacket )
      {
         relay_rx_process_single_received_packet( iInterfaceIndex, pData, iDataLength);
         return;
      }
      */

      // Discard all packets except telemetry comming from wrong models, if the first pairing was done

      if ( !bIsRelayedPacket )
      {
         // Ruby telemetry is always sent to central, so that wrong vehicle or relayed vehicles can be detected
         if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
         if ( (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED) || 
              (uPacketType == PACKET_TYPE_FC_TELEMETRY) ||
              (uPacketType == PACKET_TYPE_FC_TELEMETRY_EXTENDED) )
         {
            if ( -1 != g_fIPCToCentral )
               ruby_ipc_channel_send_message(g_fIPCToCentral, pData, iDataLength);
            if ( NULL != g_pProcessStats )
               g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
         }         
         return;
      }
   }

   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RUBY )
   {
      if ( bIsRelayedPacket )
      {
         if ( (uPacketType == PACKET_TYPE_RUBY_PAIRING_CONFIRMATION) ||
              (uPacketType == PACKET_TYPE_RUBY_PING_CLOCK_REPLY) ||
              (uPacketType == PACKET_TYPE_VIDEO_ADAPTIVE_VIDEO_PARAMS_ACK) ||
              (uPacketType == PACKET_TYPE_NEGOCIATE_RADIO_LINKS) ||
              (uPacketType == PACKET_TYPE_TEST_RADIO_LINK) ||
              (uPacketType == PACKET_TYPE_RUBY_MESSAGE) )
            _process_received_ruby_message(iRuntimeIndex, iInterfaceIndex, pData);
         return;
      }
      _process_received_ruby_message(iRuntimeIndex, iInterfaceIndex, pData);
      return;
   }

   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
   {
      if ( (uPacketType != PACKET_TYPE_COMMAND_RESPONSE) || (iPacketLength < (int)sizeof(t_packet_header) + (int)sizeof(t_packet_header_command_response)) )
         return;

      t_packet_header_command_response* pPHCR = (t_packet_header_command_response*)(pData + sizeof(t_packet_header));

      if ( ((pPHCR->origin_command_type & COMMAND_TYPE_MASK) == COMMAND_ID_SET_VIDEO_PARAMETERS) ||
           ((pPHCR->origin_command_type & COMMAND_TYPE_MASK) == COMMAND_ID_GET_CORE_PLUGINS_INFO)||
           ((pPHCR->origin_command_type & COMMAND_TYPE_MASK) == COMMAND_ID_GET_ALL_PARAMS_ZIP) )
      {
         log_line("Recv command response. Reset H264 stream detected profile and level for VID %u", pPH->vehicle_id_src);
         shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, pPH->vehicle_id_src);
         reset_video_stream_stats_detected_info(pSMVideoStreamInfo);
         g_TimeLastVideoParametersOrProfileChanged = g_TimeNow;
      }

      if ( pPHCR->origin_command_type == COMMAND_ID_GET_ALL_PARAMS_ZIP )
      {
         log_line("[Router] Received command response for get zip model settings, command counter %d, command retry counter %d, %d extra bytes, packet index: %u, on radio interface %d.",
            pPHCR->origin_command_counter, pPHCR->origin_command_resend_counter,
            pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_command_response),
            pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX,
            iInterfaceIndex+1);
      }
      
      if ( bIsRelayedPacket )
         return;

      ruby_ipc_channel_send_message(g_fIPCToCentral, pData, iDataLength);
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      
      if ( pRuntimeInfo->uLastCommandIdSent != MAX_U32 )
      if ( pRuntimeInfo->uLastCommandIdRetrySent != MAX_U32 )
      if ( pPHCR->origin_command_counter == pRuntimeInfo->uLastCommandIdSent )
      if ( pPHCR->origin_command_resend_counter == pRuntimeInfo->uLastCommandIdRetrySent ) 
      {
         pRuntimeInfo->uLastCommandIdSent = MAX_U32;
         pRuntimeInfo->uLastCommandIdRetrySent = MAX_U32;
         addCommandRTTimeToRuntimeInfo(pRuntimeInfo, g_TimeNow - pRuntimeInfo->uTimeLastCommandIdSent);
      }

      if ( pPHCR->origin_command_type == COMMAND_ID_SET_RADIO_LINK_FLAGS )
      if ( pPHCR->command_response_flags & COMMAND_RESPONSE_FLAGS_OK)
      {
         if ( pPHCR->origin_command_counter != g_uLastInterceptedCommandCounterToSetRadioFlags )
         {
            log_line("Intercepted command response Ok to command sent to set radio flags and radio data datarate on radio link %u to %u bps (%d datarate).", g_uLastRadioLinkIndexForSentSetRadioLinkFlagsCommand+1, getRealDataRateFromRadioDataRate(g_iLastRadioLinkDataRateSentForSetRadioLinkFlagsCommand, g_pCurrentModel->radioLinksParams.link_radio_flags[g_uLastRadioLinkIndexForSentSetRadioLinkFlagsCommand], 1), g_iLastRadioLinkDataRateSentForSetRadioLinkFlagsCommand);
            g_uLastInterceptedCommandCounterToSetRadioFlags = pPHCR->origin_command_counter;

            for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
            {
               radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);

               if ( (NULL == pRadioHWInfo) || controllerIsCardDisabled(pRadioHWInfo->szMAC) )
                  continue;

               int nRadioLinkId = g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId;
               if ( nRadioLinkId < 0 || nRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count )
                  continue;
               if ( nRadioLinkId != (int)g_uLastRadioLinkIndexForSentSetRadioLinkFlagsCommand )
                  continue;

               if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[nRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
                  continue;

               if ( (pRadioHWInfo->iRadioType == RADIO_TYPE_ATHEROS) ||
                    (pRadioHWInfo->iRadioType == RADIO_TYPE_RALINK) )
               {
                  int nRateTx = g_iLastRadioLinkDataRateSentForSetRadioLinkFlagsCommand;
                  update_atheros_card_datarate(g_pCurrentModel, i, nRateTx, g_pProcessStats);
                  g_TimeNow = get_current_timestamp_ms();
               }
            }
         }
      }
      return;
   }

   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
   {
      if ( ! bIsRelayedPacket )
      if ( ! g_bSearching )
      if ( g_bFirstModelPairingDone )
      if ( -1 != g_fIPCToTelemetry )
      {
         #ifdef LOG_RAW_TELEMETRY
         if ( uPacketType == PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD )
         {
            t_packet_header_telemetry_raw* pPHTR = (t_packet_header_telemetry_raw*)(pData + sizeof(t_packet_header));
            log_line("[Raw_Telem] Received raw telem packet from vehicle, index %u, %d / %d bytes",
               pPHTR->telem_segment_index, pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_telemetry_raw), pPH->total_length);
         }
         #endif
         if ( (uPacketType != PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS ) &&
              (uPacketType != PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY) )
            ruby_ipc_channel_send_message(g_fIPCToTelemetry, pData, iDataLength);
      }

      if ( is_sw_version_atleast(pModel, 11, 6) )
      {
         if ( uPacketType == PACKET_TYPE_FC_TELEMETRY )
         {
            g_State.vehiclesRuntimeInfo[iRuntimeIndex].uTimeLastRecvFCTelemetryFC = g_TimeNow;
            t_packet_header_fc_telemetry* pPHFCTelem = (t_packet_header_fc_telemetry*) (pData + sizeof(t_packet_header));
            memcpy(&g_State.vehiclesRuntimeInfo[iRuntimeIndex].headerFCTelemetry, pPHFCTelem, sizeof(t_packet_header_fc_telemetry));
         }
         if ( uPacketType == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED )
         {
            g_State.vehiclesRuntimeInfo[iRuntimeIndex].uTimeLastRecvRubyTelemetryExtended = g_TimeNow;
            t_packet_header_ruby_telemetry_extended_v6* pPHRubyTelem = (t_packet_header_ruby_telemetry_extended_v6*) (pData + sizeof(t_packet_header));
            memcpy(&g_State.vehiclesRuntimeInfo[iRuntimeIndex].headerRubyTelemetryExtended, pPHRubyTelem, sizeof(t_packet_header_ruby_telemetry_extended_v6));
         }
         if ( uPacketType == PACKET_TYPE_RUBY_TELEMETRY_SHORT )
         {
            g_State.vehiclesRuntimeInfo[iRuntimeIndex].uTimeLastRecvRubyTelemetryShort = g_TimeNow;
            t_packet_header_ruby_telemetry_short* pPHFCTelemShort = (t_packet_header_ruby_telemetry_short*) (pData + sizeof(t_packet_header));
            memcpy(&g_State.vehiclesRuntimeInfo[iRuntimeIndex].headerRubyTelemetryShort, pPHFCTelemShort, sizeof(t_packet_header_ruby_telemetry_short));
         }
         if ( uPacketType == PACKET_TYPE_TELEMETRY_MSP )
         {
            _parse_received_msp_packet(pRuntimeInfo, pData, iDataLength);
         }
      }

      if ( uPacketType == PACKET_TYPE_RUBY_TELEMETRY_SHORT )
      {
         t_packet_header_ruby_telemetry_short* pPHRTShort = (t_packet_header_ruby_telemetry_short*) (pData + sizeof(t_packet_header));

         if ( pPHRTShort->uRubyFlags & FLAG_RUBY_TELEMETRY_HAS_FAST_UPLINK_FROM_CONTROLLER )
            g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleFastUplinkFromControllerLost = false;
         else
            g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleFastUplinkFromControllerLost = true;
         
         if ( pPHRTShort->uRubyFlags & FLAG_RUBY_TELEMETRY_HAS_SLOW_UPLINK_FROM_CONTROLLER )
            g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleSlowUplinkFromControllerLost = false;
         else
            g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleSlowUplinkFromControllerLost = true;
      }

      if ( uPacketType == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED )
      {
         if ( get_sw_version_build(pModel) >= 305 )
         {
            t_packet_header_ruby_telemetry_extended_v6* pPHRTE = (t_packet_header_ruby_telemetry_extended_v6*) (pData + sizeof(t_packet_header));
            if ( pPHRTE->uRubyFlags & FLAG_RUBY_TELEMETRY_HAS_FAST_UPLINK_FROM_CONTROLLER )
               g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleFastUplinkFromControllerLost = false;
            else
               g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleFastUplinkFromControllerLost = true;
            
            if ( pPHRTE->uRubyFlags & FLAG_RUBY_TELEMETRY_HAS_SLOW_UPLINK_FROM_CONTROLLER )
               g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleSlowUplinkFromControllerLost = false;
            else
               g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleSlowUplinkFromControllerLost = true;
         }
         else if ( get_sw_version_build(pModel) >= 290 )
         {
            t_packet_header_ruby_telemetry_extended_v5* pPHRTE = (t_packet_header_ruby_telemetry_extended_v5*) (pData + sizeof(t_packet_header));
            if ( pPHRTE->uRubyFlags & FLAG_RUBY_TELEMETRY_HAS_FAST_UPLINK_FROM_CONTROLLER )
               g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleFastUplinkFromControllerLost = false;
            else
               g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleFastUplinkFromControllerLost = true;
            
            if ( pPHRTE->uRubyFlags & FLAG_RUBY_TELEMETRY_HAS_SLOW_UPLINK_FROM_CONTROLLER )
               g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleSlowUplinkFromControllerLost = false;
            else
               g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleSlowUplinkFromControllerLost = true;
         }
         else if ( get_sw_version_build(pModel) > 281 )
         {
            t_packet_header_ruby_telemetry_extended_v4* pPHRTE = (t_packet_header_ruby_telemetry_extended_v4*) (pData + sizeof(t_packet_header));
            if ( pPHRTE->uRubyFlags & FLAG_RUBY_TELEMETRY_HAS_FAST_UPLINK_FROM_CONTROLLER )
               g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleFastUplinkFromControllerLost = false;
            else
               g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleFastUplinkFromControllerLost = true;
            
            if ( pPHRTE->uRubyFlags & FLAG_RUBY_TELEMETRY_HAS_SLOW_UPLINK_FROM_CONTROLLER )
               g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleSlowUplinkFromControllerLost = false;
            else
               g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleSlowUplinkFromControllerLost = true;
         }
      }
      bool bSendToCentral = false;
      bool bSendRelayedTelemetry = false;

      // Ruby telemetry and FC telemetry from relayed vehicle is always sent to central to detect the relayed vehicle
      if ( bIsRelayedPacket )
      if ( (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED) ||
           (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_SHORT) ||
           (uPacketType == PACKET_TYPE_FC_TELEMETRY) ||
           (uPacketType == PACKET_TYPE_FC_TELEMETRY_EXTENDED) ||
           (uPacketType == PACKET_TYPE_TELEMETRY_MSP))
          bSendToCentral = true;

      if ( bIsRelayedPacket )
      if ( relay_controller_must_forward_telemetry_from(g_pCurrentModel, uVehicleIdSrc) )
         bSendRelayedTelemetry = true;

      if ( (! bIsRelayedPacket) || bSendRelayedTelemetry )
      if ( (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_SHORT) ||
           (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED) ||
           (uPacketType == PACKET_TYPE_FC_TELEMETRY) ||
           (uPacketType == PACKET_TYPE_FC_TELEMETRY_EXTENDED) ||
           (uPacketType == PACKET_TYPE_FC_RC_CHANNELS) ||
           (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY ) ||
           (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS ) ||
           (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_DEV_VIDEO_BITRATE_HISTORY) ||
           (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_VIDEO_INFO_STATS) ||
           (uPacketType == PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD) ||
           (uPacketType == PACKET_TYPE_DEBUG_INFO) ||
           (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_RADIO_RX_HISTORY) ||
           (uPacketType == PACKET_TYPE_TELEMETRY_MSP))
         bSendToCentral = true;

      if ( bSendToCentral )
      {
         ruby_ipc_channel_send_message(g_fIPCToCentral, pData, iDataLength);
        
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
         return;
      }

      if ( bIsRelayedPacket )
         return;

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

      return;
   }

   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RC )
   {
      if ( bIsRelayedPacket )
         return;

      if ( -1 != g_fIPCToRC )
         ruby_ipc_channel_send_message(g_fIPCToRC, pData, iDataLength);
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

      return;
   }

   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
   {
      if ( NULL == pModel )
      {
         for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
         {
            if ( (uVehicleIdSrc == 0) || (g_SM_RadioStats.radio_streams[i][0].uVehicleId == uVehicleIdSrc) || (g_SM_RadioStats.radio_streams[i][STREAM_ID_VIDEO_1].uVehicleId == uVehicleIdSrc) )
               g_SM_RadioStats.radio_streams[i][STREAM_ID_VIDEO_1].totalRxBytes = 0;
         }
         return;
      }
      process_received_video_component_packet(iInterfaceIndex, pData, iDataLength);
      return;
   }

   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_AUDIO )
   {
      if ( bIsRelayedPacket || g_bSearching || (uPacketType != PACKET_TYPE_AUDIO_SEGMENT) )
         return;

      process_received_audio_packet(pData);
      return;
   }
} 
