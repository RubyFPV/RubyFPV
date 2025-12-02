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
#include "../common/string_utils.h"
#include "radiopacketsqueue.h"
#include "radiopackets2.h"
#include "radiolink.h"


void packets_queue_init(t_packet_queue* pQueue)
{
   if ( NULL == pQueue )
      return;
   pQueue->queue_start_pos = -1;
   pQueue->queue_end_pos = -1;   
}

int packets_queue_is_empty(t_packet_queue* pQueue)
{
   if ( NULL == pQueue )
      return 1;
   return (pQueue->queue_start_pos == pQueue->queue_end_pos);
}

int packets_queue_has_packets(t_packet_queue* pQueue)
{
   if ( NULL == pQueue )
      return 0;
   if ( pQueue->queue_start_pos == pQueue->queue_end_pos )
      return 0;

   if ( pQueue->queue_end_pos > pQueue->queue_start_pos )
      return pQueue->queue_end_pos - pQueue->queue_start_pos;

   return pQueue->queue_end_pos + (MAX_PACKETS_IN_QUEUE - pQueue->queue_start_pos);
}

int packets_queue_inject_packet_first(t_packet_queue* pQueue, u8* pBuffer)
{
   if ( NULL == pQueue )
      return 0;

   t_packet_header* pPH = (t_packet_header*)pBuffer;
   
   if ( (-1 == pQueue->queue_start_pos) || (pQueue->queue_start_pos == pQueue->queue_end_pos) )
   {
      pQueue->queue_start_pos = 0;
      pQueue->queue_end_pos = 1;
      pQueue->timeFirstPacket = get_current_timestamp_ms();
      pQueue->packets_queue[0].uTimeAdded = 0;
      pQueue->packets_queue[0].packet_length = pPH->total_length;
      memcpy( &(pQueue->packets_queue[0].packet_buffer[0]), pBuffer, pQueue->packets_queue[0].packet_length);
      return 1;
   }

   pQueue->queue_start_pos--;
   if ( pQueue->queue_start_pos < 0 )
      pQueue->queue_start_pos = MAX_PACKETS_IN_QUEUE-1;

   pQueue->packets_queue[pQueue->queue_start_pos].uTimeAdded = 0;
   pQueue->packets_queue[pQueue->queue_start_pos].packet_length = pPH->total_length;
   memcpy( &(pQueue->packets_queue[pQueue->queue_start_pos].packet_buffer[0]), pBuffer, pQueue->packets_queue[pQueue->queue_start_pos].packet_length);
   return 1;
}

int packets_queue_inject_packet_first_mark_time(t_packet_queue* pQueue, u8* pBuffer)
{
   if ( NULL == pQueue )
      return 0;

   t_packet_header* pPH = (t_packet_header*)pBuffer;
   
   if ( (-1 == pQueue->queue_start_pos) || (pQueue->queue_start_pos == pQueue->queue_end_pos) )
   {
      pQueue->queue_start_pos = 0;
      pQueue->queue_end_pos = 1;
      pQueue->timeFirstPacket = get_current_timestamp_ms();
      pQueue->packets_queue[0].uTimeAdded = get_current_timestamp_micros();
      pQueue->packets_queue[0].packet_length = pPH->total_length;
      memcpy( &(pQueue->packets_queue[0].packet_buffer[0]), pBuffer, pQueue->packets_queue[0].packet_length);
      return 1;
   }

   pQueue->queue_start_pos--;
   if ( pQueue->queue_start_pos < 0 )
      pQueue->queue_start_pos = MAX_PACKETS_IN_QUEUE-1;

   pQueue->packets_queue[pQueue->queue_start_pos].uTimeAdded = get_current_timestamp_micros();
   pQueue->packets_queue[pQueue->queue_start_pos].packet_length = pPH->total_length;
   memcpy( &(pQueue->packets_queue[pQueue->queue_start_pos].packet_buffer[0]), pBuffer, pQueue->packets_queue[pQueue->queue_start_pos].packet_length);
   return 1;
}

int packets_queue_add_packet(t_packet_queue* pQueue, u8* pBuffer)
{
   return packets_queue_add_packet2(pQueue, pBuffer, -1, 0, 0);
}

int packets_queue_add_packet_mark_time(t_packet_queue* pQueue, u8* pBuffer)
{
   return packets_queue_add_packet2(pQueue, pBuffer, -1, 0, 1);
}

int packets_queue_add_packet2(t_packet_queue* pQueue, u8* pBuffer, int length, int has_radio_header, int iMarkTime)
{
   if ( NULL == pQueue )
      return 0;
   if ( -1 == pQueue->queue_start_pos )
   {
      pQueue->queue_start_pos = 0;
      pQueue->queue_end_pos = 0;
   }

   // Full queue ?
   if ( ((pQueue->queue_end_pos+1) % MAX_PACKETS_IN_QUEUE) == pQueue->queue_start_pos )
      return 0;

   // Empty queue ?
   if ( pQueue->queue_start_pos == pQueue->queue_end_pos )
      pQueue->timeFirstPacket = get_current_timestamp_ms();

   if ( iMarkTime )
      pQueue->packets_queue[pQueue->queue_end_pos].uTimeAdded = get_current_timestamp_micros();
   else
      pQueue->packets_queue[pQueue->queue_end_pos].uTimeAdded = 0;
   pQueue->packets_queue[pQueue->queue_end_pos].has_radio_header = (u8)has_radio_header;
   pQueue->packets_queue[pQueue->queue_end_pos].packet_length = (u16)length;
   if ( -1 == length )
   {
      t_packet_header* pPH = (t_packet_header*)pBuffer;
      pQueue->packets_queue[pQueue->queue_end_pos].packet_length = pPH->total_length;
   }

   memcpy( &(pQueue->packets_queue[pQueue->queue_end_pos].packet_buffer[0]), pBuffer, pQueue->packets_queue[pQueue->queue_end_pos].packet_length);

   pQueue->queue_end_pos++;
   if ( pQueue->queue_end_pos >= MAX_PACKETS_IN_QUEUE )
      pQueue->queue_end_pos = 0;

   return 1;
}

u8* packets_queue_pop_packet(t_packet_queue* pQueue, int* pLength)
{
   if ( NULL == pQueue )
      return NULL;
   if ( NULL != pLength )
      *pLength = 0;
   if ( pQueue->queue_start_pos == pQueue->queue_end_pos )
      return NULL;

   if ( NULL != pLength )
      *pLength = pQueue->packets_queue[pQueue->queue_start_pos].packet_length;
   u8* pRet = &(pQueue->packets_queue[pQueue->queue_start_pos].packet_buffer[0]);

   pQueue->queue_start_pos++;
   if ( pQueue->queue_start_pos >= MAX_PACKETS_IN_QUEUE )
      pQueue->queue_start_pos = 0;

   if ( pQueue->queue_start_pos == pQueue->queue_end_pos )
      pQueue->timeFirstPacket = MAX_U32;

   return pRet;
}

u8* packets_queue_pop_packet_and_time(t_packet_queue* pQueue, int* pLength, u32* puTimeAdded)
{
   if ( NULL == pQueue )
      return NULL;
   if ( NULL != pLength )
      *pLength = 0;
   if ( pQueue->queue_start_pos == pQueue->queue_end_pos )
      return NULL;

   if ( NULL != pLength )
      *pLength = pQueue->packets_queue[pQueue->queue_start_pos].packet_length;
   if ( NULL != puTimeAdded )
      *puTimeAdded = pQueue->packets_queue[pQueue->queue_start_pos].uTimeAdded;
   u8* pRet = &(pQueue->packets_queue[pQueue->queue_start_pos].packet_buffer[0]);

   pQueue->queue_start_pos++;
   if ( pQueue->queue_start_pos >= MAX_PACKETS_IN_QUEUE )
      pQueue->queue_start_pos = 0;

   if ( pQueue->queue_start_pos == pQueue->queue_end_pos )
      pQueue->timeFirstPacket = MAX_U32;

   return pRet; 
}

u8* packets_queue_peek_packet(t_packet_queue* pQueue, int index, int* pLength)
{
   if ( NULL == pQueue )
      return NULL;
   if ( NULL != pLength )
      *pLength = 0;
   if ( pQueue->queue_start_pos == pQueue->queue_end_pos )
      return NULL;

   int iPos = pQueue->queue_start_pos + index;
   if ( iPos >= MAX_PACKETS_IN_QUEUE )
      iPos -= MAX_PACKETS_IN_QUEUE;
   if ( NULL != pLength )
      *pLength = pQueue->packets_queue[iPos].packet_length;
   u8* pRet = &(pQueue->packets_queue[iPos].packet_buffer[0]);

   return pRet;
}
