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
#include "../base/ruby_ipc.h"
#include "../base/models_list.h"
#include "../base/hardware_procs.h"
#include "../base/radio_utils.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiopacketsqueue.h"
#include "../radio/radio_rx.h"
#include "../radio/radio_tx.h"
#include "../common/models_connect_frequencies.h"
#include "../common/string_utils.h"
#include "timers.h"
#include "shared_vars.h"
#include "radio_links.h"
#include "packets_utils.h"
#include "ruby_rt_station.h"
#include "adaptive_video.h"

extern t_packet_queue s_QueueRadioPacketsRegPrio;

int s_iTestLinkState = TEST_LINK_STATE_NONE;
u32 s_uTestLinkVehicleId = 0;
int s_iTestLinkIndex = -1;
int s_iTestLinkRunCount = 0;
type_radio_links_parameters s_RadioLinksParamsToTest;
type_radio_links_parameters s_RadioLinksParamsOriginal;
bool s_bTestLinkOnlyFreqChanged = false;
bool s_bTestLinkCurrentTestSucceeded = false;

int s_iTestLinkCurrentStepSendCount = 0;
u32 s_uTimeLastUpdateCurrentState = 0;
u32 s_uTimeEndCurrentStep = 0;
u32 s_uTimeLastTestLinkFinished = 0;

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

u32  test_link_get_last_finish_time()
{
   return s_uTimeLastTestLinkFinished;
}

bool test_link_is_applying_radio_params()
{
   if ( s_iTestLinkState == TEST_LINK_STATE_NONE )
      return false;
   return s_bApplyRadioParamsInProgress;
}

int test_link_get_test_link_index()
{
   return s_iTestLinkIndex;
}

void _test_link_end_and_notify()
{
   test_link_send_status_message_to_central("New parameters are supported and applied.");
   test_link_send_status_message_to_central("Saving state...");

   log_line("[TestLink-%d] Save new tested radio parameters to current model", s_iTestLinkRunCount);
   char szPrefix[32];
   sprintf(szPrefix, "[TestLink-%d]", s_iTestLinkRunCount);
   int iCountDiff = g_pCurrentModel->logVehicleRadioLinkDifferences(szPrefix, &s_RadioLinksParamsOriginal, &s_RadioLinksParamsToTest);

   if ( (iCountDiff > 0) || s_bTestLinkCurrentTestSucceeded )
   {
      memcpy(&(g_pCurrentModel->radioLinksParams), &s_RadioLinksParamsToTest, sizeof(type_radio_links_parameters));
      g_pCurrentModel->validateRadioSettings();

      u32 uMaxVideoBitrate = g_pCurrentModel->getMaxVideoBitrateSupportedForCurrentRadioLinks();
      if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS > uMaxVideoBitrate )
      {
         log_line("[TestLink-%d] Must adjust current video profile bitrate (%.1f Mbps) to max allowed on current links: %.1f Mbps",
            s_iTestLinkRunCount,
            g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS/1000.0/1000.0, uMaxVideoBitrate/1000.0/1000.0);
      
         g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS = uMaxVideoBitrate;
      }


      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == s_iTestLinkIndex )
            g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[i] = s_RadioLinksParamsToTest.link_frequency_khz[s_iTestLinkIndex];
      }
      saveControllerModel(g_pCurrentModel);

      if ( s_RadioLinksParamsOriginal.link_frequency_khz[s_iTestLinkIndex] == get_model_main_connect_frequency(g_pCurrentModel->uVehicleId) )
         set_model_main_connect_frequency(g_pCurrentModel->uVehicleId, s_RadioLinksParamsToTest.link_frequency_khz[s_iTestLinkIndex]);
   }

   test_link_send_end_message_to_central(s_bTestLinkCurrentTestSucceeded);
   s_iTestLinkState = TEST_LINK_STATE_NONE;
   s_uTimeLastTestLinkFinished = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
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
   log_line("[TestLink-%d] Reopening radio interfaces for vehicle radio link %d...", s_iTestLinkRunCount, s_iTestLinkIndex+1);
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
         if ( radio_open_interface_for_read(i, RADIO_PORT_ROUTER_DOWNLINK) > 0 )
            g_SM_RadioStats.radio_interfaces[i].openedForRead = 1;
      }

      if (s_bMustReopenRadioInterfacesForWrite[i] )
      {
         s_bMustReopenRadioInterfacesForWrite[i] = false;
         if ( radio_open_interface_for_write(i) > 0 )
            g_SM_RadioStats.radio_interfaces[i].openedForWrite = 1;
      }
   }
   log_line("[TestLink-%d] Done reopen radio interfaces for vehicle radio link %d.", s_iTestLinkRunCount, s_iTestLinkIndex+1);
}


static void * _thread_test_link_worker_apply(void *argument)
{
   log_line("[TestLink-%d] Started worker thread to update radio interfaces for vehicle radio link %d.", s_iTestLinkRunCount, s_iTestLinkIndex+1);
   hw_log_current_thread_attributes("test link apply");
   char szPrefix[32];
   sprintf(szPrefix, "[TestLink-%d]", s_iTestLinkRunCount);
   g_pCurrentModel->logVehicleRadioLinkDifferences(szPrefix, &s_RadioLinksParamsOriginal, &s_RadioLinksParamsToTest);
   
   radio_links_apply_settings(g_pCurrentModel, s_iTestLinkIndex, &s_RadioLinksParamsOriginal, &s_RadioLinksParamsToTest);
        
   log_line("[TestLink-%d] Finished worker thread to update radio interfaces for vehicle radio link %d.", s_iTestLinkRunCount, s_iTestLinkIndex+1);
   s_bApplyRadioParamsInProgress = false;
   return NULL;
}

static void * _thread_test_link_worker_revert(void *argument)
{
   log_line("[TestLink-%d] Started worker thread to revert radio interfaces for vehicle radio link %d.", s_iTestLinkRunCount, s_iTestLinkIndex+1);
   hw_log_current_thread_attributes("test link revert");
   char szPrefix[32];
   sprintf(szPrefix, "[TestLink-%d]", s_iTestLinkRunCount);
   g_pCurrentModel->logVehicleRadioLinkDifferences(szPrefix, &s_RadioLinksParamsToTest, &s_RadioLinksParamsOriginal);
   
   radio_links_apply_settings(g_pCurrentModel, s_iTestLinkIndex, &s_RadioLinksParamsToTest, &s_RadioLinksParamsOriginal);
        
   log_line("[TestLink-%d] Finished worker thread to revert radio interfaces for vehicle radio link %d.", s_iTestLinkRunCount, s_iTestLinkIndex+1);
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

   adaptive_video_pause(4000);

   s_iTestLinkState = iNewState;
   s_iTestLinkCurrentStepSendCount = 0;
   s_uTimeLastUpdateCurrentState = g_TimeNow;
   s_uTimeEndCurrentStep = g_TimeNow + uTimeout;

   log_line("[TestLink-%d] Switch from state [%s] to state [%s]. Timesout %u ms from now", s_iTestLinkRunCount, szStateOld, szStateNew, uTimeout);

   if ( s_iTestLinkState == TEST_LINK_STATE_START )
   {
      s_bTestLinkCurrentTestSucceeded = false;
      s_uTimeLastUpdateCurrentState = g_TimeNow-100;
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_APPLY_RADIO_PARAMS )
   {
      if ( ! s_bTestLinkOnlyFreqChanged )
         test_link_send_status_message_to_central("Applying new parameters...");
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

         test_link_send_status_message_to_central("Internal error (1).");
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
      log_line("[TestLink-%d] Reset pings received count.", s_iTestLinkRunCount);
      s_uTestLinkCountPingsReceived = 0;
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_PING_DOWNLINK )
   {
      log_line("[TestLink-%d] Reset pings received count.", s_iTestLinkRunCount);
      s_uTestLinkCountPingsReceived = 0;
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_END )
   {
      if ( ! s_bTestLinkCurrentTestSucceeded )
      {
         test_link_send_status_message_to_central("New parameters are not supported.");
         test_link_send_status_message_to_central("Cleaning up...");
         _test_link_switch_to_state(TEST_LINK_STATE_REVERT_RADIO_PARAMS, TIMEOUT_TEST_LINK_STATE_APPLY_RADIO_PARAMS);
         return;
      }
      s_uTimeLastTestLinkFinished = g_TimeNow;
      s_uTimeLastUpdateCurrentState = g_TimeNow-100;
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_REVERT_RADIO_PARAMS )
   {
      if ( ! s_bTestLinkOnlyFreqChanged )
      {
         test_link_send_status_message_to_central("New parameters are not supported.");
         test_link_send_status_message_to_central("Cleaning up...");
      }
      else
         test_link_send_status_message_to_central("Frequency change failed. Cleaning up...");
      
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

         test_link_send_status_message_to_central("Internal error (2).");
         s_bTestLinkCurrentTestSucceeded = false;
         s_bApplyRadioParamsInProgress = false;
         _test_link_switch_to_state(TEST_LINK_STATE_ENDED, TIMEOUT_TEST_LINK_STATE_END);
         return;
      }
      pthread_detach(s_pThreadTestLinkWorker);
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_ENDED )
   {
      test_link_send_end_message_to_central(s_bTestLinkCurrentTestSucceeded);
      s_iTestLinkState = TEST_LINK_STATE_NONE;
      s_uTimeLastTestLinkFinished = g_TimeNow;
      return;
   }
}


void test_link_send_status_message_to_central(const char* szMsg)
{
   if ( (NULL == szMsg) || (0 == szMsg[0]) )
      return;

   log_line("[TestLink-%d] Send status message to central: [%s]", s_iTestLinkRunCount, szMsg);

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = g_uControllerId;
   PH.total_length = sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE + strlen(szMsg)+2;

   u8 buffer[1024];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   buffer[sizeof(t_packet_header)] = PACKET_TYPE_TEST_RADIO_LINK_PROTOCOL_VERSION;
   buffer[sizeof(t_packet_header)+1] = PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE;
   buffer[sizeof(t_packet_header)+2] = (u8) s_iTestLinkIndex;
   buffer[sizeof(t_packet_header)+3] = (u8) s_iTestLinkRunCount;
   buffer[sizeof(t_packet_header)+4] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_STATUS;
   buffer[sizeof(t_packet_header)+5] = (u8) strlen(szMsg)+1;
   memcpy( &(buffer[sizeof(t_packet_header)+6]), szMsg, strlen(szMsg)+1);
   radio_packet_compute_crc(buffer, PH.total_length);
   if ( ! ruby_ipc_channel_send_message(g_fIPCToCentral, buffer, PH.total_length) )
      log_ipc_send_central_error(buffer, PH.total_length);
}

void test_link_send_end_message_to_central(bool bSucceeded)
{
   log_line("[TestLink-%d] Send end message to central. Succeeded? %s", s_iTestLinkRunCount, (bSucceeded?"Yes":"No"));

   adaptive_video_pause(2000);

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = g_uControllerId;
   PH.total_length = sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE + sizeof(u8);

   u8 buffer[1024];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   buffer[sizeof(t_packet_header)] = PACKET_TYPE_TEST_RADIO_LINK_PROTOCOL_VERSION;
   buffer[sizeof(t_packet_header)+1] = PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE;
   buffer[sizeof(t_packet_header)+2] = (u8) s_iTestLinkIndex;
   buffer[sizeof(t_packet_header)+3] = (u8) s_iTestLinkRunCount;
   buffer[sizeof(t_packet_header)+4] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_ENDED;
   buffer[sizeof(t_packet_header)+5] = (u8) (bSucceeded?1:0);
   radio_packet_compute_crc(buffer, PH.total_length);
   if ( ! ruby_ipc_channel_send_message(g_fIPCToCentral, buffer, PH.total_length) )
      log_ipc_send_central_error(buffer, PH.total_length);
}


bool test_link_start(u32 uControllerId, u32 uVehicleId, int iLinkId, type_radio_links_parameters* pRadioParamsToTest)
{
   log_line("[TestLink-%d] Start test flow. CID: %u, VID: %u, vehicle radio link %d", s_iTestLinkRunCount, uControllerId, uVehicleId, iLinkId+1);

   if ( (s_iTestLinkState != TEST_LINK_STATE_NONE) || test_link_is_in_progress() )
   {
      s_bTestLinkCurrentTestSucceeded = false;
      log_line("[TestLink-%d] is already in progress. Ignore request.", s_iTestLinkRunCount);
      test_link_send_status_message_to_central("Another radio link update is in progress.");
      test_link_send_end_message_to_central(false);
      return false;
   }

   adaptive_video_pause(5000);
   adaptive_video_reset_state(uVehicleId);
   s_iTestLinkRunCount++;
   s_uTestLinkVehicleId = uVehicleId;
   s_iTestLinkIndex = iLinkId;
   memcpy(&s_RadioLinksParamsOriginal, &(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));
   memcpy(&s_RadioLinksParamsToTest, pRadioParamsToTest, sizeof(type_radio_links_parameters));

   char szPrefix[32];
   sprintf(szPrefix, "[TestLink-%d]", s_iTestLinkRunCount);
   int iDifferences = g_pCurrentModel->logVehicleRadioLinkDifferences(szPrefix, &s_RadioLinksParamsOriginal, &s_RadioLinksParamsToTest);

   s_bTestLinkOnlyFreqChanged = false;
   if ( (1 == iDifferences) && (s_RadioLinksParamsOriginal.link_frequency_khz[s_iTestLinkIndex] != s_RadioLinksParamsToTest.link_frequency_khz[s_iTestLinkIndex]) )
      s_bTestLinkOnlyFreqChanged = true;

   s_bTestLinkCurrentTestSucceeded = false;

   _test_link_switch_to_state(TEST_LINK_STATE_START, TIMEOUT_TEST_LINK_STATE_START);
   return true;
}

void test_link_process_received_message(int iInterfaceIndex, u8* pPacketBuffer)
{
   if ( NULL == pPacketBuffer )
   {
      log_softerror_and_alarm("[TestLink-%d] Process received message from vehicle on radio interface %d but message is null", s_iTestLinkRunCount, iInterfaceIndex+1);
      return;
   }
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   if ( (pPH->packet_type != PACKET_TYPE_TEST_RADIO_LINK) || (pPH->total_length < sizeof(t_packet_header)+PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE) )
   {
      log_softerror_and_alarm("[TestLink-%d] Process received message from vehicle on radio interface %d but message is invalid.", s_iTestLinkRunCount, iInterfaceIndex+1);
      return;
   }

   int iProtocol = pPacketBuffer[sizeof(t_packet_header)];
   int iHeaderSize = pPacketBuffer[sizeof(t_packet_header)+1];
   //int iRadioLinkId = pPacketBuffer[sizeof(t_packet_header)+2];
   int iTestNb = pPacketBuffer[sizeof(t_packet_header)+3];
   int iCmdType = pPacketBuffer[sizeof(t_packet_header)+4];
   //int iCmdLen = pPH->total_length - sizeof(t_packet_header)-PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE;

   if ( (iProtocol != PACKET_TYPE_TEST_RADIO_LINK_PROTOCOL_VERSION) || (iHeaderSize != PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE) || (iTestNb != s_iTestLinkRunCount) )
   {
      log_softerror_and_alarm("[TestLink-%d] Process received message from vehicle (%s) on radio interface %d but message is invalid (size or index). Protocol: %d, header size: %d, total data length: %d",
         s_iTestLinkRunCount, str_get_packet_test_link_command(iCmdType), iInterfaceIndex+1, iProtocol, iHeaderSize, pPH->total_length - sizeof(t_packet_header));
      return;
   }
   
   char szBuff[128];
   test_link_state_get_state_string(s_iTestLinkState, szBuff);
   log_line("[TestLink-%d] Process received reply command (%s) from vehicle on radio interface %d, current state is: %s", s_iTestLinkRunCount, str_get_packet_test_link_command(iCmdType), iInterfaceIndex+1, szBuff);
   
   if ( iCmdType == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START )
   {
      if ( s_iTestLinkState != TEST_LINK_STATE_START )
      {
         log_line("[TestLink-%d] Ignored duplicate command (%s) from vehicle on radio interface %d", s_iTestLinkRunCount, str_get_packet_test_link_command(iCmdType), iInterfaceIndex+1);
         return;
      }
      _test_link_switch_to_state(TEST_LINK_STATE_APPLY_RADIO_PARAMS, TIMEOUT_TEST_LINK_STATE_APPLY_RADIO_PARAMS);
      return;
   }

   if ( iCmdType == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_PING_UPLINK )
   {
      if ( s_iTestLinkState != TEST_LINK_STATE_PING_UPLINK )
      {
         log_line("[TestLink-%d] Ignored command (%s) from vehicle on radio interface %d", s_iTestLinkRunCount, str_get_packet_test_link_command(iCmdType), iInterfaceIndex+1);
         return;
      }

      s_uTestLinkCountPingsReceived++;
      u32 uTmpPingsSentByVehicle;
      u32 uTmpPingsReceivedByVehicle;
      memcpy( &uTmpPingsSentByVehicle, pPacketBuffer + sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE, sizeof(u32));
      memcpy( &uTmpPingsReceivedByVehicle, pPacketBuffer + sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE + sizeof(u32), sizeof(u32));

      log_line("[TestLink-%d] Process received ping uplink reply from vehicle (count received so far: %u). Vehicle sent %u pings, Vehicle received %u pings replies.",
         s_iTestLinkRunCount, s_uTestLinkCountPingsReceived, uTmpPingsSentByVehicle, uTmpPingsReceivedByVehicle );
      
      if ( uTmpPingsSentByVehicle > 0 )
      if ( uTmpPingsReceivedByVehicle > 0 )
         _test_link_switch_to_state(TEST_LINK_STATE_PING_DOWNLINK, TIMEOUT_TEST_LINK_STATE_PING);
         
      return;
   }

   if ( iCmdType == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_PING_DOWNLINK )
   {
      if ( s_iTestLinkState != TEST_LINK_STATE_PING_DOWNLINK )
      {
         log_line("[TestLink-%d] Ignored command (%s) from vehicle on radio interface %d", s_iTestLinkRunCount, str_get_packet_test_link_command(iCmdType), iInterfaceIndex+1);
         return;
      }

      s_uTestLinkCountPingsReceived++;
      u32 uTmpPingsSentByVehicle;
      u32 uTmpPingsReceivedByVehicle;
      memcpy( &uTmpPingsSentByVehicle, pPacketBuffer + sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE, sizeof(u32));
      memcpy( &uTmpPingsReceivedByVehicle, pPacketBuffer + sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE + sizeof(u32), sizeof(u32));

      log_line("[TestLink-%d] Process received ping downlink reply from vehicle (count received so far: %u). Vehicle sent %u pings, Vehicle received %u pings replies.",
         s_iTestLinkRunCount, s_uTestLinkCountPingsReceived, uTmpPingsSentByVehicle, uTmpPingsReceivedByVehicle );
      
      if ( uTmpPingsSentByVehicle > 1 )
      if ( uTmpPingsReceivedByVehicle > 1 )
      {
         s_bTestLinkCurrentTestSucceeded = true;
         _test_link_switch_to_state(TEST_LINK_STATE_END, 500);
      }
      return;
   }

   if ( iCmdType == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_END )
   {
      if ( s_iTestLinkState != TEST_LINK_STATE_END )
      {
         log_line("[TestLink-%d] Ignored command (%s) from vehicle on radio interface %d", s_iTestLinkRunCount, str_get_packet_test_link_command(iCmdType), iInterfaceIndex+1);
         return;
      }
      _test_link_end_and_notify();
      return;
   }
}

void test_link_loop()
{
   if ( s_iTestLinkState == TEST_LINK_STATE_NONE )
      return;

   if ( s_iTestLinkState == TEST_LINK_STATE_START )
   {
      if ( g_TimeNow > s_uTimeEndCurrentStep )
      {
         log_line("[TestLink-%d] No start confirmation received from vehicle. Ending flow.", s_iTestLinkRunCount);
         s_bTestLinkCurrentTestSucceeded = false;
         _test_link_switch_to_state(TEST_LINK_STATE_ENDED, TIMEOUT_TEST_LINK_STATE_END);
         return;
      }
      if ( g_TimeNow > s_uTimeLastUpdateCurrentState + 90 )
      {
         s_iTestLinkCurrentStepSendCount++;
         s_uTimeLastUpdateCurrentState = g_TimeNow;
         t_packet_header PH;
         radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
         PH.vehicle_id_src = g_uControllerId;
         PH.vehicle_id_dest = s_uTestLinkVehicleId;
         PH.total_length = sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE + sizeof(type_radio_links_parameters);

         u8 uBuffer[1024];
         memcpy(uBuffer, (u8*)&PH, sizeof(t_packet_header));
         uBuffer[sizeof(t_packet_header)] = PACKET_TYPE_TEST_RADIO_LINK_PROTOCOL_VERSION;
         uBuffer[sizeof(t_packet_header)+1] = PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE;
         uBuffer[sizeof(t_packet_header)+2] = (u8)s_iTestLinkIndex;
         uBuffer[sizeof(t_packet_header)+3] = (u8)s_iTestLinkRunCount;
         uBuffer[sizeof(t_packet_header)+4] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START;
         memcpy(uBuffer + sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE, (u8*)&s_RadioLinksParamsToTest, sizeof(type_radio_links_parameters));

         packets_queue_add_packet(&s_QueueRadioPacketsRegPrio, uBuffer);
         log_line("[TestLink-%d] Sent start message (count %u) to vehicle", s_iTestLinkRunCount, s_iTestLinkCurrentStepSendCount);
      }
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_APPLY_RADIO_PARAMS )
   {
      if ( g_TimeNow > s_uTimeEndCurrentStep )
      {
         log_line("[TestLink-%d] Not able to apply radio params (timed out). Ending flow.", s_iTestLinkRunCount);
         
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
          _test_link_switch_to_state(TEST_LINK_STATE_PING_UPLINK, TIMEOUT_TEST_LINK_STATE_PING);
      }
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_PING_UPLINK )
   {
      if ( g_TimeNow > s_uTimeEndCurrentStep )
      {
         log_line("[TestLink-%d] Failed to confirm ping uplink (timed out). Ending flow.", s_iTestLinkRunCount);
         s_bTestLinkCurrentTestSucceeded = false;
         _test_link_switch_to_state(TEST_LINK_STATE_REVERT_RADIO_PARAMS, TIMEOUT_TEST_LINK_STATE_APPLY_RADIO_PARAMS);
         return;
      }

      if ( g_TimeNow > s_uTimeLastUpdateCurrentState + 50 )
      {
         s_iTestLinkCurrentStepSendCount++;
         s_uTimeLastUpdateCurrentState = g_TimeNow;
         t_packet_header PH;
         radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
         PH.vehicle_id_src = g_uControllerId;
         PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
         PH.total_length = sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE + 2*sizeof(u32);

         u8 uBuffer[1024];
         memcpy(uBuffer, (u8*)&PH, sizeof(t_packet_header));
         uBuffer[sizeof(t_packet_header)] = PACKET_TYPE_TEST_RADIO_LINK_PROTOCOL_VERSION;
         uBuffer[sizeof(t_packet_header)+1] = PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE;
         uBuffer[sizeof(t_packet_header)+2] = (u8)s_iTestLinkIndex;
         uBuffer[sizeof(t_packet_header)+3] = (u8)s_iTestLinkRunCount;
         uBuffer[sizeof(t_packet_header)+4] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_PING_UPLINK;
         u32 uTmp = (u32)s_iTestLinkCurrentStepSendCount;
         memcpy(uBuffer + sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE, &uTmp, sizeof(u32));
         memcpy(uBuffer + sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE + sizeof(u32), &s_uTestLinkCountPingsReceived, sizeof(u32));
         packets_queue_add_packet(&s_QueueRadioPacketsRegPrio, uBuffer);
         log_line("[TestLink-%d] Sent ping uplink %d to vehicle", s_iTestLinkRunCount, s_iTestLinkCurrentStepSendCount);
      }
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_PING_DOWNLINK )
   {
      if ( g_TimeNow > s_uTimeEndCurrentStep )
      {
         log_line("[TestLink-%d] Failed to confirm ping downlink (timed out). Ending flow.", s_iTestLinkRunCount);
         s_bTestLinkCurrentTestSucceeded = false;
         _test_link_switch_to_state(TEST_LINK_STATE_REVERT_RADIO_PARAMS, TIMEOUT_TEST_LINK_STATE_APPLY_RADIO_PARAMS);
         return;
      }

      if ( g_TimeNow > s_uTimeLastUpdateCurrentState + 50 )
      {
         s_iTestLinkCurrentStepSendCount++;
         s_uTimeLastUpdateCurrentState = g_TimeNow;
         t_packet_header PH;
         radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
         PH.vehicle_id_src = g_uControllerId;
         PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
         PH.total_length = sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE + 2*sizeof(u32);

         u8 uBuffer[1024];
         memcpy(uBuffer, (u8*)&PH, sizeof(t_packet_header));
         uBuffer[sizeof(t_packet_header)] = PACKET_TYPE_TEST_RADIO_LINK_PROTOCOL_VERSION;
         uBuffer[sizeof(t_packet_header)+1] = PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE;
         uBuffer[sizeof(t_packet_header)+2] = (u8)s_iTestLinkIndex;
         uBuffer[sizeof(t_packet_header)+3] = (u8)s_iTestLinkRunCount;
         uBuffer[sizeof(t_packet_header)+4] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_PING_DOWNLINK;
         u32 uTmp = (u32)s_iTestLinkCurrentStepSendCount;
         memcpy(uBuffer + sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE, &uTmp, sizeof(u32));
         memcpy(uBuffer + sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE + sizeof(u32), &s_uTestLinkCountPingsReceived, sizeof(u32));
         packets_queue_add_packet(&s_QueueRadioPacketsRegPrio, uBuffer);
         log_line("[TestLink-%d] Sent ping downlink %d to vehicle", s_iTestLinkRunCount, s_iTestLinkCurrentStepSendCount);
      }
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_REVERT_RADIO_PARAMS )
   {
      if ( g_TimeNow > s_uTimeEndCurrentStep )
      {
         log_line("[TestLink-%d] Not able to revert radio params (timed out). Ending flow.", s_iTestLinkRunCount);
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

   if ( s_iTestLinkState == TEST_LINK_STATE_END )
   {
      if ( g_TimeNow > s_uTimeEndCurrentStep )
      {
         _test_link_end_and_notify();
         return;
      }

      if ( g_TimeNow > s_uTimeLastUpdateCurrentState + 50 )
      {
         s_iTestLinkCurrentStepSendCount++;
         s_uTimeLastUpdateCurrentState = g_TimeNow;
         t_packet_header PH;
         radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
         PH.vehicle_id_src = g_uControllerId;
         PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
         PH.total_length = sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE + 1;

         u8 uBuffer[1024];
         memcpy(uBuffer, (u8*)&PH, sizeof(t_packet_header));
         uBuffer[sizeof(t_packet_header)] = PACKET_TYPE_TEST_RADIO_LINK_PROTOCOL_VERSION;
         uBuffer[sizeof(t_packet_header)+1] = PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE;
         uBuffer[sizeof(t_packet_header)+2] = (u8)s_iTestLinkIndex;
         uBuffer[sizeof(t_packet_header)+3] = (u8)s_iTestLinkRunCount;
         uBuffer[sizeof(t_packet_header)+4] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_END;
         uBuffer[sizeof(t_packet_header)+5] = 1;
         packets_queue_add_packet(&s_QueueRadioPacketsRegPrio, uBuffer);
         log_line("[TestLink-%d] Sent end message %d to vehicle", s_iTestLinkRunCount, s_iTestLinkCurrentStepSendCount);
      }
      return;
   }
}