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
#include "../base/shared_mem.h"
#include "../base/hardware_camera.h"
#include "../base/hardware_cam_maj.h"
#include "../base/hardware_procs.h"
#include "../base/ruby_ipc.h"
#include "../base/parser_h264.h"
#include "../base/utils.h"
#include "../common/string_utils.h"
#include "../radio/radio_rx.h"
#include "../radio/radiopackets2.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h> 
#include <getopt.h>
#include <poll.h>
#include <sched.h>

#include "video_source_majestic.h"
#include "video_sources.h"
#include "video_tx_buffers.h"
#include "events.h"
#include "timers.h"
#include "shared_vars.h"
#include "launchers_vehicle.h"
#include "packets_utils.h"
#include "adaptive_video.h"
#include "ruby_rt_vehicle.h"

#define MAX_AUDIO_MAJ_BUFFER 4096

// Tested with majestic:
// master+c953265, 2024-12-16

//To fix extern ParserH264 s_ParserH264CameraOutput;

int s_fInputVideoStreamUDPSocket = -1;
int s_iInputVideoStreamUDPPort = 5600;
bool s_bInputVideoStreamUSDPSocketSetupNonBlocking = false;
u16 s_uLastRTPSeqNumberInUDPFrames[256];
u16 s_uLastRTPSeqNumberInUDPFramesSkipCounter[256];

bool s_bLogStartOfInputVideoData = true;

u8 s_uInputVideoUDPBuffer[MAX_PACKET_TOTAL_SIZE];
u8 s_uOutputUDPNALFrameSegment[MAX_PACKET_TOTAL_SIZE+10];
u8 s_uInputMajAudioBuffer[MAX_AUDIO_MAJ_BUFFER];
int s_iInputMajAudioBufferBytes = 0;

u32 s_uDebugTimeLastUDPVideoInputCheck = 0;
u32 s_uDebugUDPInputBytes = 0;
u32 s_uDebugUDPInputReads = 0;

u32 s_uLastNALType = 0;
bool s_bLastReadIsSingleNAL = false;
bool s_bLastReadIsEndNAL = false;
bool s_bLastReadIsStartNAL = false;
u32 s_uTimeLastMajesticRecvData = 0;
u32 s_uTimeLastCheckMajesticProcess = 0;
int s_iCountMajestigProcessNotRunningChecks = 0;

u32 s_uLastVideoSourceReadTimestamps[5];
u32 s_uLastAlarmUDPOveflowTimestamp = 0;

bool s_bIsRestartingMajestic = false;
u32 s_uTimeMajesticStarted = 0;
pthread_t s_pThreadRestartMajestic;

void _video_source_majestic_move_ruby_to_other_cores()
{
   int iPIDMajestic = hw_process_exists("majestic");
   int iPIDMajestic2 = hardware_camera_maj_get_current_pid();

   if ( (iPIDMajestic <= 0) || (iPIDMajestic2 <= 0) )
   {
      log_line("[VideoSourceMaj] Can't balance CPU cores for majestic. Majestic is not running (PIDs: %d, %d)", iPIDMajestic, iPIDMajestic2);
      return;
   }

   int iCPUCore = hw_process_get_current_core(iPIDMajestic);

   log_line("[VideoSourceMaj] Current CPU core for majestic is: %d", iCPUCore);

   if ( 0 == iCPUCore )
      iCPUCore = 1;
   else
      iCPUCore = 0;
   hw_set_current_thread_affinity("rt_vehicle-maj", iCPUCore, iCPUCore);
   iCPUCore = 1 - iCPUCore;
   radio_rx_set_cpu_affinity(iCPUCore);
   log_line("[VideoSourceMaj] Done adjusting CPU cores balance for majestic-rt_vehicle.");
}

void _video_source_majestic_check_cores_affinities_balance()
{
   int iPIDMajestic = hw_process_exists("majestic");
   int iPIDMajestic2 = hardware_camera_maj_get_current_pid();

   if ( (iPIDMajestic <= 0) || (iPIDMajestic2 <= 0) )
   {
      log_line("[VideoSourceMaj] Can't check CPU cores balance for majestic. Majestic is not running (PIDs: %d, %d)", iPIDMajestic, iPIDMajestic2);
      return;
   }

   int iCPUCoreMaj = hw_process_get_current_core(iPIDMajestic);
   int iCPUCoreRuby = hw_process_get_current_core(getpid());
   
   log_line("[VideoSourceMaj] Current CPU core for majestic is: %d, for ruby_rt_vehicle is: %d, %s", iCPUCoreMaj, iCPUCoreRuby, (iCPUCoreMaj == iCPUCoreRuby)?"the same":"are different");
   if ( iCPUCoreMaj == iCPUCoreRuby )
       _video_source_majestic_move_ruby_to_other_cores();
}

void video_source_majestic_stop_program()
{
   if ( s_bIsRestartingMajestic )
      pthread_cancel(s_pThreadRestartMajestic);
   s_bIsRestartingMajestic = false;

   hardware_camera_maj_stop_threads();

   if ( -1 != s_fInputVideoStreamUDPSocket )
   {
      log_line("[VideoSourceMaj] Stop program: Closed input UDP socket.");
      close(s_fInputVideoStreamUDPSocket);
   }
   else
      log_line("[VideoSourceMaj] Stop Program: No input UDP socket to close.");
   s_fInputVideoStreamUDPSocket = -1;

   if ( hardware_camera_maj_get_current_pid() > 0 )
   {
      hardware_camera_maj_add_log("Thread: Will stop existing majestic process...", false);
      hardware_camera_maj_stop_capture_program();
   }
   s_uTimeMajesticStarted = 0;
   log_line("[VideoSourceMaj] Stopped program.");
}

int _video_source_majestic_open(int iUDPPort)
{
   if ( -1 != s_fInputVideoStreamUDPSocket )
      return s_fInputVideoStreamUDPSocket;

   s_iInputMajAudioBufferBytes = 0;
   for( int i=0; i<256; i++ )
   {
      s_uLastRTPSeqNumberInUDPFrames[i] = 0;
      s_uLastRTPSeqNumberInUDPFramesSkipCounter[i] = 0;
   }
   s_iInputVideoStreamUDPPort = iUDPPort;
   s_bInputVideoStreamUSDPSocketSetupNonBlocking = false;
   struct sockaddr_in server_addr;
   s_fInputVideoStreamUDPSocket = socket(AF_INET, SOCK_DGRAM, 0);
   if (s_fInputVideoStreamUDPSocket == -1)
   {
      log_softerror_and_alarm("[VideoSourceMaj] Can't create socket for video stream on port %d.", s_iInputVideoStreamUDPPort);
      return -1;
   }


   const int optval = 1;
   if ( 0 != setsockopt(s_fInputVideoStreamUDPSocket, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(optval)) )
       log_softerror_and_alarm("[VideoSourceMaj] Failed to set SO_REUSEADDR: %s", strerror(errno));

   if ( 0 != setsockopt(s_fInputVideoStreamUDPSocket, SOL_SOCKET, SO_RXQ_OVFL, (const void *)&optval , sizeof(optval)) )
       log_softerror_and_alarm("[VideoSourceMaj] Unable to set SO_RXQ_OVFL: %s", strerror(errno));
   
   int iRecvSize = 0;
   socklen_t iParamLen = sizeof(iRecvSize);
   getsockopt(s_fInputVideoStreamUDPSocket, SOL_SOCKET, SO_RCVBUF, &iRecvSize, &iParamLen);
   log_line("[VideoSourceMaj] Current socket recv buffer size: %d", iRecvSize);

   int iWantedRecvSize = 512*1024;
   if( 0 != setsockopt(s_fInputVideoStreamUDPSocket, SOL_SOCKET, SO_RCVBUF, (const void *)&iWantedRecvSize, sizeof(iWantedRecvSize)))
      log_softerror_and_alarm("[VideoSourceMaj] Unable to set SO_RCVBUF: %s", strerror(errno));

   iRecvSize = 0;
   iParamLen = sizeof(iRecvSize);
   getsockopt(s_fInputVideoStreamUDPSocket, SOL_SOCKET, SO_RCVBUF, &iRecvSize, &iParamLen);
   log_line("[VideoSourceMaj] New current socket recv buffer size: %d (requested: %d)", iRecvSize, iWantedRecvSize);
 
   memset(&server_addr, 0, sizeof(server_addr));
   server_addr.sin_family = AF_INET;
   server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   server_addr.sin_port = htons((unsigned short)s_iInputVideoStreamUDPPort );
 
   if( bind(s_fInputVideoStreamUDPSocket,(struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 )
   {
      log_error_and_alarm("[VideoSourceMaj] Failed to bind socket for video stream to port %d.", s_iInputVideoStreamUDPPort);
      close(s_fInputVideoStreamUDPSocket);
      s_fInputVideoStreamUDPSocket = -1;
      return -1;
   }

   log_line("[VideoSourceMaj] Opened read socket on port %d for reading video stream. socket fd = %d", s_iInputVideoStreamUDPPort, s_fInputVideoStreamUDPSocket);
   
   return s_fInputVideoStreamUDPSocket;
}

// Returns initial set video bitrate
u32 video_source_majestic_start_program(u32 uOverwriteInitialBitrate, int iOverwriteInitialKFMs, int iOverwriteInitialQPDelta, int* pInitialKFSet)
{
   log_line("[VideoSourceMaj] Start program: Majestic file size: %d bytes", get_filesize("/usr/bin/majestic") );

   if ( 0 != hardware_camera_maj_validate_config() )
   {
      log_softerror_and_alarm("[VideoSourceMaj] Start program: Invalid majestic config. Don't start it.");
      return 0;
   }

   hardware_set_oipc_gpu_boost(g_pCurrentModel->processesPriorities.iFreqGPU);

   //bool bEnableLog = true;
   //if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS) )
   //   bEnableLog = false;

   log_line("[VideoSourceMaj] Start and set params for majestic.");
   log_line("[VideoSourceMaj] Overwrite initial bitrate: %.3f", (float)uOverwriteInitialBitrate/1000.0/1000.0);
   log_line("[VideoSourceMaj] Overwrite initial keyframe: %d ms", iOverwriteInitialKFMs);
   log_line("[VideoSourceMaj] Overwrite initial QPDelta: %d", iOverwriteInitialQPDelta);
   hardware_camera_maj_add_log("Initialize...", false);

   hardware_camera_maj_set_temp_values(uOverwriteInitialBitrate, iOverwriteInitialKFMs, iOverwriteInitialQPDelta);
   video_source_majestic_apply_all_parameters();

   // Majestic starts with the RT priority of the calling process. So set it first and then revert it

   if ( g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS )
   {
      log_line("Adjust current proc and thread priorities to match video capture raw priority %d", g_pCurrentModel->processesPriorities.iThreadPriorityVideoCapture);
      if ( (g_pCurrentModel->processesPriorities.iThreadPriorityVideoCapture > 1) && (g_pCurrentModel->processesPriorities.iThreadPriorityVideoCapture <= 100) )
      {
         hw_set_priority_current_proc(g_pCurrentModel->processesPriorities.iThreadPriorityVideoCapture);
         hw_set_current_thread_raw_priority("maj start", g_pCurrentModel->processesPriorities.iThreadPriorityVideoCapture);
      }
      else
      {
         hw_set_priority_current_proc(-1);
         hw_set_current_thread_raw_priority("maj start", -1);
      }
      sched_yield();
   }

   hardware_camera_maj_start_capture_program(false);
   sched_yield();

   if ( (g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_AFFINITY_CORES_VIDEO_CAPTURE) &&
        (g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_AFFINITY_CORES) &&
        (hw_procs_get_cpu_count() > 1) )
      hw_set_process_affinity("majestic", -1, g_pCurrentModel->processesPriorities.iCoreVideoCapture, g_pCurrentModel->processesPriorities.iCoreVideoCapture);

   if ( g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS )
   {
      log_line("Adjust current proc and thread priorities to match back router raw priority %d", g_pCurrentModel->processesPriorities.iThreadPriorityRouter);
      hw_set_priority_current_proc(g_pCurrentModel->processesPriorities.iThreadPriorityRouter);
      hw_set_current_thread_raw_priority("maj start finish revert", g_pCurrentModel->processesPriorities.iThreadPriorityRouter);
   }
   sched_yield();

   if ( (! (g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_AFFINITY_CORES_VIDEO_CAPTURE)) &&
        (! (g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_AFFINITY_CORES)) &&
        (hw_procs_get_cpu_count() > 1) &&
        (g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_AFFINITY_CORES_BALANCE_OIPC) )
      _video_source_majestic_move_ruby_to_other_cores();

   u32 uInitialVideoBitrate = hardware_camera_maj_get_current_bitrate();
   if ( NULL != pInitialKFSet )
      *pInitialKFSet = hardware_camera_maj_get_current_keyframe();

   int iPID = hardware_camera_maj_get_current_pid();
   if ( 0 == iPID )
   {
      log_softerror_and_alarm("[VideoSourceMaj] Start program: Can't find the PID of majestic");
      s_uTimeMajesticStarted = 0;
      s_bIsRestartingMajestic = false;
      return 0;
   }
   log_line("[VideoSourceMaj] Started majestic capture program, PID: %d", iPID);
   hardware_camera_maj_add_log("Majestic is started.", false);

   if ( g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS )
   {
      if ( g_pCurrentModel->processesPriorities.iThreadPriorityVideoCapture > 100 )
      {
         hardware_sleep_ms(50);
         log_line("[VideoSourceMaj] Adjust majestic nice priority to %d", g_pCurrentModel->processesPriorities.iThreadPriorityVideoCapture - 120);
         char szComm[256];
         sprintf(szComm,"renice -n %d -p %d", g_pCurrentModel->processesPriorities.iThreadPriorityVideoCapture - 120, iPID);
         hw_execute_bash_command(szComm, NULL);
      }
      else if ( g_pCurrentModel->processesPriorities.iThreadPriorityVideoCapture > 1 )
      {
      }
   }

   hardware_sleep_ms(200);
   if ( g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_BALANCE_INTERRUPTS_CORES )
      hardware_balance_interupts();

   hardware_camera_maj_init_threads(g_pCurrentModel);

   _video_source_majestic_open(MAJESTIC_UDP_PORT);
   video_source_majestic_clear_input_buffers();

   s_bLogStartOfInputVideoData = true;
   s_uTimeMajesticStarted = g_TimeNow;
   s_bIsRestartingMajestic = false;

   log_line("[VideoSourceMaj] Start program: Completed. Initial video bitrate set to majestic: %.3f", (float)uInitialVideoBitrate/1000.0/1000.0);
   return uInitialVideoBitrate;
}

void video_source_majestic_apply_all_parameters()
{
   for( int i=0; i<(int)(sizeof(s_uLastVideoSourceReadTimestamps)/sizeof(s_uLastVideoSourceReadTimestamps[0])); i++ )
      s_uLastVideoSourceReadTimestamps[i] = 0;

   signal_start_long_op();
   hardware_camera_maj_apply_all_settings(g_pCurrentModel, &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile]),
          g_pCurrentModel->video_params.iCurrentVideoProfile, &(g_pCurrentModel->video_params));

   video_source_majestic_clear_input_buffers();
   video_source_majestic_clear_audio_buffers();
   signal_end_long_op();
}

u32 video_source_majestic_get_program_start_time()
{
   return s_uTimeMajesticStarted;
}

void _restart_majestic_procedure()
{
   log_line("[VideoSourceMaj] Restart procedure started.");

   signal_start_long_op();

   hardware_camera_maj_stop_threads();

   if ( -1 != s_fInputVideoStreamUDPSocket )
   {
      log_line("[VideoSourceMaj] Restart procedure: Closed input UDP socket.");
      close(s_fInputVideoStreamUDPSocket);
   }
   else
      log_line("[VideoSourceMaj] Restart procedure: No input UDP socket to close.");
   s_fInputVideoStreamUDPSocket = -1;

   if ( hardware_camera_maj_get_current_pid() > 0 )
   {
      hardware_camera_maj_add_log("Thread: Will stop existing majestic process...", false);
      hardware_camera_maj_stop_capture_program();
   }

   video_sources_start_capture();

   signal_end_long_op();
   log_line("[VideoSourceMaj] Restart procedure completed.");
}

void* _thread_restart_majestic(void *argument)
{
   log_line("[VideoSourceMaj] Thread: Started thread to stop/re-start majestic...");
   hw_log_current_thread_attributes("maj restart");

   _restart_majestic_procedure();

   s_bIsRestartingMajestic = false;

   log_line("[VideoSourceMaj] Thread: Ended thread to stop/re-start majestic.");
   return NULL;
}

uint32_t extract_udp_rxq_overflow(struct msghdr *msg)
{
    struct cmsghdr *cmsg;
    uint32_t rtn;

    for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL; cmsg = CMSG_NXTHDR(msg, cmsg))
    {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_RXQ_OVFL)
        {
            memcpy(&rtn, CMSG_DATA(cmsg), sizeof(rtn));
            return rtn;
        }
    }
    return 0;
}

int _video_source_majestic_try_read_input_udp_data(bool bAsync)
{
   if ( -1 == s_fInputVideoStreamUDPSocket )
      return -1;

   int nRecvBytes = 0;
   if ( bAsync )
   {
      if ( s_bInputVideoStreamUSDPSocketSetupNonBlocking )
      {
         s_bInputVideoStreamUSDPSocketSetupNonBlocking = false;
         if ( fcntl(s_fInputVideoStreamUDPSocket, F_SETFL, fcntl(s_fInputVideoStreamUDPSocket, F_GETFL, 0) | O_NONBLOCK) < 0 )
            log_softerror_and_alarm("[VideoSourceMaj] Unable to set socket into nonblocked mode: %s", strerror(errno));

         log_line("[VideoSourceMaj] Done first time video USD socket setup.");
      }

      static uint8_t cmsgbuf[CMSG_SPACE(sizeof(uint32_t))];

      //static int nfds = 1;
      //static struct pollfd fds[2];
      //memset(fds, '\0', sizeof(fds));
      //fds[0].fd = s_fInputVideoStreamUDPSocket;
      //fds[0].events = POLLIN;
      //int res = poll(fds, nfds, 1);
      fd_set fdSet;
      FD_ZERO(&fdSet);
      FD_SET(s_fInputVideoStreamUDPSocket, &fdSet);
      struct timeval timeWait;
      timeWait.tv_sec = 0;
      timeWait.tv_usec = 100;
      int res = select(s_fInputVideoStreamUDPSocket+1, &fdSet, NULL, NULL, &timeWait);
      if ( res < 0 )
      {
         log_error_and_alarm("[VideoSourceMaj] Failed to poll UDP socket.");
         return -1;
      }
      if ( 0 == res )
         return 0;
      /*
      if (fds[0].revents & (POLLERR | POLLNVAL))
      {
         log_softerror_and_alarm("[VideoSourceMaj] Socket error pooling: %s", strerror(errno));
         return -1;
      }
      if ( ! (fds[0].revents & POLLIN) )
         return 0;
      */
      if ( 0 == FD_ISSET(s_fInputVideoStreamUDPSocket, &fdSet) )
         return 0;
      struct iovec iov = { .iov_base = (void*)s_uInputVideoUDPBuffer,
                                            .iov_len = sizeof(s_uInputVideoUDPBuffer) };
      struct msghdr msghdr = { .msg_name = NULL,
                               .msg_namelen = 0,
                               .msg_iov = &iov,
                               .msg_iovlen = 1,
                               .msg_control = &cmsgbuf,
                               .msg_controllen = sizeof(cmsgbuf),
                               .msg_flags = 0 };

      memset(cmsgbuf, '\0', sizeof(cmsgbuf));

      nRecvBytes = recvmsg(s_fInputVideoStreamUDPSocket, &msghdr, 0);
      if ( nRecvBytes < 0 )
      {
         log_softerror_and_alarm("[VideoSourceMaj] Failed to recvmsg from UDP socket, error: %s", strerror(errno));
         return -1;
      }

      for(int i=4; i>0; i--)
         s_uLastVideoSourceReadTimestamps[i] = s_uLastVideoSourceReadTimestamps[i-1];
      s_uLastVideoSourceReadTimestamps[0] = g_TimeNow;

      if ( nRecvBytes > MAX_PACKET_TOTAL_SIZE )
      {
         log_softerror_and_alarm("[VideoSourceMaj] Read too much data %d bytes from UDP socket", nRecvBytes);
         nRecvBytes = MAX_PACKET_TOTAL_SIZE;
      }

      static uint32_t rxq_overflow = 0;
      uint32_t cur_rxq_overflow = extract_udp_rxq_overflow(&msghdr);
      static int s_iRxUDPOverflowCounter = 0;
      if (cur_rxq_overflow != rxq_overflow)
      {
          u32 uDroppedCount = cur_rxq_overflow - rxq_overflow;
          if ( s_bIsRestartingMajestic )
             log_line("[VideoSourceMaj] UDP dropped %u packets while restarting majestic.", uDroppedCount);
          else
          {
             log_softerror_and_alarm("[VideoSourceMaj] UDP rxq overflow: %u packets dropped (from %u to %u), overflow counter: %d", uDroppedCount, rxq_overflow, cur_rxq_overflow, s_iRxUDPOverflowCounter);
             log_softerror_and_alarm("[VideoSourceMaj] Last 4 majestic UDP reads: %u ms ago, %u ms ago, %u ms ago, %u ms ago",
                s_uLastVideoSourceReadTimestamps[1] - g_TimeNow, s_uLastVideoSourceReadTimestamps[2] - g_TimeNow, s_uLastVideoSourceReadTimestamps[3] - g_TimeNow, s_uLastVideoSourceReadTimestamps[4] - g_TimeNow );
             s_iRxUDPOverflowCounter++;
             if ( cur_rxq_overflow > rxq_overflow + 1 )
             if ( g_TimeNow > s_uLastAlarmUDPOveflowTimestamp + 10000 )
             if ( g_TimeNow > g_TimeStart + 10000 )
             if ( g_TimeNow > hardware_camera_maj_get_last_change_time() + 3000 )
             {
                s_uLastAlarmUDPOveflowTimestamp = g_TimeNow;
                u32 uFlags2 = 0;
                u32 uDelta = s_uLastVideoSourceReadTimestamps[0] - s_uLastVideoSourceReadTimestamps[1];
                if ( uDelta > 255 )
                   uDelta = 255;
                uFlags2 |= uDelta & 0xFF;
                uDelta = s_uLastVideoSourceReadTimestamps[1] - s_uLastVideoSourceReadTimestamps[2];
                if ( uDelta > 255 )
                   uDelta = 255;
                uFlags2 |= (uDelta & 0xFF) << 8;
                uDelta = s_uLastVideoSourceReadTimestamps[2] - s_uLastVideoSourceReadTimestamps[3];
                if ( uDelta > 255 )
                   uDelta = 255;
                uFlags2 |= (uDelta & 0xFF) << 16;
                
                send_alarm_to_controller(ALARM_ID_DEVELOPER_ALARM, ALARM_FLAG_DEVELOPER_ALARM_UDP_SKIPPED | ((uDroppedCount & 0xFF) << 8), uFlags2, 5);
             }

             if ( s_iRxUDPOverflowCounter > 10 )
             {
                log_softerror_and_alarm("[VideoSourceMaj] Too many UDP overflows: Reopen UDP port...");
                if ( -1 != s_fInputVideoStreamUDPSocket )
                {
                   close(s_fInputVideoStreamUDPSocket);
                   log_line("[VideoSourceMaj] Closed input UDP socket.");
                }
                else
                   log_line("[VideoSourceMaj] No input UDP socket to close.");
                s_fInputVideoStreamUDPSocket = -1;
                _video_source_majestic_open(MAJESTIC_UDP_PORT);
                video_source_majestic_clear_input_buffers();
                s_iRxUDPOverflowCounter = 0;
                nRecvBytes = 0;
                log_softerror_and_alarm("[VideoSourceMaj] Too many UDP overflows: Reopened UDP port.");
             }
          }
          rxq_overflow = cur_rxq_overflow;
      }
      else
         s_iRxUDPOverflowCounter = 0;
   }
   else
   {
      fd_set readset;
      FD_ZERO(&readset);
      FD_SET(s_fInputVideoStreamUDPSocket, &readset);

      struct timeval timePipeInput;
      timePipeInput.tv_sec = 0;
      timePipeInput.tv_usec = 5*1000; // 5 miliseconds timeout

      int iSelectResult = select(s_fInputVideoStreamUDPSocket+1, &readset, NULL, NULL, &timePipeInput);
      if ( iSelectResult < 0 )
      {
         log_error_and_alarm("[VideoSourceMaj] Failed to select socket.");
         return -1;
      }
      if ( iSelectResult == 0 )
         return 0;

      if( 0 == FD_ISSET(s_fInputVideoStreamUDPSocket, &readset) )
         return 0;

      struct sockaddr_in client_addr;
      socklen_t len = sizeof(client_addr);
      
      memset(&client_addr, 0, sizeof(client_addr));
      client_addr.sin_family = AF_INET;
      client_addr.sin_addr.s_addr = INADDR_ANY;
      client_addr.sin_port = htons( s_iInputVideoStreamUDPPort );

      nRecvBytes = recvfrom(s_fInputVideoStreamUDPSocket, s_uInputVideoUDPBuffer, sizeof(s_uInputVideoUDPBuffer)/sizeof(s_uInputVideoUDPBuffer[0]), 
                MSG_WAITALL, ( struct sockaddr *) &client_addr,
                &len);
      //int nRecv = recv(socket_server, szBuff, 1024, )
      if ( nRecvBytes < 0 )
      {
         log_error_and_alarm("[VideoSourceMaj] Failed to receive from UDP socket.");
         return -1;
      }
      if ( nRecvBytes == 0 )
         return 0;
   }
   return nRecvBytes;
}

// Parse input raw bytes and returns a NAL packet (in s_uOutputUDPNALFrameSegment)

int _video_source_majestic_parse_rtp_data(u8* pInputRawData, int iInputBytes)
{
   s_bLastReadIsSingleNAL = false;
   s_bLastReadIsEndNAL = false;
   s_bLastReadIsStartNAL = false;

   if ( iInputBytes <= 12 )
   {
      log_softerror_and_alarm("[VideoSourceMaj] Process RTP packet too small, only %d bytes", iInputBytes);
      return 0;
   }
 
   int iRTPHeaderLength = 0;
   if ( (pInputRawData[0] & 0x80) && (pInputRawData[1] & 0x60) )
      iRTPHeaderLength = 12;

   // ----------------------------------------------
   // Begin - Check RTP sequence number   
   bool bHasPadding = ((pInputRawData[0]>>5) & 0x01)?true:false;
   u8  uRTPPacketType = (pInputRawData[1] & 0x7F);
   u16 uRTPSeqNb = (((u16)pInputRawData[2]) << 8) | pInputRawData[3];
   int iPaddingBytes = pInputRawData[iInputBytes-1];

   if ( (0 != s_uLastRTPSeqNumberInUDPFrames[uRTPPacketType]) && (65535 != s_uLastRTPSeqNumberInUDPFrames[uRTPPacketType]) )
   if ( (s_uLastRTPSeqNumberInUDPFrames[uRTPPacketType] + 1) != uRTPSeqNb )
   {
      s_uLastRTPSeqNumberInUDPFramesSkipCounter[uRTPPacketType]++;
      log_softerror_and_alarm("[VideoSourceMaj] Read skipped RTP frames tpye %d, from seqnb %d to seqnb %d", uRTPPacketType, s_uLastRTPSeqNumberInUDPFrames[uRTPPacketType], uRTPSeqNb);
      if ( (uRTPPacketType == 98) || (uRTPPacketType == 100) )
      if ( s_uLastRTPSeqNumberInUDPFramesSkipCounter[uRTPPacketType] > 10 )
         hw_execute_bash_command("killall -1 majestic", NULL);
   }
   s_uLastRTPSeqNumberInUDPFrames[uRTPPacketType] = uRTPSeqNb;

   // End - Check RTP sequence number
   // ----------------------------------------------

   if ( (uRTPPacketType == 98) || (uRTPPacketType == 100) )
   {
      if ( g_TimeNow > hardware_camera_maj_get_last_audio_change_time() + 150 )
      if ( s_iInputMajAudioBufferBytes + (iInputBytes - iRTPHeaderLength) < MAX_AUDIO_MAJ_BUFFER )
      {
         memcpy(&(s_uInputMajAudioBuffer[s_iInputMajAudioBufferBytes]), pInputRawData + iRTPHeaderLength, iInputBytes - iRTPHeaderLength);
         s_iInputMajAudioBufferBytes += (iInputBytes - iRTPHeaderLength);
      }
      return 0;
   }
   pInputRawData += iRTPHeaderLength;
   iInputBytes -= iRTPHeaderLength;

   if ( iInputBytes < 1 )
   {
      log_softerror_and_alarm("[VideoSourceMaj] Received invalid RTP size (12 bytes header only)");
      return 0;
   }
   u8 uFragmentTypeH264 = pInputRawData[0] & 0x1F;
   u8 uFragmentTypeH265 = (pInputRawData[0]>>1) & 0x3F;

   static u8 uNALOutputHeader[6];
   int iNALOutputHeaderSize = 4;

   // Single compact (non-fragmented, non I/P frame (slice)) NAL unit (ie. SPS, PPS, etc)
   if ( (uFragmentTypeH264 != 28) && (uFragmentTypeH265 != 49) )
   {
      uNALOutputHeader[0] = 0;
      uNALOutputHeader[1] = 0;
      uNALOutputHeader[2] = 0;
      uNALOutputHeader[3] = 0x01;

      s_bLastReadIsSingleNAL = true;
      s_bLastReadIsEndNAL = true;
      s_bLastReadIsStartNAL = true;

      // H264 frame type: lower 5 bits (&0x1F) of uNALOutputHeader[4]: 5 - Iframe, 1 - Pframe
      s_uLastNALType = pInputRawData[0] & 0x1F;

      memcpy(s_uOutputUDPNALFrameSegment, uNALOutputHeader, iNALOutputHeaderSize);
      memcpy(&s_uOutputUDPNALFrameSegment[iNALOutputHeaderSize], pInputRawData, iInputBytes);
      int iOutputSize = iInputBytes + iNALOutputHeaderSize;
      if ( bHasPadding )
         iOutputSize -= iPaddingBytes;
      return iOutputSize;
   }
   
   // Fragmentation unit (fragmented NAL over multiple packets)

   s_bLastReadIsSingleNAL = false;
   s_bLastReadIsEndNAL = false;
   s_bLastReadIsStartNAL = false;

   
   u8 uFUStartBit = 0;
   u8 uFUEndBit = 0;

   if ( uFragmentTypeH264 == 28 ) 
   {
      if ( iInputBytes < 2 )
      {
         log_softerror_and_alarm("[VideoSourceMaj] Received invalid RTP size (1 byte only for 28 fragment)");
         return 0;
      }
      // H264 fragment
      uFUStartBit = pInputRawData[1] & 0x80;
      uFUEndBit = pInputRawData[1] & 0x40;
      uNALOutputHeader[4] = (pInputRawData[0] & 0xE0) | (pInputRawData[1] & 0x1F);
      // H264 frame type: lower 5 bits (& 0x1F) of uNALOutputHeader[4]: 5 - Iframe, 1 - Pframe
      s_uLastNALType = uNALOutputHeader[4] & 0x1F;

      iNALOutputHeaderSize++;
      pInputRawData++;
      iInputBytes--;
      pInputRawData++;
      iInputBytes--;
   }
   else 
   {
      if ( iInputBytes < 3 )
      {
         log_softerror_and_alarm("[VideoSourceMaj] Received invalid RTP size (2 bytes only for 49 fragment)");
         return 0;
      }
      // H265 fragment
      uFUStartBit = pInputRawData[2] & 0x80;
      uFUEndBit = pInputRawData[2] & 0x40;

      uNALOutputHeader[4] = (pInputRawData[0] & 0x81) | (pInputRawData[2] & 0x3F) << 1;
      uNALOutputHeader[5] = 1;
      // (uNALOutputHeader[4] >> 1) & 0x3F   ->  19: Iframe;  1: Pframe
      s_uLastNALType = (uNALOutputHeader[4] >> 1) & 0x3F;

      iNALOutputHeaderSize++;
      iNALOutputHeaderSize++;
      pInputRawData++;
      iInputBytes--;
      pInputRawData++;
      iInputBytes--;
      pInputRawData++;
      iInputBytes--;
   }

   if ( uFUStartBit )
      s_bLastReadIsStartNAL = true;
   if ( uFUEndBit )
      s_bLastReadIsEndNAL = true;

   if (uFUStartBit) 
   {
      uNALOutputHeader[0] = 0;
      uNALOutputHeader[1] = 0;
      uNALOutputHeader[2] = 0;
      uNALOutputHeader[3] = 0x01;
      memcpy(s_uOutputUDPNALFrameSegment, uNALOutputHeader, iNALOutputHeaderSize);
      memcpy(&s_uOutputUDPNALFrameSegment[iNALOutputHeaderSize], pInputRawData, iInputBytes);
      int iOutputSize = iInputBytes + iNALOutputHeaderSize;
      if ( bHasPadding )
         iOutputSize -= iPaddingBytes;
      return iOutputSize;
   }
   else
   {
      memcpy(s_uOutputUDPNALFrameSegment, pInputRawData, iInputBytes);
      if ( bHasPadding )
         iInputBytes -= iPaddingBytes;
      return iInputBytes;
   }

   return 0;
}


// To remove
/*
u32 s_uPrevToken = 0x11111111;
u32 s_uCurrentToken = 0x11111111;
u32 s_uTimeLastNAL = 0;
u32 s_uNALSize = 0;
void _parse_stream(unsigned char* pBuffer, int iLength)
{
   while ( iLength > 0 )
   {
      s_uPrevToken = (s_uPrevToken << 8) | (s_uCurrentToken & 0xFF);
      s_uCurrentToken = (s_uCurrentToken << 8) | (*pBuffer);
      pBuffer++;
      iLength--;
      s_uNALSize++;
      if ( s_uPrevToken == 0x00000001 )
      {
         unsigned char uNALType = s_uCurrentToken & 0b11111;
         u32 uTime = get_current_timestamp_ms();
         //log_line("NAL: %d, prev %u bytes, %u ms from last", uNALType, s_uNALSize, uTime - s_uTimeLastNAL);
         s_uTimeLastNAL = uTime;
         s_uNALSize = 0;
      }
   } 
}
*/

// Returns the buffer and number of bytes read
u8* video_source_majestic_read(int* piReadSize, bool bAsync, u32* puOutTimeDataAvailable)
{
   if ( NULL == piReadSize )
      return NULL;

   *piReadSize = 0;

   if ( s_bIsRestartingMajestic )
      return NULL;

   int iRecvBytes = _video_source_majestic_try_read_input_udp_data(bAsync);
   if ( iRecvBytes <= 0 )
      return NULL;

   if ( NULL != puOutTimeDataAvailable )
   {
      g_TimeNow = get_current_timestamp_ms();
      *puOutTimeDataAvailable = g_TimeNow;
   }

   if ( s_bLogStartOfInputVideoData )
   {
      log_line("[VideoSourceMaj] Start receiving data (H264/H265 stream) from camera");
      s_bLogStartOfInputVideoData = false;
   }
   s_iCountMajestigProcessNotRunningChecks = 0;
   s_uTimeLastMajesticRecvData = g_TimeNow;
   s_uDebugUDPInputBytes += iRecvBytes;
   s_uDebugUDPInputReads++;

   int iOutputBytes = _video_source_majestic_parse_rtp_data(s_uInputVideoUDPBuffer, iRecvBytes);

   *piReadSize = iOutputBytes;
   if ( iOutputBytes <= 0 )
      return NULL;
   return s_uOutputUDPNALFrameSegment;
}


int video_source_majestic_get_audio_data(u8* pOutputBuffer, int iMaxToRead)
{
   if ( (NULL == pOutputBuffer) || (iMaxToRead < 0 ) )
      return 0;
   
   int iRead = s_iInputMajAudioBufferBytes;
   if ( iRead > iMaxToRead )
      iRead = iMaxToRead;
   memcpy(pOutputBuffer, s_uInputMajAudioBuffer, iRead);
 
   if ( iRead == s_iInputMajAudioBufferBytes )
      s_iInputMajAudioBufferBytes = 0;
   else
   {
      for( int i=iRead; i<s_iInputMajAudioBufferBytes; i++ )
        s_uInputMajAudioBuffer[i-iRead] = s_uInputVideoUDPBuffer[i];
      s_iInputMajAudioBufferBytes -= iRead;
   }
   return iRead;
}

void video_source_majestic_clear_audio_buffers()
{
   s_iInputMajAudioBufferBytes = 0;
}

void video_source_majestic_clear_input_buffers()
{
   // Clear majestic buffers
   log_line("[VideoSourceMaj] Clearing input buffers...");
   int iCount =0;
   int iBytes = 0;
   for( int i=0; i<1000; i++ )
   {
      int iReadSize = 0;
      u8* pVideoData = video_source_majestic_read(&iReadSize, true, NULL);
      if ( NULL == pVideoData )
            break;
      iCount++;
      iBytes += iReadSize;
   }
   log_line("[VideoSourceMaj] Done clearing input buffers. Cleared %d packets, total %d bytes.", iCount, iBytes);
}

bool video_source_majestic_last_read_is_single_nal()
{
   return s_bLastReadIsSingleNAL;
}

bool video_source_majestic_last_read_is_start_nal()
{
   return s_bLastReadIsStartNAL;
}

bool video_source_majestic_last_read_is_end_nal()
{
   return s_bLastReadIsEndNAL;
}

u32 video_source_majestic_get_last_nal_type()
{
   return s_uLastNALType;
}


bool video_source_majestic_periodic_health_checks()
{
   if ( g_TimeNow >= s_uDebugTimeLastUDPVideoInputCheck+10000 )
   {
      char szBitrate[64];
      str_format_bitrate(s_uDebugUDPInputBytes/10*8, szBitrate);

      log_line("[VideoSourceMaj] Input video data: %u bytes/sec, %s, %u reads/sec",
         s_uDebugUDPInputBytes/10, szBitrate, s_uDebugUDPInputReads/10);
      s_uDebugTimeLastUDPVideoInputCheck = g_TimeNow;
      // To fix log_line("[VideoSourceMaj] Detected video stream fps: %d, slices: %d", (int)s_ParserH264CameraOutput.getDetectedFPS(), s_ParserH264CameraOutput.getDetectedSlices());
      s_uDebugUDPInputBytes = 0;
      s_uDebugUDPInputReads = 0;
   }

   if ( s_bIsRestartingMajestic || g_bVideoPaused )
      return false;


   // Check if cores affinities are balanced
   static u32 s_uTimeLastCheckAffinitiesBalance = 0;
   if ( (! (g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_AFFINITY_CORES_VIDEO_CAPTURE)) &&
        (! (g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_AFFINITY_CORES)) &&
        (hw_procs_get_cpu_count() > 1) &&
        (g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_AFFINITY_CORES_BALANCE_OIPC) )
   if ( g_TimeNow > s_uTimeLastCheckAffinitiesBalance + 10000 )
   {
      s_uTimeLastCheckAffinitiesBalance = g_TimeNow;
      _video_source_majestic_check_cores_affinities_balance();
   }

   // Check majestic process to be generating video data
   if ( s_iCountMajestigProcessNotRunningChecks >= 0 )
   if ( g_TimeNow > g_TimeStart + 10000 )
   if ( g_TimeNow > s_uTimeLastMajesticRecvData + 5000 )
   if ( (s_uTimeMajesticStarted != 0) && (g_TimeNow > s_uTimeMajesticStarted+2000))
   if ( g_TimeNow > hardware_camera_maj_get_last_change_time() + 5000 )
   {
      log_softerror_and_alarm("[VideoSourceMaj] majestic is not generating any video stream. Restart it.");
      
      signal_start_long_op();
      video_source_majestic_stop_program();
      s_bIsRestartingMajestic = true;
      s_iCountMajestigProcessNotRunningChecks++;
      signal_end_long_op();
      if ( hw_process_exists("sysupgrade") )
      {
         log_softerror_and_alarm("[VideoSourceMaj] Sysupgrade is in progress. Don't do anything else. Just quit.");
         return true;
      }
      send_alarm_to_controller(ALARM_ID_VIDEO_CAPTURE_MALFUNCTION,0,0, 5);

      // Restart did not worked. Do hard restart 
      if ( s_iCountMajestigProcessNotRunningChecks >= 2 )
      {
         // Do a full restart of vehicle
         log_line("[VideoSourceMaj] Majestic can't start. Do a full restart of vehicle...");

         sem_t* pSem = sem_open(SEMAPHORE_RESTART_VEHICLE_PROCS, O_CREAT, S_IWUSR | S_IRUSR, 0);
         if ( (NULL == pSem) || (SEM_FAILED == pSem) )
            log_softerror_and_alarm("[VideoSourceMaj] Failed to open semaphore to signal restart: %s", SEMAPHORE_RESTART_VEHICLE_PROCS);
         else
         {
            sem_post(pSem);
            sem_close(pSem);
            log_line("[VideoSourceMaj] Signaled semaphore to restart all procs."); 
         }
         
         s_iCountMajestigProcessNotRunningChecks = -2;
         return true;
      }

      pthread_attr_t attr;

      int iCoreAffinity = -1;
      if ( g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_AFFINITY_CORES )
         iCoreAffinity = g_pCurrentModel->processesPriorities.iCoreOthers;

      hw_init_worker_thread_attrs(&attr, iCoreAffinity, -1, SCHED_OTHER, 0, "maj restart");

      if ( 0 != pthread_create(&s_pThreadRestartMajestic, &attr, &_thread_restart_majestic, NULL) )
      {  
         log_softerror_and_alarm("[VideoSourceMaj] Failed to create thread to stop/restart majestic. Do it manually.");
         _restart_majestic_procedure();
         _video_source_majestic_open(MAJESTIC_UDP_PORT);
         s_bIsRestartingMajestic = false;
      }
      pthread_attr_destroy(&attr);
      return false;
   }

   static u32 s_uTimeLastOverflowCheck = 0;
   if ( g_TimeNow > s_uTimeLastOverflowCheck + 500 )
   if ( g_TimeNow > hardware_camera_maj_get_last_change_time() + 500 )
   if ( g_TimeNow > s_uTimeLastMajesticRecvData + 500 )
   if ( NULL != g_pVideoTxBuffers )
   {
      s_uTimeLastOverflowCheck = g_TimeNow;
      if ( g_pVideoTxBuffers->getResetOverflowFlag() )
      {
         log_softerror_and_alarm("[VideoSourceMaj] Detected inconsistency in packets sizes: developer mode: %s",
            (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE)?"on":"off");
         log_softerror_and_alarm("[VideoSourceMaj] Max raw usable video in tx buffers now: % bytes",
            g_pVideoTxBuffers->getCurrentMaxUsableRawVideoDataSize());
         log_softerror_and_alarm("[VideoSourceMaj] Current majestic NAL size now: % bytes",
            hardware_camera_maj_get_current_nal_size());

         hardware_camera_maj_update_nal_size(g_pCurrentModel);        
         video_source_majestic_clear_input_buffers();
      }
   }

   static u32 s_uTimeLastCheckMajestiUDPSkipCounters = 0;
   if ( g_TimeNow > s_uTimeLastCheckMajestiUDPSkipCounters + 5000 )
   {
      s_uTimeLastCheckMajestiUDPSkipCounters = g_TimeNow;
      s_uLastRTPSeqNumberInUDPFramesSkipCounter[98] = 0;
      s_uLastRTPSeqNumberInUDPFramesSkipCounter[100] = 0;
   }

   return false;
}