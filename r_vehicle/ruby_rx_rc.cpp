/*
You can use this C/C++ code however you wish (for example, but not limited to:
     as is, or by modifying it, or by adding new code, or by removing parts of the code;
     in public or private projects, in new free or commercial products) 
     only if you get a priori written consent from Petru Soroaga (petrusoroaga@yahoo.com) for your specific use
     and only if this copyright terms are preserved in the code.
     This code is public for learning and academic purposes.
Also, check the licences folder for additional licences terms.
Code written by: Petru Soroaga, 2021-2023
*/

#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../base/shared_mem.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/ruby_ipc.h"
#include "../common/string_utils.h"

#include "timers.h"

#include <time.h>
#include <sys/resource.h>

bool g_bQuit = false;
bool g_bDebug = false;
Model sModelVehicle; 

int s_fIPC_FromRouter = -1;

u8 s_BufferRCFromRouter[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeTmpBufferRCFromRouter[MAX_PACKET_TOTAL_SIZE];
int s_PipeTmpBufferRCFromRouterPos = 0;    

t_packet_header_rc_full_frame_upstream s_LastReceivedRCFrame;

t_packet_header_rc_info_downstream* s_pPHDownstreamInfoRC = NULL; // Info to send back to telemetry process and then (optionally) to ground
shared_mem_process_stats* s_pProcessStats = NULL;

int s_LastHistorySlice = 0;
u8 s_LastReceivedRCFrameIndex = 0;
u8 s_QualityRecvCount[2];
u8 s_QualityRecvIndex = 0;

void process_data_rc_full_frame(u8* pBuffer, int length)
{
   if ( NULL == s_pPHDownstreamInfoRC )
      return;

   t_packet_header_rc_full_frame_upstream* pPHRCF = (t_packet_header_rc_full_frame_upstream*)(pBuffer + sizeof(t_packet_header));
   memcpy(&s_LastReceivedRCFrame, pPHRCF, sizeof(t_packet_header_rc_full_frame_upstream));

   g_TimeLastFrameReceived = g_TimeNow;
   s_pPHDownstreamInfoRC->recv_packets++;
   s_QualityRecvCount[s_QualityRecvIndex]++;

   for( int i=0; i<(int)sModelVehicle.rc_params.channelsCount; i++ )
   {
      if ( i % 2 )
         s_pPHDownstreamInfoRC->rc_channels[i] = (u16)(pPHRCF->ch_lowBits[i]) + (((u16)((pPHRCF->ch_highBits[i/2] & 0xF0)>>4))<<8);
      else
         s_pPHDownstreamInfoRC->rc_channels[i] = (u16)(pPHRCF->ch_lowBits[i]) + (((u16)(pPHRCF->ch_highBits[i/2] & 0x0F))<<8);

      //if ( i == 2 )
      //   log_line("ch: %d", s_pPHDownstreamInfoRC->rc_channels[2] );
   }

   u8 gap = pPHRCF->rc_frame_index - s_LastReceivedRCFrameIndex - 1;
   if ( pPHRCF->rc_frame_index == s_LastReceivedRCFrameIndex )
      gap = 0xFF;
   if ( pPHRCF->rc_frame_index < s_LastReceivedRCFrameIndex )
      gap = 255 - s_LastReceivedRCFrameIndex + pPHRCF->rc_frame_index;

   s_LastReceivedRCFrameIndex = pPHRCF->rc_frame_index;
   s_pPHDownstreamInfoRC->lost_packets += gap;

   //log_line("frame: %d, gap: %d, lost: %d", pPHRCF->rc_frame_index, gap, s_pPHDownstreamInfoRC->lost_packets);

   u8 cReceived = s_pPHDownstreamInfoRC->history[s_LastHistorySlice] & 0x0F;
   u8 cGap = ((s_pPHDownstreamInfoRC->history[s_LastHistorySlice]) >> 4) & 0x0F;
   if ( cReceived < 0x0F )
      cReceived++;
   if ( gap > 0x0F )
      gap = 0x0F;
   if ( gap > cGap )
      cGap = gap;
   s_pPHDownstreamInfoRC->history[s_LastHistorySlice] = (cReceived & 0x0F) | ((cGap & 0x0F) << 4);
}

void on_failsafe_triggered()
{
   log_line("Triggered a RC failsafe due to Rx timeout: %d ms", sModelVehicle.rc_params.rc_failsafe_timeout_ms);
   s_pPHDownstreamInfoRC->is_failsafe = 1;
   s_pPHDownstreamInfoRC->failsafe_count++;
}

void on_failsafe_cleared()
{
   log_line("Cleared RC failsafe.");
   s_pPHDownstreamInfoRC->is_failsafe = 0;
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
      printf("%d.%d (b%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10, SYSTEM_SW_BUILD_NUMBER);
      return 0;
   }
   
   log_init("RX_RC");

   if ( strcmp(argv[argc-1], "-debug") == 0 )
      g_bDebug = true;
   if ( g_bDebug )
      log_enable_stdout();

   
   s_fIPC_FromRouter = ruby_open_ipc_channel_read_endpoint(IPC_CHANNEL_TYPE_ROUTER_TO_RC);
   if ( s_fIPC_FromRouter < 0 )
      return -1;

   if ( ! sModelVehicle.loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
   {
      log_error_and_alarm("Can't load current model vehicle. Exiting.");
      return -1;
   } 
   
   if ( sModelVehicle.uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS )
      log_only_errors();

   hw_set_priority_current_proc(sModelVehicle.niceRC);   

   s_pPHDownstreamInfoRC = shared_mem_rc_downstream_info_open_write();
   if ( NULL == s_pPHDownstreamInfoRC )
      log_softerror_and_alarm("Failed to open RC Download info shared memory for write.");
   else
      log_line("Opened RC Download info shared memory for write: success.");
   if ( NULL != s_pPHDownstreamInfoRC )
      memset((u8*)s_pPHDownstreamInfoRC, 0, sizeof(t_packet_header_rc_info_downstream));

   s_pProcessStats = shared_mem_process_stats_open_write(SHARED_MEM_WATCHDOG_RC_RX);
   if ( NULL == s_pProcessStats )
      log_softerror_and_alarm("Failed to open shared mem for RC Rx process watchdog for writing: %s", SHARED_MEM_WATCHDOG_RC_RX);
   else
      log_line("Opened shared mem for RC Rx process watchdog for writing.");

   log_line("Started. Running now.");
   log_line("-----------------------------");

   s_QualityRecvCount[0] = 0;
   s_QualityRecvCount[1] = 0;
   s_QualityRecvIndex = 0;

   if ( NULL != s_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      s_pProcessStats->lastActiveTime = g_TimeNow;
      s_pProcessStats->lastRadioTxTime = g_TimeNow;
      s_pProcessStats->lastRadioRxTime = g_TimeNow;
      s_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      s_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }

   if ( NULL != s_pPHDownstreamInfoRC )
   {
      s_pPHDownstreamInfoRC->rc_rssi = 0;
      s_pPHDownstreamInfoRC->is_failsafe = 1;
   }

   s_LastReceivedRCFrame.rc_frame_index = 0;
   s_LastReceivedRCFrame.flags = 0;

   g_TimeStart = get_current_timestamp_ms();

   while (!g_bQuit) 
   {
      hardware_sleep_ms(5);
      g_TimeNow = get_current_timestamp_ms();
      u32 tTime0 = g_TimeNow;

      if ( NULL != s_pProcessStats )
      {
         s_pProcessStats->uLoopCounter++;
         s_pProcessStats->lastActiveTime = g_TimeNow;
      }

      if ( NULL == s_pPHDownstreamInfoRC )
      {
         s_pPHDownstreamInfoRC = shared_mem_rc_downstream_info_open_write();
         if ( NULL == s_pPHDownstreamInfoRC )
            log_softerror_and_alarm("Failed to open RC Download info shared memory for write.");
         else
            log_line("Opened RC Download info shared memory for write: success.");
         if ( NULL != s_pPHDownstreamInfoRC )
            memset((u8*)s_pPHDownstreamInfoRC, 0, sizeof(t_packet_header_rc_info_downstream)); 
      }

      if ( NULL != s_pPHDownstreamInfoRC )
      if ( g_TimeNow >= g_TimeLastQualityMeasurement + 500 )
      {
         int quality = 0;
         if ( NULL != s_pPHDownstreamInfoRC )
         if ( 0 != sModelVehicle.rc_params.rc_frames_per_second )
            quality = (100.0 * (s_QualityRecvCount[0] + s_QualityRecvCount[1]))/(float)sModelVehicle.rc_params.rc_frames_per_second;
         if ( quality < 0 ) quality = 0;
         if ( quality > 100 ) quality = 100;

         if ( quality >= s_pPHDownstreamInfoRC->rc_rssi )
            s_pPHDownstreamInfoRC->rc_rssi = quality;
         else
            s_pPHDownstreamInfoRC->rc_rssi = 0.9*quality + 0.1*s_pPHDownstreamInfoRC->rc_rssi;
         if ( s_pPHDownstreamInfoRC->rc_rssi > 100 )
            s_pPHDownstreamInfoRC->rc_rssi = 100;
         //log_line("RC Q: %d, %d %d", s_pPHDownstreamInfoRC->rc_rssi, g_TimeLastQualityMeasurement, g_TimeNow);
         g_TimeLastQualityMeasurement = g_TimeNow;
         s_QualityRecvIndex = 1 - s_QualityRecvIndex;
         s_QualityRecvCount[s_QualityRecvIndex] = 0;
      }

      if ( NULL != s_pPHDownstreamInfoRC )
      {
         int historySlice = (g_TimeNow/50) % RC_INFO_HISTORY_SIZE;
         while ( s_LastHistorySlice != historySlice )
         {
            s_LastHistorySlice++;
            if ( s_LastHistorySlice >= RC_INFO_HISTORY_SIZE )
               s_LastHistorySlice = 0;
            s_pPHDownstreamInfoRC->history[s_LastHistorySlice] = 0;
         }
         s_pPHDownstreamInfoRC->last_history_slice = s_LastHistorySlice;
      }

      if ( NULL != s_pProcessStats )
         s_pProcessStats->lastActiveTime = g_TimeNow;

      int maxMsgToRead = 5;
      while ( (maxMsgToRead > 0) && NULL != ruby_ipc_try_read_message(s_fIPC_FromRouter, 20000, s_PipeTmpBufferRCFromRouter, &s_PipeTmpBufferRCFromRouterPos, s_BufferRCFromRouter) )
      {
         maxMsgToRead--;
         t_packet_header* pPH = (t_packet_header*)&s_BufferRCFromRouter[0];
         if ( ! packet_check_crc(s_BufferRCFromRouter, pPH->total_length) )
            continue;
 
         if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RUBY )
         if ( pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_REQUEST )
         {
            log_line("Received pairing request from router. CID: %u, VID: %u. Updating local model.", pPH->vehicle_id_src, pPH->vehicle_id_dest);
            sModelVehicle.controller_id = pPH->vehicle_id_src;
         }

         if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
         if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED )
         {
            u8 changeType = (pPH->vehicle_id_src >> 8 ) & 0xFF;
            if ( changeType == MODEL_CHANGED_GENERIC )
            {
               log_line("Received request from router to reload model.");
               sModelVehicle.loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true);
               log_line("RC Failsafe timeout: %d ms", sModelVehicle.rc_params.rc_failsafe_timeout_ms);
            }
            else
               log_line("Model change does not affect RX RC. Don't update local model.");
         }

         if ( pPH->vehicle_id_dest != sModelVehicle.vehicle_id )
            continue;

         if ( NULL != s_pProcessStats )
            s_pProcessStats->lastIPCIncomingTime = g_TimeNow;

         #ifdef FEATURE_ENABLE_RC
         if ( pPH->packet_type == PACKET_TYPE_RC_FULL_FRAME )
            process_data_rc_full_frame(s_BufferRCFromRouter, pPH->total_length);
         #endif
      }

      #ifdef FEATURE_ENABLE_RC
      bool bIsFailSafeNow = false;

      if ( sModelVehicle.rc_params.rc_enabled )
      if ( !(s_LastReceivedRCFrame.flags & RC_FULL_FRAME_FLAGS_HAS_INPUT) )
      {
         //log_line("RC No input");
         bIsFailSafeNow = true;
      }
      if ( NULL != s_pPHDownstreamInfoRC )
      if ( sModelVehicle.rc_params.rc_enabled && 0 != g_TimeLastFrameReceived &&
           g_TimeLastFrameReceived <= g_TimeNow - sModelVehicle.rc_params.rc_failsafe_timeout_ms )
      {
         //log_line("RC timeout failsafe %d ms", sModelVehicle.rc_params.rc_failsafe_timeout_ms);
         bIsFailSafeNow = true;
      }
      if ( bIsFailSafeNow )
      {
         if ( 0 == s_pPHDownstreamInfoRC->is_failsafe )
            on_failsafe_triggered();
         s_pPHDownstreamInfoRC->is_failsafe = 1;
      }

      if ( ! bIsFailSafeNow )
      if ( NULL != s_pPHDownstreamInfoRC )
      if ( sModelVehicle.rc_params.rc_enabled && 0 != g_TimeLastFrameReceived &&
           g_TimeLastFrameReceived > g_TimeNow - sModelVehicle.rc_params.rc_failsafe_timeout_ms )
      {
         if ( 1 == s_pPHDownstreamInfoRC->is_failsafe )
            on_failsafe_cleared();
         s_pPHDownstreamInfoRC->is_failsafe = 0;
      }
      #endif

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
   log_line("Stopping...");
   
   shared_mem_rc_downstream_info_close(s_pPHDownstreamInfoRC);
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_RC_RX, s_pProcessStats);

   ruby_close_ipc_channel(s_fIPC_FromRouter);
   s_fIPC_FromRouter = -1;
 
   log_line("Stopped.Exit");
   log_line("-----------------------");
   return 0;
} 