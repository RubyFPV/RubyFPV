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
#include "../base/encr.h"
#include "../base/config_hw.h"
#include "../base/hardware_procs.h"
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "radio_rx.h"
#include "radiolink.h"
#include "radio_duplicate_det.h"
#include <poll.h>


int s_iRadioRxInitialized = 0;
int s_iRadioRxThreadRunning = 0;
int s_iRadioRxSignalStop = 0;
int s_iRadioRxResetSignalInfo = 0;
int s_iRadioRxDevMode = 0;
int s_iRadioRxLoopTimeoutInterval = 60;
int s_iRadioRxMarkedForQuit = 0;
int s_iRxCPUAffinityCore = -1;
int s_iRxCPUAffinityCorePending = -1;
int s_iCurrentRxThreadRawPriority = -1;
int s_iPendingRxThreadRawPriority = -1;

t_radio_rx_state s_RadioRxState;
pthread_t s_pThreadRadioRx;
static int s_iRxThreadLoopCounter = 0;

shared_mem_radio_stats* s_pSMRadioStats = NULL;
int s_iSearchMode = 0;
u32 s_uRadioRxTimeNow = 0;
u32 s_uRadioRxMaxTimeRead = 0;
u32 s_uRadioRxLastTimeQueue = 0;
u32 s_uRadioRxLastReceivedPacket =0;

u32 s_uRadioRxLoopTimeMin = 10000;
u32 s_uRadioRxLoopTimeMax = 0;
u32 s_uRadioRxLoopTimeAvg = 2;
u32 s_uRadioRxLoopTimeSpikesAvg = 10;
u32 s_uRadioRxLoopSpikesCount = 0;
#define MAX_SPIKES_TO_LOG 10
u32 s_uRadioRxLoopLastSpikesTimes[MAX_SPIKES_TO_LOG];
u32 s_uRadioRxLoopLastSpikesRxPackets[MAX_SPIKES_TO_LOG];
u32 s_uRadioRxLoopLastSpikesIndex = 0;

int s_iRadioRxPausedInterfaces[MAX_RADIO_INTERFACES];
int s_iRadioRxAllInterfacesPaused = 0;

fd_set s_RadioRxReadSet;
fd_set s_RadioRxExceptionSet;
int s_iRadioRxCountFDs = 0;
int s_iRadioRxMaxFD = 0;
struct timeval s_iRadioRxReadTimeInterval;

u8 s_tmpLastProcessedRadioRxPacket[MAX_PACKET_TOTAL_SIZE];

u32 s_uLastRxShortPacketsVehicleIds[MAX_RADIO_INTERFACES];

// Pointers to array of int-s (max radio cards, for each card)
u8* s_pPacketsCounterOutputHighPriority = NULL;
u8* s_pPacketsCounterOutputData = NULL;
u8* s_pPacketsCounterOutputMissing = NULL;
u8* s_pPacketsCounterOutputMissingMaxGap = NULL;
u8* s_pRxAirGapTracking = NULL;

extern pthread_mutex_t s_pMutexRadioSyncRxTxThreads;
extern int s_iMutexRadioSyncRxTxThreadsInitialized;

_ATOMIC_PREFIX int s_bHasPendingExternalOperation = 0;
_ATOMIC_PREFIX int s_bCanDoExternalOperations = 0;

extern u32 s_uLastRadioPingSentTime;
extern u8 s_uLastRadioPingId;

static pthread_mutex_t s_MutexRadioRxFrameTimings = PTHREAD_MUTEX_INITIALIZER;
static u32 s_uRadioRxCurrentFrameStartTime = 0;
static u32 s_uRadioRxCurrentFrameEndTime = 0;
static u16 s_uRadioRxCurrentFrameNumber = 0;

t_radio_rx_state_vehicle* _radio_rx_get_stats_structure_for_vehicle(u32 uVehicleId)
{
   t_radio_rx_state_vehicle* pStatsVehicle = NULL;

   //----------------------------------------------
   // Begin: Compute index of stats to use

   if ( (s_RadioRxState.vehicles[0].uVehicleId == 0) || (s_RadioRxState.vehicles[0].uVehicleId == uVehicleId)  )
      pStatsVehicle = &(s_RadioRxState.vehicles[0]);
   
   if ( NULL == pStatsVehicle )
   {
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( s_RadioRxState.vehicles[i].uVehicleId == uVehicleId )
         {
            pStatsVehicle = &(s_RadioRxState.vehicles[i]);
            break;
         }
      }
   }

   if ( NULL == pStatsVehicle )
   {
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( s_RadioRxState.vehicles[i].uVehicleId == 0 )
         {
            pStatsVehicle = &(s_RadioRxState.vehicles[i]);
            break;
         }
      }
   }

   if ( NULL == pStatsVehicle )
   {
      pStatsVehicle = &(s_RadioRxState.vehicles[MAX_CONCURENT_VEHICLES-1]);
      log_softerror_and_alarm("[RadioRx] No more room in vehicles runtime info structure for new vehicle VID %u. Reuse last index: %d", uVehicleId, MAX_CONCURENT_VEHICLES-1);
      char szTmp[256];
      szTmp[0] = 0;
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         char szT[32];
         sprintf(szT, "%u", s_RadioRxState.vehicles[i].uVehicleId);
         if ( 0 != i )
            strcat(szTmp, ", ");
         strcat(szTmp, szT);
      }
      log_softerror_and_alarm("[RadioRx] Current vehicles in vehicles runtime info: [%s]", szTmp);  
   }

   pStatsVehicle->uVehicleId = uVehicleId;

   // End: Compute index of stats to use
   //----------------------------------------------
   return pStatsVehicle;
}

// returns the number of missing packets detected on the radio link
int _radio_rx_update_local_stats_on_new_radio_packet(int iInterface, int iIsShortPacket, u32 uVehicleId, u8* pPacket, int iLength, int iDataIsOk)
{
   if ( (NULL == pPacket) || ( iLength <= 2 ) )
      return 0;

   int nReturnLost = 0;

   t_radio_rx_state_vehicle* pStatsVehicle = _radio_rx_get_stats_structure_for_vehicle(uVehicleId);

   if ( iDataIsOk )
   {
      pStatsVehicle->uTotalRxPackets++;
      pStatsVehicle->uTmpRxPackets++;
   }
   else
   {
      pStatsVehicle->uTotalRxPacketsBad++;
      pStatsVehicle->uTmpRxPacketsBad++;
   }

   if ( iIsShortPacket )
   {
      t_packet_header_short* pPHS = (t_packet_header_short*)pPacket;
      u32 uNext = ((pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface]+1) & 0xFF);
      if ( pPHS->packet_id != uNext )
      {
         u32 lost = pPHS->packet_id - uNext;
         if ( pPHS->packet_id < uNext )
            lost = pPHS->packet_id + 255 - uNext;
         pStatsVehicle->uTotalRxPacketsLost += lost;
         pStatsVehicle->uTmpRxPacketsLost += lost;
         nReturnLost = lost;
      }

      pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface] = pPHS->packet_id;
   }
   else
   {
      t_packet_header* pPH = (t_packet_header*)pPacket;
      
      // Check for lost packets
      if ( pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface] > 0 )
      if ( pPH->radio_link_packet_index > pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface]+1 )
      {
         u32 lost = pPH->radio_link_packet_index - (pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface] + 1);
         pStatsVehicle->uTotalRxPacketsLost += lost;
         pStatsVehicle->uTmpRxPacketsLost += lost;
         nReturnLost = lost;
      }

      // Check for restarted on the other end
      //if ( pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface] > 10 )
      //if ( pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface] < 0xFFFF-10 ) // pPH->radio_link_packet_index is 16 bits
      //if ( pPH->radio_link_packet_index < pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface]-10 )
      //    radio_dup_detection_set_vehicle_restarted_flag(pPH->vehicle_id_src);

      pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface] = pPH->radio_link_packet_index;
   }
   return nReturnLost;
}

void _radio_rx_update_fd_sets()
{
   FD_ZERO(&s_RadioRxReadSet);
   FD_ZERO(&s_RadioRxExceptionSet);

   s_iRadioRxCountFDs = 0;
   s_iRadioRxMaxFD = 0;
   s_iRadioRxReadTimeInterval.tv_sec = 0;
   s_iRadioRxReadTimeInterval.tv_usec = 10000; // 10 milisec timeout

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( (NULL == pRadioHWInfo) || (! pRadioHWInfo->openedForRead) )
         continue;
      if ( s_RadioRxState.iRadioInterfacesBroken[i] )
         continue;
      if ( s_iRadioRxPausedInterfaces[i] )
         continue;

      FD_SET(pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd, &s_RadioRxReadSet);
      FD_SET(pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd, &s_RadioRxExceptionSet);

      s_iRadioRxCountFDs++;
      if ( pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd > s_iRadioRxMaxFD )
         s_iRadioRxMaxFD = pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd;
   }
   s_iRadioRxMaxFD++;
}



u8* _radio_rx_wait_get_queue_packet(t_radio_rx_state_packets_queue* pQueue, int iHighPriorityQueue, u32 uTimeoutMicroSec, int* pLength, int* pIsShortPacket, int* pRadioInterfaceIndex)
{
   int iRes = -1;
   /*
   if ( 0 == uTimeoutMicroSec )
      iRes = sem_trywait(pQueue->pSemaphoreRead);
   else
   {
      struct timespec ts;
      clock_gettime(CLOCK_REALTIME, &ts);
      ts.tv_nsec += 1000LL*(long long)uTimeoutMicroSec*1000LL;
      if ( ts.tv_nsec >= 1000000000LL )
      {
         ts.tv_sec++;
         ts.tv_nsec -= 1000000000L;
      }
      iRes = sem_timedwait(pQueue->pSemaphoreRead, &ts);
   }
   if ( 0 != iRes )
   {
      if ( errno != ETIMEDOUT )
         log_softerror_and_alarm("[RadioRx] Failed to timewait on %s semaphore for %u micros. Error: %d, %d, %s", iHighPriorityQueue?"high prio":"reg prio", uTimeoutMicroSec, iRes, errno, strerror(errno));
      return NULL;
   }
   */
   iRes = sem_trywait(pQueue->pSemaphoreRead);
   if ( (0 != iRes) && (0 != uTimeoutMicroSec) )
   {
      hardware_sleep_micros(200);
      iRes = sem_trywait(pQueue->pSemaphoreRead);
   }
   if ( 0 != iRes )
      return NULL;
   int iIndexToCopy = -1;
   pthread_mutex_lock(&pQueue->mutexLock);
   iIndexToCopy = pQueue->iCurrentPacketIndexToConsume;
   if ( (iIndexToCopy != pQueue->iCurrentPacketIndexToWrite) && (NULL != pQueue->pPacketsBuffers[iIndexToCopy]) && (pQueue->iPacketsLengths[iIndexToCopy] > 0) && (pQueue->iPacketsLengths[iIndexToCopy] < MAX_PACKET_TOTAL_SIZE) )
      pQueue->iCurrentPacketIndexToConsume = (pQueue->iCurrentPacketIndexToConsume + 1 ) % pQueue->iQueueSize;
   else
       iIndexToCopy = -1;
   pthread_mutex_unlock(&pQueue->mutexLock);

   if ( -1 == iIndexToCopy )
      return NULL;

   if ( NULL != pLength )
      *pLength = pQueue->iPacketsLengths[iIndexToCopy];
   if ( NULL != pIsShortPacket )
      *pIsShortPacket = pQueue->uPacketsAreShort[iIndexToCopy];
   if ( NULL != pRadioInterfaceIndex )
      *pRadioInterfaceIndex = pQueue->uPacketsRxInterface[iIndexToCopy];

   memcpy(s_tmpLastProcessedRadioRxPacket, pQueue->pPacketsBuffers[iIndexToCopy], pQueue->iPacketsLengths[iIndexToCopy]);

   return s_tmpLastProcessedRadioRxPacket;
}

u32 radio_rx_get_current_frame_start_time()
{
   u32 uTime = 0;
   pthread_mutex_lock(&s_MutexRadioRxFrameTimings);
   uTime = s_uRadioRxCurrentFrameStartTime;
   pthread_mutex_unlock(&s_MutexRadioRxFrameTimings);
   return uTime; 
}

u32 radio_rx_get_current_frame_end_time()
{
   u32 uTime = 0;
   pthread_mutex_lock(&s_MutexRadioRxFrameTimings);
   uTime = s_uRadioRxCurrentFrameEndTime;
   pthread_mutex_unlock(&s_MutexRadioRxFrameTimings);
   return uTime;
}

u16 radio_rx_get_current_frame_number()
{
   u16 uFrame = 0;
   pthread_mutex_lock(&s_MutexRadioRxFrameTimings);
   uFrame = s_uRadioRxCurrentFrameNumber;
   pthread_mutex_unlock(&s_MutexRadioRxFrameTimings);
   return uFrame;
}

u8* radio_rx_wait_get_next_received_high_prio_packet(u32 uTimeoutMicroSec, int* pLength, int* pIsShortPacket, int* pRadioInterfaceIndex)
{
   if ( NULL != pLength )
      *pLength = 0;
   if ( NULL != pIsShortPacket )
      *pIsShortPacket = 0;
   if ( NULL != pRadioInterfaceIndex )
      *pRadioInterfaceIndex = 0;
   if ( 0 == s_iRadioRxInitialized )
      return NULL;

   return _radio_rx_wait_get_queue_packet(&(s_RadioRxState.queue_high_priority), 1, uTimeoutMicroSec, pLength, pIsShortPacket, pRadioInterfaceIndex);
}

u8* radio_rx_wait_get_next_received_reg_prio_packet(u32 uTimeoutMicroSec, int* pLength, int* pIsShortPacket, int* pRadioInterfaceIndex)
{
   if ( NULL != pLength )
      *pLength = 0;
   if ( NULL != pIsShortPacket )
      *pIsShortPacket = 0;
   if ( NULL != pRadioInterfaceIndex )
      *pRadioInterfaceIndex = 0;
   if ( 0 == s_iRadioRxInitialized )
      return NULL;

   return _radio_rx_wait_get_queue_packet(&(s_RadioRxState.queue_reg_priority), 0, uTimeoutMicroSec, pLength, pIsShortPacket, pRadioInterfaceIndex);
}

void _radio_rx_add_packet_to_rx_queue(u8* pPacket, int iLength, int iRadioInterface)
{
   if ( (NULL == pPacket) || (iLength <= 0) || s_iRadioRxMarkedForQuit )
      return;

   t_packet_header* pPH = (t_packet_header*)pPacket;
   u8 uPacketFlags = pPH->packet_flags;

   t_radio_rx_state_packets_queue* pQueue = &s_RadioRxState.queue_reg_priority;
   if ( uPacketFlags & PACKET_FLAGS_BIT_HIGH_PRIORITY )
      pQueue = &s_RadioRxState.queue_high_priority;

   int iIndexToWriteTo = -1;
   pthread_mutex_lock(&pQueue->mutexLock);
   iIndexToWriteTo = pQueue->iCurrentPacketIndexToWrite;
   // No more room? Discard oldest packet
   if ( ((pQueue->iCurrentPacketIndexToWrite+1) % pQueue->iQueueSize) == pQueue->iCurrentPacketIndexToConsume )
       pQueue->iCurrentPacketIndexToConsume = (pQueue->iCurrentPacketIndexToConsume+1) % pQueue->iQueueSize;

   pQueue->iCurrentPacketIndexToWrite = (pQueue->iCurrentPacketIndexToWrite + 1) % pQueue->iQueueSize;
   pthread_mutex_unlock(&pQueue->mutexLock);

   // Add the packet to the queue
   pQueue->uPacketsRxInterface[iIndexToWriteTo] = iRadioInterface;
   pQueue->uPacketsAreShort[iIndexToWriteTo] = 0;
   pQueue->iPacketsLengths[iIndexToWriteTo] = iLength;
   memcpy(pQueue->pPacketsBuffers[iIndexToWriteTo], pPacket, iLength);
      
   if ( (NULL != pQueue->pSemaphoreWrite) && (0 != sem_post(pQueue->pSemaphoreWrite)) )
      log_softerror_and_alarm("Failed to set semaphore for packet ready.");
 
   iIndexToWriteTo = (iIndexToWriteTo + 1) % pQueue->iQueueSize;
   int iCountPackets = iIndexToWriteTo - pQueue->iCurrentPacketIndexToConsume;
   if ( iIndexToWriteTo < pQueue->iCurrentPacketIndexToConsume )
      iCountPackets = iIndexToWriteTo + (pQueue->iQueueSize - pQueue->iCurrentPacketIndexToConsume);

   if ( iCountPackets > pQueue->iStatsMaxPacketsInQueueLastMinute )
      pQueue->iStatsMaxPacketsInQueueLastMinute = iCountPackets;
   if ( iCountPackets > pQueue->iStatsMaxPacketsInQueue )
      pQueue->iStatsMaxPacketsInQueue = iCountPackets;

   //s_uRadioRxLastTimeQueue += get_current_timestamp_ms() - s_uRadioRxTimeNow;
}

void _radio_rx_check_add_packet_to_rx_queue(u8* pPacket, int iLength, int iRadioInterfaceIndex)
{
   if ( radio_dup_detection_is_duplicate_on_stream(iRadioInterfaceIndex, pPacket, iLength, s_uRadioRxTimeNow) )
      return;

   if ( NULL != s_pSMRadioStats )
     radio_stats_update_on_unique_packet_received(s_pSMRadioStats, s_uRadioRxTimeNow, iRadioInterfaceIndex, pPacket, iLength);

   _radio_rx_add_packet_to_rx_queue(pPacket, iLength, iRadioInterfaceIndex);
}

void _radio_rx_update_frame_times(u8* pPacketBuffer, int iRxDatarate)
{
   if ( (NULL == pPacketBuffer) || (0 == iRxDatarate) )
      return;

   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   t_packet_header_video_segment* pPHVS = (t_packet_header_video_segment*) (pPacketBuffer+sizeof(t_packet_header));
   u32 uBytesToEOF = pPH->total_length * 8 * ((pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_MASK_EOF_COUNTER) + ((pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_MASK_DATA_COUNTER)>>16)/2);
   u32 uTimeMsToEOF = (uBytesToEOF * 1000) / getRealDataRateFromRadioDataRate(iRxDatarate, 0, 0);
   u32 uTimeEnd = s_uRadioRxTimeNow + uTimeMsToEOF;

   u32 uTimeStart = s_uRadioRxCurrentFrameStartTime;
   if ( pPHVS->uH264FrameIndex > s_uRadioRxCurrentFrameNumber )
   {
      uTimeStart = s_uRadioRxTimeNow;
      if ( pPHVS->uFramePacketsInfo & 0xFF )
      {
         u32 uDeltaBytesBefore = pPH->total_length * 8 * (pPHVS->uFramePacketsInfo & 0xFF);
         u32 uDeltaTimeMs = (uDeltaBytesBefore * 1000) / getRealDataRateFromRadioDataRate(iRxDatarate, 0, 0);
         uTimeStart -= uDeltaTimeMs;
      }
   }

   pthread_mutex_lock(&s_MutexRadioRxFrameTimings);
   s_uRadioRxCurrentFrameStartTime = uTimeStart;
   s_uRadioRxCurrentFrameEndTime = uTimeEnd;
   s_uRadioRxCurrentFrameNumber = pPHVS->uH264FrameIndex;
   pthread_mutex_unlock(&s_MutexRadioRxFrameTimings);
   
   /*
   log_line("DBG set frame EOF to %u ms from now, fr start to %u ms ago, for fr f%d pckt [%u/%d], blk schm %d/%d, fr packet %d of %d, EOF in %d, DR: %d",
    s_uRadioRxCurrentFrameEndTime - s_uRadioRxTimeNow,
    s_uRadioRxTimeNow - s_uRadioRxCurrentFrameStartTime,
    pPHVS->uH264FrameIndex,
    pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex,
    pPHVS->uCurrentBlockDataPackets, pPHVS->uCurrentBlockECPackets,
    pPHVS->uFramePacketsInfo & 0xFF, pPHVS->uFramePacketsInfo >> 8, pPHVS->uVideoStatusFlags2 & 0xFF,
    getRealDataRateFromRadioDataRate(iRxDatarate, 0, 0) );
   */
}

int _radio_rx_process_serial_short_packet(int iInterfaceIndex, u8* pPacketBuffer, int iPacketLength)
{
   static u8 s_uLastRxShortPacketsIds[MAX_RADIO_INTERFACES];
   static u8 s_uBuffersFullMessages[MAX_RADIO_INTERFACES][MAX_PACKET_TOTAL_SIZE*2];
   static int s_uBuffersFullMessagesReadPos[MAX_RADIO_INTERFACES];
   static int s_bInitializedBuffersFullMessages = 0;

   if ( ! s_bInitializedBuffersFullMessages )
   {
      s_bInitializedBuffersFullMessages = 1;
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      {
         s_uBuffersFullMessagesReadPos[i] = 0;
         s_uLastRxShortPacketsIds[i] = 0xFF;
         s_uLastRxShortPacketsVehicleIds[i] = 0;
      }
   }

   if ( (NULL == pPacketBuffer) || (iPacketLength < (int)sizeof(t_packet_header_short)) )
      return -1;
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex > hardware_get_radio_interfaces_count()) )
      return -1;

   t_packet_header_short* pPHS = (t_packet_header_short*)pPacketBuffer;

   // If it's the start of a full packet, reset rx buffer for this interface
   if ( pPHS->start_header == SHORT_PACKET_START_BYTE_START_PACKET )
   {
     s_uBuffersFullMessagesReadPos[iInterfaceIndex] = 0;
     if ( pPHS->data_length >= sizeof(t_packet_header) - sizeof(u32) )
     {
        t_packet_header* pPH = (t_packet_header*)(pPacketBuffer + sizeof(t_packet_header_short));
        s_uLastRxShortPacketsVehicleIds[iInterfaceIndex] = pPH->vehicle_id_src;
     }
   }

   // Update radio interfaces rx stats

   _radio_rx_update_local_stats_on_new_radio_packet(iInterfaceIndex, 1, s_uLastRxShortPacketsVehicleIds[iInterfaceIndex], pPacketBuffer, iPacketLength, 1);
   if ( NULL != s_pSMRadioStats )
      radio_stats_update_on_new_radio_packet_received(s_pSMRadioStats, s_uRadioRxTimeNow, iInterfaceIndex, pPacketBuffer, iPacketLength, 1, 1);
   
   // If there are missing packets, reset rx buffer for this interface
   
   u32 uNext = (((u32)(s_uLastRxShortPacketsIds[iInterfaceIndex])) + 1) & 0xFF;
   if ( uNext != pPHS->packet_id )
   {
      s_uBuffersFullMessagesReadPos[iInterfaceIndex] = 0;
   }
   s_uLastRxShortPacketsIds[iInterfaceIndex] = pPHS->packet_id;
   // Add the content of the packet to the buffer

   memcpy(&s_uBuffersFullMessages[iInterfaceIndex][s_uBuffersFullMessagesReadPos[iInterfaceIndex]], pPacketBuffer + sizeof(t_packet_header_short), pPHS->data_length);
   s_uBuffersFullMessagesReadPos[iInterfaceIndex] += pPHS->data_length;

   // Do we have a full valid radio packet?

   if ( s_uBuffersFullMessagesReadPos[iInterfaceIndex] >= (int)sizeof(t_packet_header) )
   {
      t_packet_header* pPH = (t_packet_header*) s_uBuffersFullMessages[iInterfaceIndex];
      if ( (pPH->total_length >= sizeof(t_packet_header)) && (s_uBuffersFullMessagesReadPos[iInterfaceIndex] >= pPH->total_length) )
      {
         u32 uCRC = base_compute_crc32(&s_uBuffersFullMessages[iInterfaceIndex][sizeof(u32)], pPH->total_length - sizeof(u32));
         if ( (uCRC & 0x00FFFFFF) == (pPH->uCRC & 0x00FFFFFF) )
         {
            s_uBuffersFullMessagesReadPos[iInterfaceIndex] = 0;
            _radio_rx_check_add_packet_to_rx_queue(s_uBuffersFullMessages[iInterfaceIndex], pPH->total_length, iInterfaceIndex);
         }
      }
   }
 
   // Too much data? Then reset the buffer and wait for the start of a new full packet.
   if ( s_uBuffersFullMessagesReadPos[iInterfaceIndex] >= MAX_PACKET_TOTAL_SIZE*2 - 255 )
     s_uBuffersFullMessagesReadPos[iInterfaceIndex] = 0;
   return 1;
}

// return nb of bytes on success, -1 if the interface is now invalid or broken

int _radio_rx_parse_received_serial_radio_data(int iInterfaceIndex)
{
   static u8 s_uBuffersSerialMessages[MAX_RADIO_INTERFACES][512];
   static int s_uBuffersSerialMessagesReadPos[MAX_RADIO_INTERFACES];
   static int s_bInitializedBuffersSerialMessages = 0;

   if ( ! s_bInitializedBuffersSerialMessages )
   {
      s_bInitializedBuffersSerialMessages = 1;
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         s_uBuffersSerialMessagesReadPos[i] = 0;
   }

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
   if ( (NULL == pRadioHWInfo) || (! pRadioHWInfo->openedForRead) )
   {
      log_softerror_and_alarm("[RadioRxThread] Tried to process a received short radio packet on radio interface (%d) that is not opened for read.", iInterfaceIndex+1);
      return -1;
   }
   if ( ! hardware_radio_index_is_serial_radio(iInterfaceIndex) )
   {
      log_softerror_and_alarm("[RadioRxThread] Tried to process a received short radio packet on radio interface (%d) that is not a serial radio.", iInterfaceIndex+1);
      return 0;
   }

   int iMaxRead = 512 - s_uBuffersSerialMessagesReadPos[iInterfaceIndex];

   #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_lock(&s_pMutexRadioSyncRxTxThreads);
   #endif

   int iRead = read(pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd, &(s_uBuffersSerialMessages[iInterfaceIndex][s_uBuffersSerialMessagesReadPos[iInterfaceIndex]]), iMaxRead);

   #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
   #endif

   if ( iRead < 0 )
   {
      log_softerror_and_alarm("[RadioRxThread] Failed to read received short radio packet on serial radio interface (%d).", iInterfaceIndex+1);
      return -1;
   }

   s_uBuffersSerialMessagesReadPos[iInterfaceIndex] += iRead;
   int iBufferLength = s_uBuffersSerialMessagesReadPos[iInterfaceIndex];
   
   // Received at least the full header?
   if ( iBufferLength < (int)sizeof(t_packet_header_short) )
      return iRead;

   int iPacketPos = -1;
   do
   {
      u8* pData = (u8*)&(s_uBuffersSerialMessages[iInterfaceIndex][0]);
      iPacketPos = -1;
      for( int i=0; i<iBufferLength-(int)sizeof(t_packet_header_short); i++ )
      {
         if ( radio_buffer_is_valid_short_packet(pData+i, iBufferLength-i) )
         {
            iPacketPos = i;
            break;
         }
      }
      // No valid packet found in the buffer?
      if ( iPacketPos < 0 )
      {
         if ( iBufferLength >= 400 )
         {
            int iBytesToDiscard = iBufferLength - 256;
            // Keep only the last 255 bytes in the buffer
            for( int i=0; i<(iBufferLength-iBytesToDiscard); i++ )
               s_uBuffersSerialMessages[iInterfaceIndex][i] = s_uBuffersSerialMessages[iInterfaceIndex][i+iBytesToDiscard];
            s_uBuffersSerialMessagesReadPos[iInterfaceIndex] -= iBytesToDiscard;

            _radio_rx_update_local_stats_on_new_radio_packet(iInterfaceIndex, 1, s_uLastRxShortPacketsVehicleIds[iInterfaceIndex], s_uBuffersSerialMessages[iInterfaceIndex], iBytesToDiscard, 0);
            radio_stats_set_bad_data_on_current_rx_interval(s_pSMRadioStats, iInterfaceIndex);
         }
         return iRead;
      }

      if ( iPacketPos > 0 )
      {
         _radio_rx_update_local_stats_on_new_radio_packet(iInterfaceIndex, 1, s_uLastRxShortPacketsVehicleIds[iInterfaceIndex], s_uBuffersSerialMessages[iInterfaceIndex], iPacketPos, 0);
         radio_stats_set_bad_data_on_current_rx_interval(s_pSMRadioStats, iInterfaceIndex);
      }
      t_packet_header_short* pPHS = (t_packet_header_short*)(pData+iPacketPos);
      int iShortTotalPacketSize = (int)(pPHS->data_length + sizeof(t_packet_header_short));
      
      _radio_rx_process_serial_short_packet(iInterfaceIndex, pData+iPacketPos, iShortTotalPacketSize);

      iShortTotalPacketSize += iPacketPos;
      if ( iShortTotalPacketSize > iBufferLength )
         iShortTotalPacketSize = iBufferLength;
      for( int i=0; i<iBufferLength - iShortTotalPacketSize; i++ )
         s_uBuffersSerialMessages[iInterfaceIndex][i] = s_uBuffersSerialMessages[iInterfaceIndex][i+iShortTotalPacketSize];
      s_uBuffersSerialMessagesReadPos[iInterfaceIndex] -= iShortTotalPacketSize;
      iBufferLength -= iShortTotalPacketSize;
   } while ( (iPacketPos >= 0) && (iBufferLength >= (int)sizeof(t_packet_header_short)) );
   return iRead;
}

// return number of packets parsed, -1 if the interface is now invalid or broken

int _radio_rx_parse_received_wifi_radio_data(int iInterfaceIndex, int iMaxReads)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return 0;

   int iReturn = 0;
   int iDataIsOk = 1;
   int iRxDatarate = 0;
   int iBufferLength = 0;
   u8* pPacketBuffer = NULL;
   int iCountParsed = 0;

   //static int sdebugCountParser = 0;
   //sdebugCountParser++;

   for( int iCountReads=0; iCountReads<iMaxReads; iCountReads++ )
   {
      iBufferLength = 0;
      iRxDatarate = 0;
      pPacketBuffer = radio_process_wlan_data_in(iInterfaceIndex, &iBufferLength, &iRxDatarate, s_uRadioRxTimeNow);
      if ( NULL == pPacketBuffer )
         break;

      if ( iBufferLength <= 0 )
      {
         log_softerror_and_alarm("[RadioRxThread] Rx cap returned an empty buffer (%d length) on radio interface index %d.", iBufferLength, iInterfaceIndex+1);
         iDataIsOk = 0;
         iReturn = -1;
         break;
      }

      iCountParsed++;

      if ( s_iRadioRxPausedInterfaces[iInterfaceIndex] )
         continue;
      iDataIsOk = 1;
      t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
      u8 uPacketFlags = pPH->packet_flags;
      u8 uPacketType = pPH->packet_type;
      u32 uVehicleId = pPH->vehicle_id_src;

      if ( s_iRadioRxDevMode )
      if ( uPacketType == PACKET_TYPE_RUBY_PING_CLOCK )
      {
         s_uLastRadioPingSentTime = get_current_timestamp_ms();
         s_uLastRadioPingId = *(pPacketBuffer +sizeof(t_packet_header));
      }

      if ( uPacketFlags & PACKET_FLAGS_BIT_HIGH_PRIORITY )
      {
         if ( NULL != s_pPacketsCounterOutputHighPriority )
            s_pPacketsCounterOutputHighPriority[iInterfaceIndex]++;
      }
      else
      {      
         if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) != PACKET_COMPONENT_VIDEO )
         if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) != PACKET_COMPONENT_AUDIO )
         if ( NULL != s_pPacketsCounterOutputData )
            s_pPacketsCounterOutputData[iInterfaceIndex]++;
      }
      
      int bCRCOk = 0;
      int iPacketLength = packet_process_and_check(iInterfaceIndex, pPacketBuffer, iBufferLength, &bCRCOk);

      if ( iPacketLength <= 0 )
      {
         log_softerror_and_alarm("[RadioRxThread] Process and check packet of %d bytes failed, error: %d", iBufferLength, get_last_processing_error_code());
         iDataIsOk = 0;
         s_RadioRxState.iRadioInterfacesRxBadPackets[iInterfaceIndex] = get_last_processing_error_code();
         continue;
      }

      if ( ! bCRCOk )
      {
         log_softerror_and_alarm("[RadioRxThread] Received broken packet (wrong CRC) on radio interface %d. Packet size: %d bytes, type: %s",
            iInterfaceIndex+1, pPH->total_length, str_get_packet_type(pPH->packet_type));
         iDataIsOk = 0;
         continue;
      }

      // Save received radio datarate as we don't need the CRC anymore
      pPH->uCRC = (u32)iRxDatarate;

      if ( (pPH->packet_type == PACKET_TYPE_VIDEO_DATA) )
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
      if ( !(pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED) )
         _radio_rx_update_frame_times(pPacketBuffer, iRxDatarate);

      if ( uPacketType == PACKET_TYPE_VIDEO_DATA )
      {
       /*
      t_packet_header_video_segment* pPHVS = (t_packet_header_video_segment*) (pPacketBuffer+sizeof(t_packet_header));
      int iDbgDR = (int) pPH->uCRC;
      log_line("DBG rx %d %c%d [%u/%02d of %02d] sch %d/%d, framep %d/%d, EOF in %d+%d, %u ms from now, NAL %s%s-%s%s%s, eof?%d DR: %d", 
          s_iRxThreadLoopCounter, (pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED)?'r':'f',
          pPHVS->uH264FrameIndex, pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex,
          pPHVS->uCurrentBlockDataPackets + pPHVS->uCurrentBlockECPackets,
          pPHVS->uCurrentBlockDataPackets, pPHVS->uCurrentBlockECPackets,
          pPHVS->uFramePacketsInfo & 0xFF, pPHVS->uFramePacketsInfo >> 8,
          pPHVS->uVideoStatusFlags2 & 0xFF,
          (pPHVS->uVideoStatusFlags2 >> 16) & 0xFF,
          radio_rx_get_current_frame_end_time() - g_TimeNow,
          (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_START)?"s":"",
          (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_END)?"e":"",
          (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_I)?"i":"",
          (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_P)?"p":"",
          (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_O)?"o":"",
          (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_END_OF_FRAME)?1:0,
          iDbgDR);
      /**/
      }

      _radio_rx_check_add_packet_to_rx_queue(pPacketBuffer, iPacketLength, iInterfaceIndex);

      if ( NULL != s_pRxAirGapTracking )
      {
         s_uRadioRxTimeNow = get_current_timestamp_ms();
         u32 uGap = s_uRadioRxTimeNow - s_uRadioRxLastReceivedPacket;
         if ( uGap > 255 )
            uGap = 255;
         if ( uGap > *s_pRxAirGapTracking )
            *s_pRxAirGapTracking = uGap;
         s_uRadioRxLastReceivedPacket = s_uRadioRxTimeNow;
      }

      int nLost = _radio_rx_update_local_stats_on_new_radio_packet(iInterfaceIndex, 0, uVehicleId, pPacketBuffer, iBufferLength, iDataIsOk);
      if ( nLost > 0 )
      {
         if ( NULL != s_pPacketsCounterOutputMissing)
            s_pPacketsCounterOutputMissing[iInterfaceIndex]++;
         if ( NULL != s_pPacketsCounterOutputMissingMaxGap )
         if ( nLost > s_pPacketsCounterOutputMissingMaxGap[iInterfaceIndex] )
            s_pPacketsCounterOutputMissingMaxGap[iInterfaceIndex] = (u8)nLost;
      }
      if ( NULL != s_pSMRadioStats )
         radio_stats_update_on_new_radio_packet_received(s_pSMRadioStats, s_uRadioRxTimeNow, iInterfaceIndex, pPacketBuffer, iBufferLength, 0, iDataIsOk);
   }

   if ( iReturn < 0 )
      return iReturn;

   return iCountParsed;
}

void _radio_rx_update_stats(u32 uTimeNow)
{
   static int s_iCounterRadioRxStatsUpdate = 0;
   static int s_iCounterRadioRxStatsUpdate2 = 0;

   if ( uTimeNow >= s_RadioRxState.uTimeLastStatsUpdate + 1000 )
   {
      s_RadioRxState.uTimeLastStatsUpdate = uTimeNow;
      s_iCounterRadioRxStatsUpdate++;
      int iAnyRxPackets = 0;
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( s_RadioRxState.vehicles[i].uVehicleId == 0 )
            continue;

         if ( (s_RadioRxState.vehicles[i].uTmpRxPackets > 0) || (s_RadioRxState.vehicles[i].uTmpRxPacketsBad > 0) )
            iAnyRxPackets = 1;
         if ( s_RadioRxState.vehicles[i].uTotalRxPackets > 0 )
         {
            if ( s_RadioRxState.vehicles[i].uTmpRxPackets > (u32)(s_RadioRxState.vehicles[i].iMaxRxPacketsPerSec) )
               s_RadioRxState.vehicles[i].iMaxRxPacketsPerSec = s_RadioRxState.vehicles[i].uTmpRxPackets;
            if ( s_RadioRxState.vehicles[i].uTmpRxPackets < (u32)(s_RadioRxState.vehicles[i].iMinRxPacketsPerSec) )
               s_RadioRxState.vehicles[i].iMinRxPacketsPerSec = s_RadioRxState.vehicles[i].uTmpRxPackets;
         }

         /*
         if ( (s_iCounterRadioRxStatsUpdate % 10) == 0 )
         {
            log_line("[RadioRxThread] Received packets from VID %u: %u/sec (min: %d/sec, max: %d/sec)", 
               s_RadioRxState.vehicles[i].uVehicleId, s_RadioRxState.vehicles[i].uTmpRxPackets,
               s_RadioRxState.vehicles[i].iMinRxPacketsPerSec, s_RadioRxState.vehicles[i].iMaxRxPacketsPerSec);
            log_line("[RadioRxThread] Total recv packets from VID: %u: %u, bad: %u, lost: %u",
               s_RadioRxState.vehicles[i].uVehicleId, s_RadioRxState.vehicles[i].uTotalRxPackets, s_RadioRxState.vehicles[i].uTotalRxPacketsBad, s_RadioRxState.vehicles[i].uTotalRxPacketsLost );
         }
         */
         
         s_RadioRxState.vehicles[i].uTmpRxPackets = 0;
         s_RadioRxState.vehicles[i].uTmpRxPacketsBad = 0;
         s_RadioRxState.vehicles[i].uTmpRxPacketsLost = 0;
      }

      if ( (s_iCounterRadioRxStatsUpdate % 10) == 0 )
      if ( 0 == iAnyRxPackets )
         log_line("[RadioRxThread] No packets received in the last seconds");
   }
   if ( uTimeNow >= s_RadioRxState.uTimeLastMinuteStatsUpdate + 1000 * 20 )
   {
      s_RadioRxState.uTimeLastMinuteStatsUpdate = uTimeNow;
      s_iCounterRadioRxStatsUpdate2++;

      radio_duplicate_detection_log_info();

      log_line("[RadioRxThread] Max packets in queues (high/reg prio): %d/%d. Max packets in queue in last 10 sec: %d/%d",
         s_RadioRxState.queue_high_priority.iStatsMaxPacketsInQueue,
         s_RadioRxState.queue_reg_priority.iStatsMaxPacketsInQueue,
         s_RadioRxState.queue_high_priority.iStatsMaxPacketsInQueueLastMinute,
         s_RadioRxState.queue_reg_priority.iStatsMaxPacketsInQueueLastMinute);
      s_RadioRxState.queue_high_priority.iStatsMaxPacketsInQueueLastMinute = 0;
      s_RadioRxState.queue_reg_priority.iStatsMaxPacketsInQueueLastMinute = 0;

      int iCountPacketsHigh = s_RadioRxState.queue_high_priority.iCurrentPacketIndexToWrite - s_RadioRxState.queue_high_priority.iCurrentPacketIndexToConsume;
      if ( s_RadioRxState.queue_high_priority.iCurrentPacketIndexToWrite < s_RadioRxState.queue_high_priority.iCurrentPacketIndexToConsume )
         iCountPacketsHigh = s_RadioRxState.queue_high_priority.iCurrentPacketIndexToWrite + (s_RadioRxState.queue_high_priority.iQueueSize - s_RadioRxState.queue_high_priority.iCurrentPacketIndexToConsume);

      int iCountPacketsReg = s_RadioRxState.queue_reg_priority.iCurrentPacketIndexToWrite - s_RadioRxState.queue_reg_priority.iCurrentPacketIndexToConsume;
      if ( s_RadioRxState.queue_reg_priority.iCurrentPacketIndexToWrite < s_RadioRxState.queue_reg_priority.iCurrentPacketIndexToConsume )
         iCountPacketsReg = s_RadioRxState.queue_reg_priority.iCurrentPacketIndexToWrite + (s_RadioRxState.queue_reg_priority.iQueueSize - s_RadioRxState.queue_reg_priority.iCurrentPacketIndexToConsume);

      log_line("[RadioRxThread] Packets in queues now pending consumption (high/reg prio): %d/%d",
         iCountPacketsHigh, iCountPacketsReg);

      if ( (s_iCounterRadioRxStatsUpdate2 % 10) == 0 )
      {
         log_line("[RadioRxThread] Reset max stats.");
         s_RadioRxState.queue_high_priority.iStatsMaxPacketsInQueue = 0;
         s_RadioRxState.queue_reg_priority.iStatsMaxPacketsInQueue = 0;
      }

      //log_line("[RadioRxThread] Min/Max/Avg/Spikes-Avg (+count) radio rx loop times: %u / %u / %u / %u (%u times) ms", s_uRadioRxLoopTimeMin, s_uRadioRxLoopTimeMax, s_uRadioRxLoopTimeAvg, s_uRadioRxLoopTimeSpikesAvg, s_uRadioRxLoopSpikesCount);
      /*
      char szLog[256];
      szLog[0] = 0;
      int iIndex = s_uRadioRxLoopLastSpikesIndex;
      for ( int i=0; i<MAX_SPIKES_TO_LOG; i++ )
      {
         if ( i != 0 )
            strcat(szLog, ", ");
         char szTmp[32];
         iIndex--;
         if ( iIndex < 0 )
            iIndex = MAX_SPIKES_TO_LOG-1;
         sprintf(szTmp, "(-%d)=(%d pkts, %u ms)", i+1, s_uRadioRxLoopLastSpikesRxPackets[iIndex], s_uRadioRxLoopLastSpikesTimes[iIndex]);
         strcat(szLog, szTmp);
      }
      log_line("[RadioRxThread] Last spikes: [%s]", szLog);
      */
   }
}

void * _thread_radio_rx(void *argument)
{
   s_iRadioRxThreadRunning = 1;
   log_line("[RadioRxThread] Started.");

   if ( s_iPendingRxThreadRawPriority > 0 )
   if ( s_iPendingRxThreadRawPriority != s_iCurrentRxThreadRawPriority )
   {
      hw_set_current_thread_raw_priority("[RadioRxThread]", s_iPendingRxThreadRawPriority);
      s_iCurrentRxThreadRawPriority = s_iPendingRxThreadRawPriority;
   }
   hw_log_current_thread_attributes("radio_rx");

   for( int i=0; i<MAX_SPIKES_TO_LOG; i++ )
   {
      s_uRadioRxLoopLastSpikesTimes[i] = 0;
      s_uRadioRxLoopLastSpikesRxPackets[i] = 0;
   }
   log_line("[RadioRxThread] Initialized State. Waiting for rx messages...");

   int* piQuit = (int*) argument;
   int iPollTimeoutMs = 100; // 5 ms
   int iLoopParsedPackets = 0;
   int iLoopErrorsCounter = 0;
   u32 uTimeLastLoopCheck = get_current_timestamp_ms();

   while ( 1 )
   {
      s_iRxThreadLoopCounter++;
      if ( (NULL != piQuit) && (*piQuit != 0 ) )
      {
         log_line("[RadioRxThread] Signaled to stop by quit argument. Exit the thread.");
         break;
      }
      if ( s_iRadioRxMarkedForQuit )
      {
         log_line("[RadioRxThread] Rx is marked for quit. Exit the thread.");
         break;
      }

      if ( s_bHasPendingExternalOperation )
      {
         s_bCanDoExternalOperations = 1;
         hardware_sleep_ms(1);
         continue;
      }
      
      s_bCanDoExternalOperations = 0;
      
      u32 uDeltaTime = s_uRadioRxTimeNow - uTimeLastLoopCheck;
      uTimeLastLoopCheck = s_uRadioRxTimeNow;

      //if ( uDeltaTime > iPollTimeoutMs+1 )
      //   log_line("DBG rx loop too long, %u ms", uDeltaTime);
      if ( uDeltaTime < 10000 )
      {
         if ( uDeltaTime > (u32)iPollTimeoutMs + 3 )
            log_line("DBG rxloop took %u ms", uDeltaTime);
         if ( uDeltaTime < s_uRadioRxLoopTimeMin )
            s_uRadioRxLoopTimeMin = uDeltaTime;
         if ( uDeltaTime > s_uRadioRxLoopTimeMax )
            s_uRadioRxLoopTimeMax = uDeltaTime;
         s_uRadioRxLoopTimeAvg = (s_uRadioRxLoopTimeAvg * 99 + uDeltaTime)/100;
         
         if ( s_iRxThreadLoopCounter > 1 )
         {
            if ( uDeltaTime >= (u32)iPollTimeoutMs + 10 )
            {
               iLoopErrorsCounter++;
               if ( (iLoopErrorsCounter % 20) == 1 )
                  log_softerror_and_alarm("ERROR Rx loop (count %d) took %u ms", iLoopErrorsCounter, uDeltaTime);
            }
            else
            {
               if ( iLoopErrorsCounter > 1 )
                  log_softerror_and_alarm("ERROR Rx loop (lcount %d) took %u ms", iLoopErrorsCounter, uDeltaTime);
               iLoopErrorsCounter = 0;
            }
         }
      }

      int iDbg1 = s_RadioRxState.queue_reg_priority.iCurrentPacketIndexToWrite - s_RadioRxState.queue_reg_priority.iCurrentPacketIndexToConsume;
      if ( s_RadioRxState.queue_reg_priority.iCurrentPacketIndexToWrite < s_RadioRxState.queue_reg_priority.iCurrentPacketIndexToConsume )
         iDbg1 = s_RadioRxState.queue_reg_priority.iCurrentPacketIndexToWrite + (s_RadioRxState.queue_reg_priority.iQueueSize - s_RadioRxState.queue_reg_priority.iCurrentPacketIndexToConsume);
      
      int iDbg2 = s_RadioRxState.queue_high_priority.iCurrentPacketIndexToWrite - s_RadioRxState.queue_high_priority.iCurrentPacketIndexToConsume;
      if ( s_RadioRxState.queue_high_priority.iCurrentPacketIndexToWrite < s_RadioRxState.queue_high_priority.iCurrentPacketIndexToConsume )
         iDbg2 = s_RadioRxState.queue_high_priority.iCurrentPacketIndexToWrite + (s_RadioRxState.queue_high_priority.iQueueSize - s_RadioRxState.queue_high_priority.iCurrentPacketIndexToConsume);

      //if ( (iDbg1 > 10) || (iDbg2 > 10) )
      //   log_line("DBG radio rx has %d reg and %d high prio pending packets to consume", iDbg1, iDbg2);
      
      // Loop is executed every 100 ms max. So check and update stats about every 250 ms max
      if ( 0 == (s_iRxThreadLoopCounter % 2) )
      {
         _radio_rx_update_stats(s_uRadioRxTimeNow);
         if ( s_iPendingRxThreadRawPriority != s_iCurrentRxThreadRawPriority )
         {
            log_line("[RadioRxThread] New thread raw priority must be set, from %d to %d.", s_iCurrentRxThreadRawPriority, s_iPendingRxThreadRawPriority);
            s_iCurrentRxThreadRawPriority = s_iPendingRxThreadRawPriority;

            if ( s_iPendingRxThreadRawPriority > 0 )
               hw_set_current_thread_raw_priority("[RadioRxThread]", s_iPendingRxThreadRawPriority);
            else
               hw_set_current_thread_raw_priority("[RadioRxThread]", 0);
            hw_log_current_thread_attributes("radio_rx");
         }

         if ( s_iRxCPUAffinityCorePending != s_iRxCPUAffinityCore )
         {
            log_line("[RadioRxThread] New core affinity is pending, from core %d to core %d", s_iRxCPUAffinityCore, s_iRxCPUAffinityCorePending);
            s_iRxCPUAffinityCore = s_iRxCPUAffinityCorePending;
            hw_set_current_thread_affinity("radio_rx", s_iRxCPUAffinityCorePending, s_iRxCPUAffinityCorePending);
            hw_log_current_thread_attributes("radio_rx");
         }
      }

      iLoopParsedPackets = 0;
      struct pollfd fds[MAX_RADIO_INTERFACES];
      int iRadioInterfacesWherePaused[MAX_RADIO_INTERFACES];
      s_iRadioRxCountFDs = 0;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         iRadioInterfacesWherePaused[i] = s_iRadioRxPausedInterfaces[i];
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( (NULL == pRadioHWInfo) || (! pRadioHWInfo->openedForRead) )
            continue;
         if ( s_RadioRxState.iRadioInterfacesBroken[i] )
            continue;
         if ( s_iRadioRxPausedInterfaces[i] )
            continue;
         fds[s_iRadioRxCountFDs].fd = pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd;
         fds[s_iRadioRxCountFDs].revents = 0;
         fds[s_iRadioRxCountFDs].events = POLLIN;
         s_iRadioRxCountFDs++;
      }

      if ( s_iRadioRxCountFDs <= 0 )
      {
         hardware_sleep_ms(5);
         s_uRadioRxTimeNow = get_current_timestamp_ms();
         continue;
      }

      //s_bCanDoExternalOperations = 1;
      //int nResult = select(s_iRadioRxMaxFD, &s_RadioRxReadSet, NULL, NULL, &s_iRadioRxReadTimeInterval);
      int nResult = poll(fds, s_iRadioRxCountFDs, iPollTimeoutMs);
      //s_bCanDoExternalOperations = 0;
      s_uRadioRxTimeNow = get_current_timestamp_ms();
      s_uRadioRxLastTimeQueue = 0;

      if ( nResult < 0 )
      {
         log_line("[RadioRxThread] Radio interfaces have broken up. Exception on select read radio handle.");
         for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
         {
            radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
            if ( (NULL != pRadioHWInfo) && pRadioHWInfo->openedForRead )
               s_RadioRxState.iRadioInterfacesBroken[i] = 1;
         }
      }

      if ( nResult <= 0 )
      {
         //hardware_sleep_micros(200);
         continue;
      }

      // Received data, process it
      int iMaxReadsPerTry = 5;
      int iMaxRepeatCount = 5;
      int iMaxedInterface = -1;

      int iParsedPackets[MAX_RADIO_INTERFACES];
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         iParsedPackets[i] = iMaxReadsPerTry;

      int iInterfacesCount = hardware_get_radio_interfaces_count();
      if ( iInterfacesCount > MAX_RADIO_INTERFACES )
         iInterfacesCount = MAX_RADIO_INTERFACES;

      if ( s_iRadioRxResetSignalInfo )
      {
         s_iRadioRxResetSignalInfo = 0;
         for( int i=0; i<iInterfacesCount; i++ )
         {
            radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
            if ( (NULL == pRadioHWInfo) || (! pRadioHWInfo->openedForRead) )
               continue;
            reset_runtime_radio_rx_signal_info(&(pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoAll));
            reset_runtime_radio_rx_signal_info(&(pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoVideo));
            reset_runtime_radio_rx_signal_info(&(pRadioHWInfo->runtimeInterfaceInfoRx.radioHwRxInfo.signalInfoData));
         }
      }

      // Repeat reading while we have max reads on at least one interface
      do
      {
         iMaxRepeatCount--;
         iMaxedInterface = -1;

         for(int iPollIndex=0; iPollIndex < s_iRadioRxCountFDs; iPollIndex++)
         {
            if ( 0 == (fds[iPollIndex].revents & POLLIN) )
               continue;

            int iInterfaceIndex = -1;
            for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
            {
               radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
               if ( (NULL == pRadioHWInfo) || (! pRadioHWInfo->openedForRead) )
                  continue;
               if ( s_RadioRxState.iRadioInterfacesBroken[i] )
                  continue;
               if ( iRadioInterfacesWherePaused[i] )
                  continue;
               if ( fds[iPollIndex].fd == pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd )
               {
                  iInterfaceIndex = i;
                  break;
               }
            }

            if ( (iInterfaceIndex == -1) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
               continue;

            radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
            if ( (NULL == pRadioHWInfo) || (! pRadioHWInfo->openedForRead) )
               continue;
            if ( s_RadioRxState.iRadioInterfacesBroken[iInterfaceIndex] )
               continue;
            if ( iRadioInterfacesWherePaused[iInterfaceIndex] )
               continue;
            //if( 0 == FD_ISSET(pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd, &s_RadioRxReadSet) )
            //   continue;

            if ( iParsedPackets[iInterfaceIndex] < iMaxReadsPerTry )
               continue;

            if ( hardware_radio_index_is_serial_radio(iInterfaceIndex) )
            {
               iParsedPackets[iInterfaceIndex] = _radio_rx_parse_received_serial_radio_data(iInterfaceIndex);
               if ( iParsedPackets[iInterfaceIndex] < 0 )
               {
                  log_line("[RadioRx] Mark serial radio interface %d as broken", iInterfaceIndex+1);
                  s_RadioRxState.iRadioInterfacesBroken[iInterfaceIndex] = 1;
               }
            }
            else
            {
               iParsedPackets[iInterfaceIndex] = _radio_rx_parse_received_wifi_radio_data(iInterfaceIndex, iMaxReadsPerTry);
               if ( (iParsedPackets[iInterfaceIndex] < 0) || ( radio_get_last_read_error_code() == RADIO_READ_ERROR_INTERFACE_BROKEN ) )
               {
                  log_line("[RadioRx] Mark radio interface %d as broken", iInterfaceIndex+1);
                  s_RadioRxState.iRadioInterfacesBroken[iInterfaceIndex] = 1;
                  continue;
               }
               iLoopParsedPackets += iParsedPackets[iInterfaceIndex];
               if ( iParsedPackets[iInterfaceIndex] >= iMaxReadsPerTry )
                  iMaxedInterface = iInterfaceIndex;
            }
         }
      } while ( (iMaxedInterface != -1) && (iMaxRepeatCount > 0));

      s_uRadioRxTimeNow = get_current_timestamp_ms();
   }

   s_iRadioRxMarkedForQuit = 0;
   log_line("[RadioRxThread] Stopped.");
   s_iRadioRxThreadRunning = 0;
   return NULL;
}

int _radio_rx_init_queues()
{
   s_RadioRxState.queue_reg_priority.iQueueSize = MAX_RX_PACKETS_QUEUE_REG;
   s_RadioRxState.queue_high_priority.iQueueSize = MAX_RX_PACKETS_QUEUE_HIP;

   for( int i=0; i<s_RadioRxState.queue_reg_priority.iQueueSize; i++ )
   {
      s_RadioRxState.queue_reg_priority.iPacketsLengths[i] = 0;
      s_RadioRxState.queue_reg_priority.uPacketsAreShort[i] = 0;
      s_RadioRxState.queue_reg_priority.uPacketsRxInterface[i] = 0;
      s_RadioRxState.queue_reg_priority.pPacketsBuffers[i] = (u8*) malloc(MAX_PACKET_TOTAL_SIZE);
      if ( NULL == s_RadioRxState.queue_reg_priority.pPacketsBuffers[i] )
      {
         log_error_and_alarm("[RadioRx] Failed to allocate rx packets buffers!");
         return -1;
      }
   }
   log_line("[RadioRx] Allocated %u bytes for %d rx packets (reg priority)", s_RadioRxState.queue_reg_priority.iQueueSize * MAX_PACKET_TOTAL_SIZE, s_RadioRxState.queue_reg_priority.iQueueSize);

   for( int i=0; i<s_RadioRxState.queue_high_priority.iQueueSize; i++ )
   {
      s_RadioRxState.queue_high_priority.iPacketsLengths[i] = 0;
      s_RadioRxState.queue_high_priority.uPacketsAreShort[i] = 0;
      s_RadioRxState.queue_high_priority.uPacketsRxInterface[i] = 0;
      s_RadioRxState.queue_high_priority.pPacketsBuffers[i] = (u8*) malloc(MAX_PACKET_TOTAL_SIZE);
      if ( NULL == s_RadioRxState.queue_high_priority.pPacketsBuffers[i] )
      {
         log_error_and_alarm("[RadioRx] Failed to allocate rx packets buffers!");
         return 0;
      }
   }
   log_line("[RadioRx] Allocated %u bytes for %d rx packets (high priority)", s_RadioRxState.queue_high_priority.iQueueSize * MAX_PACKET_TOTAL_SIZE, s_RadioRxState.queue_high_priority.iQueueSize);

   s_RadioRxState.queue_high_priority.iCurrentPacketIndexToConsume = 0;
   s_RadioRxState.queue_high_priority.iCurrentPacketIndexToWrite = 0;
   s_RadioRxState.queue_reg_priority.iCurrentPacketIndexToConsume = 0;
   s_RadioRxState.queue_reg_priority.iCurrentPacketIndexToWrite = 0;
   
   s_RadioRxState.queue_high_priority.iStatsMaxPacketsInQueue = 0;
   s_RadioRxState.queue_high_priority.iStatsMaxPacketsInQueueLastMinute = 0;
   s_RadioRxState.queue_reg_priority.iStatsMaxPacketsInQueue = 0;
   s_RadioRxState.queue_reg_priority.iStatsMaxPacketsInQueueLastMinute = 0;

   sem_unlink(RUBY_SEM_RX_RADIO_HIGH_PRIORITY);
   sem_unlink(RUBY_SEM_RX_RADIO_REG_PRIORITY);

   pthread_mutexattr_t mutexAttr;
   if ( 0 != pthread_mutexattr_init(&mutexAttr) )
   {
      log_error_and_alarm("[RadioRx] Failed to initialize rx queues mutex attributes.");
      return -1;
   }
   if ( 0 != pthread_mutex_init(&s_RadioRxState.queue_high_priority.mutexLock, &mutexAttr) )
   {
      log_error_and_alarm("[RadioRx] Failed to initialize high prio rx queue mutex.");
      return -1;
   }
   if ( 0 != pthread_mutex_init(&s_RadioRxState.queue_reg_priority.mutexLock, &mutexAttr) )
   {
      log_error_and_alarm("[RadioRx] Failed to initialize reg prio rx queue mutex.");
      return -1;
   }

   if ( 0 != pthread_mutexattr_destroy(&mutexAttr) )
      log_error_and_alarm("[RadioRx] Failed to cleanup radio rx queues mutex attributes");

   s_RadioRxState.queue_high_priority.pSemaphoreWrite = sem_open(RUBY_SEM_RX_RADIO_HIGH_PRIORITY, O_CREAT | O_RDWR, S_IWUSR | S_IRUSR, 0);
   if ( (NULL == s_RadioRxState.queue_high_priority.pSemaphoreWrite) || (SEM_FAILED == s_RadioRxState.queue_high_priority.pSemaphoreWrite) )
   {
      log_error_and_alarm("[RadioRx] Failed to create write semaphore: %s, try alternative.", RUBY_SEM_RX_RADIO_HIGH_PRIORITY);
      s_RadioRxState.queue_high_priority.pSemaphoreWrite = sem_open(RUBY_SEM_RX_RADIO_HIGH_PRIORITY, O_CREAT, S_IWUSR | S_IRUSR, 0); 
      if ( (NULL == s_RadioRxState.queue_high_priority.pSemaphoreWrite) || (SEM_FAILED == s_RadioRxState.queue_high_priority.pSemaphoreWrite) )
      {
         log_error_and_alarm("[RadioRx] Failed to create write semaphore: %s", RUBY_SEM_RX_RADIO_HIGH_PRIORITY);
         return -1;
      }
   }
   s_RadioRxState.queue_high_priority.pSemaphoreRead = sem_open(RUBY_SEM_RX_RADIO_HIGH_PRIORITY, O_RDWR);
   if ( (NULL == s_RadioRxState.queue_high_priority.pSemaphoreRead) || (SEM_FAILED == s_RadioRxState.queue_high_priority.pSemaphoreRead) )
   {
      log_error_and_alarm("[RadioRx] Failed to create read semaphore: %s, try alternative.", RUBY_SEM_RX_RADIO_HIGH_PRIORITY);
      s_RadioRxState.queue_high_priority.pSemaphoreRead = sem_open(RUBY_SEM_RX_RADIO_HIGH_PRIORITY, O_CREAT, S_IWUSR | S_IRUSR, 0); 
      if ( (NULL == s_RadioRxState.queue_high_priority.pSemaphoreRead) || (SEM_FAILED == s_RadioRxState.queue_high_priority.pSemaphoreRead) )
      {
         log_error_and_alarm("[RadioRx] Failed to create read semaphore: %s", RUBY_SEM_RX_RADIO_HIGH_PRIORITY);
         return -1;
      }
   }
   int iSemVal = 0;
   if ( 0 == sem_getvalue(s_RadioRxState.queue_high_priority.pSemaphoreRead, &iSemVal) )
      log_line("[RadioRx] High priority queue semaphore initial value: %d", iSemVal);
   else
      log_softerror_and_alarm("[RadioRx] Failed to get high priority queue sem value.");

   s_RadioRxState.queue_reg_priority.pSemaphoreWrite = sem_open(RUBY_SEM_RX_RADIO_REG_PRIORITY, O_CREAT | O_RDWR, S_IWUSR | S_IRUSR, 0);
   if ( (NULL == s_RadioRxState.queue_reg_priority.pSemaphoreWrite) || (SEM_FAILED == s_RadioRxState.queue_reg_priority.pSemaphoreWrite) )
   {
      log_error_and_alarm("[RadioRx] Failed to create write semaphore: %s, try alternative.", RUBY_SEM_RX_RADIO_REG_PRIORITY);
      s_RadioRxState.queue_reg_priority.pSemaphoreWrite = sem_open(RUBY_SEM_RX_RADIO_REG_PRIORITY, O_CREAT, S_IWUSR | S_IRUSR, 0); 
      if ( (NULL == s_RadioRxState.queue_reg_priority.pSemaphoreWrite) || (SEM_FAILED == s_RadioRxState.queue_reg_priority.pSemaphoreWrite) )
      {
         log_error_and_alarm("[RadioRx] Failed to create write semaphore: %s", RUBY_SEM_RX_RADIO_REG_PRIORITY);
         return -1;
      }
   }
   s_RadioRxState.queue_reg_priority.pSemaphoreRead = sem_open(RUBY_SEM_RX_RADIO_REG_PRIORITY, O_RDWR);
   if ( (NULL == s_RadioRxState.queue_reg_priority.pSemaphoreRead) || (SEM_FAILED == s_RadioRxState.queue_reg_priority.pSemaphoreRead) )
   {
      log_error_and_alarm("[RadioRx] Failed to create read semaphore: %s, try alternative", RUBY_SEM_RX_RADIO_REG_PRIORITY);
      s_RadioRxState.queue_reg_priority.pSemaphoreRead = sem_open(RUBY_SEM_RX_RADIO_REG_PRIORITY, O_CREAT, S_IWUSR | S_IRUSR, 0); 
      if ( (NULL == s_RadioRxState.queue_reg_priority.pSemaphoreRead) || (SEM_FAILED == s_RadioRxState.queue_reg_priority.pSemaphoreRead) )
      {
         log_error_and_alarm("[RadioRx] Failed to create read semaphore: %s", RUBY_SEM_RX_RADIO_REG_PRIORITY);
         return -1;
      }
   }

   iSemVal = 0;
   if ( 0 == sem_getvalue(s_RadioRxState.queue_reg_priority.pSemaphoreRead, &iSemVal) )
      log_line("[RadioRx] Reg priority queue semaphore initial value: %d", iSemVal);
   else
      log_softerror_and_alarm("[RadioRx] Failed to get reg priority queue sem value.");

   return 0;
}

int radio_rx_start_rx_thread(shared_mem_radio_stats* pSMRadioStats, int iSearchMode, u32 uAcceptedFirmwareType)
{
   if ( s_iRadioRxInitialized )
      return 1;

   log_line("[RadioRx] Initializing data and starting rx thread...");
   s_pSMRadioStats = pSMRadioStats;
   s_iSearchMode = iSearchMode;
   s_iRadioRxSignalStop = 0;
   s_iRadioRxMarkedForQuit = 0;
   s_RadioRxState.uAcceptedFirmwareType = uAcceptedFirmwareType;
   radio_rx_reset_interfaces_broken_state();

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      s_iRadioRxPausedInterfaces[i] = 0;

   s_iRadioRxAllInterfacesPaused = 0;

   if ( _radio_rx_init_queues() < 0 )
   {
      log_error_and_alarm("[RadioRx] Failed to initialize packets queues.");
      return 0;
   }

   s_RadioRxState.uTimeLastStatsUpdate = get_current_timestamp_ms();
   s_RadioRxState.uTimeLastMinuteStatsUpdate = get_current_timestamp_ms();
   
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      s_RadioRxState.vehicles[i].uVehicleId = 0;
      s_RadioRxState.vehicles[i].uDetectedFirmwareType = s_RadioRxState.uAcceptedFirmwareType;
      s_RadioRxState.vehicles[i].uTotalRxPackets = 0;
      s_RadioRxState.vehicles[i].uTotalRxPacketsBad = 0;
      s_RadioRxState.vehicles[i].uTotalRxPacketsLost = 0;
      s_RadioRxState.vehicles[i].uTmpRxPackets = 0;
      s_RadioRxState.vehicles[i].uTmpRxPacketsBad = 0;
      s_RadioRxState.vehicles[i].uTmpRxPacketsLost = 0;
      s_RadioRxState.vehicles[i].iMaxRxPacketsPerSec = 0;
      s_RadioRxState.vehicles[i].iMinRxPacketsPerSec = 1000000;

      for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
         s_RadioRxState.vehicles[i].uLastRxRadioLinkPacketIndex[k] = 0;
   }

   s_RadioRxState.uMaxLoopTime = 0;

   log_line("[RadioRx] Initializing thread: cpu affinity: %d, pending/current raw priority: %d", s_iRxCPUAffinityCorePending, s_iPendingRxThreadRawPriority, s_iCurrentRxThreadRawPriority);
   s_iRxCPUAffinityCore = s_iRxCPUAffinityCorePending;
   pthread_attr_t attr;
   hw_init_worker_thread_attrs(&attr, s_iRxCPUAffinityCorePending, 128000, SCHED_FIFO, s_iPendingRxThreadRawPriority, "radio_rx");
   if ( s_iPendingRxThreadRawPriority != s_iCurrentRxThreadRawPriority )
      s_iCurrentRxThreadRawPriority = s_iPendingRxThreadRawPriority;

   if ( 0 != pthread_create(&s_pThreadRadioRx, &attr, &_thread_radio_rx, (void*)&s_iRadioRxSignalStop) )
   {
      log_error_and_alarm("[RadioRx] Failed to create thread for radio rx.");
      pthread_attr_destroy(&attr);
      return 0;
   }
   pthread_attr_destroy(&attr);
   s_iRadioRxInitialized = 1;
   log_line("[RadioRx] Initialized data and started rx thread, accepted firmware types: %s.", str_format_firmware_type(s_RadioRxState.uAcceptedFirmwareType));
   return 1;
}

void radio_rx_stop_rx_thread()
{
   if ( ! s_iRadioRxInitialized )
      return;

   log_line("[RadioRx] Signaled radio rx thread to stop.");

   radio_rx_mark_quit();
   s_iRadioRxSignalStop = 1;
   s_iRadioRxInitialized = 0;

   int iCount = 0;
   while ( s_iRadioRxThreadRunning && (iCount++ < 100) )
      hardware_sleep_ms(5);

   if ( 0 == s_iRadioRxThreadRunning )
      log_line("[RadioRx] Rx thread has finished.");
   else
   {
      log_softerror_and_alarm("[RadioRx] Rx thread failed to finish. Cancel it.");
      pthread_cancel(s_pThreadRadioRx);
   }

   if ( NULL != s_RadioRxState.queue_high_priority.pSemaphoreWrite )
      sem_close(s_RadioRxState.queue_high_priority.pSemaphoreWrite);
   if ( NULL != s_RadioRxState.queue_high_priority.pSemaphoreRead )
      sem_close(s_RadioRxState.queue_high_priority.pSemaphoreRead);
   if ( NULL != s_RadioRxState.queue_reg_priority.pSemaphoreWrite )
      sem_close(s_RadioRxState.queue_reg_priority.pSemaphoreWrite);
   if ( NULL != s_RadioRxState.queue_reg_priority.pSemaphoreRead )
      sem_close(s_RadioRxState.queue_reg_priority.pSemaphoreRead);
   s_RadioRxState.queue_high_priority.pSemaphoreWrite = NULL;
   s_RadioRxState.queue_high_priority.pSemaphoreRead = NULL;
   s_RadioRxState.queue_reg_priority.pSemaphoreWrite = NULL;
   s_RadioRxState.queue_reg_priority.pSemaphoreRead = NULL;

   pthread_mutex_destroy(&s_RadioRxState.queue_high_priority.mutexLock);
   pthread_mutex_destroy(&s_RadioRxState.queue_reg_priority.mutexLock);

   sem_unlink(RUBY_SEM_RX_RADIO_HIGH_PRIORITY);
   sem_unlink(RUBY_SEM_RX_RADIO_REG_PRIORITY);

   log_line("[RadioRx] Finished stopping rx thread.");
}

void radio_rx_set_cpu_affinity(int iCPUCore)
{
   s_iRxCPUAffinityCorePending = iCPUCore;
}

void radio_rx_set_custom_thread_raw_priority(int iRawPriority)
{
   s_iPendingRxThreadRawPriority = iRawPriority;
}

void radio_rx_set_timeout_interval(int iMiliSec)
{
   s_iRadioRxLoopTimeoutInterval = iMiliSec;
   log_line("[RadioRx] Set loop check timeout interval to %d ms.", s_iRadioRxLoopTimeoutInterval);
}

void _radio_rx_check_update_all_paused_flag()
{
   s_iRadioRxAllInterfacesPaused = 1;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( 0 == s_iRadioRxPausedInterfaces[i] )
      {
         s_iRadioRxAllInterfacesPaused = 0;
         break;
      }
   }
}

void radio_rx_pause_interface(int iInterfaceIndex, const char* szReason)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return;

   if ( s_iRadioRxInitialized )
   {
      s_bHasPendingExternalOperation = 1;
      while ( ! s_bCanDoExternalOperations )
         hardware_sleep_ms(1);

      s_iRadioRxPausedInterfaces[iInterfaceIndex]++;
      _radio_rx_check_update_all_paused_flag();

      s_bHasPendingExternalOperation = 0;
      s_bCanDoExternalOperations = 0;
   }
   else
   {
      s_iRadioRxPausedInterfaces[iInterfaceIndex] = 1;
      _radio_rx_check_update_all_paused_flag();
   }

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);

   char szRadioName[64];
   strcpy(szRadioName, "N/A");
   if ( NULL != pRadioHWInfo )
      strncpy(szRadioName, pRadioHWInfo->szName, sizeof(szRadioName)/sizeof(szRadioName[0]));

   char szBuff[128];
   szBuff[0] = 0;
   if ( NULL != szReason )
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), " (reason: %s)", szReason);

   log_line("[RadioRx] Pause Rx on radio interface %d, [%s] (paused %d times)%s", iInterfaceIndex+1, szRadioName, s_iRadioRxPausedInterfaces[iInterfaceIndex], szBuff);
}

void radio_rx_resume_interface(int iInterfaceIndex)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return;

   if ( s_iRadioRxInitialized )
   {
      s_bHasPendingExternalOperation = 1;
      while ( ! s_bCanDoExternalOperations )
         hardware_sleep_ms(1);

      if ( s_iRadioRxPausedInterfaces[iInterfaceIndex] > 0 )
      {
         s_iRadioRxPausedInterfaces[iInterfaceIndex]--;
         if ( s_iRadioRxPausedInterfaces[iInterfaceIndex] == 0 )
            s_iRadioRxAllInterfacesPaused = 0;
      }
      s_bHasPendingExternalOperation = 0;
      s_bCanDoExternalOperations = 0;
   }
   else
   {
      s_iRadioRxPausedInterfaces[iInterfaceIndex] = 0;
      s_iRadioRxAllInterfacesPaused = 0;
   }

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
   if ( 0 == s_iRadioRxPausedInterfaces[iInterfaceIndex] )
   {
      if ( NULL != pRadioHWInfo )
         log_line("[RadioRx] Resumed Rx on radio interface %d, [%s]", iInterfaceIndex+1, pRadioHWInfo->szName);
      else
         log_line("[RadioRx] Resumed Rx on radio interface %d, [%s]", iInterfaceIndex+1, "N/A");
   }
   else
   {
      if ( NULL != pRadioHWInfo )
         log_line("[RadioRx] Tried resume Rx on radio interface %d, [%s], still paused %d times.", iInterfaceIndex+1, pRadioHWInfo->szName, s_iRadioRxPausedInterfaces[iInterfaceIndex]);
      else
         log_line("[RadioRx] Tried resume Rx on radio interface %d, [%s], still paused %d times", iInterfaceIndex+1, "N/A", s_iRadioRxPausedInterfaces[iInterfaceIndex]);    
   }
}

void radio_rx_mark_quit()
{
   s_iRadioRxMarkedForQuit = 1;
}

void radio_rx_reset_signal_info()
{
   s_iRadioRxResetSignalInfo = 1;
}

void radio_rx_set_dev_mode(int iDevMode)
{
   s_iRadioRxDevMode = iDevMode;
   log_line("[RadioRx] Set dev mode: %d", iDevMode);
}

// Pointers to array of int-s (max radio cards, for each card)
void radio_rx_set_packet_counter_output(u8* pCounterOutputHighPriority, u8* pCounterOutputData, u8* pCounterMissingPackets, u8* pCounterMissingPacketsMaxGap)
{
   s_pPacketsCounterOutputHighPriority = pCounterOutputHighPriority;
   s_pPacketsCounterOutputData = pCounterOutputData;
   s_pPacketsCounterOutputMissing = pCounterMissingPackets;
   s_pPacketsCounterOutputMissingMaxGap = pCounterMissingPacketsMaxGap;
}

void radio_rx_set_air_gap_track_output(u8* pCounterRxAirgap)
{
   s_pRxAirGapTracking = pCounterRxAirgap;
}

int radio_rx_detect_firmware_type_from_packet(u8* pPacketBuffer, int nPacketLength)
{
   if ( (NULL == pPacketBuffer) || (nPacketLength < 4) )
      return 0;

   if ( nPacketLength >= (int)sizeof(t_packet_header) )
   {
      t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
      if ( pPH->total_length > nPacketLength )
      {
         if ( pPH->packet_flags & PACKET_FLAGS_BIT_HAS_ENCRYPTION )
         {
            int dx = sizeof(t_packet_header);
            int l = nPacketLength-dx;
            dpp(pPacketBuffer + dx, l);
         }
         u32 uCRC = 0;
         if ( pPH->packet_flags & PACKET_FLAGS_BIT_HEADERS_ONLY_CRC )
            uCRC = base_compute_crc32(pPacketBuffer+sizeof(u32), sizeof(t_packet_header)-sizeof(u32));
         else
            uCRC = base_compute_crc32(pPacketBuffer+sizeof(u32), pPH->total_length-sizeof(u32));

         if ( (uCRC & 0x00FFFFFF) == (pPH->uCRC & 0x00FFFFFF) )
            return MODEL_FIRMWARE_TYPE_RUBY;
      }
   }

   return 0;
}

int radio_rx_any_interface_broken()
{
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      if ( s_RadioRxState.iRadioInterfacesBroken[i] )
          return i+1;

   return 0;
}

int radio_rx_any_interface_bad_packets()
{
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      if ( s_RadioRxState.iRadioInterfacesRxBadPackets[i] )
          return i+1;

   return 0;
}

int radio_rx_get_interface_bad_packets_error_and_reset(int iInterfaceIndex)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return 0;
   int iError = s_RadioRxState.iRadioInterfacesRxBadPackets[iInterfaceIndex];
   s_RadioRxState.iRadioInterfacesRxBadPackets[iInterfaceIndex] = 0;
   return iError;
}

int radio_rx_any_rx_timeouts()
{
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      if ( s_RadioRxState.iRadioInterfacesRxTimeouts[i] > 0 )
          return i+1;
   return 0;
}

int radio_rx_get_timeout_count_and_reset(int iInterfaceIndex)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return 0;
   int iCount = s_RadioRxState.iRadioInterfacesRxTimeouts[iInterfaceIndex];
   s_RadioRxState.iRadioInterfacesRxTimeouts[iInterfaceIndex] = 0;
   return iCount;
}

void radio_rx_reset_interfaces_broken_state()
{
   log_line("[RadioRx] Reset broken state for all interface. Mark all interfaces as not broken.");
   
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      s_RadioRxState.iRadioInterfacesBroken[i] = 0;
      s_RadioRxState.iRadioInterfacesRxTimeouts[i] = 0;
      s_RadioRxState.iRadioInterfacesRxBadPackets[i] = 0;
   }
}

u32 radio_rx_get_and_reset_max_loop_time()
{
   u32 u = s_RadioRxState.uMaxLoopTime;
   s_RadioRxState.uMaxLoopTime = 0;
   return u;
}

u32 radio_rx_get_and_reset_max_loop_time_read()
{
   u32 u = s_uRadioRxMaxTimeRead;
   s_uRadioRxMaxTimeRead = 0;
   return u;
}

u32 radio_rx_get_and_reset_max_loop_time_queue()
{
   u32 u = s_uRadioRxLastTimeQueue;
   s_uRadioRxLastTimeQueue = 0;
   return u;
}
