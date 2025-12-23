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
#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../radio/fec.h" 

#include "../base/base.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/shared_mem.h"
#include "../base/models.h"
#include "../base/models_list.h"
#include "../base/radio_utils.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../base/ruby_ipc.h"
#include "../base/parser_h264.h"
#include "../base/camera_utils.h"
#include "../common/string_utils.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"

#include "shared_vars.h"
#include "rx_video_output.h"
#include "rx_video_recording.h"
#include "packets_utils.h"
#include "timers.h"
#include "ruby_rt_station.h"
#include "adaptive_video.h"


pthread_t s_pThreadRxVideoOutputStreamer;
sem_t* s_pRxVideoSemaphoreRestartVideoStreamer = NULL;
sem_t* s_pSemaphoreVideoStreamerOverloadAlarm = NULL; 

bool s_bRxVideoOutputStreamerThreadIsRunning = false;
bool s_bRxVideoOutputStreamerThreadMustStop = false;
bool s_bRxVideoOutputStreamerMustReinitialize = false;

bool s_bRxVideoOutputUsePipe = true;
bool s_bRxVideoOutputUseSM = false;

int s_fPipeVideoOutToStreamer = -1;
u8* s_pPipeVideoOutputBuffer = NULL;
int s_iPipeVideoOutputPos = 0;

shared_mem_process_stats* s_pSMProcessStatsMPPPlayer = NULL;
sem_t* s_pSemaphoreSMData = NULL;
u8* s_pSMVideoStreamerWrite = NULL;
u32 s_uSMVideoStreamWritePosition = 0;
bool s_bEnableVideoStreamerOutput = false;
bool s_bDidSentAnyDataToVideoStreamerSM = false;
bool s_bDidSentAnyDataToVideoStreamerPipe = false;
u8 s_uCurrentReceivedVideoStreamType = 0;
ParserH264 s_ParserH264StreamOutput;
ParserH264 s_ParserH264VideoOutput;

u32 s_uLastIOErrorAlarmFlagsVideoStreamer = 0;
u32 s_uLastIOErrorAlarmFlagsUSBPlayer = 0;
u32 s_uTimeLastOkVideoStreamerOutputToPipe = 0;
u32 s_uTimeStartGettingVideoIOErrors = 0;
         
typedef struct
{
   bool bVideoUSBTethering;
   u32 TimeLastVideoUSBTetheringCheck;
   char szIPUSBVideo[32];
   struct sockaddr_in sockAddrUSBDevice;
   int socketUSBOutput;
   int usbBlockSize;
   u8 usbBuffer[2050];
   int usbBufferPos;
} t_video_usb_output_info;

t_video_usb_output_info s_VideoUSBOutputInfo;

typedef struct 
{
   bool s_bForwardETHPipeEnabled;
   int s_ForwardETHVideoPipeFile;

   bool s_bForwardIsETHForwardEnabled;
   int s_ForwardETHSocketVideo;

   struct sockaddr_in s_ForwardETHSockAddr;
   u8 s_BufferETH[2050];
   int s_nBufferETHPos;
   int s_BufferETHPacketSize;
} t_video_eth_forward_info;

t_video_eth_forward_info s_VideoETHOutputInfo;

int s_iLastUSBVideoForwardPort = -1;
int s_iLastUSBVideoForwardPacketSize = 0;

u32 s_TimeLastPeriodicChecksUSBForward = 0;

u32 s_uLastVideoFrameTime = MAX_U32;

int s_iLocalVideoPlayerUDPSocket = -1;
struct sockaddr_in s_LocalVideoPlayuerUDPSocketAddr;

u32 s_uTimeStartLocalVideoPlayer = 0;
u32 s_uTimeLastOutputDataToLocalVideoPlayer = 0;
u32 s_uLastTimeComputedOutputBitrate = 0;
u32 s_uOutputBitrateToLocalVideoStreamerPipe = 0;
u32 s_uOutputBitrateToLocalVideoPlayerUDP = 0;

char s_szOutputVideoStreamerFilename[MAX_FILE_PATH_SIZE];
int  s_iPIDVideoStreamer = -1;

void rx_video_output_start_video_streamer()
{
   log_line("[VideoOutput] Starting video streamer [%s]", s_szOutputVideoStreamerFilename);

   if ( s_iPIDVideoStreamer > 0 )
   {
      log_line("[VideoOutput] Video streamer process already running (PID %d). Do nothing.", s_iPIDVideoStreamer);
      return;
   }

   if ( NULL != s_pSemaphoreSMData )
      sem_close(s_pSemaphoreSMData);
   s_pSemaphoreSMData = NULL;

   //s_pSemaphoreSMData = sem_open(SEMAPHORE_SM_VIDEO_DATA_AVAILABLE, O_RDWR);
   s_pSemaphoreSMData = sem_open(SEMAPHORE_SM_VIDEO_DATA_AVAILABLE, O_CREAT, S_IWUSR | S_IRUSR, 0); 
   if ( (NULL == s_pSemaphoreSMData) || (SEM_FAILED == s_pSemaphoreSMData) )
   {
      log_error_and_alarm("[VideoOutput] Failed to create semaphore: %s", SEMAPHORE_SM_VIDEO_DATA_AVAILABLE);
      return;
   }
   int iSemVal = 0;
   if ( 0 == sem_getvalue(s_pSemaphoreSMData, &iSemVal) )
      log_line("[VideoOutput] SM data semaphore initial value: %d", iSemVal);
   else
      log_softerror_and_alarm("[VideoOutput] Failed to get SM data semaphore value.");

   char szStreamerPrefixes[256];
   char szStreamerParams[256];
   char szTmp[32];
   szStreamerParams[0] = 0;
   szStreamerPrefixes[0] = 0;

   if ( 0 == s_uCurrentReceivedVideoStreamType )
      s_uCurrentReceivedVideoStreamType = VIDEO_TYPE_H264;

   if ( g_pControllerSettings->iCoresAdjustment )
   {
      sprintf(szTmp, "-af %d", (0x01 << CORE_AFFINITY_VIDEO_OUTPUT));
      if ( 0 != szStreamerParams[0] )
         strcat(szStreamerParams, " ");
      strcat(szStreamerParams, szTmp);
   }

   if ( g_pControllerSettings->iPrioritiesAdjustment && (g_pControllerSettings->iThreadPriorityVideo > 1) && (g_pControllerSettings->iThreadPriorityVideo < 100) )
   {
      sprintf(szTmp, "-rawp %d", g_pControllerSettings->iThreadPriorityVideo );
      if ( 0 != szStreamerParams[0] )
         strcat(szStreamerParams, " ");
      strcat(szStreamerParams, szTmp);
   }

   #if defined (HW_PLATFORM_RADXA)
   char szCodec[32];
   szCodec[0] = 0;
   Model* pModel = NULL;
   if ( (NULL != g_pCurrentModel) && (! g_pCurrentModel->hasCamera()) )
   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
   if ( g_pCurrentModel->relay_params.uRelayedVehicleId != 0 )
      pModel = findModelWithId(g_pCurrentModel->relay_params.uRelayedVehicleId, 33);
   if ( NULL == pModel )
      pModel = g_pCurrentModel;

   if ( (NULL != pModel) && (pModel->video_params.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265) )
   {
      strcpy(szCodec, "-h265");
      s_uCurrentReceivedVideoStreamType = VIDEO_TYPE_H265;
      if ( 0 != szStreamerParams[0] )
         strcat(szStreamerParams, " ");
      strcat(szStreamerParams, szCodec);
   }

   if ( 0 != szStreamerParams[0] )
      strcat(szStreamerParams, " ");

   if ( s_bRxVideoOutputUseSM )
      strcat(szStreamerParams, "-sm");
   else if ( s_bRxVideoOutputUsePipe )
      strcat(szStreamerParams, "-p ");
   else
      strcat(szStreamerParams, "-sm");
   #endif

   if ( 0 != szStreamerParams[0] )
      strcat(szStreamerParams, " ");
   strcat(szStreamerParams, "2>&1 1>/dev/null");

   szStreamerPrefixes[0] = 0;
   if ( g_pControllerSettings->iPrioritiesAdjustment && (g_pControllerSettings->iThreadPriorityVideo > 100) )
      sprintf(szStreamerPrefixes, "nice -n %d", g_pControllerSettings->iThreadPriorityVideo - 120);
   #ifdef HW_CAPABILITY_IONICE
   if ( g_pControllerSettings->iPrioritiesAdjustment && (g_pControllerSettings->ioNiceRxVideo > 0) )
   {
      sprintf(szStreamerPrefixes, "ionice -c 1 -n %d", g_pControllerSettings->ioNiceRxVideo);
      if ( g_pControllerSettings->iPrioritiesAdjustment && (g_pControllerSettings->iThreadPriorityVideo > 100) )
         sprintf(szStreamerPrefixes, "ionice -c 1 -n %d nice -n %d", g_pControllerSettings->ioNiceRxVideo, g_pControllerSettings->iThreadPriorityVideo-120);
   }
   #endif

   log_line("[VideoOutput] Executing video streamer command: %s %s %s &", szStreamerPrefixes, s_szOutputVideoStreamerFilename, szStreamerParams);
   hw_execute_ruby_process_wait(szStreamerPrefixes, s_szOutputVideoStreamerFilename, szStreamerParams, NULL, 0);
   log_line("[VideoOutput] Executed video streamer command.");

   char szPIDs[128];

   szPIDs[0] = 0;
   int count = 0;
   log_line("[VideoOutput] Waiting for process (%s) to start...", s_szOutputVideoStreamerFilename);
   u32 uTimeStart = get_current_timestamp_ms();

   hw_process_get_pids(s_szOutputVideoStreamerFilename, szPIDs);
   removeTrailingNewLines(szPIDs);
   while ( (strlen(szPIDs) <= 2) && (count < 1000) && (get_current_timestamp_ms() < uTimeStart+4000) )
   {
      hardware_sleep_ms(10);
      szPIDs[0] = 0;
      hw_process_get_pids(s_szOutputVideoStreamerFilename, szPIDs);
      removeTrailingNewLines(szPIDs);
   }
   replaceNewLinesToSpaces(szPIDs);
   s_iPIDVideoStreamer = atoi(szPIDs);
   log_line("[VideoOutput] Found executed process PID: %d", s_iPIDVideoStreamer);

   if ( (strlen(szPIDs) > 2) && (s_iPIDVideoStreamer > 0) )
      log_line("[VideoOutput] Started video streamer [%s], PID: %s (%d)", s_szOutputVideoStreamerFilename, szPIDs, s_iPIDVideoStreamer);
   else
   {
      log_softerror_and_alarm("[VideoOutput] Failed to launch video streamer");
      return;
   }

   if ( NULL != s_pSMProcessStatsMPPPlayer )
      shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_MPP_PLAYER, s_pSMProcessStatsMPPPlayer);
   s_pSMProcessStatsMPPPlayer = NULL;

   #if defined(HW_PLATFORM_RADXA)
   uTimeStart = g_TimeNow;
   while ( g_TimeNow < uTimeStart + 2000 )
   {
      hardware_sleep_ms(20);
      s_pSMProcessStatsMPPPlayer = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_MPP_PLAYER);
      if ( NULL == s_pSMProcessStatsMPPPlayer )
         log_softerror_and_alarm("[VideoOutput] Failed to open shared mem for process watchdog for reading: %s", SHARED_MEM_WATCHDOG_MPP_PLAYER);
      else
      {
         log_line("[VideoOutput] Opened shared mem for process watchdog for reading (%s).", SHARED_MEM_WATCHDOG_MPP_PLAYER);
         break;
      }
      hardware_sleep_ms(50);
      g_TimeNow = get_current_timestamp_ms();
   }
   #endif

   s_uTimeStartLocalVideoPlayer = g_TimeNow;
   log_line("[VideoOutput] Video streamer start procedure finised and marked start time.");
}

void rx_video_output_stop_video_streamer()
{
   log_line("[VideoOutput] Stopping video streamer...");
   if ( -1 != s_fPipeVideoOutToStreamer )
   {
      log_line("[VideoOutput] Closed video output pipe to streamer.");
      close( s_fPipeVideoOutToStreamer );
   }
   s_fPipeVideoOutToStreamer = -1;

   s_uTimeStartLocalVideoPlayer = 0;
   if ( NULL != s_pSMProcessStatsMPPPlayer )
      shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_MPP_PLAYER, s_pSMProcessStatsMPPPlayer);
   s_pSMProcessStatsMPPPlayer = NULL;

   if ( s_iPIDVideoStreamer > 0 )
   {
      log_line("[VideoOutput] Stopping video streamer by signaling existing streamer (PID %d)...", s_iPIDVideoStreamer);
      if ( s_bRxVideoOutputUsePipe )
         hw_stop_process(VIDEO_PLAYER_PIPE);
      else if ( s_bRxVideoOutputUseSM )
         hw_stop_process(VIDEO_PLAYER_SM);
      else
         log_softerror_and_alarm("[VideoOutput] No video streamer output method defined, so can't stop.");
   }
   else if ( 0 != s_szOutputVideoStreamerFilename[0] )
   {
      log_line("[VideoOutput] Stopping video streamer (%s) by command...", s_szOutputVideoStreamerFilename);
      char szComm[256];
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "ps -ef | nice grep '%s' | nice grep -v grep | awk '{print $2}' | xargs kill -1 2>/dev/null", s_szOutputVideoStreamerFilename);
      hw_execute_bash_command(szComm, NULL);
      hardware_sleep_ms(100);
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "pgrep '%s' | awk '{print $2}' | xargs kill -1 2>/dev/null", s_szOutputVideoStreamerFilename);
      hw_execute_bash_command(szComm, NULL);
      hardware_sleep_ms(100);
   }

   s_iPIDVideoStreamer = -1;

   if ( NULL != s_pSemaphoreSMData )
      sem_close(s_pSemaphoreSMData);
   s_pSemaphoreSMData = NULL;
   sem_unlink(SEMAPHORE_SM_VIDEO_DATA_AVAILABLE);

   log_line("[VideoOutput] Executed command to stop video streamer");
}

void _rx_video_output_check_start_streamer()
{
   if ( s_iPIDVideoStreamer < 10 )
   {
      log_line("[VideoOutput] Streamer is not running, start it...");
      rx_video_output_start_video_streamer();
   }
}

static void * _thread_rx_video_output_streamer_watchdog(void *argument)
{
   s_bRxVideoOutputStreamerThreadIsRunning = true;
   log_line("[VideoOutputThread] Starting worker thread for video streamer output watchdog");
   hw_log_current_thread_attributes("streamer output watchdog");

   sem_t* pSemaphore = sem_open(SEMAPHORE_RESTART_VIDEO_STREAMER, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( (NULL == pSemaphore) || (SEM_FAILED == pSemaphore) )
   {
      log_error_and_alarm("[VideoOutputThread] Failed to open semaphore: %s", SEMAPHORE_RESTART_VIDEO_STREAMER);
      s_bRxVideoOutputStreamerThreadIsRunning = false;
      return NULL;
   }
   else
      log_line("[VideoOutputThread] Opened semaphore for signaling video streamer restart");

   log_line("[VideoOutputThread] Started worker thread for video streamer output. Working state now.");
   
   while ( ! s_bRxVideoOutputStreamerThreadMustStop )
   {
      if ( s_bRxVideoOutputStreamerThreadMustStop )
         break;

      int iRet = sem_wait(pSemaphore);
      if ( 0 != iRet )
      {
         if ( s_bRxVideoOutputStreamerThreadMustStop )
            break;
         continue;
      }
      if ( s_bRxVideoOutputStreamerThreadMustStop )
         break;

      // If the reinitialize flag is not set, then just skip reinitialization
      if ( !s_bRxVideoOutputStreamerMustReinitialize )
         continue;

      log_line("[VideoOutputThread] Semaphore and flag to restart video streamer is set.");

      rx_video_output_stop_video_streamer();
      if ( s_bRxVideoOutputUseSM )
      {
         s_uSMVideoStreamWritePosition = 2*sizeof(u32);
         if ( NULL != s_pSMVideoStreamerWrite )
         {
            u32* pTmp1 = (u32*)(&(s_pSMVideoStreamerWrite[0]));
            u32* pTmp2 = (u32*)(&(s_pSMVideoStreamerWrite[sizeof(u32)]));
            *pTmp1 = s_uSMVideoStreamWritePosition;
            *pTmp2 = s_uSMVideoStreamWritePosition;
         }
         s_bDidSentAnyDataToVideoStreamerSM = false;
         log_line("[VideoOutputThread] Reseted SM video output position.");
      }

      if ( ! s_bRxVideoOutputStreamerThreadMustStop )
      {
         rx_video_output_start_video_streamer();
         rx_video_output_enable_streamer_output();
      }
      else
         log_line("[VideoOutputThread] Signal to quit thread is set. Do not restart the streamer.");
      s_bRxVideoOutputStreamerMustReinitialize = false;
      log_line("[VideoOutputThread] Finished restarting video streamer.");

      if ( s_bRxVideoOutputStreamerThreadMustStop )
         break;

      for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
      {
         if ( NULL != g_pVideoProcessorRxList[i] )
            g_pVideoProcessorRxList[i]->resumeProcessing();
      }
   }

   if ( NULL != pSemaphore )
      sem_close(pSemaphore);
   pSemaphore = NULL;

   log_line("[VideoOutputThread] Stopped worker thread for video streamer output.");
   s_bRxVideoOutputStreamerThreadIsRunning = false;
   return NULL;
}

void _processor_rx_video_forward_open_eth_pipe()
{
   log_line("[VideoOutput] Creating ETH pipes for video forward...");
   char szComm[1024];
   #ifdef HW_CAPABILITY_IONICE
   sprintf(szComm, "ionice -c 1 -n 4 nice -n -5 cat %s | nice -n -5 gst-launch-1.0 fdsrc ! h264parse ! rtph264pay pt=96 config-interval=3 ! udpsink port=%d host=127.0.0.1 > /dev/null 2>&1 &", FIFO_RUBY_STATION_VIDEO_STREAM_ETH, g_pControllerSettings->nVideoForwardETHPort);
   #else
   sprintf(szComm, "nice -n -5 cat %s | nice -n -5 gst-launch-1.0 fdsrc ! h264parse ! rtph264pay pt=96 config-interval=3 ! udpsink port=%d host=127.0.0.1 > /dev/null 2>&1 &", FIFO_RUBY_STATION_VIDEO_STREAM_ETH, g_pControllerSettings->nVideoForwardETHPort);
   #endif
   hw_execute_bash_command(szComm, NULL);

   log_line("[VideoOutput] Opening video output pipe write endpoint for ETH forward RTS: %s", FIFO_RUBY_STATION_VIDEO_STREAM_ETH);
   s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile = open(FIFO_RUBY_STATION_VIDEO_STREAM_ETH, O_WRONLY);
   if ( s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile < 0 )
   {
      log_error_and_alarm("[VideoOutput] Failed to open video output pipe write endpoint for ETH forward RTS: %s",FIFO_RUBY_STATION_VIDEO_STREAM_ETH);
      return;
   }
   log_line("[VideoOutput] Opened video output pipe write endpoint for ETH forward RTS: %s", FIFO_RUBY_STATION_VIDEO_STREAM_ETH);
   log_line("[VideoOutput] Video output pipe to ETH flags: %s", str_get_pipe_flags(fcntl(s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile, F_GETFL)));
   s_VideoETHOutputInfo.s_bForwardETHPipeEnabled = true;
}

void _processor_rx_video_forward_create_eth_socket()
{
   log_line("[VideoOutput] Creating ETH socket for video forward...");
   if ( -1 != s_VideoETHOutputInfo.s_ForwardETHSocketVideo )
      close(s_VideoETHOutputInfo.s_ForwardETHSocketVideo);

   s_VideoETHOutputInfo.s_ForwardETHSocketVideo = socket(AF_INET, SOCK_DGRAM, 0);
   if ( s_VideoETHOutputInfo.s_ForwardETHSocketVideo <= 0 )
   {
      log_softerror_and_alarm("[VideoOutput] Failed to create socket for video forward on ETH.");
      return;
   }

   int broadcastEnable = 1;
   int ret = setsockopt(s_VideoETHOutputInfo.s_ForwardETHSocketVideo, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
   if ( ret != 0 )
   {
      log_softerror_and_alarm("[VideoOutput] Failed to set the Video ETH forward socket broadcast flag.");
      close(s_VideoETHOutputInfo.s_ForwardETHSocketVideo);
      s_VideoETHOutputInfo.s_ForwardETHSocketVideo = -1;
      return;
   }
  
   memset(&s_VideoETHOutputInfo.s_ForwardETHSockAddr, '\0', sizeof(struct sockaddr_in));
   s_VideoETHOutputInfo.s_ForwardETHSockAddr.sin_family = AF_INET;
   s_VideoETHOutputInfo.s_ForwardETHSockAddr.sin_port = (in_port_t)htons(g_pControllerSettings->nVideoForwardETHPort);
   s_VideoETHOutputInfo.s_ForwardETHSockAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
   //s_ForwardETHSockAddr.sin_addr.s_addr = inet_addr("192.168.1.255");

   s_VideoETHOutputInfo.s_nBufferETHPos = 0;
   s_VideoETHOutputInfo.s_BufferETHPacketSize = g_pControllerSettings->nVideoForwardETHPacketSize;
   if ( s_VideoETHOutputInfo.s_BufferETHPacketSize < 100 || s_VideoETHOutputInfo.s_BufferETHPacketSize > 2048 )
      s_VideoETHOutputInfo.s_BufferETHPacketSize = 2048;

   log_line("[VideoOutput] Opened socket [fd=%d] for video forward on ETH on port %d.", s_VideoETHOutputInfo.s_ForwardETHSocketVideo, g_pControllerSettings->nVideoForwardETHPort);
   s_VideoETHOutputInfo.s_bForwardIsETHForwardEnabled = true;
}


void _rx_video_output_open_pipe_to_streamer()
{
   log_line("[VideoOutput] Opening video output pipe to streamer write endpoint: %s", FIFO_RUBY_STATION_VIDEO_STREAM_DISPLAY);

   _rx_video_output_check_start_streamer();

   if ( NULL == s_pPipeVideoOutputBuffer )
   {
      s_pPipeVideoOutputBuffer = (u8*)malloc(250000);
      if ( NULL == s_pPipeVideoOutputBuffer )
         log_softerror_and_alarm("[VideoOutput] Failed to create buffer for stream pipe output");
      s_iPipeVideoOutputPos = 0;
   }

   if ( -1 != s_fPipeVideoOutToStreamer )
   {
      log_line("[VideoOutput] Pipe for video output to streamer already opened.");
      return;
   }

   int iRetries = 200;
   while ( iRetries > 0 )
   {
      iRetries--;
      //s_fPipeVideoOutToStreamer = open(FIFO_RUBY_STATION_VIDEO_STREAM_DISPLAY, O_CREAT | O_WRONLY | O_NONBLOCK);
      s_fPipeVideoOutToStreamer = open(FIFO_RUBY_STATION_VIDEO_STREAM_DISPLAY, O_CREAT | O_WRONLY);
      if ( s_fPipeVideoOutToStreamer < 0 )
      {
         log_error_and_alarm("[VideoOutput] Failed to open video output pipe to streamer write endpoint: %s, error code (%d): [%s]",
            FIFO_RUBY_STATION_VIDEO_STREAM_DISPLAY, errno, strerror(errno));
         if ( iRetries == 0 )
            return;
         else
            hardware_sleep_ms(10);
      }
   }
   log_line("[VideoOutput] Opened video output pipe to streamer write endpoint: %s", FIFO_RUBY_STATION_VIDEO_STREAM_DISPLAY);
   log_line("[VideoOutput] Video output pipe to streamer flags: %s", str_get_pipe_flags(fcntl(s_fPipeVideoOutToStreamer, F_GETFL)));

   //if ( RUBY_PIPES_EXTRA_FLAGS & O_NONBLOCK )
   //if ( 0 != fcntl(s_fPipeVideoOutToStreamer, F_SETFL, O_NONBLOCK) )
   //   log_softerror_and_alarm("[IPC] Failed to set nonblock flag on PIC channel %s write endpoint.", FIFO_RUBY_STATION_VIDEO_STREAM_DISPLAY);
   //log_line("[VideoOutput] Video streamer FIFO write endpoint pipe new flags: %s", str_get_pipe_flags(fcntl(s_fPipeVideoOutToStreamer, F_GETFL)));
  
   log_line("[VideoOutput] Video streamer pipe FIFO default size: %d bytes", fcntl(s_fPipeVideoOutToStreamer, F_GETPIPE_SZ));
   fcntl(s_fPipeVideoOutToStreamer, F_SETPIPE_SZ, 250000);
   log_line("[VideoOutput] Video streamer FIFO new size: %d bytes", fcntl(s_fPipeVideoOutToStreamer, F_GETPIPE_SZ));
   s_bDidSentAnyDataToVideoStreamerPipe = false;
}

void rx_video_output_init()
{
   log_line("[VideoOutput] Init start...");
   
   log_line("[VideoOutput] Controller streamer output mode: %d", g_pControllerSettings->iStreamerOutputMode);
   if ( g_pControllerSettings->iStreamerOutputMode == 0 )
   {
      s_bRxVideoOutputUseSM = true;
      s_bRxVideoOutputUsePipe = false;
   }
   else
   {
      s_bRxVideoOutputUseSM = false;
      s_bRxVideoOutputUsePipe = true;
   }

   if ( s_bRxVideoOutputUsePipe )
   {
      strcpy(s_szOutputVideoStreamerFilename, VIDEO_PLAYER_PIPE);
      log_line("[VideoOutput] Use pipe to streamer");
   }
   else if ( s_bRxVideoOutputUseSM )
   {
      strcpy(s_szOutputVideoStreamerFilename, VIDEO_PLAYER_SM);
      log_line("[VideoOutput] Use sharedmem to streamer");
   }
   else
      log_softerror_and_alarm("[VideoOutput] No video streamer output method defined.");
   
   s_fPipeVideoOutToStreamer = -1;
   s_iLocalVideoPlayerUDPSocket = -1;
   s_uLastVideoFrameTime= MAX_U32;
   s_bDidSentAnyDataToVideoStreamerPipe = false;
   s_bDidSentAnyDataToVideoStreamerSM = false;
   
   s_ParserH264StreamOutput.init();
   s_ParserH264VideoOutput.init();
   
   s_uSMVideoStreamWritePosition = 2*sizeof(u32);
   s_pSMVideoStreamerWrite = NULL;

   if ( s_bRxVideoOutputUseSM )
   {
      int fdSM = shm_open(SM_STREAMER_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

      if( fdSM < 0 )
         log_softerror_and_alarm("[VideoOutput] Failed to open shared memory for write: %s, error: %d %s", SM_STREAMER_NAME, errno, strerror(errno));
      else
      {
         if ( ftruncate(fdSM, SM_STREAMER_SIZE) == -1 )
         {
            log_softerror_and_alarm("[VideoOutput] Failed to init (ftruncate) shared memory: %s, error: %d %s", SM_STREAMER_NAME, errno, strerror(errno));
            close(fdSM);
         }
         else
         {
            s_pSMVideoStreamerWrite = (u8*) mmap(NULL, SM_STREAMER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fdSM, 0);
            if ( (s_pSMVideoStreamerWrite == MAP_FAILED) || (s_pSMVideoStreamerWrite == NULL) )
            {
               log_softerror_and_alarm("[VideoOutput] Failed to map shared memory %s, error: %d %s", SM_STREAMER_NAME, errno, strerror(errno));
               close(fdSM);
               s_pSMVideoStreamerWrite = NULL;
            }
            else
            {
               memset(s_pSMVideoStreamerWrite, 0, SM_STREAMER_SIZE);
               close(fdSM);
               s_uSMVideoStreamWritePosition = 2*sizeof(u32);
               u32* pTmp1 = (u32*)(&(s_pSMVideoStreamerWrite[0]));
               u32* pTmp2 = (u32*)(&(s_pSMVideoStreamerWrite[sizeof(u32)]));
               *pTmp1 = s_uSMVideoStreamWritePosition;
               *pTmp2 = s_uSMVideoStreamWritePosition;
               log_line("[VideoOutput] Successfully opened and cleared SM for video output: %s", SM_STREAMER_NAME);
            }
         }
      }
   }
   s_pSemaphoreVideoStreamerOverloadAlarm = sem_open(SEMAPHORE_VIDEO_STREAMER_OVERLOAD, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( (NULL == s_pSemaphoreVideoStreamerOverloadAlarm) || (SEM_FAILED == s_pSemaphoreVideoStreamerOverloadAlarm) )
      log_softerror_and_alarm("[VideoOutput] Failed to open semaphore for video streamer to signal alarms: %s; error: %d (%s)", SEMAPHORE_VIDEO_STREAMER_OVERLOAD, errno, strerror(errno));
   else
      log_line("[VideoOutput] Opened semaphore for video streamer to signal alarms.");

   s_VideoUSBOutputInfo.bVideoUSBTethering = false;
   s_VideoUSBOutputInfo.TimeLastVideoUSBTetheringCheck = 0;
   s_VideoUSBOutputInfo.szIPUSBVideo[0] = 0;
   s_VideoUSBOutputInfo.socketUSBOutput = -1;
   s_VideoUSBOutputInfo.usbBlockSize = 1024;
   s_VideoUSBOutputInfo.usbBufferPos = 0;
   
   if ( NULL != g_pControllerSettings )
   {
      s_iLastUSBVideoForwardPort = g_pControllerSettings->iVideoForwardUSBPort;
      s_iLastUSBVideoForwardPacketSize = g_pControllerSettings->iVideoForwardUSBPacketSize;
   }
   s_VideoETHOutputInfo.s_bForwardETHPipeEnabled = false;
   s_VideoETHOutputInfo.s_bForwardIsETHForwardEnabled = false;
   s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile = -1;
   s_VideoETHOutputInfo.s_ForwardETHSocketVideo = -1;
   s_VideoETHOutputInfo.s_nBufferETHPos = 0;
   s_VideoETHOutputInfo.s_BufferETHPacketSize = 1024;

   if ( (NULL != g_pControllerSettings) && ( g_pControllerSettings->nVideoForwardETHType == 1 ) )
      s_VideoETHOutputInfo.s_bForwardIsETHForwardEnabled = true;
   if ( (NULL != g_pControllerSettings) && ( g_pControllerSettings->nVideoForwardETHType == 2 ) )
      s_VideoETHOutputInfo.s_bForwardETHPipeEnabled = true;

   if ( s_VideoETHOutputInfo.s_bForwardETHPipeEnabled )
   {
      log_line("[VideoOutput] Video ETH forwarding is enabled, type Raw.");
      _processor_rx_video_forward_open_eth_pipe();
   }
   if ( s_VideoETHOutputInfo.s_bForwardIsETHForwardEnabled )
   {
      log_line("[VideoOutput] Video ETH forwarding is enabled, type RTS.");
      _processor_rx_video_forward_create_eth_socket();
   }
   
   s_pRxVideoSemaphoreRestartVideoStreamer = sem_open(SEMAPHORE_RESTART_VIDEO_STREAMER, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( (NULL == s_pRxVideoSemaphoreRestartVideoStreamer) || (SEM_FAILED == s_pRxVideoSemaphoreRestartVideoStreamer) )
      log_error_and_alarm("[VideoOutput] Failed to open semaphore for writing: %s; error: %d (%s)", SEMAPHORE_RESTART_VIDEO_STREAMER, errno, strerror(errno));
   else
      log_line("[VideoOutput] Created semaphore for signaling video streamer worker thread.");
   s_bRxVideoOutputStreamerThreadMustStop = false;
   s_bRxVideoOutputStreamerMustReinitialize = false;

   pthread_attr_t attr;
   hw_init_worker_thread_attrs(&attr, CORE_AFFINITY_OTHERS, -1, SCHED_OTHER, 0, "video output watchdog");
   if ( 0 != pthread_create(&s_pThreadRxVideoOutputStreamer, &attr, &_thread_rx_video_output_streamer_watchdog, NULL) )
   {
      s_bRxVideoOutputStreamerThreadIsRunning = false;
      log_error_and_alarm("[VideoOutput] Failed to create thread for video streamer output check");
   }
   pthread_attr_destroy(&attr);

   rx_video_recording_init();

   s_uLastIOErrorAlarmFlagsVideoStreamer = 0;
   s_uLastIOErrorAlarmFlagsUSBPlayer = 0;
   s_uTimeLastOkVideoStreamerOutputToPipe = 0;
   s_uTimeStartGettingVideoIOErrors = 0;

   log_line("[VideoOutput] Init start complete.");
}

void rx_video_output_uninit()
{
   log_line("[VideoOutput] Uninit start...");

   if ( -1 != s_iLocalVideoPlayerUDPSocket )
   {
      close(s_iLocalVideoPlayerUDPSocket);
      s_iLocalVideoPlayerUDPSocket = -1;
      log_line("[VideoOutput] Closed local socket for local video player UDP output.");
   }

   s_bRxVideoOutputStreamerMustReinitialize = false;
   s_bRxVideoOutputStreamerThreadMustStop = true;

   rx_video_output_signal_restart_streamer();

   s_bRxVideoOutputStreamerMustReinitialize = false;
   s_bRxVideoOutputStreamerThreadMustStop = true;

   int iCounter = 400;
   while ( s_bRxVideoOutputStreamerThreadIsRunning && (iCounter > 0) )
   {
      iCounter--;
      hardware_sleep_ms(5);
   }

   if ( NULL != s_pRxVideoSemaphoreRestartVideoStreamer )
      sem_close(s_pRxVideoSemaphoreRestartVideoStreamer);
   s_pRxVideoSemaphoreRestartVideoStreamer = NULL;

   if ( 0 == sem_unlink(SEMAPHORE_RESTART_VIDEO_STREAMER) )
      log_line("[VideoOutput] Destroyed the semaphore for restarting video streamer.");
   else
      log_softerror_and_alarm("[VideoOutput] Failed to destroy the semaphore for restarting video streamer.");
   s_pRxVideoSemaphoreRestartVideoStreamer = NULL;

   rx_video_output_stop_video_streamer();
   
   if ( -1 != s_VideoETHOutputInfo.s_ForwardETHSocketVideo )
      close(s_VideoETHOutputInfo.s_ForwardETHSocketVideo);
   s_VideoETHOutputInfo.s_ForwardETHSocketVideo = -1;

   if ( -1 != s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile )
      close(s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile);
   s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile = -1;

   if ( s_VideoETHOutputInfo.s_bForwardETHPipeEnabled )
      hw_stop_process("gst-launch-1.0");
   s_VideoETHOutputInfo.s_bForwardIsETHForwardEnabled = false;
   s_VideoETHOutputInfo.s_bForwardETHPipeEnabled = false;

   if ( -1 != s_fPipeVideoOutToStreamer )
   {
      log_line("[VideoOutput] Closed video output pipe to streamer.");
      close( s_fPipeVideoOutToStreamer );
   }
   s_fPipeVideoOutToStreamer = -1;
   s_bDidSentAnyDataToVideoStreamerPipe = false;
   s_bDidSentAnyDataToVideoStreamerSM = false;

   if ( NULL != s_pPipeVideoOutputBuffer )
      free(s_pPipeVideoOutputBuffer);
   s_pPipeVideoOutputBuffer = NULL;
   s_iPipeVideoOutputPos = 0;

   if ( -1 != s_VideoUSBOutputInfo.socketUSBOutput )
      close(s_VideoUSBOutputInfo.socketUSBOutput);
   s_VideoUSBOutputInfo.socketUSBOutput = -1;
   s_VideoUSBOutputInfo.bVideoUSBTethering = false;
   s_VideoUSBOutputInfo.usbBufferPos = 0;

   rx_video_recording_uninit();

   if ( NULL != s_pSemaphoreVideoStreamerOverloadAlarm )
      sem_close(s_pSemaphoreVideoStreamerOverloadAlarm);
   s_pSemaphoreVideoStreamerOverloadAlarm = NULL;

   if ( NULL != s_pSMVideoStreamerWrite )
   {
      munmap(s_pSMVideoStreamerWrite, SM_STREAMER_SIZE);
      s_pSMVideoStreamerWrite = NULL;
      s_uSMVideoStreamWritePosition = 2*sizeof(u32);
      log_line("[VideoOutput] Closes streamer SM for video output: %S", SM_STREAMER_NAME);
   }
   log_line("[VideoOutput] Uninit complete.");
}

void rx_video_output_enable_streamer_output()
{
   log_line("[VideoOutput] Enable video output to streamer.");

   if ( s_bRxVideoOutputUseSM )
   {
      s_uSMVideoStreamWritePosition = 2*sizeof(u32);
      u32* pTmp1 = (u32*)(&(s_pSMVideoStreamerWrite[0]));
      u32* pTmp2 = (u32*)(&(s_pSMVideoStreamerWrite[sizeof(u32)]));
      *pTmp1 = s_uSMVideoStreamWritePosition;
      *pTmp2 = s_uSMVideoStreamWritePosition;
      log_line("[VideoOutput] Reseted SM video output position.");
   }

   _rx_video_output_check_start_streamer();

   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( NULL == g_pVideoProcessorRxList[i] )
         break;
      g_pVideoProcessorRxList[i]->onControllerSettingsChanged();
   }

   s_bEnableVideoStreamerOutput = true;
   s_bDidSentAnyDataToVideoStreamerSM = false;
   s_bDidSentAnyDataToVideoStreamerPipe = false;

   if ( s_bRxVideoOutputUsePipe )
      _rx_video_output_open_pipe_to_streamer();
}

void rx_video_output_disable_streamer_output()
{
   log_line("[VideoOutput] Disable video output to streamer.");
   s_bEnableVideoStreamerOutput = false;

   if ( -1 != s_fPipeVideoOutToStreamer )
   {
      close( s_fPipeVideoOutToStreamer );
      log_line("[VideoOutput] Closed video output to pipe to streamer.");
   }
   s_fPipeVideoOutToStreamer = -1;
   s_bDidSentAnyDataToVideoStreamerPipe = false;
   s_bDidSentAnyDataToVideoStreamerSM = false;
}

bool rx_video_out_is_stream_output_disabled()
{
   return ! s_bEnableVideoStreamerOutput;
}

void rx_video_output_enable_local_player_udp_output()
{
   log_line("[VideoOutput] Enabling video output to local video player UDP port...");

   if ( -1 != s_iLocalVideoPlayerUDPSocket )
   {
      log_line("[VideoOutput] Local output to video player UDP port already enabled.");
      return;
   }

   s_iLocalVideoPlayerUDPSocket = socket(AF_INET, SOCK_DGRAM, 0);
   if ( s_iLocalVideoPlayerUDPSocket < 0 )
   {
      log_error_and_alarm("[VideoOutput] Failed to create local video player UDP video output socket.");
      return;
   }
   bzero((char *) &s_LocalVideoPlayuerUDPSocketAddr, sizeof(s_LocalVideoPlayuerUDPSocketAddr));
   s_LocalVideoPlayuerUDPSocketAddr.sin_family = AF_INET;
   s_LocalVideoPlayuerUDPSocketAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
   s_LocalVideoPlayuerUDPSocketAddr.sin_port = htons((unsigned short)DEFAULT_LOCAL_VIDEO_PLAYER_UDP_PORT);

   if ( connect(s_iLocalVideoPlayerUDPSocket, (struct sockaddr *) &s_LocalVideoPlayuerUDPSocketAddr, sizeof(s_LocalVideoPlayuerUDPSocketAddr)) < 0 )
   {
      log_error_and_alarm("[VideoOutput] Failed to connect local video player UDP video output socket.");
      s_iLocalVideoPlayerUDPSocket = -1;
   }
   log_line("[VideoOutput] Created socket for local video player UDP output on port %d, fd=%d", DEFAULT_LOCAL_VIDEO_PLAYER_UDP_PORT, s_iLocalVideoPlayerUDPSocket);
}

void rx_video_output_disable_local_player_udp_output()
{
   if ( -1 != s_iLocalVideoPlayerUDPSocket )
   {
      close(s_iLocalVideoPlayerUDPSocket);
      log_line("[VideoOutput] Closed socket for local video player UDP output.");
   }
   s_iLocalVideoPlayerUDPSocket = -1;
}

void _rx_video_output_parse_h264_stream(u32 uVehicleId, u8* pBuffer, int iLength)
{
   if ( g_pControllerSettings->iEnableDebugStats )
   {
      if ( (NULL != g_pCurrentModel) && g_pControllerSettings->iDeveloperMode )
      if ( get_Preferences()->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_OUTPUT_VIDEO_FRAMES )
      {
         u32 uSize = (g_SMControllerDebugVideoRTInfo.uOutputFramesInfo[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] >> 16);
         uSize += iLength;
         g_SMControllerDebugVideoRTInfo.uOutputFramesInfo[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] = 
         (g_SMControllerDebugVideoRTInfo.uOutputFramesInfo[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] & 0x0000FFFF) | ((uSize & 0xFFFF) << 16);

         u32 uDeltaMS = g_TimeNow - g_SMControllerRTInfo.uCurrentSliceStartTime;
         if ( uDeltaMS > 128 )
            uDeltaMS = 128;
         g_SMControllerDebugVideoRTInfo.uOutputFramesInfo[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] &= 0xFFFFFF00;
         g_SMControllerDebugVideoRTInfo.uOutputFramesInfo[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] |= (uDeltaMS & 0xFF);
      }

      u32 uNewNALType = s_ParserH264StreamOutput.parseData(pBuffer, iLength, g_TimeNow);
      if ( uNewNALType == 1 )
         g_SMControllerDebugVideoRTInfo.uOutputFramesInfo[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] |= VIDEO_STATUS_FLAGS2_IS_NAL_P;
      else if ( uNewNALType == 5 )
         g_SMControllerDebugVideoRTInfo.uOutputFramesInfo[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] |= VIDEO_STATUS_FLAGS2_IS_NAL_I;
      else
         g_SMControllerDebugVideoRTInfo.uOutputFramesInfo[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] |= VIDEO_STATUS_FLAGS2_IS_NAL_O;

      if ( s_ParserH264StreamOutput.lastParseDetectedNALStart() != -1 )
         g_SMControllerDebugVideoRTInfo.uOutputFramesInfo[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] |= VIDEO_STATUS_FLAGS2_IS_NAL_START;
   }
   // To fix
   /*
   update_shared_mem_video_frames_stats_on_new_frame( &g_SM_VideoFramesStatsOutput,
       s_ParserH264StreamOutput.getSizeOfLastCompleteFrameInBytes(),
       s_ParserH264StreamOutput.getCurrentFrameType(),
       s_ParserH264StreamOutput.getDetectedSlices(), 
       s_ParserH264StreamOutput.getDetectedFPS(), g_TimeNow );
   */
}

void _rx_video_output_to_sharedmem(u8* pBuffer, u32 uLength, bool bWaitFullFrame, bool bIsEndOfNALFrame)
{
   if ( (NULL == pBuffer) || (uLength == 0 ) || (NULL == s_pSMVideoStreamerWrite) || (!s_bEnableVideoStreamerOutput) || s_bRxVideoOutputStreamerMustReinitialize )
      return;

   s_uTimeLastOutputDataToLocalVideoPlayer = g_TimeNow;
   if ( ! s_bDidSentAnyDataToVideoStreamerSM )
   {
      log_line("[VideoOutput] Send first data to video output SM for local video streamer");
      s_bDidSentAnyDataToVideoStreamerSM = true;
   }

   if ( uLength <= (SM_STREAMER_SIZE-s_uSMVideoStreamWritePosition) )
   {
      memcpy(&(s_pSMVideoStreamerWrite[s_uSMVideoStreamWritePosition]), pBuffer, uLength);
      s_uSMVideoStreamWritePosition += uLength;
      if ( s_uSMVideoStreamWritePosition >= SM_STREAMER_SIZE )
         s_uSMVideoStreamWritePosition = 2*sizeof(u32);
   }
   else
   {
      u32 uLeft = SM_STREAMER_SIZE-s_uSMVideoStreamWritePosition;
      memcpy(&(s_pSMVideoStreamerWrite[s_uSMVideoStreamWritePosition]), pBuffer, uLeft);
      u8* pTmp = pBuffer + uLeft;
      s_uSMVideoStreamWritePosition = 2*sizeof(u32);

      memcpy(&(s_pSMVideoStreamerWrite[s_uSMVideoStreamWritePosition]), pTmp, uLength-uLeft);
      s_uSMVideoStreamWritePosition += uLength-uLeft;
      if ( s_uSMVideoStreamWritePosition >= SM_STREAMER_SIZE )
         s_uSMVideoStreamWritePosition = 2*sizeof(u32);
   }

   if ( (! bWaitFullFrame) || (bWaitFullFrame && bIsEndOfNALFrame) )
   {
      u32* pTmp1 = (u32*)(&(s_pSMVideoStreamerWrite[0]));
      u32* pTmp2 = (u32*)(&(s_pSMVideoStreamerWrite[sizeof(u32)]));
      *pTmp1 = s_uSMVideoStreamWritePosition;
      *pTmp2 = s_uSMVideoStreamWritePosition;

      if ( NULL != s_pSemaphoreSMData )
      {
         if ( 0 != sem_post(s_pSemaphoreSMData) )
            log_softerror_and_alarm("[VideoOutput] Failed to set semaphore for SM data.");
      }
   }
}

void _rx_video_output_to_video_streamer_pipe(u8* pBuffer, int iLength, bool bWaitFullFrame, bool bIsEndOfNALFrame)
{
   if ( (NULL == pBuffer) || (iLength == 0) )
      return;
   if ( (-1 == s_fPipeVideoOutToStreamer) || s_bRxVideoOutputStreamerMustReinitialize )
      return;

   s_uTimeLastOutputDataToLocalVideoPlayer = g_TimeNow;
   if ( ! s_bDidSentAnyDataToVideoStreamerPipe )
   {
      log_line("[VideoOutput] Send first data to video output pipe for local video streamer");
      s_bDidSentAnyDataToVideoStreamerPipe = true;
   }
   
   int iRes = 0;
   int iExpectedWriteResult = iLength;
   if ( (NULL == s_pPipeVideoOutputBuffer) || (! bWaitFullFrame) )
   {
      g_pProcessStats->uInBlockingOperation = 1;
      iRes = write(s_fPipeVideoOutToStreamer, pBuffer, iLength);
      g_pProcessStats->uInBlockingOperation = 0;
   }
   else
   {
      iExpectedWriteResult = 0;
      if ( iLength + s_iPipeVideoOutputPos < 250000 )
      {
         memcpy(&(s_pPipeVideoOutputBuffer[s_iPipeVideoOutputPos]), pBuffer, iLength);
         s_iPipeVideoOutputPos += iLength;
      }
      if ( (! bWaitFullFrame) || (bWaitFullFrame && bIsEndOfNALFrame) )
      {
         iExpectedWriteResult = s_iPipeVideoOutputPos;
         g_pProcessStats->uInBlockingOperation = 1;
         iRes = write(s_fPipeVideoOutToStreamer, s_pPipeVideoOutputBuffer, s_iPipeVideoOutputPos);
         g_pProcessStats->uInBlockingOperation = 0;
         s_iPipeVideoOutputPos = 0;
      }
   }

   if ( iRes == iExpectedWriteResult )
   {
      s_uOutputBitrateToLocalVideoStreamerPipe += iRes*8;
      s_uTimeStartGettingVideoIOErrors = 0;
      if ( 0 != s_uLastIOErrorAlarmFlagsVideoStreamer )
      {
         s_uTimeLastOkVideoStreamerOutputToPipe = g_TimeNow;
         s_uLastIOErrorAlarmFlagsVideoStreamer = 0;
         send_alarm_to_central(ALARM_ID_CONTROLLER_IO_ERROR, 0,0);
      }
      //fsync(s_fPipeVideoOutToStreamer);
      return;
   }

   // Error outputing video to player

   log_softerror_and_alarm("[VideoOutput] Failed to write to streamer pipe (%d bytes). Ret code: %d, Error code: %d, err string: (%s)",
     iExpectedWriteResult, iRes, errno, strerror(errno));
   u32 uFlags = 0;

   if ( iRes >= 0 )
      uFlags = ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT_TRUNCATED;
   else
   {
      if ( EAGAIN == errno )
         uFlags = ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT_WOULD_BLOCK;
      else
         uFlags = ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT;
   }

   u32 uTimeLastVideoChanged = 0;
   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( NULL == g_pVideoProcessorRxList[i] )
         break;
      uTimeLastVideoChanged = g_pVideoProcessorRxList[i]->getLastTimeVideoStreamChanged();
      break;
   }

   s_uLastIOErrorAlarmFlagsVideoStreamer = uFlags;

   if ( g_TimeNow > s_uTimeStartGettingVideoIOErrors + 1000 )
   {
      if ( g_TimeNow > uTimeLastVideoChanged + 3000 )
         send_alarm_to_central(ALARM_ID_CONTROLLER_IO_ERROR, uFlags, 0);

      if ( (0 != s_uTimeLastOkVideoStreamerOutputToPipe) && (g_TimeNow > s_uTimeLastOkVideoStreamerOutputToPipe + 3000) )
      {
         log_line("[VideoOutput] Error outputing video data to video streamer. Reinitialize video streamer...");
         rx_video_output_signal_restart_streamer();
         log_line("[VideoOutput] Signaled restart of streamer");
      }
      if ( 0 == s_uTimeLastOkVideoStreamerOutputToPipe && g_TimeNow > g_TimeStart + 5000 )
      {
         log_line("[VideoOutput] Error outputing any video data to video streamer. Reinitialize video streamer...");
         rx_video_output_signal_restart_streamer();
         log_line("[VideoOutput] Signaled restart of streamer");
      }
   }
}

void _rx_video_output_to_local_video_player_udp(u8* pData, int iLength)
{
   s_uOutputBitrateToLocalVideoPlayerUDP += iLength*8;
   
   sendto(s_iLocalVideoPlayerUDPSocket, pData, iLength, 0, (struct sockaddr *)&s_LocalVideoPlayuerUDPSocketAddr, sizeof(s_LocalVideoPlayuerUDPSocketAddr) );
   /*
   struct iovec iov;
   struct msghdr msghdr;
   iov.iov_base = pData;
   iov.iov_len = iLength;
   
   memset(&msghdr, 0, sizeof(msghdr));
   msghdr.msg_iov = &iov;
   msghdr.msg_iovlen = 1;

   sendmsg(s_iLocalVideoPlayerUDPSocket, &msghdr, MSG_DONTWAIT);
   */
}

void _rx_video_output_to_eth(u8* pData, int iLength)
{
   int dataLen = iLength;
   while ( dataLen > 0 )
   {
       int maxCopy = dataLen;
       if ( maxCopy > s_VideoETHOutputInfo.s_BufferETHPacketSize - s_VideoETHOutputInfo.s_nBufferETHPos )
          maxCopy = s_VideoETHOutputInfo.s_BufferETHPacketSize - s_VideoETHOutputInfo.s_nBufferETHPos;
       memcpy( &(s_VideoETHOutputInfo.s_BufferETH[s_VideoETHOutputInfo.s_nBufferETHPos]), pData, maxCopy );
       pData += maxCopy;
       dataLen -= maxCopy;
       s_VideoETHOutputInfo.s_nBufferETHPos += maxCopy;
       if ( s_VideoETHOutputInfo.s_nBufferETHPos >= s_VideoETHOutputInfo.s_BufferETHPacketSize )
       {
          int res = sendto(s_VideoETHOutputInfo.s_ForwardETHSocketVideo, s_VideoETHOutputInfo.s_BufferETH, s_VideoETHOutputInfo.s_nBufferETHPos,
                        0, (struct sockaddr *)&s_VideoETHOutputInfo.s_ForwardETHSockAddr, sizeof(s_VideoETHOutputInfo.s_ForwardETHSockAddr) );
          if ( res < 0 )
          {
             log_line("[VideoOutput] Failed to send to ETH Port %d bytes, [fd=%d]", iLength, s_VideoETHOutputInfo.s_ForwardETHSocketVideo);
             close(s_VideoETHOutputInfo.s_ForwardETHSocketVideo);
             s_VideoETHOutputInfo.s_ForwardETHSocketVideo = -1;
          }
          //else
          //   log_line("Sent %d bytes to port %d", video_data_length, g_pControllerSettings->nVideoForwardETHPort);
          s_VideoETHOutputInfo.s_nBufferETHPos = 0;
       }
   }
}

void _rx_video_output_to_usb(u8* pData, int iLength)
{
   int dataLen = iLength;
   while ( dataLen > 0 )
   {
      int maxCopy = dataLen;
      if ( maxCopy > s_VideoUSBOutputInfo.usbBlockSize - s_VideoUSBOutputInfo.usbBufferPos )
          maxCopy = s_VideoUSBOutputInfo.usbBlockSize - s_VideoUSBOutputInfo.usbBufferPos;
      memcpy( &(s_VideoUSBOutputInfo.usbBuffer[s_VideoUSBOutputInfo.usbBufferPos]), pData, maxCopy );
      pData += maxCopy;
      dataLen -= maxCopy;
      s_VideoUSBOutputInfo.usbBufferPos += maxCopy;
      if ( s_VideoUSBOutputInfo.usbBufferPos >= s_VideoUSBOutputInfo.usbBlockSize )
      {
         int res = sendto(s_VideoUSBOutputInfo.socketUSBOutput, s_VideoUSBOutputInfo.usbBuffer, s_VideoUSBOutputInfo.usbBlockSize,
               0, (struct sockaddr *)&s_VideoUSBOutputInfo.sockAddrUSBDevice, sizeof(s_VideoUSBOutputInfo.sockAddrUSBDevice) );
         if ( res < 0 )
         {
           log_line("[VideoOutput] Failed to send to USB socket");
           if ( -1 != s_VideoUSBOutputInfo.socketUSBOutput )
              close(s_VideoUSBOutputInfo.socketUSBOutput);
           s_VideoUSBOutputInfo.socketUSBOutput = -1;
           s_VideoUSBOutputInfo.bVideoUSBTethering = false;
           s_VideoUSBOutputInfo.usbBufferPos = 0;
           log_line("[VideoOutput] Video Output to USB disabled.");
           break;
         }
         s_VideoUSBOutputInfo.usbBufferPos = 0;
      }
   }
}

void rx_video_output_discard_cached_data()
{
   if ( s_bEnableVideoStreamerOutput && s_bRxVideoOutputUseSM )
   if ( NULL != s_pSMVideoStreamerWrite )
   {
      s_uSMVideoStreamWritePosition = s_pSMVideoStreamerWrite[0];
   }

   s_iPipeVideoOutputPos = 0;
}

void rx_video_output_video_data(u32 uVehicleId, t_packet_header_video_segment* pPHVS, int width, int height, u8* pBuffer, int video_data_length, int packet_length, bool bWaitFullFrame)
{
   if ( g_bSearching )
      return;

   u8 uVideoStreamType = (pPHVS->uVideoStreamIndexAndType >> 4) & 0x0F;

   if ( g_pControllerSettings->iEnableDebugStats )
   {
      u32 uSize = g_SMControllerDebugVideoRTInfo.uOutputedFramesSizes[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] & 0xFFFFFF;
      uSize += video_data_length;
      g_SMControllerDebugVideoRTInfo.uOutputedFramesSizes[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] &= 0xFF000000;
      g_SMControllerDebugVideoRTInfo.uOutputedFramesSizes[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] |= uSize;
   }

   // To fix
   //if ( g_pCurrentModel->bDeveloperMode )
   //if ( packet_length > video_data_length + sizeof(t_packet_header) + sizeof(t_packet_header_video_full_98) + sizeof(u16) )
   //{
   //   u8* pExtraData = pBuffer + video_data_length;
   //   u32* pExtraDataU32 = (u32*)pExtraData;
   //   pExtraDataU32[6] = get_current_timestamp_ms();
      
   //   /*
   //   u32 uVehicleTimestampOrg = pExtraDataU32[3];
   //            int iVehicleTimestampNow = (int)uVehicleTimestampOrg + radio_get_link_clock_delta();
    //           if ( (int)uTimeNow - (int)iVehicleTimestampNow > 3 )
   //               log_line("DEBUG (%s)[%u/%d]%d rx v-org: %u, now-loc: %u, tr-time: %u, dclks: %d",
   //                 (pPHVF->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_IFRAME)?"I":"P",
   //                 pPHVF->video_block_index, (int)pPHVF->video_block_packet_index, iCountReads,
   //                 uVehicleTimestampOrg, uTimeNow,
    //                (int)uTimeNow - (int)iVehicleTimestampNow, radio_get_link_clock_delta());
   //  */
   //}

   bool bParseStream = false;

   if ( (NULL != g_pCurrentModel) && g_pControllerSettings->iDeveloperMode )
   if ( uVideoStreamType == VIDEO_TYPE_H264 )
   if ( g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_FLAG_SHOW_STATS_VIDEO_H264_FRAMES_INFO)
   //if ( get_ControllerSettings()->iShowVideoStreamInfoCompactType == 0 )
      bParseStream = true;

   if ( (NULL != g_pCurrentModel) && g_pControllerSettings->iDeveloperMode )
   if ( get_Preferences()->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_OUTPUT_VIDEO_FRAMES )
      bParseStream = true;

   static bool s_bLastParseVideoOutputStreamState = false;
   if ( bParseStream != s_bLastParseVideoOutputStreamState )
      s_ParserH264StreamOutput.init();
   s_bLastParseVideoOutputStreamState = bParseStream;

   g_SMControllerDebugVideoRTInfo.uOutputFramesInfo[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] |= (pPHVS->uVideoStatusFlags2 & 0x0000FF00);

   if ( bParseStream )
      _rx_video_output_parse_h264_stream(uVehicleId, pBuffer, video_data_length);


   // Check for video resolution changes or codec changes
   static int s_iRxVideoForwardLastWidth = 0;
   static int s_iRxVideoForwardLastHeight = 0;

   if ( s_bEnableVideoStreamerOutput )
   {
      if ( (s_iRxVideoForwardLastWidth == 0) || (s_iRxVideoForwardLastHeight == 0) )
      {
         s_iRxVideoForwardLastWidth = width;
         s_iRxVideoForwardLastHeight = height;
         if ( (0 != width) || (0 != height) )
            log_line("[VideoOutput] Initial video stream resolution: %d x %d", width, height);
      }

      if ( (width != s_iRxVideoForwardLastWidth) || (height != s_iRxVideoForwardLastHeight) )
      {
         log_line("[VideoOutput] Video resolution changed at VID %u (From %d x %d to %d x %d). Signal reload of the video streamer.",
            uVehicleId, s_iRxVideoForwardLastWidth, s_iRxVideoForwardLastHeight, width, height);
         s_iRxVideoForwardLastWidth = width;
         s_iRxVideoForwardLastHeight = height;
         rx_video_recording_stop();
         rx_video_output_signal_restart_streamer();
         log_line("[VideoOutput] Signaled restart of streamer");
         return;
      }

      if ( s_uCurrentReceivedVideoStreamType != uVideoStreamType )
      {
         log_line("[VideoOutput] Video stream codec changed at VID %u (From %d to %d). Signal reload of the video streamer.",
            uVehicleId, s_uCurrentReceivedVideoStreamType, uVideoStreamType);
         s_uCurrentReceivedVideoStreamType = uVideoStreamType;
         rx_video_recording_stop();
         rx_video_output_signal_restart_streamer();
         log_line("[VideoOutput] Signaled restart of streamer");
         return;
      }
   }

   if ( s_bEnableVideoStreamerOutput && s_bRxVideoOutputUseSM )
      _rx_video_output_to_sharedmem(pBuffer, (u32)video_data_length, bWaitFullFrame, (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_END)?true:false);

   if ( (-1 != s_fPipeVideoOutToStreamer) && s_bEnableVideoStreamerOutput && s_bRxVideoOutputUsePipe )
      _rx_video_output_to_video_streamer_pipe(pBuffer, video_data_length, bWaitFullFrame, (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_END)?true:false);

   if ( -1 != s_iLocalVideoPlayerUDPSocket )
      _rx_video_output_to_local_video_player_udp(pBuffer, video_data_length);

   if ( s_VideoETHOutputInfo.s_bForwardETHPipeEnabled && (-1 != s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile) )
      write(s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile, pBuffer, video_data_length);

   rx_video_recording_on_new_data(pBuffer, video_data_length);

   if ( s_VideoETHOutputInfo.s_bForwardIsETHForwardEnabled && (-1 != s_VideoETHOutputInfo.s_ForwardETHSocketVideo ) )
      _rx_video_output_to_eth(pBuffer, video_data_length);

   if ( s_VideoUSBOutputInfo.bVideoUSBTethering && 0 != s_VideoUSBOutputInfo.szIPUSBVideo[0] )
      _rx_video_output_to_usb(pBuffer, video_data_length);
}


void rx_video_output_on_controller_settings_changed()
{
   log_line("[VideoOutput] Handling controller settings changed.");
   if ( s_iLastUSBVideoForwardPort != g_pControllerSettings->iVideoForwardUSBPort ||
        s_iLastUSBVideoForwardPacketSize != g_pControllerSettings->iVideoForwardUSBPacketSize )
   if ( s_VideoUSBOutputInfo.bVideoUSBTethering )
   {
      if ( -1 != s_VideoUSBOutputInfo.socketUSBOutput )
         close(s_VideoUSBOutputInfo.socketUSBOutput);
      s_VideoUSBOutputInfo.socketUSBOutput = -1;
      s_VideoUSBOutputInfo.bVideoUSBTethering = false;
      log_line("[VideoOutput] Video Output to USB disabled due to settings changed.");
   }

   if ( g_pControllerSettings->nVideoForwardETHType == 0 )
   {
      if ( -1 != s_VideoETHOutputInfo.s_ForwardETHSocketVideo )
         close(s_VideoETHOutputInfo.s_ForwardETHSocketVideo);
      s_VideoETHOutputInfo.s_ForwardETHSocketVideo = -1;
      if ( -1 != s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile )
         close(s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile);
      s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile = -1;

      s_VideoETHOutputInfo.s_bForwardETHPipeEnabled = false;
      s_VideoETHOutputInfo.s_bForwardIsETHForwardEnabled = false;
      log_line("[VideoOutput] Video ETH forwarding was disabled.");
   }
   else if ( g_pControllerSettings->nVideoForwardETHType == 1 )
   {
      if ( -1 != s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile )
         close(s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile);
      s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile = -1;
      if ( s_VideoETHOutputInfo.s_bForwardETHPipeEnabled )
         hw_stop_process("gst-launch-1.0");
      s_VideoETHOutputInfo.s_bForwardETHPipeEnabled = false;
      s_VideoETHOutputInfo.s_bForwardIsETHForwardEnabled = true;

      log_line("[VideoOutput] Video ETH forwarding is enabled, type Raw.");
      _processor_rx_video_forward_create_eth_socket();
   }
   else if ( g_pControllerSettings->nVideoForwardETHType == 2 )
   {
      if ( -1 != s_VideoETHOutputInfo.s_ForwardETHSocketVideo )
         close(s_VideoETHOutputInfo.s_ForwardETHSocketVideo);
      s_VideoETHOutputInfo.s_ForwardETHSocketVideo = -1;
      s_VideoETHOutputInfo.s_bForwardIsETHForwardEnabled = false;

      s_VideoETHOutputInfo.s_bForwardETHPipeEnabled = true;
      log_line("[VideoOutput] Video ETH forwarding is enabled, type RTS.");
      _processor_rx_video_forward_open_eth_pipe();
   }

   s_iLastUSBVideoForwardPort = g_pControllerSettings->iVideoForwardUSBPort;
   s_iLastUSBVideoForwardPacketSize = g_pControllerSettings->iVideoForwardUSBPacketSize;
}

void rx_video_output_on_changed_video_params(video_parameters_t* pOldVideoParams, type_video_link_profile* pOldVideoProfiles, video_parameters_t* pNewVideoParams, type_video_link_profile* pNewVideoProfiles)
{
   if ( (NULL == pOldVideoParams) || (NULL == pNewVideoParams) )
   {
      log_softerror_and_alarm("[VideoOutput] Received invalid change in video parameters: NULL video params.");
      return;
   }
   if ( (NULL == pOldVideoProfiles) || (NULL == pNewVideoProfiles) )
   {
      log_softerror_and_alarm("[VideoOutput] Received invalid change in video parameters: NULL video profiles.");
      return;
   }

   char szProfileOld[32];
   char szProfileNew[32];
   strcpy(szProfileOld, str_get_video_profile_name(pOldVideoParams->iCurrentVideoProfile));
   strcpy(szProfileNew, str_get_video_profile_name(pNewVideoParams->iCurrentVideoProfile));
   log_line("[VideoOutput] Changed video params. Old video profile: %s, new video profile: %s", szProfileOld, szProfileNew);
}

void rx_video_output_signal_restart_streamer()
{
   if ( s_bRxVideoOutputStreamerMustReinitialize )
   {
      if ( s_bRxVideoOutputStreamerThreadMustStop )
         log_line("[VideoOutput] Video streamer must be signaled for stop, but it's already signaled.");
      else
         log_line("[VideoOutput] Video streamer must be signaled for restart, but it's already signaled.");
      return;
   }
   if ( NULL == s_pRxVideoSemaphoreRestartVideoStreamer )
   {
      if ( s_bRxVideoOutputStreamerThreadMustStop )
         log_error_and_alarm("[VideoOutput] Can't signal video streamer to stop. No signal.");
      else
         log_error_and_alarm("[VideoOutput] Can't signal video streamer to restart. No signal.");
      return;
   }

   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( NULL != g_pVideoProcessorRxList[i] )
         g_pVideoProcessorRxList[i]->pauseProcessing();
   }

   adaptive_video_pause(5000);

   rx_video_output_disable_streamer_output();
   s_bRxVideoOutputStreamerMustReinitialize = true;
   sem_post(s_pRxVideoSemaphoreRestartVideoStreamer);
   if ( s_bRxVideoOutputStreamerThreadMustStop )
      log_line("[VideoOutput] Signaled semaphore to stop the streamer on request.");
   else
      log_line("[VideoOutput] Signaled semaphore to restart the streamer on request.");
}

void _rx_video_output_watchdog_mpp_player()
{
   if ( NULL == s_pSMProcessStatsMPPPlayer )
      return;

   if ( g_bSearching || (!s_bEnableVideoStreamerOutput) || (!s_bRxVideoOutputUseSM) )
      return;

   static u32 s_uTimeLastCheckMPPPlayer = 0;
   if ( g_TimeNow < s_uTimeLastCheckMPPPlayer + 500 )
      return;

   s_uTimeLastCheckMPPPlayer = g_TimeNow;

   if ( (s_uTimeLastOutputDataToLocalVideoPlayer < g_TimeNow-200) || (g_TimeNow < s_uTimeStartLocalVideoPlayer + 2000) )
      return;
}

void rx_video_output_periodic_loop()
{
   rx_video_recording_periodic_loop();

   #if defined(HW_PLATFORM_RADXA)
   _rx_video_output_watchdog_mpp_player();
   #endif

   if ( g_bDebugState )
   if ( g_TimeNow >= s_uLastTimeComputedOutputBitrate + 1000 )
   {
      log_line("[VideoOutput] Output to pipe: %u bps, output to UDP: %u bps", s_uOutputBitrateToLocalVideoStreamerPipe, s_uOutputBitrateToLocalVideoPlayerUDP );
      s_uLastTimeComputedOutputBitrate = g_TimeNow;
      s_uOutputBitrateToLocalVideoStreamerPipe = 0;
      s_uOutputBitrateToLocalVideoPlayerUDP = 0;
   }

   if ( g_TimeNow > s_TimeLastPeriodicChecksUSBForward + 300 )
   {
      s_TimeLastPeriodicChecksUSBForward = g_TimeNow;

      // Stopped USB forward?
      if ( s_VideoUSBOutputInfo.bVideoUSBTethering && (g_pControllerSettings->iVideoForwardUSBType == 0) )
      {
         if ( -1 != s_VideoUSBOutputInfo.socketUSBOutput )
            close(s_VideoUSBOutputInfo.socketUSBOutput);
         s_VideoUSBOutputInfo.socketUSBOutput = -1;
         s_VideoUSBOutputInfo.bVideoUSBTethering = false;
         log_line("[VideoOutput] Video Output to USB disabled.");
      }

      if ( g_pControllerSettings->iVideoForwardUSBType != 0 )
      if ( g_TimeNow > s_VideoUSBOutputInfo.TimeLastVideoUSBTetheringCheck + 1000 )
      {
         char szFile[128];
         strcpy(szFile, FOLDER_RUBY_TEMP);
         strcat(szFile, FILE_TEMP_USB_TETHERING_DEVICE);
         s_VideoUSBOutputInfo.TimeLastVideoUSBTetheringCheck = g_TimeNow;
         if ( ! s_VideoUSBOutputInfo.bVideoUSBTethering )
         if ( access(szFile, R_OK) != -1 )
         {
         s_VideoUSBOutputInfo.szIPUSBVideo[0] = 0;
         FILE* fd = fopen(szFile, "r");
         if ( NULL != fd )
         {
            fscanf(fd, "%s", s_VideoUSBOutputInfo.szIPUSBVideo);
            fclose(fd);
         }
         log_line("[VideoOutput] USB Device Tethered for Video Output. Device IP: %s", s_VideoUSBOutputInfo.szIPUSBVideo);

         s_VideoUSBOutputInfo.socketUSBOutput = socket(AF_INET , SOCK_DGRAM, 0);
         if ( s_VideoUSBOutputInfo.socketUSBOutput != -1 && 0 != s_VideoUSBOutputInfo.szIPUSBVideo[0] )
         {
            memset(&s_VideoUSBOutputInfo.sockAddrUSBDevice, 0, sizeof(s_VideoUSBOutputInfo.sockAddrUSBDevice));
            s_VideoUSBOutputInfo.sockAddrUSBDevice.sin_family = AF_INET;
            s_VideoUSBOutputInfo.sockAddrUSBDevice.sin_addr.s_addr = inet_addr(s_VideoUSBOutputInfo.szIPUSBVideo);
            s_VideoUSBOutputInfo.sockAddrUSBDevice.sin_port = htons( g_pControllerSettings->iVideoForwardUSBPort );
         }
         s_VideoUSBOutputInfo.usbBlockSize = g_pControllerSettings->iVideoForwardUSBPacketSize;
         s_VideoUSBOutputInfo.usbBufferPos = 0;
         s_VideoUSBOutputInfo.bVideoUSBTethering = true;
         return;
         }

         if ( s_VideoUSBOutputInfo.bVideoUSBTethering )
         if ( access(szFile, R_OK) == -1 )
         {
            log_line("[VideoOutput] Tethered USB Device for Video Output Unplugged.");
            if ( -1 != s_VideoUSBOutputInfo.socketUSBOutput )
               close(s_VideoUSBOutputInfo.socketUSBOutput);
            s_VideoUSBOutputInfo.socketUSBOutput = -1;
            s_VideoUSBOutputInfo.usbBufferPos = 0;
            s_VideoUSBOutputInfo.bVideoUSBTethering = false;
         }
      }
   }

   static u32 s_uLastTimeCheckedVideoStreamAlarm = 0;
   static u32 s_uTimeLastGeneratedVideoStreamerAlarm = 0;
   int iSemVal = 0;

   if ( (s_iPIDVideoStreamer > 1) && (s_fPipeVideoOutToStreamer > 0) )
   if ( g_TimeNow > s_uLastTimeCheckedVideoStreamAlarm + 500 )
   {
      s_uLastTimeCheckedVideoStreamAlarm = g_TimeNow;
      if ( is_semaphore_signaled_clear(s_pSemaphoreVideoStreamerOverloadAlarm, SEMAPHORE_VIDEO_STREAMER_OVERLOAD) )
      {
         log_line("[VideoOutput] Video alarm from video streamer semaphore is signaled. Clear it. Value: %d", iSemVal);
         if ( g_TimeNow > s_uTimeLastGeneratedVideoStreamerAlarm + 5000 )
         {
            s_uTimeLastGeneratedVideoStreamerAlarm = g_TimeNow;
            send_alarm_to_central(ALARM_ID_CONTROLLER_IO_ERROR, ALARM_FLAG_IO_ERROR_VIDEO_MPP_DECODER_STALLED, 0);
         }
      }
   }
}
