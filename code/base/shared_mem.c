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
#include "../radio/radiopackets2.h"

void* open_shared_mem(const char* name, int size, int readOnly)
{
   int fd;
   if ( readOnly )
      fd = shm_open(name, O_RDONLY, S_IRUSR | S_IWUSR);
   else
      fd = shm_open(name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

   if(fd < 0)
   {
       log_softerror_and_alarm("[SharedMem] Failed to open shared memory for %s: %s, error: %s",
          readOnly?"read":"write", name, strerror(errno));
       return NULL;
   }
   if ( ! readOnly )
   {
      if (ftruncate(fd, size) == -1)
      {
          log_softerror_and_alarm("[SharedMem] Failed to init (ftruncate) shared memory for writing: %s", name);
          close(fd);
          return NULL;   
      }
   }
   void *retval = NULL;
   if ( readOnly )
      retval = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
   else
      retval = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   if (retval == MAP_FAILED)
   {
      log_softerror_and_alarm("[SharedMem] Failed to map shared memory for %s: %s",
         readOnly?"read":"write", name);
      close(fd);
      return NULL;
   }

   if ( ! readOnly )
      memset(retval, 0, size);

   close(fd);

   log_line("[SharedMem] Opened shared memory object %s in %s mode.", name, readOnly?"read":"write");
   return retval;
}

void* open_shared_mem_for_write(const char* name, int size)
{
   return open_shared_mem(name, size, 0);
}

void* open_shared_mem_for_read(const char* name, int size)
{
   return open_shared_mem(name, size, 1);
}

shared_mem_process_stats* shared_mem_process_stats_open_read(const char* szName)
{
   void *retVal =  open_shared_mem(szName, sizeof(shared_mem_process_stats), 1);
   shared_mem_process_stats *tretval = (shared_mem_process_stats*)retVal;
   return tretval;
}

shared_mem_process_stats* shared_mem_process_stats_open_write(const char* szName)
{
   void *retVal =  open_shared_mem(szName, sizeof(shared_mem_process_stats), 0);
   shared_mem_process_stats *tretval = (shared_mem_process_stats*)retVal;
   memset( tretval, 0, sizeof(shared_mem_process_stats));
   return tretval;
}

void shared_mem_process_stats_close(const char* szName, shared_mem_process_stats* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_process_stats));
   //shm_unlink(szName);
}

void process_stats_reset(shared_mem_process_stats* pStats, u32 timeNow)
{
   if ( NULL == pStats )
      return;

   pStats->lastActiveTime = timeNow;
   pStats->lastRadioTxTime = timeNow;
   pStats->lastRadioRxTime = timeNow;
   pStats->lastIPCIncomingTime = timeNow;
   pStats->lastIPCOutgoingTime = timeNow;
}

void process_stats_mark_active(shared_mem_process_stats* pStats, u32 timeNow)
{
   if ( NULL == pStats )
      return;

   pStats->lastActiveTime = timeNow;
}


shared_mem_radio_stats* shared_mem_radio_stats_open_for_read()
{
   void *retVal = open_shared_mem_for_read(SHARED_MEM_RADIO_STATS, sizeof(shared_mem_radio_stats));
   return (shared_mem_radio_stats*)retVal;
}

shared_mem_radio_stats* shared_mem_radio_stats_open_for_write()
{
   void *retVal = open_shared_mem_for_write(SHARED_MEM_RADIO_STATS, sizeof(shared_mem_radio_stats));
   return (shared_mem_radio_stats*)retVal;
}

void shared_mem_radio_stats_close(shared_mem_radio_stats* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_radio_stats));
   //shm_unlink(szName);
}

shared_mem_radio_stats_rx_hist* shared_mem_radio_stats_rx_hist_open_for_read()
{
   void *retVal = open_shared_mem_for_read(SHARED_MEM_RADIO_STATS_RX_HIST, sizeof(shared_mem_radio_stats_rx_hist));
   return (shared_mem_radio_stats_rx_hist*)retVal;
}

shared_mem_radio_stats_rx_hist* shared_mem_radio_stats_rx_hist_open_for_write()
{
   void *retVal = open_shared_mem_for_write(SHARED_MEM_RADIO_STATS_RX_HIST, sizeof(shared_mem_radio_stats_rx_hist));
   return (shared_mem_radio_stats_rx_hist*)retVal;
}

void shared_mem_radio_stats_rx_hist_close(shared_mem_radio_stats_rx_hist* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_radio_stats_rx_hist));
}

shared_mem_video_frames_stats* shared_mem_video_frames_stats_open_for_read()
{
   void *retVal = open_shared_mem_for_read(SHARED_MEM_VIDEO_FRAMES_STATS , sizeof(shared_mem_video_frames_stats));
   return (shared_mem_video_frames_stats*)retVal;
}

shared_mem_video_frames_stats* shared_mem_video_frames_stats_open_for_write()
{
   void *retVal = open_shared_mem_for_write(SHARED_MEM_VIDEO_FRAMES_STATS , sizeof(shared_mem_video_frames_stats));
   return (shared_mem_video_frames_stats*)retVal;
}

void shared_mem_video_frames_stats_close(shared_mem_video_frames_stats* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_video_frames_stats));
}


shared_mem_video_frames_stats* shared_mem_video_frames_stats_radio_in_open_for_read()
{
   void *retVal = open_shared_mem_for_read(SHARED_MEM_VIDEO_FRAMES_STATS_RADIO_IN , sizeof(shared_mem_video_frames_stats));
   return (shared_mem_video_frames_stats*)retVal;
}

shared_mem_video_frames_stats* shared_mem_video_frames_stats_radio_in_open_for_write()
{
   void *retVal = open_shared_mem_for_write(SHARED_MEM_VIDEO_FRAMES_STATS_RADIO_IN , sizeof(shared_mem_video_frames_stats));
   return (shared_mem_video_frames_stats*)retVal;
}

void shared_mem_video_frames_stats_radio_in_close(shared_mem_video_frames_stats* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_video_frames_stats));
}

shared_mem_video_frames_stats* shared_mem_video_frames_stats_radio_out_open_for_read()
{
   void *retVal = open_shared_mem_for_read(SHARED_MEM_VIDEO_FRAMES_STATS_RADIO_OUT , sizeof(shared_mem_video_frames_stats));
   return (shared_mem_video_frames_stats*)retVal;
}

shared_mem_video_frames_stats* shared_mem_video_frames_stats_radio_out_open_for_write()
{
   void *retVal = open_shared_mem_for_write(SHARED_MEM_VIDEO_FRAMES_STATS_RADIO_OUT , sizeof(shared_mem_video_frames_stats));
   return (shared_mem_video_frames_stats*)retVal;
}

void shared_mem_video_frames_stats_radio_out_close(shared_mem_video_frames_stats* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_video_frames_stats));
}

t_packet_header_rc_info_downstream* shared_mem_rc_downstream_info_open_read()
{
   void *retVal =  open_shared_mem(SHARED_MEM_RC_DOWNLOAD_INFO, sizeof(t_packet_header_rc_info_downstream), 1);
   t_packet_header_rc_info_downstream *tretval = (t_packet_header_rc_info_downstream*)retVal;
   return tretval;
}

t_packet_header_rc_info_downstream* shared_mem_rc_downstream_info_open_write()
{
   void *retVal =  open_shared_mem(SHARED_MEM_RC_DOWNLOAD_INFO, sizeof(t_packet_header_rc_info_downstream), 0);
   t_packet_header_rc_info_downstream *tretval = (t_packet_header_rc_info_downstream*)retVal;
   return tretval;
}

void shared_mem_rc_downstream_info_close(t_packet_header_rc_info_downstream* pRCInfo)
{
   if ( NULL != pRCInfo )
      munmap(pRCInfo, sizeof(t_packet_header_rc_info_downstream));
   //shm_unlink(SHARED_MEM_RC_DOWNLOAD_INFO);
}


t_packet_header_rc_full_frame_upstream* shared_mem_rc_upstream_frame_open_read()
{
   void *retVal =  open_shared_mem(SHARED_MEM_RC_UPSTREAM_FRAME, sizeof(t_packet_header_rc_full_frame_upstream), 1);
   t_packet_header_rc_full_frame_upstream *tretval = (t_packet_header_rc_full_frame_upstream*)retVal;
   return tretval;
}

t_packet_header_rc_full_frame_upstream* shared_mem_rc_upstream_frame_open_write()
{
   void *retVal =  open_shared_mem(SHARED_MEM_RC_UPSTREAM_FRAME, sizeof(t_packet_header_rc_full_frame_upstream), 0);
   t_packet_header_rc_full_frame_upstream *tretval = (t_packet_header_rc_full_frame_upstream*)retVal;
   return tretval;
}

void shared_mem_rc_upstream_frame_close(t_packet_header_rc_full_frame_upstream* pRCInfo)
{
   if ( NULL != pRCInfo )
      munmap(pRCInfo, sizeof(t_packet_header_rc_full_frame_upstream));
   //shm_unlink(SHARED_MEM_RC_UPSTREAM_FRAME);
}

void update_shared_mem_video_frames_stats(shared_mem_video_frames_stats* pSMVIStats, u32 uTimeNow)
{
   if ( NULL == pSMVIStats )
      return;
    
   pSMVIStats->uLastTimeStatsUpdate = uTimeNow;
}

void update_shared_mem_video_frames_stats_on_new_frame(shared_mem_video_frames_stats* pSMVFStats, u32 uLastFrameSizeBytes, int iFrameType, int iDetectedSlices, int iDetectedFPS, u32 uTimeNow)
{
   if ( NULL == pSMVFStats )
      return;

   u32 uLastFrameDuration = 0;
   if ( 0 != pSMVFStats->uLastFrameTime )
      uLastFrameDuration = uTimeNow - pSMVFStats->uLastFrameTime;
   pSMVFStats->uLastFrameTime = uTimeNow;

   if ( uLastFrameDuration > 63 )
      uLastFrameDuration = 63;
   if ( uLastFrameDuration < 1 )
      uLastFrameDuration = 1;

   u32 uLastFrameSizeKb = uLastFrameSizeBytes/1000; // transform from bytes to kbytes
   if ( uLastFrameSizeKb > 255 )
      uLastFrameSizeKb = 255; // kbytes

   u32 uFrameIndex = pSMVFStats->uLastFrameIndex;
   pSMVFStats->uFramesTypesAndDuration[uFrameIndex] = (pSMVFStats->uFramesTypesAndDuration[uFrameIndex] & VIDEO_FRAME_TYPE_MASK) | (uLastFrameDuration & VIDEO_FRAME_DURATION_MASK);
   pSMVFStats->uFramesSizes[uFrameIndex] = (u8)uLastFrameSizeKb;

   if ( pSMVFStats->uFramesSizes[uFrameIndex] > 0 )
   {
      int iLastFrameType = (pSMVFStats->uFramesTypesAndDuration[uFrameIndex] & 0xC0) >> 6;
      if ( iLastFrameType == 1 )
      {
         // IFrame
         if ( 0 == pSMVFStats->uAverageIFrameSizeBytes )
            pSMVFStats->uAverageIFrameSizeBytes = uLastFrameSizeBytes;
         else
            pSMVFStats->uAverageIFrameSizeBytes = (pSMVFStats->uAverageIFrameSizeBytes * 3 + uLastFrameSizeBytes) / 4;
      }
      else if ( iLastFrameType == 0 )
      {
         // PFrame
         if ( 0 == pSMVFStats->uAveragePFrameSizeBytes )
            pSMVFStats->uAveragePFrameSizeBytes = uLastFrameSizeBytes;
         else
            pSMVFStats->uAveragePFrameSizeBytes = (pSMVFStats->uAveragePFrameSizeBytes * 5 + uLastFrameSizeBytes) / 6;
      }
   }

   uFrameIndex++;
   uFrameIndex = (uFrameIndex % MAX_FRAMES_SAMPLES);
   pSMVFStats->uLastFrameIndex = uFrameIndex;
 
   if ( iFrameType == 1 )
      pSMVFStats->uFramesTypesAndDuration[uFrameIndex] = 0;
   else if ( iFrameType == 5 )
      pSMVFStats->uFramesTypesAndDuration[uFrameIndex] = 1<<6;
   else
      pSMVFStats->uFramesTypesAndDuration[uFrameIndex] = 2<<6;
   pSMVFStats->uFramesSizes[uFrameIndex] = 0;

   pSMVFStats->uDetectedKeyframeIntervalMs = 0;
   pSMVFStats->uDetectedFPS = (u32)iDetectedFPS;
   pSMVFStats->uDetectedSlices = (u32)iDetectedSlices;
}

