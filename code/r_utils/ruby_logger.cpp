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

#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <sys/file.h>
#include <time.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

bool g_bQuit = false;
int s_iCounter = 0;
int s_iLogKeyId = LOGGER_MESSAGE_QUEUE_ID;

char s_szLogMsg[256];

void _log_logger_message(const char* szMsg)
{
   s_iCounter++;
   char szFile[256];
   strcpy(szFile, FOLDER_LOGS);
   strcat(szFile, LOG_FILE_LOGGER);
   FILE* fd = fopen(szFile, "ab");
   if ( NULL != fd )
   {
      fprintf(fd, "%d: %s\n", s_iCounter, szMsg);
      fclose(fd);
      log_line("Write to logger (%s)", szFile);
   }
   else
      log_line("Failed to write to logger (%s)", szFile);
}

void handle_sigint(int sig) 
{ 
   log_line("--------------------------");
   log_line("Caught signal to stop: %d", sig);
   log_line("--------------------------");
   g_bQuit = true;
} 

int _open_msg_queue()
{
   int iLogMsgQueue = -1;

   key_t key = generate_msgqueue_key(s_iLogKeyId);
   iLogMsgQueue = msgget(key, IPC_CREAT | S_IRUSR | S_IRGRP | S_IROTH);
   if ( iLogMsgQueue < 0 )
   {
      log_softerror_and_alarm("Failed to create logger message queue");
      _log_logger_message("Failed to create logger message queue");
      iLogMsgQueue = msgget(key, 0444);
      if ( iLogMsgQueue >= 0 )
      {
         log_softerror_and_alarm("Queue already exists. Closing and recreating it.");
         _log_logger_message("Queue already exists. Closing and recreating it.");
         msgctl(iLogMsgQueue,IPC_RMID,NULL);
         iLogMsgQueue = msgget(key, IPC_CREAT | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
         if ( iLogMsgQueue < 0 )
         {
            log_softerror_and_alarm("Failed to recreate logger message queue");
            _log_logger_message("Failed to recreate logger message queue");
         }
      }
      log_softerror_and_alarm("Failed to open existing logger message queue");
      _log_logger_message("Failed to open existing logger message queue");
   }
   else
   {
      sprintf(s_szLogMsg, "Created logger message queue, key id: %x, msgqueue id: %d", key, iLogMsgQueue);
      log_line(s_szLogMsg);
      _log_logger_message(s_szLogMsg);
   }
   return iLogMsgQueue;
}


void _log_platform(bool bNewLine)
{
   #if defined(HW_PLATFORM_OPENIPC_CAMERA)
   printf("Built for OpenIPC camera.");
   #elif defined(HW_PLATFORM_LINUX_GENERIC)
   printf("Built for Linux");
   #elif defined(HW_PLATFORM_RASPBERRY)
   printf("Built for Raspberry");
   #elif defined(HW_PLATFORM_RADXA)
   printf("Built for Radxa Zero 3");
   #else
   printf("Built for N/A");
   #endif
   if ( bNewLine )
      printf("\n");
}


int main(int argc, char *argv[])
{
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   if ( strcmp(argv[argc-1], "-ver") == 0 )
   {
      printf("%d.%d (b-%d) ", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR, SYSTEM_SW_BUILD_NUMBER);
      _log_platform(false);
      return 0;
   }

   if ( argc >= 2 )
   if ( strcmp(argv[argc-2], "-id") == 0 )
   {
      s_iLogKeyId = atoi(argv[argc-1]);
   }

   log_enable_stdout();
   log_init_local_only("RubyLogger");
   log_arguments(argc, argv);

   type_log_message_buffer logMessage;

   int iLogMsgQueue = _open_msg_queue();

   if ( iLogMsgQueue < 0 )
   {
      log_softerror_and_alarm("No message queue available. Exit logger.");
      return -1;
   }

   log_line("Started logger main loop");

   _log_logger_message("Started");

   char szFileLog[256];
   strcpy(szFileLog, FOLDER_LOGS);
   strcat(szFileLog, LOG_FILE_SYSTEM);
   char szFileErrors[256];
   strcpy(szFileErrors, FOLDER_LOGS);
   strcat(szFileErrors, LOG_FILE_ERRORS);
   char szFileSoft[256];
   strcpy(szFileSoft, FOLDER_LOGS);
   strcat(szFileSoft, LOG_FILE_ERRORS_SOFT);

   while ( !g_bQuit )
   {
      // This is blocking
      int len = msgrcv(iLogMsgQueue, &logMessage, MAX_SERVICE_LOG_ENTRY_LENGTH, 0, MSG_NOERROR);
      if ( g_bQuit )
         break;
      
      if ( len <= 0 )
      {
          sprintf(s_szLogMsg, "Failed to read log message queue. Error code: %d, (%s)", errno, strerror(errno));
          log_line(s_szLogMsg);
          _log_logger_message(s_szLogMsg);
          iLogMsgQueue = _open_msg_queue();
          if ( iLogMsgQueue < 0 )
          {
             g_bQuit = true;
             break;
          }
          else
             continue;
      }
      logMessage.text[len-1] = 0;
      //sprintf(szTmp, "len %d", len);
      //_log_logger_message(szTmp);
      //_log_logger_message(logMessage.text);
      //log_line("Received message type: %d, length: %d bytes", logMessage.type, len);

      //if ( logMessage.type == 1 )
      if ( 1 )
      {
         FILE* fd = fopen(szFileLog, "a+");
         if ( NULL != fd )
         {
            fprintf(fd, "%s\n", logMessage.text);
            fclose(fd);
         }
      }

      if ( logMessage.type == 2 )
      {
         FILE* fd = fopen(szFileSoft, "a+");
         if ( NULL != fd )
         {
            fprintf(fd, "%s\n", logMessage.text);
            fclose(fd);
         }
      }

      if ( logMessage.type == 3 )
      {
         FILE* fd = fopen(szFileErrors, "a+");
         if ( NULL != fd )
         {
            fprintf(fd, "%s\n", logMessage.text);
            fclose(fd);
         }
      }
   }

   if ( iLogMsgQueue >= 0 )
   {
      msgctl(iLogMsgQueue,IPC_RMID,NULL);
      log_line("Closed logger message queue.");
   }

   log_line("Stopped");
   return 0;
} 