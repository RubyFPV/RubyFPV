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

#include "../base/shared_mem.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../base/config.h"
#include "../base/hardware_procs.h"
#include "../base/commands.h"
#include "../base/models.h"
#include "../base/models_list.h"
#include "../base/radio_utils.h"
#include "../base/hardware.h"
#include "../base/hardware_files.h"
#include "../base/hardware_camera.h"
#include "../base/hardware_radio.h"
#include "../base/hardware_radio_sik.h"
#include "../base/encr.h"
#include "../base/flags.h"
#include "../base/ruby_ipc.h"
#include "../base/core_plugins_settings.h"
#include "../base/vehicle_settings.h"
#include "../base/utils.h"
#include "../common/string_utils.h"
#include "../common/relay_utils.h"

#include <ctype.h>

#include "launchers_vehicle.h"
#include "video_source_csi.h"
#include "shared_vars.h"
#include "timers.h"
#include "../utils/utils_vehicle.h"
#include "process_upload.h"
#include "process_calib_file.h"

#include <time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <math.h>
#include <semaphore.h>

#define MAX_COMMAND_REPLY_BUFFER 4048

u32 lastRecvCommandType = 0xFFFFFFFF;
u32 lastRecvCommandNumber = 0xFFFFFFFF;
u32 lastRecvCommandResendCounter = 0xFFFFFFFF;
u32 lastRecvCommandTime = 0;
u32 lastRecvSourceControllerId = 0;
u8 lastRecvCommandResponseFlags = 0;
u8 lastRecvCommandReplyBuffer[MAX_COMMAND_REPLY_BUFFER];
int lastRecvCommandReplyBufferLength = 0;
u32 s_CurrentResponseCounter = 0;


int s_fIPCFromRouter = -1;
int s_fIPCToRouter = -1;

u8 s_BufferCommands[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeTmpBufferCommands[MAX_PACKET_TOTAL_SIZE];
int s_PipeTmpBufferCommandsPos = 0;

static bool s_bWaitForRadioFlagsChangeConfirmation = false;
static int s_iRadioLinkIdChangeConfirmation = -1;
static type_radio_links_parameters s_LastGoodRadioLinksParams;


static u32 s_ZIPParams_uLastRecvSourceControllerId = 0;
static u32 s_ZIPParams_uLastRecvCommandNumber = 0;
static u32 s_ZIPPAarams_uLastRecvCommandTime = 0;
static u8  s_ZIPParams_Model_Buffer[3048];
static int s_ZIPParams_Model_BufferLength = 0;

#define MAX_SEGMENTS_FILE_UPLOAD 10000 // About 10 Mbytes of data maximum

typedef struct
{
   u32 uLastFileId;
   char szFileName[128];
   u32 uFileSize;
   u32 uTotalSegments;
   u8* pSegments[MAX_SEGMENTS_FILE_UPLOAD];
   u32 uSegmentsSize[MAX_SEGMENTS_FILE_UPLOAD];
   bool bSegmentsReceived[MAX_SEGMENTS_FILE_UPLOAD];
   u32 uLastCommandIdForThisFile;
} ALIGN_STRUCT_SPEC_INFO t_structure_file_upload_info;

t_structure_file_upload_info s_InfoLastFileUploaded;


u8 s_bufferModelSettings[2048];
int s_bufferModelSettingsLength = 0;

void signalReloadModel(u32 uChangeType, u8 uExtraParam);


// Returns true if it was updated

bool _populate_camera_name()
{
   log_line("Populating camera name buffer...");
   bool bCameraNameUpdated = false;

   #ifdef HW_PLATFORM_OPENIPC_CAMERA

   char szOutput[4096];
   memset(szOutput, 0, sizeof(szOutput)/sizeof(szOutput[0]));
   char* pSensor = hardware_camera_get_oipc_sensor_raw_name(szOutput);
   if ( NULL != pSensor )
      bCameraNameUpdated = g_pCurrentModel->setCameraName(g_pCurrentModel->iCurrentCamera, pSensor);
   else
      bCameraNameUpdated = g_pCurrentModel->setCameraName(g_pCurrentModel->iCurrentCamera, "N/A");
   #else

   u8 dataCamera[1024];
   char szFile[128];
   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_TEMP_CAMERA_NAME);
   FILE* fd = fopen(szFile, "r");
   if ( NULL != fd )
   {
      log_line("Populating camera name buffer from file");
      int nSize = fread(dataCamera, 1, 1023, fd);
      fclose(fd);
      if ( nSize <= 0 )
         bCameraNameUpdated = g_pCurrentModel->setCameraName(g_pCurrentModel->iCurrentCamera, "");
      else
      {
         dataCamera[MAX_CAMERA_NAME_LENGTH-1] = 0;
         for( int i=0; i<MAX_CAMERA_NAME_LENGTH; i++ )
            if ( dataCamera[i] == 10 || dataCamera[i] == 13 )
               dataCamera[i] = 0;
         bCameraNameUpdated = g_pCurrentModel->setCameraName(g_pCurrentModel->iCurrentCamera, (char*)&(dataCamera[0]));
      }
   }
   else if ( hardware_isCameraVeye() )
   {
      log_line("Populating camera name buffer for veye cameras");
      char szCamName[64];
      str_get_hardware_camera_type_string_to_string(hardware_getCameraType(), szCamName);
      char szTmp[128];
      //sprintf(szTmp, "%s 0x%02X/0x02%X", szCamName, (u32)hardware_getVeyeCameraDevId(), (u32)hardware_getVeyeCameraHWVer());
      sprintf(szTmp, "0x%02X/%02X", (u32)hardware_getVeyeCameraDevId(), (u32)hardware_getVeyeCameraHWVer());
      bCameraNameUpdated = g_pCurrentModel->setCameraName(g_pCurrentModel->iCurrentCamera, szTmp);
   }
   else
   {
      log_line("Populating camera name buffer: no info.");
      bCameraNameUpdated = g_pCurrentModel->setCameraName(g_pCurrentModel->iCurrentCamera, "");
   }

   #endif

   if ( bCameraNameUpdated )
   {
      saveCurrentModel();
      signalReloadModel(0, 0);
   }
   return bCameraNameUpdated;
}

void populate_model_settings_buffer()
{
   _populate_camera_name();

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, "tmp_download_model.mdl");
   g_pCurrentModel->saveToFile(szFile, false);

   char szComm[256];
   sprintf(szComm, "rm -rf %s/model.tar* 2>/dev/null", FOLDER_RUBY_TEMP);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "rm -rf %s/model.mdl* 2>/dev/null", FOLDER_RUBY_TEMP);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "cp -rf %s/tmp_download_model.mdl %s/model.mdl 2>/dev/null", FOLDER_RUBY_TEMP, FOLDER_RUBY_TEMP);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "tar -C %s -cf %s/model.tar model.mdl 2>&1", FOLDER_RUBY_TEMP, FOLDER_RUBY_TEMP);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "gzip %s/model.tar 2>&1", FOLDER_RUBY_TEMP);
   hw_execute_bash_command(szComm, NULL);

   s_bufferModelSettingsLength = 0;
   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, "model.tar.gz");
   FILE* fd = fopen(szFile, "rb");
   if ( NULL != fd )
   {
      s_bufferModelSettingsLength = fread(s_bufferModelSettings, 1, 2000, fd);
      fclose(fd);
      log_line("Generated buffer with compressed model settings. Compressed size: %d bytes", s_bufferModelSettingsLength);
   }
   else
      log_error_and_alarm("Failed to load vehicle configuration from file: [%s]", szFile);

   sprintf(szComm, "rm -rf %s/model.tar*", FOLDER_RUBY_TEMP);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "rm -rf %s/model.mdl", FOLDER_RUBY_TEMP);
   hw_execute_bash_command(szComm, NULL); 
}


void send_model_settings_to_controller()
{
   populate_model_settings_buffer();

   if ( 0 == s_bufferModelSettingsLength || s_bufferModelSettingsLength > MAX_PACKET_PAYLOAD )
   {
      log_softerror_and_alarm("Invalid compressed model file size (%d). Skipping sending it to controller.", s_bufferModelSettingsLength);
      return;
   }

   log_line("Queueing to router to send to controller all model settings. Compressed size: %d bytes", s_bufferModelSettingsLength); 
   log_line("Queueing to router to send to controller tar model file. Total on time: %dm:%02ds, total mAh: %d", g_pCurrentModel->m_Stats.uCurrentOnTime/60, g_pCurrentModel->m_Stats.uCurrentOnTime%60, g_pCurrentModel->m_Stats.uCurrentTotalCurrent);
   log_line("Send radio messages, from VID %u to VID %u (%u)", g_pCurrentModel->uVehicleId, g_pCurrentModel->uControllerId, g_uControllerId);

   static u32 s_uCommandsSettingsParamsUniqueCounter = 0;
   s_uCommandsSettingsParamsUniqueCounter++;
   u32 uStartFlag = 0xFFFFFFF0; // tar gzip format
   u8 uFlags = 0;

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_RUBY_MODEL_SETTINGS, STREAM_ID_DATA);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = g_pCurrentModel->uControllerId;
   PH.total_length = sizeof(t_packet_header) + s_bufferModelSettingsLength + 2*sizeof(u32) + sizeof(u8);

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet + sizeof(t_packet_header), (u8*)&uStartFlag, sizeof(u32));
   memcpy(packet + sizeof(t_packet_header) + sizeof(u32), (u8*)&s_uCommandsSettingsParamsUniqueCounter, sizeof(u32));
   memcpy(packet + sizeof(t_packet_header) + 2*sizeof(u32), (u8*)&uFlags, sizeof(u8));
   memcpy(packet + sizeof(t_packet_header) + 2*sizeof(u32) + sizeof(u8), (u8*)&(s_bufferModelSettings[0]), s_bufferModelSettingsLength);

   ruby_ipc_channel_send_message(s_fIPCToRouter, packet, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

   log_line("Sent to router all model settings (send unique id: %u). Total compressed size: %d bytes", s_uCommandsSettingsParamsUniqueCounter, s_bufferModelSettingsLength); 
   
   int iSegmentSize = 150;
   int iCountSegments = s_bufferModelSettingsLength / iSegmentSize;
   if ( iCountSegments * iSegmentSize != s_bufferModelSettingsLength )
      iCountSegments++;
   int iPos = 0;
   int iSegment = 0;
   u8 uSegment[256];
         
   uFlags = 1;
   while ( iPos < s_bufferModelSettingsLength )
   {
      int iSize = iSegmentSize;
      if ( iPos + iSize > s_bufferModelSettingsLength )
         iSize = s_bufferModelSettingsLength - iPos;
      
      uSegment[0] = iSegment;
      uSegment[1] = iCountSegments;
      uSegment[2] = iSize;
      memcpy( &(uSegment[3]), (u8*)((&(s_bufferModelSettings[0]))+iPos), iSize);

      radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_RUBY_MODEL_SETTINGS, STREAM_ID_DATA);
      PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      PH.vehicle_id_dest = g_pCurrentModel->uControllerId;
      PH.total_length = sizeof(t_packet_header) + 2*sizeof(u32) + sizeof(u8) + 3*sizeof(u8) + iSize;
   
      memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
      memcpy(packet + sizeof(t_packet_header), (u8*)&uStartFlag, sizeof(u32));
      memcpy(packet + sizeof(t_packet_header) + sizeof(u32), (u8*)&s_uCommandsSettingsParamsUniqueCounter, sizeof(u32));
      memcpy(packet + sizeof(t_packet_header) + 2*sizeof(u32), (u8*)&uFlags, sizeof(u8));
      memcpy(packet + sizeof(t_packet_header) + 2*sizeof(u32) + sizeof(u8), uSegment, iSize + 3 * sizeof(u8));

      ruby_ipc_channel_send_message(s_fIPCToRouter, packet, PH.total_length);

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      log_line("Sent to router all model settings in small segments (send unique id: %u). Segment %d of %d, %d bytes",
          s_uCommandsSettingsParamsUniqueCounter, iSegment+1, (int)uSegment[1], (int) uSegment[2]); 
  
      iSegment++;
      iPos += iSize;
   }

   log_line("Done sending to router all model settings to be queued for sending to radio, in small packets, %d packets. Total model compressed size: %d bytes",
       iSegment, s_bufferModelSettingsLength);
}

void signalReinitializeRouterRadioLinks()
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_REINITIALIZE_RADIO_LINKS, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_COMMANDS;
   PH.total_length = sizeof(t_packet_header);

   ruby_ipc_channel_send_message(s_fIPCToRouter, (u8*)&PH, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
}

void _signalRouterSendVehicleSettings()
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SEND_MODEL_SETTINGS, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_COMMANDS;
   PH.total_length = sizeof(t_packet_header);

   ruby_ipc_channel_send_message(s_fIPCToRouter, (u8*)&PH, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
}

void signalReloadModel(u32 uChangeType, u8 uExtraParam)
{
   log_line("Signaling router to reload model as it has changed. Change type: %u (%s)", uChangeType, str_get_model_change_type((int)uChangeType) );

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_COMMANDS | (uChangeType<<8) | (uExtraParam <<16);
   PH.total_length = sizeof(t_packet_header);

   ruby_ipc_channel_send_message(s_fIPCToRouter, (u8*)&PH, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
}

void signalReboot(bool bSaveModel)
{
   log_line("Signaled reboot");

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_REBOOT, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_COMMANDS;
   PH.vehicle_id_dest = bSaveModel?1:0;
   PH.total_length = sizeof(t_packet_header);

   ruby_ipc_channel_send_message(s_fIPCToRouter, (u8*)&PH, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
}


void _signalCameraParametersChanged(u8 uCameraIndex)
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SET_CAMERA_PARAMS, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_COMMANDS;
   PH.vehicle_id_dest = PACKET_COMPONENT_RUBY;
   PH.total_length = sizeof(t_packet_header) + sizeof(u8) + sizeof(type_camera_parameters);

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(&buffer[sizeof(t_packet_header)], &uCameraIndex, sizeof(u8));
   memcpy(&buffer[sizeof(t_packet_header) + sizeof(u8)], (u8*)&(g_pCurrentModel->camera_params[uCameraIndex]), sizeof(type_camera_parameters));
   ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
}


void sendControlMessage(u8 packet_type, u32 extraParam)
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, packet_type, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_COMMANDS;
   PH.vehicle_id_dest = extraParam;
   PH.total_length = sizeof(t_packet_header);
   log_line("Send control message to router: %s, param: %u", str_get_packet_type(packet_type), extraParam);
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
}

void setCommandReplyBuffer(u8* pData, int length)
{
   if ( length <= 0 )
      length = 0;
   if ( length > MAX_COMMAND_REPLY_BUFFER)
      length = MAX_COMMAND_REPLY_BUFFER;

   lastRecvCommandReplyBufferLength = length;

   if ( 0 < length )
      memcpy(lastRecvCommandReplyBuffer, pData, length);
}

void setCommandReplyBufferExtra(u8* pData, int length, u8* pExtra, int extra)
{
   if ( (length+extra) <= 0 )
   {
      length = 0;
      extra = 0;
   }
   if ( (length+extra) > MAX_COMMAND_REPLY_BUFFER)
   {
      length = MAX_COMMAND_REPLY_BUFFER;
      extra = 0;
   }
   lastRecvCommandReplyBufferLength = length+extra;

   if ( 0 < length )
      memcpy(lastRecvCommandReplyBuffer, pData, length);
   if ( 0 < extra )
      memcpy(lastRecvCommandReplyBuffer+length, pExtra, extra);
}

 
void sendCommandReply(u8 responseFlags, int iResponseExtraParam, int delayMiliSec)
{
   if ( lastRecvCommandType & COMMAND_TYPE_FLAG_NO_RESPONSE_NEEDED )
      return;

   lastRecvCommandResponseFlags = responseFlags;

   t_packet_header PH;
   t_packet_header_command_response PHCR;

   radio_packet_init(&PH, PACKET_COMPONENT_COMMANDS, PACKET_TYPE_COMMAND_RESPONSE, STREAM_ID_DATA);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = lastRecvSourceControllerId;
   PH.total_length = sizeof(t_packet_header)+sizeof(t_packet_header_command_response) + lastRecvCommandReplyBufferLength;


   PHCR.origin_command_type = lastRecvCommandType;
   PHCR.origin_command_counter = lastRecvCommandNumber;
   PHCR.origin_command_resend_counter = lastRecvCommandResendCounter;
   PHCR.command_response_flags = lastRecvCommandResponseFlags;
   PHCR.command_response_param = iResponseExtraParam;
   PHCR.response_counter = s_CurrentResponseCounter;
   
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), (u8*)&PHCR, sizeof(t_packet_header_command_response));
   memcpy(buffer+sizeof(t_packet_header)+sizeof(t_packet_header_command_response), lastRecvCommandReplyBuffer, lastRecvCommandReplyBufferLength);
   ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, PH.total_length);

   char szBuff[64];
   szBuff[0] = 0;
   strcpy(szBuff, str_get_command_response_flags_string(lastRecvCommandResponseFlags) );
   //log_line_commands("Sent response %s to router for vehicle UID: %u, command nb.%d, command retry counter: %d, command type: %s ", szBuff, g_pCurrentModel->uVehicleId, lastRecvCommandNumber, lastRecvCommandResendCounter, commands_get_description(lastRecvCommandType));
   log_line_commands("Sent response %s, command nb.%d, retry counter: %d, type: %s ", szBuff, lastRecvCommandNumber, lastRecvCommandResendCounter, commands_get_description(lastRecvCommandType));
   s_CurrentResponseCounter++;

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

   if ( (delayMiliSec > 0) && (delayMiliSec < 500) )
      hardware_sleep_ms(delayMiliSec);
}


void save_config_file()
{
   #if defined (HW_PLATFORM_RASPBERRY)
   hardware_mount_boot();
   hardware_sleep_ms(200);
   hw_execute_bash_command("cp /boot/config.txt config.txt", NULL);

   config_file_set_value("config.txt", "over_voltage", g_pCurrentModel->processesPriorities.iOverVoltage);
   config_file_set_value("config.txt", "over_voltage_sdram", g_pCurrentModel->processesPriorities.iOverVoltage);
   config_file_set_value("config.txt", "over_voltage_min", g_pCurrentModel->processesPriorities.iOverVoltage);

   config_file_set_value("config.txt", "arm_freq", g_pCurrentModel->processesPriorities.iFreqARM);
   config_file_set_value("config.txt", "arm_freq_min", g_pCurrentModel->processesPriorities.iFreqARM);

   config_file_set_value("config.txt", "gpu_freq", g_pCurrentModel->processesPriorities.iFreqGPU);
   config_file_set_value("config.txt", "gpu_freq_min", g_pCurrentModel->processesPriorities.iFreqGPU);
   config_file_set_value("config.txt", "core_freq_min", g_pCurrentModel->processesPriorities.iFreqGPU);
   config_file_set_value("config.txt", "h264_freq_min", g_pCurrentModel->processesPriorities.iFreqGPU);
   config_file_set_value("config.txt", "isp_freq_min", g_pCurrentModel->processesPriorities.iFreqGPU);
   config_file_set_value("config.txt", "v3d_freq_min", g_pCurrentModel->processesPriorities.iFreqGPU);

   config_file_set_value("config.txt", "sdram_freq", g_pCurrentModel->processesPriorities.iFreqGPU);
   config_file_set_value("config.txt", "sdram_freq_min", g_pCurrentModel->processesPriorities.iFreqGPU);

   hw_execute_bash_command("cp config.txt /boot/config.txt", NULL);
   #endif
}

bool _process_file_download_request( u8* pBuffer, int length)
{
   t_packet_header_command* pPHC = (t_packet_header_command*)(pBuffer + sizeof(t_packet_header));

   t_packet_header_download_file_info PHDFInfo;

   u32 uFileId = pPHC->command_param & 0x00FFFFFF;
   bool bCheckStatus = (pPHC->command_param & 0xFF000000)?true:false;
   PHDFInfo.file_id = uFileId;
   PHDFInfo.szFileName[0] = 0;
   PHDFInfo.segments_count = 0;
   PHDFInfo.segment_size = 0;
   PHDFInfo.isReady = 0;

   if ( bCheckStatus )
      log_line("Received request to check download ready statusl for file id: %d", uFileId);
   else
      log_line("Received request to download file id: %d", uFileId);

   if ( uFileId == FILE_ID_VEHICLE_LOGS_ARCHIVE )
   {
      if ( bCheckStatus )
      {
         if ( hw_process_exists("zip") )
            PHDFInfo.isReady = 0;
         else
         {
            PHDFInfo.isReady = 1;
            strcpy((char*)PHDFInfo.szFileName, "logs.zip");

            char szFile[MAX_FILE_PATH_SIZE];
            strcpy(szFile, FOLDER_RUBY_TEMP);
            strcat(szFile, "logs.zip");
            FILE* fd = fopen(szFile, "rb");
            if ( NULL != fd )
            {
               fseek(fd, 0, SEEK_END);
               long fSize = ftell(fd);
               fclose(fd);
               PHDFInfo.segment_size = 1117;
               PHDFInfo.segments_count = fSize/PHDFInfo.segment_size;
               if ( fSize % PHDFInfo.segment_size )
                  PHDFInfo.segments_count++;
               log_line("File logs archive: %u bytes, %d segments, %d bytes/segment", fSize, PHDFInfo.segments_count, PHDFInfo.segment_size );
            }
            else
               log_softerror_and_alarm("Failed to create archive with log files in file [%s]", szFile);
         }
      }
      else
      {
         char szComm[256];
         sprintf(szComm, "rm -rf %s/logs.zip", FOLDER_RUBY_TEMP);
         hw_execute_bash_command(szComm, NULL);
         sprintf(szComm, "zip %s/logs.zip %s/* > /dev/null 2>&1 &", FOLDER_RUBY_TEMP, FOLDER_LOGS);
         hw_execute_bash_command(szComm, NULL);
      }
   }

   setCommandReplyBuffer((u8*)&PHDFInfo, sizeof(PHDFInfo));
   sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
   return true;
}


bool _process_file_segment_download_request( u8* pBuffer, int length)
{
   t_packet_header_command* pPHC = (t_packet_header_command*)(pBuffer + sizeof(t_packet_header));

   u32 uFileId = (pPHC->command_param & 0xFFFF);
   u32 uSegmentId = (pPHC->command_param >> 16);

   log_line("Received request to download file segment %u for file id: %u", uSegmentId, uFileId);

   u8 buffer[2000];

   u32 flags = pPHC->command_param;
   memcpy(buffer, (u8*)&flags, sizeof(u32));
   if ( uFileId == FILE_ID_VEHICLE_LOGS_ARCHIVE )
   {
      char szFile[MAX_FILE_PATH_SIZE];
      strcpy(szFile, FOLDER_RUBY_TEMP);
      strcat(szFile, "logs.zip");
      FILE* fd = fopen(szFile, "rb");
      if ( NULL != fd )
      {
          fseek(fd, uSegmentId*1117, SEEK_SET);
          if ( 1117 != fread(&buffer[4], 1, 1117, fd) )
             log_softerror_and_alarm("Failed to read vehicle logs zip file: [%s]", szFile);
          fclose(fd);
      }
      else
      {
         log_softerror_and_alarm("Failed to open for read vehicle logs zip file: [%s]", szFile);
         memset(&buffer[4], 0, 1117);
      }
   }

   lastRecvCommandType &= ~COMMAND_TYPE_FLAG_NO_RESPONSE_NEEDED;
   setCommandReplyBuffer(buffer, 1117+sizeof(u32));
   sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
   return true;
}

void _process_received_uploaded_file()
{
   if ( s_InfoLastFileUploaded.uLastFileId == FILE_ID_CORE_PLUGINS_ARCHIVE )
   {
      log_line("Processing received core plugins archive (%u bytes)...", s_InfoLastFileUploaded.uFileSize);
      char szComm[128];
      sprintf(szComm, "rm -rf %s/core_plugins.zip 2>&1", FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);
      
      char szFile[MAX_FILE_PATH_SIZE];
      strcpy(szFile, FOLDER_RUBY_TEMP);
      strcat(szFile, "core_plugins.zip");
      FILE* fd = fopen(szFile, "wb");
      if ( NULL == fd )
      {
         log_softerror_and_alarm("Failed to create file on disk for uploaded plugins: [%s]", szFile);
         return;
      }
      for( u32 u=0; u<s_InfoLastFileUploaded.uTotalSegments; u++ )
      {
          int nRes = fwrite(s_InfoLastFileUploaded.pSegments[u], 1, s_InfoLastFileUploaded.uSegmentsSize[u], fd );
          if ( nRes != (int)s_InfoLastFileUploaded.uSegmentsSize[u] )
          {
             log_softerror_and_alarm("Failed to write file on disk for uploaded plugins.");
             return;
          }
      }

      fflush(fd);
      fclose(fd);
      char szOutput[4096];
      sprintf(szComm, "chmod 777 %s/core_plugins.zip 2>&1", FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);
      hardware_sleep_ms(100);
      sprintf(szComm, "unzip %s/core_plugins.zip -d %s 2>&1", FOLDER_RUBY_TEMP, FOLDER_CORE_PLUGINS);
      hw_execute_bash_command(szComm, szOutput);
      log_line("Result: [%s]", szOutput);
      sprintf(szComm, "chmod 777 %s/*", FOLDER_CORE_PLUGINS);
      hw_execute_bash_command(szComm, NULL);
      log_line("Finised processing core plugins archive.");
   }
}

bool _process_file_segment_upload_request( u8* pBuffer, int length)
{
   //t_packet_header_command* pPHC = (t_packet_header_command*)(pBuffer + sizeof(t_packet_header));

   // Discard duplicate segments/commands for last file if already completed

   if ( s_InfoLastFileUploaded.uLastCommandIdForThisFile != 0 )
   if ( lastRecvCommandNumber <= s_InfoLastFileUploaded.uLastCommandIdForThisFile )
   {
      setCommandReplyBuffer(NULL, 0);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      return true;
   }

   command_packet_upload_file_segment segmentData;
   memcpy((u8*)&segmentData, pBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command), sizeof(command_packet_upload_file_segment));

   log_line("Received request to upload file segment %u of %u (%d bytes) for file [%s], file id: %u", segmentData.uSegmentIndex+1, segmentData.uTotalSegments, segmentData.uSegmentSize, segmentData.szFileName, segmentData.uFileId);


   if ( (s_InfoLastFileUploaded.uLastFileId != segmentData.uFileId) ||
        (s_InfoLastFileUploaded.uTotalSegments != segmentData.uTotalSegments) ||
        (s_InfoLastFileUploaded.uFileSize != segmentData.uTotalFileSize) )
   {
      for( u32 u=0; u<s_InfoLastFileUploaded.uTotalSegments; u++ )
      {
         if ( NULL != s_InfoLastFileUploaded.pSegments[u] )
            free(s_InfoLastFileUploaded.pSegments[u]);
         s_InfoLastFileUploaded.pSegments[u] = NULL;
      }

      s_InfoLastFileUploaded.uLastFileId = segmentData.uFileId;
      strncpy(s_InfoLastFileUploaded.szFileName, segmentData.szFileName, 128);
      s_InfoLastFileUploaded.uTotalSegments = segmentData.uTotalSegments;
      s_InfoLastFileUploaded.uFileSize = segmentData.uTotalFileSize;

      for( u32 u=0; u<s_InfoLastFileUploaded.uTotalSegments; u++ )
      {
         s_InfoLastFileUploaded.pSegments[u] = (u8*)malloc(MAX_PACKET_TOTAL_SIZE);
         if ( NULL == s_InfoLastFileUploaded.pSegments[u] )
         {
            s_InfoLastFileUploaded.uLastFileId = MAX_U32;
            setCommandReplyBuffer(NULL, 0);
            sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
            return true;
         }

         s_InfoLastFileUploaded.uSegmentsSize[u] = 0;
         s_InfoLastFileUploaded.bSegmentsReceived[u] = false;
      }
   }

   if ( segmentData.uSegmentIndex >= segmentData.uTotalSegments )
   {
      s_InfoLastFileUploaded.uLastFileId = MAX_U32;
      setCommandReplyBuffer(NULL, 0);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
      return true;
   }

   s_InfoLastFileUploaded.uSegmentsSize[segmentData.uSegmentIndex] = segmentData.uSegmentSize;
   memcpy(s_InfoLastFileUploaded.pSegments[segmentData.uSegmentIndex],
       pBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command) + sizeof(command_packet_upload_file_segment),
       segmentData.uSegmentSize);

   s_InfoLastFileUploaded.bSegmentsReceived[segmentData.uSegmentIndex] = true;

   bool bReceivedAll = true;

   for( u32 u=0; u<s_InfoLastFileUploaded.uTotalSegments; u++ )
      if ( ! s_InfoLastFileUploaded.bSegmentsReceived[u] )
      {
         bReceivedAll = false;
         break;
      }

   if ( bReceivedAll )
   {
      log_line("Received entire file [%s], %u bytes", s_InfoLastFileUploaded.szFileName, s_InfoLastFileUploaded.uFileSize);
      _process_received_uploaded_file();

      for( u32 u=0; u<s_InfoLastFileUploaded.uTotalSegments; u++ )
      {
         if ( NULL != s_InfoLastFileUploaded.pSegments[u] )
            free(s_InfoLastFileUploaded.pSegments[u]);
         s_InfoLastFileUploaded.pSegments[u] = NULL;
      }
      s_InfoLastFileUploaded.uLastFileId = MAX_U32;
      s_InfoLastFileUploaded.uTotalSegments = 0;
      s_InfoLastFileUploaded.uLastCommandIdForThisFile = lastRecvCommandNumber;
   }

   setCommandReplyBuffer(NULL, 0);
   sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
   return true;
}


// returns true if it knows about the command, false if it's an unknown command

bool process_command(u8* pBuffer, int length)
{
   int replyDelay = 0; //milisec
   int iParamsLength = length - sizeof(t_packet_header) - sizeof(t_packet_header_command);

   char szBuff[2048];
   t_packet_header_command* pPHC = (t_packet_header_command*)(pBuffer + sizeof(t_packet_header));

   u16 uCommandType = ((pPHC->command_type) & COMMAND_TYPE_MASK);
   u32 uCommandParam = pPHC->command_param;

   if ( uCommandType == COMMAND_ID_SET_ENCRYPTION_PARAMS )
   {
      u8* pData = (pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      u8 flags = *pData;
      pData++;
      u8 len = *pData;
      pData++;
      log_line("Received e flags %d, len: %d", (int)flags, (int)len);
      if ( flags == MODEL_ENC_FLAGS_NONE )
      {
         char szComm[128];
         sprintf(szComm, "rm -rf %s%s", FOLDER_CONFIG, FILE_CONFIG_ENCRYPTION_PASS);
         hw_execute_bash_command(szComm, NULL);
         rpp(); 
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);

         g_pCurrentModel->enc_flags = flags;
         saveCurrentModel();
         signalReloadModel(0, 0);
         return true;
      }

      char szFile[128];
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_ENCRYPTION_PASS);
      FILE* fd = fopen(szFile, "w");
      if ( NULL == fd )
      {
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
         return true;
      }
      fwrite(pData, 1, len, fd);
      fclose(fd);

      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);

      g_pCurrentModel->enc_flags = flags;
      saveCurrentModel();
      signalReloadModel(0, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_FACTORY_RESET )
   {
      for( int i=0; i<20; i++ )
      {
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 80);
         hardware_sleep_ms(50);
      }
      for( int i=0; i<2; i++ )
         hardware_sleep_ms(400);

      #if defined (HW_PLATFORM_RASPBERRY) || defined (HW_PLATFORM_RADXA)
      char szComm[256];
      if ( 0 < strlen(FOLDER_CONFIG) )
      {
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s*", FOLDER_CONFIG);
         hw_execute_bash_command(szComm, NULL);
      }
      if ( 0 < strlen(FOLDER_MEDIA) )
      {
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s*", FOLDER_MEDIA);
         hw_execute_bash_command(szComm, NULL);
      }
      if ( 0 < strlen(FOLDER_UPDATES) )
      {
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s*", FOLDER_UPDATES);
         hw_execute_bash_command(szComm, NULL);
      }
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "touch %s%s", FOLDER_CONFIG, FILE_CONFIG_FIRST_BOOT);
      hw_execute_bash_command(szComm, NULL);
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "touch %s%s", FOLDER_CONFIG, LOG_USE_PROCESS);
      hw_execute_bash_command(szComm, NULL);
      hardware_sleep_ms(50);

      if ( 0 < strlen(FOLDER_LOGS) )
      {
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s*", FOLDER_LOGS);
         hw_execute_bash_command(szComm, NULL);
      }
      hardware_reboot();
      #endif

      #if defined (HW_PLATFORM_OPENIPC_CAMERA)
      hw_execute_bash_command("firstboot", NULL);
      hardware_sleep_ms(500);
      #endif
      return true;
   }

   if ( uCommandType == COMMAND_ID_DOWNLOAD_FILE )
   {
      return _process_file_download_request( pBuffer, length);
   }

   if ( uCommandType == COMMAND_ID_DOWNLOAD_FILE_SEGMENT )
   {
      return _process_file_segment_download_request( pBuffer, length);
   }

   if ( uCommandType == COMMAND_ID_UPLOAD_FILE_SEGMENT )
   {
      return _process_file_segment_upload_request( pBuffer, length);    
   }

   if ( uCommandType == COMMAND_ID_CLEAR_LOGS )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      //hw_execute_bash_command_raw("rm -rf logs/* &", NULL);
      system("rm -rf logs/* &");
      log_line("Deleted all logs. Parameter: %d", (int)uCommandParam);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_RELAY_PARAMETERS )
   {
      if ( iParamsLength != sizeof(type_relay_parameters) )
      {
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
         return true;
      }
      for( int i=0; i<20; i++ )
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 20);

      type_relay_parameters* params = (type_relay_parameters*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      log_line("Received command with relay params: VID: %u, freq: %s, on vehicle's radio link %d, relay flags: (%s)", params->uRelayedVehicleId, str_format_frequency(params->uRelayFrequencyKhz), params->isRelayEnabledOnRadioLinkId, str_format_relay_flags(params->uRelayCapabilitiesFlags));
      memcpy(&(g_pCurrentModel->relay_params), params, sizeof(type_relay_parameters));

      saveCurrentModel();
      signalReloadModel(MODEL_CHANGED_RELAY_PARAMS, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_GET_SIK_CONFIG )
   {
      strcpy(szBuff, "Done info");
      log_line("Sending back SiK info, %d bytes", strlen(szBuff)+1);
      setCommandReplyBuffer((u8*)szBuff, strlen(szBuff)+1);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, replyDelay);
      return true;
   }

   if ( uCommandType == COMMAND_ID_GET_CORE_PLUGINS_INFO )
   {
      log_line("Received command to get core plugins info.");
      command_packet_core_plugins_response response;
      
      load_CorePlugins(1);
      load_CorePluginsSettings();
      
      response.iCountPlugins = get_CorePluginsCount();
      for( int i=0; i<response.iCountPlugins; i++ )
      {
         char* szGUID = get_CorePluginGUID(i);
         CorePluginSettings* pSettings = get_CorePluginSettings(szGUID);
         if ( NULL != pSettings )
            memcpy((u8*)&(response.listPlugins[i]), (u8*)pSettings, sizeof(CorePluginSettings));
      }
      log_line("Sending back to controller response to get core plugins info. %d core plugins:", response.iCountPlugins);
      for( int i=0; i<get_CorePluginsCount(); i++ )
      {
         char* szGUID = get_CorePluginGUID(i);
         char* szName = get_CorePluginName(i);
         CorePluginSettings* pSettings = get_CorePluginSettings(szGUID);
         if ( NULL == pSettings )
         {
            log_softerror_and_alarm("Commands: NULL core plugin settings object.");
            continue;
         }
         if ( NULL == szGUID )
         {
            log_softerror_and_alarm("Commands: NULL core plugin GUID.");
            continue;
         }
         if ( NULL == szName )
         {
            log_softerror_and_alarm("Commands: NULL core plugin name.");
            continue;
         }
         if ( 0 == pSettings->szGUID[0] )
         {
            log_softerror_and_alarm("Commands: Empty core plugin settings GUID for plugin [%s]", szName);
            continue;
         }
         if ( 0 == pSettings->szName[0] )
         {
            log_softerror_and_alarm("Commands: Empty core plugin settings name for plugin [%s]", szName);
            continue;
         }
         log_line("Core plugin %d: [%s]-[%s]: [%s]-[%s], enabled: %d", i+1, szName, szGUID, pSettings->szName, pSettings->szGUID, pSettings->iEnabled);
      }
      setCommandReplyBuffer((u8*)&response, sizeof(command_packet_core_plugins_response));
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_OVERCLOCKING_PARAMS )
   {
      if ( iParamsLength != sizeof(command_packet_overclocking_params) )
      {
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
         return true;
      }
      command_packet_overclocking_params* params = (command_packet_overclocking_params*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      g_pCurrentModel->processesPriorities.iFreqARM = params->freq_arm;
      g_pCurrentModel->processesPriorities.iFreqGPU = params->freq_gpu;
      g_pCurrentModel->processesPriorities.iOverVoltage = params->overvoltage;
      log_line("Received overclocking params: %d mhz arm freq, %d mhz gpu freq, %d overvoltage", g_pCurrentModel->processesPriorities.iFreqARM, g_pCurrentModel->processesPriorities.iFreqGPU, g_pCurrentModel->processesPriorities.iOverVoltage);
      saveCurrentModel();
      save_config_file();
      signalReloadModel(MODEL_CHANGED_OVERCLOCKING, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_RESET_CPU_SPEED )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      #ifdef HW_PLATFORM_RASPBERRY
      u32 board_type = (hardware_getBoardType() & BOARD_TYPE_MASK);
      g_pCurrentModel->processesPriorities.iFreqARM = DEFAULT_ARM_FREQ;
      if ( board_type == BOARD_TYPE_PIZERO2 )
         g_pCurrentModel->processesPriorities.iFreqARM = 1000;
      else if ( board_type == BOARD_TYPE_PI3B )
         g_pCurrentModel->processesPriorities.iFreqARM = 1200;
      else if ( (board_type == BOARD_TYPE_PI3BPLUS) || (board_type == BOARD_TYPE_PI4B) || (board_type == BOARD_TYPE_PI3APLUS) )
         g_pCurrentModel->processesPriorities.iFreqARM = 1400;
      else if ( (board_type != BOARD_TYPE_PIZERO) && (board_type != BOARD_TYPE_PIZEROW) && (board_type != BOARD_TYPE_NONE) 
               && (board_type != BOARD_TYPE_PI2B) && (board_type != BOARD_TYPE_PI2BV11) && (board_type != BOARD_TYPE_PI2BV12) )
         g_pCurrentModel->processesPriorities.iFreqARM = 1200;
      #endif

      #if defined(HW_PLATFORM_OPENIPC_CAMERA)
      g_pCurrentModel->processesPriorities.iFreqARM = DEFAULT_FREQ_OPENIPC_SIGMASTAR;
      hardware_set_default_sigmastar_cpu_freq();
      #endif
      
      g_pCurrentModel->processesPriorities.iFreqGPU = 400;
      g_pCurrentModel->processesPriorities.iOverVoltage = 3;
      saveCurrentModel();
      save_config_file();
      return true;
   }

   if ( uCommandType == COMMAND_ID_GET_MODULES_INFO )
   {
      char szBuffer[2048];
      char szOutput[4096];
      szBuffer[0] = 0;

      if ( 1 == uCommandParam )
      {
         #ifdef HW_PLATFORM_RASPBERRY
         strcpy(szBuffer, "Platform: Raspberry Pi#");
         #endif

         #ifdef HW_PLATFORM_OPENIPC_CAMERA
         strcpy(szBuffer, "Platform: OpenIPC#");
         strcat(szBuffer, "SOC: ");
         strcat(szBuffer, str_get_hardware_board_name(g_pCurrentModel->hwCapabilities.uBoardType));
         strcat(szBuffer, "#");

         char szTmpBuild[256];
         strcat(szBuffer, "OpenIPC firmware build: ");
         szTmpBuild[0] = 0;
         hw_execute_bash_command_raw("cat /etc/os-release | grep VERSION_ID", szTmpBuild);
         removeNewLines(szTmpBuild);
         log_line("Value: (%s)", szTmpBuild);
         strcat(szBuffer, szTmpBuild);
         strcat(szBuffer, "#");

         strcat(szBuffer, "OpenIPC firmware version: ");
         szTmpBuild[0] = 0;
         hw_execute_bash_command_raw("cat /etc/os-release | grep OPENIPC_VERSION", szTmpBuild);
         removeNewLines(szTmpBuild);
         log_line("Value: (%s)", szTmpBuild);
         strcat(szBuffer, szTmpBuild);
         strcat(szBuffer, "#");

         strcat(szBuffer, "OpenIPC name: ");
         szTmpBuild[0] = 0;
         hw_execute_bash_command_raw("cat /etc/os-release | grep PRETTY_NAME", szTmpBuild);
         log_line("Value: (%s)", szTmpBuild);
         removeNewLines(szTmpBuild);
         log_line("Value: (%s)", szTmpBuild);
         strcat(szBuffer, szTmpBuild);
         strcat(szBuffer, "#");

         strcat(szBuffer, "OpenIPC Git: ");
         szTmpBuild[0] = 0;
         hw_execute_bash_command_raw("cat /etc/os-release | grep GITHUB_VERSION", szTmpBuild);
         removeNewLines(szTmpBuild);
         log_line("Value: (%s)", szTmpBuild);
         strcat(szBuffer, szTmpBuild);
         strcat(szBuffer, "#");

         strcat(szBuffer, "Majestic version: ");
         szTmpBuild[0] = 0;
         hw_execute_bash_command_raw("/usr/bin/majestic --version", szTmpBuild);
         removeNewLines(szTmpBuild);
         log_line("Value: (%s)", szTmpBuild);
         strcat(szBuffer, szTmpBuild);
         strcat(szBuffer, "#");
         #endif

         hw_execute_bash_command_raw("cat /proc/device-tree/model", szOutput);
         strcat(szBuffer, "CPU: ");
         strcat(szBuffer, szOutput);
         strcat(szBuffer, "#"); 

         szOutput[0] = 0;
         #ifdef HW_PLATFORM_RASPBERRY
         hw_execute_bash_command_raw("cat /proc/cpuinfo | grep 'Revision' | awk '{print $3}'", szOutput);
         strcat(szBuffer, "CPU Id: ");
         strcat(szBuffer, szOutput);
         strcat(szBuffer, "#");
         #endif

         strcat(szBuffer, "Detected board type: ");
         strcat(szBuffer, str_get_hardware_board_name(hardware_getBoardType()));
         strcat(szBuffer, "#");
         strcat(szBuffer, "Stored board type: ");
         strcat(szBuffer, str_get_hardware_board_name(g_pCurrentModel->hwCapabilities.uBoardType));
         char szTemp[1024];
         sprintf(szTemp, " (%d.%d)", (int)(g_pCurrentModel->hwCapabilities.uBoardType & BOARD_TYPE_MASK), (int)(g_pCurrentModel->hwCapabilities.uBoardType & BOARD_SUBTYPE_MASK) >> BOARD_SUBTYPE_SHIFT);
         strcat(szBuffer, szTemp);
         strcat(szBuffer, "#");

         hw_execute_bash_command_raw("free -m  | grep Mem", szOutput);
         long lt, lu, lf;
         sscanf(szOutput, "%s %ld %ld %ld", szTemp, &lt, &lu, &lf);
         sprintf(szOutput, "%ld Mb Total, %ld Mb Used, %ld Mb Free", lt, lu, lf);
         strcat(szBuffer, "Memory: ");
         strcat(szBuffer, szOutput);
         strcat(szBuffer, "#");
      }

      if ( 0 == uCommandParam )
      {
         strcpy(szBuffer, "Modules versions: #");

         hw_execute_ruby_process_wait(NULL, "ruby_start", "-ver", szOutput, 1);
         if ( strlen(szOutput)> 0 )
         if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
            szOutput[strlen(szOutput)-1] = 0;
         if ( strlen(szOutput)> 0 )
         if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
            szOutput[strlen(szOutput)-1] = 0;
         strcat(szBuffer, "ruby_start: ");
         strcat(szBuffer, szOutput);

         hw_execute_ruby_process_wait(NULL, "ruby_rt_vehicle", "-ver", szOutput, 1);
         if ( strlen(szOutput)> 0 )
         if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
            szOutput[strlen(szOutput)-1] = 0;
         if ( strlen(szOutput)> 0 )
         if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
            szOutput[strlen(szOutput)-1] = 0;
         strcat(szBuffer, ", ruby_rt_vehicle: ");
         strcat(szBuffer, szOutput);
         
         hw_execute_ruby_process_wait(NULL, "ruby_tx_telemetry", "-ver", szOutput, 1);
         if ( strlen(szOutput)> 0 )
         if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
            szOutput[strlen(szOutput)-1] = 0;
         if ( strlen(szOutput)> 0 )
         if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
            szOutput[strlen(szOutput)-1] = 0;
         strcat(szBuffer, ", ruby_tx_telemetry: ");
         strcat(szBuffer, szOutput);

         #ifdef HW_PLATFORM_RASPBERRY
         hw_execute_bash_command_raw_silent("./ruby_capture_raspi -ver", szOutput);
         if ( strlen(szOutput)> 0 )
         if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
            szOutput[strlen(szOutput)-1] = 0;
         if ( strlen(szOutput)> 0 )
         if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
            szOutput[strlen(szOutput)-1] = 0;
         strcat(szBuffer, ", ruby_capture_csi: ");
         strcat(szBuffer, szOutput);

         hw_execute_bash_command_raw_silent("./ruby_capture_veye -ver", szOutput);
         if ( strlen(szOutput)> 0 )
         if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
            szOutput[strlen(szOutput)-1] = 0;
         if ( strlen(szOutput)> 0 )
         if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
            szOutput[strlen(szOutput)-1] = 0;
         strcat(szBuffer, ", ruby_capture_veye: ");
         strcat(szBuffer, szOutput);
         #endif

         strcat(szBuffer, " #");     

         strcat(szBuffer, "USB Devices: #");
         hw_execute_bash_command_raw("lsusb", szOutput);
         strcat(szBuffer, szOutput);
         /*
         strcat(szBuffer, " #Loaded Modules: #");

         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

         hw_execute_bash_command_raw("lsmod", szOutput);
         int len = strlen(szOutput);
         for( int i=0; i<len; i++ )
         {
            if ( szOutput[i] == 10 )
               szOutput[i] = '#';
            if ( szOutput[i] == 13 )
               szOutput[i] = ' ';
         }
         strcat(szBuffer, szOutput);
         */
      }

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      if ( strlen(szBuffer) > MAX_PACKET_PAYLOAD )
      {
         log_softerror_and_alarm("Modules info string is bigger than max allowed size: %d / %d; truncating it.", strlen(szBuffer), MAX_PACKET_PAYLOAD);
         szBuffer[MAX_PACKET_PAYLOAD-1] = 0;
      }
      log_line("Sending back modules info, %d bytes", strlen(szBuffer)+1);
      log_line("Sending back string: (%s)", szBuffer);
      setCommandReplyBuffer((u8*)szBuffer, strlen(szBuffer)+1);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, replyDelay);
      return true;

   }

   if ( uCommandType == COMMAND_ID_GET_USB_INFO )
   {
      char szComm[128];
      sprintf(szComm, "rm -rf %s/tmp_usb_info.tar 2>/dev/null", FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);
      sprintf(szComm, "rm -rf %s/tmp_usb_info.txt 2>/dev/null", FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);

      char szBuffUSB[6000];
      char szBuff2[3000];
      szBuffUSB[0] = 0;
      szBuff2[0] = 0;
      hw_execute_bash_command_raw("lsusb", szBuffUSB);
      hw_execute_bash_command_raw("lsusb -t", szBuff2);
      int iLen1 = strlen(szBuffUSB);
      int iLen2 = strlen(szBuff2);
      if ( iLen1 > 3000 )
         iLen1 = 3000;
      strcat(szBuffUSB, "\nTree:\n");
      strcat(szBuffUSB, szBuff2);
      iLen1 = strlen(szBuffUSB);

      char szFile[MAX_FILE_PATH_SIZE];
      strcpy(szFile, FOLDER_RUBY_TEMP);
      strcat(szFile, "tmp_usb_info.txt");
      FILE* fd = fopen(szFile, "wb");
      if ( NULL == fd )
      {
          sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
          return true;
      }
      fwrite(szBuffUSB, 1, iLen1, fd );
      fclose(fd);

      sprintf(szComm, "rm -rf %s/tmp_usb_info.tar* 2>/dev/null", FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);
      sprintf(szComm, "tar -C %s -cf %s/tmp_usb_info.tar tmp_usb_info.txt 2>&1", FOLDER_RUBY_TEMP, FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);
      sprintf(szComm, "gzip %s/tmp_usb_info.tar 2>&1", FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);

      strcpy(szFile, FOLDER_RUBY_TEMP);
      strcat(szFile, "tmp_usb_info.tar.gz");
      fd = fopen(szFile, "rb");
      if ( NULL == fd )
      {
          sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
          sprintf(szComm, "rm -rf %s/tmp_usb_info.tar* 2>/dev/null", FOLDER_RUBY_TEMP);
          hw_execute_bash_command(szComm, NULL);
          sprintf(szComm, "rm -rf %s/tmp_usb_info.txt 2>/dev/null", FOLDER_RUBY_TEMP);
          hw_execute_bash_command(szComm, NULL);
          return true;       
      }
      szBuff2[0] = 0;
      iLen2 = fread(szBuff2, 1, 3000, fd);
      fclose(fd);

      if ( iLen2 <= 0 )
      {
          sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
          sprintf(szComm, "rm -rf %s/tmp_usb_info.tar* 2>/dev/null", FOLDER_RUBY_TEMP);
          hw_execute_bash_command(szComm, NULL);
          sprintf(szComm, "rm -rf %s/tmp_usb_info.txt 2>/dev/null", FOLDER_RUBY_TEMP);
          hw_execute_bash_command(szComm, NULL);
          return true;
      }

      log_line("Send reply of %d bytes to get USB info command.", iLen2);
      setCommandReplyBuffer((u8*)szBuff2, iLen2);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 1, 0);
      return true;       
   }

   if ( uCommandType == COMMAND_ID_GET_USB_INFO2 )
   {
      char szComm[512];
      sprintf(szComm, "rm -rf %s/tmp_usb_info.tar 2>/dev/null", FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);
      sprintf(szComm, "rm -rf %s/tmp_usb_info.txt 2>/dev/null", FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);

      char szBuffUSB[3000];
      szBuffUSB[0] = 0;
    
      DIR *d;
      struct dirent *dir;
      char szFile[512];
      char szOutput[1024];
      d = opendir("/sys/bus/usb/devices/");
      if (d)
      {
         while ((dir = readdir(d)) != NULL)
         {
            int iLen = strlen(dir->d_name);
            if ( iLen < 3 )
               continue;
            snprintf(szFile, sizeof(szFile)/sizeof(szFile[0]), "/sys/bus/usb/devices/%s", dir->d_name);
            snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cat /sys/bus/usb/devices/%s/uevent | grep DRIVER", dir->d_name);
            szOutput[0] = 0;
            hw_execute_bash_command(szComm, szOutput);
            strcat(szBuffUSB, dir->d_name);
            strcat(szBuffUSB, " :  ");
            strcat(szBuffUSB, szOutput);

            for( int i=0; i<hardware_get_radio_interfaces_count()+1; i++ )
            {
               szOutput[0] = 0;
               sprintf(szComm, "cat /sys/bus/usb/devices/%s/net/wlan%d/uevent 2>/dev/null | grep DEVTYPE=wlan", dir->d_name, i);
               hw_execute_bash_command_silent(szComm, szOutput);
               if ( strlen(szOutput) > 0 )
               {
                  log_line("Accessed %s", szComm);
                  szOutput[0] = 0;
                  sprintf(szComm, "cat /sys/bus/usb/devices/%s/net/wlan%d/uevent 2>/dev/null | grep INTERFACE", dir->d_name, i);
                  hw_execute_bash_command_silent(szComm, szOutput);
                  if ( strlen(szOutput) > 0 )
                  {
                     strcat(szBuffUSB, ", ");
                     strcat(szBuffUSB, szOutput);
                     iLen = strlen(szBuffUSB);
                     if ( szBuffUSB[iLen-1] == 10 || szBuffUSB[iLen-1] == 13 )
                        szBuffUSB[iLen-1] = 0;
                     iLen = strlen(szBuffUSB);
                     if ( szBuffUSB[iLen-1] == 10 || szBuffUSB[iLen-1] == 13 )
                        szBuffUSB[iLen-1] = 0;
                  }
                  szOutput[0] = 0;
                  sprintf(szComm, "cat /sys/bus/usb/devices/%s/net/wlan%d/uevent 2>/dev/null | grep IFINDEX", dir->d_name, i);
                  hw_execute_bash_command_silent(szComm, szOutput);
                  if ( strlen(szOutput) > 0 )
                  {
                     strcat(szBuffUSB, ", ");
                     strcat(szBuffUSB, szOutput);
                     iLen = strlen(szBuffUSB);
                     if ( szBuffUSB[iLen-1] == 10 || szBuffUSB[iLen-1] == 13 )
                        szBuffUSB[iLen-1] = 0;
                     iLen = strlen(szBuffUSB);
                     if ( szBuffUSB[iLen-1] == 10 || szBuffUSB[iLen-1] == 13 )
                        szBuffUSB[iLen-1] = 0;
                  }

                  szOutput[0] = 0;
                  sprintf(szComm, "cat /sys/bus/usb/devices/%s/uevent 2>/dev/null | grep PRODUCT", dir->d_name);
                  hw_execute_bash_command_silent(szComm, szOutput);
                  if ( strlen(szOutput) > 0 )
                  {
                     strcat(szBuffUSB, ", ");
                     strcat(szBuffUSB, szOutput);
                     iLen = strlen(szBuffUSB);
                     if ( szBuffUSB[iLen-1] == 10 || szBuffUSB[iLen-1] == 13 )
                        szBuffUSB[iLen-1] = 0;
                     iLen = strlen(szBuffUSB);
                     if ( szBuffUSB[iLen-1] == 10 || szBuffUSB[iLen-1] == 13 )
                        szBuffUSB[iLen-1] = 0;
                  }
                  break;
               }
            }

            iLen = strlen(szBuffUSB);
            if ( iLen > 0 )
            if ( szBuffUSB[iLen-1] == 10 || szBuffUSB[iLen-1] == 13 )
               szBuffUSB[iLen-1] = 0;

            iLen = strlen(szBuffUSB);
            if ( iLen > 0 )
            if ( szBuffUSB[iLen-1] == 10 || szBuffUSB[iLen-1] == 13 )
               szBuffUSB[iLen-1] = 0;

            strcat(szBuffUSB, "\n");
         }
         closedir(d);
      }

      if ( 0 == strlen(szBuffUSB) )
         strcpy(szBuffUSB, "No info available.");
      int iLen = strlen(szBuffUSB);
      strcpy(szFile, FOLDER_RUBY_TEMP);
      strcat(szFile, "tmp_usb_info2.txt");
      FILE* fd = fopen(szFile, "wb");
      if ( NULL == fd )
      {
          sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
          return true;
      }
      fwrite(szBuffUSB, 1, iLen, fd );
      fclose(fd);

      sprintf(szComm, "rm -rf %s/tmp_usb_info2.tar* 2>/dev/null", FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);
      sprintf(szComm, "tar -C %s -cf %s/tmp_usb_info2.tar tmp_usb_info2.txt 2>&1", FOLDER_RUBY_TEMP, FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);
      sprintf(szComm, "gzip %s/tmp_usb_info2.tar 2>&1", FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);

      strcpy(szFile, FOLDER_RUBY_TEMP);
      strcat(szFile, "tmp_usb_info2.tar.gz");
      fd = fopen(szFile, "rb");
      if ( NULL == fd )
      {
          sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
          sprintf(szComm, "rm -rf %s/tmp_usb_info2.tar* 2>/dev/null", FOLDER_RUBY_TEMP);
          hw_execute_bash_command(szComm, NULL);
          sprintf(szComm, "rm -rf %s/tmp_usb_info2.txt 2>/dev/null", FOLDER_RUBY_TEMP);
          hw_execute_bash_command(szComm, NULL);
          return true;       
      }
      szBuffUSB[0] = 0;
      iLen = fread(szBuffUSB, 1, 3000, fd);
      fclose(fd);

      if ( iLen <= 0 )
      {
          sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
          sprintf(szComm, "rm -rf %s/tmp_usb_info2.tar* 2>/dev/null", FOLDER_RUBY_TEMP);
          hw_execute_bash_command(szComm, NULL);
          sprintf(szComm, "rm -rf %s/tmp_usb_info2.txt 2>/dev/null", FOLDER_RUBY_TEMP);
          hw_execute_bash_command(szComm, NULL);
          return true;
      }

      log_line("Send reply of %d bytes to get USB info command.", iLen);
      setCommandReplyBuffer((u8*)szBuffUSB, iLen);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 1, 0);
      return true;       
   }

   if ( uCommandType == COMMAND_ID_GET_MEMORY_INFO )
   {
      u32 resp[256];
      resp[0] = 0;
      resp[1] = 0;
      if ( 1 == hw_execute_bash_command_raw("free -m  | grep Mem", szBuff) )
      {
         char szTemp[1024];
         long lt, lu, lf;
         sscanf(szBuff, "%s %ld %ld %ld", szTemp, &lt, &lu, &lf);
         resp[0] = (u32)lt;
         resp[1] = (u32)lf;

         szTemp[0] = 0;
         char szComm[128];
         sprintf(szComm, "du -h %s", FOLDER_LOGS);
         hw_execute_bash_command_raw(szComm, szTemp);
         for( int i=0; i<(int)strlen(szTemp); i++ )
         {
           if ( isspace(szTemp[i]) )
           {
              szTemp[i] = 0;
              break;
           }
         }
         int iSize = strlen(szTemp)+1;
         memcpy((u8*)&resp[2], szTemp, iSize);
         setCommandReplyBuffer((u8*)resp, 2*sizeof(u32) + iSize);
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, replyDelay);
      }
      else
      {
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
      }
      return true;
   }
   if ( uCommandType == COMMAND_ID_GET_CPU_PROCS_INFO )
   {
      static char s_szBufferDbg[4096];
      if ( 0 == pPHC->command_param )
      {
         memset(s_szBufferDbg, 0, 4096);
         #if defined (HW_PLATFORM_OPENIPC_CAMERA)
         hw_execute_bash_command_timeout("/usr/sbin/ruby_start -dbgproc -a", s_szBufferDbg, 10000);
         #else
         hw_execute_bash_command_timeout("./ruby_start -dbgproc -a", s_szBufferDbg, 10000);
         #endif
      }
      char szBuffer[4096];
      memcpy(szBuffer, s_szBufferDbg, 4096);

      u8* pTmp = (u8*)&(szBuffer[0]);
      int iLen = strlen(szBuffer);
      if ( 0 == pPHC->command_param )
      {
         if ( iLen > 1100 )
         {
            iLen = 1100;
            szBuffer[iLen] = 0;
         }
      }
      else
      {
         pTmp = (u8*)&(szBuffer[1100]);
         iLen = strlen(szBuffer) - 1100;
         if ( iLen < 0 )
         {
            szBuffer[1100] = 0;
            iLen = 0;
         }
      }
      log_line("Sending back CPU procs info, %d bytes for segment %d", iLen+1, pPHC->command_param);
      setCommandReplyBuffer(pTmp, iLen+1);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, replyDelay);
      return true;
   }

   if ( uCommandType == COMMAND_ID_GET_CPU_INFO )
   {
      char szBuffer[2048];
      char szOutput[1500];
      szBuffer[0] = 0;
      
      FILE* fd = NULL;
      int iMajor = 0;
      int iMinor = 0;
      get_Ruby_BaseVersion(&iMajor, &iMinor);
      if ( 0 != iMajor )
      {
         strcat(szBuffer, "Ruby base version: ");
         if ( iMinor >= 10 )
            iMinor /= 10;
         sprintf(szOutput, "%d.%d", iMajor, iMinor);
         strcat(szBuffer, szOutput);
         strcat(szBuffer, "; ");
      }
      else
      {
         fd = try_open_base_version_file(NULL);
         if ( NULL != fd )
         {
            szOutput[0] = 0;
            if ( 1 == fscanf(fd, "%s", szOutput) )
            {
               strcat(szBuffer, "Ruby base version: ");
               strcat(szBuffer, szOutput);
               strcat(szBuffer, "; ");
            }
            fclose(fd);
         }
      }
      
      char szFile[128];
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_INFO_LAST_UPDATE);
      fd = fopen(szFile, "r");
      if ( NULL != fd )
      {
         szOutput[0] = 0;
         if ( 1 == fscanf(fd, "%s", szOutput) )
         {
            strcat(szBuffer, "last update: ");
            strcat(szBuffer, szOutput);
            strcat(szBuffer, "+");
         }
         else
         {
            strcat(szBuffer, "Ruby last update: none");
            strcat(szBuffer, "+");
         }
         fclose(fd);
      }
      else
      {
         strcat(szBuffer, "Ruby last update: none");
         strcat(szBuffer, "+");
      }

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      hw_get_process_priority("ruby_rt_vehicle", szOutput);
      strcat(szBuffer, szOutput);
      strcat(szBuffer, "+");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      hw_get_process_priority("ruby_tx_telemetry", szOutput);
      strcat(szBuffer, szOutput);
      strcat(szBuffer, "+");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      #if defined (HW_PLATFORM_RASPBERRY)
      if ( g_pCurrentModel->isActiveCameraVeye() )
         hw_get_process_priority(VIDEO_RECORDER_COMMAND_VEYE_SHORT_NAME, szOutput);
      else
         hw_get_process_priority(VIDEO_RECORDER_COMMAND, szOutput);
      strcat(szBuffer, szOutput);
      strcat(szBuffer, "+");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
      #endif
        
      #if defined (HW_PLATFORM_OPENIPC_CAMERA)
      hw_get_process_priority(VIDEO_RECORDER_COMMAND, szOutput);
      strcat(szBuffer, szOutput);
      strcat(szBuffer, "+");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
      #endif

      hw_execute_bash_command_raw("nproc --all", szOutput);
      szOutput[strlen(szOutput)-1] = 0;
      strcat(szBuffer, "CPU Cores: ");
      strcat(szBuffer, szOutput);
      strcat(szBuffer, ", ");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      #ifdef HW_PLATFORM_RASPBERRY
      int speed = hardware_get_cpu_speed();
      sprintf(szOutput, "%d Mhz; ", speed);
      strcat(szBuffer, szOutput);

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
      speed = hardware_get_gpu_speed();
      sprintf(szOutput, "GPU: %d Mhz+", speed);
      strcat(szBuffer, szOutput);

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      hw_execute_bash_command_raw("vcgencmd measure_clock h264", szOutput);
      szOutput[0] = 'F';
      szOutput[strlen(szOutput)-1] = 0;
      strcat(szBuffer, "H264: ");
      strcat(szBuffer, szOutput);
      strcat(szBuffer, " Hz+");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      hw_execute_bash_command_raw("vcgencmd measure_clock isp", szOutput);
      szOutput[0] = 'F';
      szOutput[strlen(szOutput)-1] = 0;
      strcat(szBuffer, "ISP: ");
      strcat(szBuffer, szOutput);
      strcat(szBuffer, " Hz+");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      hw_execute_bash_command_raw("vcgencmd measure_volts core", szOutput);
      szOutput[strlen(szOutput)-1] = 0;
      if ( szOutput[strlen(szOutput)-1] == 10 || 
         szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;

      strcat(szBuffer, "CPU: ");
      strcat(szBuffer, szOutput);
      strcat(szBuffer, "; ");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      hw_execute_bash_command_raw("vcgencmd measure_volts sdram_c", szOutput);
      szOutput[strlen(szOutput)-1] = 0;
      strcat(szBuffer, "SDRAM C: ");
      strcat(szBuffer, szOutput);
      strcat(szBuffer, ", ");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      hw_execute_bash_command_raw("vcgencmd measure_volts sdram_i", szOutput);
      szOutput[strlen(szOutput)-1] = 0;
      strcat(szBuffer, "I: ");
      strcat(szBuffer, szOutput);
      strcat(szBuffer, ", ");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      hw_execute_bash_command_raw("vcgencmd measure_volts sdram_p", szOutput);
      szOutput[strlen(szOutput)-1] = 0;
      strcat(szBuffer, "P: ");
      strcat(szBuffer, szOutput);
      strcat(szBuffer, "+");
      #endif

      sprintf(szOutput, "Avg/Max loops (ms): rx_commands: %u/%u;", g_pProcessStats->uAverageLoopTimeMs, g_pProcessStats->uMaxLoopTimeMs);
      strcat(szBuffer, szOutput);
      shared_mem_process_stats* pProcessStats = NULL;
      pProcessStats = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_ROUTER_TX);
      if ( NULL != pProcessStats )
      {
         sprintf(szOutput, " router: %u/%u;", pProcessStats->uAverageLoopTimeMs, pProcessStats->uMaxLoopTimeMs);
         strcat(szBuffer, szOutput);
         shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_ROUTER_TX, pProcessStats);
      }
      else
      {
         strcat(szBuffer, " router: N/A;");
      }

      pProcessStats = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_TELEMETRY_TX);
      if ( NULL != pProcessStats )
      {
         sprintf(szOutput, " tx_telemetry: %u/%u;", pProcessStats->uAverageLoopTimeMs, pProcessStats->uMaxLoopTimeMs);
         strcat(szBuffer, szOutput);
         shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_ROUTER_TX, pProcessStats);
      }
      else
      {
         strcat(szBuffer, " tx_telemetry: N/A;");
      }

      pProcessStats = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_RC_RX);
      if ( NULL != pProcessStats )
      {
         sprintf(szOutput, " rx_rc: %u/%u;", pProcessStats->uAverageLoopTimeMs, pProcessStats->uMaxLoopTimeMs);
         strcat(szBuffer, szOutput);
         shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_ROUTER_TX, pProcessStats);
      }
      else
      {
         strcat(szBuffer, " rx_rc: N/A;");
      }

      strcat(szBuffer, "+");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      log_line("Sending back CPU info, %d bytes", strlen(szBuffer)+1);
      setCommandReplyBuffer((u8*)szBuffer, strlen(szBuffer)+1);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, replyDelay);
      return true;
   }

   if ( uCommandType == COMMAND_ID_ROTATE_RADIO_LINKS )
   {
      log_line("Received command to rotate radio links");

      for( int i=0; i<5; i++ )
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 10);

      // Tell router to reload model and do the model changes
      signalReloadModel(MODEL_CHANGED_ROTATED_RADIO_LINKS, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SWAP_RADIO_INTERFACES )
   {
      log_line("Received command to swap high capacity radio interfaces");
      if ( ! g_pCurrentModel->canSwapEnabledHighCapacityRadioInterfaces() )
      {
         for( int i=0; i<10; i++ )
            sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 10);
      }
      else
      {
         for( int i=0; i<20; i++ )
            sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 30);

         // Tell router to reload model and do the model changes
         signalReloadModel(MODEL_CHANGED_SWAPED_RADIO_INTERFACES, 0);
      }
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_VEHICLE_TYPE )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      g_pCurrentModel->vehicle_type &= MODEL_FIRMWARE_MASK;
      g_pCurrentModel->vehicle_type |= pPHC->command_param & MODEL_TYPE_MASK;
      saveCurrentModel();
      signalReloadModel(MODEL_CHANGED_GENERIC, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_VEHICLE_BOARD_TYPE )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      g_pCurrentModel->hwCapabilities.uBoardType &= ~(u32)BOARD_SUBTYPE_MASK;
      g_pCurrentModel->hwCapabilities.uBoardType |= pPHC->command_param & BOARD_SUBTYPE_MASK;
      if ( (g_pCurrentModel->hwCapabilities.uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_SIGMASTAR_338Q )
      if ( ((g_pCurrentModel->hwCapabilities.uBoardType & BOARD_SUBTYPE_MASK) >> BOARD_SUBTYPE_SHIFT) == BOARD_SUBTYPE_OPENIPC_AIO_EMAX_MINI )
      {
         g_pCurrentModel->radioInterfacesParams.interface_card_model[0] = -CARD_MODEL_RTL8812AU_AF1;
      }
      g_pCurrentModel->resetAudioParams();
      saveCurrentModel();
      signalReloadModel(MODEL_CHANGED_AUDIO_PARAMS, 0);
      sendControlMessage(PACKET_TYPE_LOCAL_CONTROL_VEHICLE_APPLY_ALL_VIDEO_SETTINGS, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_TEMPERATURE_THRESHOLD )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      g_pCurrentModel->hwCapabilities.uHWFlags &= ~(0x0000FF00);
      g_pCurrentModel->hwCapabilities.uHWFlags |= ((pPHC->command_param & 0xFF) << 8);
      saveCurrentModel();
      signalReloadModel(MODEL_CHANGED_GENERIC, 0);
   }

   if ( uCommandType == COMMAND_ID_SET_PIT_AUTO_TX_POWERS_FLAGS )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      g_pCurrentModel->radioInterfacesParams.uFlagsRadioInterfaces = (pPHC->command_param & 0xFF);
      g_pCurrentModel->hwCapabilities.uHWFlags &= ~(0x0000FF00);
      g_pCurrentModel->hwCapabilities.uHWFlags |= (pPHC->command_param & 0xFF00);

      if ( pPHC->command_param & (((u32)(0x01)) << 16) )
         g_pCurrentModel->radioInterfacesParams.iAutoVehicleTxPower = 1;
      else
         g_pCurrentModel->radioInterfacesParams.iAutoVehicleTxPower = 0;

      if ( pPHC->command_param & (((u32)(0x01)) << 17) )
         g_pCurrentModel->radioInterfacesParams.iAutoControllerTxPower = 1;
      else
         g_pCurrentModel->radioInterfacesParams.iAutoControllerTxPower = 0;
      
      saveCurrentModel();
      signalReloadModel(MODEL_CHANGED_GENERIC, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_RADIO_INTERFACE_CAPABILITIES )
   {
      int iInterface = (int)(pPHC->command_param & 0xFF);
      if ( (iInterface >= 0) && (iInterface < g_pCurrentModel->radioInterfacesParams.interfaces_count) )
      {
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
         g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iInterface] &= 0xFF000000;
         g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iInterface] |= ((pPHC->command_param >> 8) & 0xFFFFFF);
         saveCurrentModel();
         signalReloadModel(MODEL_CHANGED_GENERIC,0);
         return true;
      }
      sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_RADIO_LINKS_FLAGS )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags = pPHC->command_param;
      saveCurrentModel();
      signalReloadModel(MODEL_CHANGED_GENERIC,0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_CAMERA_PARAMETERS )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);

      int nCamera = uCommandParam;
      if ( (nCamera < 0) || (nCamera >= g_pCurrentModel->iCameraCount) )
         return true;

      type_camera_parameters* pcpt = (type_camera_parameters*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      memcpy(&(g_pCurrentModel->camera_params[nCamera]), pcpt, sizeof(type_camera_parameters));
      saveCurrentModel();

      if ( !hardware_hasCamera() )
         return true;

      _signalCameraParametersChanged(nCamera);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_CAMERA_PROFILE )
   {
      if ( pPHC->command_param >= MODEL_CAMERA_PROFILES )
      {
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
         return true;
      }
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      int iOldCamProfileIndex = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;
      g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile = pPHC->command_param;
      log_line("Switched camera number %d from profile %d to profile %d.", g_pCurrentModel->iCurrentCamera+1, iOldCamProfileIndex, g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile);
      saveCurrentModel();
      if ( ! g_pCurrentModel->hasCamera() )
         return true;
      
      _signalCameraParametersChanged(g_pCurrentModel->iCurrentCamera);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_CURRENT_CAMERA )
   {
      int iCam = (int)pPHC->command_param;
      if ( (iCam < 0) || (iCam >= g_pCurrentModel->iCameraCount ) )
      {
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
         return true;         
      }
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);

      g_pCurrentModel->iCurrentCamera = iCam;
      saveCurrentModel();
      if ( !hardware_hasCamera() )
         return true;
      _signalCameraParametersChanged(iCam);
     
      return true;
   }

   if ( uCommandType == COMMAND_ID_FORCE_CAMERA_TYPE )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      int iCamType = (int)pPHC->command_param;      
      g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType = iCamType;
      saveCurrentModel();
      signalReloadModel(0, 0);
      sendControlMessage(PACKET_TYPE_LOCAL_CONTROL_FORCE_CAMERA_TYPE, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_UPLOAD_CALIBRATION_FILE )
   {
      process_calibration_file_segment(pPHC->command_param, pBuffer, length);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_RADIO_LINK_FREQUENCY )
   {
      u32 uLinkIndex = ((pPHC->command_param)>>24) & 0xFF;
      u32 uNewFreq = (pPHC->command_param) & 0xFFFFFF;
      log_line("Received command to change frequency on radio link %u to %s", uLinkIndex+1, str_format_frequency(uNewFreq));

      bool freqIsOk = false;
      if ( uLinkIndex < (u32)g_pCurrentModel->radioLinksParams.links_count )
      {
         for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
            if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == (int)uLinkIndex )
            if ( hardware_radioindex_supports_frequency(i, uNewFreq) )
               freqIsOk = true;
      }

      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == (int)uLinkIndex )
      {
         log_softerror_and_alarm("Tried to change link frequency (to %s) for a relay link (radio link %u).", str_format_frequency(uNewFreq), uLinkIndex+1);
         freqIsOk = false;
      }
      
      if ( ! freqIsOk )
      {
         for( int i=0; i<20; i++ )
            sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, i+5);
         log_line("Can't do the change.");
         log_current_full_radio_configuration(g_pCurrentModel);
         return true;
      }

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      for( int i=0; i<20; i++ )
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, i+5);

      hardware_sleep_ms(40);

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      log_line("Switching vehicle radio link %u to %s.", uLinkIndex+1, str_format_frequency(uNewFreq));
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == (int)uLinkIndex )
         {
            if ( ! g_pCurrentModel->radioLinkIsSiKRadio(i) )
               radio_utils_set_interface_frequency(g_pCurrentModel, i, (int)uLinkIndex, uNewFreq, g_pProcessStats, 0);
            g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[i] = uNewFreq;
         }
      }
      hardware_save_radio_info();

      g_pCurrentModel->radioLinksParams.link_frequency_khz[uLinkIndex] = uNewFreq;

      log_current_full_radio_configuration(g_pCurrentModel);

      saveCurrentModel();
      hardware_sleep_ms(20);

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
      
      if ( g_pCurrentModel->radioLinkIsSiKRadio(uLinkIndex) )
         signalReloadModel(MODEL_CHANGED_SIK_FREQUENCY, 0);
      else
         signalReloadModel(MODEL_CHANGED_FREQUENCY, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_RC_CAMERA_PARAMS )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      g_pCurrentModel->camera_rc_channels = pPHC->command_param;
      saveCurrentModel();
      signalReloadModel(0, 0);
      return true;
   }


   if ( uCommandType == COMMAND_ID_SET_FUNCTIONS_TRIGGERS_PARAMS )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      type_functions_parameters* params = (type_functions_parameters*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      memcpy(&(g_pCurrentModel->functions_params), params, sizeof(type_functions_parameters));

      saveCurrentModel();
      signalReloadModel(0, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_SIK_PACKET_SIZE )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      g_pCurrentModel->radioLinksParams.iSiKPacketSize = (int)pPHC->command_param;
      signalReloadModel(MODEL_CHANGED_SIK_PACKET_SIZE, (u8) g_pCurrentModel->radioLinksParams.iSiKPacketSize);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_RADIO_CARD_MODEL )
   {
      int cardIndex = (pPHC->command_param) & 0xFF;
      u32 uReqCardModel = (((pPHC->command_param) >> 8) & 0xFF);
      log_line("Recv request to set radio interface %d card model to %u", cardIndex+1, uReqCardModel);
      if ( (cardIndex < 0) || (cardIndex >= g_pCurrentModel->radioInterfacesParams.interfaces_count) )
      {
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
         return true;
      }
      if ( uReqCardModel == 0xFF )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(cardIndex);
         if ( NULL != pRadioHWInfo )
            uReqCardModel = pRadioHWInfo->iCardModel;
         g_pCurrentModel->radioInterfacesParams.interface_card_model[cardIndex] = (int)uReqCardModel;
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, uReqCardModel, 0);
      }
      else
      {
         g_pCurrentModel->radioInterfacesParams.interface_card_model[cardIndex] = -(int)uReqCardModel;
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, uReqCardModel, 0);
      }
      log_line("Did set radio interface %d card model to %d (%s)",
          cardIndex+1, g_pCurrentModel->radioInterfacesParams.interface_card_model[cardIndex],
          str_get_radio_card_model_string(g_pCurrentModel->radioInterfacesParams.interface_card_model[cardIndex]) );
      saveCurrentModel();
      signalReloadModel(0, 0);
      return true;
   }


   if ( uCommandType == COMMAND_ID_SET_RADIO_LINK_CAPABILITIES )
   {
      int linkIndex = (pPHC->command_param) & 0xFF;
      u32 flags = (pPHC->command_param) >> 8;
      char szBuffC[128];
      str_get_radio_capabilities_description(flags, szBuffC);
      log_line("Set radio link nb.%d capabilities flags to %d: %s", linkIndex+1, flags, szBuffC);
      if ( linkIndex < 0 || linkIndex >= g_pCurrentModel->radioLinksParams.links_count )
      {
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
         return true;
      }
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);

      bool bOnlyVideoDataChanged = false;
      if ( (flags & (~(RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA)) ) ==
           (g_pCurrentModel->radioLinksParams.link_capabilities_flags[linkIndex] & (~(RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA)) ) )
         bOnlyVideoDataChanged = true;

      g_pCurrentModel->radioLinksParams.link_capabilities_flags[linkIndex] = flags;
      saveCurrentModel();

      // If only video/data flags changed, do not reinitialize radio links
      if ( ! bOnlyVideoDataChanged )
         signalReinitializeRouterRadioLinks();
      signalReloadModel(MODEL_CHANGED_RADIO_LINK_CAPABILITIES, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_MODEL_FLAGS )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      bool bHadServiceLog = (g_pCurrentModel->uModelFlags & MODEL_FLAG_USE_LOGER_SERVICE)?true:false;
      if ( g_pCurrentModel->uModelFlags & MODEL_FLAG_DISABLE_ALL_LOGS )
         bHadServiceLog = false;
      g_pCurrentModel->uModelFlags = pPHC->command_param;
      if ( (g_pCurrentModel->uModelFlags & MODEL_FLAG_USE_LOGER_SERVICE) && ! (g_pCurrentModel->uModelFlags & MODEL_FLAG_DISABLE_ALL_LOGS) )
      {
         if ( ! bHadServiceLog )
         {
            char szC[128];
            sprintf(szC, "touch %s%s", FOLDER_CONFIG, LOG_USE_PROCESS);
            hw_execute_bash_command(szC,NULL);
         }
      }
      else
      {
         if ( bHadServiceLog )
         {
            char szC[128];
            sprintf(szC, "rm -rf %s%s", FOLDER_CONFIG, LOG_USE_PROCESS);
            hw_execute_bash_command(szC,NULL);
         }
      }

      saveCurrentModel();
      signalReloadModel(0, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_RXTX_SYNC_TYPE )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      g_pCurrentModel->rxtx_sync_type = pPHC->command_param;
      saveCurrentModel();
      signalReloadModel(0, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_RESET_RADIO_LINK )
   {
      u32 linkIndex = pPHC->command_param;
      if ( (linkIndex < 0) || (linkIndex >= (u32)g_pCurrentModel->radioLinksParams.links_count) )
      {
         log_error_and_alarm("Invalid link index received in command: link %d (%d radio links on vehicle)", (int)linkIndex+1, g_pCurrentModel->radioLinksParams.links_count);
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
         return true;
      }

      g_pCurrentModel->resetRadioLinkDataRatesAndFlags(linkIndex);
      int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(linkIndex);
      bool bIsAtheros = false;
      if ( iRadioInterfaceId >= 0 )
      {
         if ( (g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[iRadioInterfaceId] & 0xFF) == RADIO_TYPE_ATHEROS )
            bIsAtheros = true;
         if ( (g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[iRadioInterfaceId] & 0xFF) == RADIO_TYPE_RALINK )
            bIsAtheros = true; 
      }
      if ( bIsAtheros )
      {
         g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[linkIndex] = DEFAULT_RADIO_DATARATE_VIDEO_ATHEROS;
         g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[linkIndex] = DEFAULT_RADIO_DATARATE_VIDEO_ATHEROS;
      }
      g_pCurrentModel->validateRadioSettings();
      saveCurrentModel();
      signalReloadModel(MODEL_CHANGED_RESET_RADIO_LINK, linkIndex);

      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 2);
      for( int i=0; i<5; i++ )
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 5);
      return true;
   }
   
   if ( uCommandType == COMMAND_ID_SET_RADIO_LINK_FLAGS )
   {
      if ( iParamsLength != (int)(2*sizeof(u32)+2*sizeof(int)) )
      {
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED_INVALID_PARAMS, 0, 0);
         return true;
      }
      u32* pData = (u32*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      u32 linkIndex = *pData;
      pData++;
      u32 linkFlags = *pData;
      pData++;
      int* piData = (int*)pData;
      int datarateVideo = *piData;
      piData++;
      int datarateData = *piData;

      if ( (linkIndex < 0) || (linkIndex >= (u32)g_pCurrentModel->radioLinksParams.links_count) )
      {
         log_error_and_alarm("Invalid link index received in command: link %d (%d radio links on vehicle)", (int)linkIndex+1, g_pCurrentModel->radioLinksParams.links_count);
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
         return true;
      }

      char szBuffR[256];
      str_get_radio_frame_flags_description(g_pCurrentModel->radioLinksParams.link_radio_flags[linkIndex], szBuffR); 
      log_line("Current radio link flags for radio link %d: %s, datarates: %d/%d", (int)linkIndex+1, szBuffR, g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[linkIndex], g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[linkIndex]);

      str_get_radio_frame_flags_description(linkFlags, szBuffR); 
      log_line("Received new radio link flags for radio link %d: %s, datarates: %d/%d", (int)linkIndex+1, szBuffR, datarateVideo, datarateData);

      if ( g_pCurrentModel->radioLinkIsWiFiRadio(linkIndex) )
      {
         g_TimeLastSetRadioLinkFlagsStartOperation = g_TimeNow;
         s_bWaitForRadioFlagsChangeConfirmation = true;
         s_iRadioLinkIdChangeConfirmation = (int)linkIndex;
         memcpy(&s_LastGoodRadioLinksParams, &(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));
      }
 
      g_pCurrentModel->radioLinksParams.link_radio_flags[linkIndex] = linkFlags;
      g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[linkIndex] = datarateVideo;
      g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[linkIndex] = datarateData;

      g_pCurrentModel->validateRadioSettings();
      saveCurrentModel();
      signalReloadModel(MODEL_CHANGED_RADIO_LINK_FRAMES_FLAGS, linkIndex);

      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 2);
      for( int i=0; i<5; i++ )
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 5);

      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_RADIO_LINK_FLAGS_CONFIRMATION )
   {
      int linkId = (int)(pPHC->command_param);
      log_line("Received radio link flags change confirmation from controller for link %d.", linkId+1);

      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);

      g_TimeLastSetRadioLinkFlagsStartOperation = 0;
      s_bWaitForRadioFlagsChangeConfirmation = false;
      s_iRadioLinkIdChangeConfirmation = -1;

      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_RADIO_LINK_DATARATES )
   {
      int iRadioLink = pPHC->command_param;
      u8* pData = (u8*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      if ( (iRadioLink < 0) || (iRadioLink >= g_pCurrentModel->radioLinksParams.links_count) )
      {
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED_INVALID_PARAMS, 0, 0);
         return true;
      }
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);

      memcpy(&g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[iRadioLink], pData, sizeof(int));
      memcpy(&g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[iRadioLink], pData + sizeof(int), sizeof(int));
      memcpy(&g_pCurrentModel->radioLinksParams.uplink_datarate_video_bps[iRadioLink], pData + 2*sizeof(int), sizeof(int));
      memcpy(&g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[iRadioLink], pData + 3*sizeof(int), sizeof(int));

      log_line("Received new radio data rates for link %d: v: %d, d: %d, up v/d: %d/%d",
            iRadioLink+1,
            g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[iRadioLink],
            g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[iRadioLink],
            g_pCurrentModel->radioLinksParams.uplink_datarate_video_bps[iRadioLink],
            g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[iRadioLink]);

      saveCurrentModel();
      signalReloadModel(MODEL_CHANGED_RADIO_DATARATES, iRadioLink);
      return true;
   }

   if ( uCommandType == COMMAND_ID_RESET_ALL_TO_DEFAULTS )
   {
      for( int i=0; i<20; i++ )
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 50);
      hardware_sleep_ms(200);
      /*
      char szComm[256];
      #if defined (HW_PLATFORM_RASPBERRY) || defined (HW_PLATFORM_RADXA)
      if ( 0 < strlen(FOLDER_CONFIG) )
      {
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s*", FOLDER_CONFIG);
         hw_execute_bash_command(szComm, NULL);
      }
      if ( 0 < strlen(FOLDER_MEDIA) )
      {
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s*", FOLDER_MEDIA);
         hw_execute_bash_command(szComm, NULL);
      }
      if ( 0 < strlen(FOLDER_UPDATES) )
      {
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s*", FOLDER_UPDATES);
         hw_execute_bash_command(szComm, NULL);
      }
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "touch %s%s", FOLDER_CONFIG, LOG_USE_PROCESS);
      hw_execute_bash_command(szComm, NULL);
      #endif

      #if defined (HW_PLATFORM_OPENIPC_CAMERA)
      if ( 0 < strlen(FOLDER_CONFIG) )
      {
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s*", FOLDER_CONFIG);
         hw_execute_bash_command(szComm, NULL);
      }
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "touch %s%s", FOLDER_CONFIG, LOG_USE_PROCESS);
      hw_execute_bash_command(szComm, NULL);
      #endif
      */

      type_adaptive_metrics metrics;
      compute_adaptive_metrics(&metrics, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iAdaptiveAdjustmentStrength, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uAdaptiveWeights);
      log_adaptive_metrics(g_pCurrentModel, &metrics, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iAdaptiveAdjustmentStrength, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uAdaptiveWeights);

      g_pCurrentModel->resetAllSettingsKeepPairing(false);
      saveCurrentModel();

      compute_adaptive_metrics(&metrics, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iAdaptiveAdjustmentStrength, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uAdaptiveWeights);
      log_adaptive_metrics(g_pCurrentModel, &metrics, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iAdaptiveAdjustmentStrength, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uAdaptiveWeights);

      /*
      char szFileName[MAX_FILE_PATH_SIZE];
      strcpy(szFileName, FOLDER_CONFIG);
      strcat(szFileName, "reset_info.txt");
      FILE* fd = fopen(szFileName, "wt");
      if ( NULL != fd )
      {
         char szName[128];
         strcpy(szName, g_pCurrentModel->vehicle_name);
         if ( 0 == szName[0] )
            strcpy(szName, "*");
         for( int i=0; i<(int)strlen(szName); i++ )
         {
            if ( szName[i] <= ' ' )
               szName[i] = '-';
         }
         fprintf(fd, "%u %u %u %u %u %s\n",
           g_pCurrentModel->uVehicleId, g_pCurrentModel->uControllerId,
           g_pCurrentModel->radioLinksParams.link_frequency_khz[0],
           g_pCurrentModel->radioLinksParams.link_frequency_khz[1],
           g_pCurrentModel->radioLinksParams.link_frequency_khz[2],
           szName);
         fclose(fd);
      }
      */

      signalReboot(false);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_GPS_INFO )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      g_pCurrentModel->iGPSCount = (int) pPHC->command_param;
      saveCurrentModel();
      signalReloadModel(0, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_AUDIO_PARAMS )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      audio_parameters_t* params = (audio_parameters_t*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      bool bHasAudioDevice = g_pCurrentModel->audio_params.has_audio_device;
      memcpy(&(g_pCurrentModel->audio_params), params, sizeof(audio_parameters_t));
      g_pCurrentModel->audio_params.has_audio_device = bHasAudioDevice;
      saveCurrentModel();

      signalReloadModel(MODEL_CHANGED_AUDIO_PARAMS, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_VIDEO_PARAMETERS )
   {
      if ( iParamsLength != (int)(sizeof(video_parameters_t) + MAX_VIDEO_LINK_PROFILES * sizeof(type_video_link_profile)) )
      {
         log_softerror_and_alarm("Received video params size invalid (%d bytes received, expected %d bytes)",
            iParamsLength, (int)(sizeof(video_parameters_t) + MAX_VIDEO_LINK_PROFILES * sizeof(type_video_link_profile)));
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
         return true;
      }
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);

      log_line("Received command to set video params.");
      video_parameters_t* pNewVideoSettings = (video_parameters_t*)(pBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command));
      type_video_link_profile* pNewVideoProfiles = (type_video_link_profile*)(pBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command) + sizeof(video_parameters_t));
      g_pCurrentModel->logVideoSettingsDifferences(pNewVideoSettings, &(pNewVideoProfiles[pNewVideoSettings->iCurrentVideoProfile]));

      memcpy(&(g_pCurrentModel->video_params), pNewVideoSettings, sizeof(video_parameters_t));
      memcpy(&(g_pCurrentModel->video_link_profiles[0]), pNewVideoProfiles, MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile));

      camera_profile_parameters_t* pCameraParams = &g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile];
      if ( g_pCurrentModel->isRunningOnOpenIPCHardware() &&
           g_pCurrentModel->validate_fps_and_exposure_settings(pCameraParams, true))
      {
         log_line("Camera exposure (%d ms) was updated to accomodate the new video FPS value.", pCameraParams->iShutterSpeed);
      }

      saveCurrentModel();
      signalReloadModel(MODEL_CHANGED_VIDEO_PARAMETERS, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_OSD_PARAMS )
   {
      if ( iParamsLength != sizeof(osd_parameters_t) )
      {
         log_softerror_and_alarm("Received OSD params size invalid (%d bytes received, expected %d bytes)",
            iParamsLength, sizeof(osd_parameters_t));
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
         return true;
      }
      osd_parameters_t* pParams = (osd_parameters_t*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      //log_line("Received osd params %d bytes, expected %d bytes", iParamsLength, sizeof(osd_parameters_t));
      if ( (pParams->osd_layout_preset[g_pCurrentModel->osd_params.iCurrentOSDScreen] != OSD_PRESET_CUSTOM) && (pParams->osd_layout_preset[g_pCurrentModel->osd_params.iCurrentOSDScreen] != g_pCurrentModel->osd_params.osd_layout_preset[g_pCurrentModel->osd_params.iCurrentOSDScreen]) )
      {
         log_line("Set new layout %d on screen %d", pParams->osd_layout_preset[g_pCurrentModel->osd_params.iCurrentOSDScreen], g_pCurrentModel->osd_params.iCurrentOSDScreen);
         g_pCurrentModel->resetOSDScreenToLayout(g_pCurrentModel->osd_params.iCurrentOSDScreen, pParams->osd_layout_preset[g_pCurrentModel->osd_params.iCurrentOSDScreen]);
      }
      else
         memcpy(&g_pCurrentModel->osd_params, pParams, sizeof(osd_parameters_t));
      int nOSDIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;
      if ( nOSDIndex < 0 || nOSDIndex >= MODEL_MAX_OSD_SCREENS )
         nOSDIndex = 0;

      g_pCurrentModel->osd_params.uFlags &= ~(OSD_BIT_FLAGS_MUST_CHOOSE_PRESET);

      saveCurrentModel();
      log_line("Saved new OSD params, current layout: %d, enabled: %s", nOSDIndex, (g_pCurrentModel->osd_params.osd_flags2[nOSDIndex] & OSD_FLAG2_LAYOUT_ENABLED)?"yes":"no");
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      signalReloadModel(MODEL_CHANGED_OSD_PARAMS, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_ALARMS_PARAMS )
   {
      type_alarms_parameters* params = (type_alarms_parameters*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      memcpy(&g_pCurrentModel->alarms_params, params, sizeof(type_alarms_parameters));
      
      log_line("Received alarms params. New motor alarm value: %d/%d",
         g_pCurrentModel->alarms_params.uAlarmMotorCurrentThreshold & (1<<7),
         g_pCurrentModel->alarms_params.uAlarmMotorCurrentThreshold & 0x7F );

      saveCurrentModel();
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      signalReloadModel(0, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_OSD_CURRENT_LAYOUT )
   {
      static u32 s_uLastCommandIdSetOSDLayout = MAX_U32;
      if ( pPHC->command_counter == s_uLastCommandIdSetOSDLayout )
         return true;
      
      s_uLastCommandIdSetOSDLayout = pPHC->command_counter;

      int nOSDIndex = pPHC->command_param;
      if ( nOSDIndex < 0 || nOSDIndex >= MODEL_MAX_OSD_SCREENS )
         nOSDIndex = 0;
      g_pCurrentModel->osd_params.iCurrentOSDScreen = nOSDIndex;
      saveCurrentModel();
      log_line("Saved received new active OSD layout layout: %d, enabled: %s", nOSDIndex, (g_pCurrentModel->osd_params.osd_flags2[nOSDIndex] & OSD_FLAG2_LAYOUT_ENABLED)?"yes":"no");
      signalReloadModel(MODEL_CHANGED_OSD_PARAMS, 0);
      return true;    
   }

   if ( uCommandType == COMMAND_ID_SET_SERIAL_PORTS_INFO )
   {
      log_line("Received new serial ports info");
      for( int i=0; i<5; i++ )
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 20);
      type_vehicle_hardware_interfaces_info* pNewInfo = (type_vehicle_hardware_interfaces_info*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      g_pCurrentModel->hardwareInterfacesInfo.serial_port_count = pNewInfo->serial_port_count;
      for( int i=0; i<pNewInfo->serial_port_count; i++ )
      {
         strcpy(g_pCurrentModel->hardwareInterfacesInfo.serial_port_names[i], pNewInfo->serial_port_names[i]);
         g_pCurrentModel->hardwareInterfacesInfo.serial_port_speed[i] = pNewInfo->serial_port_speed[i];
         g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] = pNewInfo->serial_port_supported_and_usage[i];
      }

      int iCount = g_pCurrentModel->hardwareInterfacesInfo.serial_port_count;
      log_line("Received %d serial ports. Hardware has %d serial ports.", iCount, hardware_serial_get_ports_count());
      if ( iCount > hardware_serial_get_ports_count() )
         iCount = hardware_serial_get_ports_count();

      int iSiKPortToUpdate = -1;
      int iSikPortSpeedToUse = -1;   
      for( int i=0; i<iCount; i++ )
      {
         hw_serial_port_info_t* pPortInfo = hardware_get_serial_port_info(i);
         if ( NULL == pPortInfo )
            continue;

         if ( pPortInfo->iPortUsage == SERIAL_PORT_USAGE_SIK_RADIO )
         if ( pPortInfo->lPortSpeed != g_pCurrentModel->hardwareInterfacesInfo.serial_port_speed[i] )
         if ( hardware_serial_is_sik_radio(pPortInfo->szPortDeviceName) )
         {
            // Changed the serial speed of a SiK radio interface
            log_line("Received command to change the serial speed of a SiK radio interface. Serial port: [%s], old speed: %ld bps, new speed: %d bps",
               pPortInfo->szPortDeviceName, pPortInfo->lPortSpeed,
               g_pCurrentModel->hardwareInterfacesInfo.serial_port_speed[i] );
            iSikPortSpeedToUse = (int)pPortInfo->lPortSpeed;
            iSiKPortToUpdate = i;
         }

         pPortInfo->lPortSpeed = g_pCurrentModel->hardwareInterfacesInfo.serial_port_speed[i];
         pPortInfo->iPortUsage = (int)(g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & 0xFF);
      }

      hardware_serial_save_configuration();
      saveCurrentModel();
      signalReloadModel(MODEL_CHANGED_SERIAL_PORTS, 0);
      
      if ( -1 != iSiKPortToUpdate )
      {
         hw_serial_port_info_t* pPortInfo = hardware_get_serial_port_info(iSiKPortToUpdate);
         if ( NULL != pPortInfo )
         {
            radio_hw_info_t* pSiKRadio = hardware_radio_sik_get_from_serial_port(pPortInfo->szPortDeviceName);
            if ( NULL != pSiKRadio )
            {
               for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
               {
                  radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
                  if ( pRadioInfo == pSiKRadio )
                  {
                     log_line("Notify router that the serial speed of a Sik Radio must be changed.");
                     t_packet_header PH;
                     radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SET_SIK_RADIO_SERIAL_SPEED, STREAM_ID_DATA);
                     PH.vehicle_id_src = (u32)i;
                     PH.vehicle_id_dest = (u32)iSikPortSpeedToUse;
                     PH.total_length = sizeof(t_packet_header);
                     
                     u8 buffer[MAX_PACKET_TOTAL_SIZE];
                     memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
                     ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, PH.total_length);

                     if ( NULL != g_pProcessStats )
                        g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
                     if ( NULL != g_pProcessStats )
                        g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
                     break;
                  }
               }
            }
         }
      }
      return true;      
   }

   if ( uCommandType == COMMAND_ID_SET_TELEMETRY_TYPE_AND_PORT )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      int iTelemetryType = pPHC->command_param & 0x0F;
      int iSerialPort = (pPHC->command_param & 0xF0) >> 4;
      int iSerialSpeed = (pPHC->command_param >> 8);
      log_line("Received command to set telemetry type to %d, serial port to %d and serial speed to %d",
          iTelemetryType, iSerialPort, iSerialSpeed);
      g_pCurrentModel->setTelemetryTypeAndPort(iTelemetryType, iSerialPort, iSerialSpeed);
      saveCurrentModel();
      g_pCurrentModel->syncModelSerialPortsToHardwareSerialPorts();
      hardware_serial_save_configuration();
      signalReloadModel(MODEL_CHANGED_SERIAL_PORTS, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_TELEMETRY_PARAMETERS )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      telemetry_parameters_t* params = (telemetry_parameters_t*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      log_line("Received telemetry params, size: %d bytes, expected %d bytes", iParamsLength, sizeof(telemetry_parameters_t));
      memcpy(&g_pCurrentModel->telemetry_params, params, sizeof(telemetry_parameters_t));
      log_line("Received telemetry type: %d", g_pCurrentModel->telemetry_params.fc_telemetry_type);
      // Remove serial port used, if telemetry is set to None.
      if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_NONE )
      {
         for( int i=0; i<g_pCurrentModel->hardwareInterfacesInfo.serial_port_count; i++ )
         {
            u32 uPortTelemetryType = g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & 0xFF;
            if ( uPortTelemetryType == SERIAL_PORT_USAGE_TELEMETRY )
            {
               // Remove serial port usage (set it to none)
               g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] &= 0xFFFFFF00;
               log_line("Did set serial port %d to type None as it was used for telemetry.");
            }
         }
      }

      if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_NONE )
      {
         int iCurrentSerialPortIndexForTelemetry = -1;
         for( int i=0; i<g_pCurrentModel->hardwareInterfacesInfo.serial_port_count; i++ )
         {
             u32 uPortTelemetryType = g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & 0xFF;
             
             if ( g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & MODEL_SERIAL_PORT_BIT_SUPPORTED )
             if ( uPortTelemetryType == SERIAL_PORT_USAGE_TELEMETRY )
             {
                iCurrentSerialPortIndexForTelemetry = i;
                break;
             }
         }
         log_line("Currently serial port index used for telemetry: %d", iCurrentSerialPortIndexForTelemetry);
         if ( -1 == iCurrentSerialPortIndexForTelemetry )
         {
            for( int i=0; i<g_pCurrentModel->hardwareInterfacesInfo.serial_port_count; i++ )
            {
               u32 uPortUsage = g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & 0xFF;
               
               if ( g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & MODEL_SERIAL_PORT_BIT_SUPPORTED )
               if ( uPortUsage == SERIAL_PORT_USAGE_NONE )
               {
                  iCurrentSerialPortIndexForTelemetry = i;
                  break;
               }
            }
            if ( -1 == iCurrentSerialPortIndexForTelemetry )
              log_line("Could not find a default serial port for telemetry.");
            else
            {
               log_line("Found a default serial port for telemetry. Serial port index %d", iCurrentSerialPortIndexForTelemetry);

               if ( g_pCurrentModel->hardwareInterfacesInfo.serial_port_speed[iCurrentSerialPortIndexForTelemetry] <= 0 )
                  g_pCurrentModel->hardwareInterfacesInfo.serial_port_speed[iCurrentSerialPortIndexForTelemetry] = DEFAULT_FC_TELEMETRY_SERIAL_SPEED;

               g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[iCurrentSerialPortIndexForTelemetry] &= 0xFFFFFF00;
               if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MAVLINK )
                  g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[iCurrentSerialPortIndexForTelemetry] |= SERIAL_PORT_USAGE_TELEMETRY;
               if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_LTM )
                  g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[iCurrentSerialPortIndexForTelemetry] |= SERIAL_PORT_USAGE_TELEMETRY;
               if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP )
                  g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[iCurrentSerialPortIndexForTelemetry] |= SERIAL_PORT_USAGE_TELEMETRY;
            }
         }
      }

      saveCurrentModel();
      g_pCurrentModel->syncModelSerialPortsToHardwareSerialPorts();
      hardware_serial_save_configuration();
      signalReloadModel(MODEL_CHANGED_SERIAL_PORTS, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_REBOOT )
   {
      for( int i=0; i<10; i++ )
      {
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
         hardware_sleep_ms(10);
      }
      hardware_sleep_ms(300);
      signalReboot(true);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_VEHICLE_NAME )
   {
      char* szName = (char*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      strcpy(g_pCurrentModel->vehicle_name, szName);
      saveCurrentModel();
      signalReloadModel(0, 0);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      return true;
   }

   // TO FIX
   if ( uCommandType == COMMAND_ID_SET_ALL_PARAMS )
   {
      /*
      int length = pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_command);
      u8* pData = pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command);
      log_line("Received all model settings. Model file size: %d", length);
      FILE* fd = fopen(FILE_CONFIG_CURRENT_VEHICLE_MODEL, "wb");
      if ( NULL != fd )
      {
          fwrite(pData, 1, length, fd);
          fclose(fd);
          fd = NULL;
      }
      else
      {
         log_error_and_alarm("Failed to save received vehicle configuration to file: %s", FILE_CONFIG_CURRENT_VEHICLE_MODEL);
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
         return true;
      }
         
      u32 vid = g_pCurrentModel->uVehicleId;
      u32 ctrlId = g_pCurrentModel->uControllerId;
      int boardType = g_pCurrentModel->hwCapabilities.uBoardType;
      u8 cameraType = g_pCurrentModel->camera_type;

      bool bHasAudioDevice = g_pCurrentModel->audio_params.has_audio_device;

      int nicCount = g_pCurrentModel->nic_count;
      int iFr[MAX_RADIO_INTERFACES];
      u32 iType[MAX_RADIO_INTERFACES];
      u8 iBands[MAX_RADIO_INTERFACES];
      u8 iFlags[MAX_RADIO_INTERFACES];
      char iszMAC[MAX_RADIO_INTERFACES][MAX_MAC_LENGTH];
      char iszPort[MAX_RADIO_INTERFACES][3];
      for( int i=0; i<g_pCurrentModel->nic_count; i++ )
      {
         iFr[i] = g_pCurrentModel->nic_frequency[i];
         iType[i] = g_pCurrentModel->nic_type_and_driver[i];
         iBands[i] = g_pCurrentModel->nic_supported_bands[i];
         iFlags[i] = g_pCurrentModel->nic_flags[i];
         strcpy(iszMAC[i], g_pCurrentModel->nic_szMAC[i]);
         strcpy(iszPort[i], g_pCurrentModel->nic_szPort[i]);
      }

      g_pCurrentModel->loadFromFile(FILE_CONFIG_CURRENT_VEHICLE_MODEL, true);

      g_pCurrentModel->nic_count = nicCount;
      for( int i=0; i<g_pCurrentModel->nic_count; i++ )
      {
         g_pCurrentModel->nic_frequency[i] = iFr[i];
         g_pCurrentModel->nic_type_and_driver[i] = iType[i];
         g_pCurrentModel->nic_supported_bands[i] = iBands[i];
         g_pCurrentModel->nic_flags[i] = iFlags[i];
         strcpy(g_pCurrentModel->nic_szMAC[i], iszMAC[i]);
         strcpy(g_pCurrentModel->nic_szPort[i], iszPort[i]);
      }
      g_pCurrentModel->uVehicleId = vid;
      g_pCurrentModel->uControllerId = ctrlId;
      g_pCurrentModel->hwCapabilities.uBoardType = boardType;
      g_pCurrentModel->camera_type = cameraType;

      g_pCurrentModel->audio_params.has_audio_device = bHasAudioDevice;

      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);

      // To fix

      vehicle_stop_tx_router();
      vehicle_stop_audio_capture(g_pCurrentModel);
      if ( g_pCurrentModel->hasCamera() )
         vehicle_stop_video_capture(g_pCurrentModel); 

      if ( g_pCurrentModel->audio_params.has_audio_device )
         vehicle_launch_audio_capture(g_pCurrentModel);
      if ( g_pCurrentModel->hasCamera() )
      {
         if ( g_pCurrentModel->isActiveCameraHDMI() )
            hardware_sleep_ms(800);
         vehicle_launch_video_capture(g_pCurrentModel, NULL);
      }
      vehicle_launch_tx_router(g_pCurrentModel);
      signalReloadModel(0, 0);
      return true;
      */
   }

   if ( uCommandType == COMMAND_ID_GET_ALL_PARAMS_ZIP )
   {
      log_line("Get all params zip: Current OSD params, current layout: %d, enabled: %s", g_pCurrentModel->osd_params.iCurrentOSDScreen, (g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_FLAG2_LAYOUT_ENABLED)?"yes":"no");
      log_line("Get all params zip: Current on time: %02d:%02d, current flights: %d", g_pCurrentModel->m_Stats.uCurrentOnTime/60, g_pCurrentModel->m_Stats.uCurrentOnTime%60, g_pCurrentModel->m_Stats.uTotalFlights);

      bool bSendBackSmallSegments = true;
      bool bNewZIPCommand = false;

      if ( (lastRecvCommandNumber != s_ZIPParams_uLastRecvCommandNumber) || s_ZIPParams_uLastRecvSourceControllerId == 0 || (s_ZIPParams_uLastRecvSourceControllerId != lastRecvSourceControllerId) || s_ZIPPAarams_uLastRecvCommandTime == 0 || (lastRecvCommandTime > s_ZIPPAarams_uLastRecvCommandTime + 4000) )
      {
         bNewZIPCommand = true;
         s_ZIPParams_uLastRecvSourceControllerId = lastRecvSourceControllerId;
         s_ZIPParams_uLastRecvCommandNumber = lastRecvCommandNumber;
         s_ZIPPAarams_uLastRecvCommandTime = lastRecvCommandTime;
         log_line("New command for getting zip params, regenerate data.");
      }
      else
         log_line("Reuse command for getting zip params, use cached data.");

      if ( bNewZIPCommand )
      {
         _populate_camera_name();
         char szComm[256];
         sprintf(szComm, "rm -rf %s/model.tar* 2>/dev/null", FOLDER_RUBY_TEMP);
         hw_execute_bash_command(szComm, NULL);
         sprintf(szComm, "rm -rf %s/model.mdl 2>/dev/null", FOLDER_RUBY_TEMP);
         hw_execute_bash_command(szComm, NULL);

         char szFile[128];
         strcpy(szFile, FOLDER_CONFIG);
         strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
         sprintf(szComm, "cp -rf %s %s/model.mdl 2>/dev/null", szFile, FOLDER_RUBY_TEMP);
         hw_execute_bash_command(szComm, NULL);
         sprintf(szComm, "tar -C %s -cf %s/model.tar model.mdl 2>&1", FOLDER_RUBY_TEMP, FOLDER_RUBY_TEMP);
         hw_execute_bash_command(szComm, NULL);
         sprintf(szComm, "gzip %s/model.tar 2>&1", FOLDER_RUBY_TEMP);
         hw_execute_bash_command(szComm, NULL);

         s_ZIPParams_Model_BufferLength = 0;
         strcpy(szFile, FOLDER_RUBY_TEMP);
         strcat(szFile, "model.tar.gz");
         FILE* fd = fopen(szFile, "rb");
         if ( NULL != fd )
         {
            s_ZIPParams_Model_BufferLength = fread(s_ZIPParams_Model_Buffer, 1, 2500, fd);
            log_line("Read compressed model settings. %d bytes", s_ZIPParams_Model_BufferLength);
            fclose(fd);
         }
         else
            log_error_and_alarm("Failed to load current vehicle configuration from file: [%s]", szFile);

         sprintf(szComm, "rm -rf %s/model.tar*", FOLDER_RUBY_TEMP);
         hw_execute_bash_command(szComm, NULL);
         sprintf(szComm, "rm -rf %s/model.mdl", FOLDER_RUBY_TEMP);
         hw_execute_bash_command(szComm, NULL);

         if ( 0 == s_ZIPParams_Model_BufferLength || s_ZIPParams_Model_BufferLength > MAX_PACKET_PAYLOAD )
         {
            log_softerror_and_alarm("Invalid compressed model file size (%d). Skipping sending it to controller.", s_ZIPParams_Model_BufferLength); 
            sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
            return true;
         }
      }

      setCommandReplyBuffer(s_ZIPParams_Model_Buffer, s_ZIPParams_Model_BufferLength);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 1, 10);
      log_line("Sent back to router all model settings in one single command response. Total compressed size: %d bytes", s_ZIPParams_Model_BufferLength);
      
      if ( bSendBackSmallSegments )
      {
         int iSegmentSize = 150;
         int iPos = 0;
         int iSegment = 0;
         u8 uSegment[256];

         static int s_iModelSettingsSegmentFileId = 0;
         s_iModelSettingsSegmentFileId++;
         int iCountSegments = s_ZIPParams_Model_BufferLength / iSegmentSize;
         if ( iCountSegments * iSegmentSize != s_ZIPParams_Model_BufferLength )
            iCountSegments++;
            
         while ( iPos < s_ZIPParams_Model_BufferLength )
         {
            int iSize = iSegmentSize;
            if ( iPos + iSize > s_ZIPParams_Model_BufferLength )
               iSize = s_ZIPParams_Model_BufferLength - iPos;
            uSegment[0] = s_iModelSettingsSegmentFileId;
            uSegment[1] = iSegment;
            uSegment[2] = iCountSegments;
            uSegment[3] = iSize;
            memcpy( &(uSegment[4]), s_ZIPParams_Model_Buffer + iPos, iSize);
            setCommandReplyBuffer(uSegment, iSize+4);
            sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 1, 10);
            log_line("Sent back to router model settings command response as small segment (%d of %d) size: %d bytes",
               iSegment+1, iCountSegments, iSize);             
            iSegment++;
            iPos += iSize;
         }
      } 

      hardware_sleep_ms(10);
      _signalRouterSendVehicleSettings();
      return true;
   }

   if ( uCommandType == COMMAND_ID_GET_CURRENT_VIDEO_CONFIG )
   {
      u8 buffer[2048];
      int length = 0;
      char szFile[128];
      strcpy(szFile, FOLDER_RUBY_TEMP);
      strcat(szFile, FILE_TEMP_CURRENT_VIDEO_PARAMS);
      FILE* fd = fopen(szFile, "rb");
      if ( NULL != fd )
      {
         length = fread(buffer, 1, 2000, fd);
         fclose(fd);
      }
      else
         log_error_and_alarm("Failed to load current video config from log file: %s", szFile);
      buffer[length] = 0;
      length++;

      setCommandReplyBuffer(buffer, length);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      return true;
   }


   if ( uCommandType == COMMAND_ID_SET_TX_POWERS )
   {
      u8* pData = pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command);
      if ( iParamsLength < 1 )
      {
         log_softerror_and_alarm("Received invalid params to set cards tx powers: no params received.");
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED_INVALID_PARAMS, 0, 0);
         return true;
      }
      int iCountCards = iParamsLength-1;
      if ( iCountCards != *pData )
      {
         log_softerror_and_alarm("Received invalid params to set cards tx powers: count cards (%d) different than received buffer size (%d)", *pData, iParamsLength);
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED_INVALID_PARAMS, 0, 0);  
         return true;     
      }
      log_line("Received new tx power levels for %d cards:", iCountCards);
      for( int i=0; i<iCountCards; i++ )
      {
         log_line("Radio interface %d new tx raw power to set: %d (current: %d)", i+1, pData[i+1], g_pCurrentModel->radioInterfacesParams.interface_raw_power[i]);
         if ( pData[i+1] == g_pCurrentModel->radioInterfacesParams.interface_raw_power[i] )
            continue;
         g_pCurrentModel->radioInterfacesParams.interface_raw_power[i] = pData[i+1];
      }
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      saveCurrentModel();
      signalReloadModel(MODEL_CHANGED_RADIO_POWERS, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_ENABLE_DHCP)
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      g_pCurrentModel->enableDHCP = (bool)(pPHC->command_param);
      saveCurrentModel();
      hardware_mount_boot();
      hardware_sleep_ms(200);
      if ( g_pCurrentModel->enableDHCP )
      {
         log_line("Enabling DHCP");
         hw_execute_bash_command("rm -f /boot/nodhcp", NULL);
      }
      else
      {
         log_line("Disabling DHCP");
         hw_execute_bash_command("echo '1' > /boot/nodhcp", NULL);
      }
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_RC_PARAMS )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      rc_parameters_t* params = (rc_parameters_t*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      memcpy(&g_pCurrentModel->rc_params, params, sizeof(rc_parameters_t));
      log_dword("Received RC flags: ", g_pCurrentModel->rc_params.flags);
      saveCurrentModel();
      signalReloadModel(MODEL_CHANGED_RC_PARAMS, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_THREADS_PRIORITIES )
   {
      if ( iParamsLength != sizeof(type_processes_priorities) )
      {
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
         return true;
      }
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      if (  pPHC->command_param )
      {
         for( int i=0; i<10; i++ )
            sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 50);
      }
      u8* pData = pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command);
      type_processes_priorities* pNewPrio = (type_processes_priorities*)pData;
      log_line("Received new threads priorities. Different? %s", (0 == memcmp(pData, (u8*)&(g_pCurrentModel->processesPriorities), sizeof(type_processes_priorities)))?"no":"yes");
      memcpy((u8*)&(g_pCurrentModel->processesPriorities), pData, sizeof(type_processes_priorities));
      log_line("Restart processes now? %s", pPHC->command_param?"yes":"no");
      log_line("New router raw priority: %d and radio rx/tx threads raw priorities: %d/%d (adjustments enabled: %d)",
         pNewPrio->iThreadPriorityRouter, pNewPrio->iThreadPriorityRadioRx, pNewPrio->iThreadPriorityRadioTx,
         (pNewPrio->uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS)?1:0);
      saveCurrentModel();
      signalReloadModel(MODEL_CHANGED_THREADS_PRIORITIES, pPHC->command_param?1:0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_DEBUG_GET_TOP )
   {
      char szOutput[1024];
      hw_execute_bash_command_raw("top -bn1 | grep -A 1 'CPU\\|ruby\\|raspi'", szOutput);
      log_line("TOP output: %s", szOutput);
      setCommandReplyBuffer((u8*)szOutput, strlen(szOutput)+1);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_DEVELOPER_FLAGS )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);

      u8* pData = pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command);
      memcpy((u8*)&(g_pCurrentModel->uDeveloperFlags), pData, sizeof(u32));
      log_line("Received developer flags: %s", str_get_developer_flags(g_pCurrentModel->uDeveloperFlags));

      if ( iParamsLength >= 2*(int)sizeof(u32) )
      {
         u32 uTmp = 0;
         memcpy((u8*)&uTmp, pData + sizeof(u32), sizeof(u32));
         log_line("Received developer value for max radio Rx loop time: %u ms", uTmp);
         VehicleSettings* pVS = get_VehicleSettings();
         if ( NULL != pVS )
         {
            pVS->iDevRxLoopTimeout = uTmp;
            save_VehicleSettings();
         }
      }

      saveCurrentModel();
      signalReloadModel(MODEL_CHANGED_DEVELOPER_FLAGS, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_UPLOAD_SW_TO_VEHICLE63 )
   {
      process_sw_upload_new(pPHC->command_param, pBuffer, length);
      return true;
   }

   if ( uCommandType == COMMAND_ID_RESET_ALL_DEVELOPER_FLAGS )
   {
      for( int i=0; i<20; i++ )
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 20);

      g_pCurrentModel->uDeveloperFlags = (((u32)DEFAULT_DELAY_WIFI_CHANGE)<<8);

      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_RETRANSMISSIONS_DUPLICATION_PERCENT_AUTO;
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].uProfileEncodingFlags &= (~VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HQ<<8);
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_HIGH_PERF].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_RETRANSMISSIONS_DUPLICATION_PERCENT_AUTO;
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_HIGH_PERF].uProfileEncodingFlags &= (~VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_HIGH_PERF].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_USER].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_RETRANSMISSIONS_DUPLICATION_PERCENT_AUTO;
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_USER].uProfileEncodingFlags &= (~VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_USER].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_CUST].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_RETRANSMISSIONS_DUPLICATION_PERCENT_AUTO;
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_CUST].uProfileEncodingFlags &= (~VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_CUST].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);

      saveCurrentModel();
      //hardware_sleep_ms(400);
      //hardware_reboot();
      return true;
   }

   return false;
}


void on_received_command(u8* pBuffer, int length)
{
   if ( length < (int)sizeof(t_packet_header) + (int)sizeof(t_packet_header_command) )
   {
      log_line_commands("Received a invalid command (packet to small). Ignoring.");
      log_softerror_and_alarm("Received a invalid command (packet to small). Ignoring.");
      return;
   }

   t_packet_header* pPH = (t_packet_header*)pBuffer;
   t_packet_header_command* pPHC = (t_packet_header_command*)(pBuffer + sizeof(t_packet_header));

   if ( pPH->vehicle_id_dest != g_pCurrentModel->uVehicleId )
      return;

   if ( ! radio_packet_check_crc(pBuffer, pPH->total_length) )
   {
      log_line_commands("Received a invalid command (invalid CRC). Ignoring.");
      return;
   }

   // Ignore commands that are resent multiple times
   // Respond to same command retry.
   // Except for commands that do not requires a response, those just process them. (COMMAND_ID_SET_OSD_CURRENT_LAYOUT)

   if ( (pPHC->command_type & COMMAND_TYPE_MASK) != COMMAND_ID_SET_OSD_CURRENT_LAYOUT )
   if ( pPHC->command_counter == lastRecvCommandNumber )
   if ( (lastRecvCommandTime != 0) && (g_TimeNow >= lastRecvCommandTime) && (g_TimeNow < lastRecvCommandTime+4000) )
   {
      if ( pPHC->command_resend_counter > lastRecvCommandResendCounter )
      {
         log_line_commands("Received command nb.%d, retry count: %d, command type: %d: %s, command param: %u, extra info size: %d",
            pPHC->command_counter, pPHC->command_resend_counter, pPHC->command_type & COMMAND_TYPE_MASK, commands_get_description(((pPHC->command_type) & COMMAND_TYPE_MASK)), pPHC->command_param, length-sizeof(t_packet_header)-sizeof(t_packet_header_command));
         if ( ! (((pPHC->command_type) & COMMAND_TYPE_MASK) & COMMAND_TYPE_FLAG_NO_RESPONSE_NEEDED) )
         {
            log_line("Resending command response, for command id: %d", lastRecvCommandNumber);
            lastRecvCommandResendCounter = pPHC->command_resend_counter;
            sendCommandReply(lastRecvCommandResponseFlags, 0, 0);
         }
      }
      else if (pPHC->command_counter > 1 )
         log_line_commands("Ignoring command (duplicate) (current vehicle UID: %u) nb.%d, retry count: %d, command type: %s, command param: %u, extra info size: %d", g_pCurrentModel->uVehicleId, pPHC->command_counter, lastRecvCommandResendCounter, commands_get_description(((pPHC->command_type) & COMMAND_TYPE_MASK)), pPHC->command_param, length-sizeof(t_packet_header)-sizeof(t_packet_header_command));

      if ( pPHC->command_resend_counter > lastRecvCommandResendCounter || pPHC->command_counter > 1 )
         return;
   }

   log_line_commands("Received command nb.%d, retry count: %d, command type: %d: %s, command param: %u, extra info size: %d", pPHC->command_counter, pPHC->command_resend_counter, pPHC->command_type & COMMAND_TYPE_MASK, commands_get_description(((pPHC->command_type) & COMMAND_TYPE_MASK)), pPHC->command_param, length-sizeof(t_packet_header)-sizeof(t_packet_header_command));

   lastRecvSourceControllerId = pPH->vehicle_id_src;
   lastRecvCommandNumber = pPHC->command_counter;
   lastRecvCommandResendCounter = pPHC->command_resend_counter;
   lastRecvCommandType = pPHC->command_type;
   lastRecvCommandTime = g_TimeNow;
   lastRecvCommandResponseFlags = 0;
   lastRecvCommandReplyBufferLength = 0;

   if ( ! process_command(pBuffer, length) )
      sendCommandReply(COMMAND_RESPONSE_FLAGS_UNKNOWN_COMMAND, 0, 0); 
   //log_line("Finished processing command.");   
}

void _periodic_loop()
{
   if ( process_sw_upload_is_rebooting() )
      return;

   if ( (g_TimeLastSetRadioLinkFlagsStartOperation != 0) && s_bWaitForRadioFlagsChangeConfirmation && (s_iRadioLinkIdChangeConfirmation != -1) )
   {
      if ( g_TimeNow >= g_TimeLastSetRadioLinkFlagsStartOperation + TIMEOUT_RADIO_FRAMES_FLAGS_CHANGE_CONFIRMATION )
      {
         log_softerror_and_alarm("No confirmation received from controller about the last radio flags change on radio link %d", s_iRadioLinkIdChangeConfirmation+1);
         g_TimeLastSetRadioLinkFlagsStartOperation = 0;
         s_bWaitForRadioFlagsChangeConfirmation = false;

         memcpy(&(g_pCurrentModel->radioLinksParams), &s_LastGoodRadioLinksParams, sizeof(type_radio_links_parameters));
         g_pCurrentModel->validateRadioSettings();

         char szBuffR[128];
         str_get_radio_frame_flags_description(g_pCurrentModel->radioLinksParams.link_radio_flags[s_iRadioLinkIdChangeConfirmation], szBuffR); 
         log_line("Revert radio link flags for link %d to: %s, datarates: %d/%d", s_iRadioLinkIdChangeConfirmation+1, szBuffR, g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[s_iRadioLinkIdChangeConfirmation], g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[s_iRadioLinkIdChangeConfirmation]);
   
         saveCurrentModel();

         log_line("Radio link %d datarates now: %d/%d", s_iRadioLinkIdChangeConfirmation+1, g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[s_iRadioLinkIdChangeConfirmation], g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[s_iRadioLinkIdChangeConfirmation]);
         
         signalReloadModel(MODEL_CHANGED_RADIO_LINK_FRAMES_FLAGS, s_iRadioLinkIdChangeConfirmation);
         s_iRadioLinkIdChangeConfirmation = -1;
      }
   }

   if ( process_sw_upload_is_started() )
      process_sw_upload_check_timeout(g_TimeNow);
}

void handle_sigint_rcom(int sig) 
{ 
   log_line("--------------------------");
   log_line("Caught signal to stop: %d", sig);
   log_line("--------------------------");
   g_bQuit = true;
} 

int r_start_commands_rx(int argc, char* argv[])
{
   signal(SIGINT, handle_sigint_rcom);
   signal(SIGTERM, handle_sigint_rcom);
   signal(SIGQUIT, handle_sigint_rcom);

   if ( strcmp(argv[argc-1], "-ver") == 0 )
   {
      printf("%d.%d (b-%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR, SYSTEM_SW_BUILD_NUMBER);
      return 0;
   }

   log_init("RX_Commands");
   log_arguments(argc, argv);

   //utils_log_radio_packets_sizes();
   //radio_packets_log_sizes();

   s_fIPCFromRouter = ruby_open_ipc_channel_read_endpoint(IPC_CHANNEL_TYPE_ROUTER_TO_COMMANDS);
   if ( s_fIPCFromRouter < 0 )
      return -1;

   s_fIPCToRouter = ruby_open_ipc_channel_write_endpoint(IPC_CHANNEL_TYPE_COMMANDS_TO_ROUTER);
   if ( s_fIPCToRouter < 0 )
      return -1;
 
   hardware_detectBoardAndSystemType();
   // Need to have i2c enumarated already so command reset/factory reset does not take a lot of time
   hardware_i2c_enumerate_busses(0);
   hardware_i2c_load_device_settings();

   g_uControllerId = vehicle_utils_getControllerId();
   load_VehicleSettings();
   hardware_serial_reload_ports_settings();
   hardware_enumerate_radio_interfaces();

   loadAllModels();
   g_pCurrentModel = getCurrentModel();

   if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS )
      log_only_errors();

   if ( g_pCurrentModel->uModelFlags & MODEL_FLAG_DISABLE_ALL_LOGS )
   {
      log_line("Log is disabled on vehicle. Disabled logs.");
      log_disable();
   }

   if ( g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS )
      hw_set_priority_current_proc(g_pCurrentModel->processesPriorities.iThreadPriorityOthers);
   if ( g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_AFFINITY_CORES )
      hw_set_current_thread_affinity("rx_commands", g_pCurrentModel->processesPriorities.iCoreCommands, g_pCurrentModel->processesPriorities.iCoreCommands);

   g_pProcessStats = shared_mem_process_stats_open_write(SHARED_MEM_WATCHDOG_COMMANDS_RX);
   if ( NULL == g_pProcessStats )
      log_softerror_and_alarm("Failed to open shared mem for commands Rx process watchdog for writing: %s", SHARED_MEM_WATCHDOG_COMMANDS_RX);
   else
      log_line("Opened shared mem for commands Rx process watchdog for writing.");

   process_sw_upload_init();
   process_calibration_files_init();

   s_InfoLastFileUploaded.uLastFileId = MAX_U32;
   s_InfoLastFileUploaded.szFileName[0] = 0;
   s_InfoLastFileUploaded.uFileSize = 0;
   s_InfoLastFileUploaded.uTotalSegments = 0;
   s_InfoLastFileUploaded.uLastCommandIdForThisFile = 0;

   sem_t* pSemaphoreStop = sem_open(SEMAPHORE_STOP_VEHICLE_COMMANDS, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( (NULL == pSemaphoreStop) || (SEM_FAILED == pSemaphoreStop) )
   {
      log_error_and_alarm("Failed to open semaphore: %s", SEMAPHORE_STOP_VEHICLE_COMMANDS);
      pSemaphoreStop = NULL;
   } 

   g_TimeNow = get_current_timestamp_ms();
   g_TimeStart = get_current_timestamp_ms();

   log_line("Started. Running now.");
   log_line("------------------------------------------");

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastRadioTxTime = g_TimeNow;
      g_pProcessStats->lastRadioRxTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   } 
  
   g_TimeLastPeriodicCheck = get_current_timestamp_ms();
 
   int iSleepIntervalMS = 2;

   while (!g_bQuit) 
   {
      hardware_sleep_ms(iSleepIntervalMS);
      g_uLoopCounter++;
      g_TimeNow = get_current_timestamp_ms();
      u32 tTime0 = g_TimeNow;

      if ( NULL != g_pProcessStats )
      {
         g_pProcessStats->uLoopCounter++;
         g_pProcessStats->lastActiveTime = g_TimeNow;
      }

      if ( g_TimeNow > g_TimeLastPeriodicCheck + 300 )
      {
         g_TimeLastPeriodicCheck = g_TimeNow;
         _periodic_loop();

         if ( is_semaphore_signaled_clear(pSemaphoreStop, SEMAPHORE_STOP_VEHICLE_COMMANDS) )
         {
            log_line("Semaphore to stop is signaled. Stopping...");
            g_bQuit = true;
            break;
         }
      }

      if ( iSleepIntervalMS < 20 )
         iSleepIntervalMS += 2;

      int maxMsgToRead = 10 + DEFAULT_UPLOAD_PACKET_CONFIRMATION_FREQUENCY;
      while ( (maxMsgToRead > 0) && (NULL != ruby_ipc_try_read_message(s_fIPCFromRouter, s_PipeTmpBufferCommands, &s_PipeTmpBufferCommandsPos, s_BufferCommands)) )
      {
         if ( process_sw_upload_is_rebooting() )
            continue;
         iSleepIntervalMS = 1;
         maxMsgToRead--;
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
         t_packet_header* pPH = (t_packet_header*) s_BufferCommands;

         if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RUBY )
         if ( pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_REQUEST )
         {
            u32 uResendCount = 0;
            if ( pPH->total_length >= sizeof(t_packet_header) + sizeof(u32) )
               memcpy(&uResendCount, &(s_BufferCommands[sizeof(t_packet_header)]), sizeof(u32));
            if ( pPH->total_length >= sizeof(t_packet_header) + 2*sizeof(u32) )
               memcpy(&g_pCurrentModel->uDeveloperFlags, &(s_BufferCommands[sizeof(t_packet_header) + sizeof(u32)]), sizeof(u32));
            if ( pPH->total_length >= sizeof(t_packet_header) + 3*sizeof(u32) )
               memcpy(&g_pCurrentModel->uControllerBoardType, &(s_BufferCommands[sizeof(t_packet_header) + 2*sizeof(u32)]), sizeof(u32));

            log_line("Pairing request: Currently stored controller ID: %u / %u", g_uControllerId, g_pCurrentModel->uControllerId);
            log_line("Received pairing request from router (received resend count: %u). From CID %u to VID %u (%s). Developer mode: %s. Updating local model.",
               uResendCount, pPH->vehicle_id_src, pPH->vehicle_id_dest, (pPH->vehicle_id_dest == g_pCurrentModel->uVehicleId)?"self":"not self", (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE)?"on":"off");

            if ( NULL != g_pCurrentModel )
               g_pCurrentModel->onControllerIdUpdated(pPH->vehicle_id_src);
            g_uControllerId = pPH->vehicle_id_src;
            s_InfoLastFileUploaded.uLastCommandIdForThisFile = 0;
         }

         if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
         if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SEND_MODEL_SETTINGS )
         {
            log_line("Received notification from router to generate and send model settings to controller");
            send_model_settings_to_controller();
            continue;
         }

         if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
         {
            if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED )
            {
               u8 changeType = (pPH->vehicle_id_src >> 8 ) & 0xFF;
               u8 fromComponentId = (pPH->vehicle_id_src & 0xFF);
               log_line("Received request from router to reload model (from component id: %d (%s), change type: %d (%s).", (int)fromComponentId, str_get_component_id((int)fromComponentId), (int)changeType, str_get_model_change_type((int)changeType));
               char szFile[128];
               strcpy(szFile, FOLDER_CONFIG);
               strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
               if ( changeType == MODEL_CHANGED_STATS )
               {
                  log_line("Loading model including stats.");
                  if ( ! g_pCurrentModel->loadFromFile(szFile, true) )
                     log_error_and_alarm("Can't load current model vehicle.");
               }
               else
               {
                  if ( ! g_pCurrentModel->loadFromFile(szFile, false) )
                     log_error_and_alarm("Can't load current model vehicle.");
               }
            }
            if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_BROADCAST_VEHICLE_STATS )
            {
               memcpy((u8*)(&(g_pCurrentModel->m_Stats)),(u8*)&(s_BufferCommands[sizeof(t_packet_header)]), sizeof(type_vehicle_stats_info));
            }

            if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_LINK_FREQUENCY_CHANGED )
            {
               u32* pI = (u32*)((&s_BufferCommands[0])+sizeof(t_packet_header));
               u32 uLinkId = *pI;
               pI++;
               u32 uNewFreq = *pI;
               log_line("Received new radio link frequency from router (radio link %u new freq: %s). Updating local model copy.", uLinkId+1, str_format_frequency(uNewFreq));
               if ( uLinkId < (u32)g_pCurrentModel->radioLinksParams.links_count )
               {
                  g_pCurrentModel->radioLinksParams.link_frequency_khz[uLinkId] = uNewFreq;
                  for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
                  {
                     if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == (int)uLinkId )
                        g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[i] = uNewFreq;
                  }
                  log_current_full_radio_configuration(g_pCurrentModel);
               }
               else
                  log_softerror_and_alarm("Invalid parameters for changing radio link frequency. radio link: %u of %d", uLinkId+1, g_pCurrentModel->radioLinksParams.links_count);
            }
            continue;
         }

         if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
            on_received_command(s_BufferCommands, pPH->total_length);
      }

      u32 tNow = get_current_timestamp_ms();
      if ( NULL != g_pProcessStats )
      {
         if ( g_pProcessStats->uMaxLoopTimeMs < tNow - tTime0 )
            g_pProcessStats->uMaxLoopTimeMs = tNow - tTime0;
         g_pProcessStats->uTotalLoopTime += tNow - tTime0;
         if ( 0 != g_pProcessStats->uLoopCounter )
            g_pProcessStats->uAverageLoopTimeMs = g_pProcessStats->uTotalLoopTime / g_pProcessStats->uLoopCounter;
      }

      u32 dTime = get_current_timestamp_ms() - tTime0;
      if ( dTime > 150 )
         log_softerror_and_alarm("Main processing loop took too long (%u ms).", dTime);
   }

   log_line("Stopping...");

   if ( NULL != pSemaphoreStop )
       sem_close(pSemaphoreStop);
   sem_unlink(SEMAPHORE_STOP_VEHICLE_COMMANDS);

   ruby_close_ipc_channel(s_fIPCFromRouter);
   ruby_close_ipc_channel(s_fIPCToRouter);
   s_fIPCFromRouter = -1;
   s_fIPCToRouter = -1;

   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_COMMANDS_RX, g_pProcessStats);

   log_line("Stopped. Exit");
   log_line("--------------------------");
   return 0;
}
