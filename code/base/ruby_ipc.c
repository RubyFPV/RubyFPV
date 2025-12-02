/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga
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
#include "ruby_ipc.h"
#include "hardware.h"
#include "hardware_procs.h"
#include "../common/string_utils.h"
#include "../radio/radiopackets2.h"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>

//#define RUBY_USE_FIFO_PIPES 1
#define RUBY_USES_MSGQUEUES 1

#define FIFO_RUBY_ROUTER_TO_CENTRAL "/tmp/ruby/fiforoutercentral"
#define FIFO_RUBY_CENTRAL_TO_ROUTER "/tmp/ruby/fifocentralrouter"
#define FIFO_RUBY_ROUTER_TO_COMMANDS "/tmp/ruby/fiforoutercommands"
#define FIFO_RUBY_COMMANDS_TO_ROUTER "/tmp/ruby/fifocommandsrouter"
#define FIFO_RUBY_ROUTER_TO_TELEMETRY "/tmp/ruby/fiforoutertelemetry"
#define FIFO_RUBY_TELEMETRY_TO_ROUTER "/tmp/ruby/fifotelemetryrouter"
#define FIFO_RUBY_ROUTER_TO_RC "/tmp/ruby/fiforouterrc"
#define FIFO_RUBY_RC_TO_ROUTER "/tmp/ruby/fiforcrouter"


//#define PROFILE_IPC 1
#define PROFILE_IPC_MAX_TIME 20

#define MAX_CHANNELS 16

int s_iRubyIPCChannelsUniqueIds[MAX_CHANNELS];
int s_iRubyIPCChannelsFd[MAX_CHANNELS];
int s_iRubyIPCChannelsType[MAX_CHANNELS];
u8  s_uRubyIPCChannelsMsgId[MAX_CHANNELS];
key_t s_uRubyIPCChannelsKeys[MAX_CHANNELS];

static int s_iRubyIPCChannelsUniqueIdCounter = 1;

int s_iRubyIPCChannelsCount = 0;

static int s_iRubyIPCCountReadErrors = 0;

typedef struct
{
    long type;
    char data[IPC_CHANNEL_MAX_MSG_SIZE];
    // byte 0...3: CRC
    // byte 4: message type
    // byte 5..6: message data length
    // byte 7...: data
} type_ipc_message_buffer;


char* _ruby_ipc_get_channel_name(int nChannelType)
{
   static char s_szRubyIPCChannelType[128];
   s_szRubyIPCChannelType[0] = 0;
   sprintf(s_szRubyIPCChannelType, "[Unknown Channel Type %d]", nChannelType);

   if ( nChannelType == IPC_CHANNEL_TYPE_ROUTER_TO_CENTRAL )
      strcpy(s_szRubyIPCChannelType, "[ROUTER-TO-CENTRAL]");

   if ( nChannelType == IPC_CHANNEL_TYPE_CENTRAL_TO_ROUTER )
      strcpy(s_szRubyIPCChannelType, "[CENTRAL-TO-ROUTER]");

   if ( nChannelType == IPC_CHANNEL_TYPE_ROUTER_TO_TELEMETRY )
      strcpy(s_szRubyIPCChannelType, "[ROUTER-TO-TELEMETRY]");

   if ( nChannelType == IPC_CHANNEL_TYPE_TELEMETRY_TO_ROUTER )
      strcpy(s_szRubyIPCChannelType, "[TELEMETRY-TO-ROUTER]");

   if ( nChannelType == IPC_CHANNEL_TYPE_ROUTER_TO_RC )
      strcpy(s_szRubyIPCChannelType, "[ROUTER-TO-RC]");

   if ( nChannelType == IPC_CHANNEL_TYPE_RC_TO_ROUTER )
      strcpy(s_szRubyIPCChannelType, "[RC-TO-ROUTER]");

   if ( nChannelType == IPC_CHANNEL_TYPE_ROUTER_TO_COMMANDS )
      strcpy(s_szRubyIPCChannelType, "[ROUTER-TO-COMMANDS]");

   if ( nChannelType == IPC_CHANNEL_TYPE_COMMANDS_TO_ROUTER )
      strcpy(s_szRubyIPCChannelType, "[COMMANDS-TO-ROUTER]");

   return s_szRubyIPCChannelType;
}

char* _ruby_ipc_get_pipe_name(int nChannelType )
{
   static char s_szRubyPipeName[256];
   s_szRubyPipeName[0] = 0;

   if ( nChannelType == IPC_CHANNEL_TYPE_ROUTER_TO_CENTRAL )
      strcpy(s_szRubyPipeName, FIFO_RUBY_ROUTER_TO_CENTRAL);

   if ( nChannelType == IPC_CHANNEL_TYPE_CENTRAL_TO_ROUTER )
      strcpy(s_szRubyPipeName, FIFO_RUBY_CENTRAL_TO_ROUTER);

   if ( nChannelType == IPC_CHANNEL_TYPE_ROUTER_TO_TELEMETRY )
      strcpy(s_szRubyPipeName, FIFO_RUBY_ROUTER_TO_TELEMETRY);

   if ( nChannelType == IPC_CHANNEL_TYPE_TELEMETRY_TO_ROUTER )
      strcpy(s_szRubyPipeName, FIFO_RUBY_TELEMETRY_TO_ROUTER);

   if ( nChannelType == IPC_CHANNEL_TYPE_ROUTER_TO_RC )
      strcpy(s_szRubyPipeName, FIFO_RUBY_ROUTER_TO_RC);

   if ( nChannelType == IPC_CHANNEL_TYPE_RC_TO_ROUTER )
      strcpy(s_szRubyPipeName, FIFO_RUBY_RC_TO_ROUTER);

   if ( nChannelType == IPC_CHANNEL_TYPE_ROUTER_TO_COMMANDS )
      strcpy(s_szRubyPipeName, FIFO_RUBY_ROUTER_TO_COMMANDS);

   if ( nChannelType == IPC_CHANNEL_TYPE_COMMANDS_TO_ROUTER )
      strcpy(s_szRubyPipeName, FIFO_RUBY_COMMANDS_TO_ROUTER);

   return s_szRubyPipeName;
}


void _ruby_ipc_log_channels()
{
   log_line("[IPC] Currently opened channels: %d:", s_iRubyIPCChannelsCount);
   for( int i=0; i<s_iRubyIPCChannelsCount; i++ )
   {
      log_line("[IPC] Channel %d: unique id: %d, fd: %d, type: %s, key: 0x%x",
         i+1, s_iRubyIPCChannelsUniqueIds[i], s_iRubyIPCChannelsFd[i],
         _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[i]), (u32)s_uRubyIPCChannelsKeys[i]);
   }
}

void _ruby_ipc_log_channel_info(int iChannelType, int iChannelId, int iChannelFd)
{
   if ( iChannelFd < 0 )
      return;
   struct msqid_ds msg_stats;
   if ( 0 != msgctl(iChannelFd, IPC_STAT, &msg_stats) )
      log_softerror_and_alarm("[IPC] Failed to get statistics on ICP message queue %s, id %d, fd %d",
        _ruby_ipc_get_channel_name(iChannelType), iChannelId, iChannelFd );
   else
      log_line("[IPC] Channel %s (id: %d, fd: %d) info: %u pending messages, %u used bytes, max bytes in the IPC channel: %u bytes",
         _ruby_ipc_get_channel_name(iChannelType), iChannelId,
         iChannelFd, (u32)msg_stats.msg_qnum, (u32)msg_stats.msg_cbytes, (u32)msg_stats.msg_qbytes);
}

void _check_ruby_ipc_consistency()
{
   for( int i=0; i<s_iRubyIPCChannelsCount-1; i++ )
   {
      for ( int k=i+1; k<s_iRubyIPCChannelsCount; k++ )
      {
         #ifdef RUBY_USES_MSGQUEUES
         if ( s_uRubyIPCChannelsKeys[i] == s_uRubyIPCChannelsKeys[k] )
            log_error_and_alarm("[IPC] Duplicate key for IPC channels %d and %d, %s and %s.", i, k, _ruby_ipc_get_pipe_name(s_iRubyIPCChannelsType[i]), _ruby_ipc_get_pipe_name(s_iRubyIPCChannelsType[k]));
         #endif
         if ( s_iRubyIPCChannelsFd[i] == s_iRubyIPCChannelsFd[k] )
            log_error_and_alarm("[IPC] Duplicate fd for IPC channels %d and %d, %s and %s.", i, k, _ruby_ipc_get_pipe_name(s_iRubyIPCChannelsType[i]), _ruby_ipc_get_pipe_name(s_iRubyIPCChannelsType[k]));
         if ( s_iRubyIPCChannelsType[i] == s_iRubyIPCChannelsType[k] )
            log_error_and_alarm("[IPC] Duplicate channel type for IPC channels %d and %d, %s and %s.", i, k, _ruby_ipc_get_pipe_name(s_iRubyIPCChannelsType[i]), _ruby_ipc_get_pipe_name(s_iRubyIPCChannelsType[k]));
      }
   }
}


int ruby_init_ipc_channels()
{
   #if defined(HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA)
   char szBuff[256];
   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_CAMERA1);
   hw_execute_bash_command(szBuff, NULL);
      
   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_AUDIO1);
   hw_execute_bash_command(szBuff, NULL);

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_AUDIO_BUFF);
   hw_execute_bash_command(szBuff, NULL);

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_AUDIO_QUEUE);
   hw_execute_bash_command(szBuff, NULL);

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_STATION_VIDEO_STREAM_DISPLAY);
   hw_execute_bash_command(szBuff, NULL);

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_STATION_VIDEO_STREAM_RECORDING);
   hw_execute_bash_command(szBuff, NULL);

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_STATION_VIDEO_STREAM_ETH);
   hw_execute_bash_command(szBuff, NULL);
   #endif

   #ifdef RUBY_USE_FIFO_PIPES

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_ROUTER_TO_CENTRAL );
   hw_execute_bash_command(szBuff, NULL);

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_CENTRAL_TO_ROUTER );
   hw_execute_bash_command(szBuff, NULL);

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_ROUTER_TO_COMMANDS );
   hw_execute_bash_command(szBuff, NULL);

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_COMMANDS_TO_ROUTER);
   hw_execute_bash_command(szBuff, NULL);

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_ROUTER_TO_TELEMETRY );
   hw_execute_bash_command(szBuff, NULL);

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_TELEMETRY_TO_ROUTER );
   hw_execute_bash_command(szBuff, NULL);

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_ROUTER_TO_RC );
   hw_execute_bash_command(szBuff, NULL);

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_RC_TO_ROUTER );
   hw_execute_bash_command(szBuff, NULL);

   #endif

   return 1;
}

void ruby_clear_all_ipc_channels()
{
   log_line("[IPC] Clearing all IPC channels...");
   
   #ifdef RUBY_USES_MSGQUEUES

   for( int i=0; i<s_iRubyIPCChannelsCount; i++ )
   {
      int iRes = msgctl(s_iRubyIPCChannelsFd[i],IPC_RMID,NULL);
      if ( iRes < 0 )
         log_softerror_and_alarm("[IPC] Failed to remove msgque [%s], error code: %d, error: %s",
          _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[i]), errno, strerror(errno));
   }
   s_iRubyIPCChannelsCount = 0;

   #endif

   log_line("[IPC] Done clearing all IPC channels.");
}

int ruby_open_ipc_channel_write_endpoint(int nChannelType)
{
   for( int i=0; i<s_iRubyIPCChannelsCount; i++ )
      if ( s_iRubyIPCChannelsType[i] == nChannelType )
      if ( s_iRubyIPCChannelsFd[i] > 0 )
      if ( s_iRubyIPCChannelsUniqueIds[i] > 0 )
         return s_iRubyIPCChannelsUniqueIds[i];

   if ( s_iRubyIPCChannelsCount >= MAX_CHANNELS )
   {
      log_error_and_alarm("[IPC] Can't open %s channel. No more IPC channels. List full.", _ruby_ipc_get_channel_name(nChannelType));
      return 0;
   }

   s_iRubyIPCChannelsType[s_iRubyIPCChannelsCount] = nChannelType;
   s_uRubyIPCChannelsMsgId[s_iRubyIPCChannelsCount] = 0;

   #ifdef RUBY_USE_FIFO_PIPES

   char* szPipeName = _ruby_ipc_get_pipe_name(nChannelType);
   if ( NULL == szPipeName || 0 == szPipeName[0] )
   {
      log_error_and_alarm("[IPC] Can't open IPC FIFO Pipe with invalid name (requested type: %d).", nChannelType);
      return 0;
   }

   s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount] = open(szPipeName, O_WRONLY | (RUBY_PIPES_EXTRA_FLAGS & (~O_NONBLOCK)));
   if ( s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount] < 0 )
   {
      log_error_and_alarm("[IPC] Failed to open IPC channel %s pipe write endpoint.", _ruby_ipc_get_channel_name(nChannelType));
      return 0;
   }

   if ( RUBY_PIPES_EXTRA_FLAGS & O_NONBLOCK )
   if ( 0 != fcntl(s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount], F_SETFL, O_NONBLOCK) )
      log_softerror_and_alarm("[IPC] Failed to set nonblock flag on PIC channel %s pipe write endpoint.", _ruby_ipc_get_channel_name(nChannelType));

   log_line("[IPC] FIFO write endpoint pipe flags: %s", str_get_pipe_flags(fcntl(s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount], F_GETFL)));
   
   #endif

   #ifdef RUBY_USES_MSGQUEUES

   key_t key = generate_msgqueue_key(nChannelType);

   if ( key < 0 )
   {
      log_softerror_and_alarm("[IPC] Failed to generate message queue key for channel %s. Error: %d, %s",
         _ruby_ipc_get_channel_name(nChannelType),
         errno, strerror(errno));
      return 0;
   }
   s_uRubyIPCChannelsKeys[s_iRubyIPCChannelsCount] = key;
   s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount] = msgget(key, IPC_CREAT | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

   if ( s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount] < 0 )
   {
      log_softerror_and_alarm("[IPC] Failed to create IPC message queue write endpoint for channel %s, error %d, %s",
         _ruby_ipc_get_channel_name(nChannelType), errno, strerror(errno));
      log_line("%d %d %d", ENOENT, EACCES, ENOTDIR);
      return -1;
   }

   //struct msqid_ds msg_stats;
   //if ( 0 != msgctl(s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount], IPC_STAT, &msg_stats) )
   //   log_softerror_and_alarm("[IPC] Failed to get statistics on ICP message queue %s", _ruby_ipc_get_channel_name(nChannelType) );
   //else
   //   log_line("[IPC] FD: %d, id: %u, Max bytes in the IPC channel %s: %u bytes", s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount], key, _ruby_ipc_get_channel_name(nChannelType), (u32)msg_stats.msg_qbytes);

   //struct msginfo msg_info;
   //if ( 0 != msgctl(s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount], IPC_INFO, (struct msqid_ds*)&msg_info) )
   //   log_softerror_and_alarm("[IPC] Failed to get statistics on system message queues." );
   //else
   //   log_line("[IPC] IPC channels pools max: %u bytes, max msg size: %u bytes, max msg queue total size: %u bytes", (u32)msg_info.msgpool, (u32)msg_info.msgmax, (u32)msg_info.msgmnb);

   #endif

   s_iRubyIPCChannelsUniqueIds[s_iRubyIPCChannelsCount] = s_iRubyIPCChannelsUniqueIdCounter;
   s_iRubyIPCChannelsUniqueIdCounter++;

   s_iRubyIPCChannelsCount++;
   
   log_line("[IPC] Opened IPC channel %s write endpoint: success, fd: %d, id: %d. (%d channels currently opened).",
      _ruby_ipc_get_channel_name(nChannelType), s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount-1], s_iRubyIPCChannelsUniqueIds[s_iRubyIPCChannelsCount-1], s_iRubyIPCChannelsCount);
   _ruby_ipc_log_channel_info(nChannelType, s_iRubyIPCChannelsUniqueIds[s_iRubyIPCChannelsCount-1], s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount-1]);
   _check_ruby_ipc_consistency();
   _ruby_ipc_log_channels();
   return s_iRubyIPCChannelsUniqueIds[s_iRubyIPCChannelsCount-1];
}

int ruby_open_ipc_channel_read_endpoint(int nChannelType)
{
   for( int i=0; i<s_iRubyIPCChannelsCount; i++ )
      if ( s_iRubyIPCChannelsType[i] == nChannelType )
      if ( s_iRubyIPCChannelsFd[i] > 0 )
      if ( s_iRubyIPCChannelsUniqueIds[i] > 0 )
         return s_iRubyIPCChannelsUniqueIds[i];

   if ( s_iRubyIPCChannelsCount >= MAX_CHANNELS )
   {
      log_error_and_alarm("[IPC] Can't open %s channel. No more IPC channels. List full.", _ruby_ipc_get_channel_name(nChannelType));
      return 0;
   }

   s_iRubyIPCChannelsType[s_iRubyIPCChannelsCount] = nChannelType;
   s_uRubyIPCChannelsMsgId[s_iRubyIPCChannelsCount] = 0;

   #ifdef RUBY_USE_FIFO_PIPES

   char* szPipeName = _ruby_ipc_get_pipe_name(nChannelType);
   if ( NULL == szPipeName || 0 == szPipeName[0] )
   {
      log_error_and_alarm("[IPC] Can't open IPC FIFO Pipe with invalid name (requested type: %d).", nChannelType);
      return 0;
   }

   s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount] = open(szPipeName, O_RDONLY | RUBY_PIPES_EXTRA_FLAGS);

   if ( s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount] < 0 )
   {
      log_error_and_alarm("[IPC] Failed to open IPC channel %s read endpoint.", _ruby_ipc_get_channel_name(nChannelType));
      return 0;
   }

   log_line("[IPC] FIFO read endpoint pipe flags: %s", str_get_pipe_flags(fcntl(s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount], F_GETFL)));

   #endif

   #ifdef RUBY_USES_MSGQUEUES

   key_t key = generate_msgqueue_key(nChannelType);

   if ( key < 0 )
   {
      log_softerror_and_alarm("[IPC] Failed to generate message queue key for channel %s. Error: %d, %s",
         _ruby_ipc_get_channel_name(nChannelType),
         errno, strerror(errno));
      log_line("%d %d %d", ENOENT, EACCES, ENOTDIR);
      return 0;
   }

   s_uRubyIPCChannelsKeys[s_iRubyIPCChannelsCount] = key;
   s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount] = msgget(key, IPC_CREAT | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
   if ( s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount] < 0 )
   {
      log_softerror_and_alarm("[IPC] Failed to create IPC message queue read endpoint for channel %s, error %d, %s",
         _ruby_ipc_get_channel_name(nChannelType), errno, strerror(errno));
      return -1;
   }

   //struct msqid_ds msg_stats;
   //if ( 0 != msgctl(s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount], IPC_STAT, &msg_stats) )
   //   log_softerror_and_alarm("[IPC] Failed to get statistics on ICP message queue %s", _ruby_ipc_get_channel_name(nChannelType) );
   //else
   //   log_line("[IPC] FD: %d, key: %u, Max bytes in the IPC channel %s: %u bytes", s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount], key, _ruby_ipc_get_channel_name(nChannelType), (u32)msg_stats.msg_qbytes);

   //struct msginfo msg_info;
   //if ( 0 != msgctl(s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount], IPC_INFO, (struct msqid_ds*)&msg_info) )
   //   log_softerror_and_alarm("[IPC] Failed to get statistics on system message queues." );
   //else
   //   log_line("[IPC] IPC channels pools max: %u bytes, max msg size: %u bytes, max msg queue total size: %u bytes", (u32)msg_info.msgpool, (u32)msg_info.msgmax, (u32)msg_info.msgmnb);
   #endif

   s_iRubyIPCChannelsUniqueIds[s_iRubyIPCChannelsCount] = s_iRubyIPCChannelsUniqueIdCounter;
   s_iRubyIPCChannelsUniqueIdCounter++;

   s_iRubyIPCChannelsCount++;
   
   log_line("[IPC] Opened IPC channel %s read endpoint: success, fd: %d, id: %d. (%d channels currently opened).",
      _ruby_ipc_get_channel_name(nChannelType), s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount-1], s_iRubyIPCChannelsUniqueIds[s_iRubyIPCChannelsCount-1], s_iRubyIPCChannelsCount);
   _ruby_ipc_log_channel_info(nChannelType, s_iRubyIPCChannelsUniqueIds[s_iRubyIPCChannelsCount-1], s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount-1]);
   _check_ruby_ipc_consistency();
   _ruby_ipc_log_channels();
   return s_iRubyIPCChannelsUniqueIds[s_iRubyIPCChannelsCount-1];
}

int ruby_close_ipc_channel(int iChannelUniqueId)
{
   int fdToClose = 0;
   int iChannelIndex = -1;
   for( int i=0; i<s_iRubyIPCChannelsCount; i++ )
   {
      if ( s_iRubyIPCChannelsUniqueIds[i] == iChannelUniqueId )
      {
         fdToClose = s_iRubyIPCChannelsFd[i];
         iChannelIndex = i;
         break;
      }
   }

   if ( (iChannelUniqueId < 0) || (fdToClose < 0) || (-1 == iChannelIndex) )
   {
      log_softerror_and_alarm("[IPC] Tried to close invalid IPC channel.");
      return 0;
   }

   if ( fdToClose == 0 )
      log_softerror_and_alarm("[IPC] Warning: closing invalid fd 0 for unique channel %d, channel index %d, (%s)",
       iChannelUniqueId, iChannelIndex, _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iChannelIndex]));

   #ifdef RUBY_USE_FIFO_PIPES
   close(fdToClose);
   #endif

   #ifdef RUBY_USES_MSGQUEUES
   msgctl(fdToClose,IPC_RMID,NULL);
   #endif


   log_line("[IPC] Closed IPC channel %s, channel index %d, unique id %d, fd %d",
       _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iChannelIndex]),
       iChannelIndex, iChannelUniqueId, fdToClose);
   for( int k=iChannelIndex; k<s_iRubyIPCChannelsCount-1; k++ )
   {
      s_iRubyIPCChannelsFd[k] = s_iRubyIPCChannelsFd[k+1];
      s_uRubyIPCChannelsKeys[k] = s_uRubyIPCChannelsKeys[k+1];
      s_iRubyIPCChannelsType[k] = s_iRubyIPCChannelsType[k+1];
      s_iRubyIPCChannelsUniqueIds[k] = s_iRubyIPCChannelsUniqueIds[k+1];
      s_uRubyIPCChannelsMsgId[k] = s_uRubyIPCChannelsMsgId[k+1];

   }
   s_iRubyIPCChannelsCount--;
  
   _ruby_ipc_log_channels();
   return 1;
}

int ruby_ipc_channel_send_message(int iChannelUniqueId, u8* pMessage, int iLength)
{
   if ( iChannelUniqueId < 0 || s_iRubyIPCChannelsCount == 0 )
   {
      log_softerror_and_alarm("[IPC] Tried to write a message to an invalid channel (unique id %d)", iChannelUniqueId );
      return 0;
   }

   int iFoundIndex = -1;
   int iChannelFd = 0;
   //int iChannelType = 0;
   //u8 uChannelMsgId = 0;
   for( int i=0; i<s_iRubyIPCChannelsCount; i++ )
   {
      if ( s_iRubyIPCChannelsUniqueIds[i] == iChannelUniqueId )
      {
         iChannelFd = s_iRubyIPCChannelsFd[i];
         //iChannelType = s_iRubyIPCChannelsType[i];
         //uChannelMsgId = s_uRubyIPCChannelsMsgId[i];
         iFoundIndex = i;
         break;
      }
   }

   if ( iFoundIndex == -1 )
   {
      log_softerror_and_alarm("[IPC] Tried to write a message to an invalid channel (unique id %d) not in the list (%d channels active now).", iChannelUniqueId, s_iRubyIPCChannelsCount);
      return 0;
   }

   if ( iChannelFd < 0 )
   {
      log_softerror_and_alarm("[IPC] Tried to write a message to an invalid channel fd %d (unique id %d)", iChannelFd, iChannelUniqueId);
      return 0;
   }

   if ( NULL == pMessage || 0 == iLength )
   {
      log_softerror_and_alarm("[IPC] Tried to write an invalid message (null or 0) on channel %s, channel unique id %d", _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFoundIndex]), iChannelUniqueId );
      return 0;
   }

   if ( iLength >= IPC_CHANNEL_MAX_MSG_SIZE-6 )
   {
      log_softerror_and_alarm("[IPC] Tried to write a message too big (%d bytes) on channel %s, channel unique id %d", iLength, _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFoundIndex]), iChannelUniqueId );
      return 0;
   }

   u32 crc = base_compute_crc32(pMessage + sizeof(u32), iLength-sizeof(u32)); 
   u32* pTmp = (u32*)pMessage;
   *pTmp = crc;

   int res = 0;
   
   #ifdef PROFILE_IPC
   u32 uTimeStart = get_current_timestamp_ms();
   #endif

   s_uRubyIPCChannelsMsgId[iFoundIndex]++;

   #ifdef RUBY_USE_FIFO_PIPES
   res = write(iChannelFd, pMessage, iLength);
   #endif

   #ifdef RUBY_USES_MSGQUEUES
   
   type_ipc_message_buffer msg;

   msg.type = 1;
   msg.data[4] = s_uRubyIPCChannelsMsgId[iFoundIndex];
   msg.data[5] = ((u32)iLength) & 0xFF; 
   msg.data[6] = (((u32)iLength)>>8) & 0xFF;
   memcpy((u8*)&(msg.data[7]), pMessage, iLength); 
   u32 uCRC = base_compute_crc32((u8*)&(msg.data[4]), iLength+3);
   memcpy((u8*)&(msg.data[0]), (u8*)&uCRC, sizeof(u32));

   int iRetryWriteOnly = 0;
   int iRetryCounter = 2;
   int iRetriedToRecreate = 0;

   do
   {
      if ( 0 == msgsnd(iChannelFd, &msg, iLength + 7, IPC_NOWAIT) )
      {
         if ( iRetryWriteOnly )
            log_line("[IPC] Succeded to send message after write retry.");
         if ( iRetriedToRecreate )
            log_line("[IPC] Succeded to send message after recreation of the channel.");
         //log_line("[IPC] Sent message ok to %s, id: %d, %d bytes, CRC: %u", _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFoundIndex]), msg.data[4], iLength, uCRC);
         res = iLength;
         iRetryCounter = 0;
         break;
         //log_line("[IPC] Send IPC message on channel %s: %d bytes, crc: %u", _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFoundIndex]), iLength, uCRC);
      }
   
      res = 0;

      iRetryWriteOnly = 0;
      if ( errno == EAGAIN )
      {
         log_softerror_and_alarm("[IPC] Failed to write to IPC %s, error code: EAGAIN", _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFoundIndex]) );
         iRetryWriteOnly = 1;
      }
      if ( errno == EACCES )
         log_softerror_and_alarm("[IPC] Failed to write to IPC %s, error code: EACCESS", _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFoundIndex]) );
      if ( errno == EIDRM )
         log_softerror_and_alarm("[IPC] Failed to write to IPC %s, error code: EIDRM", _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFoundIndex]) );
      if ( errno == EINVAL )
         log_softerror_and_alarm("[IPC] Failed to write to IPC %s, error code: EINVAL", _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFoundIndex]) );


      struct msqid_ds msg_stats;
      if ( 0 != msgctl(iChannelFd, IPC_STAT, &msg_stats) )
         log_softerror_and_alarm("[IPC] Failed to get statistics on ICP message queue %s, fd %d, unique id %d",
           _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFoundIndex]), iChannelFd, iChannelUniqueId );
      else
      {
         log_line("[IPC] Channel %s (fd %d, unique id %d) info: %u pending messages, %u used bytes, max bytes in the IPC channel: %u bytes",
            _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFoundIndex]),
            iChannelFd, iChannelUniqueId,
            (u32)msg_stats.msg_qnum, (u32)msg_stats.msg_cbytes, (u32)msg_stats.msg_qbytes);

         if ( msg_stats.msg_cbytes > (msg_stats.msg_qbytes * 80)/100 )
            iRetryWriteOnly = 1;
      }

      if ( iRetryWriteOnly )
         log_line("[IPC] Retry write operation only (%d)...", iRetryCounter);
      else
         break;
      /*else if ( 0 == iRetriedToRecreate )
      {
         t_packet_header* pPH = (t_packet_header*)pMessage;
         log_softerror_and_alarm("[IPC] Failed to write a message (id: %d, %d bytes) on channel %s, ch unique id %d. Only %d bytes written (Message component: %d, msg type: %d, msg length:%d).",
            msg.data[4], iLength, _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFoundIndex]), iChannelUniqueId, res, (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE), pPH->packet_type, pPH->total_length);
         log_line("[IPC] Try to recreate channel, unique id: %d, fd %d", iChannelUniqueId, iChannelFd);
         ruby_close_ipc_channel(iChannelUniqueId);
         ruby_open_ipc_channel_write_endpoint(iChannelType);
         iFoundIndex = s_iRubyIPCChannelsCount-1;
         s_iRubyIPCChannelsUniqueIds[iFoundIndex] = iChannelUniqueId;
         s_uRubyIPCChannelsMsgId[iFoundIndex] = uChannelMsgId-1;
         iChannelFd = s_iRubyIPCChannelsFd[iFoundIndex];
         log_line("[IPC] Recreated channel, unique id: %d, fd %d, type: %s",
             s_iRubyIPCChannelsUniqueIds[iFoundIndex],
             s_iRubyIPCChannelsFd[iFoundIndex],
             _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFoundIndex]));

         iRetriedToRecreate = 1;
      }
      else
         log_line("[IPC] Retry write operation (%d)...", iRetryCounter);
      */
      iRetryCounter--;
      hardware_sleep_ms(10);

   } while (iRetryCounter > 0);
   #endif

   #ifdef PROFILE_IPC
   u32 uTimeTotal = get_current_timestamp_ms() - uTimeStart;
   if ( uTimeTotal > PROFILE_IPC_MAX_TIME )
   {
      t_packet_header* pPH = (t_packet_header*)pMessage;
      log_softerror_and_alarm("[IPC] Write message (id: %d, %d bytes) on channel %s took too long (%u ms) (Message component: %d, msg type: %d, msg length:%d).", msg.data[4], iLength, _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFoundIndex]), uTimeTotal, (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE), pPH->packet_type, pPH->total_length);
   }
   #endif

   return res;
}


u8* ruby_ipc_try_read_message(int iChannelUniqueId, u8* pTempBuffer, int* pTempBufferPos, u8* pOutputBuffer)
{
   if ( iChannelUniqueId < 0 || s_iRubyIPCChannelsCount == 0 )
   {
      log_softerror_and_alarm("[IPC] Tried to read a message from an invalid channel (unique id %d)", iChannelUniqueId );
      return NULL;
   }

   int iFoundIndex = -1;
   int iChannelFd = 0;
   int iChannelType = 0;
   for( int i=0; i<s_iRubyIPCChannelsCount; i++ )
   {
      if ( s_iRubyIPCChannelsUniqueIds[i] == iChannelUniqueId )
      {
         iChannelFd = s_iRubyIPCChannelsFd[i];
         iChannelType = s_iRubyIPCChannelsType[i];
         iFoundIndex = i;
         break;
      }
   }

   if ( iFoundIndex == -1 )
   {
      log_softerror_and_alarm("[IPC] Tried to read a message from an invalid channel (unique id %d) not in the list (%d channels active now).", iChannelUniqueId, s_iRubyIPCChannelsCount);
      return NULL;
   }

   if ( NULL == pTempBuffer || NULL == pTempBufferPos || NULL == pOutputBuffer )
   {
      log_softerror_and_alarm("[IPC] Tried to read a message into a NULL buffer on channel %s", _ruby_ipc_get_channel_name(iChannelType) );
      return NULL;
   }

   u8* pReturn = NULL;
   int lenReadIPCMsgQueue = 0;

   #ifdef PROFILE_IPC
   u32 uTimeStart = get_current_timestamp_ms();
   #endif

   #ifdef RUBY_USE_FIFO_PIPES

   t_packet_header* pPH = (t_packet_header*)pTempBuffer;

   if ( (*pTempBufferPos) >= (int)sizeof(t_packet_header) )
   {
      if ( pPH->total_length > MAX_PACKET_TOTAL_SIZE )
         *pTempBufferPos = 0;
   }

   if ( ((*pTempBufferPos) >= (int)sizeof(t_packet_header)) && (*pTempBufferPos) >= pPH->total_length )
   {
      if ( ! base_check_crc32( pTempBuffer, pPH->total_length ) )
      {
         log_softerror_and_alarm("[IPC] Invalid read buffers for channel [%s]. Reseting read buffer to 0.", _ruby_ipc_get_channel_name(iChannelType));
         // invalid data. just reset buffers.
         *pTempBufferPos = 0;
      }
      else
      {
         //log_line("Has data in pipe already");
         //log_buffer(pTempBuffer, 40);
         //log_line(" <<< Read existing packet from pipe: type: %d, component: %d, length: %d", pPH->packet_type, pPH->packet_flags & PACKET_FLAGS_MASK_MODULE, pPH->total_length);
         int len = pPH->total_length;
         memcpy(pOutputBuffer, pTempBuffer, len);
         *pTempBufferPos -= len;
         for( int i=len; i<MAX_PACKET_TOTAL_SIZE; i++ )
            pTempBuffer[i-len] = pTempBuffer[i];
         pReturn = pOutputBuffer;
      }
   }
   else
   {
      fd_set readset; 
      struct timeval timePipeInput;
      
      FD_ZERO(&readset);
      FD_SET(iChannelFd, &readset);

      timePipeInput.tv_sec = 0;
      timePipeInput.tv_usec = 0;

      int selectResult = select(iChannelFd+1, &readset, NULL, NULL, &timePipeInput);
      if ( selectResult > 0 && (FD_ISSET(iChannelFd, &readset)) )
      {
         int count = read(iChannelFd, pTempBuffer+(*pTempBufferPos), MAX_PACKET_TOTAL_SIZE-(*pTempBufferPos));
         if ( count < 0 )
         {
             log_error_and_alarm("[IPC] Failed to read FIFO pipe %s. Broken pipe.", _ruby_ipc_get_channel_name(iChannelType) );
         }
         else
         {
            *pTempBufferPos += count;

            if ( ((*pTempBufferPos) >= (int)sizeof(t_packet_header)) && (*pTempBufferPos) >= pPH->total_length )
            {
               if ( ! base_check_crc32( pTempBuffer, pPH->total_length ) )
               {
                  log_softerror_and_alarm("[IPC] Invalid read buffers for channel [%s]. Reseting read buffer to 0.", _ruby_ipc_get_channel_name(iChannelType));
                  // invalid data. just reset buffers.
                  *pTempBufferPos = 0;
               }
               else
               {
                  int len = pPH->total_length;
                  memcpy(pOutputBuffer, pTempBuffer, len);
                  *pTempBufferPos -= len;
                  for( int i=len; i<MAX_PACKET_TOTAL_SIZE; i++ )
                     pTempBuffer[i-len] = pTempBuffer[i];
                  pReturn = pOutputBuffer;
               }
            }
         }
      }
   }
   #endif

   #ifdef RUBY_USES_MSGQUEUES

   type_ipc_message_buffer ipcMessage;

   lenReadIPCMsgQueue = msgrcv(iChannelFd, &ipcMessage, IPC_CHANNEL_MAX_MSG_SIZE, 0, MSG_NOERROR | IPC_NOWAIT);
   if ( lenReadIPCMsgQueue > 6 )
   {
      int iMsgLen = ipcMessage.data[5] + 256*(int)ipcMessage.data[6];
      if ( iMsgLen <= 0 || iMsgLen >= IPC_CHANNEL_MAX_MSG_SIZE - 6 )
         log_softerror_and_alarm("[IPC] Received invalid message on channel %s, id: %d, length: %d", _ruby_ipc_get_channel_name(iChannelType), ipcMessage.data[4], iMsgLen );
      else
      {
         u32 uCRC = base_compute_crc32((u8*)&(ipcMessage.data[4]), iMsgLen+3);
         u32 uTmp = 0;
         memcpy((u8*)&uTmp, (u8*)&(ipcMessage.data[0]), sizeof(u32));
         if ( uCRC != uTmp )
            log_softerror_and_alarm("[IPC] Received invalid CRC on channel %s on message id: %d, CRC computed/received: %u / %u, msg length: %d", _ruby_ipc_get_channel_name(iChannelType), ipcMessage.data[4], uCRC, uTmp, iMsgLen );
         else
         {
            memcpy(pOutputBuffer, (u8*)&(ipcMessage.data[7]), iMsgLen);
            pReturn = pOutputBuffer;
            //log_line("[IPC] Received message ok on channel %s, id: %d, length: %d", _ruby_ipc_get_channel_name(iChannelType), ipcMessage.data[4], iMsgLen );
         }
      }
   }

   #endif

   #ifdef PROFILE_IPC
   u32 uTimeTotal = get_current_timestamp_ms() - uTimeStart;
   if ( (uTimeTotal > PROFILE_IPC_MAX_TIME + timeoutMicrosec/1000) || uTimeTotal >= 50 )
   {
      s_iRubyIPCCountReadErrors++;
      if ( lenReadIPCMsgQueue >= 0 )
         log_softerror_and_alarm("[IPC] Read message (%d bytes) from channel %s took too long (%u ms). Error count: %d", lenReadIPCMsgQueue, _ruby_ipc_get_channel_name(iChannelType), uTimeTotal, s_iRubyIPCCountReadErrors );
      else
         log_softerror_and_alarm("[IPC] Try read message from channel %s took too long (%u ms). No message read. Error count: %d", _ruby_ipc_get_channel_name(iChannelType), uTimeTotal, s_iRubyIPCCountReadErrors );
   }
   else
      s_iRubyIPCCountReadErrors = 0;
   #endif

   return pReturn;
}

int ruby_ipc_get_read_continous_error_count()
{
   return s_iRubyIPCCountReadErrors;
}