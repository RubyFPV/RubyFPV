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

#include "telemetry.h"
#include "telemetry_mavlink.h"
#include "telemetry_ltm.h"
#include "telemetry_msp.h"
#include "shared_vars.h"
#include "timers.h"
#include "../base/ruby_ipc.h"
#include "../base/parse_fc_telemetry.h"
#include "../radio/radiopackets2.h"
#include "../common/string_utils.h"

bool isRadioLinksInitInProgress();

extern int s_fIPCToRouter;
t_packet_header_fc_telemetry sPHFCT;
t_packet_header_fc_extra sPHFCE;

int s_iCurrentTelemetrySerialPortIndex = -1;
int s_iCurrentTelemetrySerialPortSpeed = DEFAULT_FC_TELEMETRY_SERIAL_SPEED;
int s_iTelemetrySerialPortFile = -1;
u32 s_uTimeSerialPortOpened = 0;

u32 s_uRawTelemetryTotalReadFromFCSerial = 0;
int s_iFCSerialTelemetryReadBytesTempLastSecond = 0;
int s_iFCSerialTelemetryReadBytesPerSecond = 0;
      
u8  uTelemetryBufferFromFC_Serial[RAW_TELEMETRY_MAX_BUFFER];
int iTelemetryBufferFromFC_MaxSize = RAW_TELEMETRY_MIN_SEND_LENGTH;
int iTelemetryBufferFromFC_FilledBytes = 0;
u32 uTelemetryBufferFromFC_LastSendTime = 0;

u32 s_CountMessagesFromFCPerSecond = 0;
u32 s_CountMessagesFromFCPerSecondTemp = 0;

void telemetry_init()
{
   memset(&sPHFCT, 0, sizeof(sPHFCT));
   memset(&sPHFCE, 0, sizeof(sPHFCE));

   sPHFCT.uFCFlags = 0;
   sPHFCT.arm_time = 0;
   sPHFCT.flight_mode = 0;
   sPHFCT.gps_fix_type = 0;
   sPHFCT.pitch = 0;
   sPHFCT.roll = 0;
   sPHFCT.vspeed = 100000.0; // it's 1000m offset for negative values (1/100 m)
   sPHFCT.hdop = 2000; // (20)
   sPHFCT.distance = 0;
   sPHFCT.total_distance = 0;
   sPHFCT.fc_hudmsgpersec = 0;
   sPHFCT.fc_kbps = 0;
   sPHFCT.rc_rssi = 0xFF;
   sPHFCT.temperatureC = 0;
   for( int i=0; i<(int)(sizeof(sPHFCT.extra_info)/sizeof(sPHFCT.extra_info[0])); i++ )
      sPHFCT.extra_info[i] = 0;

   sPHFCT.extra_info[1] = 0xFF; // Second GPS
   sPHFCT.extra_info[2] = 0xFF; // Second GPS
   sPHFCT.extra_info[3] = 0xFF; // Second GPS
   sPHFCT.extra_info[4] = 0xFF; // Second GPS
   sPHFCT.extra_info[5] = 0; // FC telemetry packet index
   sPHFCT.extra_info[6] = 0; // FC telemetry messages per second

   sPHFCE.chunk_index = 0;
   sPHFCE.text[0] = 0;

   iTelemetryBufferFromFC_MaxSize = 300;
   if ( iTelemetryBufferFromFC_MaxSize > RAW_TELEMETRY_MAX_BUFFER )
      iTelemetryBufferFromFC_MaxSize = RAW_TELEMETRY_MAX_BUFFER;
   iTelemetryBufferFromFC_FilledBytes = 0;
   uTelemetryBufferFromFC_LastSendTime = g_TimeNow;
   log_line("[Telem] Telemetry from FC chunk size: %d", iTelemetryBufferFromFC_MaxSize);
}

t_packet_header_fc_telemetry* telemetry_get_fc_telemetry_header()
{
   return &sPHFCT;
}

t_packet_header_fc_extra* telemetry_get_fc_extra_telemetry_header()
{
   return &sPHFCE;
}

bool telemetry_detect_serial_port_to_use()
{
   if ( NULL == g_pCurrentModel )
   {
      log_softerror_and_alarm("[Telem] NULL model.");
      return false;
   }

   log_line("[Telem] Try to detect serial ports used for telemetry (model telemetry type: %d)...", g_pCurrentModel->telemetry_params.fc_telemetry_type);

   s_iCurrentTelemetrySerialPortIndex = -1;
   
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_NONE )
   {
      log_line("[Telem] Telemetry is not enabled (none).");
      return false;
   }

   for( int i=0; i<hardware_serial_get_ports_count(); i++ )
   {
       hw_serial_port_info_t* pPortInfo = hardware_get_serial_port_info(i);
       if ( NULL == pPortInfo )
          continue;
       if ( ! pPortInfo->iSupported )
          continue;
       if ( pPortInfo->iPortUsage == SERIAL_PORT_USAGE_TELEMETRY )
       {
          s_iCurrentTelemetrySerialPortIndex = i;
          s_iCurrentTelemetrySerialPortSpeed = (int)pPortInfo->lPortSpeed;
          log_line("[Telem] Found serial port (%s) (device: %s), index %d, set as telemetry type %s at speed %d bps",
             pPortInfo->szName, pPortInfo->szPortDeviceName, i, str_get_serial_port_usage(pPortInfo->iPortUsage), (int)pPortInfo->lPortSpeed);
          return true;
       }
   }

   log_line("[Telem] No serial ports found (of %d) configured for telemetry.", hardware_serial_get_ports_count() );
   return false;
}

void _send_raw_telemetry_packet_to_controller()
{
   if ( (! g_bRouterReady) || (iTelemetryBufferFromFC_FilledBytes <= 0) )
   {
      iTelemetryBufferFromFC_FilledBytes = 0;
      return;
   }
   if ( (NULL == g_pCurrentModel) || (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_NONE) )
   {
      iTelemetryBufferFromFC_FilledBytes = 0;
      return;
   }

   if ( iTelemetryBufferFromFC_FilledBytes > RAW_TELEMETRY_MAX_BUFFER )
   {
      log_softerror_and_alarm("[Telem] Trying to send more raw telemetry data than expected to downlink: %d bytes, max expected: %d bytes. Sending only max allowed.", iTelemetryBufferFromFC_FilledBytes, RAW_TELEMETRY_MAX_BUFFER);
      iTelemetryBufferFromFC_FilledBytes = RAW_TELEMETRY_MAX_BUFFER;
   }

   static u32 s_uRawTelemetryTotalBytesSentToController = 0;
   static u32 s_uRawTelemetrySegmentIndex = 0;

   s_uRawTelemetryTotalBytesSentToController += iTelemetryBufferFromFC_FilledBytes;

   t_packet_header PH;
   t_packet_header_telemetry_raw PHTR;

   radio_packet_init(&PH, PACKET_COMPONENT_TELEMETRY, PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD, STREAM_ID_TELEMETRY);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = 0;
   PH.total_length = sizeof(t_packet_header)+sizeof(t_packet_header_telemetry_raw) + iTelemetryBufferFromFC_FilledBytes;
      
   PHTR.telem_segment_index = s_uRawTelemetrySegmentIndex;
   PHTR.telem_total_data = s_uRawTelemetryTotalBytesSentToController;
   PHTR.telem_total_serial = s_uRawTelemetryTotalReadFromFCSerial;

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), (u8*)&PHTR, sizeof(t_packet_header_telemetry_raw));
   memcpy(buffer+sizeof(t_packet_header)+sizeof(t_packet_header_telemetry_raw), uTelemetryBufferFromFC_Serial, iTelemetryBufferFromFC_FilledBytes);
   
   if ( g_bRouterReady && (!g_bLongTaskStarted) && (! isRadioLinksInitInProgress()) )
   {
      int result = ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, PH.total_length);
      if ( result != PH.total_length )
         log_softerror_and_alarm("[Telem] Failed to send data to router. Sent result: %d", result );
      #ifdef LOG_RAW_TELEMETRY
      log_line("[Raw_Telem] Sent raw telemetry packet to router, index %u, %d / %d bytes", PHTR.telem_segment_index, iTelemetryBufferFromFC_FilledBytes, PH.total_length);
      #endif
   }

   uTelemetryBufferFromFC_LastSendTime = g_TimeNow;
   iTelemetryBufferFromFC_FilledBytes = 0;
   s_uRawTelemetrySegmentIndex++;

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
}


// Returns the file id
int telemetry_open_serial_port()
{
   telemetry_close_serial_port();

   s_uTimeSerialPortOpened = g_TimeNow;

   if ( NULL == g_pCurrentModel )
   {
      log_softerror_and_alarm("[Telem] Open: NULL model.");
      return -1;
   }

   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_NONE )
   {
      log_line("[Telem] Telemetry open: telemetry is not enabled on current model (none).");
      return -1;
   }

   if ( ! telemetry_detect_serial_port_to_use() )
   {
      log_line("[Telem] Telemetry open: Can't find a serial port configured for telemetry.");
      return -1;
   }

   if ( (s_iCurrentTelemetrySerialPortIndex < 0) || (s_iCurrentTelemetrySerialPortSpeed <= 0 ) )
   {
      log_softerror_and_alarm("[Telem] Telemetry open: Serial port info is invalid.");
      return -1;
   }

   hw_serial_port_info_t* pPortInfo = hardware_get_serial_port_info(s_iCurrentTelemetrySerialPortIndex);
   if ( NULL == pPortInfo )
   {
      log_softerror_and_alarm("[Telem] Telemetry open: Serial port hardware info is NULL.");
      return -1;
   }

   hardware_configure_serial(pPortInfo->szPortDeviceName, (long)s_iCurrentTelemetrySerialPortSpeed); 
         
   log_line("Opening serial port %s (%s) to flight controller (baud rate: %d) ...", pPortInfo->szName, pPortInfo->szPortDeviceName, s_iCurrentTelemetrySerialPortSpeed);
   s_iTelemetrySerialPortFile = hardware_open_serial_port(pPortInfo->szPortDeviceName, (long)s_iCurrentTelemetrySerialPortSpeed);
   if ( -1 == s_iTelemetrySerialPortFile )
      log_softerror_and_alarm("Failed to open serial port %s (%s) to flight controller.", pPortInfo->szName, pPortInfo->szPortDeviceName);
   else
      log_line("Opened serial port %s (%s) to flight controller successfully at baudrate: %d", pPortInfo->szName, pPortInfo->szPortDeviceName, s_iCurrentTelemetrySerialPortSpeed);

   log_line("Current telemetry type: %d", g_pCurrentModel->telemetry_params.fc_telemetry_type);
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MAVLINK )
      telemetry_mavlink_on_open_port(s_iTelemetrySerialPortFile);
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_LTM )
      telemetry_ltm_on_open_port(s_iTelemetrySerialPortFile);
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP )
      telemetry_msp_on_open_port(s_iTelemetrySerialPortFile);
     
   log_line("Opened telemetry protocol.");
   return s_iTelemetrySerialPortFile;
}

int telemetry_close_serial_port()
{
   if ( -1 != s_iTelemetrySerialPortFile )
   {
      close(s_iTelemetrySerialPortFile);
      log_line("[Telem] Closed serial port.");
   }
   s_iTelemetrySerialPortFile = -1;

   if ( NULL == g_pCurrentModel )
      return 0;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MAVLINK )
      telemetry_mavlink_on_close();
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_LTM )
      telemetry_ltm_on_close();
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP )
      telemetry_msp_on_close();

   telemetry_reset_time_last_telemetry_received();
   return 0;
}

u32 telemetry_last_time_opened()
{
   return s_uTimeSerialPortOpened;
}

u32 telemetry_time_last_telemetry_received()
{
   if ( NULL == g_pCurrentModel )
      return 0;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MAVLINK )
      return get_time_last_mavlink_message_from_fc();
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_LTM )
      return get_time_last_mavlink_message_from_fc();
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP )
      return telemetry_msp_get_last_command_received_time();
   return 0;
}

void telemetry_reset_time_last_telemetry_received()
{
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MAVLINK )
      set_time_last_mavlink_message_from_fc(0);
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_LTM )
      set_time_last_mavlink_message_from_fc(0);
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP )
      telemetry_msp_set_last_command_received_time(0);
}

int telemetry_get_serial_port_file()
{
   return s_iTelemetrySerialPortFile;
}

bool _telemetry_must_send_raw_telemetry_to_controller()
{
   if ( NULL == g_pCurrentModel )
      return false;

   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_NONE )
      return false;

   bool bMustSendFullTelemetryPackets = false;

   if ( g_bOSDPluginsNeedTelemetryStreams )
      bMustSendFullTelemetryPackets = true;

   if ( (g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_FULL_BIDIRECTIONAL) ||
        (g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_SEND_FULL_TELEMETRY_TO_CONTROLLER) ||
        (g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_SEND_FULL_TELEMETRY_TO_CONTROLLER_PLUGINS) )
     bMustSendFullTelemetryPackets = true;

   return bMustSendFullTelemetryPackets;
}

void _telemetry_addSerialDataFromFCToTelemetryBuffer(u8* pData, int iDataLength)
{
   if ( (NULL == g_pCurrentModel) || (iDataLength <= 0) || (NULL == pData) )
      return;

   if ( ! _telemetry_must_send_raw_telemetry_to_controller() )
      return;

   while ( iDataLength > 0 )
   {
      if ( iTelemetryBufferFromFC_FilledBytes + iDataLength < iTelemetryBufferFromFC_MaxSize )
      {
         memcpy(&(uTelemetryBufferFromFC_Serial[iTelemetryBufferFromFC_FilledBytes]), pData, iDataLength);
         iTelemetryBufferFromFC_FilledBytes += iDataLength;
         iDataLength = 0;
      }
      else
      {
         int chunkSize = iTelemetryBufferFromFC_MaxSize - iTelemetryBufferFromFC_FilledBytes;
         if ( chunkSize > 0 )
         {
            memcpy(&(uTelemetryBufferFromFC_Serial[iTelemetryBufferFromFC_FilledBytes]), pData, chunkSize);
            iTelemetryBufferFromFC_FilledBytes += chunkSize;
            pData += chunkSize;
            iDataLength -= chunkSize;
         }
      }
      if ( (iTelemetryBufferFromFC_FilledBytes >= RAW_TELEMETRY_MIN_SEND_LENGTH) || 
          ( (iTelemetryBufferFromFC_FilledBytes > 0) && (g_TimeNow >= uTelemetryBufferFromFC_LastSendTime + RAW_TELEMETRY_SEND_TIMEOUT) ) )
         _send_raw_telemetry_packet_to_controller();
   }
}

int telemetry_try_read_serial_port()
{
   if ( NULL == g_pCurrentModel )
      return -1;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_NONE )
      return -1;

   if ( s_iTelemetrySerialPortFile <= 0 )
      return -1;

   struct timeval to;
   to.tv_sec = 0;
   to.tv_usec = 2000; // 2 ms

   fd_set readset;   
   FD_ZERO(&readset);
   FD_SET(s_iTelemetrySerialPortFile, &readset);
   
   int res = select(s_iTelemetrySerialPortFile+1, &readset, NULL, NULL, &to);
   if ( res <= 0 )
      return 0;
   if ( ! FD_ISSET(s_iTelemetrySerialPortFile, &readset) )
      return 0;

   u8 uReadBuffer[1024];
   int iReadLength = read(s_iTelemetrySerialPortFile, uReadBuffer, sizeof(uReadBuffer)/sizeof(uReadBuffer[0]));
   if ( iReadLength <= 0 )
      return 0;

   s_uRawTelemetryTotalReadFromFCSerial += iReadLength;
   s_iFCSerialTelemetryReadBytesTempLastSecond += iReadLength;

   if ( _telemetry_must_send_raw_telemetry_to_controller() )
      _telemetry_addSerialDataFromFCToTelemetryBuffer(uReadBuffer, iReadLength);

   bool bNewFCMessage = false;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MAVLINK )
      bNewFCMessage = telemetry_mavlink_on_new_serial_data(uReadBuffer, iReadLength);
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_LTM )
      bNewFCMessage = telemetry_mavlink_on_new_serial_data(uReadBuffer, iReadLength);
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP )
      bNewFCMessage = telemetry_msp_on_new_serial_data(uReadBuffer, iReadLength);

   if ( bNewFCMessage )
      s_CountMessagesFromFCPerSecondTemp++;
   return iReadLength;
}

void telemetry_periodic_loop()
{
   if ( NULL == g_pCurrentModel )
      return;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_NONE )
      return;

   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MAVLINK )
      telemetry_mavlink_periodic_loop();
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_LTM )
      telemetry_ltm_periodic_loop();
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP )
      telemetry_msp_periodic_loop();

   if ( g_TimeNow >= g_TimeLastCheckEverySecond + 1000 )
   {
      g_TimeLastCheckEverySecond = g_TimeNow;

      s_CountMessagesFromFCPerSecond = s_CountMessagesFromFCPerSecondTemp;
      s_CountMessagesFromFCPerSecondTemp = 0;

      s_iFCSerialTelemetryReadBytesPerSecond = s_iFCSerialTelemetryReadBytesTempLastSecond;
      s_iFCSerialTelemetryReadBytesTempLastSecond = 0;

      if ( s_iFCSerialTelemetryReadBytesPerSecond > 50000 )
         s_iFCSerialTelemetryReadBytesPerSecond = 50000;
      if ( (s_iFCSerialTelemetryReadBytesPerSecond*8)/1000 < 255 )
         sPHFCT.fc_kbps = (u8) ((s_iFCSerialTelemetryReadBytesPerSecond*8)/1000); // from bytes to kbits
      else
         sPHFCT.fc_kbps = 255;

      if ( (s_iFCSerialTelemetryReadBytesPerSecond > 0 ) && (sPHFCT.fc_kbps == 0) )
         sPHFCT.fc_kbps = 1;

      if ( s_iTelemetrySerialPortFile > 0 )
         log_line("Last sec telem from FC: %d bytes/sec", s_iFCSerialTelemetryReadBytesPerSecond);

      if ( (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MAVLINK) ||
           (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_LTM) )
         telemetry_mavlink_on_second_lapse();

      if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP )
         telemetry_msp_on_second_lapse();
   }

   if ( _telemetry_must_send_raw_telemetry_to_controller() )
   if ( (iTelemetryBufferFromFC_FilledBytes >= RAW_TELEMETRY_MIN_SEND_LENGTH) || 
       ( (iTelemetryBufferFromFC_FilledBytes > 0) && (g_TimeNow >= uTelemetryBufferFromFC_LastSendTime + RAW_TELEMETRY_SEND_TIMEOUT) ) )
      _send_raw_telemetry_packet_to_controller();
}

bool telemetry_will_send_full_telemetry_to_controller()
{
   return _telemetry_must_send_raw_telemetry_to_controller();
}