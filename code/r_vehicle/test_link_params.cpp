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

#include "test_link_params.h"
#include "../base/models_list.h"
#include "../base/ruby_ipc.h"
#include "../base/radio_utils.h"
#include "../base/hardware_camera.h"
#include "../base/hardware_procs.h"
#include "../radio/radiopackets2.h"
#include "../radio/radio_rx.h"
#include "../radio/radio_tx.h"
#include "../common/string_utils.h"
#include "shared_vars.h"
#include "timers.h"
#include "radio_links.h"
#include "adaptive_video.h"
#include "video_sources.h"

int s_iTestLinkState = TEST_LINK_STATE_NONE;
int s_iTestLinkIndex = -1;
int s_iTestLinkCurrentRunCount = 0;
type_radio_links_parameters s_RadioLinksParamsToTest;
type_radio_links_parameters s_RadioLinksParamsOriginal;
u32 s_uOriginalVideoBitrate = 0;

bool s_bTestLinkOnlyFreqChanged = false;
bool s_bTestLinkCurrentTestSucceeded = false;

int s_iTestLinkCurrentStepSendCount = 0;
u32 s_uTimeLastUpdateCurrentState = 0;
u32 s_uTimeEndCurrentStep = 0;

u32 s_uTimeLastReceivedTestLinkStartMessage = 0;

pthread_t s_pThreadTestLinkWorker;
bool s_bApplyRadioParamsInProgress = false;
bool s_bTestLinkPausedInterfaces = false;
bool s_bMustResumeRadioInterfacesForRead[MAX_RADIO_INTERFACES];
bool s_bMustResumeRadioInterfacesForWrite[MAX_RADIO_INTERFACES];
bool s_bMustReopenRadioInterfacesForRead[MAX_RADIO_INTERFACES];
bool s_bMustReopenRadioInterfacesForWrite[MAX_RADIO_INTERFACES];

u32 s_uTestLinkCountPingsReceived = 0;

bool test_link_is_in_progress()
{
   return (s_iTestLinkState != TEST_LINK_STATE_NONE);
}

bool test_link_is_applying_radio_params()
{
   if ( s_iTestLinkState == TEST_LINK_STATE_NONE )
      return false;
   return s_bApplyRadioParamsInProgress;
}

void _test_link_end_and_notify()
{
   log_line("[TestLink-%d] Save new tested radio parameters to current model", s_iTestLinkCurrentRunCount);
   char szPrefix[32];
   sprintf(szPrefix, "[TestLink-%d]", s_iTestLinkCurrentRunCount);
   int iCountDiff = g_pCurrentModel->logVehicleRadioLinkDifferences(szPrefix, &s_RadioLinksParamsOriginal, &s_RadioLinksParamsToTest);

   memcpy(&(g_pCurrentModel->radioLinksParams), &s_RadioLinksParamsToTest, sizeof(type_radio_links_parameters));
   g_pCurrentModel->validateRadioSettings();
   
   log_line("[TestLink-%d] Current radio power levels:", s_iTestLinkCurrentRunCount);
   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      log_line("[TestLink-%d] Radio interface %d raw power level: %d", s_iTestLinkCurrentRunCount, i+1, g_pCurrentModel->radioInterfacesParams.interface_raw_power[i]);
      
   log_line("[TestLink-%d] Current radio interfaces count: RTL8812AU: %d, RTL8812EU: %d, RTL8733BU: %d, Atheros: %d",
      s_iTestLinkCurrentRunCount, hardware_radio_has_rtl8812au_cards(), hardware_radio_has_rtl8812eu_cards(), hardware_radio_has_rtl8733bu_cards(), hardware_radio_has_atheros_cards());

   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
      log_line("[TestLink-%d] Radio Link %d data rates: video %d, downlink: %d, uplink %d",
          s_iTestLinkCurrentRunCount, s_iTestLinkIndex+1, 
          g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[i],
          g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[i],
          g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[i]);
   }
   if ( (iCountDiff > 0) || s_bTestLinkCurrentTestSucceeded )
   {
      bool bUpdated = g_pCurrentModel->validateVideoProfilesMaxVideoBitrate();
      u32 uMaxVideoBitrate = g_pCurrentModel->getMaxVideoBitrateSupportedForCurrentRadioLinks();
      if ( s_uOriginalVideoBitrate > uMaxVideoBitrate )
         s_uOriginalVideoBitrate = uMaxVideoBitrate;

      if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS > uMaxVideoBitrate )
         bUpdated = true;
      if ( bUpdated )
      {
         log_line("[TestLink-%d] Must adjust current video profile bitrate (%.2f Mbps) to max allowed on current links: %.1f Mbps",
            s_iTestLinkCurrentRunCount,
            g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS/1000.0/1000.0, uMaxVideoBitrate/1000.0/1000.0);
      
         type_video_link_profile oldVideoLinkProfiles[MAX_VIDEO_LINK_PROFILES];
         memcpy(&(oldVideoLinkProfiles[0]), &(g_pCurrentModel->video_link_profiles[0]), MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile));
         g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS = uMaxVideoBitrate;
         if ( g_pCurrentModel->validateVideoProfilesMaxVideoBitrate() )
            log_line("[TestLink-%d] Video bitrate was adjusted again to match max allowed on links");
         video_sources_on_changed_video_params(&(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile]), &(g_pCurrentModel->video_params), &oldVideoLinkProfiles[0]);
         log_line("[TestLink-%d] Done adjusting current video profile bitrate (%.1f Mbps) to max allowed on current links: %.1f Mbps",
            s_iTestLinkCurrentRunCount,
            g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS/1000.0/1000.0, uMaxVideoBitrate/1000.0/1000.0);
      }
      log_line("[TestLink-%d] Saving final model.", s_iTestLinkCurrentRunCount);
      saveCurrentModel();
   }

   adaptive_video_check_update_params();
   if ( 0 != s_uOriginalVideoBitrate )
   {
      video_sources_set_video_bitrate(s_uOriginalVideoBitrate, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iIPQuantizationDelta, "TestLinkParams OnEnd");
      s_uOriginalVideoBitrate = 0;
   }

   log_line("[TestLink-%d] Notify other components to reload model (end test)", s_iTestLinkCurrentRunCount);
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY | (MODEL_CHANGED_RADIO_LINK_PARAMS<<8) | (((u32)s_iTestLinkIndex)<<16);
   PH.total_length = sizeof(t_packet_header);
   radio_packet_compute_crc((u8*)&PH, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

   ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)&PH, PH.total_length);
   ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)&PH, PH.total_length);
   if ( g_pCurrentModel->rc_params.rc_enabled )
      ruby_ipc_channel_send_message(s_fIPCRouterToRC, (u8*)&PH, PH.total_length);
   
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

   s_iTestLinkState = TEST_LINK_STATE_NONE;
}

void _test_link_pause_interfaces()
{
   if ( s_bTestLinkPausedInterfaces )
      return;
   
   s_bTestLinkPausedInterfaces = true;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      s_bMustResumeRadioInterfacesForRead[i] = false;
      s_bMustResumeRadioInterfacesForWrite[i] = false;

      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != s_iTestLinkIndex )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( pRadioHWInfo->openedForWrite )
      {
         radio_tx_pause_radio_interface(i, "TestLinkParams");
         s_bMustResumeRadioInterfacesForWrite[i] = true;
      }
      if ( pRadioHWInfo->openedForRead )
      {
         radio_rx_pause_interface(i, "TestLinkParams");
         s_bMustResumeRadioInterfacesForRead[i] = true;
      }
   }
}

void _test_link_resume_interfaces()
{
   if ( ! s_bTestLinkPausedInterfaces )
      return;

   s_bTestLinkPausedInterfaces = false;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != s_iTestLinkIndex )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      
      if ( s_bMustResumeRadioInterfacesForWrite[i] )
         radio_tx_resume_radio_interface(i);
      s_bMustResumeRadioInterfacesForWrite[i] = false;
      
      if ( s_bMustResumeRadioInterfacesForRead[i] )
         radio_rx_resume_interface(i);
      s_bMustResumeRadioInterfacesForRead[i] = false;
   }
}


void _test_link_close_interfaces()
{
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      s_bMustReopenRadioInterfacesForRead[i] = false;
      s_bMustReopenRadioInterfacesForWrite[i] = false;
      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != s_iTestLinkIndex )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( pRadioHWInfo->openedForWrite )
      {
         // Do not close interface if only frequency changed
         if ( ! s_bTestLinkOnlyFreqChanged )
         {
            radio_close_interface_for_write(i);
            g_SM_RadioStats.radio_interfaces[i].openedForWrite = 0;
            s_bMustReopenRadioInterfacesForWrite[i] = true;
         }
      }
      if ( pRadioHWInfo->openedForRead )
      {
         // Do not close interface if only frequency changed
         if ( ! s_bTestLinkOnlyFreqChanged )
         {
            radio_close_interface_for_read(i);
            g_SM_RadioStats.radio_interfaces[i].openedForRead = 0;
            s_bMustReopenRadioInterfacesForRead[i] = true;
         }
      }

      if ( (pRadioHWInfo->iRadioType == RADIO_TYPE_ATHEROS) ||
           (pRadioHWInfo->iRadioType == RADIO_TYPE_RALINK) )
         s_uTimeEndCurrentStep = g_TimeNow + 2*TIMEOUT_TEST_LINK_STATE_START;
   }
}

void _test_link_reopen_interfaces()
{
   log_line("[TestLink-%d] Reopening radio interfaces for vehicle radio link %d...", s_iTestLinkCurrentRunCount, s_iTestLinkIndex+1);
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != s_iTestLinkIndex )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;

      if (s_bMustReopenRadioInterfacesForRead[i] )
      {
         s_bMustReopenRadioInterfacesForRead[i] = false;

         int iRes = 0;
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[s_iTestLinkIndex] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
            iRes = radio_open_interface_for_read(i, RADIO_PORT_ROUTER_DOWNLINK);
         else
            iRes = radio_open_interface_for_read(i, RADIO_PORT_ROUTER_UPLINK);

         if ( iRes > 0 )
            g_SM_RadioStats.radio_interfaces[i].openedForRead = 1;

      }

      if (s_bMustReopenRadioInterfacesForWrite[i] )
      {
         s_bMustReopenRadioInterfacesForWrite[i] = false;
         if ( radio_open_interface_for_write(i) > 0 )
            g_SM_RadioStats.radio_interfaces[i].openedForWrite = 1;
      }
   }
   log_line("[TestLink-%d] Done reopen radio interfaces for vehicle radio link %d.", s_iTestLinkCurrentRunCount, s_iTestLinkIndex+1);
}


static void * _thread_test_link_worker_apply(void *argument)
{
   log_line("[TestLink-%d] Started worker thread to update radio interfaces for vehicle radio link %d.", s_iTestLinkCurrentRunCount, s_iTestLinkIndex+1);
   hw_log_current_thread_attributes("test link apply");
   char szPrefix[32];
   sprintf(szPrefix, "[TestLink-%d]", s_iTestLinkCurrentRunCount);
   g_pCurrentModel->logVehicleRadioLinkDifferences(szPrefix, &s_RadioLinksParamsOriginal, &s_RadioLinksParamsToTest);

   radio_links_apply_settings(g_pCurrentModel, s_iTestLinkIndex, &s_RadioLinksParamsOriginal, &s_RadioLinksParamsToTest);
        
   log_line("[TestLink-%d] Finished worker thread to update radio interfaces for vehicle radio link %d.", s_iTestLinkCurrentRunCount, s_iTestLinkIndex+1);
   s_bApplyRadioParamsInProgress = false;
   return NULL;
}


static void * _thread_test_link_worker_revert(void *argument)
{
   log_line("[TestLink-%d] Started worker thread to revert radio interfaces for vehicle radio link %d.", s_iTestLinkCurrentRunCount, s_iTestLinkIndex+1);
   hw_log_current_thread_attributes("test link revert");
   char szPrefix[32];
   sprintf(szPrefix, "[TestLink-%d]", s_iTestLinkCurrentRunCount);   
   g_pCurrentModel->logVehicleRadioLinkDifferences(szPrefix, &s_RadioLinksParamsToTest, &s_RadioLinksParamsOriginal);

   radio_links_apply_settings(g_pCurrentModel, s_iTestLinkIndex, &s_RadioLinksParamsToTest, &s_RadioLinksParamsOriginal);
        
   log_line("[TestLink-%d] Finished worker thread to revert radio interfaces for vehicle radio link %d.", s_iTestLinkCurrentRunCount, s_iTestLinkIndex+1);
   s_bApplyRadioParamsInProgress = false;
   return NULL;
}


void _test_link_switch_to_state(int iNewState, u32 uTimeout)
{
   char szStateOld[64];
   char szStateNew[64];
   test_link_state_get_state_string(s_iTestLinkState, szStateOld);
   test_link_state_get_state_string(iNewState, szStateNew);

   if ( s_iTestLinkState == iNewState )
      return;

   s_iTestLinkState = iNewState;
   s_iTestLinkCurrentStepSendCount = 0;
   s_uTimeLastUpdateCurrentState = g_TimeNow;
   s_uTimeEndCurrentStep = g_TimeNow + uTimeout;

   log_line("[TestLink-%d] Switch from state [%s] to state [%s]. Timesout %u ms from now", s_iTestLinkCurrentRunCount, szStateOld, szStateNew, uTimeout);

   if ( s_iTestLinkState == TEST_LINK_STATE_START )
   {
      s_bTestLinkCurrentTestSucceeded = false;
      s_uTimeLastUpdateCurrentState = g_TimeNow-100;

      adaptive_video_check_update_params();

      s_uOriginalVideoBitrate = video_sources_get_last_set_video_bitrate();
      video_sources_set_video_bitrate(DEFAULT_LOWEST_ALLOWED_ADAPTIVE_VIDEO_BITRATE, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iIPQuantizationDelta, "TestLinkParams Start");
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_APPLY_RADIO_PARAMS )
   {
      s_bApplyRadioParamsInProgress = true;

      _test_link_pause_interfaces();
      if ( ! s_bTestLinkOnlyFreqChanged )
         _test_link_close_interfaces();

      memcpy(&(g_pCurrentModel->radioLinksParams), &s_RadioLinksParamsToTest, sizeof(type_radio_links_parameters));
      g_pCurrentModel->validateRadioSettings();

      if ( 0 != pthread_create(&s_pThreadTestLinkWorker, NULL, &_thread_test_link_worker_apply, NULL) )
      {
         if ( ! s_bTestLinkOnlyFreqChanged )
            _test_link_reopen_interfaces();
         _test_link_resume_interfaces();

         s_bApplyRadioParamsInProgress = false;
         s_bTestLinkCurrentTestSucceeded = false;
         _test_link_switch_to_state(TEST_LINK_STATE_ENDED, TIMEOUT_TEST_LINK_STATE_END);
         return;
      }
      pthread_detach(s_pThreadTestLinkWorker);
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_PING_UPLINK )
   {
      log_line("[TestLink-%d] Reset pings received count.", s_iTestLinkCurrentRunCount);
      s_uTestLinkCountPingsReceived = 0;
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_PING_DOWNLINK )
   {
      log_line("[TestLink-%d] Reset pings received count.", s_iTestLinkCurrentRunCount);
      s_uTestLinkCountPingsReceived = 0;
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_REVERT_RADIO_PARAMS )
   {
      s_bApplyRadioParamsInProgress = true;

      _test_link_pause_interfaces();
      if ( ! s_bTestLinkOnlyFreqChanged )
         _test_link_close_interfaces();

      memcpy(&(g_pCurrentModel->radioLinksParams), &s_RadioLinksParamsOriginal, sizeof(type_radio_links_parameters));
      g_pCurrentModel->validateRadioSettings();

      if ( 0 != pthread_create(&s_pThreadTestLinkWorker, NULL, &_thread_test_link_worker_revert, NULL) )
      {
         if ( ! s_bTestLinkOnlyFreqChanged )
            _test_link_reopen_interfaces();
         _test_link_resume_interfaces();

         s_bTestLinkCurrentTestSucceeded = false;
         s_bApplyRadioParamsInProgress = false;
         _test_link_switch_to_state(TEST_LINK_STATE_ENDED, TIMEOUT_TEST_LINK_STATE_END);
         return;
      }
      pthread_detach(s_pThreadTestLinkWorker);
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_END )
   {
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_ENDED )
   {
      adaptive_video_check_update_params();
      if ( 0 != s_uOriginalVideoBitrate )
      {
         video_sources_set_video_bitrate(s_uOriginalVideoBitrate, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iIPQuantizationDelta, "TestLinkParams Ended");
         s_uOriginalVideoBitrate = 0;
      }

      s_iTestLinkState = TEST_LINK_STATE_NONE;
      return;
   }
}

void _test_link_send_start_confirmation_message()
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = g_uControllerId;
   PH.total_length = sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE + sizeof(type_radio_links_parameters);

   u8 uBuffer[1024];
   memcpy(uBuffer, (u8*)&PH, sizeof(t_packet_header));
   uBuffer[sizeof(t_packet_header)] = PACKET_TYPE_TEST_RADIO_LINK_PROTOCOL_VERSION;
   uBuffer[sizeof(t_packet_header)+1] = PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE;
   uBuffer[sizeof(t_packet_header)+2] = (u8)s_iTestLinkIndex;
   uBuffer[sizeof(t_packet_header)+3] = (u8)s_iTestLinkCurrentRunCount;
   uBuffer[sizeof(t_packet_header)+4] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START;
   memcpy(uBuffer + sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE, (u8*)&s_RadioLinksParamsToTest, sizeof(type_radio_links_parameters));

   packets_queue_add_packet(&g_QueueRadioPacketsOut, uBuffer);
   log_line("[TestLink-%d] Sent start message confirmation (count %u) to controller", s_iTestLinkCurrentRunCount, s_iTestLinkCurrentStepSendCount);
}

void test_link_process_received_message(int iInterfaceIndex, u8* pPacketBuffer)
{
   if ( NULL == pPacketBuffer )
   {
      log_softerror_and_alarm("[TestLink-%d] Process received message from controller on radio interface %d but message is null", s_iTestLinkCurrentRunCount, iInterfaceIndex+1);
      return;
   }

   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   if ( (pPH->packet_type != PACKET_TYPE_TEST_RADIO_LINK) || (pPH->total_length < sizeof(t_packet_header)+PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE) )
   {
      log_softerror_and_alarm("[TestLink-%d] Process received message from controller on radio interface %d but message is invalid", s_iTestLinkCurrentRunCount, iInterfaceIndex+1);
      return;
   }

   int iProtocol = pPacketBuffer[sizeof(t_packet_header)];
   int iHeaderSize = pPacketBuffer[sizeof(t_packet_header)+1];
   int iRadioLinkId = pPacketBuffer[sizeof(t_packet_header)+2];
   int iRunCount = pPacketBuffer[sizeof(t_packet_header)+3];
   int iCmdType = pPacketBuffer[sizeof(t_packet_header)+4];
   //int iCmdLen = pPH->total_length - sizeof(t_packet_header)-PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE;

   if ( (iProtocol != PACKET_TYPE_TEST_RADIO_LINK_PROTOCOL_VERSION) || (iHeaderSize != PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE) )
   {
      log_softerror_and_alarm("[TestLink-%d] Process received message from controller (%s) on radio interface %d but message is invalid (size or index). Protocol: %d, header size: %d, total data length: %d",
         iRunCount, str_get_packet_test_link_command(iCmdType), iInterfaceIndex+1, iProtocol, iHeaderSize, pPH->total_length - sizeof(t_packet_header));
      return;
   }

   char szBuff[128];
   test_link_state_get_state_string(s_iTestLinkState, szBuff);

   log_line("[TestLink-%d] Process received command (%s) from controller on radio interface %d, current state is: %s", s_iTestLinkCurrentRunCount, str_get_packet_test_link_command(iCmdType), iInterfaceIndex+1, szBuff);


   if ( iCmdType == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START )
   {
      if ( s_iTestLinkState == TEST_LINK_STATE_START )
      {
         s_uTimeLastReceivedTestLinkStartMessage = g_TimeNow;
         log_line("[TestLink-%d] Ignored duplicate command (%s) from controller on radio interface %d", s_iTestLinkCurrentRunCount, str_get_packet_test_link_command(iCmdType), iInterfaceIndex+1);
         return;
      }
      if ( s_iTestLinkState != TEST_LINK_STATE_NONE )
         return;

      s_iTestLinkIndex = iRadioLinkId;
      s_iTestLinkCurrentRunCount = iRunCount;
      s_uTimeLastReceivedTestLinkStartMessage = g_TimeNow;

      memcpy(&s_RadioLinksParamsOriginal, &(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));
      memcpy(&s_RadioLinksParamsToTest, pPacketBuffer + sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE, sizeof(type_radio_links_parameters));
      char szPrefix[32];
      sprintf(szPrefix, "[TestLink-%d]", s_iTestLinkCurrentRunCount);
      int iDifferences = g_pCurrentModel->logVehicleRadioLinkDifferences(szPrefix, &s_RadioLinksParamsOriginal, &s_RadioLinksParamsToTest);

      s_bTestLinkOnlyFreqChanged = false;
      if ( (1 == iDifferences) && (s_RadioLinksParamsOriginal.link_frequency_khz[s_iTestLinkIndex] != s_RadioLinksParamsToTest.link_frequency_khz[s_iTestLinkIndex]) )
         s_bTestLinkOnlyFreqChanged = true;

      _test_link_switch_to_state(TEST_LINK_STATE_START, TIMEOUT_TEST_LINK_STATE_START);
      return;
   }

   if ( iCmdType == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_PING_UPLINK )
   {
      if ( s_iTestLinkState != TEST_LINK_STATE_PING_UPLINK )
      {
         log_line("[TestLink-%d] Ignored command (%s) from controller on radio interface %d", s_iTestLinkCurrentRunCount, str_get_packet_test_link_command(iCmdType), iInterfaceIndex+1);
         return;
      }

      s_uTestLinkCountPingsReceived++;

      u32 uTmpPingsSentByController;
      u32 uTmpPingsReceivedByController;
      memcpy( &uTmpPingsSentByController, pPacketBuffer + sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE, sizeof(u32));
      memcpy( &uTmpPingsReceivedByController, pPacketBuffer + sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE + sizeof(u32), sizeof(u32));

      log_line("[TestLink-%d] Process received ping uplink from controller (count received so far: %u). Controller sent %u pings, controller received %u pings replies.",
         s_iTestLinkCurrentRunCount, s_uTestLinkCountPingsReceived, uTmpPingsSentByController, uTmpPingsReceivedByController );

      s_iTestLinkCurrentStepSendCount++;

      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
      PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      PH.vehicle_id_dest = g_uControllerId;
      PH.total_length = sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE + 2*sizeof(u32);

      u8 uBuffer[1024];
      memcpy(uBuffer, (u8*)&PH, sizeof(t_packet_header));
      uBuffer[sizeof(t_packet_header)] = PACKET_TYPE_TEST_RADIO_LINK_PROTOCOL_VERSION;
      uBuffer[sizeof(t_packet_header)+1] = PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE;
      uBuffer[sizeof(t_packet_header)+2] = (u8)s_iTestLinkIndex;
      uBuffer[sizeof(t_packet_header)+3] = (u8)s_iTestLinkCurrentRunCount;
      uBuffer[sizeof(t_packet_header)+4] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_PING_UPLINK;
      u32 uTmp = s_iTestLinkCurrentStepSendCount;
      memcpy(uBuffer + sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE, &uTmp, sizeof(u32));
      memcpy(uBuffer + sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE + sizeof(u32), &s_uTestLinkCountPingsReceived, sizeof(u32));

      packets_queue_add_packet(&g_QueueRadioPacketsOut, uBuffer);
      log_line("[TestLink-%d] Sent ping uplink reply message (count %u) to controller", s_iTestLinkCurrentRunCount, s_iTestLinkCurrentStepSendCount);

      return;
   }

   if ( iCmdType == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_PING_DOWNLINK )
   {
      if ( (s_iTestLinkState != TEST_LINK_STATE_PING_UPLINK) && (s_iTestLinkState != TEST_LINK_STATE_PING_DOWNLINK) )
      {
         log_line("[TestLink-%d] Ignored command (%s) from controller on radio interface %d", s_iTestLinkCurrentRunCount, str_get_packet_test_link_command(iCmdType), iInterfaceIndex+1);
         return;
      }

      if ( s_iTestLinkState == TEST_LINK_STATE_PING_UPLINK )
      {
         if ( s_uTestLinkCountPingsReceived == 0 )
         {
            log_line("[TestLink-%d] Received ping downlink command but ping uplink was not confirmed first. Ending flow.", s_iTestLinkCurrentRunCount);
            s_bTestLinkCurrentTestSucceeded = false;
            _test_link_switch_to_state(TEST_LINK_STATE_ENDED, TIMEOUT_TEST_LINK_STATE_END);
            return;
         }
         _test_link_switch_to_state(TEST_LINK_STATE_PING_DOWNLINK, TIMEOUT_TEST_LINK_STATE_PING);
      }
      s_uTestLinkCountPingsReceived++;

      u32 uTmpPingsSentByController;
      u32 uTmpPingsReceivedByController;
      memcpy( &uTmpPingsSentByController, pPacketBuffer + sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE, sizeof(u32));
      memcpy( &uTmpPingsReceivedByController, pPacketBuffer + sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE + sizeof(u32), sizeof(u32));

      if ( uTmpPingsReceivedByController > 0 )
         s_bTestLinkCurrentTestSucceeded = true;

      log_line("[TestLink-%d] Process received ping downlink from controller (count received so far: %u). Controller sent %u pings, controller received %u pings replies.",
         s_iTestLinkCurrentRunCount, s_uTestLinkCountPingsReceived, uTmpPingsSentByController, uTmpPingsReceivedByController );

      s_iTestLinkCurrentStepSendCount++;

      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
      PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      PH.vehicle_id_dest = g_uControllerId;
      PH.total_length = sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE + 2*sizeof(u32);

      u8 uBuffer[1024];
      memcpy(uBuffer, (u8*)&PH, sizeof(t_packet_header));
      uBuffer[sizeof(t_packet_header)] = PACKET_TYPE_TEST_RADIO_LINK_PROTOCOL_VERSION;
      uBuffer[sizeof(t_packet_header)+1] = PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE;
      uBuffer[sizeof(t_packet_header)+2] = (u8)s_iTestLinkIndex;
      uBuffer[sizeof(t_packet_header)+3] = (u8)s_iTestLinkCurrentRunCount;
      uBuffer[sizeof(t_packet_header)+4] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_PING_DOWNLINK;
      u32 uTmp = s_iTestLinkCurrentStepSendCount;
      memcpy(uBuffer + sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE, &uTmp, sizeof(u32));
      memcpy(uBuffer + sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE + sizeof(u32), &s_uTestLinkCountPingsReceived, sizeof(u32));

      packets_queue_add_packet(&g_QueueRadioPacketsOut, uBuffer);
      log_line("[TestLink-%d] Sent ping downlink reply message (count %u) to controller", s_iTestLinkCurrentRunCount, s_iTestLinkCurrentStepSendCount);

      return;
   }

   if ( iCmdType == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_END )
   {
      if ( (s_iTestLinkState != TEST_LINK_STATE_PING_DOWNLINK) && (s_iTestLinkState != TEST_LINK_STATE_END) && (s_iTestLinkState != TEST_LINK_STATE_NONE) )
      {
         log_line("[TestLink-%d] Ignored command (%s) from controller on radio interface %d", s_iTestLinkCurrentRunCount, str_get_packet_test_link_command(iCmdType), iInterfaceIndex+1);
         return;
      }

      if ( s_iTestLinkState == TEST_LINK_STATE_PING_DOWNLINK )
      {
         if ( s_uTestLinkCountPingsReceived == 0 )
         {
            log_line("[TestLink-%d] Received end command but ping downlink was not confirmed first. Ending flow.", s_iTestLinkCurrentRunCount);
            s_bTestLinkCurrentTestSucceeded = false;
            _test_link_switch_to_state(TEST_LINK_STATE_ENDED, TIMEOUT_TEST_LINK_STATE_END);
            return;
         }
         s_bTestLinkCurrentTestSucceeded = true;
         _test_link_switch_to_state(TEST_LINK_STATE_END, 500);
      }
      else if ( s_iTestLinkState == TEST_LINK_STATE_NONE )
      {
         log_line("[TestLink-%d] Received end command but we already ended the flow. Ignore it, just send reply back.", s_iTestLinkCurrentRunCount);
      }
      else
      {
         s_bTestLinkCurrentTestSucceeded = true;
         _test_link_end_and_notify();
         return;
      }
   
      s_iTestLinkCurrentStepSendCount++;

      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
      PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      PH.vehicle_id_dest = g_uControllerId;
      PH.total_length = sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE + 1;

      u8 uBuffer[1024];
      memcpy(uBuffer, (u8*)&PH, sizeof(t_packet_header));
      uBuffer[sizeof(t_packet_header)] = PACKET_TYPE_TEST_RADIO_LINK_PROTOCOL_VERSION;
      uBuffer[sizeof(t_packet_header)+1] = PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE;
      uBuffer[sizeof(t_packet_header)+2] = (u8)s_iTestLinkIndex;
      uBuffer[sizeof(t_packet_header)+3] = (u8)s_iTestLinkCurrentRunCount;
      uBuffer[sizeof(t_packet_header)+4] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_END;
      uBuffer[sizeof(t_packet_header)+5] = 1;
      
      packets_queue_add_packet(&g_QueueRadioPacketsOut, uBuffer);
      log_line("[TestLink-%d] Sent end reply message (count %u) to controller", s_iTestLinkCurrentRunCount, s_iTestLinkCurrentStepSendCount);

      return;
   }
}

type_radio_links_parameters* test_link_get_temp_radio_params()
{
   return &s_RadioLinksParamsToTest;
}

void test_link_loop()
{
   if ( s_iTestLinkState == TEST_LINK_STATE_NONE )
      return;

   if ( s_iTestLinkState == TEST_LINK_STATE_START )
   {
      if ( g_TimeNow > s_uTimeEndCurrentStep )
      {
         log_line("[TestLink-%d] No progress to next stage received from controller. Ending flow.", s_iTestLinkCurrentRunCount);
         s_bTestLinkCurrentTestSucceeded = false;
         _test_link_switch_to_state(TEST_LINK_STATE_ENDED, TIMEOUT_TEST_LINK_STATE_END);
         return;
      }
      if ( g_TimeNow > s_uTimeLastUpdateCurrentState + 50 )
      {
         s_iTestLinkCurrentStepSendCount++;
         s_uTimeLastUpdateCurrentState = g_TimeNow;
         _test_link_send_start_confirmation_message();
         _test_link_send_start_confirmation_message();

         if ( g_TimeNow > s_uTimeLastReceivedTestLinkStartMessage + 250 )
            _test_link_switch_to_state(TEST_LINK_STATE_APPLY_RADIO_PARAMS, TIMEOUT_TEST_LINK_STATE_APPLY_RADIO_PARAMS);
      }
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_APPLY_RADIO_PARAMS )
   {
      if ( g_TimeNow > s_uTimeEndCurrentStep )
      {
         log_line("[TestLink-%d] Not able to apply radio params (timed out). Ending flow.", s_iTestLinkCurrentRunCount);
         
         if ( ! s_bTestLinkOnlyFreqChanged )
            _test_link_reopen_interfaces();
         _test_link_resume_interfaces();
         s_bTestLinkCurrentTestSucceeded = false;
         _test_link_switch_to_state(TEST_LINK_STATE_ENDED, TIMEOUT_TEST_LINK_STATE_END);
         return;
      }
      if ( ! s_bApplyRadioParamsInProgress )
      {
         if ( ! s_bTestLinkOnlyFreqChanged )
            _test_link_reopen_interfaces();
         _test_link_resume_interfaces();
         log_line("[TestLink-%d] Applied new radio params. Waiting for ping uplink from controller...", s_iTestLinkCurrentRunCount);
         _test_link_switch_to_state(TEST_LINK_STATE_PING_UPLINK, TIMEOUT_TEST_LINK_STATE_PING);
      }
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_REVERT_RADIO_PARAMS )
   {
      if ( g_TimeNow > s_uTimeEndCurrentStep )
      {
         log_line("[TestLink-%d] Not able to revert radio params (timed out). Ending flow.", s_iTestLinkCurrentRunCount);
         if ( ! s_bTestLinkOnlyFreqChanged )
            _test_link_reopen_interfaces();
         _test_link_resume_interfaces();

         s_bTestLinkCurrentTestSucceeded = false;
         _test_link_switch_to_state(TEST_LINK_STATE_ENDED, TIMEOUT_TEST_LINK_STATE_END);
         return;
      }
      if ( ! s_bApplyRadioParamsInProgress )
      {
         if ( ! s_bTestLinkOnlyFreqChanged )
            _test_link_reopen_interfaces();
         _test_link_resume_interfaces();
          s_bTestLinkCurrentTestSucceeded = false;
         _test_link_switch_to_state(TEST_LINK_STATE_ENDED, TIMEOUT_TEST_LINK_STATE_END);
      }
      return;
   }


   if ( s_iTestLinkState == TEST_LINK_STATE_PING_UPLINK )
   {
      if ( g_TimeNow > s_uTimeEndCurrentStep )
      {
         log_line("[TestLink-%d] Failed to confirm ping uplink (timed out). Ending flow.", s_iTestLinkCurrentRunCount);
         s_bTestLinkCurrentTestSucceeded = false;
         _test_link_switch_to_state(TEST_LINK_STATE_REVERT_RADIO_PARAMS, TIMEOUT_TEST_LINK_STATE_APPLY_RADIO_PARAMS);
         return;
      }
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_PING_DOWNLINK )
   {
      if ( g_TimeNow > s_uTimeEndCurrentStep )
      {
         if ( s_uTestLinkCountPingsReceived > 0 )
         if ( s_bTestLinkCurrentTestSucceeded )
         {
            _test_link_end_and_notify();
            return;
         }

         log_line("[TestLink-%d] Failed to confirm ping downlink (timed out). Ending flow.", s_iTestLinkCurrentRunCount);
         s_bTestLinkCurrentTestSucceeded = false;
         _test_link_switch_to_state(TEST_LINK_STATE_REVERT_RADIO_PARAMS, TIMEOUT_TEST_LINK_STATE_APPLY_RADIO_PARAMS);
         return;
      }
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_END )
   {
      if ( g_TimeNow > s_uTimeEndCurrentStep )
      {
         if ( s_bTestLinkCurrentTestSucceeded )
            _test_link_end_and_notify();
         return;
      }

      if ( g_TimeNow > s_uTimeLastUpdateCurrentState + 50 )
      {
         s_iTestLinkCurrentStepSendCount++;
         s_uTimeLastUpdateCurrentState = g_TimeNow;
         t_packet_header PH;
         radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
         PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
         PH.vehicle_id_dest = g_uControllerId;
         PH.total_length = sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE + 1;

         u8 uBuffer[1024];
         memcpy(uBuffer, (u8*)&PH, sizeof(t_packet_header));
         uBuffer[sizeof(t_packet_header)] = PACKET_TYPE_TEST_RADIO_LINK_PROTOCOL_VERSION;
         uBuffer[sizeof(t_packet_header)+1] = PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE;
         uBuffer[sizeof(t_packet_header)+2] = (u8)s_iTestLinkIndex;
         uBuffer[sizeof(t_packet_header)+3] = (u8)s_iTestLinkCurrentRunCount;
         uBuffer[sizeof(t_packet_header)+4] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_END;
         uBuffer[sizeof(t_packet_header)+5] = 1;
         
         packets_queue_add_packet(&g_QueueRadioPacketsOut, uBuffer);
         log_line("[TestLink-%d] Sent end reply message (count %u) to controller", s_iTestLinkCurrentRunCount, s_iTestLinkCurrentStepSendCount);
      }
      return;
   }
}
