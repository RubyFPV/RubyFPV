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

#include "video_rx_buffers.h"
#include "shared_vars.h"
#include "timers.h"
#include "packets_utils.h"
#include "../radio/fec.h"

int VideoRxPacketsBuffer::m_siVideoBuffersInstancesCount = 0;

VideoRxPacketsBuffer::VideoRxPacketsBuffer(int iVideoStreamIndex, int iCameraIndex)
:m_bInitialized(false)
{
   m_iInstanceIndex = m_siVideoBuffersInstancesCount;
   m_siVideoBuffersInstancesCount++;

   m_iVideoStreamIndex = iVideoStreamIndex;
   m_iCameraIndex = iCameraIndex;

   for( int i=0; i<MAX_RXTX_BLOCKS_BUFFER; i++ )
   {
      _empty_block_buffer_index(i);
      for( int k=0; k<MAX_TOTAL_PACKETS_IN_BLOCK; k++ )
      {
         m_VideoBlocks[i].packets[k].pRawData = NULL;
         m_VideoBlocks[i].packets[k].pVideoData = NULL;
         m_VideoBlocks[i].packets[k].pPH = NULL;
         m_VideoBlocks[i].packets[k].pPHVS = NULL;
         m_VideoBlocks[i].packets[k].pPHVSImp = NULL;
      }
   }
   m_bBuffersEmpty = true;
   m_iTopBufferIndex = 0;
   m_iBottomBufferIndex = 0;
   m_iBottomBufferPacketIndex = 0;
}

VideoRxPacketsBuffer::~VideoRxPacketsBuffer()
{
   uninit();

   for( int i=0; i<MAX_RXTX_BLOCKS_BUFFER; i++ )
   for( int k=0; k<MAX_TOTAL_PACKETS_IN_BLOCK; k++ )
   {
      if ( NULL != m_VideoBlocks[i].packets[k].pRawData )
         free(m_VideoBlocks[i].packets[k].pRawData);

      m_VideoBlocks[i].packets[k].pRawData = NULL;
      m_VideoBlocks[i].packets[k].pVideoData = NULL;
      m_VideoBlocks[i].packets[k].pPH = NULL;
      m_VideoBlocks[i].packets[k].pPHVS = NULL;
      m_VideoBlocks[i].packets[k].pPHVSImp = NULL;
   }

   m_siVideoBuffersInstancesCount--;
}

bool VideoRxPacketsBuffer::init(Model* pModel)
{
   if ( m_bInitialized )
      return true;

   if ( NULL == pModel )
   {
      log_softerror_and_alarm("[VideoRXBuffer] Invalid model on init.");
      return false;
   }
   log_line("[VideoRXBuffer] Initialize video Rx buffer instance number %d.", m_iInstanceIndex+1);
   _empty_buffers("init", NULL, NULL);
   m_bInitialized = true;
   log_line("[VideoRXBuffer] Initialized video Tx buffer instance number %d.", m_iInstanceIndex+1);
   return true;
}

bool VideoRxPacketsBuffer::uninit()
{
   if ( ! m_bInitialized )
      return true;

   log_line("[VideoRXBuffer] Uninitialize video Tx buffer instance number %d.", m_iInstanceIndex+1);
   
   m_bInitialized = false;
   return true;
}

void VideoRxPacketsBuffer::emptyBuffers(const char* szReason)
{
   _empty_buffers(szReason, NULL, NULL);
}

bool VideoRxPacketsBuffer::_check_allocate_video_block_in_buffer(int iBufferIndex)
{
   if ( (iBufferIndex < 0) || (iBufferIndex >= MAX_RXTX_BLOCKS_BUFFER) )
      return false;

   for( int i=0; i<m_VideoBlocks[iBufferIndex].iBlockDataPackets + m_VideoBlocks[iBufferIndex].iBlockECPackets; i++ )
   {
      if ( NULL != m_VideoBlocks[iBufferIndex].packets[i].pRawData )
         continue;

      u8* pRawData = (u8*)malloc(MAX_PACKET_TOTAL_SIZE);
      if ( NULL == pRawData )
      {
         log_error_and_alarm("[VideoRXBuffer] Failed to allocate packet, buffer index: %d, packet index %d", iBufferIndex, i);
         return false;
      }
      m_VideoBlocks[iBufferIndex].packets[i].pRawData = pRawData;
      m_VideoBlocks[iBufferIndex].packets[i].pVideoData = pRawData + sizeof(t_packet_header) + sizeof(t_packet_header_video_segment);
      m_VideoBlocks[iBufferIndex].packets[i].pPH = (t_packet_header*)pRawData;
      m_VideoBlocks[iBufferIndex].packets[i].pPHVS = (t_packet_header_video_segment*)(pRawData + sizeof(t_packet_header));
      m_VideoBlocks[iBufferIndex].packets[i].pPHVSImp = (t_packet_header_video_segment_important*)(pRawData + sizeof(t_packet_header) + sizeof(t_packet_header_video_segment));

      m_VideoBlocks[iBufferIndex].packets[i].pPHVS->uCurrentBlockDataPackets = m_VideoBlocks[iBufferIndex].iBlockDataPackets;
      m_VideoBlocks[iBufferIndex].packets[i].pPHVS->uCurrentBlockECPackets = m_VideoBlocks[iBufferIndex].iBlockECPackets;
      _empty_block_buffer_packet_index(iBufferIndex, i);
   }
   return true;
}

void VideoRxPacketsBuffer::_empty_block_buffer_packet_index(int iBufferIndex, int iPacketIndex)
{
   m_VideoBlocks[iBufferIndex].packets[iPacketIndex].uReceivedTime = 0;
   m_VideoBlocks[iBufferIndex].packets[iPacketIndex].uRequestedTime = 0;
   m_VideoBlocks[iBufferIndex].packets[iPacketIndex].bEmpty = true;
   m_VideoBlocks[iBufferIndex].packets[iPacketIndex].bReconstructed = false;
}

void VideoRxPacketsBuffer::_empty_block_buffer_index(int iBufferIndex)
{
   m_VideoBlocks[iBufferIndex].uH264FrameIndex = 0;
   m_VideoBlocks[iBufferIndex].uVideoBlockIndex = 0;
   m_VideoBlocks[iBufferIndex].uReceivedTime = 0;
   m_VideoBlocks[iBufferIndex].iTotalFramePackets = 0;
   m_VideoBlocks[iBufferIndex].iFramePacketStart = -1;
   m_VideoBlocks[iBufferIndex].iFramePacketEnd = -1;
   m_VideoBlocks[iBufferIndex].iMaxReceivedDataOrECPacketIndex = -1;
   m_VideoBlocks[iBufferIndex].iLastRecordedEOF = -1;
   m_VideoBlocks[iBufferIndex].iBlockDataSize = 0;
   m_VideoBlocks[iBufferIndex].iBlockDataPackets = 0;
   m_VideoBlocks[iBufferIndex].iBlockECPackets = 0;
   m_VideoBlocks[iBufferIndex].iRecvDataPackets = 0;
   m_VideoBlocks[iBufferIndex].iRecvECPackets = 0;
   m_VideoBlocks[iBufferIndex].iReconstructedECUsed = 0;
   for( int k=0; k<MAX_TOTAL_PACKETS_IN_BLOCK; k++ )
   {
      _empty_block_buffer_packet_index(iBufferIndex, k);
   }
}

void VideoRxPacketsBuffer::_empty_buffers(const char* szReason, t_packet_header* pPH, t_packet_header_video_segment* pPHVS)
{  
   char szLog[256];
   if ( NULL == szReason )
      strcpy(szLog, "[VRXBuffers] Empty buffers: (no reason)");
   else
      sprintf(szLog, "[VRXBuffers] Empty buffers: (%s)", szReason);

   if ( (NULL == pPH) || (NULL == pPHVS) )
      log_softerror_and_alarm("%s (no additional data)", szLog);
   else
   {
      log_softerror_and_alarm("%s (while processing recv video packet [%u/%u] (retransmitted: %s), fr index: %u, fr packet %d/%d, stream id/packet index: %u, radio index: %u)",
         szLog, pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex,
         (pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED)?"yes":"no",
         pPHVS->uH264FrameIndex, pPHVS->uFramePacketsInfo & 0xFF, pPHVS->uFramePacketsInfo >> 8,
         (pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_INDEX) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX,
         pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX, pPH->radio_link_packet_index);
   }

   if ( m_bBuffersEmpty )
   {
      log_softerror_and_alarm("[VRXBuffers] Empty buffers: Already empty. Do nothing.");
      return;
   }

   log_softerror_and_alarm("[VRXBuffers] Empty buffers: State: bottom buffer index: %d/%d, bottom video block: %u, emtpy? %s, top buffer index: %d, top video block: %u, empty? %s, max recv block packet index: %d",
      m_iBottomBufferIndex, m_iBottomBufferPacketIndex, m_VideoBlocks[m_iBottomBufferIndex].uVideoBlockIndex,
      (m_VideoBlocks[m_iBottomBufferIndex].iBlockDataPackets == 0)?"yes":"no",
      m_iTopBufferIndex, m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex,
      (m_VideoBlocks[m_iTopBufferIndex].iBlockDataPackets == 0)?"yes":"no",
      m_VideoBlocks[m_iTopBufferIndex].iMaxReceivedDataOrECPacketIndex);

   for( int i=0; i<MAX_RXTX_BLOCKS_BUFFER; i++ )
      _empty_block_buffer_index(i);

   g_SMControllerRTInfo.uOutputedVideoBlocksSkippedBlocks[g_SMControllerRTInfo.iCurrentIndex]++;
   if ( g_TimeNow > g_TimeLastVideoParametersOrProfileChanged + 3000 )
   if ( g_TimeNow > g_TimeStart + 5000 )
      g_SMControllerRTInfo.uTotalCountOutputSkippedBlocks++;

   m_bBuffersEmpty = true;
   m_iTopBufferIndex = 0;
   m_iBottomBufferIndex = 0;
   m_iBottomBufferPacketIndex = 0;
   log_line("[VRXBuffers] Done emptying buffers.");
}

void VideoRxPacketsBuffer::_check_do_ec_for_video_block(int iBufferIndex)
{
   if ( (iBufferIndex < 0) || (iBufferIndex >= MAX_RXTX_BLOCKS_BUFFER) )
      return;

   if ( m_VideoBlocks[iBufferIndex].iRecvDataPackets >= m_VideoBlocks[iBufferIndex].iBlockDataPackets )
      return;

   if ( 0 == m_VideoBlocks[iBufferIndex].iBlockECPackets )
      return;

   if ( m_VideoBlocks[iBufferIndex].iRecvDataPackets + m_VideoBlocks[iBufferIndex].iRecvECPackets < m_VideoBlocks[iBufferIndex].iBlockDataPackets )
      return;

   m_VideoBlocks[iBufferIndex].iReconstructedECUsed = m_VideoBlocks[iBufferIndex].iBlockDataPackets - m_VideoBlocks[iBufferIndex].iRecvDataPackets;

   int iPacketIndexGood = -1;

   // Add existing data packets, mark and count the ones that are missing
   // Find a good PH, PHVS and video-debug-info (if any) in the block to reuse it in reconstruction

   m_ECRxInfo.missing_packets_count = 0;
   for( int i=0; i<m_VideoBlocks[iBufferIndex].iBlockDataPackets; i++ )
   {
      m_ECRxInfo.p_decode_data_packets_pointers[i] = m_VideoBlocks[iBufferIndex].packets[i].pVideoData;

      if ( m_VideoBlocks[iBufferIndex].packets[i].bEmpty )
      {
         m_ECRxInfo.decode_missing_packets_indexes[m_ECRxInfo.missing_packets_count] = i;
         m_ECRxInfo.missing_packets_count++;
      }
      else if ( -1 == iPacketIndexGood )
         iPacketIndexGood = i;
   }

   // Add the needed FEC packets to the list
   int pos = 0;
   int iECDelta = m_VideoBlocks[iBufferIndex].iBlockDataPackets;
   for( int i=0; i<m_VideoBlocks[iBufferIndex].iBlockECPackets; i++ )
   {
      if ( ! m_VideoBlocks[iBufferIndex].packets[i+iECDelta].bEmpty )
      {
         if ( -1 == iPacketIndexGood )
            iPacketIndexGood = i+iECDelta;

         m_ECRxInfo.p_decode_ec_packets_pointers[pos] = m_VideoBlocks[iBufferIndex].packets[i+iECDelta].pVideoData;
         m_ECRxInfo.decode_ec_packets_indexes[pos] = i;
         pos++;
         if ( pos == (int)(m_ECRxInfo.missing_packets_count) )
            break;
      }
   }

   if ( -1 == iPacketIndexGood )
      return;

   t_packet_header* pPHGood = m_VideoBlocks[iBufferIndex].packets[iPacketIndexGood].pPH;
   t_packet_header_video_segment* pPHVSGood = m_VideoBlocks[iBufferIndex].packets[iPacketIndexGood].pPHVS;

   int iRes = fec_decode(m_VideoBlocks[iBufferIndex].iBlockDataSize, m_ECRxInfo.p_decode_data_packets_pointers, m_VideoBlocks[iBufferIndex].iBlockDataPackets, m_ECRxInfo.p_decode_ec_packets_pointers, m_ECRxInfo.decode_ec_packets_indexes, m_ECRxInfo.decode_missing_packets_indexes, m_ECRxInfo.missing_packets_count);
   if ( iRes < 0 )
   {
      log_softerror_and_alarm("[VideoRXBuffer] Failed to decode video block [%u], type %d/%d/%d bytes; max data recv index: %d, max data/ec received index: %d, recv: %d/%d packets, missing count: %d",
        m_VideoBlocks[iBufferIndex].uVideoBlockIndex,
        m_VideoBlocks[iBufferIndex].iBlockDataPackets, m_VideoBlocks[iBufferIndex].iBlockECPackets,
        m_VideoBlocks[iBufferIndex].iBlockDataSize,
        m_VideoBlocks[iBufferIndex].iMaxReceivedDataPacketIndex,
        m_VideoBlocks[iBufferIndex].iMaxReceivedDataOrECPacketIndex,
        m_VideoBlocks[iBufferIndex].iRecvDataPackets, m_VideoBlocks[iBufferIndex].iRecvECPackets,
        m_ECRxInfo.missing_packets_count);
   }

   // Mark all data packets reconstructed as received, set the right info in them (packet header info and video packet header info)
   for( int i=0; i<(int)(m_ECRxInfo.missing_packets_count); i++ )
   {
      int iPacketIndexToFix = m_ECRxInfo.decode_missing_packets_indexes[i];
      m_VideoBlocks[iBufferIndex].packets[iPacketIndexToFix].bEmpty = false;
      m_VideoBlocks[iBufferIndex].packets[iPacketIndexToFix].bReconstructed = true;
      m_VideoBlocks[iBufferIndex].packets[iPacketIndexToFix].uReceivedTime = g_TimeNow;
      m_VideoBlocks[iBufferIndex].iRecvDataPackets++;
      if ( iPacketIndexToFix > m_VideoBlocks[iBufferIndex].iMaxReceivedDataPacketIndex )
         m_VideoBlocks[iBufferIndex].iMaxReceivedDataPacketIndex = iPacketIndexToFix;
      if ( iPacketIndexToFix > m_VideoBlocks[iBufferIndex].iMaxReceivedDataOrECPacketIndex )
         m_VideoBlocks[iBufferIndex].iMaxReceivedDataOrECPacketIndex = iPacketIndexToFix;


      t_packet_header* pPHToFix = m_VideoBlocks[iBufferIndex].packets[iPacketIndexToFix].pPH;
      t_packet_header_video_segment* pPHVSToFix = m_VideoBlocks[iBufferIndex].packets[iPacketIndexToFix].pPHVS;
      memcpy(pPHToFix, pPHGood, sizeof(t_packet_header));
      memcpy(pPHVSToFix, pPHVSGood, sizeof(t_packet_header_video_segment));
      
      // Fix packet header
      pPHToFix->total_length = sizeof(t_packet_header) + sizeof(t_packet_header_video_segment) + pPHVSGood->uCurrentBlockPacketSize;
      pPHToFix->packet_flags &= ~PACKET_FLAGS_BIT_RETRANSMITED;
      u32 uStreamPacketIndex = pPHGood->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX;
      uStreamPacketIndex += (iPacketIndexToFix-iPacketIndexGood);
      pPHToFix->stream_packet_idx = (pPHGood->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_INDEX) | ( uStreamPacketIndex & PACKET_FLAGS_MASK_STREAM_PACKET_IDX);
      pPHToFix->radio_link_packet_index = pPHGood->radio_link_packet_index + (iPacketIndexToFix-iPacketIndexGood);

      // Fix video segment packet header
      pPHVSToFix->uStreamInfoFlags = 0;
      pPHVSToFix->uStreamInfo = 0;

      pPHVSToFix->uCurrentBlockPacketSize = pPHVSGood->uCurrentBlockPacketSize;
      pPHVSToFix->uCurrentBlockECPackets = pPHVSGood->uCurrentBlockECPackets;
      pPHVSToFix->uCurrentBlockIndex = pPHVSGood->uCurrentBlockIndex;
      pPHVSToFix->uCurrentBlockPacketIndex = iPacketIndexToFix;
      pPHVSToFix->uCurrentBlockDataPackets = pPHVSGood->uCurrentBlockDataPackets;
      pPHVSToFix->uH264FrameIndex = pPHVSGood->uH264FrameIndex;
      pPHVSToFix->uFramePacketsInfo = ((pPHVSGood->uFramePacketsInfo >> 8) & 0xFF) << 8;
      pPHVSToFix->uFramePacketsInfo |= (pPHVSGood->uFramePacketsInfo & 0xFF) + iPacketIndexToFix - iPacketIndexGood;

      pPHVSToFix->uVideoStatusFlags2 |= VIDEO_STATUS_FLAGS2_WAS_RECONSTRUCTED;
   }
}

bool VideoRxPacketsBuffer::_add_video_packet_to_buffer(int iBufferIndex, u8* pPacket, int iPacketLength)
{
   if ( (NULL == pPacket) || (iPacketLength < (int)(sizeof(t_packet_header)+sizeof(t_packet_header_video_segment) + sizeof(t_packet_header_video_segment_important))) || (iBufferIndex < 0) || (iBufferIndex >= MAX_RXTX_BLOCKS_BUFFER) )
      return false;

   t_packet_header* pPH = (t_packet_header*)pPacket;
   t_packet_header_video_segment* pPHVS = (t_packet_header_video_segment*)(pPacket + sizeof(t_packet_header));
   t_packet_header_video_segment_important* pPHVSImp = (t_packet_header_video_segment_important*)(pPacket + sizeof(t_packet_header) + sizeof(t_packet_header_video_segment));

   if ( NULL != m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].pRawData )
   if ( ! m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].bEmpty )
      return false;

   // Set basic video block size info before check allocate block in buffer as it needs block info about packets
   m_VideoBlocks[iBufferIndex].uH264FrameIndex = pPHVS->uH264FrameIndex;
   m_VideoBlocks[iBufferIndex].iTotalFramePackets = pPHVS->uFramePacketsInfo >> 8;

   if ( pPHVS->uCurrentBlockPacketIndex < pPHVS->uCurrentBlockDataPackets )
      m_VideoBlocks[iBufferIndex].iFramePacketStart = (pPHVS->uFramePacketsInfo & 0xFF) - pPHVS->uCurrentBlockPacketIndex;
   else
      m_VideoBlocks[iBufferIndex].iFramePacketStart = (pPHVS->uFramePacketsInfo & 0xFF) - (pPHVS->uCurrentBlockDataPackets-1);
   m_VideoBlocks[iBufferIndex].iFramePacketEnd = m_VideoBlocks[iBufferIndex].iFramePacketStart + pPHVS->uCurrentBlockDataPackets - 1;

   m_VideoBlocks[iBufferIndex].uVideoBlockIndex = pPHVS->uCurrentBlockIndex;
   m_VideoBlocks[iBufferIndex].iLastRecordedEOF = pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_MASK_EOF_COUNTER;

   if ( (0 == m_VideoBlocks[iBufferIndex].iBlockDataPackets) && (0 == m_VideoBlocks[iBufferIndex].iBlockECPackets) )
   {
      m_VideoBlocks[iBufferIndex].iBlockDataPackets = pPHVS->uCurrentBlockDataPackets;
      m_VideoBlocks[iBufferIndex].iBlockECPackets = pPHVS->uCurrentBlockECPackets;
      m_VideoBlocks[iBufferIndex].iBlockDataSize = pPHVS->uCurrentBlockPacketSize;
   }
   if ( (pPHVS->uCurrentBlockDataPackets > m_VideoBlocks[iBufferIndex].iBlockDataPackets) ||
        (pPHVS->uCurrentBlockPacketSize != m_VideoBlocks[iBufferIndex].iBlockDataSize) )
   {
      char szLog[128];
      sprintf(szLog, "invalid video block: scheme changed from [%d/%d]-%d to [%u/%u]-%u",
        m_VideoBlocks[iBufferIndex].iBlockDataPackets, m_VideoBlocks[iBufferIndex].iBlockECPackets, m_VideoBlocks[iBufferIndex].iBlockDataSize,
        pPHVS->uCurrentBlockDataPackets, pPHVS->uCurrentBlockECPackets, pPHVS->uCurrentBlockPacketSize);
      _empty_buffers(szLog, pPH, pPHVS);
      return false;
   }

   if ( pPHVS->uCurrentBlockPacketIndex >= m_VideoBlocks[iBufferIndex].iMaxReceivedDataOrECPacketIndex )
   if ( m_VideoBlocks[iBufferIndex].iBlockDataPackets != pPHVS->uCurrentBlockDataPackets )
   {
      m_VideoBlocks[iBufferIndex].iBlockDataPackets = pPHVS->uCurrentBlockDataPackets;
      m_VideoBlocks[iBufferIndex].iBlockECPackets = pPHVS->uCurrentBlockECPackets;

      for(u8 u=0; u<pPHVS->uCurrentBlockDataPackets+pPHVS->uCurrentBlockECPackets; u++)
      {
         if ( NULL != m_VideoBlocks[iBufferIndex].packets[u].pRawData )
         {
            m_VideoBlocks[iBufferIndex].packets[u].pPHVS->uCurrentBlockDataPackets = pPHVS->uCurrentBlockDataPackets;
            m_VideoBlocks[iBufferIndex].packets[u].pPHVS->uCurrentBlockECPackets = pPHVS->uCurrentBlockECPackets;
         }
      }
   }

   if ( ! _check_allocate_video_block_in_buffer(iBufferIndex) )
      return false;

   if ( m_bBuffersEmpty )
      log_line("[VRXBuffers] Start adding video packets to empty buffer. Adding [%u/%u] at buffer index %d",
         pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex, m_iTopBufferIndex);
   m_bBuffersEmpty = false;
   
   m_VideoBlocks[iBufferIndex].uReceivedTime = g_TimeNow;

   if ( pPHVS->uCurrentBlockPacketIndex < pPHVS->uCurrentBlockDataPackets )
   {
      m_VideoBlocks[iBufferIndex].iRecvDataPackets++;
      if ( (int)pPHVS->uCurrentBlockPacketIndex > m_VideoBlocks[iBufferIndex].iMaxReceivedDataPacketIndex )
         m_VideoBlocks[iBufferIndex].iMaxReceivedDataPacketIndex = (int)pPHVS->uCurrentBlockPacketIndex;
      if ( (int)pPHVS->uCurrentBlockPacketIndex > m_VideoBlocks[iBufferIndex].iMaxReceivedDataOrECPacketIndex )
         m_VideoBlocks[iBufferIndex].iMaxReceivedDataOrECPacketIndex = (int)pPHVS->uCurrentBlockPacketIndex;
   }
   else
   {
      m_VideoBlocks[iBufferIndex].iRecvECPackets++;
      if ( (int)pPHVS->uCurrentBlockPacketIndex > m_VideoBlocks[iBufferIndex].iMaxReceivedDataOrECPacketIndex )
         m_VideoBlocks[iBufferIndex].iMaxReceivedDataOrECPacketIndex = (int)pPHVS->uCurrentBlockPacketIndex;
   }

   m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].uReceivedTime = g_TimeNow;
   m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].bEmpty = false;
   m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].bReconstructed = false;
   
   memcpy(m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].pRawData, pPacket, iPacketLength);
   
   // Set remaining empty space to 0 as EC uses the good video data packets too.
   if ( pPHVS->uCurrentBlockPacketIndex < pPHVS->uCurrentBlockDataPackets )
   {
      u8* pVideoSource = m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].pVideoData;
      pVideoSource += sizeof(t_packet_header_video_segment_important);
      pVideoSource += pPHVSImp->uVideoDataLength;
      int iSizeToZero = MAX_PACKET_TOTAL_SIZE - sizeof(t_packet_header) - sizeof(t_packet_header_video_segment) - sizeof(t_packet_header_video_segment_important) - pPHVSImp->uVideoDataLength;
      if ( iSizeToZero > 0 )
         memset(pVideoSource, 0, iSizeToZero);
   }
   _check_do_ec_for_video_block(iBufferIndex);
   return true;
}

bool VideoRxPacketsBuffer::hasVideoPacket(u32 uVideoBlockIndex, u32 uVideoBlockPacketIndex)
{
   if ( m_bBuffersEmpty )
      return false;
   if ( (-1 != m_iTopBufferIndex) && (uVideoBlockIndex > m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex) )
      return false;

   if ( (-1 != m_iBottomBufferIndex) && (uVideoBlockIndex < m_VideoBlocks[m_iBottomBufferIndex].uVideoBlockIndex) )
      return false;

   u32 uDiffBlocks = uVideoBlockIndex - m_VideoBlocks[m_iBottomBufferIndex].uVideoBlockIndex;
   int iBufferIndex = m_iBottomBufferIndex + (int) uDiffBlocks;
   iBufferIndex = iBufferIndex % MAX_RXTX_BLOCKS_BUFFER;

   if ( m_VideoBlocks[iBufferIndex].packets[uVideoBlockPacketIndex].bEmpty )
      return false;
   return true;
}

// Returns true if the packet was added
bool VideoRxPacketsBuffer::checkAddVideoPacket(u8* pPacket, int iPacketLength)
{
   if ( (NULL == pPacket) || (iPacketLength <= (int)(sizeof(t_packet_header)+sizeof(t_packet_header_video_segment) + sizeof(t_packet_header_video_segment_important))) )
      return false;

   t_packet_header* pPH = (t_packet_header*)pPacket;
   t_packet_header_video_segment* pPHVS = (t_packet_header_video_segment*)(pPacket + sizeof(t_packet_header));

   // Empty buffers?
   if ( m_bBuffersEmpty )
   {
      // Wait for start of a recoverable block
      if ( (pPHVS->uCurrentBlockECPackets != 0) && (pPHVS->uCurrentBlockPacketIndex > (pPHVS->uCurrentBlockECPackets*2)/3) )
         return false;
      return _add_video_packet_to_buffer(m_iTopBufferIndex, pPacket, iPacketLength);
   }

   // Non-empty buffers from here on

   // On the current top block?
   if ( pPHVS->uCurrentBlockIndex == m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex )
      return _add_video_packet_to_buffer(m_iTopBufferIndex, pPacket, iPacketLength);

   // Too old ?
   if ( pPHVS->uCurrentBlockIndex < m_VideoBlocks[m_iBottomBufferIndex].uVideoBlockIndex )
      return false;
   if ( pPHVS->uCurrentBlockIndex == m_VideoBlocks[m_iBottomBufferIndex].uVideoBlockIndex )
   if ( pPHVS->uCurrentBlockPacketIndex < m_iBottomBufferPacketIndex )
      return false;

   // Compute difference from top for blocks before top block
   if ( pPHVS->uCurrentBlockIndex < m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex )
   {
      u32 uDiffBlocks = m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex - pPHVS->uCurrentBlockIndex;
      // Too old
      if ( uDiffBlocks >= MAX_RXTX_BLOCKS_BUFFER - 1 )
         return false;

      int iBufferIndex = m_iTopBufferIndex - (int) uDiffBlocks;
      if ( iBufferIndex < 0 )
         iBufferIndex += MAX_RXTX_BLOCKS_BUFFER;

      return _add_video_packet_to_buffer(iBufferIndex, pPacket, iPacketLength);
   }

   // Only future blocks (greater than top) from here on

   u32 uDiffBlocks = pPHVS->uCurrentBlockIndex - m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex;
   if ( (int)uDiffBlocks >= MAX_RXTX_BLOCKS_BUFFER - getCountBlocksInBuffer() - 1 )
   {
      char szLog[64];
      sprintf(szLog, "too much gap (overflow) (%d blocks) in received blocks", (int)uDiffBlocks);
      _empty_buffers(szLog, pPH, pPHVS);

      // Wait for start of a recoverable block
      if ( (pPHVS->uCurrentBlockECPackets != 0) && (pPHVS->uCurrentBlockPacketIndex > (pPHVS->uCurrentBlockECPackets*2)/3) )
         return false;
      return _add_video_packet_to_buffer(m_iTopBufferIndex, pPacket, iPacketLength);
   }

   int iNextTop = m_iTopBufferIndex;
   for(u32 uBlk=m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex+1; uBlk<=pPHVS->uCurrentBlockIndex; uBlk++)
   {
      iNextTop++;
      if ( iNextTop >= MAX_RXTX_BLOCKS_BUFFER )
         iNextTop = 0;

      if ( ! _check_allocate_video_block_in_buffer(iNextTop) )
         return false;

      _empty_block_buffer_index(iNextTop);
      m_VideoBlocks[iNextTop].uH264FrameIndex = 0;
      m_VideoBlocks[iNextTop].iTotalFramePackets = 0;
      m_VideoBlocks[iNextTop].iFramePacketStart = -1;
      m_VideoBlocks[iNextTop].iFramePacketEnd = -1;
      m_VideoBlocks[iNextTop].uVideoBlockIndex = uBlk;
      m_VideoBlocks[iNextTop].uReceivedTime = g_TimeNow;
   }

   m_iTopBufferIndex = iNextTop;
   return _add_video_packet_to_buffer(m_iTopBufferIndex, pPacket, iPacketLength);
}

int VideoRxPacketsBuffer::getBufferBottomIndex()
{
   return m_iBottomBufferIndex;
}

u32 VideoRxPacketsBuffer::getBufferBottomVideoBlockIndex()
{
   return m_VideoBlocks[m_iBottomBufferIndex].uVideoBlockIndex;
}

int VideoRxPacketsBuffer::getBufferTopIndex()
{
   return m_iTopBufferIndex;
}

u32 VideoRxPacketsBuffer::getBufferTopVideoBlockIndex()
{
   return m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex;
}


int VideoRxPacketsBuffer::getTopBufferMaxReceivedVideoBlockPacketIndex()
{
   return m_VideoBlocks[m_iTopBufferIndex].iMaxReceivedDataOrECPacketIndex;
}

int VideoRxPacketsBuffer::getCountBlocksInBuffer()
{
   if ( m_bBuffersEmpty )
      return 0;
   if ( m_VideoBlocks[m_iBottomBufferIndex].uVideoBlockIndex > m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex )
      return 0;
   return (m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex - m_VideoBlocks[m_iBottomBufferIndex].uVideoBlockIndex + 1);
}

type_rx_video_block_info* VideoRxPacketsBuffer::getTopBlockInBuffer()
{
   return &(m_VideoBlocks[m_iTopBufferIndex]);
}

type_rx_video_block_info* VideoRxPacketsBuffer::getBlockInBufferFromBottom(int iDeltaPosition)
{
   int iIndex = m_iBottomBufferIndex + iDeltaPosition;
   if ( iIndex >= MAX_RXTX_BLOCKS_BUFFER )
      iIndex -= MAX_RXTX_BLOCKS_BUFFER;
   return &(m_VideoBlocks[iIndex]);
}

type_rx_video_packet_info* VideoRxPacketsBuffer::getBottomBlockAndPacketInBuffer(type_rx_video_block_info** ppOutputBlock)
{
   if ( NULL != ppOutputBlock )
      *ppOutputBlock = &(m_VideoBlocks[m_iBottomBufferIndex]);

   return &(m_VideoBlocks[m_iBottomBufferIndex].packets[m_iBottomBufferPacketIndex]);
}


int VideoRxPacketsBuffer::discardOldBlocks(u32 uCutOffTime)
{
   if ( m_bBuffersEmpty )
      return 0;

   int iCountDiscarded = 0;
   
   while ( m_VideoBlocks[m_iBottomBufferIndex].uVideoBlockIndex <= m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex )
   {
      if ( (m_VideoBlocks[m_iBottomBufferIndex].uReceivedTime > uCutOffTime) ||
           (m_VideoBlocks[m_iBottomBufferIndex].uReceivedTime == 0) )
         break;
      
      if ( iCountDiscarded < 3 )
         log_line("[VideoRXBuffer] Discard bottom buffer index: %d, video block id [f%d blk %u], fr has %d packets, blk has fr pkt %d to %d (recv time: %u ms ago, recv data/ec pckts: %d,%d, scheme: %d/%d), top buffer index: %d, video block id %u, max recv pckt index: %d",
            m_iBottomBufferIndex, m_VideoBlocks[m_iBottomBufferIndex].uH264FrameIndex, m_VideoBlocks[m_iBottomBufferIndex].uVideoBlockIndex,
            m_VideoBlocks[m_iBottomBufferIndex].iTotalFramePackets, 
            m_VideoBlocks[m_iBottomBufferIndex].iFramePacketStart, m_VideoBlocks[m_iBottomBufferIndex].iFramePacketEnd, 
            g_TimeNow - m_VideoBlocks[m_iBottomBufferIndex].uReceivedTime,
            m_VideoBlocks[m_iBottomBufferIndex].iRecvDataPackets, m_VideoBlocks[m_iBottomBufferIndex].iRecvECPackets,
            m_VideoBlocks[m_iBottomBufferIndex].iBlockDataPackets, m_VideoBlocks[m_iBottomBufferIndex].iBlockECPackets,
            m_iTopBufferIndex, m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex, m_VideoBlocks[m_iTopBufferIndex].iMaxReceivedDataOrECPacketIndex);
      iCountDiscarded++;
      u32 uVideoBlockIndex = m_VideoBlocks[m_iBottomBufferIndex].uVideoBlockIndex;
      _empty_block_buffer_index(m_iBottomBufferIndex);
      if (m_iBottomBufferIndex == m_iTopBufferIndex )
         m_VideoBlocks[m_iBottomBufferIndex].uVideoBlockIndex = uVideoBlockIndex;
      m_iBottomBufferPacketIndex = 0;
      m_iBottomBufferIndex++;
      if ( m_iBottomBufferIndex >= MAX_RXTX_BLOCKS_BUFFER )
         m_iBottomBufferIndex = 0;
      m_VideoBlocks[m_iBottomBufferIndex].uVideoBlockIndex = uVideoBlockIndex+1;
   }

   return iCountDiscarded;
}

bool VideoRxPacketsBuffer::discardBottomBlockIfIncomplete()
{
   if ( m_bBuffersEmpty )
      return false;
   if ( m_VideoBlocks[m_iBottomBufferIndex].uVideoBlockIndex >= m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex )
      return false;

   if ( (m_VideoBlocks[m_iBottomBufferIndex].iBlockDataPackets == 0) ||
        (m_VideoBlocks[m_iBottomBufferIndex].iRecvDataPackets < m_VideoBlocks[m_iBottomBufferIndex].iBlockDataPackets) )
   {
      u32 uVideoBlockIndex = m_VideoBlocks[m_iBottomBufferIndex].uVideoBlockIndex;
      _empty_block_buffer_index(m_iBottomBufferIndex);
      if (m_iBottomBufferIndex == m_iTopBufferIndex )
         m_VideoBlocks[m_iBottomBufferIndex].uVideoBlockIndex = uVideoBlockIndex;
      m_iBottomBufferPacketIndex = 0;
      m_iBottomBufferIndex++;
      if ( m_iBottomBufferIndex >= MAX_RXTX_BLOCKS_BUFFER )
         m_iBottomBufferIndex = 0;
      m_VideoBlocks[m_iBottomBufferIndex].uVideoBlockIndex = uVideoBlockIndex+1;

      return true;
   }
   return false;
}


void VideoRxPacketsBuffer::advanceBottomPacketInBuffer()
{
   m_iBottomBufferPacketIndex++;

   if ( m_iBottomBufferPacketIndex < m_VideoBlocks[m_iBottomBufferIndex].iBlockDataPackets )
      return;
   
   u32 uVideoBlockIndex = m_VideoBlocks[m_iBottomBufferIndex].uVideoBlockIndex;
   _empty_block_buffer_index(m_iBottomBufferIndex);
   
   if (m_iBottomBufferIndex == m_iTopBufferIndex )
      m_VideoBlocks[m_iBottomBufferIndex].uVideoBlockIndex = uVideoBlockIndex;
   m_iBottomBufferPacketIndex = 0;
   m_iBottomBufferIndex++;
   if ( m_iBottomBufferIndex >= MAX_RXTX_BLOCKS_BUFFER )
      m_iBottomBufferIndex = 0;
   m_VideoBlocks[m_iBottomBufferIndex].uVideoBlockIndex = uVideoBlockIndex+1;

}

