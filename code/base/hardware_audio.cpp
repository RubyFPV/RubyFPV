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
#include "config.h"
#include "hardware_files.h"
#include "hardware.h"
#include "hardware_procs.h"
#include <ctype.h>
#include <pthread.h>

static bool s_bHardwareDetectedAudioDevices = false;
static bool s_bHardwareAudioHasAudioPlayback = false;
static bool s_bHardwareAudioHasAudioSwitch = false;
static bool s_bHardwareAudioHasAudioVolume = false;
static int  s_iHardwareAudiotSwitchControlId = -1;
static int  s_iHardwareAudioVolumeControlId = -1;

void _hardware_audio_enumerate_capabilities()
{
   log_line("[HardwareAudio] Detecting capabilites...");
   s_bHardwareDetectedAudioDevices = true;

   s_bHardwareAudioHasAudioPlayback = false;
   s_bHardwareAudioHasAudioSwitch = false;
   s_bHardwareAudioHasAudioVolume = false;
   s_iHardwareAudiotSwitchControlId = -1;
   s_iHardwareAudioVolumeControlId = -1;

   #if defined(HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA)
   char szOutput[4096];
   hw_execute_bash_command_raw("aplay -l 2>&1", szOutput );
   if ( (0 == szOutput[0]) || (NULL != strstr(szOutput, "no soundcards")) || (NULL != strstr(szOutput, "no sound")) )
      s_bHardwareAudioHasAudioPlayback = false;
   else
      s_bHardwareAudioHasAudioPlayback = true;
   log_line("[HardwareAudio] Device has audio playback: %s", s_bHardwareAudioHasAudioPlayback?"yes":"no");
   hw_execute_bash_command("amixer controls", szOutput);
   char* pszLine = strtok(szOutput, "\n");
   while(NULL != pszLine)
   {
      log_line("[HardwareAudio] Parsing capabilities line: [%s]", pszLine);
      if ( NULL != strstr(pszLine, "Volume") )
      {
         while(0 != *pszLine)
         {
            if ( isdigit(*pszLine) )
            {
               s_iHardwareAudioVolumeControlId = *pszLine - '0';
               s_bHardwareAudioHasAudioVolume = true;
               log_line("[HardwareAudio] Detected volume control, id: %d", s_iHardwareAudioVolumeControlId);
            }
            pszLine++;
         }
      }
      if ( NULL != strstr(pszLine, "HDMI Playback Switch") )
      {
         while(0 != *pszLine)
         {
            if ( isdigit(*pszLine) )
            {
               s_iHardwareAudiotSwitchControlId = *pszLine - '0';
               s_bHardwareAudioHasAudioSwitch = true;
               log_line("[HardwareAudio] Detected HDMI audio switch control, id: %d", s_iHardwareAudiotSwitchControlId);
            }
            pszLine++;
         }
      }
      pszLine = strtok(NULL, "\n");
   }
   #endif
   log_line("[HardwareAudio] Done detecting capabilites.");
}

bool hardware_board_has_audio_builtin(u32 uBoardType)
{
   if ( hardware_board_is_sigmastar(uBoardType) )
   if ( hardware_board_is_openipc(uBoardType & BOARD_TYPE_MASK) )
   if ( (((uBoardType & BOARD_SUBTYPE_MASK) >> BOARD_SUBTYPE_SHIFT) == BOARD_SUBTYPE_OPENIPC_AIO_MARIO) ||
        (((uBoardType & BOARD_SUBTYPE_MASK) >> BOARD_SUBTYPE_SHIFT) == BOARD_SUBTYPE_OPENIPC_AIO_RUNCAM_V1) ||
        (((uBoardType & BOARD_SUBTYPE_MASK) >> BOARD_SUBTYPE_SHIFT) == BOARD_SUBTYPE_OPENIPC_AIO_RUNCAM_V2) ||
        (((uBoardType & BOARD_SUBTYPE_MASK) >> BOARD_SUBTYPE_SHIFT) == BOARD_SUBTYPE_OPENIPC_AIO_THINKER) ||
        (((uBoardType & BOARD_SUBTYPE_MASK) >> BOARD_SUBTYPE_SHIFT) == BOARD_SUBTYPE_OPENIPC_AIO_THINKER_E) )
      return true;
   return false;
}

bool hardware_has_audio_playback()
{
   if ( ! s_bHardwareDetectedAudioDevices )
      _hardware_audio_enumerate_capabilities();
   return s_bHardwareAudioHasAudioPlayback;
}

int hardware_enable_audio_output()
{
   if ( ! s_bHardwareDetectedAudioDevices )
      _hardware_audio_enumerate_capabilities();
   if ( (! s_bHardwareAudioHasAudioSwitch) || (s_iHardwareAudiotSwitchControlId < 0) )
      return -1;

   char szComm[256];
   char szOutput[1024];
   sprintf(szComm, "amixer cset numid=%d 1", s_iHardwareAudiotSwitchControlId);
   hw_execute_bash_command(szComm, szOutput);
   log_line("[HardwareAudio] Result of enabling audio: [%s]", szOutput);
   return 1;
}

bool hardware_has_audio_volume()
{
   if ( ! s_bHardwareDetectedAudioDevices )
      _hardware_audio_enumerate_capabilities();
   return s_bHardwareAudioHasAudioVolume;
}

int hardware_set_audio_output_volume(int iAudioVolume)
{
   if ( ! s_bHardwareDetectedAudioDevices )
      _hardware_audio_enumerate_capabilities();
   if ( (! s_bHardwareAudioHasAudioVolume) || (s_iHardwareAudioVolumeControlId < 0) )
      return -1;

   char szComm[256];
   char szOutput[1024];
   sprintf(szComm, "amixer cset numid=%d %d%%", s_iHardwareAudioVolumeControlId, iAudioVolume);
   hw_execute_bash_command(szComm, szOutput);
   log_line("[HardwareAudio] Result of setting volume: [%s]", szOutput);
   log_line("[HardwareAudio] Did set output audio volume to: %d", iAudioVolume);
   return 1;
}



int hardware_audio_play_file(const char* szFile)
{
   if ( (NULL == szFile) || (0 == szFile[0]) )
      return -1;

   if ( ! s_bHardwareDetectedAudioDevices )
      _hardware_audio_enumerate_capabilities();
   if ( ! s_bHardwareAudioHasAudioPlayback )
      return -1;

   char szComm[256];
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "aplay -q %s 2>/dev/null 1>/dev/null", szFile);
   
   #if defined (HW_PLATFORM_RADXA)
   char szDevice[64];
   szDevice[0] = 0;
   if ( (hardware_getBoardType() & BOARD_TYPE_MASK) == BOARD_TYPE_RADXA_3C )
      strcpy(szDevice, "-D hw:CARD=rockchiphdmi0 ");
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "aplay -q %s%s 2>/dev/null 1>/dev/null", szDevice, szFile);
   #endif

   hw_execute_bash_command_timeout(szComm, NULL, 30000);
   return 0;
}


pthread_t s_pThreadAudioPlayAsync;
char s_szAudioFilePlayAsync[MAX_FILE_PATH_SIZE];

void* _thread_audio_play_async(void *argument)
{
   log_line("[HardwareAudio] Started thread to play file async.");
   hw_log_current_thread_attributes("play audio async");
   log_line("[HardwareAudio] Playing file: %s", s_szAudioFilePlayAsync);
   char szComm[256];
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "aplay -q %s 2>/dev/null 1>/dev/null &", s_szAudioFilePlayAsync);
   
   #if defined (HW_PLATFORM_RADXA)
   char szDevice[64];
   szDevice[0] = 0;
   if ( (hardware_getBoardType() & BOARD_TYPE_MASK) == BOARD_TYPE_RADXA_3C )
      strcpy(szDevice, "-D hw:CARD=rockchiphdmi0 ");
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "aplay -q %s%s 2>/dev/null 1>/dev/null &", szDevice, s_szAudioFilePlayAsync);
   #endif
   
   hw_execute_bash_command_nonblock(szComm, NULL);
   log_line("[HardwareAudio] Ended thread to play file async.");
   return NULL;
}

int hardware_audio_play_file_async(const char* szFile)
{
   if ( (NULL == szFile) || (0 == szFile[0]) )
      return -1;

   if ( ! s_bHardwareDetectedAudioDevices )
      _hardware_audio_enumerate_capabilities();
   if ( ! s_bHardwareAudioHasAudioPlayback )
      return -1;

   strcpy(s_szAudioFilePlayAsync, szFile);

   pthread_attr_t attr;
   hw_init_worker_thread_attrs(&attr, CORE_AFFINITY_OTHERS, 64000, SCHED_OTHER, 0, "Play audio file async");
   if ( 0 != pthread_create(&s_pThreadAudioPlayAsync, &attr, &_thread_audio_play_async, NULL) )
   {
      pthread_attr_destroy(&attr);
      log_softerror_and_alarm("[HardwareAudio] Failed to play audio file (%s)", szFile);
      return -1;
   }
   pthread_attr_destroy(&attr);
   return 0;
}

void hardware_audio_stop_async_play()
{
   hw_stop_process("aplay");
}