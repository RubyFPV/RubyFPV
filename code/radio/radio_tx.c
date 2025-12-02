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
#include "../base/hardware_procs.h"
#include "../base/hardware_radio_sik.h"
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>

#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "radio_tx.h"
#include "radiolink.h"
#include "radio_duplicate_det.h"

typedef struct
{
    long type; // type will be the local radio interface index to use for sending the message
    char data[MAX_PACKET_TOTAL_SIZE];
} type_ipc_message_tx_packet_buffer;



int s_iRadioTxInitialized = 0;
int s_iRadioTxSignalStop = 0;
int s_iRadioTxMarkedForQuit = 0;
int s_iRadioTxDevMode = 0;
int s_iRadioTxIPCQueue = -1;
int s_iRadioTxSiKPacketSize = DEFAULT_SIK_PACKET_SIZE;
int s_iRadioTxSerialPacketSize[MAX_RADIO_INTERFACES];
int s_iRadioTxSerialPacketSizeInitialized = 0;
int s_iRadioTxInterfacesPaused[MAX_RADIO_INTERFACES];

int s_iCurrentTxThreadRawPriority = -1;
int s_iPendingTxThreadRawPriority = -1;

pthread_t s_pThreadRadioTx;
pthread_mutex_t s_pThreadRadioTxMutex;

int _radio_tx_create_msg_queue()
{
   key_t key = generate_msgqueue_key(RADIO_TX_MESSAGE_QUEUE_ID);

   if ( key < 0 )
   {
      log_softerror_and_alarm("[RadioTx] Failed to generate message queue key for ipc channel. Error: %d, %s",
         errno, strerror(errno));
      log_line("%d %d %d", ENOENT, EACCES, ENOTDIR);

      char szFile[MAX_FILE_PATH_SIZE];
      strcpy(szFile, FOLDER_BINARIES);
      strcat(szFile, "ruby_start");
      key = ftok(szFile, 107);

      if ( key < 0 )
      {
         log_softerror_and_alarm("[RadioTx] Failed to generate (2) message queue key for ipc channel. Error: %d, %s",
            errno, strerror(errno));
         log_line("%d %d %d", ENOENT, EACCES, ENOTDIR);
         return 0;
      }
   }

   s_iRadioTxIPCQueue = msgget(key, IPC_CREAT | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

   if ( s_iRadioTxIPCQueue < 0 )
   {
      log_softerror_and_alarm("[RadioTx] Failed to create IPC message queue, error %d, %s",
         errno, strerror(errno));
      log_line("%d %d %d", ENOENT, EACCES, ENOTDIR);
      return 0;
   }

   log_line("[RadioTx] Created IPC msg queue, fd: %d, key: %x", s_iRadioTxIPCQueue, key);
   return 1;
}

int _radio_tx_send_msg(int iInterfaceIndex, u8* pData, int iLength)
{
   // Split the packet into small serial packets and send it
   // radio packet header is part of the total packet size, so take it into account
   
   t_packet_header_short PHS;
   u8 uBuffer[1000];
   int iTotalBytesSent = 0;

   if ( ! s_iRadioTxSerialPacketSizeInitialized )
   {
      s_iRadioTxSerialPacketSizeInitialized = 1;
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         s_iRadioTxSerialPacketSize[i] = DEFAULT_RADIO_SERIAL_AIR_PACKET_SIZE;
   }

   int iUsableDataBytesInEachPacket = s_iRadioTxSerialPacketSize[iInterfaceIndex] - sizeof(t_packet_header_short);
   if ( hardware_radio_index_is_sik_radio(iInterfaceIndex) )
      iUsableDataBytesInEachPacket = s_iRadioTxSiKPacketSize - sizeof(t_packet_header_short);

   int iBytesLeftToSend = iLength;
   u8* pDataToSend = pData;

   while ( iBytesLeftToSend > 0 )
   {
      radio_packet_short_init(&PHS);

      PHS.start_header = SHORT_PACKET_START_BYTE_REG_PACKET;
      if ( pData == pDataToSend )
         PHS.start_header = SHORT_PACKET_START_BYTE_START_PACKET;
      if ( iBytesLeftToSend <= iUsableDataBytesInEachPacket ) 
         PHS.start_header = SHORT_PACKET_START_BYTE_END_PACKET;

      int iShortPacketDataSize = iUsableDataBytesInEachPacket;
      if ( iBytesLeftToSend <= iUsableDataBytesInEachPacket )
         iShortPacketDataSize = iBytesLeftToSend;

      PHS.packet_id = radio_packets_short_get_next_id_for_radio_interface(iInterfaceIndex);
      PHS.data_length = (u8)iShortPacketDataSize;
      memcpy(uBuffer, (u8*)&PHS, sizeof(t_packet_header_short));
      memcpy(&uBuffer[sizeof(t_packet_header_short)], pDataToSend, iShortPacketDataSize);

      iBytesLeftToSend -= iShortPacketDataSize;
      pDataToSend += iShortPacketDataSize;

      iShortPacketDataSize += sizeof(t_packet_header_short);
      uBuffer[1] = base_compute_crc8(&uBuffer[2], iShortPacketDataSize - 2);
      
      int iWriteResult = 0;
      if ( hardware_radio_index_is_sik_radio(iInterfaceIndex) )
         iWriteResult = radio_write_sik_packet(iInterfaceIndex, uBuffer, iShortPacketDataSize, get_current_timestamp_ms());
      else
         iWriteResult = radio_write_serial_packet(iInterfaceIndex, uBuffer, iShortPacketDataSize, get_current_timestamp_ms());
      if ( iWriteResult != iShortPacketDataSize )
      {
         log_softerror_and_alarm("[RadioTx] Failed to send message to serial radio: sent %d bytes, only %d bytes written.",
            iShortPacketDataSize, iWriteResult);
         if ( iWriteResult > 0 )
            iTotalBytesSent += iWriteResult;
         continue; 
      }
      iTotalBytesSent += iWriteResult;
      hardware_sleep_micros(500);
   }
   return 1;
}

static void * _thread_radio_tx(void *argument)
{
   log_line("[RadioTxThread] Started.");
   hw_log_current_thread_attributes("radio tx");

   if ( s_iPendingTxThreadRawPriority > 0 )
   if ( s_iPendingTxThreadRawPriority != s_iCurrentTxThreadRawPriority )
   {
      hw_set_current_thread_raw_priority("[RadioTxThread]", s_iPendingTxThreadRawPriority);
      s_iCurrentTxThreadRawPriority = s_iPendingTxThreadRawPriority;
   }

   log_line("[RadioTxThread] SiK packet size is: %d bytes (of which %d bytes are the header)", s_iRadioTxSiKPacketSize, (int)sizeof(t_packet_header_short));
   log_line("[RadioTxThread] Initialized State. Waiting for tx messages...");

   int* piQuit = (int*) argument;
   u32 uWaitTime = 2;
   while ( 1 )
   {
      hardware_sleep_ms(uWaitTime);
      if ( uWaitTime < 30 )
         uWaitTime += 5;
      
      if ( (NULL != piQuit) && (*piQuit != 0 ) )
      {
         log_line("[RadioTxThread] Signaled to stop.");
         break;
      }
      if ( s_iRadioTxIPCQueue < 0 )
         continue;

      if ( s_iPendingTxThreadRawPriority != s_iCurrentTxThreadRawPriority )
      {
         log_line("[RadioTxThread] New thread raw priority must be set, from %d to %d.", s_iCurrentTxThreadRawPriority, s_iPendingTxThreadRawPriority);
         s_iCurrentTxThreadRawPriority = s_iPendingTxThreadRawPriority;

         if ( s_iPendingTxThreadRawPriority > 1 )
            hw_set_current_thread_raw_priority("[RadioTxThread]", s_iPendingTxThreadRawPriority);
         else
            hw_set_current_thread_raw_priority("[RadioTxThread]", 0);
      }

      type_ipc_message_tx_packet_buffer ipcMessage;
      int iIPCLength = msgrcv(s_iRadioTxIPCQueue, &ipcMessage, sizeof(ipcMessage), 0, MSG_NOERROR | IPC_NOWAIT);
      if ( iIPCLength <= 2 )
         continue;
      
      if ( iIPCLength > MAX_PACKET_TOTAL_SIZE )
      {
         log_softerror_and_alarm("[RadioTx] Read IPC message too big (%d bytes), skipping it.", iIPCLength);
         continue;
      }

      if ( (ipcMessage.type < 0) || (ipcMessage.type >= hardware_get_radio_interfaces_count()) )
      {
         log_softerror_and_alarm("[RadioTx] Read IPC message for invalid radio interface %d, skipping it.", ipcMessage.type+1);
         continue;
      }

      if ( s_iRadioTxInterfacesPaused[ipcMessage.type] )
         continue;
        
      if ( ! hardware_radio_index_is_serial_radio(ipcMessage.type) )
      {
         log_softerror_and_alarm("[RadioTx] Read IPC message for radio interface %d which is not a serial radio, skipping it.", ipcMessage.type+1);
         continue;
      }

      uWaitTime = 1;

      _radio_tx_send_msg(ipcMessage.type, (u8*)ipcMessage.data, iIPCLength);
      
   }

   log_line("[RadioTxThread] Stopped.");
   return NULL;
}

int radio_tx_start_tx_thread()
{
   if ( s_iRadioTxInitialized )
      return 1;

   s_iRadioTxSignalStop = 0;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      s_iRadioTxInterfacesPaused[i] = 0;

   if ( ! s_iRadioTxSerialPacketSizeInitialized )
   {
      s_iRadioTxSerialPacketSizeInitialized = 1;
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         s_iRadioTxSerialPacketSize[i] = DEFAULT_RADIO_SERIAL_AIR_PACKET_SIZE;
   }

   _radio_tx_create_msg_queue();

   if ( 0 != pthread_mutex_init(&s_pThreadRadioTxMutex, NULL) )
   {
      log_error_and_alarm("[RadioTx] Failed to init mutex.");
      return 0;
   }

   if ( 0 != pthread_create(&s_pThreadRadioTx, NULL, &_thread_radio_tx, (void*)&s_iRadioTxSignalStop) )
   {
      log_error_and_alarm("[RadioTx] Failed to create thread for radio rx.");
      return 0;
   }
   pthread_detach(s_pThreadRadioTx);
   s_iRadioTxInitialized = 1;
   log_line("[RadioTx] Started radio Tx thread.");
   return 1;
}


void radio_tx_stop_tx_thread()
{
   if ( ! s_iRadioTxInitialized )
      return;

   log_line("[RadioTx] Signaled radio Tx thread to stop.");
   s_iRadioTxSignalStop = 1;
   s_iRadioTxInitialized = 0;

   pthread_mutex_lock(&s_pThreadRadioTxMutex);
   pthread_mutex_unlock(&s_pThreadRadioTxMutex);

   pthread_cancel(s_pThreadRadioTx);
   pthread_mutex_destroy(&s_pThreadRadioTxMutex);

   if ( s_iRadioTxIPCQueue >= 0 )
      msgctl(s_iRadioTxIPCQueue, IPC_RMID, NULL);
   s_iRadioTxIPCQueue = -1;
}

void radio_tx_mark_quit()
{
   s_iRadioTxMarkedForQuit = 1;
}

void radio_tx_set_custom_thread_raw_priority(int iPriority)
{
   s_iPendingTxThreadRawPriority = iPriority;
}

void radio_tx_set_dev_mode(int iDevMode)
{
   s_iRadioTxDevMode = iDevMode;
   log_line("[RadioTx] Set dev mode: %d", s_iRadioTxDevMode);
}


void radio_tx_pause_radio_interface(int iRadioInterfaceIndex, const char* szReason)
{
   if ( (iRadioInterfaceIndex < 0) || (iRadioInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return;
   s_iRadioTxInterfacesPaused[iRadioInterfaceIndex]++;


   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterfaceIndex);

   char szRadioName[64];
   strcpy(szRadioName, "N/A");
   if ( NULL != pRadioHWInfo )
      strncpy(szRadioName, pRadioHWInfo->szName, sizeof(szRadioName)/sizeof(szRadioName[0]));

   char szBuff[128];
   szBuff[0] = 0;
   if ( NULL != szReason )
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), " (reason: %s)", szReason);
   log_line("[RadioTx] Pause Tx on radio interface %d, [%s] (+%d)%s", iRadioInterfaceIndex+1, szRadioName, s_iRadioTxInterfacesPaused[iRadioInterfaceIndex], szBuff);
}

void radio_tx_resume_radio_interface(int iRadioInterfaceIndex)
{
   if ( (iRadioInterfaceIndex < 0) || (iRadioInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return;

   if ( s_iRadioTxInterfacesPaused[iRadioInterfaceIndex] > 0 )
      s_iRadioTxInterfacesPaused[iRadioInterfaceIndex]--;

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterfaceIndex);

   if ( NULL != pRadioHWInfo )
      log_line("[RadioTx] Resumed Tx on radio interface %d, [%s] (+%d)", iRadioInterfaceIndex+1, pRadioHWInfo->szName, s_iRadioTxInterfacesPaused[iRadioInterfaceIndex]);
   else
      log_line("[RadioTx] Resumed Tx on radio interface %d, [%s] (+%d)", iRadioInterfaceIndex+1, "N/A", s_iRadioTxInterfacesPaused[iRadioInterfaceIndex]);
}

void radio_tx_set_sik_packet_size(int iSiKPacketSize)
{
   s_iRadioTxSiKPacketSize = iSiKPacketSize;
   log_line("[RadioTx] Set SiK packet size to %d bytes (of which %d bytes are the header)", s_iRadioTxSiKPacketSize, (int)sizeof(t_packet_header_short));
   if ( (s_iRadioTxSiKPacketSize < DEFAULT_RADIO_SERIAL_AIR_MIN_PACKET_SIZE) ||
        (s_iRadioTxSiKPacketSize > DEFAULT_RADIO_SERIAL_AIR_MAX_PACKET_SIZE) )
   {
      s_iRadioTxSiKPacketSize = DEFAULT_SIK_PACKET_SIZE;
      log_line("[RadioTx] Set SiK packet size to %d bytes (of which %d bytes are the header)", s_iRadioTxSiKPacketSize, (int)sizeof(t_packet_header_short));
   }
}

void radio_tx_set_serial_packet_size(int iRadioInterfaceIndex, int iSerialPacketSize)
{
   if ( (iRadioInterfaceIndex < 0) || (iRadioInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return;

   if ( ! s_iRadioTxSerialPacketSizeInitialized )
   {
      s_iRadioTxSerialPacketSizeInitialized = 1;
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         s_iRadioTxSerialPacketSize[i] = DEFAULT_RADIO_SERIAL_AIR_PACKET_SIZE;
   }

   s_iRadioTxSerialPacketSize[iRadioInterfaceIndex] = iSerialPacketSize;
   log_line("[RadioTx] Set serial packet size to %d bytes (of which %d bytes are the header)", s_iRadioTxSerialPacketSize[iRadioInterfaceIndex], (int)sizeof(t_packet_header_short));
   if ( (s_iRadioTxSerialPacketSize[iRadioInterfaceIndex] < DEFAULT_RADIO_SERIAL_AIR_MIN_PACKET_SIZE) ||
        (s_iRadioTxSerialPacketSize[iRadioInterfaceIndex] > DEFAULT_RADIO_SERIAL_AIR_MAX_PACKET_SIZE) )
   {
      s_iRadioTxSerialPacketSize[iRadioInterfaceIndex] = DEFAULT_RADIO_SERIAL_AIR_PACKET_SIZE;
      log_line("[RadioTx] Set serial packet size to %d bytes (of which %d bytes are the header)", s_iRadioTxSerialPacketSize[iRadioInterfaceIndex], (int)sizeof(t_packet_header_short));
   }
}

// Sends a regular radio packet to serial radios. 
// Returns 1 for success.
int radio_tx_send_serial_radio_packet(int iRadioInterfaceIndex, u8* pData, int iDataLength)
{
   if ( (iRadioInterfaceIndex < 0) || (iRadioInterfaceIndex >= hardware_get_radio_interfaces_count()) )
   {
      log_softerror_and_alarm("[RadioTx] Tried to write serial packet to invalid radio interface index (%d)", iRadioInterfaceIndex+1);
      return -1;
   }
   
   if ( (NULL == pData) || (iDataLength < 2) )
   {
      log_softerror_and_alarm("[RadioTx] Tried to write invalid serial packet (%d bytes)", iDataLength);
      return -1;
   }
   
   if ( s_iRadioTxIPCQueue < 0 )
   {
      log_softerror_and_alarm("[RadioTx] Tried to write serial packet with no IPC opened.");
      if ( ! _radio_tx_create_msg_queue() )
         return -1;
   }

   type_ipc_message_tx_packet_buffer msg;

   msg.type = iRadioInterfaceIndex;
   memcpy((u8*)&(msg.data[0]), pData, iDataLength); 
   
   int iRetryCounter = 2;
   int iSucceeded = 0;
   do
   {
      if ( 0 == msgsnd(s_iRadioTxIPCQueue, &msg, iDataLength, IPC_NOWAIT) )
      {
         iSucceeded = 1;
         iRetryCounter = 0;
         break;
      }
      
      if ( errno == EAGAIN )
         log_softerror_and_alarm("[RadioTx] Failed to write to IPC, error code: EAGAIN" );
      if ( errno == EACCES )
         log_softerror_and_alarm("[RadioTx] Failed to write to IPC, error code: EACCESS");
      if ( errno == EIDRM )
         log_softerror_and_alarm("[RadioTx] Failed to write to IPC, error code: EIDRM");
      if ( errno == EINVAL )
         log_softerror_and_alarm("[RadioTx] Failed to write to IPC, error code: EINVAL");

      struct msqid_ds msg_stats;
      if ( 0 != msgctl(s_iRadioTxIPCQueue, IPC_STAT, &msg_stats) )
         log_softerror_and_alarm("[RadioTx] Failed to get statistics on ICP message queue, fd %d",
            s_iRadioTxIPCQueue );
      else
      {
         log_line("[RadioTx] IPC Info (fd %d) info: %u pending messages, %u used bytes, max bytes in the IPC channel: %u bytes",
            s_iRadioTxIPCQueue, (u32)msg_stats.msg_qnum, (u32)msg_stats.msg_cbytes, (u32)msg_stats.msg_qbytes);
      }
      
      iRetryCounter--;
      hardware_sleep_ms(5);

   } while (iRetryCounter > 0);

   if ( iSucceeded )
     return 1;
   return -1;
}
