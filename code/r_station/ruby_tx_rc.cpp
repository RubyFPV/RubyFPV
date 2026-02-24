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
#include <sys/resource.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pcap.h>
#include <stdint.h>
#include "../radio/radiolink.h"
#include <inttypes.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <termios.h>
#include <unistd.h>
#include "../base/base.h"
#include "../base/config.h"
#include "../base/shared_mem.h"
#include "../base/shared_mem_i2c.h"
#include "../base/models.h"
#include "../base/models_list.h"
#include "../base/hardware_procs.h"
#include "../base/utils.h"
#include "../base/ctrl_interfaces.h"
#include "../base/ctrl_settings.h"
#include "../utils/utils_controller.h"
#include "../base/ruby_ipc.h"
#include "../common/string_utils.h"

#include "timers.h"
#include "shared_vars.h"

#include <sys/resource.h>
#include <semaphore.h>


#define MAX_SERIAL_BUFFER_SIZE 512

int s_fIPCFromRouter = -1;
int s_fIPCToRouter = -1;

u8 s_BufferRCDownlink[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeBufferFromRouter[MAX_PACKET_TOTAL_SIZE];
int s_PipeBufferFromRouterPos = 0;


shared_mem_process_stats* s_pProcessStats = NULL;
t_shared_mem_i2c_controller_rc_in* s_pSM_RCIn = NULL;

t_packet_header gPH;
t_packet_header_rc_full_frame_upstream g_PHRCFUpstream;
t_packet_header_rc_full_frame_upstream* s_pPHRCFUpstream = NULL;
hw_joystick_info_t s_JoystickLocalInfo;
hw_joystick_info_t* s_pJoystickInfo = NULL;
t_ControllerInputInterface* s_pCII = NULL;
u16 s_ComputedRCValues[MAX_RC_CHANNELS];
Model* g_pRCModel = NULL;
bool g_bEnableRC = false;

u32 s_uLastTimeStampRCInFrame = 0;
u8 s_uLastFrameIndexRCIn = 0;
u32 s_uTimeLastRCFrameSent = 0;
u32 s_uTimeBetweenRCFramesOutput = 100000;

void populate_rc_data(Model* pModel, t_packet_header_rc_full_frame_upstream* pPHRCF)
{
   if ( NULL == pModel )
      return;
   pPHRCF->rc_frame_index++;
   for( int i=0; i<(int)(pModel->rc_params.channelsCount); i++ )
   {
      packet_header_rc_full_set_rc_channel_value(pPHRCF, i, s_ComputedRCValues[i]);
   }
}

void _close_joystick()
{
   if ( NULL != s_pCII )
      hardware_close_joystick(s_pCII->currentHardwareIndex);
   s_pCII = NULL;
   s_pJoystickInfo = NULL;
   log_line("Closed joystick.");
}

bool _check_open_joystick(Model* pModel)
{
   if ( NULL == pModel )
      return false;
   if ( (NULL != s_pCII) && (NULL != s_pJoystickInfo) && hardware_is_joystick_opened(s_pCII->currentHardwareIndex) )
      return true;
   if ( g_TimeNow < g_TimeLastJoystickCheck + 1000 )
      return false;

   g_TimeLastJoystickCheck = g_TimeNow;

   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();
   controllerInterfacesEnumJoysticks();

   if ( 0 == pCI->inputInterfacesCount )
      controllerInterfacesEnumJoysticks();
   if ( 0 == pCI->inputInterfacesCount )
      return false;

   s_pCII = NULL;
   for( int i=0; i<pCI->inputInterfacesCount; i++ )
   {
      t_ControllerInputInterface* pCII = controllerInterfacesGetAt(i);
      if ( NULL == pCII )
         continue;
      if ( pCII->uId == pModel->rc_params.hid_id )
      {
         s_pCII = controllerInterfacesGetAt(i);
         break;
      }
   }
   static int iCounterRCError = 0;
   if ( NULL == s_pCII )
   {
      iCounterRCError++;
      if ( (iCounterRCError % 3) == 1 )
         log_softerror_and_alarm("Failed to find RC input device HID id: %u", pModel->rc_params.hid_id);
      return false;
   }

   s_pJoystickInfo = hardware_get_joystick_info(s_pCII->currentHardwareIndex);
   if ( NULL != s_pJoystickInfo )
   {
      if ( 0 == hardware_open_joystick(s_pCII->currentHardwareIndex) )
         s_pJoystickInfo = NULL;
   }

   if ( (NULL != s_pJoystickInfo) && (NULL != s_pCII) )
   {
      log_line("Opened joystick.");
      return true;
   }
   log_softerror_and_alarm("Failed to open joystick.");
   return false;
}

bool handle_joysticks(Model* pModel)
{
   if ( ! _check_open_joystick(pModel) )
      return false;

   int countEvents = hardware_read_joystick(s_pCII->currentHardwareIndex, 5);
   if ( countEvents < 0 )
   {
      log_line("Hardware: failed to read joystick.");
      hardware_close_joystick(s_pCII->currentHardwareIndex);
      return false;
   }

   memcpy(&s_JoystickLocalInfo, s_pJoystickInfo, sizeof(hw_joystick_info_t));
   return true;
}

void _check_update_rc_state()
{
   g_pRCModel = g_pCurrentModel;
   g_bEnableRC = false;
   g_TimeLastJoystickCheck = 0;

   #if defined (FEATURE_ENABLE_RC)
   if ( (NULL != g_pCurrentModel) && (! g_bSearching) && (! g_pCurrentModel->is_spectator) )
   {
      log_line("RC is enabled on current model? %s", (g_pCurrentModel->rc_params.uRCFlags & RC_FLAGS_ENABLED)?"Yes":"No");
      log_line("Relay is enabled on current model? %s", ((g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0) && (g_pCurrentModel->relay_params.uRelayedVehicleId != 0))?"Yes":"No");
      if ( (g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0) && (g_pCurrentModel->relay_params.uRelayedVehicleId != 0) )
      {
         Model* pRelayedModel = findModelWithId(g_pCurrentModel->relay_params.uRelayedVehicleId, 48);
         if ( NULL == pRelayedModel )
            log_softerror_and_alarm("Can't find local model for relayed vehicle id: %u", g_pCurrentModel->relay_params.uRelayedVehicleId);
         else if ( ! (pRelayedModel->rc_params.uRCFlags & RC_FLAGS_ENABLED) )
            log_line("RC is not enabled on relayed VID %u", pRelayedModel->uVehicleId);
         else if ( 0 == pRelayedModel->rc_params.inputType )
            log_line("RC is not configured on relayed VID %u", pRelayedModel->uVehicleId);
         else if ( pRelayedModel->rc_params.rc_frames_per_second > 0 )
         {
            s_uTimeBetweenRCFramesOutput = 1000/pRelayedModel->rc_params.rc_frames_per_second;
            g_bEnableRC = true;
            g_pRCModel = pRelayedModel;
         }
      }
      else
      {
         if ( ! (g_pCurrentModel->rc_params.uRCFlags & RC_FLAGS_ENABLED) )
            log_line("RC is not enabled on current VID %u", g_pCurrentModel->uVehicleId);
         else if ( 0 == g_pCurrentModel->rc_params.inputType )
            log_line("RC is not configured on current VID %u", g_pCurrentModel->uVehicleId);
         else if ( g_pCurrentModel->rc_params.rc_frames_per_second > 0 )
         {
            s_uTimeBetweenRCFramesOutput = 1000/g_pCurrentModel->rc_params.rc_frames_per_second;
            g_bEnableRC = true;
         }
      }
   }
   #endif

   if ( ! g_bEnableRC )
      log_line("RC is not active.");
   else
   {
      log_line("RC is active, using a RC rate of %d packets/sec, %u ms between packets", 1000/s_uTimeBetweenRCFramesOutput, s_uTimeBetweenRCFramesOutput);
      log_line("RC input type: %d", g_pRCModel->rc_params.inputType);
      log_line("RC input HID id: %u", g_pRCModel->rc_params.hid_id);
   }
   if ( g_bEnableRC && (g_pRCModel->rc_params.inputType == RC_INPUT_TYPE_USB) )
      controllerInterfacesEnumJoysticks();
}


void try_read_pipes()
{
   int maxMsgToRead = 10;

   while ( (maxMsgToRead > 0) && (NULL != ruby_ipc_try_read_message(s_fIPCFromRouter, s_PipeBufferFromRouter, &s_PipeBufferFromRouterPos, s_BufferRCDownlink)) )
   {
      maxMsgToRead--;
      t_packet_header* pPH = (t_packet_header*)&s_BufferRCDownlink[0];
      if ( ! radio_packet_check_crc(s_BufferRCDownlink, pPH->total_length) )
      {
         continue;
      }

      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
      {
         if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED )
         if ( pPH->vehicle_id_src != PACKET_COMPONENT_RC )
         {
            log_line("Received message to reload model.");
            reloadCurrentModel();
            u8 uChangeType = (pPH->vehicle_id_src >> 8) & 0xFF;
            if ( uChangeType == MODEL_CHANGED_SYNCHRONISED_SETTINGS_FROM_VEHICLE )
            {
               g_pCurrentModel->b_mustSyncFromVehicle = false;
               log_line("Model settings where synchronized from vehicle. Reset model must sync flag.");
            }
            if ( uChangeType == MODEL_CHANGED_RC_PARAMS )
               log_line("RC parameters have changed. Updating local model.");
            if ( uChangeType == MODEL_CHANGED_RELAY_PARAMS )
               log_line("Relay parameters have changed. Updating local model.");
            _check_update_rc_state();
            _close_joystick();
            load_ControllerInterfacesSettings();
         }
         if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED )
         if ( pPH->vehicle_id_src != PACKET_COMPONENT_RC )
         {
            _close_joystick();
            load_ControllerInterfacesSettings();
            load_ControllerSettings();
         }
         log_line("Finished processing local packet: %s", str_get_packet_type(pPH->packet_type));
      }

      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
      if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_STARTED )
         g_bUpdateInProgress = true;

      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
      if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED ||
           pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_FINISHED )
         g_bUpdateInProgress = false;

   }
}

void _update_loop_info(u32 tTime0)
{
   u32 tNow = get_current_timestamp_ms();
   if ( NULL != s_pProcessStats )
   {
      if ( s_pProcessStats->uMaxLoopTimeMs < tNow - tTime0 )
         s_pProcessStats->uMaxLoopTimeMs = tNow - tTime0;
      s_pProcessStats->uTotalLoopTime += tNow - tTime0;
      if ( 0 != s_pProcessStats->uLoopCounter )
         s_pProcessStats->uAverageLoopTimeMs = s_pProcessStats->uTotalLoopTime / s_pProcessStats->uLoopCounter;
   }
}

void handle_sigint(int sig)
{
   log_line("--------------------------");
   log_line("Caught signal to stop: %d", sig);
   log_line("--------------------------");
   g_bQuit = true;
}

int main(int argc, char *argv[])
{
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   if ( strcmp(argv[argc-1], "-ver") == 0 )
   {
      printf("%d.%d (b-%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR, SYSTEM_SW_BUILD_NUMBER);
      return 0;
   }

   log_init("TXRC");

   hardware_detectBoardAndSystemType();

   if ( strcmp(argv[argc-1], "-search") == 0 )
      g_bSearching = true;

   if ( strcmp(argv[argc-1], "-debug") == 0 )
      log_enable_stdout();

   loadAllModels();
   g_pCurrentModel = getCurrentModel();

   Preferences* p = get_Preferences();
   if ( p->nLogLevel != 0 )
      log_only_errors();

   load_ControllerInterfacesSettings();
   load_ControllerSettings();
   ControllerSettings* pCS = get_ControllerSettings();
   g_uControllerId = controller_utils_getControllerId();


   if ( pCS->iCoresAdjustment )
      hw_set_current_thread_affinity("rc_tx", CORE_AFFINITY_RC_TX, CORE_AFFINITY_RC_TX);

   if ( pCS->iPrioritiesAdjustment )
      hw_set_priority_current_proc(pCS->iThreadPriorityRC);

   s_pPHRCFUpstream = shared_mem_rc_upstream_frame_open_write();

   s_fIPCToRouter = ruby_open_ipc_channel_write_endpoint(IPC_CHANNEL_TYPE_RC_TO_ROUTER);
   if ( s_fIPCToRouter < 0 )
      return -1;

   s_fIPCFromRouter = ruby_open_ipc_channel_read_endpoint(IPC_CHANNEL_TYPE_ROUTER_TO_RC);
   if ( s_fIPCFromRouter < 0 )
      return -1;

   s_pProcessStats = shared_mem_process_stats_open_write(SHARED_MEM_WATCHDOG_RC_TX);
   if ( NULL == s_pProcessStats )
      log_softerror_and_alarm("Failed to open shared mem for RC tx process watchdog stats for writing: %s", SHARED_MEM_WATCHDOG_TELEMETRY_RX);
   else
      log_line("Opened shared mem for RC tx process watchdog stats for writing.");

   s_uTimeBetweenRCFramesOutput = 50;
   gPH.vehicle_id_src = 0;
   gPH.vehicle_id_dest = 0;

   _check_update_rc_state();

   gPH.stream_packet_idx = (STREAM_ID_DATA) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   g_PHRCFUpstream.rc_frame_index = 0;
   g_PHRCFUpstream.flags = 0;
   for( int i=0; i<MAX_RC_CHANNELS; i++ )
   {
      s_ComputedRCValues[i] = 0;
      packet_header_rc_full_set_rc_channel_value(&g_PHRCFUpstream, i, 1000);
   }

   if ( NULL != s_pPHRCFUpstream )
      memcpy(s_pPHRCFUpstream, &g_PHRCFUpstream, sizeof(t_packet_header_rc_full_frame_upstream) );

   u32 uTimeComputeRCFPS = get_current_timestamp_ms();
   u32 uTempRCFrames = 0;

   log_line("Started all ok. Running now.");
   log_line("--------------------------------");

   g_TimeStart = get_current_timestamp_ms();

   while ( !g_bQuit )
   {
      u32 uTimeNow = get_current_timestamp_ms();
      u32 uTimeToSleep = uTimeNow - g_TimeNow;
      if ( uTimeToSleep >= s_uTimeBetweenRCFramesOutput )
         uTimeToSleep = 0;
      else
         uTimeToSleep = s_uTimeBetweenRCFramesOutput - uTimeToSleep;
      if ( uTimeToSleep > 0 )
         hardware_sleep_ms(uTimeToSleep);

      g_uLoopCounter++;
      g_TimeNow = get_current_timestamp_ms();

      if ( NULL != s_pProcessStats )
      {
         s_pProcessStats->uLoopCounter++;
         s_pProcessStats->lastActiveTime = g_TimeNow;
      }

      try_read_pipes();

      if ( (!g_bEnableRC) || g_bUpdateInProgress )
      {
         _update_loop_info(g_TimeNow);
         continue;
      }

      #ifdef FEATURE_ENABLE_RC

      if ( g_TimeNow < s_uTimeLastRCFrameSent + s_uTimeBetweenRCFramesOutput )
      {
         _update_loop_info(g_TimeNow);
         continue;
      }

      u32 miliSec = g_TimeNow - s_uTimeLastRCFrameSent;

      if ( g_pRCModel->rc_params.inputType == RC_INPUT_TYPE_USB )
      {
         if ( handle_joysticks(g_pRCModel) )
            g_PHRCFUpstream.flags |= RC_FULL_FRAME_FLAGS_HAS_INPUT;
         else
            g_PHRCFUpstream.flags &= (~RC_FULL_FRAME_FLAGS_HAS_INPUT);

         for( int i=0; i<(int)(g_pRCModel->rc_params.channelsCount); i++ )
            s_ComputedRCValues[i] = (u16) compute_controller_rc_value(g_pRCModel, i, (int)(s_ComputedRCValues[i]), NULL, &s_JoystickLocalInfo, s_pCII, miliSec);
      }

      if ( g_pRCModel->rc_params.inputType == RC_INPUT_TYPE_RC_IN_SBUS_IBUS )
      {
         g_PHRCFUpstream.flags &= (~RC_FULL_FRAME_FLAGS_HAS_INPUT);

         if ( NULL == s_pSM_RCIn )
            s_pSM_RCIn = shared_mem_i2c_controller_rc_in_open_for_read();
         if ( NULL != s_pSM_RCIn )
         if ( s_pSM_RCIn->uFlags & RC_IN_FLAG_HAS_INPUT )
         {
            g_PHRCFUpstream.flags |= RC_FULL_FRAME_FLAGS_HAS_INPUT;
            if ( (s_uLastTimeStampRCInFrame != s_pSM_RCIn->uTimeStamp) && (s_uLastFrameIndexRCIn != s_pSM_RCIn->uFrameIndex) )
            {
               s_uLastTimeStampRCInFrame = s_pSM_RCIn->uTimeStamp;
               s_uLastFrameIndexRCIn = s_pSM_RCIn->uFrameIndex;
               int nCh = g_pRCModel->rc_params.channelsCount;
               if ( nCh > (int)(s_pSM_RCIn->uChannelsCount) )
                  nCh = (int)(s_pSM_RCIn->uChannelsCount);
               for( int i=0; i<nCh; i++ )
               {
                  //s_ComputedRCValues[i] = s_pSM_RCIn->uChannels[i];
                  s_ComputedRCValues[i] = compute_controller_rc_value(g_pRCModel, i, (int)(s_ComputedRCValues[i]), NULL, NULL, NULL, miliSec);
               }
               //log_line("%d %d %d", s_pSM_RCIn->uChannels[0], s_pSM_RCIn->uChannels[1], s_pSM_RCIn->uChannels[2] );
            }
         }

         if ( s_uLastTimeStampRCInFrame + g_pRCModel->rc_params.rc_failsafe_timeout_ms < g_TimeNow )
            g_PHRCFUpstream.flags &= ~RC_FULL_FRAME_FLAGS_HAS_INPUT;
      }

      s_uTimeLastRCFrameSent = g_TimeNow;

      populate_rc_data(g_pRCModel, &g_PHRCFUpstream);

      if ( NULL != s_pPHRCFUpstream )
         memcpy(s_pPHRCFUpstream, &g_PHRCFUpstream, sizeof(t_packet_header_rc_full_frame_upstream) );

      radio_packet_init(&gPH, PACKET_COMPONENT_RC, PACKET_TYPE_RC_FULL_FRAME, STREAM_ID_DATA);
      gPH.total_length = sizeof(t_packet_header)+sizeof(t_packet_header_rc_full_frame_upstream);
      gPH.vehicle_id_src = g_uControllerId;
      gPH.vehicle_id_dest = g_pRCModel->uVehicleId;

      u8 buffer[MAX_PACKET_TOTAL_SIZE];
      memcpy(buffer, &gPH, sizeof(t_packet_header));
      memcpy(buffer+sizeof(t_packet_header), (u8*)&g_PHRCFUpstream, sizeof(t_packet_header_rc_full_frame_upstream));
      radio_packet_compute_crc(buffer, gPH.total_length);
      ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, gPH.total_length);

      uTempRCFrames++;
      if ( g_TimeNow >= uTimeComputeRCFPS + 2000 )
      {
         log_line("Generated %u RC frames/sec", (uTempRCFrames * 1000) / (g_TimeNow - uTimeComputeRCFPS));
         uTimeComputeRCFPS = g_TimeNow;
         uTempRCFrames = 0;
      }
      #endif

      _update_loop_info(g_TimeNow);
   }

   if ( NULL != s_pCII )
      hardware_close_joystick(s_pCII->currentHardwareIndex);
   hardware_uninit_joysticks();

   ruby_close_ipc_channel(s_fIPCFromRouter);
   ruby_close_ipc_channel(s_fIPCToRouter);
   s_fIPCFromRouter = -1;
   s_fIPCToRouter = -1;

   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_RC_TX, s_pProcessStats);
   shared_mem_i2c_controller_rc_in_close(s_pSM_RCIn);
   shared_mem_rc_upstream_frame_close(s_pPHRCFUpstream);
   return 0;
}
