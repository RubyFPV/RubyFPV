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
#include <pthread.h>
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "radio_duplicate_det.h"
#include "radiolink.h"


#define PACKETS_INDEX_HASH_SIZE 512
#define PACKETS_INDEX_HASH_MASK 0x01FF

typedef struct
{
   u32 uMaxReceivedPacketIndex;
   u32 uLastReceivedPacketIndex;
   u32 uLastTimeReceivedPacket;
   u32 packetsHashIndexes[PACKETS_INDEX_HASH_SIZE];
} ALIGN_STRUCT_SPEC_INFO t_stream_history_packets_indexes;

typedef struct
{
   u32 uVehicleId;
   t_stream_history_packets_indexes streamsPacketsHistory[MAX_RADIO_STREAMS];
   int iRestartDetected;
} ALIGN_STRUCT_SPEC_INFO t_vehicle_history_packets_indexes;

t_vehicle_history_packets_indexes s_ListHistoryRxPacketsVehicles[MAX_CONCURENT_VEHICLES];

extern u32 s_uRadioRxTimeNow;

shared_mem_radio_stats* s_pSMRadioStatsDuplicateDetection = NULL;


void _radio_dd_reset_duplication_stats_for_vehicle(int iVehicleIndex, int iReason)
{
   if ( (iVehicleIndex < 0) || (iVehicleIndex >= MAX_CONCURENT_VEHICLES) )
      return;

   log_line("[RadioDuplicateDetection] Reset duplicate detection info for VID %u, buff index %d, reason: %d", s_ListHistoryRxPacketsVehicles[iVehicleIndex].uVehicleId, iVehicleIndex, iReason);

   s_ListHistoryRxPacketsVehicles[iVehicleIndex].uVehicleId = 0;
   s_ListHistoryRxPacketsVehicles[iVehicleIndex].iRestartDetected = 0;
   for( int k=0; k<MAX_RADIO_STREAMS; k++ )
   {
      s_ListHistoryRxPacketsVehicles[iVehicleIndex].streamsPacketsHistory[k].uMaxReceivedPacketIndex = 0;
      s_ListHistoryRxPacketsVehicles[iVehicleIndex].streamsPacketsHistory[k].uLastReceivedPacketIndex = MAX_U32;
      s_ListHistoryRxPacketsVehicles[iVehicleIndex].streamsPacketsHistory[k].uLastTimeReceivedPacket = 0;
      memset((u8*)s_ListHistoryRxPacketsVehicles[iVehicleIndex].streamsPacketsHistory[k].packetsHashIndexes, 0xFF, PACKETS_INDEX_HASH_SIZE * sizeof(u32));
   }
}


void radio_duplicate_detection_init()
{
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      _radio_dd_reset_duplication_stats_for_vehicle(i, 0);
}

void radio_duplicate_detection_log_info()
{
   char szBuff[256];
   u32 uTime = get_current_timestamp_ms();
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( s_ListHistoryRxPacketsVehicles[i].uVehicleId == 0 )
         continue;
      for( int k=0; k<MAX_RADIO_STREAMS; k++ )
      {
         if ( s_ListHistoryRxPacketsVehicles[i].streamsPacketsHistory[k].uLastReceivedPacketIndex == MAX_U32 )
            continue;
         sprintf(szBuff, "[RadioDuplicateDetection] VID %u: Stream %d: Max recv packet: %u, last recv packet: %u (%u ms ago)",
            s_ListHistoryRxPacketsVehicles[i].uVehicleId, k,
            s_ListHistoryRxPacketsVehicles[i].streamsPacketsHistory[k].uMaxReceivedPacketIndex,
            s_ListHistoryRxPacketsVehicles[i].streamsPacketsHistory[k].uLastReceivedPacketIndex,
            uTime - s_ListHistoryRxPacketsVehicles[i].streamsPacketsHistory[k].uLastTimeReceivedPacket );
         log_line(szBuff);
      }
   }
}

int _radio_dup_detection_get_runtime_index_for_vid(u32 uVehicleId, u8* pPacketBuffer, int iPacketLength)
{
   int iStatsIndex = -1;

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( uVehicleId == s_ListHistoryRxPacketsVehicles[i].uVehicleId )
      {
         iStatsIndex = i;
         break;
      }
   }
   if ( iStatsIndex != -1 )
      return iStatsIndex;

   // New vehicle id, add it to the runtime list

   log_line("[RadioDuplicateDetection] Start receiving data from VID: %u", uVehicleId);
   if ( (NULL != pPacketBuffer) && (iPacketLength >= (int)sizeof(t_packet_header)) )
   {
      t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
      log_line("[RadioDuplicateDetection] Received packet type: %s, %d bytes, source VID: %u, dest VID:%u",
         str_get_packet_type(pPH->packet_type), pPH->total_length, pPH->vehicle_id_src, pPH->vehicle_id_dest);
   }

   char szBuff[256];
   szBuff[0] = 0;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      char szTmp[32];
      sprintf(szTmp, "%u", s_ListHistoryRxPacketsVehicles[i].uVehicleId);
      if ( 0 != i )
         strcat(szBuff, ", ");
      strcat(szBuff, szTmp);
   }
   if ( 0 == szBuff[0] )
      strcpy(szBuff, "None");
   log_line("[RadioDuplicateDetection] Current list of receiving VIDs: [%s]", szBuff);

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( 0 == s_ListHistoryRxPacketsVehicles[i].uVehicleId )
      {
         iStatsIndex = i;
         break;
      }
   }

   if ( -1 == iStatsIndex )
   {
      szBuff[0] = 0;
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         char szTmp[32];
         sprintf(szTmp, "%u", s_ListHistoryRxPacketsVehicles[i].uVehicleId);
         if ( 0 != i )
            strcat(szBuff, ", ");
         strcat(szBuff, szTmp);
      }
      if ( 0 == szBuff[0] )
         strcpy(szBuff, "None");

      log_softerror_and_alarm("[RadioDuplicateDetection] No more room in vehicles list. Received data from VID %u, list of current VIDs (%d): [%s]", 
         uVehicleId, MAX_CONCURENT_VEHICLES, szBuff);
      return -1;
   }

   _radio_dd_reset_duplication_stats_for_vehicle(iStatsIndex, 1);
   s_ListHistoryRxPacketsVehicles[iStatsIndex].uVehicleId = uVehicleId;

   szBuff[0] = 0;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      char szTmp[32];
      sprintf(szTmp, "%u", s_ListHistoryRxPacketsVehicles[i].uVehicleId);
      if ( 0 != i )
         strcat(szBuff, ", ");
      strcat(szBuff, szTmp);
   }
   if ( 0 == szBuff[0] )
      strcpy(szBuff, "None");
   log_line("[RadioDuplicateDetection] Updated list of receiving VIDs: [%s]", szBuff);
   return iStatsIndex;
}

// return 1 if packet is duplicate, 0 if it's not duplicate

int radio_dup_detection_is_duplicate_on_stream(int iRadioInterfaceIndex, u8* pPacketBuffer, int iPacketLength, u32 uTimeNow)
{
   if ( (NULL == pPacketBuffer) || (iPacketLength <= 0) )
      return 1;

   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   
   u32 uVehicleId = pPH->vehicle_id_src;
   u32 uStreamPacketIndex = (pPH->stream_packet_idx) & PACKET_FLAGS_MASK_STREAM_PACKET_IDX;
   u32 uStreamIndex = (pPH->stream_packet_idx)>>PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX; 
   u8 uPacketType = pPH->packet_type;   
   int iStatsIndex = -1;

   iStatsIndex = _radio_dup_detection_get_runtime_index_for_vid(uVehicleId, pPacketBuffer, iPacketLength);
   if ( -1 == iStatsIndex )
      return 1;

   t_vehicle_history_packets_indexes* pDupInfo = &s_ListHistoryRxPacketsVehicles[iStatsIndex];
   pDupInfo->uVehicleId = uVehicleId;
   
   static u32 s_TimeLastLogAlarmStreamPacketsVariation = 0;

   u32 uMaxDeltaForVideoStream = 2000;
   u32 uMaxDeltaForDataStream = 50;
   if ( hardware_radio_index_is_serial_radio(iRadioInterfaceIndex) )
      uMaxDeltaForDataStream = 200;

   if ( (uStreamIndex != STREAM_ID_AUDIO) && (uStreamIndex < STREAM_ID_VIDEO_1) )
   if ((pDupInfo->streamsPacketsHistory[uStreamIndex].uMaxReceivedPacketIndex > uMaxDeltaForDataStream ) && 
           (uStreamPacketIndex < pDupInfo->streamsPacketsHistory[uStreamIndex].uMaxReceivedPacketIndex - uMaxDeltaForDataStream) )
   if ( pDupInfo->streamsPacketsHistory[uStreamIndex].uLastTimeReceivedPacket > uTimeNow - 4000 )
   if ( uTimeNow > s_TimeLastLogAlarmStreamPacketsVariation + 1000 )
   {
      s_TimeLastLogAlarmStreamPacketsVariation = get_current_timestamp_ms();
      log_line("[RadioDuplicateDetection] Received stream-%d packet index %u on radio interface %d, is %u packets older than max packet for the stream (%u).",
         (int)uStreamIndex, iRadioInterfaceIndex+1, uStreamPacketIndex,
         pDupInfo->streamsPacketsHistory[uStreamIndex].uMaxReceivedPacketIndex-uStreamPacketIndex,
         pDupInfo->streamsPacketsHistory[uStreamIndex].uMaxReceivedPacketIndex);
   }

   // --------------------------------------------------------
   // Begin: Detect if stream restarted

   int iStreamRestarted = 0;

   if ( (uStreamIndex == STREAM_ID_AUDIO) || (uStreamIndex >= STREAM_ID_VIDEO_1) )
   if ( pDupInfo->streamsPacketsHistory[uStreamIndex].uMaxReceivedPacketIndex > uStreamPacketIndex + uMaxDeltaForVideoStream )
      iStreamRestarted = 1;

   if ( (uStreamIndex != STREAM_ID_AUDIO) && (uStreamIndex < STREAM_ID_VIDEO_1) )
   if ( pDupInfo->streamsPacketsHistory[uStreamIndex].uMaxReceivedPacketIndex > uStreamPacketIndex + uMaxDeltaForDataStream )
      iStreamRestarted = 1;

   if ( 0 != pDupInfo->streamsPacketsHistory[uStreamIndex].uLastTimeReceivedPacket )
   if ( pDupInfo->streamsPacketsHistory[uStreamIndex].uLastTimeReceivedPacket < uTimeNow - 8000 )
   if ( uStreamPacketIndex+10 < pDupInfo->streamsPacketsHistory[uStreamIndex].uMaxReceivedPacketIndex )
      iStreamRestarted = 1;

   if ( iStreamRestarted )
   {
      char szTime[128];
      u32 uDeltaTime = uTimeNow - pDupInfo->streamsPacketsHistory[uStreamIndex].uLastTimeReceivedPacket;
      if ( uDeltaTime < 1000 )
         sprintf(szTime, "%u ms", uDeltaTime);
      else
         sprintf(szTime, "%u.%03u sec", uDeltaTime/1000, uDeltaTime%1000);

      if ( NULL != s_pSMRadioStatsDuplicateDetection )
      {
         uDeltaTime = radio_stats_get_time_last_received_packet_on_stream(s_pSMRadioStatsDuplicateDetection, uVehicleId, uStreamIndex);
         char szTmp[32];
         if ( uDeltaTime < 1000 )
            sprintf(szTmp, " (%u ms)", uDeltaTime);
         else
            sprintf(szTmp, " (%u.%03u sec)", uDeltaTime/1000, uDeltaTime%1000);
         strcat(szTime, szTmp);
      }

      log_line("[RadioDuplicateDetection] Detected stream restart on the other end of the radio link for VID %u. On stream: %d (%s), received stream packet index: %u, max recv stream packet index: %u, last received packet on this stream was %s ago. Packet tpye: %s. Do reset duplicate info.",
         uVehicleId, uStreamIndex, str_get_radio_stream_name(uStreamIndex),
         uStreamPacketIndex, pDupInfo->streamsPacketsHistory[uStreamIndex].uMaxReceivedPacketIndex,
         szTime, str_get_packet_type(uPacketType) );

      _radio_dd_reset_duplication_stats_for_vehicle(iStatsIndex, 2);
      pDupInfo->iRestartDetected = 1;
      pDupInfo->uVehicleId = uVehicleId;
   }

   // End: Detect if stream restarted
   // --------------------------------------------------------

   // ---------------------------------------------------
   // Check for packet duplication on stream for vehicle

   int bIsDuplicatePacket = 0;
   int iHashIndex = uStreamPacketIndex & PACKETS_INDEX_HASH_MASK;
   if ( uStreamPacketIndex == pDupInfo->streamsPacketsHistory[uStreamIndex].packetsHashIndexes[iHashIndex] )
      bIsDuplicatePacket = 1;

   if ( (uPacketType == PACKET_TYPE_RUBY_PING_CLOCK) || (uPacketType == PACKET_TYPE_RUBY_PING_CLOCK_REPLY) )
      bIsDuplicatePacket = 0;

   if ( bIsDuplicatePacket )
      return 1;

   
   pDupInfo->streamsPacketsHistory[uStreamIndex].packetsHashIndexes[iHashIndex] = uStreamPacketIndex;
   pDupInfo->streamsPacketsHistory[uStreamIndex].uLastReceivedPacketIndex = uStreamPacketIndex;
   if ( uStreamPacketIndex > pDupInfo->streamsPacketsHistory[uStreamIndex].uMaxReceivedPacketIndex )
      pDupInfo->streamsPacketsHistory[uStreamIndex].uMaxReceivedPacketIndex = uStreamPacketIndex;

   pDupInfo->streamsPacketsHistory[uStreamIndex].uLastTimeReceivedPacket = s_uRadioRxTimeNow;

   // End - Check for packet duplication on stream for vehicle
   // -------------------------------------------------------------

   return 0;
}

void radio_duplicate_detection_remove_data_for_all_except(u32 uVehicleId)
{
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( (uVehicleId == 0) || (uVehicleId != s_ListHistoryRxPacketsVehicles[i].uVehicleId) )
         _radio_dd_reset_duplication_stats_for_vehicle(i, 3);
   }
}

void radio_duplicate_detection_remove_data_for_vid(u32 uVehicleId)
{
   if ( (uVehicleId == 0) || (uVehicleId == MAX_U32) )
      return;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( uVehicleId == s_ListHistoryRxPacketsVehicles[i].uVehicleId )
         _radio_dd_reset_duplication_stats_for_vehicle(i, 7);
   }
}

int radio_dup_detection_is_vehicle_restarted(u32 uVehicleId)
{
   for ( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      if ( s_ListHistoryRxPacketsVehicles[i].uVehicleId == uVehicleId )
         return s_ListHistoryRxPacketsVehicles[i].iRestartDetected;
   return 0;
}

void radio_dup_detection_set_vehicle_restarted_flag(u32 uVehicleId)
{
   for ( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( s_ListHistoryRxPacketsVehicles[i].uVehicleId == uVehicleId )
      {
         s_ListHistoryRxPacketsVehicles[i].iRestartDetected = 1;
         break;
      }
   }
}

void radio_dup_detection_reset_vehicle_restarted_flag(u32 uVehicleId)
{
   for ( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( s_ListHistoryRxPacketsVehicles[i].uVehicleId == uVehicleId )
      {
         _radio_dd_reset_duplication_stats_for_vehicle(i, 4);
         s_ListHistoryRxPacketsVehicles[i].iRestartDetected = 0;
         s_ListHistoryRxPacketsVehicles[i].uVehicleId = uVehicleId;
         break;
      }
   }
}

u32 radio_dup_detection_get_max_received_packet_index_for_stream(u32 uVehicleId, u32 uStreamId)
{
   if ( uVehicleId == 0 || uVehicleId == MAX_U32 )
      return 0;

   if ( uStreamId >= MAX_RADIO_STREAMS )
      return 0;

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( s_ListHistoryRxPacketsVehicles[i].uVehicleId == uVehicleId )
         return s_ListHistoryRxPacketsVehicles[i].streamsPacketsHistory[uStreamId].uMaxReceivedPacketIndex;
   }

   return 0;
}