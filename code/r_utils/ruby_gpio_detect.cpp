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
#include "../base/gpio.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../base/models.h"
#include "../common/string_utils.h"
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


bool g_bQuit = false;

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

   log_init("RubyGPIODetect");


   if ( strcmp(argv[argc-1], "-debug") == 0 )
      log_enable_stdout();

   #ifdef HW_CAPABILITY_GPIO
   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CONTROLLER_BUTTONS);
   if ( access(szFile, R_OK) != -1 )
   {
      log_line("GPIO configuration is already detected (config buttons file exists).");
      FILE* fp = fopen(szFile, "rb");
      if ( NULL != fp )
      {
         int iDetected = 0;
         int iPullDirection = 0;
         if ( 2 == fscanf(fp, "%d %d", &iDetected, &iPullDirection) )
         {
            log_line("GPIO direction detected: %s, direction: %s",
                (iDetected==1)?"yes":"not yet",
                (iPullDirection==0)?"pulled down":"pulled up");
            fclose(fp);
            return 0;
         }
         log_softerror_and_alarm("Failed to read buttons config file [%s]", szFile);
         fclose(fp);
      }
      else
         log_softerror_and_alarm("Failed to open [%s] buttons config file.", szFile);
   }
   else
      log_line("Buttons config file [%s] does not exists.", szFile);


   log_line("Start to detect GPIO configuration...");

   while ( ! g_bQuit )
   {
      GPIOInitButtons();

      while ( ! g_bQuit )
      {
         hardware_sleep_ms(10);
         int iRes = GPIOButtonsTryDetectPullUpDown();
         if ( iRes == 1 )
         {
            log_line("Detected GPIO direction.");
            break;
         }
      }

      log_line("Finished doing GPIO detection (quit: %s).", g_bQuit?"yes":"no");
      FILE* fp = fopen(szFile, "rb");
      if ( NULL != fp )
      {
         int iDetected = 0;
         int iPullDirection = 0;
         int iGPIOPin = 0;
         if ( 3 == fscanf(fp, "%d %d %d", &iDetected, &iPullDirection, &iGPIOPin) )
         {
            log_line("GPIO direction detected: %s, direction: %s, initial GPIO pin that was detected: %d",
                   (iDetected==1)?"yes":"not yet",
                   (iPullDirection==0)?"pulled down":"pulled up",
                   iGPIOPin);
            fclose(fp);
            break;
         }
         log_softerror_and_alarm("Failed to read values from buttons config file [%s]", szFile);
         fclose(fp);
      }
      else
         log_softerror_and_alarm("Failed to open [%s] buttons config file. Retry detection.", szFile);
   }
   #endif
   log_line("GPIO detection process finished.");

   return 0;
}