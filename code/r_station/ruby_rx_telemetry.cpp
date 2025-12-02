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

#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../base/shared_mem.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../base/commands.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/models_list.h"
#include "../base/utils.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../utils/utils_controller.h"
#include "../base/ruby_ipc.h"
#include "../common/string_utils.h"
#include "../radio/radiopackets2.h"

#include "timers.h"
#include "shared_vars.h"

#include <time.h>
#include <sys/resource.h>
#include <termios.h>
#include <unistd.h>
#include <semaphore.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int s_fIPCFromRouter = -1;
int s_fIPCToRouter = -1;

u8 s_BufferTelemetryDownlink[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeBufferTelemetryDownlink[MAX_PACKET_TOTAL_SIZE];
int s_PipeBufferTelemetryDownlinkPos = 0;

t_packet_header_rc_info_downstream* s_pPHDownstreamInfoRC = NULL;

int g_iSerialPortDataLinkFD = -1;
int g_iSerialPortDataLinkSpeed = 0;
int g_iSerialPortIndexDataLink = -1;

int g_iSerialPortTelemetryFD = -1;
int g_iSerialPortTelemetrySpeed = 0;
int g_iSerialPortIndexTelemetry = -1;

u8  telemetryBufferToVehicle[RAW_TELEMETRY_MAX_BUFFER];
int telemetryBufferToVehicleMaxSize = RAW_TELEMETRY_MAX_BUFFER;
int telemetryBufferToVehicleCount = 0;
u32 telemetryBufferToVehicleLastSendTime = 0;

u8  dataLinkBufferToVehicle[RAW_TELEMETRY_MAX_BUFFER];
int dataLinkBufferToVehicleMaxSize = RAW_TELEMETRY_MAX_BUFFER;
int dataLinkBufferToVehicleCount = 0;
u32 dataLinkBufferToVehicleLastSendTime = 0;

u32 s_uRawTelemetryUploadTotalReadFromSerial = 0;
u32 s_uRawTelemetryUploadTotalSend = 0;
u32 s_uRawTelemetryUploadSegmentIndex = 0;
u32 s_uRawTelemetryLastReceivedDownloadedSegmentIndex = MAX_U32;

u32 s_uDataLinkUploadSegmentIndex = 0;
u32 s_uDataLinkLastReceivedDownloadedSegmentIndex = MAX_U32;

u32 s_LastReceivedStreamPacketIndexFromRouter = MAX_U32;

u32 s_TimeLastUplinkKbpsComputation = 0;
u32 s_uUplink_bps = 0;

typedef struct
{
   bool bUSBTethering;
   u32 TimeLastUSBTetheringCheck;
   char szIPUSB[32];
   struct sockaddr_in sockAddrUSBDevice;
   int socketUSBOutput;
   int usbBlockSize;
   u8 usbBuffer[2048];
   int usbBufferPos;
} t_telemetry_usb_output_info;

t_telemetry_usb_output_info s_TelemetryUSBOutputInfo;

void checkTelemetrySettingsOnControllerChanged();


void process_datalink_packet_download(u8* pBuffer, int length)
{
   t_packet_header* pPH = (t_packet_header*)pBuffer;

   u32 segment = 0;
   memcpy((u8*)&segment, pBuffer+sizeof(t_packet_header), sizeof(u32));

   if ( s_uDataLinkLastReceivedDownloadedSegmentIndex != MAX_U32 )
   if ( segment <= s_uDataLinkLastReceivedDownloadedSegmentIndex )
     return;

   if ( s_uDataLinkLastReceivedDownloadedSegmentIndex != MAX_U32 )
   if ( s_uDataLinkLastReceivedDownloadedSegmentIndex + 1 != segment )
   {
      //log_line("-------------------------------");
      //log_line("Missing datalink download packet");
      //log_line("-------------------------------");
   }
   s_uDataLinkLastReceivedDownloadedSegmentIndex = segment;

   if ( (NULL == g_pCurrentModel) || (g_pCurrentModel->is_spectator) )
      return;
   if ( -1 == g_iSerialPortDataLinkFD )
      return;

   int len = pPH->total_length - sizeof(t_packet_header)-sizeof(u32);

   u8* pDataLinkData = pBuffer + sizeof(t_packet_header)+sizeof(u32);
   if ( len != write(g_iSerialPortDataLinkFD, pDataLinkData, len) )
      log_softerror_and_alarm("Failed to write to serial port for datalink output on controller.");
}

void process_data_telemetry_raw_download(u8* pBuffer, int length)
{
   t_packet_header* pPH = (t_packet_header*)pBuffer;
   t_packet_header_telemetry_raw* pPHTR = (t_packet_header_telemetry_raw*)(pBuffer + sizeof(t_packet_header));

   #ifdef LOG_RAW_TELEMETRY
   log_line("[Raw_Telem] Received from router raw telemetry packet, index %u, %d / %d bytes",
       pPHTR->telem_segment_index, pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_telemetry_raw), pPH->total_length);
   #endif

   if ( (s_uRawTelemetryLastReceivedDownloadedSegmentIndex != MAX_U32) &&
        (s_uRawTelemetryLastReceivedDownloadedSegmentIndex > 10) &&
        (pPHTR->telem_segment_index < s_uRawTelemetryLastReceivedDownloadedSegmentIndex-10) )
      s_uRawTelemetryLastReceivedDownloadedSegmentIndex = MAX_U32;

   if ( s_uRawTelemetryLastReceivedDownloadedSegmentIndex != MAX_U32 )
   if ( pPHTR->telem_segment_index <= s_uRawTelemetryLastReceivedDownloadedSegmentIndex )
      return;

   if ( s_uRawTelemetryLastReceivedDownloadedSegmentIndex + 1 != pPHTR->telem_segment_index )
   {
      //log_line("-------------------------------");
      //log_line("Missing telemetry download packet");
      //log_line("-------------------------------");
   }
   s_uRawTelemetryLastReceivedDownloadedSegmentIndex = pPHTR->telem_segment_index;
      
   int len = pPH->total_length - sizeof(t_packet_header)-sizeof(t_packet_header_telemetry_raw);
   u8* pTelemetryData = pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_telemetry_raw);

   //log_line("Received downloaded raw telemetry: segment index (last/now): %u/%u, total serial: %u, total size: %u, len: %d", s_uRawTelemetryLastReceivedDownloadedSegmentIndex, pPHTR->telem_segment_index, pPHTR->telem_total_serial, pPHTR->telem_total_data, len);

   if ( NULL == g_pCurrentModel )
      return;

   if ( s_TelemetryUSBOutputInfo.bUSBTethering && 0 != s_TelemetryUSBOutputInfo.szIPUSB[0] )
   if ( -1 != s_TelemetryUSBOutputInfo.socketUSBOutput )
   {
      memcpy( &(s_TelemetryUSBOutputInfo.usbBuffer[s_TelemetryUSBOutputInfo.usbBufferPos]), pTelemetryData, len );
      s_TelemetryUSBOutputInfo.usbBufferPos += len;

      while ( s_TelemetryUSBOutputInfo.usbBufferPos >= s_TelemetryUSBOutputInfo.usbBlockSize )
      {
         sendto(s_TelemetryUSBOutputInfo.socketUSBOutput, s_TelemetryUSBOutputInfo.usbBuffer, s_TelemetryUSBOutputInfo.usbBlockSize,
               0, (struct sockaddr *)&s_TelemetryUSBOutputInfo.sockAddrUSBDevice, sizeof(s_TelemetryUSBOutputInfo.sockAddrUSBDevice) );
         //log_line("Sent USB temeletry packet, size: %d", s_TelemetryUSBOutputInfo.usbBlockSize);
         for( int i=0; i<(s_TelemetryUSBOutputInfo.usbBufferPos-s_TelemetryUSBOutputInfo.usbBlockSize); i++ )
            s_TelemetryUSBOutputInfo.usbBuffer[i] = s_TelemetryUSBOutputInfo.usbBuffer[i+s_TelemetryUSBOutputInfo.usbBlockSize];
         s_TelemetryUSBOutputInfo.usbBufferPos -= s_TelemetryUSBOutputInfo.usbBlockSize;
      }
   }

   if ( (g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_SEND_FULL_TELEMETRY_TO_CONTROLLER) ||
        (g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_SEND_FULL_TELEMETRY_TO_CONTROLLER_PLUGINS) )
   {
      // Send the raw telemetry to central too, if requested or for OSD plugins consumption
      pPH->packet_flags &= (~PACKET_FLAGS_MASK_MODULE);
      pPH->packet_flags |= PACKET_COMPONENT_LOCAL_CONTROL;
      ruby_ipc_channel_send_message(s_fIPCToRouter, pBuffer, length);
   }

   if ( g_pCurrentModel->is_spectator && (!(g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_SPECTATOR_ENABLE)) )
      return;

   if ( -1 != g_iSerialPortTelemetryFD )
   {
      int iWrite = write(g_iSerialPortTelemetryFD, pTelemetryData, len);
      if ( len != iWrite )
         log_softerror_and_alarm("Failed to write to serial port for telemetry output on controller.");

      #ifdef LOG_RAW_TELEMETRY
      log_line("[Raw_Telem] Sent %d of %d bytes of raw telemetry to serial port.", iWrite, len);
      #endif
   }
}

void _process_data_rc_telemetry(u8* pBuffer, int length)
{
   if ( NULL != s_pPHDownstreamInfoRC )
      memcpy((u8*)s_pPHDownstreamInfoRC, pBuffer + sizeof(t_packet_header), sizeof(t_packet_header_rc_info_downstream));

   if ( NULL != g_pProcessStats )
      g_pProcessStats->timeLastReceivedPacket = g_TimeNow;
}

void upload_telemetry_packet()
{
   if ( (NULL == g_pCurrentModel) || g_pCurrentModel->is_spectator || g_bUpdateInProgress )
      return;

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

   if ( telemetryBufferToVehicleCount > RAW_TELEMETRY_MAX_BUFFER )
   {
      log_softerror_and_alarm("Trying to send more telemetry data than expected to uplink: %d bytes, max expected: %d bytes. Sending only max allowed.", telemetryBufferToVehicleCount, RAW_TELEMETRY_MAX_BUFFER);
      telemetryBufferToVehicleCount = RAW_TELEMETRY_MAX_BUFFER;
   }

   s_uRawTelemetryUploadTotalSend += telemetryBufferToVehicleCount;
   s_uUplink_bps += telemetryBufferToVehicleCount * 8;

   t_packet_header PH;
   t_packet_header_telemetry_raw PHTR;

   radio_packet_init(&PH, PACKET_COMPONENT_TELEMETRY, PACKET_TYPE_TELEMETRY_RAW_UPLOAD, STREAM_ID_TELEMETRY);

   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
   PH.total_length = sizeof(t_packet_header)+sizeof(t_packet_header_telemetry_raw) + telemetryBufferToVehicleCount;
      
   PHTR.telem_segment_index = s_uRawTelemetryUploadSegmentIndex;
   PHTR.telem_total_data = s_uRawTelemetryUploadTotalSend;
   PHTR.telem_total_serial = s_uRawTelemetryUploadTotalReadFromSerial;

   //log_line("Sending raw telemetry to controller, segment index: %u, total serial: %u, total data: %u, length: %d", dptr.telem_segment_index, dptr.telem_total_serial, dptr.telem_total_data, telemetryBufferToVehicleCount);

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), (u8*)&PHTR, sizeof(t_packet_header_telemetry_raw));
   memcpy(buffer+sizeof(t_packet_header)+sizeof(t_packet_header_telemetry_raw), telemetryBufferToVehicle, telemetryBufferToVehicleCount);
   radio_packet_compute_crc(buffer, PH.total_length);
   ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, PH.total_length);
 
   #ifdef LOG_RAW_TELEMETRY
   log_line("[Raw_Telem] Sent raw telemetry packet to router, index %u, %d / %d bytes", PHTR.telem_segment_index, telemetryBufferToVehicleCount, PH.total_length);
   #endif
   telemetryBufferToVehicleLastSendTime = g_TimeNow;
   telemetryBufferToVehicleCount = 0;
   s_uRawTelemetryUploadSegmentIndex++;
}


void upload_datalink_packet()
{
   if ( (NULL == g_pCurrentModel) || g_pCurrentModel->is_spectator || g_bUpdateInProgress )
      return;

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

   if ( dataLinkBufferToVehicleCount > RAW_TELEMETRY_MAX_BUFFER )
   {
      log_softerror_and_alarm("Trying to send more data link data than expected to uplink: %d bytes, max expected: %d bytes. Sending only max allowed.", dataLinkBufferToVehicleCount, RAW_TELEMETRY_MAX_BUFFER);
      dataLinkBufferToVehicleCount = RAW_TELEMETRY_MAX_BUFFER;
   }

   s_uUplink_bps += dataLinkBufferToVehicleCount * 8;

   t_packet_header PH;

   radio_packet_init(&PH, PACKET_COMPONENT_TELEMETRY, PACKET_TYPE_AUX_DATA_LINK_UPLOAD, STREAM_ID_DATA2);
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
   PH.total_length = sizeof(t_packet_header) + sizeof(u32) + dataLinkBufferToVehicleCount;

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), (u8*)&s_uDataLinkUploadSegmentIndex, sizeof(u32));
   memcpy(buffer+sizeof(t_packet_header)+sizeof(u32), dataLinkBufferToVehicle, dataLinkBufferToVehicleCount);
   radio_packet_compute_crc(buffer, PH.total_length);
   ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, PH.total_length);
   dataLinkBufferToVehicleLastSendTime = g_TimeNow;
   dataLinkBufferToVehicleCount = 0;
   s_uDataLinkUploadSegmentIndex++;
}

int try_read_serial_telemetry()
{
   if ( -1 == g_iSerialPortTelemetryFD )
      return 0;
   if ( (NULL == g_pCurrentModel) || g_pCurrentModel->is_spectator )
      return 0;

   u8 bufferIn[RAW_TELEMETRY_MAX_BUFFER];
   struct timeval to;
   to.tv_sec = 0;
   to.tv_usec = 1000; // 1 ms
   fd_set readset;   
   FD_ZERO(&readset);
   FD_SET(g_iSerialPortTelemetryFD, &readset);
   int res = select(g_iSerialPortTelemetryFD+1, &readset, NULL, NULL, &to);
   if ( res <= 0 )
      return 0;
   if ( ! FD_ISSET(g_iSerialPortTelemetryFD, &readset) )
      return 0;

   int length = read(g_iSerialPortTelemetryFD, bufferIn, RAW_TELEMETRY_MAX_BUFFER);
   if ( length <= 0 )
      return 0;

   s_uRawTelemetryUploadTotalReadFromSerial += length;
   u8* pData = &bufferIn[0];
   while ( length > 0 )
   {
      if ( telemetryBufferToVehicleCount + length < telemetryBufferToVehicleMaxSize )
      {
         memcpy(&(telemetryBufferToVehicle[telemetryBufferToVehicleCount]), pData, length);
         telemetryBufferToVehicleCount += length;
         return 1;
      }
      int chunkSize = telemetryBufferToVehicleMaxSize-telemetryBufferToVehicleCount;
      memcpy(&(telemetryBufferToVehicle[telemetryBufferToVehicleCount]), pData, chunkSize);
      telemetryBufferToVehicleCount += chunkSize;
      pData += chunkSize;
      length -= chunkSize;
      upload_telemetry_packet();
   }
   return 1;
}  

int try_read_serial_datalink()
{
   if ( -1 == g_iSerialPortDataLinkFD )
      return 0;
   if ( (NULL == g_pCurrentModel) || g_pCurrentModel->is_spectator )
      return 0;

   u8 bufferIn[RAW_TELEMETRY_MAX_BUFFER];
   struct timeval to;
   to.tv_sec = 0;
   to.tv_usec = 1000; // 1 ms
   fd_set readset;   
   FD_ZERO(&readset);
   FD_SET(g_iSerialPortDataLinkFD, &readset);
   int res = select(g_iSerialPortDataLinkFD+1, &readset, NULL, NULL, &to);
   if ( res <= 0 )
      return 0;
   if ( ! FD_ISSET(g_iSerialPortDataLinkFD, &readset) )
      return 0;

   int length = read(g_iSerialPortDataLinkFD, bufferIn, RAW_TELEMETRY_MAX_BUFFER);
   if ( length <= 0 )
      return 0;

   //log_line("Serial datalink read %d bytes.", length);
   //char szBuff[2000];
   //memcpy(szBuff, &bufferIn[0], length);
   //szBuff[length] = 0;
   //log_line("Sending datalink data: %s", szBuff);
   

   u8* pData = &bufferIn[0];
   while ( length > 0 )
   {
      if ( dataLinkBufferToVehicleCount + length < dataLinkBufferToVehicleMaxSize )
      {
         memcpy(&(dataLinkBufferToVehicle[dataLinkBufferToVehicleCount]), pData, length);
         dataLinkBufferToVehicleCount += length;
         return 1;
      }
      int chunkSize = dataLinkBufferToVehicleMaxSize-dataLinkBufferToVehicleCount;
      memcpy(&(dataLinkBufferToVehicle[dataLinkBufferToVehicleCount]), pData, chunkSize);
      dataLinkBufferToVehicleCount += chunkSize;
      pData += chunkSize;
      length -= chunkSize;
      upload_datalink_packet();
   }
   return 1;
}  

void try_read_messages_from_router()
{
   int maxMessagesToRead = 15;
 
   while ( (maxMessagesToRead > 0) && (NULL != ruby_ipc_try_read_message(s_fIPCFromRouter, s_PipeBufferTelemetryDownlink, &s_PipeBufferTelemetryDownlinkPos, s_BufferTelemetryDownlink)) )
   {
      maxMessagesToRead--;

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;

      t_packet_header* pPH = (t_packet_header*)&s_BufferTelemetryDownlink[0];

      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
      {
            if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED )
            if ( pPH->vehicle_id_src != PACKET_COMPONENT_TELEMETRY )
            {
               log_line("Received controller message to reload telemetry settings.");
               checkTelemetrySettingsOnControllerChanged();
            }

            if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED )
            if ( pPH->vehicle_id_src != PACKET_COMPONENT_TELEMETRY )
            {
               log_line("Received local event to reload model");
               reloadCurrentModel();
               u8 uChangeType = (pPH->vehicle_id_src >> 8) & 0xFF;
               if ( uChangeType == MODEL_CHANGED_SYNCHRONISED_SETTINGS_FROM_VEHICLE )
               {
                  g_pCurrentModel->b_mustSyncFromVehicle = false;
                  log_line("Model settings where synchronized from vehicle. Reset model must sync flag.");
               }
               s_LastReceivedStreamPacketIndexFromRouter = MAX_U32;
               s_uDataLinkLastReceivedDownloadedSegmentIndex = MAX_U32;
               s_uRawTelemetryLastReceivedDownloadedSegmentIndex = MAX_U32;
            }

            if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_STARTED )
               g_bUpdateInProgress = true;
            if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED ||
                 pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_FINISHED )
               g_bUpdateInProgress = false;

            if ( maxMessagesToRead <= 0 )
               break;
            continue;
      }

      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) != PACKET_COMPONENT_TELEMETRY )
      {
         if ( maxMessagesToRead <= 0 )
            break;
         continue;
      }
      
      if ( pPH->vehicle_id_src != g_pCurrentModel->uVehicleId )
      {
         if ( maxMessagesToRead <= 0 )
            break;
         continue;
      }

      if ( ! radio_packet_check_crc(s_BufferTelemetryDownlink, pPH->total_length) )
      {
         if ( maxMessagesToRead <= 0 )
            break;
         continue;
      }
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastRadioRxTime = g_TimeNow;

      //log_line("Received a packet type: %d", pPH->packet_type);

      if ( s_LastReceivedStreamPacketIndexFromRouter != MAX_U32 &&
           s_LastReceivedStreamPacketIndexFromRouter > 20 &&
           ((pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX) < s_LastReceivedStreamPacketIndexFromRouter-20) )
      {
         s_LastReceivedStreamPacketIndexFromRouter = MAX_U32;
         s_uDataLinkLastReceivedDownloadedSegmentIndex = MAX_U32;
         s_uRawTelemetryLastReceivedDownloadedSegmentIndex = MAX_U32;
      }

      if ( s_LastReceivedStreamPacketIndexFromRouter != MAX_U32 &&
          (pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX) <= s_LastReceivedStreamPacketIndexFromRouter )
      {
         if ( maxMessagesToRead <= 0 )
            break;
         continue;
      }
      s_LastReceivedStreamPacketIndexFromRouter = (pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX);

      if ( pPH->packet_type == PACKET_TYPE_AUX_DATA_LINK_DOWNLOAD )
         process_datalink_packet_download(s_BufferTelemetryDownlink, pPH->total_length);
      else if ( pPH->packet_type == PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD )
         process_data_telemetry_raw_download(s_BufferTelemetryDownlink, pPH->total_length);
      else if ( pPH->packet_type == PACKET_TYPE_RC_TELEMETRY )
         _process_data_rc_telemetry(s_BufferTelemetryDownlink, pPH->total_length);
   }
}

void init_serial_ports()
{
   g_iSerialPortIndexDataLink = -1;
   g_iSerialPortDataLinkSpeed = -1;
   g_iSerialPortIndexTelemetry = -1;
   g_iSerialPortTelemetrySpeed = -1;
   hw_serial_port_info_t* pPortInfo = NULL;

   for( int i=0; i<hardware_serial_get_ports_count(); i++ )
   {
      pPortInfo = hardware_get_serial_port_info(i);
      if ( (NULL != pPortInfo) && (pPortInfo->iPortUsage == SERIAL_PORT_USAGE_DATA_LINK) )
      {
          g_iSerialPortIndexDataLink = i;
          g_iSerialPortDataLinkSpeed = pPortInfo->lPortSpeed;
      }
      if ( (NULL != pPortInfo) && (pPortInfo->iPortUsage == SERIAL_PORT_USAGE_TELEMETRY) )
      {
          g_iSerialPortIndexTelemetry = i;
          g_iSerialPortTelemetrySpeed = pPortInfo->lPortSpeed;
      }
   }

   if ( -1 != g_iSerialPortIndexDataLink )
   {
      log_line("Auxiliary data link is enabled on controller serial port %s (%s). Configuring serial port...", pPortInfo->szName, pPortInfo->szPortDeviceName);
      hardware_configure_serial(pPortInfo->szPortDeviceName, pPortInfo->lPortSpeed );
      g_iSerialPortDataLinkFD = hardware_open_serial_port(pPortInfo->szPortDeviceName, pPortInfo->lPortSpeed);
      if ( -1 == g_iSerialPortDataLinkFD )
         log_softerror_and_alarm("Failed to open serial port %s (%s) for auxiliary data link.", pPortInfo->szName, pPortInfo->szPortDeviceName);
      else
         log_line("Opened serial port %s (%s) for auxiliary data link successfully at %d bps.", pPortInfo->szName, pPortInfo->szPortDeviceName, (int)pPortInfo->lPortSpeed);
   }
   else
      log_line("No serial port configured for auxiliary data link. Skipping it.");

   if ( (NULL == g_pCurrentModel) || (g_pCurrentModel->is_spectator) )
      return;

   if ( -1 == g_iSerialPortIndexTelemetry )
   {
      log_line("Telemetry is not enabled on any serial port on controller. Serial for telemetry not enabled/configured.");
      return;
   }

   log_line("Telemetry enabled on local serial port index: %d", g_iSerialPortIndexTelemetry);

   telemetryBufferToVehicleMaxSize = 320;
   if ( telemetryBufferToVehicleMaxSize > RAW_TELEMETRY_MAX_BUFFER )
      telemetryBufferToVehicleMaxSize = RAW_TELEMETRY_MAX_BUFFER;
   log_line("Using a telemetry serial read buffer of maximum %d bytes", telemetryBufferToVehicleMaxSize);
   log_line("Using a telemetry serial buffer minimum send size of %d bytes", RAW_TELEMETRY_MIN_SEND_LENGTH);

   telemetryBufferToVehicleCount = 0;
   telemetryBufferToVehicleLastSendTime = g_TimeNow;
   
   pPortInfo = hardware_get_serial_port_info(g_iSerialPortIndexTelemetry);

   if ( NULL == pPortInfo )
   {
      log_softerror_and_alarm("Can't get serial port info. Serial not enabled/configured.");
      return;
   }
   
   log_line("Telemetry is enabled on controller serial port %s (%s). Configuring serial port...", pPortInfo->szName, pPortInfo->szPortDeviceName);
   hardware_configure_serial(pPortInfo->szPortDeviceName, pPortInfo->lPortSpeed );
   g_iSerialPortTelemetryFD = hardware_open_serial_port(pPortInfo->szPortDeviceName, pPortInfo->lPortSpeed);
   if ( -1 == g_iSerialPortTelemetryFD )
      log_softerror_and_alarm("Failed to open serial port %s (%s) for telemetry.", pPortInfo->szName, pPortInfo->szPortDeviceName);
   else
      log_line("Opened serial port %s (%s) for telemetry successfully at %d bps.", pPortInfo->szName, pPortInfo->szPortDeviceName, (int)pPortInfo->lPortSpeed);
}

void checkTelemetrySettingsOnControllerChanged()
{
   hardware_serial_reload_ports_settings();
   load_ControllerSettings();

   bool bParamsChanged = false;

   if ( s_TelemetryUSBOutputInfo.bUSBTethering )
   {
      if ( -1 != s_TelemetryUSBOutputInfo.socketUSBOutput )
         close(s_TelemetryUSBOutputInfo.socketUSBOutput);
      s_TelemetryUSBOutputInfo.socketUSBOutput = -1;
      s_TelemetryUSBOutputInfo.bUSBTethering = false;
      log_line("Telemetry Output to USB closed momentarly due to telemetry serial ports settings changed.");
   }

   // Telemetry serial port changed ?

   int dataLinkPort = -1;
   int dataLinkSpeed = -1;
   int telemetryLinkPort = -1;
   int telemetryLinkSpeed = -1;
   for( int i=0; i<hardware_serial_get_ports_count(); i++ )
   {
      hw_serial_port_info_t* pPortInfo = hardware_get_serial_port_info(i);
      if ( (NULL != pPortInfo) && (pPortInfo->iPortUsage == SERIAL_PORT_USAGE_DATA_LINK) )
      {
          dataLinkPort = i;
          dataLinkSpeed = pPortInfo->lPortSpeed;
      }
      if ( (NULL != pPortInfo) && (pPortInfo->iPortUsage == SERIAL_PORT_USAGE_TELEMETRY) )
      {
          telemetryLinkPort = i;
          telemetryLinkSpeed = pPortInfo->lPortSpeed;
      }
   }

   if ( (telemetryLinkPort != g_iSerialPortIndexTelemetry) || (telemetryLinkSpeed != g_iSerialPortTelemetrySpeed) )
      bParamsChanged = true;
   if ( (dataLinkPort != g_iSerialPortIndexDataLink) || (dataLinkSpeed != g_iSerialPortDataLinkSpeed) )
      bParamsChanged = true;

   if ( ! bParamsChanged )
   {
      log_line("Telemetry/datalink params on the controller are unchanged. Do no change to the serial ports used.");
      return;
   }

   log_line("Telemetry params or the auxiliary data link changed on the controller. Reinitializing serial ports and telemetry params...");

   if ( -1 != g_iSerialPortDataLinkFD )
      close(g_iSerialPortDataLinkFD);
   g_iSerialPortDataLinkFD = -1;

   if ( -1 != g_iSerialPortTelemetryFD )
      close(g_iSerialPortTelemetryFD);
   g_iSerialPortTelemetryFD = -1;

   init_serial_ports();
}

void periodic_checks()
{
   ControllerSettings* pCS = get_ControllerSettings();

   if ( g_TimeNow >= s_TimeLastUplinkKbpsComputation+250 )
   {
      s_uUplink_bps = 0;
      s_TimeLastUplinkKbpsComputation = g_TimeNow;
   }

   if ( s_TelemetryUSBOutputInfo.bUSBTethering && (pCS->iTelemetryForwardUSBType == 0) )
   {
      if ( -1 != s_TelemetryUSBOutputInfo.socketUSBOutput )
         close(s_TelemetryUSBOutputInfo.socketUSBOutput);
      s_TelemetryUSBOutputInfo.socketUSBOutput = -1;
      s_TelemetryUSBOutputInfo.bUSBTethering = false;
      log_line("Telemetry Output to USB disabled.");
   }
   if ( pCS->iTelemetryForwardUSBType != 0 )
   if ( g_TimeNow > s_TelemetryUSBOutputInfo.TimeLastUSBTetheringCheck + 1000 )
   {
      char szFile[128];
      strcpy(szFile, FOLDER_RUBY_TEMP);
      strcat(szFile, FILE_TEMP_USB_TETHERING_DEVICE);
      s_TelemetryUSBOutputInfo.TimeLastUSBTetheringCheck = g_TimeNow;
      if ( ! s_TelemetryUSBOutputInfo.bUSBTethering )
      if ( access(szFile, R_OK) != -1 )
      {
         s_TelemetryUSBOutputInfo.szIPUSB[0] = 0;
         FILE* fd = fopen(szFile, "r");
         if ( NULL != fd )
         {
            fscanf(fd, "%s", s_TelemetryUSBOutputInfo.szIPUSB);
            fclose(fd);
         }
         log_line("USB Device Tethered for Telemetry Output. Device IP: %s", s_TelemetryUSBOutputInfo.szIPUSB);

         s_TelemetryUSBOutputInfo.socketUSBOutput = socket(AF_INET , SOCK_DGRAM, 0);
         if ( s_TelemetryUSBOutputInfo.socketUSBOutput != -1 && 0 != s_TelemetryUSBOutputInfo.szIPUSB[0] )
         {
            memset(&s_TelemetryUSBOutputInfo.sockAddrUSBDevice, 0, sizeof(s_TelemetryUSBOutputInfo.sockAddrUSBDevice));
            s_TelemetryUSBOutputInfo.sockAddrUSBDevice.sin_family = AF_INET;
            s_TelemetryUSBOutputInfo.sockAddrUSBDevice.sin_addr.s_addr = inet_addr(s_TelemetryUSBOutputInfo.szIPUSB);
            s_TelemetryUSBOutputInfo.sockAddrUSBDevice.sin_port = htons( pCS->iTelemetryForwardUSBPort );
         }
         s_TelemetryUSBOutputInfo.usbBlockSize = pCS->iTelemetryForwardUSBPacketSize;
         s_TelemetryUSBOutputInfo.usbBufferPos = 0;
         s_TelemetryUSBOutputInfo.bUSBTethering = true;
         return;
      }
      if ( s_TelemetryUSBOutputInfo.bUSBTethering )
      if ( access(szFile, R_OK) == -1 )
      {
         log_line("Tethered USB Device for Telemetry Output Unplugged.");
         if ( -1 != s_TelemetryUSBOutputInfo.socketUSBOutput )
            close(s_TelemetryUSBOutputInfo.socketUSBOutput);
         s_TelemetryUSBOutputInfo.socketUSBOutput = -1;
         s_TelemetryUSBOutputInfo.usbBufferPos = 0;
         s_TelemetryUSBOutputInfo.bUSBTethering = false;
      }
   }
}

void open_shared_mem_objects()
{
   g_pProcessStats = shared_mem_process_stats_open_write(SHARED_MEM_WATCHDOG_TELEMETRY_RX);
   if ( NULL == g_pProcessStats )
      log_softerror_and_alarm("Failed to open shared mem for telemetry rx process watchdog stats for writing: %s", SHARED_MEM_WATCHDOG_TELEMETRY_RX);
   else
      log_line("Opened shared mem for telemetry rx process watchdog stats for writing.");

   if ( NULL != g_pProcessStats )
      g_pProcessStats->timeLastReceivedPacket = 0;

   s_pPHDownstreamInfoRC = shared_mem_rc_downstream_info_open_write();
   if ( NULL == s_pPHDownstreamInfoRC )
      log_softerror_and_alarm("Failed to open RC Downloaded info shared memory for write.");
   else
      log_line("Opened RC Downloaded info shared memory for write: success.");
   if ( NULL != s_pPHDownstreamInfoRC )
      memset((u8*)s_pPHDownstreamInfoRC, 0, sizeof(t_packet_header_rc_info_downstream));
}

int open_pipes()
{
   s_fIPCFromRouter = ruby_open_ipc_channel_read_endpoint(IPC_CHANNEL_TYPE_ROUTER_TO_TELEMETRY);
   if ( s_fIPCFromRouter < 0 )
      return -1;

   s_fIPCToRouter = ruby_open_ipc_channel_write_endpoint(IPC_CHANNEL_TYPE_TELEMETRY_TO_ROUTER);
   if ( s_fIPCToRouter < 0 )
      return -1;
   return 0;
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
   
   log_init("RX_Telemetry");

   hardware_detectBoardAndSystemType();

   //log_add_file("logs/log_rx_telemetry.log");

   if ( strcmp(argv[argc-1], "-debug") == 0 )
      log_enable_stdout();
  
   g_uControllerId = controller_utils_getControllerId();
   log_line("Controller UID: %u", g_uControllerId);

   utils_log_radio_packets_sizes();
   radio_packets_log_sizes();

   loadAllModels();
   g_pCurrentModel = getCurrentModel();

   if ( -1 == open_pipes() )
      return -1;

   load_ControllerInterfacesSettings();
   load_ControllerSettings();
   ControllerSettings* pCS = get_ControllerSettings();

   if ( pCS->iCoresAdjustment )
      hw_set_current_thread_affinity("rx_telemetry", CORE_AFFINITY_TELEMETRY_RX, CORE_AFFINITY_TELEMETRY_RX);
 
   if ( pCS->iPrioritiesAdjustment )
      hw_set_priority_current_proc(pCS->iThreadPriorityOthers); 

   init_serial_ports();

   Preferences* p = get_Preferences();   
   if ( p->nLogLevel != 0 )
      log_only_errors();

   s_TelemetryUSBOutputInfo.bUSBTethering = false;
   s_TelemetryUSBOutputInfo.TimeLastUSBTetheringCheck = 0;
   s_TelemetryUSBOutputInfo.szIPUSB[0] = 0;
   s_TelemetryUSBOutputInfo.socketUSBOutput = -1;
   s_TelemetryUSBOutputInfo.usbBlockSize = 1024;
   s_TelemetryUSBOutputInfo.usbBufferPos = 0;

   radio_enable_crc_gen(1);

   open_shared_mem_objects();
 
   log_line("Started all ok. Running now.");
   log_line("--------------------------------");

   g_TimeStart = get_current_timestamp_ms();
 
   int iSleepTime = 50;

   while (!g_bQuit) 
   {
      hardware_sleep_ms(iSleepTime);

      g_uLoopCounter++;
      g_TimeNow = get_current_timestamp_ms();
      if ( NULL != g_pProcessStats )
      {
         g_pProcessStats->uLoopCounter++;
         g_pProcessStats->lastActiveTime = g_TimeNow;
      }

      u32 uTimeStart = g_TimeNow;
      u32 tTime0 = g_TimeNow;

      periodic_checks();

      iSleepTime = 50;
      if ( -1 != g_iSerialPortTelemetryFD )
      {
         iSleepTime = 20;
         int iReads = 3;
         while ( (try_read_serial_telemetry() > 0) && (iReads > 0) )
         {
            iReads--;
            if ( telemetryBufferToVehicleCount >= RAW_TELEMETRY_MIN_SEND_LENGTH || 
                (telemetryBufferToVehicleCount > 0 && g_TimeNow >= telemetryBufferToVehicleLastSendTime + RAW_TELEMETRY_SEND_TIMEOUT ) )
               upload_telemetry_packet();
         }
      }

      if ( -1 != g_iSerialPortDataLinkFD )
      {
         iSleepTime = 20;
         int iReads = 3;
         while ( (try_read_serial_telemetry() > 0) && (iReads > 0) )
         {
            iReads--;
            if ( dataLinkBufferToVehicleCount >= AUXILIARY_DATA_LINK_MIN_SEND_LENGTH || 
                (dataLinkBufferToVehicleCount > 0 && g_TimeNow >= dataLinkBufferToVehicleLastSendTime + AUXILIARY_DATA_LINK_SEND_TIMEOUT ) )
               upload_datalink_packet();
         }
      }

      try_read_messages_from_router();

      u32 tNow = get_current_timestamp_ms();

      if ( NULL != g_pProcessStats )
      {
         if ( g_pProcessStats->uMaxLoopTimeMs < tNow - tTime0 )
            g_pProcessStats->uMaxLoopTimeMs = tNow - tTime0;
         g_pProcessStats->uTotalLoopTime += tNow - tTime0;
         if ( 0 != g_pProcessStats->uLoopCounter )
            g_pProcessStats->uAverageLoopTimeMs = g_pProcessStats->uTotalLoopTime / g_pProcessStats->uLoopCounter;
      }

      u32 dTime = get_current_timestamp_ms() - uTimeStart;
      if ( dTime > 50 )
         log_softerror_and_alarm("Main processing loop took too long (%u ms).", dTime);
   }

   log_line("Stopping...");

   if ( -1 != g_iSerialPortDataLinkFD )
      close(g_iSerialPortDataLinkFD);
   g_iSerialPortDataLinkFD = -1;

   if ( -1 != g_iSerialPortTelemetryFD )
      close(g_iSerialPortTelemetryFD);
   g_iSerialPortTelemetryFD = -1;
   
   ruby_close_ipc_channel(s_fIPCFromRouter);
   ruby_close_ipc_channel(s_fIPCToRouter);
   s_fIPCFromRouter = -1;
   s_fIPCToRouter = -1;
    
   if ( -1 != s_TelemetryUSBOutputInfo.socketUSBOutput )
      close(s_TelemetryUSBOutputInfo.socketUSBOutput);
   s_TelemetryUSBOutputInfo.socketUSBOutput = -1;
   s_TelemetryUSBOutputInfo.bUSBTethering = false;
   s_TelemetryUSBOutputInfo.usbBufferPos = 0;

   shared_mem_rc_downstream_info_close(s_pPHDownstreamInfoRC);
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_TELEMETRY_RX, g_pProcessStats);
   log_line("Stopped. Exit");
   log_line("--------------------------");

   return (0);
}
