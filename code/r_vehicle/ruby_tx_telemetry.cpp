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
#include <sys/resource.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pcap.h>
#include <pthread.h>
#include <stdint.h>
#include "../radio/radiolink.h"
#include <inttypes.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <semaphore.h>
#include <termios.h>
#include <unistd.h>
#include <math.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/shared_mem.h"
#include "../base/hardware_procs.h"
#include "../base/hardware.h"
#include "../base/hardware_camera.h"
#include "../base/models.h"
#include "../base/models_list.h"
#include "../base/commands.h"
#include "../base/utils.h"
#include "../base/ruby_ipc.h"
#include "../base/vehicle_settings.h"
#include "../common/string_utils.h"
#include "../common/relay_utils.h"
#include "../../mavlink/common/mavlink.h"
#include "../base/parse_fc_telemetry.h"
#include "../radio/radiopackets2.h"
#include "shared_vars.h"
#include "timers.h"
#include "telemetry.h"
#include "telemetry_mavlink.h"
#include "telemetry_ltm.h"
#include "telemetry_msp.h"
#include "../utils/utils_vehicle.h"

sem_t* s_pSemaphoreStop = NULL;

int s_fIPCToRouter = -1;
int s_fIPCFromRouter = -1;

u8 s_BufferMessageFromRouter[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeTmpBuffer[MAX_PACKET_TOTAL_SIZE];
int s_PipeTmpBufferPos = 0;  

int s_iSerialDataLinkFileHandle = -1;
int s_fSerialToFC = -1;

int s_iCurrentDataLinkSerialPortIndex = -1;
u32 s_uCurrentDataLinkSerialPortSpeed = DEFAULT_FC_TELEMETRY_SERIAL_SPEED;

u8 serialBufferIn[300];
u8 serialBufferOut[300];

u8  dataLinkSerialBuffer[RAW_TELEMETRY_MAX_BUFFER];
int dataLinkSerialBufferMaxSize = AUXILIARY_DATA_LINK_MIN_SEND_LENGTH;
int dataLinkSerialBufferCount = 0;
u32 dataLinkSerialBufferLastSendTime = 0;

u32 s_uDataLinkDownlinkSegmentIndex = 0;
u32 s_uDataLinkLastReceivedUploadedSegmentIndex = MAX_U32;
u32 s_uRawTelemetryLastReceivedUploadedSegmentIndex = MAX_U32;

t_packet_header sPH;
t_packet_header_ruby_telemetry_extended_v6 sPHRTE;


u32 s_SendIntervalMiliSec_FCTelemetry = 0;
u32 s_SendIntervalMiliSec_RubyTelemetry = 0;
u32 s_LastSendFCTelemetryTime = 0;
u32 s_LastSendRubyTelemetryTime = 0;

u32 s_uLastTotalPacketsReceived = 0;

t_packet_header_rc_info_downstream* s_pPHDownstreamInfoRC = NULL; // Info to send back to ground

//shared_mem_video_frames_stats* s_pSM_VideoInfoStats = NULL;
//shared_mem_video_frames_stats* s_pSM_VideoInfoStatsRadioOut = NULL;
shared_mem_radio_stats_rx_hist* s_pSM_HistoryRxStats = NULL;

long int home_lat = 0;
long int home_lon = 0;
bool home_set = false;

bool s_bSendRCInfoBack = false;

static u32 s_uTimeLastCheckForRadioReinit = 0;
static bool s_bRadioInterfacesReinitIsInProgress = false;

u32 s_uTimeToAdjustBalanceInterupts = 0;

bool isRadioLinksInitInProgress()
{
   return s_bRadioInterfacesReinitIsInProgress;
}

void check_open_datalink_serial_port();

void close_datalink_serial_port()
{
   if ( -1 != s_iSerialDataLinkFileHandle )
      close(s_iSerialDataLinkFileHandle);
   s_iSerialDataLinkFileHandle = -1;
}


int _open_pipes(bool bOpenReadPipes, bool bOpenWritePipes)
{
   if ( bOpenWritePipes )
   {
      if ( s_fIPCToRouter < 0 )
         s_fIPCToRouter = ruby_open_ipc_channel_write_endpoint(IPC_CHANNEL_TYPE_TELEMETRY_TO_ROUTER);
      if ( s_fIPCToRouter < 0 )
         return -1;
   }

   if ( bOpenReadPipes )
   {
      if ( s_fIPCFromRouter < 0 )
         s_fIPCFromRouter = ruby_open_ipc_channel_read_endpoint(IPC_CHANNEL_TYPE_ROUTER_TO_TELEMETRY);
      if ( s_fIPCFromRouter < 0 )
         return -1;
   }
   return 1;
}

void _compute_telemetry_intervals()
{
   log_line("Vehicle telemetry user set update rate: %d Hz", g_pCurrentModel->telemetry_params.iUpdateRateHz);
   s_SendIntervalMiliSec_FCTelemetry = 1000/g_pCurrentModel->telemetry_params.iUpdateRateHz;
   s_SendIntervalMiliSec_RubyTelemetry = 1000/g_pCurrentModel->telemetry_params.iUpdateRateHz;
   log_line("Using FC telemetry rate of %d ms, Ruby telemetry rate of %d ms", s_SendIntervalMiliSec_FCTelemetry, s_SendIntervalMiliSec_RubyTelemetry);
}


void broadcast_vehicle_stats()
{
   static u32 s_u32LastTimeBroadcastVehicleStats = 0;

   if ( (NULL == g_pCurrentModel) || (g_TimeNow < s_u32LastTimeBroadcastVehicleStats + 3000) )
      return;

   s_u32LastTimeBroadcastVehicleStats = g_TimeNow;

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_BROADCAST_VEHICLE_STATS, STREAM_ID_TELEMETRY);
   PH.vehicle_id_src = PACKET_COMPONENT_TELEMETRY;
   PH.vehicle_id_dest = 0;
   PH.total_length = sizeof(t_packet_header) + sizeof(type_vehicle_stats_info);

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header),(u8*)&(g_pCurrentModel->m_Stats), sizeof(type_vehicle_stats_info));
   
   if ( g_bRouterReady )
   {
      ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, PH.total_length);
      log_line("Broadcast stats: router is ready. Did sent broadcast stats.");
   }
   else
      log_line("Broadcast stats: router is not ready. Skip it.");
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow; 
}

void _add_hardware_telemetry_info( t_packet_header_ruby_telemetry_extended_v6* pPHRTE )
{
   static unsigned long long s_val_cpu[4] = {0,0,0,0};
   static int counter_tx_telemetry_info = 0;
   static u32 s_time_tx_telemetry_cpu = 0;

   counter_tx_telemetry_info++;

   int rate = g_pCurrentModel->telemetry_params.iUpdateRateHz;
   if ( rate < 1 )
      rate = 1;
   if ( rate >= 3 )
   if ( counter_tx_telemetry_info % 2 )
      return;

   if ( rate >= 5 )
   if ( counter_tx_telemetry_info % 4 )
      return;

   FILE* fd = NULL;
   unsigned long long tmp[4];
   unsigned long long total;

   fd = fopen("/proc/stat", "r");
   if ( NULL != fd )
   {
      fscanf(fd, "%*s %llu %llu %llu %llu", &tmp[0], &tmp[1], &tmp[2], &tmp[3] );
      fclose(fd);
      fd = NULL;
   }
   else
   {
      tmp[0] = tmp[1] = tmp[2] = tmp[3] = 0;
   }
   if ( tmp[0] < s_val_cpu[0] || tmp[1] < s_val_cpu[1] || tmp[2] < s_val_cpu[2] || tmp[3] < s_val_cpu[3] )
      pPHRTE->cpu_load = 0;
   else
   {
      total = (tmp[0] - s_val_cpu[0]) + (tmp[1] - s_val_cpu[1]) + (tmp[2] - s_val_cpu[2]);
      pPHRTE->cpu_load = (total * 100) / (total + (tmp[3] - s_val_cpu[3]));
   }
   s_val_cpu[0] = tmp[0]; s_val_cpu[1] = tmp[1]; s_val_cpu[2] = tmp[2]; s_val_cpu[3] = tmp[3]; 

   if ( g_TimeNow < s_time_tx_telemetry_cpu + 5000 )
      return;

   if ( counter_tx_telemetry_info % 10 )
      return;

   s_time_tx_telemetry_cpu = g_TimeNow;

   pPHRTE->temperatureC = hardware_get_cpu_temp();
   pPHRTE->throttled = hardware_get_flags();
   pPHRTE->cpu_mhz = (u16) hardware_get_cpu_speed();
}

void _populate_ruby_telemetry_data(t_packet_header_ruby_telemetry_extended_v6* pPHRTE)
{
   u32 vMaj = SYSTEM_SW_VERSION_MAJOR;
   u32 vMin = SYSTEM_SW_VERSION_MINOR;
   if ( vMaj > 15 )
      vMaj = 15;
   if ( vMin > 15 )
      vMin = 15;

   pPHRTE->rubyVersion = ((vMaj<<4) | vMin);

   _add_hardware_telemetry_info(pPHRTE);
   g_pCurrentModel->populateVehicleTelemetryData_v6(pPHRTE);

   // link stats get populated by router before sending it out
   pPHRTE->downlink_tx_video_bitrate_bps = 0;
   pPHRTE->downlink_tx_video_all_bitrate_bps = 0;
   pPHRTE->downlink_tx_data_bitrate_bps = 0;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      pPHRTE->uplink_rssi_dbm[i] = 0;
      pPHRTE->uplink_link_quality[i] = 0;
   }
   if ( NULL != s_pPHDownstreamInfoRC )
      pPHRTE->uplink_rc_rssi = s_pPHDownstreamInfoRC->rc_rssi;
}

void _store_reboot_info_cache()
{
   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_VEHICLE_REBOOT_CACHE);
   FILE* fd = fopen(szFile, "w");
   if ( NULL == fd )
      return;

   t_packet_header_fc_telemetry* pFCTelem = telemetry_get_fc_telemetry_header();
   fprintf(fd, "%u ", pFCTelem->uFCFlags);
   fprintf(fd, "%u ", pFCTelem->flight_mode);
   fprintf(fd, "%u ", pFCTelem->arm_time);
   fprintf(fd, "%u ", pFCTelem->distance);
   fprintf(fd, "%u ", pFCTelem->total_distance);
   fprintf(fd, "%u ", pFCTelem->gps_fix_type);
   fprintf(fd, "%u \n", pFCTelem->hdop);
   fprintf(fd, "%d ", pFCTelem->latitude);
   fprintf(fd, "%d ", pFCTelem->longitude);
   fprintf(fd, "%d ", home_set);
   fprintf(fd, "%li ", home_lat);
   fprintf(fd, "%li \n", home_lon);
   fclose(fd);

   log_line("Stored cached info before reboot: distance: %u, total_distance: %u, flags: %d, flight_mode: %d, arm_time: %u, home is set: %d, lat,lon: %li , %li",
      pFCTelem->distance, pFCTelem->total_distance, pFCTelem->uFCFlags, pFCTelem->flight_mode, pFCTelem->arm_time, home_set, home_lat, home_lon);
}

void _process_cached_reboot_info()
{
   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_VEHICLE_REBOOT_CACHE);
   if ( access(szFile, R_OK) == -1 )
   {
      log_line("No stored cached telemetry info.");
      return;
   }

   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to open cached telemetry info file for reading.");
      return;
   }
   int tmp1 = 0;
   t_packet_header_fc_telemetry* pFCTelem = telemetry_get_fc_telemetry_header();
   fscanf(fd, "%d", &tmp1); pFCTelem->uFCFlags = (u8)tmp1;
   fscanf(fd, "%d", &tmp1); pFCTelem->flight_mode = (u8)tmp1;
   fscanf(fd, "%u", &pFCTelem->arm_time);
   fscanf(fd, "%u", &pFCTelem->distance);
   fscanf(fd, "%u", &pFCTelem->total_distance);
   fscanf(fd, "%d", &tmp1); pFCTelem->gps_fix_type = (u8)tmp1;
   fscanf(fd, "%d", &tmp1); pFCTelem->hdop = (u8)tmp1;
   fscanf(fd, "%d", &pFCTelem->latitude);
   fscanf(fd, "%d", &pFCTelem->longitude);
   fscanf(fd, "%d", &tmp1); home_set = (bool)tmp1;
   fscanf(fd, "%li", &home_lat);
   fscanf(fd, "%li", &home_lon);

   fclose(fd);
   unlink(szFile);
   char szComm[256];
   sprintf(szComm, "rm -rf %s%s 2>&1", FOLDER_CONFIG, FILE_CONFIG_VEHICLE_REBOOT_CACHE);
   hw_execute_bash_command_silent(szComm, NULL);

   log_line("Restored cached info after reboot: distance: %u, total_distance: %u, flags: %d, flight_mode: %d, arm_time: %u, home is set: %d, lat,lon: %li , %li",
      pFCTelem->distance, pFCTelem->total_distance, pFCTelem->uFCFlags, pFCTelem->flight_mode, pFCTelem->arm_time, home_set, home_lat, home_lon);

   g_pCurrentModel->m_Stats.uCurrentFlightTime = pFCTelem->arm_time;
   if ( g_pCurrentModel->m_Stats.uCurrentOnTime < g_pCurrentModel->m_Stats.uCurrentFlightTime )
      g_pCurrentModel->m_Stats.uCurrentOnTime = g_pCurrentModel->m_Stats.uCurrentFlightTime;
   broadcast_vehicle_stats();
}

void _send_rc_data_to_FC()
{
   static u16 s_ch_last_values[18];
   static u8 s_is_failsafe = 0;

   if ( ! (g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_REQUEST_DATA_STREAMS) )
   {
      log_line("Telemetry is set as read only with no requests for data streams. Do not send data to FC.");
      return;
   }
 
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MAVLINK )
      return;
   if ( telemetry_get_serial_port_file() < 0 )
      return;
   if ( NULL == s_pPHDownstreamInfoRC )
      return;
   if ( g_TimeNow < g_TimeStart + 1000 )
      return;
   if ( s_pPHDownstreamInfoRC->recv_packets < 2 )
      return;

   bool bSend = false;

   if ( s_is_failsafe != s_pPHDownstreamInfoRC->is_failsafe )
      bSend = true;
   if ( g_TimeNow >= g_TimeLastRCSentToFC + 1000/g_pCurrentModel->rc_params.rc_frames_per_second )
      bSend = true;

   if ( ! bSend )
      return;

   int componentId = MAV_COMP_ID_MISSIONPLANNER;
   //int componentId = MAV_COMP_ID_AUTOPILOT1;

   g_TimeLastRCSentToFC = g_TimeNow;
   s_is_failsafe = s_pPHDownstreamInfoRC->is_failsafe;
  
   int count = g_pCurrentModel->rc_params.channelsCount;
   if ( count > 18 )
      count = 18;
   for( int i=0; i<count; i++ )
      s_ch_last_values[i] = s_pPHDownstreamInfoRC->rc_channels[i];

   if ( s_pPHDownstreamInfoRC->is_failsafe )
   {
     for( int i=0; i<count; i++ )
         s_ch_last_values[i] = get_rc_channel_failsafe_value(g_pCurrentModel, i, s_pPHDownstreamInfoRC->rc_channels[i]);
   }

   /*
      char szBuff[256];
      sprintf(szBuff, "fs:%d ", s_pPHDownstreamInfoRC->is_failsafe );
      for( int i=0; i<(int)g_pCurrentModel->rc_params.channelsCount; i++ )
      {
         char szTmp[32];
         sprintf(szTmp, "%d: %d  ", i+1, s_ch_last_values[i]);
         strcat(szBuff, szTmp);
      }
      log_line(szBuff);
   */

   if ( s_is_failsafe && ((g_pCurrentModel->rc_params.failsafeFlags & 0xFF) == RC_FAILSAFE_NOOUTPUT ) )
      return;

   //log_line("send RC, from %d, component %d, to %d, component %d", g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, 1, MAV_COMP_ID_ALL);

   mavlink_message_t msg;
   int len;
   if ( g_TimeNow > g_TimeLastMAVLinkHeartbeatSent + 800 )
   {
      g_TimeLastMAVLinkHeartbeatSent = g_TimeNow;
      //log_line("Sending MAVLink heartbeat...");

      mavlink_msg_heartbeat_pack(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msg, MAV_TYPE_GCS, MAV_AUTOPILOT_INVALID, 0,0,0);
      len = mavlink_msg_to_send_buffer(serialBufferOut, &msg);
      if ( len != write(telemetry_get_serial_port_file(), serialBufferOut, len) )
         log_softerror_and_alarm("Failed to write to serial port to FC");
   }

   mavlink_msg_rc_channels_override_pack(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msg, g_pCurrentModel->telemetry_params.vehicle_mavlink_id, MAV_COMP_ID_ALL, s_ch_last_values[0], s_ch_last_values[1], s_ch_last_values[2], s_ch_last_values[3],
         s_ch_last_values[4], s_ch_last_values[5], s_ch_last_values[6], s_ch_last_values[7], s_ch_last_values[8], s_ch_last_values[9], s_ch_last_values[10], s_ch_last_values[11], s_ch_last_values[12],
         s_ch_last_values[13], s_ch_last_values[14], s_ch_last_values[15], s_ch_last_values[16], s_ch_last_values[17]);

   len = mavlink_msg_to_send_buffer(serialBufferOut, &msg);
   if ( len != write(telemetry_get_serial_port_file(), serialBufferOut, len) )
      log_softerror_and_alarm("Failed to write to serial port to FC");
}


void save_model()
{
   log_line("Saving model...");

   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
   if ( ! g_pCurrentModel->loadFromFile(szFile, false) )
      g_pCurrentModel->resetToDefaults(true);

   saveCurrentModel();

   if ( g_bQuit )
      return;

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_TELEMETRY | (MODEL_CHANGED_STATS<<8);
   PH.vehicle_id_dest = 0;
   PH.total_length = sizeof(t_packet_header);

   if ( g_bRouterReady )
      ruby_ipc_channel_send_message(s_fIPCToRouter, (u8*)&PH, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms(); 
}

void reload_model(u8 changeType)
{
   if ( g_bQuit )
      return;

   log_line("Reloading model");

   int last_datalink_serial_port_index = s_iCurrentDataLinkSerialPortIndex;
   u32 last_datalink_serial_port_speed = s_uCurrentDataLinkSerialPortSpeed;

   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
   if ( ! g_pCurrentModel->loadFromFile(szFile, false) )
      g_pCurrentModel->resetToDefaults(true);

   if ( changeType != MODEL_CHANGED_GENERIC )
   if ( changeType != MODEL_CHANGED_CONTROLLER_TELEMETRY )
   if ( changeType != MODEL_CHANGED_SWAPED_RADIO_INTERFACES )
   if ( changeType != MODEL_CHANGED_SERIAL_PORTS )
   if ( changeType != MODEL_CHANGED_DEVELOPER_FLAGS )
   {
      log_line("Model change does not affect TX telemetry. Done updating local model.");
      return;
   }

   hardware_serial_reload_ports_settings();

   _compute_telemetry_intervals();

   s_bSendRCInfoBack = false;
   if ( g_pCurrentModel->osd_params.show_stats_rc ||
       (g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_FLAG_SHOW_HID_IN_OSD) ||
       (g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_FLAG2_SHOW_STATS_RC)
      )
      s_bSendRCInfoBack = true;
   log_line("Sending RC info back to controller: %s", s_bSendRCInfoBack?"yes":"no");


   s_iCurrentDataLinkSerialPortIndex = -1;
   
   for( int i=0; i<hardware_serial_get_ports_count(); i++ )
   {
       hw_serial_port_info_t* pPortInfo = hardware_get_serial_port_info(i);
       if ( NULL == pPortInfo )
          continue;
       if ( ! pPortInfo->iSupported )
          continue;
       if ( pPortInfo->iPortUsage == SERIAL_PORT_USAGE_DATA_LINK )
       {
          s_iCurrentDataLinkSerialPortIndex = i;
          s_uCurrentDataLinkSerialPortSpeed = pPortInfo->lPortSpeed;
       }
   }

   log_line("New assigned serial port index for data link: %d, at baudrate: %u", s_iCurrentDataLinkSerialPortIndex, s_uCurrentDataLinkSerialPortSpeed);

   telemetry_close_serial_port();

   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_NONE )
   if ( telemetry_open_serial_port() > 0 )
      s_uTimeToAdjustBalanceInterupts = g_TimeNow + 4000;
 
   bool bLocalVSpeed = false;
   int li = g_pCurrentModel->osd_params.iCurrentOSDScreen;
   if ( li >= 0 && li < MODEL_MAX_OSD_SCREENS )
   if ( g_pCurrentModel->osd_params.osd_flags2[li] & OSD_FLAG2_SHOW_LOCAL_VERTICAL_SPEED )
      bLocalVSpeed = true;

   parse_telemetry_set_show_local_vspeed(bLocalVSpeed);

   parse_telemetry_force_always_armed((g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_FORCE_ARMED)? true: false);

   if ( last_datalink_serial_port_index != s_iCurrentDataLinkSerialPortIndex ||
        last_datalink_serial_port_speed != s_uCurrentDataLinkSerialPortSpeed )
   {
      check_open_datalink_serial_port();
   }

   log_line("Telemetry will send full telemetry back to controller? %s", telemetry_will_send_full_telemetry_to_controller()?"Yes":"No");
}

void onRebootRequest(bool bSaveModelToo)
{
   log_line("Executing request to reboot.");

   _store_reboot_info_cache();
   if ( bSaveModelToo )
      save_model();
   else
      log_line("Do not save model before rebooting.");
   hardware_sleep_ms(200);
   log_line("Will reboot now.");
   hardware_reboot();
}


bool try_read_messages_from_router()
{
   if ( NULL == ruby_ipc_try_read_message(s_fIPCFromRouter, s_PipeTmpBuffer, &s_PipeTmpBufferPos, s_BufferMessageFromRouter) )
      return false;

   if ( g_bQuit )
      return false;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;

   t_packet_header* pPH = (t_packet_header*)&s_BufferMessageFromRouter[0];

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_LONG_TASK )
   {
      g_bLongTaskStarted = (pPH->vehicle_id_src?true:false);
      log_line("Received local message that router %s a long task.", g_bLongTaskStarted?"started":"finished");
      return true;
   }
   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SET_CAMERA_PARAMS )
   {
      log_line("Received message to update camera params.");
      // Do not save model. Saved by rx_comands. Just update to the new values.
      u8 uCameraChangedIndex = 0;
      type_camera_parameters newCamParams;

      memcpy(&uCameraChangedIndex, &s_BufferMessageFromRouter[0] + sizeof(t_packet_header), sizeof(u8));
      memcpy((u8*)&newCamParams, &s_BufferMessageFromRouter[0] + sizeof(t_packet_header) + sizeof(u8), sizeof(type_camera_parameters));
      memcpy(&(g_pCurrentModel->camera_params[uCameraChangedIndex]), (u8*)&newCamParams, sizeof(type_camera_parameters));
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_VEHICLE_ROUTER_READY )
   {
      log_line("Received notification that router is ready.");
      _open_pipes(false, true);
      log_line("Opened pipes. Mark router as read.");   
      g_bRouterReady = true;
      return true;
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_OSD_PLUGINS_NEED_TELEMETRY )
   {
      log_line("Received notification about OSD plugins telemetry flags.");
      g_bOSDPluginsNeedTelemetryStreams = pPH->vehicle_id_src?true:false;
      log_line("Received local message that OSD plugins %s", g_bOSDPluginsNeedTelemetryStreams?"need full telemetry stream":"don't need full telemetry stream");
      reload_model(MODEL_CHANGED_CONTROLLER_TELEMETRY);
      return true;
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RUBY )
   if ( pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_REQUEST )
   {
      u32 uResendCount = 0;
      if ( pPH->total_length >= sizeof(t_packet_header) + sizeof(u32) )
         memcpy(&uResendCount, &(s_BufferMessageFromRouter[sizeof(t_packet_header)]), sizeof(u32));

      if ( pPH->total_length >= sizeof(t_packet_header) + 2*sizeof(u32) )
         memcpy(&g_pCurrentModel->uDeveloperFlags, &(s_BufferMessageFromRouter[sizeof(t_packet_header) + sizeof(u32)]), sizeof(u32));

      if ( pPH->total_length >= sizeof(t_packet_header) + 3*sizeof(u32) )
         memcpy(&g_pCurrentModel->uControllerBoardType, &(s_BufferMessageFromRouter[sizeof(t_packet_header) + 2*sizeof(u32)]), sizeof(u32));

      log_line("Pairing request: Currently stored controller ID: %u / %u", g_uControllerId, g_pCurrentModel->uControllerId);
      log_line("Received pairing request from router (received resend count: %u). From CID %u to VID %u (%s). Developer mode: %s. Updating local model.",
         uResendCount, pPH->vehicle_id_src, pPH->vehicle_id_dest, (pPH->vehicle_id_dest == g_pCurrentModel->uVehicleId)?"self":"not self", (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE)?"on":"off");

      if ( NULL != g_pCurrentModel )
         g_pCurrentModel->onControllerIdUpdated(pPH->vehicle_id_src);
      g_uControllerId = pPH->vehicle_id_src;
      return true;
   }
 
   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
   {
      if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED )
      {
         u8 changeType = (pPH->vehicle_id_src >> 8 ) & 0xFF;      
         log_line("Received request from router to reload model (change type: %d (%s)).", (int)changeType, str_get_model_change_type((int)changeType));
         reload_model(changeType);
         return true;
      }

      if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_LINK_FREQUENCY_CHANGED )
      {
         u32* pI = (u32*)((&s_BufferMessageFromRouter[0])+sizeof(t_packet_header));
         u32 uLinkId = *pI;
         pI++;
         u32 uNewFreq = *pI;
         log_line("Received new model radio link frequency from router (radio link %u new freq: %s). Updating local model copy.", uLinkId+1, str_format_frequency(uNewFreq));
         if ( uLinkId < (u32)g_pCurrentModel->radioLinksParams.links_count )
         { 
            g_pCurrentModel->radioLinksParams.link_frequency_khz[uLinkId] = uNewFreq;
            for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
            {
               if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == (int)uLinkId )
                  g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[i] = uNewFreq;
            }
         }
         else
            log_softerror_and_alarm("Invalid parameters for changing radio link frequency. radio link: %u of %d", uLinkId+1, g_pCurrentModel->radioLinksParams.links_count);

         return true;
      }

      if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_REBOOT )
         onRebootRequest((pPH->vehicle_id_dest != 0)?true:false);
      return true;
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) != PACKET_COMPONENT_TELEMETRY )
      return true;

   if ( ! radio_packet_check_crc(s_BufferMessageFromRouter, pPH->total_length) )
      return true;

   if ( pPH->vehicle_id_dest != g_pCurrentModel->uVehicleId )
   {
      log_softerror_and_alarm("Received telemetry targeted to another vehicle (current vehicle UID: %u, received vehicle UID:%u).", g_pCurrentModel->uVehicleId, pPH->vehicle_id_dest);
      return true;
   }

   if (pPH->packet_type == PACKET_TYPE_TELEMETRY_RAW_UPLOAD )
   {
      int len = pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_telemetry_raw);
      u8* pTelemetryData = (&s_BufferMessageFromRouter[0]) + sizeof(t_packet_header)+sizeof(t_packet_header_telemetry_raw);
      t_packet_header_telemetry_raw* pPHTR = (t_packet_header_telemetry_raw*)(&s_BufferMessageFromRouter[sizeof(t_packet_header)]); 
      
      #ifdef LOG_RAW_TELEMETRY
      log_line("[Raw_Telem] Received raw telemetry packet from controller, index %u, %d / %d bytes", pPHTR->telem_segment_index, len, pPH->total_length);
      #endif
      if ( s_uRawTelemetryLastReceivedUploadedSegmentIndex != MAX_U32 &&
           s_uRawTelemetryLastReceivedUploadedSegmentIndex > 10 && 
           pPHTR->telem_segment_index < s_uRawTelemetryLastReceivedUploadedSegmentIndex-10 )
         s_uRawTelemetryLastReceivedUploadedSegmentIndex = MAX_U32;

      if ( s_uRawTelemetryLastReceivedUploadedSegmentIndex != MAX_U32 )
      if ( pPHTR->telem_segment_index <= s_uRawTelemetryLastReceivedUploadedSegmentIndex )
         return true;

      if ( s_uRawTelemetryLastReceivedUploadedSegmentIndex != MAX_U32 )
      if ( s_uRawTelemetryLastReceivedUploadedSegmentIndex + 1 != pPHTR->telem_segment_index )
      {
      //log_line("-------------------------------");
      //log_line("Missing telemetry upload packet");
      //log_line("-------------------------------");
      }

      s_uRawTelemetryLastReceivedUploadedSegmentIndex = pPHTR->telem_segment_index;

      if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MAVLINK )
      {
         if ( telemetry_get_serial_port_file() < 0 )
            log_softerror_and_alarm("[Raw_Telem] No serial port opened to FC to send data to.");
         else if ( len != write(telemetry_get_serial_port_file(), pTelemetryData, len) )
            log_softerror_and_alarm("Failed to write to serial port for telemetry output to FC.");
      }
      return true;
   }

   if (pPH->packet_type == PACKET_TYPE_AUX_DATA_LINK_UPLOAD )
   {
      int len = pPH->total_length - sizeof(t_packet_header) - sizeof(u32);
      u8* pDataLinkData = (&s_BufferMessageFromRouter[0]) + sizeof(t_packet_header);
      u32 segment = 0;
      memcpy((u8*)&segment, pDataLinkData, sizeof(u32));
      pDataLinkData += sizeof(u32);
      if ( s_uDataLinkLastReceivedUploadedSegmentIndex != MAX_U32 &&
           s_uDataLinkLastReceivedUploadedSegmentIndex > 10 && 
           segment < s_uDataLinkLastReceivedUploadedSegmentIndex-10 )
         s_uDataLinkLastReceivedUploadedSegmentIndex = MAX_U32;

      if ( s_uDataLinkLastReceivedUploadedSegmentIndex != MAX_U32 )
      if ( segment <= s_uDataLinkLastReceivedUploadedSegmentIndex )
         return true;

      //log_line("Received uploaded raw telemetry: segment index (last/now): %u/%u, len: %d", s_uDataLinkLastReceivedUploadedSegmentIndex, segment,  len);

      if ( s_uDataLinkLastReceivedUploadedSegmentIndex != MAX_U32 )
      if ( s_uDataLinkLastReceivedUploadedSegmentIndex + 1 != segment )
      {
      //log_line("-------------------------------");
      //log_line("Missing telemetry upload packet");
      //log_line("-------------------------------");
      }

      s_uDataLinkLastReceivedUploadedSegmentIndex = segment;
      if ( -1 != s_iSerialDataLinkFileHandle )
      if ( len != write(s_iSerialDataLinkFileHandle, pDataLinkData, len) )
         log_softerror_and_alarm("Failed to write to serial port for auxiliary data link output.");

      return true;
   }
   return true;
}

void send_datalink_data_packet_to_controller()
{
   if ( ! g_bRouterReady )
      return;
   if ( dataLinkSerialBufferCount > RAW_TELEMETRY_MAX_BUFFER )
   {
      log_softerror_and_alarm("Trying to send more data link data than expected to downlink: %d bytes, max expected: %d bytes. Sending only max allowed.", dataLinkSerialBufferCount, RAW_TELEMETRY_MAX_BUFFER);
      dataLinkSerialBufferCount = RAW_TELEMETRY_MAX_BUFFER;
   }

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_TELEMETRY, PACKET_TYPE_AUX_DATA_LINK_DOWNLOAD, STREAM_ID_DATA2);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = 0;
   PH.total_length = sizeof(t_packet_header)+sizeof(u32) + dataLinkSerialBufferCount;
      
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), (u8*)&s_uDataLinkDownlinkSegmentIndex, sizeof(u32));
   memcpy(buffer+sizeof(t_packet_header)+sizeof(u32), dataLinkSerialBuffer, dataLinkSerialBufferCount);
   
   if ( g_bRouterReady && (! g_bLongTaskStarted) && (! s_bRadioInterfacesReinitIsInProgress) )
   {
      int result = ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, PH.total_length);
      if ( result != PH.total_length )
         log_softerror_and_alarm("Failed to send data to router. Sent result: %d", result );
   }

   dataLinkSerialBufferLastSendTime = g_TimeNow;
   dataLinkSerialBufferCount = 0;
   s_uDataLinkDownlinkSegmentIndex++;

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
}


void try_read_serial_datalink()
{
   if ( -1 == s_iSerialDataLinkFileHandle )
      return;

   struct timeval to;
   to.tv_sec = 0;
   to.tv_usec = 2000; // 2 ms

   fd_set readset;   
   FD_ZERO(&readset);
   FD_SET(s_iSerialDataLinkFileHandle, &readset);
   
   int res = select(s_iSerialDataLinkFileHandle+1, &readset, NULL, NULL, &to);
   if ( res <= 0 )
      return;
   if ( ! FD_ISSET(s_iSerialDataLinkFileHandle, &readset) )
      return;

   int length = read(s_iSerialDataLinkFileHandle, serialBufferIn, 270);
   if ( length <= 0 )
      return;

   char szBuff[2000];
   memcpy(szBuff, &serialBufferIn[0], length);
   szBuff[length] = 0;

   u8* pData = &serialBufferIn[0];

   while ( length > 0 )
   {
      if ( dataLinkSerialBufferCount + length < dataLinkSerialBufferMaxSize )
      {
         memcpy(&(dataLinkSerialBuffer[dataLinkSerialBufferCount]), pData, length);
         dataLinkSerialBufferCount += length;
         return;
      }
      int chunkSize = dataLinkSerialBufferMaxSize-dataLinkSerialBufferCount;
      memcpy(&(dataLinkSerialBuffer[dataLinkSerialBufferCount]), pData, chunkSize);
      dataLinkSerialBufferCount += chunkSize;
      pData += chunkSize;
      length -= chunkSize;
      send_datalink_data_packet_to_controller();
   }
}

void check_send_telemetry_to_controller()
{
   if ( ! g_bRouterReady )
      return;

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   bool bDidSentRubyTelemetry = false;

   int iCountMessagesSent = 0;

   //---------------------------------------
   // Send Ruby Telemetry Extended packet

   if ( (g_TimeNow < s_LastSendRubyTelemetryTime) || (g_TimeNow >= s_LastSendRubyTelemetryTime + s_SendIntervalMiliSec_RubyTelemetry+5) )
   {
      //log_line("Send Ruby telemetry to controller");
      s_LastSendRubyTelemetryTime = g_TimeNow;
      bDidSentRubyTelemetry = true;
      _populate_ruby_telemetry_data(&sPHRTE);

      //-------------------------------------------
      // Send Ruby Telemetry Extended

      if ( hardware_hasCamera() )
         sPHRTE.uRubyFlags |= FLAG_RUBY_TELEMETRY_VEHICLE_HAS_CAMERA;
      else
         sPHRTE.uRubyFlags &= (~FLAG_RUBY_TELEMETRY_VEHICLE_HAS_CAMERA);
      sPHRTE.uRubyFlags &= ~(FLAG_RUBY_TELEMETRY_ENCRYPTED_DATA | FLAG_RUBY_TELEMETRY_ENCRYPTED_VIDEO);
      if ( (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_DATA) || (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_ALL) )
         sPHRTE.uRubyFlags |= FLAG_RUBY_TELEMETRY_ENCRYPTED_DATA;
      if ( (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_VIDEO) || (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_ALL) )
         sPHRTE.uRubyFlags |= FLAG_RUBY_TELEMETRY_ENCRYPTED_VIDEO;

      if ( g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_SPECTATOR_ENABLE )
         sPHRTE.uRubyFlags |= FLAG_RUBY_TELEMETRY_ALLOW_SPECTATOR_TELEMETRY;
      else
         sPHRTE.uRubyFlags &= ~FLAG_RUBY_TELEMETRY_ALLOW_SPECTATOR_TELEMETRY;

      sPHRTE.uRubyFlags &= ~(FLAG_RUBY_TELEMETRY_RC_FAILSAFE | FLAG_RUBY_TELEMETRY_RC_ALIVE);

      if ( (g_TimeNow > TIMEOUT_FC_TELEMETRY_LOST) && (telemetry_time_last_telemetry_received() > g_TimeNow - TIMEOUT_FC_TELEMETRY_LOST) )
         sPHRTE.uRubyFlags |= FLAG_RUBY_TELEMETRY_HAS_VEHICLE_TELEMETRY_DATA;
      else
         sPHRTE.uRubyFlags &= ~(FLAG_RUBY_TELEMETRY_HAS_VEHICLE_TELEMETRY_DATA);

      #ifdef FEATURE_ENABLE_RC
      if ( g_pCurrentModel->rc_params.rc_enabled && NULL != s_pPHDownstreamInfoRC )
      {
         if ( s_pPHDownstreamInfoRC->is_failsafe )
            sPHRTE.uRubyFlags |= FLAG_RUBY_TELEMETRY_RC_FAILSAFE;
         else
            sPHRTE.uRubyFlags |= FLAG_RUBY_TELEMETRY_RC_ALIVE;
      }
      #endif

      sPHRTE.uExtraRubyFlags = 0;
      
      t_packet_header_ruby_telemetry_extended_extra_info PHTExtraInfo;

      memset((u8*)&PHTExtraInfo, 0, sizeof(t_packet_header_ruby_telemetry_extended_extra_info));
      PHTExtraInfo.uExtraFlags = FLAG_RUBY_TELEMETRY_EXTRA_INFO_IS_VALID;
      PHTExtraInfo.uTimeNow = g_TimeNow;
      PHTExtraInfo.uRelayedVehicleId = g_pCurrentModel->relay_params.uRelayedVehicleId;
      PHTExtraInfo.uThrottleInput = get_mavlink_rc_channels()[2];
      PHTExtraInfo.uThrottleOutput = telemetry_get_fc_telemetry_header()->throttle;

      sPHRTE.uRubyFlags |= FLAG_RUBY_TELEMETRY_HAS_EXTENDED_INFO;

      // Add relaying flags

      sPHRTE.uRubyFlags &= ~FLAG_RUBY_TELEMETRY_HAS_RELAY_LINK;

      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId < MAX_RADIO_INTERFACES )
      if ( g_pCurrentModel->relay_params.uRelayFrequencyKhz != 0 )
      if ( g_TimeNow > 500 )
      if ( g_SM_RadioStats.radio_interfaces[g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId].timeLastRxPacket+500 > g_TimeNow )
         sPHRTE.uRubyFlags |= FLAG_RUBY_TELEMETRY_HAS_RELAY_LINK;
      
      sPHRTE.uRubyFlags &= ~FLAG_RUBY_TELEMETRY_IS_RELAYING;

      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
      if ( g_pCurrentModel->relay_params.uRelayedVehicleId != 0 )
      if ( (g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_REMOTE) ||
           (g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_PIP_MAIN) ||
           (g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_PIP_REMOTE) )
         sPHRTE.uRubyFlags |= FLAG_RUBY_TELEMETRY_IS_RELAYING;

      // Gets populated by router

      t_packet_header_ruby_telemetry_extended_extra_info_retransmissions ph_extra_info;
      memset((u8*)&ph_extra_info,0, sizeof(ph_extra_info));
      
      sPHRTE.extraSize = 0;

      radio_packet_init(&sPH, PACKET_COMPONENT_TELEMETRY, PACKET_TYPE_RUBY_TELEMETRY_EXTENDED, STREAM_ID_TELEMETRY);
      sPH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      sPH.vehicle_id_dest = 0;
      sPH.total_length = (u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v6) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions) + sPHRTE.extraSize;
      
      int dx = 0;
      memcpy(buffer, &sPH, sizeof(t_packet_header));
      dx += sizeof(t_packet_header);
      memcpy(buffer+dx, &sPHRTE, sizeof(t_packet_header_ruby_telemetry_extended_v6));
      dx += sizeof(t_packet_header_ruby_telemetry_extended_v6);
      memcpy(buffer+dx , &PHTExtraInfo, sizeof(t_packet_header_ruby_telemetry_extended_extra_info));
      dx += sizeof(t_packet_header_ruby_telemetry_extended_extra_info);
      memcpy(buffer+dx , &ph_extra_info, sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions));
      dx += sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions);

      if ( g_bRouterReady && (! s_bRadioInterfacesReinitIsInProgress) )
      {
         int result = ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, sPH.total_length);
         if ( result != sPH.total_length )
            log_softerror_and_alarm("Failed to send data to router. Sent result: %d", result );
         iCountMessagesSent++;
      }

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

      // Send debug info
      static u32 s_uTimeDebugSentLastDebugInfoPacket = 0;

      if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE )
      if ( g_TimeNow > s_uTimeDebugSentLastDebugInfoPacket + 1000 )
      {
         s_uTimeDebugSentLastDebugInfoPacket = g_TimeNow;
         radio_packet_init(&sPH, PACKET_COMPONENT_TELEMETRY, PACKET_TYPE_DEBUG_INFO, STREAM_ID_TELEMETRY);
         sPH.vehicle_id_src = g_pCurrentModel->uVehicleId;
         sPH.vehicle_id_dest = 0;
         sPH.total_length = (u16)sizeof(t_packet_header)+(u16)sizeof(type_u32_couters);
      
         type_u32_couters dummyCounters; // gets populated by router
         reset_counters(&dummyCounters);
         memcpy(buffer, &sPH, sizeof(t_packet_header));
         memcpy(buffer+sizeof(t_packet_header), &dummyCounters, sizeof(type_u32_couters));
         
         if ( g_bRouterReady && (!g_bLongTaskStarted) && (! s_bRadioInterfacesReinitIsInProgress) )
         {
            int result = ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, sPH.total_length);
            if ( result != sPH.total_length )
               log_softerror_and_alarm("Failed to send debug packet data to router. Sent result: %d", result );
         }
      }
   }


   //----------------------------------------
   // Send short Ruby telemetry
   
   if ( bDidSentRubyTelemetry )
   if ( hardware_radio_has_low_capacity_links() )
   {
      t_packet_header_ruby_telemetry_short PHRTShort;

      PHRTShort.uRubyFlags = sPHRTE.uRubyFlags;
      PHRTShort.uFCFlags = 0;
      PHRTShort.rubyVersion = sPHRTE.rubyVersion;
      PHRTShort.radio_links_count = sPHRTE.radio_links_count;
      if ( PHRTShort.radio_links_count > 3 )
         PHRTShort.radio_links_count = 3;
      for( int i=0; i<PHRTShort.radio_links_count; i++ )
         PHRTShort.uRadioFrequenciesKhz[i] = sPHRTE.uRadioFrequenciesKhz[i];

      t_packet_header_fc_telemetry* pFCTelem = telemetry_get_fc_telemetry_header();
      if ( NULL != pFCTelem )
      {
         PHRTShort.uFCFlags = pFCTelem->uFCFlags;
         PHRTShort.flight_mode = pFCTelem->flight_mode;
         PHRTShort.throttle = pFCTelem->throttle;
         PHRTShort.voltage = pFCTelem->voltage; // 1/1000 volts
         PHRTShort.current = pFCTelem->current; // 1/1000 amps
         PHRTShort.altitude = pFCTelem->altitude; // 1/100 meters  -1000 m
         PHRTShort.altitude_abs = pFCTelem->altitude_abs; // 1/100 meters -1000 m
         PHRTShort.distance = pFCTelem->distance; // 1/100 meters
         PHRTShort.heading = pFCTelem->heading;
         
         PHRTShort.vspeed = pFCTelem->vspeed; // 1/100 meters -1000 m
         PHRTShort.aspeed = pFCTelem->aspeed; // airspeed (1/100 meters - 1000 m)
         PHRTShort.hspeed = pFCTelem->hspeed; // 1/100 meters -1000 m
      }
      radio_packet_init(&sPH, PACKET_COMPONENT_TELEMETRY, PACKET_TYPE_RUBY_TELEMETRY_SHORT, STREAM_ID_TELEMETRY);
      sPH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      sPH.vehicle_id_dest = 0;
      sPH.packet_flags_extended |= PACKET_FLAGS_EXTENDED_BIT_SEND_ON_LOW_CAPACITY_LINK_ONLY;
      sPH.total_length = (u16)sizeof(t_packet_header) + (u16)sizeof(t_packet_header_ruby_telemetry_short);
      
      memcpy(buffer, &sPH, sizeof(t_packet_header));
      memcpy(buffer+sizeof(t_packet_header), &PHRTShort, sizeof(t_packet_header_ruby_telemetry_short));
      
      if ( g_bRouterReady && (! isRadioLinksInitInProgress()) )
      {
         int result = ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, sPH.total_length);
         if ( result != sPH.total_length )
            log_softerror_and_alarm("Failed to send data to router. Sent result: %d", result );
      }
   } 

   // ----------------------------------
   // Send FC Telemetry packet

   if ( (g_TimeNow < s_LastSendFCTelemetryTime) || (g_TimeNow >= s_LastSendFCTelemetryTime + s_SendIntervalMiliSec_FCTelemetry) )
   {
      //log_line("Send FC telemetry to controller");
      s_LastSendFCTelemetryTime = g_TimeNow;

      //-------------------------------
      // Send FC telemetry
      if ( (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MAVLINK) ||
           (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_LTM) )
         telemetry_mavlink_send_to_controller();
      if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP )
         telemetry_msp_send_to_controller();
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }


   // Send Radio Rx History if enabled

   static u32 s_uTimeLastSentRadioRxHistory = 0;
   static u32 s_uLastRadioRxHistorySentInterface = 0;

   if ( g_pCurrentModel->osd_params.osd_flags3[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_FLAG3_SHOW_RADIO_RX_HISTORY_VEHICLE)
   if ( (g_TimeNow < s_uTimeLastSentRadioRxHistory) || (g_TimeNow >= s_uTimeLastSentRadioRxHistory + 433/g_pCurrentModel->radioInterfacesParams.interfaces_count) )
   {
      s_uTimeLastSentRadioRxHistory = g_TimeNow;
      s_uLastRadioRxHistorySentInterface++;
      if ( (int)s_uLastRadioRxHistorySentInterface >= g_pCurrentModel->radioInterfacesParams.interfaces_count )
         s_uLastRadioRxHistorySentInterface = 0;

      if ( NULL == s_pSM_HistoryRxStats )
      {
         s_pSM_HistoryRxStats = shared_mem_radio_stats_rx_hist_open_for_read();
         if ( NULL == s_pSM_HistoryRxStats )
            log_softerror_and_alarm("Failed to open shared mem radio rx history for reading.");
         else
            log_line("Opened shared mem raadio rx history for reading.");
      }

      if ( NULL != s_pSM_HistoryRxStats )
      {
         radio_packet_init(&sPH, PACKET_COMPONENT_TELEMETRY, PACKET_TYPE_RUBY_TELEMETRY_RADIO_RX_HISTORY, STREAM_ID_TELEMETRY);
         sPH.vehicle_id_src = g_pCurrentModel->uVehicleId;
         sPH.vehicle_id_dest = 0;
         sPH.total_length = (u16)sizeof(t_packet_header) + sizeof(u32) + (u16)sizeof(shared_mem_radio_stats_interface_rx_hist);

         memcpy(buffer, &sPH, sizeof(t_packet_header));
         memcpy(buffer+sizeof(t_packet_header), (u8*)&s_uLastRadioRxHistorySentInterface, sizeof(u32));
         memcpy(buffer+sizeof(t_packet_header) + sizeof(u32), (u8*)&(s_pSM_HistoryRxStats->interfaces_history[s_uLastRadioRxHistorySentInterface]), sizeof(shared_mem_radio_stats_interface_rx_hist));
         
         if ( g_bRouterReady && (! s_bRadioInterfacesReinitIsInProgress) )
         {
            int result = ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, sPH.total_length);
            if ( result != sPH.total_length )
               log_softerror_and_alarm("Failed to send data to router. Sent result: %d", result );
            iCountMessagesSent++;
         }

         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      }
   }   

   if ( ! bDidSentRubyTelemetry )
      return;


   // --------------------------------------
   // Send video frames stats

   /*
   if ( NULL == s_pSM_VideoInfoStats )
   {
      s_pSM_VideoInfoStats = shared_mem_video_frames_stats_open_for_read();
      if ( NULL == s_pSM_VideoInfoStats )
         log_softerror_and_alarm("Failed to open shared mem video info stats for reading.");
      else
         log_line("Opened shared mem video info stats for reading.");
   }
   
   if ( NULL == s_pSM_VideoInfoStatsRadioOut )
   {
      s_pSM_VideoInfoStatsRadioOut = shared_mem_video_frames_stats_radio_out_open_for_read();
      if ( NULL == s_pSM_VideoInfoStatsRadioOut )
         log_softerror_and_alarm("Failed to open shared mem video info stats radio out for reading.");
      else
         log_line("Opened shared mem video info stats radio out for reading.");
   }
   
   if ( (NULL != g_pCurrentModel) && (NULL != s_pSM_VideoInfoStats) && (NULL != s_pSM_VideoInfoStatsRadioOut) )
   if ( g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_FLAG_SHOW_STATS_VIDEO_H264_FRAMES_INFO)
   {
      radio_packet_init(&sPH, PACKET_COMPONENT_TELEMETRY, PACKET_TYPE_RUBY_TELEMETRY_VIDEO_INFO_STATS, STREAM_ID_TELEMETRY);
      sPH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      sPH.vehicle_id_dest = 0;
      sPH.total_length = (u16)sizeof(t_packet_header) + 2*(u16)sizeof(shared_mem_video_frames_stats);

      memcpy(buffer, &sPH, sizeof(t_packet_header));
      memcpy(buffer+sizeof(t_packet_header), (u8*)s_pSM_VideoInfoStats, sizeof(shared_mem_video_frames_stats));
      memcpy(buffer+sizeof(t_packet_header) + sizeof(shared_mem_video_frames_stats), (u8*)s_pSM_VideoInfoStatsRadioOut, sizeof(shared_mem_video_frames_stats));
      
      if ( g_bRouterReady && (! g_bLongTaskStarted) && (! s_bRadioInterfacesReinitIsInProgress) )
      {
         int result = ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, sPH.total_length);
         if ( result != sPH.total_length )
            log_softerror_and_alarm("Failed to send data to router. Sent result: %d", result );
         iCountMessagesSent++;
      }

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }
   */

   // FC RC channels and RC extra packets are sent at the same rate as Ruby telemetry

   // ---------------------------------
   // Send FC RC Channels

   if ( g_pCurrentModel->osd_params.show_stats_rc ||
       (g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_FLAG_SHOW_HID_IN_OSD) ||
       (g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_FLAG2_SHOW_STATS_RC)
      )
   if ( g_bRouterReady && (! g_bLongTaskStarted) && (! s_bRadioInterfacesReinitIsInProgress) )
   {
      radio_packet_init(&sPH, PACKET_COMPONENT_TELEMETRY, PACKET_TYPE_FC_RC_CHANNELS, STREAM_ID_TELEMETRY);
      sPH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      sPH.vehicle_id_dest = 0;
      sPH.total_length = (u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_fc_rc_channels);

      t_packet_header_fc_rc_channels PHFCRCChannels;
      int* pRCCh = get_mavlink_rc_channels();
      PHFCRCChannels.channels[0] = (u16) pRCCh[0];
      PHFCRCChannels.channels[1] = (u16) pRCCh[1];
      PHFCRCChannels.channels[2] = (u16) pRCCh[2];
      PHFCRCChannels.channels[3] = (u16) pRCCh[3];
      PHFCRCChannels.channels[4] = (u16) pRCCh[4];
      PHFCRCChannels.channels[5] = (u16) pRCCh[5];
      PHFCRCChannels.channels[6] = (u16) pRCCh[6];
      PHFCRCChannels.channels[7] = (u16) pRCCh[7];
      PHFCRCChannels.channels[8] = (u16) pRCCh[8];
      PHFCRCChannels.channels[9] = (u16) pRCCh[9];
      PHFCRCChannels.channels[10] = (u16) pRCCh[10];
      PHFCRCChannels.channels[11] = (u16) pRCCh[11];
      PHFCRCChannels.channels[12] = (u16) pRCCh[12];
      PHFCRCChannels.channels[13] = (u16) pRCCh[13];

      memcpy(buffer, &sPH, sizeof(t_packet_header));
      memcpy(buffer+sizeof(t_packet_header), &PHFCRCChannels, sizeof(t_packet_header_fc_rc_channels));
      
      int result = ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, sPH.total_length);
      if ( result != sPH.total_length )
         log_softerror_and_alarm("Failed to send data to router. Sent result: %d", result );
      iCountMessagesSent++;

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }

   // ----------------------------------
   // Send RC downlink info packet

   bool bAddRCInfo = false;

   #ifdef FEATURE_ENABLE_RC
   if ( g_pCurrentModel->rc_params.rc_enabled && NULL != s_pPHDownstreamInfoRC )
   if ( s_bSendRCInfoBack )
      bAddRCInfo = true;
   #endif

   if ( bAddRCInfo )
   {
      radio_packet_init(&sPH, PACKET_COMPONENT_TELEMETRY, PACKET_TYPE_RC_TELEMETRY, STREAM_ID_TELEMETRY);
      sPH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      sPH.vehicle_id_dest = 0;
      sPH.total_length = (u16)sizeof(t_packet_header) + (u16)sizeof(t_packet_header_rc_info_downstream);

      memcpy(buffer, &sPH, sizeof(t_packet_header));
      memcpy(buffer+sizeof(t_packet_header), (u8*)s_pPHDownstreamInfoRC, sizeof(t_packet_header_rc_info_downstream));
      
      if ( g_bRouterReady && (! g_bLongTaskStarted) && (! s_bRadioInterfacesReinitIsInProgress) )
      {
         int result = ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, sPH.total_length);
         if ( result != sPH.total_length )
            log_softerror_and_alarm("Failed to send data to router. Sent result: %d", result );
         iCountMessagesSent++;
      }

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }
}

void _periodic_loop()
{
   if ( g_TimeNow > s_uTimeLastCheckForRadioReinit + 2000 )
   {
      s_uTimeLastCheckForRadioReinit = g_TimeNow; 
      char szFile[128];
      strcpy(szFile, FOLDER_RUBY_TEMP);
      strcat(szFile, FILE_TEMP_REINIT_RADIO_IN_PROGRESS);  
      if ( access(szFile, R_OK) != -1 )
         s_bRadioInterfacesReinitIsInProgress = true;
      else
         s_bRadioInterfacesReinitIsInProgress = false;
   }
}

void check_open_datalink_serial_port()
{
   close_datalink_serial_port();

   s_iCurrentDataLinkSerialPortIndex = -1;
   hw_serial_port_info_t* pPortInfo = NULL;
       
   for( int i=0; i<hardware_serial_get_ports_count(); i++ )
   {
       pPortInfo = hardware_get_serial_port_info(i);
       if ( NULL == pPortInfo )
          continue;
       if ( ! pPortInfo->iSupported )
          continue;
       if ( pPortInfo->iPortUsage == SERIAL_PORT_USAGE_DATA_LINK )
       {
          s_iCurrentDataLinkSerialPortIndex = i;
          s_uCurrentDataLinkSerialPortSpeed = pPortInfo->lPortSpeed;
          break;
       }
   }

   if ( s_iCurrentDataLinkSerialPortIndex < 0 )
      return;

   hardware_configure_serial(pPortInfo->szPortDeviceName, pPortInfo->lPortSpeed);
   
   log_line("Opening serial port %s (%s) for auxiliary data link (baud rate: %u) ...", pPortInfo->szName, pPortInfo->szPortDeviceName, (int)pPortInfo->lPortSpeed);
   s_iSerialDataLinkFileHandle = hardware_open_serial_port(pPortInfo->szPortDeviceName, pPortInfo->lPortSpeed);
   if ( -1 == s_iSerialDataLinkFileHandle )
      log_softerror_and_alarm("Failed to open serial port %s (%s) for auxiliary datalink.", pPortInfo->szName, pPortInfo->szPortDeviceName );
   else
      log_line("Opened serial port %s (%s) for auxiliary datalink successfully.", pPortInfo->szName, pPortInfo->szPortDeviceName);
}


void open_shared_mem_objects()
{
   g_pProcessStats = shared_mem_process_stats_open_write(SHARED_MEM_WATCHDOG_TELEMETRY_TX);
   if ( NULL == g_pProcessStats)
      log_softerror_and_alarm("Failed to open shared mem for telemetry tx process watchdog stats for writing: %s", SHARED_MEM_WATCHDOG_TELEMETRY_TX);
   else
      log_line("Opened shared mem for telemetry tx process watchdog stats for writing.");

   /*
   s_pSM_VideoInfoStats = shared_mem_video_frames_stats_open_for_read();
   if ( NULL == s_pSM_VideoInfoStats )
      log_softerror_and_alarm("Failed to open shared mem video info stats for reading.");
   else
      log_line("Opened shared mem video info stats for reading.");

   s_pSM_VideoInfoStatsRadioOut = shared_mem_video_frames_stats_radio_out_open_for_read();
   if ( NULL == s_pSM_VideoInfoStatsRadioOut )
      log_softerror_and_alarm("Failed to open shared mem video info stats radio out for reading.");
   else
      log_line("Opened shared mem video info stats radio out for reading.");
   */

   s_pSM_HistoryRxStats = shared_mem_radio_stats_rx_hist_open_for_read();
   if ( NULL == s_pSM_HistoryRxStats )
      log_softerror_and_alarm("Failed to open shared mem radio rx history for reading.");
   else
      log_line("Opened shared mem raadio rx history for reading.");
}

void _init_telemetry_structures()
{
   sPH.stream_packet_idx = STREAM_ID_DATA << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   sPH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   sPH.vehicle_id_dest = 0;
   _populate_ruby_telemetry_data(&sPHRTE);

   sPHRTE.rubyVersion = ((SYSTEM_SW_VERSION_MAJOR<<4) | SYSTEM_SW_VERSION_MINOR);
   if ( g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_SPECTATOR_ENABLE )
      sPHRTE.uRubyFlags |= FLAG_RUBY_TELEMETRY_ALLOW_SPECTATOR_TELEMETRY;
   else
      sPHRTE.uRubyFlags &= ~FLAG_RUBY_TELEMETRY_ALLOW_SPECTATOR_TELEMETRY;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      sPHRTE.last_sent_datarate_bps[i][0] = 0;
      sPHRTE.last_sent_datarate_bps[i][1] = 0;
      sPHRTE.last_recv_datarate_bps[i]= 0;
      sPHRTE.uplink_rssi_dbm[i] = 0;
      sPHRTE.uplink_link_quality[i] = 0;
   }

   sPHRTE.uplink_rc_rssi = 255;
   sPHRTE.uplink_mavlink_rc_rssi = 255;
   sPHRTE.uplink_mavlink_rx_rssi = 255;

   telemetry_init();

   for( int i=0; i<MAX_MAVLINK_RC_CHANNELS; i++ )
      get_mavlink_rc_channels()[i] = 0;

   _process_cached_reboot_info();
}

void _main_loop();

void handle_sigint(int sig) 
{ 
   log_line_forced_to_file("--------------------------");
   log_line_forced_to_file("Caught signal to stop: %d", sig);
   log_line_forced_to_file("--------------------------");
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


   log_init("TXTelemetry");
   log_arguments(argc, argv);

   utils_log_radio_packets_sizes();
   radio_packets_log_sizes();

   hardware_detectBoardAndSystemType();
  
   if ( strcmp(argv[argc-1], "-debug") == 0 )
      log_enable_stdout();

   g_uControllerId = vehicle_utils_getControllerId();
   load_VehicleSettings();
   loadAllModels();
   g_pCurrentModel = getCurrentModel();
 
   if ( g_pCurrentModel->uModelFlags & MODEL_FLAG_DISABLE_ALL_LOGS )
   {
      log_line("Log is disabled on vehicle. Disabled logs.");
      log_disable();
   }

   _open_pipes(true, false);
   
   open_shared_mem_objects();

   if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS )
   if ( 0 != strcmp(argv[argc-1], "-log") )
      log_only_errors();

   hardware_serial_reload_ports_settings();

   if ( g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS )
      hw_set_priority_current_proc(g_pCurrentModel->processesPriorities.iThreadPriorityTelemetry);
   if ( g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_AFFINITY_CORES )
      hw_set_current_thread_affinity("tx_telemetry", g_pCurrentModel->processesPriorities.iCoreTelemetry, g_pCurrentModel->processesPriorities.iCoreTelemetry);

   bool bLocalVSpeed = false;
   int li = g_pCurrentModel->osd_params.iCurrentOSDScreen;
   if ( li >= 0 && li < MODEL_MAX_OSD_SCREENS )
   if ( g_pCurrentModel->osd_params.osd_flags2[li] & OSD_FLAG2_SHOW_LOCAL_VERTICAL_SPEED )
      bLocalVSpeed = true;

   parse_telemetry_init(g_pCurrentModel->telemetry_params.vehicle_mavlink_id, bLocalVSpeed);

   _compute_telemetry_intervals();

   s_bSendRCInfoBack = false;
   if ( g_pCurrentModel->osd_params.show_stats_rc ||
       (g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_FLAG_SHOW_HID_IN_OSD) ||
       (g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_FLAG2_SHOW_STATS_RC)
      )
      s_bSendRCInfoBack = true;

   log_line("Sending RC info back to controller: %s", s_bSendRCInfoBack?"yes":"no");

   g_TimeNow = get_current_timestamp_ms();
   process_stats_reset(g_pProcessStats, g_TimeNow);

   s_iCurrentDataLinkSerialPortIndex = -1;
   
   for( int i=0; i<g_pCurrentModel->hardwareInterfacesInfo.serial_port_count; i++ )
   {
       if ( g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & MODEL_SERIAL_PORT_BIT_SUPPORTED )
       if ( (g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & 0xFF) == SERIAL_PORT_USAGE_DATA_LINK )
       {
          s_iCurrentDataLinkSerialPortIndex = i;
          s_uCurrentDataLinkSerialPortSpeed = g_pCurrentModel->hardwareInterfacesInfo.serial_port_speed[i];
       }
   }

   log_line("Currently assigned serial port index for data link: %d, at baudrate: %u", s_iCurrentDataLinkSerialPortIndex, s_uCurrentDataLinkSerialPortSpeed);

   _init_telemetry_structures();

   log_line("OSD plugins %s", g_bOSDPluginsNeedTelemetryStreams?"need full telemetry stream":"don't need full telemetry stream"); 
   log_line("Telemetry will send full telemetry back to controller? %s", telemetry_will_send_full_telemetry_to_controller()?"Yes":"No");

   g_TimeStart = get_current_timestamp_ms();

   broadcast_vehicle_stats();
   
   check_open_datalink_serial_port();

   log_line("Started TX Telemetry. Running now.");
   log_line("----------------------------------");

   g_TimeNow = get_current_timestamp_ms();
   process_stats_reset(g_pProcessStats, g_TimeNow);

   parse_telemetry_force_always_armed((g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_FORCE_ARMED)? true: false);

   s_pSemaphoreStop = sem_open(SEMAPHORE_STOP_VEHICLE_TELEM_TX, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( (NULL == s_pSemaphoreStop) || (SEM_FAILED == s_pSemaphoreStop) )
   {
      log_error_and_alarm("Failed to open semaphore: %s", SEMAPHORE_STOP_VEHICLE_TELEM_TX);
      s_pSemaphoreStop = NULL;
   }

   broadcast_vehicle_stats();

   _main_loop();


   log_line("Stopping...");

   if ( NULL != s_pSemaphoreStop )
      sem_close(s_pSemaphoreStop);
   sem_unlink(SEMAPHORE_STOP_VEHICLE_TELEM_TX);
   
   //shared_mem_video_frames_stats_close(s_pSM_VideoInfoStats);
   //shared_mem_video_frames_stats_radio_out_close(s_pSM_VideoInfoStatsRadioOut);
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_TELEMETRY_TX, g_pProcessStats);
   shared_mem_radio_stats_rx_hist_close(s_pSM_HistoryRxStats);

   #ifdef FEATURE_ENABLE_RC
   shared_mem_rc_downstream_info_close(s_pPHDownstreamInfoRC);
   #endif
   
   ruby_close_ipc_channel(s_fIPCToRouter);
   ruby_close_ipc_channel(s_fIPCFromRouter);
   s_fIPCToRouter = -1;
   s_fIPCFromRouter = -1;

   close_datalink_serial_port();
   telemetry_close_serial_port();

   if ( -1 != s_fSerialToFC )
      close(s_fSerialToFC);
   s_fSerialToFC = -1;

   log_line("Stopped.Exit");
   log_line("------------------------");
   return 0;
}

void _main_loop()
{
   int iMaxSleepIntervalMs = 25;

   int iSleepTime = 10;

   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_NONE )
      iSleepTime = iMaxSleepIntervalMs;

   while ( !g_bQuit )
   {
      hardware_sleep_ms(iSleepTime);
      g_uLoopCounter++;
      g_TimeNow = get_current_timestamp_ms();
      u32 tTime0 = g_TimeNow;

      if ( NULL != g_pProcessStats )
      {
         g_pProcessStats->uLoopCounter++;
         g_pProcessStats->lastActiveTime = g_TimeNow;
      }

      if ( is_semaphore_signaled_clear(s_pSemaphoreStop, SEMAPHORE_STOP_VEHICLE_TELEM_TX) )
      {
         log_line("Semaphore to stop is set. Quit now.");
         g_bQuit = true;
         break;
      }

      _periodic_loop();
      
      if ( (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_NONE) || (! g_bRouterReady) )
         iSleepTime = iMaxSleepIntervalMs;
      else
      {
         if ( telemetry_get_serial_port_file() <= 0 && (g_TimeNow > telemetry_last_time_opened() + 4000) )
         {
             log_line("FC Telemetry is enabled. Trying to open serial ports...");
             if ( telemetry_open_serial_port() > 0 )
                s_uTimeToAdjustBalanceInterupts = g_TimeNow + 4000;
         }
         if ( telemetry_get_serial_port_file() > 0 )
         if ( g_TimeNow > telemetry_last_time_opened() + 4000 )
         if ( g_TimeNow > telemetry_time_last_telemetry_received() + 4000 )
         {
            log_line("Flight controller telemetry is enabled and no telemetry received from flight controller in the last few seconds. Reinitialize serial telemetry...");
            telemetry_close_serial_port();
            if ( telemetry_open_serial_port() > 0 )
               s_uTimeToAdjustBalanceInterupts = g_TimeNow + 4000;
         }
         iSleepTime = 15;
         if ( telemetry_try_read_serial_port() > 0 )
            iSleepTime = 5;
         telemetry_periodic_loop();

         if ( (0 != s_uTimeToAdjustBalanceInterupts) && (g_TimeNow > s_uTimeToAdjustBalanceInterupts) )
         if ( telemetry_time_last_telemetry_received() != 0 )
         {
            s_uTimeToAdjustBalanceInterupts = 0;
            if ( g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_BALANCE_INTERRUPTS_CORES )
               hardware_balance_interupts();
         }
      }
      
      try_read_serial_datalink();

      if ( g_pCurrentModel->rc_params.rc_enabled )
      if ( NULL == s_pPHDownstreamInfoRC )
      {
         #ifdef FEATURE_ENABLE_RC
         s_pPHDownstreamInfoRC = shared_mem_rc_downstream_info_open_read();
         if ( NULL == s_pPHDownstreamInfoRC )
            log_softerror_and_alarm("Failed to open RC Download info shared memory for read.");
         else
            log_line("Opened RC Download info shared memory for read: success.");
         #endif
      }

      int maxMsgToRead = 10;
      while ( (maxMsgToRead > 0) && try_read_messages_from_router() )
         maxMsgToRead--;

      if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MAVLINK )
      if ( g_pCurrentModel->rc_params.rc_enabled )
      if ( g_pCurrentModel->rc_params.flags & RC_FLAGS_OUTPUT_ENABLED )
      {
         if ( iSleepTime > 10 )
            iSleepTime = 10;
         _send_rc_data_to_FC();
      }

      if ( dataLinkSerialBufferCount >= AUXILIARY_DATA_LINK_MIN_SEND_LENGTH || 
          (dataLinkSerialBufferCount > 0 && g_TimeNow >= dataLinkSerialBufferLastSendTime + AUXILIARY_DATA_LINK_SEND_TIMEOUT ) )
         send_datalink_data_packet_to_controller();

      g_pCurrentModel->updateStatsMaxCurrentVoltage(telemetry_get_fc_telemetry_header());

      // If we just sent Ruby and FC telemetry to controller, just continue
      /*
      if ( g_TimeNow < (s_LastSendRubyTelemetryTime + s_SendIntervalMiliSec_RubyTelemetry) )
      if ( g_TimeNow < (s_LastSendFCTelemetryTime + s_SendIntervalMiliSec_FCTelemetry) )
      {
         iSleepTime += 10;
         u32 tNow = get_current_timestamp_ms();
         if ( NULL != g_pProcessStats )
         {
            if ( g_pProcessStats->uMaxLoopTimeMs < tNow - tTime0 )
               g_pProcessStats->uMaxLoopTimeMs = tNow - tTime0;
            g_pProcessStats->uTotalLoopTime += tNow - tTime0;
            if ( 0 != g_pProcessStats->uLoopCounter )
               g_pProcessStats->uAverageLoopTimeMs = g_pProcessStats->uTotalLoopTime / g_pProcessStats->uLoopCounter;
         }

         u32 dTime = get_current_timestamp_ms() - tTime0;
         if ( dTime > 150 )
            log_softerror_and_alarm("Main processing loop took too long (%u ms).", dTime);
         continue;
      }
      */

      check_send_telemetry_to_controller();

      u32 tNow = get_current_timestamp_ms();
      if ( NULL != g_pProcessStats )
      {
         if ( g_pProcessStats->uMaxLoopTimeMs < tNow - tTime0 )
            g_pProcessStats->uMaxLoopTimeMs = tNow - tTime0;
         g_pProcessStats->uTotalLoopTime += tNow - tTime0;
         if ( 0 != g_pProcessStats->uLoopCounter )
            g_pProcessStats->uAverageLoopTimeMs = g_pProcessStats->uTotalLoopTime / g_pProcessStats->uLoopCounter;
      }
      u32 dTime = get_current_timestamp_ms() - tTime0;
      if ( dTime > 150 )
         log_softerror_and_alarm("Main processing loop took too long (%u ms).", dTime);
   }
}