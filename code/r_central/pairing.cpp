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
#include "../base/ctrl_interfaces.h"
#include "../base/controller_rt_info.h"
#include "../base/vehicle_rt_info.h"
//#include "../base/radio_utils.h"
#include "../base/hardware.h"
#include "../base/commands.h"
#include "../base/ruby_ipc.h"
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include <semaphore.h>

#include "pairing.h"
#include "shared_vars.h"
#include "link_watch.h"
#include "warnings.h"
#include "popup.h"
#include "colors.h"
#include "osd_common.h"
#include "ruby_central.h"
#include "local_stats.h"
#include "launchers_controller.h"
#include "handle_commands.h"
#include "forward_watch.h"
#include "timers.h"
#include "events.h"
#include "process_router_messages.h"
#include "notifications.h"
#include "menu_objects.h"
#include "menu.h"

extern bool s_bDebugOSDShowAll;

u32 s_uPairingStartTime = 0;
bool s_isRXStarted = false;
bool s_isVideoReceiving = false;
bool s_bPairingIsRouterReady = false;

void _pairing_open_shared_mem();
void _pairing_close_shared_mem();

bool _pairing_start()
{
   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      log_softerror_and_alarm("No radio interfaces detect on the system. Do nothing (no pairing).");
      log_line("------------------------------------------");
      return false;
   }

   if ( hardware_is_running_on_runcam_vrx() )
      hardware_led_green_set_on();

   controller_launch_router(g_bSearching, g_iSearchFirmwareType);
   controller_launch_rx_telemetry();
   controller_launch_tx_rc();

   handle_commands_start_on_pairing();
   start_pipes_to_router();

   /*
   if ( (! g_bSearching) && (NULL != g_pCurrentModel) )
   if ( g_pCurrentModel->getVehicleFirmwareType() == MODEL_FIRMWARE_TYPE_OPENIPC )
   {
      if ( access("/usr/bin/gst-launch-1.0", R_OK) == -1 )
      {
         //warnings_add("You are missing base components required to pair with OpenIPC cameras. Please do a full install of the Ruby firmware on the controller. Version 8.0 or newer.");
         Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"Missing components",NULL);
         pm->m_xPos = 0.32; pm->m_yPos = 0.4;
         pm->m_Width = 0.36;
         pm->addTopLine("You are missing base components required to pair with OpenIPC cameras.");
         pm->addTopLine("Please do a full install of the Ruby firmware on the controller. Version 8.0 or newer.");
         add_menu_to_stack(pm); 
      }
   }
   */
   
   if ( NULL != g_pCurrentModel )
   {
      char szFile[128];
      snprintf(szFile, sizeof(szFile)/sizeof(szFile[0]), LOG_FILE_VEHICLE, g_pCurrentModel->getShortName());      
      char szPath[256];
      strcpy(szPath, FOLDER_LOGS);
      strcat(szPath, szFile);
      FILE* fd = fopen(szPath, "wb");
      if ( NULL != fd )
         fclose(fd);
   }
 
   s_isRXStarted = true;
   s_isVideoReceiving = false;
   s_uPairingStartTime = g_TimeNow;
   
   link_reset_reconfiguring_radiolink();

   if ( NULL != g_pCurrentModel && (! g_pCurrentModel->is_spectator) )
      g_pCurrentModel->b_mustSyncFromVehicle = true;

   if ( NULL != g_pCurrentModel && g_pCurrentModel->rc_params.rc_enabled )
   {
       Popup* p = new Popup(true, "RC link is enabled", 3 );
       p->setIconId(g_idIconJoystick, get_Color_IconWarning());
       popups_add_topmost(p);
   }

   forward_streams_on_pairing_start();

   g_bMenuPopupUpdateVehicleShown = false;

   log_line("-----------------------------------------");
   log_line("Started pairing processes successfully.");
   log_line("-----------------------------------------");
   return true;
}

bool pairing_start_normal()
{
   log_line("-----------------------------------------------");
   log_line("Starting pairing processes for current model...");

   g_bSearching = false;
   g_iSearchFrequency = 0;
   g_iSearchSiKAirDataRate = -1;
   g_iSearchSiKECC = -1;
   g_iSearchSiKLBT = -1;
   g_iSearchSiKMCSTR = -1;
   g_bMustNegociateRadioLinksFlag = false;
   g_bAskedForNegociateRadioLink = false;
   
   onEventBeforePairing();

   if ( NULL == g_pCurrentModel )
   {
      log_line("No current model. Do nothing (no pairing).");
      log_line("----------------------------------------------");
      return true;
   }

   bool bRes = _pairing_start();

   if ( bRes )
      onEventPaired();

   return bRes;
}

bool pairing_start_search_mode(int iFreqKhz, int iModelsFirmwareType)
{
   log_line("-----------------------------------------------");
   log_line("Starting pairing processes in search mode (%s)...", str_format_frequency(iFreqKhz));

   g_bSearching = true;
   g_iSearchFrequency = iFreqKhz;
   g_iSearchFirmwareType = iModelsFirmwareType;
   g_iSearchSiKAirDataRate = -1;
   g_iSearchSiKECC = -1;
   g_iSearchSiKLBT = -1;
   g_iSearchSiKMCSTR = -1;

   return _pairing_start();
}

bool pairing_start_search_sik_mode(int iFreqKhz, int iAirDataRate, int iECC, int iLBT, int iMCSTR)
{
   log_line("----------------------------------------------------");
   log_line("Starting pairing processes in SiK search mode (%s)...", str_format_frequency(iFreqKhz));

   g_bSearching = true;
   g_iSearchFrequency = iFreqKhz;
   g_iSearchFirmwareType = MODEL_FIRMWARE_TYPE_RUBY;
   g_iSearchSiKAirDataRate = iAirDataRate;
   g_iSearchSiKECC = iECC;
   g_iSearchSiKLBT = iLBT;
   g_iSearchSiKMCSTR = iMCSTR;

   return _pairing_start();
}

bool pairing_stop()
{
   if ( ! s_isRXStarted )
      return true;
   log_line("----------------------------------");
   log_line("Stopping pairing processes...");

   s_uPairingStartTime = 0;
   onEventBeforePairingStop();
   s_bPairingIsRouterReady = false;

   forward_streams_on_pairing_stop();
   if ( hardware_is_running_on_runcam_vrx() )
      hardware_led_green_set_off();

   _pairing_close_shared_mem();

   handle_commands_stop_on_pairing();

   ruby_signal_alive();
   controller_stop_tx_rc();

   ruby_signal_alive();
   controller_stop_router();

   ruby_signal_alive();
   controller_stop_rx_telemetry();

   ruby_signal_alive();
   controller_wait_for_stop_all();

   ruby_signal_alive();
   onEventPairingStopped();

   stop_pipes_to_router();
   handle_commands_stop_on_pairing();

   ruby_signal_alive();

   s_isRXStarted = false;
   s_isVideoReceiving = false;

   ruby_clear_all_ipc_channels();

   g_bIsRouterReady = false;
   g_RouterIsReadyTimestamp = 0;

   log_line("------------------------------------------");
   log_line("Stopped pairing processes successfully.");
   log_line("------------------------------------------");
   return true;
}

void pairing_on_router_ready()
{
   log_line("Pairing: received event that router is ready. Radio interfaces configuration (refreshed) when router is ready:");
   hardware_load_radio_info();
   log_line("Pairing: received event that router is ready. Open shared mem objects...");
   _pairing_open_shared_mem();
   log_line("Pairing: received event that router is ready. Open shared mem objects complete.");
   s_bPairingIsRouterReady = true;
}

void _pairing_open_shared_mem()
{
   hardware_sleep_ms(20);

   int iRetryCount = 20;
   int iAnyNewOpen = 0;
   int iAnyFailed = 0;

   for(int i=0; i<iRetryCount; i++ )
   {
      if ( NULL != g_pSMControllerRTInfo )
         break;
      g_pSMControllerRTInfo = controller_rt_info_open_for_read();
      
      hardware_sleep_ms(2);
      iAnyNewOpen++;
   }
   if ( NULL == g_pSMControllerRTInfo )
      iAnyFailed++;

   for(int i=0; i<iRetryCount; i++ )
   {
      if ( NULL != g_pSMControllerDebugVideoRTInfo )
         break;
      g_pSMControllerDebugVideoRTInfo = controller_debug_video_rt_info_open_for_read();
      
      hardware_sleep_ms(2);
      iAnyNewOpen++;
   }
   if ( NULL == g_pSMControllerDebugVideoRTInfo )
      iAnyFailed++;

   for( int i=0; i<iRetryCount; i++ )
   {
      if ( NULL != g_pSM_RCIn )
         break;
      g_pSM_RCIn = shared_mem_i2c_controller_rc_in_open_for_read();
      hardware_sleep_ms(2);
      iAnyNewOpen++;
   }
   if ( NULL == g_pSM_RCIn )
      iAnyFailed++;

   ruby_signal_alive();
   for( int i=0; i<iRetryCount; i++ )
   {
      if ( NULL != g_pSM_RadioStats )
         break;
      g_pSM_RadioStats = shared_mem_radio_stats_open_for_read();
      hardware_sleep_ms(2);
      iAnyNewOpen++;
   }
   if ( NULL == g_pSM_RadioStats )
      iAnyFailed++;

   ruby_signal_alive();
   for( int i=0; i<iRetryCount; i++ )
   {
      if ( NULL != g_pSM_VideoFramesStatsOutput )
         break;
      g_pSM_VideoFramesStatsOutput = shared_mem_video_frames_stats_open_for_read();
      hardware_sleep_ms(2);
      iAnyNewOpen++;
   }
   if ( NULL == g_pSM_VideoFramesStatsOutput )
      iAnyFailed++;

   /*
   ruby_signal_alive();
   for( int i=0; i<iRetryCount; i++ )
   {
      if ( NULL != g_pSM_VideoInfoStatsRadioIn )
         break;
      g_pSM_VideoInfoStatsRadioIn = shared_mem_video_frames_stats_radio_in_open_for_read();
      hardware_sleep_ms(2);
      iAnyNewOpen++;
   }
   if ( NULL == g_pSM_VideoInfoStatsRadioIn )
      iAnyFailed++;
   */

   ruby_signal_alive();
   for( int i=0; i<iRetryCount; i++ )
   {
      if ( NULL != g_pSM_VideoDecodeStats )
         break;
      g_pSM_VideoDecodeStats = shared_mem_video_stream_stats_rx_processors_open_for_read();
      hardware_sleep_ms(2);
      iAnyNewOpen++;
   }
   if ( NULL == g_pSM_VideoDecodeStats )
      iAnyFailed++;

   ruby_signal_alive();
   for( int i=0; i<iRetryCount; i++ )
   {
      if ( NULL != g_pSM_RadioRxQueueInfo )
         break;
      g_pSM_RadioRxQueueInfo = shared_mem_radio_rx_queue_info_open_for_read();
      hardware_sleep_ms(2);
      iAnyNewOpen++;
   }
   if ( NULL == g_pSM_RadioRxQueueInfo )
      iAnyFailed++;

   ruby_signal_alive();

   if ( (NULL != g_pCurrentModel) && (!g_bSearching) )
   {
      for( int i=0; i<iRetryCount; i++ )
      {
         if ( NULL != g_pSM_DownstreamInfoRC )
            break;
         g_pSM_DownstreamInfoRC = shared_mem_rc_downstream_info_open_read();
         hardware_sleep_ms(2);
         iAnyNewOpen++;
      }
      if ( NULL == g_pSM_DownstreamInfoRC )
         iAnyFailed++;
   }

   ruby_signal_alive();
   for( int i=0; i<iRetryCount; i++ )
   {
      if ( NULL != g_pSM_RouterVehiclesRuntimeInfo )
         break;
      g_pSM_RouterVehiclesRuntimeInfo = shared_mem_router_vehicles_runtime_info_open_for_read();
      hardware_sleep_ms(2);
      iAnyNewOpen++;
   }
   if ( NULL == g_pSM_RouterVehiclesRuntimeInfo )
      iAnyFailed++;

   if ( 0 == iAnyNewOpen )
      log_line("No new shared mem objects to open.");
   else
      log_line("Opened an additional %d shared mem objects.", iAnyNewOpen );

   if ( 0 != iAnyFailed )
      log_softerror_and_alarm("Failed to open %d shared memory objects.", iAnyFailed);

   synchronize_shared_mems();
}

void _pairing_close_shared_mem()
{
   controller_rt_info_close(g_pSMControllerRTInfo);
   g_pSMControllerRTInfo = NULL;

   controller_debug_video_rt_info_close(g_pSMControllerDebugVideoRTInfo);
   g_pSMControllerDebugVideoRTInfo = NULL;

   shared_mem_router_vehicles_runtime_info_close(g_pSM_RouterVehiclesRuntimeInfo);
   g_pSM_RouterVehiclesRuntimeInfo = NULL;

   shared_mem_radio_stats_rx_hist_close(g_pSM_HistoryRxStats);
   g_pSM_HistoryRxStats = NULL;

   shared_mem_radio_stats_close(g_pSM_RadioStats);
   g_pSM_RadioStats = NULL;


   shared_mem_video_frames_stats_close(g_pSM_VideoFramesStatsOutput);
   g_pSM_VideoFramesStatsOutput = NULL;

   //shared_mem_video_frames_stats_radio_in_close(g_pSM_VideoInfoStatsRadioIn);
   //g_pSM_VideoInfoStatsRadioIn = NULL;

   shared_mem_video_stream_stats_rx_processors_close(g_pSM_VideoDecodeStats);
   g_pSM_VideoDecodeStats = NULL;

   shared_mem_radio_rx_queue_info_close(g_pSM_RadioRxQueueInfo);
   g_pSM_RadioRxQueueInfo = NULL;


   shared_mem_rc_downstream_info_close(g_pSM_DownstreamInfoRC);
   g_pSM_DownstreamInfoRC = NULL;

   shared_mem_i2c_controller_rc_in_close(g_pSM_RCIn);
   g_pSM_RCIn = NULL;
}

bool pairing_isStarted()
{
   return s_isRXStarted;
}

u32 pairing_getStartTime()
{
   return s_uPairingStartTime;
}

bool pairing_isRouterReady()
{
   return s_bPairingIsRouterReady;
}

void pairing_loop()
{
   if ( ! pairing_isStarted() )
      return;
   
   if ( g_bSearching )
      return;
}
