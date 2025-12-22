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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/resource.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/socket.h> 
#include <getopt.h>
#include <semaphore.h>
#include <pthread.h>
#include <ctype.h>
#include <poll.h>
#include <unistd.h>

#include "../base/base.h"
#include "../base/shared_mem.h"
#include "../base/config.h"
#include "../base/commands.h"
#include "../base/hardware_procs.h"
#include "../base/hardware_files.h"
#include "../base/models.h"
#include "../base/models_list.h"
#include "../base/radio_utils.h"
#include "../base/encr.h"
#include "../base/ruby_ipc.h"
#include "../base/utils.h"
#include "../base/camera_utils.h"
#include "../base/vehicle_settings.h"
#include "../base/vehicle_rt_info.h"
#include "../base/hardware_radio_serial.h"
#include "../common/string_utils.h"
#include "../common/radio_stats.h"
#include "../common/relay_utils.h"

#include "shared_vars.h"
#include "timers.h"

#include "processor_tx_audio.h"
#include "processor_tx_video.h"
#include "process_radio_in_packets.h"
#include "process_radio_out_packets.h"
#include "launchers_vehicle.h"
#include "../utils/utils_vehicle.h"
#include "periodic_loop.h"

#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"
#include "../radio/radiopacketsqueue.h"
#include "../radio/radio_rx.h"
#include "../radio/radio_tx.h"
#include "../radio/radio_duplicate_det.h"
#include "../radio/fec.h" 
#include "packets_utils.h"
#include "ruby_rt_vehicle.h"
#include "radio_links.h"
#include "events.h"
#include "process_local_packets.h"
#include "processor_relay.h"
#include "test_link_params.h"
#include "adaptive_video.h"
#include "video_sources.h"
#include "video_source_csi.h"
#include "video_source_majestic.h"
#include "video_tx_buffers.h"
#include "negociate_radio.h"

#define MAX_RECV_UPLINK_HISTORY 12
#define SEND_ALARM_MAX_COUNT 5

sem_t* s_pSemaphoreStop = NULL;

static int s_iCountCPULoopOverflows = 0;

u8 s_BufferCommandsReply[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeTmpBufferCommandsReply[MAX_PACKET_TOTAL_SIZE];
int s_PipeTmpBufferCommandsReplyPos = 0;

u8 s_BufferTelemetryDownlink[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeTmpBufferTelemetryDownlink[MAX_PACKET_TOTAL_SIZE];
int s_PipeTmpBufferTelemetryDownlinkPos = 0;  

u8 s_BufferRCDownlink[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeTmpBufferRCDownlink[MAX_PACKET_TOTAL_SIZE];
int s_PipeTmpBufferRCDownlinkPos = 0;  

u16 s_countTXVideoPacketsOutPerSec[2];
u16 s_countTXDataPacketsOutPerSec[2];
u16 s_countTXCompactedPacketsOutPerSec[2];

u32 s_uTimeLastLoopOverloadError = 0;
u32 s_LoopOverloadErrorCount = 0;

extern u32 s_uLastAlarms;
extern u32 s_uLastAlarmsFlags1;
extern u32 s_uLastAlarmsFlags2;
extern u32 s_uLastAlarmsTime;
extern u32 s_uLastAlarmsCount;

u32 s_uTimeLastTryReadIPCMessages = 0;
u32 s_uLastFrameExpectedTxTimeMicros = 0;

static int s_iLongOperationCounter = 0;

void signal_start_long_op()
{
   s_iLongOperationCounter++;
   if ( 1 == s_iLongOperationCounter )
   {
      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_LONG_TASK, STREAM_ID_DATA);
      PH.vehicle_id_src = 1;
      PH.vehicle_id_dest = 1;
      PH.total_length = sizeof(t_packet_header);

      ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)&PH, PH.total_length);
      ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)&PH, PH.total_length);

      if ( NULL != g_pProcessStats )
      {
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
         g_pProcessStats->lastActiveTime = g_TimeNow;
      }
   }
}

void signal_end_long_op()
{
   if ( s_iLongOperationCounter <= 0 )
      return;
   s_iLongOperationCounter--;
   if ( 0 == s_iLongOperationCounter )
   {
      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_LONG_TASK, STREAM_ID_DATA);
      PH.vehicle_id_src = 0;
      PH.vehicle_id_dest = 0;
      PH.total_length = sizeof(t_packet_header);

      ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)&PH, PH.total_length);
      ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)&PH, PH.total_length);

      if ( NULL != g_pProcessStats )
      {
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
         g_pProcessStats->lastActiveTime = g_TimeNow;
      }
   }
}


void checkDeveloperFlagsChanges(u32 uOldDeveloperFlags, u32 uNewDeveloperFlags)
{
   if ( uOldDeveloperFlags == uNewDeveloperFlags )
   {
      log_line("Checking developer flags: no changes.");
      return;
   }

   log_line("Checking developer flags: changed from %u to %u, current flags: %s",
      uOldDeveloperFlags, uNewDeveloperFlags,
      str_get_developer_flags(uNewDeveloperFlags));

   bool bOldDevMode = (uOldDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE)?true:false;
   bool bNewDevMode = (uNewDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE)?true:false;
   if ( bOldDevMode != bNewDevMode )
   {
      video_sources_on_changed_developer_flags(uOldDeveloperFlags, uNewDeveloperFlags);
   }

   if ( (uNewDeveloperFlags & DEVELOPER_FLAGS_USE_PCAP_RADIO_TX ) != (uOldDeveloperFlags & DEVELOPER_FLAGS_USE_PCAP_RADIO_TX) )
   {
      log_line("Radio Tx mode (PPCAP/Socket) changed. Must reinit radio interfaces (async)...");
      radio_links_restart(true);
   }
}

void close_and_mark_sik_interfaces_to_reopen()
{
   int iCount = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
      if ( pRadioHWInfo->openedForWrite || pRadioHWInfo->openedForRead )
      {
         if ( (g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex == -1 ) ||
              (g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex == i) )
         {
            radio_rx_pause_interface(i, "SiK config start, close interfaces");
            radio_tx_pause_radio_interface(i, "SiK config start, close interfaces");
            g_SiKRadiosState.bInterfacesToReopen[i] = true;
            hardware_radio_sik_close(i);
            iCount++;
         }
      }
   }
   log_line("[Router] Closed SiK radio interfaces (%d closed) and marked them for reopening.", iCount);
}

void reopen_marked_sik_interfaces()
{
   int iCount = 0;
   int iCountSikInterfacesOpened = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
      if ( g_SiKRadiosState.bInterfacesToReopen[i] )
      {
         if ( hardware_radio_sik_open_for_read_write(i) <= 0 )
            log_softerror_and_alarm("[Router] Failed to reopen SiK radio interface %d", i+1);
         else
         {
            iCount++;
            iCountSikInterfacesOpened++;
            radio_tx_resume_radio_interface(i);
            radio_rx_resume_interface(i);
         }
      }
      g_SiKRadiosState.bInterfacesToReopen[i] = false;
   }

   log_line("[Router] Reopened SiK radio interfaces (%d reopened).", iCount);
}


void send_radio_config_to_controller()
{
   if ( NULL == g_pCurrentModel )
   {
      log_softerror_and_alarm("Tried to send current radio config to controller but there is no current model.");
      return;
   }
 
   log_line("Notify controller (send a radio message, from VID %u to VID %u) about current vehicle radio configuration.", g_pCurrentModel->uVehicleId, g_uControllerId);

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_RUBY_RADIO_CONFIG_UPDATED, STREAM_ID_DATA);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = g_uControllerId;
   PH.total_length = sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters) + sizeof(type_radio_links_parameters) + sizeof(type_radio_runtime_capabilities_parameters);

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet + sizeof(t_packet_header), (u8*)&(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));
   memcpy(packet + sizeof(t_packet_header) + sizeof(type_relay_parameters), (u8*)&(g_pCurrentModel->radioInterfacesParams), sizeof(type_radio_interfaces_parameters));
   memcpy(packet + sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters), (u8*)&(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));
   memcpy(packet + sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters) + sizeof(type_radio_links_parameters), (u8*)&(g_pCurrentModel->radioRuntimeCapabilities), sizeof(type_radio_runtime_capabilities_parameters));
   //send_packet_to_radio_interfaces(packet, PH.total_length, -1);
   packets_queue_add_packet(&g_QueueRadioPacketsOut, packet);
}

void send_radio_reinitialized_message()
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_RUBY_RADIO_REINITIALIZED, STREAM_ID_DATA);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = 0;
   PH.total_length = sizeof(t_packet_header);

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));

   send_packet_to_radio_interfaces(packet, PH.total_length, -1);
}

void flag_update_sik_interface(int iInterfaceIndex)
{
   if ( g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex >= 0 )
   {
      log_line("Router was re-flagged to reconfigure SiK radio interface %d.", iInterfaceIndex+1);
      return;
   }
   log_line("Router was flagged to reconfigure SiK radio interface %d.", iInterfaceIndex+1);
   g_SiKRadiosState.uTimeIntervalSiKReinitCheck = 500;
   g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex = iInterfaceIndex;
   g_SiKRadiosState.iThreadRetryCounter = 0;

   close_and_mark_sik_interfaces_to_reopen();
}

void flag_reinit_sik_interface(int iInterfaceIndex)
{
   // Already flagged ?

   if ( g_SiKRadiosState.bMustReinitSiKInterfaces )
      return;

   log_softerror_and_alarm("Router was flagged to reinit SiK radio interfaces.");
   g_SiKRadiosState.uTimeIntervalSiKReinitCheck = 500;
   g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex = -1;

   close_and_mark_sik_interfaces_to_reopen();

   g_SiKRadiosState.bMustReinitSiKInterfaces = true;
   g_SiKRadiosState.uSiKInterfaceIndexThatBrokeDown = (u32) iInterfaceIndex;
   g_SiKRadiosState.iThreadRetryCounter = 0;
}


void reinit_radio_interfaces()
{
   if ( g_bQuit || g_bReinitializeRadioInProgress )
      return;

   g_bReinitializeRadioInProgress = true;

   char szComm[128];
   log_line("Reinit radio interfaces (PID %d)...", getpid());

   sprintf(szComm, "touch %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_ALARM_ON);
   hw_execute_bash_command_silent(szComm, NULL);

   sprintf(szComm, "touch %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_REINIT_RADIO_IN_PROGRESS);
   hw_execute_bash_command_silent(szComm, NULL);

   u32 uTimeStart = get_current_timestamp_ms();

   radio_rx_stop_rx_thread();
   radio_links_close_rxtx_radio_interfaces();

   if ( g_bQuit )
   {
      g_bReinitializeRadioInProgress = false;
      return;
   }
   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

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

   hardware_radio_remove_stored_config();
   
   video_sources_stop_capture();
   // Clean up video pipe data
   video_sources_flush_discard_all_pending_data();

   while ( true )
   {
      log_line("Try to do recovery action...");
      hardware_sleep_ms(400);
      if ( get_current_timestamp_ms() > uTimeStart + 20000 )
      {
         g_bReinitializeRadioInProgress = false;
         hardware_reboot();
         sleep(10);
      }

      if ( NULL != g_pProcessStats )
      {
         g_TimeNow = get_current_timestamp_ms();
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }

      hw_execute_bash_command("/etc/init.d/udev restart", NULL);
      hardware_sleep_ms(200);
      hw_execute_bash_command("systemctl restart networking", NULL);
      hardware_sleep_ms(200);
      hw_execute_bash_command("ip link", NULL);

      hardware_sleep_ms(50);

      hw_execute_bash_command("systemctl stop networking", NULL);
      hardware_sleep_ms(200);
      hw_execute_bash_command("ip link", NULL);

      hardware_sleep_ms(50);

      if ( NULL != g_pProcessStats )
      {
         g_TimeNow = get_current_timestamp_ms();
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }

      char szOutput[4096];
      szOutput[0] = 0;
      hw_execute_bash_command_raw("ip link | grep wlan", szOutput);
      if ( 0 == strlen(szOutput) )
         continue;

      if ( NULL != g_pProcessStats )
      {
         g_TimeNow = get_current_timestamp_ms();
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }

      log_line("Reinitializing radio interfaces: found interfaces on ip link: [%s]", szOutput);
      hardware_radio_remove_stored_config();
      hardware_reset_radio_enumerated_flag();
      hardware_enumerate_radio_interfaces();
      if ( 0 < hardware_get_radio_interfaces_count() )
         break;
   }

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }
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
   
   hardware_radio_remove_stored_config();
   
   // Remove radio initialize file flag
   sprintf(szComm, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_RADIOS_CONFIGURED);
   hw_execute_bash_command(szComm, NULL);

   hw_execute_ruby_process_wait(NULL, "ruby_start", szCommRadioParams, NULL, 1);
   
   hardware_sleep_ms(100);
   hardware_reset_radio_enumerated_flag();
   hardware_enumerate_radio_interfaces();

   log_line("=================================================================");
   log_line("Detected hardware radio interfaces:");
   hardware_log_radio_info(NULL, 0);

   log_line("Setting all the cards frequencies again...");

   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] < 0 )
      {
         log_softerror_and_alarm("No radio link is assigned to radio interface %d", i+1);
         continue;
      }
      if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] >= g_pCurrentModel->radioLinksParams.links_count )
      {
         log_softerror_and_alarm("Invalid radio link (%d of max %d) is assigned to radio interface %d", i+1, g_pCurrentModel->radioInterfacesParams.interface_link_id[i]+1, g_pCurrentModel->radioLinksParams.links_count);
         continue;
      }
      g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[i] = g_pCurrentModel->radioLinksParams.link_frequency_khz[g_pCurrentModel->radioInterfacesParams.interface_link_id[i]];
      radio_utils_set_interface_frequency(g_pCurrentModel, i, g_pCurrentModel->radioInterfacesParams.interface_link_id[i], g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[i], g_pProcessStats, 0 );
   }
   log_line("Setting all the cards frequencies again. Done.");
   hardware_save_radio_info();
   hardware_sleep_ms(100);
 
   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

   radio_links_restart(false);

   log_line("Reinit radio interfaces: completed.");

   g_bRadioReinitialized = true;
   g_TimeRadioReinitialized = get_current_timestamp_ms();

   sprintf(szComm, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_REINIT_RADIO_IN_PROGRESS);
   hw_execute_bash_command_silent(szComm, NULL);

   sprintf(szComm, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_REINIT_RADIO_REQUEST);
   hw_execute_bash_command_silent(szComm, NULL); 

   // wait here so that whatchdog restarts everything

   log_line("Reinit radio interfaces procedure complete. Now waiting for watchdog to restart everything (PID: %d).", getpid());
   int iCount = 0;
   int iSkip = 1;
   while ( !g_bQuit )
   {
      hardware_sleep_ms(50);
      iCount++;
      iSkip--;

      if ( iSkip == 0 )
      {
         send_radio_reinitialized_message();
         iSkip = iCount;
      }
   }
   log_line("Reinit radio interfaces procedure was signaled to quit (PID: %d).", getpid());

   g_bReinitializeRadioInProgress = false;
}

void send_model_settings_to_controller()
{
   log_line("Send model settings to controller. Notify rx_commands to do it.");
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SEND_MODEL_SETTINGS, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = g_uControllerId;
   PH.total_length = sizeof(t_packet_header);

   ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)&PH, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
}

void _read_ipc_pipes(u32 uTimeNow)
{
   s_uTimeLastTryReadIPCMessages = uTimeNow;
   int maxToRead = 20;
   int maxPacketsToRead = maxToRead;

   while ( (maxPacketsToRead > 0) && (NULL != ruby_ipc_try_read_message(s_fIPCRouterFromCommands, s_PipeTmpBufferCommandsReply, &s_PipeTmpBufferCommandsReplyPos, s_BufferCommandsReply)) )
   {
      maxPacketsToRead--;
      t_packet_header* pPH = (t_packet_header*)s_BufferCommandsReply;      
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
         packets_queue_add_packet(&s_QueueControlPackets, s_BufferCommandsReply); 
      else
         packets_queue_add_packet(&g_QueueRadioPacketsOut, s_BufferCommandsReply);
   } 
   if ( maxToRead - maxPacketsToRead > 6 )
      log_line("Read %d messages from commands msgqueue.", maxToRead - maxPacketsToRead);

   maxPacketsToRead = maxToRead;
   while ( (maxPacketsToRead > 0) && (NULL != ruby_ipc_try_read_message(s_fIPCRouterFromTelemetry, s_PipeTmpBufferTelemetryDownlink, &s_PipeTmpBufferTelemetryDownlinkPos, s_BufferTelemetryDownlink)) )
   {
      maxPacketsToRead--;
      t_packet_header* pPH = (t_packet_header*)s_BufferTelemetryDownlink;      
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
         packets_queue_add_packet(&s_QueueControlPackets, s_BufferTelemetryDownlink); 
      else
         packets_queue_add_packet(&g_QueueRadioPacketsOut, s_BufferTelemetryDownlink); 
   }
   if ( maxToRead - maxPacketsToRead > 6 )
      log_line("Read %d messages from telemetry msgqueue.", maxToRead - maxPacketsToRead);

   maxPacketsToRead = maxToRead;
   while ( (maxPacketsToRead > 0) && (NULL != ruby_ipc_try_read_message(s_fIPCRouterFromRC, s_PipeTmpBufferRCDownlink, &s_PipeTmpBufferRCDownlinkPos, s_BufferRCDownlink)) )
   {
      maxPacketsToRead--;
      t_packet_header* pPH = (t_packet_header*)s_BufferRCDownlink;      
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
         packets_queue_add_packet(&s_QueueControlPackets, s_BufferRCDownlink); 
      else
         packets_queue_add_packet(&g_QueueRadioPacketsOut, s_BufferRCDownlink); 
   }
   if ( maxToRead - maxPacketsToRead > 3 )
      log_line("Read %d messages from RC msgqueue.", maxToRead - maxPacketsToRead);
}

int _consume_ipc_messages()
{
   int iConsumed = 0;
   int iMaxToConsume = 20;

   while ( packets_queue_has_packets(&s_QueueControlPackets) && (iMaxToConsume > 0) )
   {
      iMaxToConsume--;
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;

      int length = -1;
      u8* pBuffer = packets_queue_pop_packet(&s_QueueControlPackets, &length);
      if ( NULL == pBuffer || -1 == length )
         break;

      t_packet_header* pPH = (t_packet_header*)pBuffer;
      //log_line("Received local packet from component %s, type: %s", str_get_component_id(pPH->vehicle_id_src), str_get_packet_type(pPH->packet_type));
      process_local_control_packet(pPH);
      iConsumed++;
   }
   return iConsumed;
}

int process_and_send_packets(bool bIsEndOfTransmissionFrame)
{
   int iCountSent = 0;
   
   while ( packets_queue_has_packets(&g_QueueRadioPacketsOut) )
   {
      u32 uTime = get_current_timestamp_ms();
      if ( (0 != s_uTimeLastTryReadIPCMessages) && (uTime > s_uTimeLastTryReadIPCMessages + 500) )
      {
         log_softerror_and_alarm("Too much time since last ipc messages read (%u ms) while sending radio packets, read ipc messages.", uTime - s_uTimeLastTryReadIPCMessages);
         _read_ipc_pipes(uTime);
      }

      int iPacketLength = -1;
      u8* pPacketBuffer = packets_queue_pop_packet(&g_QueueRadioPacketsOut, &iPacketLength);
      if ( (NULL == pPacketBuffer) || (-1 == iPacketLength) )
         break;

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;

      preprocess_radio_out_packet(pPacketBuffer, iPacketLength, bIsEndOfTransmissionFrame);
      send_packet_to_radio_interfaces(pPacketBuffer, iPacketLength, -1);
      iCountSent++;
   }
   return iCountSent;
}

void _synchronize_shared_mems()
{
   /*
   if ( g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_FLAG_SHOW_STATS_VIDEO_H264_FRAMES_INFO)
   if ( g_TimeNow >= g_VideoInfoStatsCameraOutput.uTimeLastUpdate + 200 )
   {
      update_shared_mem_video_frames_stats( &g_VideoInfoStatsCameraOutput, g_TimeNow);

      if ( NULL != g_pSM_VideoInfoStatsCameraOutput )
         memcpy((u8*)g_pSM_VideoInfoStatsCameraOutput, (u8*)&g_VideoInfoStatsCameraOutput, sizeof(shared_mem_video_frames_stats));
      else
      {
        g_pSM_VideoInfoStatsCameraOutput = shared_mem_video_frames_stats_open_for_write();
        if ( NULL == g_pSM_VideoInfoStatsCameraOutput )
           log_error_and_alarm("Failed to open shared mem video info camera stats for write!");
        else
           log_line("Opened shared mem video info stats camera for write.");
      }
   }
   */

   /*
   if ( g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_FLAG_SHOW_STATS_VIDEO_H264_FRAMES_INFO)
   if ( g_TimeNow >= g_VideoInfoStatsRadioOut.uTimeLastUpdate + 200 )
   {
      update_shared_mem_video_frames_stats( &g_VideoInfoStatsRadioOut, g_TimeNow);

      if ( NULL != g_pSM_VideoInfoStatsRadioOut )
         memcpy((u8*)g_pSM_VideoInfoStatsRadioOut, (u8*)&g_VideoInfoStatsRadioOut, sizeof(shared_mem_video_frames_stats));
      else
      {
        g_pSM_VideoInfoStatsRadioOut = shared_mem_video_frames_stats_radio_out_open_for_write();
        if ( NULL == g_pSM_VideoInfoStatsRadioOut )
           log_error_and_alarm("Failed to open shared mem video info stats radio out for write!");
        else
           log_line("Opened shared mem video info stats radio out for write.");
      }
   }
   */

   static u32 s_uTimeLastRxHistorySync = 0;

   if ( g_TimeNow >= s_uTimeLastRxHistorySync + 100 )
   {
      s_uTimeLastRxHistorySync = g_TimeNow;
      memcpy((u8*)g_pSM_HistoryRxStats, (u8*)&g_SM_HistoryRxStats, sizeof(shared_mem_radio_stats_rx_hist));
   }
}

void cleanUp()
{
   radio_links_close_rxtx_radio_interfaces();

   if ( NULL != g_pProcessorTxAudio )
   {
      g_pProcessorTxAudio->stopLocalRecording();
      g_pProcessorTxAudio->closeAudioStream();
   }
   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->audio_params.has_audio_device) )
      vehicle_stop_audio_capture(g_pCurrentModel);

   video_sources_stop_capture();
   // Clean up video pipe data
   video_sources_flush_discard_all_pending_data();
   video_sources_uninit();

   ruby_close_ipc_channel(s_fIPCRouterToCommands);
   ruby_close_ipc_channel(s_fIPCRouterFromCommands);
   ruby_close_ipc_channel(s_fIPCRouterToTelemetry);
   ruby_close_ipc_channel(s_fIPCRouterFromTelemetry);
   ruby_close_ipc_channel(s_fIPCRouterToRC);
   ruby_close_ipc_channel(s_fIPCRouterFromRC);

   s_fIPCRouterToCommands = -1;
   s_fIPCRouterFromCommands = -1;
   s_fIPCRouterToTelemetry = -1;
   s_fIPCRouterFromTelemetry = -1;
   s_fIPCRouterToRC = -1;
   s_fIPCRouterFromRC = -1;
}
  
int router_open_pipes()
{
   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms(); 
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastRadioTxTime = g_TimeNow;
      g_pProcessStats->lastRadioRxTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }

   s_fIPCRouterFromRC = ruby_open_ipc_channel_read_endpoint(IPC_CHANNEL_TYPE_RC_TO_ROUTER);
   if ( s_fIPCRouterFromRC < 0 )
      return -1;

   s_fIPCRouterToRC = ruby_open_ipc_channel_write_endpoint(IPC_CHANNEL_TYPE_ROUTER_TO_RC);
   if ( s_fIPCRouterToRC < 0 )
      return -1;

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms(); 
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastRadioTxTime = g_TimeNow;
      g_pProcessStats->lastRadioRxTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }

   s_fIPCRouterToCommands = ruby_open_ipc_channel_write_endpoint(IPC_CHANNEL_TYPE_ROUTER_TO_COMMANDS);
   if ( s_fIPCRouterToCommands < 0 )
   {
      cleanUp();
      return -1;
   }
   
   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms(); 
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastRadioTxTime = g_TimeNow;
      g_pProcessStats->lastRadioRxTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }
 
   s_fIPCRouterFromCommands = ruby_open_ipc_channel_read_endpoint(IPC_CHANNEL_TYPE_COMMANDS_TO_ROUTER);
   if ( s_fIPCRouterFromCommands < 0 )
   {
      cleanUp();
      return -1;
   }
   
   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms(); 
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastRadioTxTime = g_TimeNow;
      g_pProcessStats->lastRadioRxTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }

   s_fIPCRouterFromTelemetry = ruby_open_ipc_channel_read_endpoint(IPC_CHANNEL_TYPE_TELEMETRY_TO_ROUTER);
   if ( s_fIPCRouterFromTelemetry < 0 )
   {
      cleanUp();
      return -1;
   }

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms(); 
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastRadioTxTime = g_TimeNow;
      g_pProcessStats->lastRadioRxTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }

   s_fIPCRouterToTelemetry = ruby_open_ipc_channel_write_endpoint(IPC_CHANNEL_TYPE_ROUTER_TO_TELEMETRY);
   if ( s_fIPCRouterToTelemetry < 0 )
   {
      cleanUp();
      return -1;
   }
   
   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms(); 
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastRadioTxTime = g_TimeNow;
      g_pProcessStats->lastRadioRxTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }
   return 0;
}

void _check_loop_consistency(int iStep, u32 uLastTotalTxPackets, u32 uLastTotalTxBytes, u32 tTime0, u32 tTime1, u32 tTime2, u32 tTime3, u32 tTime4, u32 tTime5)
{
   if ( g_TimeNow < g_TimeStart + 10000 )
      return;
   
   if ( (video_sources_get_capture_start_time() != 0) && ( g_TimeNow < video_sources_get_capture_start_time() + 4000 ) )
      return;

   if ( tTime5 > tTime0 + DEFAULT_MAX_LOOP_TIME_MILISECONDS )
   {
      uLastTotalTxPackets = g_SM_RadioStats.radio_links[0].totalTxPackets - uLastTotalTxPackets;
      uLastTotalTxBytes = g_SM_RadioStats.radio_links[0].totalTxBytes - uLastTotalTxBytes;

      s_iCountCPULoopOverflows++;
      if ( s_iCountCPULoopOverflows > 5 )
      if ( g_TimeNow > g_TimeLastSetRadioFlagsCommandReceived + 5000 )
      {
         log_line("Too many CPU loop overflows. Send alarm to controller (%d overflows, now overflow: %u ms)", s_iCountCPULoopOverflows, tTime5 - tTime0);
         send_alarm_to_controller(ALARM_ID_VEHICLE_CPU_LOOP_OVERLOAD,(tTime5-tTime0), 0, 2);
      }
      if ( tTime5 >= tTime0 + 500 )
      if ( g_TimeNow > g_TimeLastSetRadioFlagsCommandReceived + 5000 )
      {
         log_line("CPU loop overflow is too big (%d ms). Send alarm to controller", tTime5 - tTime0);
         send_alarm_to_controller(ALARM_ID_VEHICLE_CPU_LOOP_OVERLOAD,(tTime5-tTime0)<<16, 0, 2);
      }
      if ( g_TimeNow > s_uTimeLastLoopOverloadError + 3000 )
          s_LoopOverloadErrorCount = 0;
      s_uTimeLastLoopOverloadError = g_TimeNow;
      s_LoopOverloadErrorCount++;
      if ( (s_LoopOverloadErrorCount < 10) || ((s_LoopOverloadErrorCount%20)==0) )
         log_softerror_and_alarm("Router loop(%d), took too long to complete (%u milisec (%u + %u + %u + %u + %u ms)), sent %u packets, %u bytes!!!",
            iStep, tTime5 - tTime0, tTime1-tTime0, tTime2-tTime1, tTime3-tTime2, tTime4-tTime3, tTime5-tTime4, uLastTotalTxPackets, uLastTotalTxBytes);
   }
   else
      s_iCountCPULoopOverflows = 0;

   if ( NULL != g_pProcessStats )
   if ( g_TimeNow > g_TimeLastSetRadioFlagsCommandReceived + 5000 )
   {
      if ( g_pProcessStats->uMaxLoopTimeMs < tTime5 - tTime0 )
         g_pProcessStats->uMaxLoopTimeMs = tTime5 - tTime0;
      g_pProcessStats->uTotalLoopTime += tTime5 - tTime0;
      if ( 0 != g_pProcessStats->uLoopCounter )
         g_pProcessStats->uAverageLoopTimeMs = g_pProcessStats->uTotalLoopTime / g_pProcessStats->uLoopCounter;
   }
}

void _check_rx_loop_consistency()
{
   int iAnyBrokeInterface = radio_rx_any_interface_broken();
   if ( iAnyBrokeInterface > 0 )
   {
      if ( hardware_radio_index_is_sik_radio(iAnyBrokeInterface-1) )
         flag_reinit_sik_interface(iAnyBrokeInterface-1);
      else
      {
         reinit_radio_interfaces();
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
         send_alarm_to_controller(ALARM_ID_VEHICLE_RX_TIMEOUT,(u32)(iAnyRxTimeouts-1), (u32)iCount, 1);
      }
   }

   int iAnyRxErrors = radio_rx_any_interface_bad_packets();
   if ( iAnyRxErrors > 0 )
   {
      int iError = radio_rx_get_interface_bad_packets_error_and_reset(iAnyRxErrors-1);
      send_alarm_to_controller(ALARM_ID_RECEIVED_INVALID_RADIO_PACKET, (u32)iError, 0, 1);
   }

   u32 uMaxRxLoopTime = radio_rx_get_and_reset_max_loop_time();
   if ( (int)uMaxRxLoopTime > get_VehicleSettings()->iDevRxLoopTimeout )
   {
      static u32 s_uTimeLastAlarmRxLoopOverload = 0;
      if ( g_TimeNow > s_uTimeLastAlarmRxLoopOverload + 10000 )
      {
         s_uTimeLastAlarmRxLoopOverload = g_TimeNow;
         u32 uRead = radio_rx_get_and_reset_max_loop_time_read();
         u32 uQueue = radio_rx_get_and_reset_max_loop_time_queue();
         if ( uRead > 0xFFFF ) uRead = 0xFFFF;
         if ( uQueue > 0xFFFF ) uQueue = 0xFFFF;

         send_alarm_to_controller(ALARM_ID_VEHICLE_CPU_RX_LOOP_OVERLOAD, uMaxRxLoopTime, uRead | (uQueue<<16), 1);
      }
   }
}

void _broadcast_router_ready()
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_VEHICLE_ROUTER_READY, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = 0;
   PH.total_length = sizeof(t_packet_header);

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   
   if ( ! ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, buffer, PH.total_length) )
      log_softerror_and_alarm("No pipe to telemetry to broadcast router ready to.");

   log_line("Broadcasted that router is ready.");
}

void _try_receive_packets(u32 uReadFrameDuration)
{
   // Receive any high priority packets, if any

   u8* pPacket = NULL;
   int iPacketLength = 0;
   int iPacketIsShort = 0;
   int iRadioInterfaceIndex = 0;
   int iCountConsumedHighPrio = 0;

   g_pProcessStats->uLoopCounter2 = 0;
   g_pProcessStats->uLoopCounter3 = 0;

   u32 uTimeStart = g_TimeNow;
   u32 uTimeoutMicros = 300;
   if ( (uReadFrameDuration > 0) && (g_pCurrentModel->video_params.iVideoFPS > 0) && (uReadFrameDuration < (u32)1000/g_pCurrentModel->video_params.iVideoFPS/2) )
   {
      uTimeoutMicros = (u32)1000*1000/g_pCurrentModel->video_params.iVideoFPS/2;
      if ( uTimeoutMicros > 2000 )
         uTimeoutMicros = 2000;
   }

   while ( (iCountConsumedHighPrio < 10) && (!g_bQuit) )
   {
      pPacket = radio_rx_wait_get_next_received_high_prio_packet(uTimeoutMicros, &iPacketLength, &iPacketIsShort, &iRadioInterfaceIndex);
      if ( (NULL == pPacket) || g_bQuit )
         break;

      g_pProcessStats->uLoopCounter2++;
      iCountConsumedHighPrio++;
      uTimeoutMicros = 0;
      process_received_single_radio_packet(iRadioInterfaceIndex, pPacket, iPacketLength);
      shared_mem_radio_stats_rx_hist_update(&g_SM_HistoryRxStats, iRadioInterfaceIndex, pPacket, g_TimeNow);

      g_TimeNow = get_current_timestamp_ms();
      if ( g_TimeNow > uTimeStart + 500 )
      {
         log_softerror_and_alarm("Consuming radio rx packets takes too long (%u ms), read ipc messages.", g_TimeNow - uTimeStart);
         uTimeStart = g_TimeNow;
         _read_ipc_pipes(g_TimeNow);
      }
      if ( (0 != s_uTimeLastTryReadIPCMessages) && (g_TimeNow > s_uTimeLastTryReadIPCMessages + 500) )
      {
         log_softerror_and_alarm("Too much time since last ipc messages read (%u ms) while consuming radio messages, read ipc messages.", g_TimeNow - s_uTimeLastTryReadIPCMessages);
         uTimeStart = g_TimeNow;
         _read_ipc_pipes(g_TimeNow);
      }
   }

   // Receive any reg priority packets, if any

   int iCountConsumedRegPrio = 0;
   uTimeStart = g_TimeNow;

   while ( (iCountConsumedRegPrio < 50) && (!g_bQuit) )
   {
      pPacket = radio_rx_wait_get_next_received_reg_prio_packet(0, &iPacketLength, &iPacketIsShort, &iRadioInterfaceIndex);
      if ( (NULL == pPacket) || g_bQuit )
         break;

      g_pProcessStats->uLoopCounter3++;
      iCountConsumedRegPrio++;

      process_received_single_radio_packet(iRadioInterfaceIndex, pPacket, iPacketLength);
      shared_mem_radio_stats_rx_hist_update(&g_SM_HistoryRxStats, iRadioInterfaceIndex, pPacket, g_TimeNow);

      g_TimeNow = get_current_timestamp_ms();
      if ( g_TimeNow > uTimeStart + 500 )
      {
         log_softerror_and_alarm("Consuming radio rx packets takes too long (%u ms), read ipc messages.", g_TimeNow - uTimeStart);
         uTimeStart = g_TimeNow;
         _read_ipc_pipes(g_TimeNow);
      }
      if ( (0 != s_uTimeLastTryReadIPCMessages) && (g_TimeNow > s_uTimeLastTryReadIPCMessages + 500) )
      {
         log_softerror_and_alarm("Too much time since last ipc messages read (%u ms) while consuming radio messages, read ipc messages.", g_TimeNow - s_uTimeLastTryReadIPCMessages);
         uTimeStart = g_TimeNow;
         _read_ipc_pipes(g_TimeNow);
      }
   }

   // Check Radio Rx state

   if ( (0 == iCountConsumedHighPrio) && (0 == iCountConsumedRegPrio) )
   if ( (0 != radio_links_get_last_start_time()) && (g_TimeNow > radio_links_get_last_start_time() + 3000) )
   if ( (NULL != g_pProcessStats) && (0 != g_pProcessStats->lastRadioRxTime) && (g_TimeNow > TIMEOUT_LINK_TO_CONTROLLER_LOST) && (g_pProcessStats->lastRadioRxTime + TIMEOUT_LINK_TO_CONTROLLER_LOST < g_TimeNow) )
   {
      if ( g_TimeLastReceivedFastRadioPacketFromController + TIMEOUT_LINK_TO_CONTROLLER_LOST < g_TimeNow )
      if ( g_TimeLastReceivedSlowRadioPacketFromController + TIMEOUT_LINK_TO_CONTROLLER_LOST < g_TimeNow )
      if ( g_bHasFastUplinkFromController || g_bHasSlowUplinkFromController )
      {
         bool bHadSlowSpeedLink = g_bHasSlowUplinkFromController;
         bool bHadHighSpeedLink = g_bHasFastUplinkFromController;

         log_line("Link from controller lost. Last received fast radio packet: %u ms ago. Last received slow radio packet: %u ms ago.",
             g_TimeNow - g_TimeLastReceivedFastRadioPacketFromController,
             g_TimeNow - g_TimeLastReceivedSlowRadioPacketFromController);

         g_bHasFastUplinkFromController = false;
         g_bHasSlowUplinkFromController = false;

         if ( bHadHighSpeedLink )
            g_LastTimeLostFastLinkFromController = g_TimeNow;
         if ( bHadSlowSpeedLink )
            g_LastTimeLostSlowLinkFromController = g_TimeNow;

         adaptive_video_on_uplink_lost();
         //if ( g_pCurrentModel->osd_params.osd_preferences[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_PREFERENCES_BIT_FLAG_SHOW_CONTROLLER_LINK_LOST_ALARM )
            send_alarm_to_controller(ALARM_ID_LINK_TO_CONTROLLER_LOST, bHadHighSpeedLink?1:0, bHadSlowSpeedLink?1:0, 10);
      }
   }
}

void handle_sigint(int sig) 
{ 
   log_line("--------------------------");
   log_line("Caught signal to stop: %d", sig);
   log_line("--------------------------");
   g_bQuit = true;
   radio_rx_mark_quit();
} 

void _main_loop2();

int main(int argc, char *argv[])
{
   if ( strcmp(argv[argc-1], "-ver") == 0 )
   {
      printf("%d.%d (b-%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR, SYSTEM_SW_BUILD_NUMBER);
      return 0;
   }

   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   log_init("Router");
   log_arguments(argc, argv);
   log_line_forced_to_file("Linux mem page size: %d bytes", getpagesize());

   utils_log_radio_packets_sizes();

   hardware_detectBoardAndSystemType();
 
   hardware_files_check_config_folder();

   g_uControllerId = vehicle_utils_getControllerId();
   load_VehicleSettings();

   VehicleSettings* pVS = get_VehicleSettings();
   if ( NULL != pVS )
      radio_rx_set_timeout_interval(pVS->iDevRxLoopTimeout);
   hardware_serial_reload_ports_settings();

   hardware_enumerate_radio_interfaces(); 

   reset_sik_state_info(&g_SiKRadiosState);

   g_pProcessStats = shared_mem_process_stats_open_write(SHARED_MEM_WATCHDOG_ROUTER_TX);
   if ( NULL == g_pProcessStats )
      log_softerror_and_alarm("Start sequence: Failed to open shared mem for router process watchdog for writing: %s", SHARED_MEM_WATCHDOG_ROUTER_TX);
   else
      log_line("Start sequence: Opened shared mem for router process watchdog for writing.");

   loadAllModels();
   g_pCurrentModel = getCurrentModel();

   log_line("Current vehicle encryption type: %d", g_pCurrentModel->enc_flags);
   if ( g_pCurrentModel->enc_flags != MODEL_ENC_FLAGS_NONE )
   if ( ! lpp(NULL, 0) )
   {
      g_pCurrentModel->enc_flags = MODEL_ENC_FLAGS_NONE;
      saveCurrentModel();
   }
  
   log_line_forced_to_file("Start sequence: Loaded model. Developer flags: live log: %s, enable radio silence failsafe: %s, log only errors: %s, radio config guard interval: %d ms",
         (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LIVE_LOG)?"yes":"no",
         (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_RADIO_SILENCE_FAILSAFE)?"yes":"no",
         (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS)?"yes":"no",
          (int)((g_pCurrentModel->uDeveloperFlags >> DEVELOPER_FLAGS_WIFI_GUARD_DELAY_MASK_SHIFT) & 0xFF) );
   log_line_forced_to_file("Model flags: %u (%s), developer flags: %u (%s)",
      g_pCurrentModel->uModelFlags, str_get_model_flags(g_pCurrentModel->uModelFlags), g_pCurrentModel->uDeveloperFlags, str_get_developer_flags(g_pCurrentModel->uDeveloperFlags));
   log_line_forced_to_file("Start sequence: Vehicle has camera? %s", g_pCurrentModel->hasCamera()?"Yes":"No");
   log_line_forced_to_file("Start sequence: Board type: %s", str_get_hardware_board_name(hardware_getBoardType()));

   if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS )
      log_only_errors();

   #if defined(HW_PLATFORM_OPENIPC_CAMERA)
   log_line("Setting CPU speed for OpenIPC hardware to %d Mhs...", g_pCurrentModel->processesPriorities.iFreqARM);
   hardware_set_oipc_cpu_freq(g_pCurrentModel->processesPriorities.iFreqARM);
   #endif

   if ( g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_AFFINITY_CORES )
   {
      hw_set_current_thread_affinity("rt_vehicle", g_pCurrentModel->processesPriorities.iCoreRouter, g_pCurrentModel->processesPriorities.iCoreRouter);
      radio_rx_set_cpu_affinity(g_pCurrentModel->processesPriorities.iCoreRadioRx);
   }

   if ( g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS )
   {
      hw_set_priority_current_proc(g_pCurrentModel->processesPriorities.iThreadPriorityRouter);
      radio_rx_set_custom_thread_raw_priority(g_pCurrentModel->processesPriorities.iThreadPriorityRadioRx);
      radio_tx_set_custom_thread_raw_priority(g_pCurrentModel->processesPriorities.iThreadPriorityRadioTx);
   }

   if ( g_pCurrentModel->uModelFlags & MODEL_FLAG_DISABLE_ALL_LOGS )
   {
      log_line("Log is disabled on vehicle. Disabled logs.");
      log_disable();
   }

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

   if ( -1 == router_open_pipes() )
      log_error_and_alarm("Start sequence: Failed to open some pipes.");
   
   g_bVehicleArmed = false;
   if ( g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_FORCE_ARMED )
      g_bVehicleArmed = true;

   radio_stats_reset(&g_SM_RadioStats, g_pCurrentModel->osd_params.iRadioInterfacesGraphRefreshIntervalMs);

   g_SM_RadioStats.countLocalRadioInterfaces = hardware_get_radio_interfaces_count();
   if ( NULL != g_pCurrentModel )
      g_SM_RadioStats.countVehicleRadioLinks = g_pCurrentModel->radioLinksParams.links_count;
   else
      g_SM_RadioStats.countVehicleRadioLinks = 1;

   g_SM_RadioStats.countLocalRadioLinks = g_SM_RadioStats.countVehicleRadioLinks;
   
   for( int i=0; i<g_SM_RadioStats.countLocalRadioLinks; i++ )
      g_SM_RadioStats.radio_links[i].matchingVehicleRadioLinkId = i;

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

   hardware_sleep_ms(50);
   radio_init_link_structures();
   if ( g_pCurrentModel->rc_params.rc_enabled )
   {
      log_line("RC link is enabled. Slow down telemetry packets frequency on slow links.");
      radio_reset_packets_default_frequencies(1);
   }
   radio_enable_crc_gen(1);
   packet_utils_init();
   radio_duplicate_detection_init();

   if ( ! radio_links_restart(false) )
   {
      g_bQuit = true;
      packet_utils_uninit();
      shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_ROUTER_TX, g_pProcessStats);
      radio_link_cleanup();
      return -1;
   }

   s_countTXVideoPacketsOutPerSec[0] = s_countTXVideoPacketsOutPerSec[1] = 0;
   s_countTXDataPacketsOutPerSec[0] = s_countTXDataPacketsOutPerSec[1] = 0;
   s_countTXCompactedPacketsOutPerSec[0] = s_countTXCompactedPacketsOutPerSec[1] = 0;

   for( int i=0; i<MAX_HISTORY_VEHICLE_TX_STATS_SLICES; i++ )
   {
      g_PHVehicleTxStats.historyTxGapMaxMiliseconds[i] = 0xFF;
      g_PHVehicleTxStats.historyTxGapMinMiliseconds[i] = 0xFF;
      g_PHVehicleTxStats.historyTxGapAvgMiliseconds[i] = 0xFF;
      g_PHVehicleTxStats.historyTxPackets[i] = 0;
      g_PHVehicleTxStats.historyVideoPacketsGapMax[i] = 0xFF;
      g_PHVehicleTxStats.historyVideoPacketsGapAvg[i] = 0xFF;
   }
   g_PHVehicleTxStats.tmp_uAverageTxSum = 0;
   g_PHVehicleTxStats.tmp_uAverageTxCount = 0;
   g_PHVehicleTxStats.tmp_uVideoIntervalsSum = 0;
   g_PHVehicleTxStats.tmp_uVideoIntervalsCount = 0;

   g_TimeLastHistoryTxComputation = get_current_timestamp_ms();
   g_TimeLastTxPacket = get_current_timestamp_ms();

   memset((u8*)&g_SM_DevVideoBitrateHistory, 0, sizeof(shared_mem_dev_video_bitrate_history));
   g_SM_DevVideoBitrateHistory.uGraphSliceInterval = 100;
   g_SM_DevVideoBitrateHistory.uTotalDataPoints = MAX_INTERVALS_VIDEO_BITRATE_HISTORY;
   g_SM_DevVideoBitrateHistory.uCurrentDataPoint = 0;
   g_SM_DevVideoBitrateHistory.uLastTimeSendToController = 0;

   log_line("Start sequence: Done setting up stats structures.");

   g_pSM_HistoryRxStats = shared_mem_radio_stats_rx_hist_open_for_write();

   if ( NULL == g_pSM_HistoryRxStats )
      log_softerror_and_alarm("Start sequence: Failed to open history radio rx stats shared memory for write.");
   else
      shared_mem_radio_stats_rx_hist_reset(&g_SM_HistoryRxStats);
  
   log_line("Start sequence: Done setting up radio stats history.");

   video_sources_init();

   adaptive_video_init();
   video_sources_start_capture();   
   
   g_pVideoTxBuffers = new VideoTxPacketsBuffer(0,0);
   g_pVideoTxBuffers->init(g_pCurrentModel);
   
   g_pProcessorTxVideo = new ProcessorTxVideo(0,0);
   g_pProcessorTxVideo->init();

   adaptive_video_load_state();

   log_line("Start sequence: Done creating video processor.");
   
   /*
   g_pSM_VideoInfoStatsCameraOutput = shared_mem_video_frames_stats_open_for_write();
   if ( NULL == g_pSM_VideoInfoStatsCameraOutput )
      log_error_and_alarm("Start sequence: Failed to open shared mem video camera info stats for write!");
   else
      log_line("Start sequence: Opened shared mem video camera info stats for write.");

   g_pSM_VideoInfoStatsRadioOut = shared_mem_video_frames_stats_radio_out_open_for_write();
   if ( NULL == g_pSM_VideoInfoStatsRadioOut )
      log_error_and_alarm("Start sequence: Failed to open shared mem video info stats radio out for write!");
   else
      log_line("Start sequence: Opened shared mem video info stats radio out for write.");

   memset(&g_VideoInfoStatsCameraOutput, 0, sizeof(shared_mem_video_frames_stats));
   memset(&g_VideoInfoStatsRadioOut, 0, sizeof(shared_mem_video_frames_stats));
   */

   g_pProcessorTxAudio = new ProcessorTxAudio();
   g_pProcessorTxAudio->init(g_pCurrentModel);
   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->audio_params.has_audio_device) && (g_pCurrentModel->audio_params.enabled) )
   {
      vehicle_launch_audio_capture(g_pCurrentModel);
      g_pProcessorTxAudio->openAudioStream();
   }
   else
      log_line("Start sequence: Audio is not enabled (has device: %s, enabled: %s).", g_pCurrentModel->audio_params.has_audio_device?"yes":"no", g_pCurrentModel->audio_params.enabled?"yes":"no");

   log_line("Start sequence: Done creating audio processor.");

   s_pSemaphoreStop = sem_open(SEMAPHORE_STOP_VEHICLE_ROUTER, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( (NULL == s_pSemaphoreStop) || (SEM_FAILED == s_pSemaphoreStop) )
      log_error_and_alarm("Failed to open semaphore: %s", SEMAPHORE_STOP_VEHICLE_ROUTER);
   else
      log_line("Opened semaphore for watching for stop signal.");
   
   send_radio_config_to_controller();

   reset_counters(&g_CoutersMainLoop);
   log_line("Running main loop for sync type: %d", g_pCurrentModel->rxtx_sync_type);

   log_line("");
   log_line("");
   log_line("----------------------------------------------"); 
   log_line("         Started all ok. Running now."); 
   log_line("----------------------------------------------"); 
   log_line("");
   log_line("");

   hardware_sleep_ms(20);

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      
      if ( pRadioHWInfo->openedForRead && pRadioHWInfo->openedForWrite )
         log_line(" * Interface %d: %s, %s, %s opened for read/write, radio link %d", i+1, pRadioHWInfo->szName, pRadioHWInfo->szDriver, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), g_pCurrentModel->radioInterfacesParams.interface_link_id[i]+1 );
      else if ( pRadioHWInfo->openedForRead )
         log_line(" * Interface %d: %s, %s, %s opened for read, radio link %d", i+1, pRadioHWInfo->szName, pRadioHWInfo->szDriver, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), g_pCurrentModel->radioInterfacesParams.interface_link_id[i]+1 );
      else if ( pRadioHWInfo->openedForWrite )
         log_line(" * Interface %d: %s, %s, %s opened for write, radio link %d", i+1, pRadioHWInfo->szName, pRadioHWInfo->szDriver, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), g_pCurrentModel->radioInterfacesParams.interface_link_id[i]+1 );
      else
         log_line(" * Interface %d: %s, %s, %s not used, radio link %d", i+1, pRadioHWInfo->szName, pRadioHWInfo->szDriver, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), g_pCurrentModel->radioInterfacesParams.interface_link_id[i]+1 );
   }

   g_TimeNow = get_current_timestamp_ms();
   g_TimeStart = get_current_timestamp_ms(); 

   if ( NULL != g_pProcessStats )
   {
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastRadioTxTime = g_TimeNow;
      g_pProcessStats->lastRadioRxTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }
      
   _broadcast_router_ready();

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( pRadioHWInfo->uExtraFlags & RADIO_HW_EXTRA_FLAG_FIRMWARE_OLD )
         send_alarm_to_controller(ALARM_ID_FIRMWARE_OLD, i, 0, 5);
   }

   if ( g_pCurrentModel->hasCamera() )
   {
      // Clear up video pipes on start
      video_sources_flush_discard_all_pending_data();
   }

   g_TimeNow = get_current_timestamp_ms();

   // -----------------------------------------------------------
   // Main loop here
   
   int iLoopTimeErrorsCount = 0;
   u32 uLastLoopTime = g_TimeNow;
   g_pProcessStats->uLoopTimer1 = g_pProcessStats->uLoopTimer2 = g_pProcessStats->uLoopTimer3 = g_pProcessStats->uLoopTimer4 = g_TimeNow;

   while ( !g_bQuit )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->uLoopSubStep = 0;
      g_pProcessStats->uLoopCounter++;

      u32 uMaxDelta = 6 + s_uLastFrameExpectedTxTimeMicros/1000;
      bool bError = false;
      char szError[128];
      if ( g_TimeNow - uLastLoopTime > uMaxDelta )
      {
         bError = true;
         sprintf(szError, "Main loop (tx was expected to be %.1f ms) took too long", (float)s_uLastFrameExpectedTxTimeMicros/1000.0);
      }
      if ( g_pProcessStats->uLoopTimer3 - g_pProcessStats->uLoopTimer2 > 5 )
      {
         bError = true;
         sprintf(szError, "Main loop Rx packets took too long (%u ms)", g_pProcessStats->uLoopTimer3 - g_pProcessStats->uLoopTimer2);
      }
      if ( bError )
      {
         iLoopTimeErrorsCount++;
         log_softerror_and_alarm("%s; Loop: %u ms (%u+%u+%u+%u+%u) cnt1 (read cam ms): %u, cnt4 (sent pckts ms): %u, cnt2: %u, cnt3: %u, cnt5 (sent pckts): %d",
            szError, g_TimeNow - uLastLoopTime,
            g_pProcessStats->uLoopTimer1 - uLastLoopTime,
            g_pProcessStats->uLoopTimer2 - g_pProcessStats->uLoopTimer1,
            g_pProcessStats->uLoopTimer3 - g_pProcessStats->uLoopTimer2,
            g_pProcessStats->uLoopTimer4 - g_pProcessStats->uLoopTimer3,
            g_TimeNow - g_pProcessStats->uLoopTimer4,
            g_pProcessStats->uLoopCounter1, g_pProcessStats->uLoopCounter4,
            g_pProcessStats->uLoopCounter2, g_pProcessStats->uLoopCounter3,
            g_pProcessStats->uLoopCounter5);

         if ( (NULL != g_pCurrentModel) && g_pCurrentModel->hasCamera() )
         if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
            video_source_csi_log_input_data();
      }
  
      uLastLoopTime = g_TimeNow;
      g_uLoopCounter++;

      _main_loop2();
      if ( NULL != g_pProcessStats )
         g_pProcessStats->uLoopSubStep = 0xFF;

      if ( is_semaphore_signaled_clear(s_pSemaphoreStop, SEMAPHORE_STOP_VEHICLE_ROUTER) )
      {
         log_line("Semaphore to stop is set. Quit now.");
         g_bQuit = true;
         break;
      }

      if ( g_bQuit )
         break;
   }
   if ( NULL != g_pProcessStats )
      g_pProcessStats->uLoopSubStep = 0xFFFFFFFF;

   // End main loop
   //------------------------------------------------------------

   log_line("Stopping...");

   if ( NULL != s_pSemaphoreStop )
      sem_close(s_pSemaphoreStop);
   sem_unlink(SEMAPHORE_STOP_VEHICLE_ROUTER);

   packet_utils_uninit();
   radio_rx_stop_rx_thread();
   radio_link_cleanup();

   radio_links_close_rxtx_radio_interfaces();

   cleanUp();

   delete g_pProcessorTxVideo;
   delete g_pVideoTxBuffers;
   shared_mem_radio_stats_rx_hist_close(g_pSM_HistoryRxStats);
   //shared_mem_video_frames_stats_close(g_pSM_VideoInfoStatsCameraOutput);
   //shared_mem_video_frames_stats_radio_out_close(g_pSM_VideoInfoStatsRadioOut);
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_ROUTER_TX, g_pProcessStats);
   log_line("Stopped.Exit now. (PID %d)", getpid());
   log_line("---------------------\n");
   return 0;
}

void _update_main_loop_debug_info()
{
   g_CoutersMainLoop.uCounter++;
   g_CoutersMainLoop.uCounter2++;

   if ( g_CoutersMainLoop.uTime == 0 )
   {
      g_CoutersMainLoop.uTime = g_TimeNow;
      g_CoutersMainLoop.uTime2 = g_TimeNow;
      return;
   }

   if ( g_TimeNow < g_CoutersMainLoop.uTime + 500 )
      return;


   // Compute frames per second values
   u32 dTime = g_TimeNow - g_CoutersMainLoop.uTime;
   g_CoutersMainLoop.uValueNow = (g_CoutersMainLoop.uCounter2 * 1000) / dTime;

   if ( (0 == g_CoutersMainLoop.uValueMinim) || (g_CoutersMainLoop.uValueNow < g_CoutersMainLoop.uValueMinim) )
      g_CoutersMainLoop.uValueMinim = g_CoutersMainLoop.uValueNow;
   if ( (0 == g_CoutersMainLoop.uValueMaxim) || (g_CoutersMainLoop.uValueNow > g_CoutersMainLoop.uValueMaxim) )
      g_CoutersMainLoop.uValueMaxim = g_CoutersMainLoop.uValueNow;
   if ( 0 == g_CoutersMainLoop.uValueAverage )
      g_CoutersMainLoop.uValueAverage = g_CoutersMainLoop.uValueNow;
   else
      g_CoutersMainLoop.uValueAverage = (g_CoutersMainLoop.uValueAverage*9)/10 + g_CoutersMainLoop.uValueNow/10;

   if ( (0 == g_CoutersMainLoop.uValueMinimLocal) || (g_CoutersMainLoop.uValueNow < g_CoutersMainLoop.uValueMinimLocal) )
      g_CoutersMainLoop.uValueMinimLocal = g_CoutersMainLoop.uValueNow;
   if ( (0 == g_CoutersMainLoop.uValueMaximLocal) || (g_CoutersMainLoop.uValueNow > g_CoutersMainLoop.uValueMaximLocal) )
      g_CoutersMainLoop.uValueMaximLocal = g_CoutersMainLoop.uValueNow;
   if ( 0 == g_CoutersMainLoop.uValueAverageLocal )
      g_CoutersMainLoop.uValueAverageLocal = g_CoutersMainLoop.uValueNow;
   else
      g_CoutersMainLoop.uValueAverageLocal = (g_CoutersMainLoop.uValueAverageLocal*9)/10 + g_CoutersMainLoop.uValueNow/10;


   g_CoutersMainLoop.uTime = g_TimeNow;
   g_CoutersMainLoop.uCounter2 = 0;

   if ( g_TimeNow > g_CoutersMainLoop.uTime2 + 20000 )
   {
      g_CoutersMainLoop.uValueAverageLocal = 0;
      g_CoutersMainLoop.uValueMinimLocal = 0;
      g_CoutersMainLoop.uValueMaximLocal = 0;
      g_CoutersMainLoop.uTime2 = g_TimeNow;
   }
}

extern u8 s_uLastRadioPingId;
extern u32 s_uLastRadioPingSentTime;

#define MAX_CAM_FRAMES_HIST 100
int g_iDbgCamHistBuffIndex = 0;
int g_iDbgCamHistCountReads[MAX_CAM_FRAMES_HIST];
u32 g_uDbgCamHistReadTime[MAX_CAM_FRAMES_HIST];
int g_iDbgCamHistSendCount[MAX_CAM_FRAMES_HIST];
u32 g_uDbgCamHistSendTime[MAX_CAM_FRAMES_HIST];
int g_iDbgCamHistSendOtherCount[MAX_CAM_FRAMES_HIST];
u32 g_uDbgCamHistSendOtherTime[MAX_CAM_FRAMES_HIST];

void _show_cam_hist_dbg_stats()
{
   static u32 s_uLastTimeDbgCamHistStats = 0;
   if ( g_TimeNow < s_uLastTimeDbgCamHistStats + 2000 )
      return;
   s_uLastTimeDbgCamHistStats = g_TimeNow;

   int iCountMin = 1000;
   int iCountMax = 0;
   int iCountAvg = 0;

   u32 uTimeReadMin = 1000;
   u32 uTimeReadMax = 0;
   u32 uTimeReadAvg = 0;

   int iCountSendMin = 1000;
   int iCountSendMax = 0;
   int iCountSendAvg = 0;

   u32 uTimeSendMin = 1000;
   u32 uTimeSendMax = 0;
   u32 uTimeSendAvg = 0;

   int iCountSendOtherMin = 1000;
   int iCountSendOtherMax = 0;
   int iCountSendOtherAvg = 0;

   u32 uTimeSendOtherMin = 1000;
   u32 uTimeSendOtherMax = 0;
   u32 uTimeSendOtherAvg = 0;

   for( int i=0; i<MAX_CAM_FRAMES_HIST; i++ )
   {
      if ( g_iDbgCamHistCountReads[i] < iCountMin ) iCountMin = g_iDbgCamHistCountReads[i];
      if ( g_iDbgCamHistCountReads[i] > iCountMax ) iCountMax = g_iDbgCamHistCountReads[i];
      iCountAvg += g_iDbgCamHistCountReads[i];

      if ( g_uDbgCamHistReadTime[i] < uTimeReadMin ) uTimeReadMin = g_uDbgCamHistReadTime[i];
      if ( g_uDbgCamHistReadTime[i] > uTimeReadMax ) uTimeReadMax = g_uDbgCamHistReadTime[i];
      uTimeReadAvg += g_uDbgCamHistReadTime[i];

      if ( g_iDbgCamHistSendCount[i] < iCountSendMin ) iCountSendMin = g_iDbgCamHistSendCount[i];
      if ( g_iDbgCamHistSendCount[i] > iCountSendMax ) iCountSendMax = g_iDbgCamHistSendCount[i];
      iCountSendAvg += g_iDbgCamHistSendCount[i];

      if ( g_uDbgCamHistSendTime[i] < uTimeSendMin ) uTimeSendMin = g_uDbgCamHistSendTime[i];
      if ( g_uDbgCamHistSendTime[i] > uTimeSendMax ) uTimeSendMax = g_uDbgCamHistSendTime[i];
      uTimeSendAvg += g_uDbgCamHistSendTime[i];

      if ( g_iDbgCamHistSendOtherCount[i] < iCountSendOtherMin ) iCountSendOtherMin = g_iDbgCamHistSendOtherCount[i];
      if ( g_iDbgCamHistSendOtherCount[i] > iCountSendOtherMax ) iCountSendOtherMax = g_iDbgCamHistSendOtherCount[i];
      iCountSendOtherAvg += g_iDbgCamHistSendOtherCount[i];

      if ( g_uDbgCamHistSendOtherTime[i] < uTimeSendOtherMin ) uTimeSendOtherMin = g_uDbgCamHistSendOtherTime[i];
      if ( g_uDbgCamHistSendOtherTime[i] > uTimeSendOtherMax ) uTimeSendOtherMax = g_uDbgCamHistSendOtherTime[i];
      uTimeSendOtherAvg += g_uDbgCamHistSendOtherTime[i];
   }

   float fCountAvg = (float)iCountAvg / (float)MAX_CAM_FRAMES_HIST;
   float fTimeReadAvg = (float)uTimeReadAvg / (float)MAX_CAM_FRAMES_HIST;
   float fCountSendAvg = (float)iCountSendAvg / (float)MAX_CAM_FRAMES_HIST;
   float fTimeSendAvg = (float)uTimeSendAvg / (float)MAX_CAM_FRAMES_HIST;
   float fCountSendOtherAvg = (float)iCountSendOtherAvg / (float)MAX_CAM_FRAMES_HIST;
   float fTimeSendOtherAvg = (float)uTimeSendOtherAvg / (float)MAX_CAM_FRAMES_HIST;

   log_line("* Cam read count (avg/min/max): %.1f, %d, %d", fCountAvg, iCountMin, iCountMax);
   log_line("* Cam read time (avg/min/max) ms: %.1f ms, %u ms, %u ms", fTimeReadAvg, uTimeReadMin, uTimeReadMax);
   log_line("* Cam send pckts video count (avg/min/max): %.1f, %d, %d", fCountSendAvg, iCountSendMin, iCountSendMax);
   log_line("* Cam send time (avg/min/max) ms: %.1f ms, %u ms, %u ms", fTimeSendAvg, uTimeSendMin, uTimeSendMax);
   log_line("* Cam send others count (avg/min/max): %.1f, %d, %d", fCountSendOtherAvg, iCountSendOtherMin, iCountSendOtherMax);
   log_line("* Cam send pctks others time (avg/min/max) ms: %.1f ms, %u ms, %u ms", fTimeSendOtherAvg, uTimeSendOtherMin, uTimeSendOtherMax);
}

void _main_loop2()
{
   g_pProcessStats->uLoopSubStep = 1;
   _update_main_loop_debug_info();
   g_pProcessStats->uLoopSubStep = 2;
   
   bool bHasCamera = g_pCurrentModel->hasCamera();
   bool bEndOfFrame = false;
   bool bReadAnyCameraFrameData = false;
   u32 uLastCameraReadDuration = 0;
   u32 uLastVideoFrameSendVideoDuration = 0;
   u32 uLastVideoFrameReadAndSendVideoDuration = 0;
   u32 uLastVideoFrameSendOtherDuration = 0;
   u32 uLastVideoFrameReadAndSendAllDuration = 0;
   u32 uTimeStartFrame = g_TimeNow;

   //--------------------------------------------
   // Video/camera read
   g_pProcessStats->uLoopCounter1 = 0;
   g_pProcessStats->uLoopCounter4 = 0;
   g_pProcessStats->uLoopCounter5 = 0;
   if ( bHasCamera )
   {
      g_pProcessStats->uLoopCounter1 = get_current_timestamp_ms();

      int iCountReadCamera = 50;
      do
      {
         iCountReadCamera--;
         // Read camera times out in 0.2ms if no camera data is available
         bReadAnyCameraFrameData = video_sources_try_read_camera_frame(&bEndOfFrame);
         if ( bReadAnyCameraFrameData && bEndOfFrame )
         {
            g_TimeNow = get_current_timestamp_ms();
            uLastCameraReadDuration = g_TimeNow - uTimeStartFrame;
            adaptive_video_on_end_of_frame();
         }
      } while ( bReadAnyCameraFrameData && (!bEndOfFrame) && (iCountReadCamera > 0) );

      g_pProcessStats->uLoopCounter1 = get_current_timestamp_ms() - g_pProcessStats->uLoopCounter1;

      if ( bReadAnyCameraFrameData && bEndOfFrame && g_pVideoTxBuffers->hasPendingPacketsToSend() )
      {
         if ( ! (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS) )
         {
            g_iDbgCamHistBuffIndex = (g_iDbgCamHistBuffIndex+1) % MAX_CAM_FRAMES_HIST;
            g_iDbgCamHistSendOtherCount[g_iDbgCamHistBuffIndex] = 0;

            g_iDbgCamHistCountReads[g_iDbgCamHistBuffIndex] = 100 - iCountReadCamera;
            g_uDbgCamHistReadTime[g_iDbgCamHistBuffIndex] = uLastCameraReadDuration;
         }

         int iCountPacketsAfterVideo = packets_queue_has_packets(&g_QueueRadioPacketsOut);
         if ( g_pCurrentModel->audio_params.has_audio_device && g_pCurrentModel->audio_params.enabled )
         if ( NULL != g_pProcessorTxAudio )
            iCountPacketsAfterVideo += g_pProcessorTxAudio->getPendingAudioPackets();

         u32 uTmp = get_current_timestamp_ms();

         // Send audio packets if any
         if ( g_pCurrentModel->audio_params.has_audio_device && g_pCurrentModel->audio_params.enabled )
         if ( NULL != g_pProcessorTxAudio )
            g_pProcessorTxAudio->sendAudioPackets();

         // Send video packets
         g_pProcessStats->uLoopSubStep = 10;
         u32 uTime = g_TimeNow;
         g_iDbgCamHistSendCount[g_iDbgCamHistBuffIndex] = g_pVideoTxBuffers->sendAvailablePackets(iCountPacketsAfterVideo);
         s_uLastFrameExpectedTxTimeMicros = g_pVideoTxBuffers->getLastFrameExpectedTxTimeMicros();

         g_TimeNow = get_current_timestamp_ms();
         uLastVideoFrameSendVideoDuration = g_TimeNow - uTime;
         uLastVideoFrameReadAndSendVideoDuration = g_TimeNow - uTimeStartFrame;

         g_pProcessStats->uLoopCounter4 = g_TimeNow - uTmp;
         g_pProcessStats->uLoopCounter5 = g_iDbgCamHistSendCount[g_iDbgCamHistBuffIndex];

         if ( ! (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS) )
         {
            g_uDbgCamHistSendTime[g_iDbgCamHistBuffIndex] = uLastVideoFrameSendVideoDuration;
            g_iDbgCamHistSendOtherCount[g_iDbgCamHistBuffIndex] = 0;
         }
      }
   }
   else // No camera present
   {
      bReadAnyCameraFrameData = true;
      bEndOfFrame = true;
   }

   g_pProcessStats->uLoopTimer1 = get_current_timestamp_ms();

   // Send any pending telemetry/commands/etc after video data frame end

   if ( bReadAnyCameraFrameData && bEndOfFrame )
   {
      if ( packets_queue_has_packets(&g_QueueRadioPacketsOut) )
         g_iDbgCamHistSendOtherCount[g_iDbgCamHistBuffIndex] += process_and_send_packets(true);
      u32 uTime = get_current_timestamp_ms();
      uLastVideoFrameSendOtherDuration = uTime - g_TimeNow;
      if ( g_iDbgCamHistSendOtherCount[g_iDbgCamHistBuffIndex] > 0 )
      if ( uLastVideoFrameSendOtherDuration == 0 )
         uLastVideoFrameSendOtherDuration = 1;
      uLastVideoFrameReadAndSendAllDuration = uTime - uTimeStartFrame;;
      g_TimeNow = uTime;
   }

   if ( bHasCamera && bReadAnyCameraFrameData && bEndOfFrame )
      g_pVideoTxBuffers->setLastFrameTimers(uLastCameraReadDuration, uLastVideoFrameSendVideoDuration, s_uLastFrameExpectedTxTimeMicros, uLastVideoFrameSendOtherDuration, uLastVideoFrameReadAndSendAllDuration);

   if ( ! (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS) )
   if ( bHasCamera )
   {
      g_uDbgCamHistSendOtherTime[g_iDbgCamHistBuffIndex] = uLastVideoFrameSendOtherDuration;
      _show_cam_hist_dbg_stats();
   }

   g_pProcessStats->uLoopTimer2 = get_current_timestamp_ms();
   g_pProcessStats->uLoopSubStep = 15;

   if ( bEndOfFrame || (!bReadAnyCameraFrameData) )
      _try_receive_packets(uLastVideoFrameReadAndSendAllDuration);

   g_pProcessStats->uLoopTimer3 = get_current_timestamp_ms();

   if ( g_TimeLastReceivedFastRadioPacketFromController + TIMEOUT_LINK_TO_CONTROLLER_LOST < g_TimeNow )
   if ( g_bHasFastUplinkFromController )
   if ( (0 != radio_links_get_last_start_time()) && (g_TimeNow > radio_links_get_last_start_time() + 3000) )
   {
      log_line("Fast link from controller lost. Last received fast radio packet: %u ms ago. Last received slow radio packet: %u ms ago.",
          g_TimeNow - g_TimeLastReceivedFastRadioPacketFromController,
          g_TimeNow - g_TimeLastReceivedSlowRadioPacketFromController);
      g_bHasFastUplinkFromController = false;
      g_LastTimeLostFastLinkFromController = g_TimeNow;
      adaptive_video_on_uplink_lost();

      //if ( g_pCurrentModel->osd_params.osd_preferences[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_PREFERENCES_BIT_FLAG_SHOW_CONTROLLER_LINK_LOST_ALARM )
         send_alarm_to_controller(ALARM_ID_LINK_TO_CONTROLLER_LOST, 1, 0, 10);
   }

   //-------------------------------------------
   // Process IPCs

   static u32 s_uMainLoopIPCCheckLastTime = 0;
   if ( g_TimeNow >= s_uMainLoopIPCCheckLastTime + 10 )
   {
      s_uMainLoopIPCCheckLastTime = g_TimeNow;

      _read_ipc_pipes(g_TimeNow);
      g_pProcessStats->uLoopSubStep = 28;

      _consume_ipc_messages();
      g_pProcessStats->uLoopSubStep = 29;
   }

   g_pProcessStats->uLoopTimer4 = get_current_timestamp_ms();

   //------------------------------------------
   // Periodic loops

   g_pProcessStats->uLoopSubStep = 41;
   static u32 s_uMainLoopPeriodicCheckLastTime = 0;
   if ( g_TimeNow > s_uMainLoopPeriodicCheckLastTime + 10 )
   {
      s_uMainLoopPeriodicCheckLastTime = g_TimeNow;

      if ( bHasCamera )
      {
         process_data_tx_video_loop();
         if ( bEndOfFrame || (!bReadAnyCameraFrameData) )
            adaptive_video_periodic_loop();
      }
      g_pProcessStats->uLoopSubStep = 45;

      if ( bHasCamera )
      if ( video_sources_periodic_health_checks() )
      {
         log_line("Router is marked for restart due to video sources periodic health checks. Quit it.");
         if ( hw_process_exists("sysupgrade") )
         {
            log_softerror_and_alarm("Sysupgrade is in progress. Don't do anything else. Just quit.");
            g_bQuit = true;
            return;
         }
         if ( packets_queue_has_packets(&g_QueueRadioPacketsOut) )
         {
            //g_iDbgCamHistSendOtherCount[g_iDbgCamHistBuffIndex] += process_and_send_packets(false);
            process_and_send_packets(false);
         }
         log_line("Done sending pending radio packets. Quit now.");
         g_bQuit = true;
         return;
      }


      g_pProcessStats->uLoopSubStep = 46;
      _check_rx_loop_consistency();
      g_pProcessStats->uLoopSubStep = 47;
      
      if ( periodicLoop() )
      {
         reinit_radio_interfaces();
         return;
      }

      _synchronize_shared_mems();
      g_pProcessStats->uLoopSubStep = 49;
      send_pending_alarms_to_controller();
      g_pProcessStats->uLoopSubStep = 50;

      if ( bHasCamera && (NULL != g_pProcessorTxAudio) )
         g_pProcessorTxAudio->tryReadAudioInputStream();
      g_pProcessStats->uLoopSubStep = 51;
   }

   // Send radio packets right away if:
   // * there is no camera (video feed) or it is configuring now
   // otherways, they get sent after a video frame end
   if ( packets_queue_has_packets(&g_QueueRadioPacketsOut) )
   {
      bool bSendNow = false;
      if ( ! bHasCamera )
         bSendNow = true;
      if ( ! video_sources_has_stream_data() )
         bSendNow = true;
      if ( video_sources_last_stream_data() < g_TimeNow - 70 )
         bSendNow = true;
      if ( 0 == video_sources_get_capture_start_time() )
         bSendNow = true;
      //if ( bEndOfFrame || (!bReadAnyCameraFrameData) )
      //   bSendNow = true;
      if ( bSendNow )
      {
         g_iDbgCamHistSendOtherCount[g_iDbgCamHistBuffIndex] += process_and_send_packets(false);
         process_and_send_packets(false);
      }
   }
}