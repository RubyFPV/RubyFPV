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
#include "generic_tx_ecbuffers.h"
#include "timers.h"


GenericTxECBuffers::GenericTxECBuffers()
{
   m_bEnableCRC = false;
   m_iMaxBlocks = 0;
   m_pBlocks = NULL;
   m_uBlockDataPackets = 0;
   m_uBlockECPackets = 0;
   m_iBlockPacketLength = 0;

   m_uCurrentBlockIndex = 0;
   m_uCurrentBlockPacketIndex = 0;
   m_iBottomBufferIndex = 0;
   m_iTopBufferIndex = 0;
   m_iBottomBufferIndexFirstUnsend = 0;
   m_iBottomPacketIndexFirstUnsend = 0;
}

GenericTxECBuffers::~GenericTxECBuffers()
{
   _deleteBuffers();
}


void GenericTxECBuffers::init(int iMaxBlocks, bool bEnableCRC, u32 uDataPackets, u32 uECPackets, int iPacketLength)
{
   m_bEnableCRC = bEnableCRC;
   m_iMaxBlocks = iMaxBlocks;
   m_uBlockDataPackets = uDataPackets;
   m_uBlockECPackets = uECPackets;
   m_iBlockPacketLength = iPacketLength;
   _reinitBuffers();
}

void GenericTxECBuffers::_deleteBuffers()
{
   if ( NULL == m_pBlocks )
      return;
   for( int i=0; i<m_iMaxBlocks; i++ )
   {
      for( int k=0; k<MAX_TOTAL_PACKETS_IN_BLOCK; k++)
      {
         if ( NULL != m_pBlocks[i].pPackets[k] )
            free(m_pBlocks[i].pPackets[k]);
         m_pBlocks[i].pPackets[k] = NULL;
      }
   }
   free(m_pBlocks);
   m_pBlocks = NULL;
}

void GenericTxECBuffers::_reinitBuffers()
{
   _deleteBuffers();

   m_uCurrentBlockIndex = 0;
   m_uCurrentBlockPacketIndex = 0;
   m_iBottomBufferIndex = 0;
   m_iTopBufferIndex = 0;
   m_iBottomBufferIndexFirstUnsend = 0;
   m_iBottomPacketIndexFirstUnsend = 0;

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

   m_pBlocks = (type_generic_tx_ec_block*)malloc(m_iMaxBlocks*sizeof(type_generic_tx_ec_block));
   if ( NULL == m_pBlocks )
      return;

   for( int i=0; i<m_iMaxBlocks; i++ )
   {
      m_pBlocks[i].uBlockIndex = MAX_U32;
      m_pBlocks[i].iFilledPackets = 0;
      for( u32 k=0; k<m_uBlockDataPackets+m_uBlockECPackets; k++ )
      {
         m_pBlocks[i].pPackets[k] = (type_generic_tx_ec_packet*) malloc(sizeof(type_generic_tx_ec_packet));
         if ( NULL != m_pBlocks[i].pPackets[k] )
         {
            m_pBlocks[i].pPackets[k]->iFilledBytes = 0;
            m_pBlocks[i].pPackets[k]->bSent = false;
         }
      }
   }
}

void GenericTxECBuffers::_computeECDataOnCurrentBlock()
{
   if ( 0 == m_uBlockECPackets )
      return;
   if ( (NULL == m_pBlocks) || (m_pBlocks[m_iTopBufferIndex].iFilledPackets < (int)m_uBlockDataPackets) )
      return;

   for(u32 u=0; u<m_uBlockDataPackets; u++ )
      m_p_ec_data_packets[u] = &(m_pBlocks[m_iTopBufferIndex].pPackets[u]->uPacketData[0]);

   for(u32 u=0; u<m_uBlockECPackets; u++ )
      m_p_ec_ec_packets[u] = &(m_pBlocks[m_iTopBufferIndex].pPackets[u + m_uBlockDataPackets]->uPacketData[0]);

   fec_encode(m_iBlockPacketLength, m_p_ec_data_packets, (unsigned int)m_uBlockDataPackets, m_p_ec_ec_packets, (unsigned int)m_uBlockECPackets);
   
   for(u32 u=0; u<m_uBlockECPackets; u++ )
   {
      m_pBlocks[m_iTopBufferIndex].pPackets[m_uBlockDataPackets+u]->bSent = false;
      m_pBlocks[m_iTopBufferIndex].pPackets[m_uBlockDataPackets+u]->iFilledBytes = m_iBlockPacketLength;
      m_pBlocks[m_iTopBufferIndex].iFilledPackets++;
      m_uCurrentBlockPacketIndex++;
   }
}

// Returns number of fully filled packets
int GenericTxECBuffers::addData(u8* pData, int iDataLength)
{
   if ( (NULL == pData) || (iDataLength <= 0) || (NULL == m_pBlocks) )
      return 0;

   int iCountFilledPackets = 0;

   while ( iDataLength > 0 )
   {
      int iFilledBytes = m_pBlocks[m_iTopBufferIndex].pPackets[m_uCurrentBlockPacketIndex]->iFilledBytes;
      if ( 0 == iFilledBytes )
      if ( m_bEnableCRC )
         iFilledBytes += sizeof(u32);
      int iAvailableBytes = m_iBlockPacketLength - iFilledBytes;

      int iToFill = iDataLength;
      if ( iToFill > iAvailableBytes )
         iToFill = iAvailableBytes;

      m_pBlocks[m_iTopBufferIndex].uBlockIndex = m_uCurrentBlockIndex;
      memcpy(&(m_pBlocks[m_iTopBufferIndex].pPackets[m_uCurrentBlockPacketIndex]->uPacketData[iFilledBytes]), pData, iToFill);
      m_pBlocks[m_iTopBufferIndex].pPackets[m_uCurrentBlockPacketIndex]->iFilledBytes += iToFill;
      pData += iToFill;
      iDataLength -= iToFill;
      iAvailableBytes -= iToFill;

      if ( iAvailableBytes == 0 )
      {
         iCountFilledPackets++;
         if ( m_bEnableCRC )
         {
            u32 uCRC = base_compute_crc32(&(m_pBlocks[m_iTopBufferIndex].pPackets[m_uCurrentBlockPacketIndex]->uPacketData[sizeof(u32)]), m_iBlockPacketLength - sizeof(u32));
            memcpy(&(m_pBlocks[m_iTopBufferIndex].pPackets[m_uCurrentBlockPacketIndex]->uPacketData[0]), (u8*)&uCRC, sizeof(u32));
         }
         m_pBlocks[m_iTopBufferIndex].iFilledPackets++;
         m_uCurrentBlockPacketIndex++;
         if ( m_uCurrentBlockPacketIndex >= m_uBlockDataPackets )
         {
            _computeECDataOnCurrentBlock();
            m_uCurrentBlockPacketIndex = 0;
            m_uCurrentBlockIndex++;
            m_iTopBufferIndex++;
            if ( m_iTopBufferIndex >= m_iMaxBlocks )
               m_iTopBufferIndex = 0;
            m_pBlocks[m_iTopBufferIndex].uBlockIndex = m_uCurrentBlockIndex;
            m_pBlocks[m_iTopBufferIndex].iFilledPackets = 0;
            if ( m_iTopBufferIndex == m_iBottomBufferIndex )
            {
               m_iBottomBufferIndex++;
               if ( m_iBottomBufferIndex >= m_iMaxBlocks )
                  m_iBottomBufferIndex = 0;
            }
            if ( m_iTopBufferIndex == m_iBottomBufferIndexFirstUnsend )
            {
               m_iBottomPacketIndexFirstUnsend = 0;
               m_iBottomBufferIndexFirstUnsend++;
               if ( m_iBottomBufferIndexFirstUnsend >= m_iMaxBlocks )
                  m_iBottomBufferIndexFirstUnsend = 0;
            }
         }
         m_pBlocks[m_iTopBufferIndex].pPackets[m_uCurrentBlockPacketIndex]->bSent = false;
         m_pBlocks[m_iTopBufferIndex].pPackets[m_uCurrentBlockPacketIndex]->iFilledBytes = 0;
      }
   }
   return iCountFilledPackets;
}

int GenericTxECBuffers::getUnsendPacketsCount()
{
   if ( (NULL == m_pBlocks) || ((m_iTopBufferIndex == m_iBottomBufferIndexFirstUnsend) && ((u32)m_iBottomPacketIndexFirstUnsend == m_uCurrentBlockPacketIndex)) )
      return 0;

   int iCount = m_pBlocks[m_iBottomBufferIndexFirstUnsend].iFilledPackets - m_iBottomPacketIndexFirstUnsend;
   if ( iCount < 0 )
      iCount = 0;

   if ( m_iBottomBufferIndexFirstUnsend == m_iTopBufferIndex )
      return iCount;
   int iBlock = (m_iBottomBufferIndexFirstUnsend + 1) % m_iMaxBlocks;
   do
   {
      iCount += m_pBlocks[iBlock].iFilledPackets;
      iBlock = (iBlock + 1) % m_iMaxBlocks;
   } while ( iBlock != m_iTopBufferIndex );
   
   return iCount;
}

type_generic_tx_ec_packet* GenericTxECBuffers::getMarkFirstUnsendPacket(u32* puBlockIndex, int* piBufferIndex, int* piPacketIndex)
{
   if ( (NULL == m_pBlocks) || ((m_iTopBufferIndex == m_iBottomBufferIndexFirstUnsend) && ((u32)m_iBottomPacketIndexFirstUnsend == m_uCurrentBlockPacketIndex)) )
      return NULL;

   if ( m_pBlocks[m_iBottomBufferIndexFirstUnsend].pPackets[m_iBottomPacketIndexFirstUnsend]->bSent )
      return NULL;
   if ( m_pBlocks[m_iBottomBufferIndexFirstUnsend].pPackets[m_iBottomPacketIndexFirstUnsend]->iFilledBytes < (int) (m_uBlockDataPackets + m_uBlockECPackets) )
      return NULL;

   if ( NULL != puBlockIndex )
      *puBlockIndex = m_pBlocks[m_iBottomBufferIndexFirstUnsend].uBlockIndex;
   if ( NULL != piBufferIndex )
      *piBufferIndex = m_iBottomBufferIndexFirstUnsend;
   if ( NULL != piPacketIndex )
      *piPacketIndex = m_iBottomPacketIndexFirstUnsend;

   type_generic_tx_ec_packet* pRet = m_pBlocks[m_iBottomBufferIndexFirstUnsend].pPackets[m_iBottomPacketIndexFirstUnsend];

   m_pBlocks[m_iBottomBufferIndexFirstUnsend].pPackets[m_iBottomPacketIndexFirstUnsend]->bSent = true;
   m_iBottomPacketIndexFirstUnsend++;
   if ( m_iBottomPacketIndexFirstUnsend >= (int)(m_uBlockDataPackets + m_uBlockECPackets) )
   {
      m_iBottomPacketIndexFirstUnsend = 0;
      if ( m_iBottomBufferIndexFirstUnsend != m_iTopBufferIndex )
         m_iBottomBufferIndexFirstUnsend++;
      if ( m_iBottomBufferIndexFirstUnsend >= m_iMaxBlocks )
         m_iBottomBufferIndexFirstUnsend = 0;
   }
   return pRet;
}

u8* GenericTxECBuffers::getPacket(u32 uBlockIndex, u32 uPacketIndex, int* piOutputSize)
{
   if ( NULL != piOutputSize )
      *piOutputSize = 0;
   if ( (uBlockIndex > m_uCurrentBlockIndex) || (NULL == m_pBlocks) )
      return NULL;
   if ( (uBlockIndex > m_pBlocks[m_iTopBufferIndex].uBlockIndex) ||
        (uBlockIndex < m_pBlocks[m_iBottomBufferIndex].uBlockIndex) )
      return NULL;

   int iBlocksDelta = uBlockIndex - m_pBlocks[m_iBottomBufferIndex].uBlockIndex;
   if ( iBlocksDelta >= m_iMaxBlocks )
      return NULL;

   int iBlockIndex = m_iBottomBufferIndex + iBlocksDelta;
   if ( iBlockIndex >= m_iMaxBlocks )
      iBlockIndex -= m_iMaxBlocks;

   if ( m_pBlocks[iBlockIndex].uBlockIndex != uBlockIndex )
      return NULL;

   if ( (int)uPacketIndex >= m_pBlocks[iBlockIndex].iFilledPackets )
      return NULL;

   *piOutputSize = m_pBlocks[iBlockIndex].pPackets[uPacketIndex]->iFilledBytes;
   return m_pBlocks[iBlockIndex].pPackets[uPacketIndex]->uPacketData;
}


void GenericTxECBuffers::markPacketAsSent(int iBufferIndex, u32 uPacketIndex)
{
   if ( (NULL == m_pBlocks) || (iBufferIndex < 0) || (iBufferIndex >= m_iMaxBlocks) || (uPacketIndex >= (m_uBlockDataPackets+m_uBlockECPackets)) )
      return;

   if ( NULL != m_pBlocks[iBufferIndex].pPackets[uPacketIndex] )
      m_pBlocks[iBufferIndex].pPackets[uPacketIndex]->bSent = true;
}
