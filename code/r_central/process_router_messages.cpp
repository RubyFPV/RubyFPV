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

#include "../base/alarms.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../base/ruby_ipc.h"
#include "../base/ctrl_interfaces.h"
#include "../base/ctrl_preferences.h"
#include "../base/ctrl_settings.h"
#include "../base/msp.h"
#include "../common/models_connect_frequencies.h"
#include "../common/string_utils.h"
#include "menu/menu.h"
#include "menu/menu_objects.h"
#include "menu/menu_search.h"
#include "menu/menu_diagnose_radio_link.h"
#include "menu/menu_vehicle_radio_link.h"
#include "menu/menu_negociate_radio.h"
#include "process_router_messages.h"
#include <pthread.h>
#include "shared_vars.h"
#include "timers.h"
#include "pairing.h"
#include "warnings.h"
#include "ui_alarms.h"
#include "popup_log.h"
#include "link_watch.h"
#include "osd.h"
#include "osd_common.h"
#include "notifications.h"
#include "events.h"
#include "colors.h"
#include "ruby_central.h"
#include "ui_alarms.h"
#include "parse_msp.h"

#define MAX_ROUTER_MESSAGES 200

pthread_t s_pThreadIPC;
pthread_mutex_t s_pThreadIPCMutex;
bool s_bThreadInitOk = false;
u8 s_pMessagesFromRouter[MAX_ROUTER_MESSAGES][MAX_PACKET_TOTAL_SIZE];
int s_MessagesFromRouterSize[MAX_ROUTER_MESSAGES];
u8  s_MessagesFromRouterTypes[MAX_ROUTER_MESSAGES];
u8  s_MessagesFromRouterFlags[MAX_ROUTER_MESSAGES];
int s_iCountMessagesFromRouter = 0;
u32 s_uTimeLastRouterIPCFullAlarm = 0;

int s_fIPCToRouter = -1;
int s_fIPCFromRouter = -1;
bool s_bIPCRouterHasReadErrors = false;

u8 s_BufferPipeFromRouter[MAX_PACKET_TOTAL_SIZE];
u8 s_BufferTmpOutputRouterMessage[MAX_PACKET_TOTAL_SIZE];
int s_BufferTmpOutputRouterMessagePos = 0;

char s_BufferVehicleLogFileData[2048];
int s_BufferVehicleLogFilePos = 0;

void * _router_ipc_thread_func(void *ignored_argument);

void start_pipes_to_router()
{
   log_line("[Router COMM] Start pipes to router.");

   s_bThreadInitOk = false;
   s_bIPCRouterHasReadErrors = false;

   if ( -1 == s_fIPCToRouter )
   {
      s_fIPCToRouter = ruby_open_ipc_channel_write_endpoint(IPC_CHANNEL_TYPE_CENTRAL_TO_ROUTER);
      if ( s_fIPCToRouter < 0 )
         return;
   }
   else
      log_line("[Router COMM] Already has IPC write endpoint to router.");

   if ( -1 == s_fIPCFromRouter )
   {
      s_fIPCFromRouter = ruby_open_ipc_channel_read_endpoint(IPC_CHANNEL_TYPE_ROUTER_TO_CENTRAL);
      if ( s_fIPCFromRouter < 0 )
         return;
   }
   else
      log_line("[Router COMM] Already has IPC read endpoint from router.");

   s_BufferTmpOutputRouterMessagePos = 0;
   
   pthread_attr_t attr;
   int iPrio = g_pControllerSettings->iThreadPriorityCentral;
   if ( g_pControllerSettings->iPrioritiesAdjustment && (iPrio > 2) && (iPrio < 99) )
   {
      iPrio++;
      hw_init_worker_thread_attrs(&attr, (g_pControllerSettings->iCoresAdjustment?CORE_AFFINITY_CENTRAL_UI:-1), -1, SCHED_FIFO, iPrio, "central IPC");
   }
   else
      hw_init_worker_thread_attrs(&attr, (g_pControllerSettings->iCoresAdjustment?CORE_AFFINITY_CENTRAL_UI:-1), -1, SCHED_OTHER, 0, "central IPC");
    
   s_iCountMessagesFromRouter = 0;
   if ( 0 != pthread_mutex_init(&s_pThreadIPCMutex, NULL) )
      log_softerror_and_alarm("[Router COMM] Failed to init mutex for router IPC");
   else if ( 0 != pthread_create(&s_pThreadIPC, &attr, &_router_ipc_thread_func, NULL) )
   {
      log_softerror_and_alarm("[Router COMM] Failed to create thread for router IPC");
      pthread_mutex_destroy(&s_pThreadIPCMutex);
   }
   else
   {
      s_bThreadInitOk = true;
      log_line("[Router COMM] Created IPC receiving thread.");
   }
   pthread_attr_destroy(&attr);
}

void stop_pipes_to_router()
{
   log_line("[Router COMM] Stopping IPC to/from router.");
   
   if ( s_bThreadInitOk )
   {
      pthread_cancel(s_pThreadIPC);
      pthread_mutex_destroy(&s_pThreadIPCMutex);
      log_line("[Router COMM] Stopped IPC receiving thread and mutex.");
   }
   s_bThreadInitOk = false;

   ruby_close_ipc_channel(s_fIPCFromRouter);
   ruby_close_ipc_channel(s_fIPCToRouter);
   s_fIPCToRouter = -1;
   s_fIPCFromRouter = -1;
   s_bIPCRouterHasReadErrors = false;
   log_line("[Router COMM] Stopped IPC to/from router.");
}

void send_pause_adaptive_to_router(u32 uTimeToPauseMs)
{
   if ( 0 == uTimeToPauseMs )
      log_line("Sending global adaptive resume to router.");
   else
      log_line("Sending global adaptive pause to router, for %u ms", uTimeToPauseMs);
   send_control_message_to_router(PACEKT_TYPE_LOCAL_CONTROLLER_ADAPTIVE_VIDEO_PAUSE, uTimeToPauseMs);
}

void send_reset_adaptive_state_to_router(u32 uVehicleId)
{
   log_line("Sending adaptive reset state to router, for VID %u", uVehicleId);
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACEKT_TYPE_LOCAL_CONTROLLER_ADAPTIVE_VIDEO_PAUSE, STREAM_ID_DATA);
   PH.vehicle_id_src = uVehicleId;
   PH.vehicle_id_dest = 0xFFFFFFFF;
   PH.total_length = sizeof(t_packet_header);

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   send_packet_to_router(buffer, PH.total_length);
}

int send_model_changed_message_to_router(u32 uChangeType, u8 uExtraParam)
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY | (uChangeType<<8) | (uExtraParam <<16); 
   PH.total_length = sizeof(t_packet_header);

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   return send_packet_to_router(buffer, PH.total_length);
}

int send_control_message_to_router(u8 packet_type, u32 extraParam)
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, packet_type, STREAM_ID_DATA);
   PH.vehicle_id_src = extraParam;
   PH.vehicle_id_dest = extraParam;
   PH.total_length = sizeof(t_packet_header);

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   return send_packet_to_router(buffer, PH.total_length);
}

int send_control_message_to_router_and_data(u8 packet_type, u8* pData, int nDataLength)
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, packet_type, STREAM_ID_DATA);
   PH.vehicle_id_src = 0;
   PH.vehicle_id_dest = 0;
   PH.total_length = sizeof(t_packet_header) + nDataLength;

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   if ( nDataLength > 0 )
      memcpy(&(buffer[sizeof(t_packet_header)]), pData, nDataLength);

   return send_packet_to_router(buffer, PH.total_length);
}

int send_packet_to_router(u8* pPacket, int nLength)
{
   if ( (NULL == pPacket) || (nLength <= 0) )
      return 0;
   if ( ! pairing_isStarted() )
      return 0;
   if ( -1 == s_fIPCToRouter )
   {
      log_softerror_and_alarm("[Router COMM] No IPC to router to send message to.");
      return 0; 
   }
   if ( hardware_is_running_on_runcam_vrx() )
      hardware_led_green_set_blinking_fast(1000);
   int iRes = ruby_ipc_channel_send_message(s_fIPCToRouter, pPacket, nLength);
   if ( iRes != nLength )
      log_softerror_and_alarm("[Router COM] Failed to send message to router (msg size: %d bytes), error: %d", nLength, iRes);
   return iRes;
}

t_structure_vehicle_info* _get_runtime_info_for_packet(u8* pPacketBuffer)
{
   t_packet_header* pPH = (t_packet_header*) pPacketBuffer;
   
   // Searching ?
   if ( g_bSearching )
   {
      if ( g_SearchVehicleRuntimeInfo.uVehicleId != pPH->vehicle_id_src )
         log_line("Process data from router: Set search-vehicle-runtime-info to this new VID: %u (old VID was %u)", pPH->vehicle_id_src, g_SearchVehicleRuntimeInfo.uVehicleId);
      g_SearchVehicleRuntimeInfo.uVehicleId = pPH->vehicle_id_src;
      g_SearchVehicleRuntimeInfo.pModel = NULL;
      if ( controllerHasModelWithId(pPH->vehicle_id_src) )
         g_SearchVehicleRuntimeInfo.pModel = findModelWithId(pPH->vehicle_id_src, 40);
      return &g_SearchVehicleRuntimeInfo;
   }

   // First time pairing?
   if ( (NULL == g_pCurrentModel) || ( ! g_bFirstModelPairingDone ) )
   {
      if ( NULL != g_pCurrentModel )
      {
         log_line("Process data from router: Set vehicle-runtime-info[0] to this new VID: %u (old VID was %u)", pPH->vehicle_id_src, g_VehiclesRuntimeInfo[0].uVehicleId);
         g_VehiclesRuntimeInfo[0].uVehicleId = g_pCurrentModel->uVehicleId;
         g_VehiclesRuntimeInfo[0].pModel = g_pCurrentModel;
      }
      return &(g_VehiclesRuntimeInfo[0]);
   }

   // Find it in the expected vehicles list
   int iIndexFirstEmpty = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == pPH->vehicle_id_src )
         return &(g_VehiclesRuntimeInfo[i]);
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == 0 )
      if ( iIndexFirstEmpty == -1 )
         iIndexFirstEmpty = i;
   }

   // We did not find it already. Add it if it's a vehicle we exepct

   if ( (NULL != g_pCurrentModel ) && (pPH->vehicle_id_src == g_pCurrentModel->uVehicleId) )
   if ( iIndexFirstEmpty != -1 )
   {
      log_line("Process data from router: Add VID %u as a new known vehicle runtime info, to position index %d.", pPH->vehicle_id_src, iIndexFirstEmpty);
      g_VehiclesRuntimeInfo[iIndexFirstEmpty].uVehicleId = g_pCurrentModel->uVehicleId;
      g_VehiclesRuntimeInfo[iIndexFirstEmpty].pModel = g_pCurrentModel;
      log_current_runtime_vehicles_info();
      return &(g_VehiclesRuntimeInfo[iIndexFirstEmpty]);
   }
   if ( (NULL != g_pCurrentModel ) && (g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0) && (g_pCurrentModel->relay_params.uRelayedVehicleId > 0) && (pPH->vehicle_id_src == g_pCurrentModel->relay_params.uRelayedVehicleId) )
   if ( iIndexFirstEmpty != -1 )
   {
      log_line("Process data from router: Add relayed VID %u as a new known vehicle runtime info, to position index %d.", pPH->vehicle_id_src, iIndexFirstEmpty);
      g_VehiclesRuntimeInfo[iIndexFirstEmpty].uVehicleId = g_pCurrentModel->relay_params.uRelayedVehicleId;
      g_VehiclesRuntimeInfo[iIndexFirstEmpty].pModel = findModelWithId(g_pCurrentModel->relay_params.uRelayedVehicleId, 415);
      log_current_runtime_vehicles_info();
      return &(g_VehiclesRuntimeInfo[iIndexFirstEmpty]);
   }

   // Unexpected vehicle

   if ( g_UnexpectedVehicleRuntimeInfo.uVehicleId != pPH->vehicle_id_src )
      log_line("Process data from router: Set unexpected-vehicle-runtime-info to this new VID: %u (old VID was %u)", pPH->vehicle_id_src, g_UnexpectedVehicleRuntimeInfo.uVehicleId);

   g_UnexpectedVehicleRuntimeInfo.uVehicleId = pPH->vehicle_id_src;
   g_UnexpectedVehicleRuntimeInfo.pModel = NULL;
   if ( controllerHasModelWithId(pPH->vehicle_id_src) )
      g_UnexpectedVehicleRuntimeInfo.pModel = findModelWithId(pPH->vehicle_id_src, 41);
   return &(g_UnexpectedVehicleRuntimeInfo);
}

void _process_received_ruby_telemetry_extended(u8* pPacketBuffer)
{
   t_packet_header* pPH = (t_packet_header*) pPacketBuffer;
   t_structure_vehicle_info* pRuntimeInfo = _get_runtime_info_for_packet(pPacketBuffer);
   if ( NULL == pRuntimeInfo )
   {
      log_softerror_and_alarm("Process received Ruby telemetry from router: Failed to find vehicle runtime info for vehicle %u. Ignoring this telemetry packet.", pPH->vehicle_id_src);
      return;
   }

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

   if ( iTelemetryVersion == 0 )
   {
      log_softerror_and_alarm("Received unknown ruby telemetry version from vehicle id %u", pPH->vehicle_id_src);
      return;
   }

   bool bFirstTimeGotTelemetry = false;
   if ( ! pRuntimeInfo->bGotRubyTelemetryInfo )
   {
      bFirstTimeGotTelemetry = true;
      int iIndexRuntime = -1;
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( &(g_VehiclesRuntimeInfo[i]) == pRuntimeInfo )
         {
            iIndexRuntime = i;
            break;
         }
      }
      if ( g_bSearching )
         log_line("Start receiving Ruby telemetry (version %d) from router for vehicle id %u, runtime index: search", iTelemetryVersion, pRuntimeInfo->uVehicleId);
      else
         log_line("Start receiving Ruby telemetry (version %d) from router for vehicle id %u, runtime index: %d.", iTelemetryVersion, pRuntimeInfo->uVehicleId, iIndexRuntime);
      log_current_runtime_vehicles_info();
   }

   pRuntimeInfo->bGotRubyTelemetryInfo = true;
   pRuntimeInfo->uTimeLastRecvRubyTelemetry = g_TimeNow;
   pRuntimeInfo->uTimeLastRecvRubyTelemetryExtended = g_TimeNow;
   pRuntimeInfo->uTimeLastRecvAnyRubyTelemetry = g_TimeNow;
   pRuntimeInfo->tmp_iCountRubyTelemetryPacketsFull++;

   // v3 ruby telemetry
   if ( 3 == iTelemetryVersion )
   {
      t_packet_header_ruby_telemetry_extended_v3* pPHRTE = (t_packet_header_ruby_telemetry_extended_v3*)(pPacketBuffer+sizeof(t_packet_header));
      radio_populate_ruby_telemetry_v6_from_ruby_telemetry_v3(&(pRuntimeInfo->headerRubyTelemetryExtended), pPHRTE);

      int dx = sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v3);
      int totalLength = sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v3);
   
      if ( pPHRTE->uRubyFlags & FLAG_RUBY_TELEMETRY_HAS_EXTENDED_INFO )
      {
         t_packet_header_ruby_telemetry_extended_extra_info* pPHRTExtraInfo = (t_packet_header_ruby_telemetry_extended_extra_info*)(pPacketBuffer + dx);
         dx += sizeof(t_packet_header_ruby_telemetry_extended_extra_info);
         totalLength += sizeof(t_packet_header_ruby_telemetry_extended_extra_info);
         
         if ( ! pRuntimeInfo->bGotRubyTelemetryExtraInfo )
         {
            pRuntimeInfo->bGotRubyTelemetryExtraInfo = true;
            log_line("Start receiving Ruby telemetry extra info from router for vehicle id %u.", pRuntimeInfo->uVehicleId);
            log_current_runtime_vehicles_info();
         }
         memcpy(&(pRuntimeInfo->headerRubyTelemetryExtraInfo), pPHRTExtraInfo, sizeof(t_packet_header_ruby_telemetry_extended_extra_info));
      }

      if ( pPHRTE->extraSize > 0 )
      if ( pPHRTE->extraSize == sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions) )
      if ( pPH->total_length == (totalLength + sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)) )
      {
          if ( ! pRuntimeInfo->bGotRubyTelemetryExtraInfoRetransmissions )
            log_line("Start receiving Ruby telemetry retransmissions extra info from router for vehicle id %u.", pRuntimeInfo->uVehicleId);
         pRuntimeInfo->bGotRubyTelemetryExtraInfoRetransmissions = true;
         memcpy(&(pRuntimeInfo->headerRubyTelemetryExtraInfoRetransmissions), pPacketBuffer + dx, sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions));
      }
   }

   // v4 ruby telemetry
   if ( 4 == iTelemetryVersion )
   {
      t_packet_header_ruby_telemetry_extended_v4* pPHRTE = (t_packet_header_ruby_telemetry_extended_v4*)(pPacketBuffer+sizeof(t_packet_header));
      radio_populate_ruby_telemetry_v6_from_ruby_telemetry_v4(&(pRuntimeInfo->headerRubyTelemetryExtended), pPHRTE);

      int dx = sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v4);
      int totalLength = sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v4);
   
      if ( pPHRTE->uRubyFlags & FLAG_RUBY_TELEMETRY_HAS_EXTENDED_INFO )
      {
         t_packet_header_ruby_telemetry_extended_extra_info* pPHRTExtraInfo = (t_packet_header_ruby_telemetry_extended_extra_info*)(pPacketBuffer + dx);
         dx += sizeof(t_packet_header_ruby_telemetry_extended_extra_info);
         totalLength += sizeof(t_packet_header_ruby_telemetry_extended_extra_info);
         
         if ( ! pRuntimeInfo->bGotRubyTelemetryExtraInfo )
         {
            pRuntimeInfo->bGotRubyTelemetryExtraInfo = true;
            log_line("Start receiving Ruby telemetry extra info from router for vehicle id %u.", pRuntimeInfo->uVehicleId);
            log_current_runtime_vehicles_info();
         }
         memcpy(&(pRuntimeInfo->headerRubyTelemetryExtraInfo), pPHRTExtraInfo, sizeof(t_packet_header_ruby_telemetry_extended_extra_info));
      }

      if ( pPHRTE->extraSize > 0 )
      if ( pPHRTE->extraSize == sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions) )
      if ( pPH->total_length == (totalLength + sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)) )
      {
          if ( ! pRuntimeInfo->bGotRubyTelemetryExtraInfoRetransmissions )
            log_line("Start receiving Ruby telemetry retransmissions extra info from router for vehicle id %u.", pRuntimeInfo->uVehicleId);
         pRuntimeInfo->bGotRubyTelemetryExtraInfoRetransmissions = true;
         memcpy(&(pRuntimeInfo->headerRubyTelemetryExtraInfoRetransmissions), pPacketBuffer + dx, sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions));
      }
   }

   // v5 ruby telemetry
   if ( 5 == iTelemetryVersion )
   {
      t_packet_header_ruby_telemetry_extended_v5* pPHRTE = (t_packet_header_ruby_telemetry_extended_v5*)(pPacketBuffer+sizeof(t_packet_header));
      radio_populate_ruby_telemetry_v6_from_ruby_telemetry_v5(&(pRuntimeInfo->headerRubyTelemetryExtended), pPHRTE);

      int dx = sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v5);
      int totalLength = sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v5);
   
      if ( pPHRTE->uRubyFlags & FLAG_RUBY_TELEMETRY_HAS_EXTENDED_INFO )
      {
         t_packet_header_ruby_telemetry_extended_extra_info* pPHRTExtraInfo = (t_packet_header_ruby_telemetry_extended_extra_info*)(pPacketBuffer + dx);
         dx += sizeof(t_packet_header_ruby_telemetry_extended_extra_info);
         totalLength += sizeof(t_packet_header_ruby_telemetry_extended_extra_info);
         
         if ( ! pRuntimeInfo->bGotRubyTelemetryExtraInfo )
         {
            pRuntimeInfo->bGotRubyTelemetryExtraInfo = true;
            log_line("Start receiving Ruby telemetry extra info from router for vehicle id %u.", pRuntimeInfo->uVehicleId);
            log_current_runtime_vehicles_info();
         }
         memcpy(&(pRuntimeInfo->headerRubyTelemetryExtraInfo), pPHRTExtraInfo, sizeof(t_packet_header_ruby_telemetry_extended_extra_info));
      }

      if ( pPHRTE->extraSize > 0 )
      if ( pPHRTE->extraSize == sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions) )
      if ( pPH->total_length == (totalLength + sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)) )
      {
          if ( ! pRuntimeInfo->bGotRubyTelemetryExtraInfoRetransmissions )
            log_line("Start receiving Ruby telemetry retransmissions extra info from router for vehicle id %u.", pRuntimeInfo->uVehicleId);
         pRuntimeInfo->bGotRubyTelemetryExtraInfoRetransmissions = true;
         memcpy(&(pRuntimeInfo->headerRubyTelemetryExtraInfoRetransmissions), pPacketBuffer + dx, sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions));
      }
   }

   // v6 ruby telemetry
   if ( 6 == iTelemetryVersion )
   {
      t_packet_header_ruby_telemetry_extended_v6* pPHRTE = (t_packet_header_ruby_telemetry_extended_v6*)(pPacketBuffer+sizeof(t_packet_header));

      memcpy(&(pRuntimeInfo->headerRubyTelemetryExtended), pPacketBuffer+sizeof(t_packet_header), sizeof(t_packet_header_ruby_telemetry_extended_v6) );

      int dx = sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v6);
      int totalLength = sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v6);
   
      if ( pPHRTE->uRubyFlags & FLAG_RUBY_TELEMETRY_HAS_EXTENDED_INFO )
      {
         t_packet_header_ruby_telemetry_extended_extra_info* pPHRTExtraInfo = (t_packet_header_ruby_telemetry_extended_extra_info*)(pPacketBuffer + dx);
         dx += sizeof(t_packet_header_ruby_telemetry_extended_extra_info);
         totalLength += sizeof(t_packet_header_ruby_telemetry_extended_extra_info);
         
         if ( ! pRuntimeInfo->bGotRubyTelemetryExtraInfo )
         {
            pRuntimeInfo->bGotRubyTelemetryExtraInfo = true;
            log_line("Start receiving Ruby telemetry (v6) extra info from router for VID %u", pRuntimeInfo->uVehicleId);
            log_current_runtime_vehicles_info();
         }
         memcpy(&(pRuntimeInfo->headerRubyTelemetryExtraInfo), pPHRTExtraInfo, sizeof(t_packet_header_ruby_telemetry_extended_extra_info));
      }

      if ( pPHRTE->extraSize > 0 )
      if ( pPHRTE->extraSize == sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions) )
      if ( pPH->total_length == (totalLength + sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)) )
      {
          if ( ! pRuntimeInfo->bGotRubyTelemetryExtraInfoRetransmissions )
            log_line("Start receiving Ruby telemetry (v6) retransmissions extra info from router for VID %u", pRuntimeInfo->uVehicleId);
         pRuntimeInfo->bGotRubyTelemetryExtraInfoRetransmissions = true;
         memcpy(&(pRuntimeInfo->headerRubyTelemetryExtraInfoRetransmissions), pPacketBuffer + dx, sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions));
      }
   }

   if ( bFirstTimeGotTelemetry )
   {
      log_line("Received telemetry has camera flag set? %s",
          (pRuntimeInfo->headerRubyTelemetryExtended.uRubyFlags & FLAG_RUBY_TELEMETRY_VEHICLE_HAS_CAMERA)?"yes":"no");
      log_line("Received telemetry radio links count: %d", pRuntimeInfo->headerRubyTelemetryExtended.radio_links_count);
      for( int i=0; i<pRuntimeInfo->headerRubyTelemetryExtended.radio_links_count; i++ )
         log_line("Received telemetry radio link %d freq: %u Mhz", i+1, pRuntimeInfo->headerRubyTelemetryExtended.uRadioFrequenciesKhz[i]/1000);
      onEventPairingStartReceivingData(pPH->vehicle_id_src);
   }

   if ( g_bSearching )
   {
      char szFreq1[64];
      char szFreq2[64];
      char szFreq3[64];
      strcpy(szFreq1, str_format_frequency(pRuntimeInfo->headerRubyTelemetryExtended.uRadioFrequenciesKhz[0]) );
      strcpy(szFreq2, str_format_frequency(pRuntimeInfo->headerRubyTelemetryExtended.uRadioFrequenciesKhz[1]) );
      strcpy(szFreq3, str_format_frequency(pRuntimeInfo->headerRubyTelemetryExtended.uRadioFrequenciesKhz[2]) );
      
      static u32 s_uTimeLastLogTelemetrySearch = 0;
      if ( g_TimeNow > s_uTimeLastLogTelemetrySearch + 200 )
      {
         s_uTimeLastLogTelemetrySearch = g_TimeNow;
         log_line("Received a Ruby telemetry packet while searching: telem vehicle ID: %u, radio links (%d): %s, %s, %s",
            pRuntimeInfo->headerRubyTelemetryExtended.uVehicleId, pRuntimeInfo->headerRubyTelemetryExtended.radio_links_count,
            szFreq1, szFreq2, szFreq3 );
      }
   }
}

void _process_received_msp_telemetry(u8* pPacketBuffer)
{
   t_packet_header* pPH = (t_packet_header*) pPacketBuffer;
   t_structure_vehicle_info* pRuntimeInfo = _get_runtime_info_for_packet(pPacketBuffer);
   if ( NULL == pRuntimeInfo )
   {
      log_softerror_and_alarm("Process received MSP telemetry from router: Failed to find vehicle runtime info for vehicle %u. Ignoring this telemetry packet.", pPH->vehicle_id_src);
      return;
   }
   t_packet_header_telemetry_msp* pPHMSP = (t_packet_header_telemetry_msp*)(pPacketBuffer + sizeof(t_packet_header));

   if ( (pPHMSP->uSegmentIdAndExtraInfo & 0xFFFF) <= (pRuntimeInfo->mspState.headerTelemetryMSP.uSegmentIdAndExtraInfo & 0xFFFF) )
   {
      if ( (pRuntimeInfo->mspState.headerTelemetryMSP.uSegmentIdAndExtraInfo & 0xFFFF) - (pPHMSP->uSegmentIdAndExtraInfo & 0xFFFF) < 50 )
         return;
   }
   memcpy(&(pRuntimeInfo->mspState.headerTelemetryMSP), pPHMSP, sizeof(t_packet_header_telemetry_msp));

   parse_msp_incoming_data(&(pRuntimeInfo->mspState), pPacketBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_telemetry_msp), pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_telemetry_msp), !g_bFreezeOSD);
}

void _process_received_model_settings(u8* pPacketBuffer)
{
   if ( NULL == pPacketBuffer )
      return;
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;

   t_structure_vehicle_info* pRuntimeInfo = get_vehicle_runtime_info_for_vehicle_id(pPH->vehicle_id_src);
   if ( (NULL == pRuntimeInfo) || (NULL == pRuntimeInfo->pModel) )
      return;
      
   u8* pData = pPacketBuffer + sizeof(t_packet_header);
   int iDataSize = (int)pPH->total_length - sizeof(t_packet_header);

   u32 uStartFlag = MAX_U32;
   if ( (get_sw_version_build(pRuntimeInfo->pModel) > 79) || (get_sw_version_major(pRuntimeInfo->pModel) >= 10) ) // v 7.7
   {
      memcpy((u8*)&uStartFlag, pData, sizeof(u32));
      pData += 2*sizeof(u32) + sizeof(u8);
      iDataSize -= 2*sizeof(u32) - sizeof(u8);
      log_line("Received model settings packet: vehicle is new mode (7.7)");
   }
   else
      log_line("Received model settings packet: vehicle is in legacy mode (less than 7.7)");

   // Received all settings in a single segment?
   if ( iDataSize > 255 )
   {
      if ( (get_sw_version_build(pRuntimeInfo->pModel) > 79) || (get_sw_version_major(pRuntimeInfo->pModel) >= 10) ) // v 7.7
      {
         u32 uTransferId = 0;
         memcpy((u8*)&uTransferId, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32), sizeof(u32));
         log_line("Received model settings from router in a single packet with full zip model settings from vehicle %u (%d bytes). Transfer id: %u", pPH->vehicle_id_src, iDataSize, uTransferId);
      }
      else
         log_line("Received legacy model settings from router in a single packet with full zip model settings from vehicle %u (%d bytes).", pPH->vehicle_id_src, iDataSize);
      int iResponseParam = 0;
      if ( uStartFlag != MAX_U32 )
         iResponseParam = 1;
      handle_commands_on_full_model_settings_received(pPH->vehicle_id_src, iResponseParam, pData, iDataSize);
      return;
   }

   if ( (get_sw_version_build(pRuntimeInfo->pModel) < 79) && (get_sw_version_major(pRuntimeInfo->pModel) < 10) ) // v 7.7
   {
      u8* pData = pPacketBuffer + sizeof(t_packet_header);
      int iSegmentId = (int)(*pData);
      int iSegment = (int)(*(pData+1));
      int iTotalSegments = *(pData+2);
      int iSegmentSize = *(pData+3);
      log_line("Received message from router (%d bytes, from vehicle id %u) with segment model settings from vehicle (segment id %d, %d of %d, %d bytes).", iDataSize, pPH->vehicle_id_src, iSegmentId, iSegment+1, iTotalSegments, iSegmentSize);
      
      if ( iSegment >= 0 && iSegment < 20 )
      if ( iTotalSegments > 0 && iTotalSegments < 20 )
      if ( iSegmentSize <= iDataSize )
      {
         memcpy( &(pRuntimeInfo->pSegmentsModelSettingsDownload[iSegment][0]), pData+4, iSegmentSize);
         pRuntimeInfo->uSegmentsModelSettingsIds[iSegment] = iSegmentId;
         pRuntimeInfo->uSegmentsModelSettingsSize[iSegment] = iSegmentSize;
         pRuntimeInfo->uSegmentsModelSettingsCount = iTotalSegments;
         log_line("Stored segment %d, %d bytes", iSegment+1, iSegmentSize);
         
         bool bHasAll = true;
         int iTotalSize = 0;
         u8 bufferAll[4096];

         iSegmentId = pRuntimeInfo->uSegmentsModelSettingsIds[0];
            
         for( int i=0; i<iTotalSegments; i++ )
         {
            if ( (int)pRuntimeInfo->uSegmentsModelSettingsIds[i] != iSegmentId )
            {
               bHasAll = false;
               break;
            }
            if ( pRuntimeInfo->uSegmentsModelSettingsSize[i] == 0 )
            {
               bHasAll = false;
               break;
            }
            memcpy(bufferAll + iTotalSize, &(pRuntimeInfo->pSegmentsModelSettingsDownload[i][0]), (int) pRuntimeInfo->uSegmentsModelSettingsSize[i]);
            iTotalSize += (int) pRuntimeInfo->uSegmentsModelSettingsSize[i];
         }
         if ( bHasAll )
         {
            log_line("Got all model settings segments. Total size: %d bytes", iTotalSize);
            handle_commands_on_full_model_settings_received(pPH->vehicle_id_src, 0, bufferAll, iTotalSize);
         }
      }
      return;
   }

   u32 uTransferId = 0;
   u8 uTransferFlags = 0;
   memcpy((u8*)&uTransferId, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32), sizeof(u32));
   memcpy((u8*)&uTransferFlags, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32) + sizeof(u8), sizeof(u8));

   int iSegmentIndex = (int)(*pData);
   int iTotalSegments = (int)(*(pData+1));
   int iSegmentSize = (int)(*(pData+2));
   log_line("Received message from router (%d bytes, from vehicle id %u) with model settings segment (%d of %d, %d bytes) for transfer id %u.",
       iDataSize, pPH->vehicle_id_src, iSegmentIndex+1, iTotalSegments, iSegmentSize, uTransferId);

   if ( iSegmentIndex >= 0 && iSegmentIndex < 20 )
   if ( iTotalSegments > 0 && iTotalSegments < 20 )
   if ( iSegmentSize <= iDataSize )
   {
      memcpy( &(pRuntimeInfo->pSegmentsModelSettingsDownload[iSegmentIndex][0]), pData+3, iSegmentSize);
      pRuntimeInfo->uSegmentsModelSettingsIds[iSegmentIndex] = uTransferId;
      pRuntimeInfo->uSegmentsModelSettingsSize[iSegmentIndex] = iSegmentSize;
      pRuntimeInfo->uSegmentsModelSettingsCount = iTotalSegments;
      log_line("Stored segment %d, %d bytes", iSegmentIndex+1, iSegmentSize);
      
      bool bHasAll = true;
      int iTotalSize = 0;
      
      u8 bufferAll[4096];
      char szBufferReceived[256];
      szBufferReceived[0] = 0;

      for( int i=0; i<iTotalSegments; i++ )
      {
         if ( pRuntimeInfo->uSegmentsModelSettingsIds[i] != uTransferId )
         {
            bHasAll = false;
            continue;
         }
         if ( pRuntimeInfo->uSegmentsModelSettingsSize[i] == 0 )
         {
            bHasAll = false;
            continue;
         }
         char szTmp[32];
         sprintf(szTmp, " %d", i+1);
         strcat(szBufferReceived, szTmp);
         memcpy(bufferAll + iTotalSize, &(pRuntimeInfo->pSegmentsModelSettingsDownload[i][0]), (int) pRuntimeInfo->uSegmentsModelSettingsSize[i]);
         iTotalSize += (int) pRuntimeInfo->uSegmentsModelSettingsSize[i];
      }
      if ( bHasAll )
      {
         int iResponseParam = 0;
         if ( uStartFlag != MAX_U32 )
            iResponseParam = 1;
         log_line("Got all model settings segments. Total size: %d bytes. Process it.", iTotalSize);
         handle_commands_on_full_model_settings_received(pPH->vehicle_id_src, iResponseParam, bufferAll, iTotalSize);
      }
      else
         log_line("Still has not received all small segments. Received segments so far: [%s]", szBufferReceived);
   }
}

int _process_received_message_from_router(u8* pPacketBuffer)
{
   if ( NULL == pPacketBuffer )
      return -1;

   t_packet_header* pPH = (t_packet_header*) pPacketBuffer;

   // Do not process all telemetry packets while searching. Only required ones.

   if ( g_bSearching )
   if ( (pPH->packet_type != PACKET_TYPE_RUBY_TELEMETRY_EXTENDED) &&
        (pPH->packet_type != PACKET_TYPE_RUBY_TELEMETRY_SHORT) &&
        (pPH->packet_type != PACKET_TYPE_LOCAL_CONTROLLER_ROUTER_READY) &&
        (pPH->packet_type != PACKET_TYPE_LOCAL_CONTROLL_VIDEO_DETECTED_ON_SEARCH) &&
        (pPH->packet_type != PACKET_TYPE_LOCAL_CONTROL_UPDATED_RADIO_TX_POWERS) ) 
      return 0;

   if ( pPH->packet_type == PACKET_TYPE_FIRST_PAIRING_DONE )
   {
      log_line("Received notification from router that first pairing was done.");
      log_line("Current local model VID %u, ptr: %X (before updating local copy)", g_pCurrentModel->uVehicleId, g_pCurrentModel);
      loadAllModels();
      g_pCurrentModel = getCurrentModel();
      g_pCurrentModel->b_mustSyncFromVehicle = true;
      ruby_set_active_model_id(g_pCurrentModel->uVehicleId);
      g_bFirstModelPairingDone = true;
      g_bSyncModelSettingsOnLinkRecover = true;
      log_line("Updated current local model VID %u, ptr: %X", g_pCurrentModel->uVehicleId, g_pCurrentModel);
      g_VehiclesRuntimeInfo[0].uVehicleId = g_pCurrentModel->uVehicleId;
      g_VehiclesRuntimeInfo[0].pModel = g_pCurrentModel;
      log_line("Updated runtime info index 0");
      warnings_add(0, "First pairing started...");
      onModelAdded(g_pCurrentModel->uVehicleId);
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROLLER_ROUTER_READY )
   {
      log_line("Received message that router is ready, in working state.");
      
      pairing_on_router_ready();
      notification_remove_start_pairing();
      g_bIsRouterReady = true;
      g_RouterIsReadyTimestamp = g_TimeNow;
      return 0;
   }

   if ( (! g_bSearching) && g_bFirstModelPairingDone )
   if ( (NULL == g_pCurrentModel) || (0 == g_uActiveControllerModelVID) ||
     ((0 == getControllerModelsCount()) && (0 == getControllerModelsSpectatorCount())) )
   {
      log_softerror_and_alarm("Ignore message from station as there is no active model and no searching.");
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_MESSAGE )
   {
      if ( pPH->total_length <= sizeof(t_packet_header) + sizeof(u16) + sizeof(u8) + 1 )
         return 0;
      if ( pPH->total_length > sizeof(t_packet_header) + sizeof(u16) + sizeof(u8) + 250 )
         return 0;

      u16 uMsgId = 0;
      u8 uMsgType = 0;
      memcpy(&uMsgId, pPacketBuffer + sizeof(t_packet_header), sizeof(u16));
      memcpy(&uMsgType, pPacketBuffer + sizeof(t_packet_header)+sizeof(u16), sizeof(u8));
      char* pMsg = (char*)pPacketBuffer + sizeof(t_packet_header)+sizeof(u16)+sizeof(u8);
      pPacketBuffer[pPH->total_length-1] = 0;
      log_line("Received message index %d, type %d, from vehicle VID %u, text: [%s]", uMsgId, uMsgType, pPH->vehicle_id_src, pMsg);
      static u16 s_uLastMsgIndex = 0xFFFF;
      if ( s_uLastMsgIndex == uMsgId )
         return  0;
      s_uLastMsgIndex = uMsgId;
      char szBuff[256];
      szBuff[0] = 0;
      if ( 0 == uMsgType )
        strcpy(szBuff, "Developer Info: ");
      strcat(szBuff, pMsg);
      warnings_add(pPH->vehicle_id_src, szBuff, g_idIconInfo);
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_VIDEO_RECORDING )
   {
      u8 uCmd = pPacketBuffer[sizeof(t_packet_header)];
      if ( 0 == uCmd )
      {
         if ( g_bIsVideoRecording )
         {
             if ( hardware_is_running_on_runcam_vrx() )
                hardware_led_red_set_on();
             else
                hardware_led_green_set_off();

             notification_add_recording_end();
             warnings_add(0, L("Video file processing complete."), g_idIconCamera, get_Color_IconNormal());
             warnings_add(0, L("Video recording stopped"), g_idIconCamera, get_Color_IconNormal());
         }
         g_bIsVideoRecording = false;
         g_bIsVideoProcessing = false;
         g_uVideoRecordingStartTime = 0;
      }
      else if ( 1 == uCmd )
      {
         if ( ! g_bIsVideoRecording )
         {
            g_uVideoRecordingStartTime = g_TimeNow;
            notification_add_recording_start();

            Preferences* p = get_Preferences();
            if ( (p->iRecordingLedAction == 2) || (hardware_is_running_on_runcam_vrx()) )
            {
               if ( hardware_is_running_on_runcam_vrx() )
                  hardware_led_red_set_blinking(MAX_U32);
               else
                  hardware_led_green_set_blinking(MAX_U32);
            }
            else if ( p->iRecordingLedAction == 1 )
               hardware_led_green_set_on();
         }
         g_bIsVideoRecording = true;
         g_bIsVideoProcessing = false;
      }
      else if ( 2 == uCmd )
      {
         g_bIsVideoProcessing = true;
      }
      else if ( (0xFF == uCmd) && (pPH->total_length >= (int)(sizeof(t_packet_header) + 3*sizeof(u8))) )
      {
         u8 uType = pPacketBuffer[sizeof(t_packet_header)+1];
         char* pMsg = (char*)(pPacketBuffer + sizeof(t_packet_header) + 2*sizeof(u8));
         pPacketBuffer[pPH->total_length-1] = 0;
         if ( uType == 0 )
            warnings_add(0, L(pMsg), g_idIconCamera, get_Color_IconNormal());
         else if ( uType == 1 )
            warnings_add(0, L(pMsg), g_idIconCamera, get_Color_IconWarning());
         else
            warnings_add(0, L(pMsg), g_idIconCamera, get_Color_IconError());
      }
      return 0;
   }

   if ( pPH->packet_type == PACEKT_TYPE_LOCAL_CONTROLLER_ADAPTIVE_VIDEO_PAUSE )
   {
      u32 uVehicleId = pPH->vehicle_id_src;
      int iAdaptivePaused = pPH->vehicle_id_dest;

      if ( 0 == uVehicleId )
      {
         log_line("Received message that router has globally %s adaptive video.", iAdaptivePaused?"paused":"resumed");
         g_bAdaptiveVideoIsPaused = iAdaptivePaused?true:false;
      }
      else
      {
         log_line("Received message that router has %s adaptive video for vehicle id %u", iAdaptivePaused?"paused":"resumed", uVehicleId);
         t_structure_vehicle_info* pVehicleRTInfo = get_vehicle_runtime_info_for_vehicle_id(uVehicleId);
         if ( NULL == pVehicleRTInfo )
            log_softerror_and_alarm("Received message from router about unknown VID %u", uVehicleId);
         else
            pVehicleRTInfo->bIsAdaptiveVideoPaused = iAdaptivePaused?true:false;
      }
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_CONFIRMATION )
   {
      u32 uResendCount = 0;
      u16 uVehicleSoftwareVersion = 0;
      if ( pPH->total_length >= sizeof(t_packet_header) + sizeof(u32) )
         memcpy(&uResendCount, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));
      if ( pPH->total_length >= (int)(sizeof(t_packet_header) + sizeof(u32) + sizeof(16)) )
         memcpy(&uVehicleSoftwareVersion, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32), sizeof(u16));

      log_line("Received pairing confirmation from router (received vehicle resend counter: %u). VID: %u, CID: %u, vehicle SW version: %d.%d", uResendCount, pPH->vehicle_id_src, pPH->vehicle_id_dest, uVehicleSoftwareVersion >> 8, uVehicleSoftwareVersion & 0xFF);
      t_structure_vehicle_info* pRuntimeInfo = _get_runtime_info_for_packet(pPacketBuffer);
      if ( NULL == pRuntimeInfo )
         log_softerror_and_alarm("Failed to create a vehicle runtime info for vehicle id %u", pPH->vehicle_id_src);
      else
      {
         pRuntimeInfo->bPairedConfirmed = true;
         log_line("Pairing confirmed for vehicle VID %u", pRuntimeInfo->uVehicleId);
         if ( g_bSyncModelSettingsOnLinkRecover )
         {
            log_line("Must sync model setings on link recover.");
            g_bSyncModelSettingsOnLinkRecover = false;
            if ( NULL != g_pCurrentModel )
               g_pCurrentModel->b_mustSyncFromVehicle = true;
         }
      }
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATED_RADIO_TX_POWERS )
   {
      log_line("Received notification from router that controller tx powers have changed. Update local config.");
      load_ControllerSettings();
      load_ControllerInterfacesSettings();
      menu_refresh_all_menus();
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_DEBUG_INFO )
   {
      t_structure_vehicle_info* pRuntimeInfo = _get_runtime_info_for_packet(pPacketBuffer);
      if ( NULL != pRuntimeInfo )
      {
         if ( pPH->total_length >= sizeof(t_packet_header) + sizeof(type_u32_couters) )
            memcpy(&(pRuntimeInfo->vehicleDebugRouterCounters), pPacketBuffer + sizeof(t_packet_header), sizeof(type_u32_couters));
      }
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_RELAY_RADIO_INFO )
   {
      t_structure_vehicle_info* pRuntimeInfo = _get_runtime_info_for_packet(pPacketBuffer);
      if ( NULL != pRuntimeInfo )
      {
         if ( pPH->total_length >= sizeof(t_packet_header) + sizeof(t_packet_header_relay_radio_info) )
            memcpy(&(pRuntimeInfo->headerRelayRadioLinksInfo), pPacketBuffer + sizeof(t_packet_header), sizeof(t_packet_header_relay_radio_info));
      }
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_NEGOCIATE_RADIO_LINKS )
   {
      if ( menu_has_menu(MENU_ID_NEGOCIATE_RADIO) )
      {
         MenuNegociateRadio* pMenu = (MenuNegociateRadio*) menu_get_menu_by_id(MENU_ID_NEGOCIATE_RADIO);
         if ( NULL != pMenu )
            pMenu->onReceivedVehicleResponse(pPacketBuffer, pPH->total_length);
      }
      return 0;
   }
   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROLL_VIDEO_DETECTED_ON_SEARCH )
   {
      MenuSearch::onVideoReceived(pPH->vehicle_id_src);
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_OTA_UPDATE_STATUS )
   {
      g_TimeNow = get_current_timestamp_ms();
      u8 uStatus = 0;
      u32 uCounter = 0;

      memcpy(&uStatus, pPacketBuffer + sizeof(t_packet_header), sizeof(u8));
      memcpy(&uCounter, pPacketBuffer + sizeof(t_packet_header)+sizeof(u8), sizeof(u32));
      log_line("Received OTA status: %d, counter %u", uStatus, uCounter);
      Menu::updateOTAStatus(uStatus, uCounter);
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_SWITCH_FAVORIVE_VEHICLE )
   {
      log_line("Received message from router that favorite vehicle was switched to VID: %u", pPH->vehicle_id_dest);
      g_bSwitchingFavoriteVehicle = false;

      Model* pTmp = findModelWithId(pPH->vehicle_id_dest, 42);
      if ( NULL == pTmp )
      {
         warnings_add(0, "Failed to switch favorite vehicle");
         return 0;
      }
      char szBuff[256];
      sprintf(szBuff, "Switched to vehicle: %s", pTmp->getLongName());
      warnings_replace("Switching to favorite", szBuff);
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_TEST_RADIO_LINK )
   { 
      g_TimeNow = get_current_timestamp_ms();
      if ( ! is_sw_version_atleast(g_pCurrentModel, 10, 0) )
         return 0;
      if ( pPH->total_length < (int)sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE )
      {
         log_line("Ignore invalid (too small) test link message.");
         return 0;
      }

      int iHeader = (int) sizeof(t_packet_header);
      int iProtocolVersion = pPacketBuffer[iHeader];
      int iHeaderSize = pPacketBuffer[iHeader+1];
      int iRadioLinkId = pPacketBuffer[iHeader+2];
      int iTestNb = pPacketBuffer[iHeader+3]; 
      int iCmdId = pPacketBuffer[iHeader+4];
      int iDataLen = pPH->total_length - (int)sizeof(t_packet_header)-iHeaderSize;
      log_line("Processing received test link (run %d) message type %s from router, %d data bytes for radio link %d",
         iTestNb, str_get_packet_test_link_command(iCmdId), iDataLen, iRadioLinkId+1);

      if ( (iProtocolVersion != PACKET_TYPE_TEST_RADIO_LINK_PROTOCOL_VERSION) || (iHeaderSize != PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE ) )
      {
         log_line("Ignore invalid test link message.");
         return 0;
      }

      if ( iCmdId == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_STATUS )
      {
         char szBuff[256];
         strcpy(szBuff, (char*)(pPacketBuffer + sizeof(t_packet_header) + iHeaderSize+1));
         log_line("Received test link status: %s", szBuff);
         warnings_add_configuring_radio_link_line(szBuff);
      }

      if ( iCmdId == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_ENDED )
      {
         bool bSucceeded = false;
         if ( pPacketBuffer[sizeof(t_packet_header)+iHeaderSize] )
            bSucceeded = true;
         log_line("Radio link params update succeeded? %s", bSucceeded?"Yes":"No");

         warnings_remove_configuring_radio_link(bSucceeded);
         link_reset_reconfiguring_radiolink();

         if ( bSucceeded )
         {
            u32 uCurrentProfileVideoBitrate = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS;
            bool bMustUpdateMainConnectFreq = false;
            u32 uMainConnectFrequency = get_model_main_connect_frequency(g_pCurrentModel->uVehicleId);
            if ( g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLinkId] == uMainConnectFrequency )
               bMustUpdateMainConnectFreq = true;
            reloadCurrentModel();
            g_pCurrentModel = getCurrentModel();

            if ( bMustUpdateMainConnectFreq )
               set_model_main_connect_frequency(g_pCurrentModel->uVehicleId, g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLinkId]);

            if ( uCurrentProfileVideoBitrate != g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS )
            {
               Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE, L("Video bitrate updated"),NULL);
               pm->m_xPos = 0.32; pm->m_yPos = 0.4;
               pm->m_Width = 0.36;
               pm->m_bDisableStacking = true;
               pm->addTopLine(L("Your video bitrate was adjusted to accomodate the new radio links configuration."));
               add_menu_to_stack(pm);
            }
         }
         else
         {
            //Popup* p = new Popup("Failed to set the new parameters.",0.25,0.44, 0.5, 4);
            //p->setIconId(g_idIconError, get_Color_IconError());
            //popups_add_topmost(p);
            Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE, "Change failed",NULL);
            pm->m_xPos = 0.32; pm->m_yPos = 0.4;
            pm->m_Width = 0.36;
            pm->m_bDisableStacking = true;
            pm->addTopLine("The selected parameters are not supported by the radio interfaces.");
            add_menu_to_stack(pm);
         }
         if ( menu_has_menu(MENU_ID_VEHICLE_RADIO_LINK) )
         {
            MenuVehicleRadioLink* pMenuRadioLink = (MenuVehicleRadioLink*) menu_get_menu_by_id(MENU_ID_VEHICLE_RADIO_LINK);
            if ( NULL != pMenuRadioLink )
               pMenuRadioLink->onChangeRadioConfigFinished(bSucceeded);
         }
         menu_update_ui_all_menus();
      }
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_SIK_CONFIG )
   {
       u8 uCommandId = *(pPacketBuffer + sizeof(t_packet_header) + sizeof(u8));
       if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
          log_line("Received from router local response to SiK config command %d", (int)uCommandId);
       else
          log_line("Received from router vehicle response to SiK config command %d", (int)uCommandId);
       MenuDiagnoseRadioLink* pMenu = (MenuDiagnoseRadioLink*) menu_get_menu_by_id(MENU_ID_DIAGNOSE_RADIO_LINK);
       if ( NULL != pMenu )
       {
          u8* pBuffer = pPacketBuffer + sizeof(t_packet_header) + 2*sizeof(u8);
          if ( 0 == uCommandId )
          {
             if ( pPH->vehicle_id_dest == g_uControllerId )
                pMenu->onReceivedControllerData(pBuffer, pPH->total_length - sizeof(t_packet_header) - 2 * sizeof(u8));
             else
                pMenu->onReceivedVehicleData(pBuffer, pPH->total_length - sizeof(t_packet_header) - 2 * sizeof(u8));
          }
       }
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_SWITCH_RADIO_LINK )
   {
      int iLink = (int)pPH->vehicle_id_src;
      int iSucceeded = (int) pPH->vehicle_id_dest;
      u32 uFreqKhz = 0;
      if ( NULL != g_pCurrentModel )
         uFreqKhz = g_pCurrentModel->radioLinksParams.link_frequency_khz[iLink];
      g_bSwitchingRadioLink = false;

      if ( NULL != g_pSM_RadioStats )
         memcpy((u8*)&g_SM_RadioStats, (u8*)g_pSM_RadioStats, sizeof(shared_mem_radio_stats));

      log_line("Received response from router to switch to vehicle radio link %d: succeeded: %d", iLink+1, iSucceeded);
      warnings_remove_switching_radio_link(iLink, uFreqKhz, (bool) iSucceeded);
      menu_refresh_all_menus();
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_SHORT )
   {
      if ( g_bSearching )
         log_line("Received a short Ruby telemetry packet while searching, from vehicle id: %u", pPH->vehicle_id_src);

      t_structure_vehicle_info* pRuntimeInfo = _get_runtime_info_for_packet(pPacketBuffer);
      if ( pPH->total_length != (u16)sizeof(t_packet_header) + (u16)sizeof(t_packet_header_ruby_telemetry_short) )
      {
         log_softerror_and_alarm("Received invalid short telemetry packet from vehicle id %u. Received invalid size: %d bytes, expected %d bytes",
            pPH->vehicle_id_src, pPH->total_length, sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_short) );
         return 0;
      }
      
      t_packet_header_ruby_telemetry_short* pPHRTS = (t_packet_header_ruby_telemetry_short*)(pPacketBuffer+sizeof(t_packet_header));
      memcpy(&(pRuntimeInfo->headerRubyTelemetryShort), pPacketBuffer+sizeof(t_packet_header), sizeof(t_packet_header_ruby_telemetry_short) );
      
      if ( ! pRuntimeInfo->bGotFCTelemetry )
      {
         pRuntimeInfo->bGotFCTelemetry = true;
         log_line("Start receiving short FC telemetry from router for vehicle id %u.", pRuntimeInfo->uVehicleId);
         log_current_runtime_vehicles_info();
      }
      if ( (! pRuntimeInfo->bGotRubyTelemetryInfo) || ( 0 == pRuntimeInfo->uTimeLastRecvRubyTelemetryShort ) )
      {
         pRuntimeInfo->bGotRubyTelemetryInfo = true;
         log_line("Start receiving short Ruby telemetry from router for vehicle id %u.", pRuntimeInfo->uVehicleId);
         log_current_runtime_vehicles_info();
         onEventPairingStartReceivingData(pPH->vehicle_id_src);
      }

      pRuntimeInfo->bGotFCTelemetryShort = true;
      pRuntimeInfo->bGotRubyTelemetryInfo = true;
      pRuntimeInfo->bGotRubyTelemetryInfoShort = true;
      pRuntimeInfo->uTimeLastRecvFCTelemetry = g_TimeNow;
      pRuntimeInfo->uTimeLastRecvFCTelemetryShort = g_TimeNow;
      pRuntimeInfo->uTimeLastRecvRubyTelemetry = g_TimeNow;
      pRuntimeInfo->uTimeLastRecvRubyTelemetryShort = g_TimeNow;
      pRuntimeInfo->uTimeLastRecvAnyRubyTelemetry = g_TimeNow;

      pRuntimeInfo->tmp_iCountRubyTelemetryPacketsShort++;
      pRuntimeInfo->tmp_iCountFCTelemetryPacketsShort++;

      t_packet_header_fc_telemetry PHFCT;
      if ( pRuntimeInfo->bGotFCTelemetryFull )
         memcpy((u8*)&PHFCT, &(pRuntimeInfo->headerFCTelemetry), sizeof(t_packet_header_fc_telemetry));
      else
         memset((u8*)&PHFCT, 0, sizeof(t_packet_header_fc_telemetry));

      PHFCT.uFCFlags = pPHRTS->uFCFlags;
      PHFCT.flight_mode = pPHRTS->flight_mode;
      PHFCT.throttle = pPHRTS->throttle;
      PHFCT.voltage = pPHRTS->voltage; // 1/1000 volts
      PHFCT.current = pPHRTS->current; // 1/1000 amps
      PHFCT.altitude = pPHRTS->altitude; // 1/100 meters  -1000 m
      PHFCT.altitude_abs = pPHRTS->altitude_abs; // 1/100 meters -1000 m
      PHFCT.distance = pPHRTS->distance; // 1/100 meters
      PHFCT.heading = pPHRTS->heading;
      
      PHFCT.vspeed = pPHRTS->vspeed; // 1/100 meters -1000 m
      PHFCT.aspeed = pPHRTS->aspeed; // airspeed (1/100 meters - 1000 m)
      PHFCT.hspeed = pPHRTS->hspeed; // 1/100 meters -1000 m
      
      memcpy(&(pRuntimeInfo->headerFCTelemetry), (u8*)&PHFCT, sizeof(t_packet_header_fc_telemetry) );
      
      t_packet_header_ruby_telemetry_extended_v6 PHRTE;
      if ( pRuntimeInfo->bGotRubyTelemetryInfo )
         memcpy((u8*)&PHRTE, &(pRuntimeInfo->headerRubyTelemetryExtended), sizeof(t_packet_header_ruby_telemetry_extended_v6));
      else
         memset((u8*)&PHRTE, 0, sizeof(t_packet_header_ruby_telemetry_extended_v6));

      PHRTE.uVehicleId = pPH->vehicle_id_src;
      PHRTE.rubyVersion = pPHRTS->rubyVersion;
      PHRTE.radio_links_count = pPHRTS->radio_links_count;
      if ( PHRTE.radio_links_count > 3 )
         PHRTE.radio_links_count = 3;
      for( int i=0; i<PHRTE.radio_links_count; i++ )
         PHRTE.uRadioFrequenciesKhz[i] = pPHRTS->uRadioFrequenciesKhz[i];

      memcpy(&(pRuntimeInfo->headerRubyTelemetryExtended), (u8*)&PHRTE, sizeof(t_packet_header_ruby_telemetry_extended_v6));
      
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED )
   {
      _process_received_ruby_telemetry_extended(pPacketBuffer);
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_TELEMETRY_MSP )
   {
      _process_received_msp_telemetry(pPacketBuffer);
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_FC_TELEMETRY )
   if ( pPH->total_length == (u16)sizeof(t_packet_header) + (u16)sizeof(t_packet_header_fc_telemetry) )
   if ( (! osd_is_debug()) && (!g_bSearching) )
   {
      t_structure_vehicle_info* pRuntimeInfo = _get_runtime_info_for_packet(pPacketBuffer);

      if ( ! pRuntimeInfo->bGotFCTelemetry )
      {
         pRuntimeInfo->bGotFCTelemetry = true;
         log_line("Start receiving FC telemetry from router for vehicle id %u.", pRuntimeInfo->uVehicleId);
         log_current_runtime_vehicles_info();
      }
      pRuntimeInfo->bGotFCTelemetryFull = true;
      pRuntimeInfo->uTimeLastRecvFCTelemetry = g_TimeNow;
      pRuntimeInfo->uTimeLastRecvFCTelemetryFull = g_TimeNow;
      pRuntimeInfo->tmp_iCountFCTelemetryPacketsFull++;
      memcpy(&(pRuntimeInfo->headerFCTelemetry), pPacketBuffer+sizeof(t_packet_header), sizeof(t_packet_header_fc_telemetry) );
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_FC_TELEMETRY_EXTENDED )
   if ( pPH->total_length == (u16)sizeof(t_packet_header) + (u16)sizeof(t_packet_header_fc_telemetry) + (u16)sizeof(t_packet_header_fc_extra) )
   if ( (! osd_is_debug()) && (!g_bSearching) )
   {
      t_structure_vehicle_info* pRuntimeInfo = _get_runtime_info_for_packet(pPacketBuffer);

      if ( ! pRuntimeInfo->bGotFCTelemetry )
      {
         pRuntimeInfo->bGotFCTelemetry = true;
         log_line("Start receiving FC telemetry extended from router for vehicle id %u.", pRuntimeInfo->uVehicleId);
         log_current_runtime_vehicles_info();
      }
      pRuntimeInfo->bGotFCTelemetryFull = true;
      pRuntimeInfo->uTimeLastRecvFCTelemetry = g_TimeNow;
      pRuntimeInfo->uTimeLastRecvFCTelemetryFull = g_TimeNow;
      pRuntimeInfo->tmp_iCountFCTelemetryPacketsFull++;      
      memcpy(&(pRuntimeInfo->headerFCTelemetry), pPacketBuffer+sizeof(t_packet_header), sizeof(t_packet_header_fc_telemetry) );
      
      if ( pRuntimeInfo->headerFCTelemetry.uFCFlags & FC_TELE_FLAGS_HAS_MESSAGE )
      {
         memcpy(&(pRuntimeInfo->headerFCTelemetryExtra), pPacketBuffer+sizeof(t_packet_header) + sizeof(t_packet_header_fc_telemetry), sizeof(t_packet_header_fc_extra) );
         pRuntimeInfo->bGotFCTelemetryExtra = true;

         if ( (0 == strncmp(pRuntimeInfo->szLastMessageFromFC, (const char*)(pRuntimeInfo->headerFCTelemetryExtra.text), FC_MESSAGE_MAX_LENGTH-1)) &&
              (g_TimeNow < pRuntimeInfo->uTimeLastMessageFromFC+5000) )
            return 0;

         strcpy(pRuntimeInfo->szLastMessageFromFC, (const char*)(pRuntimeInfo->headerFCTelemetryExtra.text));
         char szBuff[512];
         if ( (pRuntimeInfo->szLastMessageFromFC[0] == 'R') && (pRuntimeInfo->szLastMessageFromFC[1] == ':') )
         {
            strcpy(szBuff, "Ruby: ");
            strcat(szBuff, &(pRuntimeInfo->szLastMessageFromFC[0]));
         }
         else
         {
            strcpy(szBuff, "FC: ");
            strcat(szBuff, pRuntimeInfo->szLastMessageFromFC);
         }
         if ( pRuntimeInfo->pModel != NULL )
         if ( ! (pRuntimeInfo->pModel->osd_params.osd_preferences[osd_get_current_layout_index()] & OSD_PREFERENCES_BIT_FLAG_DONT_SHOW_FC_MESSAGES) )
         if ( ! (pRuntimeInfo->pModel->telemetry_params.flags & TELEMETRY_FLAGS_DONT_SHOW_FC_MESSAGES) )
            warnings_add(pPH->vehicle_id_src, szBuff, g_idIconInfo, get_Color_IconNormal(), 8);
         pRuntimeInfo->uTimeLastMessageFromFC = g_TimeNow;
      }
      return 0;    
   }

   if ( pPH->packet_type == PACKET_TYPE_FC_RC_CHANNELS )
   if ( pPH->total_length == (u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_fc_rc_channels) )
   if ( (!g_bSearching) && g_bFirstModelPairingDone )
   {
      t_structure_vehicle_info* pRuntimeInfo = _get_runtime_info_for_packet(pPacketBuffer);

      memcpy(&(pRuntimeInfo->headerFCTelemetryRCChannels), pPacketBuffer+sizeof(t_packet_header), sizeof(t_packet_header_fc_rc_channels) );
      return 0;
   }


   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_VIDEO_INFO_STATS )
   {
      //if ( pPH->total_length != sizeof(t_packet_header) + 2*sizeof(shared_mem_video_frames_stats) )
      //   return 0;
      //memcpy((u8*)&g_VideoInfoStatsFromVehicleCameraOut, (u8*)(pPacketBuffer + sizeof(t_packet_header)), sizeof(shared_mem_video_frames_stats));
      //memcpy((u8*)&g_VideoInfoStatsFromVehicleRadioOut, (u8*)(pPacketBuffer + sizeof(t_packet_header) + sizeof(shared_mem_video_frames_stats)), sizeof(shared_mem_video_frames_stats));
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_DEV_VIDEO_BITRATE_HISTORY )
   {
      if ( pPH->total_length != sizeof(t_packet_header) + sizeof(shared_mem_dev_video_bitrate_history) )
         return 0;
      memcpy((u8*)&g_SM_DevVideoBitrateHistory, (u8*)(pPacketBuffer + sizeof(t_packet_header)), sizeof(shared_mem_dev_video_bitrate_history));
      g_bGotStatsVideoBitrate = true;
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY )
   {
      if ( pPH->total_length != sizeof(t_packet_header) + sizeof(t_packet_header_vehicle_tx_history) )
         return 0;
      memcpy((u8*)&g_PHVehicleTxHistory, (u8*)(pPacketBuffer + sizeof(t_packet_header)), sizeof(t_packet_header_vehicle_tx_history));
      g_bGotStatsVehicleTx = true;
      return 0;
   }


   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_RADIO_RX_HISTORY )
   {
      if ( pPH->total_length != sizeof(t_packet_header) + sizeof(u32) + sizeof(shared_mem_radio_stats_interface_rx_hist) )
         return 0;

      u32 uInt = 0;
      memcpy((u8*)&uInt, (u8*)(pPacketBuffer + sizeof(t_packet_header)), sizeof(u32));
      memcpy((u8*)&(g_SM_HistoryRxStatsVehicle.interfaces_history[uInt]), (u8*)(pPacketBuffer + sizeof(t_packet_header) + sizeof(u32)), sizeof(shared_mem_radio_stats_interface_rx_hist));
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROLLER_RADIO_INTERFACE_FAILED_TO_INITIALIZE )
   {
      g_TimeNow = get_current_timestamp_ms();
      int iRadioInterface = pPH->vehicle_id_dest;
      char szBuff[256];
      
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(iRadioInterface);
      if ( NULL == pNICInfo )
         sprintf(szBuff, "Radio Interface %d failed to initialize!", iRadioInterface+1);
      else
      {
         t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pNICInfo->szMAC);
         if ( NULL != pCardInfo )
            sprintf(szBuff, "Radio Interface %d (%s) failed to initialize!", iRadioInterface+1, str_get_radio_card_model_string(pCardInfo->cardModel));
         else
            sprintf(szBuff, "Radio Interface %d (%s) failed to initialize!", iRadioInterface+1, "Unknown Type");
      }
      warnings_add(pPH->vehicle_id_src, szBuff);

      Popup* p = new Popup(szBuff, 0.5, 0.5, 0.6, 10.0);
      p->setCentered();
      p->setFont(g_idFontOSD);
      p->setIconId(g_idIconWarning, get_Color_IconWarning());
      popups_add_topmost(p);

      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS )
   {
      if ( g_bFreezeOSD )
         return 0;

      t_structure_vehicle_info* pRuntimeInfo = _get_runtime_info_for_packet(pPacketBuffer);
      if ( NULL == pRuntimeInfo )
         return 0;
      u8 uType = pPacketBuffer[sizeof(t_packet_header)];
      u8 uCardIndex = pPacketBuffer[sizeof(t_packet_header) + sizeof(u8)];
      if ( (uType != 0xFF) && (uType != 0xF0) && (uType != 0x0F) )
         return 0;
      
      if ( uType == 0xFF )
      if ( (uCardIndex > 0) && (uCardIndex <= MAX_RADIO_INTERFACES) )
      if ( pPH->total_length >= (sizeof(t_packet_header) + 2*sizeof(u8) + sizeof(shared_mem_radio_stats_radio_interface)) )
      {
         memcpy((u8*)&(pRuntimeInfo->SMVehicleRxStats[0]), (u8*)(pPacketBuffer + sizeof(t_packet_header) + 2*sizeof(u8)), uCardIndex*sizeof(shared_mem_radio_stats_radio_interface));
         pRuntimeInfo->uTimeLastRecvVehicleRxStats = g_TimeNow;
         pRuntimeInfo->bGotStatsVehicleRxCards = true;
      }

      if ( uType == 0xF0 )
      if ( (uCardIndex >= 0) && (uCardIndex < MAX_RADIO_INTERFACES) )
      if ( pPH->total_length == (sizeof(t_packet_header) + 2*sizeof(u8) + sizeof(shared_mem_radio_stats_radio_interface)) )
      {
         memcpy((u8*)&(pRuntimeInfo->SMVehicleRxStats[uCardIndex]), (u8*)(pPacketBuffer + sizeof(t_packet_header) + 2*sizeof(u8)), sizeof(shared_mem_radio_stats_radio_interface));
         pRuntimeInfo->uTimeLastRecvVehicleRxStats = g_TimeNow;
         pRuntimeInfo->bGotStatsVehicleRxCards = true;
      }

      if ( uType == 0x0F )
      if ( (uCardIndex >= 0) && (uCardIndex < MAX_RADIO_INTERFACES) )
      if ( pPH->total_length == (sizeof(t_packet_header) + 2*sizeof(u8) + sizeof(shared_mem_radio_stats_radio_interface_compact)) )
      {
         shared_mem_radio_stats_radio_interface_compact statsCompact;
         memcpy((u8*)&statsCompact, (u8*)(pPacketBuffer + sizeof(t_packet_header) + 2*sizeof(u8)), sizeof(shared_mem_radio_stats_radio_interface_compact));
         
         //pRuntimeInfo->SMVehicleRxStats[countCards].lastDbm = statsCompact.lastDbm;
         //pRuntimeInfo->SMVehicleRxStats[countCards].lastDbmVideo = statsCompact.lastDbmVideo;
         //pRuntimeInfo->SMVehicleRxStats[countCards].lastDbmData = statsCompact.lastDbmData;
         memcpy( &(pRuntimeInfo->SMVehicleRxStats[uCardIndex].signalInfo), &statsCompact.signalInfo, sizeof(shared_mem_radio_stats_radio_interface_rx_signal_all));

         pRuntimeInfo->SMVehicleRxStats[uCardIndex].lastRecvDataRate = statsCompact.lastRecvDataRate;
         pRuntimeInfo->SMVehicleRxStats[uCardIndex].lastRecvDataRateVideo = statsCompact.lastRecvDataRateVideo;
         pRuntimeInfo->SMVehicleRxStats[uCardIndex].lastRecvDataRateData = statsCompact.lastRecvDataRateData;

         pRuntimeInfo->SMVehicleRxStats[uCardIndex].totalRxBytes = statsCompact.totalRxBytes;
         pRuntimeInfo->SMVehicleRxStats[uCardIndex].totalTxBytes = statsCompact.totalTxBytes;
         pRuntimeInfo->SMVehicleRxStats[uCardIndex].rxBytesPerSec = statsCompact.rxBytesPerSec;
         pRuntimeInfo->SMVehicleRxStats[uCardIndex].txBytesPerSec = statsCompact.txBytesPerSec;
         pRuntimeInfo->SMVehicleRxStats[uCardIndex].totalRxPackets = statsCompact.totalRxPackets;
         pRuntimeInfo->SMVehicleRxStats[uCardIndex].totalRxPacketsBad = statsCompact.totalRxPacketsBad;
         pRuntimeInfo->SMVehicleRxStats[uCardIndex].totalRxPacketsLost = statsCompact.totalRxPacketsLost;
         pRuntimeInfo->SMVehicleRxStats[uCardIndex].totalTxPackets = statsCompact.totalTxPackets;
         pRuntimeInfo->SMVehicleRxStats[uCardIndex].rxPacketsPerSec = statsCompact.rxPacketsPerSec;
         pRuntimeInfo->SMVehicleRxStats[uCardIndex].txPacketsPerSec = statsCompact.txPacketsPerSec;
         pRuntimeInfo->SMVehicleRxStats[uCardIndex].timeLastRxPacket = statsCompact.timeLastRxPacket;
         pRuntimeInfo->SMVehicleRxStats[uCardIndex].timeLastTxPacket = statsCompact.timeLastTxPacket;

         pRuntimeInfo->SMVehicleRxStats[uCardIndex].rxQuality = statsCompact.rxQuality;
         pRuntimeInfo->SMVehicleRxStats[uCardIndex].rxRelativeQuality = statsCompact.rxRelativeQuality;

         pRuntimeInfo->SMVehicleRxStats[uCardIndex].hist_rxPacketsCurrentIndex = statsCompact.hist_rxPacketsCurrentIndex;
         memcpy(pRuntimeInfo->SMVehicleRxStats[uCardIndex].hist_rxPacketsCount, statsCompact.hist_rxPacketsCount, MAX_HISTORY_RADIO_STATS_RECV_SLICES * sizeof(u8));
         memcpy(pRuntimeInfo->SMVehicleRxStats[uCardIndex].hist_rxPacketsLostCountVideo, statsCompact.hist_rxPacketsLostCountVideo, MAX_HISTORY_RADIO_STATS_RECV_SLICES * sizeof(u8));
         memcpy(pRuntimeInfo->SMVehicleRxStats[uCardIndex].hist_rxPacketsLostCountData, statsCompact.hist_rxPacketsLostCountData, MAX_HISTORY_RADIO_STATS_RECV_SLICES * sizeof(u8));
         memcpy(pRuntimeInfo->SMVehicleRxStats[uCardIndex].hist_rxGapMiliseconds, statsCompact.hist_rxGapMiliseconds, MAX_HISTORY_RADIO_STATS_RECV_SLICES * sizeof(u8));
         memset(pRuntimeInfo->SMVehicleRxStats[uCardIndex].hist_rxPacketsBadCount, 0, MAX_HISTORY_RADIO_STATS_RECV_SLICES*sizeof(u8));
         
         pRuntimeInfo->bGotStatsVehicleRxCards = true;      
      }
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD )
   {
      t_structure_vehicle_info* pRuntimeInfo = _get_runtime_info_for_packet(pPacketBuffer);
      if ( (NULL == pRuntimeInfo) || (NULL == pRuntimeInfo->pModel) )
         return 0;

      int iDataLen = pPH->total_length - sizeof(t_packet_header)-sizeof(t_packet_header_telemetry_raw);
      u8* pTelemetryData = pPacketBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_telemetry_raw);

      if ( pRuntimeInfo->pModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP )
         parse_msp_incoming_data(&(pRuntimeInfo->mspState), pTelemetryData, iDataLen, g_bFreezeOSD);

      if ( g_bOSDPluginsNeedTelemetryStreams )
      {
         for( int i=0; i<g_iPluginsOSDCount; i++ )
         {
            if ( NULL == g_pPluginsOSD[i]->pFunctionRequestTelemetryStreams ||
                 NULL == g_pPluginsOSD[i]->pFunctionOnTelemetryStreamData ||
                 NULL == g_pCurrentModel )
               continue;

            int iRes = (*(g_pPluginsOSD[i]->pFunctionRequestTelemetryStreams))();
            if ( iRes )
               (*(g_pPluginsOSD[i]->pFunctionOnTelemetryStreamData))(pTelemetryData, iDataLen, g_pCurrentModel->telemetry_params.fc_telemetry_type);
         }
      }
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_COMMAND_RESPONSE )
   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
   if ( g_bFirstModelPairingDone )
   {
      handle_commands_on_response_received(pPacketBuffer, pPH->total_length);
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_BROADCAST_RADIO_REINITIALIZED )
   {
      g_TimeNow = get_current_timestamp_ms();
      warnings_add_radio_reinitialized();
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_ALARM )
   {
      g_TimeNow = get_current_timestamp_ms();
      u32 uAlarmIndex = 0;
      u32 uAlarm = 0;
      u32 uFlags1 = 0;
      u32 uFlags2 = 0;

      // Old format ?
      if ( pPH->total_length == (int)(sizeof(t_packet_header) + 3 * sizeof(u32)) )
      {
         memcpy(&uAlarm, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));
         memcpy(&uFlags1, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32), sizeof(u32));
         memcpy(&uFlags2, pPacketBuffer + sizeof(t_packet_header) + 2*sizeof(u32), sizeof(u32));
      }

      // New format, version 7.5
      else if ( pPH->total_length == (int)(sizeof(t_packet_header) + 4 * sizeof(u32)) )
      {
         memcpy(&uAlarmIndex, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));
         memcpy(&uAlarm, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32), sizeof(u32));
         memcpy(&uFlags1, pPacketBuffer + sizeof(t_packet_header) + 2*sizeof(u32), sizeof(u32));
         memcpy(&uFlags2, pPacketBuffer + sizeof(t_packet_header) + 3*sizeof(u32), sizeof(u32));
      }
      else
      {
         log_softerror_and_alarm("Received invalid alarm from router. Received %d bytes, expected %d bytes", pPH->total_length, (int)sizeof(t_packet_header) + 4 * (int)sizeof(u32));
         return 0;
      }

      char szBuff[256];
      alarms_to_string(uAlarm, uFlags1, uFlags2, szBuff);

      // Alarm generated by the controller ?
      bool bLocalAlarm = false;
      if ( (pPH->vehicle_id_src == 0) || (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
         bLocalAlarm = true;
      if ( bLocalAlarm )
      {
         log_line("Received local alarm: %s, alarm index: %u", szBuff, uAlarmIndex);
         alarms_add_from_local(uAlarm, uFlags1, uFlags2);

         if ( uAlarm == ALARM_ID_CONTROLLER_PAIRING_COMPLETED )
         if ( g_bSyncModelSettingsOnLinkRecover )
         {
            log_line("Must sync model setings on link recover.");
            g_bSyncModelSettingsOnLinkRecover = false;
            if ( NULL != g_pCurrentModel )
               g_pCurrentModel->b_mustSyncFromVehicle = true;
         }

      }
      else
      {
         log_line("Received vehicle alarm: %s, alarm index: %u", szBuff, uAlarmIndex);

         if ( (uAlarm & ALARM_ID_LINK_TO_CONTROLLER_LOST) || (uAlarm & ALARM_ID_LINK_TO_CONTROLLER_RECOVERED) )
         {
            Preferences* pP = get_Preferences();
            if ( ! (pP->uEnabledAlarms & ALARM_ID_LINK_TO_CONTROLLER_LOST) )
            {
               log_line("This alarm (controller link lost/recovered) is disabled. Do not show it in UI.");
               return 0;
            }

            bool bShowAlarm = true;
            t_structure_vehicle_info* pRuntimeInfo = _get_runtime_info_for_packet(pPacketBuffer);
            if ( (NULL != pRuntimeInfo) && (NULL != pRuntimeInfo->pModel) )
            if ( ! (pRuntimeInfo->pModel->osd_params.osd_preferences[pRuntimeInfo->pModel->osd_params.iCurrentOSDScreen] & OSD_PREFERENCES_BIT_FLAG_SHOW_CONTROLLER_LINK_LOST_ALARM) )
               bShowAlarm = false;

            if ( bShowAlarm )
            {
               if ( uAlarm & ALARM_ID_LINK_TO_CONTROLLER_LOST )
                  warnings_add_link_to_controller_lost(pPH->vehicle_id_src);
               else if ( uAlarm & ALARM_ID_LINK_TO_CONTROLLER_RECOVERED )
                  warnings_add_link_to_controller_recovered(pPH->vehicle_id_src);
            }
         }
         else
            alarms_add_from_vehicle(pPH->vehicle_id_src, uAlarm, uFlags1, uFlags2);
      }
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_MODEL_SETTINGS )
   if ( g_bFirstModelPairingDone )
   {
      int iDataSize = (int)pPH->total_length - sizeof(t_packet_header);

      t_structure_vehicle_info* pRuntimeInfo = get_vehicle_runtime_info_for_vehicle_id(pPH->vehicle_id_src);
      if ( NULL == pRuntimeInfo )
      {
         log_error_and_alarm("Received model settings packet (%d bytes) but there is no runtime info for VID %u. Ignoring it.",
            iDataSize, pPH->vehicle_id_src);
         return 0;
      }
      if ( NULL == pRuntimeInfo->pModel )
      {
         log_error_and_alarm("Received model settings packet (%d bytes) but there is no model in runtime info for VID %u. Ignoring it.",
            iDataSize, pPH->vehicle_id_src);
         return 0;
      }

      _process_received_model_settings(pPacketBuffer);
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_RECEIVED_VEHICLE_LOG_SEGMENT )
   if ( g_bFirstModelPairingDone )
   {
      log_line("Received a vehicle live log file segment");

      if ( NULL != g_pCurrentModel && (!g_pCurrentModel->is_spectator) )
      if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LIVE_LOG )
      {
         t_packet_header_file_segment* pPHFS = (t_packet_header_file_segment*) (pPacketBuffer + sizeof(t_packet_header));
         int length = pPHFS->segment_size;
         u8* pData = pPacketBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_file_segment);
         memcpy((&(s_BufferVehicleLogFileData[0])) + s_BufferVehicleLogFilePos, pData, length);
         
         FILE* fpLiveLog = NULL;
         char szFileName[MAX_FILE_PATH_SIZE];
         strcpy(szFileName, FOLDER_LOGS);
         strcat(szFileName, LOG_FILE_LIVE_VEHICLE_LOG);
         fpLiveLog = fopen(szFileName, "a");
         if ( NULL != fpLiveLog )
         {
            fwrite(pData, 1, length, fpLiveLog);
            fclose(fpLiveLog);
         } 
         s_BufferVehicleLogFilePos += length;
         int parsePos = 0;
         while ( parsePos < s_BufferVehicleLogFilePos )
         {
            if ( s_BufferVehicleLogFileData[parsePos] == 10 || s_BufferVehicleLogFileData[parsePos] == 13 )
            {
                s_BufferVehicleLogFileData[parsePos] = 0;
                if ( 0 < parsePos )
                   popup_log_add_vehicle_entry(s_BufferVehicleLogFileData);

                for( int k=parsePos+1; k<s_BufferVehicleLogFilePos; k++ )
                    s_BufferVehicleLogFileData[k-parsePos-1] = s_BufferVehicleLogFileData[k];
                s_BufferVehicleLogFilePos -= parsePos+1;
                if ( s_BufferVehicleLogFilePos < 0 )
                   s_BufferVehicleLogFilePos = 0;
                parsePos = 0;
             }
            parsePos++;
         }
      }
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_LINK_FREQUENCY_CHANGED )
   {
      u32* pI = (u32*)(pPacketBuffer+sizeof(t_packet_header));
      u32 uLinkId = *pI;
      pI++;
      u32 uNewFreq = *pI;
      log_line("Received new model link frequency from router (link %u new frequency: %s). Updating local model copy.", uLinkId+1, str_format_frequency(uNewFreq));
      if ( (int)uLinkId < g_pCurrentModel->radioLinksParams.links_count )
      {
         g_pCurrentModel->radioLinksParams.link_frequency_khz[uLinkId] = uNewFreq;
         for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         {
            if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == (int)uLinkId )
               g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[i] = uNewFreq;
         }
         notification_add_frequency_changed((int)uLinkId, uNewFreq);
      }
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_RADIO_CONFIG_UPDATED )
   {
      log_line("Received current radio configuration from vehicle VID %u, packet size: %d bytes.", pPH->vehicle_id_src, pPH->total_length);

      Model* pModel = findModelWithId(pPH->vehicle_id_src, 381);
      if ( NULL == pModel )
      {
         log_softerror_and_alarm("Received vehicle's current radio configuration but can't find the coresponding controller model. Ignore this config update.");
         return 0;
      }

      if ( NULL == g_pCurrentModel )
      {
         log_softerror_and_alarm("There is no current vehicle. Ignore this vehicle radio configuration update.");
         return 0;
      }
      if ( ! is_sw_version_atleast(pModel, 11, 7) )
      {
         log_line("Vehicle SW version is too old (%d.%d). Ignore this vehicle radio config update.", get_sw_version_major(pModel), get_sw_version_minor(pModel));
         return 0;
      }

      if ( pPH->total_length < (sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters) + sizeof(type_radio_links_parameters)) )
      {
         log_softerror_and_alarm("Received current radio configuration: invalid packet size. Ignoring.");
         return 0;
      }
      bool bChanged = false;
      if ( 0 != memcmp(&(pModel->relay_params), pPacketBuffer + sizeof(t_packet_header), sizeof(type_relay_parameters)) )
         bChanged = true;
      if ( 0 != memcmp(&(pModel->radioInterfacesParams), pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters), sizeof(type_radio_interfaces_parameters)) )
         bChanged = true;
      if ( 0 != memcmp(&(pModel->radioLinksParams), pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters), sizeof(type_radio_links_parameters)) )
         bChanged = true;

      if ( pPH->total_length >= (sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters) + sizeof(type_radio_links_parameters) + sizeof(type_radio_runtime_capabilities_parameters)) )
      if ( 0 != memcmp(&(pModel->radioRuntimeCapabilities), pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters) + sizeof(type_radio_links_parameters), sizeof(type_radio_runtime_capabilities_parameters)) )
         bChanged = true;

      if ( g_bDidAnUpdate && (g_nSucceededOTAUpdates > 0) )
         g_bLinkWizardAfterUpdate = true;
      g_bDidAnUpdate = false;
      g_nSucceededOTAUpdates = 0;

      if ( bChanged )
      {
         log_line("Radio configuration has changed on the vehicle.");
         memcpy(&(pModel->relay_params), pPacketBuffer + sizeof(t_packet_header), sizeof(type_relay_parameters));
         memcpy(&(pModel->radioInterfacesParams), pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters), sizeof(type_radio_interfaces_parameters));
         memcpy(&(pModel->radioLinksParams), pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters), sizeof(type_radio_links_parameters));

         if ( pPH->total_length >= (sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters) + sizeof(type_radio_links_parameters) + sizeof(type_radio_runtime_capabilities_parameters)) )
            memcpy(&pModel->radioRuntimeCapabilities, pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters) + sizeof(type_radio_links_parameters), sizeof(type_radio_runtime_capabilities_parameters));

         pModel->validateRadioSettings();
         
         warnings_add(pPH->vehicle_id_src, "Radio configuration has changed on the vehicle. Updating controller radio configuration.", g_idIconRadio);
         hardware_load_radio_info();
      }
      else
         log_line("Received new radio configuration from vehicle is the same as old one, nothing to do, ignoring it.");
   }
   return 0;
}


// Returns number of messages received

int try_read_messages_from_router(u32 uMaxMiliseconds)
{
   g_TimeNow = get_current_timestamp_ms();
   u32 uTimeStart = g_TimeNow;
   if ( -1 == s_fIPCFromRouter )
   {
       hardware_sleep_ms(uMaxMiliseconds/2+1);
       return 0;
   }

   if ( s_bIPCRouterHasReadErrors )
   {
      log_line("Too many read errors on pipe from router. Reinit pipes.");
      stop_pipes_to_router();
      start_pipes_to_router();
   }

   u8 uTmpMsg[MAX_PACKET_TOTAL_SIZE];

   int iCountMessagesProcessed = 0;
   int iCountMessagesProcessedTelemetry = 0;
   int iCountMessagesProcessedRuby = 0;
   int iCountMessagesProcessedLocal = 0;
   while(true)
   {
      u8* pResult = NULL;
      if ( s_bThreadInitOk )
      {
         if ( 0 != s_uTimeLastRouterIPCFullAlarm )
         {
            log_line("[Router COMM] Will start to consume again IPC messages from thread, after alarm. Alarm was %u ms ago", get_current_timestamp_ms() - s_uTimeLastRouterIPCFullAlarm);
            s_uTimeLastRouterIPCFullAlarm = 0;
         }
         pthread_mutex_lock(&s_pThreadIPCMutex);
         g_pProcessStatsCentral->lastIPCIncomingTime = g_TimeNow;
         int iTmpCount = s_iCountMessagesFromRouter;
         if ( 0 == iTmpCount )
         {
            pthread_mutex_unlock(&s_pThreadIPCMutex);
            pResult = NULL;
            hardware_sleep_ms(uMaxMiliseconds/4+1);
         }
         else
         {
            memcpy(&(uTmpMsg[0]), &(s_pMessagesFromRouter[0][0]), s_MessagesFromRouterSize[0]);
            pResult = &uTmpMsg[0];
            s_iCountMessagesFromRouter--;
            for( int i=0; i<s_iCountMessagesFromRouter; i++ )
            {
               s_MessagesFromRouterSize[i] = s_MessagesFromRouterSize[i+1];
               s_MessagesFromRouterTypes[i] = s_MessagesFromRouterTypes[i+1];
               s_MessagesFromRouterFlags[i] = s_MessagesFromRouterFlags[i+1];
               memcpy(&(s_pMessagesFromRouter[i][0]), &(s_pMessagesFromRouter[i+1][0]), s_MessagesFromRouterSize[i+1]);
            }
            pthread_mutex_unlock(&s_pThreadIPCMutex);
         }        
      }
      else
      {
         if ( 0 != s_uTimeLastRouterIPCFullAlarm )
         {
            log_line("[Router COMM] Will start to consume again IPC messages directly, after alarm. Alarm was %u ms ago", get_current_timestamp_ms() - s_uTimeLastRouterIPCFullAlarm);
            s_uTimeLastRouterIPCFullAlarm = 0;
         }
         pResult = ruby_ipc_try_read_message(s_fIPCFromRouter, s_BufferTmpOutputRouterMessage, &s_BufferTmpOutputRouterMessagePos, s_BufferPipeFromRouter);
      }

      if ( NULL != pResult )
      {
         t_packet_header* pPH = (t_packet_header*) pResult;
         iCountMessagesProcessed++;
         if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
            iCountMessagesProcessedTelemetry++;
         if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RUBY )
            iCountMessagesProcessedRuby++;
         if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
            iCountMessagesProcessedLocal++;

         if ( ! radio_packet_check_crc(pResult, pPH->total_length) )
             log_softerror_and_alarm("[Router COMM] Received invalid message (invalid CRC) from router. Ignoring it.");
         else
         {
             _process_received_message_from_router(pResult);
         }
         if ( iCountMessagesProcessed > MAX_ROUTER_MESSAGES/4 )
         {
            log_softerror_and_alarm("[Router COMM] Processing too many messages from router (%d messages: %d telem, %d Ruby, %d local, %d others)",
               iCountMessagesProcessed, iCountMessagesProcessedTelemetry, iCountMessagesProcessedRuby, iCountMessagesProcessedLocal,
               iCountMessagesProcessed - iCountMessagesProcessedTelemetry - iCountMessagesProcessedRuby - iCountMessagesProcessedLocal);
            return iCountMessagesProcessed;
         }
      }

      g_TimeNow = get_current_timestamp_ms();
      if ( g_TimeNow >= uTimeStart + uMaxMiliseconds )
         return iCountMessagesProcessed;
   }
   return iCountMessagesProcessed;
}

void * _router_ipc_thread_func(void *ignored_argument)
{
   ControllerSettings* pCS = get_ControllerSettings();
   if ( pCS->iPrioritiesAdjustment )
      hw_set_current_thread_raw_priority("central IPC", pCS->iThreadPriorityCentral);
   hw_log_current_thread_attributes("central IPC");
   u32 uWaitTimeMs = 5;
   while ( true )
   {
      u8* pResult = NULL;
      if ( -1 != s_fIPCFromRouter )
         pResult = ruby_ipc_try_read_message(s_fIPCFromRouter, s_BufferTmpOutputRouterMessage, &s_BufferTmpOutputRouterMessagePos, s_BufferPipeFromRouter);
      if ( NULL == pResult )
      {
         hardware_sleep_ms(uWaitTimeMs);
         if ( uWaitTimeMs < 50 )
            uWaitTimeMs += 2;
         if ( ruby_ipc_get_read_continous_error_count() > 100 )
         {
            log_line("Too many read errors on pipe from router. Flag read error.");
            s_bIPCRouterHasReadErrors = true;
         }
         continue;
      }
      uWaitTimeMs = 1;
      pthread_mutex_lock(&s_pThreadIPCMutex);
      t_packet_header* pPH = (t_packet_header*)pResult;
      if ( s_iCountMessagesFromRouter >= MAX_ROUTER_MESSAGES )
      {
         log_softerror_and_alarm("[Router COMM] The local router message queue is full (%d messages pending consumption, last consumed %u ms ago). Ignoring message received from router (msg type: %s)",
             s_iCountMessagesFromRouter, get_current_timestamp_ms() - g_pProcessStatsCentral->lastIPCIncomingTime, str_get_packet_type(pPH->packet_type));
         int iCountTelemMsg = 0;
         for( int i=s_iCountMessagesFromRouter-1; i>0; i-- )
         {
            if ( (s_MessagesFromRouterFlags[i] & PACKET_FLAGS_MASK_MODULE) != PACKET_COMPONENT_TELEMETRY )
               continue;
            for( int k=i; k<s_iCountMessagesFromRouter-1; k++ )
            {
               s_MessagesFromRouterSize[k] = s_MessagesFromRouterSize[k+1];
               s_MessagesFromRouterTypes[k] = s_MessagesFromRouterTypes[k+1];
               s_MessagesFromRouterFlags[k] = s_MessagesFromRouterFlags[k+1];
               memcpy(&(s_pMessagesFromRouter[k][0]), &(s_pMessagesFromRouter[k+1][0]), s_MessagesFromRouterSize[k+1]);
            }
            s_iCountMessagesFromRouter--;
            iCountTelemMsg++;
            if ( iCountTelemMsg > 50 )
               break;
         }
         log_softerror_and_alarm("[Router COM] Discarded %d telemetry IPC messages from queue");
      }

      if ( s_iCountMessagesFromRouter < MAX_ROUTER_MESSAGES )
      {
         if ( s_iCountMessagesFromRouter >= MAX_ROUTER_MESSAGES/2 )
         {
            int iCountTelemMsg = 0;
            int iCountRubyMsg = 0;
            int iCountLocalMsg = 0;
            for( int i=0; i<s_iCountMessagesFromRouter; i++ )
            {
               if ( (s_MessagesFromRouterFlags[i] & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
                  iCountTelemMsg++;
               if ( (s_MessagesFromRouterFlags[i] & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RUBY )
                  iCountRubyMsg++;
               if ( (s_MessagesFromRouterFlags[i] & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
                  iCountLocalMsg++;
            }
            log_softerror_and_alarm("[Router COMM] The local router message queue is getting full (%d messages (%d telem, %d Ruby, %d local, %d others) pending consumption of max %d, last consumed %u ms ago). Ignoring message received from router (msg type: %s)",
                s_iCountMessagesFromRouter,
                iCountTelemMsg, iCountRubyMsg, iCountLocalMsg,
                s_iCountMessagesFromRouter - iCountTelemMsg - iCountRubyMsg - iCountLocalMsg,
                MAX_ROUTER_MESSAGES, get_current_timestamp_ms() - g_pProcessStatsCentral->lastIPCIncomingTime, str_get_packet_type(pPH->packet_type));
            s_uTimeLastRouterIPCFullAlarm = get_current_timestamp_ms();
         }

         s_MessagesFromRouterSize[s_iCountMessagesFromRouter] = pPH->total_length;
         s_MessagesFromRouterTypes[s_iCountMessagesFromRouter] = pPH->packet_type;
         s_MessagesFromRouterFlags[s_iCountMessagesFromRouter] = pPH->packet_flags;
         if ( (pPH->total_length >= MAX_PACKET_TOTAL_SIZE) || (pPH->total_length < sizeof(t_packet_header)) )
            log_softerror_and_alarm("[Router COMM] Received message from router too big or small (%d bytes). Ignoring it.", (int)pPH->total_length);
         else
         {
            memcpy(&(s_pMessagesFromRouter[s_iCountMessagesFromRouter][0]), pResult, s_MessagesFromRouterSize[s_iCountMessagesFromRouter]);
            s_iCountMessagesFromRouter++;
         }
      }
      pthread_mutex_unlock(&s_pThreadIPCMutex);
   }
   log_line("[Router COMM] Stopped IPC receiving thread.");
}
