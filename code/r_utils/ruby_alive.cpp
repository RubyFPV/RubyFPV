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

bool gbQuit = false;

void power_leds(int onoff)
{
   #ifdef HW_PLATFORM_RASPBERRY
   DIR *d;
   struct dirent *dir;
   char szBuff[1024];
   d = opendir("/sys/class/leds/");
   if (d)
   {
      while ((dir = readdir(d)) != NULL)
      {
         if ( strlen(dir->d_name) < 3 )
            continue;
         if ( dir->d_name[0] != 'l' )
            continue;
         snprintf(szBuff, 1023, "/sys/class/leds/%s/brightness", dir->d_name);
         FILE* fd = fopen(szBuff, "w");
         if ( NULL != fd )
         {
            fprintf(fd, "%d\n", onoff);
            fclose(fd);
         }
      }
      closedir(d);
   }

   if ( onoff )
      GPIOWrite(GPIOGetPinLedRed(), HIGH);
   else
      GPIOWrite(GPIOGetPinLedRed(), LOW);
   #endif
}

void _check_cpu_watchdog(u32 uTimeNow, int iCounter)
{
   static u32 s_uLastTimeWatchDog = 0;

   if ( 0 == s_uLastTimeWatchDog )
      s_uLastTimeWatchDog = uTimeNow;

   if ( uTimeNow > (s_uLastTimeWatchDog + 200) )
      log_softerror_and_alarm("FROZE FOR %u ms", uTimeNow - s_uLastTimeWatchDog);

   s_uLastTimeWatchDog = uTimeNow;
}


void handle_sigint(int sig) 
{ 
   gbQuit = true;
} 

int main(int argc, char *argv[])
{
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   log_init("RubyAlive");

   int counter = 0;
   int nLeds = 0;

   char szFileUpdate[128];
   strcpy(szFileUpdate, FOLDER_RUBY_TEMP);
   strcat(szFileUpdate, FILE_TEMP_UPDATE_IN_PROGRESS);

   char szFileUpdateApply[128];
   strcpy(szFileUpdateApply, FOLDER_RUBY_TEMP);
   strcat(szFileUpdateApply, FILE_TEMP_UPDATE_IN_PROGRESS_APPLY);

   char szFileAlarm[128];
   strcpy(szFileAlarm, FOLDER_RUBY_TEMP);
   strcat(szFileAlarm, FILE_TEMP_ALARM_ON);

   bool bHasAlarm = false;
   bool bHasUpdateInProgress = false;
   bool bHasUpdateApplyInProgress = false;

   int nSleepMs = 100;

   log_line("Entering main loop...");

   while ( ! gbQuit )
   {
      g_uLoopCounter++;
      u32 uTimeStart = get_current_timestamp_ms();

      _check_cpu_watchdog(uTimeStart, counter);

      hardware_sleep_ms(nSleepMs);
      counter++;


      if ( (counter % 20) == 0 )
      {
         if ( access(szFileAlarm, R_OK) != -1 )
            bHasAlarm = true;
         else
            bHasAlarm = false;

         if ( access(szFileUpdate, R_OK) != -1 )
            bHasUpdateInProgress = true;
         else
            bHasUpdateInProgress = false;

         if ( access(szFileUpdateApply, R_OK) != -1 )
         {
            bHasUpdateApplyInProgress = true;
            nSleepMs = 40;
         }
         else
            bHasUpdateApplyInProgress = false;
      }

      if ( (counter % 100) == 0 )
      {
         if ( bHasAlarm )
            log_line("Pi is alive. Alarm on.");
         else
            log_line("Pi is alive");
      }

      if ( bHasAlarm )
      {
         if ( counter % 2 )
         {
            nLeds = 1 - nLeds;
            power_leds(nLeds);
         }
      }
      else if ( bHasUpdateInProgress )
      {
         if ( (counter % 2) == 0 )
         {
            nLeds = 1 - nLeds;
            power_leds(nLeds);
         }
      }
      else if ( bHasUpdateApplyInProgress )
      {
         if ( (counter % 2) == 0 )
         {
            nLeds = 1 - nLeds;
            power_leds(nLeds);
         }
      }
      else
      {
         if ( hardware_is_vehicle() )
         {
            if ( (counter%10) == 0 )
            {
               nLeds = 1;
               power_leds(nLeds);
            }
            if ( (counter%10) == 3 )
            {
               nLeds = 0;
               power_leds(nLeds);
            }
         }
         else
         {
            if ( (counter%30) == 0 )
            {
               nLeds = 1;
               power_leds(nLeds);
            }
            if ( (counter%30) == 5 )
            {
               nLeds = 0;
               power_leds(nLeds);
            }
         }
      }

      u32 dTime = get_current_timestamp_ms() - uTimeStart;
      if ( dTime > 150 )
         log_softerror_and_alarm("Main processing loop took too long (%u ms).", dTime);
   }

   log_line("Exiting...");
   power_leds(1);
   hardware_release();
   return (0);
} 