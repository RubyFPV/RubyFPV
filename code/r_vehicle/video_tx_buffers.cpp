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

#include <math.h>
#include "video_tx_buffers.h"
#include "video_sources.h"
#include "shared_vars.h"
#include "timers.h"
#include "packets_utils.h"
#include "../common/string_utils.h"
#include "../base/hardware_cam_maj.h"
#include "../radio/fec.h"
#include "adaptive_video.h"
#include "processor_tx_video.h"
#include "processor_relay.h"

typedef struct
{
   unsigned int fec_decode_missing_packets_indexes[MAX_TOTAL_PACKETS_IN_BLOCK];
   unsigned int fec_decode_fec_indexes[MAX_TOTAL_PACKETS_IN_BLOCK];
   u8* fec_decode_data_packets_pointers[MAX_TOTAL_PACKETS_IN_BLOCK];
   u8* fec_decode_fec_packets_pointers[MAX_TOTAL_PACKETS_IN_BLOCK];
   unsigned int missing_packets_count;
} type_fec_info;
type_fec_info s_FECDecodeInfo;

int VideoTxPacketsBuffer::m_siVideoBuffersInstancesCount = 0;

u32 s_uTimeTotalFecTimeMicroSec = 0;
u32 s_uTimeFecMsPerSec = 0;
u32 s_uLastTimeFecCalculation = 0;

VideoTxPacketsBuffer::VideoTxPacketsBuffer(int iVideoStreamIndex, int iCameraIndex)
:m_bInitialized(false)
{
   m_iInstanceIndex = m_siVideoBuffersInstancesCount;
   m_siVideoBuffersInstancesCount++;

   m_bOverflowFlag = false;
   m_iVideoStreamIndex = iVideoStreamIndex;
   m_iCameraIndex = iCameraIndex;

   for( int i=0; i<MAX_RXTX_BLOCKS_BUFFER; i++ )
   for( int k=0; k<MAX_TOTAL_PACKETS_IN_BLOCK; k++ )
   {
      m_VideoPackets[i][k].pRawData = NULL;
      m_VideoPackets[i][k].pVideoData = NULL;
      m_VideoPackets[i][k].pPH = NULL;
      m_VideoPackets[i][k].pPHVS = NULL;
      m_VideoPackets[i][k].pPHVSImp = NULL;
      m_VideoPackets[i][k].bEmpty = true;
   }
   m_uCurrentH264FrameIndex = 0;
   m_iCurrentBufferIndexToSend = 0;
   m_iCurrentBufferPacketIndexToSend = 0;
   m_iNextBufferIndexToFill = 0;
   m_iNextBufferPacketIndexToFill = 0;

   m_uCustomECScheme = 0;
   m_uLastAppliedECSchemeDataPackets = 0;
   m_uLastAppliedECSchemeECPackets = 0;
   m_uNextVideoBlockIndexToGenerate = 0;
   m_uNextVideoBlockPacketIndexToGenerate = 0;
   m_uRadioStreamPacketIndex = 0;
   m_iVideoStreamInfoIndex = 0;
   m_iUsableRawVideoDataSize = 0;
   memset(&m_PacketHeaderVideo, 0, sizeof(t_packet_header_video_segment));
   memset(&m_PacketHeaderVideoImportant, 0, sizeof(t_packet_header_video_segment_important));

   m_pLastPacketHeaderVideoFilledIn = &m_PacketHeaderVideo;
   m_pLastPacketHeaderVideoImportantFilledIn = &m_PacketHeaderVideoImportant;
   m_ParserInputH264.init();
   m_uLastFrameTimers = 0;
   m_uLastFrameDistanceMs = 0;
   m_uTempNALPresenceFlags = 0;
   m_uTimeDataAvailable = 0;
   m_pTempVideoFrameBuffer = NULL;
   m_iTempVideoFrameBufferSize = 256000;
   m_iTempVideoBufferFilledBytes = 0;
   m_iCurrentRealFPS = 0;
   m_uLastTimeSentVideoPacketMicros = 0;
   m_uLastSentVideoPacketDurationMicros = 0;
   m_uLastSentVideoPacketDatarateBPS = 0;
   m_uLastSentVideoPacketDatarateMinBPS = 0;
   m_uLastSentVideoPacketDatarateMaxBPS = 0;
   m_uTelemetryVideoBitsPerSec = 0;
   m_uTelemetryTotalVideoBitsPerSec = 0;
   m_uTelemetryVideoTxTimeMsPerSec = 0;
}

VideoTxPacketsBuffer::~VideoTxPacketsBuffer()
{
   uninit();

   for( int i=0; i<MAX_RXTX_BLOCKS_BUFFER; i++ )
   for( int k=0; k<MAX_TOTAL_PACKETS_IN_BLOCK; k++ )
   {
      if ( NULL != m_VideoPackets[i][k].pRawData )
         free(m_VideoPackets[i][k].pRawData);

      m_VideoPackets[i][k].pRawData = NULL;
      m_VideoPackets[i][k].pVideoData = NULL;
      m_VideoPackets[i][k].pPH = NULL;
      m_VideoPackets[i][k].pPHVS = NULL;
      m_VideoPackets[i][k].pPHVSImp = NULL;
      m_VideoPackets[i][k].bEmpty = true;
   }

   if ( NULL != m_pTempVideoFrameBuffer )
      free(m_pTempVideoFrameBuffer);
   m_pTempVideoFrameBuffer = NULL;
   m_siVideoBuffersInstancesCount--;
}

bool VideoTxPacketsBuffer::init(Model* pModel)
{
   if ( m_bInitialized )
      return true;
   if ( NULL == pModel )
   {
      log_softerror_and_alarm("[VideoTxBuffer] Invalid model on init.");
      return false;
   }
   log_line("[VideoTxBuffer] Initialize video Tx buffer instance number %d.", m_iInstanceIndex+1);

   if ( NULL == m_pTempVideoFrameBuffer )
   {
      m_pTempVideoFrameBuffer = (u8*)malloc(m_iTempVideoFrameBufferSize);
      if ( NULL == m_pTempVideoFrameBuffer )
         log_error_and_alarm("[VideoTxBuffer] Failed to allocate %d bytes for video frame buffer", m_iTempVideoFrameBufferSize);
      else
         log_line("[VideoTxBuffer] Allocated %d bytes for video frame buffer.", m_iTempVideoFrameBufferSize);
   }
   m_uNextVideoBlockIndexToGenerate = 0;
   m_uNextVideoBlockPacketIndexToGenerate = 0;
   updateVideoHeader(pModel);
   
   m_iTempVideoBufferFilledBytes = 0;
   m_iNextBufferIndexToFill = 0;
   m_iNextBufferPacketIndexToFill = 0;
   m_iCurrentBufferIndexToSend = 0;
   m_iCurrentBufferPacketIndexToSend = 0;
   m_bInitialized = true;
   m_bOverflowFlag = false;
   m_uLastFrameTimers = 0;
   m_uLastFrameDistanceMs = 0;
   m_iCurrentRealFPS = 0;
   m_uExpectedFrameTransmissionTimeMicros = 0;
   log_line("[VideoTxBuffer] Initialized video Tx buffer instance number %d.", m_iInstanceIndex+1);
   return (NULL != m_pTempVideoFrameBuffer);
}

bool VideoTxPacketsBuffer::uninit()
{
   if ( ! m_bInitialized )
      return true;

   log_line("[VideoTxBuffer] Uninitialize video Tx buffer instance number %d.", m_iInstanceIndex+1);
   
   m_bInitialized = false;
   return true;
}

void VideoTxPacketsBuffer::discardBuffer()
{
   m_uNextVideoBlockIndexToGenerate = 0;
   m_uNextVideoBlockPacketIndexToGenerate = 0;
   
   m_iTempVideoBufferFilledBytes = 0;
   m_iNextBufferIndexToFill = 0;
   m_iNextBufferPacketIndexToFill = 0;
   m_iCurrentBufferIndexToSend = 0;
   m_iCurrentBufferPacketIndexToSend = 0;
   m_uTimeDataAvailable = 0;
   log_line("[VideoTxBuffer] Discarded entire buffer.");
}


void VideoTxPacketsBuffer::_checkAllocatePacket(int iBufferIndex, int iPacketIndex)
{
   if ( (iBufferIndex < 0) || (iBufferIndex >= MAX_RXTX_BLOCKS_BUFFER) || (iPacketIndex < 0) || (iPacketIndex >= MAX_TOTAL_PACKETS_IN_BLOCK) )
      return;
   if ( NULL != m_VideoPackets[iBufferIndex][iPacketIndex].pRawData )
      return;

   u8* pRawData = (u8*)malloc(MAX_PACKET_TOTAL_SIZE);
   if ( NULL == pRawData )
   {
      log_error_and_alarm("Failed to allocate video buffer at index: [%d/%d]", iPacketIndex, iBufferIndex);
      return;
   }
   m_VideoPackets[iBufferIndex][iPacketIndex].pRawData = pRawData;
   m_VideoPackets[iBufferIndex][iPacketIndex].pVideoData = pRawData + sizeof(t_packet_header) + sizeof(t_packet_header_video_segment);
   m_VideoPackets[iBufferIndex][iPacketIndex].pPH = (t_packet_header*)pRawData;
   m_VideoPackets[iBufferIndex][iPacketIndex].pPHVS = (t_packet_header_video_segment*)(pRawData + sizeof(t_packet_header));
   m_VideoPackets[iBufferIndex][iPacketIndex].pPHVSImp = (t_packet_header_video_segment_important*)(pRawData + sizeof(t_packet_header) + sizeof(t_packet_header_video_segment));
   m_VideoPackets[iBufferIndex][iPacketIndex].bEmpty = true;
}

void VideoTxPacketsBuffer::_fillVideoPacketHeaders(int iBufferIndex, int iPacketIndex, bool bIsECPacket, int iRawVideoDataSize, bool bIsLastPacket)
{
   m_VideoPackets[iBufferIndex][iPacketIndex].bEmpty = false;

   //------------------------------------
   // Update packet header

   t_packet_header* pCurrentPacketHeader = m_VideoPackets[iBufferIndex][iPacketIndex].pPH;
   memcpy(pCurrentPacketHeader, &m_PacketHeader, sizeof(t_packet_header));

   pCurrentPacketHeader->total_length = sizeof(t_packet_header)+sizeof(t_packet_header_video_segment) + sizeof(t_packet_header_video_segment_important);
   pCurrentPacketHeader->total_length += iRawVideoDataSize;

   // Add dbg info only to full video packets
   if ( (!bIsECPacket) && (iRawVideoDataSize == m_iUsableRawVideoDataSize) )
   if ( (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE) &&
        (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_STREAM_TIMINGS) )
      pCurrentPacketHeader->total_length += sizeof(t_packet_header_video_segment_debug_info);

   pCurrentPacketHeader->packet_flags_extended = PACKET_FLAGS_EXTENDED_BIT_SEND_ON_HIGH_CAPACITY_LINK_ONLY;

   static u32 s_uLastTimeCheckVideoTotalThroughput = 0;
   static u32 s_uVideoThroughputTotalVideoBytes = 0;
   s_uVideoThroughputTotalVideoBytes += (u32)(pCurrentPacketHeader->total_length);
   if ( g_TimeNow >= s_uLastTimeCheckVideoTotalThroughput + 100 )
   {
      s_uVideoThroughputTotalVideoBytes = ((s_uVideoThroughputTotalVideoBytes*8) / (g_TimeNow - s_uLastTimeCheckVideoTotalThroughput)) * 1000;
      m_uTelemetryTotalVideoBitsPerSec = s_uVideoThroughputTotalVideoBytes;
      s_uLastTimeCheckVideoTotalThroughput = g_TimeNow;
      s_uVideoThroughputTotalVideoBytes = 0;
   }

   //-------------------------------------
   // Update packet header video segment

   // Update last block EC scheme if needed (if this will be a short video block due to end of frame)
   if ( (!bIsECPacket) && bIsLastPacket )
   if ( iPacketIndex < (int)m_PacketHeaderVideo.uCurrentBlockDataPackets-1 )
   {
      float fECRate = (float)m_PacketHeaderVideo.uCurrentBlockECPackets/(float)m_PacketHeaderVideo.uCurrentBlockDataPackets;
      m_PacketHeaderVideo.uCurrentBlockDataPackets = (u8) iPacketIndex + 1;
      float fECPackets = (float)m_PacketHeaderVideo.uCurrentBlockDataPackets * fECRate + 0.001;
      u32 uECPackets = (u32)ceil(fECPackets);

      if ( (fECRate > 0.0001) && (uECPackets == 0) )
         uECPackets = 1;
      if ( uECPackets > m_PacketHeaderVideo.uCurrentBlockDataPackets )
         uECPackets = m_PacketHeaderVideo.uCurrentBlockDataPackets;

      if ( 1 == m_PacketHeaderVideo.uCurrentBlockDataPackets )
         uECPackets = 0;
      m_PacketHeaderVideo.uCurrentBlockECPackets = uECPackets;

      int iPacketPrevIndex = iPacketIndex;
      while ( iPacketPrevIndex >= 0 )
      {
         if ( NULL == m_VideoPackets[iBufferIndex][iPacketPrevIndex].pRawData )
            break;
         m_VideoPackets[iBufferIndex][iPacketPrevIndex].pPHVS->uCurrentBlockDataPackets = iPacketIndex+1;
         m_VideoPackets[iBufferIndex][iPacketPrevIndex].pPHVS->uCurrentBlockECPackets = uECPackets;
         iPacketPrevIndex--;
      }
   }

   m_PacketHeaderVideo.uCurrentVideoBitrateBPS = video_sources_get_last_set_video_bitrate();
   m_PacketHeaderVideo.uCurrentVideoKeyframeIntervalMs = video_sources_get_last_set_keyframe();
   //m_PacketHeaderVideo.uCurrentVideoKeyframeIntervalMs = g_pCurrentModel->getInitialKeyframeIntervalMs(g_pCurrentModel->video_params.iCurrentVideoProfile);

   m_pLastPacketHeaderVideoFilledIn = m_VideoPackets[iBufferIndex][iPacketIndex].pPHVS;
   memcpy(m_pLastPacketHeaderVideoFilledIn, &m_PacketHeaderVideo, sizeof(t_packet_header_video_segment));
   m_pLastPacketHeaderVideoFilledIn->uH264FrameIndex = m_uCurrentH264FrameIndex;
   m_pLastPacketHeaderVideoFilledIn->uCurrentBlockIndex = m_uNextVideoBlockIndexToGenerate;
   m_pLastPacketHeaderVideoFilledIn->uCurrentBlockPacketIndex = m_uNextVideoBlockPacketIndexToGenerate;

   int iVideoProfile = g_pCurrentModel->video_params.iCurrentVideoProfile;
   m_pLastPacketHeaderVideoFilledIn->uCurrentVideoLinkProfile = iVideoProfile;
   
   if ( adaptive_video_is_on_lower_video_bitrate() )
      m_pLastPacketHeaderVideoFilledIn->uVideoStatusFlags2 |= VIDEO_STATUS_FLAGS2_IS_ON_LOWER_BITRATE;
   else
      m_pLastPacketHeaderVideoFilledIn->uVideoStatusFlags2 &= ~VIDEO_STATUS_FLAGS2_IS_ON_LOWER_BITRATE;

   m_pLastPacketHeaderVideoFilledIn->uVideoStatusFlags2 &= ~(VIDEO_STATUS_FLAGS2_IS_NAL_I | VIDEO_STATUS_FLAGS2_IS_NAL_P | VIDEO_STATUS_FLAGS2_IS_NAL_O | VIDEO_STATUS_FLAGS2_IS_NAL_END);
   m_pLastPacketHeaderVideoFilledIn->uVideoStatusFlags2 |= m_uTempNALPresenceFlags;
   
   if ( bIsLastPacket )
      m_pLastPacketHeaderVideoFilledIn->uVideoStatusFlags2 |= VIDEO_STATUS_FLAGS2_IS_NAL_END;

   m_pLastPacketHeaderVideoFilledIn->uRuntimeMetrics = m_uLastFrameDistanceMs & 0xFF;

   // Add dbg info only to full video packets
   m_pLastPacketHeaderVideoFilledIn->uVideoStatusFlags2 &= ~VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS;
   if ( (!bIsECPacket) && (iRawVideoDataSize == m_iUsableRawVideoDataSize) )
   if ( (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE) &&
        (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_STREAM_TIMINGS) )
      m_pLastPacketHeaderVideoFilledIn->uVideoStatusFlags2 |= VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS;

   if ( bIsLastPacket )
   {
      m_pLastPacketHeaderVideoFilledIn->uStreamInfoFlags = (u8)VIDEO_STREAM_INFO_FLAG_LAST_FRAME_TIMERS;
      m_pLastPacketHeaderVideoFilledIn->uStreamInfo = m_uLastFrameTimers;
   }
   else
   {
      m_iVideoStreamInfoIndex++;
      if ( m_iVideoStreamInfoIndex == VIDEO_STREAM_INFO_FLAG_RETRANSMISSION_ID )
         m_iVideoStreamInfoIndex++;
      if ( (m_iVideoStreamInfoIndex >= VIDEO_STREAM_INFO_FLAG_LAST) )
         m_iVideoStreamInfoIndex = VIDEO_STREAM_INFO_FLAG_SIZE;
      m_pLastPacketHeaderVideoFilledIn->uStreamInfo = 0;
      u32 uTmp1, uTmp2;

      switch ( m_iVideoStreamInfoIndex )
      {
         case VIDEO_STREAM_INFO_FLAG_NONE:
             m_pLastPacketHeaderVideoFilledIn->uStreamInfo = 0;
             break;

         case VIDEO_STREAM_INFO_FLAG_SIZE:
             uTmp1 = (u32)g_pCurrentModel->video_params.iVideoWidth;
             uTmp2 = (u32)g_pCurrentModel->video_params.iVideoHeight;
             m_pLastPacketHeaderVideoFilledIn->uStreamInfo = (uTmp1 & 0xFFFF) | ((uTmp2 & 0xFFFF)<<16);
             break;

         case VIDEO_STREAM_INFO_FLAG_FPS:
             m_pLastPacketHeaderVideoFilledIn->uStreamInfo = (u32)m_iCurrentRealFPS;
             break;

         case VIDEO_STREAM_INFO_FLAG_EC_TX_TIME:
             m_pLastPacketHeaderVideoFilledIn->uStreamInfo = (s_uTimeFecMsPerSec & 0xFFFF) | ((m_uTelemetryVideoTxTimeMsPerSec & 0xFFFF)<<16);
             break;

         case VIDEO_STREAM_INFO_FLAG_VIDEO_PROFILE_FLAGS:
             m_pLastPacketHeaderVideoFilledIn->uStreamInfo = g_pCurrentModel->video_link_profiles[iVideoProfile].uProfileEncodingFlags;
             break;

         case VIDEO_STREAM_INFO_FLAG_SET_KF_MS:
             m_pLastPacketHeaderVideoFilledIn->uStreamInfo = video_sources_get_last_set_keyframe();
             break;

         case VIDEO_STREAM_INFO_FLAG_LAST_FRAME_TIMERS:
             m_pLastPacketHeaderVideoFilledIn->uStreamInfo = m_uLastFrameTimers;
             break;

         case VIDEO_STREAM_INFO_FLAG_VIDEO_BYTES_PER_SEC:
             m_pLastPacketHeaderVideoFilledIn->uStreamInfo = (m_uTelemetryVideoBitsPerSec/1000) | ((m_uTelemetryTotalVideoBitsPerSec/1000)<<16);
             break;
      }
      m_pLastPacketHeaderVideoFilledIn->uStreamInfoFlags = (u8)m_iVideoStreamInfoIndex;
   }
   if ( bIsECPacket )
      return;
   
   //---------------------------------------------
   // Update packet header video segment important

   m_pLastPacketHeaderVideoImportantFilledIn = m_VideoPackets[iBufferIndex][iPacketIndex].pPHVSImp;
   memcpy(m_pLastPacketHeaderVideoImportantFilledIn, &m_PacketHeaderVideoImportant, sizeof(t_packet_header_video_segment_important));
   m_pLastPacketHeaderVideoImportantFilledIn->uVideoDataLength = (u16)iRawVideoDataSize;
   m_pLastPacketHeaderVideoImportantFilledIn->uVideoImportantFlags = 0;
}

void VideoTxPacketsBuffer::setLastFrameTimers(u32 uTimeReadCamera, u32 uTimeSendVideo, u32 uTimeExpectedSendVideoTime, u32 uTimeSendOthers, u32 uTotalProcessingTime)
{
   if ( uTimeReadCamera > 0x0F ) uTimeReadCamera = 0x0F;
   if ( uTimeSendVideo > 0xFF ) uTimeSendVideo = 0xFF;
   if ( uTimeExpectedSendVideoTime > 0xFF ) uTimeExpectedSendVideoTime = 0xFF;
   if ( uTimeSendOthers > 0x0F ) uTimeSendOthers = 0x0F;
   if ( uTotalProcessingTime > 0xFF ) uTotalProcessingTime = 0xFF;
   
   m_uLastFrameTimers = (uTimeReadCamera & 0x0F) |
      ((uTimeSendOthers & 0x0F) << 4) |
      ((uTimeSendVideo & 0xFF) << 8) |
      ((uTotalProcessingTime & 0xFF) << 16);
      ((uTimeExpectedSendVideoTime & 0xFF) << 24);
}

void VideoTxPacketsBuffer::setLastFrameDistanceMs(u32 uDistanceMs)
{
   m_uLastFrameDistanceMs = uDistanceMs;
}

void VideoTxPacketsBuffer::setCurrentRealFPS(int iFPS)
{
   m_iCurrentRealFPS = iFPS;
}

int VideoTxPacketsBuffer::getCurrentRealFPS()
{
   return m_iCurrentRealFPS;
}

void VideoTxPacketsBuffer::setCustomECScheme(u16 uECScheme)
{
   m_uCustomECScheme = uECScheme;
   updateVideoHeader(g_pCurrentModel);
}

int VideoTxPacketsBuffer::getCurrentTotalBlockPackets()
{
   return m_uNextBlockDataPackets + m_uNextBlockECPackets;
}

void VideoTxPacketsBuffer::updateVideoHeader(Model* pModel)
{
   if ( NULL == pModel )
      return;

   log_line("[VideoTxBuffer] On update video header: before: EC scheme: %d/%d, %d bytes model video packet size, developer mode: %s)",
       m_PacketHeaderVideo.uCurrentBlockDataPackets, m_PacketHeaderVideo.uCurrentBlockECPackets,
       m_PacketHeaderVideo.uCurrentBlockPacketSize, (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE)?"on":"off");

   log_line("[VideoTxBuffer] On update video header: last applied EC scheme: %d/%d, custom scheme: %d/%d",
      m_uLastAppliedECSchemeDataPackets, m_uLastAppliedECSchemeECPackets,
       (m_uCustomECScheme >> 8) & 0xFF, m_uCustomECScheme & 0xFF);

   log_line("[VideoTxBuffer] On update video header: before: %d usable raw video bytes, maj NAL size: %d",
       m_iUsableRawVideoDataSize, hardware_camera_maj_get_current_nal_size());

   // Update status flags
   m_PacketHeaderVideo.uVideoStatusFlags2 = 0;

   radio_packet_init(&m_PacketHeader, PACKET_COMPONENT_VIDEO | PACKET_FLAGS_BIT_HEADERS_ONLY_CRC, PACKET_TYPE_VIDEO_DATA, STREAM_ID_VIDEO_1);
   m_PacketHeader.vehicle_id_src = pModel->uVehicleId;
   m_PacketHeader.vehicle_id_dest = 0;

   m_uNextBlockPacketSize = pModel->video_link_profiles[pModel->video_params.iCurrentVideoProfile].video_data_length;

   int iVideoProfile = g_pCurrentModel->video_params.iCurrentVideoProfile;
   pModel->convertECPercentageToData(&(pModel->video_link_profiles[iVideoProfile]));

   m_uNextBlockDataPackets = pModel->video_link_profiles[iVideoProfile].iBlockDataPackets;
   m_uNextBlockECPackets = pModel->video_link_profiles[iVideoProfile].iBlockECs;
   if ( (0 != m_uCustomECScheme) && (0xFFFF != m_uCustomECScheme) )
   {
      m_uNextBlockECPackets = m_uCustomECScheme & 0xFF;
      if ( m_uNextBlockECPackets > m_uNextBlockDataPackets )
         m_uNextBlockECPackets = m_uNextBlockDataPackets;
      if ( m_uNextBlockECPackets > MAX_FECS_PACKETS_IN_BLOCK )
         m_uNextBlockECPackets = MAX_FECS_PACKETS_IN_BLOCK;
   }

   log_line("[VideoTxBuffer] Next EC scheme to use: %d/%d (%d bytes), now is on video block index: [%u/%u] for scheme [%u/%u]",
       m_uNextBlockDataPackets, m_uNextBlockECPackets, m_uNextBlockPacketSize,
         m_pLastPacketHeaderVideoFilledIn->uCurrentBlockIndex, m_pLastPacketHeaderVideoFilledIn->uCurrentBlockPacketIndex,
         m_pLastPacketHeaderVideoFilledIn->uCurrentBlockDataPackets, m_pLastPacketHeaderVideoFilledIn->uCurrentBlockECPackets);

   // Apply right away (we just start a new video block now)?
   if ( 0 == m_iNextBufferPacketIndexToFill )
   {
      m_PacketHeaderVideo.uCurrentBlockDataPackets = m_uNextBlockDataPackets;
      m_PacketHeaderVideo.uCurrentBlockECPackets = m_uNextBlockECPackets;
      m_PacketHeaderVideo.uCurrentBlockPacketSize = m_uNextBlockPacketSize;
      m_uLastAppliedECSchemeDataPackets = m_uNextBlockDataPackets;
      m_uLastAppliedECSchemeECPackets = m_uNextBlockECPackets;
      
      m_iUsableRawVideoDataSize = m_PacketHeaderVideo.uCurrentBlockPacketSize - sizeof(t_packet_header_video_segment_important);

      #if defined (HW_PLATFORM_OPENIPC_CAMERA)
      hardware_camera_maj_update_nal_size(g_pCurrentModel);
      #endif
      log_line("[VideoTxBuffer] Current EC scheme to use rightaway: %d/%d, %d model video packet bytes", m_PacketHeaderVideo.uCurrentBlockDataPackets, m_PacketHeaderVideo.uCurrentBlockECPackets, m_PacketHeaderVideo.uCurrentBlockPacketSize);
      log_line("[VideoTxBuffer] Current usable raw bytes: %d, majestic NAL size now: %d", m_iUsableRawVideoDataSize, hardware_camera_maj_get_current_nal_size());
   }
   else
      log_line("[VideoTxBuffer] Will use EC scheme at next video block.");

   if ( pModel->video_params.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265 )
   {
      m_PacketHeaderVideo.uVideoStreamIndexAndType = 0 | (VIDEO_TYPE_H265<<4); // video stream index is 0
      log_line("[VideoTxBuffer] Set video header as H265 stream");
   }
   else
   {
      m_PacketHeaderVideo.uVideoStreamIndexAndType = 0 | (VIDEO_TYPE_H264<<4);
      log_line("[VideoTxBuffer] Set video header as H264 stream");
   }
   
   m_PacketHeaderVideo.uCurrentVideoLinkProfile = iVideoProfile;
   m_PacketHeaderVideo.uStreamInfoFlags = 0;
   m_PacketHeaderVideo.uStreamInfo = 0;
}

void VideoTxPacketsBuffer::appendDataToCurrentFrame(u8* pVideoData, int iDataSize, u32 uNALPresenceFlags, bool bIsEndOfFrame, u32 uTimeDataAvailable)
{
   if ( (NULL == m_pTempVideoFrameBuffer) || g_bVideoPaused )
      return;

   m_uTempNALPresenceFlags |= uNALPresenceFlags;

   if ( (NULL != g_pProcessorTxVideo) && (NULL != pVideoData) && (iDataSize > 0) )
      process_data_tx_video_on_new_data(pVideoData, iDataSize);

   if ( 0 == m_iTempVideoBufferFilledBytes )
      m_uTimeDataAvailable = uTimeDataAvailable;

   // Append data to current video frame buffer
   if ( (NULL != pVideoData) && (iDataSize > 0) && (iDataSize <= m_iTempVideoFrameBufferSize - m_iTempVideoBufferFilledBytes) )
   {
      memcpy(&(m_pTempVideoFrameBuffer[m_iTempVideoBufferFilledBytes]), pVideoData, iDataSize);
      m_iTempVideoBufferFilledBytes += iDataSize;
   }

   // If not the end of frame, just append data to current video frame buffer
   if ( ! bIsEndOfFrame )
   {
      if ( m_iTempVideoBufferFilledBytes > 200000 )
         log_softerror_and_alarm("Too much data in tx buffers %d bytes", m_iTempVideoBufferFilledBytes);
      return;
   }

   // We are at the end of frame,
   // Generate video data packets from the temporary video frame buffer
   // and then clean the temporary video frame buffer
   m_uCurrentH264FrameIndex++;

   // First check for pending video block scheme changes
   if ( (m_PacketHeaderVideo.uCurrentBlockPacketSize != m_uNextBlockPacketSize) ||
        (m_PacketHeaderVideo.uCurrentBlockDataPackets != m_uNextBlockDataPackets) ||
        (m_PacketHeaderVideo.uCurrentBlockECPackets != m_uNextBlockECPackets) )
   {
      /*
      log_line("[VideoTxBuffer] Pending update video header before: EC scheme: %d/%d, model video packet size: %d",
          m_PacketHeaderVideo.uCurrentBlockDataPackets, m_PacketHeaderVideo.uCurrentBlockECPackets,
          m_PacketHeaderVideo.uCurrentBlockPacketSize);

      log_line("[VideoTxBuffer] Pending update video header before: usable raw video bytes: %d, maj NAL size: %d",
          m_iUsableRawVideoDataSize, hardware_camera_maj_get_current_nal_size());
      */
      m_PacketHeaderVideo.uCurrentBlockPacketSize = m_uNextBlockPacketSize;
      m_PacketHeaderVideo.uCurrentBlockDataPackets = m_uNextBlockDataPackets;
      m_PacketHeaderVideo.uCurrentBlockECPackets = m_uNextBlockECPackets;
      m_uLastAppliedECSchemeDataPackets = m_uNextBlockDataPackets;
      m_uLastAppliedECSchemeECPackets = m_uNextBlockECPackets;
      
      #if defined (HW_PLATFORM_OPENIPC_CAMERA)
      hardware_camera_maj_update_nal_size(g_pCurrentModel);
      #endif
   }

   m_iUsableRawVideoDataSize = m_PacketHeaderVideo.uCurrentBlockPacketSize - sizeof(t_packet_header_video_segment_important);

   int iDataSizeLeft = m_iTempVideoBufferFilledBytes;
   u8* pVideoDataLeft = &(m_pTempVideoFrameBuffer[0]);

   int iComputedVideoDataPacketsInThisFrame = iDataSizeLeft/m_iUsableRawVideoDataSize;
   if ( 0 != (iDataSizeLeft % m_iUsableRawVideoDataSize) )
      iComputedVideoDataPacketsInThisFrame++;

   int iVideoDataPacketsInThisFrame = 0;
   int iTotalCountAdded = 0;
   int iTotalCountRemaining = iComputedVideoDataPacketsInThisFrame;
   while ( iDataSizeLeft > 0 )
   {
      int iPacketSize = m_iUsableRawVideoDataSize;
      if ( iPacketSize > iDataSizeLeft )
         iPacketSize = iDataSizeLeft;

      iTotalCountAdded += _addNewVideoPacket(pVideoDataLeft, iPacketSize, iTotalCountRemaining, (iDataSizeLeft == iPacketSize));
      iVideoDataPacketsInThisFrame++;
      iTotalCountRemaining--;
      iDataSizeLeft -= iPacketSize;
      pVideoDataLeft += iPacketSize;
   }

   // Update EOF counter, eof and start/end nal for each video packet in this frame
   // Update uFramePacketsInfo for frame info for all packets in frame
   int iBufferIndex = m_iNextBufferIndexToFill;
   int iPacketIndex = m_iNextBufferPacketIndexToFill;
   u32 uCountToEOF = 0;
   if ( iVideoDataPacketsInThisFrame > 255 )
      iVideoDataPacketsInThisFrame = 255;
   int iVideoDataPacketsCounter = iVideoDataPacketsInThisFrame;
   iVideoDataPacketsCounter--;
   do
   {
       iTotalCountAdded--;
       iPacketIndex--;
       if ( iPacketIndex < 0 )
       {
          iBufferIndex--;
          if ( iBufferIndex < 0 )
             iBufferIndex = MAX_RXTX_BLOCKS_BUFFER - 1;
          iPacketIndex = m_VideoPackets[iBufferIndex][0].pPHVS->uCurrentBlockDataPackets + m_VideoPackets[iBufferIndex][0].pPHVS->uCurrentBlockECPackets - 1;
       }

       t_packet_header* pPH = m_VideoPackets[iBufferIndex][iPacketIndex].pPH;
       t_packet_header_video_segment* pPHVS = m_VideoPackets[iBufferIndex][iPacketIndex].pPHVS;
       if ( (NULL == pPH) || (NULL == pPHVS) )
           break;
       if ( pPHVS->uH264FrameIndex != m_uCurrentH264FrameIndex )
           break;

       if ( pPHVS->uCurrentBlockPacketIndex >= pPHVS->uCurrentBlockDataPackets )
          pPHVS->uFramePacketsInfo = (u16)((((u16)iVideoDataPacketsInThisFrame)<<8) | ((u16)iVideoDataPacketsCounter));
       else
       {
          pPHVS->uFramePacketsInfo = (u16)((((u16)iVideoDataPacketsInThisFrame)<<8) | ((u16)iVideoDataPacketsCounter));
          iVideoDataPacketsCounter--;
       }
       pPHVS->uVideoStatusFlags2 &= ~VIDEO_STATUS_FLAGS2_MASK_EOF_COUNTER;
       pPHVS->uVideoStatusFlags2 |= (uCountToEOF & VIDEO_STATUS_FLAGS2_MASK_EOF_COUNTER);
       if ( 0 == uCountToEOF )
          pPHVS->uVideoStatusFlags2 |= VIDEO_STATUS_FLAGS2_IS_END_OF_FRAME;
       if ( 0 == iTotalCountAdded )
          pPHVS->uVideoStatusFlags2 |= VIDEO_STATUS_FLAGS2_IS_NAL_START;
       else if ( (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_I) || (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_P) )
          pPHVS->uVideoStatusFlags2 &= ~VIDEO_STATUS_FLAGS2_IS_NAL_O;

       uCountToEOF++;
   } while ( iTotalCountAdded > 0 );


   m_iTempVideoBufferFilledBytes = 0;
   m_uTempNALPresenceFlags = 0;
}

int VideoTxPacketsBuffer::_addNewVideoPacket(u8* pRawVideoData, int iRawVideoDataSize, int iRemainingVideoPackets, bool bIsLastPacket)
{
   if ( (! m_bInitialized) || (NULL == pRawVideoData) || (iRawVideoDataSize <= 0) || (iRawVideoDataSize > MAX_PACKET_PAYLOAD) )
      return 0;
   int iCountPacketsAdded = 0;

   _checkAllocatePacket(m_iNextBufferIndexToFill, m_iNextBufferPacketIndexToFill);

   // Started a new video block? Clear the block state
   if ( 0 == m_iNextBufferPacketIndexToFill )
   {
      for(int i=0; i<(int)(m_PacketHeaderVideo.uCurrentBlockDataPackets + m_PacketHeaderVideo.uCurrentBlockECPackets); i++)
         _checkAllocatePacket(m_iNextBufferIndexToFill, i);
      for(int i=0; i<MAX_TOTAL_PACKETS_IN_BLOCK; i++)
         m_VideoPackets[m_iNextBufferIndexToFill][i].bEmpty = true;

      m_PacketHeaderVideo.uCurrentBlockPacketSize = m_uNextBlockPacketSize;
      m_PacketHeaderVideo.uCurrentBlockDataPackets = m_uNextBlockDataPackets;
      m_PacketHeaderVideo.uCurrentBlockECPackets = m_uNextBlockECPackets;
      m_uLastAppliedECSchemeDataPackets = m_uNextBlockDataPackets;
      m_uLastAppliedECSchemeECPackets = m_uNextBlockECPackets;

      if ( iRemainingVideoPackets > m_PacketHeaderVideo.uCurrentBlockDataPackets )
      if ( iRemainingVideoPackets < m_PacketHeaderVideo.uCurrentBlockDataPackets + 5 )
      if ( iRemainingVideoPackets < MAX_DATA_PACKETS_IN_BLOCK )
      {
         int iTmpDataPackets = m_PacketHeaderVideo.uCurrentBlockDataPackets;
         m_PacketHeaderVideo.uCurrentBlockDataPackets = iRemainingVideoPackets;
         if ( m_uNextBlockECPackets > 0 )
            m_PacketHeaderVideo.uCurrentBlockECPackets = (m_PacketHeaderVideo.uCurrentBlockDataPackets * m_uNextBlockECPackets) / m_uNextBlockDataPackets;
         //log_line("DBG should append packets in block, block size: %d, remaining %d; old: %d/%d, new scheme: %d/%d", iTmpDataPackets, iRemainingVideoPackets, m_uNextBlockDataPackets, m_uNextBlockECPackets, m_PacketHeaderVideo.uCurrentBlockDataPackets, m_PacketHeaderVideo.uCurrentBlockECPackets);
      }
   }

   _fillVideoPacketHeaders(m_iNextBufferIndexToFill, m_iNextBufferPacketIndexToFill, false, iRawVideoDataSize, bIsLastPacket);

   // Copy video data
   t_packet_header_video_segment* pCurrentVideoPacketHeader = m_VideoPackets[m_iNextBufferIndexToFill][m_iNextBufferPacketIndexToFill].pPHVS;
   u8* pVideoDestination = m_VideoPackets[m_iNextBufferIndexToFill][m_iNextBufferPacketIndexToFill].pVideoData;
   pVideoDestination += sizeof(t_packet_header_video_segment_important);

   memcpy(pVideoDestination, pRawVideoData, iRawVideoDataSize);
   pVideoDestination += iRawVideoDataSize;

   // Add dbg info only to full video packets
   if ( iRawVideoDataSize == m_iUsableRawVideoDataSize )
   if ( (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE) &&
        (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_STREAM_TIMINGS) )
   {
      t_packet_header_video_segment_debug_info* pDbgInfo = (t_packet_header_video_segment_debug_info*)pVideoDestination;
      pDbgInfo->uFrameIndex = m_uCurrentH264FrameIndex;
      pDbgInfo->uTime1 = m_uTimeDataAvailable;
      pVideoDestination += sizeof(t_packet_header_video_segment_debug_info);
   }

   // Set remaining empty space in packet to 0 as EC uses the good video data packets too.
   int iSizeToZero = MAX_PACKET_TOTAL_SIZE - sizeof(t_packet_header) - sizeof(t_packet_header_video_segment) - sizeof(t_packet_header_video_segment_important);
   iSizeToZero -= iRawVideoDataSize;
   if ( iSizeToZero > 0 )
      memset(pVideoDestination, 0, iSizeToZero);

   // Update state
   m_iNextBufferPacketIndexToFill++;
   m_uNextVideoBlockPacketIndexToGenerate++;
   iCountPacketsAdded++;

   if ( m_uNextVideoBlockPacketIndexToGenerate >= pCurrentVideoPacketHeader->uCurrentBlockDataPackets )
   if ( pCurrentVideoPacketHeader->uCurrentBlockDataPackets > 1 )
   if ( pCurrentVideoPacketHeader->uCurrentBlockECPackets > 0 )
   {
      // Compute EC packets
      u8* p_fec_data_packets[MAX_DATA_PACKETS_IN_BLOCK];
      u8* p_fec_data_fecs[MAX_FECS_PACKETS_IN_BLOCK];

      for( int i=0; i<pCurrentVideoPacketHeader->uCurrentBlockDataPackets; i++ )
      {
         _checkAllocatePacket(m_iNextBufferIndexToFill, i);
         p_fec_data_packets[i] = m_VideoPackets[m_iNextBufferIndexToFill][i].pVideoData;
      }
      int iECDelta = pCurrentVideoPacketHeader->uCurrentBlockDataPackets;
      for( int i=0; i<pCurrentVideoPacketHeader->uCurrentBlockECPackets; i++ )
      {
         _checkAllocatePacket(m_iNextBufferIndexToFill, i+iECDelta);
         p_fec_data_fecs[i] = m_VideoPackets[m_iNextBufferIndexToFill][i+iECDelta].pVideoData;
      }

      u32 tTemp = get_current_timestamp_micros();
      fec_encode(pCurrentVideoPacketHeader->uCurrentBlockPacketSize, p_fec_data_packets, pCurrentVideoPacketHeader->uCurrentBlockDataPackets, p_fec_data_fecs, pCurrentVideoPacketHeader->uCurrentBlockECPackets);
      tTemp = get_current_timestamp_micros() - tTemp;
      s_uTimeTotalFecTimeMicroSec += tTemp;
      if ( 0 == s_uLastTimeFecCalculation )
      {
         s_uTimeFecMsPerSec = 0;
         s_uTimeTotalFecTimeMicroSec = 0;
         s_uLastTimeFecCalculation = g_TimeNow;
      }
      else if ( g_TimeNow >= s_uLastTimeFecCalculation + 500 )
      {
         s_uTimeFecMsPerSec = s_uTimeTotalFecTimeMicroSec / (g_TimeNow - s_uLastTimeFecCalculation);
         s_uTimeTotalFecTimeMicroSec = 0;
         s_uLastTimeFecCalculation = g_TimeNow;
      }

      int iECDataSize = pCurrentVideoPacketHeader->uCurrentBlockPacketSize - sizeof(t_packet_header_video_segment_important);

      for( int i=0; i<pCurrentVideoPacketHeader->uCurrentBlockECPackets; i++ )
      {
         // Update packet headers
         _fillVideoPacketHeaders(m_iNextBufferIndexToFill, i+iECDelta, true, iECDataSize, bIsLastPacket);
         m_iNextBufferPacketIndexToFill++;
         m_uNextVideoBlockPacketIndexToGenerate++;
         iCountPacketsAdded++;
      }
   }

   if ( m_uNextVideoBlockPacketIndexToGenerate >= pCurrentVideoPacketHeader->uCurrentBlockDataPackets + pCurrentVideoPacketHeader->uCurrentBlockECPackets )
   {
      m_uNextVideoBlockPacketIndexToGenerate = 0;
      m_uNextVideoBlockIndexToGenerate++;
      m_iNextBufferPacketIndexToFill = 0;
      m_iNextBufferIndexToFill++;
      if ( m_iNextBufferIndexToFill >= MAX_RXTX_BLOCKS_BUFFER )
         m_iNextBufferIndexToFill = 0;

      // Buffer is full, discard old packets
      if ( m_iNextBufferIndexToFill == m_iCurrentBufferIndexToSend )
      {
         log_softerror_and_alarm("[VideoTxBuffer] Buffer is full. Discard all blocks.");
         discardBuffer();
         iCountPacketsAdded = 0;
      }

      for(int i=0; i<(int)(m_PacketHeaderVideo.uCurrentBlockDataPackets + m_PacketHeaderVideo.uCurrentBlockECPackets); i++)
         _checkAllocatePacket(m_iNextBufferIndexToFill, i);
      for(int i=0; i<MAX_TOTAL_PACKETS_IN_BLOCK; i++)
         m_VideoPackets[m_iNextBufferIndexToFill][i].bEmpty = true;
   }
   return iCountPacketsAdded;
}

void VideoTxPacketsBuffer::_sendPacket(int iBufferIndex, int iPacketIndex, u32 uRetransmissionId, int iCountPacketsAferVideo)
{
   if ( m_VideoPackets[iBufferIndex][iPacketIndex].bEmpty )
      return;

   t_packet_header* pCurrentPacketHeader = m_VideoPackets[iBufferIndex][iPacketIndex].pPH;
   t_packet_header_video_segment* pCurrentVideoPacketHeader = m_VideoPackets[iBufferIndex][iPacketIndex].pPHVS;

   if ( pCurrentPacketHeader->total_length < sizeof(t_packet_header) + sizeof(t_packet_header_video_segment) + sizeof(t_packet_header_video_segment_important))
      return;

   process_data_tx_video_on_data_sent(pCurrentPacketHeader->total_length);

   // stream_packet_idx: high 4 bits: stream id (0..15), lower 28 bits: stream packet index
   pCurrentPacketHeader->stream_packet_idx = (m_uRadioStreamPacketIndex & PACKET_FLAGS_MASK_STREAM_PACKET_IDX);
   pCurrentPacketHeader->stream_packet_idx |= (STREAM_ID_VIDEO_1) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   m_uRadioStreamPacketIndex++;

   pCurrentPacketHeader->packet_flags = PACKET_COMPONENT_VIDEO;
   pCurrentPacketHeader->packet_flags |= PACKET_FLAGS_BIT_HEADERS_ONLY_CRC;

   if ( 0 == uRetransmissionId )
   {
      pCurrentPacketHeader->packet_flags &= ~PACKET_FLAGS_BIT_RETRANSMITED;
      pCurrentPacketHeader->packet_flags &= ~PACKET_FLAGS_BIT_HIGH_PRIORITY;
      pCurrentVideoPacketHeader->uVideoStatusFlags2 &= ~VIDEO_STATUS_FLAGS2_MASK_DATA_COUNTER;
      pCurrentVideoPacketHeader->uVideoStatusFlags2 |= (((u32)iCountPacketsAferVideo) & 0xFF) << 16;
   }
   else
   {
      pCurrentPacketHeader->packet_flags |= PACKET_FLAGS_BIT_RETRANSMITED;
      pCurrentPacketHeader->packet_flags |= PACKET_FLAGS_BIT_HIGH_PRIORITY;
      pCurrentVideoPacketHeader->uStreamInfoFlags = VIDEO_STREAM_INFO_FLAG_RETRANSMISSION_ID;
      pCurrentVideoPacketHeader->uStreamInfo = uRetransmissionId;
   }

   if ( g_bVideoPaused || (! relay_current_vehicle_must_send_own_video_feeds()) )
      return;

   /*
   if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_INJECT_VIDEO_FAULTS )
   if ( ((g_TimeNow/1000/10) % 6) == 0 )
      return true;

   if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_INJECT_RECOVERABLE_VIDEO_FAULTS )
   {
      if ( ((g_TimeNow/1000) % 20) < 10 )
      {
         if ( (pCurrentVideoPacketHeader->uCurrentBlockPacketIndex < pCurrentVideoPacketHeader->uCurrentBlockDataPackets-1) || (pCurrentVideoPacketHeader->uCurrentBlockDataPackets == 1) )
            return true;
         if ( (((g_TimeNow/100) % 200) % 10) < 5 )
         if ( pCurrentVideoPacketHeader->uCurrentBlockPacketIndex < pCurrentVideoPacketHeader->uCurrentBlockDataPackets + pCurrentVideoPacketHeader->uCurrentBlockECPackets - 2 )
            return true;
      }
   }
   */

   /*
   t_packet_header_video_segment* pPHVS = pCurrentVideoPacketHeader;
   t_packet_header* pPH = pCurrentPacketHeader;
      log_line("DBG %c%d [%u/%02d of %02d] sch %d/%d, framep %d/%d, EOF in %d+%d, NAL %s%s-%s%s%s, eof?%d", 
          (pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED)?'r':'f',
          pPHVS->uH264FrameIndex, pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex,
          pPHVS->uCurrentBlockDataPackets + pPHVS->uCurrentBlockECPackets,
          pPHVS->uCurrentBlockDataPackets, pPHVS->uCurrentBlockECPackets,
          pPHVS->uFramePacketsInfo & 0xFF, pPHVS->uFramePacketsInfo >> 8,
          pPHVS->uVideoStatusFlags2 & 0xFF,
          (pPHVS->uVideoStatusFlags2 >> 16) & 0xFF,
          (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_START)?"s":"",
          (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_END)?"e":"",
          (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_I)?"i":"",
          (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_P)?"p":"",
          (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_O)?"o":"",
          (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_END_OF_FRAME)?1:0);
   */

   u32 uTimeMicros = get_current_timestamp_micros();

   if ( uTimeMicros > m_uLastTimeSentVideoPacketMicros )
   if ( (uTimeMicros - m_uLastTimeSentVideoPacketMicros) < m_uLastSentVideoPacketDurationMicros)
   {
      // Don't spam the driver with packets while it's sending a packet;
      // Better yield that time to other processes.
      hardware_sleep_micros(((m_uLastSentVideoPacketDurationMicros - (uTimeMicros - m_uLastTimeSentVideoPacketMicros))*3)/4 );
   }
   m_uLastTimeSentVideoPacketMicros = uTimeMicros;

   packet_utils_reset_last_used_video_datarate();
   send_packet_to_radio_interfaces((u8*)pCurrentPacketHeader, pCurrentPacketHeader->total_length, -1);
   m_uLastSentVideoPacketDatarateBPS = get_last_tx_maximum_video_radio_datarate_bps();
   if ( (0 == m_uLastSentVideoPacketDatarateMinBPS) || (m_uLastSentVideoPacketDatarateBPS < m_uLastSentVideoPacketDatarateMinBPS) )
      m_uLastSentVideoPacketDatarateMinBPS = m_uLastSentVideoPacketDatarateBPS;
   if ( (0 == m_uLastSentVideoPacketDatarateMaxBPS) || (m_uLastSentVideoPacketDatarateBPS > m_uLastSentVideoPacketDatarateMaxBPS) )
      m_uLastSentVideoPacketDatarateMaxBPS = m_uLastSentVideoPacketDatarateBPS;

   m_uLastSentVideoPacketDurationMicros = (8 * 1000 * (pCurrentPacketHeader->total_length+18) / (m_uLastSentVideoPacketDatarateBPS/1000));
   // If a retransmission, then push it faster so we can get back sooner at waiting/getting next requests from controller
   if ( 0 != uRetransmissionId )
      m_uLastSentVideoPacketDurationMicros = m_uLastSentVideoPacketDurationMicros/2;

   if ( 0 == uRetransmissionId )
      m_uExpectedFrameTransmissionTimeMicros += m_uLastSentVideoPacketDurationMicros;

   static u32 s_uLastTimeCheckTxTimes = 0;
   static u32 s_uTotalTxTimeMicros = 0;
   
   s_uTotalTxTimeMicros += m_uLastSentVideoPacketDurationMicros;
   if ( g_TimeNow >= s_uLastTimeCheckTxTimes + 100 )
   {
      s_uTotalTxTimeMicros = (s_uTotalTxTimeMicros * 1000) / (g_TimeNow - s_uLastTimeCheckTxTimes);
      m_uTelemetryVideoTxTimeMsPerSec = s_uTotalTxTimeMicros/1000;
      s_uLastTimeCheckTxTimes = g_TimeNow;
      s_uTotalTxTimeMicros = 0;
   }
}

bool VideoTxPacketsBuffer::hasPendingPacketsToSend()
{
   if ( m_iCurrentBufferIndexToSend == m_iNextBufferIndexToFill )
   if ( m_iCurrentBufferPacketIndexToSend == m_iNextBufferPacketIndexToFill )
      return false;
   return true;
}

int VideoTxPacketsBuffer::sendAvailablePackets(int iCountPacketsAferVideo)
{
   packet_utils_reset_last_used_max_video_datarate();
   m_uExpectedFrameTransmissionTimeMicros = 0;
   m_uLastSentVideoPacketDatarateMinBPS = 0;
   m_uLastSentVideoPacketDatarateMaxBPS = 0;

   if ( m_iCurrentBufferIndexToSend == m_iNextBufferIndexToFill )
   if ( m_iCurrentBufferPacketIndexToSend == m_iNextBufferPacketIndexToFill )
      return 0;

   int iCountSent = 0;
   while ( true )
   {
      if ( m_iCurrentBufferIndexToSend == m_iNextBufferIndexToFill )
      if ( m_iCurrentBufferPacketIndexToSend == m_iNextBufferPacketIndexToFill )
         break;

      if ( NULL == m_VideoPackets[m_iCurrentBufferIndexToSend][m_iCurrentBufferPacketIndexToSend].pPH )
         log_softerror_and_alarm("Invalid packet [%d/%d], video next to gen: [%u/%u], header: %X", m_iCurrentBufferIndexToSend, m_iCurrentBufferPacketIndexToSend,
            m_uNextVideoBlockIndexToGenerate, m_uNextVideoBlockPacketIndexToGenerate, m_VideoPackets[m_iCurrentBufferIndexToSend][m_iCurrentBufferPacketIndexToSend].pPH);
      else if ( m_VideoPackets[m_iCurrentBufferIndexToSend][m_iCurrentBufferPacketIndexToSend].bEmpty )
         log_softerror_and_alarm("Try to send empty packet [%d/%d], video next to gen: [%u/%u], ready to send: %d, header: %X", m_iCurrentBufferIndexToSend, m_iCurrentBufferPacketIndexToSend,
            m_uNextVideoBlockIndexToGenerate, m_uNextVideoBlockPacketIndexToGenerate, m_VideoPackets[m_iCurrentBufferIndexToSend][m_iCurrentBufferPacketIndexToSend].pPH);
      else
      {
         _sendPacket(m_iCurrentBufferIndexToSend, m_iCurrentBufferPacketIndexToSend, 0, iCountPacketsAferVideo);

         t_packet_header_video_segment* pCurrentVideoPacketHeader = m_VideoPackets[m_iCurrentBufferIndexToSend][m_iCurrentBufferPacketIndexToSend].pPHVS;
         if ( (pCurrentVideoPacketHeader->uCurrentBlockECPackets == 0) && (pCurrentVideoPacketHeader->uCurrentBlockDataPackets == 1) )
            _sendPacket(m_iCurrentBufferIndexToSend, m_iCurrentBufferPacketIndexToSend, 0, iCountPacketsAferVideo);
      }

      iCountSent++;

      if ( NULL == m_VideoPackets[m_iCurrentBufferIndexToSend][m_iCurrentBufferPacketIndexToSend].pPHVS )
      {
         m_iCurrentBufferPacketIndexToSend = 0;
         m_iCurrentBufferIndexToSend++;
         if ( m_iCurrentBufferIndexToSend >= MAX_RXTX_BLOCKS_BUFFER )
            m_iCurrentBufferIndexToSend = 0;
         continue;
      }

      t_packet_header_video_segment* pCurrentVideoPacketHeader = m_VideoPackets[m_iCurrentBufferIndexToSend][m_iCurrentBufferPacketIndexToSend].pPHVS;
      m_iCurrentBufferPacketIndexToSend++;

      if ( m_iCurrentBufferPacketIndexToSend >= pCurrentVideoPacketHeader->uCurrentBlockDataPackets + pCurrentVideoPacketHeader->uCurrentBlockECPackets )
      {
         m_iCurrentBufferPacketIndexToSend = 0;
         m_iCurrentBufferIndexToSend++;
         if ( m_iCurrentBufferIndexToSend >= MAX_RXTX_BLOCKS_BUFFER )
            m_iCurrentBufferIndexToSend = 0;
      }
   }
   return iCountSent;
}


void VideoTxPacketsBuffer::resendVideoPacket(u32 uRetransmissionId, u32 uVideoBlockIndex, u32 uVideoBlockPacketIndex)
{
   if ( uVideoBlockIndex > m_uNextVideoBlockIndexToGenerate )
   {
      log_softerror_and_alarm("[VideoTxBuffer] Recv req for retr for block index %u which is greater than max present in buffer: %u",
         uVideoBlockIndex, m_uNextVideoBlockIndexToGenerate-1);
      return;
   }
   if ( uVideoBlockIndex == m_uNextVideoBlockIndexToGenerate )
   if ( (uVideoBlockPacketIndex >= m_uNextVideoBlockPacketIndexToGenerate) && (uVideoBlockPacketIndex != 0xFF) )
   {
      log_line("[VideoTxBuffer] Recv req for retr for block packet [%u/%u] which is greater than max present in buffer: [%u/%u], top video block packet index: %d, is at end of frame: %d of %d",
         uVideoBlockIndex, uVideoBlockPacketIndex, m_uNextVideoBlockIndexToGenerate, m_uNextVideoBlockPacketIndexToGenerate-1,
         (int)m_pLastPacketHeaderVideoFilledIn->uCurrentBlockPacketIndex,
         (m_pLastPacketHeaderVideoFilledIn->uFramePacketsInfo & 0xFF),
         ((m_pLastPacketHeaderVideoFilledIn->uFramePacketsInfo >> 8) & 0xFF));
      return;
   }
   int iDeltaBlocksBack = (int)m_uNextVideoBlockIndexToGenerate - (int)uVideoBlockIndex;
   if ( (iDeltaBlocksBack < 0) || (iDeltaBlocksBack >= MAX_RXTX_BLOCKS_BUFFER) )
   {
      log_softerror_and_alarm("[VideoTxBuffer] Recv req for retr for block index out of range: %d blocks back (of max %d blocks)", iDeltaBlocksBack, MAX_RXTX_BLOCKS_BUFFER);
      return;
   }
   int iBufferIndex = m_iNextBufferIndexToFill - iDeltaBlocksBack;
   if ( iBufferIndex < 0 )
      iBufferIndex += MAX_RXTX_BLOCKS_BUFFER;

   // Still too old?
   if ( iBufferIndex < 0 )
   {
      log_softerror_and_alarm("[VideoTxBuffer] Recv request for retr for block index still out of range: %d ", iBufferIndex);
      return;
   }
   if ( (uVideoBlockPacketIndex != 0xFF) && (NULL == m_VideoPackets[iBufferIndex][uVideoBlockPacketIndex].pPH) )
   {
      log_softerror_and_alarm("[VideoTxBuffer] Recv request for retr of empty video block index [%u/%u]", uVideoBlockIndex, uVideoBlockPacketIndex);
      return;
   }
   if ( (uVideoBlockPacketIndex != 0xFF) && (m_VideoPackets[iBufferIndex][uVideoBlockPacketIndex].pPHVS->uCurrentBlockIndex != uVideoBlockIndex) )
   {
      log_softerror_and_alarm("[VideoTxBuffer] Recv request for retr of invalid video block [%u], buffer has video block [%u] at that position (%d)", uVideoBlockIndex, m_VideoPackets[iBufferIndex][uVideoBlockPacketIndex].pPHVS->uCurrentBlockIndex, iBufferIndex);
      return;
   }
   if ( (uVideoBlockPacketIndex != 0xFF) && (m_VideoPackets[iBufferIndex][uVideoBlockPacketIndex].pPHVS->uCurrentBlockPacketIndex != uVideoBlockPacketIndex) )
   {
      log_softerror_and_alarm("[VideoTxBuffer] Recv request for retr of invalid video block [%u/%u], buffer has video block [%u/%u] at that position (%d)", uVideoBlockIndex, uVideoBlockPacketIndex, m_VideoPackets[iBufferIndex][uVideoBlockPacketIndex].pPHVS->uCurrentBlockIndex, m_VideoPackets[iBufferIndex][uVideoBlockPacketIndex].pPHVS->uCurrentBlockPacketIndex, iBufferIndex);
      return;
   }

   if ( 0 == uRetransmissionId )
      uRetransmissionId = MAX_U32-1;

   if ( 0xFF == uVideoBlockPacketIndex )
   {
      for( u8 u=0; u<m_VideoPackets[iBufferIndex][0].pPHVS->uCurrentBlockDataPackets; u++ )
      {
         if ( m_VideoPackets[iBufferIndex][u].bEmpty )
         {
            log_softerror_and_alarm("[VideoTxBuffer] Recv request for retr of full block [%u/%u], but buffer has empty video block [%u/%u] at that position (%d), next video packet to generate now is: [%u/%u]",
               uVideoBlockIndex, uVideoBlockPacketIndex, m_VideoPackets[iBufferIndex][u].pPHVS->uCurrentBlockIndex, m_VideoPackets[iBufferIndex][u].pPHVS->uCurrentBlockPacketIndex, iBufferIndex,
               m_uNextVideoBlockIndexToGenerate, m_uNextVideoBlockPacketIndexToGenerate);
            return;
         }
         _sendPacket(iBufferIndex, (int)u, uRetransmissionId, 0);
      }
   }
   else
   {
      if ( m_VideoPackets[iBufferIndex][uVideoBlockPacketIndex].bEmpty )
      {
         log_softerror_and_alarm("[VideoTxBuffer] Recv request for retr of empty video packet [%u/%u], buffer has video block [%u/%u] at that position (%d), next video packet to generate now is: [%u/%u]",
            uVideoBlockIndex, uVideoBlockPacketIndex, m_VideoPackets[iBufferIndex][uVideoBlockPacketIndex].pPHVS->uCurrentBlockIndex, m_VideoPackets[iBufferIndex][uVideoBlockPacketIndex].pPHVS->uCurrentBlockPacketIndex, iBufferIndex,
            m_uNextVideoBlockIndexToGenerate, m_uNextVideoBlockPacketIndexToGenerate);
         return;
      }
      _sendPacket(iBufferIndex, (int)uVideoBlockPacketIndex, uRetransmissionId, 0);
   }
}

void VideoTxPacketsBuffer::resendVideoPacketsFromFrameEnd(u32 uRetransmissionId, u16 uH264FrameIndex, u8 uPacketsToEOF)
{
   int iBufferIndex = m_iNextBufferIndexToFill;
   bool bFrameFound = false;
   bool bMatched = false;
   int iDbgIter = 0;
   while ( true )
   {
      iDbgIter++;
      if ( (m_VideoPackets[iBufferIndex][0].pPHVS != NULL) && (m_VideoPackets[iBufferIndex][0].pPHVS->uH264FrameIndex == uH264FrameIndex) )
      {
         int iCountBlockPackets = m_VideoPackets[iBufferIndex][0].pPHVS->uCurrentBlockDataPackets;
         for( int i=0; i<iCountBlockPackets; i++ )
         {
            if ( (m_VideoPackets[iBufferIndex][i].pPHVS->uFramePacketsInfo & 0xFF) >= uPacketsToEOF )
            {
               bMatched = true;
               _sendPacket(iBufferIndex, i, uRetransmissionId, 0);
            }
         }
         bFrameFound = true;
         if ( ! bMatched )
            break;
      }
      else if ( bFrameFound )
         break;
      iBufferIndex--;
      if ( iBufferIndex < 0 )
         iBufferIndex += MAX_RXTX_BLOCKS_BUFFER;

      if ( m_VideoPackets[iBufferIndex][0].bEmpty )
         break;
      if ( iBufferIndex == m_iNextBufferIndexToFill )
         break;
   }
}

void VideoTxPacketsBuffer::setTelemetryInfoVideoThroughput(u32 uVideoBitsPerSec)
{
   m_uTelemetryVideoBitsPerSec = uVideoBitsPerSec;
}

u32 VideoTxPacketsBuffer::getLastFrameExpectedTxTimeMicros()
{
   return m_uExpectedFrameTransmissionTimeMicros;
}

u32 VideoTxPacketsBuffer::getLastFrameSentDRMin_BPS()
{
   return m_uLastSentVideoPacketDatarateMinBPS;
}

u32 VideoTxPacketsBuffer::getLastFrameSentDRMax_BPS()
{
   return m_uLastSentVideoPacketDatarateMaxBPS;
}

int VideoTxPacketsBuffer::getCurrentUsableRawVideoDataSize()
{
   return m_iUsableRawVideoDataSize;
}

bool VideoTxPacketsBuffer::getResetOverflowFlag()
{
   bool bRet = m_bOverflowFlag;
   m_bOverflowFlag = false;
   return bRet;
}

int VideoTxPacketsBuffer::getCurrentMaxUsableRawVideoDataSize()
{
   return m_iUsableRawVideoDataSize;
}
