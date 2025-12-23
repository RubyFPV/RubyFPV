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

#include "telemetry_msp.h"
#include "telemetry.h"
#include "shared_vars.h"
#include "timers.h"
#include "../base/ruby_ipc.h"
#include "../base/msp.h"

void broadcast_vehicle_stats();
bool isRadioLinksInitInProgress();

extern int s_fIPCToRouter;
extern t_packet_header_ruby_telemetry_extended_v6 sPHRTE;
extern u32 s_CountMessagesFromFCPerSecond;

u8 s_uMSPRawInputStream[256]; // Max size is one byte long
int s_iMSPRawInputStreamFilledBytes = 0;
int s_iMSPState = 0;
int s_iMSPDirection = 0;
u8  s_uMSPCommand = 0;
u8  s_uMSPPreviousCommand = 0xFF;
u8  s_uMSPDisplayPortCommand = 0xFF;
u8  s_uMSPPreviousDisplayPortCommand = 0xFF;
int s_iMSPCommandPayloadSize = 0;
int s_iMSPParsedPayloadSoFar = 0;
u8 s_uMSPCommandPayload[256]; // Max size is one byte long
u8 s_uMSPChecksum = 0;
u32 s_uLastMSPCommandReceivedTime = 0;

u8 s_uMSPOutputBuffer[MAX_PACKET_PAYLOAD];
int s_iMSPOutputBufferFilledBytes = 0;
u32 s_uTimeLastSentMSPPacketToRouter = 0;

t_packet_header_telemetry_msp s_PHTMSP;

u32 s_uMSPTimeLastConfigCommandToFC = 0;
bool s_bMSPGotFCInfo = false;
bool s_bMSPSentOSDCanvasSize = false;

u32 s_uMSPLastRequestBatteryInfoTime = 0;

void _send_msp_to_fc(u8 uCommand, u8* pData, int iDataLength)
{
   if ( telemetry_get_serial_port_file() <= 0 )
      return;

   if ( iDataLength < 0 )
      iDataLength = 0;
   if ( iDataLength > 250 )
      iDataLength = 250;
   
   u8 uMSPBuffer[256];
   u8 uChecksum;

   uMSPBuffer[0] = '$';
   uMSPBuffer[1] = 'M';
   uMSPBuffer[2] = '<';
   uMSPBuffer[3] = iDataLength;
   uMSPBuffer[4] = uCommand;

   uChecksum = (u8)iDataLength;
   uChecksum ^= uCommand;

   if ( NULL != pData )
   {
      for( int i=0; i<iDataLength; i++ )
      {
         uMSPBuffer[5+i] = pData[i];
         uChecksum ^= pData[i];
      }
   }

   uMSPBuffer[5 + iDataLength] = uChecksum;
   int iTotalSize = iDataLength + 6;

   static int s_iCountTelemetryMSPWriteErrors = 0;
   if ( write(telemetry_get_serial_port_file(), uMSPBuffer, iTotalSize) != iTotalSize )
   {
      s_iCountTelemetryMSPWriteErrors++;
      if ( s_iCountTelemetryMSPWriteErrors < 10 )
         log_softerror_and_alarm("[Telem] Failed to write MSP (%d bytes) to serial port to FC", iTotalSize);
   }
   else
      s_iCountTelemetryMSPWriteErrors = 0;
}


void _send_msp_telemetry_packet_to_controller(bool bSendIfEmpty)
{
   if ( (! bSendIfEmpty) && (s_iMSPOutputBufferFilledBytes <= 0) )
      return;

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_TELEMETRY, PACKET_TYPE_TELEMETRY_MSP, STREAM_ID_TELEMETRY);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = 0;
   PH.total_length = sizeof(t_packet_header) + sizeof(t_packet_header_telemetry_msp) + s_iMSPOutputBufferFilledBytes;

   u16 uId = s_PHTMSP.uSegmentIdAndExtraInfo & 0xFFFF;
   uId++;
   s_PHTMSP.uSegmentIdAndExtraInfo = (s_PHTMSP.uSegmentIdAndExtraInfo & 0xFFFF0000) | uId;

   s_PHTMSP.uSegmentIdAndExtraInfo = (s_PHTMSP.uSegmentIdAndExtraInfo & 0xFF00FFFF) | (((u32)base_compute_crc8(s_uMSPOutputBuffer, s_iMSPOutputBufferFilledBytes))<<16);
   
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, &PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), &s_PHTMSP, sizeof(t_packet_header_telemetry_msp));
   if ( 0 < s_iMSPOutputBufferFilledBytes )
      memcpy(buffer+sizeof(t_packet_header)+sizeof(t_packet_header_telemetry_msp), s_uMSPOutputBuffer, s_iMSPOutputBufferFilledBytes);

   if ( g_bRouterReady && (!g_bLongTaskStarted) && (! isRadioLinksInitInProgress()) )
   {
      int result = ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, PH.total_length);
      if ( result != PH.total_length )
         log_softerror_and_alarm("[Telem] Failed to send data to router. Sent result: %d", result );
   }

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   s_uTimeLastSentMSPPacketToRouter = g_TimeNow;
   s_iMSPOutputBufferFilledBytes = 0;
}

void telemetry_msp_on_open_port(int iSerialPortFile)
{
   s_iMSPRawInputStreamFilledBytes = 0;
   s_iMSPOutputBufferFilledBytes = 0;
   s_uTimeLastSentMSPPacketToRouter = g_TimeNow;
   s_iMSPState = MSP_STATE_WAIT_HEADER1;
   s_bMSPGotFCInfo = false;
   s_uMSPLastRequestBatteryInfoTime = 0;
   s_bMSPSentOSDCanvasSize = false;
   s_uMSPPreviousCommand = 0xFF;
   s_uMSPDisplayPortCommand = 0xFF;
   s_uMSPPreviousDisplayPortCommand = 0xFF;

   memset(&s_PHTMSP, 0, sizeof(t_packet_header_telemetry_msp));
   s_PHTMSP.uMSPOSDCols = 60;
   s_PHTMSP.uMSPOSDRows = 22;
   log_line("[Telem] Reset MSP OSD canvas size to: cols: %d, rows: %d", s_PHTMSP.uMSPOSDCols, s_PHTMSP.uMSPOSDRows);
}

void telemetry_msp_on_close()
{

}

void telemetry_msp_periodic_loop()
{
   if ( ! s_bMSPGotFCInfo )
   if ( g_TimeNow >= s_uMSPTimeLastConfigCommandToFC + 500 )
   {
      s_uMSPTimeLastConfigCommandToFC = g_TimeNow;
      _send_msp_to_fc(MSP_CMD_FC_VARIANT, NULL, 0);
      return;
   }

   if ( g_TimeNow >= s_uMSPLastRequestBatteryInfoTime + 500 )
   {
      s_uMSPLastRequestBatteryInfoTime = g_TimeNow;
      _send_msp_to_fc(MSP_CMD_BATTERY_STATE, NULL, 0);
      _send_msp_to_fc(MSP_CMD_STATUS, NULL, 0);

      // If in full raw mode (no MSP messages sent), then send some periodic empty MSP telemetry messages so that controller knows FC time and MSP screen resolution
      if ( telemetry_will_send_full_telemetry_to_controller() )
      {
         static int siCountSendEmptyMSP = 0;
         siCountSendEmptyMSP++;
         if ( (siCountSendEmptyMSP % 4) == 0 )
         {
             s_iMSPOutputBufferFilledBytes = 0;
             _send_msp_telemetry_packet_to_controller(true);
         }
      }
   } 
}


void telemetry_msp_on_second_lapse()
{
   t_packet_header_fc_telemetry* pFCTelem = telemetry_get_fc_telemetry_header();
   if ( NULL == pFCTelem )
      return;
   
   if ( pFCTelem->flight_mode != 0 )
   if ( pFCTelem->flight_mode & FLIGHT_MODE_ARMED )
      pFCTelem->arm_time++;
   
   g_pCurrentModel->updateStatsEverySecond(pFCTelem);
   broadcast_vehicle_stats();
}


u32 telemetry_msp_get_last_command_received_time()
{
   return s_uLastMSPCommandReceivedTime;
}

void telemetry_msp_set_last_command_received_time(u32 uTime)
{
   s_uLastMSPCommandReceivedTime = uTime;
}

void _add_msp_data_to_output(u8* pData, int iDataLength, bool bSendNow)
{
   if ( (NULL == pData) || (iDataLength <= 0) || (iDataLength > 255) || (telemetry_will_send_full_telemetry_to_controller()) )
      return;

   // No more room in the output? Send packet
   if ( s_iMSPOutputBufferFilledBytes + iDataLength >= 1100 )
      _send_msp_telemetry_packet_to_controller(false);

   memcpy(&s_uMSPOutputBuffer[s_iMSPOutputBufferFilledBytes], pData, iDataLength);
   s_iMSPOutputBufferFilledBytes += iDataLength;

   if ( bSendNow )
      _send_msp_telemetry_packet_to_controller(false);
}

void _parse_msp_osd_command()
{
   if ( (s_uMSPCommand != MSP_CMD_DISPLAYPORT) || (s_iMSPCommandPayloadSize < 1) || (s_iMSPDirection != MSP_DIR_FROM_FC) )
      return;

   if (! (s_PHTMSP.uMSPFlags & MSP_FLAG_GOT_FC_TYPE) )
      return;
   if ( 0 == (s_PHTMSP.uMSPFlags & MSP_FLAGS_FC_TYPE_MASK) )
      return;

   s_uMSPPreviousDisplayPortCommand = s_uMSPDisplayPortCommand;
   s_uMSPDisplayPortCommand = s_uMSPCommandPayload[0];

   bool bSendNow = false;
   bool bSkip = false;
   static u32 s_uLastTimeMSPUpdateScreenCommand = 0;

   switch ( s_uMSPDisplayPortCommand )
   {
      case MSP_DISPLAYPORT_DRAW_STRING:
         {
            int y = s_uMSPCommandPayload[1];
            int x = s_uMSPCommandPayload[2];
            if ( x >= s_PHTMSP.uMSPOSDCols )
            {
               s_PHTMSP.uMSPFlags |= MSP_FLAG_AUTO_ADJUSTED_OSD_SIZE;
               if ( x >= 50 )
                  s_PHTMSP.uMSPOSDCols = 60;
               else if ( x >= 30 )
                  s_PHTMSP.uMSPOSDCols = 50;
               else
                  s_PHTMSP.uMSPOSDCols = 30;
               log_line("[Telem] Auto adjusted MSP OSD canvas size to: cols: %d, rows: %d", s_PHTMSP.uMSPOSDCols, s_PHTMSP.uMSPOSDRows);
            }
            if ( y >= s_PHTMSP.uMSPOSDRows )
            {
               s_PHTMSP.uMSPFlags |= MSP_FLAG_AUTO_ADJUSTED_OSD_SIZE;
               if ( y >= 20 )
                  s_PHTMSP.uMSPOSDRows = 22;
               else if ( y >= 18 )
                  s_PHTMSP.uMSPOSDRows = 20;
               else if ( y >= 16 )
                  s_PHTMSP.uMSPOSDRows = 18;
               else
                  s_PHTMSP.uMSPOSDRows = 16;
               log_line("[Telem] Auto adjusted MSP OSD canvas size to: cols: %d, rows: %d", s_PHTMSP.uMSPOSDCols, s_PHTMSP.uMSPOSDRows);
            }
            char szData[128];
            memset(szData, 0, 128);
            memcpy(szData, &s_uMSPCommandPayload[4], s_iMSPCommandPayloadSize-4);
            if ( s_iMSPCommandPayloadSize < 4 )
               bSkip = true;
         }
         break;

      case MSP_DISPLAYPORT_SET_OPTIONS:
         {
            if ( s_iMSPCommandPayloadSize >= 3 )
            {
               s_PHTMSP.uMSPFlags |= MSP_FLAG_GOT_FC_DISPLAY_OPTIONS;
               bool bAdjusted = false;
               if ( s_uMSPCommandPayload[2] == MSP_SD_OPTION_30_16 )
               {
                  s_PHTMSP.uMSPOSDCols = 30;
                  s_PHTMSP.uMSPOSDRows = 16;
                  bAdjusted = true;
               }
               if ( s_uMSPCommandPayload[2] == MSP_HD_OPTION_50_18 )
               {
                  s_PHTMSP.uMSPOSDCols = 50;
                  s_PHTMSP.uMSPOSDRows = 18;
                  bAdjusted = true;
               }
               if ( s_uMSPCommandPayload[2] == MSP_HD_OPTION_30_16 )
               {
                  s_PHTMSP.uMSPOSDCols = 30;
                  s_PHTMSP.uMSPOSDRows = 16;
                  bAdjusted = true;
               }
               if ( s_uMSPCommandPayload[2] == MSP_HD_OPTION_60_22 )
               {
                  s_PHTMSP.uMSPOSDCols = 60;
                  s_PHTMSP.uMSPOSDRows = 22;
                  bAdjusted = true;
               }
               if ( bAdjusted )
               {
                  log_line("[Telem] Got MSP OSD SET OPTIONS: value: %d, canvas size adjusted to: cols: %d, rows: %d", s_uMSPCommandPayload[2], s_PHTMSP.uMSPOSDCols, s_PHTMSP.uMSPOSDRows);
                  s_PHTMSP.uMSPFlags |= MSP_FLAG_FC_DID_ADJUSTED_OSD_SIZE;
               }
               else
                  log_line("[Telem] Got MSP OSD SET OPTIONS: value: %d, no change in canvas size: cols: %d, rows: %d", s_uMSPCommandPayload[2], s_PHTMSP.uMSPOSDCols, s_PHTMSP.uMSPOSDRows);
            }
         }
         break;

      case MSP_DISPLAYPORT_CLEAR:
         //bSendNow = true;
         _send_msp_telemetry_packet_to_controller(false);
         break;

      case MSP_DISPLAYPORT_KEEPALIVE:
         if ( s_uMSPPreviousDisplayPortCommand != MSP_DISPLAYPORT_KEEPALIVE )
         if ( s_uMSPPreviousDisplayPortCommand != MSP_DISPLAYPORT_DRAW_SCREEN )
         {
            if ( g_TimeNow > s_uLastTimeMSPUpdateScreenCommand + 200 )
            {
               bSendNow = true;
               s_uLastTimeMSPUpdateScreenCommand = g_TimeNow;
            }
         }
         break;

      case MSP_DISPLAYPORT_DRAW_SCREEN:
         if ( s_uMSPPreviousDisplayPortCommand != MSP_DISPLAYPORT_DRAW_SCREEN )
         if ( s_uMSPPreviousDisplayPortCommand != MSP_DISPLAYPORT_KEEPALIVE )
         {
            if ( g_TimeNow > s_uLastTimeMSPUpdateScreenCommand + 200 )
            {
               bSendNow = true;
               s_uLastTimeMSPUpdateScreenCommand = g_TimeNow;
            }
         }
         break;

      case MSP_DISPLAYPORT_DRAW_SYSTEM:
         if ( g_TimeNow > s_uLastTimeMSPUpdateScreenCommand + 200 )
         {
            bSendNow = true;
            s_uLastTimeMSPUpdateScreenCommand = g_TimeNow;
         }
         break;

      default:
         break;
   }

   if ( ! bSkip )
      _add_msp_data_to_output(s_uMSPRawInputStream, s_iMSPRawInputStreamFilledBytes, bSendNow);
}

void _parse_msp_command()
{
   if ( s_iMSPDirection != MSP_DIR_FROM_FC )
      return;

   switch ( s_uMSPCommand )
   {
      case MSP_CMD_STATUS:
       {
          int iArmed = (s_uMSPCommandPayload[6] & 0x01);
          // To remove
          //iArmed = (g_TimeNow/1000/5) % 2;

          t_packet_header_fc_telemetry* pFCTelem = telemetry_get_fc_telemetry_header();
          if ( NULL != pFCTelem )
          {
             pFCTelem->flight_mode &= ~FLIGHT_MODE_ARMED;
             if ( iArmed )
                pFCTelem->flight_mode |= FLIGHT_MODE_ARMED;

             if ( pFCTelem->flight_mode & FLIGHT_MODE_ARMED )
                pFCTelem->uFCFlags |= FC_TELE_FLAGS_ARMED;
             else
                pFCTelem->uFCFlags &= ~FC_TELE_FLAGS_ARMED;

             if ( g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_FORCE_ARMED )
                pFCTelem->flight_mode |= FLIGHT_MODE_ARMED;
          }
       }
       break;
      case MSP_CMD_STATUS_EX:
       {
       }
       break;

      case MSP_CMD_BATTERY_STATE:
       {
       }
       break;
      case MSP_CMD_FC_VARIANT:
         {
            char szBuff[5];
            strncpy(szBuff, (char*)s_uMSPCommandPayload, 4);
            szBuff[4] = 0;
            log_line("[Telem] Got MSP FC variant: (%s)", szBuff);
            s_PHTMSP.uMSPFlags |= MSP_FLAG_GOT_FC_TYPE;
            s_PHTMSP.uMSPFlags &= ~MSP_FLAGS_FC_TYPE_MASK;
            if ( strncmp("BTFL", (char*)s_uMSPCommandPayload, s_iMSPCommandPayloadSize) == 0 )
               s_PHTMSP.uMSPFlags |= MSP_FLAGS_FC_TYPE_BETAFLIGHT;
            else if ( strncmp("ARDU", (char*)s_uMSPCommandPayload, s_iMSPCommandPayloadSize) == 0 )
               s_PHTMSP.uMSPFlags |= MSP_FLAGS_FC_TYPE_ARDUPILOT;
            else if ( strncmp("PITL", (char*)s_uMSPCommandPayload, s_iMSPCommandPayloadSize) == 0 )
               s_PHTMSP.uMSPFlags |= MSP_FLAGS_FC_TYPE_PITLAB;
            else
               s_PHTMSP.uMSPFlags |= MSP_FLAGS_FC_TYPE_INAV;

            _send_msp_to_fc(MSP_CMD_API_VERSION, NULL, 0);
         }
         break;

      case MSP_CMD_API_VERSION:
         {
            s_bMSPGotFCInfo = true;
            for( int i=0; i<s_iMSPCommandPayloadSize; i++ )
               log_line("[Telem] Got MSP API version, byte[%d]=%d", i, s_uMSPCommandPayload[i]);

            if ( s_PHTMSP.uMSPFlags & MSP_FLAG_GOT_FC_TYPE )
            if ( (s_PHTMSP.uMSPFlags & MSP_FLAGS_FC_TYPE_MASK) == MSP_FLAGS_FC_TYPE_BETAFLIGHT )
            if ( s_uMSPCommandPayload[2] >= 45 ) // minor version
            if ( ! s_bMSPSentOSDCanvasSize )
            {
               s_bMSPSentOSDCanvasSize = true;
               u8 uBuffer[2];
               uBuffer[0] = s_PHTMSP.uMSPOSDCols;
               uBuffer[1] = s_PHTMSP.uMSPOSDRows;
               _send_msp_to_fc(MSP_CMD_SET_OSD_CANVAS, uBuffer, 2);
               log_line("[Telem] Sent MSP OSD SET CANVAS to FC, cols: %d, rows: %d", s_PHTMSP.uMSPOSDCols, s_PHTMSP.uMSPOSDRows);
            }
         }
         break;
      default: break;
   }
}

bool telemetry_msp_on_new_serial_data(u8* pData, int iDataLength)
{
   if ( (NULL == pData) || (iDataLength <= 0) )
      return false;
   bool bReturn = false;

   for( int i=0; i<iDataLength; i++ )
   {
      s_uMSPRawInputStream[s_iMSPRawInputStreamFilledBytes] = *pData;
      s_iMSPRawInputStreamFilledBytes++;
      if ( s_iMSPRawInputStreamFilledBytes >= 256 )
         s_iMSPRawInputStreamFilledBytes = 0;

      switch(s_iMSPState)
      {
         case MSP_STATE_ERROR:
         case MSP_STATE_WAIT_HEADER1:
            s_iMSPRawInputStreamFilledBytes = 0;
            if ( *pData == '$' )
            {
               s_uMSPRawInputStream[0] = *pData;
               s_iMSPRawInputStreamFilledBytes = 1;
               s_iMSPState = MSP_STATE_WAIT_HEADER2;
            }
            break;

         case MSP_STATE_WAIT_HEADER2:
            if ( *pData == 'M' )
               s_iMSPState = MSP_STATE_WAIT_DIR;
            else
            {
               s_iMSPState = MSP_STATE_ERROR;
               s_iMSPRawInputStreamFilledBytes = 0;
            }
            break;

         case MSP_STATE_WAIT_DIR:
            if ( *pData == '<' )
            {
               s_iMSPState = MSP_STATE_WAIT_SIZE;
               s_iMSPDirection = MSP_DIR_TO_FC;
            }
            else if ( *pData == '>' )
            {
               s_iMSPState = MSP_STATE_WAIT_SIZE;
               s_iMSPDirection = MSP_DIR_FROM_FC;
            }
            else
            {
               s_iMSPState = MSP_STATE_WAIT_HEADER1;
               s_iMSPRawInputStreamFilledBytes = 0;
            }
            break;

         case MSP_STATE_WAIT_SIZE:
            s_iMSPCommandPayloadSize = (int) *pData;
            s_uMSPChecksum = *pData;
            s_iMSPState = MSP_STATE_WAIT_TYPE;
            break;

         case MSP_STATE_WAIT_TYPE:
            s_uMSPPreviousCommand = s_uMSPCommand;
            s_uMSPCommand = *pData;
            s_uMSPChecksum ^= *pData;
            s_iMSPParsedPayloadSoFar = 0;
            if ( s_iMSPCommandPayloadSize > 0 )
               s_iMSPState = MSP_STATE_PARSE_DATA;
            else
               s_iMSPState = MSP_STATE_WAIT_CHECKSUM;
            break;

         case MSP_STATE_PARSE_DATA:
            s_uMSPCommandPayload[s_iMSPParsedPayloadSoFar] = *pData;
            s_iMSPParsedPayloadSoFar++;
            s_uMSPChecksum ^= *pData;
            if ( s_iMSPParsedPayloadSoFar >= s_iMSPCommandPayloadSize )
               s_iMSPState = MSP_STATE_WAIT_CHECKSUM;
            break;

         case MSP_STATE_WAIT_CHECKSUM:
            if ( s_uMSPChecksum == *pData )
            {
               if ( 0 == s_uLastMSPCommandReceivedTime )
                  log_line("Started receiving valid MSP telemetry from FC");
               s_uLastMSPCommandReceivedTime = g_TimeNow;
               if ( s_uMSPCommand == MSP_CMD_DISPLAYPORT )
                  _parse_msp_osd_command();
               else
                  _parse_msp_command();
               bReturn = true;
            }
            s_iMSPState = MSP_STATE_WAIT_HEADER1;
            s_iMSPRawInputStreamFilledBytes = 0;
            break;

         default:
            break;
      }
      pData++;
   }

   return bReturn;
}


void telemetry_msp_send_to_controller()
{
   if ( (NULL == g_pCurrentModel) || (!g_bRouterReady) )
      return;

   t_packet_header PH;

   radio_packet_init(&PH, PACKET_COMPONENT_TELEMETRY, PACKET_TYPE_FC_TELEMETRY, STREAM_ID_TELEMETRY);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = 0;
   t_packet_header_fc_telemetry* pFCTelem = telemetry_get_fc_telemetry_header();

   pFCTelem->fc_telemetry_type = g_pCurrentModel->telemetry_params.fc_telemetry_type;
   if ( g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_FORCE_ARMED )
      pFCTelem->flight_mode |= FLIGHT_MODE_ARMED;

   pFCTelem->extra_info[5]++;
   pFCTelem->extra_info[6] = (s_CountMessagesFromFCPerSecond>255)?255:s_CountMessagesFromFCPerSecond;

   pFCTelem->uFCFlags = pFCTelem->uFCFlags & (~FC_TELE_FLAGS_HAS_MESSAGE);
   pFCTelem->uFCFlags = pFCTelem->uFCFlags & (~FC_TELE_FLAGS_RC_FAILSAFE);

   radio_packet_init(&PH, PACKET_COMPONENT_TELEMETRY, PACKET_TYPE_FC_TELEMETRY, STREAM_ID_TELEMETRY);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = 0;
   PH.total_length = (u16)sizeof(t_packet_header) + (u16)sizeof(t_packet_header_fc_telemetry);

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, &PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), pFCTelem, sizeof(t_packet_header_fc_telemetry));

   if ( g_bRouterReady && (! isRadioLinksInitInProgress()) )
   {
      int result = ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, PH.total_length);
      if ( result != PH.total_length )
         log_softerror_and_alarm("Failed to send data to router. Sent result: %d", result );
   }

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
}
