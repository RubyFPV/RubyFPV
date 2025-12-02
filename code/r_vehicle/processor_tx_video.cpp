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
#include "../base/models.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../radio/fec.h"
#include "../base/camera_utils.h"
#include "../base/parser_h264.h"
#include "../common/string_utils.h"
#include "shared_vars.h"
#include "timers.h"

#include "processor_tx_video.h"
#include "processor_relay.h"
#include "packets_utils.h"
#include "../utils/utils_vehicle.h"
#include "adaptive_video.h"
#include "video_source_csi.h"
#include "video_source_majestic.h"
#include <semaphore.h>

int ProcessorTxVideo::m_siInstancesCount = 0;
u32 s_lCountBytesVideoIn = 0;
u32 s_lCountBytesSend = 0;

ProcessorTxVideo::ProcessorTxVideo(int iVideoStreamIndex, int iCameraIndex)
:m_bInitialized(false)
{
   m_iInstanceIndex = m_siInstancesCount;
   m_siInstancesCount++;

   m_iVideoStreamIndex = iVideoStreamIndex;
   m_iCameraIndex = iCameraIndex;

   for( int i=0; i<MAX_VIDEO_BITRATE_HISTORY_VALUES; i++ )
   {
      m_BitrateHistorySamples[i].uTimeStampTaken = 0;
      m_BitrateHistorySamples[i].uTotalBitrateBPS = 0;
      m_BitrateHistorySamples[i].uVideoBitrateBPS = 0;
   }
   
   m_uVideoBitrateKbAverageSum = 0;
   m_uTotalVideoBitrateKbAverageSum = 0;
   m_uVideoBitrateAverage = 0;
   m_uTotalVideoBitrateAverage = 0;
   m_uIntervalMSComputeVideoBitrateSample = 50; // Must compute video bitrate averages quite often as they are used for auto adustments of video bitrate and quantization
   m_uTimeLastVideoBitrateSampleTaken = 0;
   m_iVideoBitrateSampleIndex = 0;
}

ProcessorTxVideo::~ProcessorTxVideo()
{
   uninit();
   m_siInstancesCount--;
}

bool ProcessorTxVideo::init()
{
   if ( m_bInitialized )
      return true;

   log_line("[VideoTX] Initialize video processor Tx instance number %d.", m_iInstanceIndex+1);
   m_bInitialized = true;

   return true;
}

bool ProcessorTxVideo::uninit()
{
   if ( ! m_bInitialized )
      return true;

   log_line("[VideoTX] Uninitialize video processor Tx instance number %d.", m_iInstanceIndex+1);
   
   m_bInitialized = false;
   return true;
}


// Returns bps
u32 ProcessorTxVideo::getCurrentVideoBitrate()
{
   return m_BitrateHistorySamples[m_iVideoBitrateSampleIndex].uVideoBitrateBPS;
}

// Returns bps
u32 ProcessorTxVideo::getCurrentVideoBitrateAverage()
{
   return m_uVideoBitrateAverage;
}

// Returns bps
u32 ProcessorTxVideo::getCurrentVideoBitrateAverageLastMs(u32 uMilisec)
{
   u32 uSumKb = 0;
   u32 uCount = 0;
   int iIndex = m_iVideoBitrateSampleIndex;

   u32 uTime0 = m_BitrateHistorySamples[iIndex].uTimeStampTaken;
   
   while ( (uCount < MAX_VIDEO_BITRATE_HISTORY_VALUES) && (m_BitrateHistorySamples[iIndex].uTimeStampTaken >= (uTime0 - uMilisec)) )
   {
      uSumKb += m_BitrateHistorySamples[iIndex].uVideoBitrateBPS/1000;
      uCount++;
      iIndex--;
      if ( iIndex < 0 )
        iIndex = MAX_VIDEO_BITRATE_HISTORY_VALUES-1;
   }
   if ( uCount > 0 )
      return 1000*(uSumKb/uCount);
   return 0;
}

// Returns bps
u32 ProcessorTxVideo::getCurrentTotalVideoBitrate()
{
   return m_BitrateHistorySamples[m_iVideoBitrateSampleIndex].uTotalBitrateBPS;
}

// Returns bps
u32 ProcessorTxVideo::getCurrentTotalVideoBitrateAverage()
{
   return m_uTotalVideoBitrateAverage;
}

// Returns bps
u32 ProcessorTxVideo::getCurrentTotalVideoBitrateAverageLastMs(u32 uMilisec)
{
   u32 uSumKb = 0;
   u32 uCount = 0;
   int iIndex = m_iVideoBitrateSampleIndex;
   
   u32 uTime0 = m_BitrateHistorySamples[iIndex].uTimeStampTaken;
   
   while ( (uCount < MAX_VIDEO_BITRATE_HISTORY_VALUES) && (m_BitrateHistorySamples[iIndex].uTimeStampTaken >= (uTime0 - uMilisec)) )
   {
      uSumKb += m_BitrateHistorySamples[iIndex].uTotalBitrateBPS/1000;
      uCount++;
      iIndex--;
      if ( iIndex < 0 )
        iIndex = MAX_VIDEO_BITRATE_HISTORY_VALUES-1;
   }

   if ( uCount > 0 )
      return 1000*(uSumKb/uCount);
   return 0;
}

void ProcessorTxVideo::periodicLoop()
{
   if ( g_TimeNow < (m_uTimeLastVideoBitrateSampleTaken + m_uIntervalMSComputeVideoBitrateSample) )
      return;

   u32 uDeltaTime = g_TimeNow - m_uTimeLastVideoBitrateSampleTaken;
   m_uTimeLastVideoBitrateSampleTaken = g_TimeNow;

   m_iVideoBitrateSampleIndex++;
   if ( m_iVideoBitrateSampleIndex >= MAX_VIDEO_BITRATE_HISTORY_VALUES )
      m_iVideoBitrateSampleIndex = 0;

   m_uVideoBitrateKbAverageSum -= m_BitrateHistorySamples[m_iVideoBitrateSampleIndex].uVideoBitrateBPS/1000;
   m_uTotalVideoBitrateKbAverageSum -= m_BitrateHistorySamples[m_iVideoBitrateSampleIndex].uTotalBitrateBPS/1000;
  
   if ( 0 != uDeltaTime )
   {
      m_BitrateHistorySamples[m_iVideoBitrateSampleIndex].uVideoBitrateBPS = ((s_lCountBytesVideoIn * 8) / uDeltaTime) * 1000;
      m_BitrateHistorySamples[m_iVideoBitrateSampleIndex].uTotalBitrateBPS = ((s_lCountBytesSend * 8) / uDeltaTime) * 1000;
   }
   m_BitrateHistorySamples[m_iVideoBitrateSampleIndex].uTimeStampTaken = g_TimeNow;

   m_uVideoBitrateKbAverageSum += m_BitrateHistorySamples[m_iVideoBitrateSampleIndex].uVideoBitrateBPS/1000;
   m_uTotalVideoBitrateKbAverageSum += m_BitrateHistorySamples[m_iVideoBitrateSampleIndex].uTotalBitrateBPS/1000;
  
   m_uVideoBitrateAverage = 1000*(m_uVideoBitrateKbAverageSum/MAX_VIDEO_BITRATE_HISTORY_VALUES);
   m_uTotalVideoBitrateAverage = 1000*(m_uTotalVideoBitrateKbAverageSum/MAX_VIDEO_BITRATE_HISTORY_VALUES);
   
   s_lCountBytesVideoIn = 0;
   s_lCountBytesSend = 0;
}

void _log_encoding_scheme()
{
 /*
   if ( g_TimeNow > s_TimeLastEncodingSchemeLog + 5000 )
   {
      s_TimeLastEncodingSchemeLog = g_TimeNow;
      char szScheme[64];
      char szVideoStream[64];
      strcpy(szScheme, "[Unknown]");
      strcpy(szVideoStream, "[Unknown]");

      // lower 4 bits: current video profile
      // higher 4 bits: user selected video profile

      strcpy(szScheme, str_get_video_profile_name(s_CurrentPHVF.video_link_profile & 0x0F));
      
      sprintf(szVideoStream, "[%d]", (s_CurrentPHVF.video_link_profile>>4) & 0x0F );
      u32 uValueDup = (s_CurrentPHVF.uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT) >> 16;

      log_line("New current video profile used: %s (video stream Id: %s), extra params: 0x%08X, (for video res %d x %d, %d FPS, %d ms keyframe):",
         szScheme, szVideoStream, s_CurrentPHVF.uProfileEncodingFlags, s_CurrentPHVF.video_width, s_CurrentPHVF.video_height, s_CurrentPHVF.video_fps, s_CurrentPHVF.video_keyframe_interval_ms );
      log_line("New encoding scheme used: %d/%d block data/ECs, video data length: %d, R-data/R-retr: %d/%d",
         s_CurrentPHVF.block_packets, s_CurrentPHVF.block_fecs, s_CurrentPHVF.video_data_length,
         (uValueDup & 0x0F), ((uValueDup >> 4) & 0x0F) );
      log_line("Encoding change (%u times) active starting with stream packet: %u, video block index: %u, video packet index: %u", s_uCountEncodingChanges,
         (s_CurrentPH.stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX), s_CurrentPHVF.video_block_index, s_CurrentPHVF.video_block_packet_index);
      return;
   }

   return;
   log_line("New encoding scheme: [%u/%u], %d/%d, %d/%d/%d", s_CurrentPHVF.video_block_index, s_CurrentPHVF.video_block_packet_index,
      (s_CurrentPHVF.video_link_profile>>4) & 0x0F, s_CurrentPHVF.video_link_profile & 0x0F,
         s_CurrentPHVF.block_packets, s_CurrentPHVF.block_fecs, s_CurrentPHVF.video_data_length);
*/
}


bool process_data_tx_video_command(int iRadioInterface, u8* pPacketBuffer)
{
   if ( ! g_bReceivedPairingRequest )
      return false;

   if ( g_bVideoPaused )
   {
      log_line("[TxVideoProc] Video is paused (due to update). Do not process video commands.");
      return false;
   }

   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;

   if ( pPH->packet_type == PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS )
   {
      static u32 s_uLastRecvRetransmissionId = 0;
      static u32 s_uTimeLastRetransmissionRequest = 0;
      u32 uRetrId = 0;
      memcpy(&uRetrId, &pPacketBuffer[sizeof(t_packet_header)], sizeof(u32));
      u8 uFlags = pPacketBuffer[sizeof(t_packet_header) + sizeof(u32) + sizeof(u8)];
      u8 uCount = pPacketBuffer[sizeof(t_packet_header) + sizeof(u32) + 2*sizeof(u8)];
      
      if ( uRetrId == s_uLastRecvRetransmissionId )
      {
         log_line("[TxVideoProc] Received duplicate retr request id %u from controller for %d packets, flags: %s %s, last request was %u ms ago. Ignored.",
            uRetrId, (int)uCount, (uFlags & 0x01)?"has re-requested packets":"", (uFlags & (0x01<<2))?"has frame eof request":"",
            g_TimeNow - s_uTimeLastRetransmissionRequest);
         s_uTimeLastRetransmissionRequest = g_TimeNow;
         return false;
      }

      log_line("[TxVideoProc] Received retr request id %u from controller for %d packets, flags: %s %s, lost retransmissions requests: %d, last request was %u ms ago.",
         uRetrId, (int)uCount, (uFlags & 0x01)?"has re-requested packets":"", (uFlags & (0x01<<2))?"has frame eof request":"",
         uRetrId - s_uLastRecvRetransmissionId - 1, g_TimeNow - s_uTimeLastRetransmissionRequest);
      s_uTimeLastRetransmissionRequest = g_TimeNow;

      int iCounter = 0;
      while ( iCounter < 3 )
      {
         if ( iCounter > 0 )
            log_line("[TxVideoProc] Duplicate the retransmission id %u", uRetrId);
         u8* pDataPackets = pPacketBuffer + sizeof(t_packet_header) + sizeof(u32) + 3*sizeof(u8);
         for( int i=0; i<(int)uCount; i++ )
         {
            u32 uBlockId = 0;
            u8 uPacketIndex = 0;
            memcpy(&uBlockId, pDataPackets, sizeof(u32));
            pDataPackets += sizeof(u32);
            uPacketIndex = (u8) *pDataPackets;
            pDataPackets++;
            if ( uPacketIndex == 0xFF )
               log_line("[TxVideoProc] Received request for full video block [%u] in retr id %u", uBlockId, uRetrId);
            g_pVideoTxBuffers->resendVideoPacket(uRetrId, uBlockId, (u32)uPacketIndex);
         }
         if ( uFlags & (0x01<<2) )
         {
             u16 uFrameIndex = 0;
             memcpy(&uFrameIndex, pDataPackets, sizeof(u16));
             pDataPackets += sizeof(u16);
             log_line("[TxVideoProc] Received request for end of frame f%d retransmission in retr id %u", uFrameIndex, uRetrId);
             g_pVideoTxBuffers->resendVideoPacketsFromFrameEnd(uRetrId, uFrameIndex, *pDataPackets);
         }

         iCounter++;
         if ( iCounter > 1 )
            break;

         bool bDuplicate = false;
         int iMaxCountThreshold = 5;
         if ( uFlags & 0x01 )
            iMaxCountThreshold = 8;

         if ( (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAG_RETRANSMISSIONS_AGGRESIVE) || (uFlags & 0x01) )
         if ( (s_uLastRecvRetransmissionId != uRetrId) && (uCount < iMaxCountThreshold) )
            bDuplicate = true;
         if ( uCount < iMaxCountThreshold )
         if ( uFlags & 0x01 )
            bDuplicate = true;

         if ( ! bDuplicate )
            break;
      }
      s_uLastRecvRetransmissionId = uRetrId;
   }

   if ( pPH->packet_type == PACKET_TYPE_VIDEO_ADAPTIVE_VIDEO_PARAMS )
   {
      if ( pPH->total_length < sizeof(t_packet_header) + 2*sizeof(u32) + 3*sizeof(u8) + 2*sizeof(int) + sizeof(u16) )
         return true;
      
      u32 uRequestId = 0;
      u8 uFlags = 0;
      u8 uVideoStreamIndex = 0;
      u32 uVideoBitrate = 0;
      u16 uEC = 0;
      int iRadioDatarate = 0;
      int iKeyframeMS = 0;
      u8 uDRBoost = 0;
      u8* pData = pPacketBuffer + sizeof(t_packet_header);
      memcpy( &uRequestId, pData, sizeof(u32));
      pData += sizeof(u32);
      memcpy( &uFlags, pData, sizeof(u8));
      pData += sizeof(u8);
      memcpy( &uVideoStreamIndex, pData, sizeof(u8));
      pData += sizeof(u8);
      memcpy( &uVideoBitrate, pData, sizeof(u32));

      pData += sizeof(u32);
      memcpy( &uEC, pData, sizeof(u16));
      pData += sizeof(u16);
      memcpy( &iRadioDatarate, pData, sizeof(int));
      pData += sizeof(int);
      memcpy( &iKeyframeMS, pData, sizeof(int));
      pData += sizeof(int);
      memcpy( &uDRBoost, pData, sizeof(u8));
      pData += sizeof(u8);

      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_VIDEO, PACKET_TYPE_VIDEO_ADAPTIVE_VIDEO_PARAMS_ACK, STREAM_ID_VIDEO_1);
      PH.packet_flags |= PACKET_FLAGS_BIT_HIGH_PRIORITY;
      PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      PH.vehicle_id_dest = g_uControllerId;
      PH.total_length = sizeof(t_packet_header) + sizeof(u32);
      u8 packet[MAX_PACKET_TOTAL_SIZE];
      memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
      memcpy(packet+sizeof(t_packet_header), &uRequestId, sizeof(u32));
      if ( PH.packet_flags & PACKET_FLAGS_BIT_HIGH_PRIORITY )
         send_packet_to_radio_interfaces(packet, PH.total_length, -1);
      else
         packets_queue_add_packet(&g_QueueRadioPacketsOut, packet);

      adaptive_video_on_message_from_controller(uRequestId, uFlags, uVideoBitrate, uEC, uVideoStreamIndex, iRadioDatarate, iKeyframeMS, uDRBoost);
      return true;
   }
   return false;
}

bool process_data_tx_video_loop()
{
   g_pProcessorTxVideo->periodicLoop();
   return true;
}

void process_data_tx_video_on_new_data(u8* pData, int iDataSize)
{
   s_lCountBytesVideoIn += iDataSize;
}

void process_data_tx_video_on_data_sent(int iDataSize)
{
   s_lCountBytesSend += iDataSize;
}