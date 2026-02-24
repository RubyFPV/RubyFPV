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
#include "../base/models.h"
#include "../base/utils.h"
#include "../radio/fec.h"
#include "shared_vars.h"
#include "generic_rx_ecbuffers.h"
#include "timers.h"


GenericRxECBuffers::GenericRxECBuffers()
{
   m_bEnableCRC = false;
   m_iMaxBlocks = 0;
   m_pBlocks = NULL;
   m_uBlockDataPackets = 0;
   m_uBlockECPackets = 0;
   m_iBlockPacketLength = 0;

   m_iTopBufferIndex = 0;
   m_iBottomBufferIndexToOutput = 0;
   m_iBottomPacketIndexToOutput = 0;
}

GenericRxECBuffers::~GenericRxECBuffers()
{
   _deleteBuffers();
}

void GenericRxECBuffers::init(int iMaxBlocks, bool bEnableCRC, u32 uDataPackets, u32 uECPackets, int iPacketLength)
{
   m_bEnableCRC = bEnableCRC;
   m_iMaxBlocks = iMaxBlocks;
   m_uBlockDataPackets = uDataPackets;
   m_uBlockECPackets = uECPackets;
   m_iBlockPacketLength = iPacketLength;
   _reinitBuffers();
}


void GenericRxECBuffers::_deleteBuffers()
{
   if ( NULL == m_pBlocks )
      return;
   for( int i=0; i<m_iMaxBlocks; i++ )
   {
      for( u32 k=0; k<m_uBlockDataPackets+m_uBlockECPackets; k++)
      {
         if ( NULL != m_pBlocks[i].pPackets[k] )
            free(m_pBlocks[i].pPackets[k]);
         m_pBlocks[i].pPackets[k] = NULL;
      }
   }
   free(m_pBlocks);
   m_pBlocks = NULL;
}

void GenericRxECBuffers::_reinitBuffers()
{
   _deleteBuffers();

   if ( m_iMaxBlocks < 1 )
      m_iMaxBlocks = 1;
   if ( m_iMaxBlocks > 1000 )
      m_iMaxBlocks = 1000;
   if ( m_uBlockDataPackets == 0 )
      m_uBlockDataPackets = 1;
   if ( m_uBlockDataPackets > MAX_DATA_PACKETS_IN_BLOCK )
      m_uBlockDataPackets = MAX_DATA_PACKETS_IN_BLOCK;

   if ( m_uBlockECPackets == 0 )
      m_uBlockECPackets = 1;
   if ( m_uBlockECPackets > MAX_FECS_PACKETS_IN_BLOCK )
      m_uBlockECPackets = MAX_FECS_PACKETS_IN_BLOCK;

   m_pBlocks = (type_generic_rx_ec_block*)malloc(m_iMaxBlocks*sizeof(type_generic_rx_ec_block));
   if ( NULL == m_pBlocks )
      return;

   for( int i=0; i<m_iMaxBlocks; i++ )
   {
      for( u32 k=0; k<m_uBlockDataPackets+m_uBlockECPackets; k++ )
      {
         m_pBlocks[i].pPackets[k] = (type_generic_rx_ec_packet*) malloc(sizeof(type_generic_rx_ec_packet));
      }
   }

   _clearBuffers("reinit buffers");
}

void GenericRxECBuffers::_clearBufferBlock(int iBlockIndex)
{
   m_pBlocks[iBlockIndex].uBlockIndex = 0;
   m_pBlocks[iBlockIndex].bEmpty = true;
   m_pBlocks[iBlockIndex].iMaxReceivedDataPacketIndex = -1;
   m_pBlocks[iBlockIndex].iReceivedDataPackets = 0;
   m_pBlocks[iBlockIndex].iReceivedECPackets = 0;
   for( u32 k=0; k<m_uBlockDataPackets+m_uBlockECPackets; k++ )
   {
      if ( NULL != m_pBlocks[iBlockIndex].pPackets[k] )
      {
         m_pBlocks[iBlockIndex].pPackets[k]->uReceivedTime = 0;
         m_pBlocks[iBlockIndex].pPackets[k]->iFilledBytes = 0;
         m_pBlocks[iBlockIndex].pPackets[k]->bEmpty = true;
         m_pBlocks[iBlockIndex].pPackets[k]->bReconstructed = false;
         m_pBlocks[iBlockIndex].pPackets[k]->bOutputed = false;
      }
   }
}

void GenericRxECBuffers::_clearBuffers(const char* szReason)
{
   log_line("[GenericRxECBuffers] Clear buffers (reason: %s), bottom block: %u, top index: %d, block: %u",
      (NULL != szReason)?szReason:"N/A", m_pBlocks[0].uBlockIndex, m_iTopBufferIndex, m_pBlocks[m_iTopBufferIndex].uBlockIndex);
   m_iTopBufferIndex = 0;
   m_iBottomBufferIndexToOutput = 0;
   m_iBottomPacketIndexToOutput = 0;

   if ( NULL == m_pBlocks )
      return;
   for( int i=0; i<m_iMaxBlocks; i++ )
   {
      if ( ! m_pBlocks[i].bEmpty )
         g_SMControllerRTInfo.uOutputedAudioPacketsSkipped[g_SMControllerRTInfo.iCurrentIndex]++;
      _clearBufferBlock(i);
   }
}

void GenericRxECBuffers::_addPacketToBuffer(u32 uBlockIndex, u32 uPacketIndex, int iBufferIndex, u8* pData, int iDataLength, u32 uTimeNow)
{
   if ( (NULL == m_pBlocks) || (NULL == pData) || (iDataLength < (int)sizeof(u32)) || (m_iMaxBlocks < 1) )
      return;

   if ( ! m_pBlocks[iBufferIndex].pPackets[uPacketIndex]->bEmpty )
      return;

   m_pBlocks[iBufferIndex].uBlockIndex = uBlockIndex;
   m_pBlocks[iBufferIndex].bEmpty = false;

   if ( uPacketIndex < m_uBlockDataPackets )
   {
      m_pBlocks[iBufferIndex].iReceivedDataPackets++;
      if ( (int)uPacketIndex > m_pBlocks[iBufferIndex].iMaxReceivedDataPacketIndex )
            m_pBlocks[iBufferIndex].iMaxReceivedDataPacketIndex = (int)uPacketIndex;
   }
   else
      m_pBlocks[iBufferIndex].iReceivedECPackets++;

   m_pBlocks[iBufferIndex].pPackets[uPacketIndex]->bEmpty = false;
   m_pBlocks[iBufferIndex].pPackets[uPacketIndex]->bReconstructed = false;
   m_pBlocks[iBufferIndex].pPackets[uPacketIndex]->bOutputed = false;
   m_pBlocks[iBufferIndex].pPackets[uPacketIndex]->uReceivedTime = uTimeNow;
   m_pBlocks[iBufferIndex].pPackets[uPacketIndex]->iFilledBytes = iDataLength;
   memcpy(m_pBlocks[iBufferIndex].pPackets[uPacketIndex]->uPacketData, pData, iDataLength);

   _computeECDataOnBlock(iBufferIndex);
}


void GenericRxECBuffers::_computeECDataOnBlock(int iBufferIndex)
{
   if ( (NULL == m_pBlocks) || (iBufferIndex < 0) || (iBufferIndex >= m_iMaxBlocks) || (0 == m_uBlockECPackets) )
      return;
   if ( m_pBlocks[iBufferIndex].iReceivedDataPackets >= (int)m_uBlockDataPackets )
      return;
   if ( m_pBlocks[iBufferIndex].iReceivedDataPackets + m_pBlocks[iBufferIndex].iReceivedECPackets < (int)m_uBlockDataPackets )
      return;

   m_missing_packets_count_for_ec = 0;
   for( u32 u=0; u<m_uBlockDataPackets; u++ )
   {
      m_p_ec_decode_data_packets[u] = &(m_pBlocks[iBufferIndex].pPackets[u]->uPacketData[0]);
      if ( m_pBlocks[iBufferIndex].pPackets[u]->bEmpty )
      {
         m_ec_decode_missing_packets_indexes[m_missing_packets_count_for_ec] = u;
         m_missing_packets_count_for_ec++;
      }
   }

   // Add the needed FEC packets to the list
   unsigned int pos = 0;
   for( u32 u=0; u<m_uBlockECPackets; u++ )
   {
      if ( ! m_pBlocks[iBufferIndex].pPackets[u+m_uBlockDataPackets]->bEmpty )
      {
         m_p_ec_decode_ec_packets[pos] =  &(m_pBlocks[iBufferIndex].pPackets[u+m_uBlockDataPackets]->uPacketData[0]);
         m_ec_decode_ec_indexes[pos] = u;
         pos++;
         if ( pos == m_missing_packets_count_for_ec )
            break;
      }
   }

   int iRes = fec_decode(m_iBlockPacketLength, m_p_ec_decode_data_packets, (unsigned int)m_uBlockDataPackets, m_p_ec_decode_ec_packets, m_ec_decode_ec_indexes, m_ec_decode_missing_packets_indexes, m_missing_packets_count_for_ec );
   if ( iRes < 0 )
   {
      log_softerror_and_alarm("[GenericRxEcBuffer] Failed to decode block type %u/%u/%d bytes; recv: %d/%d packets, missing count: %d",
        m_uBlockDataPackets, m_uBlockECPackets, m_iBlockPacketLength,
        m_pBlocks[iBufferIndex].iReceivedDataPackets, m_pBlocks[iBufferIndex].iReceivedECPackets,
        m_missing_packets_count_for_ec);
   }
   // Mark all data packets reconstructed as received, set the right info in them
   for( int i=0; i<(int)m_missing_packets_count_for_ec; i++ )
   {
      int iPacketIndexToFix = m_ec_decode_missing_packets_indexes[i];

      m_pBlocks[iBufferIndex].pPackets[iPacketIndexToFix]->bEmpty = false;
      m_pBlocks[iBufferIndex].pPackets[iPacketIndexToFix]->bOutputed = false;
      m_pBlocks[iBufferIndex].pPackets[iPacketIndexToFix]->bReconstructed = true;
      m_pBlocks[iBufferIndex].pPackets[iPacketIndexToFix]->iFilledBytes = m_iBlockPacketLength;
      m_pBlocks[iBufferIndex].iReceivedDataPackets++;
      if ( iPacketIndexToFix > m_pBlocks[iBufferIndex].iMaxReceivedDataPacketIndex )
         m_pBlocks[iBufferIndex].iMaxReceivedDataPacketIndex = iPacketIndexToFix;
   }
}

void GenericRxECBuffers::checkAddPacket(u32 uBlockIndex, u32 uPacketIndex, u8* pData, int iDataLength, u32 uTimeNow)
{
   if ( (NULL == m_pBlocks) || (NULL == pData) || (iDataLength < (int)sizeof(u32)) || (m_iMaxBlocks < 1) )
      return;

   if ( uPacketIndex >= (m_uBlockDataPackets + m_uBlockECPackets) )
      return;

   // Empty buffers ?
   if ( m_pBlocks[m_iTopBufferIndex].bEmpty )
   {
      // Wait for start of a block
      if ( uPacketIndex != 0 )
         return;
      _addPacketToBuffer(uBlockIndex, uPacketIndex, m_iTopBufferIndex, pData, iDataLength, uTimeNow);
      return;
   }

   // Top block is not empty
   // Non-empty buffers from now on

   // On the current top block?
   if ( m_pBlocks[m_iTopBufferIndex].uBlockIndex == uBlockIndex )
   {
      _addPacketToBuffer(uBlockIndex, uPacketIndex, m_iTopBufferIndex, pData, iDataLength, uTimeNow);
      return;
   }

   // Stream restarted?
   if ( m_pBlocks[m_iTopBufferIndex].uBlockIndex > 50 )
   if ( uBlockIndex < m_pBlocks[m_iTopBufferIndex].uBlockIndex - 50 )
   {
      _clearBuffers("stream restarted");
      // Wait for start of a block
      if ( uPacketIndex != 0 )
         return;
      _addPacketToBuffer(uBlockIndex, uPacketIndex, m_iTopBufferIndex, pData, iDataLength, uTimeNow);
      return;
   }

   // Compute difference from top

   // Old block packet (older than top block)?

   if ( uBlockIndex < m_pBlocks[m_iTopBufferIndex].uBlockIndex )
   {
      if ( uBlockIndex < m_pBlocks[m_iBottomBufferIndexToOutput].uBlockIndex )
         return;

      u32 uDiffBlocks = m_pBlocks[m_iTopBufferIndex].uBlockIndex - uBlockIndex;
      if ( uDiffBlocks >= (u32)m_iMaxBlocks - 1 )
         return;

      int iBufferIndex = m_iTopBufferIndex - (int) uDiffBlocks;
      if ( iBufferIndex < 0 )
         iBufferIndex += m_iMaxBlocks;
      _addPacketToBuffer(uBlockIndex, uPacketIndex, iBufferIndex, pData, iDataLength, uTimeNow);
      return;
   }

   // Future block packet

   u32 uDiffBlocks = uBlockIndex - m_pBlocks[m_iTopBufferIndex].uBlockIndex;
   if ( uDiffBlocks >= (u32)m_iMaxBlocks - 1 )
   {
      _clearBuffers("too much gap in received blocks");

      // Wait for start of a block
      if ( uPacketIndex != 0 )
         return;
      _addPacketToBuffer(uBlockIndex, uPacketIndex, m_iTopBufferIndex, pData, iDataLength, uTimeNow);
      return;
   }

   for(u32 uBlk=m_pBlocks[m_iTopBufferIndex].uBlockIndex+1; uBlk<=uBlockIndex; uBlk++)
   {
      m_iTopBufferIndex++;
      if ( m_iTopBufferIndex >= m_iMaxBlocks )
         m_iTopBufferIndex = 0;
      _clearBufferBlock(m_iTopBufferIndex);
      m_pBlocks[m_iTopBufferIndex].uBlockIndex = uBlk;
      m_pBlocks[m_iTopBufferIndex].bEmpty = false;
      if ( m_iTopBufferIndex == m_iBottomBufferIndexToOutput )
      {
         m_iBottomPacketIndexToOutput = 0;
         m_iBottomBufferIndexToOutput++;
         if ( m_iBottomBufferIndexToOutput >= m_iMaxBlocks )
            m_iBottomBufferIndexToOutput = 0;
      }
   }
   _addPacketToBuffer(uBlockIndex, uPacketIndex, m_iTopBufferIndex, pData, iDataLength, uTimeNow);
}

u8* GenericRxECBuffers::getMarkFirstPacketToOutput(int* piLength, u32* puBlockIndex, u32* puBlockPacketIndex, bool bPushIncompleteBlocks)
{
   if ( NULL != piLength )
      *piLength = 0;
   if ( NULL != puBlockIndex )
      *puBlockIndex = 0;
   if ( NULL != puBlockPacketIndex )
      *puBlockPacketIndex = 0;

   if ( (NULL == m_pBlocks) || m_pBlocks[m_iTopBufferIndex].bEmpty )
      return NULL;

   // Skip and advance up to the next available packet if any
   int iCount = m_iMaxBlocks;
   while ( iCount > 0 )
   {
      iCount--;

      // Break on packet ready to output
      if ( ! m_pBlocks[m_iBottomBufferIndexToOutput].bEmpty )
      if ( ! m_pBlocks[m_iBottomBufferIndexToOutput].pPackets[m_iBottomPacketIndexToOutput]->bEmpty )
      if ( ! m_pBlocks[m_iBottomBufferIndexToOutput].pPackets[m_iBottomPacketIndexToOutput]->bOutputed )
      if ( m_iBottomPacketIndexToOutput <= m_pBlocks[m_iBottomBufferIndexToOutput].iMaxReceivedDataPacketIndex )
         break;

      // Break on end (top) of buffer
      if ( m_iBottomBufferIndexToOutput == m_iTopBufferIndex )
      if ( m_iBottomPacketIndexToOutput >= m_pBlocks[m_iTopBufferIndex].iMaxReceivedDataPacketIndex )
         break;

      // Don't skip empty packets if not requested so
      if ( ! bPushIncompleteBlocks )
      if ( m_pBlocks[m_iBottomBufferIndexToOutput].bEmpty )
         break;

      if ( ! bPushIncompleteBlocks )
      if ( m_pBlocks[m_iBottomBufferIndexToOutput].pPackets[m_iBottomPacketIndexToOutput]->bEmpty )
         break;

      // Break on gaps on current top block (can be reconstructed later)
      if ( m_iBottomBufferIndexToOutput == m_iTopBufferIndex )
      if ( m_pBlocks[m_iBottomBufferIndexToOutput].pPackets[m_iBottomPacketIndexToOutput]->bEmpty )
         break;

      // Will skip this packet now
      if ( m_pBlocks[m_iBottomBufferIndexToOutput].pPackets[m_iBottomPacketIndexToOutput]->bEmpty ||
          (!m_pBlocks[m_iBottomBufferIndexToOutput].pPackets[m_iBottomPacketIndexToOutput]->bOutputed) )
      {
         g_SMControllerRTInfo.uOutputedAudioPacketsSkipped[g_SMControllerRTInfo.iCurrentIndex]++;
      }
      m_iBottomPacketIndexToOutput++;
      if ( m_iBottomPacketIndexToOutput >= (int)m_uBlockDataPackets )
      {
         m_iBottomPacketIndexToOutput = 0;
         m_iBottomBufferIndexToOutput++;
         if ( m_iBottomBufferIndexToOutput >= m_iMaxBlocks )
            m_iBottomBufferIndexToOutput = 0;
      }
   }
   if ( iCount == 0 )
      log_softerror_and_alarm("[GenericRxEcBuffer] Tried to iterate past the rx-ec-buffer size (%d blocks)", m_iMaxBlocks);


   if ( m_pBlocks[m_iBottomBufferIndexToOutput].bEmpty ||
        m_pBlocks[m_iBottomBufferIndexToOutput].pPackets[m_iBottomPacketIndexToOutput]->bEmpty ||
        m_pBlocks[m_iBottomBufferIndexToOutput].pPackets[m_iBottomPacketIndexToOutput]->bOutputed )
     return NULL;

   if ( NULL != piLength )
      *piLength = m_pBlocks[m_iBottomBufferIndexToOutput].pPackets[m_iBottomPacketIndexToOutput]->iFilledBytes;
   if ( NULL != puBlockIndex )
      *puBlockIndex = m_pBlocks[m_iBottomBufferIndexToOutput].uBlockIndex;
   if ( NULL != puBlockPacketIndex )
      *puBlockPacketIndex = (u32)m_iBottomPacketIndexToOutput;

   m_pBlocks[m_iBottomBufferIndexToOutput].pPackets[m_iBottomPacketIndexToOutput]->bOutputed = true;
   if ( m_pBlocks[m_iBottomBufferIndexToOutput].pPackets[m_iBottomPacketIndexToOutput]->bReconstructed )
      g_SMControllerRTInfo.uOutputedAudioPacketsCorrected[g_SMControllerRTInfo.iCurrentIndex]++;
   else
      g_SMControllerRTInfo.uOutputedAudioPackets[g_SMControllerRTInfo.iCurrentIndex]++;

   return m_pBlocks[m_iBottomBufferIndexToOutput].pPackets[m_iBottomPacketIndexToOutput]->uPacketData;
}
