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
#include <stdio.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/commands.h"
#include "../base/models.h"
#include "../base/models_list.h"
#include "../base/ruby_ipc.h"
#include "../base/hardware_files.h"
#include "../base/hardware_procs.h"
#include "../common/radio_stats.h"
#include "../radio/radiolink.h"
#include "../radio/radio_rx.h"
#include "../radio/radiopacketsqueue.h"
#include "periodic_loop.h"
#include "shared_vars.h"
#include "shared_vars_state.h"
#include "timers.h"
#include "ruby_rt_station.h"
#include "radio_links.h"
#include "radio_links_sik.h"
#include "adaptive_video.h"
#include "test_link_params.h"
#include "packets_utils.h"
#include "rx_video_output.h"
#include "processor_rx_audio.h"

u32 s_debugLastFPSTime = 0;
u32 s_debugFramesCount = 0; 

extern t_packet_queue s_QueueRadioPacketsHighPrio;
extern t_packet_queue s_QueueRadioPacketsRegPrio;
extern t_packet_queue s_QueueControlPackets;

void _synchronize_shared_mems()
{
   if ( g_bQuit )
      return;

   //------------------------------
   static u32 s_TimeLastVideoStatsUpdate = 0;

   if ( g_TimeNow >= s_TimeLastVideoStatsUpdate + 200 )
   {
      s_TimeLastVideoStatsUpdate = g_TimeNow;
      memcpy((u8*)g_pSM_VideoDecodeStats, (u8*)(&g_SM_VideoDecodeStats), sizeof(shared_mem_video_stream_stats_rx_processors));
   
      if ( NULL != g_pSM_RouterVehiclesRuntimeInfo )
      {
         for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
         {
            g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i] = g_State.vehiclesRuntimeInfo[i].uVehicleId;
            g_SM_RouterVehiclesRuntimeInfo.bIsVehicleFastUplinkFromControllerLost[i] = g_State.vehiclesRuntimeInfo[i].bIsVehicleFastUplinkFromControllerLost;
            g_SM_RouterVehiclesRuntimeInfo.bIsVehicleSlowUplinkFromControllerLost[i] = g_State.vehiclesRuntimeInfo[i].bIsVehicleSlowUplinkFromControllerLost;
            g_SM_RouterVehiclesRuntimeInfo.bIsDoingRetransmissions[i] = g_State.vehiclesRuntimeInfo[i].bIsDoingRetransmissions;
            g_SM_RouterVehiclesRuntimeInfo.bIsAdaptiveVideoActive[i] = g_State.vehiclesRuntimeInfo[i].bIsAdaptiveVideoActive;
            g_SM_RouterVehiclesRuntimeInfo.uLastTimeReceivedAckFromVehicle[i] = g_State.vehiclesRuntimeInfo[i].uLastTimeReceivedAckFromVehicle;
            g_SM_RouterVehiclesRuntimeInfo.iVehicleClockDeltaMilisec[i] = g_State.vehiclesRuntimeInfo[i].iVehicleClockDeltaMilisec;
            g_SM_RouterVehiclesRuntimeInfo.uCurrentAdaptiveECScheme[i] = g_State.vehiclesRuntimeInfo[i].uCurrentAdaptiveVideoECScheme;
 
            g_SM_RouterVehiclesRuntimeInfo.uAverageCommandRoundtripMiliseconds[i] = g_State.vehiclesRuntimeInfo[i].uAverageCommandRoundtripMiliseconds;
            g_SM_RouterVehiclesRuntimeInfo.uMaxCommandRoundtripMiliseconds[i] = g_State.vehiclesRuntimeInfo[i].uMaxCommandRoundtripMiliseconds;
            g_SM_RouterVehiclesRuntimeInfo.uMinCommandRoundtripMiliseconds[i] = g_State.vehiclesRuntimeInfo[i].uMinCommandRoundtripMiliseconds;
         }
         memcpy((u8*)g_pSM_RouterVehiclesRuntimeInfo, (u8*)&g_SM_RouterVehiclesRuntimeInfo, sizeof(shared_mem_router_vehicles_runtime_info));
      }
   }
   //------------------------------------------

   //-------------------------------------------
   static u32 s_TimeLastControllerRTInfoUpdate = 0;

   if ( g_TimeNow >= s_TimeLastControllerRTInfoUpdate + 70 )
   {
      s_TimeLastControllerRTInfoUpdate = g_TimeNow;
      if ( NULL != g_pSMControllerRTInfo )
         memcpy((u8*)g_pSMControllerRTInfo, (u8*)&g_SMControllerRTInfo, sizeof(controller_runtime_info));
      
      if ( (NULL != g_pSMControllerDebugVideoRTInfo) && (NULL != g_pCurrentModel) )
      if ( g_pControllerSettings->iEnableDebugStats || (g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_FLAG2_SHOW_VIDEO_FRAMES_STATS) )
         memcpy((u8*)g_pSMControllerDebugVideoRTInfo, (u8*)&g_SMControllerDebugVideoRTInfo, sizeof(controller_debug_video_runtime_info));
   }
   //---------------------------------------------
   
   if ( (NULL != g_pCurrentModel) && g_pControllerSettings->iDeveloperMode )
   if ( g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_FLAG_SHOW_STATS_VIDEO_H264_FRAMES_INFO )
   if ( g_TimeNow >= g_SM_VideoFramesStatsOutput.uLastTimeStatsUpdate + 200 )
   {
      update_shared_mem_video_frames_stats( &g_SM_VideoFramesStatsOutput, g_TimeNow);
      //update_shared_mem_video_frames_stats( &g_SM_VideoInfoStatsRadioIn, g_TimeNow);

      if ( NULL != g_pSM_VideoFramesStatsOutput )
         memcpy((u8*)g_pSM_VideoFramesStatsOutput, (u8*)&g_SM_VideoFramesStatsOutput, sizeof(shared_mem_video_frames_stats));
      //if ( NULL != g_pSM_VideoInfoStatsRadioIn )
      //   memcpy((u8*)g_pSM_VideoInfoStatsRadioIn, (u8*)&g_SM_VideoInfoStatsRadioIn, sizeof(shared_mem_video_frames_stats));
   }

   static u32 s_uTimeLastRxHistorySync = 0;

   if ( g_TimeNow >= s_uTimeLastRxHistorySync + 100 )
   {
      s_uTimeLastRxHistorySync = g_TimeNow;
      memcpy((u8*)g_pSM_HistoryRxStats, (u8*)&g_SM_HistoryRxStats, sizeof(shared_mem_radio_stats_rx_hist));
   }
}

void _check_rx_loop_consistency()
{
   if ( g_bSearching )
      return;

   int iAnyBrokeInterface = radio_rx_any_interface_broken();
   if ( iAnyBrokeInterface > 0 )
   {
      if ( hardware_radio_index_is_sik_radio(iAnyBrokeInterface-1) )
         radio_links_flag_reinit_sik_interface(iAnyBrokeInterface-1);
      else
      {
         send_alarm_to_central(ALARM_ID_RADIO_INTERFACE_DOWN, iAnyBrokeInterface-1, 0); 
         radio_links_reinit_radio_interfaces();
         return;
      }
   }

   int iAnyRxTimeouts = radio_rx_any_rx_timeouts();
   if ( iAnyRxTimeouts > 0 )
   {
      static u32 s_uTimeLastAlarmRxTimeout = 0;
      if ( g_TimeNow > s_uTimeLastAlarmRxTimeout + 3000 )
      {
         s_uTimeLastAlarmRxTimeout = g_TimeNow;

         int iCount = radio_rx_get_timeout_count_and_reset(iAnyRxTimeouts-1);
         send_alarm_to_central(ALARM_ID_CONTROLLER_RX_TIMEOUT,(u32)(iAnyRxTimeouts-1), (u32)iCount);
      }
   }

   int iAnyRxErrors = radio_rx_any_interface_bad_packets();
   if ( (iAnyRxErrors > 0) && (!g_bSearching) )
   {
      log_softerror_and_alarm("Has receive Rx errors. Send alarm to central");
      int iError = radio_rx_get_interface_bad_packets_error_and_reset(iAnyRxErrors-1);
      send_alarm_to_central(ALARM_ID_RECEIVED_INVALID_RADIO_PACKET, (u32)iError, 0);
   }

   u32 uMaxRxLoopTime = radio_rx_get_and_reset_max_loop_time();
   if ( (int)uMaxRxLoopTime > get_ControllerSettings()->iDevRxLoopTimeout )
   {
      static u32 s_uTimeLastAlarmRxLoopOverload = 0;
      if ( g_TimeNow > s_uTimeLastAlarmRxLoopOverload + 10000 )
      {
         s_uTimeLastAlarmRxLoopOverload = g_TimeNow;
         u32 uRead = radio_rx_get_and_reset_max_loop_time_read();
         u32 uQueue = radio_rx_get_and_reset_max_loop_time_queue();
         if ( uRead > 0xFFFF ) uRead = 0xFFFF;
         if ( uQueue > 0xFFFF ) uQueue = 0xFFFF;
         
         send_alarm_to_central(ALARM_ID_CONTROLLER_CPU_RX_LOOP_OVERLOAD, uMaxRxLoopTime, uRead | (uQueue<<16));
      }
   }
}

void _check_send_pairing_requests()
{
   if ( ! g_bFirstModelPairingDone )
      return;
   if ( g_bSearching )
      return;
   if ( (g_SM_RadioStats.radio_links[0].totalRxPackets == 0) && (g_SM_RadioStats.radio_links[1].totalRxPackets == 0) )
      return;

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_State.vehiclesRuntimeInfo[i].uVehicleId == 0 )
         continue;
      if ( g_State.vehiclesRuntimeInfo[i].bIsPairingDone )
         continue;

      // Ignore unknown vehicles
      if ( ! controllerHasModelWithId(g_State.vehiclesRuntimeInfo[i].uVehicleId) )
         continue;
      Model* pModel = findModelWithId(g_State.vehiclesRuntimeInfo[i].uVehicleId, 130);
      if ( NULL == pModel )
         continue;
      if ( pModel->is_spectator )
         continue;
      if ( pModel->getVehicleFirmwareType() != MODEL_FIRMWARE_TYPE_RUBY )
         continue;

      bool bExpectedVehicle = false;
      if ( pModel->uVehicleId == g_pCurrentModel->uVehicleId )
         bExpectedVehicle = true;

      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
      if ( g_pCurrentModel->relay_params.uRelayedVehicleId == pModel->uVehicleId )
         bExpectedVehicle = true;
        
      if ( ! bExpectedVehicle )
         continue;

      if ( g_TimeNow < g_State.vehiclesRuntimeInfo[i].uPairingRequestTime + g_State.vehiclesRuntimeInfo[i].uPairingRequestInterval )
         continue;

      g_State.vehiclesRuntimeInfo[i].uPairingRequestId++;
      g_State.vehiclesRuntimeInfo[i].uPairingRequestTime = g_TimeNow;
      if ( g_State.vehiclesRuntimeInfo[i].uPairingRequestInterval < 400 )
         g_State.vehiclesRuntimeInfo[i].uPairingRequestInterval += 10;

      log_line("Send pairing request to VID: %u", pModel->uVehicleId);

      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_RUBY_PAIRING_REQUEST, STREAM_ID_DATA);
      PH.vehicle_id_src = g_uControllerId;
      PH.vehicle_id_dest = pModel->uVehicleId;
      PH.total_length = sizeof(t_packet_header) + 3*sizeof(u32);

      u8 packet[MAX_PACKET_TOTAL_SIZE];
      if ( ! is_sw_version_atleast(pModel, 11, 6) )
      {
         PH.total_length = sizeof(t_packet_header) + sizeof(u32);
         memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
         memcpy(packet + sizeof(t_packet_header), &(g_State.vehiclesRuntimeInfo[i].uPairingRequestId), sizeof(u32));
         log_line("Vehicle SW version (%d.%d) is older, send minimal pairing request.", get_sw_version_major(pModel), get_sw_version_minor(pModel));
      }
      else
      {
         u32 uDeveloperFlags = g_pCurrentModel->uDeveloperFlags;
         if ( (NULL != g_pControllerSettings) && g_pControllerSettings->iDeveloperMode )
            uDeveloperFlags |= DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE;
         else
            uDeveloperFlags &= ~DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE;

         u32 uBoardType = hardware_getBoardType();
         memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
         memcpy(packet + sizeof(t_packet_header), &(g_State.vehiclesRuntimeInfo[i].uPairingRequestId), sizeof(u32));
         memcpy(packet + sizeof(t_packet_header) + sizeof(u32), &uDeveloperFlags, sizeof(u32));
         memcpy(packet + sizeof(t_packet_header) + 2*sizeof(u32), &uBoardType, sizeof(u32));
      }
      if ( 0 == send_packet_to_radio_interfaces(packet, PH.total_length, -1, 1, 500) )
      {
         if ( g_State.vehiclesRuntimeInfo[i].uPairingRequestId < 2 )
            send_alarm_to_central(ALARM_ID_GENERIC_STATUS_UPDATE, ALARM_FLAG_GENERIC_STATUS_SENT_PAIRING_REQUEST, PH.vehicle_id_dest);
         if ( (g_State.vehiclesRuntimeInfo[i].uPairingRequestId % 5) == 0 )
         {
            log_line("Sent pairing request to vehicle (retry count: %u). CID: %u, VID: %u", g_State.vehiclesRuntimeInfo[i].uPairingRequestId, PH.vehicle_id_src, PH.vehicle_id_dest);  
            if ( (g_State.vehiclesRuntimeInfo[i].uPairingRequestId % 10) == 0 )
               send_alarm_to_central(ALARM_ID_GENERIC_STATUS_UPDATE, ALARM_FLAG_GENERIC_STATUS_SENT_PAIRING_REQUEST, PH.vehicle_id_dest);
         }
      }
   }
}

// returns 0 if not ping must be sent now, or the ping interval in ms if it must be inserted
int _must_inject_ping_now()
{
   if ( g_bSearching || (NULL == g_pCurrentModel) || g_pCurrentModel->is_spectator || g_pCurrentModel->b_mustSyncFromVehicle )
      return 0;

   if ( g_pCurrentModel->getVehicleFirmwareType() != MODEL_FIRMWARE_TYPE_RUBY )
      return 0;

   if ( g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_DOWNLINK_ONLY )
      return 0;

   u32 ping_interval_ms = compute_ping_interval_ms(g_pCurrentModel->uModelFlags, g_pCurrentModel->rxtx_sync_type, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags);

   static u32 sl_uTimeLastPingSent = 0;

   if ( (g_TimeNow > sl_uTimeLastPingSent + ping_interval_ms) || (g_TimeNow < sl_uTimeLastPingSent) )
   {
      sl_uTimeLastPingSent = g_TimeNow;   
      return (int)ping_interval_ms;
   }
   return 0;
}

// returns true if it added a packet to the radio queue
bool _check_queue_ping()
{
   if ( (NULL == g_pCurrentModel) || g_bSearching )
      return false;
   int iPingMs = _must_inject_ping_now();
   if ( iPingMs <= 0 )
      return false;

   static u8 s_uPingToSendId = 0xFF;
   static u8 s_uPingToSendLocalRadioLinkId = 0;

   // Send next ping id
   s_uPingToSendId++;


   s_uPingToSendLocalRadioLinkId++;
   if ( s_uPingToSendLocalRadioLinkId >= g_SM_RadioStats.countLocalRadioLinks )
      s_uPingToSendLocalRadioLinkId = 0;
   
   u32 uDestinationVehicleId = g_pCurrentModel->uVehicleId;
   
   // If vehicle is not paired yet, do not send pings
   if ( ! isPairingDoneWithVehicle(uDestinationVehicleId) )
      return false;

   int iVehicleRadioLinkId = g_SM_RadioStats.radio_links[s_uPingToSendLocalRadioLinkId].matchingVehicleRadioLinkId;
   if ( (iVehicleRadioLinkId < 0) || (iVehicleRadioLinkId >= MAX_RADIO_INTERFACES) ||
      ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY ) )
      return false;

   if ( test_link_is_in_progress() )
   if ( test_link_get_test_link_index() == iVehicleRadioLinkId )
      return false;
     
   // Store info about this ping
   g_TimeNow = get_current_timestamp_ms();
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_State.vehiclesRuntimeInfo[i].uVehicleId != 0 )
      {
         g_State.vehiclesRuntimeInfo[i].uLastPingIdSentToVehicleOnLocalRadioLinks[s_uPingToSendLocalRadioLinkId] = s_uPingToSendId;
         g_State.vehiclesRuntimeInfo[i].uTimeLastPingInitiatedToVehicleOnLocalRadioLinks[s_uPingToSendLocalRadioLinkId] = g_TimeNow;
      }
   }

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_RUBY_PING_CLOCK, STREAM_ID_DATA);
   PH.packet_flags |= PACKET_FLAGS_BIT_HIGH_PRIORITY;
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = uDestinationVehicleId;
   PH.total_length = sizeof(t_packet_header) + 5*sizeof(u8);

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   u8 uDummy = 0;
   // u8 ping id, u8 radio link id, u8 dummy
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), &s_uPingToSendId, sizeof(u8));
   memcpy(packet+sizeof(t_packet_header)+sizeof(u8), &s_uPingToSendLocalRadioLinkId, sizeof(u8));
   memcpy(packet+sizeof(t_packet_header)+2*sizeof(u8), &uDummy, sizeof(u8));
   memcpy(packet+sizeof(t_packet_header)+3*sizeof(u8), &uDummy, sizeof(u8));

   u8 uPingFlags = 0;
   if ( g_bOSDPluginsNeedTelemetryStreams )
      uPingFlags |= 0x01;
   memcpy(packet + sizeof(t_packet_header) + 4*sizeof(u8), &uPingFlags, sizeof(u8));

   packets_queue_add_packet_mark_time(&s_QueueRadioPacketsRegPrio, packet);

   return true;
}


void _check_free_storage_space()
{
   static u32 sl_uCountFreeSpaceChecks = 0;
   //static u32 sl_uTimeLastFreeSpaceCheck = 0;
   static bool s_bWaitingForFreeSpaceyAsync = false;
   ControllerSettings* pCS = get_ControllerSettings();
   int iMinFreeKb = 100*1000;

   if ( ! s_bWaitingForFreeSpaceyAsync )
   //if ( ((0 == sl_uCountFreeSpaceChecks) && (g_TimeNow > g_TimeStart+15000)) || (g_TimeNow > sl_uTimeLastFreeSpaceCheck + 60000) )
   if ( (0 == sl_uCountFreeSpaceChecks) && (g_TimeNow > g_TimeStart+10000) )
   {
      sl_uCountFreeSpaceChecks++;
      //sl_uTimeLastFreeSpaceCheck = g_TimeNow;
      
      int iFreeSpaceKb = hardware_get_free_space_kb_async(pCS->iCoresAdjustment? CORE_AFFINITY_OTHERS:-1);
      if ( iFreeSpaceKb < 0 )
      {
         s_bWaitingForFreeSpaceyAsync = false;
         iFreeSpaceKb = hardware_get_free_space_kb();
         if ( (iFreeSpaceKb >= 0) && (iFreeSpaceKb < iMinFreeKb) )
            send_alarm_to_central(ALARM_ID_CONTROLLER_LOW_STORAGE_SPACE, (u32)iFreeSpaceKb/1000, 0);
      }
      else
         s_bWaitingForFreeSpaceyAsync = true;
   }

   if ( s_bWaitingForFreeSpaceyAsync )
   {
      int iFreeSpaceKb = hardware_has_finished_get_free_space_kb_async();
      if ( iFreeSpaceKb >= 0 )
      {
         log_line("Free space: %d kb", iFreeSpaceKb);
         s_bWaitingForFreeSpaceyAsync = false;
         if ( iFreeSpaceKb < iMinFreeKb )
            send_alarm_to_central(ALARM_ID_CONTROLLER_LOW_STORAGE_SPACE, (u32)iFreeSpaceKb/1000, 0);
      }
   }
}

void _check_retransmissions_state()
{
   static u32 s_uTimeLastCheckForRetransmissionsDevAlarm = 0;
   if ( g_TimeNow < s_uTimeLastCheckForRetransmissionsDevAlarm + 250 )
      return;

   s_uTimeLastCheckForRetransmissionsDevAlarm = g_TimeNow;

   if ( (NULL == g_pCurrentModel) || g_bSearching )
      return;
   if ( (! g_pControllerSettings->iDeveloperMode) || g_pCurrentModel->is_spectator )
      return;
   if ( test_link_is_in_progress() || isNegociatingRadioLink() )
      return;
   if ( adaptive_video_is_paused() )
      return;
   if ( adaptive_video_get_last_paused_time() != 0 )
   if ( adaptive_video_get_last_paused_time() > g_TimeNow - 5000 )
      return;

   if ( (g_TimeNow < g_TimeStart + 10000) || (g_TimeNow < test_link_get_last_finish_time() + 4000) )
      return;
   if ( g_TimeNow < g_TimeLastVideoParametersOrProfileChanged + 10000 )
      return;

   controller_runtime_info_vehicle* pRTInfoVehicle = controller_rt_info_get_vehicle_info(&g_SMControllerRTInfo, g_pCurrentModel->uVehicleId);
   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(g_pCurrentModel->uVehicleId);
   if ( (NULL == pRuntimeInfo) || (NULL == pRTInfoVehicle) )
      return;
   if ( ! pRuntimeInfo->bIsDoingRetransmissions )
      return;
   if ( (! pRuntimeInfo->bIsPairingDone) || (g_TimeNow < pRuntimeInfo->uPairingRequestTime + 100) )
      return;
   if ( ! (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS) )
      return;
   if ( rx_video_out_is_stream_output_disabled() )
      return;

   bool bHasAnyVideoPackets = false;
   ProcessorRxVideo* pProcessorRxVideo = NULL;
   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( NULL != g_pVideoProcessorRxList[i] )
      if ( g_pVideoProcessorRxList[i]->m_uVehicleId == g_pCurrentModel->uVehicleId )
      {
         pProcessorRxVideo = g_pVideoProcessorRxList[i];
         if ( 0 != g_pVideoProcessorRxList[i]->getLastestVideoPacketReceiveTime() )
         {
            if ( g_TimeNow < g_pVideoProcessorRxList[i]->getLastTempRetrPauseResume() + SYSTEM_RT_INFO_INTERVALS * g_SMControllerRTInfo.uUpdateIntervalMs )
               return;
            bHasAnyVideoPackets = true;
            break;
         }
      }
   }

   if ( (! bHasAnyVideoPackets) || (NULL == pProcessorRxVideo) )
      return;
   if ( (pProcessorRxVideo->getLastActivationTime() == 0) || (g_TimeNow < pProcessorRxVideo->getLastActivationTime() + 2000) )
      return;
   // Check only the older part of the buffer for skipped blocks. Newer skipped blocks might not have yet been re-requested
   int iHasSkippedBlocksCount = 0;
   u32 uLastSkipBlockTime = 0;
   u32 uLastRetransmissionTime = 0;
   u32 uLastRetransmissionAckTime = 0;

   int iDeltaSlicesFromNow = 50;
   int iRTStartIndex = g_SMControllerRTInfo.iCurrentIndex - iDeltaSlicesFromNow;
   if ( iRTStartIndex < 0 )
      iRTStartIndex += SYSTEM_RT_INFO_INTERVALS;
   
   int iRTIndex = iRTStartIndex;
   for( int i=0; i<SYSTEM_RT_INFO_INTERVALS/2; i++ )
   {
      iDeltaSlicesFromNow++;
      iRTIndex--;
      if ( iRTIndex < 0 )
         iRTIndex = SYSTEM_RT_INFO_INTERVALS - 1;
      if ( g_SMControllerRTInfo.uOutputedVideoBlocksSkippedBlocks[iRTIndex] > 0 )
      {
         iHasSkippedBlocksCount++;
         uLastSkipBlockTime = g_TimeNow - iDeltaSlicesFromNow * g_SMControllerRTInfo.uUpdateIntervalMs;
      }
   }
   if ( 0 == iHasSkippedBlocksCount )
      return;

   int iHasRequestedRetransmissionsCount = 0;
   iRTIndex = g_SMControllerRTInfo.iCurrentIndex;
   iDeltaSlicesFromNow = 0;

   for( int i=0; i<SYSTEM_RT_INFO_INTERVALS; i++ )
   {
      iDeltaSlicesFromNow++;
      iRTIndex--;
      if ( iRTIndex < 0 )
         iRTIndex = SYSTEM_RT_INFO_INTERVALS - 1;
      if ( pRTInfoVehicle->uCountReqRetransmissions[iRTIndex] > 0 )
      {
         iHasRequestedRetransmissionsCount += pRTInfoVehicle->uCountReqRetransmissions[iRTIndex];
         uLastRetransmissionTime = g_TimeNow - iDeltaSlicesFromNow * g_SMControllerRTInfo.uUpdateIntervalMs;
         break;
      }
   }

   int iHasAckRetransmissionsCount = 0;
   iRTIndex = g_SMControllerRTInfo.iCurrentIndex;
   iDeltaSlicesFromNow = 0;
   
   for( int i=0; i<SYSTEM_RT_INFO_INTERVALS; i++ )
   {
      iDeltaSlicesFromNow++;
      iRTIndex--;
      if ( iRTIndex < 0 )
         iRTIndex = SYSTEM_RT_INFO_INTERVALS - 1;
      if ( pRTInfoVehicle->uCountAckRetransmissions[iRTIndex] > 0 )
      {
         iHasAckRetransmissionsCount += pRTInfoVehicle->uCountAckRetransmissions[iRTIndex];
         uLastRetransmissionAckTime = g_TimeNow - iDeltaSlicesFromNow * g_SMControllerRTInfo.uUpdateIntervalMs;
         break;
      }
   }

   static u32 s_uTimeLastSentDevAlarmRetransmissionsToCentral = 0;
   if ( (0 == iHasRequestedRetransmissionsCount) && (0 == iHasAckRetransmissionsCount) )
   {
      if ( 0 == uLastRetransmissionTime )
         uLastRetransmissionTime = pProcessorRxVideo->getLastTimeRequestedRetransmission();
      if ( 0 == uLastRetransmissionAckTime )
         uLastRetransmissionAckTime = pProcessorRxVideo->getLastTimeReceivedRetransmission();
      log_line("Video has skipped blocks (%u ms ago) but checks failed: %d requested retransmissions (last retr id: %u), last was %u ms ago; %d acknowledged retransmissions, last was %u ms ago; and retransmissions are enabled.",
         g_TimeNow - uLastSkipBlockTime,
         iHasRequestedRetransmissionsCount, pProcessorRxVideo->getLastRetransmissionId(), g_TimeNow - uLastRetransmissionTime,
         iHasAckRetransmissionsCount, g_TimeNow - uLastRetransmissionAckTime);
      if ( g_TimeNow > s_uTimeLastSentDevAlarmRetransmissionsToCentral + 10000 )
      {
         s_uTimeLastSentDevAlarmRetransmissionsToCentral = g_TimeNow;
         send_alarm_to_central(ALARM_ID_DEVELOPER_ALARM, ALARM_FLAG_DEVELOPER_ALARM_RETRANSMISSIONS_OFF, 0);
      }
   }
}

void router_periodic_loop()
{
   radio_links_check_reinit_sik_interfaces();

   if ( test_link_is_in_progress() )
      test_link_loop();

   if ( is_audio_processing_started() )
      periodic_loop_audio();

   /*
   bool bInterfcesWithNoData = false;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( ! hardware_radio_is_wifi_radio(pRadioHWInfo) )
         continue;

      if ( g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId >= 0 )
      if ( g_SM_RadioStats.radio_interfaces[i].timeLastRxPacket != 0 )
      if ( g_SM_RadioStats.radio_interfaces[i].timeLastRxPacket < g_TimeNow-2000 )
      if ( g_TimeNow > 2000 )
      {
         bInterfcesWithNoData = true;
      }
   }
   if ( bInterfcesWithNoData )
   if ( g_TimeNow > radio_linkgs_get_last_set_monitor_time() + 2000 )
   if ( g_TimeNow > g_TimeStart + 2000 )
   if ( ! g_bUpdateInProgress )
   if ( ! test_link_is_in_progress() )
      radio_links_set_monitor_mode();
   */

   if ( radio_stats_periodic_update(&g_SM_RadioStats, g_TimeNow) )
   {
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( 0 == g_State.vehiclesRuntimeInfo[i].uVehicleId )
            continue;
         if ( radio_stats_get_reset_stream_lost_packets_flags(&g_SM_RadioStats, g_State.vehiclesRuntimeInfo[i].uVehicleId, STREAM_ID_TELEMETRY) > 0 )
         {
            static u32 s_uLastTimeSentTelemetryLostAlarmToUI =0;
            if ( g_TimeNow > s_uLastTimeSentTelemetryLostAlarmToUI + 3000 )
            {
               s_uLastTimeSentTelemetryLostAlarmToUI = g_TimeNow;
               send_alarm_to_central(ALARM_ID_GENERIC, ALARM_ID_GENERIC_TYPE_MISSED_TELEMETRY_DATA, 0);
            }
         }
      }

      static u32 s_uTimeLastRadioStatsSharedMemSync = 0;

      if ( g_TimeNow >= s_uTimeLastRadioStatsSharedMemSync + 100 )
      {
         s_uTimeLastRadioStatsSharedMemSync = g_TimeNow;
         if ( NULL != g_pSM_RadioStats )
            memcpy((u8*)g_pSM_RadioStats, (u8*)&g_SM_RadioStats, sizeof(shared_mem_radio_stats));
      }

      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( g_SM_RadioStats.radio_streams[i][STREAM_ID_DATA].uVehicleId == 0 )
            continue;
         type_global_state_vehicle_runtime_info* pVehicleRuntimeInfo = getVehicleRuntimeInfo(g_SM_RadioStats.radio_streams[i][STREAM_ID_DATA].uVehicleId);
         if ( NULL == pVehicleRuntimeInfo )
            continue;
         bool bHasRecentData = false;
         u32 uLastRxTime = pVehicleRuntimeInfo->uLastTimeRecvDataFromVehicle;
         if ( g_SM_RadioStats.radio_streams[i][STREAM_ID_DATA].timeLastRxPacket + 1000 > g_TimeNow )
         {
            bHasRecentData = true;
            if ( g_SM_RadioStats.radio_streams[i][STREAM_ID_DATA].timeLastRxPacket > pVehicleRuntimeInfo->uLastTimeRecvDataFromVehicle )
               pVehicleRuntimeInfo->uLastTimeRecvDataFromVehicle = g_SM_RadioStats.radio_streams[i][STREAM_ID_DATA].timeLastRxPacket;
         }
         if ( ! bHasRecentData )
         if ( g_SM_RadioStats.radio_streams[i][STREAM_ID_TELEMETRY].timeLastRxPacket + 1000 > g_TimeNow )
         {
            bHasRecentData = true;
            if ( g_SM_RadioStats.radio_streams[i][STREAM_ID_TELEMETRY].timeLastRxPacket > pVehicleRuntimeInfo->uLastTimeRecvDataFromVehicle )
               pVehicleRuntimeInfo->uLastTimeRecvDataFromVehicle = g_SM_RadioStats.radio_streams[i][STREAM_ID_TELEMETRY].timeLastRxPacket;
         }
         if ( ! bHasRecentData )
         if ( g_SM_RadioStats.radio_streams[i][STREAM_ID_AUDIO].timeLastRxPacket + 1000 > g_TimeNow )
         {
            bHasRecentData = true;
            if ( g_SM_RadioStats.radio_streams[i][STREAM_ID_AUDIO].timeLastRxPacket > pVehicleRuntimeInfo->uLastTimeRecvDataFromVehicle )
               pVehicleRuntimeInfo->uLastTimeRecvDataFromVehicle = g_SM_RadioStats.radio_streams[i][STREAM_ID_AUDIO].timeLastRxPacket;
         }
         if ( ! bHasRecentData )
         if ( g_SM_RadioStats.radio_streams[i][STREAM_ID_VIDEO_1].timeLastRxPacket + 1000 > g_TimeNow )
         {
            bHasRecentData = true;
            if ( g_SM_RadioStats.radio_streams[i][STREAM_ID_VIDEO_1].timeLastRxPacket > pVehicleRuntimeInfo->uLastTimeRecvDataFromVehicle )
               pVehicleRuntimeInfo->uLastTimeRecvDataFromVehicle = g_SM_RadioStats.radio_streams[i][STREAM_ID_VIDEO_1].timeLastRxPacket;
         }
         if ( ! bHasRecentData )
         if ( uLastRxTime > g_TimeNow-2000 )
            log_line("Link to vehicle VID %u is lost (no rx packets. last rx packets: %u ms ago)", pVehicleRuntimeInfo->uVehicleId, g_TimeNow - uLastRxTime);
      }
   }

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = g_TimeNow;

   s_debugFramesCount++;
   if ( g_TimeNow > s_debugLastFPSTime + 500 )
   {
      s_debugLastFPSTime = g_TimeNow;
      s_debugFramesCount = 0;

      if ( g_iGetSiKConfigAsyncResult != 0 )
      {
         char szBuff[256];
         strcpy(szBuff, "SiK config: done.");

         if ( 1 == g_iGetSiKConfigAsyncResult )
         {
            hardware_radio_sik_save_configuration();
            hardware_save_radio_info();
            radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(g_iGetSiKConfigAsyncRadioInterfaceIndex);
         
            char szTmp[256];
            szTmp[0] = 0;
            for( int i=0; i<16; i++ )
            {
               char szB[32];
               sprintf(szB, "%u\n", pRadioHWInfo->uHardwareParamsList[i]);
               strcat(szTmp, szB);
            }
            strcpy(szBuff, szTmp);
         }
         else
            strcpy(szBuff, "Failed to get SiK configuration from device.");

         t_packet_header PH;
         radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_SIK_CONFIG, STREAM_ID_DATA);
         PH.vehicle_id_src = g_uControllerId;
         PH.vehicle_id_dest = g_uControllerId;
         PH.total_length = sizeof(t_packet_header) + strlen(szBuff)+3*sizeof(u8);

         u8 uCommandId = 0;

         u8 packet[MAX_PACKET_TOTAL_SIZE];
         memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
         memcpy(packet+sizeof(t_packet_header), &g_uGetSiKConfigAsyncVehicleLinkIndex, sizeof(u8));
         memcpy(packet+sizeof(t_packet_header) + sizeof(u8), &uCommandId, sizeof(u8));
         memcpy(packet+sizeof(t_packet_header) + 2*sizeof(u8), szBuff, strlen(szBuff)+1);
         radio_packet_compute_crc(packet, PH.total_length);
         if ( ! ruby_ipc_channel_send_message(g_fIPCToCentral, packet, PH.total_length) )
            log_ipc_send_central_error(packet, PH.total_length);
         else
            log_line("Send back to central Sik current config for vehicle radio link %d", (int)g_uGetSiKConfigAsyncVehicleLinkIndex+1);

         g_iGetSiKConfigAsyncResult = 0;
      }
   }

   static u32 s_uTimeLastVideoStatsUpdate = 0;
   if ( g_TimeNow >= s_uTimeLastVideoStatsUpdate + 50 )
   {
      s_uTimeLastVideoStatsUpdate = g_TimeNow;
      memcpy(g_pSM_VideoDecodeStats, &g_SM_VideoDecodeStats, sizeof(shared_mem_video_stream_stats_rx_processors));
   }

   if ( g_TimeNow >= g_SM_RadioRxQueueInfo.uLastMeasureTime + g_SM_RadioRxQueueInfo.uMeasureIntervalMs )
   {
      g_SM_RadioRxQueueInfo.uLastMeasureTime = g_TimeNow;
      g_SM_RadioRxQueueInfo.uCurrentIndex++;
      if ( g_SM_RadioRxQueueInfo.uCurrentIndex >= MAX_RADIO_RX_QUEUE_INFO_VALUES )
         g_SM_RadioRxQueueInfo.uCurrentIndex = 0;
      g_SM_RadioRxQueueInfo.uPendingRxPackets[g_SM_RadioRxQueueInfo.uCurrentIndex] = 0;
      memcpy(g_pSM_RadioRxQueueInfo, &g_SM_RadioRxQueueInfo, sizeof(shared_mem_radio_rx_queue_info));
   }

   _check_free_storage_space();
   _check_send_pairing_requests();
   _check_retransmissions_state();
   
   _synchronize_shared_mems();
   _check_rx_loop_consistency();
   _check_queue_ping();
}