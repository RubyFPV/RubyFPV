/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted 
     that the following conditions are met:
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
#include <stdarg.h>
#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>

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
#include "../base/models_list.h"
#include "../base/radio_utils.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../common/string_utils.h"
#include "../common/relay_utils.h"
#include "../common/radio_stats.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../radio/radio_rx.h"
#include "../radio/radiopacketsqueue.h"

#include "shared_vars.h"
#include "shared_vars_state.h"
#include "processor_rx_video.h"
#include "rx_video_output.h"
#include "packets_utils.h"
#include "video_rx_buffers.h"
#include "timers.h"
#include "ruby_rt_station.h"
#include "test_link_params.h"
#include "adaptive_video.h"

#define DEFAULT_RETRANSMISSION_MIN_REQUEST_INTERVAL_MS 5

extern t_packet_queue s_QueueRadioPacketsHighPrio;

int ProcessorRxVideo::m_siInstancesCount = 0;

void ProcessorRxVideo::oneTimeInit()
{
   m_siInstancesCount = 0;
   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
      g_pVideoProcessorRxList[i] = NULL;
   log_line("[ProcessorRxVideo] Did one time initialization.");
}

ProcessorRxVideo* ProcessorRxVideo::getVideoProcessorForVehicleId(u32 uVehicleId, u32 uVideoStreamIndex)
{
   if ( (0 == uVehicleId) || (MAX_U32 == uVehicleId) )
      return NULL;

   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( NULL != g_pVideoProcessorRxList[i] )
      if ( g_pVideoProcessorRxList[i]->m_uVehicleId == uVehicleId )
      if ( g_pVideoProcessorRxList[i]->m_uVideoStreamIndex == uVideoStreamIndex )
         return g_pVideoProcessorRxList[i];
   }
   return NULL;
}

ProcessorRxVideo::ProcessorRxVideo(u32 uVehicleId, u8 uVideoStreamIndex)
:m_bInitialized(false)
{
   m_iInstanceIndex = m_siInstancesCount;
   m_siInstancesCount++;

   log_line("[ProcessorRxVideo] Created new instance (number %d of %d) for VID: %u, stream %u", m_iInstanceIndex+1, m_siInstancesCount, uVehicleId, uVideoStreamIndex);
   m_uVehicleId = uVehicleId;
   m_uVideoStreamIndex = uVideoStreamIndex;
   m_uLastTimeRequestedRetransmission = 0;
   m_uLastTimeReceivedRetransmission = 0;
   m_uLastTimeCheckedForMissingPackets = 0;
   m_uRequestRetransmissionUniqueId = 0;
   m_TimeLastHistoryStatsUpdate = 0;
   m_TimeLastRetransmissionsStatsUpdate = 0;

   m_uLastVideoBlockIndexResolutionChange = 0;
   m_uLastVideoBlockPacketIndexResolutionChange = 0;

   m_bPaused = false;
   m_bPauseTempRetrUntillANewVideoPacket = true;
   m_uTimeLastResumedTempRetrPause = 0;
   m_bMustParseStream = false;
   m_bWasParsingStream = false;
   m_ParserH264.init();

   m_pVideoRxBuffer = new VideoRxPacketsBuffer(uVideoStreamIndex, 0);
   Model* pModel = findModelWithId(uVehicleId, 201);
   if ( NULL == pModel )
      log_softerror_and_alarm("[ProcessorRxVideo] Can't find model for VID %u", uVehicleId);
   else
      m_pVideoRxBuffer->init(pModel);

   // Add it to the video decode stats shared mem list

   m_iIndexVideoDecodeStats = -1;
   for(int i=0; i<MAX_VIDEO_PROCESSORS; i++)
   {
      if ( g_SM_VideoDecodeStats.video_streams[i].uVehicleId == 0 )
      {
         m_iIndexVideoDecodeStats = i;
         break;
      }
      if ( g_SM_VideoDecodeStats.video_streams[i].uVehicleId == uVehicleId )
      {
         m_iIndexVideoDecodeStats = i;
         break;
      }
   }
   if ( -1 != m_iIndexVideoDecodeStats )
   {
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uVehicleId = uVehicleId;
      reset_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, uVehicleId);
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uVideoStreamIndex = uVideoStreamIndex;
   }
}

ProcessorRxVideo::~ProcessorRxVideo()
{
   
   log_line("[ProcessorRxVideo] Video processor deleted for VID %u, video stream %u", m_uVehicleId, m_uVideoStreamIndex);

   m_siInstancesCount--;

   // Remove this processor from video decode stats list
   if ( m_iIndexVideoDecodeStats != -1 )
   {
      memset(&(g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats]), 0, sizeof(shared_mem_video_stream_stats));
      for( int i=m_iIndexVideoDecodeStats; i<MAX_VIDEO_PROCESSORS-1; i++ )
         memcpy(&(g_SM_VideoDecodeStats.video_streams[i]), &(g_SM_VideoDecodeStats.video_streams[i+1]), sizeof(shared_mem_video_stream_stats));
      memset(&(g_SM_VideoDecodeStats.video_streams[MAX_VIDEO_PROCESSORS-1]), 0, sizeof(shared_mem_video_stream_stats));
   }
   m_iIndexVideoDecodeStats = -1;
}

bool ProcessorRxVideo::init()
{
   if ( m_bInitialized )
      return true;
   m_bInitialized = true;

   log_line("[ProcessorRxVideo] Initialize video processor Rx instance number %d, for VID %u, video stream index %u", m_iInstanceIndex+1, m_uVehicleId, m_uVideoStreamIndex);

   m_uRetryRetransmissionAfterTimeoutMiliseconds = g_pControllerSettings->nRetryRetransmissionAfterTimeoutMS;
   log_line("[ProcessorRxVideo] Using timers: Retransmission retry after timeout of %d ms; Request retransmission after video silence (no video packets) timeout of %d ms", m_uRetryRetransmissionAfterTimeoutMiliseconds, g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs);
      
   fullResetState("init");
  
   log_line("[ProcessorRxVideo] Initialize video processor complete.");
   log_line("[ProcessorRxVideo] ====================================");
   return true;
}

bool ProcessorRxVideo::uninit()
{
   if ( ! m_bInitialized )
      return true;

   log_line("[ProcessorRxVideo] Uninitialize video processor Rx instance number %d for VID %u, video stream index %d", m_iInstanceIndex+1, m_uVehicleId, m_uVideoStreamIndex);
   
   m_bInitialized = false;
   return true;
}

void ProcessorRxVideo::resetReceiveState()
{
   log_line("[ProcessorRxVideo] Start: Reset video RX state and buffers");
   
   controller_debug_video_rt_info_init(&g_SMControllerDebugVideoRTInfo);

   m_uRetryRetransmissionAfterTimeoutMiliseconds = g_pControllerSettings->nRetryRetransmissionAfterTimeoutMS;
   m_uTimeIntervalMsForRequestingRetransmissions = DEFAULT_RETRANSMISSION_MIN_REQUEST_INTERVAL_MS;

   log_line("[ProcessorRxVideo] Using timers: Retransmission retry after timeout of %d ms; Request retransmission after video silence (no video packets) timeout of %d ms", m_uRetryRetransmissionAfterTimeoutMiliseconds, g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs);
   
   if ( NULL != m_pVideoRxBuffer )
      m_pVideoRxBuffer->emptyBuffers("Reset receiver state.");

   m_uTimeLastVideoStreamChanged = g_TimeNow;

   m_uLastTimeCheckedForMissingPackets = g_TimeNow;
   m_uLastTopBlockIdRequested = MAX_U32;
   m_iMaxRecvPacketTopBlockWhenRequested = -1;

   m_uRequestRetransmissionUniqueId = 0;
   m_uLastVideoBlockIndexResolutionChange = 0;
   m_uLastVideoBlockPacketIndexResolutionChange = 0;
   memset(&m_NewestReceivedVideoPacketInfo, 0, sizeof(t_packet_header_video_segment));
   memset(&m_CopyNewestReceivedVideoRxBlockInfo, 0, sizeof(type_rx_video_block_info));
   m_uNewestReceivedVideoPacketTime = 0;
   m_uLastTimeActivated = g_TimeNow;

   log_line("[ProcessorRxVideo] End: Reset video RX state and buffers");
   log_line("--------------------------------------------------------");
   log_line("");
}

void ProcessorRxVideo::_resetOutputState()
{
   log_line("[ProcessorRxVideo] Reset output state.");
   memset(&m_LastOutputedVideoPacketInfo, 0, sizeof(t_packet_header_video_segment));
   memset(&m_CopyLastOutputedVideoRxBlockInfo, 0, sizeof(type_rx_video_block_info));
   m_uTimeLastOutputedVideoPacket = 0;
   m_uTimeReceivedLastOutputedVideoPacket = 0;
   rx_video_output_discard_cached_data();
}

void ProcessorRxVideo::fullResetState(const char* szReason)
{
   if ( NULL != szReason )
      log_line("[ProcessorRxVideo] VID %d, video stream %u: Reset state, full, due to: (%s)", m_uVehicleId, m_uVideoStreamIndex, szReason);
   else
      log_line("[ProcessorRxVideo] VID %d, video stream %u: Reset state, full, due to unknown reason.", m_uVehicleId, m_uVideoStreamIndex);
   resetReceiveState();
   _resetOutputState();
   m_uLastTimeActivated = g_TimeNow;
}

void ProcessorRxVideo::onControllerSettingsChanged()
{
   log_line("[ProcessorRxVideo] VID %u, video stream %u: Controller params changed. Reinitializing RX video state...", m_uVehicleId, m_uVideoStreamIndex);

   m_uRetryRetransmissionAfterTimeoutMiliseconds = g_pControllerSettings->nRetryRetransmissionAfterTimeoutMS;
   log_line("[ProcessorRxVideo]: Using timers: Retransmission retry after timeout of %d ms; Request retransmission after video silence (no video packets) timeout of %d ms", m_uRetryRetransmissionAfterTimeoutMiliseconds, g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs);
   fullResetState("controller settings changed");
}

void ProcessorRxVideo::pauseProcessing()
{
   m_bPaused = true;
   log_line("[ProcessorRxVideo] VID %u, video stream %u: paused processing.", m_uVehicleId, m_uVideoStreamIndex);
   m_uLastTimeActivated = 0;
}

void ProcessorRxVideo::resumeProcessing()
{
   m_bPaused = false;
   log_line("[ProcessorRxVideo] VID %u, video stream %u: resumed processing.", m_uVehicleId, m_uVideoStreamIndex);
   m_uLastTimeActivated = g_TimeNow;
}      

void ProcessorRxVideo::setMustParseStream(bool bParse)
{
   m_bMustParseStream = bParse;

   if ( m_bMustParseStream )
   {
      m_ParserH264.init();
      shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, m_uVehicleId); 
      reset_video_stream_stats_detected_info(pSMVideoStreamInfo);
      log_line("[ProcessorRxVideo] Was set to parse the received video stream.");
   }
}

bool ProcessorRxVideo::isParsingStream()
{
   return m_bMustParseStream;
}

void ProcessorRxVideo::checkAndDiscardBlocksTooOld()
{
   if ( (NULL == m_pVideoRxBuffer) || (0 == m_pVideoRxBuffer->getCountBlocksInBuffer()) )
      return;

   // Discard blocks that are too old, past retransmission window
   u32 uCutoffTime = g_TimeNow - m_iMilisecondsMaxRetransmissionWindow;
   int iCountSkipped = m_pVideoRxBuffer->discardOldBlocks(uCutoffTime);

   if ( iCountSkipped > 0 )
   {
      log_line("[ProcessorRxVideo] Discarded %d blocks too old (at least %d ms old), last successfull missing packets check for retransmission: %u ms ago",
          iCountSkipped, m_iMilisecondsMaxRetransmissionWindow, g_TimeNow - m_uLastTimeCheckedForMissingPackets );
      log_line("[ProcessorRxVideo] When discarded block, last received video block packet was [f%d pckt %u/%u eof %d], fr pckt %d of %d, received %u ms ago, last outputed video block packet was [f%d pkt %u/%u] fr pkt %d of %d, outputed %u ms ago and received %u ms ago",
         m_NewestReceivedVideoPacketInfo.uH264FrameIndex, m_NewestReceivedVideoPacketInfo.uCurrentBlockIndex,
         m_NewestReceivedVideoPacketInfo.uCurrentBlockPacketIndex, m_NewestReceivedVideoPacketInfo.uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_MASK_EOF_COUNTER,
         m_NewestReceivedVideoPacketInfo.uFramePacketsInfo & 0xFF, m_NewestReceivedVideoPacketInfo.uFramePacketsInfo >> 8,
         g_TimeNow - m_uNewestReceivedVideoPacketTime,
         m_LastOutputedVideoPacketInfo.uH264FrameIndex, m_LastOutputedVideoPacketInfo.uCurrentBlockIndex, m_LastOutputedVideoPacketInfo.uCurrentBlockPacketIndex,
         m_LastOutputedVideoPacketInfo.uFramePacketsInfo & 0xFF, m_LastOutputedVideoPacketInfo.uFramePacketsInfo >> 8,

         g_TimeNow - m_uTimeLastOutputedVideoPacket,
         g_TimeNow - m_uTimeReceivedLastOutputedVideoPacket);

      g_SMControllerRTInfo.uOutputedVideoBlocksSkippedBlocks[g_SMControllerRTInfo.iCurrentIndex] += iCountSkipped;
      if ( g_TimeNow > g_TimeLastVideoParametersOrProfileChanged + 3000 )
      if ( g_TimeNow > g_TimeStart + 5000 )
         g_SMControllerRTInfo.uTotalCountOutputSkippedBlocks++;
   }
}

u32 ProcessorRxVideo::getLastActivationTime()
{
   return m_uLastTimeActivated;
}

u32 ProcessorRxVideo:: getLastRetransmissionId()
{
    return m_uRequestRetransmissionUniqueId;
}

u32 ProcessorRxVideo::getLastTimeRequestedRetransmission()
{
   return m_uLastTimeRequestedRetransmission;
}

u32 ProcessorRxVideo::getLastTimeReceivedRetransmission()
{
   return m_uLastTimeReceivedRetransmission;
}

u32 ProcessorRxVideo::getLastTimeVideoStreamChanged()
{
   return m_uTimeLastVideoStreamChanged;
}

u32 ProcessorRxVideo::getLastestVideoPacketReceiveTime()
{
   return m_uNewestReceivedVideoPacketTime;
}

int ProcessorRxVideo::getVideoWidth()
{
   int iVideoWidth = 0;
   if ( m_iIndexVideoDecodeStats != -1 )
   {
      if ( (0 != g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoWidth) && (0 != g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoHeight) )
         iVideoWidth = g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoWidth;
   }
   else
   {
      Model* pModel = findModelWithId(m_uVehicleId, 177);
      if ( NULL != pModel )
         iVideoWidth = pModel->video_params.iVideoWidth;
   }
   return iVideoWidth;
}

int ProcessorRxVideo::getVideoHeight()
{
   int iVideoHeight = 0;
   if ( m_iIndexVideoDecodeStats != -1 )
   {
      if ( (0 != g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoWidth) && (0 != g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoHeight) )
         iVideoHeight = g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoHeight;
   }
   else
   {
      Model* pModel = findModelWithId(m_uVehicleId, 177);
      if ( NULL != pModel )
         iVideoHeight = pModel->video_params.iVideoHeight;
   }
   return iVideoHeight;
}

int ProcessorRxVideo::getVideoFPS()
{
   int iFPS = 0;
   if ( -1 != m_iIndexVideoDecodeStats )
      iFPS = g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoTxSourceFPS;
   if ( 0 == iFPS )
   {
      Model* pModel = findModelWithId(m_uVehicleId, 177);
      if ( NULL != pModel )
         iFPS = pModel->video_params.iVideoFPS;
   }
   return iFPS;
}

int ProcessorRxVideo::getVideoType()
{
   int iVideoType = 0;
   if ( (-1 != m_iIndexVideoDecodeStats ) &&
        (0 != ((g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS.uVideoStreamIndexAndType >> 4) & 0x0F) ) )
      iVideoType = (g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS.uVideoStreamIndexAndType >> 4) & 0x0F;
   else
   {
      Model* pModel = findModelWithId(m_uVehicleId, 177);
      if ( NULL != pModel )
      {
         iVideoType = VIDEO_TYPE_H264;
         if ( pModel->video_params.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265 )
            iVideoType = VIDEO_TYPE_H265;
      }
   }
   return iVideoType;
}

u32 ProcessorRxVideo::getLastTempRetrPauseResume()
{
   return m_uTimeLastResumedTempRetrPause;
}

int ProcessorRxVideo::periodicLoopProcessor(u32 uTimeNow, bool bForceSyncNow)
{
   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(m_uVehicleId);
   Model* pModel = findModelWithId(m_uVehicleId, 155);
   if ( (NULL == pModel) || (NULL == pRuntimeInfo) || (! pRuntimeInfo->bIsPairingDone) )
      return -1;

   controller_runtime_info_vehicle* pCtrlRTInfo = controller_rt_info_get_vehicle_info(&g_SMControllerRTInfo, m_uVehicleId);
   if ( (NULL != pCtrlRTInfo) && (NULL != m_pVideoRxBuffer) )
      pCtrlRTInfo->iCountBlocksInVideoRxBuffers = m_pVideoRxBuffer->getCountBlocksInBuffer();

   checkUpdateRetransmissionsState();
   return checkAndRequestMissingPackets(bForceSyncNow);
}

void ProcessorRxVideo::handleReceivedVideoRetrPacket(int interfaceNb, u8* pBuffer, int iBufferLength)
{
   if ( m_bPaused )
      return;

   t_packet_header* pPH = (t_packet_header*)pBuffer;
   t_packet_header_video_segment* pPHVS = (t_packet_header_video_segment*) (pBuffer+sizeof(t_packet_header));
   controller_runtime_info_vehicle* pCtrlRTInfo = controller_rt_info_get_vehicle_info(&g_SMControllerRTInfo, pPH->vehicle_id_src);
   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(m_uVehicleId);
   Model* pModel = findModelWithId(m_uVehicleId, 170);

   if ( (NULL == m_pVideoRxBuffer) || (NULL == pRuntimeInfo) || (NULL == pModel) )
      return;

   if ( (!pModel->is_spectator) && (! pRuntimeInfo->bIsPairingDone) )
      return;

   // Discard retransmitted packets that:
   // * Are from before latest video stream resolution change;
   // * Are from before a vehicle restart detected;
   // * We already received the original packet meanwhile
   // Retransmitted packets are sent from vehicle: on controller request or automatically (ie on a missing ACK)

   m_uLastTimeReceivedRetransmission = g_TimeNow;
   pCtrlRTInfo->uCountAckRetransmissions[g_SMControllerRTInfo.iCurrentIndex]++;
   if ( pPHVS->uStreamInfoFlags == VIDEO_STREAM_INFO_FLAG_RETRANSMISSION_ID )
   if ( pPHVS->uStreamInfo == m_uRequestRetransmissionUniqueId )
   {
      u32 uDeltaTime = g_TimeNow - m_uLastTimeRequestedRetransmission;
      controller_rt_info_update_ack_rt_time(&g_SMControllerRTInfo, pPH->vehicle_id_src, g_SM_RadioStats.radio_interfaces[interfaceNb].assignedLocalRadioLinkId, uDeltaTime, 0x04);
   }

   bool bDiscard = false;
   bool bBeforeResChange = false;
   if ( pPHVS->uStreamInfo > m_uRequestRetransmissionUniqueId )
   {
      g_SMControllerRTInfo.uOutputedVideoPacketsRetransmittedDiscarded[g_SMControllerRTInfo.iCurrentIndex]++;
      log_line("[ProcessorRxVideo] Discard retr video pckt [f%d %u/%u eof %d] (part of retr id %u) as it's before retr state reset (oldest video block in video rx buffer: %u, last retr id: %u)",
            pPHVS->uH264FrameIndex, pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex,
            pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_MASK_EOF_COUNTER,
            pPHVS->uStreamInfo, m_pVideoRxBuffer->getBufferBottomVideoBlockIndex(), m_uRequestRetransmissionUniqueId);
      bDiscard = true;
   }
   else if ( (m_pVideoRxBuffer->getBufferBottomIndex() != -1) && (m_pVideoRxBuffer->getBufferBottomVideoBlockIndex() != 0) && (pPHVS->uCurrentBlockIndex < m_pVideoRxBuffer->getBufferBottomVideoBlockIndex()) )
   {
      g_SMControllerRTInfo.uOutputedVideoPacketsRetransmittedDiscarded[g_SMControllerRTInfo.iCurrentIndex]++;
      log_line("[ProcessorRxVideo] Discard retr video pckt [f%d %u/%u eof %d] (part of retr id %u) as it's too old (oldest video block in video rx buffer: %u)",
            pPHVS->uH264FrameIndex, pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex,
            pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_MASK_EOF_COUNTER,
            pPHVS->uStreamInfo, m_pVideoRxBuffer->getBufferBottomVideoBlockIndex());
      bDiscard = true;
   }
   else if ( m_pVideoRxBuffer->hasVideoPacket(pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex) ||
        (pPHVS->uCurrentBlockIndex < m_pVideoRxBuffer->getBufferBottomVideoBlockIndex()) ||
        (pPHVS->uCurrentBlockIndex < m_LastOutputedVideoPacketInfo.uCurrentBlockIndex) )
   {
      g_SMControllerRTInfo.uOutputedVideoPacketsRetransmittedDiscarded[g_SMControllerRTInfo.iCurrentIndex]++;
      log_line("[ProcessorRxVideo] Discard retr video pckt [f%d %u/%u eof %d] (part of retr id %u) as it's already received or outputed.",
            pPHVS->uH264FrameIndex, pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex,
            pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_MASK_EOF_COUNTER,
            pPHVS->uStreamInfo);
      bDiscard = true;
   }

   if ( (0 != m_uLastVideoBlockIndexResolutionChange) && (0 != m_uLastVideoBlockPacketIndexResolutionChange) )
   {
      if ( pPHVS->uCurrentBlockIndex < m_uLastVideoBlockIndexResolutionChange )
      {
         bDiscard = true;
         bBeforeResChange = true;
      }
      if ( pPHVS->uCurrentBlockIndex == m_uLastVideoBlockIndexResolutionChange )
      if ( pPHVS->uCurrentBlockPacketIndex < m_uLastVideoBlockPacketIndexResolutionChange )
      {
         bDiscard = true;
         bBeforeResChange = true;
      }
   }
   if ( bDiscard && bBeforeResChange )
      log_line("[ProcessorRxVideo] Discard retr video pkt [f%d %u/%u eof %d] [eof in %d packets] (part of retr id %u) as it's received from before a video resolution change (change happened at video block index [%u/%u])",
            pPHVS->uH264FrameIndex, pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex,
            pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_MASK_EOF_COUNTER,
            pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_MASK_EOF_COUNTER,
            pPHVS->uStreamInfo, m_uLastVideoBlockIndexResolutionChange, m_uLastVideoBlockPacketIndexResolutionChange);
   if ( bDiscard )
   {
      log_line("[ProcessorRxVideo] At discard time, last received video packet was: [f%d %d/%d eof %d] received %u ms ago",
         m_NewestReceivedVideoPacketInfo.uH264FrameIndex, m_NewestReceivedVideoPacketInfo.uCurrentBlockIndex, m_NewestReceivedVideoPacketInfo.uCurrentBlockPacketIndex, m_NewestReceivedVideoPacketInfo.uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_MASK_EOF_COUNTER, g_TimeNow - m_uNewestReceivedVideoPacketTime);
      log_line("[ProcessorRxVideo] At discard time, last outputed video frame block packet was: [f%d %d/%d], outputed %u ms ago and received %u ms ago",
         m_LastOutputedVideoPacketInfo.uH264FrameIndex, m_LastOutputedVideoPacketInfo.uCurrentBlockIndex, m_LastOutputedVideoPacketInfo.uCurrentBlockPacketIndex,
         g_TimeNow - m_uTimeLastOutputedVideoPacket,
         g_TimeNow - m_uTimeReceivedLastOutputedVideoPacket);
      return;
   }

   if ( pPHVS->uCurrentBlockIndex == m_uLastTopBlockIdRequested )
   {
      log_line("[ProcessorRxVideo] Recv useful retr top video block pckt [f%d %u/%u] [eof in %d packets] that was requested for retr on retr id %u",
            pPHVS->uH264FrameIndex, pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex,
            pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_MASK_EOF_COUNTER,
            pPHVS->uStreamInfo);
   }
   else
   {
      log_line("[ProcessorRxVideo] Recv useful retr video pckt [f%d %u/%u] [eof in %d packets] that was requested for retr on retr id %u",
            pPHVS->uH264FrameIndex, pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex,
            pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_MASK_EOF_COUNTER,
            pPHVS->uStreamInfo);
   }

   if ( ! m_pVideoRxBuffer->checkAddVideoPacket(pBuffer, iBufferLength) )
      return;

   _checkAndOutputAvailablePackets(pRuntimeInfo, pModel);
}

void ProcessorRxVideo::handleReceivedVideoPacket(int interfaceNb, u8* pBuffer, int iBufferLength)
{
   if ( m_bPaused )
      return;

   t_packet_header* pPH = (t_packet_header*)pBuffer;
   t_packet_header_video_segment* pPHVS = (t_packet_header_video_segment*) (pBuffer+sizeof(t_packet_header));
   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(m_uVehicleId);
   Model* pModel = findModelWithId(m_uVehicleId, 170);

   if ( (NULL == m_pVideoRxBuffer) || (NULL == pRuntimeInfo) || (NULL == pModel) )
      return;

   if ( (!pModel->is_spectator) && (! pRuntimeInfo->bIsPairingDone) )
      return;

   // Check for video stream restart before discarding this packet (if not needed)

   if ( (m_NewestReceivedVideoPacketInfo.uCurrentBlockIndex > 50) && (pPHVS->uCurrentBlockIndex < m_NewestReceivedVideoPacketInfo.uCurrentBlockIndex - 50) )
   {
      log_line("[ProcessorRxVideo] Video stream restart detected: received video block %u, last newest received video block was: %u",
         pPHVS->uCurrentBlockIndex, m_NewestReceivedVideoPacketInfo.uCurrentBlockIndex);
      fullResetState("vehicle video stream restarted");
   }

   // Discard packet if it's too old (older than last outputed packet) and so it's not needed
   if ( (pPHVS->uCurrentBlockIndex < m_LastOutputedVideoPacketInfo.uCurrentBlockIndex) ||
        ((m_uTimeLastOutputedVideoPacket != 0) &&
         (pPHVS->uCurrentBlockIndex == m_LastOutputedVideoPacketInfo.uCurrentBlockIndex) &&
         (pPHVS->uCurrentBlockPacketIndex <= m_LastOutputedVideoPacketInfo.uCurrentBlockPacketIndex)) )
      return;

   // Save info about it

   bool bIsNewest = false;
   if ( (pPHVS->uCurrentBlockIndex > m_NewestReceivedVideoPacketInfo.uCurrentBlockIndex) )
      bIsNewest = true;
   if ( pPHVS->uCurrentBlockIndex == m_NewestReceivedVideoPacketInfo.uCurrentBlockIndex )
   if ( pPHVS->uCurrentBlockPacketIndex >= m_NewestReceivedVideoPacketInfo.uCurrentBlockPacketIndex )
      bIsNewest = true;
   if ( bIsNewest )
   {
      memcpy(&m_NewestReceivedVideoPacketInfo, pPHVS, sizeof(t_packet_header_video_segment));
      m_uNewestReceivedVideoPacketTime = g_TimeNow;
      updateControllerRTInfoAndVideoDecodingStats(pBuffer, iBufferLength);
   }

   if ( pPHVS->uCurrentBlockPacketIndex >= pPHVS->uCurrentBlockDataPackets )
      g_SMControllerRTInfo.uRxVideoECPackets[g_SMControllerRTInfo.iCurrentIndex][0]++;
   else
      g_SMControllerRTInfo.uRxVideoPackets[g_SMControllerRTInfo.iCurrentIndex][0]++;

   if ( pPHVS->uCurrentBlockIndex == m_uLastTopBlockIdRequested )
   {
      log_line("[ProcessorRxVideo] Recv org packet from top video block [%u/%u] [eof in %d packets] after it was requested for retr.",
         pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex,
         pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_MASK_EOF_COUNTER);
   }

   if ( m_bPauseTempRetrUntillANewVideoPacket )
   {
      m_bPauseTempRetrUntillANewVideoPacket = false;
      m_uTimeLastResumedTempRetrPause = g_TimeNow;
      log_line("[ProcessorRxVideo] Cleared flag to temporarly pause retransmissions");
      adaptive_video_reset_time_for_vehicle(m_uVehicleId);
   }

   #if defined(RUBY_BUILD_HW_PLATFORM_PI)
   if (((pPHVS->uVideoStreamIndexAndType >> 4) & 0x0F) == VIDEO_TYPE_H265 )
   {
      static u32 s_uTimeLastSendVideoUnsuportedAlarmToCentral = 0;
      if ( g_TimeNow > s_uTimeLastSendVideoUnsuportedAlarmToCentral + 20000 )
      {
         s_uTimeLastSendVideoUnsuportedAlarmToCentral = g_TimeNow;
         send_alarm_to_central(ALARM_ID_UNSUPPORTED_VIDEO_TYPE, pPHVS->uVideoStreamIndexAndType, pPH->vehicle_id_src);
      }
   }
   #endif


   if ( ! m_pVideoRxBuffer->checkAddVideoPacket(pBuffer, iBufferLength) )
   {
      if ( bIsNewest )
         memcpy(&m_CopyNewestReceivedVideoRxBlockInfo, m_pVideoRxBuffer->getTopBlockInBuffer(), sizeof(type_rx_video_block_info));
      return;
   }
   if ( bIsNewest )
      memcpy(&m_CopyNewestReceivedVideoRxBlockInfo, m_pVideoRxBuffer->getTopBlockInBuffer(), sizeof(type_rx_video_block_info));

   _checkAndOutputAvailablePackets(pRuntimeInfo, pModel);
}

void ProcessorRxVideo::_checkAndOutputAvailablePackets(type_global_state_vehicle_runtime_info* pRuntimeInfo, Model* pModel)
{
   // If one way link, or retransmissions are off, 
   //    or spectator mode, or not paired yet, or test link is in progress
   //    or negociate radio link is in progress,
   //    or vehicle has lost link to controller,
   //
   // then skip blocks, if there are more video blocks with gaps in buffer

   checkUpdateRetransmissionsState(); // updates bIsDoingRetransmissions state

   bool bSkipMissing = (! pRuntimeInfo->bIsDoingRetransmissions) ||
         ((pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ONE_WAY_FIXED_VIDEO)?true:false);
   bool bWaitFullFrame = (g_pControllerSettings->iWaitFullFrameForOutput?true:false) && (! bSkipMissing);

   type_rx_video_block_info* pVideoBlock = NULL;
   type_rx_video_packet_info* pVideoPacket = NULL;

   while ( m_pVideoRxBuffer->getCountBlocksInBuffer() != 0 )
   {
      while ( bSkipMissing )
      {
         if ( m_pVideoRxBuffer->discardBottomBlockIfIncomplete() )
         {
            g_SMControllerRTInfo.uOutputedVideoBlocksSkippedBlocks[g_SMControllerRTInfo.iCurrentIndex]++;
            if ( g_TimeNow > g_TimeLastVideoParametersOrProfileChanged + 3000 )
            if ( g_TimeNow > g_TimeStart + 5000 )
               g_SMControllerRTInfo.uTotalCountOutputSkippedBlocks++;
         }
         else
            break;
      }

      if ( m_pVideoRxBuffer->getCountBlocksInBuffer() == 0 )
         break;

      pVideoPacket = m_pVideoRxBuffer->getBottomBlockAndPacketInBuffer(&pVideoBlock);
      
      // Reached an empty packet?
      if ( (0 == pVideoBlock->uReceivedTime) || pVideoPacket->bEmpty )
            break;

      // Output and advance to next video packet, even if empty

      processAndOutputVideoPacket(pVideoBlock, pVideoPacket, bWaitFullFrame);
      m_pVideoRxBuffer->advanceBottomPacketInBuffer();
   }
}

void ProcessorRxVideo::processAndOutputVideoPacket(type_rx_video_block_info* pVideoBlock, type_rx_video_packet_info* pVideoPacket, bool bWaitFullFrame)
{
   t_packet_header_video_segment* pPHVS = pVideoPacket->pPHVS;
   t_packet_header_video_segment_important* pPHVSImp = pVideoPacket->pPHVSImp;
   u8* pVideoRawStreamData = pVideoPacket->pVideoData;
   pVideoRawStreamData += sizeof(t_packet_header_video_segment_important);


   if ( g_pControllerSettings->iEnableDebugStats ||
        ((NULL != g_pCurrentModel) && (g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_FLAG2_SHOW_VIDEO_FRAMES_STATS)) )
      _updateDebugStatsOnVideoPacket(pVideoPacket);

   memcpy(&m_LastOutputedVideoPacketInfo, pPHVS, sizeof(t_packet_header_video_segment));
   memcpy(&m_CopyLastOutputedVideoRxBlockInfo, pVideoBlock, sizeof(type_rx_video_block_info));
   m_uTimeLastOutputedVideoPacket = g_TimeNow;
   m_uTimeReceivedLastOutputedVideoPacket = pVideoPacket->uReceivedTime;

   int iVideoWidth = getVideoWidth();
   int iVideoHeight = getVideoHeight();

   shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, m_uVehicleId); 
   bool bMustParseStream = false;
   if ( NULL != pSMVideoStreamInfo )
   if ( (pSMVideoStreamInfo->iDetectedFPS <= 0) || (pSMVideoStreamInfo->iDetectedSlices <= 0) || (pSMVideoStreamInfo->iDetectedKeyframeMs <= 0) )
      bMustParseStream = true;
   if ( NULL != pSMVideoStreamInfo )
   if ( (0 == pSMVideoStreamInfo->uDetectedH264Profile) || (0 == pSMVideoStreamInfo->uDetectedH264Level) )
      bMustParseStream = true;

   if ( ! bMustParseStream )
   {
      if ( m_bMustParseStream )
         log_line("[ProcessorRxVideo] Finished parsing the received video stream.");
      m_bMustParseStream = false;
   }

   if ( m_bMustParseStream || bMustParseStream )
   {
      if ( ! m_bWasParsingStream )
      {
         log_line("[ProcessorRxVideo] Started parsing the received video stream.");
         m_ParserH264.init();
      }
      m_bWasParsingStream = true;
      m_ParserH264.parseData(pVideoRawStreamData, pPHVSImp->uVideoDataLength, g_TimeNow);

      shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, m_uVehicleId); 
      if ( NULL != pSMVideoStreamInfo )
      {
         pSMVideoStreamInfo->uDetectedH264Profile = m_ParserH264.getDetectedProfile();
         pSMVideoStreamInfo->uDetectedH264ProfileConstrains = m_ParserH264.getDetectedProfileConstrains();
         pSMVideoStreamInfo->uDetectedH264Level = m_ParserH264.getDetectedLevel();
      
         if ( pSMVideoStreamInfo->iDetectedFPS <= 0 )
         {
            pSMVideoStreamInfo->iDetectedFPS = m_ParserH264.getDetectedFPS();
            if ( 0 != pSMVideoStreamInfo->iDetectedFPS )
               log_line("[ProcessorRxVideo] Detected video stream FPS: %d FPS", pSMVideoStreamInfo->iDetectedFPS);
         }
         if ( pSMVideoStreamInfo->iDetectedSlices <= 0 )
         {
            pSMVideoStreamInfo->iDetectedSlices = m_ParserH264.getDetectedSlices();
            if ( 0 != pSMVideoStreamInfo->iDetectedSlices )
               log_line("[ProcessorRxVideo] Detected video stream slices: %d slices", pSMVideoStreamInfo->iDetectedSlices);
         }

         if ( pSMVideoStreamInfo->iDetectedKeyframeMs <= 0 )
         {
             pSMVideoStreamInfo->iDetectedKeyframeMs = m_ParserH264.getDetectedKeyframeIntervalMs();
             if ( 0 != pSMVideoStreamInfo->iDetectedKeyframeMs )
                log_line("[ProcessorRxVideo] Detected video stream KF interval: %d ms", pSMVideoStreamInfo->iDetectedKeyframeMs);
         }
      }
   }
   else
   {
      if ( m_bWasParsingStream )
         log_line("[ProcessorRxVideo] Stopped parsing the received video stream.");
      m_bWasParsingStream = false;
   }

   rx_video_output_video_data(m_uVehicleId, pVideoPacket->pPHVS, iVideoWidth, iVideoHeight, pVideoRawStreamData, pPHVSImp->uVideoDataLength, pVideoPacket->pPH->total_length, bWaitFullFrame);

   // Update controller stats

   g_SMControllerRTInfo.uOutputedVideoPackets[g_SMControllerRTInfo.iCurrentIndex]++;
   if ( pVideoPacket->pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
      g_SMControllerRTInfo.uOutputedVideoPacketsRetransmitted[g_SMControllerRTInfo.iCurrentIndex]++;
   
   static u32 s_uLastOutputedVideoBlockId = 0;
   if ( s_uLastOutputedVideoBlockId != pVideoBlock->uVideoBlockIndex )
   {
      s_uLastOutputedVideoBlockId = pVideoBlock->uVideoBlockIndex;
      g_SMControllerRTInfo.uOutputedVideoBlocks[g_SMControllerRTInfo.iCurrentIndex]++;
   }

   static u32 s_uLastOutputedVideoBlockIdReconstructed = 0;
   if ( pVideoPacket->bReconstructed )
   if ( s_uLastOutputedVideoBlockIdReconstructed != pVideoBlock->uVideoBlockIndex )
   {
      s_uLastOutputedVideoBlockIdReconstructed = pVideoBlock->uVideoBlockIndex;
      g_SMControllerRTInfo.uOutputedVideoBlocksECUsed[g_SMControllerRTInfo.iCurrentIndex]++;

      if ( (pPHVS->uCurrentBlockECPackets > 0) &&
           (pVideoBlock->iReconstructedECUsed >= pPHVS->uCurrentBlockECPackets) )
      {
         g_SMControllerRTInfo.uOutputedVideoBlocksMaxECUsed[g_SMControllerRTInfo.iCurrentIndex]++;
         g_SMControllerDebugVideoRTInfo.uOutputFramePackets[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] |= ((u32)0x01)<<24;
      }   
      
      if ( pVideoBlock->iReconstructedECUsed == 1 )
         g_SMControllerRTInfo.uOutputedVideoBlocksSingleECUsed[g_SMControllerRTInfo.iCurrentIndex]++;
      else if ( pVideoBlock->iReconstructedECUsed == 2 )
         g_SMControllerRTInfo.uOutputedVideoBlocksTwoECUsed[g_SMControllerRTInfo.iCurrentIndex]++;
      else
         g_SMControllerRTInfo.uOutputedVideoBlocksMultipleECUsed[g_SMControllerRTInfo.iCurrentIndex]++;
      pVideoBlock->iReconstructedECUsed = 0;
   }
}

void ProcessorRxVideo::updateControllerRTInfoAndVideoDecodingStats(u8* pRadioPacket, int iPacketLength)
{
   if ( (m_iIndexVideoDecodeStats < 0) || (m_iIndexVideoDecodeStats >= MAX_VIDEO_PROCESSORS) )
      return;
   t_packet_header_video_segment* pPHVS = (t_packet_header_video_segment*) (pRadioPacket+sizeof(t_packet_header));    
   Model* pModel = findModelWithId(m_uVehicleId, 179);

   if ( g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS.uCurrentVideoLinkProfile != pPHVS->uCurrentVideoLinkProfile )
   {
      // Video profile changed on the received stream
      // To fix may2025
      /*
      if ( pPHVS->uCurrentVideoLinkProfile == VIDEO_PROFILE_MQ ||
           pPHVS->uCurrentVideoLinkProfile == VIDEO_PROFILE_LQ ||
           g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS.uCurrentVideoLinkProfile == VIDEO_PROFILE_MQ ||
           g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS.uCurrentVideoLinkProfile == VIDEO_PROFILE_LQ )
      {
         if ( pPHVS->uCurrentVideoLinkProfile > g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS.uCurrentVideoLinkProfile )
            g_SMControllerRTInfo.uFlagsAdaptiveVideo[g_SMControllerRTInfo.iCurrentIndex] |= CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCHED_LOWER;
         else
            g_SMControllerRTInfo.uFlagsAdaptiveVideo[g_SMControllerRTInfo.iCurrentIndex] |= CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCHED_HIGHER;
      }
      else
         g_SMControllerRTInfo.uFlagsAdaptiveVideo[g_SMControllerRTInfo.iCurrentIndex] |= CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCHED_USER_SELECTABLE;
      */
   }

   memcpy( &(g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS), pPHVS, sizeof(t_packet_header_video_segment));

   if ( g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uLastSetVideoBitrate != pPHVS->uCurrentVideoBitrateBPS )
      log_line("[ProcessorRxVideo] Detected video stream set bitrate changed. From %.3f Mbps to %.3f Mbps", (float)g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uLastSetVideoBitrate/1000.0/1000.0, (float)pPHVS->uCurrentVideoBitrateBPS/1000.0/1000.0);
   g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uLastSetVideoBitrate = pPHVS->uCurrentVideoBitrateBPS;

   if ( pPHVS->uStreamInfoFlags == VIDEO_STREAM_INFO_FLAG_SIZE )
   if ( 0 != pPHVS->uCurrentBlockIndex )
   {
      if ( (g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoWidth != (int)(pPHVS->uStreamInfo & 0xFFFF)) ||
           (g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoHeight != (int)((pPHVS->uStreamInfo >> 16) & 0xFFFF)) )
      {
          m_uLastVideoBlockIndexResolutionChange = pPHVS->uCurrentBlockIndex;
          m_uLastVideoBlockPacketIndexResolutionChange = pPHVS->uCurrentBlockPacketIndex;
      }
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoWidth = pPHVS->uStreamInfo & 0xFFFF;
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoHeight = (pPHVS->uStreamInfo >> 16) & 0xFFFF;
   }
   if ( pPHVS->uStreamInfoFlags == VIDEO_STREAM_INFO_FLAG_FPS )
   {
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoTxSourceFPS = pPHVS->uStreamInfo;
   }
   if ( pPHVS->uStreamInfoFlags == VIDEO_STREAM_INFO_FLAG_EC_TX_TIME )
   if ( NULL != pModel )
   {
      if ( is_sw_version_atleast(pModel, 11, 6) )
      {
         g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uCurrentECTimeMsPerSec = (pPHVS->uStreamInfo & 0xFFFF);
         g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uCurrentTxTimeMsPerSec = ((pPHVS->uStreamInfo >> 16) & 0xFFFF);
      }
      else
      {
         g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uCurrentECTimeMsPerSec = (pPHVS->uStreamInfo & 0xFFFF)/1000;
         g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uCurrentTxTimeMsPerSec = 0;
      }
   }
   if ( pPHVS->uStreamInfoFlags == VIDEO_STREAM_INFO_FLAG_VIDEO_PROFILE_FLAGS )
   {
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uCurrentVideoProfileEncodingFlags = pPHVS->uStreamInfo;
   }

   if ( pPHVS->uStreamInfoFlags == VIDEO_STREAM_INFO_FLAG_SET_KF_MS )
   {
      if ( g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uLastSetVideoKeyframeMs != pPHVS->uStreamInfo )
         log_line("[ProcessorRxVideo] Detected video stream set keyframe changed. From %u ms to %u ms", g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uLastSetVideoKeyframeMs, pPHVS->uStreamInfo);
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uLastSetVideoKeyframeMs = pPHVS->uStreamInfo;
   }
}

void ProcessorRxVideo::_updateDebugStatsOnVideoPacket(type_rx_video_packet_info* pVideoPacket)
{
   u8* pRadioPacket = pVideoPacket->pRawData;
   t_packet_header* pPH = (t_packet_header*)pRadioPacket;
   t_packet_header_video_segment* pPHVS = (t_packet_header_video_segment*) (pRadioPacket+sizeof(t_packet_header));
   int iPacketLength = pPH->total_length;
   
   if ( g_pControllerSettings->iEnableDebugStats )
   {
      if ( pPHVS->uStreamInfoFlags == VIDEO_STREAM_INFO_FLAG_LAST_FRAME_TIMERS )
      {
         int iPrevIndex = g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex - 1;
         if ( iPrevIndex < 0 )
            iPrevIndex = SYSTEM_RT_INFO_INTERVALS_FRAMES - 1;
         if ( pPHVS->uH264FrameIndex == (g_SMControllerDebugVideoRTInfo.uPreviousReceivedH264Frame + 1) )
         if ( 0 == g_SMControllerDebugVideoRTInfo.uVideoFramesProcessingTimes[iPrevIndex] )
            g_SMControllerDebugVideoRTInfo.uVideoFramesProcessingTimes[iPrevIndex] = pPHVS->uStreamInfo;
      }

      if ( pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_END_OF_FRAME )
      {
         g_SMControllerDebugVideoRTInfo.uOutputFramesInfo[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] |= VIDEO_STATUS_FLAGS2_IS_END_OF_FRAME;
         u32 uDeltaMS = g_TimeNow - g_SMControllerRTInfo.uCurrentSliceStartTime;
         if ( uDeltaMS > 128 )
            uDeltaMS = 128;
         g_SMControllerDebugVideoRTInfo.uOutputFramesInfo[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] &= 0xFFFFFF00;
         g_SMControllerDebugVideoRTInfo.uOutputFramesInfo[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] |= (uDeltaMS & 0xFF);
      }
   }

   if ( g_bSearching || (NULL == g_pCurrentModel) )
      return;

   if ( (! g_pControllerSettings->iEnableDebugStats) && ( !(g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_FLAG2_SHOW_VIDEO_FRAMES_STATS)) )
      return;


   if ( pVideoPacket->bReconstructed )
   {
      u8 uPckts = (g_SMControllerDebugVideoRTInfo.uOutputFramePackets[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] >> 16) & 0xFF;
      uPckts++;
      g_SMControllerDebugVideoRTInfo.uOutputFramePackets[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] = 
        (g_SMControllerDebugVideoRTInfo.uOutputFramePackets[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] & 0xFF00FFFF) | (((u32)uPckts) << 16);
   }

   if ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
   {
      u8 uPckts = (g_SMControllerDebugVideoRTInfo.uOutputFramePackets[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] >> 8) & 0xFF;
      uPckts++;
      g_SMControllerDebugVideoRTInfo.uOutputFramePackets[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] = 
        (g_SMControllerDebugVideoRTInfo.uOutputFramePackets[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] & 0xFFFF00FF) | (((u32)uPckts) << 8);
   }
   else
   {
      u8 uPckts = (g_SMControllerDebugVideoRTInfo.uOutputFramePackets[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex]) & 0xFF;
      uPckts++;
      g_SMControllerDebugVideoRTInfo.uOutputFramePackets[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] = 
        (g_SMControllerDebugVideoRTInfo.uOutputFramePackets[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] & 0xFFFFFF00) | uPckts;
   }

   if  ( g_SMControllerDebugVideoRTInfo.uCurrentReceivedH264Frame == pPHVS->uH264FrameIndex ) 
   {
      g_SMControllerDebugVideoRTInfo.iCurrentFrameRecvBytes += pPHVS->uCurrentBlockPacketSize;
      g_SMControllerDebugVideoRTInfo.uCurrentFrameLastPacketTimeTensMS = get_current_timestamp_ms_tens();
      return;
   }

   // New H264 frame received

   controller_debug_video_rt_info_advance_frame(&g_SMControllerDebugVideoRTInfo, pPHVS->uH264FrameIndex, g_pCurrentModel->video_params.iVideoFPS, g_TimeNow);
   g_SMControllerDebugVideoRTInfo.uCaptureFramesDistanceTimes[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] = pPHVS->uRuntimeMetrics & 0xFF;
   g_SMControllerDebugVideoRTInfo.uReceivedFrameNALFlags[g_SMControllerDebugVideoRTInfo.iCurrentFrameBufferIndex] = (pPHVS->uVideoStatusFlags2 >> 8) & 0xFF;
}

void ProcessorRxVideo::checkUpdateRetransmissionsState()
{
   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(m_uVehicleId);
   if ( NULL == pRuntimeInfo )
      return;

   bool bRetransmissionsState = pRuntimeInfo->bIsDoingRetransmissions;
   _checkUpdateRetransmissionsState();

   if ( bRetransmissionsState != pRuntimeInfo->bIsDoingRetransmissions )
   {
      log_line("[ProcessorRxVideo] Retransmissions state changed from %s to %s", bRetransmissionsState?"on":"off", pRuntimeInfo->bIsDoingRetransmissions?"on":"off");
      if ( pRuntimeInfo->bIsDoingRetransmissions )
         m_uLastTimeActivated = g_TimeNow;
      else
         m_uLastTimeActivated = 0;
   }

   if ( pRuntimeInfo->bIsDoingRetransmissions && (0 == m_uLastTimeActivated) )
      m_uLastTimeActivated = g_TimeNow;
   if ( ! pRuntimeInfo->bIsDoingRetransmissions )
      m_uLastTimeActivated = 0;
}

void ProcessorRxVideo::_checkUpdateRetransmissionsState()
{
   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(m_uVehicleId);
   if ( NULL == pRuntimeInfo )
      return;

   pRuntimeInfo->bIsDoingRetransmissions = false;

   Model* pModel = findModelWithId(m_uVehicleId, 181);
   if ( NULL == pModel )
      return;

   if ( (! pRuntimeInfo->bIsPairingDone) || (g_TimeNow < pRuntimeInfo->uPairingRequestTime + 100) )
      return;
   if ( pModel->isVideoLinkFixedOneWay() || (!(pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS)) )
      return;

   if ( g_bSearching || g_bUpdateInProgress || m_bPaused || pModel->is_spectator || test_link_is_in_progress() || isNegociatingRadioLink() )
      return;

   // Do not request from models older than 11.6
   if ( ! is_sw_version_atleast(pModel, 11, 6) )
      return;

   // If we haven't received any video yet, don't try retransmissions
   if ( (0 == m_uNewestReceivedVideoPacketTime) || (-1 == m_iIndexVideoDecodeStats) )
      return;

   if ( g_TimeNow < g_TimeLastVideoParametersOrProfileChanged + 200 )
      return;

   // If link is lost, do not request retransmissions
   if ( pRuntimeInfo->bIsVehicleFastUplinkFromControllerLost )
      return;

   pRuntimeInfo->bIsDoingRetransmissions = true;
}


int ProcessorRxVideo::checkAndRequestMissingPackets(bool bForceSyncNow)
{
   /*
   static u32 ultchr = 0;
   if ( g_TimeNow > ultchr + 5 )
   {
      ultchr = g_TimeNow;
      log_line("DBG retr check r EOF was %ums ago last recv fr f%d blk %u/%d fr pkt %d of %d; last recv block: f%d blk %u, scheme %d/%d, recv: %d/%d, recon %d",
          g_TimeNow - radio_rx_get_current_frame_end_time(),
         m_NewestReceivedVideoPacketInfo.uH264FrameIndex, m_NewestReceivedVideoPacketInfo.uCurrentBlockIndex, m_NewestReceivedVideoPacketInfo.uCurrentBlockPacketIndex,
         m_NewestReceivedVideoPacketInfo.uFramePacketsInfo & 0xFF, m_NewestReceivedVideoPacketInfo.uFramePacketsInfo >> 8,
         m_CopyNewestReceivedVideoRxBlockInfo.uH264FrameIndex, m_CopyNewestReceivedVideoRxBlockInfo.uVideoBlockIndex,
         m_CopyNewestReceivedVideoRxBlockInfo.iBlockDataPackets, m_CopyNewestReceivedVideoRxBlockInfo.iBlockECPackets,
         m_CopyNewestReceivedVideoRxBlockInfo.iRecvDataPackets, m_CopyNewestReceivedVideoRxBlockInfo.iRecvECPackets,
         m_CopyNewestReceivedVideoRxBlockInfo.iReconstructedECUsed);

      log_line("DBG retr check r EOF was %ums ago last output fr f%d blk %u/%d fr pkt %d of %d recon: %d, last output block: f%d blk %u, scheme %d/%d, recv: %d/%d, recon %d",
          g_TimeNow - radio_rx_get_current_frame_end_time(),
         m_LastOutputedVideoPacketInfo.uH264FrameIndex, m_LastOutputedVideoPacketInfo.uCurrentBlockIndex, m_LastOutputedVideoPacketInfo.uCurrentBlockPacketIndex,
         m_LastOutputedVideoPacketInfo.uFramePacketsInfo & 0xFF, m_LastOutputedVideoPacketInfo.uFramePacketsInfo >> 8,
         (m_NewestReceivedVideoPacketInfo.uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_WAS_RECONSTRUCTED)?1:0,
         m_CopyLastOutputedVideoRxBlockInfo.uH264FrameIndex, m_CopyLastOutputedVideoRxBlockInfo.uVideoBlockIndex,
         m_CopyLastOutputedVideoRxBlockInfo.iBlockDataPackets, m_CopyLastOutputedVideoRxBlockInfo.iBlockECPackets,
         m_CopyLastOutputedVideoRxBlockInfo.iRecvDataPackets, m_CopyLastOutputedVideoRxBlockInfo.iRecvECPackets,
         m_CopyLastOutputedVideoRxBlockInfo.iReconstructedECUsed);
   }
   */

   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(m_uVehicleId);
   Model* pModel = findModelWithId(m_uVehicleId, 179);
   if ( (NULL == pModel) || (NULL == pRuntimeInfo) || g_bUpdateInProgress )
      return -1;

   if ( ! is_sw_version_atleast(pModel, 11, 6) )
      return -1;

   if ( (NULL == m_pVideoRxBuffer) || (0 == m_pVideoRxBuffer->getCountBlocksInBuffer()) )
      return -1;

   if ( rx_video_out_is_stream_output_disabled() )
      return -1;

   if ( m_bPauseTempRetrUntillANewVideoPacket )
      return -1;

   if ( (!bForceSyncNow) && (! router_is_eof()) )
      return -1;
   checkUpdateRetransmissionsState();

   m_iMilisecondsMaxRetransmissionWindow = pModel->getCurrentVideoProfileMaxRetransmissionWindow();

   checkAndDiscardBlocksTooOld();
   if ( 0 == m_pVideoRxBuffer->getCountBlocksInBuffer() )
      return -1;
   if ( ! pRuntimeInfo->bIsDoingRetransmissions )
      return -1;

   if ( adaptive_video_is_paused() )
      return -1;

   // If too much time since we last received a new video packet, then discard the entire rx buffer
   if ( m_iMilisecondsMaxRetransmissionWindow >= 10 + 1500/pModel->video_params.iVideoFPS )
   if ( g_TimeNow >= m_uNewestReceivedVideoPacketTime + m_iMilisecondsMaxRetransmissionWindow - 10 )
   {
      log_line("[ProcessorRxVideo] Discard all video buffer due to no new video packet (last newest recv video packet was %u ms ago) past max retransmission window of %d ms", g_TimeNow - m_uNewestReceivedVideoPacketTime, m_iMilisecondsMaxRetransmissionWindow);
      fullResetState("no video received timeout");
      if ( ! m_bPauseTempRetrUntillANewVideoPacket )
      {
         m_bPauseTempRetrUntillANewVideoPacket = true;
         log_line("[ProcessorRxVideo] Set flag to temporarly pause retransmissions as we have no new video data for %u ms", m_iMilisecondsMaxRetransmissionWindow );
      }
      return -1;
   }

   if ( (!bForceSyncNow) && (0 != g_pControllerSettings->iDisableRetransmissionsAfterControllerLinkLostMiliseconds) )
   {
      u32 uDelta = (u32)g_pControllerSettings->iDisableRetransmissionsAfterControllerLinkLostMiliseconds;
      if ( uDelta > 50 )
      if ( uDelta > (u32)m_iMilisecondsMaxRetransmissionWindow )
      if ( g_TimeNow > pRuntimeInfo->uLastTimeReceivedAckFromVehicle + uDelta )
         return -1;
   }

   if ( (!bForceSyncNow) && (g_TimeNow < m_uLastTimeRequestedRetransmission + m_uTimeIntervalMsForRequestingRetransmissions) )
      return -1;

   int iCountBlocks = m_pVideoRxBuffer->getCountBlocksInBuffer();
   if ( 0 == iCountBlocks )
      return -1;

   type_rx_video_block_info* pVideoBlockDbg = m_pVideoRxBuffer->getBlockInBufferFromBottom(0);
   /*
   log_line("DBG retr check: video buffer has %d blocks to check. Bottom block: f%d blk %u, scheme %d/%d, recv %d/%d pckts, frame has %d pckts, this block has %d to %d f pckts",
       iCountBlocks, pVideoBlockDbg->uH264FrameIndex,
       pVideoBlockDbg->uVideoBlockIndex,
       pVideoBlockDbg->iBlockDataPackets, pVideoBlockDbg->iBlockECPackets,
       pVideoBlockDbg->iRecvDataPackets, pVideoBlockDbg->iRecvECPackets,
       pVideoBlockDbg->iTotalFramePackets,
       pVideoBlockDbg->iFramePacketStart, pVideoBlockDbg->iFramePacketEnd
       );
   */

   if ( g_TimeNow >= m_uLastTimeRequestedRetransmission + m_iMilisecondsMaxRetransmissionWindow )
      m_uTimeIntervalMsForRequestingRetransmissions = DEFAULT_RETRANSMISSION_MIN_REQUEST_INTERVAL_MS;

   // Request all missing packets except current block which is requested only on some cases

   //#define PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS 20
   // params after header:
   //   u32: retransmission request id
   //   u8: video stream index
   //   u8: flags:
   //         bit 0: contains re-requested packets
   //         bit 1: contains request for start of video frame packets at the end
   //         bit 2: contains request for end of video frame packets at the end
   //   u8: number of individual video packets requested
   //   (u32+u8)*n = each (video block index + video packet index) requested
   //   (u16+u8) frame id and frame packets from start to get
   //   (u16+u8) frame id and frame packets to EOF to get

   u32 uTimePrevCheck = m_uLastTimeCheckedForMissingPackets;
   m_uLastTimeCheckedForMissingPackets = g_TimeNow;

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_VIDEO, PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS, STREAM_ID_DATA);
   PH.packet_flags |= PACKET_FLAGS_BIT_HIGH_PRIORITY;
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = m_uVehicleId;
   if ( m_uVehicleId == 0 || m_uVehicleId == MAX_U32 )
   {
      PH.vehicle_id_dest = pModel->uVehicleId;
      log_softerror_and_alarm("[ProcessorRxVideo] Tried to request retransmissions before having received a video packet.");
   }

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   u8* pDataInfo = packet + sizeof(t_packet_header) + sizeof(u32) + 3*sizeof(u8);
   u32 uTopVideoBlockIdInBuffer = 0;
   int iTopVideoBlockPacketIndexInBuffer = -1;
   //u32 uTopVideoBlockLastRecvTime = 0;
   u32 uLastRequestedVideoBlockIndex = 0;
   int iLastRequestedVideoBlockPacketIndex = 0;

   char szBufferBlocks[128];
   szBufferBlocks[0] = 0;

   int iCountPacketsRequested = 0;
   type_rx_video_block_info* pVideoBlock = NULL;
   bool bContainsReRequestedPackets = false;

   for( int i=0; i<iCountBlocks-1; i++ )
   {
      pVideoBlock = m_pVideoRxBuffer->getBlockInBufferFromBottom(i);
      if ( i < 2 )
      {
         if ( 0 != szBufferBlocks[0] )
            strcat(szBufferBlocks, ", ");
         char szTmp[128];
         sprintf(szTmp, "[f%d %u %d/%d pckts of %d/%d, eof %d]", pVideoBlock->uH264FrameIndex, pVideoBlock->uVideoBlockIndex, pVideoBlock->iRecvDataPackets, pVideoBlock->iRecvECPackets, pVideoBlock->iBlockDataPackets, pVideoBlock->iBlockECPackets, pVideoBlock->iLastRecordedEOF);
         strcat(szBufferBlocks, szTmp);
      }

      if ( 0 == (pVideoBlock->iRecvDataPackets + pVideoBlock->iRecvECPackets) )
      {
         memcpy(pDataInfo, &pVideoBlock->uVideoBlockIndex, sizeof(u32));
         pDataInfo += sizeof(u32);
         u8 uPacketIndex = 0xFF;
         memcpy(pDataInfo, &uPacketIndex, sizeof(u8));
         pDataInfo += sizeof(u8);
         iCountPacketsRequested++;
         //log_line("DBG will request full block [%u]", pVideoBlock->uVideoBlockIndex);
         continue;
      }

      int iCountToRequestFromBlock = pVideoBlock->iBlockDataPackets - pVideoBlock->iRecvDataPackets - pVideoBlock->iRecvECPackets;
      if ( pVideoBlock->iBlockDataPackets == 0 )
         iCountToRequestFromBlock = 1;
      if ( iCountToRequestFromBlock <= 0 )
         continue;

      for( int k=0; k<pVideoBlock->iBlockDataPackets+1; k++ )
      {
         if ( NULL == pVideoBlock->packets[k].pRawData )
            continue;
         if ( ! pVideoBlock->packets[k].bEmpty )
            continue;

         if ( 0 != pVideoBlock->packets[k].uRequestedTime )
            bContainsReRequestedPackets = true;
         pVideoBlock->packets[k].uRequestedTime = g_TimeNow;
         uLastRequestedVideoBlockIndex = pVideoBlock->uVideoBlockIndex;
         iLastRequestedVideoBlockPacketIndex = k;
         memcpy(pDataInfo, &pVideoBlock->uVideoBlockIndex, sizeof(u32));
         pDataInfo += sizeof(u32);
         u8 uPacketIndex = k;
         memcpy(pDataInfo, &uPacketIndex, sizeof(u8));
         pDataInfo += sizeof(u8);

         iCountToRequestFromBlock--;
         iCountPacketsRequested++;
         if ( iCountToRequestFromBlock == 0 )
            break;
         if ( iCountPacketsRequested >= DEFAULT_VIDEO_RETRANS_MAX_PCOUNT )
           break;
      }
   
      if ( iCountPacketsRequested >= DEFAULT_VIDEO_RETRANS_MAX_PCOUNT )
        break;
   }

   // Check and handle top block. Frame ended at this point, so request data if missing

   pVideoBlock = m_pVideoRxBuffer->getBlockInBufferFromBottom(iCountBlocks-1);
   int iCountToRequestFromBlock = pVideoBlock->iBlockDataPackets - pVideoBlock->iRecvDataPackets - pVideoBlock->iRecvECPackets;

   m_uLastTopBlockIdRequested = MAX_U32;
   m_iMaxRecvPacketTopBlockWhenRequested = -1;

   if ( (0 != pVideoBlock->iBlockDataPackets) && (pVideoBlock->iMaxReceivedDataOrECPacketIndex >= 0) && (iCountToRequestFromBlock > 0) )
   {
      /*log_line("DBG rx video check top block f%d [%u-%d/%d] for retransmissions, recv %d + %d packets, EOF computed as %u ms ago for frame %d",
        pVideoBlock->uH264FrameIndex, pVideoBlock->uVideoBlockIndex, pVideoBlock->iBlockDataPackets, pVideoBlock->iBlockECPackets,
        pVideoBlock->iRecvDataPackets, pVideoBlock->iRecvECPackets, g_TimeNow - radio_rx_get_current_frame_end_time(), radio_rx_get_current_frame_number());
      */
      uTopVideoBlockIdInBuffer = pVideoBlock->uVideoBlockIndex;
      iTopVideoBlockPacketIndexInBuffer = pVideoBlock->iMaxReceivedDataOrECPacketIndex;
      //uTopVideoBlockLastRecvTime = pVideoBlock->uReceivedTime;

      if ( 0 != szBufferBlocks[0] )
         strcat(szBufferBlocks, ", ");
      char szTmp[128];
      sprintf(szTmp, "t[f%d %u %d/%d pckts of %d/%d, eof %d]", pVideoBlock->uH264FrameIndex, pVideoBlock->uVideoBlockIndex, pVideoBlock->iRecvDataPackets, pVideoBlock->iRecvECPackets, pVideoBlock->iBlockDataPackets, pVideoBlock->iBlockECPackets, pVideoBlock->iLastRecordedEOF);
      strcat(szBufferBlocks, szTmp);

      bool bRequestData = false;

      // Not enough EC packets to reconstruct
      if ( (pVideoBlock->iRecvDataPackets + pVideoBlock->iRecvECPackets) < pVideoBlock->iBlockDataPackets )
      {
         for( int k=0; k<pVideoBlock->iBlockDataPackets; k++ )
         {
            if ( NULL == pVideoBlock->packets[k].pRawData )
               continue;
            if ( ! pVideoBlock->packets[k].bEmpty )
               continue;

            if ( 0 != pVideoBlock->packets[k].uRequestedTime )
               bContainsReRequestedPackets = true;
            pVideoBlock->packets[k].uRequestedTime = g_TimeNow;
            uLastRequestedVideoBlockIndex = pVideoBlock->uVideoBlockIndex;
            iLastRequestedVideoBlockPacketIndex = k;
            memcpy(pDataInfo, &pVideoBlock->uVideoBlockIndex, sizeof(u32));
            pDataInfo += sizeof(u32);
            u8 uPacketIndex = k;
            memcpy(pDataInfo, &uPacketIndex, sizeof(u8));
            pDataInfo += sizeof(u8);

            iCountToRequestFromBlock--;
            iCountPacketsRequested++;
            if ( iCountToRequestFromBlock == 0 )
               break;
            if ( iCountPacketsRequested >= DEFAULT_VIDEO_RETRANS_MAX_PCOUNT )
              break;
         }
         m_uLastTopBlockIdRequested = pVideoBlock->uVideoBlockIndex;
         m_iMaxRecvPacketTopBlockWhenRequested = pVideoBlock->iMaxReceivedDataOrECPacketIndex;
         log_line("[ProcessorRxVideo] Will request top block (%u, scheme %d/%d, %d blocks in Rx buffer, bottom block is: %u) has %d/%d recv data/ec packets, max recv index: %d, last recv packet %u ms ago, last retr check %u ms ago, last check for video rx %u ms ago, will request %d packets.",
             pVideoBlock->uVideoBlockIndex, pVideoBlock->iBlockDataPackets, pVideoBlock->iBlockECPackets,
             iCountBlocks, m_pVideoRxBuffer->getBufferBottomVideoBlockIndex(),
             pVideoBlock->iRecvDataPackets, pVideoBlock->iRecvECPackets, pVideoBlock->iMaxReceivedDataOrECPacketIndex,
             g_TimeNow - pVideoBlock->uReceivedTime, g_TimeNow - uTimePrevCheck, g_TimeNow - router_get_last_time_checked_for_video_packets(),
             iCountPacketsRequested);
      }
   }

   // Do we have full missing blocks at the end of frame?
   pVideoBlock = m_pVideoRxBuffer->getBlockInBufferFromBottom(iCountBlocks-1);
   bool bMissingEnd = false;
   if ( pVideoBlock->iFramePacketEnd < pVideoBlock->iTotalFramePackets-1 )
   {
      //log_line("DBG we have missing full blocks at end of f%d. Blocks in rx-buffer: %d, last block: [%u], frame total packets: %d, last received block end frame packet: %d",
      //   pVideoBlock->uH264FrameIndex, iCountBlocks, pVideoBlock->uVideoBlockIndex, pVideoBlock->iTotalFramePackets, pVideoBlock->iFramePacketEnd);
      bMissingEnd = true;
      memcpy(pDataInfo, &(pVideoBlock->uH264FrameIndex), sizeof(u16));
      pDataInfo += sizeof(u16);
      *pDataInfo = pVideoBlock->iFramePacketEnd + 1;
      pDataInfo++;
   }
   if ( iCountPacketsRequested == 0 )
      return 0;

   u8 uFlags = 0;
   u8 uCount = iCountPacketsRequested;
   m_uRequestRetransmissionUniqueId++;

   if ( bContainsReRequestedPackets )
      uFlags |= 0x01;
   if ( bMissingEnd )
      uFlags |= 0x01<<2;
   memcpy(packet + sizeof(t_packet_header), (u8*)&m_uRequestRetransmissionUniqueId, sizeof(u32));
   memcpy(packet + sizeof(t_packet_header) + sizeof(u32), (u8*)&m_uVideoStreamIndex, sizeof(u8));
   memcpy(packet + sizeof(t_packet_header) + sizeof(u32) + sizeof(u8), (u8*)&uFlags, sizeof(u8));
   memcpy(packet + sizeof(t_packet_header) + sizeof(u32) + 2*sizeof(u8), (u8*)&uCount, sizeof(u8));
   PH.total_length = sizeof(t_packet_header) + sizeof(u32) + 3*sizeof(u8);
   PH.total_length += iCountPacketsRequested*(sizeof(u32) + sizeof(u8)); 
   if ( bMissingEnd )
      PH.total_length += sizeof(u16) + sizeof(u8);

   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));

   if ( g_TimeNow < m_uLastTimeRequestedRetransmission + m_iMilisecondsMaxRetransmissionWindow )
   if ( m_uTimeIntervalMsForRequestingRetransmissions < 10 )
       m_uTimeIntervalMsForRequestingRetransmissions++;

   u32 uLastRetransmissionRequestTime = m_uLastTimeRequestedRetransmission;
   m_uLastTimeRequestedRetransmission = g_TimeNow;

   controller_runtime_info_vehicle* pRTInfo = controller_rt_info_get_vehicle_info(&g_SMControllerRTInfo, m_uVehicleId);
   if ( NULL != pRTInfo )
   {
      pRTInfo->uCountReqRetransmissions[g_SMControllerRTInfo.iCurrentIndex]++;
      if ( pRTInfo->uCountReqRetrPackets[g_SMControllerRTInfo.iCurrentIndex] + uCount > 255 )
         pRTInfo->uCountReqRetrPackets[g_SMControllerRTInfo.iCurrentIndex] = 255;
      else
         pRTInfo->uCountReqRetrPackets[g_SMControllerRTInfo.iCurrentIndex] += uCount;
   }

   pDataInfo = packet + sizeof(t_packet_header) + sizeof(u32) + 3*sizeof(u8);
   u32 uFirstReqBlockIndex =0;
   memcpy(&uFirstReqBlockIndex, pDataInfo, sizeof(u32));
   if ( 1 == iCountPacketsRequested )
      log_line("[ProcessorRxVideo] * Requested retr id %u from vehicle for 1 packet ([%u/%d]) (%s%s), last retr req was %u ms ago, EOF was detected %u ms ago for frame %d",
         m_uRequestRetransmissionUniqueId, uFirstReqBlockIndex, (int)pDataInfo[sizeof(u32)],
         bContainsReRequestedPackets?"has re-requested packets":"no re-requests",
         bMissingEnd?", has missing frame end":"", g_TimeNow - uLastRetransmissionRequestTime, g_TimeNow - radio_rx_get_current_frame_end_time(), radio_rx_get_current_frame_number() );
   else
      log_line("[ProcessorRxVideo] * Requested retr id %u from vehicle for %d packets ([%u/%d]...[%u/%d]) (%s%s), last retr req was %u ms ago, EOF was detected %u ms ago for frame %d",
         m_uRequestRetransmissionUniqueId, iCountPacketsRequested,
         uFirstReqBlockIndex, (int)pDataInfo[sizeof(u32)], uLastRequestedVideoBlockIndex, iLastRequestedVideoBlockPacketIndex,
         bContainsReRequestedPackets?"has re-requested packets":"no re-requests",
         bMissingEnd?", has missing frame end":"", g_TimeNow - uLastRetransmissionRequestTime, g_TimeNow - radio_rx_get_current_frame_end_time(), radio_rx_get_current_frame_number());
   
   log_line("[ProcessorRxVideo] * Video blocks in buffer: %d (%s), top/max video block in buffer: [%u/pkt %d] / [%u/pkt %d], last recv video pkt [f%d %u/%u eof %d], received %u ms ago",
      iCountBlocks, szBufferBlocks, uTopVideoBlockIdInBuffer, iTopVideoBlockPacketIndexInBuffer, m_pVideoRxBuffer->getBufferTopVideoBlockIndex(), m_pVideoRxBuffer->getTopBufferMaxReceivedVideoBlockPacketIndex(),
      m_NewestReceivedVideoPacketInfo.uH264FrameIndex, m_NewestReceivedVideoPacketInfo.uCurrentBlockIndex, m_NewestReceivedVideoPacketInfo.uCurrentBlockPacketIndex, m_NewestReceivedVideoPacketInfo.uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_MASK_EOF_COUNTER, g_TimeNow - m_uNewestReceivedVideoPacketTime);
   //log_line("[ProcessorRxVideo] Max Video block packet received: [%u/%d], top video block last recv time: %u ms ago",
   //   m_pVideoRxBuffer->getBufferTopVideoBlockIndex(), m_pVideoRxBuffer->getTopBufferMaxReceivedVideoBlockPacketIndex(), g_TimeNow - uTopVideoBlockLastRecvTime);

   packets_queue_add_packet_mark_time(&s_QueueRadioPacketsHighPrio, packet);
   return iCountPacketsRequested;
}

void discardRetransmissionsInfoAndBuffersOnLengthyOp()
{
   log_line("[ProcessorRxVideo] Discard all retransmissions info after a lengthy router operation.");
   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( NULL == g_pVideoProcessorRxList[i] )
         break;
      g_pVideoProcessorRxList[i]->resetReceiveState();
   }
}