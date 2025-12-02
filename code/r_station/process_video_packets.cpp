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

#include "process_video_packets.h"
#include "relay_rx.h"
#include "../base/models.h"
#include "../base/models_list.h"
#include "../base/controller_rt_info.h"
#include "../base/parser_h264.h"
#include "../common/relay_utils.h"
#include "../radio/radio_rx.h"
#include "adaptive_video.h"
#include "shared_vars.h"
#include "timers.h"

extern ParserH264 s_ParserH264RadioInput;

ProcessorRxVideo* _find_create_rx_video_processor(u32 uVehicleId, u32 uVideoStreamIndex)
{
   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( NULL != g_pVideoProcessorRxList[i] )
      if ( g_pVideoProcessorRxList[i]->m_uVehicleId == uVehicleId )
      if ( g_pVideoProcessorRxList[i]->m_uVideoStreamIndex == uVideoStreamIndex )
         return g_pVideoProcessorRxList[i];
   }


   int iFirstFreeSlot = -1;
   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( NULL == g_pVideoProcessorRxList[i] )
      {
         iFirstFreeSlot = i;
         break;
      }
   }

   if ( -1 == iFirstFreeSlot )
   {
      log_softerror_and_alarm("No more free slots to create new video rx processor, for VID: %u", uVehicleId);
      return NULL;
   }

   log_line("Creating new video Rx processor for VID %u, video stream id %d", uVehicleId, uVideoStreamIndex);
   g_pVideoProcessorRxList[iFirstFreeSlot] = new ProcessorRxVideo(uVehicleId, uVideoStreamIndex);
   g_pVideoProcessorRxList[iFirstFreeSlot]->init();

   int iRuntimeIndex = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_State.vehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
      {
         iRuntimeIndex = i;
         break;
      }
   }
   if ( -1 == iRuntimeIndex )
      log_softerror_and_alarm("Failed to find vehicle runtime info for VID %u while processing a video packet.", uVehicleId);
   else
      adaptive_video_on_new_vehicle(iRuntimeIndex);
   return g_pVideoProcessorRxList[iFirstFreeSlot];
}

void process_received_video_component_packet(int iInterfaceIndex, u8* pPacket, int iPacketLength)
{
   if ( g_bSearching || (NULL == pPacket) || (iPacketLength <= 0) )
         return ;

   t_packet_header* pPH = (t_packet_header*)pPacket;

   u32 uVehicleId = pPH->vehicle_id_src;
   Model* pModel = findModelWithId(uVehicleId, 111);

   if ( (NULL == pModel) || ( ! is_sw_version_atleast(pModel, 11, 6)) )
      return ;

   if ( pPH->packet_type == PACKET_TYPE_VIDEO_DATA )
   {
      /*
      t_packet_header_video_segment* pPHVS = (t_packet_header_video_segment*) (pPacket+sizeof(t_packet_header));
      int iDbgDR = (int) pPH->uCRC;
      log_line("DBG %c%d [%u/%02d of %02d] sch %d/%d, framep %d/%d, EOF in %d+%d, %u ms from now, NAL %s%s-%s%s%s, eof?%d DR: %d", 
          (pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED)?'r':'f',
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
      ProcessorRxVideo* pProcessorVideo = _find_create_rx_video_processor(uVehicleId, 0);

      if ( NULL == pProcessorVideo )
         return;

      if ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
         pProcessorVideo->handleReceivedVideoRetrPacket(iInterfaceIndex, pPacket, iPacketLength);
      else
         pProcessorVideo->handleReceivedVideoPacket(iInterfaceIndex, pPacket, iPacketLength);
   }

   if ( pPH->packet_type == PACKET_TYPE_VIDEO_ADAPTIVE_VIDEO_PARAMS_ACK )
   {
      u32 uRequestId = 0;
      memcpy((u8*)&uRequestId, pPacket + sizeof(t_packet_header), sizeof(u32));
      adaptive_video_received_vehicle_msg_ack(uRequestId, pPH->vehicle_id_src, iInterfaceIndex);
   }
}