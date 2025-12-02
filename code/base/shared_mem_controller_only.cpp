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
#include "shared_mem_controller_only.h"
#include "../radio/radiopackets2.h"

shared_mem_video_stream_stats_rx_processors* shared_mem_video_stream_stats_rx_processors_open_for_read()
{
   void *retVal =  open_shared_mem(SHARED_MEM_VIDEO_STREAM_STATS, sizeof(shared_mem_video_stream_stats_rx_processors), 1);
   shared_mem_video_stream_stats_rx_processors *tretval = (shared_mem_video_stream_stats_rx_processors*)retVal;
   return tretval;
}

shared_mem_video_stream_stats_rx_processors* shared_mem_video_stream_stats_rx_processors_open_for_write()
{
   void *retVal =  open_shared_mem(SHARED_MEM_VIDEO_STREAM_STATS, sizeof(shared_mem_video_stream_stats_rx_processors), 0);
   shared_mem_video_stream_stats_rx_processors *tretval = (shared_mem_video_stream_stats_rx_processors*)retVal;
   return tretval;
}

void shared_mem_video_stream_stats_rx_processors_close(shared_mem_video_stream_stats_rx_processors* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_video_stream_stats_rx_processors));
   //shm_unlink(SHARED_MEM_VIDEO_STREAM_STATS);
}

shared_mem_video_stream_stats* get_shared_mem_video_stream_stats_for_vehicle(shared_mem_video_stream_stats_rx_processors* pSM, u32 uVehicleId)
{
   if ( (NULL == pSM) || (0 == uVehicleId) || (MAX_U32 == uVehicleId) )
      return NULL;

   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( pSM->video_streams[i].uVehicleId == uVehicleId )
         return &(pSM->video_streams[i]);
   }
   return NULL;
}

void reset_video_stream_stats_for_vehicle(shared_mem_video_stream_stats_rx_processors* pSM, u32 uVehicleId)
{
   if ( (NULL == pSM) || (0 == uVehicleId) )
      return;
   shared_mem_video_stream_stats* pVSStats = get_shared_mem_video_stream_stats_for_vehicle(pSM, uVehicleId);
   if ( NULL == pVSStats )
      return;

   memset((u8*)pVSStats, 0, sizeof(shared_mem_video_stream_stats));
   pVSStats->uVehicleId = uVehicleId;
   reset_video_stream_stats_detected_info(pVSStats);
}

void reset_video_stream_stats_detected_info(shared_mem_video_stream_stats* pVSStats)
{
   if ( NULL == pVSStats )
      return;
   pVSStats->uDetectedH264Profile = 0;
   pVSStats->uDetectedH264ProfileConstrains = 0;
   pVSStats->uDetectedH264Level = 0;
   pVSStats->iDetectedFPS = -1;
   pVSStats->iDetectedSlices = -1;
   pVSStats->iDetectedKeyframeMs = -1;

   memset((u8*)&(pVSStats->adaptiveHitsLow), 0, sizeof(type_adaptive_info));
   memset((u8*)&(pVSStats->adaptiveHitsHigh), 0, sizeof(type_adaptive_info));
}

shared_mem_radio_rx_queue_info* shared_mem_radio_rx_queue_info_open_for_read()
{
   void *retVal = open_shared_mem(SHARED_MEM_RADIO_RX_QUEUE_INFO_STATS, sizeof(shared_mem_radio_rx_queue_info), 1);
   return (shared_mem_radio_rx_queue_info*)retVal; 
}

shared_mem_radio_rx_queue_info* shared_mem_radio_rx_queue_info_open_for_write()
{
   void *retVal = open_shared_mem(SHARED_MEM_RADIO_RX_QUEUE_INFO_STATS, sizeof(shared_mem_radio_rx_queue_info), 0);
   return (shared_mem_radio_rx_queue_info*)retVal;
}

void shared_mem_radio_rx_queue_info_close(shared_mem_radio_rx_queue_info* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_radio_rx_queue_info));
}

shared_mem_router_vehicles_runtime_info* shared_mem_router_vehicles_runtime_info_open_for_read()
{
   void *retVal = open_shared_mem_for_read(SHARED_MEM_CONTROLLER_ROUTER_VEHICLES_INFO, sizeof(shared_mem_router_vehicles_runtime_info));
   return (shared_mem_router_vehicles_runtime_info*)retVal;
}

shared_mem_router_vehicles_runtime_info* shared_mem_router_vehicles_runtime_info_open_for_write()
{
   void *retVal = open_shared_mem_for_write(SHARED_MEM_CONTROLLER_ROUTER_VEHICLES_INFO, sizeof(shared_mem_router_vehicles_runtime_info));
   return (shared_mem_router_vehicles_runtime_info*)retVal;
}

void shared_mem_router_vehicles_runtime_info_close(shared_mem_router_vehicles_runtime_info* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_router_vehicles_runtime_info));
   //shm_unlink(szName);
}


shared_mem_ctrl_ping_stats* shared_mem_ctrl_ping_stats_info_open_for_read()
{
   void *retVal = open_shared_mem_for_read(SHARED_MEM_CONTROLLER_PING_STATS, sizeof(shared_mem_ctrl_ping_stats));
   return (shared_mem_ctrl_ping_stats*)retVal;
}

shared_mem_ctrl_ping_stats* shared_mem_rctrl_ping_stats_info_open_for_write()
{
   void *retVal = open_shared_mem_for_write(SHARED_MEM_CONTROLLER_PING_STATS, sizeof(shared_mem_ctrl_ping_stats));
   return (shared_mem_ctrl_ping_stats*)retVal;
}

void shared_mem_ctrl_ping_stats_info_close(shared_mem_ctrl_ping_stats* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_ctrl_ping_stats));
   //shm_unlink(szName);
}
