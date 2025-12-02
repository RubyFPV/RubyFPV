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

#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "r_test.h"
#include <pthread.h>

pthread_t s_pThreadTestSpeed1;
pthread_t s_pThreadTestSpeed2;

void _test_clock_resolution()
{
   u32 uTime1 = get_current_timestamp_ms();
   u32 uTime2 = 0;
   for( int i=0; i<50; i++ )
   {
      hardware_sleep_ms(1);
      uTime2 = get_current_timestamp_ms();
      log_line("Sleep 1 ms, diff: %u - %u = %u", uTime2, uTime1, uTime2-uTime1);
      uTime1 = uTime2;
   }

   for( int i=1; i<30; i++ )
   {
      uTime1 = get_current_timestamp_ms();
      hardware_sleep_ms(i);
      uTime2 = get_current_timestamp_ms();
      log_line("Sleep %d ms, diff: %u - %u = %u", i, uTime2, uTime1, uTime2-uTime1);
   }

   uTime1 = get_current_timestamp_ms();
   int tmp = 0;
   float f = 0;
   float b = 0;

   for( int i=0; i<40; i++ )
   {
      for( int k=0; k<i*20000; k++ )
      {
         tmp += i;
         f += k;
         b = f-tmp*3;
         b = b-1;
      }
      uTime2 = get_current_timestamp_ms();
      log_line("Work %d, diff: %u - %u = %u", i, uTime2, uTime1, uTime2-uTime1);
      uTime1 = uTime2;
   }

   int socket_server;
   struct sockaddr_in server_addr, client_addr;
 
   socket_server = socket(AF_INET , SOCK_DGRAM, 0);
   if (socket_server == -1)
   {
      return;
   }

   memset(&server_addr, 0, sizeof(server_addr));
   memset(&client_addr, 0, sizeof(client_addr));
    
   server_addr.sin_family = AF_INET;
   server_addr.sin_addr.s_addr = INADDR_ANY;
   server_addr.sin_port = htons( 5555 );
 
   client_addr.sin_family = AF_INET;
   client_addr.sin_addr.s_addr = INADDR_ANY;
   client_addr.sin_port = htons( 5555 );

   if( bind(socket_server,(struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 )
   {
      return;
   }

   uTime1 = get_current_timestamp_ms();
   int iLoop = 0;
   while ( true )
   {
       iLoop++;
       if ( iLoop > 20 )
          break;

      fd_set readset;
      FD_ZERO(&readset);
      FD_SET(socket_server, &readset);

      struct timeval timePipeInput;
      timePipeInput.tv_sec = 0;
      timePipeInput.tv_usec = iLoop*500; // x miliseconds timeout

      int selectResult = select(socket_server+1, &readset, NULL, NULL, &timePipeInput);
      
      uTime2 = get_current_timestamp_ms();
      log_line("Select %d, %d ms, result: %d, diff: %u - %u = %u", iLoop, iLoop/2, selectResult, uTime2, uTime1, uTime2-uTime1);
      uTime1 = uTime2;
   }

   uTime1 = get_current_timestamp_ms();
   iLoop = 0;
   while ( true )
   {
       iLoop++;
       if ( iLoop > 20 )
          break;

      fd_set readset;
      FD_ZERO(&readset);
      FD_SET(socket_server, &readset);

      struct timeval timePipeInput;
      timePipeInput.tv_sec = 0;
      timePipeInput.tv_usec = 0;

      int selectResult = select(socket_server+1, &readset, NULL, NULL, &timePipeInput);
      
      uTime2 = get_current_timestamp_ms();
      log_line("Select no wait, %d result: %d, diff: %u - %u = %u", iLoop, selectResult, uTime2, uTime1, uTime2-uTime1);
      uTime1 = uTime2;
   }
   close(socket_server);
}

void _test_speed(const char* szId)
{
   u32 uTimeStart = get_current_timestamp_ms();
   
   char* pArray = (char*)malloc(2005);

   int k = 0;
   int p = 15;
   for( int i=0; i<500000; i++ )
   {
      k += rand();
      p += 5;
      p += i;
      p *= k;
      int index = (i+(rand()*2000))%2000;
      index = (index+2000) % 2000;
      pArray[index] = p & 0xFF;
   }

   u32 uTimeEnd = get_current_timestamp_ms();
   free(pArray);
   log_line("Speed test: %s. Executed in %u milisec", szId, uTimeEnd - uTimeStart);
}

static void * _thread_speed(void *argument)
{
   _test_speed((char*)argument);
   return NULL;
}

static void * _thread_speed10(void *argument)
{
   hw_set_current_thread_rt_priority((char*)argument, 10);
   _test_speed((char*)argument);
   return NULL;
}

static void * _thread_speed20(void *argument)
{
   hw_set_current_thread_rt_priority((char*)argument, 20);
   _test_speed((char*)argument);
   return NULL;
}

static void * _thread_speed30(void *argument)
{
   hw_set_current_thread_rt_priority((char*)argument, 30);
   _test_speed((char*)argument);
   return NULL;
}

static void * _thread_speed50(void *argument)
{
   hw_set_current_thread_rt_priority((char*)argument, 50);
   _test_speed((char*)argument);
   return NULL;
}

static void * _thread_speed70(void *argument)
{
   hw_set_current_thread_rt_priority((char*)argument, 70);
   _test_speed((char*)argument);
   return NULL;
}

void start_test()
{
   log_init("Test");
   log_enable_stdout();
   log_line("\nStarted.\n");

   _test_clock_resolution();

   char szId1[32];
   char szId2[32];

   log_line("------------------------------------------------");
   log_line("Test speed single thread...");
   _test_speed("SingleThread");

   for( int i=0; i<2; i++ )
   {
      hw_set_current_thread_rt_priority("main", 10);
      _test_speed("SingleThreadPri10");
      hw_set_current_thread_rt_priority("main", 30);
      _test_speed("SingleThreadPri30");
      hw_set_current_thread_rt_priority("main", 50);
      _test_speed("SingleThreadPri50");
      hw_set_current_thread_rt_priority("main", 70);
      _test_speed("SingleThreadPri70");
   }

   log_line("------------------------------------------------");
   log_line("Test speed multiple threads, default priority...");
   strcpy(szId1, "Thread1");
   strcpy(szId2, "Thread2");

   pthread_create(&s_pThreadTestSpeed1, NULL, &_thread_speed, szId1);
   pthread_create(&s_pThreadTestSpeed2, NULL, &_thread_speed, szId2);

   log_line("Threads started, waiting to finish...");
   pthread_join(s_pThreadTestSpeed1, NULL );
   pthread_join(s_pThreadTestSpeed2, NULL );

   log_line("Threads finished.");


   log_line("------------------------------------------------");
   log_line("Test speed multiple threads, high priority...");

   for( int i=0; i<2; i++ )
   {
      strcpy(szId1, "ThreadHighA10");
      strcpy(szId2, "ThreadHighB10");
      pthread_create(&s_pThreadTestSpeed1, NULL, &_thread_speed10, szId1);
      pthread_create(&s_pThreadTestSpeed2, NULL, &_thread_speed10, szId2);
      pthread_join(s_pThreadTestSpeed1, NULL );
      pthread_join(s_pThreadTestSpeed2, NULL );

      strcpy(szId1, "ThreadHighA20");
      strcpy(szId2, "ThreadHighB20");
      pthread_create(&s_pThreadTestSpeed1, NULL, &_thread_speed20, szId1);
      pthread_create(&s_pThreadTestSpeed2, NULL, &_thread_speed20, szId2);
      pthread_join(s_pThreadTestSpeed1, NULL );
      pthread_join(s_pThreadTestSpeed2, NULL );

      strcpy(szId1, "ThreadHighA30");
      strcpy(szId2, "ThreadHighB30");
      pthread_create(&s_pThreadTestSpeed1, NULL, &_thread_speed30, szId1);
      pthread_create(&s_pThreadTestSpeed2, NULL, &_thread_speed30, szId2);
      pthread_join(s_pThreadTestSpeed1, NULL );
      pthread_join(s_pThreadTestSpeed2, NULL );

      strcpy(szId1, "ThreadHighA50");
      strcpy(szId2, "ThreadHighB50");
      pthread_create(&s_pThreadTestSpeed1, NULL, &_thread_speed50, szId1);
      pthread_create(&s_pThreadTestSpeed2, NULL, &_thread_speed50, szId2);
      pthread_join(s_pThreadTestSpeed1, NULL );
      pthread_join(s_pThreadTestSpeed2, NULL );

      strcpy(szId1, "ThreadHighA70");
      strcpy(szId2, "ThreadHighB70");
      pthread_create(&s_pThreadTestSpeed1, NULL, &_thread_speed70, szId1);
      pthread_create(&s_pThreadTestSpeed2, NULL, &_thread_speed70, szId2);
      pthread_join(s_pThreadTestSpeed1, NULL );
      pthread_join(s_pThreadTestSpeed2, NULL );
   }
   log_line("Threads finished.");
}