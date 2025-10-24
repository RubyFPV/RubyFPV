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
#include "../base/config.h"
#include "../base/shared_mem.h"
#include "../base/hardware_procs.h"
#include "../base/hardware_camera.h"
#include "../base/hardware_cam_maj.h"
#include "../base/ruby_ipc.h"
#include "../base/camera_utils.h"
#include "../base/utils.h"
#include "../common/string_utils.h"

#include <errno.h>
#include <sys/stat.h>
#include <math.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "video_sources.h"
#include "events.h"
#include "timers.h"
#include "shared_vars.h"
#include "adaptive_video.h"
#include "video_source_csi.h"
#include "video_source_majestic.h"
#include "video_source_wifi_direct.h"
#include "negociate_radio.h"
#include "packets_utils.h"


u32 s_uLastVideoSourcesStreamData = 0;
u32 s_uTotalVideoSourceReadBytes = 0;
u32 s_uLastSetVideoBitrateBPS = 0;
int s_iLastSetIPQDelta = -1000;
int s_iLastSetVideoKeyframeMs = 0;

void video_sources_start_capture()
{
   log_line("[VideoSources] Start capture begin (initial video bitrate: %.3f Mbps, QPdelta: %d, KF: %d ms)...",
      (float)s_uLastSetVideoBitrateBPS/1000.0/1000.0, s_iLastSetIPQDelta, s_iLastSetVideoKeyframeMs );

   s_uTotalVideoSourceReadBytes = 0;
   if ( ! g_pCurrentModel->hasCamera() )
   {
      log_line("[VideoSources] Vehicle has no camera. Init done. Video capture not started.");
      return;
   }

   log_line("[VideoSources] Current camera type: %s", str_get_hardware_camera_type_string(g_pCurrentModel->getActiveCameraType()));
   u32 uInitialVideoBitrate = 0;
   int iInitialKeyframeMs = 0;
   
   // Check if WiFi Direct is enabled as an additional video source
   bool bWiFiDirectEnabled = false;
   wifi_direct_config_t wifiConfig;
   memset(&wifiConfig, 0, sizeof(wifiConfig));
   
   // Load WiFi Direct config from model settings
   if (g_pCurrentModel->wifi_direct_params.uFlags & WIFI_DIRECT_FLAG_ENABLED) {
      wifiConfig.enabled = true;
      strcpy(wifiConfig.vtx_ip, g_pCurrentModel->wifi_direct_params.szVTXIP);
      wifiConfig.udp_port = g_pCurrentModel->wifi_direct_params.iPort;
      wifiConfig.is_multicast = (g_pCurrentModel->wifi_direct_params.uFlags & WIFI_DIRECT_FLAG_MULTICAST) ? true : false;
      strcpy(wifiConfig.multicast_group, "239.255.0.1"); // Default multicast group
   } else {
      wifiConfig.enabled = false;
   }
   
   if (wifiConfig.enabled) {
      log_line("[VideoSources] WiFi Direct video source is enabled");
      video_source_wifi_direct_set_config(&wifiConfig);
      if (video_source_wifi_direct_start()) {
         bWiFiDirectEnabled = true;
         log_line("[VideoSources] WiFi Direct video source started successfully");
      } else {
         log_softerror_and_alarm("[VideoSources] Failed to start WiFi Direct video source");
      }
   }
   
   // Start regular camera sources
   if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
      uInitialVideoBitrate = video_source_csi_start_program(s_uLastSetVideoBitrateBPS, s_iLastSetVideoKeyframeMs, &iInitialKeyframeMs);
   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
      uInitialVideoBitrate = video_source_majestic_start_program(s_uLastSetVideoBitrateBPS, s_iLastSetVideoKeyframeMs, s_iLastSetIPQDelta, &iInitialKeyframeMs);

   s_uLastSetVideoBitrateBPS = uInitialVideoBitrate;
   s_iLastSetVideoKeyframeMs = iInitialKeyframeMs;

   log_line("[VideoSources] Start capture completed.");
}

void video_sources_stop_capture()
{
   log_line("[VideoSources] Stop capture begin...");

   // Stop WiFi Direct if running
   if (video_source_wifi_direct_is_started()) {
      log_line("[VideoSources] Stopping WiFi Direct video source");
      video_source_wifi_direct_stop();
   }

   if ( (NULL == g_pCurrentModel) || (! g_pCurrentModel->hasCamera()) )
   {
      log_line("[VideoSources] Vehicle has no camera. Uninit done. Video capture not stopped.");
      return;
   }

   if ( (NULL != g_pCurrentModel) && g_pCurrentModel->isActiveCameraOpenIPC() )
      video_source_majestic_stop_program();
   else
      video_source_csi_stop_program();

   log_line("[VideoSources] Stop capture completed.");
}

u32 video_sources_get_capture_start_time()
{
   if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
      return video_source_cs_get_program_start_time();
   return video_source_majestic_get_program_start_time(); 
}

void video_sources_flush_all_pending_data()
{
   log_line("[VideoSources] Clear video pipes...");
      
   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
      video_source_majestic_clear_input_buffers();
   if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
      video_source_csi_flush_discard();
   log_line("[VideoSources] Done clear video pipes.");
}


// Returns true if any video data was read and processed
bool video_sources_read_process_stream(bool* pbEndOfFrameDetected, int iHasPendingDataPacketsToSend)
{
   if ( NULL != pbEndOfFrameDetected )
      *pbEndOfFrameDetected = false;
   
   // Check WiFi Direct video source first (if enabled)
   if (video_source_wifi_direct_is_started() && video_source_wifi_direct_has_data()) {
      static u8 wifiDirectBuffer[4096];  // Buffer for WiFi Direct data
      int iWiFiReadSize = video_source_wifi_direct_read(wifiDirectBuffer, sizeof(wifiDirectBuffer));
      
      if (iWiFiReadSize > 0) {
         s_uLastVideoSourcesStreamData = g_TimeNow;
         s_uTotalVideoSourceReadBytes += iWiFiReadSize;
         
         // Process WiFi Direct video data
         // Assume it's already packetized H.264 stream
         bool bIsEndOfFrame = false;  // TODO: Implement frame boundary detection
         
         if ( NULL != g_pVideoTxBuffers )
            g_pVideoTxBuffers->fillVideoPacketsFromCSI(wifiDirectBuffer, iWiFiReadSize, bIsEndOfFrame, iHasPendingDataPacketsToSend);
         
         if ( NULL != pbEndOfFrameDetected )
            *pbEndOfFrameDetected = bIsEndOfFrame;
         
         // Return true to indicate we processed data
         // Continue to also check regular camera sources
         return true;
      }
   }
   
   if ( ! g_pCurrentModel->hasCamera() )
      return false;

   u8* pVideoData = NULL;
   int iReadSize = 0;
 
   if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
   {
      pVideoData = video_source_csi_read(&iReadSize);
      if ( (iReadSize > 0) && (NULL != pVideoData) )
      {
         s_uLastVideoSourcesStreamData = g_TimeNow;
         s_uTotalVideoSourceReadBytes += iReadSize;
         int iBuffSize = video_source_csi_get_buffer_size();
         bool bIsEndOfFrame = (iReadSize < iBuffSize)?true:false;
         // Concatenate SPS,PSP units to the next I/P unit
         if ( iReadSize < 50 )
            bIsEndOfFrame = false;

         if ( NULL != g_pVideoTxBuffers )
            g_pVideoTxBuffers->fillVideoPacketsFromCSI(pVideoData, iReadSize, bIsEndOfFrame, iHasPendingDataPacketsToSend);
         if ( NULL != pbEndOfFrameDetected )
            *pbEndOfFrameDetected = bIsEndOfFrame;
         return true;
      }
   }
   
   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
   {
      return video_source_majestic_read_process_stream(pbEndOfFrameDetected, iHasPendingDataPacketsToSend);

      /*
      bool bDidReadVideoData = false;
      bool bIsEndOfFrame = false;

      // Read mulitple times for end of frame detection and backward adjustment
      for( int k=0; k<4; k++ )
      {
         pVideoData = video_source_majestic_read(&iReadSize, true);
         if ( (iReadSize > 0) && (NULL != pVideoData) )
         {
            s_uLastVideoSourcesStreamData = g_TimeNow;
            s_uTotalVideoSourceReadBytes += iReadSize;
            bDidReadVideoData = true;
            bool bSingle = video_source_majestic_last_read_is_single_nal();
            bool bEnd = video_source_majestic_last_read_is_end_nal();
            u32 uNALType = video_source_majestic_get_last_nal_type();

            bIsEndOfFrame |= g_pVideoTxBuffers->fillVideoPacketsFromRTSPPacket(pVideoData, iReadSize, bSingle, bEnd, uNALType, iHasPendingDataPacketsToSend);
         }
         if ( (iReadSize <= 0) || bIsEndOfFrame )
            break;
      }
      if ( NULL != pbEndOfFrameDetected )
         *pbEndOfFrameDetected = bIsEndOfFrame;
      return bDidReadVideoData;
      */
   }
   return false;
}

bool video_sources_has_stream_data()
{
   return (s_uTotalVideoSourceReadBytes > 0)?true:false;
}

u32 video_sources_last_stream_data()
{
   return s_uLastVideoSourcesStreamData;
}

void video_sources_apply_all_parameters()
{
   log_line("[VideoSources] Requested to apply all cam/video parameters.");
   log_line("[VideoSources] Current video profile video bitrate: %.2f Mbps, current set video bitrate: %.2f Mbps, current set IPQ delta: %d",
      (float)g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].bitrate_fixed_bps/1000.0/1000.0,
      (float)s_uLastSetVideoBitrateBPS/1000.0/1000.0,
      s_iLastSetIPQDelta);
   log_line("[VideoSources] Current video profile keyframe ms: %d, current set keyframe ms: %d",
      g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].keyframe_ms,
      s_iLastSetVideoKeyframeMs);

   if ( NULL != g_pVideoTxBuffers )
   {
      g_pVideoTxBuffers->discardBuffer();
      g_pVideoTxBuffers->updateVideoHeader(g_pCurrentModel);
   }

   if ( ! g_pCurrentModel->hasCamera() )
   {
      log_line("[VideoSources] Vehicle has no camera. Apply all paramters request ignored.");
      return;
   }

   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
      video_source_majestic_apply_all_parameters();
   else
      video_source_csi_request_restart_program();

   log_line("[VideoSources] Done handling requested apply all cam/video parameters.");
}


void video_sources_on_changed_camera_params(type_camera_parameters* pNewCamParams, type_camera_parameters* pOldCamParams)
{
   if ( (NULL == pNewCamParams) || (NULL == pOldCamParams) || ( ! g_pCurrentModel->hasCamera()))
      return;

   g_pCurrentModel->log_camera_profiles_differences(&(pOldCamParams->profiles[pOldCamParams->iCurrentProfile]), &(pNewCamParams->profiles[pNewCamParams->iCurrentProfile]), pOldCamParams->iCurrentProfile, pNewCamParams->iCurrentProfile);

   log_line("[VideoSources] Start applying all camera params due to received changes...");

   if ( (pNewCamParams->iCameraBinProfile != pOldCamParams->iCameraBinProfile) ||
        (0 != strcmp(pNewCamParams->szCameraBinProfileName, pOldCamParams->szCameraBinProfileName)) )
   {
      log_line("[VideoSources] Camera calibration file changed from (%s) type %d to (%s) type %d",
         pOldCamParams->szCameraBinProfileName, pOldCamParams->iCameraBinProfile,
         pNewCamParams->szCameraBinProfileName, pNewCamParams->iCameraBinProfile);

      if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
      {
         hardware_camera_maj_set_calibration_file(g_pCurrentModel->getActiveCameraType(), g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraBinProfile, g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName);
         video_source_majestic_clear_input_buffers(); 
      }
   }

   if ( g_pCurrentModel->isActiveCameraVeye307() && (fabs(pNewCamParams->profiles[pNewCamParams->iCurrentProfile].hue - pOldCamParams->profiles[pOldCamParams->iCurrentProfile].hue) > 0.1 ) )
      video_source_csi_update_camera_params(g_pCurrentModel, g_pCurrentModel->iCurrentCamera);
   else if ( g_pCurrentModel->isActiveCameraVeye327290() )
      video_source_csi_update_camera_params(g_pCurrentModel, g_pCurrentModel->iCurrentCamera);
   else if ( g_pCurrentModel->isActiveCameraVeye307() )
      video_source_csi_update_camera_params(g_pCurrentModel, g_pCurrentModel->iCurrentCamera);
   else
      video_sources_apply_all_parameters();
   log_line("[VideoSources] Done applying all camera params due to received changes.");
}

void video_sources_on_changed_developer_flags(u32 uOldDeveloperFlags, u32 uNewDeveloperFlags)
{
   bool bOldDevMode = (uNewDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE)?true:false;
   bool bNewDevMode = (uOldDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE)?true:false;
   if ( bOldDevMode != bNewDevMode )
   {
      log_line("[VideoSources] Dev mode changed from %s to %s", (uOldDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE)?"[on]":"[off]", (uNewDeveloperFlags)?"[on]":"[off]");
      video_sources_apply_all_parameters();
   }
}

void video_sources_on_changed_video_params(camera_profile_parameters_t* pOldCameraProfileParams, video_parameters_t* pOldVideoParams, type_video_link_profile* pOldVideoProfiles)
{
   if ( ! g_pCurrentModel->hasCamera() )
   {
      log_softerror_and_alarm("[VideoSources] Vehicle has no camera. No video changes to apply.");
      return;
   }
   if ( NULL == pOldCameraProfileParams )
   {
      log_softerror_and_alarm("[VideoSources] Received invalid change in video parameters: NULL camera profile params.");
      return;
   }
   if ( NULL == pOldVideoParams )
   {
      log_softerror_and_alarm("[VideoSources] Received invalid change in video parameters: NULL video params.");
      return;
   }
   if ( NULL == pOldVideoProfiles )
   {
      log_softerror_and_alarm("[VideoSources] Received invalid change in video parameters: NULL video profiles.");
      return;
   }

   camera_profile_parameters_t* pCurrentCameraProfileParams = &g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile];
   video_parameters_t* pNewVideoParams = &(g_pCurrentModel->video_params);
   type_video_link_profile* pOldVideoProfile = &(pOldVideoProfiles[pOldVideoParams->iCurrentVideoProfile]);
   type_video_link_profile* pNewVideoProfile = &(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile]);
   char szProfileOld[32];
   char szProfileNew[32];
   strcpy(szProfileOld, str_get_video_profile_name(pOldVideoParams->iCurrentVideoProfile));
   strcpy(szProfileNew, str_get_video_profile_name(pNewVideoParams->iCurrentVideoProfile));
   log_line("[VideoSources] Changed video params. Old video profile: %s, new video profile: %s", szProfileOld, szProfileNew);
   g_pCurrentModel->logVideoSettingsDifferences(pOldVideoParams, pOldVideoProfile, true);
   log_line("[VideoSources] Processing differences...");
   int iCountChanges = 0;
   int iCountChangesApplied = 0;
   int iCountChangesTransparent = 0;

   bool bDoFullRestart = false;
   bool bDiscardTxBuffers = false;

   if ( negociate_radio_link_is_in_progress() )
   {
      log_line("[VideoSources] Negociate radio flow is in progress. Do no changes now.");
      if ( pOldVideoProfile->bitrate_fixed_bps != pNewVideoProfile->bitrate_fixed_bps )
         negociate_radio_set_end_video_bitrate(pNewVideoProfile->bitrate_fixed_bps);
      return;
   }

   if ( pCurrentCameraProfileParams->shutterspeed != pOldCameraProfileParams->shutterspeed )
   {
      iCountChanges++;
      iCountChangesApplied++;
      // To fix may2025: set exposure for majestic
   }

   if ( (pOldVideoParams->iVideoWidth != pNewVideoParams->iVideoWidth) ||
        (pOldVideoParams->iVideoHeight != pNewVideoParams->iVideoHeight) ||
        (pOldVideoParams->iVideoFPS != pNewVideoParams->iVideoFPS) ||
        (pOldVideoParams->iH264Slices != pNewVideoParams->iH264Slices) ||
        ((pOldVideoParams->uVideoExtraFlags & (~VIDEO_FLAG_RETRANSMISSIONS_FAST)) != (pNewVideoParams->uVideoExtraFlags & (~VIDEO_FLAG_RETRANSMISSIONS_FAST))) ||
        ((pOldVideoParams->uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265) != (pNewVideoParams->uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265)) ||
 
        (pOldVideoProfile->h264profile != pNewVideoProfile->h264profile) ||
        (pOldVideoProfile->h264level != pNewVideoProfile->h264level) ||
        (pOldVideoProfile->h264refresh != pNewVideoProfile->h264refresh) ||
        (pOldVideoProfile->h264quantization != pNewVideoProfile->h264quantization) )
      bDoFullRestart = true;
   
   if ( hardware_is_running_on_openipc() )
   if ( pOldVideoProfile->video_data_length != pNewVideoProfile->video_data_length )
      bDoFullRestart = true;

   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
   if ( hardware_board_is_goke(g_pCurrentModel->hwCapabilities.uBoardType) )
   if ( pOldVideoProfile->bitrate_fixed_bps != pNewVideoProfile->bitrate_fixed_bps )
   {
      log_line("[VideoSources] Video bitrate changed on Goke. Do full restart.");
      bDoFullRestart = true;
   }
   //if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
   //if ( pOldVideoParams->iCurrentVideoProfile != pNewVideoParams->iCurrentVideoProfile )
   //   bDoFullRestart = true;

   if ( bDoFullRestart )
   {
      log_line("[VideoSources] Do a full apply/restart of video capture settings and program.");

      // To fix may 2025
      //adaptive_video_init();

      video_sources_apply_all_parameters();
      log_line("[VideoSources] Finished processing video settings changes.");
      return;
   }

   if ( pOldVideoProfile->iAdaptiveAdjustmentStrength != pNewVideoProfile->iAdaptiveAdjustmentStrength )
   {
      iCountChanges++;
      iCountChangesTransparent++;
   }
   if ( pOldVideoParams->uMaxAutoKeyframeIntervalMs != pNewVideoParams->uMaxAutoKeyframeIntervalMs )
   {
      iCountChanges++;
      iCountChangesApplied++;
   }

   if ( pOldVideoParams->iCurrentVideoProfile != pNewVideoParams->iCurrentVideoProfile )
   {
      log_line("[VideoSources] OnChangedVideoParams: Handle selected video profile change...");
      iCountChanges++;
      iCountChangesApplied++;

      if ( NULL != g_pVideoTxBuffers )
         g_pVideoTxBuffers->updateVideoHeader(g_pCurrentModel);
      
      bDiscardTxBuffers = true;
      if ( g_pCurrentModel->isActiveCameraOpenIPC() )
         hardware_camera_maj_clear_temp_values();
      log_line("[VideoSources] OnChangedVideoParams: Handled selected video profile change.");
   }

   if ( (pOldVideoProfile->keyframe_ms != pNewVideoProfile->keyframe_ms) ||
        (pOldVideoProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME) !=  (pNewVideoProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME) )
   {
      log_line("[VideoSources] OnChangedVideoParams: Handle keyframe change...");
      iCountChanges++;
      iCountChangesApplied++;
      log_line("[VideoSources] Old/New profile adaptive keyframe: %s / %s",
         (pOldVideoProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME)?"yes":"no",
         (pNewVideoProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME)?"yes":"no");
      log_line("[VideoSources] Old/New value for user selected video profile keyframe: %d / %d ms",
         pOldVideoProfile->keyframe_ms, pNewVideoProfile->keyframe_ms);

      if ( ! (pNewVideoProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME) )
      if ( pNewVideoProfile->keyframe_ms > 0 )
      {
         adaptive_video_reset_requested_keyframe();
         video_sources_set_keyframe(pNewVideoProfile->keyframe_ms);
      }
      log_line("[VideoSources] OnChangedVideoParams: Handled keyframe change.");
   }

   if ( ((pOldVideoProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK) != (pNewVideoProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK)) ||
        ((pOldVideoProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO) != (pNewVideoProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO)) ||
        ((pOldVideoProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO) != (pNewVideoProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO)) )
   {
      log_line("[VideoSources] OnChangedVideoParams: Handle adaptive video flags change...");

      if ( ! (pNewVideoProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK) )
      {
         adaptive_video_reset_to_defaults();
         if ( NULL != g_pVideoTxBuffers )
            g_pVideoTxBuffers->setCustomECScheme(0);
         video_sources_set_video_bitrate(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].bitrate_fixed_bps, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iIPQuantizationDelta, "AdaptiveVideo turned off");
         packet_utils_set_adaptive_video_bitrate(0);
      }
      iCountChanges++;
      iCountChangesApplied++;
      log_line("[VideoSources] OnChangedVideoParams: Handled adaptive video flags change.");
   }

   if ( (pOldVideoProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ONE_WAY_FIXED_VIDEO) != (pNewVideoProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ONE_WAY_FIXED_VIDEO) )
   {
      log_line("[VideoSources] OnChangedVideoParams: Handle one way video link change...");

      if ( pNewVideoProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ONE_WAY_FIXED_VIDEO )
      {
         log_line("[VideoSources] Video link was set to one way.");
         adaptive_video_reset_to_defaults();
         if ( NULL != g_pVideoTxBuffers )
            g_pVideoTxBuffers->setCustomECScheme(0);
         video_sources_set_video_bitrate(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].bitrate_fixed_bps, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iIPQuantizationDelta, "AdaptiveVideo turned off");
         packet_utils_set_adaptive_video_bitrate(0);
      }
      iCountChanges++;
      iCountChangesApplied++;
      log_line("[VideoSources] OnChangedVideoParams: Handled one way video link change.");
   }

   if ( (pOldVideoProfile->iBlockDataPackets != pNewVideoProfile->iBlockDataPackets) ||
        (pOldVideoProfile->iBlockECs != pNewVideoProfile->iBlockECs) ||
        (pOldVideoProfile->iECPercentage != pNewVideoProfile->iECPercentage) )
   {
      log_line("[VideoSources] OnChangedVideoParams: Handle EC scheme change...");
      iCountChanges++;
      iCountChangesApplied++;
      if ( NULL != g_pVideoTxBuffers )
         g_pVideoTxBuffers->updateVideoHeader(g_pCurrentModel);
      log_line("[VideoSources] OnChangedVideoParams: Handled EC scheme change.");
   }

   if ( pOldVideoProfile->video_data_length != pNewVideoProfile->video_data_length )
   {
      log_line("[VideoSources] OnChangedVideoParams: Handle video data length change...");
      iCountChanges++;
      iCountChangesApplied++;
      if ( NULL != g_pVideoTxBuffers )
         g_pVideoTxBuffers->updateVideoHeader(g_pCurrentModel);
      log_line("[VideoSources] OnChangedVideoParams: Handled video data length change.");
   }

   if ( pOldVideoProfile->bitrate_fixed_bps != pNewVideoProfile->bitrate_fixed_bps )
   {
      log_line("[VideoSources] OnChangedVideoParams: Handle video bitrate change...");
      iCountChanges++;
      iCountChangesApplied++;
      bool bApplyNow = false;
      if ( g_pCurrentModel->isVideoLinkFixedOneWay() ||
          ( !(pNewVideoProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK)) )
         bApplyNow = true;

      if ( 0 != s_uLastSetVideoBitrateBPS )
      if ( pNewVideoProfile->bitrate_fixed_bps < s_uLastSetVideoBitrateBPS )
         bApplyNow = true;

      if ( g_pCurrentModel->isAllVideoLinksFixedRate() )
         bApplyNow = true;

      if ( bApplyNow )
        video_sources_set_video_bitrate(pNewVideoProfile->bitrate_fixed_bps, pNewVideoProfile->iIPQuantizationDelta, "Updated video profile video bitrate");
      else
         log_line("[VideoSources] Do not update video bitrate as it will be updated by adaptive video.");
      log_line("[VideoSources] OnChangedVideoParams: Handled video bitrate change.");
   }

   if ( pOldVideoProfile->iIPQuantizationDelta != pNewVideoProfile->iIPQuantizationDelta )
   {
      log_line("[VideoSources] OnChangedVideoParams: Handle video quantization change...");
      iCountChanges++;
      if ( g_pCurrentModel->isActiveCameraOpenIPC() && hardware_board_is_sigmastar(g_pCurrentModel->hwCapabilities.uBoardType) )
      {
         hardware_camera_maj_set_qpdelta(pNewVideoProfile->iIPQuantizationDelta);
         iCountChangesApplied++;
      }
      else
         iCountChangesTransparent++;
      log_line("[VideoSources] OnChangedVideoParams: Handled video quantization change.");
   }

   if ( bDiscardTxBuffers )
   if ( NULL != g_pVideoTxBuffers )
      g_pVideoTxBuffers->discardBuffer();

   log_line("[VideoSources] Processed %d changes: %d applied inline changes + %d transparent changes.", iCountChanges, iCountChangesApplied, iCountChangesTransparent);

   log_line("[VideoSources] Finished processing video settings changes.");
}

void video_sources_set_video_bitrate(u32 uVideoBitrateBPS, int iIPQDelta, const char* szReason)
{
   if ( (NULL == g_pCurrentModel) || (! g_pCurrentModel->hasCamera()) )
      return;

   if ( (NULL == szReason) || (0 == szReason[0]) )
      log_line("[VideoSources] Set video bitrate to %u kbps (reason: N/A), %s", uVideoBitrateBPS/1000, (s_uLastSetVideoBitrateBPS == uVideoBitrateBPS)?"is unchanged":"is changed");
   else
      log_line("[VideoSources] Set video bitrate to %u kbps (reason: %s), %s", uVideoBitrateBPS/1000, szReason, (s_uLastSetVideoBitrateBPS == uVideoBitrateBPS)?"is unchanged":"is changed");
   
   if ( s_uLastSetVideoBitrateBPS == uVideoBitrateBPS )
      return;

   s_uLastSetVideoBitrateBPS = uVideoBitrateBPS;
   s_iLastSetIPQDelta = iIPQDelta;
   if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
      video_source_csi_send_control_message(RASPIVID_COMMAND_ID_VIDEO_BITRATE, uVideoBitrateBPS/100000, 0);
   else if ( g_pCurrentModel->isActiveCameraOpenIPC() )
   {
      if ( (uVideoBitrateBPS != hardware_camera_maj_get_current_bitrate()) &&
           (hardware_camera_maj_get_current_qpdelta() != iIPQDelta) )
         hardware_camera_maj_set_bitrate_and_qpdelta( uVideoBitrateBPS, iIPQDelta);
      else if ( uVideoBitrateBPS != hardware_camera_maj_get_current_bitrate() )
         hardware_camera_maj_set_bitrate(uVideoBitrateBPS); 
      else if ( hardware_camera_maj_get_current_qpdelta() != iIPQDelta )
         hardware_camera_maj_set_qpdelta(iIPQDelta);
   }
}

u32 video_sources_get_last_set_video_bitrate()
{
   return s_uLastSetVideoBitrateBPS;
}

void video_sources_set_keyframe(int iKeyframeMs)
{
   if ( (NULL == g_pCurrentModel) || (iKeyframeMs == s_iLastSetVideoKeyframeMs) )
      return;

   s_iLastSetVideoKeyframeMs = iKeyframeMs;

   log_line("[VideoSources] Set KF to: %d ms", s_iLastSetVideoKeyframeMs);

   // Send the actual keyframe change to video source/capture

   if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
   {
      int iCurrentFPS = g_pCurrentModel->video_params.iVideoFPS;
      int iKeyFrame_FrameCountValue = (iCurrentFPS * s_iLastSetVideoKeyframeMs) / 1000; 
      video_source_csi_send_control_message(RASPIVID_COMMAND_ID_KEYFRAME, (u16)iKeyFrame_FrameCountValue, 0);
   }
   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
      hardware_camera_maj_set_keyframe(s_iLastSetVideoKeyframeMs);                
}

int video_sources_get_last_set_keyframe()
{
   return s_iLastSetVideoKeyframeMs;
}

void video_sources_set_temporary_image_saturation_off(bool bTurnOff)
{
   if ( !(g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_ENABLE_FOCUS_MODE_BW) )
      return;
   if ( bTurnOff )
      log_line("[VideoSources] Set temporary image saturation to off");
   else
      log_line("[VideoSources] Removed temporary image saturation off");

   int iCurrentCamera = g_pCurrentModel->iCurrentCamera;
   int iCurrentCamProfile = g_pCurrentModel->camera_params[iCurrentCamera].iCurrentProfile;

   int iSaturation = g_pCurrentModel->camera_params[iCurrentCamera].profiles[iCurrentCamProfile].saturation;
   if ( bTurnOff )
      iSaturation = 0;
   log_line("[VideoSources] Set temporary image saturation to %d", iSaturation);
   if ( g_pCurrentModel->isRunningOnOpenIPCHardware() && (! hardware_board_is_goke(g_pCurrentModel->hwCapabilities.uBoardType)) )
      hardware_camera_maj_set_saturation(iSaturation);
   else if ( g_pCurrentModel->isActiveCameraCSICompatible() )
      video_source_csi_send_control_message( RASPIVID_COMMAND_ID_SATURATION, iSaturation, 0 );
}

bool video_sources_periodic_health_checks()
{
   if ( ! g_pCurrentModel->hasCamera() )
      return false;
   if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
      return video_source_csi_periodic_health_checks();
   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
      return video_source_majestic_periodic_health_checks();

   return false;
}