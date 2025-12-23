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
#include "../base/models_list.h"
#include "../base/utils.h"
#include "../base/shared_mem_controller_only.h"
#include "../common/string_utils.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiopacketsqueue.h"
#include <math.h>

#include "adaptive_video.h"
#include "test_link_params.h"
#include "shared_vars.h"
#include "shared_vars_state.h"
#include "timers.h"
#include "ruby_rt_station.h"

extern t_packet_queue s_QueueRadioPacketsHighPrio;

u32 s_uLastTimeAdaptiveVideoWasPaused = 0;
u32 s_uTimePauseAdaptiveVideoUntil = 0;
bool s_bAdaptiveIsInTestMode = false;
bool s_bAdaptiveTestModeDirectionUp = false;
u32 s_uTimeLastTestModeAdaptiveLevelUpdate = 0;
u32 s_uTimeLastAdaptiveVideoPeriodicChecks = 0;

static u32 s_uAdaptiveMetric_TimeToLookBackMs = 0;
static int s_iAdaptiveMetric_IntervalsToLookBack = 0;
static int s_iAdaptiveMetric_IntervalsWithVideoBlocks = 0;
static int s_iAdaptiveMetric_IntervalsWithAnyRadioData = 0;

type_adaptive_metrics s_AdaptiveMetrics;
// EC
static int s_iAdaptiveMetric_IntervalsWithECHits = 0;
static int s_iAdaptiveMetric_IntervalsWithMaxECHits = 0;
static int s_iAdaptiveMetric_PercentageBlocksWithECHits = 0;
static int s_iAdaptiveMetric_PercentageBlocksWithMaxECHits = 0;


static int s_iAdaptiveMetric_IntervalsWithBadPackets = 0;
static int s_iAdaptiveMetric_PercentageIntervalsWithBadPackets =0;

static int s_iAdaptiveMetric_TotalRequestedRetr = 0;
static int s_iAdaptiveMetric_TotalOutputVideoBlocks = 0;
static int s_iAdaptiveMetric_TotalBadVideoBlocksIntervals = 0;
static int s_iAdaptiveMetric_MinimSNRThresh = 1000;
static int s_iAdaptiveMetric_MinimRSSIThresh = 1000;

void _adaptive_video_log_DRlinks(Model* pModel, type_global_state_vehicle_runtime_info* pRuntimeInfo, char* szOutput)
{
   if ( (NULL == pModel) || (NULL == pRuntimeInfo) || (NULL == szOutput) )
      return;
   szOutput[0] = 0;
   for( int i=0; i<pModel->radioLinksParams.links_count; i++ )
   {
      if ( 0 != szOutput[0] )
         strcat(szOutput, ", ");
      strcat(szOutput, str_format_datarate_inline(pRuntimeInfo->iCurrentDataratesForLinks[i]));
   }
}

void _adaptive_video_log_state(Model* pModel, type_global_state_vehicle_runtime_info* pRuntimeInfo, const char* szReason)
{
   if ( (NULL == pModel) || (NULL == pRuntimeInfo) || (NULL == szReason) )
   {
      log_softerror_and_alarm("[AdaptiveVideo] Tried to log invalid params.");
      return;
   }

   u32 uProfileFlags = pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uProfileFlags;
   u32 uMaxDRBoost = (uProfileFlags & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK) >> VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT;
   int iDataRateForCurrentVideoBitrate = pModel->getRadioDataRateForVideoBitrate(pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS, 0, true);
   char szDR[128];
   _adaptive_video_log_DRlinks(pModel, pRuntimeInfo, szDR);

   log_line("[AdaptiveVideo] State at %s: Active: %s, Level: %d, Current links DR: %s, DR boost: %d/%d, EC Scheme %d/%d: Adaptive video bitrate: %.2f / %.2f Mbps (fits in datarate: %s), Last request id to vehicle: %u, was %u ms ago",
      szReason, pRuntimeInfo->bIsAdaptiveVideoActive?"yes":"no",
      pRuntimeInfo->iAdaptiveLevelNow, szDR,
      pRuntimeInfo->uCurrentDRBoost, uMaxDRBoost,
      pRuntimeInfo->uCurrentAdaptiveVideoECScheme >> 8, pRuntimeInfo->uCurrentAdaptiveVideoECScheme & 0xFF,
      (float)pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS/1000.0/1000.0,
      (float)pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS/1000.0/1000.0,
      str_format_datarate_inline(iDataRateForCurrentVideoBitrate),
      pRuntimeInfo->uAdaptiveVideoRequestId, g_TimeNow - pRuntimeInfo->uLastTimeSentAdaptiveVideoRequest);
}

void adaptive_video_pause(u32 uMilisec)
{
   if ( 0 == uMilisec )
   {
      log_line("[AdaptiveVideo] Global resumed.");
      if ( 0 != s_uTimePauseAdaptiveVideoUntil )
      {
         adaptive_video_reset_time_for_vehicle(0);
         send_adaptive_video_paused_to_central(0, false);
      }
      s_uTimePauseAdaptiveVideoUntil = 0;
   }
   else
   {
      if ( uMilisec > 40000 )
         uMilisec = 40000;

      u32 uTimeToPauseUntil = g_TimeNow + uMilisec;
      if ( uTimeToPauseUntil > s_uTimePauseAdaptiveVideoUntil )
      {
         if ( 0 == s_uTimePauseAdaptiveVideoUntil )
            send_adaptive_video_paused_to_central(0, true);
         s_uTimePauseAdaptiveVideoUntil = uTimeToPauseUntil;
         s_uLastTimeAdaptiveVideoWasPaused = s_uTimePauseAdaptiveVideoUntil;
         log_line("[AdaptiveVideo] Global paused for %u milisec from now", s_uTimePauseAdaptiveVideoUntil - g_TimeNow);
      }
   }
}

bool adaptive_video_is_paused()
{
   return (s_uTimePauseAdaptiveVideoUntil != 0)?true: false;
}

u32 adaptive_video_get_last_paused_time()
{
   return s_uLastTimeAdaptiveVideoWasPaused;
}

void adaptive_video_reset_time_for_vehicle(u32 uVehicleId)
{
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( (0 != uVehicleId) && (uVehicleId == g_State.vehiclesRuntimeInfo[i].uVehicleId) )
      {
         g_State.vehiclesRuntimeInfo[i].uLastTimeSentAdaptiveVideoRequest = g_TimeNow;
         g_State.vehiclesRuntimeInfo[i].uTimeStartCountingMetricAreOkToSwithHigher = 0;
         log_line("[AdaptiveVideo] Did reset time for VID %u", uVehicleId);
         return;
      }
      if ( (0 == uVehicleId) && (0 != g_State.vehiclesRuntimeInfo[i].uVehicleId) )
      {
         g_State.vehiclesRuntimeInfo[i].uLastTimeSentAdaptiveVideoRequest = g_TimeNow;
         g_State.vehiclesRuntimeInfo[i].uTimeStartCountingMetricAreOkToSwithHigher = 0;
         log_line("[AdaptiveVideo] Did reset time for VID %u", g_State.vehiclesRuntimeInfo[i].uVehicleId);
      }
   }
}


void adaptive_video_init()
{
   log_line("[AdaptiveVideo] Init");
}

void _adaptive_video_reset_kf_state(Model* pModel, type_global_state_vehicle_runtime_info* pRuntimeInfo)
{
   if ( (NULL == pModel) || (NULL == pRuntimeInfo) )
      return;
   if ( pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME )
   {
      pRuntimeInfo->iCurrentAdaptiveVideoKeyFrameMsTarget = DEFAULT_VIDEO_AUTO_INITIAL_KEYFRAME_INTERVAL;
      #if defined (HW_PLATFORM_RASPBERRY)
      if ( pModel->isRunningOnOpenIPCHardware() )
         pRuntimeInfo->iCurrentAdaptiveVideoKeyFrameMsTarget = DEFAULT_VIDEO_AUTO_INITIAL_KEYFRAME_INTERVAL_FALLBACK;
      #endif
   }
   else
      pRuntimeInfo->iCurrentAdaptiveVideoKeyFrameMsTarget = pModel->getInitialKeyframeIntervalMs(pModel->video_params.iCurrentVideoProfile);
   pRuntimeInfo->iPendingKeyFrameMsToSet = pRuntimeInfo->iCurrentAdaptiveVideoKeyFrameMsTarget;
   log_line("[AdaptiveVideo] Did reset %s keyframe state to: request %u ms keyframe from vehicle", 
      (pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME)?"adaptive":"fixed", pRuntimeInfo->iPendingKeyFrameMsToSet);
}

void adaptive_video_reset_state(u32 uVehicleId)
{
   log_line("[AdaptiveVideo] Reseting state for VID: %u", uVehicleId);

   int iRuntimeIndex = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( uVehicleId == g_State.vehiclesRuntimeInfo[i].uVehicleId )
      {
         iRuntimeIndex = i;
         break;
      }
   }
   if ( iRuntimeIndex == -1 )
   {
      log_softerror_and_alarm("[AdaptiveVideo] Tried to reset state for VID %u which is not present in runtime info list.", uVehicleId);
      return;
   }

   shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, uVehicleId);
   if ( NULL != pSMVideoStreamInfo )
      pSMVideoStreamInfo->iAdaptiveVideoLevelNow = 0;

   g_State.vehiclesRuntimeInfo[iRuntimeIndex].bDidFirstTimeAdaptiveHandshake = false;
   g_State.vehiclesRuntimeInfo[iRuntimeIndex].uAdaptiveVideoActivationTime = g_TimeNow;
   g_State.vehiclesRuntimeInfo[iRuntimeIndex].uAdaptiveVideoRequestId = 0;
   g_State.vehiclesRuntimeInfo[iRuntimeIndex].uAdaptiveVideoAckId = 0;
   g_State.vehiclesRuntimeInfo[iRuntimeIndex].uLastTimeSentAdaptiveVideoRequest = 0;
   g_State.vehiclesRuntimeInfo[iRuntimeIndex].uTimeStartCountingMetricAreOkToSwithHigher = 0;
   g_State.vehiclesRuntimeInfo[iRuntimeIndex].uLastTimeRecvAdaptiveVideoAck = 0;

   g_State.vehiclesRuntimeInfo[iRuntimeIndex].uCurrentAdaptiveVideoTargetVideoBitrateBPS = 0;
   g_State.vehiclesRuntimeInfo[iRuntimeIndex].uCurrentAdaptiveVideoECScheme = 0xFFFF;
   g_State.vehiclesRuntimeInfo[iRuntimeIndex].uCurrentDRBoost = 0xFF;
   Model* pModel = findModelWithId(g_State.vehiclesRuntimeInfo[iRuntimeIndex].uVehicleId, 47);
   if ( NULL != pModel )
   {
      g_State.vehiclesRuntimeInfo[iRuntimeIndex].uCurrentAdaptiveVideoTargetVideoBitrateBPS = pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS;

      for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
      {
         g_State.vehiclesRuntimeInfo[iRuntimeIndex].iCurrentDataratesForLinks[k] = 0;
         if ( k < pModel->radioLinksParams.links_count )
         {
            g_State.vehiclesRuntimeInfo[iRuntimeIndex].iCurrentDataratesForLinks[k] = pModel->getRadioDataRateForVideoBitrate(g_State.vehiclesRuntimeInfo[iRuntimeIndex].uCurrentAdaptiveVideoTargetVideoBitrateBPS, k, true);
         }
      }
      if ( pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAG_USE_HIGHER_DATARATE )
         g_State.vehiclesRuntimeInfo[iRuntimeIndex].uCurrentDRBoost = (pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK) >> VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT;

      _adaptive_video_reset_kf_state(pModel, &(g_State.vehiclesRuntimeInfo[iRuntimeIndex]));
      shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, pModel->uVehicleId);
      if ( NULL != pSMVideoStreamInfo )
         pSMVideoStreamInfo->bIsOnLowestAdaptiveLevel = false;
   }

   g_State.vehiclesRuntimeInfo[iRuntimeIndex].uPendingVideoBitrateToSet = g_State.vehiclesRuntimeInfo[iRuntimeIndex].uCurrentAdaptiveVideoTargetVideoBitrateBPS;
   g_State.vehiclesRuntimeInfo[iRuntimeIndex].uPendingECSchemeToSet = g_State.vehiclesRuntimeInfo[iRuntimeIndex].uCurrentAdaptiveVideoECScheme;
   g_State.vehiclesRuntimeInfo[iRuntimeIndex].uPendingDRBoostToSet = g_State.vehiclesRuntimeInfo[iRuntimeIndex].uCurrentDRBoost;
   g_State.vehiclesRuntimeInfo[iRuntimeIndex].iPendingKeyFrameMsToSet = g_State.vehiclesRuntimeInfo[iRuntimeIndex].iCurrentAdaptiveVideoKeyFrameMsTarget;
   g_State.vehiclesRuntimeInfo[iRuntimeIndex].iAdaptiveLevelNow = 0;
   g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsOnLowestAdaptiveLevel = false;

   char szDR[128];
   _adaptive_video_log_DRlinks(pModel, &(g_State.vehiclesRuntimeInfo[iRuntimeIndex]), szDR);
   log_line("[AdaptiveVideo] Default reseted state for VID %u: video bitrate: %.2f Mbps, EC scheme: %d/%d, Current DR for links: %s, DR boost: %d, pending KF: %d ms",
      uVehicleId, (float)g_State.vehiclesRuntimeInfo[iRuntimeIndex].uCurrentAdaptiveVideoTargetVideoBitrateBPS/1000.0/1000.0,
      (g_State.vehiclesRuntimeInfo[iRuntimeIndex].uCurrentAdaptiveVideoECScheme >> 8) & 0xFF,
      g_State.vehiclesRuntimeInfo[iRuntimeIndex].uCurrentAdaptiveVideoECScheme & 0xFF,
      szDR, g_State.vehiclesRuntimeInfo[iRuntimeIndex].uCurrentDRBoost,
      g_State.vehiclesRuntimeInfo[iRuntimeIndex].iPendingKeyFrameMsToSet);
   log_line("[AdaptiveVideo] Did reset state for VID: %u, currently adaptive is active for it? %s, last adaptive request sent to vehicle: %u ms ago", uVehicleId, g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsAdaptiveVideoActive?"yes":"no", g_TimeNow - g_State.vehiclesRuntimeInfo[iRuntimeIndex].uLastTimeSentAdaptiveVideoRequest);

   if ( NULL == pModel )
   {
      log_softerror_and_alarm("[AdaptiveVideo] Did reset adaptive state for VID %u, but there is no matching model for it on the controller.", uVehicleId);
      return;
   }

   int iAdaptiveStrength = pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].iAdaptiveAdjustmentStrength;
   u32 uAdaptiveWeights = pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uAdaptiveWeights;
   compute_adaptive_metrics(&s_AdaptiveMetrics, iAdaptiveStrength, uAdaptiveWeights);
   log_adaptive_metrics(pModel, &s_AdaptiveMetrics, iAdaptiveStrength, uAdaptiveWeights);

   bool bOnlyMediumAdaptive = false;
   if ( (pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO )
      bOnlyMediumAdaptive = true;

   if ( bOnlyMediumAdaptive )
      log_line("[AdaptiveVideo] VID %u is using only medium adaptive video, adaptive strength: %d", uVehicleId, iAdaptiveStrength);
   else
      log_line("[AdaptiveVideo] VID %u is using full adaptive video, adaptive strength: %d", uVehicleId, iAdaptiveStrength);


   for( int i=0; i<MAX_MCS_INDEX; i++)
   {
      u32 uMaxVideoBitrateForLink = pModel->getMaxVideoBitrateForRadioDatarate(-1-i, 0);
      log_line("[AdaptiveVideo] Max video bitrate for this vehicle for MCS-%d is: %.1f", i, (float)uMaxVideoBitrateForLink/1000.0/1000.0);
   }
}

void adaptive_video_on_new_vehicle(int iRuntimeIndex)
{
  if ( (iRuntimeIndex < 0) || (iRuntimeIndex >= MAX_CONCURENT_VEHICLES) )
     return;

  Model* pModel = findModelWithId(g_State.vehiclesRuntimeInfo[iRuntimeIndex].uVehicleId, 41);
  if ( (NULL == pModel) || (! pModel->hasCamera()) )
     return;

  g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsAdaptiveVideoActive = false;
  log_line("[AdaptiveVideo] Set adaptive as inactive for VID %u (on new vehicle)", g_State.vehiclesRuntimeInfo[iRuntimeIndex].uVehicleId);
  adaptive_video_reset_state(pModel->uVehicleId);
  log_line("[AdaptiveVideo] On new vehicle ID: %u, did reset adaptive state for it.", g_State.vehiclesRuntimeInfo[iRuntimeIndex].uVehicleId);
}

void _adaptive_video_init_first_handshake(int iRuntimeIndex)
{
   if ( (iRuntimeIndex < 0) || (iRuntimeIndex >= MAX_CONCURENT_VEHICLES) )
     return;

   Model* pModel = findModelWithId(g_State.vehiclesRuntimeInfo[iRuntimeIndex].uVehicleId, 41);
   if ( (NULL == pModel) || (! pModel->hasCamera()) )
      return;

   if ( g_State.vehiclesRuntimeInfo[iRuntimeIndex].bDidFirstTimeAdaptiveHandshake )
      return;

   if ( 0 != g_State.vehiclesRuntimeInfo[iRuntimeIndex].iCurrentAdaptiveVideoKeyFrameMsTarget )
      g_State.vehiclesRuntimeInfo[iRuntimeIndex].iPendingKeyFrameMsToSet = g_State.vehiclesRuntimeInfo[iRuntimeIndex].iCurrentAdaptiveVideoKeyFrameMsTarget;

   if ( 0 != g_State.vehiclesRuntimeInfo[iRuntimeIndex].uCurrentAdaptiveVideoTargetVideoBitrateBPS )
      g_State.vehiclesRuntimeInfo[iRuntimeIndex].uPendingVideoBitrateToSet = g_State.vehiclesRuntimeInfo[iRuntimeIndex].uCurrentAdaptiveVideoTargetVideoBitrateBPS;
   
   if ( 0 != g_State.vehiclesRuntimeInfo[iRuntimeIndex].uCurrentAdaptiveVideoECScheme )
      g_State.vehiclesRuntimeInfo[iRuntimeIndex].uPendingECSchemeToSet = g_State.vehiclesRuntimeInfo[iRuntimeIndex].uCurrentAdaptiveVideoECScheme;
   else
      g_State.vehiclesRuntimeInfo[iRuntimeIndex].uPendingECSchemeToSet = 0xFFFF;

   g_State.vehiclesRuntimeInfo[iRuntimeIndex].uPendingDRBoostToSet = g_State.vehiclesRuntimeInfo[iRuntimeIndex].uCurrentDRBoost;
   
   g_State.vehiclesRuntimeInfo[iRuntimeIndex].uAdaptiveVideoRequestId++;

   log_line("[AdaptiveVideo] Initial handshake for VID %u: video bitrate: %.2f, EC scheme: %d/%d, DR boost: %d, KF: %d ms",
       g_State.vehiclesRuntimeInfo[iRuntimeIndex].uVehicleId,
       (float)g_State.vehiclesRuntimeInfo[iRuntimeIndex].uPendingVideoBitrateToSet/1000.0/1000.0,
       (g_State.vehiclesRuntimeInfo[iRuntimeIndex].uPendingECSchemeToSet>>8) & 0xFF,
       g_State.vehiclesRuntimeInfo[iRuntimeIndex].uPendingECSchemeToSet & 0xFF,
       g_State.vehiclesRuntimeInfo[iRuntimeIndex].uPendingDRBoostToSet,
       g_State.vehiclesRuntimeInfo[iRuntimeIndex].iPendingKeyFrameMsToSet);
}

void adaptive_video_on_vehicle_video_params_changed(u32 uVehicleId, video_parameters_t* pOldVideoParams, type_video_link_profile* pOldVideoProfiles)
{
   log_line("[AdaptiveVideo] Video params where updated for VID %u", uVehicleId);
   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(uVehicleId);
   Model* pModel = findModelWithId(uVehicleId, 43);
   if ( NULL == pRuntimeInfo )
   {
      log_softerror_and_alarm("[AdaptiveVideo] Can't find runtime info for VID %u", uVehicleId);
      return;
   }
   if ( NULL == pModel )
   {
      log_softerror_and_alarm("[AdaptiveVideo] Can't find model for VID %u", uVehicleId);
      return;
   }

   int iAdaptiveStrength = pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].iAdaptiveAdjustmentStrength;
   u32 uAdaptiveWeights = pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uAdaptiveWeights;
   compute_adaptive_metrics(&s_AdaptiveMetrics, iAdaptiveStrength, uAdaptiveWeights);
   log_adaptive_metrics(pModel, &s_AdaptiveMetrics, iAdaptiveStrength, uAdaptiveWeights);

   ProcessorRxVideo* pProcessorRxVideo = ProcessorRxVideo::getVideoProcessorForVehicleId(uVehicleId, 0);
   if ( NULL != pProcessorRxVideo )
      pProcessorRxVideo->setMustParseStream(true);

   if ( (NULL != pOldVideoParams) && (NULL != pOldVideoProfiles) )
   {
      if ( (pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].iKeyframeMS != pOldVideoProfiles[pOldVideoParams->iCurrentVideoProfile].iKeyframeMS) ||
           ((pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME) != (pOldVideoProfiles[pOldVideoParams->iCurrentVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME)) )
      {
         _adaptive_video_reset_kf_state(pModel, pRuntimeInfo);
      }
   }
}

void adaptive_video_enable_test_mode(bool bEnableTestMode)
{
   s_bAdaptiveIsInTestMode = bEnableTestMode;
   s_uTimeLastTestModeAdaptiveLevelUpdate = g_TimeNow;
   s_bAdaptiveTestModeDirectionUp = false;
   log_line("[AdaptiveVideo] Test mode was set to: %s", s_bAdaptiveIsInTestMode?"On":"Off");
   log_line("[AdaptiveVideo] Is global paused right now? %s", (s_uTimePauseAdaptiveVideoUntil==0)?"No":"Yes");
   
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( (g_State.vehiclesRuntimeInfo[i].uVehicleId == 0) || (g_State.vehiclesRuntimeInfo[i].uVehicleId == MAX_U32) )
         continue;
      if ( ! g_State.vehiclesRuntimeInfo[i].bIsPairingDone )
         continue;
      g_State.vehiclesRuntimeInfo[i].uAdaptiveVideoRequestId++;

      if ( ! bEnableTestMode )
         adaptive_video_reset_state(g_State.vehiclesRuntimeInfo[i].uVehicleId);
   }
}

void _adaptive_video_send_adaptive_message_to_vehicle(u32 uVehicleId)
{
   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(uVehicleId);
   if ( NULL == pRuntimeInfo )
      return;
   Model* pModel = findModelWithId(uVehicleId, 29);
   if ( NULL == pModel )
      return;

   static u32 s_uDeltaForRequestAdaptive = 10;
   if ( pRuntimeInfo->uLastTimeSentAdaptiveVideoRequest > g_TimeNow-s_uDeltaForRequestAdaptive )
      return;

   if ( g_TimeNow - pRuntimeInfo->uLastTimeSentAdaptiveVideoRequest < 100 )
   if ( s_uDeltaForRequestAdaptive < 100 )
      s_uDeltaForRequestAdaptive += 10;
   if ( g_TimeNow - pRuntimeInfo->uLastTimeSentAdaptiveVideoRequest > 500 )
      s_uDeltaForRequestAdaptive = 10;
   
   if ( g_TimeNow > g_TimeStart+5000 )
   if ( pRuntimeInfo->uLastTimeRecvDataFromVehicle < g_TimeNow-1000 )
   {
      log_line("[AdaptiveVideo] Skip sending adaptive video message to vehicle %u (req id: %u) as the link is lost.",
         uVehicleId, pRuntimeInfo->uAdaptiveVideoRequestId);
      return;
   }

   pRuntimeInfo->uLastTimeSentAdaptiveVideoRequest = g_TimeNow;
   // To fix may 2025
   //g_SMControllerRTInfo.uFlagsAdaptiveVideo[g_SMControllerRTInfo.iCurrentIndex] |= pRuntimeInfo->uPendingVideoProfileToSetRequestedBy;

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_VIDEO, PACKET_TYPE_VIDEO_ADAPTIVE_VIDEO_PARAMS, STREAM_ID_DATA);
   PH.packet_flags |= PACKET_FLAGS_BIT_HIGH_PRIORITY;
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = uVehicleId;
   PH.total_length = sizeof(t_packet_header) + 2*sizeof(u32) + 3*sizeof(u8) + 2*sizeof(int) + sizeof(u16);

   u8 uFlags = 0;

   if ( 0 != pRuntimeInfo->iPendingKeyFrameMsToSet )
      uFlags |= FLAG_ADAPTIVE_VIDEO_KEYFRAME;

   if ( 0 != pRuntimeInfo->uPendingVideoBitrateToSet )
      uFlags |= FLAG_ADAPTIVE_VIDEO_BITRATE;

   if ( 0 != pRuntimeInfo->uPendingECSchemeToSet )
      uFlags |= FLAG_ADAPTIVE_VIDEO_EC;

   if ( 0xFF != pRuntimeInfo->uPendingDRBoostToSet )
      uFlags |= FLAG_ADAPTIVE_VIDEO_DR_BOOST;

   if ( s_bAdaptiveIsInTestMode )
      uFlags |= FLAG_ADAPTIVE_IN_TEST_MODE;

   u8 uVideoStreamIndex = 0;
   u32 uVideoBitrate = pRuntimeInfo->uPendingVideoBitrateToSet;
   u16 uEC = pRuntimeInfo->uPendingECSchemeToSet;
   int iRadioDatarate = 0;
   int iKeyframeMS = pRuntimeInfo->iPendingKeyFrameMsToSet;
   u8 uDRBoost = pRuntimeInfo->uPendingDRBoostToSet;

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   u8* pData = &packet[0];
   memcpy(pData, (u8*)&PH, sizeof(t_packet_header));
   pData += sizeof(t_packet_header);
   memcpy(pData, (u8*)&(pRuntimeInfo->uAdaptiveVideoRequestId), sizeof(u32));
   pData += sizeof(u32);
   memcpy(pData, &uFlags, sizeof(u8));
   pData += sizeof(u8);
   memcpy(pData, &uVideoStreamIndex, sizeof(u8));
   pData += sizeof(u8);
   memcpy(pData, (u8*)&uVideoBitrate, sizeof(u32));
   pData += sizeof(u32);
   memcpy(pData, (u8*)&uEC, sizeof(u16));
   pData += sizeof(u16);
   memcpy(pData, (u8*)&iRadioDatarate, sizeof(int));
   pData += sizeof(int);
   memcpy(pData, (u8*)&iKeyframeMS, sizeof(int));
   pData += sizeof(int);
   memcpy(pData, (u8*)&uDRBoost, sizeof(u8));
   pData += sizeof(u8);
   
   packets_queue_inject_packet_first_mark_time(&s_QueueRadioPacketsHighPrio, packet);

   char szDR[128];
   _adaptive_video_log_DRlinks(pModel, pRuntimeInfo, szDR);
   if ( 0 != uVideoBitrate )
   {
      int iDRToFitVideo = pModel->getRadioDataRateForVideoBitrate(uVideoBitrate, 0, true);
      log_line("[AdaptiveVideo] Sent adaptive video message to vehicle %u (req id: %u), %s: %s, Video bitrate: %.2f Mbps (of %.2f Mbps) (fits in datarate %s, max video bitrate for this radio rate is: %.2f Mbps, max radio data for this rate: %.2f Mbps), EC: %d/%d, Current DR for links: %s, DR boost: %d",
         uVehicleId, pRuntimeInfo->uAdaptiveVideoRequestId,
         pRuntimeInfo->bDidFirstTimeAdaptiveHandshake?"flags":" (first time handshake) flags",
         str_format_adaptive_video_flags(uFlags),
         (float)uVideoBitrate/1000.0/1000.0, (float)pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS/1000.0/1000.0,
         str_format_datarate_inline(iDRToFitVideo),
         (float)pModel->getMaxVideoBitrateForRadioDatarate(iDRToFitVideo, 0)/1000.0/1000.0,
         (float)getRealDataRateFromRadioDataRate(iDRToFitVideo, pModel->radioLinksParams.link_radio_flags[0], 1)/1000.0/1000.0,
         (uEC >> 8) & 0xFF, uEC & 0xFF,
         szDR, uDRBoost);
   }
   else
      log_line("[AdaptiveVideo] Sent adaptive video message to vehicle %u (req id: %u),%s flags: %s, (curent target video bitrate: %.2f Mbps), EC: %d/%d, Current DR for links: %s, DR boost: %d",
         uVehicleId, pRuntimeInfo->uAdaptiveVideoRequestId,
         pRuntimeInfo->bDidFirstTimeAdaptiveHandshake?" ":" (first time handshake)",
         str_format_adaptive_video_flags(uFlags),
         (float)pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS/1000.0/1000.0,
         (uEC >> 8) & 0xFF, uEC & 0xFF,
         szDR, uDRBoost);
}

void adaptive_video_received_vehicle_msg_ack(u32 uRequestId, u32 uVehicleId, int iInterfaceIndex)
{
   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(uVehicleId);
   if ( NULL == pRuntimeInfo )
      return;
   log_line("[AdaptiveVideo] Received msg ack from VID %u, req id: %u",
      uVehicleId, uRequestId);

   if ( ! pRuntimeInfo->bDidFirstTimeAdaptiveHandshake )
   {
      pRuntimeInfo->bDidFirstTimeAdaptiveHandshake = true;
      log_line("[AdaptiveVideo] Finished doing first time handshake.");
   }
   
   g_SMControllerRTInfo.uFlagsAdaptiveVideo[g_SMControllerRTInfo.iCurrentIndex] |= CTRL_RT_INFO_FLAG_RECV_ACK;

   pRuntimeInfo->uAdaptiveVideoAckId = uRequestId;
   if ( pRuntimeInfo->uAdaptiveVideoRequestId == uRequestId )
   {
      if ( 0 != pRuntimeInfo->iPendingKeyFrameMsToSet )
      {
         ProcessorRxVideo* pProcessorRxVideo = ProcessorRxVideo::getVideoProcessorForVehicleId(pRuntimeInfo->uVehicleId, 0);
         if ( NULL != pProcessorRxVideo )
            pProcessorRxVideo->setMustParseStream(true);
      }
      pRuntimeInfo->uLastTimeRecvAdaptiveVideoAck = g_TimeNow;
      pRuntimeInfo->uPendingVideoBitrateToSet = 0;
      pRuntimeInfo->uPendingECSchemeToSet = 0;
      pRuntimeInfo->uPendingDRBoostToSet = 0xFF;
      pRuntimeInfo->iPendingKeyFrameMsToSet = 0;
      u32 uDeltaTime = pRuntimeInfo->uLastTimeRecvAdaptiveVideoAck - pRuntimeInfo->uLastTimeSentAdaptiveVideoRequest;
      controller_rt_info_update_ack_rt_time(&g_SMControllerRTInfo, uVehicleId, g_SM_RadioStats.radio_interfaces[iInterfaceIndex].assignedLocalRadioLinkId, uDeltaTime, 0x02);
   }
}

void _adaptive_video_test_mode_update(type_global_state_vehicle_runtime_info* pRuntimeInfo)
{
   static u32 s_uTimeLastCheckAdaptiveTestMode = 0;
   static u32 s_uCounterAdaptiveTest = 0;

   if ( g_TimeNow < s_uTimeLastCheckAdaptiveTestMode + 3000 )
      return;

   s_uTimeLastCheckAdaptiveTestMode = g_TimeNow;
   s_uCounterAdaptiveTest++;

   if ( (s_uCounterAdaptiveTest % 10) < 5 )
      pRuntimeInfo->iPendingKeyFrameMsToSet = 500 + (s_uCounterAdaptiveTest % 10)*500;
   else
      pRuntimeInfo->iPendingKeyFrameMsToSet = 500 + (9-(s_uCounterAdaptiveTest % 10))*500;
   pRuntimeInfo->uAdaptiveVideoRequestId++;
   log_line("[AdaptiveVideo] Test mode update pending KF to %d ms, adaptive request id is now: %d", pRuntimeInfo->iPendingKeyFrameMsToSet, pRuntimeInfo->uAdaptiveVideoRequestId);
}

void _adaptive_video_compute_metrics(Model* pModel, type_global_state_vehicle_runtime_info* pRuntimeInfo)
{
   // Adaptive adjustment strength is in: pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].iAdaptiveAdjustmentStrength
   // 1: lowest (slower) adjustment strength;
   // 10: highest (fastest) adjustment strength;
   //

   int iAdaptiveStrength = pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].iAdaptiveAdjustmentStrength;
   u32 uAdaptiveWeights = pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uAdaptiveWeights;

   controller_runtime_info_vehicle* pRTInfoVehicle = controller_rt_info_get_vehicle_info(&g_SMControllerRTInfo, pModel->uVehicleId);
   
   s_uAdaptiveMetric_TimeToLookBackMs = 500 + 100 * (10-iAdaptiveStrength);
   
   if ( s_AdaptiveMetrics.uTimeToLookBackForRxLost > s_uAdaptiveMetric_TimeToLookBackMs )
      s_uAdaptiveMetric_TimeToLookBackMs = s_AdaptiveMetrics.uTimeToLookBackForRxLost;
   if ( s_AdaptiveMetrics.uTimeToLookBackForRetr > s_uAdaptiveMetric_TimeToLookBackMs )
      s_uAdaptiveMetric_TimeToLookBackMs = s_AdaptiveMetrics.uTimeToLookBackForRetr;
   if ( s_AdaptiveMetrics.uTimeToLookBackForECUsed > s_uAdaptiveMetric_TimeToLookBackMs )
      s_uAdaptiveMetric_TimeToLookBackMs = s_AdaptiveMetrics.uTimeToLookBackForECUsed;
   if ( s_AdaptiveMetrics.uTimeToLookBackForECMax > s_uAdaptiveMetric_TimeToLookBackMs )
      s_uAdaptiveMetric_TimeToLookBackMs = s_AdaptiveMetrics.uTimeToLookBackForECMax;

   if ( g_TimeNow - pRuntimeInfo->uLastTimeSentAdaptiveVideoRequest < s_uAdaptiveMetric_TimeToLookBackMs )
      s_uAdaptiveMetric_TimeToLookBackMs = g_TimeNow - pRuntimeInfo->uLastTimeSentAdaptiveVideoRequest;

   if ( g_TimeNow - pRuntimeInfo->uLastTimeRecvAdaptiveVideoAck < s_uAdaptiveMetric_TimeToLookBackMs )
      s_uAdaptiveMetric_TimeToLookBackMs = g_TimeNow - pRuntimeInfo->uLastTimeRecvAdaptiveVideoAck;

   s_iAdaptiveMetric_IntervalsToLookBack = s_uAdaptiveMetric_TimeToLookBackMs/g_SMControllerRTInfo.uUpdateIntervalMs;
   s_iAdaptiveMetric_IntervalsToLookBack--;
   if ( s_iAdaptiveMetric_IntervalsToLookBack >= SYSTEM_RT_INFO_INTERVALS )
      s_iAdaptiveMetric_IntervalsToLookBack = SYSTEM_RT_INFO_INTERVALS - 1;
   
   int iRTInfoIndex = g_SMControllerRTInfo.iCurrentIndex;

   compute_adaptive_metrics(&s_AdaptiveMetrics, iAdaptiveStrength, uAdaptiveWeights);

   s_iAdaptiveMetric_IntervalsWithBadPackets = 0;
   s_iAdaptiveMetric_PercentageIntervalsWithBadPackets = 0;
   s_iAdaptiveMetric_IntervalsWithVideoBlocks = 0;
   s_iAdaptiveMetric_IntervalsWithAnyRadioData = 0;
   s_iAdaptiveMetric_IntervalsWithECHits = 0;
   s_iAdaptiveMetric_IntervalsWithMaxECHits = 0;

   s_iAdaptiveMetric_PercentageBlocksWithECHits = 0;
   s_iAdaptiveMetric_PercentageBlocksWithMaxECHits = 0;
   s_iAdaptiveMetric_TotalOutputVideoBlocks = 0;
   s_iAdaptiveMetric_TotalRequestedRetr = 0;
   s_iAdaptiveMetric_TotalBadVideoBlocksIntervals = 0;
   s_iAdaptiveMetric_MinimSNRThresh = 1000;
   s_iAdaptiveMetric_MinimRSSIThresh = 1000;

   for( int i=0; i<s_iAdaptiveMetric_IntervalsToLookBack; i++ )
   {
       if ( g_SMControllerRTInfo.uSliceStartTimeMs[iRTInfoIndex] < g_TimeNow - s_uAdaptiveMetric_TimeToLookBackMs )
       {
          s_iAdaptiveMetric_IntervalsToLookBack = i;
          break;
       }
       int iMinLocalBadPackets = 1000;
       int iMinLocalMaxGap = 1000;

       for( int k=0; k<hardware_get_radio_interfaces_count(); k++ )
       {
          if ( g_SMControllerRTInfo.uRxMissingPackets[iRTInfoIndex][k] > 0 )
          if ( g_SMControllerRTInfo.uRxMissingPackets[iRTInfoIndex][k] < iMinLocalBadPackets )
             iMinLocalBadPackets = g_SMControllerRTInfo.uRxMissingPackets[iRTInfoIndex][k];

          if ( g_SMControllerRTInfo.uRxMissingPacketsMaxGap[iRTInfoIndex][k] > 0 )
          if ( g_SMControllerRTInfo.uRxMissingPacketsMaxGap[iRTInfoIndex][k] < iMinLocalMaxGap )
             iMinLocalMaxGap = g_SMControllerRTInfo.uRxMissingPacketsMaxGap[iRTInfoIndex][k];

       }

       for( int k=0; k<hardware_get_radio_interfaces_count(); k++ )
       {
          if ( g_SMControllerRTInfo.uRxVideoPackets[iRTInfoIndex] ||
               g_SMControllerRTInfo.uRxVideoECPackets[iRTInfoIndex] ||
               g_SMControllerRTInfo.uRxDataPackets[iRTInfoIndex] ||
               g_SMControllerRTInfo.uRxHighPriorityPackets[iRTInfoIndex] ||
               g_SMControllerRTInfo.uRxMissingPackets[iRTInfoIndex][k] )
          {
             s_iAdaptiveMetric_IntervalsWithAnyRadioData++;
             break;
          }
       }

       if ( s_uAdaptiveMetric_TimeToLookBackMs >= s_AdaptiveMetrics.uTimeToLookBackForRxLost )
       if ( iMinLocalBadPackets < 1000 )
          s_iAdaptiveMetric_IntervalsWithBadPackets++;

       if ( s_uAdaptiveMetric_TimeToLookBackMs >= s_AdaptiveMetrics.uTimeToLookBackForRetr )
       if ( NULL != pRTInfoVehicle )
          s_iAdaptiveMetric_TotalRequestedRetr += pRTInfoVehicle->uCountReqRetransmissions[iRTInfoIndex];

       s_iAdaptiveMetric_TotalOutputVideoBlocks += g_SMControllerRTInfo.uOutputedVideoBlocks[iRTInfoIndex];

       if ( g_SMControllerRTInfo.uOutputedVideoBlocks[iRTInfoIndex] )
          s_iAdaptiveMetric_IntervalsWithVideoBlocks++;

       if ( g_SMControllerRTInfo.uOutputedVideoBlocksSkippedBlocks[iRTInfoIndex] > 0 )
          s_iAdaptiveMetric_TotalBadVideoBlocksIntervals++;
       
       if ( s_uAdaptiveMetric_TimeToLookBackMs >= s_AdaptiveMetrics.uTimeToLookBackForECUsed )
       if ( g_SMControllerRTInfo.uOutputedVideoBlocksECUsed[iRTInfoIndex] )
          s_iAdaptiveMetric_IntervalsWithECHits++;
  
       if ( s_uAdaptiveMetric_TimeToLookBackMs >= s_AdaptiveMetrics.uTimeToLookBackForECMax )
       if ( g_SMControllerRTInfo.uOutputedVideoBlocksMaxECUsed[iRTInfoIndex] )
          s_iAdaptiveMetric_IntervalsWithMaxECHits++;

       if ( (s_AdaptiveMetrics.iMinimRSSIThreshold > -1000) || (s_AdaptiveMetrics.iMinimSNRThreshold > -1000) )
       {
          int iMaxLocalSNRThresh = -1000;
          int iMaxLocalRSSIThresh = -1000;
          for( int k=0; k<hardware_get_radio_interfaces_count(); k++ )
          {
             if ( (g_SMControllerRTInfo.radioInterfacesSignalInfoVideo[iRTInfoIndex][k].iSNRThreshMin < 500) &&
                  (g_SMControllerRTInfo.radioInterfacesSignalInfoVideo[iRTInfoIndex][k].iSNRThreshMin > -200) )
             if ( (iMaxLocalSNRThresh == -1000) || (iMaxLocalSNRThresh < g_SMControllerRTInfo.radioInterfacesSignalInfoVideo[iRTInfoIndex][k].iSNRThreshMin) )
                iMaxLocalSNRThresh = g_SMControllerRTInfo.radioInterfacesSignalInfoVideo[iRTInfoIndex][k].iSNRThreshMin;

             if ( (g_SMControllerRTInfo.radioInterfacesSignalInfoVideo[iRTInfoIndex][k].iDbmThreshMin < 500) &&
                  (g_SMControllerRTInfo.radioInterfacesSignalInfoVideo[iRTInfoIndex][k].iDbmThreshMin > -200) )
             if ( (iMaxLocalRSSIThresh == -1000) || (iMaxLocalRSSIThresh < g_SMControllerRTInfo.radioInterfacesSignalInfoVideo[iRTInfoIndex][k].iDbmThreshMin) )
                iMaxLocalRSSIThresh = g_SMControllerRTInfo.radioInterfacesSignalInfoVideo[iRTInfoIndex][k].iDbmThreshMin;
          }
    
          if ( (iMaxLocalSNRThresh > -200) && (iMaxLocalSNRThresh < s_iAdaptiveMetric_MinimSNRThresh) )
             s_iAdaptiveMetric_MinimSNRThresh = iMaxLocalSNRThresh;
          if ( (iMaxLocalRSSIThresh > -200) && (iMaxLocalRSSIThresh < s_iAdaptiveMetric_MinimRSSIThresh) )
             s_iAdaptiveMetric_MinimRSSIThresh = iMaxLocalRSSIThresh;
       }
 
       iRTInfoIndex--;
       if ( iRTInfoIndex < 0 )
          iRTInfoIndex = SYSTEM_RT_INFO_INTERVALS-1;
   }

   if ( 0 != s_iAdaptiveMetric_IntervalsWithVideoBlocks )
   {
       if ( s_uAdaptiveMetric_TimeToLookBackMs >= s_AdaptiveMetrics.uTimeToLookBackForECUsed )
          s_iAdaptiveMetric_PercentageBlocksWithECHits = (100*s_iAdaptiveMetric_IntervalsWithECHits)/s_iAdaptiveMetric_IntervalsWithVideoBlocks;
       if ( s_uAdaptiveMetric_TimeToLookBackMs >= s_AdaptiveMetrics.uTimeToLookBackForECMax )
          s_iAdaptiveMetric_PercentageBlocksWithMaxECHits = (100*s_iAdaptiveMetric_IntervalsWithMaxECHits)/s_iAdaptiveMetric_IntervalsWithVideoBlocks;
   }

   if ( MAX_U32 != s_AdaptiveMetrics.uTimeToLookBackForRxLost )
   if ( 0 != s_iAdaptiveMetric_IntervalsWithAnyRadioData )
      s_iAdaptiveMetric_PercentageIntervalsWithBadPackets = (s_iAdaptiveMetric_IntervalsWithBadPackets * 100) / s_iAdaptiveMetric_IntervalsWithAnyRadioData;
}

static bool bLastAdaptiveCheckWasAllAbove = false;

// Returns true if metrics are below a minimum cutoff for given strength
bool _adaptive_video_is_metrics_below_or_above_strength(int iStrength, Model* pModel, type_global_state_vehicle_runtime_info* pRuntimeInfo, bool bBelow)
{
   // Adaptive adjustment strength is
   // 1: lowest (slower) adjustment strength;
   // 10+1: highest (fastest) adjustment strength;

   // Metrics importance, from less important/urgent, to most important/urgent:
   //  (Compute them all, check them in order of importance from highest to lowest)
   //
   //  RSSI                       (least important, least urgent to act on)
   //  SNR
   //  Rx lost packets
   //  Blocks with EC used
   //  Percentage of EC blocks used
   //  Blocks with max EC used
   //  Percentage of EC max blocks used
   //  Retransmissions
   //  Bad output video data       (most important, most urgent to act on)

   // Check them in order of importance, from most important one to less important one

   int iMaxBadVideoBlocks = 0;
   if ( iStrength < 5 )
      iMaxBadVideoBlocks = 1;
   if ( iStrength < 3 )
      iMaxBadVideoBlocks = 2;
   if ( iStrength < 2 )
      iMaxBadVideoBlocks = 3;

   // Bad video blocks

   if ( bBelow && (s_iAdaptiveMetric_TotalBadVideoBlocksIntervals > iMaxBadVideoBlocks) )
   {
      shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, pModel->uVehicleId);
      if ( NULL != pSMVideoStreamInfo )
          pSMVideoStreamInfo->adaptiveHitsLow.iCountHitVideoLost++;
      bLastAdaptiveCheckWasAllAbove = false;
      log_line("[AdaptiveVideo] Hit on (strength %d, %u ms, %d intvls, %d video blcks) total bad video %d is greater than %d", iStrength, s_uAdaptiveMetric_TimeToLookBackMs, s_iAdaptiveMetric_IntervalsToLookBack, s_iAdaptiveMetric_TotalOutputVideoBlocks, s_iAdaptiveMetric_TotalBadVideoBlocksIntervals, iMaxBadVideoBlocks);
      return true;
   }

   if ( (!bBelow) && (s_iAdaptiveMetric_TotalBadVideoBlocksIntervals > iMaxBadVideoBlocks) )
   {
      bLastAdaptiveCheckWasAllAbove = false;
      return false;
   }

   // Retransmissions

   if ( MAX_U32 != s_AdaptiveMetrics.uTimeToLookBackForRetr )
   if ( s_uAdaptiveMetric_TimeToLookBackMs >= s_AdaptiveMetrics.uTimeToLookBackForRetr )
   if ( bBelow && (s_iAdaptiveMetric_TotalRequestedRetr >= s_AdaptiveMetrics.iMaxRetr) )
   {
      shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, pModel->uVehicleId);
      if ( NULL != pSMVideoStreamInfo )
          pSMVideoStreamInfo->adaptiveHitsLow.iCountHitRetr++;
      bLastAdaptiveCheckWasAllAbove = false;
      log_line("[AdaptiveVideo] Hit on (strength %d, %u ms, %d intvls, %d video blcks) total retr %d in %u ms is greater than %d", iStrength, s_uAdaptiveMetric_TimeToLookBackMs, s_iAdaptiveMetric_IntervalsToLookBack, s_iAdaptiveMetric_TotalOutputVideoBlocks, s_iAdaptiveMetric_TotalRequestedRetr, s_AdaptiveMetrics.uTimeToLookBackForRetr, s_AdaptiveMetrics.iMaxRetr);
      return true;
   }

   if ( MAX_U32 != s_AdaptiveMetrics.uTimeToLookBackForRetr )
   if ( s_uAdaptiveMetric_TimeToLookBackMs >= s_AdaptiveMetrics.uTimeToLookBackForRetr )
   if ( (!bBelow) && (s_iAdaptiveMetric_TotalRequestedRetr >= s_AdaptiveMetrics.iMaxRetr) )
   {
      bLastAdaptiveCheckWasAllAbove = false;
      return false;
   }

   // Rx lost packets

   if ( MAX_U32 != s_AdaptiveMetrics.uTimeToLookBackForRxLost )
   if ( s_uAdaptiveMetric_TimeToLookBackMs >= s_AdaptiveMetrics.uTimeToLookBackForRxLost )
   if ( bBelow && (s_iAdaptiveMetric_PercentageIntervalsWithBadPackets > s_AdaptiveMetrics.iMaxRxLostPercent) )
   {
      shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, pModel->uVehicleId);
      if ( NULL != pSMVideoStreamInfo )
          pSMVideoStreamInfo->adaptiveHitsLow.iCountHitRxLost++;
      bLastAdaptiveCheckWasAllAbove = false;
      log_line("[AdaptiveVideo] Hit on (strength %d, %u ms, %d intvls %d video blocks) bad packets intervals %d%% in %u ms is greater than %d%%", iStrength, s_uAdaptiveMetric_TimeToLookBackMs, s_iAdaptiveMetric_IntervalsToLookBack, s_iAdaptiveMetric_TotalOutputVideoBlocks, s_iAdaptiveMetric_PercentageIntervalsWithBadPackets, s_AdaptiveMetrics.uTimeToLookBackForRxLost, s_AdaptiveMetrics.iMaxRxLostPercent);
      return true;
   }

   if ( MAX_U32 != s_AdaptiveMetrics.uTimeToLookBackForRxLost )
   if ( s_uAdaptiveMetric_TimeToLookBackMs >= s_AdaptiveMetrics.uTimeToLookBackForRxLost )
   if ( (!bBelow) && (s_iAdaptiveMetric_PercentageIntervalsWithBadPackets > s_AdaptiveMetrics.iMaxRxLostPercent) )
   {
      bLastAdaptiveCheckWasAllAbove = false;
      return false;
   }
   // EC

   if ( MAX_U32 != s_AdaptiveMetrics.uTimeToLookBackForECUsed )
   if ( s_uAdaptiveMetric_TimeToLookBackMs >= s_AdaptiveMetrics.uTimeToLookBackForECUsed )
   if ( bBelow && (s_iAdaptiveMetric_PercentageBlocksWithECHits >= s_AdaptiveMetrics.iPercentageECUsed) )
   {
      shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, pModel->uVehicleId);
      if ( NULL != pSMVideoStreamInfo )
          pSMVideoStreamInfo->adaptiveHitsLow.iCountHitECUsed++;
      bLastAdaptiveCheckWasAllAbove = false;
      log_line("[AdaptiveVideo] Hit on (strength %d, %u ms, %d intvls, %d video blcks) blocks with EC hits in last %u ms: %d%%, is greater than %d%%", iStrength, s_uAdaptiveMetric_TimeToLookBackMs, s_iAdaptiveMetric_IntervalsToLookBack, s_iAdaptiveMetric_TotalOutputVideoBlocks, s_AdaptiveMetrics.uTimeToLookBackForECUsed, s_iAdaptiveMetric_PercentageBlocksWithECHits, s_AdaptiveMetrics.iPercentageECUsed);
      return true;
   }

   if ( MAX_U32 != s_AdaptiveMetrics.uTimeToLookBackForECUsed )
   if ( s_uAdaptiveMetric_TimeToLookBackMs >= s_AdaptiveMetrics.uTimeToLookBackForECUsed )
   if ( (!bBelow) && (s_iAdaptiveMetric_PercentageBlocksWithECHits >= s_AdaptiveMetrics.iPercentageECUsed) )
   {
      bLastAdaptiveCheckWasAllAbove = false;
      return false;
   }

   if ( MAX_U32 != s_AdaptiveMetrics.uTimeToLookBackForECMax )
   if ( s_uAdaptiveMetric_TimeToLookBackMs >= s_AdaptiveMetrics.uTimeToLookBackForECMax )
   if ( bBelow && (s_iAdaptiveMetric_PercentageBlocksWithMaxECHits >= s_AdaptiveMetrics.iPercentageECMax) )
   {
      shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, pModel->uVehicleId);
      if ( NULL != pSMVideoStreamInfo )
          pSMVideoStreamInfo->adaptiveHitsLow.iCountHitECMax++;
      bLastAdaptiveCheckWasAllAbove = false;
      log_line("[AdaptiveVideo] Hit on (strength %d, %u ms, %d intvls, %d video blcks) blocks with max EC hits in last %u ms: %d%%, is greater than %d%%", iStrength, s_uAdaptiveMetric_TimeToLookBackMs, s_iAdaptiveMetric_IntervalsToLookBack, s_iAdaptiveMetric_TotalOutputVideoBlocks, s_AdaptiveMetrics.uTimeToLookBackForECMax, s_iAdaptiveMetric_PercentageBlocksWithMaxECHits, s_AdaptiveMetrics.iPercentageECMax);
      return true;
   }

   if ( MAX_U32 != s_AdaptiveMetrics.uTimeToLookBackForECMax )
   if ( s_uAdaptiveMetric_TimeToLookBackMs >= s_AdaptiveMetrics.uTimeToLookBackForECMax )
   if ( (!bBelow) && (s_iAdaptiveMetric_PercentageBlocksWithMaxECHits >= s_AdaptiveMetrics.iPercentageECMax) )
   {
      bLastAdaptiveCheckWasAllAbove = false;
      return false;
   }
   // RSSI

   if ( (s_AdaptiveMetrics.iMinimRSSIThreshold > -1000) && (s_iAdaptiveMetric_MinimRSSIThresh < 1000) )
   if ( bBelow && (s_iAdaptiveMetric_MinimRSSIThresh < s_AdaptiveMetrics.iMinimRSSIThreshold) )
   {
      shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, pModel->uVehicleId);
      if ( NULL != pSMVideoStreamInfo )
          pSMVideoStreamInfo->adaptiveHitsLow.iCountHitRSSI++;
      bLastAdaptiveCheckWasAllAbove = false;
      log_line("[AdaptiveVideo] Hit on (strength %d, %u ms, %d intvls, %d video blcks) RSSI margin %d is less than %d", iStrength, s_uAdaptiveMetric_TimeToLookBackMs, s_iAdaptiveMetric_IntervalsToLookBack, s_iAdaptiveMetric_TotalOutputVideoBlocks, s_iAdaptiveMetric_MinimRSSIThresh, s_AdaptiveMetrics.iMinimRSSIThreshold);
      return true;
   }

   if ( (s_AdaptiveMetrics.iMinimRSSIThreshold > -1000) && (s_iAdaptiveMetric_MinimRSSIThresh < 1000) )
   if ( (!bBelow) && (s_iAdaptiveMetric_MinimRSSIThresh < s_AdaptiveMetrics.iMinimRSSIThreshold) )
   {
      bLastAdaptiveCheckWasAllAbove = false;
      return false;
   }
   // SNR

   if ( (s_AdaptiveMetrics.iMinimSNRThreshold > -1000) && (s_iAdaptiveMetric_MinimSNRThresh < 1000) )
   if ( bBelow && (s_iAdaptiveMetric_MinimSNRThresh < s_AdaptiveMetrics.iMinimSNRThreshold) )
   {
      shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, pModel->uVehicleId);
      if ( NULL != pSMVideoStreamInfo )
          pSMVideoStreamInfo->adaptiveHitsLow.iCountHitSNR++;
      bLastAdaptiveCheckWasAllAbove = false;
      log_line("[AdaptiveVideo] Hit on (strength %d, %u ms, %d intvls, %d video blcks) SNR margin %d is less than %d", iStrength, s_uAdaptiveMetric_TimeToLookBackMs, s_iAdaptiveMetric_IntervalsToLookBack, s_iAdaptiveMetric_TotalOutputVideoBlocks, s_iAdaptiveMetric_MinimSNRThresh, s_AdaptiveMetrics.iMinimSNRThreshold);
      return true;
   }

   if ( (s_AdaptiveMetrics.iMinimSNRThreshold > -1000) && (s_iAdaptiveMetric_MinimSNRThresh < 1000) )
   if ( (!bBelow) && (s_iAdaptiveMetric_MinimSNRThresh < s_AdaptiveMetrics.iMinimSNRThreshold) )
   {
      bLastAdaptiveCheckWasAllAbove = false;
      return false;
   }
   if ( bBelow )
      return false;

   // Above all
   shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, pModel->uVehicleId);
   if ( (NULL != pSMVideoStreamInfo) && (!bLastAdaptiveCheckWasAllAbove) )
   {
      bLastAdaptiveCheckWasAllAbove = true;
      pSMVideoStreamInfo->adaptiveHitsHigh.iCountHitVideoLost++;
      pSMVideoStreamInfo->adaptiveHitsHigh.iCountHitRetr++;
      pSMVideoStreamInfo->adaptiveHitsHigh.iCountHitRxLost++;
      pSMVideoStreamInfo->adaptiveHitsHigh.iCountHitECUsed++;
      pSMVideoStreamInfo->adaptiveHitsHigh.iCountHitECMax++;
      pSMVideoStreamInfo->adaptiveHitsHigh.iCountHitRSSI++;
      pSMVideoStreamInfo->adaptiveHitsHigh.iCountHitSNR++;

      static u32 s_uLastAdaptiveLogRelax = 0;
      if ( g_TimeNow >= s_uLastAdaptiveLogRelax + 100 )
      {
         s_uLastAdaptiveLogRelax = g_TimeNow;
         log_line("[AdaptiveVideo] Relax on (strength %d, %u ms, %d intvls, %d video blcks) total bad video %d is lower than %d", iStrength, s_uAdaptiveMetric_TimeToLookBackMs, s_iAdaptiveMetric_IntervalsToLookBack, s_iAdaptiveMetric_TotalOutputVideoBlocks, s_iAdaptiveMetric_TotalBadVideoBlocksIntervals, iMaxBadVideoBlocks);
         log_line("[AdaptiveVideo] Relax on (strength %d, %u ms, %d intvls, %d video blcks) total retr %d in %u ms is lower than %d", iStrength, s_uAdaptiveMetric_TimeToLookBackMs, s_iAdaptiveMetric_IntervalsToLookBack, s_iAdaptiveMetric_TotalOutputVideoBlocks, s_iAdaptiveMetric_TotalRequestedRetr, s_AdaptiveMetrics.uTimeToLookBackForRetr, s_AdaptiveMetrics.iMaxRetr);
         log_line("[AdaptiveVideo] Relax on (strength %d, %u ms, %d intvls %d video blocks) bad packets intervals %d%% in %u ms is lower than %d%%", iStrength, s_uAdaptiveMetric_TimeToLookBackMs, s_iAdaptiveMetric_IntervalsToLookBack, s_iAdaptiveMetric_TotalOutputVideoBlocks, s_iAdaptiveMetric_PercentageIntervalsWithBadPackets, s_AdaptiveMetrics.uTimeToLookBackForRxLost, s_AdaptiveMetrics.iMaxRxLostPercent);
         log_line("[AdaptiveVideo] Relax on (strength %d, %u ms, %d intvls, %d video blcks) blocks with EC hits in last %u ms: %d%%, is lower than %d%%", iStrength, s_uAdaptiveMetric_TimeToLookBackMs, s_iAdaptiveMetric_IntervalsToLookBack, s_iAdaptiveMetric_TotalOutputVideoBlocks, s_AdaptiveMetrics.uTimeToLookBackForECUsed, s_iAdaptiveMetric_PercentageBlocksWithECHits, s_AdaptiveMetrics.iPercentageECUsed);
         log_line("[AdaptiveVideo] Relax on (strength %d, %u ms, %d intvls, %d video blcks) blocks with max EC hits in last %u ms: %d%%, is lower than %d%%", iStrength, s_uAdaptiveMetric_TimeToLookBackMs, s_iAdaptiveMetric_IntervalsToLookBack, s_iAdaptiveMetric_TotalOutputVideoBlocks, s_AdaptiveMetrics.uTimeToLookBackForECMax, s_iAdaptiveMetric_PercentageBlocksWithMaxECHits, s_AdaptiveMetrics.iPercentageECMax);
         log_line("[AdaptiveVideo] Relax on (strength %d, %u ms, %d intvls, %d video blcks) RSSI margin %d is greater than %d", iStrength, s_uAdaptiveMetric_TimeToLookBackMs, s_iAdaptiveMetric_IntervalsToLookBack, s_iAdaptiveMetric_TotalOutputVideoBlocks, s_iAdaptiveMetric_MinimRSSIThresh, s_AdaptiveMetrics.iMinimRSSIThreshold);
         log_line("[AdaptiveVideo] Relax on (strength %d, %u ms, %d intvls, %d video blcks) SNR margin %d is greater than %d", iStrength, s_uAdaptiveMetric_TimeToLookBackMs, s_iAdaptiveMetric_IntervalsToLookBack, s_iAdaptiveMetric_TotalOutputVideoBlocks, s_iAdaptiveMetric_MinimSNRThresh, s_AdaptiveMetrics.iMinimSNRThreshold);
      }
   }
   return true;
}

bool _adaptive_video_should_switch_lower(Model* pModel, type_global_state_vehicle_runtime_info* pRuntimeInfo)
{
   return _adaptive_video_is_metrics_below_or_above_strength(pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].iAdaptiveAdjustmentStrength, pModel, pRuntimeInfo, true);
}

bool _adaptive_video_should_switch_higher(Model* pModel, type_global_state_vehicle_runtime_info* pRuntimeInfo)
{
   return _adaptive_video_is_metrics_below_or_above_strength(pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].iAdaptiveAdjustmentStrength + 1, pModel, pRuntimeInfo, false);
}

// Returns true if it switched
bool _adaptive_video_switch_lower(Model* pModel, type_global_state_vehicle_runtime_info* pRuntimeInfo)
{
   int iCurrentVideoProfile = pModel->video_params.iCurrentVideoProfile;
   bool bOnlyMediumAdaptive = false;
   if ( (pModel->video_link_profiles[iCurrentVideoProfile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO )
      bOnlyMediumAdaptive = true;

   u32 uProfileFlags = pModel->video_link_profiles[iCurrentVideoProfile].uProfileFlags;
   u32 uMaxDRBoost = (uProfileFlags & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK) >> VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT;

   _adaptive_video_log_state(pModel, pRuntimeInfo, "Starting switch lower");

   //--------------------------------------------------------
   // First decrease DR boost if possible

   if ( uProfileFlags & VIDEO_PROFILE_FLAG_USE_HIGHER_DATARATE )
   if ( uMaxDRBoost > 0 )
   if ( (pRuntimeInfo->uCurrentDRBoost == 0xFF) || (pRuntimeInfo->uCurrentDRBoost > 0) )
   {
      if ( pRuntimeInfo->uCurrentDRBoost == 0xFF )
         pRuntimeInfo->uCurrentDRBoost = uMaxDRBoost;
      pRuntimeInfo->uCurrentDRBoost--;
      log_line("[AdaptiveVideo] Switch to lower DR boost: %d", pRuntimeInfo->uCurrentDRBoost);
      pRuntimeInfo->uPendingDRBoostToSet = pRuntimeInfo->uCurrentDRBoost;
      pRuntimeInfo->uAdaptiveVideoRequestId++;
      pRuntimeInfo->iAdaptiveLevelNow++;

      _adaptive_video_log_state(pModel, pRuntimeInfo, "After switch lower");
      return true;
   }

   //------------------------------------------------------------------
   // Second decrease bitrate and datarate if possible (if there is room)

   u32 uNewLowerVideoBitrate = 0;
   int iNewLowerDR = 0;
   int iNewLowerLink = -1;
   for( int iLink=0; iLink<pModel->radioLinksParams.links_count; iLink++)
   {
      // Skip disabled, low capacity or fixed datarates links
      if ( ! pModel->isRadioLinkAdaptiveUsable(iLink) )
         continue;

      int iDataRateForCurrentVideoBitrate = pModel->getRadioDataRateForVideoBitrate(pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS, iLink, true);
      char szCurDR[64];
      strcpy(szCurDR, str_format_datarate_inline(pRuntimeInfo->iCurrentDataratesForLinks[iLink]));
      log_line("[AdaptiveVideo] Switch lower: Radio link %d required datarate for current video bitrate is: %s; Current DR is: %s", iLink+1, str_format_datarate_inline(iDataRateForCurrentVideoBitrate), szCurDR);

      // Can't go lower rate? Skip it
      if ( iDataRateForCurrentVideoBitrate == 0 )
         continue;
      if ( (-1 == iDataRateForCurrentVideoBitrate) || (iDataRateForCurrentVideoBitrate == getLegacyDataRatesBPS()[0]) )
         continue;
      if ( bOnlyMediumAdaptive && (-2 == iDataRateForCurrentVideoBitrate) )
         continue;
      if ( bOnlyMediumAdaptive && (iDataRateForCurrentVideoBitrate == getLegacyDataRatesBPS()[1]) )
         continue;

      if ( iDataRateForCurrentVideoBitrate < -1 )
         iDataRateForCurrentVideoBitrate++;
      if ( iDataRateForCurrentVideoBitrate > 0 )
      {
         for( int i=1; i<getLegacyDataRatesCount(); i++ )
         {
            if ( getLegacyDataRatesBPS()[i] == iDataRateForCurrentVideoBitrate )
            {
               iDataRateForCurrentVideoBitrate = getLegacyDataRatesBPS()[i-1];
               break;
            }
         }
      }

      u32 uVideoBitrateForLink = pModel->getMaxVideoBitrateForRadioDatarate(iDataRateForCurrentVideoBitrate, iLink);
      log_line("[AdaptiveVideo] Switch lower: New radio link %d datarate would be: %s, max video bitrate for it is %.2f Mbps", iLink+1, str_format_datarate_inline(iDataRateForCurrentVideoBitrate), (float)uVideoBitrateForLink/1000.0/1000.0);
      uVideoBitrateForLink = uVideoBitrateForLink - uVideoBitrateForLink/20;

      if ( uVideoBitrateForLink > uNewLowerVideoBitrate )
      {
         uNewLowerVideoBitrate = uVideoBitrateForLink;
         iNewLowerDR = iDataRateForCurrentVideoBitrate;
         iNewLowerLink = iLink;
      }
   }

   if ( (0 != uNewLowerVideoBitrate) && (iNewLowerLink >= 0) )
   if ( uNewLowerVideoBitrate != pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS )
   {
      log_line("[AdaptiveVideo] Switch to lower video bitrate: %.2f Mbps and to lower DR on radio link %d: %s", (float)uNewLowerVideoBitrate/1000.0/1000.0, iNewLowerLink+1, str_format_datarate_inline(iNewLowerDR));
      pRuntimeInfo->iCurrentDataratesForLinks[iNewLowerLink] = iNewLowerDR;
      pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS = uNewLowerVideoBitrate;
      pRuntimeInfo->uPendingVideoBitrateToSet = pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS;
      pRuntimeInfo->uAdaptiveVideoRequestId++;
      pRuntimeInfo->iAdaptiveLevelNow++;

      _adaptive_video_log_state(pModel, pRuntimeInfo, "After switch lower");
      return true;
   }

   //-----------------------------------------------------------------------
   // Third, increase EC scheme and lower the bitrate if on lowest datarate already

   if ( (pRuntimeInfo->uCurrentAdaptiveVideoECScheme != 0) && (pRuntimeInfo->uCurrentAdaptiveVideoECScheme != 0xFFFF) )
   {
      log_line("[AdaptiveVideo] Switch lower: Can't switch lower. Already at bottom.");
      pRuntimeInfo->bIsOnLowestAdaptiveLevel = true;
      return false;
   }

   int iBlockPackets = pModel->video_link_profiles[iCurrentVideoProfile].iBlockDataPackets;
   int iMaxEC = ceil((iBlockPackets*MAX_VIDEO_EC_PERCENTAGE)/100 - 0.001);
   if ( iMaxEC <= 0 )
      iMaxEC = 1;

   // current_total = video + ec;
   // new_total = video + ec + extra_ec;
   float fLoadIncrease = (float)(iMaxEC - pModel->video_link_profiles[iCurrentVideoProfile].iBlockECs)/(float)(pModel->video_link_profiles[iCurrentVideoProfile].iBlockDataPackets);
   log_line("[AdaptiveVideo] Switch lower: Will switch to EC scheme %d / %d, load increase: %.1f%%", iBlockPackets, iMaxEC, fLoadIncrease*100.0);

   //pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS = pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS - pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS * fLoadIncrease;
   //if ( pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS < DEFAULT_LOWEST_ALLOWED_ADAPTIVE_VIDEO_BITRATE )
   //   pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS = DEFAULT_LOWEST_ALLOWED_ADAPTIVE_VIDEO_BITRATE;
   
   pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS = DEFAULT_LOWEST_ALLOWED_ADAPTIVE_VIDEO_BITRATE;
   if ( pModel->isActiveCameraVeye() )
      pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS *= 2;
   if ( bOnlyMediumAdaptive )
   {
      pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS = DEFAULT_LOWEST_ALLOWED_ADAPTIVE_MEDIUM_VIDEO_BITRATE;
      if ( pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAG_USE_LOWER_DR_FOR_EC_PACKETS )
         pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS = (DEFAULT_LOWEST_ALLOWED_ADAPTIVE_MEDIUM_VIDEO_BITRATE*2)/3;
   }
   log_line("[AdaptiveVideo] Switch lower: Will switch to EC scheme %d / %d, new target video bitrate: %.2f Mbps", iBlockPackets, iMaxEC, (float)pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS/1000.0/1000.0);

   pRuntimeInfo->uCurrentAdaptiveVideoECScheme = (((u16)iBlockPackets)<<8) | (((u16)iMaxEC) & 0x00FF);

   pRuntimeInfo->uPendingVideoBitrateToSet = pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS;
   pRuntimeInfo->uPendingECSchemeToSet = pRuntimeInfo->uCurrentAdaptiveVideoECScheme;
   pRuntimeInfo->uAdaptiveVideoRequestId++;
   pRuntimeInfo->iAdaptiveLevelNow++;
   pRuntimeInfo->bIsOnLowestAdaptiveLevel = true;

   shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, pModel->uVehicleId);
   if ( NULL != pSMVideoStreamInfo )
      pSMVideoStreamInfo->bIsOnLowestAdaptiveLevel = true;

   _adaptive_video_log_state(pModel, pRuntimeInfo, "After switch lower");
   return true;
}

// Returns true if it switched higher
bool _adaptive_video_switch_higher(Model* pModel, type_global_state_vehicle_runtime_info* pRuntimeInfo)
{
   int iCurrentVideoProfile = pModel->video_params.iCurrentVideoProfile;
   u32 uProfileFlags = pModel->video_link_profiles[iCurrentVideoProfile].uProfileFlags;
   u32 uMaxDRBoost = (uProfileFlags & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK) >> VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT;

   _adaptive_video_log_state(pModel, pRuntimeInfo, "Starting switch higher");

   //-----------------------------------------------------------------------
   // First, revert EC scheme and increase the bitrate if on lowest datarate already

   if ( (pRuntimeInfo->uCurrentAdaptiveVideoECScheme != 0) && (pRuntimeInfo->uCurrentAdaptiveVideoECScheme != 0xFFFF) && pRuntimeInfo->bIsOnLowestAdaptiveLevel )
   {
      log_line("[AdaptiveVideo] Switch higher: Is on lower EC scheme. Switch to default EC scheme. Video bitrate is now (before change): %.1f", (float)pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS/1000.0/1000.0);
      pRuntimeInfo->uCurrentAdaptiveVideoECScheme = 0;
      pRuntimeInfo->uPendingECSchemeToSet = 0xFFFF;

      u32 uNewHigherVideoBitrate = 0;//pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS;

      for( int iLink=0; iLink<pModel->radioLinksParams.links_count; iLink++)
      {
         // Skip disabled, low capacity or fixed datarates links
         if ( ! pModel->isRadioLinkAdaptiveUsable(iLink) )
            continue;

         //int iDataRateForCurrentVideoBitrate = pModel->getRadioDataRateForVideoBitrate(pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS, iLink, true);
         int iDataRateForCurrentVideoBitrate = pRuntimeInfo->iCurrentDataratesForLinks[iLink];
         u32 uMaxVideoBitrateForLink = pModel->getMaxVideoBitrateForRadioDatarate(iDataRateForCurrentVideoBitrate, iLink);
         log_line("[AdaptiveVideo] Switch higher to default EC scheme: current datarate for radio link %d: %d, max video bitrate for current datarate: %.1f", iLink+1, iDataRateForCurrentVideoBitrate, (float)uMaxVideoBitrateForLink/1000.0/1000.0);
         uMaxVideoBitrateForLink = uMaxVideoBitrateForLink - uMaxVideoBitrateForLink/20;
         if ( 0 == uNewHigherVideoBitrate )
            uNewHigherVideoBitrate = uMaxVideoBitrateForLink;
         else if ( uMaxVideoBitrateForLink < uNewHigherVideoBitrate )
            uNewHigherVideoBitrate = uMaxVideoBitrateForLink;
      }

      if ( 0 == uNewHigherVideoBitrate )
         uNewHigherVideoBitrate = pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS;
      else
         uNewHigherVideoBitrate = (uNewHigherVideoBitrate + pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS)/2;
      pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS = uNewHigherVideoBitrate;
      log_line("[AdaptiveVideo] New video bitrate after EC scheme revert to default will be: %.1f", (float)pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS/1000.0/1000.0);
      pRuntimeInfo->uPendingVideoBitrateToSet = pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS;
      pRuntimeInfo->uAdaptiveVideoRequestId++;
      pRuntimeInfo->iAdaptiveLevelNow--;
      pRuntimeInfo->bIsOnLowestAdaptiveLevel = false;
      shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, pModel->uVehicleId);
      if ( NULL != pSMVideoStreamInfo )
         pSMVideoStreamInfo->bIsOnLowestAdaptiveLevel = false;
      _adaptive_video_log_state(pModel, pRuntimeInfo, "After switch higher");
      return true;
   }

   //------------------------------------------------------------------
   // Second increase the bitrate and datarate if possible (is below target)

   u32 uNewHigherVideoBitrate = pModel->video_link_profiles[iCurrentVideoProfile].uTargetVideoBitrateBPS;
   int iNewHigherDR = 0;
   int iNewHigherLink = -1;
   int iCurrentDatarates[MAX_RADIO_INTERFACES];

   for( int iLink=0; iLink<pModel->radioLinksParams.links_count; iLink++)
   {
      // Skip disabled, low capacity or fixed datarates links
      if ( ! pModel->isRadioLinkAdaptiveUsable(iLink) )
         continue;

      iCurrentDatarates[iLink] = pModel->getRadioDataRateForVideoBitrate(pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS, iLink, true);
      log_line("[AdaptiveVideo] Switch higher: Radio link %d required datarate for current video bitrate is: %s", iLink+1, str_format_datarate_inline(iCurrentDatarates[iLink]));

      // Can't go higher rate? Skip it
      if ( iCurrentDatarates[iLink] == 0 )
         continue;

      u32 uMaxVideoBitrateForLinkDatarate = pModel->getMaxVideoBitrateForRadioDatarate(iCurrentDatarates[iLink], iLink);
      u32 uMaxLinkLoadPercentage = (u32)pModel->radioLinksParams.uMaxLinkLoadPercent[iLink];
      if ( (pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].iDefaultLinkLoad > 0) && (pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].iDefaultLinkLoad <= 90) )
         uMaxLinkLoadPercentage = pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].iDefaultLinkLoad;

      log_line("[AdaptiveVideo] Switch higher: Radio link %d datarate %s, max video bitrate for it: %.2f Mbps", iLink+1, str_format_datarate_inline(iCurrentDatarates[iLink]), (float)uMaxVideoBitrateForLinkDatarate/1000.0/1000.0);
      log_line("[AdaptiveVideo] Switch higher: Radio link %d datarate %s, max data throughtput: %u bps", iLink+1, str_format_datarate_inline(iCurrentDatarates[iLink]), getRealDataRateFromRadioDataRate(iCurrentDatarates[iLink], pModel->radioLinksParams.link_radio_flags[iLink], 1));
      log_line("[AdaptiveVideo] Switch higher: Radio link %d datarate %s, max load percent: radio: %d%%, EC: %d%%", iLink+1, str_format_datarate_inline(iCurrentDatarates[iLink]), uMaxLinkLoadPercentage, pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].iECPercentage);
      uMaxVideoBitrateForLinkDatarate = uMaxVideoBitrateForLinkDatarate + uMaxVideoBitrateForLinkDatarate/20;
      if ( uMaxVideoBitrateForLinkDatarate > pModel->video_link_profiles[iCurrentVideoProfile].uTargetVideoBitrateBPS )
         uMaxVideoBitrateForLinkDatarate = pModel->video_link_profiles[iCurrentVideoProfile].uTargetVideoBitrateBPS;
      int iNewDR = pModel->getRadioDataRateForVideoBitrate(uMaxVideoBitrateForLinkDatarate, iLink, true);
      log_line("[AdaptiveVideo] Switch higher: Radio link %d datarate would be: %s for new video bitrate: %.2f Mbps", iLink+1, str_format_datarate_inline(iNewDR), (float)uMaxVideoBitrateForLinkDatarate/1000.0/1000.0);

      if ( uMaxVideoBitrateForLinkDatarate < uNewHigherVideoBitrate )
      {
         uNewHigherVideoBitrate = uMaxVideoBitrateForLinkDatarate;
         iNewHigherDR = iNewDR;
         iNewHigherLink = iLink;
      }
   }

   if ( iNewHigherLink >= 0 )
   if ( uNewHigherVideoBitrate != pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS )
   {
      bool bUpdatedRates = false;
      for( int iLink=0; iLink<pModel->radioLinksParams.links_count; iLink++)
      {
         int iNewDatarate = pModel->getRadioDataRateForVideoBitrate(uNewHigherVideoBitrate, iLink, true);
         char szNewDatarate[64];
         strcpy(szNewDatarate, str_format_datarate_inline(iNewDatarate));
         log_line("[AdaptiveVideo] Do switch higher: current/new radio link %d datarate: %s / %s", iLink+1, str_format_datarate_inline(iCurrentDatarates[iLink]), szNewDatarate);
         if ( iNewDatarate != iCurrentDatarates[iLink] )
            bUpdatedRates = true;
      }
      log_line("[AdaptiveVideo] Do switch higher: New video bitrate to set: %.2f bps", (float)uNewHigherVideoBitrate/1000.0/1000.0);
      log_line("[AdaptiveVideo] Do switch higher to %.2f Mbps video bitrate", (float)uNewHigherVideoBitrate/1000.0/1000.0);
      pRuntimeInfo->iCurrentDataratesForLinks[iNewHigherLink] = iNewHigherDR;
      pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS = uNewHigherVideoBitrate;
      pRuntimeInfo->uPendingVideoBitrateToSet = pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS;
      pRuntimeInfo->uAdaptiveVideoRequestId++;
      if ( bUpdatedRates )
         pRuntimeInfo->iAdaptiveLevelNow--;

      char szDR[128];
      _adaptive_video_log_DRlinks(pModel, pRuntimeInfo, szDR);

      log_line("[AdaptiveVideo] Do switch higher: Radio datarates have been updated? %s; New DR for links: %s", bUpdatedRates?"yes":"no", szDR);

      _adaptive_video_log_state(pModel, pRuntimeInfo, "After switch higher");
      return true;
   }


   //--------------------------------------------------------
   // Third increase DR boost if possible

   if ( uProfileFlags & VIDEO_PROFILE_FLAG_USE_HIGHER_DATARATE )
   if ( uMaxDRBoost > 0 )
   if ( (pRuntimeInfo->uCurrentDRBoost == 0xFF) || (pRuntimeInfo->uCurrentDRBoost < uMaxDRBoost) )
   {
      if ( pRuntimeInfo->uCurrentDRBoost == 0xFF )
         pRuntimeInfo->uCurrentDRBoost = uMaxDRBoost;
      if ( pRuntimeInfo->uCurrentDRBoost < uMaxDRBoost )
      {
         pRuntimeInfo->uCurrentDRBoost++;
         log_line("[AdaptiveVideo] Switch to higher DR boost: %d", pRuntimeInfo->uCurrentDRBoost);
         pRuntimeInfo->uPendingDRBoostToSet = pRuntimeInfo->uCurrentDRBoost;
         pRuntimeInfo->uAdaptiveVideoRequestId++;
         pRuntimeInfo->iAdaptiveLevelNow--;
         _adaptive_video_log_state(pModel, pRuntimeInfo, "After switch higher");
         return true;
      }
   }

   return false;
}

// Returns true if it switched adaptive level
bool _adaptive_video_check_vehicle(Model* pModel, type_global_state_vehicle_runtime_info* pRuntimeInfo, shared_mem_video_stream_stats* pSMVideoStreamInfo)
{
   if ( (NULL == pRuntimeInfo) || (NULL == pSMVideoStreamInfo) || (NULL == pModel) )
      return false;

   pSMVideoStreamInfo->iAdaptiveVideoLevelNow = pRuntimeInfo->iAdaptiveLevelNow;

   if ( s_bAdaptiveIsInTestMode )
   {
      if ( g_TimeNow < s_uTimeLastTestModeAdaptiveLevelUpdate + 3000 )
         return false;
      log_line("[AdaptiveVideo] Test mode update");
      s_uTimeLastTestModeAdaptiveLevelUpdate = g_TimeNow;

      _adaptive_video_test_mode_update(pRuntimeInfo);

      bool bSwitchedLevel = false;
      if ( !s_bAdaptiveTestModeDirectionUp )
      {
         bSwitchedLevel = _adaptive_video_switch_lower(pModel, pRuntimeInfo);
         log_line("[AdaptiveVideo] Test mode update switched lower? %s", bSwitchedLevel?"yes":"no");
         if ( ! bSwitchedLevel )
            s_bAdaptiveTestModeDirectionUp = true;
      }
      else
      {
         bSwitchedLevel = _adaptive_video_switch_higher(pModel, pRuntimeInfo);
         log_line("[AdaptiveVideo] Test mode update switched higher? %s", bSwitchedLevel?"yes":"no");
         if ( ! bSwitchedLevel )
            s_bAdaptiveTestModeDirectionUp = false;
      }
      return bSwitchedLevel;
   }

   // Adaptive adjustment strength is in: pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].iAdaptiveAdjustmentStrength
   // 1: lowest (slower) adjustment strength;
   // 10: highest (fastest) adjustment strength;

   _adaptive_video_compute_metrics(pModel, pRuntimeInfo);
   
   if ( g_TimeNow > pRuntimeInfo->uLastTimeSentAdaptiveVideoRequest + s_AdaptiveMetrics.uMinimumTimeToSwitchLower )
   if ( (pRuntimeInfo->uCurrentAdaptiveVideoECScheme == 0xFFFF) || (pRuntimeInfo->uCurrentAdaptiveVideoECScheme == 0) )
   if ( _adaptive_video_should_switch_lower(pModel, pRuntimeInfo) )
   {
      pRuntimeInfo->uTimeStartCountingMetricAreOkToSwithHigher = 0;
      return _adaptive_video_switch_lower(pModel, pRuntimeInfo);
   }

   u32 uProfileFlags = pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uProfileFlags;
   u32 uMaxDRBoost = (uProfileFlags & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK) >> VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT;
   if ( !(uProfileFlags & VIDEO_PROFILE_FLAG_USE_HIGHER_DATARATE) )
      uMaxDRBoost = 0;

   ProcessorRxVideo* pProcessorRxVideo = ProcessorRxVideo::getVideoProcessorForVehicleId(pModel->uVehicleId, 0);
   bool bChecksToSwitchHigherSucceeded = false;

   if ( g_TimeNow > pRuntimeInfo->uLastTimeSentAdaptiveVideoRequest + s_AdaptiveMetrics.uMinimumTimeToSwitchHigher )
   if ( 0 != pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS )
   if ( 0 != pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS )
   if ( (pRuntimeInfo->uCurrentAdaptiveVideoTargetVideoBitrateBPS < pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS) || ((pRuntimeInfo->uCurrentDRBoost != uMaxDRBoost) && (pRuntimeInfo->uCurrentDRBoost != 0xFF)) )
   if ( (NULL != pProcessorRxVideo) && (pProcessorRxVideo->getLastestVideoPacketReceiveTime() > g_TimeNow - 100) )
   if ( _adaptive_video_should_switch_higher(pModel, pRuntimeInfo) )
   {
      bChecksToSwitchHigherSucceeded = true;
      if ( 0 == pRuntimeInfo->uTimeStartCountingMetricAreOkToSwithHigher )
      {
         pRuntimeInfo->uTimeStartCountingMetricAreOkToSwithHigher = g_TimeNow;
         return false;
      }
      if ( g_TimeNow < pRuntimeInfo->uTimeStartCountingMetricAreOkToSwithHigher + s_AdaptiveMetrics.uMinimumGoodTimeToSwitchHigher )
         return false;
      pRuntimeInfo->uTimeStartCountingMetricAreOkToSwithHigher = g_TimeNow;
      return _adaptive_video_switch_higher(pModel, pRuntimeInfo);
   }
   if ( ! bChecksToSwitchHigherSucceeded )
      pRuntimeInfo->uTimeStartCountingMetricAreOkToSwithHigher = 0;

   return false;
}

void _adaptive_keyframe_check_vehicle(Model* pModel, type_global_state_vehicle_runtime_info* pRuntimeInfo, shared_mem_video_stream_stats* pSMVideoStreamInfo)
{
   if ( (NULL == pRuntimeInfo) || (NULL == pSMVideoStreamInfo) || (NULL == pModel) )
      return;

   /*
   static u32 s_uAdaptiveVideoLastTimeSetKeyframe = 0;
   if ( g_TimeNow > s_uAdaptiveVideoLastTimeSetKeyframe + 10000 )
   {
      s_uAdaptiveVideoLastTimeSetKeyframe = g_TimeNow;
      if ( pRuntimeInfo->iCurrentAdaptiveVideoKeyFrameMsTarget > 0 )
      if ( pRuntimeInfo->iPendingKeyFrameMsToSet == 0 )
         pRuntimeInfo->iPendingKeyFrameMsToSet = pRuntimeInfo->iCurrentAdaptiveVideoKeyFrameMsTarget;
   }
   */

   if ( pRuntimeInfo->iPendingKeyFrameMsToSet == 0 )
   if ( pRuntimeInfo->iCurrentAdaptiveVideoKeyFrameMsTarget != 0 )
   if ( g_TimeNow > pRuntimeInfo->uLastTimeSentAdaptiveVideoRequest + 1000 )
   if ( pSMVideoStreamInfo->PHVS.uCurrentVideoKeyframeIntervalMs != pRuntimeInfo->iCurrentAdaptiveVideoKeyFrameMsTarget )
   {
      log_line("[AdaptiveVideo] Video stream received keyframe interval (%u ms) it's the same as target one (%d ms). Set pending one (0 ms) to current target one.",
         pSMVideoStreamInfo->PHVS.uCurrentVideoKeyframeIntervalMs, pRuntimeInfo->iCurrentAdaptiveVideoKeyFrameMsTarget);
      pRuntimeInfo->iPendingKeyFrameMsToSet = 0;
      //s_uAdaptiveVideoLastTimeSetKeyframe = g_TimeNow;
   }
}


void _adaptive_video_periodic_loop_for_vehicle(int iRuntimeIndex, bool bForceSyncNow)
{
   type_global_state_vehicle_runtime_info* pRuntimeInfo = &(g_State.vehiclesRuntimeInfo[iRuntimeIndex]);
   Model* pModel = findModelWithId(g_State.vehiclesRuntimeInfo[iRuntimeIndex].uVehicleId, 28);

   if ( g_TimeNow < pRuntimeInfo->uAdaptiveVideoActivationTime + 1000 )
      return;

   bool bDoChecks = false;
   if ( g_TimeNow > pRuntimeInfo->uAdaptiveVideoLastCheckTime + 50 )
      bDoChecks = true;

   if ( bForceSyncNow )
   if ( g_TimeNow > pRuntimeInfo->uAdaptiveVideoLastCheckTime + 2 )
      bDoChecks = true;

   if ( g_TimeNow > pRuntimeInfo->uLastTimeRecvAdaptiveVideoAck + 30 )
   if ( g_TimeNow >= pRuntimeInfo->uAdaptiveVideoLastCheckTime + 5 )
      bDoChecks = true;

   if ( pRuntimeInfo->uAdaptiveVideoRequestId != pRuntimeInfo->uAdaptiveVideoAckId )
   if ( g_TimeNow >= pRuntimeInfo->uAdaptiveVideoLastCheckTime + 5 )
      bDoChecks = true;

   if ( ! bDoChecks )
      return;

   pRuntimeInfo->uAdaptiveVideoLastCheckTime = g_TimeNow;

   if ( (NULL == pModel) || (! pModel->hasCamera()) || (!is_sw_version_atleast(pModel, 11, 6)) )
   {
      if ( pRuntimeInfo->bIsAdaptiveVideoActive )
      {
         send_adaptive_video_paused_to_central(pRuntimeInfo->uVehicleId, true);
         log_line("[AdaptiveVideo] Set adaptive as inactive for VID %u (old vehicle or no camera)", pRuntimeInfo->uVehicleId);
      }
      pRuntimeInfo->bIsAdaptiveVideoActive = false;
      return;
   }

   if ( ! pRuntimeInfo->bDidFirstTimeAdaptiveHandshake )
   {
      if ( 0 == pRuntimeInfo->uAdaptiveVideoRequestId )
         _adaptive_video_init_first_handshake(iRuntimeIndex);
      
      u32 uDeltaTimeRequests = 10;
      if ( pModel->isVideoLinkFixedOneWay() )
         uDeltaTimeRequests = 2000;
      else if ( ! ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK) )
         uDeltaTimeRequests = 1000;
      // We still have a pending unacknowledged message
      if ( pRuntimeInfo->uAdaptiveVideoRequestId != pRuntimeInfo->uAdaptiveVideoAckId )
      if ( g_TimeNow > pRuntimeInfo->uLastTimeSentAdaptiveVideoRequest + uDeltaTimeRequests )
         _adaptive_video_send_adaptive_message_to_vehicle(pRuntimeInfo->uVehicleId);
      return;
   }

   if ( pModel->isVideoLinkFixedOneWay() )
   {
      if ( pRuntimeInfo->bIsAdaptiveVideoActive )
      {
         send_adaptive_video_paused_to_central(pRuntimeInfo->uVehicleId, true);
         log_line("[AdaptiveVideo] Set adaptive as inactive for VID %u (one way link)", pRuntimeInfo->uVehicleId);
      }
      pRuntimeInfo->bIsAdaptiveVideoActive = false;
      return;
   }

   // If link is lost, do not try to send adaptive packets to vehicle
   if ( pRuntimeInfo->bIsVehicleFastUplinkFromControllerLost && pRuntimeInfo->bIsVehicleSlowUplinkFromControllerLost )
   {
      if ( pRuntimeInfo->bIsAdaptiveVideoActive )
      {
         send_adaptive_video_paused_to_central(pRuntimeInfo->uVehicleId, true);
         log_line("[AdaptiveVideo] Set adaptive as inactive for VID %u (link lost)", pRuntimeInfo->uVehicleId);
      }
      pRuntimeInfo->bIsAdaptiveVideoActive = false;
      return;
   }

   shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, pModel->uVehicleId);
   ProcessorRxVideo* pProcessorRxVideo = ProcessorRxVideo::getVideoProcessorForVehicleId(pModel->uVehicleId, 0);

   // If haven't received yet the video stream, just skip
   if ( (NULL == pSMVideoStreamInfo) || (NULL == pProcessorRxVideo) )
   {
      if ( pRuntimeInfo->bIsAdaptiveVideoActive )
      {
         send_adaptive_video_paused_to_central(pRuntimeInfo->uVehicleId, true);
         log_line("[AdaptiveVideo] Set adaptive as inactive for VID %u (no video stream yet)", pRuntimeInfo->uVehicleId);
      }
      pRuntimeInfo->bIsAdaptiveVideoActive = false;
      return;
   }

   // Only fixed datarates video radio links? Then do no adaptive adjustments
   if ( pModel->isAllVideoLinksFixedRate() )
   {
      if ( pRuntimeInfo->bIsAdaptiveVideoActive )
      {
         send_adaptive_video_paused_to_central(pRuntimeInfo->uVehicleId, true);
         log_line("[AdaptiveVideo] Set adaptive as inactive for VID %u (fixed radio rates)", pRuntimeInfo->uVehicleId);
      }
      pRuntimeInfo->bIsAdaptiveVideoActive = false;
      return;
   }

   // Do adaptive video logic?
   if ( (pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK )
   {
      if ( ! pRuntimeInfo->bIsAdaptiveVideoActive )
      {
         send_adaptive_video_paused_to_central(pRuntimeInfo->uVehicleId, false);
         log_line("[AdaptiveVideo] Set adaptive as active for VID %u", pRuntimeInfo->uVehicleId);
      }
      pRuntimeInfo->bIsAdaptiveVideoActive = true;
      _adaptive_video_check_vehicle(pModel, pRuntimeInfo, pSMVideoStreamInfo);
   }

   // Do adaptive keyframe logic?
   if ( ((pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME) ||
         (pRuntimeInfo->iPendingKeyFrameMsToSet != 0) )
      _adaptive_keyframe_check_vehicle(pModel, pRuntimeInfo, pSMVideoStreamInfo);

   // We still have a pending unacknowledged message
   if ( pRuntimeInfo->uAdaptiveVideoRequestId != pRuntimeInfo->uAdaptiveVideoAckId )
   {
     if ( bForceSyncNow )
         pRuntimeInfo->uLastTimeSentAdaptiveVideoRequest = g_TimeNow-500;
     if ( g_TimeNow > pRuntimeInfo->uLastTimeSentAdaptiveVideoRequest + 10 )
        _adaptive_video_send_adaptive_message_to_vehicle(pRuntimeInfo->uVehicleId);
   }
}

void _adaptive_video_inactivate_all(const char* szReason)
{
   bool bAnyPaused = false;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( (0 != g_State.vehiclesRuntimeInfo[i].uVehicleId) && (MAX_U32 != g_State.vehiclesRuntimeInfo[i].uVehicleId) )
      if ( g_State.vehiclesRuntimeInfo[i].bIsAdaptiveVideoActive )
      {
         send_adaptive_video_paused_to_central(g_State.vehiclesRuntimeInfo[i].uVehicleId, true);
         bAnyPaused = true;
         log_line("[AdaptiveVideo] Inactivate all: Set adaptive as inactive for VID %u (is global paused)", g_State.vehiclesRuntimeInfo[i].uVehicleId);
      }
      g_State.vehiclesRuntimeInfo[i].bIsAdaptiveVideoActive = false;
      g_State.vehiclesRuntimeInfo[i].uAdaptiveVideoActivationTime = g_TimeNow;
   }

   if ( bAnyPaused )
   {
      if ( (NULL == szReason) || (0 == szReason[0]) )
         log_line("[AdaptiveVideo] Inactivate all (no reason)");
      else
         log_line("[AdaptiveVideo] Inactivate all (reason: %s)", szReason);
      log_line("[AdaptiveVideo] Inactivate all: All vehicles are now inactivated.");
   }
}

void adaptive_video_periodic_loop(bool bForceSyncNow)
{
   if ( g_TimeNow < s_uTimeLastAdaptiveVideoPeriodicChecks + 5 )
      return;
   if ( g_bSearching )
      return;

   s_uTimeLastAdaptiveVideoPeriodicChecks = g_TimeNow;

   if ( (g_TimeNow < g_TimeStart + 3000) || isNegociatingRadioLink() || test_link_is_in_progress() || g_bUpdateInProgress )
   {
      _adaptive_video_inactivate_all("Initial start or update/test/negociate link in progress.");
      return;
   }

   if ( 0 != s_uTimePauseAdaptiveVideoUntil )
   if ( g_TimeNow < s_uTimePauseAdaptiveVideoUntil )
   {
      _adaptive_video_inactivate_all("Global paused.");
      return;
   }

   if ( 0 != s_uTimePauseAdaptiveVideoUntil )
   if ( g_TimeNow >= s_uTimePauseAdaptiveVideoUntil )
   {
      log_line("[AdaptiveVideo] Periodic loop: Global resume after global pause elapsed.");
      s_uTimePauseAdaptiveVideoUntil = 0;
      adaptive_video_reset_time_for_vehicle(0);
      send_adaptive_video_paused_to_central(0, false);
   }
   
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( (g_State.vehiclesRuntimeInfo[i].uVehicleId == 0) || (g_State.vehiclesRuntimeInfo[i].uVehicleId == MAX_U32) )
         continue;
      if ( ! g_State.vehiclesRuntimeInfo[i].bIsPairingDone )
         continue;

      _adaptive_video_periodic_loop_for_vehicle(i, bForceSyncNow);
   }
}