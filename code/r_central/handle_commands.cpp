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

#include <pthread.h>
//#include "../base/radio_utils.h"
#include "../base/ctrl_settings.h"
#include "../common/models_connect_frequencies.h"
#include "../common/string_utils.h"
#include "../utils/utils_controller.h"
#include "handle_commands.h"
#include "popup.h"
#include "popup_log.h"
#include "shared_vars.h"
#include "menu.h"
#include "colors.h"
#include "osd_common.h"
#include "osd_plugins.h"
#include "link_watch.h"
#include "pairing.h"
#include "warnings.h"
#include "pairing.h"
#include "ruby_central.h"
#include "osd.h"
#include "launchers_controller.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"
#include "menu_update_vehicle.h"
#include "menu_confirmation.h"
#include "menu_diagnose_radio_link.h"
#include "menu_info_procs.h"
#include "process_router_messages.h"
#include "timers.h"
#include "events.h"


#include <sys/resource.h>
#include <termios.h>
#include <unistd.h>
#include <semaphore.h>
#include <math.h>
 
static bool s_bHasCommandInProgress = false;

static bool s_bHasToSyncCorePluginsInfoFromVehicle = true;
static bool s_bHasReceivedVehicleCorePluginsInfo = false;

#define MAX_COM_SEND_RETRY 5

static u32 s_CommandCounter = 0;
static u32 s_CommandLastProcessedResponseToCommandCounter = MAX_U32;
static bool s_bLastCommandSucceeded = false;
static u32 s_CommandStartTime = 0;
static u32 s_CommandTimeout = 20;
static u32 s_CommandTargetVehicleId = 0;
static u32 s_CommandType = 0;
static u32 s_CommandParam = 0;
static u8  s_CommandResendCounter = 0;
static u8  s_CommandMaxResendCounter = 20;
static u32 s_CommandLastResponseReceivedTime = 0;

static u8  s_CommandBuffer[4096];
static int s_CommandBufferLength = 0;

static u8 s_CommandReplyBuffer[MAX_PACKET_TOTAL_SIZE];
static int s_CommandReplyLength = 0;

static int s_iCountRetriesToGetModelSettingsCommand = 0;
static int s_RetryGetCorePluginsCounter = 0;


#define MAX_FILE_SEGMENTS_TO_DOWNLOAD 5000

static u32 s_uFileIdToDownload = 0;
static u8  s_uFileToDownloadState = 0xFF;
static u32 s_uFileToDownloadSegmentSize = 0;
static bool s_bListFileSegmentsToDownload[MAX_FILE_SEGMENTS_TO_DOWNLOAD];
static u16 s_uListFileSegmentsSize[MAX_FILE_SEGMENTS_TO_DOWNLOAD];
static u8* s_pListFileSegments[MAX_FILE_SEGMENTS_TO_DOWNLOAD];
static u32 s_uCountFileSegmentsToDownload = 0;
static u32 s_uCountFileSegmentsDownloaded = 0;
static u32 s_uLastFileSegmentRequestTime = 0;
static u32 s_uLastTimeDownloadProgress = 0;

Menu* s_pMenuVehicleHWInfo = NULL;
Menu* s_pMenuUSBInfoVehicle = NULL;

int handle_commands_on_full_model_settings_received(u32 uVehicleId, int iResponseParam, u8* pData, int iLength)
{
   if ( (NULL == pData) || (iLength <= 0) )
      return -1;

   log_line("[Commands] Handling received full model settings, %d bytes, from VID %u, response param: %d", iLength, uVehicleId, iResponseParam);

   int iIndexRuntime = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
      {
         iIndexRuntime = i;
         break;
      }
   }

   if ( iIndexRuntime == -1 )
   {
      log_softerror_and_alarm("[Commands] Received model settings for vehicle not found in the runtime list. Ignoring it. Current runtime list:");
      log_current_runtime_vehicles_info();
      s_CommandType = 0;
      s_bHasCommandInProgress = false;
      return 0;
   }
   else
   {
      if ( g_VehiclesRuntimeInfo[iIndexRuntime].uTimeLastReceivedModelSettings == MAX_U32 )
         log_line("[Commands] First time receiving full model settings for vehicle runtime index %d, model's sw version: %d.%d (b-%d)", iIndexRuntime, get_sw_version_major(g_VehiclesRuntimeInfo[iIndexRuntime].pModel), get_sw_version_minor(g_VehiclesRuntimeInfo[iIndexRuntime].pModel), get_sw_version_build(g_VehiclesRuntimeInfo[iIndexRuntime].pModel));
      else
         log_line("[Commands] Last time received full model settings for vehicle runtime index %d, model's sw version: %d.%d (b-%d), was %u ms ago", iIndexRuntime, get_sw_version_major(g_VehiclesRuntimeInfo[iIndexRuntime].pModel), get_sw_version_minor(g_VehiclesRuntimeInfo[iIndexRuntime].pModel), get_sw_version_build(g_VehiclesRuntimeInfo[iIndexRuntime].pModel), g_TimeNow - g_VehiclesRuntimeInfo[iIndexRuntime].uTimeLastReceivedModelSettings);
   }

   if ( g_VehiclesRuntimeInfo[iIndexRuntime].uTimeLastReceivedModelSettings != MAX_U32 )
   if ( g_TimeNow < g_VehiclesRuntimeInfo[iIndexRuntime].uTimeLastReceivedModelSettings + 4000 )
   {
      log_line("[Commands] Received duplicate model settings for VID %u. Ignoring it. Last time received model settings for this VID was %d ms ago.",
         uVehicleId, g_TimeNow - g_VehiclesRuntimeInfo[iIndexRuntime].uTimeLastReceivedModelSettings );
      s_CommandType = 0;
      s_bHasCommandInProgress = false;

      // Reset temporary download model settings buffers
      for( int k=0; k<20; k++ )
      {
         g_VehiclesRuntimeInfo[iIndexRuntime].uSegmentsModelSettingsSize[k] = 0;
         g_VehiclesRuntimeInfo[iIndexRuntime].uSegmentsModelSettingsIds[k] = 0;
      }
      g_VehiclesRuntimeInfo[iIndexRuntime].uSegmentsModelSettingsCount = 0;
      log_line("[Commands] Reset temp model download buffers for VID %u, runtime index %d", uVehicleId, iIndexRuntime);
      return 0;
   }

   char szComm[256];
   sprintf(szComm, "rm -rf %s/model.mdl", FOLDER_RUBY_TEMP);
   hw_execute_bash_command(szComm, NULL);
   char szRecvFile[MAX_FILE_PATH_SIZE];
   sprintf(szRecvFile, "%s/last_recv_model.tar", FOLDER_RUBY_TEMP);
   if ( iResponseParam != 0 )
      sprintf(szRecvFile, "%s/last_recv_model.tar.gz", FOLDER_RUBY_TEMP);

   FILE* fd = fopen(szRecvFile, "wb");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to write received model settings to temporary model file [%s].", szRecvFile);
      return -1;
   }

   fwrite(pData, 1, iLength, fd);
   fclose(fd);
   fd = NULL;

   if ( 0 == iResponseParam )
   {
      sprintf(szComm, "tar -C %s -zxf %s/last_recv_model.tar 2>&1", FOLDER_RUBY_TEMP, FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);
   }
   else
   {
      sprintf(szComm, "gzip -df %s/last_recv_model.tar.gz 2>&1", FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);
      sprintf(szComm, "tar -C %s -xf %s/last_recv_model.tar 2>&1", FOLDER_RUBY_TEMP, FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);    
   }

   char szFile[MAX_FILE_PATH_SIZE];
   sprintf(szFile, "%s/model.mdl", FOLDER_RUBY_TEMP);
   if ( 0 == iResponseParam )
      sprintf(szFile, "%s/tmp/model.mdl", FOLDER_RUBY_TEMP);
   
   fd = fopen(szFile, "rb");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to read received model settings from temporary model file (%s).", szFile);
      return -1;
   }
   fseek(fd, 0, SEEK_END);
   long fsize = ftell(fd);
   fseek(fd, 0, SEEK_SET);
   fclose(fd);

   log_line("[Commands] Received temporary model settings uncompressed file size: %d bytes", (int)fsize);

   Model modelTemp;
   if ( ! modelTemp.loadFromFile(szFile, true) )
   {
      log_softerror_and_alarm("Failed to load temporary model file (%s).", szFile);
      sprintf(szComm, "cp -rf %s/last_recv_model.tar %s/last_error_model.tar", FOLDER_RUBY_TEMP, FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);
      if ( 0 == iResponseParam )
         sprintf(szComm, "cp -rf %s/tmp/model.mdl %s/last_error_model.mdl", FOLDER_RUBY_TEMP, FOLDER_RUBY_TEMP);
      else
         sprintf(szComm, "cp -rf %s/model.mdl %s/last_error_model.mdl", FOLDER_RUBY_TEMP, FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);
      return -1;
   }
   log_line("[Commands] Received full model settings for vehicle id %u.", modelTemp.uVehicleId);

   bool bFoundVehicle = false;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == modelTemp.uVehicleId )
      {
         bFoundVehicle = true;
         if ( g_VehiclesRuntimeInfo[i].uTimeLastReceivedModelSettings == MAX_U32 )
            log_line("[Commands] Was waiting for model settings for vehicle id %u? %s. Never received model settings for this vehicle.",
               g_VehiclesRuntimeInfo[i].uVehicleId, (g_VehiclesRuntimeInfo[i].bWaitingForModelSettings)?"Yes":"No");
         else
            log_line("[Commands] Was waiting for model settings for vehicle id %u? %s. Last received model settings for this vehicle: %u seconds ago.",
               g_VehiclesRuntimeInfo[i].uVehicleId, (g_VehiclesRuntimeInfo[i].bWaitingForModelSettings)?"Yes":"No",
               (g_TimeNow - g_VehiclesRuntimeInfo[i].uTimeLastReceivedModelSettings)/1000);
         if ( ! g_VehiclesRuntimeInfo[i].bWaitingForModelSettings )
         {
            bool bRadioConfigChanged = IsModelRadioConfigChanged( &modelTemp.radioLinksParams, &modelTemp.radioInterfacesParams,
                &(g_VehiclesRuntimeInfo[i].pModel->radioLinksParams),
                &(g_VehiclesRuntimeInfo[i].pModel->radioInterfacesParams));

            if ( bRadioConfigChanged )
               log_line("[Commands] Was not waiting for model settings for vehicle id %u, but it's radio configuration changed.", modelTemp.uVehicleId);
            else
            {
               bool bCameraChanged = false;
               if ( NULL != g_VehiclesRuntimeInfo[i].pModel )
               {
                  if ( modelTemp.iCameraCount != g_VehiclesRuntimeInfo[i].pModel->iCameraCount )
                     bCameraChanged = true;
                  if ( modelTemp.iCurrentCamera != g_VehiclesRuntimeInfo[i].pModel->iCurrentCamera )
                     bCameraChanged = true;
                  if ( (modelTemp.iCurrentCamera >= 0) && (modelTemp.iCameraCount > 0) )
                  if ( (g_VehiclesRuntimeInfo[i].pModel->iCurrentCamera >= 0 ) &&
                       (g_VehiclesRuntimeInfo[i].pModel->iCameraCount > 0) )
                  if ( (modelTemp.camera_params[modelTemp.iCurrentCamera].iCameraType != g_VehiclesRuntimeInfo[i].pModel->camera_params[g_VehiclesRuntimeInfo[i].pModel->iCurrentCamera].iCameraType ) ||
                       (modelTemp.camera_params[modelTemp.iCurrentCamera].iForcedCameraType != g_VehiclesRuntimeInfo[i].pModel->camera_params[g_VehiclesRuntimeInfo[i].pModel->iCurrentCamera].iForcedCameraType ) )
                     bCameraChanged = true;

                  if ( bCameraChanged )
                     log_line("[Commands] Was not waiting for model settings for vehicle id %u, but it's active camera changed.", modelTemp.uVehicleId);
                  else
                  //{
                     log_line("[Commands] Was not waiting for model settings for vehicle id %u and radio config did not changed.", modelTemp.uVehicleId);
                  //   return 0;
                  //}
               }
            }
         }
         // Reset temporary download model settings buffers
         for( int k=0; k<20; k++ )
         {
            g_VehiclesRuntimeInfo[i].uSegmentsModelSettingsSize[k] = 0;
            g_VehiclesRuntimeInfo[i].uSegmentsModelSettingsIds[k] = 0;
         }
         g_VehiclesRuntimeInfo[i].uSegmentsModelSettingsCount = 0;
         g_VehiclesRuntimeInfo[i].bWaitingForModelSettings = false;
         log_line("[Commands] Reset temp model download buffers for VID %u, runtime index %d", g_VehiclesRuntimeInfo[i].uVehicleId, i);
      
         if ( g_VehiclesRuntimeInfo[i].uTimeLastReceivedModelSettings != MAX_U32 )
         if ( g_VehiclesRuntimeInfo[i].uTimeLastReceivedModelSettings+5000 > g_TimeNow )
         {
            log_line("[Commands] Already has up to date model settings for vehicle id %u. Ignoring it.", modelTemp.uVehicleId);
            return 0;
         }
         g_VehiclesRuntimeInfo[i].uTimeLastReceivedModelSettings = g_TimeNow;
         break;
      }
   }
   if ( ! bFoundVehicle )
   {
      log_softerror_and_alarm("[Commands] Received model settings for unknown vehicle id %u. Skipping it.", modelTemp.uVehicleId);
      return -1;
   }

   if ( (NULL != g_pCurrentModel) && (modelTemp.uVehicleId != g_pCurrentModel->uVehicleId) )
      log_line("[Commands] Received model settings for a different vehicle (%u) than the current model (%u)", modelTemp.uVehicleId, g_pCurrentModel->uVehicleId);

   sprintf(szFile, "%s/model.mdl", FOLDER_RUBY_TEMP);
   if ( 0 == iResponseParam )
      sprintf(szFile, "%s/tmp/model.mdl", FOLDER_RUBY_TEMP);

   fd = fopen(szFile, "rb");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("[Commands] Failed to read received model settings from temporary model file (%s)", szFile);
      return -1;
   }
   
   u8 uBuffer[5048];
   int length = fread(uBuffer, 1, 5000, fd);
   fclose(fd);
   
   onEventReceivedModelSettings(modelTemp.uVehicleId, uBuffer, length, false);
   if ( 0 == iResponseParam )
      sprintf(szComm, "rm -rf %s/tmp/model.mdl", FOLDER_RUBY_TEMP);
   else
      sprintf(szComm, "rm -rf %s/model.mdl", FOLDER_RUBY_TEMP);
   hw_execute_bash_command(szComm, NULL);

   s_CommandType = 0;
   s_bHasCommandInProgress = false;
   return 0;
}

void _handle_received_command_response_to_get_all_params_zip(u8* pPacket, int iLength)
{
   s_iCountRetriesToGetModelSettingsCommand = 0;

   t_packet_header* pPH = (t_packet_header*)pPacket;
   t_packet_header_command_response* pPHCR = (t_packet_header_command_response*)(pPacket + sizeof(t_packet_header));

   u8* pDataBuffer = pPacket + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);
   int iDataLength = pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_command_response);
   
   // Did we a full, complete, single zip response?
   if ( iDataLength > 500 )
   {
      log_line("[Commands] Received model settings response (from VID %u) as full single compressed file. Model file size (compressed): %d, command response param: %d", pPH->vehicle_id_src, iDataLength, pPHCR->command_response_param);
      handle_commands_on_full_model_settings_received(pPH->vehicle_id_src, pPHCR->command_response_param, pDataBuffer, iDataLength);
      return;
   }
   
   Model* pModel = findModelWithId(pPH->vehicle_id_src, 6);
   if ( NULL == pModel )
   {
      log_softerror_and_alarm("[Commands] Received small segment (%d bytes) for model settings, but can't find VID %u", iDataLength, pPH->vehicle_id_src);
      return;
   }

   int iSegmentUniqueId = (int)(*pDataBuffer);
   int iSegmentIndex = (int)(*(pDataBuffer+1));
   int iTotalSegments = (int)(*(pDataBuffer+2));
   int iSegmentSize = (int)(*(pDataBuffer+3));
   
   if ( (iSegmentIndex < 0) || (iSegmentIndex > 20) )
   {
      log_softerror_and_alarm("[Commands] Received invalid small segment index %d for model settings. Ignoring it.", iSegmentIndex);
      return;
   }

   if ( (iTotalSegments < 1) || (iTotalSegments > 20) )
   {
      log_softerror_and_alarm("[Commands] Received invalid total small segments value %d for segment %d for model settings. Ignoring it.", iTotalSegments, iSegmentIndex);
      return;
   }

   if ( (iSegmentSize < 0) || (iSegmentSize > iDataLength - 4 * (int)sizeof(u8)) )
   {
      log_softerror_and_alarm("[Commands] Received invalid small segment size (%d bytes, expected %d bytes) for segment %d for model settings. Ignoring it.", iSegmentSize, iDataLength - 4*sizeof(u8), iSegmentIndex);
      return;
   }

   log_line("[Commands] Received model settings as small segment (%d of %d) from VID %u, unique transfer id %d, %d bytes (should be %d bytes)",
      iSegmentIndex+1, iTotalSegments, pPH->vehicle_id_src, iSegmentUniqueId, iSegmentSize, iDataLength - 4*sizeof(u8));

   int iRuntimeIndex = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == pPH->vehicle_id_src )
      {
         iRuntimeIndex = i;
         break;
      }
   }

   if ( -1 == iRuntimeIndex )
   {
      log_softerror_and_alarm("[Commands] Can't find the runtime info for VID %u", pPH->vehicle_id_src);
      return;
   }

   memcpy( &(g_VehiclesRuntimeInfo[iRuntimeIndex].pSegmentsModelSettingsDownload[iSegmentIndex][0]), pDataBuffer+4, iSegmentSize);
   g_VehiclesRuntimeInfo[iRuntimeIndex].uSegmentsModelSettingsIds[iSegmentIndex] = iSegmentUniqueId;
   g_VehiclesRuntimeInfo[iRuntimeIndex].uSegmentsModelSettingsSize[iSegmentIndex] = iSegmentSize;
   g_VehiclesRuntimeInfo[iRuntimeIndex].uSegmentsModelSettingsCount = iTotalSegments;
   
   bool bHasAll = true;
   int iTotalSize = 0;
   u8 bufferAll[4096];
   
   char szTmp[32];
   char szSegments[256];
   szSegments[0] = 0;

   for( int i=0; i<iTotalSegments; i++ )
   {
      if ( (int)g_VehiclesRuntimeInfo[iRuntimeIndex].uSegmentsModelSettingsIds[i] != iSegmentUniqueId )
      {
         bHasAll = false;
         continue;                   
      }
      if ( g_VehiclesRuntimeInfo[iRuntimeIndex].uSegmentsModelSettingsSize[i] == 0 )
      {
         bHasAll = false;
         continue;
      }
      
      sprintf(szTmp, " %d", i+1);
      strcat(szSegments, szTmp);
      if ( bHasAll )
      {
         memcpy(bufferAll + iTotalSize, &(g_VehiclesRuntimeInfo[iRuntimeIndex].pSegmentsModelSettingsDownload[i][0]), (int) g_VehiclesRuntimeInfo[iRuntimeIndex].uSegmentsModelSettingsSize[i]);
         iTotalSize += (int) g_VehiclesRuntimeInfo[iRuntimeIndex].uSegmentsModelSettingsSize[i];
      }
   }
   if ( bHasAll )
   {
      log_line("[Commands] Got all model settings segments. Total size: %d bytes, response param: %d", iTotalSize, pPHCR->command_response_param);
      handle_commands_on_full_model_settings_received(pPH->vehicle_id_src, pPHCR->command_response_param, bufferAll, iTotalSize);
   }
   else
      log_line("[Commands] Still hasn't all %d segments. Has these segments: [%s]", iTotalSegments, szSegments);
}

void handle_commands_send_current_command()
{
   if ( NULL == g_pCurrentModel )
      return;

   s_CommandStartTime = g_TimeNow;
   s_bHasCommandInProgress = true;

   t_packet_header PH;
   t_packet_header_command PHC;

   radio_packet_init(&PH, PACKET_COMPONENT_COMMANDS, PACKET_TYPE_COMMAND, STREAM_ID_DATA);
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
   PH.total_length = sizeof(t_packet_header)+sizeof(t_packet_header_command) + s_CommandBufferLength;
   

   PHC.command_type = s_CommandType;
   PHC.command_counter = s_CommandCounter;
   PHC.command_param = s_CommandParam;
   PHC.command_resend_counter = s_CommandResendCounter;
  
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), (u8*)&PHC, sizeof(t_packet_header_command));
   memcpy(buffer+sizeof(t_packet_header)+sizeof(t_packet_header_command), s_CommandBuffer, s_CommandBufferLength);
   
   send_packet_to_router(buffer, PH.total_length);
 
   log_line_commands("[Commands] [Sent] to vId %u, cmd nb. %d, retry %d, type [%s], param: %u, buff len: %d]", g_pCurrentModel->uVehicleId, s_CommandCounter, s_CommandResendCounter, commands_get_description(s_CommandType), s_CommandParam, s_CommandBufferLength);
}


bool handle_commands_start_on_pairing()
{
   log_line("[Commands] Handle start pairing...");
   if ( NULL != g_pCurrentModel )
      reset_model_settings_download_buffers(g_pCurrentModel->uVehicleId);

   s_iCountRetriesToGetModelSettingsCommand = 0;
   s_RetryGetCorePluginsCounter = 0;
   s_CommandLastProcessedResponseToCommandCounter = MAX_U32;
   s_bLastCommandSucceeded = false;

   s_bHasToSyncCorePluginsInfoFromVehicle = true;
   s_bHasReceivedVehicleCorePluginsInfo = false;

   s_uFileIdToDownload = 0;
   s_uFileToDownloadState = 0xFF;
   s_uCountFileSegmentsToDownload = 0;
   s_uCountFileSegmentsDownloaded = 0;
   s_uLastFileSegmentRequestTime = 0;
   s_uLastTimeDownloadProgress = 0;

   for( u32 u=0; u<MAX_FILE_SEGMENTS_TO_DOWNLOAD; u++ )
      s_pListFileSegments[u] = NULL;
   
   log_line("[Commands] Handled start pairing. Complete.");
   return true;
}

bool handle_commands_stop_on_pairing()
{
   log_line("[Commands] Handle stop pairing...");

   if ( 0 < s_uCountFileSegmentsToDownload )
   {
       for( u32 u=0; u<s_uCountFileSegmentsToDownload; u++ )
       {
          if ( NULL != s_pListFileSegments[u] )
             free(s_pListFileSegments[u]);
          s_pListFileSegments[u] = NULL;
       }
   }
   s_uFileIdToDownload = 0;
   s_uFileToDownloadState = 0xFF;
   s_uCountFileSegmentsToDownload = 0;
   s_uCountFileSegmentsDownloaded = 0;
   s_uLastFileSegmentRequestTime = 0;
   s_uLastTimeDownloadProgress = 0;

   log_line("[Commands] Handled stop pairing. Complete.");
   return true;
}


u8* handle_commands_get_last_command_response()
{
   return s_CommandReplyBuffer;
}

// Merge from Src to Dest
void merge_osd_params(osd_parameters_t* pParamsVehicleSrc, osd_parameters_t* pParamsCtrlDest)
{
   if ( NULL == pParamsVehicleSrc || NULL == pParamsCtrlDest )
      return;
         
   memcpy(pParamsCtrlDest, pParamsVehicleSrc, sizeof(osd_parameters_t));
}


void _handle_download_file_response()
{
   u32 uFileId = s_CommandParam;
   u8* pBuffer = &s_CommandReplyBuffer[0] + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);
   t_packet_header_download_file_info* pFileInfo = (t_packet_header_download_file_info*)pBuffer;
   log_line("[Commands]: Received file download request response from vehicle (file id %d, name: [%s]), segments: %u, segment size: %u, file state: %s", pFileInfo->file_id, pFileInfo->szFileName, pFileInfo->segments_count, pFileInfo->segment_size, pFileInfo->isReady?"file ready":"file preprocessing");

   if ( s_uFileToDownloadState == 0xFF )
   {
      warnings_add(0, "Waiting for vehicle to preprocess the file...");
      s_uFileIdToDownload = uFileId;
      s_uFileToDownloadState = pFileInfo->isReady;
   }

   if ( pFileInfo->isReady == 1 )
   {
      s_uLastFileSegmentRequestTime = g_TimeNow;
      s_uFileIdToDownload = uFileId;
      s_uFileToDownloadState = pFileInfo->isReady;
      s_uFileToDownloadSegmentSize = pFileInfo->segment_size;
      s_uCountFileSegmentsToDownload = pFileInfo->segments_count;
      s_uCountFileSegmentsDownloaded = 0;
      if ( s_uCountFileSegmentsToDownload >= MAX_FILE_SEGMENTS_TO_DOWNLOAD )
         s_uCountFileSegmentsToDownload = MAX_FILE_SEGMENTS_TO_DOWNLOAD-1;

      for( u32 u=0; u<s_uCountFileSegmentsToDownload; u++ )
      {
         s_bListFileSegmentsToDownload[u] = true;
         s_uListFileSegmentsSize[u] = 0;
         if ( NULL != s_pListFileSegments[u] )
            free(s_pListFileSegments[u]);
         s_pListFileSegments[u] = (u8*) malloc(s_uFileToDownloadSegmentSize);
      }

      s_uLastTimeDownloadProgress = g_TimeNow;

      if ( pFileInfo->file_id == FILE_ID_VEHICLE_LOGS_ARCHIVE )
      {
         char szComm[256];
         sprintf(szComm, "rm -rf %s/vehicle_logs.zip", FOLDER_RUBY_TEMP);
         hw_execute_bash_command(szComm, NULL);
         sprintf(szComm, "touch %s/vehicle_logs.zip", FOLDER_RUBY_TEMP);
         hw_execute_bash_command(szComm, NULL);
      }
   }
   else
   {
      s_uFileIdToDownload = uFileId;
      s_uFileToDownloadState = pFileInfo->isReady;
   }

   char szBuff[256];
   szBuff[0] = 0;

   if ( pFileInfo->isReady == 1 )
   {
      if ( pFileInfo->file_id == FILE_ID_VEHICLE_LOGS_ARCHIVE )
         strcpy(szBuff, "Downloading vehicle logs...");  
   }

   if ( 0 != szBuff[0] )
      warnings_add(0, szBuff);
}


void _handle_download_file_segment_response()
{
   t_packet_header* pPH = (t_packet_header*)s_CommandReplyBuffer;
   int length = pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_command_response);
   u8* pBuffer = &s_CommandReplyBuffer[0] + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);
   u32 flags = 0;
   memcpy((u8*)&flags, pBuffer, sizeof(u32));
   u32 uFileId = flags & 0xFFFF;
   u32 uFileSegment = (flags>>16);
   
   length -= sizeof(u32);
   pBuffer += sizeof(u32);
   log_line("[Commands]: Received file segment %d of %d from vehicle (for file id %d), lenght: %d bytes.", uFileSegment, s_uCountFileSegmentsToDownload, uFileId, length);

   if ( s_uFileIdToDownload == 0 )
      return;
   if ( s_uFileToDownloadState != 1 )
      return;

   if ( (uFileSegment != s_uCountFileSegmentsToDownload) && (length != (int)s_uFileToDownloadSegmentSize) )
      log_softerror_and_alarm("Received file segment of unexpected size. Expected %d bytes segment.", s_uFileToDownloadSegmentSize );

   if ( uFileSegment < s_uCountFileSegmentsToDownload )
   {
      if ( s_bListFileSegmentsToDownload[uFileSegment] )
         s_uCountFileSegmentsDownloaded++;
      s_bListFileSegmentsToDownload[uFileSegment] = false;
      s_uListFileSegmentsSize[uFileSegment] = (u16)length;
      if ( NULL != s_pListFileSegments[uFileSegment] )
         memcpy(s_pListFileSegments[uFileSegment], pBuffer, length);
   }

   bool bHasMoreSegmentsToDownload = false;
   u32 uTotalSize = 0;
   for( u32 u=0; u<s_uCountFileSegmentsToDownload; u++ )
   {
      if ( s_bListFileSegmentsToDownload[u] )
      {
         bHasMoreSegmentsToDownload = true;
         break;
      }
      uTotalSize += (u32) s_uListFileSegmentsSize[u];
   }

   if ( g_TimeNow > s_uLastTimeDownloadProgress + 5000 )
   {
      s_uLastTimeDownloadProgress = g_TimeNow;
      char szBuff[128];
      if ( s_uCountFileSegmentsToDownload > 0 )
         sprintf(szBuff, "Downloading %u%%", s_uCountFileSegmentsDownloaded*100 / s_uCountFileSegmentsToDownload );
      else
         strcpy(szBuff, "Downloading ...");
      warnings_add(0, szBuff);
   }

   if ( bHasMoreSegmentsToDownload )
      return;


   log_line("[Commands] Received entire file. File size: %u bytes.", uTotalSize);

   if ( uFileId == FILE_ID_VEHICLE_LOGS_ARCHIVE )
   {
      warnings_add(0, "Received complete vehicles logs.");

      char szComm[256];
      char szFile[MAX_FILE_PATH_SIZE];
      sprintf(szComm, "rm -rf %s/vehicle_logs.zip", FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);
      sprintf(szComm, "touch %s/vehicle_logs.zip", FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);

      strcpy(szFile, FOLDER_RUBY_TEMP);
      strcat(szFile, "vehicle_logs.zip");
      FILE* fd = fopen(szFile, "wb");
      if ( NULL != fd )
      {
         for( u32 u=0; u<s_uCountFileSegmentsToDownload; u++ )
            if ( s_uFileToDownloadSegmentSize != fwrite(s_pListFileSegments[u], 1, s_uFileToDownloadSegmentSize, fd) )
               log_softerror_and_alarm("[Commands] Failed to write vehicle logs zip file segment to storage.");
         fclose(fd);
      }
      else
         log_softerror_and_alarm("[Commands] Failed to write received vehicle logs zip file to storage (%s).", szFile);

      char szFolder[256];
      char szBuff[256];

      sprintf(szFolder, FOLDER_MEDIA_VEHICLE_DATA, g_pCurrentModel->uVehicleId);
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "mkdir -p %s", szFolder);
      hw_execute_bash_command(szComm, NULL);
      sprintf(szBuff, "logs_%s_%u_%u.zip", g_pCurrentModel->getShortName(), g_pCurrentModel->m_Stats.uTotalFlights, g_pCurrentModel->m_Stats.uTotalOnTime);
      for( int i=0; i<(int)strlen(szBuff); i++ )
         if ( szBuff[i] == ' ' )
            szBuff[i] = '-';
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "mv -f %s/vehicle_logs.zip %s/%s", FOLDER_RUBY_TEMP, szFolder, szBuff);
      hw_execute_bash_command(szComm, NULL);
   }

   if ( 0 < s_uCountFileSegmentsToDownload )
   {
       for( u32 u=0; u<s_uCountFileSegmentsToDownload; u++ )
       {
          if ( NULL != s_pListFileSegments[u] )
             free(s_pListFileSegments[u]);
          s_pListFileSegments[u] = NULL;
       }
   }
   s_uFileIdToDownload = 0;
   s_uFileToDownloadState = 0xFF;
   s_uCountFileSegmentsToDownload = 0;
}


// Returns true if the command will still be in progress

bool handle_last_command_result()
{
   if ( NULL == g_pCurrentModel )
      return false;

   log_line_commands( "[Commands] [Handling Response] from VID %u, cmd nb. %d, retry %d, type [%s], param: %u]", g_pCurrentModel->uVehicleId, s_CommandCounter, s_CommandResendCounter, commands_get_description(s_CommandType), s_CommandParam);

   if ( NULL != g_pCurrentModel )
      popup_log_add_entry("Received response from vehicle to command %s.", commands_get_description(s_CommandType));
   else
      popup_log_add_entry("Received response from vehicle to command %s.", commands_get_description(s_CommandType));

   if ( NULL == g_pCurrentModel )
   {
      log_softerror_and_alarm("[Commands] Command succeeded but current vehicle is NULL." );
      return false;
   }

   t_packet_header* pPH = (t_packet_header*)s_CommandReplyBuffer;
   t_packet_header_command_response* pPHCR = (t_packet_header_command_response*)(s_CommandReplyBuffer + sizeof(t_packet_header));

   char szBuff[1500];
   int tmp = 0, tmp1 = 0;
   u32 *pTmp32 = NULL;
   u8* pBuffer = NULL;
   Model modelTemp;

   int iDataLength = pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_command_response);
   const char* szTokens = "+";
   char* szWord = NULL;

   switch ( s_CommandType )
   {
      case COMMAND_ID_SET_TELEMETRY_TYPE_AND_PORT:
         {
            int iTelemetryType = s_CommandParam & 0x0F;
            int iSerialPort = (s_CommandParam & 0xF0) >> 4;
            int iSerialSpeed = (s_CommandParam >> 8);
            log_line("Received command response to set telemetry type to %d, serial port to %d and serial speed to %d",
                iTelemetryType, iSerialPort, iSerialSpeed);
            if ( NULL != g_pCurrentModel )
               g_pCurrentModel->setTelemetryTypeAndPort(iTelemetryType, iSerialPort, iSerialSpeed);
            saveControllerModel(g_pCurrentModel);
            send_model_changed_message_to_router(MODEL_CHANGED_SERIAL_PORTS, 0);
         }
         break;

      case COMMAND_ID_SET_THREADS_PRIORITIES:
        {
         memcpy((u8*)&(g_pCurrentModel->processesPriorities), s_CommandBuffer, sizeof(type_processes_priorities));
         log_line("Received confirmation for new threads priorities.");
         saveControllerModel(g_pCurrentModel);
         send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
        }
        break;

      case COMMAND_ID_GET_CORE_PLUGINS_INFO:
         {
         s_RetryGetCorePluginsCounter = 0;
         warnings_add(pPH->vehicle_id_src, "Got vehicle core plugins info.");
         s_bHasReceivedVehicleCorePluginsInfo = true;
         s_bHasToSyncCorePluginsInfoFromVehicle = false;

         u8* pTmp = s_CommandReplyBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);
         command_packet_core_plugins_response* pResponse = (command_packet_core_plugins_response*)pTmp;
         
         g_iVehicleCorePluginsCount = pResponse->iCountPlugins;
         if ( g_iVehicleCorePluginsCount < 0 || g_iVehicleCorePluginsCount > MAX_CORE_PLUGINS_COUNT )
            g_iVehicleCorePluginsCount = 0;
         for( int i=0; i<g_iVehicleCorePluginsCount; i++ )
            memcpy((u8*)&(g_listVehicleCorePlugins[i]), (u8*)&(pResponse->listPlugins[i]), sizeof(CorePluginSettings));
         }
         break;

      case COMMAND_ID_UPLOAD_FILE_SEGMENT:
         {
             if ( g_CurrentUploadingFile.uLastSegmentIndexUploaded < g_CurrentUploadingFile.uTotalSegments )
                g_CurrentUploadingFile.bSegmentsUploaded[g_CurrentUploadingFile.uLastSegmentIndexUploaded] = true;

             bool bAllUploaded = false;
             for( u32 u=0; u<g_CurrentUploadingFile.uTotalSegments; u++ )
                if ( ! g_CurrentUploadingFile.bSegmentsUploaded[u] )
                {
                   bAllUploaded = false;
                   break;
                }

             if ( bAllUploaded )
                g_bHasFileUploadInProgress = false;
         }
         break;

      case COMMAND_ID_GET_SIK_CONFIG:
         {
            MenuDiagnoseRadioLink* pMenu = (MenuDiagnoseRadioLink*) menu_get_menu_by_id(MENU_ID_DIAGNOSE_RADIO_LINK);
            if ( NULL != pMenu )
            {
               pBuffer = s_CommandReplyBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);
               pMenu->onReceivedVehicleData( pBuffer, iDataLength);
            }
         }
         break;

      case COMMAND_ID_CLEAR_LOGS:
      {
         log_line("[Commands] Received confirmation from vehicle that logs have been deleted. Parameter: %d", (int)s_CommandParam);
         if ( 1 == s_CommandParam )
         {
            char szTextW[256];
            sprintf(szTextW, "Your %s logs have been deleted.", g_pCurrentModel->getVehicleTypeString());
            MenuConfirmation* pMC = new MenuConfirmation("Confirmation", szTextW, -1, true);
            pMC->m_yPos = 0.3;
            add_menu_to_stack(pMC);
         }
         break;
      }

      case COMMAND_ID_UPLOAD_CALIBRATION_FILE:
      {
         send_pause_adaptive_to_router(3000);
         t_packet_header_command_upload_calib_file* pParams = (t_packet_header_command_upload_calib_file*)s_CommandBuffer;
         g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraBinProfile = pParams->calibration_file_type;
         if ( 0 == pParams->calibration_file_type )
         {
            g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName[0] = 0;
            saveControllerModel(g_pCurrentModel);
            send_model_changed_message_to_router(MODEL_CHANGED_CAMERA_CALIBRATION_FILE, 0);
            menu_refresh_all_menus();
         }
         else if ( pParams->is_last_segment )
         {
            strncpy(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName, pParams->szFileName, MAX_CAMERA_BIN_PROFILE_NAME);
            saveControllerModel(g_pCurrentModel);
            send_model_changed_message_to_router(MODEL_CHANGED_CAMERA_CALIBRATION_FILE, 0);
            menu_refresh_all_menus();
         }
         break;
      }

      case COMMAND_ID_SET_RELAY_PARAMETERS:
         {
            g_uTimeLastRelaySettingsChanged = g_TimeNow;
            type_relay_parameters* pParams = (type_relay_parameters*)s_CommandBuffer;
            type_relay_parameters oldRelayParams;
            memcpy(&oldRelayParams, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));
            memcpy(&(g_pCurrentModel->relay_params), pParams, sizeof(type_relay_parameters));
            g_pCurrentModel->validateRadioSettings();
            saveControllerModel(g_pCurrentModel);
            
            log_line("[HandleCommands] Finished updating model. Now doing relay state updates...");
            if ( (g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId < 0) ||
                 (g_pCurrentModel->relay_params.uRelayedVehicleId == 0) )
            {
               log_line("[HandleCommands] Relaing was disabled. Remove relayed node runtime info.");
               for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
               {
                  if ( oldRelayParams.uRelayedVehicleId == g_VehiclesRuntimeInfo[i].uVehicleId )
                     reset_vehicle_runtime_info(&(g_VehiclesRuntimeInfo[i]));
               }
            }

            // If relayed vehicle changed
            if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
            if ( g_pCurrentModel->relay_params.uRelayedVehicleId != 0 )
            if ( oldRelayParams.uRelayedVehicleId != g_pCurrentModel->relay_params.uRelayedVehicleId )
            {
                log_line("[HandleCommands] Relayed vehicle changed. Find empty slot...");
                int iIndexEmptySlot = -1;
                for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
                {
                   if ( (g_VehiclesRuntimeInfo[i].uVehicleId == g_pCurrentModel->relay_params.uRelayedVehicleId) ||
                        (g_VehiclesRuntimeInfo[i].uVehicleId == oldRelayParams.uRelayedVehicleId) )
                      reset_vehicle_runtime_info(&(g_VehiclesRuntimeInfo[i]));

                   if ( -1 == iIndexEmptySlot )
                   if ( g_VehiclesRuntimeInfo[i].uVehicleId == 0 )
                       iIndexEmptySlot = i;
                }
                if ( iIndexEmptySlot == -1 )
                {
                   iIndexEmptySlot = MAX_CONCURENT_VEHICLES-1;
                   log_softerror_and_alarm("[HandleCommands] No more room in vehicles runtime info structure for new relayed vehicle VID: %u. Reuse last index: %d", g_pCurrentModel->relay_params.uRelayedVehicleId, iIndexEmptySlot);
                   char szTmp[256];
                   szTmp[0] = 0;
                   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
                   {
                      char szT[32];
                      sprintf(szT, "%u", g_VehiclesRuntimeInfo[i].uVehicleId);
                      if ( 0 != i )
                         strcat(szTmp, ", ");
                      strcat(szTmp, szT);
                   }
                   log_softerror_and_alarm("[HandleCommands] Current vehicles in vehicles runtime info: [%s]", szTmp);                   
                }
                log_line("[HandleCommands] Assign vehicle runtime index %d (currently has VID: %u) to relayed node VID %u",
                    iIndexEmptySlot, g_VehiclesRuntimeInfo[iIndexEmptySlot].uVehicleId, g_pCurrentModel->relay_params.uRelayedVehicleId);
                g_VehiclesRuntimeInfo[iIndexEmptySlot].uVehicleId = g_pCurrentModel->relay_params.uRelayedVehicleId;
                g_VehiclesRuntimeInfo[iIndexEmptySlot].pModel = findModelWithId(g_VehiclesRuntimeInfo[iIndexEmptySlot].uVehicleId, 7);
            }

            u8 uOldRelayMode = oldRelayParams.uCurrentRelayMode;
            oldRelayParams.uCurrentRelayMode = g_pCurrentModel->relay_params.uCurrentRelayMode;
         
            // Only relay mode changed?

            if ( 0 == memcmp(&oldRelayParams, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters)) )
            {
               if ( uOldRelayMode != g_pCurrentModel->relay_params.uCurrentRelayMode )
               {
                  char szTmp1[64];
                  char szTmp2[64];
                  strncpy(szTmp1, str_format_relay_mode(uOldRelayMode), 63);
                  strncpy(szTmp2, str_format_relay_mode(g_pCurrentModel->relay_params.uCurrentRelayMode), 63);
                  log_line("[HandleCommands] Recv response confirmation to change relay mode from %u to %u (%s to %s)", uOldRelayMode, g_pCurrentModel->relay_params.uCurrentRelayMode, szTmp1, szTmp2);
                  onEventRelayModeChanged();
                  send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_RELAY_MODE_SWITCHED, g_pCurrentModel->relay_params.uCurrentRelayMode);
               }
               else
                  log_line("[HandleCommands] No change in relay flags or relay mode detected.");
               break;
            }
            else
            {
               log_line("[HandleCommands] Relay flags and parameters changed. Notify router about the change.");
               send_model_changed_message_to_router(MODEL_CHANGED_RELAY_PARAMS, 0);
            }
            break;
         }

      case COMMAND_ID_SET_ENCRYPTION_PARAMS:
         g_pCurrentModel->enc_flags = s_CommandBuffer[0];
         saveControllerModel(g_pCurrentModel);
         send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
         break;

      case COMMAND_ID_FACTORY_RESET:
        {
           pairing_stop();
           ruby_set_active_model_id(0);
           deleteModel(g_pCurrentModel);
           deletePluginModelSettings(g_pCurrentModel->uVehicleId);
           g_pCurrentModel = NULL;
           save_PluginsSettings();
           log_line("[Commands] Command response factory reset: Deleted model 1/3.");
           menu_discard_all();
           log_line("[Commands] Command response factory reset: Deleted model 2/3.");
           //Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"Factory Reset Complete",NULL);
           //pm->m_xPos = 0.4; pm->m_yPos = 0.4;
           //pm->m_Width = 0.36;
           //pm->addTopLine("Your vehicle was reset to default settings (including name, id, frequencies) and the full configuration is as on a fresh instalation. It will reboot now.");
           //pm->addTopLine("You need to search for it again and pair with the vehicle as with a new vehicle.");
           MenuConfirmation* pM = new MenuConfirmation("Factory Reset Complete", "Your vehicle was reset to default settings (including name, id, frequencies) and the full configuration is as on a fresh instalation. It will reboot now.", 0, true);
           pM->addTopLine("You need to search for it again and pair with the vehicle as with a new vehicle.");
           add_menu_to_stack(pM);
           log_line("[Commands] Command response factory reset: Deleted model 2/3.");
        }
        break;

      case COMMAND_ID_SET_SIK_PACKET_SIZE:
         g_pCurrentModel->radioLinksParams.iSiKPacketSize = (int)s_CommandParam;
         saveControllerModel(g_pCurrentModel);  
         send_model_changed_message_to_router(MODEL_CHANGED_SIK_PACKET_SIZE, s_CommandParam);
         break;

      case COMMAND_ID_SET_VEHICLE_BOARD_TYPE:
         {
            log_line("Received confirmation on set board type. Set board type to: %s", str_get_hardware_board_name(s_CommandParam));
            g_pCurrentModel->hwCapabilities.uBoardType = s_CommandParam;
            if ( (g_pCurrentModel->hwCapabilities.uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_SIGMASTAR_338Q )
            if ( ((g_pCurrentModel->hwCapabilities.uBoardType & BOARD_SUBTYPE_MASK) >> BOARD_SUBTYPE_SHIFT) == BOARD_SUBTYPE_OPENIPC_AIO_EMAX_MINI )
            {
               g_pCurrentModel->radioInterfacesParams.interface_card_model[0] = -CARD_MODEL_RTL8812AU_AF1;
            }
            g_pCurrentModel->resetAudioParams();
            saveControllerModel(g_pCurrentModel);
            send_model_changed_message_to_router(MODEL_CHANGED_AUDIO_PARAMS, 0);
            if ( menu_has_menu(MENU_ID_VEHICLE_BOARD) )
            {
               Menu* pMenu = menu_get_menu_by_id(MENU_ID_VEHICLE_BOARD);
               remove_menu_from_stack(pMenu);
            }
            menu_refresh_all_menus();
            char szName[128];
            snprintf(szName, sizeof(szName)/sizeof(szName[0]), "Did set vehicle type to: %s", str_get_hardware_board_name(s_CommandParam));
            warnings_add(g_pCurrentModel->uVehicleId, szName, g_idIconCPU);
            if ( onEventPairingTookUIActions() )
               onEventFinishedPairUIAction();
         }
         break;

      case COMMAND_ID_SET_TEMPERATURE_THRESHOLD:
         {
           g_pCurrentModel->hwCapabilities.uHWFlags &= ~(0x0000FF00);
           g_pCurrentModel->hwCapabilities.uHWFlags |= ((s_CommandParam & 0xFF) << 8);
           saveControllerModel(g_pCurrentModel);
           send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
         }
         break;

      case COMMAND_ID_SET_PIT_AUTO_TX_POWERS_FLAGS:
         {
           g_pCurrentModel->radioInterfacesParams.uFlagsRadioInterfaces = (s_CommandParam & 0xFF);
           g_pCurrentModel->hwCapabilities.uHWFlags &= ~(0x0000FF00);
           g_pCurrentModel->hwCapabilities.uHWFlags |= (s_CommandParam & 0xFF00);

           if ( s_CommandParam & (((u32)(0x01)) << 16) )
              g_pCurrentModel->radioInterfacesParams.iAutoVehicleTxPower = 1;
           else
              g_pCurrentModel->radioInterfacesParams.iAutoVehicleTxPower = 0;

           if ( s_CommandParam & (((u32)(0x01)) << 17) )
              g_pCurrentModel->radioInterfacesParams.iAutoControllerTxPower = 1;
           else
              g_pCurrentModel->radioInterfacesParams.iAutoControllerTxPower = 0;
           
           saveControllerModel(g_pCurrentModel);
           send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
         }
         break;

      case COMMAND_ID_SET_DEVELOPER_FLAGS:
         {
           u32 uTmp = 0;
           memcpy(&uTmp, &s_CommandBuffer[0], sizeof(u32));
           g_pCurrentModel->uDeveloperFlags = uTmp;
           saveControllerModel(g_pCurrentModel);
           log_line("[Commands] Vehicle new development flags: %u (%s)", g_pCurrentModel->uDeveloperFlags, str_get_developer_flags(g_pCurrentModel->uDeveloperFlags));
           send_model_changed_message_to_router(MODEL_CHANGED_DEVELOPER_FLAGS, 0);
         }
         break;

      case COMMAND_ID_SET_MODEL_FLAGS:
         g_pCurrentModel->uModelFlags = s_CommandParam;
         saveControllerModel(g_pCurrentModel);  
         send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
         break;


      case COMMAND_ID_GET_MODULES_INFO:
         pBuffer = s_CommandReplyBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);
         
         if ( 0 == s_CommandParam )
         {
            s_pMenuVehicleHWInfo = new Menu(0,"Vehicle Modules Info",NULL);
            s_pMenuVehicleHWInfo->m_xPos = 0.18; s_pMenuVehicleHWInfo->m_yPos = 0.16;
            s_pMenuVehicleHWInfo->m_Width = 0.6;
            s_pMenuVehicleHWInfo->addTopLine(" ");
            add_menu_to_stack(s_pMenuVehicleHWInfo);

            strncpy(szBuff, (const char*)pBuffer, sizeof(szBuff)/sizeof(szBuff[0]));
            removeNewLines(szBuff);
            szWord = strtok(szBuff, "#");
            while( NULL != szWord )
            {
               s_pMenuVehicleHWInfo->addTopLine(szWord);
               szWord = strtok(NULL, "#");
            }
         }
         if ( 1 == s_CommandParam )
         {
            s_pMenuVehicleHWInfo = new Menu(0,"Vehicle Hardware Info",NULL);
            s_pMenuVehicleHWInfo->m_xPos = 0.18; s_pMenuVehicleHWInfo->m_yPos = 0.16;
            s_pMenuVehicleHWInfo->m_Width = 0.48;
            s_pMenuVehicleHWInfo->addTopLine(" ");         
            add_menu_to_stack(s_pMenuVehicleHWInfo);

            strncpy(szBuff, (const char*)pBuffer, sizeof(szBuff)/sizeof(szBuff[0]));
            removeNewLines(szBuff);
            szWord = strtok(szBuff, "#");
            while( NULL != szWord )
            {
               s_pMenuVehicleHWInfo->addTopLine(szWord);
               szWord = strtok(NULL, "#");
            }
         }
         break;

      case COMMAND_ID_GET_CURRENT_VIDEO_CONFIG:
         pBuffer = s_CommandReplyBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);
         s_pMenuVehicleHWInfo = new Menu(0,"Vehicle Hardware Info",NULL);
         s_pMenuVehicleHWInfo->m_xPos = 0.32; s_pMenuVehicleHWInfo->m_yPos = 0.17;
         s_pMenuVehicleHWInfo->m_Width = 0.6;
         sprintf(szBuff, "Board type: %s (id: %u.%u), software version: %u.%u (b-%u)", str_get_hardware_board_name(g_pCurrentModel->hwCapabilities.uBoardType), (g_pCurrentModel->hwCapabilities.uBoardType & BOARD_TYPE_MASK), (g_pCurrentModel->hwCapabilities.uBoardType & BOARD_SUBTYPE_MASK) >> BOARD_SUBTYPE_SHIFT, get_sw_version_major(g_pCurrentModel), get_sw_version_minor(g_pCurrentModel), get_sw_version_build(g_pCurrentModel));
         s_pMenuVehicleHWInfo->addTopLine(szBuff);
         s_pMenuVehicleHWInfo->addTopLine(" ");
         s_pMenuVehicleHWInfo->addTopLine(" ");
         
         for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         {
            sprintf(szBuff, "Radio Interface %d: %s, USB port %s,  %s, driver %s", i+1, g_pCurrentModel->radioInterfacesParams.interface_szMAC[i], g_pCurrentModel->radioInterfacesParams.interface_szPort[i], str_get_radio_type_description(g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[i]), str_get_radio_driver_description((g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[i]>>8) & 0xFF));
            s_pMenuVehicleHWInfo->addTopLine(szBuff);
            sprintf(szBuff, ". . . currently at %s, supported bands: ", str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[i]));
            if ( g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_433 )
               strcat(szBuff, "433 ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_868 )
               strcat(szBuff, "868 ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_915 )
               strcat(szBuff, "915 ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_23 )
               strcat(szBuff, "2.3 ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_24 )
               strcat(szBuff, "2.4 ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_25 )
               strcat(szBuff, "2.5 ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_58 )
               strcat(szBuff, "5.8 ");
            s_pMenuVehicleHWInfo->addTopLine(szBuff);
            sprintf(szBuff, ". . . flags: ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
               strcat(szBuff, "TX/RX, ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
            if ( !( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
               strcat(szBuff, "TX Only, ");
            if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
               strcat(szBuff, "RX Only, ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
               strcat(szBuff, "Video, ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
               strcat(szBuff, "Data, ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
               strcat(szBuff, "Relay");
            s_pMenuVehicleHWInfo->addTopLine(szBuff);
         }

         add_menu_to_stack(s_pMenuVehicleHWInfo);
         s_pMenuVehicleHWInfo->addTopLine(" ");
         s_pMenuVehicleHWInfo->addTopLine(" ");
         strcpy(szBuff, (const char*)pBuffer);

            szWord = strtok(szBuff, "#");
            while( NULL != szWord )
            {    
               s_pMenuVehicleHWInfo->addTopLine(szWord);
               szWord = strtok(NULL, "#");
            }

            s_bHasCommandInProgress = false;
            handle_commands_send_to_vehicle(COMMAND_ID_GET_MEMORY_INFO, 0, NULL, 0);
            return true;
            break;

      case COMMAND_ID_GET_CPU_PROCS_INFO:
         {
            pBuffer = s_CommandReplyBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);
            pBuffer[iDataLength-1] = 0;
            static MenuInfoProcs* s_pMenuInfoProcs = NULL;

            log_line("Received cpu proc debug info, for segment %d, %d bytes, menu now: %x", s_CommandParam, iDataLength, s_pMenuInfoProcs);
            if ( (0 == s_CommandParam) && (NULL == s_pMenuInfoProcs) )
            {
               s_pMenuInfoProcs = new MenuInfoProcs((char*)pBuffer);
               add_menu_to_stack(s_pMenuInfoProcs);
               s_bHasCommandInProgress = false;
               handle_commands_send_to_vehicle(COMMAND_ID_GET_CPU_PROCS_INFO, 1, NULL, 0);
               return true;
            }
            if ( (1 == s_CommandParam) && (NULL != s_pMenuInfoProcs) )
            {
               s_pMenuInfoProcs->appendInfo((char*)pBuffer);
               s_pMenuInfoProcs = NULL;
            }
         }
         break;

      case COMMAND_ID_GET_MEMORY_INFO:
         pBuffer = s_CommandReplyBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);
         pTmp32 = (u32*)pBuffer;
         if ( iDataLength >= 2*(int)sizeof(u32) )
         {
            sprintf(szBuff, "Memory: %u Mb free out of %u Mb total", *(pTmp32+1), *pTmp32);
            s_pMenuVehicleHWInfo->addTopLine(" ");
            s_pMenuVehicleHWInfo->addTopLine(szBuff);
         }
         if ( iDataLength > 2*(int)sizeof(u32) )
         {
            *(pBuffer + iDataLength-1) = 0;
            sprintf(szBuff, "Logs: %s", pBuffer + 2*sizeof(u32));
            s_pMenuVehicleHWInfo->addTopLine(szBuff);
         }
         s_bHasCommandInProgress = false;
         handle_commands_send_to_vehicle(COMMAND_ID_GET_CPU_INFO, 0, NULL, 0);
         return true;
         break;

      case COMMAND_ID_GET_CPU_INFO:
         pBuffer = s_CommandReplyBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);
         s_pMenuVehicleHWInfo->addTopLine(" ");
         //s_pMenuVehicleHWInfo->addTopLine("CPU Info:");

         strcpy(szBuff, (const char*)pBuffer);
         szWord = strtok(szBuff, szTokens);
         while( NULL != szWord )
         {
            log_line("[Commands] Add vehicle info to menu: [%s]", szWord);
            s_pMenuVehicleHWInfo->addTopLine(szWord);
            szWord = strtok(NULL, szTokens);
         }
         break;

      case COMMAND_ID_GET_ALL_PARAMS_ZIP:
      {
         _handle_received_command_response_to_get_all_params_zip((u8*)pPH, pPH->total_length);
         break;
      }

     case COMMAND_ID_GET_USB_INFO:
     {
        s_pMenuUSBInfoVehicle = new Menu(0,"Vehicle USB Radio Interfaces Info",NULL);
        s_pMenuUSBInfoVehicle->m_xPos = 0.18;
        s_pMenuUSBInfoVehicle->m_yPos = 0.15;
        s_pMenuUSBInfoVehicle->m_Width = 0.56;
        add_menu_to_stack(s_pMenuUSBInfoVehicle);

        pBuffer = s_CommandReplyBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);
        log_line("[Commands] Received %d bytes for vehicle USB radio interfaces info.", iDataLength);
        
        char szComm[256];
        char szFileUSB[MAX_FILE_PATH_SIZE];
        sprintf(szFileUSB, "%s/tmp_usb_info.tar", FOLDER_RUBY_TEMP);
        if ( pPHCR->command_response_param != 0 )
           sprintf(szFileUSB, "%s/tmp_usb_info.tar.gz", FOLDER_RUBY_TEMP);
        log_line("Storing received USB info to file: [%s]", szFileUSB);
        char szText[3000];
        FILE* fd = fopen(szFileUSB, "wb");
        if ( NULL != fd )
        {
           fwrite(pBuffer, 1, iDataLength, fd);
           fclose(fd);
           if ( pPHCR->command_response_param == 0 )
           {
              sprintf(szComm, "tar -C %s -zxf %s 2>&1", FOLDER_RUBY_TEMP, szFileUSB);
              hw_execute_bash_command(szComm, NULL);
           }
           else
           {
              sprintf(szComm, "gzip -df %s 2>&1", szFileUSB);
              hw_execute_bash_command(szComm, NULL);
              sprintf(szComm, "tar -C %s -xf %s/tmp_usb_info.tar 2>&1", FOLDER_RUBY_TEMP, FOLDER_RUBY_TEMP);
              hw_execute_bash_command(szComm, NULL);
           }
           iDataLength = 0;
           strcpy(szFileUSB, FOLDER_RUBY_TEMP);
           strcat(szFileUSB, "tmp_usb_info.txt");
           fd = fopen(szFileUSB, "rb");
           if ( NULL != fd )
           {
              iDataLength = fread(szText,1,3000, fd);
              fclose(fd);
           }
        }
        else
        {
           log_softerror_and_alarm("[Commands] Failed to write file with vehicle USB radio info: [%s]", szFileUSB);
           iDataLength = 0;
        }

        if ( iDataLength <= 0 )
           s_pMenuUSBInfoVehicle->addTopLine("Received invalid info.");
        else
           log_line("[Commands] Received %d uncompressed bytes for vehicle USB radio interfaces info.", iDataLength);

        int iPos = 0;
        int iStartLine = iPos;
        while (iPos < iDataLength )
        {
           if ( szText[iPos] == 10 || szText[iPos] == 13 || szText[iPos] == 0 )
           {
              szText[iPos] = 0;
              if ( iPos > iStartLine + 2 )
                 s_pMenuUSBInfoVehicle->addTopLine(szText+iStartLine);
              iStartLine = iPos+1;
           }
           iPos++;
        }
        szText[iPos] = 0;
        if ( iPos > iStartLine + 2 )
          s_pMenuUSBInfoVehicle->addTopLine(szText+iStartLine);

        s_bHasCommandInProgress = false;
        handle_commands_send_to_vehicle(COMMAND_ID_GET_USB_INFO2, 0, NULL, 0);
        return true;
        break;
     }

     case COMMAND_ID_GET_USB_INFO2:
     {
        if ( NULL == s_pMenuUSBInfoVehicle )
           break;
        s_pMenuUSBInfoVehicle->addTopLine("List:");
        pBuffer = s_CommandReplyBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);
        log_line("[Commands] Received %d bytes for vehicle USB radio interfaces info2.", iDataLength);
        char szComm[256];
        char szFileUSB[MAX_FILE_PATH_SIZE];
        sprintf(szFileUSB, "%s/tmp_usb_info2.tar", FOLDER_RUBY_TEMP);
        if ( pPHCR->command_response_param != 0 )
           sprintf(szFileUSB, "%s/tmp_usb_info2.tar.gz", FOLDER_RUBY_TEMP);
        log_line("Storing received USB2 info to file: [%s]", szFileUSB);

        char szText[3000];
        FILE* fd = fopen(szFileUSB, "wb");
        if ( NULL != fd )
        {
           fwrite(pBuffer, 1, iDataLength, fd);
           fclose(fd);

           if ( pPHCR->command_response_param == 0 )
           {
              sprintf(szComm, "tar -C %s -zxf %s 2>&1", FOLDER_RUBY_TEMP, szFileUSB);
              hw_execute_bash_command(szComm, NULL);
           }
           else
           {
              sprintf(szComm, "gzip -df %s 2>&1", szFileUSB);
              hw_execute_bash_command(szComm, NULL);
              sprintf(szComm, "tar -C %s -xf %s/tmp_usb_info2.tar 2>&1", FOLDER_RUBY_TEMP, FOLDER_RUBY_TEMP);
              hw_execute_bash_command(szComm, NULL);
           }

           iDataLength = 0;
           strcpy(szFileUSB, FOLDER_RUBY_TEMP);
           strcat(szFileUSB, "tmp_usb_info2.txt");
           fd = fopen(szFileUSB, "rb");
           if ( NULL != fd )
           {
              iDataLength = fread(szText,1,3000, fd);
              fclose(fd);
           }
        }
        else
        {
           log_softerror_and_alarm("[Commands] Failed to write file with vehicle USB radio info2: [%s]", szFileUSB);
           iDataLength = 0;
        }

        if ( iDataLength <= 0 )
           s_pMenuUSBInfoVehicle->addTopLine("Received invalid info 2.");
        else
           log_line("[Commands] Received %d uncompressed bytes for vehicle USB radio interfaces info2.", iDataLength);

        int iPos = 0;
        int iStartLine = iPos;
        while (iPos < iDataLength )
        {
           if ( szText[iPos] == 10 || szText[iPos] == 13 || szText[iPos] == 0 )
           {
              szText[iPos] = 0;
              if ( iPos > iStartLine + 2 )
                 s_pMenuUSBInfoVehicle->addTopLine(szText+iStartLine);
              iStartLine = iPos+1;
           }
           iPos++;
        }
        szText[iPos] = 0;
        if ( iPos > iStartLine + 2 )
          s_pMenuUSBInfoVehicle->addTopLine(szText+iStartLine);
        break;
      }

      case COMMAND_ID_SET_VEHICLE_TYPE:
         log_line("[Commands] Command set vehicle type succeeded, vehicle type: %d", s_CommandParam );
         g_pCurrentModel->vehicle_type &= MODEL_FIRMWARE_MASK;
         g_pCurrentModel->vehicle_type |= (s_CommandParam & MODEL_TYPE_MASK);
         g_pCurrentModel->constructLongName();
         saveControllerModel(g_pCurrentModel);         
         break;

      case COMMAND_ID_SET_ENABLE_DHCP:
         log_line("[Commands] Command enable DHCP (0/1) succeeded, enabled: %d", s_CommandParam );
         g_pCurrentModel->enableDHCP = (bool)s_CommandParam;
         saveControllerModel(g_pCurrentModel);         
         break;

      case COMMAND_ID_SET_RC_CAMERA_PARAMS:
         g_pCurrentModel->camera_rc_channels = s_CommandParam;
         saveControllerModel(g_pCurrentModel);         
         send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
         break;

      case COMMAND_ID_SET_RADIO_INTERFACE_CAPABILITIES:
         tmp = (int)(s_CommandParam & 0xFF);
         if ( (tmp >= 0) && (tmp < g_pCurrentModel->radioInterfacesParams.interfaces_count) )
         {
            u32 uOldCapabilities = g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[tmp];
            g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[tmp] &= 0xFF000000;
            g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[tmp] |= ((s_CommandParam >> 8) & 0xFFFFFF);
            saveControllerModel(g_pCurrentModel);
            if ( (uOldCapabilities & (RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_4W | RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_2W)) !=
                 (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[tmp] & (RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_4W | RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_2W)) )
               send_model_changed_message_to_router(MODEL_CHANGED_RADIO_POWERS, 0);
            else
               send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
         }
         break;

      case COMMAND_ID_SET_RADIO_LINKS_FLAGS:
         g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags = s_CommandParam;
         saveControllerModel(g_pCurrentModel);
         send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
         break;

      case COMMAND_ID_SET_OVERCLOCKING_PARAMS:
         {
            command_packet_overclocking_params params;
            memcpy(&params, s_CommandBuffer, sizeof(command_packet_overclocking_params));
            g_pCurrentModel->processesPriorities.iFreqARM = params.freq_arm;
            g_pCurrentModel->processesPriorities.iFreqGPU = params.freq_gpu;
            g_pCurrentModel->processesPriorities.iOverVoltage = params.overvoltage;
            saveControllerModel(g_pCurrentModel);
            send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
         }
         break;

      case COMMAND_ID_SET_FUNCTIONS_TRIGGERS_PARAMS:
         {
            memcpy(&(g_pCurrentModel->functions_params), s_CommandBuffer, sizeof(type_functions_parameters));
            saveControllerModel(g_pCurrentModel);
            send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
         }
         break;

      case COMMAND_ID_ROTATE_RADIO_LINKS:
         {
            log_line("[Commands] Received response from vehicle to rotate radio links command.");
            g_pCurrentModel->rotateRadioLinksOrder();
            saveControllerModel(g_pCurrentModel);
            send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, MODEL_CHANGED_ROTATED_RADIO_LINKS);
            menu_refresh_all_menus();
         }
         break;

      case COMMAND_ID_SWAP_RADIO_INTERFACES:
         {
            log_line("[Commands] Received response from vehicle to swap radio interfaces command.");
            if ( g_pCurrentModel->canSwapEnabledHighCapacityRadioInterfaces() )
            {
               g_pCurrentModel->swapEnabledHighCapacityRadioInterfaces();
               saveControllerModel(g_pCurrentModel);
               send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, MODEL_CHANGED_SWAPED_RADIO_INTERFACES);
               menu_refresh_all_menus();
            }
         }
         break;

      case COMMAND_ID_SET_CAMERA_PARAMETERS:
         tmp = s_CommandParam;
         if ( tmp < 0 || tmp >= g_pCurrentModel->iCameraCount )
            tmp = 0;
         memcpy(&(g_pCurrentModel->camera_params[tmp]), s_CommandBuffer, sizeof(type_camera_parameters));
         saveControllerModel(g_pCurrentModel);
         send_model_changed_message_to_router(MODEL_CHANGED_CAMERA_PARAMS, 0);
         break;

      case COMMAND_ID_SET_CAMERA_PROFILE:
         tmp = s_CommandParam;
         if ( tmp < 0 || tmp >= MODEL_CAMERA_PROFILES )
            tmp = 0;
         g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile = tmp;
         saveControllerModel(g_pCurrentModel);  
         send_model_changed_message_to_router(MODEL_CHANGED_CAMERA_PARAMS, 0);
         sprintf(szBuff, "Switched camera %d to profile %s", g_pCurrentModel->iCurrentCamera+1, model_getCameraProfileName(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile));
         warnings_add(pPH->vehicle_id_src, szBuff);
         break;

      case COMMAND_ID_SET_CURRENT_CAMERA:
         tmp = s_CommandParam;
         if ( tmp < 0 || tmp >= g_pCurrentModel->iCameraCount )
            tmp = 0;
         g_pCurrentModel->iCurrentCamera = tmp;
         saveControllerModel(g_pCurrentModel);
         send_model_changed_message_to_router(MODEL_CHANGED_CAMERA_PARAMS, 0);
         break;

      case COMMAND_ID_FORCE_CAMERA_TYPE:
         tmp = s_CommandParam;
         g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType = tmp;
         saveControllerModel(g_pCurrentModel);
         send_model_changed_message_to_router(MODEL_CHANGED_CAMERA_PARAMS, 0);
         break;

      case COMMAND_ID_RESET_ALL_TO_DEFAULTS:
         {
            log_line("Received confirmation from vehicle to reset to defaults. Reseting local model to defaults...");
            g_pCurrentModel->resetAllSettingsKeepPairing(false);
            saveControllerModel(g_pCurrentModel);
            g_pCurrentModel->b_mustSyncFromVehicle = true;
            g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bPairedConfirmed = false;
            g_bSyncModelSettingsOnLinkRecover = true;
            send_model_changed_message_to_router(MODEL_CHANGED_RESET_TO_DEFAULTS, 0);
            log_line("Done reseting local model to defaults.");
            break;
         }

      case COMMAND_ID_SET_ALL_PARAMS:
         {
            u32 vid = g_pCurrentModel->uVehicleId;
            u32 ctrlId = g_pCurrentModel->uControllerId;
            u32 uBoardType = g_pCurrentModel->hwCapabilities.uBoardType;
            int cameraType = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraType;
            int forcedCameraType = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType;

            type_vehicle_stats_info stats;
            memcpy((u8*)&stats, (u8*)&(g_pCurrentModel->m_Stats), sizeof(type_vehicle_stats_info));

            type_radio_links_parameters radio_links;
            type_radio_interfaces_parameters radio_interfaces;

            memcpy(&radio_links, &g_pCurrentModel->radioLinksParams, sizeof(type_radio_links_parameters) );
            memcpy(&radio_interfaces, &g_pCurrentModel->radioInterfacesParams, sizeof(type_radio_interfaces_parameters) );
 
            char szFile[MAX_FILE_PATH_SIZE];
            strcpy(szFile, FOLDER_RUBY_TEMP);
            strcat(szFile, "tempVehicleSettings.txt");
            g_pCurrentModel->loadFromFile(szFile); 

            memcpy(&g_pCurrentModel->radioLinksParams, &radio_links, sizeof(type_radio_links_parameters) );
            memcpy(&g_pCurrentModel->radioInterfacesParams, &radio_interfaces, sizeof(type_radio_interfaces_parameters) );

            g_pCurrentModel->uVehicleId = vid;
            g_pCurrentModel->uControllerId = ctrlId;
            g_pCurrentModel->hwCapabilities.uBoardType = uBoardType;
            g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraType = cameraType;
            g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType = forcedCameraType;

            memcpy((u8*)&(g_pCurrentModel->m_Stats), (u8*)&stats, sizeof(type_vehicle_stats_info));

            g_pCurrentModel->is_spectator = false;
            saveControllerModel(g_pCurrentModel);
            g_pCurrentModel->b_mustSyncFromVehicle = true;
            send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
            menu_discard_all();
            break;
         }

      case COMMAND_ID_SET_VEHICLE_NAME:
         strcpy(g_pCurrentModel->vehicle_name, (const char*)s_CommandBuffer );
         g_pCurrentModel->constructLongName();
         saveControllerModel(g_pCurrentModel);
         send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
         break;

      case COMMAND_ID_SET_RADIO_CARD_MODEL:
      {
         int iRadioInterface = (s_CommandParam & 0xFF);
         u32 uReqCardModel = ((s_CommandParam >> 8) & 0xFF);
         u32 uRecvCardModel = (pPHCR->command_response_param & 0xFF);
         log_line("Received set radio card model response. Requested card model: %u, received card model: %d", uReqCardModel, uRecvCardModel);
         if ( 0xFF == uReqCardModel )
         {
            // Requested autodetection from vehicle
            char szMsg[128];
            sprintf(szMsg, "The vehicle radio interface was autodetected as: %s", str_get_radio_card_model_string(uRecvCardModel));
            if ( NULL != menu_get_top_menu() )
               menu_get_top_menu()->addMessage2(34, szMsg, "The selected value was updated.");
         }

         if ( (iRadioInterface >= 0) && (iRadioInterface < g_pCurrentModel->radioInterfacesParams.interfaces_count) )
         {
            if ( 0xFF == uReqCardModel )
               g_pCurrentModel->radioInterfacesParams.interface_card_model[tmp1] = (int)uRecvCardModel;
            else
               g_pCurrentModel->radioInterfacesParams.interface_card_model[tmp1] = -(int)uRecvCardModel;
            saveControllerModel(g_pCurrentModel);
            send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
            menu_refresh_all_menus();
         }
         break;
      }
      case COMMAND_ID_SET_RADIO_LINK_FREQUENCY:
       {
         warnings_remove_configuring_radio_link(true);
         link_reset_reconfiguring_radiolink();
              
         u32 tmpLink = (s_CommandParam>>24) & 0xFF;
         u32 tmpFreq = (s_CommandParam & 0xFFFFFF);
         log_line("[Commands] Received response Ok from vehicle to the link frequency change (new format). Vehicle radio link %u new freq: %s", tmpLink+1, str_format_frequency(tmpFreq));
         if ( (int)tmpLink < g_pCurrentModel->radioLinksParams.links_count )
         {
            u32 uMainConnectFrequency = get_model_main_connect_frequency(g_pCurrentModel->uVehicleId);
            if ( g_pCurrentModel->radioLinksParams.link_frequency_khz[tmpLink] == uMainConnectFrequency )
               set_model_main_connect_frequency(g_pCurrentModel->uVehicleId, tmpFreq);
            g_pCurrentModel->radioLinksParams.link_frequency_khz[tmpLink] = tmpFreq;
            for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
            {
               if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == (int)tmpLink )
                  g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[i] = tmpFreq;
            }
            saveControllerModel(g_pCurrentModel);
            u32 data[2];
            data[0] = tmpLink;
            data[1] = tmpFreq;
            send_control_message_to_router_and_data(PACKET_TYPE_LOCAL_CONTROL_LINK_FREQUENCY_CHANGED, (u8*)(&data[0]), 2*sizeof(u32));
         }
        
         break;
      }

      case COMMAND_ID_SET_RADIO_LINK_CAPABILITIES:
         {
            int linkIndex = s_CommandParam & 0xFF;
            u32 flags = s_CommandParam >> 8;
            char szBuffC[128];
            str_get_radio_capabilities_description(flags, szBuffC);
            log_line("[Commands] Set radio link nb.%d capabilities flags to %s", linkIndex+1, szBuffC);
            bool bOnlyVideoDataFlagsChanged = false;
            if ( (flags & (~(RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO))) ==
                 (g_pCurrentModel->radioLinksParams.link_capabilities_flags[linkIndex] & (~(RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO))) )
               bOnlyVideoDataFlagsChanged = true;
            g_pCurrentModel->radioLinksParams.link_capabilities_flags[linkIndex] = flags;
            saveControllerModel(g_pCurrentModel);
            if ( bOnlyVideoDataFlagsChanged )
               send_model_changed_message_to_router(MODEL_CHANGED_RADIO_LINK_CAPABILITIES, 0);
            else
            {
               log_line("[Commands] Radio link capabilities changed. Reinitializing radio links and router...");
               pairing_stop();
               render_all(g_TimeNow);
               hardware_sleep_ms(100);
               render_all(g_TimeNow);
               pairing_start_normal();
               log_line("[Commands] Finished repairing due to radio links capabilities changed.");
               menu_refresh_all_menus();
            }
            break;
         }

      case COMMAND_ID_SET_RXTX_SYNC_TYPE:
         g_pCurrentModel->rxtx_sync_type = s_CommandParam;
         saveControllerModel(g_pCurrentModel);  
         send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
         break;

      case COMMAND_ID_RESET_RADIO_LINK:
         {
            u32 linkIndex = s_CommandParam;
            log_line("[Commands] Received command response for command to reset radio link %u.", linkIndex+1);
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
            saveControllerModel(g_pCurrentModel);
            send_model_changed_message_to_router(MODEL_CHANGED_RESET_RADIO_LINK, linkIndex);
            return true;
         }
         
      case COMMAND_ID_SET_RADIO_LINK_FLAGS:
         {
            u32* p = (u32*)&(s_CommandBuffer[0]);
            u32 linkIndex = *p;
            p++;
            //u32 linkRadioFlags = *p;
            p++;
            int* pi = (int*)p;
            //int datarateVideo = *pi;
            pi++;
            //int datarateData = *pi;

            if ( (int)linkIndex >= g_pCurrentModel->radioLinksParams.links_count )
            {
               log_softerror_and_alarm("[Commands] Received command response from vehicle to set radio link flags, but radio link id is invalid (%d). Only %d radio links present.", linkIndex, g_pCurrentModel->radioLinksParams.links_count);
               break;
            }
            if ( g_pCurrentModel->radioLinkIsELRSRadio(linkIndex) )
            {
               warnings_remove_configuring_radio_link(true);
               link_reset_reconfiguring_radiolink();
               send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
               return false;
            }
            else
               warnings_add_configuring_radio_link_line("Waiting for confirmation from vehicle");
            return true;
         }

      case COMMAND_ID_SET_RADIO_LINK_FLAGS_CONFIRMATION:
         {
            warnings_remove_configuring_radio_link(true);
            link_reset_reconfiguring_radiolink();
            send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
            break;
         }
      case COMMAND_ID_SET_RADIO_LINK_DATARATES:
         {
            if ( (s_CommandParam < 0) || (s_CommandParam >= (u32)g_pCurrentModel->radioLinksParams.links_count) )
               break;
            memcpy(&g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[s_CommandParam], &s_CommandBuffer[0], sizeof(int));
            memcpy(&g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[s_CommandParam], &s_CommandBuffer[sizeof(int)], sizeof(int));
            memcpy(&g_pCurrentModel->radioLinksParams.uplink_datarate_video_bps[s_CommandParam], &s_CommandBuffer[2*sizeof(int)], sizeof(int));
            memcpy(&g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[s_CommandParam], &s_CommandBuffer[3*sizeof(int)], sizeof(int));

            saveControllerModel(g_pCurrentModel);
            send_model_changed_message_to_router(MODEL_CHANGED_RADIO_DATARATES, s_CommandParam);
            break;
         }

      case COMMAND_ID_SET_VIDEO_PARAMETERS:
         {
            video_parameters_t* pNewVideoSettings = (video_parameters_t*)&s_CommandBuffer[0];
            type_video_link_profile* pNewVideoProfiles = (type_video_link_profile*)&(s_CommandBuffer[sizeof(video_parameters_t)]);
            g_pCurrentModel->logVideoSettingsDifferences(pNewVideoSettings, &(pNewVideoProfiles[pNewVideoSettings->iCurrentVideoProfile]));

            memcpy(&(g_pCurrentModel->video_params), pNewVideoSettings, sizeof(video_parameters_t));
            memcpy(&(g_pCurrentModel->video_link_profiles[0]), pNewVideoProfiles, MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile));

            camera_profile_parameters_t* pCameraParams = &g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile];
            if ( g_pCurrentModel->isRunningOnOpenIPCHardware() &&
                 g_pCurrentModel->validate_fps_and_exposure_settings(pCameraParams, true))
            {
               log_line("Camera exposure (%d ms) was updated to accomodate the new video FPS value.", pCameraParams->iShutterSpeed);
            }

            saveControllerModel(g_pCurrentModel);

            if ( menu_has_menu(MENU_ID_VEHICLE_VIDEO_PROFILE) )
               menu_stack_pop(1);
            menu_refresh_all_menus();

            send_model_changed_message_to_router(MODEL_CHANGED_VIDEO_PARAMETERS, 0);
            break;
         }


      case COMMAND_ID_SET_AUDIO_PARAMS:
         {
            audio_parameters_t params;
            memcpy(&params, s_CommandBuffer, sizeof(audio_parameters_t));
            memcpy(&(g_pCurrentModel->audio_params), &params, sizeof(audio_parameters_t));
            saveControllerModel(g_pCurrentModel);
            send_model_changed_message_to_router(MODEL_CHANGED_AUDIO_PARAMS, 0);
            menu_refresh_all_menus();
            break;
         }

      case COMMAND_ID_SET_GPS_INFO:
         g_pCurrentModel->iGPSCount = s_CommandParam;
         saveControllerModel(g_pCurrentModel);         
         break;

      case COMMAND_ID_SET_OSD_PARAMS:
         {
            osd_parameters_t* pParams = (osd_parameters_t*)s_CommandBuffer;
            if ( (pParams->osd_layout_preset[g_pCurrentModel->osd_params.iCurrentOSDScreen] != OSD_PRESET_CUSTOM) && (pParams->osd_layout_preset[g_pCurrentModel->osd_params.iCurrentOSDScreen] != g_pCurrentModel->osd_params.osd_layout_preset[g_pCurrentModel->osd_params.iCurrentOSDScreen]) )
               g_pCurrentModel->resetOSDScreenToLayout( g_pCurrentModel->osd_params.iCurrentOSDScreen, pParams->osd_layout_preset[g_pCurrentModel->osd_params.iCurrentOSDScreen]);
            else
               merge_osd_params(pParams, &(g_pCurrentModel->osd_params));
            g_pCurrentModel->osd_params.uFlags &= ~(OSD_BIT_FLAGS_MUST_CHOOSE_PRESET);
            saveControllerModel(g_pCurrentModel);
            send_model_changed_message_to_router(MODEL_CHANGED_OSD_PARAMS, 0);

            if ( g_bChangedOSDStatsFontSize )
            {
               g_bChangedOSDStatsFontSize = false;
               
               u32 scale = g_pCurrentModel->osd_params.osd_preferences[g_pCurrentModel->osd_params.iCurrentOSDScreen] & 0xFF;
               osd_setScaleOSD((int)scale);
               scale = (g_pCurrentModel->osd_params.osd_preferences[g_pCurrentModel->osd_params.iCurrentOSDScreen]>>16) & 0x0F;
               osd_setScaleOSDStats((int)scale);
               osd_apply_preferences();
               applyFontScaleChanges();
            }
            menu_refresh_all_menus();
            break;
         }

      case COMMAND_ID_SET_ALARMS_PARAMS:
         {
            type_alarms_parameters* pParams = (type_alarms_parameters*)s_CommandBuffer;
            memcpy(&(g_pCurrentModel->alarms_params), pParams, sizeof(type_alarms_parameters));

            log_line("[Commands] Received alarms set confirmation: Current motor alarm value: %d/%d, command buffer byte 0: %d",
               g_pCurrentModel->alarms_params.uAlarmMotorCurrentThreshold & (1<<7),
               g_pCurrentModel->alarms_params.uAlarmMotorCurrentThreshold & 0x7F,
               s_CommandBuffer[0] );
            
            saveControllerModel(g_pCurrentModel);
            send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
            break;
         }

      case COMMAND_ID_SET_SERIAL_PORTS_INFO:
         {
            type_vehicle_hardware_interfaces_info* pNewInfo = (type_vehicle_hardware_interfaces_info*)s_CommandBuffer;
            g_pCurrentModel->hardwareInterfacesInfo.serial_port_count = pNewInfo->serial_port_count;
            for( int i=0; i<pNewInfo->serial_port_count; i++ )
            {
               strcpy(g_pCurrentModel->hardwareInterfacesInfo.serial_port_names[i], pNewInfo->serial_port_names[i]);
               g_pCurrentModel->hardwareInterfacesInfo.serial_port_speed[i] = pNewInfo->serial_port_speed[i];
               g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] = pNewInfo->serial_port_supported_and_usage[i];
            }
            saveControllerModel(g_pCurrentModel);
            send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
         }
         break;

      case COMMAND_ID_SET_TELEMETRY_PARAMETERS:
         memcpy(&g_pCurrentModel->telemetry_params, s_CommandBuffer, sizeof(telemetry_parameters_t));
         saveControllerModel(g_pCurrentModel);
         reset_vehicle_telemetry_runtime_info(get_vehicle_runtime_info_for_vehicle_id(g_pCurrentModel->uVehicleId));
         send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
         menu_refresh_all_menus();
         break;

      case COMMAND_ID_SET_TX_POWERS:
      {
         u8* pData = &s_CommandBuffer[0];
         int iCount = *pData;
         pData++;

         if ( iCount > MAX_RADIO_INTERFACES )
            iCount = MAX_RADIO_INTERFACES;
         for( int i=0; i<iCount; i++ )
         {
            g_pCurrentModel->radioInterfacesParams.interface_raw_power[i] = *pData;
            pData++;
         }

         saveControllerModel(g_pCurrentModel);
         send_model_changed_message_to_router(MODEL_CHANGED_RADIO_POWERS, 0);
         menu_refresh_all_menus();
         break;
      }
      case COMMAND_ID_SET_RC_PARAMS:
         {
            bool bPrevRC = g_pCurrentModel->rc_params.rc_enabled;
            memcpy(&g_pCurrentModel->rc_params, s_CommandBuffer, sizeof(rc_parameters_t));
            saveControllerModel(g_pCurrentModel);
            if ( bPrevRC != g_pCurrentModel->rc_params.rc_enabled )
            {
               if ( g_pCurrentModel->rc_params.rc_enabled )
               {
                  Popup* p = new Popup(true, "RC link is enabled", 3 );
                  p->setIconId(g_idIconJoystick, get_Color_IconWarning());
                  popups_add_topmost(p);
               }
               else
               {
                  Popup* p = new Popup(true, "RC link is disabled", 3 );
                  p->setIconId(g_idIconJoystick, get_Color_IconWarning());
                  popups_add_topmost(p);
               }
            }
            send_model_changed_message_to_router(MODEL_CHANGED_RC_PARAMS, 0);
         break;
         }

      case COMMAND_ID_DEBUG_GET_TOP:
         pBuffer = s_CommandReplyBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);

         log_line("[Commands] Vehicle TOP output (%d chars):\n%s", iDataLength, pBuffer);
         break;

      case COMMAND_ID_DOWNLOAD_FILE:
         _handle_download_file_response();
         break;

      case COMMAND_ID_DOWNLOAD_FILE_SEGMENT:
         _handle_download_file_segment_response();
         break;
   }

   menu_invalidate_all();
   return false;
}

bool _commands_check_send_get_settings()
{
   if ( ! g_bFirstModelPairingDone )
      return true;

   if ( g_bDebugState )
   {
      if ( NULL != g_pCurrentModel )
         g_pCurrentModel->b_mustSyncFromVehicle = false;
      return false;
   }

   //log_line("%d, %d, %d, %d, %u, %u, %d", s_bHasCommandInProgress, g_pCurrentModel->b_mustSyncFromVehicle, g_pCurrentModel->is_spectator, g_bSearching,
   //           g_TimeNow, g_RouterIsReadyTimestamp, s_iCountRetriesToGetModelSettingsCommand);

   if ( ! g_bIsReinit )
   if ( ! g_bIsVideoPlaying )
   if ( ! g_bSearching )
   if ( ! s_bHasCommandInProgress )
   if ( ! g_bUpdateInProgress )
   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->b_mustSyncFromVehicle || g_bIsFirstConnectionToCurrentVehicle ) && (!g_pCurrentModel->is_spectator))
   if ( g_pCurrentModel->getVehicleFirmwareType() == MODEL_FIRMWARE_TYPE_RUBY )
   if ( link_is_vehicle_online_now(g_pCurrentModel->uVehicleId) )
   if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bPairedConfirmed )
   if ( g_TimeNow > g_RouterIsReadyTimestamp + 500  )
   if ( s_iCountRetriesToGetModelSettingsCommand < 5 )
   {
      log_line("[Commands] Must sync settings from vehicle...");
      s_iCountRetriesToGetModelSettingsCommand++;
      log_line("[Commands] Current vehicle sw version: %d.%d (b-%d)", get_sw_version_major(g_pCurrentModel), get_sw_version_minor(g_pCurrentModel), get_sw_version_build(g_pCurrentModel));

      u32 uFlags = 0;
      if ( get_sw_version_build(g_pCurrentModel) < 289 )
      {
         Preferences* pP = get_Preferences();

         u8 uRefreshGraphIntervalIndex = 3;

         uFlags = (((u32)0x01)<<6); // Request response in small segments
         uFlags |= ((DEFAULT_MAVLINK_SYS_ID_CONTROLLER & 0xFF) << 8);
         uFlags |= ((pP->iDebugWiFiChangeDelay & 0xFF) << 16);
         uFlags |= ((uRefreshGraphIntervalIndex & 0x0F) + 1) << 24;
      }

      warnings_add(g_pCurrentModel->uVehicleId, "Synchronizing vehicle settings...");

      log_line("[Commands] Send request to router to request model settings from vehicle.");
      reset_model_settings_download_buffers(g_pCurrentModel->uVehicleId);
      return handle_commands_send_to_vehicle(COMMAND_ID_GET_ALL_PARAMS_ZIP, uFlags, NULL, 0);
   }

   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->b_mustSyncFromVehicle || g_bIsFirstConnectionToCurrentVehicle ) && (!g_pCurrentModel->is_spectator))
   if ( g_pCurrentModel->getVehicleFirmwareType() == MODEL_FIRMWARE_TYPE_RUBY )
   if ( ! g_bIsVideoPlaying )
   {
      static u32 s_uLastTimeErrorVehicleSync = 0;
      if ( g_TimeNow >= s_uLastTimeErrorVehicleSync + 2000 )
      {
         s_uLastTimeErrorVehicleSync = g_TimeNow;
         log_line("[Commands] Must sync vehicle settings for VID %u, but checks fail: command in progress: %s, searching: %s, reinit: %s, is receiving main telemetry: %s; is vehicle online: %s; retry sync counter: %d, is router ready: %s, paired: %s, ctrl update in progress: %s",
            g_pCurrentModel->uVehicleId,
            s_bHasCommandInProgress?"yes":"no",
            g_bSearching?"yes":"no",
            g_bIsReinit?"yes":"no",
            link_has_received_main_vehicle_ruby_telemetry()?"yes":"no",
            link_is_vehicle_online_now(g_pCurrentModel->uVehicleId)?"yes":"no",
            s_iCountRetriesToGetModelSettingsCommand,
            (g_TimeNow > g_RouterIsReadyTimestamp + 500)?"yes":"no",
            g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bPairedConfirmed?"yes":"no",
            g_bUpdateInProgress?"yes":"no"
            );
      }
   }
   
   if ( get_CorePluginsCount() > 0 )
   if ( s_bHasToSyncCorePluginsInfoFromVehicle )
   if ( ! g_bIsReinit )
   if ( ! s_bHasCommandInProgress )
   if ( NULL != g_pCurrentModel && (!g_pCurrentModel->is_spectator))
   if ( ! g_pCurrentModel->b_mustSyncFromVehicle )
   if ( ! g_bSearching )
   if ( link_is_vehicle_online_now(g_pCurrentModel->uVehicleId) )
   if ( g_bIsRouterReady )
   if ( g_TimeNow > g_RouterIsReadyTimestamp + 500  )
   if ( s_RetryGetCorePluginsCounter < 10 )
   {
      s_RetryGetCorePluginsCounter++;
      warnings_add(g_pCurrentModel->uVehicleId, "Synchronizing vehicle plugins...");
      handle_commands_send_to_vehicle(COMMAND_ID_GET_CORE_PLUGINS_INFO, 0, NULL, 0);
   }
   
   return false;
}

bool _commands_check_download_file_segments()
{
   if ( s_bHasCommandInProgress )
      return false;
   
   if ( s_uFileIdToDownload == 0 )
      return false;

   if ( (s_uFileToDownloadState == 1) && (s_uCountFileSegmentsToDownload == 0) )
      return false;

   if ( s_uFileToDownloadState != 1 )
   if ( s_CommandStartTime+4000 < g_TimeNow )
   {
      handle_commands_send_to_vehicle(COMMAND_ID_DOWNLOAD_FILE, s_uFileIdToDownload | 0xFF000000, NULL, 0);
      return true;
   }

   if ( g_TimeNow < s_uLastFileSegmentRequestTime + 100 )
      return false;

   for( u32 u=0; u<s_uCountFileSegmentsToDownload; u++ )
   {
      if ( s_bListFileSegmentsToDownload[u] )
      {
         s_uLastFileSegmentRequestTime = g_TimeNow;
         handle_commands_send_single_oneway_command(0, COMMAND_ID_DOWNLOAD_FILE_SEGMENT, (s_uFileIdToDownload & 0xFFFF) | (u<<16), NULL, 0);
         return true;
      }
   }
   return false;
}


bool _commands_check_upload_file_segments()
{
   if ( s_bHasCommandInProgress )
      return false;
   
   if ( ! g_bHasFileUploadInProgress )
      return false;

   if ( g_CurrentUploadingFile.uFileId == 0 || 0 == g_CurrentUploadingFile.szFileName[0] )
   {
      g_bHasFileUploadInProgress = false;
      return false;
   }

   if ( 0 == g_CurrentUploadingFile.uTotalSegments )
   {
      g_bHasFileUploadInProgress = false;
      return false;    
   }

   if ( g_TimeNow < g_CurrentUploadingFile.uTimeLastUploadSegment + 100 )
      return false;

   if ( g_TimeNow < s_CommandLastResponseReceivedTime + 50 )
      return false;

   bool bUploadedAnySegment = false;
   for( u32 u=0; u<g_CurrentUploadingFile.uTotalSegments; u++ )
   {
      if ( g_CurrentUploadingFile.bSegmentsUploaded[u] )
         continue;

      bUploadedAnySegment = true;

      g_CurrentUploadingFile.currentUploadSegment.uSegmentSize = g_CurrentUploadingFile.uSegmentsSize[u];
      g_CurrentUploadingFile.currentUploadSegment.uSegmentIndex = u;

      u8 buffer[MAX_PACKET_TOTAL_SIZE];
      memcpy(buffer, (u8*)&g_CurrentUploadingFile.currentUploadSegment, sizeof(command_packet_upload_file_segment));
      memcpy(buffer + sizeof(command_packet_upload_file_segment), g_CurrentUploadingFile.pSegments[u], g_CurrentUploadingFile.uSegmentsSize[u]);
      handle_commands_send_to_vehicle(COMMAND_ID_UPLOAD_FILE_SEGMENT, 0, buffer, sizeof(command_packet_upload_file_segment) + g_CurrentUploadingFile.uSegmentsSize[u]);
      g_CurrentUploadingFile.uTimeLastUploadSegment = g_TimeNow;
      g_CurrentUploadingFile.uLastSegmentIndexUploaded = u;
      break;
   }

   if ( ! bUploadedAnySegment )
   {
     // No more segments to upload
     warnings_add(0, "Finished uploading core plugins to vehicle.");
     g_bHasFileUploadInProgress = false;
   }
   return false;
}

void _handle_commands_on_command_timeout()
{
   log_line("[Commands] Command number %d, resend counter %d, type %d, has timedout. Abandon command.", s_CommandCounter, s_CommandResendCounter, commands_get_description(s_CommandType));

   if ( s_CommandType == COMMAND_ID_SET_RADIO_LINK_FREQUENCY )
   {
      warnings_remove_configuring_radio_link(false);
      link_reset_reconfiguring_radiolink();
   }
   else if ( s_CommandType == COMMAND_ID_SET_RADIO_LINK_FLAGS ||
        s_CommandType == COMMAND_ID_SET_RADIO_LINK_FLAGS_CONFIRMATION )
   {
      if ( s_CommandType == COMMAND_ID_SET_RADIO_LINK_FLAGS )
         log_softerror_and_alarm("[Commands] The new radio flags where not received by vehicle (no command acknolwedge received)." );
      else
         log_softerror_and_alarm("[Commands] The new radio flags where not acknowledged by vehicle (no confirmation received)." );

      log_line("[Commands] Reverting radio links changes to last good ones.");
      memcpy(&(g_pCurrentModel->radioLinksParams), &g_LastGoodRadioLinksParams, sizeof(type_radio_links_parameters));            
      g_pCurrentModel->validateRadioSettings();
      saveControllerModel(g_pCurrentModel);
      send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
         
      warnings_remove_configuring_radio_link(false);
      link_reset_reconfiguring_radiolink();
            
      char szTextW[256];
      sprintf(szTextW, "Your %s radio link does not support this combination of radio params.", g_pCurrentModel->getVehicleTypeString());
      MenuConfirmation* pMC = new MenuConfirmation("Unsupported Parameter", szTextW, -1, true);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      menu_invalidate_all();
   }
   else if ( s_CommandType == COMMAND_ID_GET_ALL_PARAMS_ZIP )
      warnings_add(g_pCurrentModel->uVehicleId, "Could not get vehicle settings.");
   else
   {
      Popup* p = new Popup("No response from vehicle. Command not applied.",0.25,0.4, 0.5, 4);
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      menu_invalidate_all();
   }

   s_CommandType = 0;
   s_bHasCommandInProgress = false;
}

void handle_commands_loop()
{
   if ( g_bSearching )
      return;

   if ( _commands_check_send_get_settings() )
      return;

   // Check for out of bound responses (not expected responses)

   if ( ! s_bHasCommandInProgress )
   {
      _commands_check_download_file_segments();
      _commands_check_upload_file_segments();
      return;
   }

   // A command is in progress

   // Did it timed out?

   if ( g_TimeNow > s_CommandStartTime + s_CommandTimeout )
   {
      if ( s_CommandResendCounter >= s_CommandMaxResendCounter )
      {
         log_softerror_and_alarm("[Commands] Last command did not complete (timed out waiting for a response)." );
         popup_log_add_entry("Command timed out (No response from vehicle).");
         _handle_commands_on_command_timeout();
         return;
      }

      if ( NULL != g_pCurrentModel )
         reset_model_settings_download_buffers(g_pCurrentModel->uVehicleId);

      if ( s_CommandTimeout < 300 )
         s_CommandTimeout += 10;
      s_CommandResendCounter++;
      for( int i=0; i<=(s_CommandResendCounter/10); i++ )
      {
         if ( i != 0 )
            hardware_sleep_micros(500);
         handle_commands_send_current_command();
      }
   }
}

void handle_commands_on_response_received(u8* pPacketBuffer, int iLength)
{
   s_CommandReplyLength = 0;

   t_packet_header* pPH = (t_packet_header*) pPacketBuffer;

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) != PACKET_COMPONENT_COMMANDS ) 
      return;
   if ( pPH->packet_type != PACKET_TYPE_COMMAND_RESPONSE )
      return;
   
   t_packet_header_command_response* pPHCR = (t_packet_header_command_response*)(pPacketBuffer + sizeof(t_packet_header));
   log_line_commands( "[Commands] [Recv] Response from VID %u, cmd resp nb. %d, origin cmd nb. %d, origin retry %d, type [%s], response flags: %s, extra info len. %d]", pPH->vehicle_id_src, pPHCR->response_counter, pPHCR->origin_command_counter, pPHCR->origin_command_resend_counter, commands_get_description(pPHCR->origin_command_type), str_get_command_response_flags_string(pPHCR->command_response_flags), pPH->total_length-sizeof(t_packet_header)-sizeof(t_packet_header_command_response));

   s_CommandLastResponseReceivedTime = g_TimeNow;

   if ( pPHCR->origin_command_type == COMMAND_ID_DOWNLOAD_FILE_SEGMENT )
   {
      s_bLastCommandSucceeded = true;
      memcpy( s_CommandReplyBuffer, pPacketBuffer, pPH->total_length );
      s_CommandReplyLength = pPH->total_length;
      _handle_download_file_segment_response();
      return;
   }

   // Received a response to an old command? Ignore it
   if ( pPHCR->origin_command_counter != s_CommandCounter )
   {
      log_line_commands( "[Commands] [Recv] Ignore out of bound response from vId %u, cmd resp nb. %d, origin cmd nb. %d, origin retry %d, type [%s], flags %d, extra info length: %d]", pPH->vehicle_id_src, pPHCR->response_counter, pPHCR->origin_command_counter, pPHCR->origin_command_resend_counter, commands_get_description(pPHCR->origin_command_type), pPHCR->command_response_flags, pPH->total_length-sizeof(t_packet_header)-sizeof(t_packet_header_command_response));
      return;
   }

   // Received duplicate response to the last sent command?
   if ( s_CommandLastProcessedResponseToCommandCounter == s_CommandCounter )
   if ( s_CommandType != COMMAND_ID_GET_ALL_PARAMS_ZIP )
   {
      log_line_commands( "[Commands] [Ignoring duplicate response] of vId %u, cmd nb. %d, retry %d, type [%s], param: %u]", g_pCurrentModel->uVehicleId, s_CommandCounter, s_CommandResendCounter, commands_get_description(s_CommandType), s_CommandParam);
      return;
   }

   // Process this response to the last command sent
   s_CommandLastProcessedResponseToCommandCounter = s_CommandCounter;
   
   memcpy( s_CommandReplyBuffer, pPacketBuffer, pPH->total_length );
   s_CommandReplyLength = pPH->total_length;

   if ( (pPHCR->command_response_flags & COMMAND_RESPONSE_FLAGS_UNKNOWN_COMMAND) ||
        (pPHCR->command_response_flags & COMMAND_RESPONSE_FLAGS_FAILED_INVALID_PARAMS) )
   {
      if ( NULL != menu_get_top_menu() )
         menu_get_top_menu()->onVehicleCommandFinished(s_CommandCounter, s_CommandType, false);

      s_bLastCommandSucceeded = false;
      if ( pPHCR->origin_command_type == COMMAND_ID_UPLOAD_FILE_SEGMENT )
      {
         g_CurrentUploadingFile.uTotalSegments = 0;
         g_bHasFileUploadInProgress = false;
         s_CommandType = 0;
         s_bHasCommandInProgress = false;
         return;
      }
      if ( ! g_bUpdateInProgress )
      {
         Popup* p = NULL;
         if ( pPHCR->command_response_flags & COMMAND_RESPONSE_FLAGS_FAILED_INVALID_PARAMS )
         {
            popup_log_add_entry("Invalid command parameters. Update the vehicle software.");
            log_softerror_and_alarm("[Commands] Last command failed on the vehicle: invalid command parameters. Update the vehicle software." );
            p = new Popup("Invalid command parameters. Update the vehicle software.",0.25,0.44, 0.5, 4);
         }
         else
         {
            popup_log_add_entry("Command not understood by the vehicle. Update the vehicle software.");
            log_softerror_and_alarm("[Commands] Last command failed on the vehicle: vehicle does not knows the command. Update the vehicle software." );
            p = new Popup("Command not understood by the vehicle. Update the vehicle software.",0.25,0.44, 0.5, 4);
         }
         p->setIconId(g_idIconError, get_Color_IconError());
         popups_add_topmost(p);
      }
      menu_invalidate_all();
      s_CommandType = 0;
      s_bHasCommandInProgress = false;
      return;
   }

   s_bLastCommandSucceeded = false;
   if ( pPHCR->command_response_flags & COMMAND_RESPONSE_FLAGS_OK )
      s_bLastCommandSucceeded = true;

   if ( NULL != menu_get_top_menu() )
      menu_get_top_menu()->onVehicleCommandFinished(s_CommandCounter, s_CommandType, s_bLastCommandSucceeded);
   
   if ( ! s_bLastCommandSucceeded )
   {
      if ( s_CommandType == COMMAND_ID_SET_RADIO_LINK_FREQUENCY )
      {
         warnings_remove_configuring_radio_link(false);
         link_reset_reconfiguring_radiolink();
      }

      if ( ! g_bUpdateInProgress )
      {
         popup_log_add_entry("Command failed on the vehicle side.");
         log_softerror_and_alarm("[Commands] Last command failed on the vehicle." );
         Popup* p = new Popup("Command failed on vehicle side.",0.25,0.4, 0.5, 4);
         p->setIconId(g_idIconError, get_Color_IconError());
         popups_add_topmost(p);
         menu_invalidate_all();
      }
      s_CommandType = 0;
      s_bHasCommandInProgress = false;
      return;
   }

   if ( ! handle_last_command_result() )
      s_bHasCommandInProgress = false;
}

u32  handle_commands_get_last_command_id_response_received()
{
   return s_CommandLastProcessedResponseToCommandCounter;
}

bool handle_commands_last_command_succeeded()
{
   return s_bLastCommandSucceeded;
}

bool handle_commands_send_to_vehicle(u8 commandType, u32 param, u8* pBuffer, int iLength)
{
   return handle_commands_send_to_vehicle(commandType, param, pBuffer, iLength, NULL, 0);
}

bool handle_commands_send_to_vehicle(u8 commandType, u32 param, u8* pBuffer, int iLength, u8* pBuffer2, int iLength2)
{
   if ( (NULL == g_pCurrentModel) || (g_pCurrentModel->is_spectator) )
      return false;

   if ( s_bHasCommandInProgress )
   {
      log_line_commands( "[Commands] [Send] to vId %u, cmd nb. %d, retry: %d, type [%s], param: %u] Tried to send a new command while another one (nb %d) was in progress.", g_pCurrentModel->uVehicleId, s_CommandCounter+1, s_CommandResendCounter, commands_get_description(s_CommandType), s_CommandParam, s_CommandCounter);
      handle_commands_show_popup_progress();
      return false;
   }
 
   if ( ! link_has_received_main_vehicle_ruby_telemetry() )
      return false;

   s_CommandTargetVehicleId = g_pCurrentModel->uVehicleId;
   s_CommandBufferLength = 0;
   if ( NULL != pBuffer )
   {
      s_CommandBufferLength += iLength;
      memcpy(s_CommandBuffer, pBuffer, iLength );
      log_line("[Commands] Stored command buffer of %d bytes, byte 0: %d.", iLength, s_CommandBuffer[0]);
   }
   if ( NULL != pBuffer2 )
   {
      s_CommandBufferLength += iLength2;
      memcpy(&s_CommandBuffer[iLength], pBuffer2, iLength2 );
      log_line("[Commands] Stored command second buffer of %d bytes, byte 0: %d.", iLength2, s_CommandBuffer[iLength]);
   }
   s_CommandCounter++;
   s_CommandType = commandType;
   s_CommandParam = param;
   s_CommandResendCounter = 0;

   s_CommandReplyLength = 0;
   //if ( length > 0 )
   //   log_line_commands( "[Commands] [Sending] to vId %u, cmd nb. %d, retry %d, type [%s], param: %u, extra length: %d", g_pCurrentModel->uVehicleId, s_CommandCounter, s_CommandResendCounter, commands_get_description(s_CommandType), s_CommandParam, length);
   //else
   //   log_line_commands( "[Commands] [Sending] to vId %u, cmd nb. %d, retry %d, type [%s], param: %u", g_pCurrentModel->uVehicleId, s_CommandCounter, s_CommandResendCounter, commands_get_description(s_CommandType), s_CommandParam);
   popup_log_add_entry("Sending command %s to vehicle...", commands_get_description(s_CommandType));

   s_CommandTimeout = 50;
   s_CommandMaxResendCounter = 40;
   if ( s_CommandType == COMMAND_ID_SET_RADIO_LINK_FREQUENCY )
      s_CommandTimeout = 250;
   if ( s_CommandType == COMMAND_ID_GET_MEMORY_INFO )
      s_CommandTimeout = 1000;
   if ( s_CommandType == COMMAND_ID_GET_CPU_INFO )
      s_CommandTimeout = 1000;
   if ( s_CommandType == COMMAND_ID_SET_CAMERA_PARAMETERS )
      s_CommandTimeout = 250;
   if ( s_CommandType == COMMAND_ID_SET_VIDEO_PARAMETERS )
      s_CommandTimeout = 250;
   if ( s_CommandType == COMMAND_ID_GET_SIK_CONFIG )
   {
      s_CommandTimeout = 5000;
      s_CommandMaxResendCounter = 3;
   }
   if ( s_CommandType == COMMAND_ID_GET_CPU_PROCS_INFO )
   {
      s_CommandTimeout = 7000;
      s_CommandMaxResendCounter = 2;
   }
   if ( s_CommandType == COMMAND_ID_GET_ALL_PARAMS_ZIP )
   {
      s_CommandTimeout = 1000;
      if ( NULL != g_pCurrentModel )
         reset_model_settings_download_buffers(g_pCurrentModel->uVehicleId);
   }
   if ( s_CommandType == COMMAND_ID_DOWNLOAD_FILE )
   {
      s_CommandTimeout = 500;
      s_CommandMaxResendCounter = 5;
   }
   if ( s_CommandType == COMMAND_ID_DOWNLOAD_FILE_SEGMENT )
   {
      s_CommandTimeout = 250;
      s_CommandMaxResendCounter = 10;
   }

   if ( s_CommandType == COMMAND_ID_SET_RADIO_LINK_FLAGS )
   {
      s_CommandTimeout = 250;
      s_CommandMaxResendCounter = 12;
      bool bModelHasAtheros = false;
      if ( NULL != g_pCurrentModel )
         for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         {
            if ( (g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[i] & 0xFF) == RADIO_TYPE_ATHEROS )
            {
               log_line("[Commands] Current model has Atheros radio cards. Using longer timeouts to send radio flags.");
               bModelHasAtheros = true;
            }
            if ( (g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[i] & 0xFF) == RADIO_TYPE_RALINK )
            {
               log_line("[Commands] Current model has RaLink radio cards. Using longer timeouts to send radio flags.");
               bModelHasAtheros = true;
            }
         }
      if ( bModelHasAtheros )
      {
         s_CommandTimeout = 300;
         s_CommandMaxResendCounter = 20;
      }
   }
   if ( s_CommandType == COMMAND_ID_SET_RADIO_LINK_FLAGS_CONFIRMATION )
   {
      s_CommandTimeout = 250;
      s_CommandMaxResendCounter = 12;
   }

   if ( s_CommandTimeout >= 100 )
      s_CommandMaxResendCounter = 30;
   if ( s_CommandTimeout >= 200 )
      s_CommandMaxResendCounter = 15;
   if ( s_CommandTimeout >= 500 )
      s_CommandMaxResendCounter = 10;
   if ( s_CommandTimeout >= 1000 )
      s_CommandMaxResendCounter = 5;

   handle_commands_send_current_command();

   s_bHasCommandInProgress = true;
   return true;
}

u32 handle_commands_get_current_command_counter()
{
   return s_CommandCounter;
}

u32 handle_commands_increment_command_counter()
{
   s_CommandCounter++;
   return s_CommandCounter;
}

u32 handle_commands_decrement_command_counter()
{
   s_CommandCounter--;
   return s_CommandCounter; 
}

bool handle_commands_send_command_once_to_vehicle(u8 commandType, u8 resendCounter, u32 param, u8* pBuffer, int length)
{
   if ( NULL == g_pCurrentModel )
      return false;

   if ( s_bHasCommandInProgress )
   {
      log_line_commands( "[Commands] [Send] to vId %u, cmd nb. %d, retry: %d, type [%s], param: %u] Tried to send a new command while another one (nb %d) was in progress.", g_pCurrentModel->uVehicleId, s_CommandCounter+1, resendCounter, commands_get_description(commandType), param, s_CommandCounter);
      handle_commands_show_popup_progress();
      return false;
   }


   s_CommandTargetVehicleId = g_pCurrentModel->uVehicleId;
   s_CommandBufferLength = length;
   if ( NULL != pBuffer )
      memcpy(s_CommandBuffer, pBuffer, s_CommandBufferLength );
   s_CommandType = commandType;
   s_CommandParam = param;
   s_CommandResendCounter = resendCounter;

   s_CommandReplyLength = 0;
   s_CommandTimeout = 5000;
   s_CommandMaxResendCounter = 0;
   s_CommandStartTime = g_TimeNow;

   t_packet_header PH;
   t_packet_header_command PHC;

   radio_packet_init(&PH, PACKET_COMPONENT_COMMANDS, PACKET_TYPE_COMMAND, STREAM_ID_DATA);
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
   PH.total_length = sizeof(t_packet_header)+sizeof(t_packet_header_command) + length;

   PHC.command_type = commandType;
   PHC.command_counter = s_CommandCounter;
   PHC.command_param = param;
   PHC.command_resend_counter = resendCounter;
  
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), (u8*)&PHC, sizeof(t_packet_header_command));
   if ( NULL != pBuffer )
      memcpy(buffer+sizeof(t_packet_header)+sizeof(t_packet_header_command), pBuffer, length);
   
   send_packet_to_router(buffer, PH.total_length);
 
   log_line_commands("[CommandsTh] [Sent] to vId %u, cmd nb. %d, retry %d, type [%s], param: %u]", g_pCurrentModel->uVehicleId, s_CommandCounter, resendCounter, commands_get_description(commandType), param);
   return true;
}

bool handle_commands_send_single_oneway_command(u8 resendCounter, u8 commandType, u32 param, u8* pBuffer, int length)
{
   return handle_commands_send_single_oneway_command(resendCounter, commandType, param, pBuffer, length, 2);
}

bool handle_commands_send_single_oneway_command(u8 resendCounter, u8 commandType, u32 param, u8* pBuffer, int length, int delayMs)
{
   if ( NULL == g_pCurrentModel )
      return false;
   return handle_commands_send_single_oneway_command_to_vehicle(g_pCurrentModel->uVehicleId, resendCounter, commandType, param, pBuffer, length, delayMs);
}

bool handle_commands_send_single_oneway_command_to_vehicle(u32 uVehicleId, u8 resendCounter, u8 commandType, u32 param, u8* pBuffer, int length, int delayMs)
{
   if ( s_bHasCommandInProgress )
   {
      log_line_commands( "[Commands] [Send] to VID %u, cmd nb. %d, retry: %d, type [%s], param: %u] Tried to send a new command while another one (nb %d) was in progress.",
          s_CommandTargetVehicleId, s_CommandCounter+1, resendCounter, commands_get_description(commandType), param, s_CommandCounter);
      handle_commands_show_popup_progress();
      return false;
   }

   s_CommandTargetVehicleId = uVehicleId;

   t_packet_header PH;
   t_packet_header_command PHC;
   u8 buffer[MAX_PACKET_TOTAL_SIZE];

   radio_packet_init(&PH, PACKET_COMPONENT_COMMANDS, PACKET_TYPE_COMMAND, STREAM_ID_DATA);
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = uVehicleId;
   PH.total_length = sizeof(t_packet_header)+sizeof(t_packet_header_command) + length;

   handle_commands_increment_command_counter();

   for( int i=0; i<=resendCounter; i++ )
   {
      PHC.command_type = commandType | COMMAND_TYPE_FLAG_NO_RESPONSE_NEEDED;
      PHC.command_counter = s_CommandCounter;
      PHC.command_param = param;
      PHC.command_resend_counter = i;
  
      memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
      memcpy(buffer+sizeof(t_packet_header), (u8*)&PHC, sizeof(t_packet_header_command));
      memcpy(buffer+sizeof(t_packet_header)+sizeof(t_packet_header_command), pBuffer, length);
      send_packet_to_router(buffer, PH.total_length);
      if ( 0 < delayMs )
         hardware_sleep_ms(delayMs);
   }
   //log_line_commands("[Commands] [Sent] oneway to vId %u, cmd nb. %d, repeat count: %d, type [%s], param: %u]", g_pCurrentModel->uVehicleId, s_CommandCounter, resendCounter, commands_get_description(commandType), param);
   return true;
}

bool handle_commands_send_ruby_message(t_packet_header* pPH, u8* pBuffer, int length)
{
   if ( NULL == pPH )
      return false;

   if ( ! pairing_isStarted() )
      return false;

   pPH->vehicle_id_src = g_uControllerId;
   pPH->vehicle_id_dest = g_pCurrentModel->uVehicleId;
   pPH->total_length = sizeof(t_packet_header)+length;
   
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)pPH, sizeof(t_packet_header));
   if ( NULL != pBuffer )
      memcpy(buffer+sizeof(t_packet_header), pBuffer, length);
   send_packet_to_router(buffer, pPH->total_length);
   return true;
}


void handle_commands_abandon_command()
{
   s_CommandType = 0;
   s_bHasCommandInProgress = false;
}

bool handle_commands_is_command_in_progress()
{
   return (s_bHasCommandInProgress || link_is_reconfiguring_radiolink());
}

void handle_commands_show_popup_progress()
{
   Popup* p = new Popup("A command is in progress. Please wait for it to finish.",0.25,0.4, 0.5, 3);
   p->setIconId(g_idIconWarning, get_Color_IconWarning());
   popups_add_topmost(p);
}

void handle_commands_reset_has_received_vehicle_core_plugins_info()
{
   s_bHasReceivedVehicleCorePluginsInfo = false;
}

bool handle_commands_has_received_vehicle_core_plugins_info()
{
   return s_bHasReceivedVehicleCorePluginsInfo;
}

void handle_commands_initiate_file_upload(u32 uFileId, const char* szFileName)
{
   for( u32 i=0; i<g_CurrentUploadingFile.uTotalSegments; i++ )
   {
      if ( NULL != g_CurrentUploadingFile.pSegments[i] )
         free(g_CurrentUploadingFile.pSegments[i]);
      g_CurrentUploadingFile.pSegments[i] = NULL;
   }

   g_CurrentUploadingFile.uTotalSegments = 0;
   g_CurrentUploadingFile.uFileId = uFileId;
   g_CurrentUploadingFile.szFileName[0] = 0;
 
   g_bHasFileUploadInProgress = false;
 
   if ( NULL == szFileName || 0 == szFileName[0] )
      return;

   strncpy(g_CurrentUploadingFile.szFileName, szFileName, 127);
 
   FILE* fd = fopen(szFileName, "rb");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("[Commands] Failed to open file for uploading to vehicle: [%s].", szFileName);
      g_CurrentUploadingFile.szFileName[0] = 0;
      return;
   }

   fseek(fd, 0, SEEK_END);
   long lSize = ftell(fd);
   fclose(fd);
   long lSegmentSize = 800;

   g_CurrentUploadingFile.uTotalSegments = lSize/lSegmentSize;
   for( u32 u=0; u<g_CurrentUploadingFile.uTotalSegments; u++ )
   {
      g_CurrentUploadingFile.uSegmentsSize[u] = lSegmentSize;
      g_CurrentUploadingFile.bSegmentsUploaded[u] = false;
      g_CurrentUploadingFile.pSegments[u] = (u8*) malloc(lSegmentSize);
      if ( NULL == g_CurrentUploadingFile.pSegments[u] )
      {
         log_softerror_and_alarm("[Commands] Failed to allocate memory for uploading file segment to vehicle.");
         g_CurrentUploadingFile.szFileName[0] = 0;
         g_CurrentUploadingFile.uTotalSegments = 0;
         return;
      }
   }

   if ( lSize > (long)g_CurrentUploadingFile.uTotalSegments*lSegmentSize )
   {
      g_CurrentUploadingFile.uSegmentsSize[g_CurrentUploadingFile.uTotalSegments] = lSize - g_CurrentUploadingFile.uTotalSegments*lSegmentSize ;
      g_CurrentUploadingFile.bSegmentsUploaded[g_CurrentUploadingFile.uTotalSegments] = false;
      g_CurrentUploadingFile.pSegments[g_CurrentUploadingFile.uTotalSegments] = (u8*) malloc(g_CurrentUploadingFile.uSegmentsSize[g_CurrentUploadingFile.uTotalSegments]);
      if ( NULL == g_CurrentUploadingFile.pSegments[g_CurrentUploadingFile.uTotalSegments] )
      {
         log_softerror_and_alarm("[Commands] Failed to allocate memory for uploading file segment to vehicle.");
         g_CurrentUploadingFile.szFileName[0] = 0;
         g_CurrentUploadingFile.uTotalSegments = 0;
         return;
      }
      g_CurrentUploadingFile.uTotalSegments++;
   }

   log_line("[Commands] Allocated %d bytes for %u segments to upload file [%s] to vehicle.", lSize, g_CurrentUploadingFile.uTotalSegments, g_CurrentUploadingFile.szFileName);
   
   fd = fopen(szFileName, "rb");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("[Commands] Failed to open file for uploading to vehicle: [%s].", szFileName);
      g_CurrentUploadingFile.szFileName[0] = 0;
      return;
   }

   for( u32 u=0; u<g_CurrentUploadingFile.uTotalSegments; u++ )
   {
      int nRes = fread(g_CurrentUploadingFile.pSegments[u], 1, g_CurrentUploadingFile.uSegmentsSize[u], fd);
      if ( nRes != (int)g_CurrentUploadingFile.uSegmentsSize[u] )
      {
         log_softerror_and_alarm("[Commands] Failed to read file segment %u for uploading file to vehicle.", u);
         g_CurrentUploadingFile.szFileName[0] = 0;
         g_CurrentUploadingFile.uTotalSegments = 0;
         fclose(fd);
         return;
      }
   }

   fclose(fd);

   log_line("[Commands] Initiated file upload to vehicle: [%s], %d bytes, %u segments.", g_CurrentUploadingFile.szFileName, lSize, g_CurrentUploadingFile.uTotalSegments);
   g_CurrentUploadingFile.uTimeLastUploadSegment = 0;
   g_CurrentUploadingFile.currentUploadSegment.uFileId = g_CurrentUploadingFile.uFileId;
   g_CurrentUploadingFile.currentUploadSegment.uTotalFileSize = lSize;
   g_CurrentUploadingFile.currentUploadSegment.uTotalSegments = g_CurrentUploadingFile.uTotalSegments;
   g_CurrentUploadingFile.currentUploadSegment.uSegmentSize = lSegmentSize;
   strncpy(g_CurrentUploadingFile.currentUploadSegment.szFileName, szFileName, 127);

   g_CurrentUploadingFile.uLastSegmentIndexUploaded = 0xFFFFFFFF;
   g_bHasFileUploadInProgress = true;
}

bool handle_commands_send_developer_flags(u32 uDevFlags)
{
   if ( NULL == g_pCurrentModel )
      return false;

   ControllerSettings* pCS = get_ControllerSettings();
   if ( pCS->iDeveloperMode )
      uDevFlags |= DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE;
   else
      uDevFlags &= ~DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE;

   log_line("Sending new developer flags to vehicle: %s", str_get_developer_flags(uDevFlags));

   u8 buffer[32];
   memcpy(&buffer[0], (u8*)&uDevFlags, sizeof(u32));
   u32 uTmp = (u32)pCS->iDevRxLoopTimeout;
   memcpy(&buffer[sizeof(u32)], (u8*)&uTmp, sizeof(u32));
   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_DEVELOPER_FLAGS, 0, buffer, 2*sizeof(u32)) )
      return false;
   return true;
}