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

int s_iNegociateRadioCurrentInterfaceIndex = 0;
u32 s_uNegociateRadioCurrentTestRadioFlags = 0;
int s_iNegociateRadioCurrentTestDataRate = 0;
int s_iNegociateRadioCurrentTestTxPowerMw = 0;

bool s_bNegociateRadioHasReceivedApplyParams = false;
type_radio_interfaces_runtime_capabilities_parameters s_NegociateRadioApplyRadioIntCapabilities;
u32 s_uNegociateRadioApplyRadioIntSupportedRadioFlags[MAX_RADIO_INTERFACES];
u32 s_uNegociateRadioApplyRadioLinksTxRadioFlags[MAX_RADIO_INTERFACES];
u32 s_uNegociateRadioApplyRadioLinksRxRadioFlags[MAX_RADIO_INTERFACES];

void _negociate_radio_link_on_start()
{
   if ( ! s_bIsNegociatingRadioLinks )
      log_line("[NegociateRadioLink] Started negociation.");
   s_bIsNegociatingRadioLinks = true;
   s_bNegociateRadioHasReceivedApplyParams = false;
   s_uTimeStartOfNegociatingRadioLinks = g_TimeNow;

   s_uOriginalNegociateRadioVideoBitrate = video_sources_get_last_set_video_bitrate();
   video_sources_set_video_bitrate(DEFAULT_LOWEST_ALLOWED_ADAPTIVE_VIDEO_BITRATE, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iIPQuantizationDelta, "NegociateRadio Start");
}

void _negociate_radio_link_cleanup()
{
   if ( s_bIsNegociatingRadioLinks )
      log_line("[NegociateRadioLink] End negociation.");
   s_bIsNegociatingRadioLinks = false;
   s_bNegociateRadioHasReceivedApplyParams = false;
   adaptive_video_check_update_params();

   if ( 0 != s_uOriginalNegociateRadioVideoBitrate )
   {
      video_sources_set_video_bitrate(s_uOriginalNegociateRadioVideoBitrate, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iIPQuantizationDelta, "NegociateRadio End");
      s_uOriginalNegociateRadioVideoBitrate = 0;
   }
}

void _negociate_radio_link_save_model(bool bSucceeded)
{
   log_line("[NegociateRadioLink] Saving model (succeeded negociation?: %s), has computed radio links flag set before? %s, has radio link negociated flag set before? %s",
      bSucceeded?"yes":"no",
      (g_pCurrentModel->radioInterfacesRuntimeCapab.uFlagsRuntimeCapab & MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED)?"yes":"no",
      (g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS)?"yes":"no");
   if ( (! bSucceeded) || (! s_bNegociateRadioHasReceivedApplyParams) )
   {
      log_line("[NegociateRadioLink] Remove negociated radio links flags as neither succeeded or neither received apply params.");
      g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags &= ~MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS;
      g_pCurrentModel->radioInterfacesRuntimeCapab.uFlagsRuntimeCapab &= ~MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED;
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( hardware_radio_type_is_wifi(g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[i]) )
            g_pCurrentModel->radioInterfacesRuntimeCapab.uInterfaceFlags[i] &= ~MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED;
      }
   }
   else
   {
      log_line("[NegociateRadioLink] Add negociated radio links flags as either succeeded or either received apply params.");

      memcpy(&g_pCurrentModel->radioInterfacesRuntimeCapab, &s_NegociateRadioApplyRadioIntCapabilities, sizeof(type_radio_interfaces_runtime_capabilities_parameters));
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( ! hardware_radio_type_is_wifi(g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[i]) )
            continue;
         g_pCurrentModel->radioInterfacesParams.interface_supported_radio_flags[i] = s_uNegociateRadioApplyRadioIntSupportedRadioFlags[i];
         int iLinkIndex = g_pCurrentModel->radioInterfacesParams.interface_link_id[i];
         if ( (iLinkIndex >= 0) && (iLinkIndex < g_pCurrentModel->radioLinksParams.links_count) )
         {
            g_pCurrentModel->radioLinksParams.link_radio_flags_tx[iLinkIndex] = s_uNegociateRadioApplyRadioLinksTxRadioFlags[iLinkIndex];
            g_pCurrentModel->radioLinksParams.link_radio_flags_rx[iLinkIndex] = s_uNegociateRadioApplyRadioLinksRxRadioFlags[iLinkIndex];
         }
      }

      g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags |= MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS;
      g_pCurrentModel->radioInterfacesRuntimeCapab.uFlagsRuntimeCapab |= MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED;
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( hardware_radio_type_is_wifi(g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[i]) )
            g_pCurrentModel->radioInterfacesRuntimeCapab.uInterfaceFlags[i] |= MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED;
      }
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
   if ( g_pCurrentModel->rc_params.uRCFlags & RC_FLAGS_ENABLED )
      ruby_ipc_channel_send_message(s_fIPCRouterToRC, (u8*)&PH, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
}

void _negociate_radio_link_end(const char* szReason, bool bApply)
{
   if ( ! s_bIsNegociatingRadioLinks )
      return;

   log_line("[NegociateRadioLink] Ending. Reason: %s, Apply changes: %s", szReason, bApply?"yes":"no");

   s_uLastNegociateRadioTestIndex = 0xFF;
   s_uLastNegociateRadioTestCommand = 0;
   s_iLastNegociateRadioTestTxPower = 0;
   s_uTimeStartOfNegociatingRadioLinks = 0;
   s_uTimeLastNegociateRadioLinksReceivedCommand = 0;

   _negociate_radio_link_save_model(bApply || s_bNegociateRadioHasReceivedApplyParams);
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
   int iTxPowerMw = 0;

   if ( uCommand == NEGOCIATE_RADIO_TEST_PARAMS )
   {
      u8* pTmp = &(pPacketBuffer[sizeof(t_packet_header) + 3*sizeof(u8) + sizeof(int) + sizeof(u32)]);
      memcpy(&iTxPowerMw, pTmp, sizeof(int));
   }

   if ( (uTestIndex == s_uLastNegociateRadioTestIndex) &&
        (uCommand == s_uLastNegociateRadioTestCommand) &&
        (iTxPowerMw == s_iLastNegociateRadioTestTxPower) )
   {
      log_line("[NegociateRadioLink] Received duplicate test %d, command %d, just send reply back.", uTestIndex, uCommand);
      if ( uCommand != NEGOCIATE_RADIO_KEEP_ALIVE )
         packets_queue_add_packet(&g_QueueRadioPacketsOut, s_uBufferLastPacketNegociateReplyToController);
      return 0;
   }

   log_line("[NegociateRadioLink] Received test message %d, command %d, tx power: %d mW", uTestIndex, uCommand, iTxPowerMw);

   s_uLastNegociateRadioTestIndex = uTestIndex;
   s_uLastNegociateRadioTestCommand = uCommand;
   s_iLastNegociateRadioTestTxPower = iTxPowerMw;

   if ( uCommand == NEGOCIATE_RADIO_TEST_PARAMS )
   {
      s_iNegociateRadioCurrentInterfaceIndex = (int) pPacketBuffer[sizeof(t_packet_header) + 2*sizeof(u8)];
      u8* pTmp = &(pPacketBuffer[sizeof(t_packet_header) + 3*sizeof(u8)]);
      memcpy(&s_iNegociateRadioCurrentTestDataRate, pTmp, sizeof(int));
      pTmp += sizeof(int);
      memcpy(&s_uNegociateRadioCurrentTestRadioFlags, pTmp, sizeof(u32));
      pTmp += sizeof(u32);
      memcpy(&s_iNegociateRadioCurrentTestTxPowerMw, pTmp, sizeof(int));
      pTmp += sizeof(int);
      s_uNegociateRadioCurrentTestRadioFlags |= RADIO_FLAGS_FRAME_TYPE_DATA;

      log_line("[NegociateRadioLink] Recv test %d for radio interface %d, command %d, datarate: %s, radio flags: %s, tx power: %d mW",
         uTestIndex, s_iNegociateRadioCurrentInterfaceIndex+1, uCommand, str_format_datarate_inline(s_iNegociateRadioCurrentTestDataRate), str_get_radio_frame_flags_description2(s_uNegociateRadioCurrentTestRadioFlags), s_iNegociateRadioCurrentTestTxPowerMw);

      if ( ! s_bIsNegociatingRadioLinks )
         _negociate_radio_link_on_start();
      adaptive_video_check_update_params();

      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_NEGOCIATE_RADIO_LINKS, STREAM_ID_DATA);
      PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      PH.vehicle_id_dest = g_uControllerId;
      PH.total_length = pPH->total_length;
      memcpy(s_uBufferLastPacketNegociateReplyToController, (u8*)&PH, sizeof(t_packet_header));
      memcpy(&(s_uBufferLastPacketNegociateReplyToController[sizeof(t_packet_header)]), pPacketBuffer + sizeof(t_packet_header), pPH->total_length - sizeof(t_packet_header));
      packets_queue_add_packet(&g_QueueRadioPacketsOut, s_uBufferLastPacketNegociateReplyToController);
   }

   if ( uCommand == NEGOCIATE_RADIO_APPLY_PARAMS )
   {
      log_line("[NegociateRadioLink] Recv apply test number %d, command %d", uTestIndex, uCommand);

      u8* pTmp = &(pPacketBuffer[sizeof(t_packet_header) + 2*sizeof(u8)]);
      int iExpectedSize = (int)(sizeof(t_packet_header) + 2*sizeof(u8) + 3*MAX_RADIO_INTERFACES*sizeof(u32) + sizeof(type_radio_interfaces_runtime_capabilities_parameters));
      if ( pPH->total_length != iExpectedSize )
      {
         log_softerror_and_alarm("[NegociateRadioLink] Received invalid apply params, packet size invalid: %d bytes, expected %d bytes",
            pPH->total_length, iExpectedSize);
      }
      else
      {
         log_line("[NegociateRadioLink] Received valid apply negociated radio params, %d bytes", pPH->total_length);
         s_bNegociateRadioHasReceivedApplyParams = true;
         memcpy(&(s_uNegociateRadioApplyRadioIntSupportedRadioFlags[0]), pTmp, MAX_RADIO_INTERFACES*sizeof(u32));
         pTmp += MAX_RADIO_INTERFACES*sizeof(u32);
         memcpy(&(s_uNegociateRadioApplyRadioLinksTxRadioFlags[0]), pTmp, MAX_RADIO_INTERFACES*sizeof(u32));
         pTmp += MAX_RADIO_INTERFACES*sizeof(u32);
         memcpy(&(s_uNegociateRadioApplyRadioLinksRxRadioFlags[0]), pTmp, MAX_RADIO_INTERFACES*sizeof(u32));
         pTmp += MAX_RADIO_INTERFACES*sizeof(u32);
         memcpy(&s_NegociateRadioApplyRadioIntCapabilities, pTmp, sizeof(type_radio_interfaces_runtime_capabilities_parameters));

         _negociate_radio_link_end("Recv apply from controller", true);
      }

      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_NEGOCIATE_RADIO_LINKS, STREAM_ID_DATA);
      PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      PH.vehicle_id_dest = g_uControllerId;
      PH.total_length = pPH->total_length;
      memcpy(s_uBufferLastPacketNegociateReplyToController, (u8*)&PH, sizeof(t_packet_header));
      memcpy(&(s_uBufferLastPacketNegociateReplyToController[sizeof(t_packet_header)]), pPacketBuffer + sizeof(t_packet_header), pPH->total_length - sizeof(t_packet_header));
      packets_queue_add_packet(&g_QueueRadioPacketsOut, s_uBufferLastPacketNegociateReplyToController);
   }

   if ( uCommand == NEGOCIATE_RADIO_END_TESTS )
   {
      u8 uCanceled = pPacketBuffer[sizeof(t_packet_header) + 2*sizeof(u8)];
      log_line("[NegociateRadioLink] Received message from controller to end negociate radio links tests. Canceled? %s", uCanceled?"yes":"no");

      if ( s_bIsNegociatingRadioLinks )
      {
         if ( uCanceled )
         {
            _negociate_radio_link_cleanup();
            if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE )
               _negociate_radio_link_save_model(false);
         }
         else
            _negociate_radio_link_save_model(true);
      }
      else
         log_line("[NegociateRadioLink] Received negociate end flow message after it was actually ended or applied. Ignore it. Just reply back.");

      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_NEGOCIATE_RADIO_LINKS, STREAM_ID_DATA);
      PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      PH.vehicle_id_dest = g_uControllerId;
      PH.total_length = pPH->total_length;
      memcpy(s_uBufferLastPacketNegociateReplyToController, (u8*)&PH, sizeof(t_packet_header));
      memcpy(&(s_uBufferLastPacketNegociateReplyToController[sizeof(t_packet_header)]), pPacketBuffer + sizeof(t_packet_header), pPH->total_length - sizeof(t_packet_header));
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

   if ( (g_TimeNow > s_uTimeStartOfNegociatingRadioLinks + 60*3*1000) && (g_TimeNow > s_uTimeLastNegociateRadioLinksReceivedCommand + 10000) )
   {
      log_line("[NegociateRadioLink] Trigger end due to flow timeout (flow took too link).");
      _negociate_radio_link_end("Timeout flow", false);
   }

   if (  g_TimeNow > s_uTimeLastNegociateRadioLinksReceivedCommand + 12000 )
   {
      log_line("[NegociateRadioLink] Trigger end due to timeout (no progress/command received from controller).");
      _negociate_radio_link_end("Timeout from controller", false);
   }
}

void negociate_radio_set_end_video_bitrate(u32 uVideoBitrateBPS)
{
   log_line("[NegociateRadioLink] Set end (revert to) video bitrate to %u bps (from %u bps)",
       uVideoBitrateBPS, s_uOriginalNegociateRadioVideoBitrate);
   s_uOriginalNegociateRadioVideoBitrate = uVideoBitrateBPS;
}

bool negociate_radio_link_is_in_progress()
{
   return s_bIsNegociatingRadioLinks;
}

int negociate_radio_link_get_current_test_interface()
{
   if ( ! s_bIsNegociatingRadioLinks )
      return -1;
   return s_iNegociateRadioCurrentInterfaceIndex;
}

u32 negociate_radio_link_get_radio_flags(int iInterfaceIndex)
{
   if ( iInterfaceIndex != s_iNegociateRadioCurrentInterfaceIndex )
      return 0;
   return s_uNegociateRadioCurrentTestRadioFlags;
}

int negociate_radio_link_get_data_rate(int iInterfaceIndex)
{
   if ( iInterfaceIndex != s_iNegociateRadioCurrentInterfaceIndex )
      return 0;
   return s_iNegociateRadioCurrentTestDataRate;
}

int negociate_radio_link_get_txpower_mw(int iInterfaceIndex)
{
   if ( iInterfaceIndex != s_iNegociateRadioCurrentInterfaceIndex )
      return 0;
   return s_iNegociateRadioCurrentTestTxPowerMw;
}
