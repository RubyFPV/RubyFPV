/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga
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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "base.h"
#include "shared_mem.h"
#include "controller_rt_info.h"
#include "hardware_radio.h"



controller_debug_video_runtime_info* controller_debug_video_rt_info_open_for_read()
{
   void *retVal = open_shared_mem_for_read(SHARED_MEM_CONTROLLER_DEBUG_VIDEO_RUNTIME_INFO, sizeof(controller_debug_video_runtime_info));
   return (controller_debug_video_runtime_info*)retVal;
}

controller_debug_video_runtime_info* controller_debug_video_rt_info_open_for_write()
{
   void *retVal = open_shared_mem_for_write(SHARED_MEM_CONTROLLER_DEBUG_VIDEO_RUNTIME_INFO, sizeof(controller_debug_video_runtime_info));
   controller_debug_video_runtime_info* pRTInfo = (controller_debug_video_runtime_info*)retVal;
   controller_debug_video_rt_info_init(pRTInfo);
   return pRTInfo;
}

void controller_debug_video_rt_info_close(controller_debug_video_runtime_info* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(controller_debug_video_runtime_info));
   //shm_unlink(szName);
}


void controller_debug_video_rt_info_init(controller_debug_video_runtime_info* pCVideoRTInfo)
{
   if ( NULL == pCVideoRTInfo )
      return;

   log_line("controller_debug_video_runtime_info total size: %d", sizeof(controller_debug_video_runtime_info));
   memset(pCVideoRTInfo, 0, sizeof(controller_debug_video_runtime_info));

   pCVideoRTInfo->iCurrentFrameBufferIndex = 0;
   pCVideoRTInfo->uCurrentReceivedH264Frame = 0xFFFF;
   pCVideoRTInfo->uPreviousReceivedH264Frame = 0xFFFF;
   pCVideoRTInfo->uLastComputedFPSAtFrameId = 0xFFFF;
   pCVideoRTInfo->uLastComputedFPSAtTime = MAX_U32;
}

void controller_debug_video_rt_info_advance_frame(controller_debug_video_runtime_info* pCVideoRTInfo, u16 uNewCurrentFrameH264Index, int iCurrentFPS, u32 uTimeNowMs)
{
   if ( (NULL == pCVideoRTInfo) || (uNewCurrentFrameH264Index == pCVideoRTInfo->uCurrentReceivedH264Frame) )
      return;

   int iExpectedFPS = iCurrentFPS;
   if ( iExpectedFPS <= 10 )
      iExpectedFPS = 30;

   // Reset?
   bool bReset = false;
   if ( (pCVideoRTInfo->uCurrentReceivedH264Frame == 0xFFFF) && (pCVideoRTInfo->uPreviousReceivedH264Frame == 0xFFFF) )
      bReset = true;
   if ( ! bReset )
   if ( uNewCurrentFrameH264Index > pCVideoRTInfo->uCurrentReceivedH264Frame )
   if ( (uNewCurrentFrameH264Index - pCVideoRTInfo->uCurrentReceivedH264Frame) > iExpectedFPS/2 )
      bReset = true;
   if ( ! bReset )
   if ( uNewCurrentFrameH264Index < pCVideoRTInfo->uCurrentReceivedH264Frame )
   if ( (uNewCurrentFrameH264Index > 2) || (pCVideoRTInfo->uCurrentReceivedH264Frame < (0xFFFF-2)) )
      bReset = true;

   if ( bReset )
   {
      controller_debug_video_rt_info_init(pCVideoRTInfo);
      pCVideoRTInfo->uCurrentReceivedH264Frame = uNewCurrentFrameH264Index;
      pCVideoRTInfo->uReceivedFrameStartTime[pCVideoRTInfo->iCurrentFrameBufferIndex] = uTimeNowMs;
      return;
   }

   // Update current frame stats
   pCVideoRTInfo->uReceivedFrameTotalSizeBytes[pCVideoRTInfo->iCurrentFrameBufferIndex] = pCVideoRTInfo->iCurrentFrameRecvBytes;
   pCVideoRTInfo->iCurrentFrameRecvBytes = 0;

   u32 uDelta = pCVideoRTInfo->uCurrentFrameLastPacketTimeTensMS - pCVideoRTInfo->uCurrentFrameFirstPacketTimeTensMS;
   if ( uDelta > 255 ) uDelta = 255;
      pCVideoRTInfo->uReceivedFrameDurationTensMs[pCVideoRTInfo->iCurrentFrameBufferIndex] = uDelta;

   pCVideoRTInfo->uCurrentFrameFirstPacketTimeTensMS = pCVideoRTInfo->uCurrentFrameLastPacketTimeTensMS = get_current_timestamp_ms_tens();

   int iIndex = pCVideoRTInfo->iCurrentFrameBufferIndex;
   u32 uTotalBytes = 0;
   u32 uTotalTensMs = 0;
   for( int i=0; i<10; i++ )
   {
      uTotalBytes +=  pCVideoRTInfo->uReceivedFrameTotalSizeBytes[iIndex];
      uTotalTensMs += pCVideoRTInfo->uReceivedFrameDurationTensMs[iIndex];
      iIndex--;
      if ( iIndex < 0 )
         iIndex = SYSTEM_RT_INFO_INTERVALS_FRAMES-1;
   }
   if ( uTotalTensMs > 0 )
   {
      u32 uThroughput = (8*10*1000*uTotalBytes)/uTotalTensMs;
      if ( uThroughput < 100000000 )
         pCVideoRTInfo->uReceivedFrameThroughputBPS[pCVideoRTInfo->iCurrentFrameBufferIndex] = uThroughput;
   }

   // Advance to new frame

   int iCountFrames = 0;
   while ( pCVideoRTInfo->uCurrentReceivedH264Frame != uNewCurrentFrameH264Index )
   {
      iCountFrames++;
      pCVideoRTInfo->uPreviousReceivedH264Frame = pCVideoRTInfo->uCurrentReceivedH264Frame;
      pCVideoRTInfo->uCurrentReceivedH264Frame++;
      pCVideoRTInfo->iCurrentFrameBufferIndex = (pCVideoRTInfo->iCurrentFrameBufferIndex+1) % SYSTEM_RT_INFO_INTERVALS_FRAMES;

      pCVideoRTInfo->uOutputFramePackets[pCVideoRTInfo->iCurrentFrameBufferIndex] = 0;
      pCVideoRTInfo->uOutputFramesInfo[pCVideoRTInfo->iCurrentFrameBufferIndex] = 0;
      pCVideoRTInfo->uOutputedFramesSizes[pCVideoRTInfo->iCurrentFrameBufferIndex] = 0;
      pCVideoRTInfo->uVideoFramesProcessingTimes[pCVideoRTInfo->iCurrentFrameBufferIndex] = 0;
      pCVideoRTInfo->uReceivedFrameTotalSizeBytes[pCVideoRTInfo->iCurrentFrameBufferIndex] = 0;
      pCVideoRTInfo->uReceivedFrameThroughputBPS[pCVideoRTInfo->iCurrentFrameBufferIndex] = 0;
      pCVideoRTInfo->uReceivedFrameStartTime[pCVideoRTInfo->iCurrentFrameBufferIndex] = 0;
      pCVideoRTInfo->uReceivedFrameDurationTensMs[pCVideoRTInfo->iCurrentFrameBufferIndex] = 0;

      if ( iCountFrames > iExpectedFPS )
      {
         controller_debug_video_rt_info_init(pCVideoRTInfo);
         pCVideoRTInfo->uCurrentReceivedH264Frame = uNewCurrentFrameH264Index;
         pCVideoRTInfo->uReceivedFrameStartTime[pCVideoRTInfo->iCurrentFrameBufferIndex] = uTimeNowMs;
         return;
      }
   }

   pCVideoRTInfo->uReceivedFrameStartTime[pCVideoRTInfo->iCurrentFrameBufferIndex] = uTimeNowMs;

   if ( (pCVideoRTInfo->uLastComputedFPSAtFrameId == 0xFFFF) || (pCVideoRTInfo->uLastComputedFPSAtTime == MAX_U32) )
   {
      pCVideoRTInfo->uLastComputedFPSAtFrameId = pCVideoRTInfo->uCurrentReceivedH264Frame;
      pCVideoRTInfo->uLastComputedFPSAtTime = uTimeNowMs;
   }
   else if ( uTimeNowMs >= pCVideoRTInfo->uLastComputedFPSAtTime + 500 )
   {
      int iCountFrames = ((int)pCVideoRTInfo->uCurrentReceivedH264Frame) - ((int)pCVideoRTInfo->uLastComputedFPSAtFrameId);
      if ( pCVideoRTInfo->uCurrentReceivedH264Frame < pCVideoRTInfo->uLastComputedFPSAtFrameId )
         iCountFrames = ((int)pCVideoRTInfo->uCurrentReceivedH264Frame) + ((int)0xFFFF - ((int)pCVideoRTInfo->uLastComputedFPSAtFrameId));
      pCVideoRTInfo->iComputedRxVideoFPS = (iCountFrames * 1000) / (uTimeNowMs - pCVideoRTInfo->uLastComputedFPSAtTime);
      pCVideoRTInfo->uLastComputedFPSAtFrameId = pCVideoRTInfo->uCurrentReceivedH264Frame;
      pCVideoRTInfo->uLastComputedFPSAtTime = uTimeNowMs;
   }
}


controller_runtime_info* controller_rt_info_open_for_read()
{
   void *retVal = open_shared_mem_for_read(SHARED_MEM_CONTROLLER_RUNTIME_INFO, sizeof(controller_runtime_info));
   return (controller_runtime_info*)retVal;
}

controller_runtime_info* controller_rt_info_open_for_write()
{
   void *retVal = open_shared_mem_for_write(SHARED_MEM_CONTROLLER_RUNTIME_INFO, sizeof(controller_runtime_info));
   controller_runtime_info* pRTInfo = (controller_runtime_info*)retVal;
   controller_rt_info_init(pRTInfo);
   return pRTInfo;
}

void controller_rt_info_close(controller_runtime_info* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(controller_runtime_info));
   //shm_unlink(szName);
}

void _controller_runtime_info_reset_slice_signal_info(controller_runtime_info* pCRTInfo, int iSliceIndex)
{
   if ( NULL == pCRTInfo )
      return;
   for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
   {
      reset_runtime_radio_rx_signal_info(&(pCRTInfo->radioInterfacesSignalInfoVideo[iSliceIndex][k]));
      reset_runtime_radio_rx_signal_info(&(pCRTInfo->radioInterfacesSignalInfoData[iSliceIndex][k]));
   }
}

void controller_rt_info_init(controller_runtime_info* pCRTInfo)
{
   if ( NULL == pCRTInfo )
      return;

   log_line("controller_runtime_info total size: %d", sizeof(controller_runtime_info));
   log_line("controller_runtime_info dbm size: %d", sizeof(pCRTInfo->radioInterfacesSignalInfoVideo));
   memset(pCRTInfo, 0, sizeof(controller_runtime_info));
   
   pCRTInfo->uUpdateIntervalMs = SYSTEM_RT_INFO_UPDATE_INTERVAL_MS;
   pCRTInfo->uCurrentSliceStartTime = 0;
   pCRTInfo->iCurrentIndex = 0;
   pCRTInfo->iCurrentIndex2 = 0;
   pCRTInfo->iCurrentIndex3 = 0;

   for( int i=0; i<SYSTEM_RT_INFO_INTERVALS; i++ )
   {
      pCRTInfo->uSliceStartTimeMs[i] = 0;
      pCRTInfo->uSliceDurationMs[i] = 0;
      _controller_runtime_info_reset_slice_signal_info(pCRTInfo, i);
   }

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      pCRTInfo->radioInterfacesSignals[i].iMaxSNRDataForInterface = 1000;
      pCRTInfo->radioInterfacesSignals[i].iMaxDBMDataForInterface = 1000;
      pCRTInfo->radioInterfacesSignals[i].uLastUpdateTimeData = 0;
      pCRTInfo->radioInterfacesSignals[i].iMaxSNRVideoForInterface = 1000;
      pCRTInfo->radioInterfacesSignals[i].iMaxDBMVideoForInterface = 1000;
      pCRTInfo->radioInterfacesSignals[i].uLastUpdateTimeVideo = 0;
      pCRTInfo->radioInterfacesSignals[i].iMaxSNRForInterface = 1000;
      pCRTInfo->radioInterfacesSignals[i].iMaxDBMForInterface = 1000;
      pCRTInfo->radioInterfacesSignals[i].uLastUpdateTime = 0;
   }

   pCRTInfo->uTotalCountOutputSkippedBlocks = 0;
}

controller_runtime_info_vehicle* controller_rt_info_get_vehicle_info(controller_runtime_info* pRTInfo, u32 uVehicleId)
{
   if ( (NULL == pRTInfo) || (0 == uVehicleId) || (MAX_U32 == uVehicleId) )
      return NULL;

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if (pRTInfo->vehicles[i].uVehicleId == uVehicleId )
         return &(pRTInfo->vehicles[i]);
   }

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( pRTInfo->vehicles[i].uVehicleId == 0 )
      {
         pRTInfo->vehicles[i].uVehicleId = uVehicleId;
         pRTInfo->vehicles[i].iCountBlocksInVideoRxBuffers = 0;
         return &(pRTInfo->vehicles[i]);
      }
   }
   return NULL;
}

void controller_rt_info_update_ack_rt_time(controller_runtime_info* pRTInfo, u32 uVehicleId, int iRadioLink, u32 uRoundTripTime, u8 uAckType)
{
   if ( (NULL == pRTInfo) || (0 == uVehicleId) || (MAX_U32 == uVehicleId) )
      return;
   if ( (iRadioLink < 0) || (iRadioLink >= MAX_RADIO_INTERFACES) )
      return;

   controller_runtime_info_vehicle* pRTInfoVehicle = controller_rt_info_get_vehicle_info(pRTInfo, uVehicleId);
   if ( NULL == pRTInfoVehicle )
      return;


   pRTInfoVehicle->iAckTimeIndex[iRadioLink]++;
   if ( pRTInfoVehicle->iAckTimeIndex[iRadioLink] >= SYSTEM_RT_INFO_INTERVALS/2 )
      pRTInfoVehicle->iAckTimeIndex[iRadioLink] = 0;
   pRTInfoVehicle->uAckTimes[pRTInfoVehicle->iAckTimeIndex[iRadioLink]][iRadioLink] = uRoundTripTime;
   pRTInfoVehicle->uAckTypes[pRTInfoVehicle->iAckTimeIndex[iRadioLink]][iRadioLink] = uAckType;

   if ( 0 == pRTInfoVehicle->uMinAckTime[pRTInfo->iCurrentIndex][iRadioLink] )
      pRTInfoVehicle->uMinAckTime[pRTInfo->iCurrentIndex][iRadioLink] = (u8)uRoundTripTime;
   else if ( (u8)uRoundTripTime < pRTInfoVehicle->uMinAckTime[pRTInfo->iCurrentIndex][iRadioLink] )
      pRTInfoVehicle->uMinAckTime[pRTInfo->iCurrentIndex][iRadioLink] = (u8)uRoundTripTime;
   if ( 0 == pRTInfoVehicle->uMaxAckTime[pRTInfo->iCurrentIndex][iRadioLink] )
      pRTInfoVehicle->uMaxAckTime[pRTInfo->iCurrentIndex][iRadioLink] = (u8)uRoundTripTime;
   else if ( (u8)uRoundTripTime > pRTInfoVehicle->uMaxAckTime[pRTInfo->iCurrentIndex][iRadioLink] )
      pRTInfoVehicle->uMaxAckTime[pRTInfo->iCurrentIndex][iRadioLink] = (u8)uRoundTripTime;
}

int controller_rt_info_will_advance_index(controller_runtime_info* pRTInfo, u32 uTimeNowMs)
{
   if ( NULL == pRTInfo )
      return 0;
   if ( uTimeNowMs < (pRTInfo->uCurrentSliceStartTime + pRTInfo->uUpdateIntervalMs) )
      return 0;
   return 1;
}

int controller_rt_info_check_advance_index(controller_runtime_info* pRTInfo, u32 uTimeNowMs)
{
   if ( ! controller_rt_info_will_advance_index(pRTInfo, uTimeNowMs) )
      return 0;

   int iIndex = pRTInfo->iCurrentIndex;

   if ( 0 != pRTInfo->uCurrentSliceStartTime )
      pRTInfo->uSliceDurationMs[iIndex] = uTimeNowMs - pRTInfo->uCurrentSliceStartTime;
   pRTInfo->uCurrentSliceStartTime = uTimeNowMs;

   // ------------------------------------------------
   // Begin Compute derived values
   
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( (pRTInfo->radioInterfacesSignalInfoVideo[pRTInfo->iCurrentIndex][i].iDbmMax > -500) && (pRTInfo->radioInterfacesSignalInfoVideo[pRTInfo->iCurrentIndex][i].iDbmMax < 500) )
      {
         if ( (pRTInfo->radioInterfacesSignals[i].uLastUpdateTimeVideo == 0) ||
              (pRTInfo->radioInterfacesSignalInfoVideo[pRTInfo->iCurrentIndex][i].iDbmMax > pRTInfo->radioInterfacesSignals[i].iMaxDBMVideoForInterface + 5) ||
              (pRTInfo->radioInterfacesSignalInfoVideo[pRTInfo->iCurrentIndex][i].iDbmMax < pRTInfo->radioInterfacesSignals[i].iMaxDBMVideoForInterface - 5) )
             pRTInfo->radioInterfacesSignals[i].iMaxDBMVideoForInterface = pRTInfo->radioInterfacesSignalInfoVideo[pRTInfo->iCurrentIndex][i].iDbmMax;
         else
             pRTInfo->radioInterfacesSignals[i].iMaxDBMVideoForInterface = (pRTInfo->radioInterfacesSignals[i].iMaxDBMVideoForInterface*60 + pRTInfo->radioInterfacesSignalInfoVideo[pRTInfo->iCurrentIndex][i].iDbmMax*40)/100;
         pRTInfo->radioInterfacesSignals[i].uLastUpdateTimeVideo = uTimeNowMs;
      }
      
      if ( (pRTInfo->radioInterfacesSignalInfoVideo[pRTInfo->iCurrentIndex][i].iSNRMax > -500) && (pRTInfo->radioInterfacesSignalInfoVideo[pRTInfo->iCurrentIndex][i].iSNRMax < 500) )
      {
         if ( (pRTInfo->radioInterfacesSignals[i].uLastUpdateTimeVideo == 0) ||
              (pRTInfo->radioInterfacesSignalInfoVideo[pRTInfo->iCurrentIndex][i].iSNRMax > pRTInfo->radioInterfacesSignals[i].iMaxSNRVideoForInterface + 5) ||
              (pRTInfo->radioInterfacesSignalInfoVideo[pRTInfo->iCurrentIndex][i].iSNRMax < pRTInfo->radioInterfacesSignals[i].iMaxSNRVideoForInterface - 5) )
             pRTInfo->radioInterfacesSignals[i].iMaxSNRVideoForInterface = pRTInfo->radioInterfacesSignalInfoVideo[pRTInfo->iCurrentIndex][i].iSNRMax;
         else
             pRTInfo->radioInterfacesSignals[i].iMaxSNRVideoForInterface = (pRTInfo->radioInterfacesSignals[i].iMaxSNRVideoForInterface*60 + pRTInfo->radioInterfacesSignalInfoVideo[pRTInfo->iCurrentIndex][i].iSNRMax*40)/100;
         pRTInfo->radioInterfacesSignals[i].uLastUpdateTimeVideo = uTimeNowMs;
      }
      
      if ( pRTInfo->radioInterfacesSignals[i].uLastUpdateTimeVideo < uTimeNowMs-200 )
      {
         pRTInfo->radioInterfacesSignals[i].iMaxDBMVideoForInterface = 1000;
         pRTInfo->radioInterfacesSignals[i].iMaxSNRVideoForInterface = 1000;
      }

      if ( (pRTInfo->radioInterfacesSignalInfoData[pRTInfo->iCurrentIndex][i].iDbmMax > -500) && (pRTInfo->radioInterfacesSignalInfoVideo[pRTInfo->iCurrentIndex][i].iDbmMax < 500) )
      {
         if ( (pRTInfo->radioInterfacesSignals[i].uLastUpdateTimeData == 0) ||
              (pRTInfo->radioInterfacesSignalInfoData[pRTInfo->iCurrentIndex][i].iDbmMax > pRTInfo->radioInterfacesSignals[i].iMaxDBMDataForInterface + 5) ||
              (pRTInfo->radioInterfacesSignalInfoData[pRTInfo->iCurrentIndex][i].iDbmMax < pRTInfo->radioInterfacesSignals[i].iMaxDBMDataForInterface - 5) )
             pRTInfo->radioInterfacesSignals[i].iMaxDBMDataForInterface = pRTInfo->radioInterfacesSignalInfoData[pRTInfo->iCurrentIndex][i].iDbmMax;
         else
             pRTInfo->radioInterfacesSignals[i].iMaxDBMDataForInterface = (pRTInfo->radioInterfacesSignals[i].iMaxDBMDataForInterface*60 + pRTInfo->radioInterfacesSignalInfoData[pRTInfo->iCurrentIndex][i].iDbmMax*40)/100;
         pRTInfo->radioInterfacesSignals[i].uLastUpdateTimeData = uTimeNowMs;
      }
      
      if ( (pRTInfo->radioInterfacesSignalInfoData[pRTInfo->iCurrentIndex][i].iSNRMax > -500) && (pRTInfo->radioInterfacesSignalInfoData[pRTInfo->iCurrentIndex][i].iSNRMax < 500) )
      {
         if ( (pRTInfo->radioInterfacesSignals[i].uLastUpdateTimeData == 0) ||
              (pRTInfo->radioInterfacesSignalInfoData[pRTInfo->iCurrentIndex][i].iSNRMax > pRTInfo->radioInterfacesSignals[i].iMaxSNRDataForInterface + 5) ||
              (pRTInfo->radioInterfacesSignalInfoData[pRTInfo->iCurrentIndex][i].iSNRMax < pRTInfo->radioInterfacesSignals[i].iMaxSNRDataForInterface - 5) )
             pRTInfo->radioInterfacesSignals[i].iMaxSNRDataForInterface = pRTInfo->radioInterfacesSignalInfoData[pRTInfo->iCurrentIndex][i].iSNRMax;
         else
             pRTInfo->radioInterfacesSignals[i].iMaxSNRDataForInterface = (pRTInfo->radioInterfacesSignals[i].iMaxSNRDataForInterface*60 + pRTInfo->radioInterfacesSignalInfoData[pRTInfo->iCurrentIndex][i].iSNRMax*40)/100;
         pRTInfo->radioInterfacesSignals[i].uLastUpdateTimeData = uTimeNowMs;
      }
      
      if ( pRTInfo->radioInterfacesSignals[i].uLastUpdateTimeData < uTimeNowMs-1000 )
      {
         pRTInfo->radioInterfacesSignals[i].iMaxDBMDataForInterface = 1000;
         pRTInfo->radioInterfacesSignals[i].iMaxSNRDataForInterface = 1000;
      }

      if ( (pRTInfo->radioInterfacesSignals[i].iMaxDBMVideoForInterface > -500) && (pRTInfo->radioInterfacesSignals[i].iMaxDBMVideoForInterface < 500) )
      {
         pRTInfo->radioInterfacesSignals[i].iMaxDBMForInterface = pRTInfo->radioInterfacesSignals[i].iMaxDBMVideoForInterface;
         pRTInfo->radioInterfacesSignals[i].uLastUpdateTime = uTimeNowMs;
      }
      if ( (pRTInfo->radioInterfacesSignals[i].iMaxSNRVideoForInterface > -500) && (pRTInfo->radioInterfacesSignals[i].iMaxSNRVideoForInterface < 500) )
      {
         pRTInfo->radioInterfacesSignals[i].iMaxSNRForInterface = pRTInfo->radioInterfacesSignals[i].iMaxSNRVideoForInterface;
         pRTInfo->radioInterfacesSignals[i].uLastUpdateTime = uTimeNowMs;
      }

      if ( pRTInfo->radioInterfacesSignals[i].uLastUpdateTime < uTimeNowMs-1000 )
      {
         if ( (pRTInfo->radioInterfacesSignals[i].iMaxDBMDataForInterface > -500) && (pRTInfo->radioInterfacesSignals[i].iMaxDBMDataForInterface < 500) )
         {
            pRTInfo->radioInterfacesSignals[i].iMaxDBMForInterface = pRTInfo->radioInterfacesSignals[i].iMaxDBMDataForInterface;
            pRTInfo->radioInterfacesSignals[i].uLastUpdateTime = uTimeNowMs;
         }
         if ( (pRTInfo->radioInterfacesSignals[i].iMaxSNRDataForInterface > -500) && (pRTInfo->radioInterfacesSignals[i].iMaxSNRDataForInterface < 500) )
         {
            pRTInfo->radioInterfacesSignals[i].iMaxSNRForInterface = pRTInfo->radioInterfacesSignals[i].iMaxSNRDataForInterface;
            pRTInfo->radioInterfacesSignals[i].uLastUpdateTime = uTimeNowMs;
         }       
      }
   }

   // End Compute derived values
   // ------------------------------------------------
   // Advance index

   pRTInfo->iCurrentIndex++;
   if ( pRTInfo->iCurrentIndex >= SYSTEM_RT_INFO_INTERVALS )
      pRTInfo->iCurrentIndex = 0;
   pRTInfo->iCurrentIndex2 = pRTInfo->iCurrentIndex;
   pRTInfo->iCurrentIndex3 = pRTInfo->iCurrentIndex;

   // ---------------------------------------------------
   // Reset the new slice

   iIndex = pRTInfo->iCurrentIndex;
   pRTInfo->uSliceStartTimeMs[iIndex] = uTimeNowMs;
   pRTInfo->uSliceDurationMs[iIndex] = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      pRTInfo->uRxLastDeltaTime[iIndex][i] = 0;
      pRTInfo->uRxVideoPackets[iIndex][i] = 0;
      pRTInfo->uRxVideoECPackets[iIndex][i] = 0;
      pRTInfo->uRxDataPackets[iIndex][i] = 0;
      pRTInfo->uRxHighPriorityPackets[iIndex][i] = 0;
      pRTInfo->uRxMissingPackets[iIndex][i] = 0;
      pRTInfo->uRxMissingPacketsMaxGap[iIndex][i] = 0;
   }
   pRTInfo->uRxProcessedPackets[iIndex] = 0;
   pRTInfo->uRxMaxAirgapSlots[iIndex] = 0;
   pRTInfo->uRxMaxAirgapSlots2[iIndex] = 0;

   pRTInfo->uTxFirstDeltaTime[iIndex] = 0xFF;
   pRTInfo->uTxLastDeltaTime[iIndex] = 0;
   pRTInfo->uTxPackets[iIndex] = 0;
   pRTInfo->uTxHighPriorityPackets[iIndex] = 0;
 
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
      {
         pRTInfo->vehicles[i].uMinAckTime[iIndex][k] = 0;
         pRTInfo->vehicles[i].uMaxAckTime[iIndex][k] = 0;
      }
      pRTInfo->vehicles[i].uCountReqRetransmissions[iIndex] = 0;
      pRTInfo->vehicles[i].uCountReqRetrPackets[iIndex] = 0;
      pRTInfo->vehicles[i].uCountAckRetransmissions[iIndex] = 0;
   }
   pRTInfo->uOutputedVideoPackets[iIndex] = 0;
   pRTInfo->uOutputedVideoPacketsRetransmitted[iIndex] = 0;
   pRTInfo->uOutputedVideoPacketsRetransmittedDiscarded[iIndex] = 0;
   pRTInfo->uOutputedVideoBlocks[iIndex] = 0;
   pRTInfo->uOutputedVideoBlocksSkippedBlocks[iIndex] = 0;
   pRTInfo->uOutputedVideoBlocksECUsed[iIndex] = 0;
   pRTInfo->uOutputedVideoBlocksSingleECUsed[iIndex] = 0;
   pRTInfo->uOutputedVideoBlocksTwoECUsed[iIndex] = 0;
   pRTInfo->uOutputedVideoBlocksMultipleECUsed[iIndex] = 0;
   pRTInfo->uOutputedVideoBlocksMaxECUsed[iIndex] = 0;

   pRTInfo->uOutputedAudioPackets[iIndex] = 0;
   pRTInfo->uOutputedAudioPacketsCorrected[iIndex] = 0;
   pRTInfo->uOutputedAudioPacketsSkipped[iIndex] = 0;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      pRTInfo->iRecvVideoDataRate[iIndex][i] = 0;
   }

   pRTInfo->uFlagsAdaptiveVideo[iIndex] = 0;
   _controller_runtime_info_reset_slice_signal_info(pRTInfo, iIndex);
   return 1;
}