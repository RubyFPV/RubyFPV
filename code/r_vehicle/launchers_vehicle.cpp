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

#include "launchers_vehicle.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hardware_i2c.h"
#include "../base/hardware_procs.h"
#include "../base/hardware_cam_maj.h"
#include "../base/radio_utils.h"
#include <math.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/types.h>

#include "shared_vars.h"
#include "timers.h"

static bool s_bAudioCaptureIsStarted = false;
static pthread_t s_pThreadAudioCapture;

void vehicle_launch_tx_telemetry(Model* pModel)
{
   if (NULL == pModel )
   {
      log_error_and_alarm("Invalid model (NULL) on launching TX telemetry. Can't start TX telemetry.");
      return;
   }

   char szPrefix[64];
   szPrefix[0] = 0;
   if ( pModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS )
   if ( pModel->processesPriorities.iThreadPriorityTelemetry > 100 )
      sprintf(szPrefix, "nice -n %d", pModel->processesPriorities.iThreadPriorityTelemetry - 120);
   hw_execute_ruby_process(szPrefix, "ruby_tx_telemetry", NULL, NULL);
}

void vehicle_stop_tx_telemetry()
{
   sem_t* pSem = sem_open(SEMAPHORE_STOP_VEHICLE_TELEM_TX, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( (NULL == pSem) || (SEM_FAILED == pSem) )
      log_softerror_and_alarm("Failed to open semaphore to signal stop of tx_telemetry: %s", SEMAPHORE_STOP_VEHICLE_TELEM_TX);
   else
   {
      sem_post(pSem);
      sem_close(pSem);
      log_line("Signaled semaphore to stop tx_telemetry");
   }
}

void vehicle_launch_rx_rc(Model* pModel)
{
   if (NULL == pModel )
   {
      log_error_and_alarm("Invalid model (NULL) on launching RX RC. Can't start RX RC.");
      return;
   }
   char szPrefix[64];
   szPrefix[0] = 0;

   if ( pModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS )
   {
      if ( pModel->processesPriorities.iThreadPriorityRC > 100 )
         sprintf(szPrefix, "nice -n %d", pModel->processesPriorities.iThreadPriorityRC - 120);
      #ifdef HW_CAPABILITY_IONICE
      if ( DEFAULT_IO_PRIORITY_RC > 0 )
      {
         sprintf(szPrefix, "ionice -c 1 -n %d", DEFAULT_IO_PRIORITY_RC );
         if ( pModel->processesPriorities.iThreadPriorityRC > 100 )
            sprintf(szPrefix, "ionice -c 1 -n %d nice -n %d", DEFAULT_IO_PRIORITY_RC, pModel->processesPriorities.iThreadPriorityRC - 120);
      }
      #endif
   }
   hw_execute_ruby_process(szPrefix, "ruby_start", "-rc", NULL);
}

void vehicle_stop_rx_rc()
{
   sem_t* pSem = sem_open(SEMAPHORE_STOP_VEHICLE_RC_RX, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( (NULL == pSem) || (SEM_FAILED == pSem) )
      log_softerror_and_alarm("Failed to open semaphore to signal stop of rc_rx: %s", SEMAPHORE_STOP_VEHICLE_RC_RX);
   else
   {
      sem_post(pSem);
      sem_close(pSem);
      log_line("Signaled semaphore to stop rx_rc");
   }
}

void vehicle_launch_rx_commands(Model* pModel)
{
   if (NULL == pModel )
   {
      log_error_and_alarm("Invalid model (NULL) on launching RX commands. Can't start RX commands.");
      return;
   }
   char szPrefix[64];
   szPrefix[0] = 0;
   if ( pModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS )
   if ( pModel->processesPriorities.iThreadPriorityOthers > 100 )
      sprintf(szPrefix, "nice -n %d", pModel->processesPriorities.iThreadPriorityOthers - 120);
   hw_execute_ruby_process(szPrefix, "ruby_start", "-rx_commands", NULL);
}

void vehicle_stop_rx_commands()
{
   sem_t* pSem = sem_open(SEMAPHORE_STOP_VEHICLE_COMMANDS, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( (NULL == pSem) || (SEM_FAILED == pSem) )
      log_softerror_and_alarm("Failed to open semaphore to signal stop of rx_commands: %s", SEMAPHORE_STOP_VEHICLE_COMMANDS);
   else
   {
      sem_post(pSem);
      sem_close(pSem);
      log_line("Signaled semaphore to stop rx_commands");
   }
}

void vehicle_launch_tx_router(Model* pModel)
{
   if (NULL == pModel )
   {
      log_error_and_alarm("Invalid model (NULL) on launching TX video pipeline. Can't start TX video pipeline.");
      return;
   }

   hardware_sleep_ms(20);

   char szPrefix[64];
   szPrefix[0] = 0;

   if ( pModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS )
   {
      if ( pModel->processesPriorities.iThreadPriorityRouter > 100 )
         sprintf(szPrefix, "nice -n %d", pModel->processesPriorities.iThreadPriorityRouter - 120);
      #ifdef HW_CAPABILITY_IONICE
      if ( pModel->processesPriorities.ioNiceRouter > 0 )
      {
         sprintf(szPrefix, "ionice -c 1 -n %d", pModel->processesPriorities.ioNiceRouter );
         if ( pModel->processesPriorities.iThreadPriorityRouter > 100 )
            sprintf(szPrefix, "ionice -c 1 -n %d nice -n %d", pModel->processesPriorities.ioNiceRouter, pModel->processesPriorities.iThreadPriorityRouter - 120);
      }
      #endif
   }
   
   hw_execute_ruby_process(szPrefix, "ruby_rt_vehicle", NULL, NULL);
}

void vehicle_stop_tx_router()
{
   sem_t* pSem = sem_open(SEMAPHORE_STOP_VEHICLE_ROUTER, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( (NULL == pSem) || (SEM_FAILED == pSem) )
      log_softerror_and_alarm("Failed to open semaphore to signal stop of router: %s", SEMAPHORE_STOP_VEHICLE_ROUTER);
   else
   {
      sem_post(pSem);
      sem_close(pSem);
      log_line("Signaled semaphore to stop router");
   }
}

#if defined (HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA)
static void * _thread_audio_capture(void *argument)
{
   s_bAudioCaptureIsStarted = true;

   Model* pModel = (Model*) argument;
   if ( NULL == pModel )
   {
      s_bAudioCaptureIsStarted = false;
      return NULL;
   }
   hw_log_current_thread_attributes("audio capture");

   char szCommFlag[256];
   char szCommCapture[256];
   char szRate[32];
   char szPriority[64];
   int iIntervalSec = 5;

   sprintf(szCommCapture, "amixer -c 1 sset Mic %d%%", pModel->audio_params.volume);
   hw_execute_bash_command(szCommCapture, NULL);

   strcpy(szRate, "8000");
   if ( 0 == pModel->audio_params.quality )
      strcpy(szRate, "14000");
   if ( 1 == pModel->audio_params.quality )
      strcpy(szRate, "24000");
   if ( 2 == pModel->audio_params.quality )
      strcpy(szRate, "32000");
   if ( 3 == pModel->audio_params.quality )
      strcpy(szRate, "44100");

   strcpy(szRate, "44100");

   szPriority[0] = 0;
   if ( pModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS )
   {
      if ( (pModel->processesPriorities.iThreadPriorityOthers > 100) && (pModel->processesPriorities.iThreadPriorityOthers < 140) )
         sprintf(szPriority, "nice -n %d", pModel->processesPriorities.iThreadPriorityOthers -  120);
   }

   sprintf(szCommCapture, "%s arecord --device=hw:1,0 --file-type wav --format S16_LE --rate %s -c 1 -d %d -q >> %s",
      szPriority, szRate, iIntervalSec, FIFO_RUBY_AUDIO1);

   sprintf(szCommFlag, "echo '0123456789' > %s", FIFO_RUBY_AUDIO1);

   hw_stop_process("arecord");

   while ( s_bAudioCaptureIsStarted && ( ! g_bQuit) )
   {
      if ( g_bReinitializeRadioInProgress )
      {
         hardware_sleep_ms(50);
         continue;         
      }

      u32 uTimeCheck = get_current_timestamp_ms();

      hw_execute_bash_command(szCommCapture, NULL);
      
      u32 uTimeNow = get_current_timestamp_ms();
      if ( uTimeNow < uTimeCheck + (u32)iIntervalSec * 500 )
      {
         log_softerror_and_alarm("[AudioCaptureThread] Audio capture segment finished too soon (took %u ms, expected %d ms)",
             uTimeNow - uTimeCheck, iIntervalSec*1000);
         for( int i=0; i<10; i++ )
            hardware_sleep_ms(iIntervalSec*50);
      }

      hw_execute_bash_command(szCommFlag, NULL);
   }
   s_bAudioCaptureIsStarted = false;
   return NULL;
}
#endif

bool vehicle_is_audio_capture_started()
{
  return s_bAudioCaptureIsStarted;
}

void vehicle_launch_audio_capture(Model* pModel)
{
   if ( s_bAudioCaptureIsStarted )
      return;

   if ( NULL == pModel || (! pModel->audio_params.has_audio_device) || (! pModel->audio_params.enabled) )
      return;

   s_bAudioCaptureIsStarted = true;

   #if defined (HW_PLATFORM_RASPBERRY)
   if ( 0 != pthread_create(&s_pThreadAudioCapture, NULL, &_thread_audio_capture, g_pCurrentModel) )
   {
      log_softerror_and_alarm("Failed to create thread for audio capture.");
      s_bAudioCaptureIsStarted = false;
      return;
   }
   pthread_detach(s_pThreadAudioCapture);
   log_line("Created thread for audio capture.");
   #endif

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   hardware_camera_maj_enable_audio(true, 4000*(1+pModel->audio_params.quality), pModel->audio_params.volume);
   #endif
}

void vehicle_stop_audio_capture(Model* pModel)
{
   if ( ! s_bAudioCaptureIsStarted )
      return;
   s_bAudioCaptureIsStarted = false;

   if ( NULL == pModel || (! pModel->audio_params.has_audio_device) )
      return;

   #if defined (HW_PLATFORM_RASPBERRY)
   hw_stop_process("arecord");
   #endif

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   hardware_camera_maj_enable_audio(false, 0, 0);
   #endif
}
