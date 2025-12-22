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

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../radio/fec.h" 

#include "../base/base.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/shared_mem.h"
#include "../base/models.h"
#include "../base/radio_utils.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../common/string_utils.h"

#include "shared_vars.h"
#include "timers.h"
#include "rx_video_recording_data.h"

static bool s_bRecordingDataOSDStarted = false;
static bool s_bRecordingDataSRTStarted = false;

FILE* s_pFileRecordingSRTData = NULL;
u32 s_uTimeStartedRecordingSRTData = 0;
u32 s_uLastTimeRecordedSRTData = 0;
int s_iSRTDataFrameCount = 0;

FILE* s_pFileRecordingOSDData = NULL;
u32 s_uTimeStartedRecordingOSDData = 0;
u32 s_uLastTimeRecordedOSDData = 0;
int s_iOSDDataFrameCount = 0;

void rx_video_recording_data_start_srt()
{
   if ( s_bRecordingDataSRTStarted )
      return;

   char szFile[MAX_FILE_PATH_SIZE];
   char szComm[MAX_FILE_PATH_SIZE];

   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_TEMP_VIDEO_FILE_SRT);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s 2>/dev/null", szFile);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "touch %s", szFile);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s 2>/dev/null", szFile);
   hw_execute_bash_command(szComm, NULL);
   s_pFileRecordingSRTData = fopen(szFile, "wb");
   if ( NULL == s_pFileRecordingSRTData )
   {
      log_softerror_and_alarm("[VideoRecordingData] Failed to create temp SRT recording file [%s]", szFile);
      return;
   }
   s_uTimeStartedRecordingSRTData = g_TimeNow;
   s_uLastTimeRecordedSRTData = g_TimeNow;
   s_iSRTDataFrameCount = 1;
   s_bRecordingDataSRTStarted = true;
   log_line("[VideoRecordingData] Started recording STR data");
}

void rx_video_recording_data_stop_srt()
{
   if ( NULL != s_pFileRecordingSRTData )
      fclose(s_pFileRecordingSRTData);
   s_pFileRecordingSRTData = NULL;

   if ( ! s_bRecordingDataSRTStarted )
      return;

   s_bRecordingDataSRTStarted = false;
   log_line("[VideoRecordingData] Stopped recording STR data");
}

void rx_video_recording_data_start_osd()
{
   if ( s_bRecordingDataOSDStarted )
      return;

   char szFile[MAX_FILE_PATH_SIZE];
   char szComm[MAX_FILE_PATH_SIZE];

   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_TEMP_VIDEO_FILE_OSD);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s 2>/dev/null", szFile);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "touch %s", szFile);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s 2>/dev/null", szFile);
   hw_execute_bash_command(szComm, NULL);
   s_pFileRecordingOSDData = fopen(szFile, "wb");
   if ( NULL == s_pFileRecordingOSDData )
   {
      log_softerror_and_alarm("[VideoRecordingData] Failed to create temp OSD recording file [%s]", szFile);
      return;
   }
   s_uTimeStartedRecordingOSDData = g_TimeNow;
   s_uLastTimeRecordedOSDData = 0;
   s_iOSDDataFrameCount = 0;
   s_bRecordingDataOSDStarted = true;
   log_line("[VideoRecordingData] Started recording OSD data");
}

void rx_video_recording_data_stop_osd()
{
   if ( NULL != s_pFileRecordingOSDData )
      fclose(s_pFileRecordingOSDData);
   s_pFileRecordingOSDData = NULL;

   if ( ! s_bRecordingDataOSDStarted )
      return;

   s_bRecordingDataOSDStarted = false;
   log_line("[VideoRecordingData] Stopped recording OSD data");
}

float _convertMeters(float m)
{
   Preferences* p = get_Preferences();

   if ( (p->iUnits == prefUnitsMetric) || (p->iUnits == prefUnitsMeters) )
      return m;
   if ( (p->iUnits == prefUnitsImperial) || (p->iUnits == prefUnitsFeets) )
      return m*3.28084;
   return m;
}

void rx_video_recording_data_add_srt_frame()
{
   if ( NULL == s_pFileRecordingSRTData )
      return;

   u32 uDeltaTimeStartMs = s_uLastTimeRecordedSRTData - s_uTimeStartedRecordingSRTData;
   u32 uDeltaTimeEndMs = g_TimeNow - s_uTimeStartedRecordingSRTData;
   fprintf(s_pFileRecordingSRTData, "%d\n", s_iSRTDataFrameCount);
   fprintf(s_pFileRecordingSRTData, "%02u:%02u:%02u,%03u --> %02u:%02u:%02u,%03u\n",
      uDeltaTimeStartMs/3600000, (uDeltaTimeStartMs % 3600000) / 60000,
      (uDeltaTimeStartMs % 60000) / 1000, uDeltaTimeStartMs % 1000,
      uDeltaTimeEndMs/3600000, (uDeltaTimeEndMs % 3600000) / 60000,
      (uDeltaTimeEndMs % 60000) / 1000,  uDeltaTimeEndMs % 1000);

   if ( g_pControllerSettings->iRecordSTRHome || g_pControllerSettings->iRecordSTRAlt || g_pControllerSettings->iRecordSTRGPS || g_pControllerSettings->iRecordSTRVoltage )
   {
      bool bHasData = true;
      if ( (NULL == g_pCurrentModel) || (g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MAVLINK) )
         bHasData = false;

      type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(g_pCurrentModel->uVehicleId);
      if ( (NULL == pRuntimeInfo) || ((0 == pRuntimeInfo->uTimeLastRecvRubyTelemetryExtended) && (0 == pRuntimeInfo->uTimeLastRecvRubyTelemetryShort)) )
         bHasData = false;
      if ( (NULL == pRuntimeInfo) || (0 == pRuntimeInfo->uTimeLastRecvFCTelemetryFC) )
         bHasData = false;
      if ( bHasData )
      {
         Preferences* p = get_Preferences();
         bool bAddedAnything = false;
         if ( g_pControllerSettings->iRecordSTRHome )
         {
             float fDist = _convertMeters(pRuntimeInfo->headerFCTelemetry.distance/100.0);
             fprintf(s_pFileRecordingSRTData, "D: %u", (unsigned int) fDist);
             if ( (p->iUnits == prefUnitsImperial) || (p->iUnits == prefUnitsFeets) )
                fprintf(s_pFileRecordingSRTData, "ft");
             else
                fprintf(s_pFileRecordingSRTData, "m");
             bAddedAnything = true;
         }
         if ( g_pControllerSettings->iRecordSTRAlt )
         {
            if ( bAddedAnything )
               fprintf(s_pFileRecordingSRTData, "  ");

            float fAlt = _convertMeters(pRuntimeInfo->headerFCTelemetry.altitude_abs/100.0f-1000.0);
            if ( g_pCurrentModel->osd_params.altitude_relative )
               fAlt = _convertMeters(pRuntimeInfo->headerFCTelemetry.altitude/100.0f-1000.0);
            if ( fAlt < -500 )
               fprintf(s_pFileRecordingSRTData, "H: ---");
            else
            {
               if ( fAlt < 10.0 )
                  fprintf(s_pFileRecordingSRTData, "H: %.1f", fAlt);
               else
                  fprintf(s_pFileRecordingSRTData, "H: %d", (int)fAlt);
               if ( (p->iUnits == prefUnitsImperial) || (p->iUnits == prefUnitsFeets) )
                  fprintf(s_pFileRecordingSRTData, "ft");
               else
                  fprintf(s_pFileRecordingSRTData, "m");
            }
            bAddedAnything = true;
         }

         if ( g_pControllerSettings->iRecordSTRGPS )
         {
            if ( bAddedAnything )
               fprintf(s_pFileRecordingSRTData, "  ");

            fprintf(s_pFileRecordingSRTData, "%.6f, %.6f",
               pRuntimeInfo->headerFCTelemetry.latitude/10000000.0f,
               pRuntimeInfo->headerFCTelemetry.longitude/10000000.0f);
            bAddedAnything = true;
         }
         if ( g_pControllerSettings->iRecordSTRVoltage )
         {
            if ( bAddedAnything )
               fprintf(s_pFileRecordingSRTData, "  ");
            fprintf(s_pFileRecordingSRTData, "%.1f V", pRuntimeInfo->headerFCTelemetry.voltage/1000.0);
            bAddedAnything = true;
         }
         fprintf(s_pFileRecordingSRTData, "\n");
      }
      else
         fprintf(s_pFileRecordingSRTData, "No MAVLink telemetry\n");
   }
   int iCountSecLine = 0;
   if ( g_pControllerSettings-> iRecordSTRTime )
   {
      if ( iCountSecLine )
         fprintf(s_pFileRecordingSRTData, "  ");
      fprintf(s_pFileRecordingSRTData, "%02u:%02u", uDeltaTimeEndMs / 60000, (uDeltaTimeEndMs % 60000) / 1000);
      iCountSecLine++;
   }

   if ( g_pControllerSettings->iRecordSTRRSSI )
   {
      if ( iCountSecLine )
         fprintf(s_pFileRecordingSRTData, "  ");

      for ( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
            continue;
         if ( ! pRadioHWInfo->isHighCapacityInterface )
            continue;
         
         fprintf(s_pFileRecordingSRTData, "Radio %d: ", i+1);

         int iRadioDBM = g_SMControllerRTInfo.radioInterfacesSignals[i].iMaxDBMVideoForInterface;
         int iSNR = g_SMControllerRTInfo.radioInterfacesSignals[i].iMaxSNRVideoForInterface;
         
         if ( (NULL == g_pCurrentModel) || (! g_pCurrentModel->hasCamera()) )
         {
            iRadioDBM = g_SMControllerRTInfo.radioInterfacesSignals[i].iMaxDBMDataForInterface;
            iSNR = g_SMControllerRTInfo.radioInterfacesSignals[i].iMaxSNRDataForInterface;
         }

         if ( (iRadioDBM > -500) && (iRadioDBM < 500) )
            fprintf(s_pFileRecordingSRTData, "%d dBm  ", iRadioDBM);

         if ( (iSNR > -500) && (iSNR < 500) )
            fprintf(s_pFileRecordingSRTData, "%d SNR  ", iSNR);

      }

      iCountSecLine++;
   }
 
   if ( g_pControllerSettings->iRecordSTRBitrate )
   {
      if ( iCountSecLine )
         fprintf(s_pFileRecordingSRTData, "  ");
      shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, g_pCurrentModel->uVehicleId);
      type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(g_pCurrentModel->uVehicleId);
      if ( (NULL == pSMVideoStreamInfo) || (NULL == pRuntimeInfo) )
         fprintf(s_pFileRecordingSRTData, "--- Mb/s");
      else
      {
         u32 totalMaxVideo_bps = 0;
         for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
         {
            if ( g_SM_RadioStats.radio_streams[i][STREAM_ID_VIDEO_1].uVehicleId != g_pCurrentModel->uVehicleId )
               continue;
            totalMaxVideo_bps = g_SM_RadioStats.radio_streams[i][STREAM_ID_VIDEO_1].rxBytesPerSec * 8;
            break;
         }
         fprintf(s_pFileRecordingSRTData, "%.1f Mbps", (float)totalMaxVideo_bps/1000.0/1000.0);
      }
      iCountSecLine++;
   }

   if ( iCountSecLine )
      fprintf(s_pFileRecordingSRTData, "\n");
 
   fprintf(s_pFileRecordingSRTData, "\n");
   s_iSRTDataFrameCount++;
}

void rx_video_recording_data_add_osd_frame()
{
   if ( NULL == s_pFileRecordingOSDData )
      return;

   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(g_pCurrentModel->uVehicleId);
   if ( NULL == pRuntimeInfo )
      return;

   if ( 0 == s_uLastTimeRecordedOSDData )
   {
      char uHeader[40];
      memset(uHeader, 0, 40);
      switch ( pRuntimeInfo->mspState.headerTelemetryMSP.uMSPFlags & MSP_FLAGS_FC_TYPE_MASK )
      {
         case MSP_FLAGS_FC_TYPE_BETAFLIGHT: strcpy(uHeader, "BTFL"); break;
         case MSP_FLAGS_FC_TYPE_INAV: strcpy(uHeader, "INAV"); break;
         case MSP_FLAGS_FC_TYPE_PITLAB: strcpy(uHeader, "PITL"); break;
         case MSP_FLAGS_FC_TYPE_ARDUPILOT: strcpy(uHeader, "ARDU"); break;
      }
      fwrite(uHeader, 1, 40, s_pFileRecordingOSDData);
      s_uLastTimeRecordedOSDData = g_TimeNow;
      return;
   }

   if ( pRuntimeInfo->mspState.iLastDrawFrameNumber == s_iOSDDataFrameCount )
      return;

   u32 uTimeMs = g_TimeNow - s_uTimeStartedRecordingOSDData;
   fwrite((u8*)&uTimeMs, 1, sizeof(u32), s_pFileRecordingOSDData);
   u16 uBuffer[MAX_MSP_CHARS_BUFFER];
   int iBuffPos = 0;
   for( int y=0; y<DEFAULT_MSPOSD_RECORDING_ROWS; y++ )
   for( int x=0; x<DEFAULT_MSPOSD_RECORDING_COLS; x++ )
      uBuffer[iBuffPos++] = pRuntimeInfo->mspState.uScreenChars[x + y * pRuntimeInfo->mspState.headerTelemetryMSP.uMSPOSDCols];
      
   fwrite(uBuffer, 1, iBuffPos * sizeof(u16), s_pFileRecordingOSDData);
   s_iOSDDataFrameCount = pRuntimeInfo->mspState.iLastDrawFrameNumber;
   s_uLastTimeRecordedOSDData = g_TimeNow;
}

void rx_video_recording_periodic_data_loop()
{
   if ( s_bRecordingDataSRTStarted )
   {
      u32 uIntervalMs = 200;
      if ( g_pControllerSettings->iRecordSTRFramerate == 0 )
         uIntervalMs = 50;
      if ( g_pControllerSettings->iRecordSTRFramerate == 1 )
         uIntervalMs = 100;
      if ( g_pControllerSettings->iRecordSTRFramerate == 2 )
         uIntervalMs = 200;
      if ( g_pControllerSettings->iRecordSTRFramerate == 3 )
         uIntervalMs = 500;
      if ( g_TimeNow >= s_uLastTimeRecordedSRTData + uIntervalMs )
      {
         rx_video_recording_data_add_srt_frame();
         s_uLastTimeRecordedSRTData = g_TimeNow;
      }
   }

   if ( s_bRecordingDataOSDStarted )
      rx_video_recording_data_add_osd_frame();
}

