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
#include "../base/models_list.h"
#include "../base/utils.h"
#include "../base/ruby_ipc.h"
#include "../common/string_utils.h"
#include "adaptive_video.h"
#include "shared_vars.h"
#include "timers.h"
#include "negociate_radio.h"
#include "packets_utils.h"
#include "video_sources.h"

bool s_bIsNegociatingRadioLinks = false;
u32 s_uTimeStartOfNegociatingRadioLinks = 0;
u32 s_uTimeLastNegociateRadioLinksReceivedCommand = 0;
u8 s_uLastNegociateRadioTestIndex = 0xFF;
u8 s_uLastNegociateRadioTestCommand = 0;
int s_iLastNegociateRadioTestTxPower = 0;
u32 s_uOriginalNegociateRadioVideoBitrate = 0;

u32 s_uNegociateRadioCurrentTestRadioFlags = 0;
int s_iNegociateRadioCurrentTestDataRate = 0;
int s_iNegociateRadioCurrentTestTxPowerMw = 0;

void _negociate_radio_link_on_start()
{
   if ( ! s_bIsNegociatingRadioLinks )
      log_line("[NegociateRadioLink] Started negociation.");
   s_bIsNegociatingRadioLinks = true;
   s_uTimeStartOfNegociatingRadioLinks = g_TimeNow;

   s_uOriginalNegociateRadioVideoBitrate = video_sources_get_last_set_video_bitrate();
   video_sources_set_video_bitrate(DEFAULT_LOWEST_ALLOWED_ADAPTIVE_VIDEO_BITRATE, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iIPQuantizationDelta, "NegociateRadio Start");
}

void _negociate_radio_link_cleanup()
{
   if ( s_bIsNegociatingRadioLinks )
      log_line("[NegociateRadioLink] End negociation.");
   s_bIsNegociatingRadioLinks = false;
   adaptive_video_check_update_params();

   if ( 0 != s_uOriginalNegociateRadioVideoBitrate )
   {
      video_sources_set_video_bitrate(s_uOriginalNegociateRadioVideoBitrate, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iIPQuantizationDelta, "NegociateRadio End");
      s_uOriginalNegociateRadioVideoBitrate = 0;
   }
}
void _negociate_radio_link_save_model(bool bSucceeded, type_radio_runtime_capabilities_parameters* pRadioRuntimeCapab)
{
   u8 uFlagsRuntimeCapab = g_pCurrentModel->radioRuntimeCapabilities.uFlagsRuntimeCapab;
   if ( NULL != pRadioRuntimeCapab )
   {
      log_line("[NegociateRadioLink] Save final runtime capability frame flags: %s", str_get_radio_frame_flags_description2(pRadioRuntimeCapab->uSupportedMCSFlags));
      memcpy(&g_pCurrentModel->radioRuntimeCapabilities, pRadioRuntimeCapab, sizeof(type_radio_runtime_capabilities_parameters));
   }
   if ( bSucceeded )
   {
      g_pCurrentModel->radioRuntimeCapabilities.uFlagsRuntimeCapab = uFlagsRuntimeCapab | MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED;
      g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags |= MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS;
   }
   else
   {
      g_pCurrentModel->radioRuntimeCapabilities.uFlagsRuntimeCapab = uFlagsRuntimeCapab & (~MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED);
      g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags &= ~MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS;
   }

   g_pCurrentModel->validateRadioSettings();
   saveCurrentModel();

   log_line("[NegociateRadioLink] Notify other processes to reload model, has negociated radio: %s",
      (g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS)?"yes":"no");
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY | (MODEL_CHANGED_GENERIC<<8);
   PH.total_length = sizeof(t_packet_header);

   ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)&PH, PH.total_length);
   ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)&PH, PH.total_length);
   if ( g_pCurrentModel->rc_params.rc_enabled )
      ruby_ipc_channel_send_message(s_fIPCRouterToRC, (u8*)&PH, PH.total_length);
            
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

}

void _negociate_radio_link_end(const char* szReason, bool bApply, u32 uRadioFlagsToApply, int iDataRateToApply, int iTxPowerMwToApply, type_radio_runtime_capabilities_parameters* pRadioRuntimeCapab)
{
   if ( ! s_bIsNegociatingRadioLinks )
      return;

   log_line("[NegociateRadioLink] Ending. Reason: %s, Apply changes: %s", szReason, bApply?"yes":"no");

   s_uLastNegociateRadioTestIndex = 0xFF;
   s_uLastNegociateRadioTestCommand = 0;
   s_iLastNegociateRadioTestTxPower = 0;
   s_uTimeStartOfNegociatingRadioLinks = 0;
   s_uTimeLastNegociateRadioLinksReceivedCommand = 0;

   if ( bApply )
   {
      if ( (0 == iDataRateToApply) && (0 == iTxPowerMwToApply) && (0 == uRadioFlagsToApply) )
         log_line("[NegociateRadioLink] Ending with no changes to apply.");
      else
      {
         log_line("[NegociateRadioLink] Apply final negociated params to model: %d datarate, %d tx power mw, radio flags: %s",
            iDataRateToApply, iTxPowerMwToApply, str_get_radio_frame_flags_description2(uRadioFlagsToApply));
         g_pCurrentModel->radioLinksParams.link_radio_flags[0] = uRadioFlagsToApply;
      }
   }

   _negociate_radio_link_save_model(bApply, pRadioRuntimeCapab);
   _negociate_radio_link_cleanup();

   if ( ! bApply )
      log_line("[NegociateRadioLink] Ended negociation.");
   else
      log_line("[NegociateRadioLink] Ended negociation and applied new settings if any.");
}

int negociate_radio_process_received_radio_link_messages(u8* pPacketBuffer)
{
   if ( NULL == pPacketBuffer )
      return 0;

   static u8 s_uBufferLastPacketNegociateReplyToController[MAX_PACKET_TOTAL_SIZE];

   s_uTimeLastNegociateRadioLinksReceivedCommand = g_TimeNow;

   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   u8 uTestIndex = pPacketBuffer[sizeof(t_packet_header)];
   u8 uCommand = pPacketBuffer[sizeof(t_packet_header) + sizeof(u8)];
   int iRadioLinkIndex = 0;
   int iTxPowerMw = 0;
   int iDatarate = 0;
   u32 uRadioFlags = 0;

   if ( pPH->total_length > (int)(sizeof(t_packet_header) + 2*sizeof(u8)) )
      iRadioLinkIndex = (int) pPacketBuffer[sizeof(t_packet_header) + 2*sizeof(u8)];
   if ( (uCommand == NEGOCIATE_RADIO_TEST_PARAMS) || (uCommand == NEGOCIATE_RADIO_APPLY_PARAMS) )
   {
      u8* pTmp = &(pPacketBuffer[sizeof(t_packet_header) + 3*sizeof(u8)]);
      memcpy(&iDatarate, pTmp, sizeof(int));
      pTmp += sizeof(int);
      memcpy(&uRadioFlags, pTmp, sizeof(u32));
      pTmp += sizeof(u32);
      memcpy(&iTxPowerMw, pTmp, sizeof(int));
      pTmp += sizeof(int);    
   }

   if ( (s_uLastNegociateRadioTestIndex == uTestIndex) && (uCommand == s_uLastNegociateRadioTestCommand) && ( iTxPowerMw == s_iLastNegociateRadioTestTxPower) )
   {
      log_line("[NegociateRadioLink] Received duplicate test %d, command %d, tx power %d mW, just send reply back.", uTestIndex, uCommand, iTxPowerMw);
      if ( uCommand != NEGOCIATE_RADIO_KEEP_ALIVE )
         packets_queue_add_packet(&g_QueueRadioPacketsOut, s_uBufferLastPacketNegociateReplyToController);
      return 0;
   }

   s_uLastNegociateRadioTestIndex = uTestIndex;
   s_uLastNegociateRadioTestCommand = uCommand;
   s_iLastNegociateRadioTestTxPower = iTxPowerMw;

   log_line("[NegociateRadioLink] Received test message %d, command %d, link index: %d", uTestIndex, uCommand, iRadioLinkIndex);

   if ( (uCommand == NEGOCIATE_RADIO_TEST_PARAMS) || (uCommand == NEGOCIATE_RADIO_APPLY_PARAMS) )
   {
      u8* pTmp = &(pPacketBuffer[sizeof(t_packet_header) + 3*sizeof(u8)]);
      memcpy(&iDatarate, pTmp, sizeof(int));
      pTmp += sizeof(int);
      memcpy(&uRadioFlags, pTmp, sizeof(u32));
      pTmp += sizeof(u32);
      memcpy(&iTxPowerMw, pTmp, sizeof(int));
      pTmp += sizeof(int);
      log_line("[NegociateRadioLink] Recv test %d for radio link %d, command %d, datarate: %s, radio flags: %s, tx power: %d mW",
         uTestIndex, iRadioLinkIndex, uCommand, str_format_datarate_inline(iDatarate), str_get_radio_frame_flags_description2(uRadioFlags), iTxPowerMw);

      if ( uCommand == NEGOCIATE_RADIO_TEST_PARAMS )
      {
         if ( ! s_bIsNegociatingRadioLinks )
            _negociate_radio_link_on_start();

         s_uNegociateRadioCurrentTestRadioFlags = uRadioFlags | RADIO_FLAGS_FRAME_TYPE_DATA;
         s_iNegociateRadioCurrentTestDataRate = iDatarate;
         s_iNegociateRadioCurrentTestTxPowerMw = iTxPowerMw;

         adaptive_video_check_update_params();
      }
   
      if ( uCommand == NEGOCIATE_RADIO_APPLY_PARAMS )
      {
         if ( pPH->total_length < (int)(sizeof(t_packet_header) + 3*sizeof(u8) + sizeof(u32) + 2*sizeof(int) + sizeof(type_radio_runtime_capabilities_parameters)) )
         {
            log_softerror_and_alarm("[NegociateRadioLink] Received invalid apply params, packet too short: %d bytes, expected %d bytes",
               pPH->total_length, (int)(sizeof(t_packet_header) + 3*sizeof(u8) + sizeof(u32) + 2*sizeof(int) + sizeof(type_radio_runtime_capabilities_parameters)));
         }
         else
         {
            log_line("[NegociateRadioLink] Received valid apply negociated radio params, %d bytes", pPH->total_length);
            type_radio_runtime_capabilities_parameters* pRadioCapab = (type_radio_runtime_capabilities_parameters*)pTmp;
            _negociate_radio_link_end("Recv apply from controller", true, uRadioFlags | RADIO_FLAGS_FRAME_TYPE_DATA, iDatarate, iTxPowerMw, pRadioCapab);
         }
      }
  
      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_NEGOCIATE_RADIO_LINKS, STREAM_ID_DATA);
      PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      PH.vehicle_id_dest = g_uControllerId;
      PH.total_length = sizeof(t_packet_header) + 3*sizeof(u8) + sizeof(u32) + 2*sizeof(int);

      memcpy(s_uBufferLastPacketNegociateReplyToController, (u8*)&PH, sizeof(t_packet_header));
      u8* pBuffer = &(s_uBufferLastPacketNegociateReplyToController[sizeof(t_packet_header)]);
      *pBuffer = uTestIndex;
      pBuffer++;
      *pBuffer = uCommand;
      pBuffer++;
      *pBuffer = (u8)iRadioLinkIndex;
      pBuffer++;
      memcpy(pBuffer, &iDatarate, sizeof(int));
      pBuffer += sizeof(int);
      memcpy(pBuffer, &uRadioFlags, sizeof(u32));
      pBuffer += sizeof(u32);
      memcpy(pBuffer, &iTxPowerMw, sizeof(int));
      pBuffer += sizeof(int);
      packets_queue_add_packet(&g_QueueRadioPacketsOut, s_uBufferLastPacketNegociateReplyToController);
   }

   if ( uCommand == NEGOCIATE_RADIO_END_TESTS )
   {
      u8 uCanceled = pPacketBuffer[sizeof(t_packet_header) + 2*sizeof(u8)];
      log_line("[NegociateRadioLink] Received message from controller to end negociate radio links tests. Canceled? %s", uCanceled?"yes":"no");
      _negociate_radio_link_cleanup();

      t_packet_header PH;

      if ( uCanceled )
      {
         if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE )
            _negociate_radio_link_save_model(false, NULL);
      }
      else
         _negociate_radio_link_save_model(true, NULL);

      radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_NEGOCIATE_RADIO_LINKS, STREAM_ID_DATA);
      PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      PH.vehicle_id_dest = g_uControllerId;
      PH.total_length = sizeof(t_packet_header) + 3*sizeof(u8);

      memcpy(s_uBufferLastPacketNegociateReplyToController, (u8*)&PH, sizeof(t_packet_header));
      u8* pBuffer = &(s_uBufferLastPacketNegociateReplyToController[sizeof(t_packet_header)]);
      *pBuffer = uTestIndex;
      pBuffer++;
      *pBuffer = uCommand;
      pBuffer++;
      *pBuffer = uCanceled;
      pBuffer++;
      packets_queue_add_packet(&g_QueueRadioPacketsOut, s_uBufferLastPacketNegociateReplyToController);
   }
   return 0;
}

void negociate_radio_periodic_loop()
{
   if ( (! s_bIsNegociatingRadioLinks) || (0 == s_uTimeStartOfNegociatingRadioLinks) || (0 == s_uTimeLastNegociateRadioLinksReceivedCommand) )
      return;

   if ( ! g_pCurrentModel->hasCamera() )
   {
      for( int i=0; i<3; i++ )
      {
         u8 uBuffer[1024];
         t_packet_header PH;
         radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_NEGOCIATE_RADIO_LINKS, STREAM_ID_DATA);
         PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
         PH.vehicle_id_dest = g_uControllerId;
         PH.total_length = 1023;

         memcpy(uBuffer, (u8*)&PH, sizeof(t_packet_header));
         u8* pBuffer = &(uBuffer[sizeof(t_packet_header)]);
         *pBuffer = 0;
         pBuffer++;
         *pBuffer = NEGOCIATE_RADIO_KEEP_ALIVE;
         pBuffer++;
         packets_queue_add_packet(&g_QueueRadioPacketsOut, uBuffer);
         hardware_sleep_micros(50);
      }
   }

   if ( (g_TimeNow > s_uTimeStartOfNegociatingRadioLinks + 60*2*1000) || (g_TimeNow > s_uTimeLastNegociateRadioLinksReceivedCommand + 12000) )
   {
      log_line("[NegociateRadioLink] Trigger end due to timeout (no progress received from controller).");
      _negociate_radio_link_end("Timeout from controller", false, 0,0,0, NULL);
   }
}

bool negociate_radio_link_is_in_progress()
{
   return s_bIsNegociatingRadioLinks;
}

void negociate_radio_set_end_video_bitrate(u32 uVideoBitrateBPS)
{
   log_line("[NegociateRadioLink] Set end (revert to) video bitrate to %u bps (from %u bps)",
       uVideoBitrateBPS, s_uOriginalNegociateRadioVideoBitrate);
   s_uOriginalNegociateRadioVideoBitrate = uVideoBitrateBPS;
}

u32 negociate_radio_link_get_radio_flags()
{
   return s_uNegociateRadioCurrentTestRadioFlags;
}

int negociate_radio_link_get_data_rate()
{
   return s_iNegociateRadioCurrentTestDataRate;
}

int negociate_radio_link_get_txpower_mw()
{
   return s_iNegociateRadioCurrentTestTxPowerMw;
}