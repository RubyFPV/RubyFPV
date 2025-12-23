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
#include "../base/hardware.h"
#include "../base/hardware_radio.h"
#include "../base/flags.h"
#include "../base/flags_video.h"
#include "../radio/radioflags.h"
#include "../radio/radiopackets2.h"
#include <ctype.h>
#include <sched.h>
#include "string_utils.h"
#include "strings_loc.h"


void str_sanitize_modelname(char* szName)
{
   if ( NULL == szName || 0 == szName[0] )
      return;

   //log_line("Sanitize model name: [%s]", szName);
   int len = strlen(szName);

   // Remove ending invalid chars

   len--;
   while ( len > 0 )
   {
      int isValid = 1;

      if ( szName[len] == ' ' )
         isValid = 0;
      if ( szName[len] < 48 )
         isValid = 0;
      if ( szName[len] >= 58 && szName[len] <= 64 )
         isValid = 0;
      if ( szName[len] >= 91 && szName[len] <= 96 )
         isValid = 0;
      if ( szName[len] > 122 )
         isValid = 0;
      if ( isValid )
         break;
      szName[len] = 0;
      len--;
   }

   // Remove starting invalid chars

   len = strlen(szName);

   int iSkip = 0;
   while ( iSkip < len )
   {
      int isValid = 1;

      if ( szName[iSkip] == ' ' )
         isValid = 0;
      if ( szName[iSkip] < 48 )
         isValid = 0;
      if ( szName[iSkip] >= 58 && szName[iSkip] <= 64 )
         isValid = 0;
      if ( szName[iSkip] >= 91 && szName[iSkip] <= 96 )
         isValid = 0;
      if ( szName[iSkip] > 122 )
         isValid = 0;
      if ( isValid )
         break;
      iSkip++;
   }

   if ( iSkip > 0 )
   {
      for( int i=0; i<len-iSkip; i++ )
         szName[i] = szName[i+iSkip];
      szName[len-iSkip] = 0;
      len -= iSkip;
   }

   // Replace middle invalid chars

   len = strlen(szName);

   for( int i=0; i<len; i++ )
   {
      int isValid = 1;

      if ( szName[i] == ' ' )
         isValid = 0;
      if ( szName[i] < 48 )
         isValid = 0;
      if ( szName[i] >= 58 && szName[i] <= 64 )
         isValid = 0;
      if ( szName[i] >= 91 && szName[i] <= 96 )
         isValid = 0;
      if ( szName[i] > 122 )
         isValid = 0;
      if ( ! isValid )
         szName[i] = '_';
   }
   //log_line("Sanitized model name: [%s]", szName);
}

void str_sanitize_filename(char* szFileName)
{
   if ( NULL == szFileName || 0 == szFileName[0] )
      return;

   log_line("Sanitize filename: [%s]", szFileName);
   int len = strlen(szFileName);

   len--;
   while ( len > 0 )
   {
      if ( szFileName[len] != ' ' && szFileName[len] != '.' && szFileName[len] != '/' && szFileName[len] != '\\' && szFileName[len] != 96 && szFileName[len] <= 122 && szFileName[len] >= 48 )
         break;
      szFileName[len] = 0;
      len--;
   }

   len = strlen(szFileName);
   for( int i=0; i<len; i++ )
   {
      if ( szFileName[i] == ' ' || szFileName[i] == '\'' || szFileName[i] == '\"' )
         szFileName[i] = '_';
      if ( szFileName[i] < 32 )
         szFileName[i] = '_';
      else
      {
         if ( szFileName[i] < 48 )
            szFileName[i] = '-';
         else if ( szFileName[i] >= 58 && szFileName[i] <= 64 )
            szFileName[i] = '-';
         else if ( szFileName[i] >= 91 && szFileName[i] <= 96 )
            szFileName[i] = '-';
         else if ( szFileName[i] > 122 )
            szFileName[i] = '-';
      }
   }
   log_line("Sanitized filename: [%s]", szFileName);
}

char* str_capitalize_first_letter(char* szText)
{
   if ( (NULL != szText) && (0 != szText[0]) )
      szText[0] = toupper(szText[0]);
   return szText;
}

char* str_format_time(u32 miliseconds)
{
   static char s_szFormatTime[4][64];
   static int s_iFormatTimeIndex = 0;
   s_iFormatTimeIndex++;
   if ( s_iFormatTimeIndex >= 4 )
      s_iFormatTimeIndex = 0;
   sprintf(s_szFormatTime[s_iFormatTimeIndex],"%d:%02d:%02d.%03d", (int)(miliseconds/1000/60/60), (int)(miliseconds/1000/60)%60, (int)((miliseconds/1000)%60), (int)(miliseconds%1000));
   return s_szFormatTime[s_iFormatTimeIndex];
}

char* str_format_binary_number(u32 uNumber)
{
   static char s_szFormatBinaryNumber[64];
   s_szFormatBinaryNumber[0] = 0;
   char* p = s_szFormatBinaryNumber;
   for( int k=0; k<4; k++ )
   {
      if ( k != 0 )
      {
         *p = ' ';
         p++;
         *p = 0;
      }
      if ( uNumber & 0xFF000000 )
      {
         strcpy(p, "0b");
         for( int i=0; i<8; i++ )
         {
            *p = (uNumber & (0x01<<31))?'1':'0';
            p++;
            *p = 0;
            uNumber = (uNumber<<1);
         }
      }
      else
      {
         strcpy(p, "0x00");
         p += 4;
         uNumber = (uNumber << 8);
      }
   }
   *p = 0;
   return s_szFormatBinaryNumber;
}

char* str_format_schedule_policy(int iSchedulePolicy)
{
   static char s_szSchedulePolicyName[64];
   switch(iSchedulePolicy)
   {
      //case SCHED_DEADLINE: strcpy(s_szSchedulePolicyName,"SCHED_DEADLINE"); break;
      case SCHED_RR: strcpy(s_szSchedulePolicyName,"SCHED_RR"); break;
      case SCHED_FIFO: strcpy(s_szSchedulePolicyName,"SCHED_FIFO"); break;
      //case SCHED_NORMAL: strcpy(s_szSchedulePolicyName,"SCHED_NORMAL"); break;
      case SCHED_OTHER: strcpy(s_szSchedulePolicyName,"SCHED_OTHER"); break;
      //case SCHED_IDLE: strcpy(s_szSchedulePolicyName,"SCHED_IDLE"); break;
      default: strcpy(s_szSchedulePolicyName, "N/A"); break;
   }
   return s_szSchedulePolicyName;
}


char* str_get_pipe_flags(int iFlags)
{
   static char s_szBufferPipeFlags[256];
   s_szBufferPipeFlags[0] = 0;

   strcpy(s_szBufferPipeFlags, "[");

   if ( iFlags & O_RDONLY )
      strcat(s_szBufferPipeFlags, " O_RDONLY");
   else
      strcat(s_szBufferPipeFlags, " !O_RDONLY");

   if ( iFlags & O_WRONLY )
      strcat(s_szBufferPipeFlags, " O_WRONLY");
   else
      strcat(s_szBufferPipeFlags, " !O_WRONLY");

   if ( iFlags & O_RDWR )
      strcat(s_szBufferPipeFlags, " O_RDWR");
   else
      strcat(s_szBufferPipeFlags, " !O_RDWR");

   if ( iFlags & O_APPEND )
      strcat(s_szBufferPipeFlags, " O_APPEND");
   else
      strcat(s_szBufferPipeFlags, " !O_APPEND");

   if ( iFlags & O_ASYNC )
      strcat(s_szBufferPipeFlags, " O_ASYNC");
   else
      strcat(s_szBufferPipeFlags, " !O_ASYNC");

   if ( iFlags & O_NONBLOCK )
      strcat(s_szBufferPipeFlags, " O_NONBLOCK");
   else
      strcat(s_szBufferPipeFlags, " !O_NONBLOCK");

   if ( iFlags & O_DSYNC )
      strcat(s_szBufferPipeFlags, " O_DSYNC");
   else
      strcat(s_szBufferPipeFlags, " !O_DSYNC");

   if ( iFlags & O_SYNC )
      strcat(s_szBufferPipeFlags, " O_SYNC");
   else
      strcat(s_szBufferPipeFlags, " !O_SYNC");

   if ( iFlags & O_EXCL )
      strcat(s_szBufferPipeFlags, " O_EXCL");
   else
      strcat(s_szBufferPipeFlags, " !O_EXCL");

   strcat(s_szBufferPipeFlags, " ]");
   return s_szBufferPipeFlags;
}

char* str_format_processes_flags(u32 uProcessesFlags)
{
   static char s_szProcessesFlags[128];
   s_szProcessesFlags[0] = 0;

   if ( uProcessesFlags & PROCESSES_FLAGS_BALANCE_INTERRUPTS_CORES )
      strcat(s_szProcessesFlags, "BALANCE_INTERRUPTS ");
   if ( uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS )
      strcat(s_szProcessesFlags, "ENABLE_PRIORITY_ADJUSTMENT ");
   if ( uProcessesFlags & PROCESSES_FLAGS_ENABLE_AFFINITY_CORES )
      strcat(s_szProcessesFlags, "ENABLE_CORE_AFFINITY ");
   if ( uProcessesFlags & PROCESSES_FLAGS_ENABLE_AFFINITY_CORES_VIDEO_CAPTURE )
      strcat(s_szProcessesFlags, "ENABLE_CORE_AFFINITY_VIDEOCAP ");
   return s_szProcessesFlags;
}

char* str_get_packet_type(int iPacketType)
{
   static char s_szPacketType[128];
   snprintf(s_szPacketType, sizeof(s_szPacketType)/sizeof(s_szPacketType[0]), "Unknown p-type %d", iPacketType);

   switch(iPacketType)
   {
      case PACKET_TYPE_EMBEDED_SHORT_PACKET: strcpy(s_szPacketType, "PACKET_TYPE_EMBEDED_SHORT_PACKET"); break;
      case PACKET_TYPE_EMBEDED_FULL_PACKET: strcpy(s_szPacketType, "PACKET_TYPE_EMBEDED_FULL_PACKET"); break;
      
      case PACKET_TYPE_RUBY_PING_CLOCK:          strcpy(s_szPacketType, "PACKET_TYPE_RUBY_PING_CLOCK"); break;
      case PACKET_TYPE_RUBY_PING_CLOCK_REPLY:    strcpy(s_szPacketType, "PACKET_TYPE_RUBY_PING_CLOCK_REPLY"); break;
      case PACKET_TYPE_RUBY_RADIO_REINITIALIZED: strcpy(s_szPacketType, "PACKET_TYPE_RUBY_RADIO_REINITIALIZED"); break;
      case PACKET_TYPE_RUBY_MODEL_SETTINGS:      strcpy(s_szPacketType, "PACKET_TYPE_RUBY_MODEL_SETTINGS"); break;
      case PACKET_TYPE_RUBY_PAIRING_REQUEST:     strcpy(s_szPacketType, "PACKET_TYPE_RUBY_PAIRING_REQUEST"); break;
      case PACKET_TYPE_RUBY_PAIRING_CONFIRMATION: strcpy(s_szPacketType, "PACKET_TYPE_RUBY_PAIRING_CONFIRMATION"); break;
      case PACKET_TYPE_RUBY_RADIO_CONFIG_UPDATED: strcpy(s_szPacketType, "PACKET_TYPE_RUBY_RADIO_CONFIG_UPDATED"); break;
      case PACKET_TYPE_RUBY_LOG_FILE_SEGMENT:    strcpy(s_szPacketType, "PACKET_TYPE_RUBY_LOG_FILE_SEGMENT"); break;
      case PACKET_TYPE_RUBY_MESSAGE:             strcpy(s_szPacketType, "PACKET_TYPE_RUBY_MESSAGE"); break;
      case PACKET_TYPE_RUBY_ALARM:               strcpy(s_szPacketType, "PACKET_TYPE_RUBY_ALARM"); break;
      case PACKET_TYPE_VIDEO_DATA:               strcpy(s_szPacketType, "PACKET_TYPE_VIDEO_DATA"); break;
      case PACKET_TYPE_AUDIO_SEGMENT:            strcpy(s_szPacketType, "PACKET_TYPE_AUDIO_SEGMENT"); break;
      case PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS:   strcpy(s_szPacketType, "PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS"); break;
      case PACKET_TYPE_VIDEO_ADAPTIVE_VIDEO_PARAMS:     strcpy(s_szPacketType, "PACKET_TYPE_VIDEO_ADAPTIVE_VIDEO_PARAMS"); break;
      case PACKET_TYPE_VIDEO_ADAPTIVE_VIDEO_PARAMS_ACK: strcpy(s_szPacketType, "PACKET_TYPE_VIDEO_ADAPTIVE_VIDEO_PARAMS_ACK"); break;
      case PACKET_TYPE_COMMAND:                  strcpy(s_szPacketType, "PACKET_TYPE_COMMAND"); break;
      case PACKET_TYPE_COMMAND_RESPONSE:         strcpy(s_szPacketType, "PACKET_TYPE_COMMAND_RESPONSE"); break;
      case PACKET_TYPE_SIK_CONFIG:               strcpy(s_szPacketType, "PACKET_TYPE_SIK_CONFIG"); break;
      case PACKET_TYPE_DEBUG_INFO:               strcpy(s_szPacketType, "PACKET_TYPE_DEBUG_INFO"); break;
      case PACKET_TYPE_RC_FULL_FRAME:            strcpy(s_szPacketType, "PACKET_TYPE_RC_FULL_FRAME"); break;
      case PACKET_TYPE_RC_DOWNLOAD_INFO:         strcpy(s_szPacketType, "PACKET_TYPE_RC_DOWNLOAD_INFO"); break;
      case PACKET_TYPE_TEST_RADIO_LINK:          strcpy(s_szPacketType, "PACKET_TYPE_TEST_RADIO_LINK"); break;
      // Telemetry

      case PACKET_TYPE_RUBY_TELEMETRY_SHORT:      strcpy(s_szPacketType, "PACKET_TYPE_RUBY_TELEMETRY_SHORT"); break;
      case PACKET_TYPE_RUBY_TELEMETRY_EXTENDED:   strcpy(s_szPacketType, "PACKET_TYPE_RUBY_TELEMETRY_EXTENDED"); break;
      case PACKET_TYPE_FC_TELEMETRY:              strcpy(s_szPacketType, "PACKET_TYPE_FC_TELEMETRY"); break;
      case PACKET_TYPE_FC_TELEMETRY_EXTENDED:     strcpy(s_szPacketType, "PACKET_TYPE_FC_TELEMETRY_EXTENDED"); break;
      case PACKET_TYPE_FC_RC_CHANNELS:            strcpy(s_szPacketType, "PACKET_TYPE_FC_RC_CHANNELS"); break;
      case PACKET_TYPE_RC_TELEMETRY:              strcpy(s_szPacketType, "PACKET_TYPE_RC_TELEMETRY"); break;
      case PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY:     strcpy(s_szPacketType, "PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY"); break;
      case PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS: strcpy(s_szPacketType, "PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS"); break;
      case PACKET_TYPE_RUBY_TELEMETRY_DEV_VIDEO_BITRATE_HISTORY: strcpy(s_szPacketType, "PACKET_TYPE_RUBY_TELEMETRY_DEV_VIDEO_BITRATE_HISTORY"); break;
      case PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD:    strcpy(s_szPacketType, "PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD"); break;
      case PACKET_TYPE_TELEMETRY_RAW_UPLOAD:      strcpy(s_szPacketType, "PACKET_TYPE_TELEMETRY_RAW_UPLOAD"); break;
      case PACKET_TYPE_AUX_DATA_LINK_UPLOAD:      strcpy(s_szPacketType, "PACKET_TYPE_AUX_DATA_LINK_UPLOAD"); break;
      case PACKET_TYPE_AUX_DATA_LINK_DOWNLOAD:    strcpy(s_szPacketType, "PACKET_TYPE_AUX_DATA_LINK_DOWNLOAD"); break;
      case PACKET_TYPE_RUBY_TELEMETRY_VIDEO_INFO_STATS:  strcpy(s_szPacketType, "PACKET_TYPE_RUBY_TELEMETRY_VIDEO_INFO_STATS"); break;
      case PACKET_TYPE_RUBY_TELEMETRY_RADIO_RX_HISTORY: strcpy(s_szPacketType, "PACKET_TYPE_RUBY_TELEMETRY_RADIO_RX_HISTORY"); break;
      case PACKET_TYPE_TELEMETRY_MSP:             strcpy(s_szPacketType, "PACKET_TYPE_TELEMETRY_MSP"); break;
      case PACKET_TYPE_VEHICLE_RECORDING: strcpy(s_szPacketType, "PACKET_TYPE_VEHICLE_RECORDING"); break;
      case PACKET_TYPE_NEGOCIATE_RADIO_LINKS: strcpy(s_szPacketType, "PACKET_TYPE_NEGOCIATE_RADIO_LINKS"); break;       
      // Local packets

      case PACKET_TYPE_LOCAL_CONTROL_PAUSE_RESUME_AUDIO:    strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_PAUSE_RESUME_AUDIO"); break;
      case PACKET_TYPE_LOCAL_CONTROL_PAUSE_VIDEO:           strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_PAUSE_VIDEO"); break;
      case PACKET_TYPE_LOCAL_CONTROL_RESUME_VIDEO:          strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_RESUME_VIDEO"); break;
      case PACKET_TYPE_LOCAL_CONTROL_PAUSE_LOCAL_VIDEO_DISPLAY: strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_PAUSE_LOCAL_VIDEO_DISPLAY"); break;
      case PACKET_TYPE_LOCAL_CONTROL_VIDEO_RECORDING:       strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_VIDEO_RECORDING"); break;
      case PACKET_TYPE_TEST_ADAPTIVE_VIDEO:                 strcpy(s_szPacketType, "PACKET_TYPE_TEST_ADAPTIVE_VIDEO"); break;
      case PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED:         strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED:    strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_FORCE_CAMERA_TYPE:     strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_FORCE_CAMERA_TYPE"); break;
      case PACKET_TYPE_LOCAL_CONTROL_REBOOT:                strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_REBOOT"); break;
      case PACKET_TYPE_LOCAL_CONTROL_UPDATE_STARTED:        strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_UPDATE_STARTED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED:         strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_UPDATE_FINISHED:       strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_UPDATE_FINISHED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_RELAY_MODE_SWITCHED:            strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_RELAY_MODE_SWITCHED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_BROADCAST_RADIO_REINITIALIZED:  strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_BROADCAST_RADIO_REINITIALIZED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_RECEIVED_MODEL_SETTING:         strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_RECEIVED_MODEL_SETTING"); break;
      case PACKET_TYPE_LOCAL_CONTROL_REINITIALIZE_RADIO_LINKS:       strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_REINITIALIZE_RADIO_LINKS"); break;
      case PACKET_TYPE_LOCAL_CONTROL_UPDATED_RADIO_TX_POWERS:        strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_UPDATED_RADIO_TX_POWERS"); break;
      case PACKET_TYPE_LOCAL_CONTROL_RECEIVED_VEHICLE_LOG_SEGMENT:   strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_RECEIVED_VEHICLE_LOG_SEGMENT"); break;
      case PACKET_TYPE_LOCAL_CONTROL_PASSPHRASE_CHANGED:      strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_PASSPHRASE_CHANGED"); break;
      case PACKET_TYPE_LOCAL_CONTROLL_VIDEO_DETECTED_ON_SEARCH: strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROLL_VIDEO_DETECTED_ON_SEARCH"); break;
      case PACKET_TYPE_LOCAL_CONTROL_I2C_DEVICE_CHANGED:      strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_I2C_DEVICE_CHANGED"); break;
      case PACKET_TYPE_LOCAL_CONTROLLER_ROUTER_READY:         strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROLLER_ROUTER_READY"); break;
      case PACKET_TYPE_LOCAL_CONTROLLER_RADIO_INTERFACE_FAILED_TO_INITIALIZE:  strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROLLER_RADIO_INTERFACE_FAILED_TO_INITIALIZE"); break;
      case PACKET_TYPE_LOCAL_CONTROLLER_RELOAD_CORE_PLUGINS:  strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROLLER_RELOAD_CORE_PLUGINS"); break;
      case PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SET_CAMERA_PARAMS: strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SET_CAMERA_PARAMS"); break;
      case PACKET_TYPE_LOCAL_CONTROL_BROADCAST_VEHICLE_STATS: strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_BROADCAST_VEHICLE_STATS"); break;
      case PACKET_TYPE_LOCAL_CONTROLLER_SEARCH_FREQ_CHANGED:  strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROLLER_SEARCH_FREQ_CHANGED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_LINK_FREQUENCY_CHANGED:          strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_LINK_FREQUENCY_CHANGED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_VEHICLE_ROUTER_READY:             strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_VEHICLE_ROUTER_READY"); break;
      case PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SET_SIK_RADIO_SERIAL_SPEED:  strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SET_SIK_RADIO_SERIAL_SPEED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SEND_MODEL_SETTINGS:      strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SEND_MODEL_SETTINGS"); break;
      case PACEKT_TYPE_LOCAL_CONTROLLER_ADAPTIVE_VIDEO_PAUSE: strcpy(s_szPacketType, "PACEKT_TYPE_LOCAL_CONTROLLER_ADAPTIVE_VIDEO_PAUSE"); break;
      case PACKET_TYPE_LOCAL_CONTROL_VEHICLE_APPLY_ALL_VIDEO_SETTINGS: strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_VEHICLE_APPLY_ALL_VIDEO_SETTINGS"); break;
      case PACKET_TYPE_LOCAL_CONTROL_OSD_PLUGINS_NEED_TELEMETRY: strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_OSD_PLUGINS_NEED_TELEMETRY"); break;
      case PACKET_TYPE_LOCAL_CONTROL_LONG_TASK: strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_LONG_TASK"); break;

      case PACKET_TYPE_OTA_UPDATE_STATUS:          strcpy(s_szPacketType, "PACKET_TYPE_OTA_UPDATE_STATUS"); break;
      case PACKET_TYPE_LOCAL_CONTROL_VEHICLE_CALIBRATION_FILE: strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_VEHICLE_CALIBRATION_FILE"); break;
   }
   return s_szPacketType;
}

char* str_get_packet_history_symbol(int iPacketType, int iRepeatCount)
{
   static char s_szOSDRenderRxHistoryPacketSymbol[4];
   s_szOSDRenderRxHistoryPacketSymbol[0] = 'o';
   s_szOSDRenderRxHistoryPacketSymbol[1] = 0;

   if ( iPacketType == 0xFF ) // End of seccond
   {
      s_szOSDRenderRxHistoryPacketSymbol[0] = '*';
      return s_szOSDRenderRxHistoryPacketSymbol;
   }

   if ( iPacketType == 0x00 ) // Missing packets or none
   {
      if ( iRepeatCount > 0 )
      {
         s_szOSDRenderRxHistoryPacketSymbol[0] = 'x';
         return s_szOSDRenderRxHistoryPacketSymbol;
      }
      s_szOSDRenderRxHistoryPacketSymbol[0] = ' ';
      return s_szOSDRenderRxHistoryPacketSymbol;
   }
   if ( iPacketType == PACKET_TYPE_RUBY_PING_CLOCK ||
        iPacketType == PACKET_TYPE_RUBY_PING_CLOCK_REPLY )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'P';

   if ( iPacketType == PACKET_TYPE_COMMAND ||
        iPacketType == PACKET_TYPE_COMMAND_RESPONSE )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'C';

   if ( iPacketType == PACKET_TYPE_AUDIO_SEGMENT )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'A';

   if ( iPacketType == PACKET_TYPE_VIDEO_DATA ||
        iPacketType == PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS )
     s_szOSDRenderRxHistoryPacketSymbol[0] = 'V';

   if ( iPacketType == PACKET_TYPE_AUX_DATA_LINK_UPLOAD ||
        iPacketType == PACKET_TYPE_AUX_DATA_LINK_DOWNLOAD )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'D';

   if ( iPacketType == PACKET_TYPE_RUBY_TELEMETRY_SHORT ||
        iPacketType == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'R';

   if ( iPacketType == PACKET_TYPE_RC_TELEMETRY ||
        iPacketType == PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY ||
        iPacketType == PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS ||
        iPacketType == PACKET_TYPE_RUBY_TELEMETRY_DEV_VIDEO_BITRATE_HISTORY ||
        iPacketType == PACKET_TYPE_RUBY_TELEMETRY_VIDEO_INFO_STATS ||
        iPacketType == PACKET_TYPE_RUBY_TELEMETRY_RADIO_RX_HISTORY )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'r';

   if ( iPacketType == PACKET_TYPE_FC_TELEMETRY ||
        iPacketType == PACKET_TYPE_FC_TELEMETRY_EXTENDED )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'T';
   if ( iPacketType == PACKET_TYPE_FC_RC_CHANNELS )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 't';

   if ( iPacketType == PACKET_TYPE_VIDEO_ADAPTIVE_VIDEO_PARAMS ||
        iPacketType == PACKET_TYPE_VIDEO_ADAPTIVE_VIDEO_PARAMS_ACK )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'S';

   if ( iPacketType == PACKET_TYPE_RUBY_MODEL_SETTINGS )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'M';

   if ( iPacketType == PACKET_TYPE_RUBY_RADIO_CONFIG_UPDATED )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'U';

   if ( iPacketType == PACKET_TYPE_RUBY_ALARM )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'a';

   return s_szOSDRenderRxHistoryPacketSymbol;
}

char* str_get_packet_test_link_command(int iTestCommandId)
{
   static char s_szTestRadioLinkCommandType[32];
   s_szTestRadioLinkCommandType[0] = 0;

   if ( iTestCommandId == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_STATUS )
      strcpy(s_szTestRadioLinkCommandType, "[Status]");
   if ( iTestCommandId == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START )
      strcpy(s_szTestRadioLinkCommandType, "[Start]");
   if ( iTestCommandId == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_PING_UPLINK )
      strcpy(s_szTestRadioLinkCommandType, "[PingUplink]");
   if ( iTestCommandId == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_PING_DOWNLINK )
      strcpy(s_szTestRadioLinkCommandType, "[PingDownlink]");
   if ( iTestCommandId == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_END )
      strcpy(s_szTestRadioLinkCommandType, "[End]");

   if ( iTestCommandId == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_ENDED )
      strcpy(s_szTestRadioLinkCommandType, "[Ended]");

   return s_szTestRadioLinkCommandType;
}

void str_getDataRateDescription(int dataRateBPS, int iHT40, char* szOutput)
{
   if ( NULL == szOutput )
      return;
   szOutput[0] = 0;

   if ( dataRateBPS <= -100 )
   {
      strcpy(szOutput, "Lowest");
   }
   else if ( dataRateBPS < 0 )
   {
      int mcsIndex = -dataRateBPS-1;
      if ( mcsIndex <= MAX_MCS_INDEX )
         sprintf(szOutput, "MCS-%d %u Mb", mcsIndex, getRealDataRateFromMCSRate(mcsIndex, iHT40)/1000/1000 );
      else
         sprintf(szOutput, "MCS-?");
   }
   else if ( 0 == dataRateBPS )
   {
      strcpy(szOutput, "Auto");
   }
   else if ( dataRateBPS <= 56 )
   {
       sprintf(szOutput, "*%d Mbps", dataRateBPS);
   }
   else
   {
      if ( dataRateBPS >= 1000000 )
      {
         if ( ((dataRateBPS /1000) % 1000) != 0 )
            sprintf(szOutput, "%1.f Mbps", (float)dataRateBPS/1000.0/1000.0);
         else
            sprintf(szOutput, "%d Mbps", dataRateBPS/1000/1000);
      }
      else if ( dataRateBPS >= 10000 )
         sprintf(szOutput, "%d kbps", dataRateBPS/1000);
      else
         sprintf(szOutput, "%d bps", dataRateBPS);
   }
}


void str_getDataRateDescriptionNoSufix(int dataRateBPS, char* szOutput)
{
   if ( NULL == szOutput )
      return;
   szOutput[0] = 0;

   if ( dataRateBPS < 0 )
   {
      int mcsIndex = -dataRateBPS-1;
      if ( mcsIndex <= MAX_MCS_INDEX )
         sprintf(szOutput, "MCS-%d", mcsIndex);
      else
         sprintf(szOutput, "MCS-?");
   }
   else if ( 0 == dataRateBPS )
      strcpy(szOutput,"-");
   else if ( dataRateBPS <= 56 )
   {
       sprintf(szOutput, "*%d", dataRateBPS);
   }
   else
   {
      if ( dataRateBPS >= 1000000 )
      {
         if ( ((dataRateBPS /1000) % 1000) != 0 )
            sprintf(szOutput, "%1.f", (float)dataRateBPS/1000.0/1000.0);
         else
            sprintf(szOutput, "%d", dataRateBPS/1000/1000);
      }
      else if ( dataRateBPS >= 10000 )
         sprintf(szOutput, "%d", dataRateBPS/1000);
      else
         sprintf(szOutput, "%d", dataRateBPS);
   }
}

char* str_getDataRateDescriptionAlternative(int dataRateBPS)
{
   static char s_szFormatDatarateAlternate[64];
   s_szFormatDatarateAlternate[0] = 0;
   if ( (dataRateBPS >= 0) || (dataRateBPS <= -100) )
      sprintf(s_szFormatDatarateAlternate, ".n-%s", str_format_datarate_inline(dataRateBPS));
   else
   {
      if ( dataRateBPS == -1 )
         strcpy(s_szFormatDatarateAlternate, "BPSK");
      else if ( (dataRateBPS == -2) || (dataRateBPS == -3) )
         strcpy(s_szFormatDatarateAlternate, "QPSK");
      else if ( (dataRateBPS == -4) || (dataRateBPS == -5) )
         strcpy(s_szFormatDatarateAlternate, "16-QAM");
      else if ( (dataRateBPS == -6) || (dataRateBPS == -7) || (dataRateBPS == -8) )
         strcpy(s_szFormatDatarateAlternate, "64-QAM");
      else
         strcpy(s_szFormatDatarateAlternate, "256-QAM");
   }
   return s_szFormatDatarateAlternate;
}

char* str_format_datarate_inline(int dataRateBPS)
{
   static char s_szFormatDatarateInline[64];
   s_szFormatDatarateInline[0] = 0;

   if ( dataRateBPS <= -100 )
   {
      strcpy(s_szFormatDatarateInline, "Lowest");
   }
   else if ( dataRateBPS < 0 )
   {
      int mcsIndex = -dataRateBPS-1;
      if ( mcsIndex <= MAX_MCS_INDEX )
         sprintf(s_szFormatDatarateInline, "MCS-%d", mcsIndex );
      else
         sprintf(s_szFormatDatarateInline, "MCS-?");
   }
   else if ( 0 == dataRateBPS )
   {
      strcpy(s_szFormatDatarateInline, "Auto");
   }
   else if ( dataRateBPS <= 56 )
   {
       sprintf(s_szFormatDatarateInline, "*%d Mbps", dataRateBPS);
   }
   else
   {
      if ( dataRateBPS >= 1000000 )
      {
         if ( ((dataRateBPS /1000) % 1000) != 0 )
            sprintf(s_szFormatDatarateInline, "%1.f Mbps", (float)dataRateBPS/1000.0/1000.0);
         else
            sprintf(s_szFormatDatarateInline, "%d Mbps", dataRateBPS/1000/1000);
      }
      else if ( dataRateBPS >= 10000 )
         sprintf(s_szFormatDatarateInline, "%d kbps", dataRateBPS/1000);
      else
         sprintf(s_szFormatDatarateInline, "%d bps", dataRateBPS);
   }

   return s_szFormatDatarateInline;   
}

char* str_format_bitrate_inline(int iBitrateBPS)
{
   static char s_szStrFormatBitrate[64];
   str_format_bitrate(iBitrateBPS, s_szStrFormatBitrate);
   return s_szStrFormatBitrate;
}

void str_format_bitrate(int bitrate_bps, char* szBuffer)
{
   if ( NULL == szBuffer )
      return;
   szBuffer[0] = 0;
   if ( bitrate_bps < 0 )
      sprintf(szBuffer, "MCS-%d", -bitrate_bps-1); // starts from MCS-0 (which is -1)
   else if ( bitrate_bps <= 56 )
   {
      if ( bitrate_bps == 0 )
         strcpy(szBuffer, "0 bps");
      else
         sprintf(szBuffer, "*%d Mbps", bitrate_bps);
   }
   else if ( bitrate_bps < 1000 ) // less than 1kb
      sprintf(szBuffer, "%d bps", bitrate_bps);
   else if ( bitrate_bps < 10000 ) // less than 10 kb
      sprintf(szBuffer, "%.1f kbps", (float)bitrate_bps/1000.0);
   else if ( bitrate_bps < 500000 ) // less than 500 kb
      sprintf(szBuffer, "%d kbps", bitrate_bps/1000);
   else
   {
      if ( ((bitrate_bps/1000)%1000) == 0 )
         sprintf(szBuffer, "%d Mbps", bitrate_bps/1000/1000);
      else
         sprintf(szBuffer, "%.1f Mbps", (float)bitrate_bps/1000.0/1000.0);
   }
}

void str_format_bitrate_no_sufix(int bitrate_bps, char* szBuffer)
{
   if ( NULL == szBuffer )
      return;
   if ( bitrate_bps < 0 )
      sprintf(szBuffer, "MCS-%d", -bitrate_bps-1); // starts from MCS-0 (which is -1)
   else if ( bitrate_bps <= 56 )
      sprintf(szBuffer, "*%d", bitrate_bps*1000*1000);
   else if ( bitrate_bps < 1000 ) // less than 1kb
      sprintf(szBuffer, "%d", bitrate_bps);
   else if ( bitrate_bps < 10000 ) // less than 10 kb
      sprintf(szBuffer, "%.1f", (float)bitrate_bps/1000.0);
   else if ( bitrate_bps < 500000 ) // less than 500 kb
      sprintf(szBuffer, "%d", bitrate_bps/1000);
   else
   {
      if ( ((bitrate_bps/1000)%1000) == 0 )
         sprintf(szBuffer, "%d", bitrate_bps/1000/1000);
      else
         sprintf(szBuffer, "%.1f", (float)bitrate_bps/1000.0/1000.0);
   }
}

const char* str_getBandName(u32 band)
{
   if ( band & RADIO_HW_SUPPORTED_BAND_433 )
      return "433 Mhz";
   if ( band & RADIO_HW_SUPPORTED_BAND_868 )
      return "868 Mhz";
   if ( band & RADIO_HW_SUPPORTED_BAND_915 )
      return "915 Mhz";
   if ( band & RADIO_HW_SUPPORTED_BAND_23 )
      return "2.3 Ghz";
   if ( band & RADIO_HW_SUPPORTED_BAND_24 )
      return "2.4 Ghz";
   if ( band & RADIO_HW_SUPPORTED_BAND_25 )
      return "2.5 Ghz";
   if ( band & RADIO_HW_SUPPORTED_BAND_58 )
      return "5.8 Ghz";
   return "";
}

char* str_format_frequency(u32 uFrequencyKhz)
{
   static char s_szFrequencyFormat[64];

   s_szFrequencyFormat[0] = 0;
   
   if ( uFrequencyKhz < 10000 )
      uFrequencyKhz *= 1000;
     
   if ( (uFrequencyKhz%1000) == 0 )
      sprintf(s_szFrequencyFormat, "%u Mhz", uFrequencyKhz/1000);
   else if ( (uFrequencyKhz%100) == 0 )
      sprintf(s_szFrequencyFormat, "%u.%u Mhz", uFrequencyKhz/1000, (uFrequencyKhz%1000) / 100);
   else if ( (uFrequencyKhz%10) == 0 )
      sprintf(s_szFrequencyFormat, "%u.%u Mhz", uFrequencyKhz/1000, (uFrequencyKhz%1000) / 10);
   else
      sprintf(s_szFrequencyFormat, "%u.%u Mhz", uFrequencyKhz/1000, uFrequencyKhz%1000);

   return s_szFrequencyFormat;
}


char* str_format_frequency_no_sufix(u32 uFrequencyKhz)
{
   static char s_szFrequencyFormatNoSufix[64];

   s_szFrequencyFormatNoSufix[0] = 0;
   
   if ( (uFrequencyKhz%1000) == 0 )
      sprintf(s_szFrequencyFormatNoSufix, "%u", uFrequencyKhz/1000);
   else if ( (uFrequencyKhz%100) == 0 )
      sprintf(s_szFrequencyFormatNoSufix, "%u.%u", uFrequencyKhz/1000, (uFrequencyKhz%1000) / 100);
   else if ( (uFrequencyKhz%10) == 0 )
      sprintf(s_szFrequencyFormatNoSufix, "%u.%u", uFrequencyKhz/1000, (uFrequencyKhz%1000) / 10);
   else
      sprintf(s_szFrequencyFormatNoSufix, "%u.%u", uFrequencyKhz/1000, uFrequencyKhz%1000);

   return s_szFrequencyFormatNoSufix;
}


const char* str_get_hardware_board_name(u32 board_type)
{
   static const char* s_szBoardTypeUnknown = "Unknown";

   static const char* s_szBoardTypePi0 = "Raspberry Pi Zero";
   static const char* s_szBoardTypePi0W = "Raspberry Pi Zero W";
   static const char* s_szBoardTypePi02 = "Raspberry Pi Zero 2";
   static const char* s_szBoardTypePi2B = "Raspberry Pi 2B";
   static const char* s_szBoardTypePi2BV1 = "Raspberry Pi 2B v1.1";
   static const char* s_szBoardTypePi2BV12 = "Raspberry Pi 2B v1.2";
   static const char* s_szBoardTypePi3AP = "Raspberry Pi 3A+";
   static const char* s_szBoardTypePi3B = "Raspberry Pi 3B";
   static const char* s_szBoardTypePi3BP = "Raspberry Pi 3B+";
   static const char* s_szBoardTypePi4B = "Raspberry Pi 4B";

   static const char* s_szBoardTypeRadxaZero3 = "Radxa Zero 3";
   static const char* s_szBoardTypeRadxa3C = "Radxa 3C";
   static const char* s_szBoardTypeRadxaRuncamVRx = "RunCam VRx";

   static const char* s_szBoardTypeOpenIPCGoke200 = "OpenIPC Goke200";
   static const char* s_szBoardTypeOpenIPCGoke210 = "OpenIPC Goke210";
   static const char* s_szBoardTypeOpenIPCGoke300 = "OpenIPC Goke300";
   static const char* s_szBoardTypeOpenIPCSigmasterGeneric = "OpenIPC SSC338Q Generic";
   static const char* s_szBoardTypeOpenIPCSigmasterGeneric30KQ = "OpenIPC SSC30KQ Generic";
   static const char* s_szBoardTypeOpenIPCSigmasterUltrasight = "Ultrasight AIO";
   static const char* s_szBoardTypeOpenIPCSigmasterMario = "Mario AIO";
   static const char* s_szBoardTypeOpenIPCSigmasterRuncam = "Runcam V1";
   static const char* s_szBoardTypeOpenIPCSigmasterRuncam2 = "Runcam V2";
   static const char* s_szBoardTypeOpenIPCSigmasterEMax = "EMax";
   static const char* s_szBoardTypeOpenIPCSigmasterEMaxMini = "EMax-Mini";
   static const char* s_szBoardTypeOpenIPCSigmasterThinker = "OpenIPC Thinker (builtin radio)";
   static const char* s_szBoardTypeOpenIPCSigmasterThinkerE = "OpenIPC Thinker (ext radio)";

   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PIZERO )
      return s_szBoardTypePi0;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PIZEROW )
      return s_szBoardTypePi0W;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PIZERO2 )
      return s_szBoardTypePi02;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PI2B )
      return s_szBoardTypePi2B;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PI2BV11 )
      return s_szBoardTypePi2BV1;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PI2BV12 )
      return s_szBoardTypePi2BV12;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PI3APLUS )
      return s_szBoardTypePi3AP;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PI3B )
      return s_szBoardTypePi3B;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PI3BPLUS )
      return s_szBoardTypePi3BP;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PI4B )
      return s_szBoardTypePi4B;

   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_RADXA_ZERO3 )
      return s_szBoardTypeRadxaZero3;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_RADXA_3C )
      return s_szBoardTypeRadxa3C;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_RADXA_RUNCAM_VRX )
      return s_szBoardTypeRadxaRuncamVRx;

   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE200 )
      return s_szBoardTypeOpenIPCGoke200;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE210 )
      return s_szBoardTypeOpenIPCGoke210;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE300 )
      return s_szBoardTypeOpenIPCGoke300;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_SIGMASTAR_338Q )
   {
      switch ( (board_type & BOARD_SUBTYPE_MASK) >> BOARD_SUBTYPE_SHIFT )
      {
         case BOARD_SUBTYPE_OPENIPC_GENERIC: return s_szBoardTypeOpenIPCSigmasterGeneric;
         case BOARD_SUBTYPE_OPENIPC_GENERIC_30KQ: return s_szBoardTypeOpenIPCSigmasterGeneric30KQ;
         case BOARD_SUBTYPE_OPENIPC_AIO_ULTRASIGHT: return s_szBoardTypeOpenIPCSigmasterUltrasight;
         case BOARD_SUBTYPE_OPENIPC_AIO_MARIO: return s_szBoardTypeOpenIPCSigmasterMario;
         case BOARD_SUBTYPE_OPENIPC_AIO_RUNCAM_V1: return s_szBoardTypeOpenIPCSigmasterRuncam;
         case BOARD_SUBTYPE_OPENIPC_AIO_RUNCAM_V2: return s_szBoardTypeOpenIPCSigmasterRuncam2;
         case BOARD_SUBTYPE_OPENIPC_AIO_EMAX: return s_szBoardTypeOpenIPCSigmasterEMax;
         case BOARD_SUBTYPE_OPENIPC_AIO_EMAX_MINI: return s_szBoardTypeOpenIPCSigmasterEMaxMini;
         case BOARD_SUBTYPE_OPENIPC_AIO_THINKER: return s_szBoardTypeOpenIPCSigmasterThinker;
         case BOARD_SUBTYPE_OPENIPC_AIO_THINKER_E: return s_szBoardTypeOpenIPCSigmasterThinkerE;
         default: return s_szBoardTypeOpenIPCSigmasterGeneric;
      }
   }
   return s_szBoardTypeUnknown;
}

const char* str_get_hardware_board_name_short(u32 board_type)
{
   static const char* s_szBoardSTypeUnknown = "Unknown";

   static const char* s_szBoardSTypePi0 = "Pi 0";
   static const char* s_szBoardSTypePi0W = "Pi 0W";
   static const char* s_szBoardSTypePi02 = "Pi 02";
   static const char* s_szBoardSTypePi2B = "Pi 2B";
   static const char* s_szBoardSTypePi2BV1 = "Pi 2B-v1.1";
   static const char* s_szBoardSTypePi2BV12 = "Pi 2B-v1.2";
   static const char* s_szBoardSTypePi3AP = "Pi 3A+";
   static const char* s_szBoardSTypePi3B = "Pi 3B";
   static const char* s_szBoardSTypePi3BP = "Pi 3B+";
   static const char* s_szBoardSTypePi4B = "Pi 4B";

   static const char* s_szBoardSTypeRadxaZero3 = "Radxa Zero3";
   static const char* s_szBoardSTypeRadxa3C = "Radxa 3C";

   static const char* s_szBoardSTypeOpenIPCGoke200 = "Goke200";
   static const char* s_szBoardSTypeOpenIPCGoke210 = "Goke210";
   static const char* s_szBoardSTypeOpenIPCGoke300 = "Goke300";
   static const char* s_szBoardSTypeOpenIPCSigmasterGeneric = "SSC338Q";
   static const char* s_szBoardSTypeOpenIPCSigmasterGeneric30KQ = "SSC30KQ";
   static const char* s_szBoardSTypeOpenIPCSigmasterUltrasight = "Ultrasight";
   static const char* s_szBoardSTypeOpenIPCSigmasterMario = "Mario";
   static const char* s_szBoardSTypeOpenIPCSigmasterRuncam = "Runcam V1";
   static const char* s_szBoardSTypeOpenIPCSigmasterRuncam2 = "Runcam V2";
   static const char* s_szBoardSTypeOpenIPCSigmasterEMax = "EMax";
   static const char* s_szBoardSTypeOpenIPCSigmasterEMaxMini = "EMax-Mini";
   static const char* s_szBoardSTypeOpenIPCSigmasterThinker = "Thinker";
   static const char* s_szBoardSTypeOpenIPCSigmasterThinkerE = "Thinker(E)";

   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PIZERO )
      return s_szBoardSTypePi0;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PIZEROW )
      return s_szBoardSTypePi0W;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PIZERO2 )
      return s_szBoardSTypePi02;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PI2B )
      return s_szBoardSTypePi2B;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PI2BV11 )
      return s_szBoardSTypePi2BV1;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PI2BV12 )
      return s_szBoardSTypePi2BV12;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PI3APLUS )
      return s_szBoardSTypePi3AP;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PI3B )
      return s_szBoardSTypePi3B;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PI3BPLUS )
      return s_szBoardSTypePi3BP;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PI4B )
      return s_szBoardSTypePi4B;

   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_RADXA_ZERO3 )
      return s_szBoardSTypeRadxaZero3;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_RADXA_3C )
      return s_szBoardSTypeRadxa3C;

   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE200 )
      return s_szBoardSTypeOpenIPCGoke200;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE210 )
      return s_szBoardSTypeOpenIPCGoke210;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE300 )
      return s_szBoardSTypeOpenIPCGoke300;
   if ( (board_type & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_SIGMASTAR_338Q )
   {
      switch ( (board_type & BOARD_SUBTYPE_MASK) >> BOARD_SUBTYPE_SHIFT)
      {
         case BOARD_SUBTYPE_OPENIPC_GENERIC: return s_szBoardSTypeOpenIPCSigmasterGeneric;
         case BOARD_SUBTYPE_OPENIPC_GENERIC_30KQ: return s_szBoardSTypeOpenIPCSigmasterGeneric30KQ;
         case BOARD_SUBTYPE_OPENIPC_AIO_ULTRASIGHT: return s_szBoardSTypeOpenIPCSigmasterUltrasight;
         case BOARD_SUBTYPE_OPENIPC_AIO_MARIO: return s_szBoardSTypeOpenIPCSigmasterMario;
         case BOARD_SUBTYPE_OPENIPC_AIO_RUNCAM_V1: return s_szBoardSTypeOpenIPCSigmasterRuncam;
         case BOARD_SUBTYPE_OPENIPC_AIO_RUNCAM_V2: return s_szBoardSTypeOpenIPCSigmasterRuncam2;
         case BOARD_SUBTYPE_OPENIPC_AIO_EMAX: return s_szBoardSTypeOpenIPCSigmasterEMax;
         case BOARD_SUBTYPE_OPENIPC_AIO_EMAX_MINI: return s_szBoardSTypeOpenIPCSigmasterEMaxMini;
         case BOARD_SUBTYPE_OPENIPC_AIO_THINKER: return s_szBoardSTypeOpenIPCSigmasterThinker;
         case BOARD_SUBTYPE_OPENIPC_AIO_THINKER_E: return s_szBoardSTypeOpenIPCSigmasterThinkerE;
         default: return s_szBoardSTypeOpenIPCSigmasterGeneric;
      }
   }

   return s_szBoardSTypeUnknown;
}

const char* str_get_hardware_wifi_name(u32 wifi_type)
{
   static const char* s_szWiFiTypeUnknown = "N/A";
   static const char* s_szWiFiTypeA = "A";
   static const char* s_szWiFiTypeG = "G";
   static const char* s_szWiFiTypeAG = "AG";

   if ( wifi_type == WIFI_TYPE_A )
      return s_szWiFiTypeA;
   if ( wifi_type == WIFI_TYPE_G )
      return s_szWiFiTypeG;
   if ( wifi_type == WIFI_TYPE_AG )
      return s_szWiFiTypeAG;

   return s_szWiFiTypeUnknown;
}

const char* str_get_hardware_camera_type_string(u32 uCamType)
{
   static char s_szStrCameraTypeName[128];
   s_szStrCameraTypeName[0] = 0;
   str_get_hardware_camera_type_string_to_string(uCamType, s_szStrCameraTypeName);
   return s_szStrCameraTypeName;
}

void str_get_hardware_camera_type_string_to_string(u32 uCamType, char* szOutput)
{
   if ( NULL == szOutput )
      return;
   szOutput[0] = 0;

   if ( uCamType == 0 )
   {
      strcpy(szOutput, "None");
      return;
   }
   
   strcpy(szOutput, "Unknown");

   if ( uCamType == CAMERA_TYPE_CSI )
      strcpy(szOutput, "Standard CSI");
   else if ( uCamType == CAMERA_TYPE_VEYE290 )
      strcpy(szOutput, "VEYE/IMX290");
   else if ( uCamType == CAMERA_TYPE_VEYE307 )
      strcpy(szOutput, "VEYE/IMX307");
   else if ( uCamType == CAMERA_TYPE_VEYE327 )
      strcpy(szOutput, "VEYE/IMX327");
   else if ( uCamType == CAMERA_TYPE_HDMI )
      strcpy(szOutput, "HDMI CSI");
   else if ( uCamType == CAMERA_TYPE_IP )
      strcpy(szOutput, "IP");

   if ( uCamType == CAMERA_TYPE_OPENIPC_IMX307 )
      strcpy(szOutput, "Sony IMX307");
   else if ( uCamType == CAMERA_TYPE_OPENIPC_IMX335 )
      strcpy(szOutput, "Sony IMX335");
   else if ( uCamType == CAMERA_TYPE_OPENIPC_IMX415 )
      strcpy(szOutput, "Sony IMX415");
}

void str_get_supported_bands_string(u32 bands, char* szOut)
{
   if ( NULL == szOut )
      return;

   int iCount = 0;
   
   szOut[0] = 0;
   if ( bands & RADIO_HW_SUPPORTED_BAND_433 )
      strcat(szOut, "433");

   if ( bands & RADIO_HW_SUPPORTED_BAND_868 )
   {
      if ( 0 != szOut[0] )
         strcat(szOut, ", ");
      strcat(szOut, "868");
      iCount++;
   }

   if ( bands & RADIO_HW_SUPPORTED_BAND_915 )
   {
      if ( 0 != szOut[0] )
         strcat(szOut, ", ");
      strcat(szOut, "915");
      iCount++;
   }

   if ( 0 != szOut[0] )
      strcat(szOut, " Mhz");

   int hasGhz = 0;
   if ( bands & RADIO_HW_SUPPORTED_BAND_23 )
   {
      if ( 0 != szOut[0] )
         strcat(szOut, ", ");
      strcat(szOut, "2.3");
      hasGhz = 1;
      iCount++;
   }
   if ( bands & RADIO_HW_SUPPORTED_BAND_24 )
   {
      if ( 0 != szOut[0] )
         strcat(szOut, ", ");
      strcat(szOut, "2.4");
      hasGhz = 1;
      iCount++;
   }
   if ( bands & RADIO_HW_SUPPORTED_BAND_25 )
   {
      if ( 0 != szOut[0] )
         strcat(szOut, ", ");
      strcat(szOut, "2.5");
      hasGhz = 1;
      iCount++;
   }
   if ( bands & RADIO_HW_SUPPORTED_BAND_58 )
   {
      if ( 0 != szOut[0] )
         strcat(szOut, ", ");
      strcat(szOut, "5.8");
      hasGhz = 1;
      iCount++;
   }

   if ( (0 == szOut[0]) || (0 == iCount) )
   {
      strcpy(szOut, "N/A");
      return;
   }
   if ( hasGhz )
      strcat(szOut, " Ghz");

   if ( iCount > 1 )
      strcat(szOut, " bands");
   else
      strcat(szOut, " band");
}


const char* str_get_radio_type_description(int iRadioType)
{
   static char sszNICTypeDescription[32];
   strcpy(sszNICTypeDescription, "N/A");
   if ( iRadioType == RADIO_TYPE_RALINK )
      strcpy(sszNICTypeDescription, "Ralink");
   if ( iRadioType == RADIO_TYPE_ATHEROS )
      strcpy(sszNICTypeDescription, "Atheros");
   if ( iRadioType == RADIO_TYPE_REALTEK )
      strcpy(sszNICTypeDescription, "Realtek");
   if ( iRadioType == RADIO_TYPE_MEDIATEK )
      strcpy(sszNICTypeDescription, "Mediatek");
   if ( iRadioType == RADIO_TYPE_SIK )
      strcpy(sszNICTypeDescription, "SiK-Radio");
   if ( iRadioType == RADIO_TYPE_SERIAL )
      strcpy(sszNICTypeDescription, "Serial-Radio");
   return sszNICTypeDescription;
}

const char* str_get_radio_driver_description(int iDriverType)
{
   static char sszNICDriverDescription[32];
   strcpy(sszNICDriverDescription, "N/A");
  
   if ( iDriverType == RADIO_HW_DRIVER_ATHEROS )
      strcpy(sszNICDriverDescription, "ath9k_htc");
   if ( iDriverType == RADIO_HW_DRIVER_RALINK )
      strcpy(sszNICDriverDescription, "rt2800usb");
   if ( iDriverType == RADIO_HW_DRIVER_MEDIATEK )
      strcpy(sszNICDriverDescription, "mt7601u");
   if ( iDriverType == RADIO_HW_DRIVER_REALTEK_RTL88XXAU )
      strcpy(sszNICDriverDescription, "rtl88xxau");
   if ( iDriverType == RADIO_HW_DRIVER_REALTEK_RTL8812AU )
      strcpy(sszNICDriverDescription, "rtl8812au");
   if ( iDriverType == RADIO_HW_DRIVER_REALTEK_8812AU )
      strcpy(sszNICDriverDescription, "8812au");
   if ( iDriverType == RADIO_HW_DRIVER_REALTEK_8812EU )
      strcpy(sszNICDriverDescription, "8812eu");
   if ( iDriverType == RADIO_HW_DRIVER_REALTEK_8733BU )
      strcpy(sszNICDriverDescription, "8733bu");
   if ( iDriverType == RADIO_HW_DRIVER_SERIAL_SIK )
      strcpy(sszNICDriverDescription, "SiK");
   if ( iDriverType == RADIO_HW_DRIVER_SERIAL )
      strcpy(sszNICDriverDescription, "Serial");
   return sszNICDriverDescription;
}

const char* str_get_radio_card_model_string(int cardModel)
{
   if ( cardModel < 0 )
      cardModel = -cardModel;
   static char s_szCardModelDescription[64];
   s_szCardModelDescription[0] = 0;
   strcpy(s_szCardModelDescription, "Generic");
   if ( cardModel == CARD_MODEL_TPLINK722N )        strcpy(s_szCardModelDescription, "TPLink 722N");
   if ( cardModel == CARD_MODEL_ATHEROS_GENERIC )   strcpy(s_szCardModelDescription, "Atheros Generic");
   if ( cardModel == CARD_MODEL_RTL8812AU_GENERIC ) strcpy(s_szCardModelDescription, "RTL8812AU Generic");
   if ( cardModel == CARD_MODEL_ALFA_AWUS036NHA )   strcpy(s_szCardModelDescription, "Alfa AWUS036NHA");
   if ( cardModel == CARD_MODEL_ALFA_AWUS036NH )    strcpy(s_szCardModelDescription, "Alfa AWUS036NH");
   if ( cardModel == CARD_MODEL_ALFA_AWUS036ACH )   strcpy(s_szCardModelDescription, "Alfa AWUS036ACH");
   if ( cardModel == CARD_MODEL_ALFA_AWUS036ACS )   strcpy(s_szCardModelDescription, "Alfa AWUS036ACS");
   if ( cardModel == CARD_MODEL_ASUS_AC56 )         strcpy(s_szCardModelDescription, "ASUS AC56");
   if ( cardModel == CARD_MODEL_BLUE_STICK )        strcpy(s_szCardModelDescription, "Blue Stick");
   if ( cardModel == CARD_MODEL_RTL8812AU_DUAL_ANTENNA ) strcpy(s_szCardModelDescription, "RTL8812AU DualAnt");
   if ( cardModel == CARD_MODEL_NETGEAR_A6100 )     strcpy(s_szCardModelDescription, "Netgear A6100");
   if ( cardModel == CARD_MODEL_TENDA_U12 )         strcpy(s_szCardModelDescription, "Tenda U12");
   // 10
   if ( cardModel == CARD_MODEL_RTL8812AU_AF1 )     strcpy(s_szCardModelDescription, "RTL8812AU-AF1");
   if ( cardModel == CARD_MODEL_ZIPRAY )            strcpy(s_szCardModelDescription, "Zipray 1W");
   if ( cardModel == CARD_MODEL_ARCHER_T2UPLUS )    strcpy(s_szCardModelDescription, "Archer T2U Plus");
   if ( cardModel == CARD_MODEL_RTL8814AU )         strcpy(s_szCardModelDescription, "RTL8814AU");
   if ( cardModel == CARD_MODEL_BLUE_8812EU )       strcpy(s_szCardModelDescription, "Blue RTL8812EU");
   if ( cardModel == CARD_MODEL_RTL8812AU_OIPC_USIGHT ) strcpy(s_szCardModelDescription, "RTL8812AU Ultrasight");
   if ( cardModel == CARD_MODEL_RTL8812AU_OIPC_USIGHT2 ) strcpy(s_szCardModelDescription, "RTL8812AU Ultrasight 2");
   if ( cardModel == CARD_MODEL_RTL8733BU )         strcpy(s_szCardModelDescription, "RTL8733BU");
   if ( cardModel == CARD_MODEL_BONNET_LOW_POWER )  strcpy(s_szCardModelDescription, "Bonnet L");
   if ( cardModel == CARD_MODEL_BONNET_HIGH_POWER ) strcpy(s_szCardModelDescription, "Bonnet H");
   
   if ( cardModel == CARD_MODEL_SIK_RADIO )         strcpy(s_szCardModelDescription, "SiK-Radio");
   if ( cardModel == CARD_MODEL_SERIAL_RADIO )      strcpy(s_szCardModelDescription, "Serial-Radio");
   if ( cardModel == CARD_MODEL_SERIAL_RADIO_ELRS ) strcpy(s_szCardModelDescription, "ELRS-Radio");

   return s_szCardModelDescription;
}

const char* str_get_radio_card_model_string_short(int cardModel)
{
   if ( cardModel < 0 )
      cardModel = -cardModel;
   static char s_szCardModelDescription[64];
   s_szCardModelDescription[0] = 0;
   strcpy(s_szCardModelDescription, "Generic");
   if ( cardModel == CARD_MODEL_TPLINK722N )        strcpy(s_szCardModelDescription, "TPL-722N");
   if ( cardModel == CARD_MODEL_ATHEROS_GENERIC )   strcpy(s_szCardModelDescription, "Atheros");
   if ( cardModel == CARD_MODEL_RTL8812AU_GENERIC ) strcpy(s_szCardModelDescription, "RTL8812AU");
   if ( cardModel == CARD_MODEL_ALFA_AWUS036NHA )   strcpy(s_szCardModelDescription, "AWUS036NHA");
   if ( cardModel == CARD_MODEL_ALFA_AWUS036NH )    strcpy(s_szCardModelDescription, "AWUS036NH");
   if ( cardModel == CARD_MODEL_ALFA_AWUS036ACH )   strcpy(s_szCardModelDescription, "AWUS036ACH");
   if ( cardModel == CARD_MODEL_ALFA_AWUS036ACS )   strcpy(s_szCardModelDescription, "AWUS036ACS");
   if ( cardModel == CARD_MODEL_ASUS_AC56 )         strcpy(s_szCardModelDescription, "AC56");
   if ( cardModel == CARD_MODEL_BLUE_STICK )        strcpy(s_szCardModelDescription, "BlueStick");
   if ( cardModel == CARD_MODEL_RTL8812AU_DUAL_ANTENNA ) strcpy(s_szCardModelDescription, "DualAnt");
   if ( cardModel == CARD_MODEL_NETGEAR_A6100 )     strcpy(s_szCardModelDescription, "A6100");
   if ( cardModel == CARD_MODEL_TENDA_U12 )         strcpy(s_szCardModelDescription, "T-U12");
   // 10
   if ( cardModel == CARD_MODEL_RTL8812AU_AF1 )     strcpy(s_szCardModelDescription, "RTL881AU-AF1");
   if ( cardModel == CARD_MODEL_ZIPRAY )            strcpy(s_szCardModelDescription, "Zipray-1W");
   if ( cardModel == CARD_MODEL_ARCHER_T2UPLUS )    strcpy(s_szCardModelDescription, "T2U+");
   if ( cardModel == CARD_MODEL_RTL8814AU )         strcpy(s_szCardModelDescription, "RTL8814AU");
   if ( cardModel == CARD_MODEL_BLUE_8812EU )       strcpy(s_szCardModelDescription, "RTL8812EU");
   if ( cardModel == CARD_MODEL_RTL8812AU_OIPC_USIGHT ) strcpy(s_szCardModelDescription, "RTL8812AU USight");
   if ( cardModel == CARD_MODEL_RTL8812AU_OIPC_USIGHT2 ) strcpy(s_szCardModelDescription, "RTL8812AU USight2");
   if ( cardModel == CARD_MODEL_RTL8733BU )         strcpy(s_szCardModelDescription, "RTL8733BU");
   if ( cardModel == CARD_MODEL_BONNET_LOW_POWER )  strcpy(s_szCardModelDescription, "Bonnet-L");
   if ( cardModel == CARD_MODEL_BONNET_HIGH_POWER ) strcpy(s_szCardModelDescription, "Bonnet-H");

   if ( cardModel == CARD_MODEL_SIK_RADIO )         strcpy(s_szCardModelDescription, "SiK-Radio");
   if ( cardModel == CARD_MODEL_SERIAL_RADIO )      strcpy(s_szCardModelDescription, "Serial-Radio");
   if ( cardModel == CARD_MODEL_SERIAL_RADIO_ELRS ) strcpy(s_szCardModelDescription, "ELRS");

   return s_szCardModelDescription;
}

void str_get_radio_capabilities_description(u32 flags, char* szOutput)
{
   if ( NULL == szOutput )
      return;
   szOutput[0] = 0;
   
   if ( flags & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      strcat(szOutput, "[DISABLED] ");
   if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX) && (flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
      strcat(szOutput, "[CAN TX/RX] ");
   else if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      strcat(szOutput, "[CAN RX] ");
   else if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      strcat(szOutput, "[CAN TX] ");
   else
      strcat(szOutput, "![CAN'T RX OR TX]! ");

   if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
      strcat(szOutput, "[DATA]");

   if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
      strcat(szOutput, "[VIDEO]");

   if ( flags & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY )
      strcat(szOutput, "[HIGH CAPACITY]");
   if ( flags & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      strcat(szOutput, "[RELAY]");
}

char* str_get_radio_frame_flags_description2(u32 frameFlags)
{
   static char s_szRadioFrameFlagsDescription[256];
   s_szRadioFrameFlagsDescription[0] = 0;
   str_get_radio_frame_flags_description(frameFlags, s_szRadioFrameFlagsDescription);   
   return s_szRadioFrameFlagsDescription;
}

void str_get_radio_frame_flags_description(u32 frameFlags, char* szOutput)
{
   if ( NULL == szOutput )
      return;
   if ( (frameFlags & RADIO_FLAGS_USE_LEGACY_DATARATES) && (frameFlags & RADIO_FLAGS_USE_MCS_DATARATES) )
      strcpy(szOutput, "[MIXED rates]");
   else if ( frameFlags & RADIO_FLAGS_USE_LEGACY_DATARATES )
      strcpy(szOutput, "[LEGACY rates]");
   else if ( frameFlags & RADIO_FLAGS_USE_MCS_DATARATES )
      strcpy(szOutput, "[MCS rates]");
   else
      strcpy(szOutput, "[Unknown rates]");

   if ( frameFlags & RADIO_FLAGS_FRAME_TYPE_DATA )
      strcat(szOutput, " [Fr Type: DATA]");
   else
      strcat(szOutput, " [Frames Type: UNKNOWN]");

   if ( frameFlags & RADIO_FLAGS_SIK_ECC )
      strcat(szOutput, " [SIK_ECC]");
   if ( frameFlags & RADIO_FLAGS_SIK_LBT )
      strcat(szOutput, " [SIK_LBT]");
   if ( frameFlags & RADIO_FLAGS_SIK_MCSTR )
      strcat(szOutput, " [SIK_MCSTR]");

   if ( (frameFlags & RADIO_FLAG_HT40_VEHICLE) && (frameFlags & RADIO_FLAG_HT40_CONTROLLER) )
      strcat(szOutput, " [HT40 V/C]");
   else if ( frameFlags & RADIO_FLAG_HT40_VEHICLE )
      strcat(szOutput, " [HT40 V]");
   else if ( frameFlags & RADIO_FLAG_HT40_CONTROLLER )
      strcat(szOutput, " [HT40 C]");
   else
      strcat(szOutput, " [HT20 V/C]");

   if ( (frameFlags & RADIO_FLAG_SGI_VEHICLE) && (frameFlags & RADIO_FLAG_SGI_CONTROLLER) )
      strcat(szOutput, " [SGI V/C]");
   else if ( frameFlags & RADIO_FLAG_SGI_VEHICLE )
      strcat(szOutput, " [SGI V]");
   else if ( frameFlags & RADIO_FLAG_SGI_CONTROLLER )
      strcat(szOutput, " [SGI C]");

   if ( (frameFlags & RADIO_FLAG_STBC_VEHICLE) && (frameFlags & RADIO_FLAG_STBC_CONTROLLER) )
      strcat(szOutput, " [STBC V/C]");
   else if ( frameFlags & RADIO_FLAG_STBC_VEHICLE )
      strcat(szOutput, " [STBC V]");
   else if ( frameFlags & RADIO_FLAG_STBC_CONTROLLER )
      strcat(szOutput, " [STBC C]");
   
   if ( (frameFlags & RADIO_FLAG_LDPC_VEHICLE) && (frameFlags & RADIO_FLAG_LDPC_CONTROLLER) )
      strcat(szOutput, " [LDPC V/C]");
   else if ( frameFlags & RADIO_FLAG_LDPC_VEHICLE )
      strcat(szOutput, " [LDPC V]");
   else if ( frameFlags & RADIO_FLAG_LDPC_CONTROLLER )
      strcat(szOutput, " [LDPC C]");
}

char* str_format_adaptive_video_flags(u8 uFlags)
{
   static char s_szAdaptiveVideoFlagsString[128];

   s_szAdaptiveVideoFlagsString[0] = 0;

   if ( uFlags & FLAG_ADAPTIVE_VIDEO_BITRATE )
      strcat(s_szAdaptiveVideoFlagsString, "FLAG_BITRATE ");
   if ( uFlags & FLAG_ADAPTIVE_VIDEO_EC )
      strcat(s_szAdaptiveVideoFlagsString, "FLAG_EC ");
   if ( uFlags & FLAG_ADAPTIVE_VIDEO_DATARATE )
      strcat(s_szAdaptiveVideoFlagsString, "FLAG_DATARATE ");
   if ( uFlags & FLAG_ADAPTIVE_VIDEO_KEYFRAME )
      strcat(s_szAdaptiveVideoFlagsString, "FLAG_KEYFRAME ");
   if ( uFlags & FLAG_ADAPTIVE_VIDEO_DR_BOOST )
      strcat(s_szAdaptiveVideoFlagsString, "DR_BOOST ");
   if ( uFlags & FLAG_ADAPTIVE_IN_TEST_MODE )
      strcat(s_szAdaptiveVideoFlagsString, "FLAG_TEST ");

   if ( 0 == s_szAdaptiveVideoFlagsString[0] )
      strcpy(s_szAdaptiveVideoFlagsString, "None");
   return s_szAdaptiveVideoFlagsString;
}

char* str_format_video_profile_flags(u32 uVideoProfileFlags)
{
   static char s_szVideoProfileFlagsString[256];

   s_szVideoProfileFlagsString[0] = 0;
   if ( (uVideoProfileFlags & VIDEO_PROFILE_FLAGS_MASK_NOISE) == 0 )
      strcpy(s_szVideoProfileFlagsString, "NOISE_0");
   if ( (uVideoProfileFlags & VIDEO_PROFILE_FLAGS_MASK_NOISE) == 1 )
      strcpy(s_szVideoProfileFlagsString, "NOISE_1");
   if ( (uVideoProfileFlags & VIDEO_PROFILE_FLAGS_MASK_NOISE) == 2 )
      strcpy(s_szVideoProfileFlagsString, "NOISE_2");
   if ( (uVideoProfileFlags & VIDEO_PROFILE_FLAGS_MASK_NOISE) == 3 )
      strcpy(s_szVideoProfileFlagsString, "NOISE_NONE");

   if ( uVideoProfileFlags & VIDEO_PROFILE_FLAG_USE_HIGHER_DATARATE )
   {
      strcat(s_szVideoProfileFlagsString, " USE_HIGHER_RATE");
      int iRate = (uVideoProfileFlags & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK) >> VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT;
      char szTmp[32];
      sprintf(szTmp, "-%d", iRate);
      strcat(s_szVideoProfileFlagsString, szTmp);
   }

   if ( uVideoProfileFlags & VIDEO_PROFILE_FLAG_USE_LOWER_DR_FOR_EC_PACKETS )
      strcat(s_szVideoProfileFlagsString, " USE_LOWER_DR_FOR_EC");
   if ( uVideoProfileFlags & VIDEO_PROFILE_FLAG_USE_LOWER_DR_FOR_RETR_PACKETS )
      strcat(s_szVideoProfileFlagsString, " USE_LOWER_DR_FOR_RETR");
   return s_szVideoProfileFlagsString;
}

char* str_format_video_encoding_flags(u32 uVideoProfileEncodingFlags)
{
   static char sl_szVideoEncodingFlagsString[256];
   sl_szVideoEncodingFlagsString[0] = 0;

   if ( uVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS )
      strcat(sl_szVideoEncodingFlagsString, " RETRANSMISSIONS_ENABLED");
   if ( uVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME )
      strcat(sl_szVideoEncodingFlagsString, " ENABLE_ADAPTIVE_VIDEO_KEYFRAME");
   if ( uVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK )
      strcat(sl_szVideoEncodingFlagsString, " ADAPTIVE_VIDEO");
   if ( uVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO )
      strcat(sl_szVideoEncodingFlagsString, " ADAPTIVE_USE_CONTROLLER_INFO_TOO");
   if ( uVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST )
      strcat(sl_szVideoEncodingFlagsString, " ADAPTIVE_GO_LOWER_ON_LINK_LOST");
   if ( uVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO )
      strcat(sl_szVideoEncodingFlagsString, " ADAPTIVE_USE_MEDIUM_STRENGTH");
   if ( uVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION )
      strcat(sl_szVideoEncodingFlagsString, " AUTO_H264_QUANTISATION_ENABLED");
   if ( uVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH_HIGH )
      strcat(sl_szVideoEncodingFlagsString, " AUTO_QUANTISATION_HIGH");
   if ( uVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_AUTO_EC_SCHEME )
      strcat(sl_szVideoEncodingFlagsString, " AUTO_EC_SCHEME");
   if ( uVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ONE_WAY_FIXED_VIDEO )
      strcat(sl_szVideoEncodingFlagsString, " ONE_WAY");

   //if ( 0 != (uVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK) )
   {
      char szTmp[64];
      sprintf(szTmp, " MAX_RETRANSMISSION_WINDOW=%d", ((uVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK) >> 8) * 5);
      strcat(sl_szVideoEncodingFlagsString, szTmp);
   }

   //if ( 0 != (uVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT) )
   {
      char szTmp2[64];
      sprintf(szTmp2, " RETRANSMISSION_DUPLICATION_PERCENT=%d", ((uVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT) >> 16));
      strcat(sl_szVideoEncodingFlagsString, szTmp2);
   }

   u32 uECSpreadHigh = (uVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_HIGHBIT)?1:0;
   u32 uECSpreadLow = (uVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_LOWBIT)?1:0;
   char szBuff[16];
   sprintf(szBuff, " EC_SPREAD: %u", (uECSpreadHigh<<1) | uECSpreadLow);
   strcat(sl_szVideoEncodingFlagsString, szBuff);

   return sl_szVideoEncodingFlagsString;
}


char* str_get_video_profile_name(u32 videoProfileId)
{
   static char s_szProfileName[32];

   strcpy(s_szProfileName, "NA");
   if ( videoProfileId == VIDEO_PROFILE_HIGH_PERF )
      strcpy(s_szProfileName,"HP");
   else if ( videoProfileId == VIDEO_PROFILE_HIGH_QUALITY )
      strcpy(s_szProfileName,"HQ");
   else if ( videoProfileId == VIDEO_PROFILE_LONG_RANGE )
      strcpy(s_szProfileName,"LR");
   else if ( videoProfileId == VIDEO_PROFILE_USER )
      strcpy(s_szProfileName,"USR");
   else if ( videoProfileId == VIDEO_PROFILE_CUST )
      strcpy(s_szProfileName,"CUST");
   else if ( videoProfileId == VIDEO_PROFILE_PIP )
      strcpy(s_szProfileName,"PIP");
   else
      strcpy(s_szProfileName,"N/A");

   return s_szProfileName;
}

char* str_get_decode_h264_profile_name(u8 uH264Profile, u8 uH264ProfileConstrains, u8 uH264Level)
{
   static char s_szH264DecodedProfileName[64];
   s_szH264DecodedProfileName[0] = 0;

   if ( uH264Profile == 0x42 ) //Baseline
   {
      if ( uH264ProfileConstrains & 0b01000000 )
         strcpy(s_szH264DecodedProfileName, "Constrained Baseline");
      else
         strcpy(s_szH264DecodedProfileName, "Baseline");
   }
   if ( uH264Profile == 0x4D ) // Main
   {
      if ( uH264ProfileConstrains & 0b10000000 )
         strcpy(s_szH264DecodedProfileName, "Constrained Main");
      else
         strcpy(s_szH264DecodedProfileName, "Main");
   }
   if ( uH264Profile == 0x53 ) // Scalable baseline
   {
      strcpy(s_szH264DecodedProfileName, "Scalable Baseline");
   }
   if ( uH264Profile == 0x56 ) // Scalable high
   {
      strcpy(s_szH264DecodedProfileName, "Scalable High");
   }

   if ( uH264Profile == 0x58 ) // Extended
   {
      if ( uH264ProfileConstrains & 0b11000000 )
         strcpy(s_szH264DecodedProfileName, "Constrained Extended");
      else
         strcpy(s_szH264DecodedProfileName, "Extended");
   }
   if ( uH264Profile == 0x64 ) // High
   {
      strcpy(s_szH264DecodedProfileName, "High");
   }
   if ( uH264Profile == 0x6E ) // High10
   {
      if ( uH264ProfileConstrains & 0b00010000 )
         strcpy(s_szH264DecodedProfileName, "High10Intra");
      else
         strcpy(s_szH264DecodedProfileName, "High10");
   }
   if ( uH264Profile == 0x7A ) // High422
   {
      if ( uH264ProfileConstrains & 0b00010000 )
         strcpy(s_szH264DecodedProfileName, "High422Intra");
      else
         strcpy(s_szH264DecodedProfileName, "High422");
   }
   if ( uH264Profile == 0xF4 ) // High444
   {
      if ( uH264ProfileConstrains & 0b00010000 )
         strcpy(s_szH264DecodedProfileName, "High444Intra");
      else
         strcpy(s_szH264DecodedProfileName, "High444");
   }
   
   if ( uH264Profile == 0x2C )
   {
      strcpy(s_szH264DecodedProfileName, "CAVL444Intra");
   }

   return s_szH264DecodedProfileName;
}

char* str_get_radio_stream_name(int iStreamId)
{
   static char s_szStreamName[32];

   strcpy(s_szStreamName, "N/A");
   if ( iStreamId == STREAM_ID_DATA )
      strcpy(s_szStreamName, "Data Stream 1");
   else if ( iStreamId == STREAM_ID_TELEMETRY )
      strcpy(s_szStreamName, "Telemetry Stream");
   else if ( iStreamId == STREAM_ID_AUDIO )
      strcpy(s_szStreamName, "Audio Stream");
   else if ( iStreamId == STREAM_ID_DATA2 )
      strcpy(s_szStreamName, "Data Stream 2");
   else
      sprintf(s_szStreamName, "Video Stream %d", iStreamId-((int)STREAM_ID_VIDEO_1)+1);

   return s_szStreamName;
}

char* str_get_osd_screen_name(int iOSDId)
{
   static char s_szOSDScreenName[64];
   s_szOSDScreenName[0] = 0;

   if ( 0 == iOSDId )
      strcpy(s_szOSDScreenName, "Screen 1");
   if ( 1 == iOSDId )
      strcpy(s_szOSDScreenName, "Screen 2");
   if ( 2 == iOSDId )
      strcpy(s_szOSDScreenName, "Screen 3");
   if ( 3 == iOSDId )
      strcpy(s_szOSDScreenName, "Screen Lean");
   if ( 4 == iOSDId )
      strcpy(s_szOSDScreenName, "Screen Lean Extended");

   return s_szOSDScreenName;
}

char* str_get_serial_port_usage(int iSerialPortUsage)
{
   static char s_szSerialPortUsage[64];

   strcpy(s_szSerialPortUsage, "None");

   if ( iSerialPortUsage == SERIAL_PORT_USAGE_TELEMETRY )
      strcpy(s_szSerialPortUsage, "FC Telemetry");
   if ( iSerialPortUsage == SERIAL_PORT_USAGE_DATA_LINK )
      strcpy(s_szSerialPortUsage, "Data Link");

   if ( iSerialPortUsage == SERIAL_PORT_USAGE_SIK_RADIO )
      strcpy(s_szSerialPortUsage, "SiK Radio Interface");

   if ( iSerialPortUsage == SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_433 )
      strcpy(s_szSerialPortUsage, "ELRS 433Mhz Radio");
   if ( iSerialPortUsage == SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_868 )
      strcpy(s_szSerialPortUsage, "ELRS 868Mhz Radio");
   if ( iSerialPortUsage == SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_915 )
      strcpy(s_szSerialPortUsage, "ELRS 915Mhz Radio");
   if ( iSerialPortUsage == SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_24 )
      strcpy(s_szSerialPortUsage, "ELRS 2.4Ghz Radio");

   if ( iSerialPortUsage >= SERIAL_PORT_USAGE_CORE_PLUGIN )
      strcpy(s_szSerialPortUsage, "Core Plugin");

   return s_szSerialPortUsage;
}

char* str_get_model_flags(u32 uModelFlags)
{
   static char s_szModelFlagsDesc[1024];

   s_szModelFlagsDesc[0] = 0;

   if ( uModelFlags & MODEL_FLAG_PRIORITIZE_UPLINK)
      strcat(s_szModelFlagsDesc, " PRIORITIZE_UPLINK");
   if ( uModelFlags & MODEL_FLAG_USE_LOGER_SERVICE)
      strcat(s_szModelFlagsDesc, " USE_LOGER_SERVICE");
   if ( uModelFlags & MODEL_FLAG_DISABLE_ALL_LOGS)
      strcat(s_szModelFlagsDesc, " DISABLE_ALL_LOGS");

   return s_szModelFlagsDesc;
}

char* str_get_developer_flags(u32 uDeveloperFlags)
{
   static char s_szDeveloperFlagsDesc[1024];

   s_szDeveloperFlagsDesc[0] = 0;

   if ( uDeveloperFlags & DEVELOPER_FLAGS_BIT_LIVE_LOG)
      strcat(s_szDeveloperFlagsDesc, " LIVE_LOG");
   if ( uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS)
      strcat(s_szDeveloperFlagsDesc, " LOG_ONLY_ERRORS");
   if ( uDeveloperFlags & DEVELOPER_FLAGS_BIT_RADIO_SILENCE_FAILSAFE)
      strcat(s_szDeveloperFlagsDesc, " RADIO_SILENCE_FAILSAFE");
   if ( uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE)
      strcat(s_szDeveloperFlagsDesc, " ENABLE_DEVELOPER_MODE");
   if ( uDeveloperFlags & DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_TX_GAP)
      strcat(s_szDeveloperFlagsDesc, " SEND_VEHICLE_TX_GAP");
   if ( uDeveloperFlags & DEVELOPER_FLAGS_BIT_INJECT_VIDEO_FAULTS)
      strcat(s_szDeveloperFlagsDesc, " INJECT_VIDEO_FAULTS");
   if ( uDeveloperFlags & DEVELOPER_FLAGS_BIT_INJECT_RECOVERABLE_VIDEO_FAULTS)
      strcat(s_szDeveloperFlagsDesc, " INJECT_VIDEO_FAULTS2");
   if ( uDeveloperFlags & DEVELOPER_FLAGS_USE_PCAP_RADIO_TX )
      strcat(s_szDeveloperFlagsDesc, " USE_PCAP_RADIO_TX");
   
   if ( 0 == s_szDeveloperFlagsDesc[0] )
      strcpy(s_szDeveloperFlagsDesc, "[None]");
   return s_szDeveloperFlagsDesc;
}

char* str_get_command_response_flags_string(u32 uResponseFlags)
{
   static char s_szCommandResponseFlagsString[64];

   strcpy(s_szCommandResponseFlagsString, "[Uknown Response Flags]");

   if ( uResponseFlags == COMMAND_RESPONSE_FLAGS_OK )
      strcpy(s_szCommandResponseFlagsString, "[Ok]");
   if ( uResponseFlags == COMMAND_RESPONSE_FLAGS_FAILED )
      strcpy(s_szCommandResponseFlagsString, "[Failed]");
   if ( uResponseFlags == COMMAND_RESPONSE_FLAGS_UNKNOWN_COMMAND )
      strcpy(s_szCommandResponseFlagsString, "[Unknown Command]");
   if ( uResponseFlags == COMMAND_RESPONSE_FLAGS_FAILED_INVALID_PARAMS )
      strcpy(s_szCommandResponseFlagsString, "[Failed, Invalid Params]");
   return s_szCommandResponseFlagsString;
}

char* str_get_component_id(int iComponentId)
{
   static char s_szComponentIdString[64];
   s_szComponentIdString[0] = 0;

   if ( iComponentId == PACKET_COMPONENT_LOCAL_CONTROL )
      strcpy(s_szComponentIdString, "PACKET_COMPONENT_LOCAL_CONTROL");
   else if ( iComponentId == PACKET_COMPONENT_VIDEO )
      strcpy(s_szComponentIdString, "PACKET_COMPONENT_VIDEO");
   else if ( iComponentId == PACKET_COMPONENT_TELEMETRY )
      strcpy(s_szComponentIdString, "PACKET_COMPONENT_TELEMETRY");
   else if ( iComponentId == PACKET_COMPONENT_COMMANDS )
      strcpy(s_szComponentIdString, "PACKET_COMPONENT_COMMANDS");
   else if ( iComponentId == PACKET_COMPONENT_RC )
      strcpy(s_szComponentIdString, "PACKET_COMPONENT_RC");
   else if ( iComponentId == PACKET_COMPONENT_RUBY )
      strcpy(s_szComponentIdString, "PACKET_COMPONENT_RUBY");
   else if ( iComponentId == PACKET_COMPONENT_AUDIO )
      strcpy(s_szComponentIdString, "PACKET_COMPONENT_AUDIO");
   else
      strcpy(s_szComponentIdString, "INVALID");

   return s_szComponentIdString;
}

char* str_get_model_change_type(int iModelChangeType)
{
   static char s_szModelChangeTypeString[64];
   s_szModelChangeTypeString[0] = 0;

   if ( iModelChangeType == MODEL_CHANGED_GENERIC )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_GENERIC");
   else if ( iModelChangeType == MODEL_CHANGED_VIDEO_PARAMETERS )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_VIDEO_PARAMETERS");
   else if ( iModelChangeType == MODEL_CHANGED_DEVELOPER_FLAGS )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_DEVELOPER_FLAGS");
   else if ( iModelChangeType == MODEL_CHANGED_VIDEO_CODEC )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_VIDEO_CODEC");
   else if ( iModelChangeType == MODEL_CHANGED_RADIO_LINK_FRAMES_FLAGS )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_RADIO_LINK_FRAMES_FLAGS");
   else if ( iModelChangeType == MODEL_CHANGED_FREQUENCY )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_FREQUENCY");
   else if ( iModelChangeType == MODEL_CHANGED_RADIO_LINK_CAPABILITIES )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_RADIO_LINK_CAPABILITIES");
   else if ( iModelChangeType == MODEL_CHANGED_RADIO_DATARATES )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_RADIO_DATARATES");
   else if ( iModelChangeType == MODEL_CHANGED_RADIO_POWERS )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_RADIO_POWERS");
   else if ( iModelChangeType == MODEL_CHANGED_RADIO_LINK_PARAMS )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_RADIO_LINK_PARAMS");
   else if ( iModelChangeType == MODEL_CHANGED_CAMERA_PARAMS )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_CAMERA_PARAMS");
   else if ( iModelChangeType == MODEL_CHANGED_STATS )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_STATS");
   else if ( iModelChangeType == MODEL_CHANGED_THREADS_PRIORITIES )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_THREADS_PRIORITIES");
   else if ( iModelChangeType == MODEL_CHANGED_OVERCLOCKING )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_OVERCLOCKING");

   else if ( iModelChangeType == MODEL_CHANGED_SWAPED_RADIO_INTERFACES )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_SWAPED_RADIO_INTERFACES");
   else if ( iModelChangeType == MODEL_CHANGED_CONTROLLER_TELEMETRY )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_CONTROLLER_TELEMETRY");
   else if ( iModelChangeType == MODEL_CHANGED_RELAY_PARAMS )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_RELAY_PARAMS");
   else if ( iModelChangeType == MODEL_CHANGED_SERIAL_PORTS )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_SERIAL_PORTS");
   else if ( iModelChangeType == MODEL_CHANGED_OSD_PARAMS )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_OSD_PARAMS");
   else if ( iModelChangeType == MODEL_CHANGED_SYNCHRONISED_SETTINGS_FROM_VEHICLE )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_SYNCHRONISED_SETTINGS_FROM_VEHICLE");
   else if ( iModelChangeType == MODEL_CHANGED_RC_PARAMS )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_RC_PARAMS");
   else if ( iModelChangeType == MODEL_CHANGED_RESET_RADIO_LINK )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_RESET_RADIO_LINK");
   else
      strcpy(s_szModelChangeTypeString, "UNKNOWN");
   return s_szModelChangeTypeString;
}

char* str_format_relay_flags(u32 uRelayCapabilitiesFlags)
{
   static char s_szRelayFlagsDescription[128];
   s_szRelayFlagsDescription[0] = 0;

  
   if ( uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_TELEMETRY )
      strcat(s_szRelayFlagsDescription, " [TELEM]");
   if ( uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_VIDEO )
      strcat(s_szRelayFlagsDescription, " [VIDEO]");
   if ( uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_COMMANDS )
      strcat(s_szRelayFlagsDescription, " [COMM]");
   if ( uRelayCapabilitiesFlags & RELAY_CAPABILITY_SWITCH_OSD )
      strcat(s_szRelayFlagsDescription, " [OSD-SWITCH]");
   if ( uRelayCapabilitiesFlags & RELAY_CAPABILITY_MERGE_OSD )
      strcat(s_szRelayFlagsDescription, " [OSD-MERGE]");

   if ( 0 == s_szRelayFlagsDescription[0] )
      strcpy(s_szRelayFlagsDescription, "None");

   return s_szRelayFlagsDescription;
}

char* str_format_relay_mode(u32 uRelayMode)
{
   static char s_szRelayModeDescription[128];
   s_szRelayModeDescription[0] = 0;

   if ( uRelayMode & RELAY_MODE_PERMANENT_REMOTE )
      strcat(s_szRelayModeDescription, " Permanent-Remote");

   if ( uRelayMode & RELAY_MODE_MAIN )
      strcat(s_szRelayModeDescription, " Main");
   if ( uRelayMode & RELAY_MODE_REMOTE )
      strcat(s_szRelayModeDescription, " Remote");
   if ( uRelayMode & RELAY_MODE_PIP_MAIN )
      strcat(s_szRelayModeDescription, " PIP-Main");
   if ( uRelayMode & RELAY_MODE_PIP_REMOTE )
      strcat(s_szRelayModeDescription, " PIP-Remote");
   if ( uRelayMode & RELAY_MODE_IS_RELAY_NODE )
      strcat(s_szRelayModeDescription, " Is-Relay-Node");
   if ( uRelayMode & RELAY_MODE_IS_RELAYED_NODE )
      strcat(s_szRelayModeDescription, " Is-Relayed-Node");
   
   if ( 0 == s_szRelayModeDescription[0] )
      strcpy(s_szRelayModeDescription, "None");
   return s_szRelayModeDescription;
}

char* str_format_firmware_type(u32 uFirmwareType)
{
   static char s_szFormatFirmwareType[32];
   s_szFormatFirmwareType[0] = 0;
 
   if ( uFirmwareType == MODEL_FIRMWARE_TYPE_RUBY )
      strcpy(s_szFormatFirmwareType, "Ruby");
   else
      strcpy(s_szFormatFirmwareType, "Unknown");
   return s_szFormatFirmwareType;  
}