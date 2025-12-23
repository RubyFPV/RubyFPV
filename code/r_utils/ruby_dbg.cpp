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
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>
#include <ctype.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../base/config.h"

static bool s_bQuit = false;
static int s_iLastValues[20];

void _check_process(const char* szProc, int iLogIndex)
{
   char szComm[MAX_FILE_PATH_SIZE];
   char szOutput[1024];
   int iValue = 0;

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "ps -eo min_flt,comm | grep %s", szProc);
   hw_execute_bash_command(szComm, szOutput);
   if ( (strlen(szOutput) < 6) || (1 != sscanf(szOutput, "%d", &iValue)) )
      iValue = 0;

   if ( iValue == 0 )
   {
      if ( 0 == s_iLastValues[iLogIndex] )
      {
         s_iLastValues[iLogIndex] = 1;
         printf("----\t%s\n", szProc);
      }
      return;
   }

   if ( 0 == s_iLastValues[iLogIndex] )
   {
      printf("%d\t%s\n", iValue, szProc);
      s_iLastValues[iLogIndex] = iValue;
      return;
   }

   if ( iValue != s_iLastValues[iLogIndex] )
      printf("%d/sec\t%s\n", iValue - s_iLastValues[iLogIndex], szProc);
   s_iLastValues[iLogIndex] = iValue;
}


void log_page_faults()
{
   printf("Mem Page Faults / Proc\n");
   printf("-----------------------\n");

   u32 uTimeCheck = get_current_timestamp_ms();
   int iCounter = 0;

   while ( ! s_bQuit )
   {   
      _check_process("ruby_logger", 0);
      _check_process("ruby_rt_vehicle", 1);
      _check_process("ruby_tx_telemetry", 2);

      #if defined(HW_PLATFORM_OPENIPC_CAMERA)
      _check_process("majestic", 3);
      #else
      _check_process("ruby_capture_raspi", 4);
      #endif

      _check_process("ruby_central", 5);
      _check_process("ruby_rt_station", 6);
      _check_process("ruby_rx_telemetry", 7);

      #if defined(HW_PLATFORM_RADXA)
      _check_process("ruby_player_radxa", 8);
      #else
      _check_process("ruby_player_p", 9);
      _check_process("ruby_player_s", 10);
      #endif

      if ( 0 == iCounter )
      {
         printf("-----------------------\n");
         printf("Ctrl+C to stop\n");
         printf("Page Fauts/Sec  Proc\n");
      }
      iCounter++;
      u32 uTime = get_current_timestamp_ms();
      if ( uTime < uTimeCheck + 1000 )
      {
         hardware_sleep_ms(20);
         continue;
      }
   }
}

void handle_sigint(int sig) 
{ 
   log_line("--------------------------");
   log_line("Caught signal to stop: %d", sig);
   log_line("--------------------------");
   s_bQuit = true;
} 


int main(int argc, char *argv[])
{
   if ( NULL != strstr(argv[argc-1], "-v") )
   {
      printf("%d.%d (b-%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR, SYSTEM_SW_BUILD_NUMBER);
      return 0;
   }

   if ( NULL != strstr(argv[argc-1], "-pagefaults") )
   {
      signal(SIGINT, handle_sigint);
      signal(SIGTERM, handle_sigint);
      signal(SIGQUIT, handle_sigint);

      for(int i=0; i<20; i++ )
         s_iLastValues[i] = 0;
      log_page_faults();
      return 0;
   }
   hw_log_processes(argc, argv);
   return 0;
} 