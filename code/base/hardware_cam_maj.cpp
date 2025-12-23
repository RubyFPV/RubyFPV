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

#include "base.h"
#include "hardware_cam_maj.h"
#include "hardware_camera.h"
#include "hardware_procs.h"
#include "hardware_files.h"
#include "../common/string_utils.h"
#include <math.h>
#include <pthread.h>

static int s_iPIDMajestic = 0;
static u32 s_uMajesticLastChangeTime = 0;
static u32 s_uMajesticLastChangeAudioTime = 0;

pthread_t s_ThreadMajLogEntry;

static Model* s_pCurrentMajesticModel = NULL;
static camera_profile_parameters_t s_CurrentMajesticCamSettings;
static video_parameters_t s_CurrentMajesticVideoParams;
static int s_iCurrentMajesticVideoProfile = -1;

static int s_iLastMajesticIRFilterMode = -5;
static int s_iLastMajesticDaylightMode = -5;
static int s_iCurrentMajesticNALSize = 0;

static float s_fCurrentMajesticGOP = -1.0;
static float s_fTemporaryMajesticGOP = -1.0;
static int s_iCurrentMajesticKeyframeMs = 0;
static int s_iTemporaryMajesticKeyframeMs = 0;

static u32 s_uCurrentMajesticBitrate = 0;
static u32 s_uTemporaryMajesticBitrate = 0;

static int s_iCurrentMajesticQPDelta = -1000;
static int s_iTemporaryMajesticQPDelta = -1000;

static int s_iCurrentMajAudioVolume = 0;
static int s_iCurrentMajAudioBitrate = 0;


#define MAX_MAJESTIC_COMMAND_LENGTH 64
#define MAX_MAJESTIC_COMMANDS 48
typedef struct
{
   u32 uCommandNumber;
   bool bSignalMajestic;
   char szCommand[MAX_MAJESTIC_COMMAND_LENGTH];
} t_majestic_command;

typedef struct
{
   t_majestic_command commands[MAX_MAJESTIC_COMMANDS];
   int iNextCommandIndexToFill;
   u32 uNextCommandNumber;
} t_majestic_command_queue;

static pthread_t s_pThreadSetMajesticParams;
static volatile bool s_bThreadSetMajesticParamsRunning = false;
static pthread_mutex_t s_MutexSetMajesticParams = PTHREAD_MUTEX_INITIALIZER;
static sem_t* s_pSemaphoreSetMajesticParamsWrite = NULL;
static sem_t* s_pSemaphoreSetMajesticParamsRead = NULL;
static t_majestic_command_queue s_MajesticSetParamsCommandsQueue;


int _execute_maj_command_wait(const char* szCommand)
{
   //return hw_execute_bash_command(szCommand, NULL);
   return hw_execute_process_wait(szCommand);
}

int _hardware_maj_send_req(char *szCommand)
{
   struct sockaddr_in addr;
   char szRequest[256];

   int iSock = socket(AF_INET, SOCK_STREAM, 0);
   if ( iSock < 0 )
   {
      log_softerror_and_alarm("[HwCamMajestic] Failed to open command socket to majestic.");
      return -1;
   }

   struct timeval tv = { .tv_sec = 0, .tv_usec = 300000 }; // 300ms timeout
   setsockopt(iSock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

   addr.sin_family = AF_INET;
   addr.sin_port = htons(80);
   addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

   int iRes = connect(iSock, (struct sockaddr *)&addr, sizeof(addr));
   if ( iRes < 0 )
   {
       log_softerror_and_alarm("[HwCamMajestic] Failed to connect to commands majestic.");
       close(iSock);
       return -1;
   }

   char* szTmp = strstr(szCommand, "/api");
   if ( NULL == szTmp )
   {
      log_softerror_and_alarm("[HwCamMajestic] Failed to parse majestic command.");
      close(iSock);
      return -1;
   }

   snprintf(szRequest, sizeof(szRequest)/sizeof(szRequest[0]), "GET %s HTTP/1.0\r\n\r\n", szTmp);

   iRes = send(iSock, szRequest, strlen(szRequest), 0);
   if ( iRes < 0 )
   {
      log_softerror_and_alarm("[HwCamMajestic] Failed to send majestic command.");
      close(iSock);
      return -1;
   }

   /*
   char szBuff[258];
   iRes = read(iSock, szBuff, 255);
   if ( iRes > 0 )
   {
      szBuff[iRes] = 0;
   }
   */
   close(iSock);
   return 0;
}


void* _thread_set_majestic_params_async(void *argument)
{
   sched_yield();
   hw_log_current_thread_attributes("[HwCamMajesticThread]");

   t_majestic_command_queue localCommandsQueue;
   int iNextCommandIndexToExecute = 0;
   u32 uLastCommandNumberExecuted = MAX_U32;

   while ( s_bThreadSetMajesticParamsRunning )
   {
      int iRes = sem_wait(s_pSemaphoreSetMajesticParamsRead);
      if ( 0 != iRes )
      {
         if ( ! s_bThreadSetMajesticParamsRunning )
            break;
         continue;
      }
      pthread_mutex_lock(&s_MutexSetMajesticParams);
      if ( ! s_bThreadSetMajesticParamsRunning )
      {
         pthread_mutex_unlock(&s_MutexSetMajesticParams);
         break;
      }
      memcpy((u8*)(&localCommandsQueue), (u8*)(&s_MajesticSetParamsCommandsQueue), sizeof(t_majestic_command_queue));
      pthread_mutex_unlock(&s_MutexSetMajesticParams);
      bool bSignalMajestic = false;
      while ( iNextCommandIndexToExecute != localCommandsQueue.iNextCommandIndexToFill )
      {
         if ( (uLastCommandNumberExecuted == localCommandsQueue.commands[iNextCommandIndexToExecute].uCommandNumber) || (localCommandsQueue.commands[iNextCommandIndexToExecute].szCommand[0] == 0) )
         {
             iNextCommandIndexToExecute = (iNextCommandIndexToExecute + 1) % MAX_MAJESTIC_COMMANDS;
             continue;
         }
         if ( 0 != strncmp(localCommandsQueue.commands[iNextCommandIndexToExecute].szCommand, "curl", 4) )
            hw_execute_process_wait(localCommandsQueue.commands[iNextCommandIndexToExecute].szCommand);

         if ( 0 != s_iPIDMajestic )
         if ( 0 == strncmp(localCommandsQueue.commands[iNextCommandIndexToExecute].szCommand, "curl", 4) )
            _hardware_maj_send_req(localCommandsQueue.commands[iNextCommandIndexToExecute].szCommand);
         
         //if ( 0 != strstr(localCommandsQueue.commands[iNextCommandIndexToExecute].szCommand, "bitrate") )
         //    log_line("[HwCamMajestic] Video throughtput updated maj bitrate to: [%s]", &localCommandsQueue.commands[iNextCommandIndexToExecute].szCommand[10]);
         bSignalMajestic |= localCommandsQueue.commands[iNextCommandIndexToExecute].bSignalMajestic;
         iNextCommandIndexToExecute = (iNextCommandIndexToExecute + 1) % MAX_MAJESTIC_COMMANDS;
      }
      if ( bSignalMajestic )
         hw_execute_bash_command_raw("killall -1 majestic", NULL);
   }

   log_line("[HwCamMajesticThread] Thread ended.");
   return NULL;
}

void _add_maj_command_to_queue(const char* szCommand, bool bSignalMajestic)
{
   if ( (NULL == szCommand) || (0 == szCommand[0]) )
      return;

   pthread_mutex_lock(&s_MutexSetMajesticParams);
   strncpy(s_MajesticSetParamsCommandsQueue.commands[s_MajesticSetParamsCommandsQueue.iNextCommandIndexToFill].szCommand, szCommand, MAX_MAJESTIC_COMMAND_LENGTH);
   s_MajesticSetParamsCommandsQueue.commands[s_MajesticSetParamsCommandsQueue.iNextCommandIndexToFill].bSignalMajestic = bSignalMajestic;
   s_MajesticSetParamsCommandsQueue.commands[s_MajesticSetParamsCommandsQueue.iNextCommandIndexToFill].uCommandNumber = s_MajesticSetParamsCommandsQueue.uNextCommandNumber;
   s_MajesticSetParamsCommandsQueue.uNextCommandNumber++;
   s_MajesticSetParamsCommandsQueue.iNextCommandIndexToFill = (s_MajesticSetParamsCommandsQueue.iNextCommandIndexToFill + 1) % MAX_MAJESTIC_COMMANDS;
   pthread_mutex_unlock(&s_MutexSetMajesticParams);
   if ( 0 != sem_post(s_pSemaphoreSetMajesticParamsWrite) )
      log_softerror_and_alarm("Failed to signal semaphore for executing majestic commands.");
}

void _add_maj_command_to_queue_or_exec(const char* szCommand, bool bSignalMajestic)
{
   if ( (0 != s_iPIDMajestic) && s_bThreadSetMajesticParamsRunning )
      _add_maj_command_to_queue(szCommand, bSignalMajestic);
   else
      _execute_maj_command_wait(szCommand);
}

void hardware_camera_maj_init_threads(Model* pModel)
{
   if ( s_bThreadSetMajesticParamsRunning )
   {
      log_line("[HwCamMajestic] Settings thread already running.");
      return;
   }

   memset((u8*) &s_MajesticSetParamsCommandsQueue, 0, sizeof(t_majestic_command_queue));
   s_MajesticSetParamsCommandsQueue.iNextCommandIndexToFill = 0;

   s_pSemaphoreSetMajesticParamsWrite = sem_open(SEMAPHORE_SET_MAJESTIC_PARAMS_ASYNC, O_CREAT | O_RDWR, S_IWUSR | S_IRUSR, 0);
   if ( (NULL == s_pSemaphoreSetMajesticParamsWrite) || (SEM_FAILED == s_pSemaphoreSetMajesticParamsWrite) )
   {
      log_error_and_alarm("[HwCamMajestic] Failed to create write semaphore: %s, try alternative.", SEMAPHORE_SET_MAJESTIC_PARAMS_ASYNC);
      s_pSemaphoreSetMajesticParamsWrite = sem_open(SEMAPHORE_SET_MAJESTIC_PARAMS_ASYNC, O_CREAT, S_IWUSR | S_IRUSR, 0); 
      if ( (NULL == s_pSemaphoreSetMajesticParamsWrite) || (SEM_FAILED == s_pSemaphoreSetMajesticParamsWrite) )
      {
         log_error_and_alarm("[HwCamMajestic] Failed to create write semaphore: %s", SEMAPHORE_SET_MAJESTIC_PARAMS_ASYNC);
         s_pSemaphoreSetMajesticParamsWrite = NULL;
         return;
      }
      log_line("[HwCamMajestic] Opened semaphore for signaling set majestic params: (%s)", SEMAPHORE_SET_MAJESTIC_PARAMS_ASYNC);
   }
   else
      log_line("[HwCamMajestic] Opened semaphore for signaling set majestic params: (%s)", SEMAPHORE_SET_MAJESTIC_PARAMS_ASYNC);

   s_pSemaphoreSetMajesticParamsRead = sem_open(SEMAPHORE_SET_MAJESTIC_PARAMS_ASYNC, O_RDWR);
   if ( (NULL == s_pSemaphoreSetMajesticParamsRead) || (SEM_FAILED == s_pSemaphoreSetMajesticParamsRead) )
   {
      log_error_and_alarm("[HwCamMajestic] Failed to create read semaphore: %s, try alternative.", SEMAPHORE_SET_MAJESTIC_PARAMS_ASYNC);
      s_pSemaphoreSetMajesticParamsRead = sem_open(SEMAPHORE_SET_MAJESTIC_PARAMS_ASYNC, O_CREAT, S_IWUSR | S_IRUSR, 0); 
      if ( (NULL == s_pSemaphoreSetMajesticParamsRead) || (SEM_FAILED == s_pSemaphoreSetMajesticParamsRead) )
      {
         log_error_and_alarm("[HwCamMajestic] Failed to create read semaphore: %s", SEMAPHORE_SET_MAJESTIC_PARAMS_ASYNC);
         s_pSemaphoreSetMajesticParamsRead = NULL;
         return;
      }
      else
         log_line("[HwCamMajestic] Opened semaphore for checking set majestic params: (%s)", SEMAPHORE_SET_MAJESTIC_PARAMS_ASYNC);
   }
   else
      log_line("[HwCamMajestic] Opened semaphore for checking set majestic params: (%s)", SEMAPHORE_SET_MAJESTIC_PARAMS_ASYNC);

   int iSemVal = 0;
   if ( 0 == sem_getvalue(s_pSemaphoreSetMajesticParamsRead, &iSemVal) )
      log_line("[HwCamMajestic] Semaphore set majestic params initial value: %d", iSemVal);
   else
      log_softerror_and_alarm("[HwCamMajestic] Failed to get semaphore set majestic params initial value.");

   pthread_attr_t attr;
   int iCoreAff = CORE_AFFINITY_OTHERS_OIPC;
   if ( NULL != pModel )
   {
      if ( pModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_AFFINITY_CORES )
         iCoreAff = pModel->processesPriorities.iCoreOthers;
      else
         iCoreAff = -1;
   }
   /*
   int iPrio = g_pCurrentModel->processesPriorities.iThreadPriorityOthers;
   if ( ! (g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS) )
      iPrio = -1;
   */
   int iPrio = -1;

   if ( (iPrio > 1) && (iPrio < 100) )
      hw_init_worker_thread_attrs(&attr, iCoreAff, 32000, SCHED_FIFO, iPrio, "[HwCamMajesticThread]");
   else
      hw_init_worker_thread_attrs(&attr, iCoreAff, 32000, SCHED_OTHER, 0, "[HwCamMajesticThread]");

   if ( 0 != pthread_create(&s_pThreadSetMajesticParams, &attr, &_thread_set_majestic_params_async, NULL) )
   {
      s_bThreadSetMajesticParamsRunning = false;
      log_softerror_and_alarm("[HwCamMajestic] Failed to create thread to set settings.");
   }
   else
   {
      s_bThreadSetMajesticParamsRunning = true;
      log_line("[HwCamMajestic] Started thread to set settings.");
   }
   pthread_attr_destroy(&attr);
}

void hardware_camera_maj_stop_threads()
{
   if ( ! s_bThreadSetMajesticParamsRunning )
      log_line("[HwCamMajestic] Thread to set settings is not running. Nothing to stop.");
   else
   {
      pthread_mutex_lock(&s_MutexSetMajesticParams);
      s_bThreadSetMajesticParamsRunning = false;
      pthread_mutex_unlock(&s_MutexSetMajesticParams);

      if ( (NULL != s_pSemaphoreSetMajesticParamsWrite) && (0 != sem_post(s_pSemaphoreSetMajesticParamsWrite)) )
         log_softerror_and_alarm("[HwCamMajestic] Failed to signal semaphore for quiting majestic set params thread.");

      hardware_sleep_ms(50);
   }

   if ( NULL != s_pSemaphoreSetMajesticParamsWrite )
      sem_close(s_pSemaphoreSetMajesticParamsWrite);
   if ( NULL != s_pSemaphoreSetMajesticParamsRead )
      sem_close(s_pSemaphoreSetMajesticParamsRead);
   s_pSemaphoreSetMajesticParamsWrite = NULL;
   s_pSemaphoreSetMajesticParamsRead = NULL;
   sem_unlink(SEMAPHORE_SET_MAJESTIC_PARAMS_ASYNC);

   log_line("[HwCamMajestic] Stopped thread to set settings.");
}

int hardware_camera_maj_validate_config()
{
   if ( (access("/etc/majestic.yaml", R_OK) != -1) && (hardware_file_get_file_size("/etc/majestic.yaml") > 200) )
   {
      log_line("[HwCamMajestic] majestic config file is ok.");
      if ( access("/etc/majestic.yaml.org", R_OK == -1) || (hardware_file_get_file_size("/etc/majestic.yaml.org") < 200) )
      {
         log_softerror_and_alarm("[HwCamMajestic] Failed to find majestic backup config file. Restore it.");
         hw_execute_bash_command("cp -rf /etc/majestic.yaml /etc/majestic.yaml.org", NULL);
         if ( access("/etc/majestic.yaml.org", R_OK == -1) || (hardware_file_get_file_size("/etc/majestic.yaml.org") < 200) )
            log_softerror_and_alarm("[HwCamMajestic] Failed to restore majestic backup config file.");
      }
      else
         log_line("[HwCamMajestic] majestic backup config file is ok.");
      return 0;
   }
  
   
   log_softerror_and_alarm("[HwCamMajestic] Invalid majestic config file. Restore it...");
   if ( access("/etc/majestic.yaml.org", R_OK == -1) || (hardware_file_get_file_size("/etc/majestic.yaml.org") < 200) )
   {
      log_error_and_alarm("[HwCamMajestic] Invalid majestic config file and no backup present. Abort start.");
      return -1;
   }
   hw_execute_bash_command("cp -rf /etc/majestic.yaml.org /etc/majestic.yaml", NULL);
   if ( access("/etc/majestic.yaml", R_OK == -1) || (hardware_file_get_file_size("/etc/majestic.yaml") < 200) )
   {
      log_error_and_alarm("[HwCamMajestic] Failed to restore majestic config file. Abort start.");
      return -1;
   }
   log_line("[HwCamMajestic] Restored majestic config file from backup.");
   return 0;
}
   
void* _thread_majestic_log_entry(void *argument)
{
   char* szLog = (char*)argument;
   if ( NULL == szLog )
      return NULL;
   char szComm[256];
   sprintf(szComm, "echo \"----------------------------------------------\" >> %s", CONFIG_FILE_FULLPATH_MAJESTIC_LOG);
   hw_execute_bash_command_raw_silent(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "echo \"Ruby: %s\" >> %s", szLog, CONFIG_FILE_FULLPATH_MAJESTIC_LOG);
   hw_execute_bash_command_raw_silent(szComm, NULL);
   free(szLog);
   return NULL;
}

void hardware_camera_maj_add_log(const char* szLog, bool bAsync)
{
   /*
   if ( NULL == szLog )
      return;
   if ( !bAsync )
   {
      char szComm[256];
      sprintf(szComm, "echo \"----------------------------------------------\" >> %s", CONFIG_FILE_FULLPATH_MAJESTIC_LOG);
      hw_execute_bash_command_raw_silent(szComm, NULL);
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "echo \"Ruby: %s\" >> %s", szLog, CONFIG_FILE_FULLPATH_MAJESTIC_LOG);
      hw_execute_bash_command_raw_silent(szComm, NULL);
      return;
   }

   char* pszLog = (char*)malloc(strlen(szLog)+1);
   if ( NULL == pszLog )
      return;
   strcpy(pszLog, szLog);
   pthread_attr_t attr;
   hw_init_worker_thread_attrs(&attr);
   pthread_create(&s_ThreadMajLogEntry, &attr, &_thread_majestic_log_entry, pszLog);
   pthread_attr_destroy(&attr);
   */
}

int hardware_camera_maj_get_current_pid()
{
   return s_iPIDMajestic;
}

bool hardware_camera_maj_start_capture_program(bool bEnableLog)
{
   if ( 0 != s_iPIDMajestic )
      return true;

   char szOutput[1024];
   char szComm[256];

   int iStartCount = 4;
   while ( iStartCount > 0 )
   {
      iStartCount--;
      if ( bEnableLog )
         sprintf(szComm, "/usr/bin/majestic -s 2>/dev/null 1>%s &", CONFIG_FILE_FULLPATH_MAJESTIC_LOG);
      else
         sprintf(szComm, "/usr/bin/majestic -s 2>/dev/null 1>/dev/null &");

      hw_execute_bash_command_raw(szComm, NULL);
      hardware_sleep_ms(100);
      s_iPIDMajestic = hw_process_exists("majestic");
      int iRetries = 10;
      while ( (0 == s_iPIDMajestic) && (iRetries > 0) )
      {
         iRetries--;
         hardware_sleep_ms(50);
         s_iPIDMajestic = hw_process_exists("majestic");
      }
      if ( s_iPIDMajestic != 0 )
         return true;

      hw_execute_bash_command_raw("ps -ae | grep majestic | grep -v \"grep\"", szOutput);
      log_line("[HwCamMajestic] Found majestic PID(s): (%s)", szOutput);
      removeTrailingNewLines(szOutput);
      hw_execute_bash_command_raw("ps -ae | grep ruby_rt_vehicle | grep -v \"grep\"", szOutput);
      removeTrailingNewLines(szOutput);
      log_line("[HwCamMajestic] Found ruby PID(s): (%s)", szOutput);
      hardware_sleep_ms(500);
   }
   return false;
}

bool _hardware_camera_maj_signal_stop_capture_program(int iSignal)
{
   char szOutput[256];
   szOutput[0] = 0;
   //hw_execute_bash_command_raw("/etc/init.d/S95majestic stop", szOutput);
   hw_execute_bash_command_raw("killall -9 majestic", szOutput);

   removeTrailingNewLines(szOutput);
   log_line("[HwCamMajestic] Result of stop majestic using killall: (%s)", szOutput);
   hardware_sleep_ms(100);

   hw_process_get_pids("majestic", szOutput);
   log_line("[HwCamMajestic] Majestic PID after killall command: (%s)", szOutput);
   if ( strlen(szOutput) < 2 )
   {
      s_iPIDMajestic = 0;
      return true;    
   }


   hw_execute_bash_command_raw("killall -1 majestic", NULL);
   hardware_sleep_ms(10);
   hw_process_get_pids("majestic", szOutput);
   log_line("[HwCamMajestic] Majestic PID after killall command: (%s)", szOutput);
   if ( strlen(szOutput) < 2 )
   {
      s_iPIDMajestic = 0;
      return true;    
   }

   hw_kill_process("majestic", iSignal);
   hardware_sleep_ms(10);
 
   hw_process_get_pids("majestic", szOutput);
   log_line("[HwCamMajestic] Majestic PID after stop command: (%s)", szOutput);
   if ( strlen(szOutput) > 2 )
      return false;

   s_iPIDMajestic = 0;
   return true;
}

bool hardware_camera_maj_stop_capture_program()
{
   if ( _hardware_camera_maj_signal_stop_capture_program(-1) )
   {
      s_iPIDMajestic = 0;
      log_line("[HwCamMajestic] Stopped majestic.");
      return true;
   }
   hardware_sleep_ms(50);
   if ( _hardware_camera_maj_signal_stop_capture_program(-9) )
   {
      s_iPIDMajestic = 0;
      log_line("[HwCamMajestic] Stopped majestic.");
      return true;
   }
   hardware_sleep_ms(50);

   char szPID[256];
   szPID[0] = 0;
   hw_process_get_pids("majestic", szPID);
   log_line("[HwCamMajestic] Stopping majestic: PID after try signaling to stop: (%s)", szPID);
   int iRetry = 15;
   while ( (iRetry > 0) && (strlen(szPID) > 1) )
   {
      iRetry--;
      hardware_sleep_ms(50);
      hw_execute_bash_command_raw("killall -1 majestic", NULL);
      hardware_sleep_ms(100);
      hw_process_get_pids("majestic", szPID);
   }

   log_line("[HwCamMajestic] Init: stopping majestic (2): PID after force try stop: (%s)", szPID);
   if ( strlen(szPID) < 2 )
   {
      s_iPIDMajestic = 0;
      log_line("[HwCamMajestic] Stopped majestic.");
      return true;
   }

   hw_kill_process("majestic", -9);
   hw_process_get_pids("majestic", szPID);
   s_iPIDMajestic = atoi(szPID);
   if ( strlen(szPID) < 2 )
   {
      s_iPIDMajestic = 0;
      log_line("[HwCamMajestic] Stopped majestic.");
      return true;
   }
   log_softerror_and_alarm("[HwCamMajestic] Init: failed to stop majestic. Current majestic PID: (%s) %d", szPID, s_iPIDMajestic);
   return false;
}


int hardware_camera_maj_get_current_nal_size()
{
   return s_iCurrentMajesticNALSize;
}

void hardware_camera_maj_update_nal_size(Model* pModel)
{
   if ( NULL == pModel )
      return;
   s_pCurrentMajesticModel = pModel;
   s_iCurrentMajesticVideoProfile = s_pCurrentMajesticModel->video_params.iCurrentVideoProfile;

   // Allow room for video header important and 5 bytes for NAL header
   int iNALSize = s_pCurrentMajesticModel->video_link_profiles[s_iCurrentMajesticVideoProfile].video_data_length;
   iNALSize -= 6; // NAL delimitator+header (00 00 00 01 [aa] [bb])
   iNALSize -= sizeof(t_packet_header_video_segment_important);
   iNALSize = iNALSize - (iNALSize % 4);

   if ( iNALSize == s_iCurrentMajesticNALSize )
   {
      //log_line("[HwCamMajestic] Received req to update nal size, but it's unchanged: %d bytes", s_iCurrentMajesticNALSize);
      return;
   }
   log_line("[HwCamMajestic] Received req to update nal size, from %d bytes to %d bytes (for video profile index: %d, %s)", s_iCurrentMajesticNALSize, iNALSize, s_iCurrentMajesticVideoProfile, str_get_video_profile_name(s_iCurrentMajesticVideoProfile));

   s_uMajesticLastChangeTime = get_current_timestamp_ms();
   s_iCurrentMajesticNALSize = iNALSize;

   char szComm[256];
   sprintf(szComm, "cli -s .outgoing.naluSize %d", s_iCurrentMajesticNALSize);
   _execute_maj_command_wait(szComm);
   hardware_sleep_ms(1);
   hw_execute_bash_command_raw("killall -1 majestic", NULL);
   hardware_sleep_ms(5);
}


void _hardware_camera_maj_apply_image_settings()
{
   s_uMajesticLastChangeTime = get_current_timestamp_ms();

   char szComm[128];
   sprintf(szComm, "cli -s .image.luminance %d", s_CurrentMajesticCamSettings.brightness);
   _execute_maj_command_wait(szComm);

   sprintf(szComm, "cli -s .image.contrast %d", s_CurrentMajesticCamSettings.contrast);
   _execute_maj_command_wait(szComm);

   sprintf(szComm, "cli -s .image.saturation %d", s_CurrentMajesticCamSettings.saturation/2);
   _execute_maj_command_wait(szComm);

   sprintf(szComm, "cli -s .image.hue %d", s_CurrentMajesticCamSettings.hue);
   _execute_maj_command_wait(szComm);

   if ( s_CurrentMajesticCamSettings.uFlags & CAMERA_FLAG_OPENIPC_3A_FPV )
      strcpy(szComm, "cli -s .fpv.enabled true");
   else
      strcpy(szComm, "cli -s .fpv.enabled false");

   _execute_maj_command_wait(szComm);

   if ( s_CurrentMajesticCamSettings.flip_image )
   {
      strcpy(szComm, "cli -s .image.flip true");
      _execute_maj_command_wait(szComm);
      strcpy(szComm, "cli -s .image.mirror true");
      _execute_maj_command_wait(szComm);
   }
   else
   {
      strcpy(szComm, "cli -s .image.flip false");
      _execute_maj_command_wait(szComm);
      strcpy(szComm, "cli -s .image.mirror false");
      _execute_maj_command_wait(szComm);
   }

   if ( 0 == s_CurrentMajesticCamSettings.iShutterSpeed )
   {
      sprintf(szComm, "cli -d .isp.exposure");
      _execute_maj_command_wait(szComm);
   }
   else
   {
      int iShutterSpeed = s_CurrentMajesticCamSettings.iShutterSpeed;
      if ( (iShutterSpeed > -3) && (iShutterSpeed < 3) )
         iShutterSpeed = 3;
      if ( iShutterSpeed > 0 )
         sprintf(szComm, "cli -s .isp.exposure %.2f", (float)iShutterSpeed/1000.0);
      else
         sprintf(szComm, "cli -s .isp.exposure %.2f", -(float)iShutterSpeed/1000.0);

      // exposure is in milisec for ssc338q
      if ( hardware_board_is_sigmastar(hardware_getBoardType()) )
      {
         if ( iShutterSpeed > 0 )
            sprintf(szComm, "cli -s .isp.exposure %d", iShutterSpeed);
         else
            sprintf(szComm, "cli -s .isp.exposure %d", -iShutterSpeed);
      }
      _execute_maj_command_wait(szComm);
   }

   hardware_camera_maj_set_irfilter_off(s_CurrentMajesticCamSettings.uFlags & CAMERA_FLAG_IR_FILTER_OFF, false);
}

void _hardware_camera_maj_set_all_params()
{
   char szComm[128];

   //hw_execute_bash_command_raw("cli -s .watchdog.enabled false", NULL);
   _execute_maj_command_wait("cli -s .watchdog.enabled false");
   _execute_maj_command_wait("cli -s .system.logLevel info");
   _execute_maj_command_wait("cli -s .rtsp.enabled false");
   _execute_maj_command_wait("cli -s .video1.enabled false");
   _execute_maj_command_wait("cli -s .video0.enabled true");
   _execute_maj_command_wait("cli -s .video0.rcMode cbr");
   _execute_maj_command_wait("cli -s .isp.slowShutter disabled");

   if ( NULL != s_pCurrentMajesticModel )
      hardware_set_oipc_gpu_boost(s_pCurrentMajesticModel->processesPriorities.iFreqGPU);

   if ( s_CurrentMajesticVideoParams.iH264Slices <= 1 )
   {
      _execute_maj_command_wait("cli -s .video0.sliceUnits 0");
   }
   else
   {
      sprintf(szComm, "cli -s .video0.sliceUnits %d", s_CurrentMajesticVideoParams.iH264Slices);
      _execute_maj_command_wait(szComm);
   }

   if ( s_CurrentMajesticVideoParams.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265 )
      _execute_maj_command_wait("cli -s .video0.codec h265");
   else
      _execute_maj_command_wait("cli -s .video0.codec h264");

   sprintf(szComm, "cli -s .video0.fps %d", s_pCurrentMajesticModel->video_params.iVideoFPS);
   _execute_maj_command_wait(szComm);

   s_uCurrentMajesticBitrate = DEFAULT_VIDEO_BITRATE_OPIC_SIGMASTAR;
   if ( NULL != s_pCurrentMajesticModel )
   {
      s_pCurrentMajesticModel->getVideoProfileInitialVideoBitrate(s_pCurrentMajesticModel->video_params.iCurrentVideoProfile);
      if ( ! (s_pCurrentMajesticModel->radioRuntimeCapabilities.uFlagsRuntimeCapab & MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED) )
      {
         s_uCurrentMajesticBitrate = s_pCurrentMajesticModel->getMaxVideoBitrateForRadioDatarate(9000000, 0);
         log_line("[HwCamMajestic] Model has not negociated radio links. Use a lower target default video bitrate for video params: %.1f Mbps", (float)s_uCurrentMajesticBitrate/1000.0/1000.0);
      }
   }

   if ( s_uTemporaryMajesticBitrate > 0 )
   if ( s_uTemporaryMajesticBitrate < s_uCurrentMajesticBitrate )
      s_uCurrentMajesticBitrate = s_uTemporaryMajesticBitrate;

   sprintf(szComm, "cli -s .video0.bitrate %u", s_uCurrentMajesticBitrate/1000);
   _execute_maj_command_wait(szComm);

   sprintf(szComm, "cli -s .video0.size %dx%d", s_pCurrentMajesticModel->video_params.iVideoWidth, s_pCurrentMajesticModel->video_params.iVideoHeight);
   _execute_maj_command_wait(szComm);

   s_iCurrentMajesticQPDelta = s_pCurrentMajesticModel->video_link_profiles[s_iCurrentMajesticVideoProfile].iIPQuantizationDelta;
   if ( s_iTemporaryMajesticQPDelta > -100 )
      s_iCurrentMajesticQPDelta = s_iTemporaryMajesticQPDelta;
   sprintf(szComm, "cli -s .video0.qpDelta %d", s_iCurrentMajesticQPDelta);
   _execute_maj_command_wait(szComm);

   s_iCurrentMajesticKeyframeMs = s_pCurrentMajesticModel->getInitialKeyframeIntervalMs(s_iCurrentMajesticVideoProfile);
   s_fCurrentMajesticGOP = ((float)s_iCurrentMajesticKeyframeMs) / 1000.0;
   if ( (s_fTemporaryMajesticGOP > 0) && (s_iTemporaryMajesticKeyframeMs > 0) )
   {
      s_fCurrentMajesticGOP = s_fTemporaryMajesticGOP;
      s_iCurrentMajesticKeyframeMs = s_iTemporaryMajesticKeyframeMs;
   }
   if ( (s_fCurrentMajesticGOP < 0.05) || (s_iCurrentMajesticKeyframeMs <= 0) )
   {
      s_fCurrentMajesticGOP = 0.1;
      s_iCurrentMajesticKeyframeMs = 100;
   }

   sprintf(szComm, "cli -s .video0.gopSize %.2f", s_fCurrentMajesticGOP);
   _execute_maj_command_wait(szComm);

   _execute_maj_command_wait("cli -s .outgoing.enabled true");
   _execute_maj_command_wait("cli -s .outgoing.server udp://127.0.0.1:5600");

   // Allow room for video header important and 5 bytes for NAL header
   int iNALSize = s_pCurrentMajesticModel->video_link_profiles[s_iCurrentMajesticVideoProfile].video_data_length;
   iNALSize -= 6; // NAL delimitator+header (00 00 00 01 [aa] [bb])
   iNALSize -= sizeof(t_packet_header_video_segment_important);
   iNALSize = iNALSize - (iNALSize % 4);
   s_iCurrentMajesticNALSize = iNALSize;
   log_line("[HwCamMajestic] Set majestic NAL size to %d bytes (for video profile index: %d, %s)", iNALSize, s_iCurrentMajesticVideoProfile, str_get_video_profile_name(s_iCurrentMajesticVideoProfile));
   sprintf(szComm, "cli -s .outgoing.naluSize %d", iNALSize);
   _execute_maj_command_wait(szComm);


   u32 uNoiseLevel = s_pCurrentMajesticModel->video_link_profiles[s_iCurrentMajesticVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAGS_MASK_NOISE;
   if ( uNoiseLevel > 2 )
   {
      _execute_maj_command_wait("cli -d .fpv.noiseLevel");
      _execute_maj_command_wait("cli -d .fpv.enabled");
   }
   else
   {
      _execute_maj_command_wait("cli -s .fpv.enabled true");
      if ( uNoiseLevel == 2 )
         _execute_maj_command_wait("cli -s .fpv.noiseLevel 2");
      else if ( uNoiseLevel == 1 )
         _execute_maj_command_wait("cli -s .fpv.noiseLevel 1");
      else
         _execute_maj_command_wait("cli -s .fpv.noiseLevel 0");
   }
   hardware_camera_maj_set_daylight_off((s_CurrentMajesticCamSettings.uFlags & CAMERA_FLAG_OPENIPC_DAYLIGHT_OFF)?1:0, false);

   _hardware_camera_maj_apply_image_settings();
}


void hardware_camera_maj_apply_all_settings(Model* pModel, camera_profile_parameters_t* pCameraParams, int iVideoProfile, video_parameters_t* pVideoParams)
{
   if ( (NULL == pCameraParams) || (NULL == pModel) || (iVideoProfile < 0) || (NULL == pVideoParams) )
   {
      log_softerror_and_alarm("[HwCamMajestic] Received invalid params to set majestic settings.");
      return;
   }
   s_uMajesticLastChangeTime = get_current_timestamp_ms();

   memcpy(&s_CurrentMajesticCamSettings, pCameraParams, sizeof(camera_profile_parameters_t));
   memcpy(&s_CurrentMajesticVideoParams, pVideoParams, sizeof(video_parameters_t));
   s_pCurrentMajesticModel = pModel;
   s_iCurrentMajesticVideoProfile = iVideoProfile;

   log_line("[HwCamMajestic] Apply all settings..");

   _hardware_camera_maj_set_all_params();

   log_line("[HwCamMajestic] Applied all settings. Signal majestic to update its' settings.");
   hardware_camera_maj_add_log("Applied settings. Signal majestic to reload it's settings...", false);
   hw_execute_bash_command_raw("killall -1 majestic", NULL);
}

void _hardware_camera_maj_set_irfilter_off_sync()
{
   u32 uSubBoardType = (hardware_getBoardType() & BOARD_SUBTYPE_MASK) >> BOARD_SUBTYPE_SHIFT;
 
   if ( (uSubBoardType == BOARD_SUBTYPE_OPENIPC_UNKNOWN) ||
        (uSubBoardType == BOARD_SUBTYPE_OPENIPC_GENERIC) ||
        (uSubBoardType == BOARD_SUBTYPE_OPENIPC_GENERIC_30KQ) )
   {

   // IR cut filer off?
   if ( s_iLastMajesticIRFilterMode )
   {
      if ( hardware_board_is_sigmastar(hardware_getBoardType()) )
      {
         _add_maj_command_to_queue_or_exec("gpio set 23", false);
         _add_maj_command_to_queue_or_exec("gpio clear 24", false);
      }
      if ( (hardware_getBoardType() & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE200 )
      {
         _add_maj_command_to_queue_or_exec("gpio set 14", false);
         _add_maj_command_to_queue_or_exec("gpio clear 15", false);
      }
      if ( (hardware_getBoardType() & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE210 )
      {
         _add_maj_command_to_queue_or_exec("gpio set 13", false);
         _add_maj_command_to_queue_or_exec("gpio clear 15", false);
      }
      if ( (hardware_getBoardType() & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE300 )
      {
         _add_maj_command_to_queue_or_exec("gpio set 10", false);
         _add_maj_command_to_queue_or_exec("gpio clear 11", false);
      }
   }
   else
   {
      if ( hardware_board_is_sigmastar(hardware_getBoardType()) )
      {
         _add_maj_command_to_queue_or_exec("gpio set 24", false);
         _add_maj_command_to_queue_or_exec("gpio clear 23", false);
      }
      if ( (hardware_getBoardType() & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE200 )
      {
         _add_maj_command_to_queue_or_exec("gpio set 15", false);
         _add_maj_command_to_queue_or_exec("gpio clear 14", false);
      }
      if ( (hardware_getBoardType() & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE210 )
      {
         _add_maj_command_to_queue_or_exec("gpio set 15", false);
         _add_maj_command_to_queue_or_exec("gpio clear 13", false);
      }
      if ( (hardware_getBoardType() & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE300 )
      {
         _add_maj_command_to_queue_or_exec("gpio set 11", false);
         _add_maj_command_to_queue_or_exec("gpio clear 10", false);
      }
   }
   }
}

void hardware_camera_maj_set_irfilter_off(int iOff, bool bAsync)
{
   if ( ! hardware_board_is_openipc(hardware_getBoardType()) )
      return;
   s_uMajesticLastChangeTime = get_current_timestamp_ms();
   s_iLastMajesticIRFilterMode = iOff;
   _hardware_camera_maj_set_irfilter_off_sync();
}

void hardware_camera_maj_set_daylight_off(int iDLOff, bool bAsync)
{
   if ( !hardware_board_is_openipc(hardware_getBoardType()) )
      return;
   s_uMajesticLastChangeTime = get_current_timestamp_ms();

   if ( iDLOff == s_iLastMajesticDaylightMode )
      return;

   s_iLastMajesticDaylightMode = iDLOff;
   if ( (0 != s_iPIDMajestic) && s_bThreadSetMajesticParamsRunning )
   {
      if (s_iLastMajesticDaylightMode)
         _add_maj_command_to_queue("curl -s localhost/night/on", false);
      else 
         _add_maj_command_to_queue("curl -s localhost/night/off", false);
   }
   else
   {
   }
}

void hardware_camera_maj_set_calibration_file(int iCameraType, int iCalibrationFileType, char* szCalibrationFile)
{
   log_line("[HwCamMajestic] Set calibration file: type: %d, file: [%s], for camera type: %d (%s)", iCalibrationFileType, szCalibrationFile, iCameraType, str_get_hardware_camera_type_string(iCameraType));

   char szFileName[MAX_FILE_PATH_SIZE];
   char szComm[256];

   hw_execute_bash_command("rm -rf /etc/sensors/c_*", NULL);

   if ( (0 == iCalibrationFileType) || (0 == szCalibrationFile[0]) )
   {
      hardware_camera_set_default_oipc_calibration(iCameraType);
   }
   else
   {
      strcpy(szFileName, "c_");
      strcat(szFileName, szCalibrationFile);
      if ( NULL == strstr(szFileName, ".bin") )
         strcat(szFileName, ".bin");
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s%s /etc/sensors/%s", FOLDER_RUBY_TEMP, szFileName, szFileName);
      hw_execute_bash_command(szComm, NULL);
      hw_execute_bash_command("sync", NULL);
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cli -s .isp.sensorConfig /etc/sensors/%s", szFileName);
      hw_execute_bash_command_raw(szComm, NULL);
   }
   hw_execute_bash_command_raw("killall -1 majestic", NULL);   
}


void hardware_camera_maj_set_brightness(u32 uValue)
{
   if ( uValue == s_CurrentMajesticCamSettings.brightness )
      return;

   s_uMajesticLastChangeTime = get_current_timestamp_ms();
   s_CurrentMajesticCamSettings.brightness = uValue;

   char szComm[128];

   if ( (0 != s_iPIDMajestic) && s_bThreadSetMajesticParamsRunning )
   {
      sprintf(szComm, "curl -s localhost/api/v1/set?image.luminance=%u", s_CurrentMajesticCamSettings.brightness);
      _add_maj_command_to_queue(szComm, false);
   }
   else
   {
      sprintf(szComm, "cli -s .image.luminance %d", s_CurrentMajesticCamSettings.brightness);
      _execute_maj_command_wait(szComm);
   }
}

void hardware_camera_maj_set_contrast(u32 uValue)
{
   if ( uValue == s_CurrentMajesticCamSettings.contrast )
      return;

   s_uMajesticLastChangeTime = get_current_timestamp_ms();
   s_CurrentMajesticCamSettings.contrast = uValue;
   
   char szComm[128];
   if ( (0 != s_iPIDMajestic) && s_bThreadSetMajesticParamsRunning )
   {
      sprintf(szComm, "curl -s localhost/api/v1/set?image.contrast=%u", s_CurrentMajesticCamSettings.contrast);
      _add_maj_command_to_queue(szComm, false);
   }
   else
   {
      sprintf(szComm, "cli -s .image.contrast %d", s_CurrentMajesticCamSettings.contrast);
      _execute_maj_command_wait(szComm);
   }
}

void hardware_camera_maj_set_hue(u32 uValue)
{
   if ( uValue == s_CurrentMajesticCamSettings.hue )
      return;

   s_uMajesticLastChangeTime = get_current_timestamp_ms();
   s_CurrentMajesticCamSettings.hue = uValue;

   char szComm[128];
   if ( (0 != s_iPIDMajestic) && s_bThreadSetMajesticParamsRunning )
   {
      sprintf(szComm, "curl -s localhost/api/v1/set?image.hue=%u", s_CurrentMajesticCamSettings.hue);
      _add_maj_command_to_queue(szComm, false);
   }
   else
   {
      sprintf(szComm, "cli -s .image.hue %d", s_CurrentMajesticCamSettings.hue);
      _execute_maj_command_wait(szComm);
   }
}


void hardware_camera_maj_set_saturation(u32 uValue)
{
   if ( uValue == s_CurrentMajesticCamSettings.saturation )
      return;

   s_uMajesticLastChangeTime = get_current_timestamp_ms();
   s_CurrentMajesticCamSettings.saturation = uValue;

   char szComm[128];
   if ( (0 != s_iPIDMajestic) && s_bThreadSetMajesticParamsRunning )
   {
      sprintf(szComm, "curl -s localhost/api/v1/set?image.saturation=%u", s_CurrentMajesticCamSettings.saturation/2);
      _add_maj_command_to_queue(szComm, false);
   }
   else
   {
      sprintf(szComm, "cli -s .image.saturation %d", s_CurrentMajesticCamSettings.saturation/2);
      _execute_maj_command_wait(szComm);
   }
}

void hardware_camera_maj_set_exposure(int iValue)
{
   if ( (iValue == s_CurrentMajesticCamSettings.iShutterSpeed) || (iValue == 0) || (iValue > 100) || (iValue < -100) )
      return;

   s_uMajesticLastChangeTime = get_current_timestamp_ms();
   s_CurrentMajesticCamSettings.iShutterSpeed = iValue;

   if ( iValue < 0 )
      iValue = -iValue;

   char szComm[128];
   if ( (0 != s_iPIDMajestic) && s_bThreadSetMajesticParamsRunning )
   {
      sprintf(szComm, "curl -s localhost/api/v1/set?isp.exposure=%d", iValue);
      _add_maj_command_to_queue(szComm, false);
   }
   else
   {
      sprintf(szComm, "cli -s .isp.exposure %d", iValue);
      _execute_maj_command_wait(szComm);
   }
}

void hardware_camera_maj_set_temp_values(u32 uBitrate, int iKeyframeMs, int iQPDelta)
{
   char szComm[256];
   if ( uBitrate > 0 )
   {
      s_uTemporaryMajesticBitrate = uBitrate;
      sprintf(szComm, "cli -s .video0.bitrate %u", s_uTemporaryMajesticBitrate/1000);
      _execute_maj_command_wait(szComm);
      log_line("[HwCamMajestic] Did set temp runtime video bitrate value to: %.3f", (float)s_uTemporaryMajesticBitrate/1000.0/1000.0);
   }
   if ( iQPDelta > -100 )
   {
      s_iTemporaryMajesticQPDelta = iQPDelta;
      sprintf(szComm, "cli -s .video0.qpDelta %d", s_iTemporaryMajesticQPDelta);
      _execute_maj_command_wait(szComm);
      log_line("[HwCamMajestic] Did set temp runtime QPDelta value to: %d", s_iTemporaryMajesticQPDelta);
   }
   if ( iKeyframeMs > 0 )
   {
      s_iTemporaryMajesticKeyframeMs = iKeyframeMs;
      s_fTemporaryMajesticGOP = ((float)iKeyframeMs)/1000.0;
      sprintf(szComm, "cli -s .video0.gopSize %.2f", s_fTemporaryMajesticGOP);
      _execute_maj_command_wait(szComm);
      log_line("[HwCamMajestic] Did set temp runtime keyframe value to: %d ms", s_iTemporaryMajesticKeyframeMs);
   }
}

void hardware_camera_maj_clear_temp_values()
{
   s_fTemporaryMajesticGOP = -1;
   s_iTemporaryMajesticKeyframeMs = 0;
   s_uTemporaryMajesticBitrate = 0;
   s_iTemporaryMajesticQPDelta = -1000;
   log_line("[HwCamMajestic] Cleared temp runtime values.");
}


void hardware_camera_maj_set_keyframe(int iKeyframeMs)
{
   float fGOP = ((float)iKeyframeMs)/1000.0;

   if ( (fabs(fGOP-s_fCurrentMajesticGOP)<0.0001) && (fabs(fGOP-s_fTemporaryMajesticGOP)<0.0001) )
      return;
   if ( (s_fTemporaryMajesticGOP < 0.0001) && (fabs(fGOP-s_fCurrentMajesticGOP)<0.0001) )
      return;
   if ( fGOP <= 0.0001 )
      return;

   s_uMajesticLastChangeTime = get_current_timestamp_ms();
   s_fTemporaryMajesticGOP = fGOP;
   s_iTemporaryMajesticKeyframeMs = iKeyframeMs;

   char szComm[128];
   if ( (0 != s_iPIDMajestic) && s_bThreadSetMajesticParamsRunning )
   {
      sprintf(szComm, "curl -s localhost/api/v1/set?video0.gopSize=%.2f", s_fTemporaryMajesticGOP);
      _add_maj_command_to_queue(szComm, false);
   }
   else
   {
      sprintf(szComm, "cli -s .video0.gopSize %.2f", s_fTemporaryMajesticGOP);
      _execute_maj_command_wait(szComm);
   }
}

int hardware_camera_maj_get_current_keyframe()
{
   if ( s_iTemporaryMajesticKeyframeMs > 0 )
      return s_iTemporaryMajesticKeyframeMs;
   return s_iCurrentMajesticKeyframeMs;
}

u32 hardware_camera_maj_get_current_bitrate()
{
   if ( 0 != s_uTemporaryMajesticBitrate )
      return s_uTemporaryMajesticBitrate;
   return s_uCurrentMajesticBitrate;
}

void hardware_camera_maj_set_bitrate(u32 uBitrate)
{
   if ( (uBitrate == s_uCurrentMajesticBitrate) && (uBitrate == s_uTemporaryMajesticBitrate) )
      return;
   if ( (0 == s_uTemporaryMajesticBitrate) && (uBitrate == s_uCurrentMajesticBitrate) )
      return;
   if ( 0 == uBitrate )
      return;

   s_uMajesticLastChangeTime = get_current_timestamp_ms();
   s_uTemporaryMajesticBitrate = uBitrate;
   
   char szComm[128];
   if ( (0 != s_iPIDMajestic) && s_bThreadSetMajesticParamsRunning )
   {
      sprintf(szComm, "curl -s localhost/api/v1/set?video0.bitrate=%u", s_uTemporaryMajesticBitrate/1000);
      _add_maj_command_to_queue(szComm, false);
   }
   else
   {
      sprintf(szComm, "cli -s .video0.bitrate %u", s_uTemporaryMajesticBitrate/1000);
      _execute_maj_command_wait(szComm);
   }
}


int hardware_camera_maj_get_current_qpdelta()
{
   if ( s_iTemporaryMajesticQPDelta > -100 )
      return s_iTemporaryMajesticQPDelta;
   return s_iCurrentMajesticQPDelta;
}

void hardware_camera_maj_set_qpdelta(int iQPDelta)
{
   if ( (iQPDelta == s_iTemporaryMajesticQPDelta) && (iQPDelta == s_iCurrentMajesticQPDelta) )
      return;
   if ( (s_iTemporaryMajesticQPDelta < -100) && (iQPDelta == s_iCurrentMajesticQPDelta) )
      return;

   s_uMajesticLastChangeTime = get_current_timestamp_ms();
   s_iTemporaryMajesticQPDelta = iQPDelta;

   char szComm[128];
   if ( (0 != s_iPIDMajestic) && s_bThreadSetMajesticParamsRunning )
   {
      sprintf(szComm, "curl -s localhost/api/v1/set?video0.qpDelta=%d", s_iTemporaryMajesticQPDelta);
      _add_maj_command_to_queue(szComm, false);
   }
   else
   {
      sprintf(szComm, "cli -s .video0.qpDelta %d", s_iTemporaryMajesticQPDelta);
      _execute_maj_command_wait(szComm);
   }
}

void hardware_camera_maj_set_bitrate_and_qpdelta(u32 uBitrate, int iQPDelta)
{
   if ( (iQPDelta == s_iCurrentMajesticQPDelta) && (iQPDelta == s_iTemporaryMajesticQPDelta) &&
        (uBitrate == s_uCurrentMajesticBitrate) && (uBitrate == s_uTemporaryMajesticBitrate) )
      return;

   bool bBitrateDif = true;
   if ( (uBitrate == s_uCurrentMajesticBitrate) && (uBitrate == s_uTemporaryMajesticBitrate) )
      bBitrateDif = false;
   if ( (0 == s_uTemporaryMajesticBitrate) && (uBitrate == s_uCurrentMajesticBitrate) )
      bBitrateDif = false;

   bool bQPDiff = true;
   if ( (iQPDelta == s_iTemporaryMajesticQPDelta) && (iQPDelta == s_iCurrentMajesticQPDelta) )
      bQPDiff = false;
   if ( (s_iTemporaryMajesticQPDelta < -100) && (iQPDelta == s_iCurrentMajesticQPDelta) )
      bQPDiff = false;

   if ( (!bBitrateDif) && (!bQPDiff) )
      return;

   if ( iQPDelta < -100 )
   {
      hardware_camera_maj_set_bitrate(uBitrate);
      return;
   }
   if ( uBitrate == 0 )
   {
      hardware_camera_maj_set_qpdelta(iQPDelta);
      return;
   }

   s_uMajesticLastChangeTime = get_current_timestamp_ms();
   s_uTemporaryMajesticBitrate = uBitrate;
   s_iTemporaryMajesticQPDelta = iQPDelta;
   
   char szComm[128];
   if ( (0 != s_iPIDMajestic) && s_bThreadSetMajesticParamsRunning )
   {
      sprintf(szComm, "curl -s localhost/api/v1/set?video0.bitrate=%u", s_uTemporaryMajesticBitrate/1000);
      _add_maj_command_to_queue(szComm, false);
      sprintf(szComm, "curl -s localhost/api/v1/set?video0.qpDelta=%d", s_iTemporaryMajesticQPDelta);
      _add_maj_command_to_queue(szComm, false);
   }
   else
   {
      sprintf(szComm, "cli -s .video0.bitrate %u", s_uTemporaryMajesticBitrate/1000);
      _execute_maj_command_wait(szComm);

      sprintf(szComm, "cli -s .video0.qpDelta %d", s_iTemporaryMajesticQPDelta);
      _execute_maj_command_wait(szComm);
   }
}

u32 hardware_camera_maj_get_last_change_time()
{
   return s_uMajesticLastChangeTime;
}

void hardware_camera_maj_enable_audio(bool bEnable, int iBitrate, int iVolume)
{
   log_line("[HwCamMajestic] Enable audio: %s", bEnable?"yes":"no");

   s_uMajesticLastChangeTime = s_uMajesticLastChangeAudioTime = get_current_timestamp_ms();
   char szComm[128];

   if ( bEnable )
   {
      s_iCurrentMajAudioVolume = iVolume;
      s_iCurrentMajAudioBitrate = iBitrate;
      if ( s_iCurrentMajAudioBitrate < 4000 )
         s_iCurrentMajAudioBitrate = 4000;
      if ( s_iCurrentMajAudioBitrate >= 32000 )
         s_iCurrentMajAudioBitrate = 48000;

      _add_maj_command_to_queue_or_exec("cli -s .audio.outputEnabled false", false);
      _add_maj_command_to_queue_or_exec("cli -s .audio.codec pcm", false);
      sprintf(szComm, "cli -s .audio.srate %d", s_iCurrentMajAudioBitrate);
      _add_maj_command_to_queue_or_exec(szComm, false);
      sprintf(szComm, "cli -s .audio.volume %d", s_iCurrentMajAudioVolume);
      _add_maj_command_to_queue_or_exec(szComm, false);
      _add_maj_command_to_queue_or_exec("cli -s .audio.enabled true", true);
   }
   else
      _add_maj_command_to_queue_or_exec("cli -s .audio.enabled false", true);
   hardware_sleep_ms(10);
}

void hardware_camera_maj_set_audio_volume(int iVolume)
{
   if ( iVolume == s_iCurrentMajAudioVolume )
   {
      log_line("[HwCamMajestic] Received request to change audio volume, but it's unchanged: %d", iVolume);
      return;
   }
   s_uMajesticLastChangeTime = s_uMajesticLastChangeAudioTime = get_current_timestamp_ms();
   s_iCurrentMajAudioVolume = iVolume;
   
   char szComm[128];
   sprintf(szComm, "cli -s .audio.volume %d", s_iCurrentMajAudioVolume);
   _add_maj_command_to_queue_or_exec(szComm, true);
}

void hardware_camera_maj_set_audio_quality(int iBitrate)
{
   if ( iBitrate == s_iCurrentMajAudioBitrate )
      return;

   int iNewBitrate = iBitrate;
   if ( iNewBitrate < 4000 )
      iNewBitrate = 4000;
   if ( iNewBitrate >= 32000 )
      iNewBitrate = 48000;
   
   if ( iNewBitrate == s_iCurrentMajAudioBitrate )
   {
      log_line("[HwCamMajestic] Received request to change bitrate, but it's unchanged: %d", iNewBitrate);
      return;
   }
   
   s_uMajesticLastChangeTime = s_uMajesticLastChangeAudioTime = get_current_timestamp_ms();
   s_iCurrentMajAudioBitrate = iNewBitrate;

   char szComm[128];
   sprintf(szComm, "cli -s .audio.srate %d", s_iCurrentMajAudioBitrate);
   _add_maj_command_to_queue_or_exec(szComm, true);
}

u32  hardware_camera_maj_get_last_audio_change_time()
{
   return s_uMajesticLastChangeAudioTime;
}
