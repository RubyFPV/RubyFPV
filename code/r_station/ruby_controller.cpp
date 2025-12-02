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
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/gpio.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../base/shared_mem.h"

#include "shared_vars.h"
#include "timers.h"

shared_mem_process_stats* s_pProcessStatsCentral = NULL; 

void try_open_process_stats()
{
   if ( NULL == s_pProcessStatsCentral )
   {
      s_pProcessStatsCentral = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_CENTRAL);
      if ( NULL == s_pProcessStatsCentral )
         log_softerror_and_alarm("Failed to open shared mem to ruby central process watchdog for reading: %s", SHARED_MEM_WATCHDOG_CENTRAL);
      else
         log_line("Opened shared mem to ruby central process watchdog for reading.");
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

   log_init("RubyController");

   hardware_detectBoardAndSystemType();

   load_ControllerSettings();
   ControllerSettings* pCS = get_ControllerSettings();

   if ( pCS->iCoresAdjustment )
      hw_set_current_thread_affinity("ruby_controller", CORE_AFFINITY_OTHERS, CORE_AFFINITY_OTHERS);

   char szPrefix[256];
   szPrefix[0] = 0;

   if ( pCS->iPrioritiesAdjustment && (pCS->iThreadPriorityCentral > 100) )
      sprintf(szPrefix, "nice -n %d", pCS->iThreadPriorityCentral - 120);    

   hw_execute_ruby_process(szPrefix, "ruby_central", NULL, NULL);
   
   log_line("Executed central process.");
   
   for( int i=0; i<5; i++ )
      hardware_sleep_ms(800);
   
   char szComm[MAX_FILE_PATH_SIZE];
   u32 uSleepIntervalMs = 500;
   u32 maxTimeForProcess = 9000;
   int counter = 0;

   log_line("Enter watchdog state");
   log_line_watchdog("Watchdog state started.");

   sem_t* pSemaphoreRestart = sem_open(SEMAPHORE_WATCHDOG_CONTROLLER_RESTART, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( (NULL == pSemaphoreRestart) || (SEM_FAILED == pSemaphoreRestart) )
      log_softerror_and_alarm("Failed to open semaphore for signaling restart: %s; error: %d (%s)", SEMAPHORE_WATCHDOG_CONTROLLER_RESTART, errno, strerror(errno));
   else
      log_line("Opened semaphore for signaling restart.");


   sem_t* pSemaphorePause = sem_open(SEMAPHORE_WATCHDOG_CONTROLLER_PAUSE, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( (NULL == pSemaphorePause) || (SEM_FAILED == pSemaphorePause) )
      log_softerror_and_alarm("Failed to open semaphore for signaling pause: %s; error: %d (%s)", SEMAPHORE_WATCHDOG_CONTROLLER_PAUSE, errno, strerror(errno));
   else
      log_line("Opened semaphore for signaling pause.");

   sem_t* pSemaphoreResume = sem_open(SEMAPHORE_WATCHDOG_CONTROLLER_RESUME, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( (NULL == pSemaphoreResume) || (SEM_FAILED == pSemaphoreResume) )
      log_softerror_and_alarm("Failed to open semaphore for signaling resume: %s; error: %d (%s)", SEMAPHORE_WATCHDOG_CONTROLLER_RESUME, errno, strerror(errno));
   else
      log_line("Opened semaphore for signaling resume.");

   bool bWatchdogPaused = false;
   int iCountPauses = 0;
   g_TimeNow = get_current_timestamp_ms();
   u32 uTimeLastPaused = g_TimeNow;

   while ( ! g_bQuit )
   {
      u32 uTimeStart = get_current_timestamp_ms();

      hardware_sleep_ms(uSleepIntervalMs);

      counter++;
      if ( (counter % 4) != 0 )
      {
         u32 dTime = get_current_timestamp_ms() - uTimeStart;
         if ( dTime > (u32)uSleepIntervalMs + 20 )
            log_softerror_and_alarm("Main processing loop took too long (%u ms).", dTime);
         continue;
      }
      
      // Executes once every 2 seconds

      g_TimeNow = get_current_timestamp_ms();

      if ( g_TimeNow < uTimeLastPaused + 5000 )
         continue;

      if ( is_semaphore_signaled_clear(pSemaphoreRestart, SEMAPHORE_WATCHDOG_CONTROLLER_RESTART) )
      {
         log_line("Signaled to restart processes.");

         load_ControllerSettings();
         pCS = get_ControllerSettings();
         if ( pCS->iCoresAdjustment )
            hw_set_current_thread_affinity("ruby_controller", CORE_AFFINITY_OTHERS, CORE_AFFINITY_OTHERS);
         else
            hw_set_current_thread_affinity("ruby_controller", 0, hw_procs_get_cpu_count()-1);

         shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_CENTRAL, s_pProcessStatsCentral);   
         s_pProcessStatsCentral = NULL;

         char szPrefix[256];
         szPrefix[0] = 0;

         if ( pCS->iPrioritiesAdjustment && (pCS->iThreadPriorityCentral > 100) )
            sprintf(szPrefix, "nice -n %d", pCS->iThreadPriorityCentral - 120);    
         hw_execute_ruby_process(szPrefix, "ruby_central", "-nointro", NULL);
         log_line_watchdog("Restarting ruby_central process done.");
         uTimeLastPaused = g_TimeNow;
         continue;
      }

      if ( NULL == s_pProcessStatsCentral )
         try_open_process_stats();

      static u32 s_uTimeLastClearScreen = 0;
      static int s_iClearScreenCounter = 0;
      if ( s_iClearScreenCounter < 1 )
      if ( g_TimeNow > s_uTimeLastClearScreen + 20000 )
      {
         s_iClearScreenCounter++;
         s_uTimeLastClearScreen = g_TimeNow;
         system("clear");
      }

      if ( is_semaphore_signaled_clear(pSemaphorePause, SEMAPHORE_WATCHDOG_CONTROLLER_PAUSE) )
      {
         bWatchdogPaused = true;
         iCountPauses++;
         log_line("Watchdog is paused (pause counter: %d)", iCountPauses);
      }
      if ( is_semaphore_signaled_clear(pSemaphoreResume, SEMAPHORE_WATCHDOG_CONTROLLER_RESUME) )
      {
         bWatchdogPaused = false;
         iCountPauses--;
         log_line("Watchdog is resumed (pause counter: %d)", iCountPauses);
      }
      if ( bWatchdogPaused )
      {
         log_line("Watchdog is paused.");
         u32 dTime = get_current_timestamp_ms() - uTimeStart;
         if ( dTime > (u32)uSleepIntervalMs + 20 )
            log_softerror_and_alarm("Main processing loop took too long (%u ms).", dTime);
         uTimeLastPaused = g_TimeNow;
         continue;
      }

      bool bMustRestart = false;
      char szTime[64];
      if ( NULL != s_pProcessStatsCentral )
      {
         if ( (g_TimeNow > maxTimeForProcess) && (s_pProcessStatsCentral->lastActiveTime + maxTimeForProcess < g_TimeNow ) )
         {
            log_format_time(s_pProcessStatsCentral->lastActiveTime, szTime);
            log_line_watchdog("Ruby controller watchdog check failed: ruby_central process has stopped !!! Last active time: %s", szTime);
            bMustRestart = true;
         }
      }

      u32 dTime = get_current_timestamp_ms() - uTimeStart;
      if ( dTime > (u32)uSleepIntervalMs + 20 )
         log_softerror_and_alarm("Main processing loop took too long (%u ms).", dTime);

      if ( bMustRestart )
      {
         for( int i=0; i<1000; i++ )
            hardware_sleep_ms(500);
           
         log_line_watchdog("Restarting ruby_central process...");

         hw_stop_process("ruby_central");
         shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_CENTRAL, s_pProcessStatsCentral);   
         s_pProcessStatsCentral = NULL;

         sprintf(szComm, "touch %s%s", FOLDER_CONFIG, FILE_TEMP_CONTROLLER_CENTRAL_CRASHED);
         hw_execute_bash_command(szComm, NULL);
   
         hardware_sleep_ms(200);

         hw_execute_bash_command("./ruby_central -reinit &", NULL);
         for( int i=0; i<3; i++ )
            hardware_sleep_ms(800);
   
         while ( hw_process_exists("ruby_central") )
         {
            log_line("Ruby reinit still in progress...");
            hardware_sleep_ms(200);
         }
         hardware_sleep_ms(200);

         char szPrefix[256];
         szPrefix[0] = 0;

         if ( pCS->iPrioritiesAdjustment && (pCS->iThreadPriorityCentral > 100) )
            sprintf(szPrefix, "nice -n %d", pCS->iThreadPriorityCentral - 120);    
         hw_execute_ruby_process(szPrefix, "ruby_central", "-nointro", NULL);
         uTimeLastPaused = g_TimeNow;
         log_line_watchdog("Restarting ruby_central process done.");
      } 
   }

   if ( NULL != pSemaphoreRestart )
      sem_close(pSemaphoreRestart);
   if ( NULL != pSemaphorePause )
      sem_close(pSemaphorePause);
   if ( NULL != pSemaphoreResume )
      sem_close(pSemaphoreResume);
   pSemaphoreRestart = NULL;
   pSemaphorePause = NULL;
   pSemaphoreResume = NULL;

   sem_unlink(SEMAPHORE_WATCHDOG_CONTROLLER_PAUSE);
   sem_unlink(SEMAPHORE_WATCHDOG_CONTROLLER_RESUME);
   sem_unlink(SEMAPHORE_WATCHDOG_CONTROLLER_RESTART);


   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_CENTRAL, s_pProcessStatsCentral);   
   s_pProcessStatsCentral = NULL;
   log_line("Finished execution.");
   log_line_watchdog("Finished execution");
   return (0);
} 