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
#include "../base/hardware_procs.h"
#include "../base/ruby_ipc.h"
#include "../base/camera_utils.h"
#include "../base/utils.h"
#include "../common/string_utils.h"

#include <errno.h>
#include <sys/stat.h>
#include <math.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "video_source_csi.h"
#include "video_sources.h"
#include "packets_utils.h"
#include "processor_relay.h"
#include "launchers_vehicle.h"
#include "events.h"
#include "timers.h"
#include "shared_vars.h"
#include "adaptive_video.h"
#include "ruby_rt_vehicle.h"

#ifdef HW_PLATFORM_RASPBERRY

pthread_t s_pThreadWatchDogVideoCapture;
bool s_bStopThreadWatchDogVideoCapture = false;
bool s_bHasThreadWatchDogVideoCapture = false;
bool s_bIsFirstCameraParamsUpdate = true;

static type_camera_parameters s_LastAppliedVeyeCameraParams;
static video_parameters_t s_LastAppliedVeyeVideoParams;

int s_fInputVideoStreamCSIPipe = -1;
char s_szInputVideoStreamCSIPipeName[128];
bool s_bInputVideoStreamCSIPipeOpenFailed = false;
u8 s_uInputVideoCSIPipeBuffer[65536];

bool s_bRequestedVideoCSICaptureRestart = false;
bool s_bVideoCSICaptureProgramStarted = false;
u32  s_uTimeToRestartVideoCapture = 0;
u32  s_uRaspiVidStartTimeMs = 0;
int s_iMsgQueueCSICommands = -1;
u32 s_uTimeMustSendRaspividBitrateRefreshAt = 0;
u32 s_uLastSetCSIVideoBitrateBPS = 0;

ParserH264 s_ParserH264CSICamera;
u32 s_uDebugTimeLastCSIVideoInputLog = 0;
u32 s_uDebugTimeIntervalCSIVideoInputLog = 1000;
u32 s_uDebugCSIInputBytes = 0;
u32 s_uDebugCSIInputReads = 0;
u32 s_uDebugCSIVideoBitrate = 0;


void _vehicle_launch_video_capture_csi_init_params()
{
   s_ParserH264CSICamera.init();

   if ( s_bIsFirstCameraParamsUpdate )
   {
      memset((u8*)&s_LastAppliedVeyeCameraParams, 0, sizeof(type_camera_parameters));
      memset((u8*)&s_LastAppliedVeyeVideoParams, 0, sizeof(video_parameters_t));
   }

   if ( g_pCurrentModel->isActiveCameraVeye() )
   {
      char szComm[1024];
      char szOutput[1024];
 
      int nBus = hardware_i2c_get_device_bus_number(I2C_DEVICE_ADDRESS_CAMERA_VEYE);
      log_line("[VideoSourceCSI] Applying VeYe camera commands to I2C bus number %d, dev address: 0x%02X", nBus, I2C_DEVICE_ADDRESS_CAMERA_VEYE);

      sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -r -f devid -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
      hw_execute_bash_command_raw(szComm, szOutput);
      log_line("[VideoSourceCSI] VEYE Camera Dev Id output: %s", szOutput);

      sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -r -f hdver -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
      hw_execute_bash_command_raw(szComm, szOutput);
      log_line("[VideoSourceCSI] VEYE Camera HW Ver output: %s", szOutput);

      video_source_csi_update_camera_params(g_pCurrentModel, g_pCurrentModel->iCurrentCamera);
      hardware_sleep_ms(200);
   }

   if ( g_pCurrentModel->isActiveCameraHDMI() )
      hardware_sleep_ms(200);
}

// Returns set video bitrate
u32 _vehicle_launch_video_capture_csi(Model* pModel, u32 uOverwriteInitialBitrate, int iOverwriteInitialKFMs)
{
   if ( NULL == pModel )
   {
      log_error_and_alarm("[VideoSourceCSI] Tried to launch the video program without a model definition.");
      return 0;
   }

   _vehicle_launch_video_capture_csi_init_params();

   log_line("[VideoSourceCSI] Computing video capture parameters for active camera, camera type: %s...", str_get_hardware_camera_type_string(pModel->getActiveCameraType()));
   log_line("[VideoSourceCSI] Last dynamic set video bitrate: %.2f Mbps", (float)video_sources_get_last_set_video_bitrate()/1000.0/1000.0);
   log_line("[VideoSourceCSI] Last dynamic set keyframe ms: %d", video_sources_get_last_set_keyframe());
   log_line("[VideoSourceCSI] Overwrite initial video bitrate: %.2f Mbps", (float)uOverwriteInitialBitrate/1000.0/1000.0);
   log_line("[VideoSourceCSI] Overwrite initial keyframe: %d ms", iOverwriteInitialKFMs);
   log_line("[VideoSourceCSI] Current video profile (%s) target video bitrate: %.2f Mbps",
      str_get_video_profile_name(g_pCurrentModel->video_params.iCurrentVideoProfile),
      (float)(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS)/1000.0/1000.0);

   char szFile[MAX_FILE_PATH_SIZE];
   char szBuff[256];
   char szCameraFlags[256];
   char szVideoFlags[256];
   char szPrefixPriority[64];

   szCameraFlags[0] = 0;
   szVideoFlags[0] = 0;
   szPrefixPriority[0] = 0;
   pModel->getCameraFlags(szCameraFlags);
   u32 uSetVideoBitrate = pModel->getVideoFlags(szVideoFlags, pModel->video_params.iCurrentVideoProfile, uOverwriteInitialBitrate, iOverwriteInitialKFMs);

   char szProcessParams[32];
   szProcessParams[0] = 0;

   if ( pModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS )
   if ( (pModel->processesPriorities.iThreadPriorityVideoCapture > 1) && (pModel->processesPriorities.iThreadPriorityVideoCapture < 100) )
      sprintf(szProcessParams, " -rawp %d", pModel->processesPriorities.iThreadPriorityVideoCapture);

   if ( pModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_AFFINITY_CORES_VIDEO_CAPTURE )
   {
      char szTmp[32];
      sprintf(szTmp, " -aff %d", (0x01 << g_pCurrentModel->processesPriorities.iCoreVideoCapture));
      strcat(szProcessParams, szTmp);
   }

   if ( pModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS )
   if ( pModel->processesPriorities.iThreadPriorityVideoCapture > 100)
      sprintf(szPrefixPriority, "nice -n %d", pModel->processesPriorities.iThreadPriorityVideoCapture - 120);

   #ifdef HW_CAPABILITY_IONICE
   if ( pModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS )
   if ( pModel->processesPriorities.ioNiceVideo > 0 )
   {
      sprintf(szPrefixPriority, "ionice -c 1 -n %d", pModel->processesPriorities.ioNiceVideo);
      if ( pModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS )
      if ( pModel->processesPriorities.iThreadPriorityVideoCapture > 100)
         snprintf(szPrefixPriority, sizeof(szPrefixPriority)/sizeof(szPrefixPriority[0]), "ionice -c 1 -n %d nice -n %d", pModel->processesPriorities.ioNiceVideo, pModel->processesPriorities.iThreadPriorityVideoCapture - 120);
   }
   #endif

   if ( pModel->isActiveCameraVeye() )
   {
      if ( pModel->camera_params[pModel->iCurrentCamera].iCameraType == CAMERA_TYPE_VEYE307 )
      {
         if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE )
            snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s %s -dbg %s -t 0 -o - &", szPrefixPriority, VIDEO_RECORDER_COMMAND_VEYE307, szVideoFlags );
         else
            snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s %s %s -t 0 -o - &", szPrefixPriority, VIDEO_RECORDER_COMMAND_VEYE307, szVideoFlags );
      }
      else
      {
         if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE )
            snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s %s -dbg %s -t 0 -o - &", szPrefixPriority, VIDEO_RECORDER_COMMAND_VEYE, szVideoFlags );
         else
            snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s %s %s -t 0 -o - &", szPrefixPriority, VIDEO_RECORDER_COMMAND_VEYE, szVideoFlags );
      }
   }
   else
   {
      if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE )
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s ./%s -dbg %s %s%s -log -t 0 -o - &", szPrefixPriority, VIDEO_RECORDER_COMMAND, szVideoFlags, szCameraFlags, szProcessParams );
      else
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s ./%s %s %s%s -t 0 -o - &", szPrefixPriority, VIDEO_RECORDER_COMMAND, szVideoFlags, szCameraFlags, szProcessParams );
   }

   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_TEMP_CURRENT_VIDEO_PARAMS);

   FILE* fd = fopen(szFile, "w");
   if ( NULL == fd )
      log_softerror_and_alarm("[VideoSourceCSI] Failed to save current video config log to file: %s", szFile);
   else
   {
      fprintf(fd, "Video Flags: %s  # ", szVideoFlags);
      fprintf(fd, "Camera Profile: %s; Camera Flags: %s", model_getCameraProfileName(pModel->camera_params[pModel->iCurrentCamera].iCurrentProfile), szCameraFlags);
      fprintf(fd, "# Process params: [%s]", szProcessParams);
      fclose(fd);
   }

   hw_execute_bash_command_nonblock(szBuff, NULL);
   return uSetVideoBitrate;
}

void _vehicle_stop_video_capture_csi(Model* pModel)
{
   video_source_csi_send_control_message(RASPIVID_COMMAND_ID_QUIT, 0,0);
   hardware_sleep_ms(20);

   if ( pModel->isActiveCameraVeye() )
   {
      hw_stop_process(VIDEO_RECORDER_COMMAND_VEYE);
      //hw_stop_process(VIDEO_RECORDER_COMMAND_VEYE307);
      //hw_stop_process(VIDEO_RECORDER_COMMAND_VEYE_SHORT_NAME);
   }
   else
      hw_stop_process(VIDEO_RECORDER_COMMAND);

   log_line("Stopped video capture process.");
   video_source_csi_flush_discard();
}

static void * _thread_watchdog_video_capture(void *ignored_argument)
{
   hw_log_current_thread_attributes("csi watchdog");
   int iCount = 0;
   while ( ! g_bQuit )
   {
      for( int i=0; i<100; i++)
      {
         hardware_sleep_ms(60);
         if ( g_bQuit || s_bStopThreadWatchDogVideoCapture )
         {
            s_bStopThreadWatchDogVideoCapture = false;
            s_bHasThreadWatchDogVideoCapture = false;
            return NULL;
         }
      }

      iCount++;

      // If video capture is not started, do nothing
      if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
      if ( ! s_bVideoCSICaptureProgramStarted )
         continue;

      // If video capture was flagged or is in the process of restarting, do not check video capture running
      if ( s_bRequestedVideoCSICaptureRestart )
         continue;

      // Check video capture program up and running
   
      if ( g_pCurrentModel->hasCamera() && (video_source_cs_get_program_start_time() > 0) && (g_TimeNow > video_source_cs_get_program_start_time()+5000) )
      if ( ! g_bVideoPaused )
      //if ( g_TimeNow > g_TimeLastVideoCaptureProgramRunningCheck + 2000 )
      {
         g_TimeLastVideoCaptureProgramRunningCheck = g_TimeNow;

         bool bProgramRunning = false;
         bool bNeedsRestart = false;
         if ( (!bProgramRunning) && hw_process_exists(VIDEO_RECORDER_COMMAND) )
            bProgramRunning = true;
         if ( (!bProgramRunning) && hw_process_exists(VIDEO_RECORDER_COMMAND_VEYE) )
            bProgramRunning = true;
         if ( (!bProgramRunning) && hw_process_exists(VIDEO_RECORDER_COMMAND_VEYE307) )
            bProgramRunning = true;
         if ( (!bProgramRunning) && hw_process_exists(VIDEO_RECORDER_COMMAND_VEYE_SHORT_NAME) )
            bProgramRunning = true;

         if ( ! bProgramRunning )
         {
            log_line("[VideoCaptureCSITh] Can't find the running video capture program. Search for it.");
            char szOutput[2048];
            hw_process_get_pids("ruby_capture_raspi", szOutput);
            removeTrailingNewLines(szOutput);
            log_line("[VideoCaptureCSITh] PID of ruby capture: [%s]", szOutput);
            hw_execute_bash_command_raw("ps -ef | grep ruby 2>&1", szOutput);
            log_line("[VideoCaptureCSITh] Current running Ruby processes: [%s]", szOutput);
            if ( NULL != strstr(szOutput, "ruby_capture_") )
            {
               log_line("[VideoCaptureCSITh] Video capture is still running. Do not restart it.");
            }
            else
            {
               log_softerror_and_alarm("[VideoCaptureCSITh] The video capture program has crashed (no process). Restarting it.");
               bNeedsRestart = true;
            }
         }
         if ( bProgramRunning )
         {
            if ( (iCount % 5) == 0 )
               log_line("[VideoCaptureCSITh] Video capture watchdog: capture is running ok.");
              
            static int s_iCheckVideoOutputBitrateCounter = 0;
            if ( relay_current_vehicle_must_send_own_video_feeds() &&
                (g_pProcessorTxVideo->getCurrentVideoBitrateAverageLastMs(2000) < 50000) )
            {
               log_softerror_and_alarm("[VideoCaptureCSITh] Current output video bitrate is less than 50 kbps!");
               s_iCheckVideoOutputBitrateCounter++;
               if ( s_iCheckVideoOutputBitrateCounter >= 3 )
               {
                  log_softerror_and_alarm("[VideoCaptureCSITh] The video capture program has crashed (no video output). Restarting it.");
                  bNeedsRestart = true;
               }
            }
            else
               s_iCheckVideoOutputBitrateCounter = 0;
         }
         if ( bNeedsRestart )
         {
            send_alarm_to_controller(ALARM_ID_VEHICLE_VIDEO_CAPTURE_RESTARTED, ALARM_FLAG_VIDEO_CAPTURE_MALFUNCTION,0, 5);
            log_line("[VideoCaptureCSITh] Signaled router main thread to restart video capture.");
            video_source_csi_request_restart_program();
         }
      }
   }
   s_bHasThreadWatchDogVideoCapture = false;
   return NULL;
}

int _video_source_csi_open(const char* szPipeName)
{
   if ( -1 != s_fInputVideoStreamCSIPipe )
      return s_fInputVideoStreamCSIPipe;

   if ( (NULL == szPipeName) || (0 == szPipeName[0]) )
   {
      log_error_and_alarm("[VideoSourceCSI] Tried to open a video input source pipe with an empty name.");
      return s_fInputVideoStreamCSIPipe;
   }
   
   strcpy(s_szInputVideoStreamCSIPipeName, szPipeName);
   log_line("[VideoSourceCSI] Opening video input stream pipe: %s", s_szInputVideoStreamCSIPipeName);

   s_bInputVideoStreamCSIPipeOpenFailed = true;

   struct stat statsBuff;
   memset(&statsBuff, 0, sizeof(statsBuff));
   int iRes = stat(s_szInputVideoStreamCSIPipeName, &statsBuff);
   if ( 0 != iRes )
   {
      log_error_and_alarm("[VideoSourceCSI] Failed to stat pipe [%s], error: %d, [%s]", s_szInputVideoStreamCSIPipeName, iRes, strerror(errno));
      return -1;
   }

   if ( ! (S_ISFIFO(statsBuff.st_mode)) )
   {
      log_error_and_alarm("[VideoSourceCSI] File [%s] is not a pipe. Not opening it.", s_szInputVideoStreamCSIPipeName);
      return -1;    
   }
   /*
   if ( access( szPipeName, R_OK ) != -1 )
   {
      log_error_and_alarm("[VideoSourceCSI] Pipe [%s] can't be found. Not opening it.", s_szInputVideoStreamCSIPipeName);
      return -1;    
   }
   */

   s_fInputVideoStreamCSIPipe = open( FIFO_RUBY_CAMERA1, O_RDONLY | RUBY_PIPES_EXTRA_FLAGS);
   if ( s_fInputVideoStreamCSIPipe < 0 )
   {
      log_error_and_alarm("[VideoSourceCSI] Failed to open video input stream: %s", s_szInputVideoStreamCSIPipeName);
      return -1;
   }

   s_bInputVideoStreamCSIPipeOpenFailed = false;
   log_line("[VideoSourceCSI] Opened video input stream: %s", s_szInputVideoStreamCSIPipeName);
   log_line("[VideoSourceCSI] Pipe read end flags: %s", str_get_pipe_flags(fcntl(s_fInputVideoStreamCSIPipe, F_GETFL)));
   
   return s_fInputVideoStreamCSIPipe;
}

void video_source_csi_flush_discard()
{
   log_line("[VideoSourceCSI] Flushing video stream input buffer (pipe)...");
   if ( -1 == s_fInputVideoStreamCSIPipe )
   {
      log_line("[VideoSourceCSI] Flushed video stream input buffer (no pipe present)");
      return;
   }
   int iCount = 0;
   int iBytes = 0;
   for( int i=0; i<200; i++ )
   {
      int iReadSize = 0;
      u8* pData = video_source_csi_read(&iReadSize, NULL);
      if ( NULL == pData )
         break;
      iCount++;
      iBytes += iReadSize;
   }
   log_line("[VideoSourceCSI] Flushed video stream input buffer (pipe) in %d reads, total %d bytes", iCount, iBytes);
}

int video_source_csi_get_buffer_size()
{
   return sizeof(s_uInputVideoCSIPipeBuffer)/sizeof(s_uInputVideoCSIPipeBuffer[0]);
}

// Returns the buffer and number of bytes read
u8* video_source_csi_read(int* piReadSize, u32* puOutTimeDataAvailable)
{
   if ( (NULL == piReadSize) )
      return NULL;

   if ( s_bRequestedVideoCSICaptureRestart )
   {
      *piReadSize = 0;
      return NULL;
   }
   struct timeval timePipeInput;
   fd_set readset;
   int iSelectResult = 0;
   static int s_iLastCameraReadTimedOutCount = 0;

   *piReadSize = 0;
   if ( -1 == s_fInputVideoStreamCSIPipe )
   {
      if ( s_bInputVideoStreamCSIPipeOpenFailed )
         _video_source_csi_open(s_szInputVideoStreamCSIPipeName);
      if ( -1 == s_fInputVideoStreamCSIPipe )
         return NULL;
   }

   // Check for exceptions first, only if we are not already getting a lot of data

   FD_ZERO(&readset);
   FD_SET(s_fInputVideoStreamCSIPipe, &readset);

   if ( s_iLastCameraReadTimedOutCount > 10 )
   {
      s_iLastCameraReadTimedOutCount = 1;
      timePipeInput.tv_sec = 0;
      timePipeInput.tv_usec = 10; // 0.01 miliseconds timeout

      iSelectResult = select(s_fInputVideoStreamCSIPipe+1, NULL, NULL, &readset, &timePipeInput);
      if ( iSelectResult > 0 )
      {
         log_softerror_and_alarm("[VideoSourceCSI] Exception on reading input pipe. Closing pipe.");
         close(s_fInputVideoStreamCSIPipe);
         s_fInputVideoStreamCSIPipe = -1;
         return NULL;
      }
   }

   // Try read

   FD_ZERO(&readset);
   FD_SET(s_fInputVideoStreamCSIPipe, &readset);

   timePipeInput.tv_sec = 0;

   if ( s_iLastCameraReadTimedOutCount )
      timePipeInput.tv_usec = 200; // 0.2 miliseconds timeout
   else
      timePipeInput.tv_usec = 300; // 0.3 miliseconds timeout

   s_iLastCameraReadTimedOutCount++;

   iSelectResult = select(s_fInputVideoStreamCSIPipe+1, &readset, NULL, NULL, &timePipeInput);
   if ( iSelectResult <= 0 )
      return s_uInputVideoCSIPipeBuffer;

   if( 0 == FD_ISSET(s_fInputVideoStreamCSIPipe, &readset) )
      return s_uInputVideoCSIPipeBuffer;

   if ( NULL != puOutTimeDataAvailable )
   {
      g_TimeNow = get_current_timestamp_ms();
      *puOutTimeDataAvailable = g_TimeNow;
   }

   s_iLastCameraReadTimedOutCount = 0;

   int iRead = read(s_fInputVideoStreamCSIPipe, s_uInputVideoCSIPipeBuffer, video_source_csi_get_buffer_size());
   if ( iRead < 0 )
   {
      log_error_and_alarm("[VideoSourceCSI] Failed to read from video input pipe, returned code: %d, error: %s. Closing pipe.", iRead, strerror(errno));
      close(s_fInputVideoStreamCSIPipe);
      s_fInputVideoStreamCSIPipe = -1;
      return NULL;
   }

   s_uDebugCSIInputBytes += iRead;
   s_uDebugCSIInputReads++;
   *piReadSize = iRead;

   return s_uInputVideoCSIPipeBuffer;
}

void _video_source_csi_open_commands_msg_queue()
{
   if ( s_iMsgQueueCSICommands > 0 )
      return;

   log_line("[VideoSourceCSI] Opening video capture commands msq queue...");
   key_t key = generate_msgqueue_key(IPC_CHANNEL_CSI_VIDEO_COMMANDS);

   if ( key < 0 )
   {
      log_softerror_and_alarm("[IPC] Failed to generate message queue key for channel %d. Error: %d, %s",
         IPC_CHANNEL_CSI_VIDEO_COMMANDS,
         errno, strerror(errno));
      return;
   }
   s_iMsgQueueCSICommands = msgget(key, IPC_CREAT | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

   if ( s_iMsgQueueCSICommands < 0 )
   {
      log_softerror_and_alarm("[IPC] Failed to create IPC message queue write endpoint for channel %d, error %d, %s",
         IPC_CHANNEL_CSI_VIDEO_COMMANDS, errno, strerror(errno));
      log_line("%d %d %d", ENOENT, EACCES, ENOTDIR);
      return;
   }

   log_line("[IPC] Opened IPC channel %d write endpoint: success, fd: %d",
      IPC_CHANNEL_CSI_VIDEO_COMMANDS, s_iMsgQueueCSICommands);
}

// Returns initial set video bitrate
u32 video_source_csi_start_program(u32 uOverwriteInitialBitrate, int iOverwriteInitialKFMs, int* pInitialKFSet)
{
   _video_source_csi_open_commands_msg_queue();

   if ( -1 == s_fInputVideoStreamCSIPipe )
       _video_source_csi_open(FIFO_RUBY_CAMERA1);

   s_bVideoCSICaptureProgramStarted = true;
   s_uLastSetCSIVideoBitrateBPS = 0;
   u32 uSetVideoBitrate = _vehicle_launch_video_capture_csi(g_pCurrentModel, uOverwriteInitialBitrate, iOverwriteInitialKFMs);

   if ( NULL != pInitialKFSet )
   {
      *pInitialKFSet = g_pCurrentModel->getInitialKeyframeIntervalMs(g_pCurrentModel->video_params.iCurrentVideoProfile);
      if ( video_sources_get_last_set_keyframe() > 0 )
         *pInitialKFSet = video_sources_get_last_set_keyframe();
      else if ( video_sources_get_last_set_keyframe() < 0 )
         *pInitialKFSet = -video_sources_get_last_set_keyframe();
   }
   s_bStopThreadWatchDogVideoCapture = false;
   s_bHasThreadWatchDogVideoCapture = false;

   pthread_attr_t attr;

   int iCoreAffinity = -1;
   if ( g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_AFFINITY_CORES )
      iCoreAffinity = g_pCurrentModel->processesPriorities.iCoreOthers;

   int iPrio = g_pCurrentModel->processesPriorities.iThreadPriorityOthers;
   if ( ! (g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS) )
      iPrio = -1;

   if ( (iPrio > 1) && (iPrio < 100) )
      hw_init_worker_thread_attrs(&attr, iCoreAffinity, -1, SCHED_FIFO, iPrio, "csi watchdog");
   else
      hw_init_worker_thread_attrs(&attr, iCoreAffinity, -1, SCHED_OTHER, 0, "csi watchdog");

   if ( 0 != pthread_create(&s_pThreadWatchDogVideoCapture, &attr, &_thread_watchdog_video_capture, NULL) )
      log_softerror_and_alarm("[VideoSourceCSI] Failed to create thread for watchdog.");
   else
   {
      s_bHasThreadWatchDogVideoCapture = true;
      log_line("[VideoSourceCSI] Created thread for watchdog.");
   }
   pthread_attr_destroy(&attr);
   s_uRaspiVidStartTimeMs = g_TimeNow;

   //s_uLastSetCSIVideoBitrateBPS = DEFAULT_VIDEO_BITRATE;
   //if ( 0 != video_sources_get_last_set_video_bitrate() )
   //   s_uLastSetCSIVideoBitrateBPS = video_sources_get_last_set_video_bitrate();
   //else if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS > 0 )
   //   s_uLastSetCSIVideoBitrateBPS = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS;
   s_uLastSetCSIVideoBitrateBPS = uSetVideoBitrate;

   u32 uDeltaMs = 500;
   s_uTimeMustSendRaspividBitrateRefreshAt = g_TimeNow + uDeltaMs;
   log_line("[VideoSourceCSI] Capture program started. Will send a bitrate refresh to it %u ms from now, for initial video bitrate of: %.3f Mbps.", uDeltaMs, (float)s_uLastSetCSIVideoBitrateBPS/1000.0/1000.0);
   return s_uLastSetCSIVideoBitrateBPS;
}

void video_source_csi_stop_program()
{
   log_line("[VideoSourceCSI] Video capture program stop procedure started...");
   s_bVideoCSICaptureProgramStarted = false;
   s_uLastSetCSIVideoBitrateBPS = 0;
   s_uRaspiVidStartTimeMs = 0;

   if ( s_bHasThreadWatchDogVideoCapture )
   {
      s_bStopThreadWatchDogVideoCapture = true;
      int iCount = 0;
      while ( s_bHasThreadWatchDogVideoCapture && (iCount < 30) )
      {
         hardware_sleep_ms(10);
         iCount++;
      }
      //pthread_cancel(s_pThreadWatchDogVideoCapture); 
   }
   log_line("[VideoSourceCSI] Stopped video watchdog thread.");

   _vehicle_stop_video_capture_csi(g_pCurrentModel);
   
   if ( s_iMsgQueueCSICommands > 0 )
   {
      msgctl(s_iMsgQueueCSICommands,IPC_RMID,NULL);
      s_iMsgQueueCSICommands = -1;
   }

   log_line("[VideoSourceCSI] Closed msgqueue to raspi video commands.");
   log_line("[VideoSourceCSI] Video capture program stop procedure completed.");

   if ( -1 != s_fInputVideoStreamCSIPipe )
   {
      log_line("[VideoSourceCSI] Closed input pipe.");
      close(s_fInputVideoStreamCSIPipe);
   }
   else
      log_line("[VideoSourceCSI] No input pipe to close.");
   s_fInputVideoStreamCSIPipe = -1;
   s_bInputVideoStreamCSIPipeOpenFailed = false;
   s_uTimeMustSendRaspividBitrateRefreshAt = 0;
}

u32 video_source_cs_get_program_start_time()
{
   return s_uRaspiVidStartTimeMs;
}

void video_source_csi_request_restart_program()
{
   s_bRequestedVideoCSICaptureRestart = true;
}

void video_source_csi_log_input_data()
{
   float fSecPassed = ((float)(g_TimeNow - s_uDebugTimeLastCSIVideoInputLog))/1000.0;
   if ( (g_TimeNow <= s_uDebugTimeLastCSIVideoInputLog+100) || (fSecPassed < 0.1) )
   {
      log_line("[VideoSourceCSI] No input video data info captured to log.");
      return;
   }
   s_uDebugCSIVideoBitrate = s_uDebugCSIInputBytes*8.0/fSecPassed;
   log_line("[VideoSourceCSI] Input video data: %.2f Mbps, %u reads (%u reads/sec) in the last %u ms",
      (float)s_uDebugCSIVideoBitrate/1000.0/1000.0, s_uDebugCSIInputReads, (u32)((float)s_uDebugCSIInputReads/fSecPassed), g_TimeNow - s_uDebugTimeLastCSIVideoInputLog);

   s_uDebugTimeLastCSIVideoInputLog = g_TimeNow;
   s_uDebugTimeIntervalCSIVideoInputLog = 1000;
   s_uDebugCSIInputBytes = 0;
   s_uDebugCSIInputReads = 0;
}

u32 video_source_csi_get_debug_videobitrate()
{
   return s_uDebugCSIVideoBitrate;
}

void video_source_csi_send_control_message(u8 parameter, u16 value1, u16 value2)
{
   if ( s_bRequestedVideoCSICaptureRestart )
   {
      log_line("[VideoSourceCSI] Video capture is flagged for restarting, do not send command (%d) to video capture program.", parameter);
      return;
   }

   if ( ! s_bVideoCSICaptureProgramStarted )
   {
      log_line("[VideoSourceCSI] Video capture is not started, do not send command (%d) to video capture program.", parameter);
      return;
   }

   if ( (NULL == g_pCurrentModel) || (! g_pCurrentModel->hasCamera()) || (-1 == s_fInputVideoStreamCSIPipe) )
   {
      log_softerror_and_alarm("[VideoSourceCSI] Tried to send a raspi CSI command with no model or pipe opened.");
      return;
   }
   #ifdef HW_PLATFORM_RASPBERRY

   if ( (! g_pCurrentModel->isActiveCameraCSICompatible()) && (! g_pCurrentModel->isActiveCameraVeye()) )
   {
      log_softerror_and_alarm("[VideoSourceCSI] Can't signal on the fly video capture parameter change. Capture camera is not raspi or veye.");
      return;
   }

   if ( parameter == RASPIVID_COMMAND_ID_KEYFRAME )
   {
   }

   if ( parameter == RASPIVID_COMMAND_ID_VIDEO_BITRATE )
   {
      video_source_csi_log_input_data();
      s_uLastSetCSIVideoBitrateBPS = ((u32)value1) * 100000; 
      s_uTimeMustSendRaspividBitrateRefreshAt = g_TimeNow+200;
   }

   _video_source_csi_open_commands_msg_queue();

   if ( s_iMsgQueueCSICommands <= 0 )
   {
      log_softerror_and_alarm("[VideoSourceCSI] Can't send video/camera command to video capture programm, no msq queue.");
      return;
   }

   static u8 s_CameraCommandNumber = 0;
   s_CameraCommandNumber++;
   if ( 0 == s_CameraCommandNumber )
      s_CameraCommandNumber++;
   
   // 6 bytes each command
   // byte 0: command counter;
   // byte 1: command type;
   // byte 2-3: command parameter 1;
   // byte 4-5: command parameter 2;
  
   typedef struct
   {
       long type;
       char data[IPC_CHANNEL_MAX_MSG_SIZE];
       // byte 0...3: CRC
       // byte 4: message type
       // byte 5..6: message data length
       // byte 7... : data
   } type_ipc_message_buffer;

   type_ipc_message_buffer msg;

   msg.type = 1;
   msg.data[4] = 0;
   msg.data[5] = 6; //length of actual data 
   msg.data[6] = 0;

   msg.data[7] = s_CameraCommandNumber;
   msg.data[8] = parameter;
   msg.data[9] = (value1 >> 8) & 0xFF;
   msg.data[10] = value1 & 0xFF;
   msg.data[11] = (value2 >> 8) & 0xFF;
   msg.data[12] = value2 & 0xFF;

   u32 uCRC = base_compute_crc32((u8*)&(msg.data[4]), 9);
   memcpy((u8*)&(msg.data[0]), (u8*)&uCRC, sizeof(u32));

   if ( 0 != msgsnd(s_iMsgQueueCSICommands, &msg, 13, IPC_NOWAIT) )
   {
      if ( errno == EAGAIN )
         log_softerror_and_alarm("[IPC] Failed to write to IPC %d, error code: EAGAIN", IPC_CHANNEL_CSI_VIDEO_COMMANDS );
      if ( errno == EACCES )
         log_softerror_and_alarm("[IPC] Failed to write to IPC %d, error code: EACCESS", IPC_CHANNEL_CSI_VIDEO_COMMANDS );
      if ( errno == EIDRM )
         log_softerror_and_alarm("[IPC] Failed to write to IPC %d, error code: EIDRM", IPC_CHANNEL_CSI_VIDEO_COMMANDS );
      if ( errno == EINVAL )
         log_softerror_and_alarm("[IPC] Failed to write to IPC %d, error code: EINVAL", IPC_CHANNEL_CSI_VIDEO_COMMANDS );
   }
   #endif
}

bool video_source_csi_periodic_health_checks()
{
   #ifdef HW_PLATFORM_RASPBERRY

   if ( 0 != s_uTimeMustSendRaspividBitrateRefreshAt )
   if ( 0 != s_uLastSetCSIVideoBitrateBPS )
   if ( (0 != s_uRaspiVidStartTimeMs) && ( g_TimeNow > s_uRaspiVidStartTimeMs + 900 ) )
   if ( g_TimeNow > s_uTimeMustSendRaspividBitrateRefreshAt )
   {
      log_line("[VideoSourceCSI] Send video bitrate refresh to capture program (to %.2f Mbps)", (float)s_uLastSetCSIVideoBitrateBPS/1000.0/1000.0);
      video_source_csi_send_control_message(RASPIVID_COMMAND_ID_VIDEO_BITRATE, s_uLastSetCSIVideoBitrateBPS/100000, 0);
      s_uLastSetCSIVideoBitrateBPS = 0;
      s_uTimeMustSendRaspividBitrateRefreshAt = 0;
   }

   if ( s_bRequestedVideoCSICaptureRestart )
   if ( 0 == s_uTimeToRestartVideoCapture )
   {
      log_line("[VideoSourceCSI] Request to restart capture flag is set. Check and restart capture.");
      if ( s_bVideoCSICaptureProgramStarted )
      if ( 0 != s_uRaspiVidStartTimeMs )
      {
         signal_start_long_op();
         video_source_csi_stop_program();
         signal_end_long_op();
      }
      g_TimeNow = get_current_timestamp_ms();
      s_uTimeToRestartVideoCapture = g_TimeNow + 50;
      if ( g_pCurrentModel->isActiveCameraHDMI() )
      {
         log_line("[VideoSourceCSI] HDMI camera detected.");
         s_uTimeToRestartVideoCapture = g_TimeNow + 1500;
      }
      log_line("[VideoSourceCSI] Set start video capture %u miliseconds from now.", s_uTimeToRestartVideoCapture - g_TimeNow);
   }

   if ( 0 != s_uTimeToRestartVideoCapture )
   if ( g_TimeNow > s_uTimeToRestartVideoCapture )
   {
      log_line("[VideoSourceCSI] It's time to start raspi video capture...");
      video_sources_start_capture();
      s_bRequestedVideoCSICaptureRestart = false;
      s_uTimeToRestartVideoCapture = 0;
      send_alarm_to_controller(ALARM_ID_VEHICLE_VIDEO_CAPTURE_RESTARTED, ALARM_FLAG_VIDEO_CAPTURE_PARAMETERS_UPDATE,0, 5);
      log_line("[VideoSourceCSI] Finished video capture restart on demand procedure.");
   }

   #endif

   if ( g_TimeNow >= s_uDebugTimeLastCSIVideoInputLog + s_uDebugTimeIntervalCSIVideoInputLog )
   {
      u32 uInterval = s_uDebugTimeIntervalCSIVideoInputLog;
      if ( uInterval < 10000 )
         uInterval += 500;
      video_source_csi_log_input_data();
      s_uDebugTimeIntervalCSIVideoInputLog = uInterval;
   }
   return false;
}

void video_source_csi_update_camera_params(Model* pModel, int iCameraIndex)
{
   if ( ! pModel->isActiveCameraVeye() )
   {
      log_line("[VideoSourceCSI] Active camera is not VeYe, can't apply camera params on the fly. Do it by restart.");
      return;
   }
   char szComm[1024];
   char szCameraFlags[512];
   szCameraFlags[0] = 0;
   pModel->getCameraFlags(szCameraFlags);
   if ( iCameraIndex < 0 || iCameraIndex >= pModel->iCameraCount )
      iCameraIndex = 0;

   int iProfile = pModel->camera_params[iCameraIndex].iCurrentProfile;
   bool bApplyAll = false;
   if ( s_LastAppliedVeyeVideoParams.iVideoWidth == 0 )
      bApplyAll = true;

   if ( s_bIsFirstCameraParamsUpdate )
   {
      memset((u8*)&s_LastAppliedVeyeCameraParams, 0, sizeof(type_camera_parameters));
      memset((u8*)&s_LastAppliedVeyeVideoParams, 0, sizeof(video_parameters_t));
      s_bIsFirstCameraParamsUpdate = false;

      if ( pModel->isActiveCameraVeye327290() )
      {
         int nBus = hardware_i2c_get_device_bus_number(I2C_DEVICE_ADDRESS_CAMERA_VEYE);
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f  videofmt -p1 NTSC -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         hw_execute_bash_command(szComm, NULL);
      }
      bApplyAll = true;
   }

   if ( pModel->isActiveCameraVeye307() )
   {
      int nBus = hardware_i2c_get_device_bus_number(I2C_DEVICE_ADDRESS_CAMERA_VEYE);
      
      if ( (s_LastAppliedVeyeVideoParams.iVideoWidth != pModel->video_params.iVideoWidth) ||
           (s_LastAppliedVeyeVideoParams.iVideoHeight != pModel->video_params.iVideoHeight) ||
           (s_LastAppliedVeyeVideoParams.iVideoFPS != pModel->video_params.iVideoFPS) )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f  videofmt -p1 %d -p2 %d -p3 %d -b %d; cd $current_dir",
          VEYE_COMMANDS_FOLDER307, pModel->video_params.iVideoWidth, pModel->video_params.iVideoHeight, pModel->video_params.iVideoFPS, nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].dayNightMode != pModel->camera_params[iCameraIndex].profiles[iProfile].dayNightMode )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].dayNightMode == 0 )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f daynightmode -p1 0x1 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
         else
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f daynightmode -p1 0x2 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].brightness != pModel->camera_params[iCameraIndex].profiles[iProfile].brightness )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f aetarget -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, (int)(2.5f*(pModel->camera_params[iCameraIndex].profiles[iProfile].brightness)), nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].contrast != pModel->camera_params[iCameraIndex].profiles[iProfile].contrast )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f contrast -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, pModel->camera_params[iCameraIndex].profiles[iProfile].contrast, nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].saturation != pModel->camera_params[iCameraIndex].profiles[iProfile].saturation )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f satu -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, (pModel->camera_params[iCameraIndex].profiles[iProfile].saturation/2), nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || (fabs(s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].hue - pModel->camera_params[iCameraIndex].profiles[iProfile].hue) > 0.1) )
      {
         int hue = pModel->camera_params[iCameraIndex].profiles[iProfile].hue;
         if ( (hue > 1) && (hue < 100) )
         {
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f hue -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, hue, nBus);
            hw_execute_bash_command(szComm, NULL);
         }
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].iShutterSpeed != pModel->camera_params[iCameraIndex].profiles[iProfile].iShutterSpeed )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].iShutterSpeed <= 0 )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f expmode -p1 0 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
         else
         {
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f expmode -p1 1 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
            hw_execute_bash_command(szComm, NULL);

            char szShutter[24];
            szShutter[0] = 0;
            if ( pModel->camera_params[iCameraIndex].profiles[iProfile].iShutterSpeed >= 30 )
               sprintf(szShutter, "%d", (int)(1000000l/(long)pModel->camera_params[iCameraIndex].profiles[iProfile].iShutterSpeed));
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f metime -p1 %s -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, szShutter, nBus);
         }
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].whitebalance != pModel->camera_params[iCameraIndex].profiles[iProfile].whitebalance )
      {
         if ( 0 == pModel->camera_params[iCameraIndex].profiles[iProfile].whitebalance )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f awbmode -p1 1 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
         else
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f awbmode -p1 0 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].flip_image != pModel->camera_params[iCameraIndex].profiles[iProfile].flip_image )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].flip_image )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f imagedir -p1 3 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
         else
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f imagedir -p1 0 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
         hw_execute_bash_command(szComm, NULL);
      }
   }
   else // IMX 327 camera
   {
      int nBus = hardware_i2c_get_device_bus_number(I2C_DEVICE_ADDRESS_CAMERA_VEYE);
      log_line("[VideoSourceCSI] Applying VeYe camera commands to I2C bus number %d, dev address: 0x%02X", nBus, I2C_DEVICE_ADDRESS_CAMERA_VEYE);
      if ( bApplyAll )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f  videofmt -p1 NTSC -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].dayNightMode != pModel->camera_params[iCameraIndex].profiles[iProfile].dayNightMode )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].dayNightMode == 0 )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f daynightmode -p1 0xFF -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         else
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f daynightmode -p1 0xFE -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].brightness != pModel->camera_params[iCameraIndex].profiles[iProfile].brightness )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f brightness -p1 0x%02X -b %d 2>&1; cd $current_dir", VEYE_COMMANDS_FOLDER, (int)(pModel->camera_params[iCameraIndex].profiles[iProfile].brightness), nBus);
         char szOutput[1024];
         szOutput[0] = 0;
         hw_execute_bash_command_raw(szComm, szOutput);
         log_line("output: [%s]", szOutput);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].contrast != pModel->camera_params[iCameraIndex].profiles[iProfile].contrast )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f contrast -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, (int)(2.5*(float)(pModel->camera_params[iCameraIndex].profiles[iProfile].contrast)), nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].saturation != pModel->camera_params[iCameraIndex].profiles[iProfile].saturation )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f saturation -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, (pModel->camera_params[iCameraIndex].profiles[iProfile].saturation/2), nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].wdr != pModel->camera_params[iCameraIndex].profiles[iProfile].wdr )
      {
         int wdrmode = (int) pModel->camera_params[iCameraIndex].profiles[iProfile].wdr;
         if ( (wdrmode < 0) || (wdrmode > 3) )
            wdrmode = 0;
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f wdrmode -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, wdrmode, nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].iShutterSpeed != pModel->camera_params[iCameraIndex].profiles[iProfile].iShutterSpeed )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].iShutterSpeed <= 0 )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f mshutter -p1 0x40 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         else
         {
            int iShutterSpeed = pModel->camera_params[iCameraIndex].profiles[iProfile].iShutterSpeed;
            char szShutter[24];
            strcpy(szShutter, "0x40");
            if ( iShutterSpeed < 50 )
               strcpy(szShutter, "0x41"); // 1/30
            else if ( iShutterSpeed < 100 )
               strcpy(szShutter, "0x42"); // 1/60
            else if ( iShutterSpeed < 200 )
               strcpy(szShutter, "0x43"); // 1/120
            else if ( iShutterSpeed < 400 )
               strcpy(szShutter, "0x44"); // 1/240
            else if ( iShutterSpeed < 700 )
               strcpy(szShutter, "0x45"); // 1/480
            else if ( iShutterSpeed < 1600 )
               strcpy(szShutter, "0x46"); // 1/1000
            else if ( iShutterSpeed < 3500 )
               strcpy(szShutter, "0x47"); // 1/2000
            else if ( iShutterSpeed < 7000 )
               strcpy(szShutter, "0x48"); // 1/5000
            else if ( iShutterSpeed < 20000 )
               strcpy(szShutter, "0x49"); // 1/10000
            else
               strcpy(szShutter, "0x4A"); // 1/50000

            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f mshutter -p1 %s -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, szShutter, nBus);
         }
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].whitebalance != pModel->camera_params[iCameraIndex].profiles[iProfile].whitebalance )
      {
         if ( 0 == pModel->camera_params[iCameraIndex].profiles[iProfile].whitebalance )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f wbmode -p1 0x1B -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         else
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f wbmode -p1 0x18 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].sharpness != pModel->camera_params[iCameraIndex].profiles[iProfile].sharpness )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].sharpness >= 100 )
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].sharpness <= 110 )
         {
            if ( pModel->camera_params[iCameraIndex].profiles[iProfile].sharpness == 100 )
               sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f sharppen -p1 0x0 -p2 0x0 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
            else
               sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f sharppen -p1 0x1 -p2 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, pModel->camera_params[iCameraIndex].profiles[iProfile].sharpness - 101, nBus);
            hw_execute_bash_command(szComm, NULL);
         }
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].drc != pModel->camera_params[iCameraIndex].profiles[iProfile].drc )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].drc<16 )
         {
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f agc -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, pModel->camera_params[iCameraIndex].profiles[iProfile].drc, nBus );
            hw_execute_bash_command(szComm, NULL);
         }
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].flip_image != pModel->camera_params[iCameraIndex].profiles[iProfile].flip_image )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].flip_image )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f mirrormode -p1 0x03 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         else
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f mirrormode -p1 0x00 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         hw_execute_bash_command(szComm, NULL);
      }
   }
   
   memcpy((u8*)&s_LastAppliedVeyeCameraParams, (u8*)&(pModel->camera_params[iCameraIndex]), sizeof(type_camera_parameters));
   memcpy((u8*)&s_LastAppliedVeyeVideoParams, (u8*)&(pModel->video_params), sizeof(video_parameters_t));
}

#else

void video_source_csi_log_input_data() {}
u32 video_source_csi_get_debug_videobitrate() {return 0;}
void video_source_csi_flush_discard() {}
int video_source_csi_get_buffer_size() {return 0;}
u8* video_source_csi_read(int* piReadSize, u32* puOutTimeDataAvailable)
{
   if ( NULL != piReadSize )
      *piReadSize = 0;
   return NULL;
}
u32 video_source_csi_start_program(u32 uOverwriteInitialBitrate, int iOverwriteInitialKFMs, int* pInitialKFSet) { return 0; }
void video_source_csi_stop_program() {}
u32 video_source_cs_get_program_start_time() { return 0; }
void video_source_csi_request_restart_program() {}
void video_source_csi_send_control_message(u8 parameter, u16 value1, u16 value2) {}
bool video_source_csi_periodic_health_checks() { return false; }

void video_source_csi_update_camera_params(Model* pModel, int iCameraIndex) {}

#endif