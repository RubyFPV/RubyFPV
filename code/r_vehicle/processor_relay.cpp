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
#include "../base/models_list.h"
#include "../base/ruby_ipc.h"
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "../common/relay_utils.h"
#include "../radio/radiolink.h"
#include "../radio/radio_rx.h"
#include "../utils/utils_vehicle.h"
#include "processor_relay.h"
#include "ruby_rt_vehicle.h"
#include "radio_links.h"
#include "shared_vars.h"
#include "timers.h"

bool s_bHasEverReceivedDataFromRelayedVehicle = false;
u32 s_uLastReceivedRelayedVehicleID = MAX_U32;

u8 s_RadioRawPacketRelayed[MAX_PACKET_TOTAL_SIZE];

// It's a pointer to: type_uplink_rx_info_stats s_UplinkInfoRxStats[MAX_RADIO_INTERFACES];
type_uplink_rx_info_stats* s_pRelayRxInfoStats = NULL;

u8 s_uLastLocalRadioLinkUsedForPingToRelayedVehicle = 0;

u32 s_uLastTimeReceivedRubyTelemetryFromRelayedVehicle = 0;

u32 relay_get_time_last_received_ruby_telemetry_from_relayed_vehicle()
{
   return s_uLastTimeReceivedRubyTelemetryFromRelayedVehicle;
}

void _process_received_ruby_telemetry_from_relayed_vehicle(u8* pPacket, int iLenght)
{
   s_uLastTimeReceivedRubyTelemetryFromRelayedVehicle = g_TimeNow;
}

void relay_init_and_set_rx_info_stats(type_uplink_rx_info_stats* pUplinkStats)
{
   s_pRelayRxInfoStats = pUplinkStats;
   s_bHasEverReceivedDataFromRelayedVehicle = false;
   s_uLastReceivedRelayedVehicleID = MAX_U32;
}

void relay_process_received_single_radio_packet_from_controller_to_relayed_vehicle(int iRadioInterfaceIndex, u8* pBufferData, int iBufferLength)
{
   t_packet_header* pPH = (t_packet_header*)pBufferData;
        
   if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK )
      s_uLastLocalRadioLinkUsedForPingToRelayedVehicle = (u8) g_pCurrentModel->radioInterfacesParams.interface_link_id[iRadioInterfaceIndex];

   relay_send_single_packet_to_relayed_vehicle(pBufferData, iBufferLength);
}


void relay_process_received_radio_packet_from_relayed_vehicle(int iRadioLink, int iRadioInterfaceIndex, u8* pBufferData, int iBufferLength)
{
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastRadioRxTime = g_TimeNow;

   radio_hw_info_t* pRadioInfo = hardware_get_radio_info(iRadioInterfaceIndex);
   if ( NULL == pRadioInfo )
      return;

   t_packet_header* pPH = (t_packet_header*)pBufferData;
   u32 uVehicleIdSrc = pPH->vehicle_id_src;
   int iTotalLength = pPH->total_length;
   u8 uPacketType = pPH->packet_type;
   u8 uPacketFlags = pPH->packet_flags;

   if ( ! g_bReceivedPairingRequest )
      return;

   if ( (uVehicleIdSrc == 0) || (uVehicleIdSrc != g_pCurrentModel->relay_params.uRelayedVehicleId) )
   {
      static bool s_bFirstTimeReceivedDataFromWrongRelayedVID = true;
      if ( s_bFirstTimeReceivedDataFromWrongRelayedVID )
      {
         log_line("[Relay] Started to receive data for first time from wrong relayed VID (%u). Expected relayed VID: %u",
            uVehicleIdSrc, g_pCurrentModel->relay_params.uRelayedVehicleId);
         s_bFirstTimeReceivedDataFromWrongRelayedVID = false;
      }
      return;
   }

   if ( ! s_bHasEverReceivedDataFromRelayedVehicle )
   {
      log_line("[Relay] Started to receive valid data from relayed VID: %u, current relay flags: %s, current relay mode: %s",
         uVehicleIdSrc, str_format_relay_flags(g_pCurrentModel->relay_params.uRelayCapabilitiesFlags), str_format_relay_mode(g_pCurrentModel->relay_params.uCurrentRelayMode));
      s_bHasEverReceivedDataFromRelayedVehicle = true;
      s_uLastReceivedRelayedVehicleID = uVehicleIdSrc;
   }

   // Do not relay video packets if relay mode is not one where remote video through this vehicle is needed

   if ( ((uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO) ||
        ((uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_AUDIO) )
   if ( (uPacketType == PACKET_TYPE_VIDEO_DATA) ||
        (uPacketType == PACKET_TYPE_AUDIO_SEGMENT) )
   if ( ! relay_vehicle_must_forward_video_from_relayed_vehicle(g_pCurrentModel, uVehicleIdSrc) )
      return;

   //type_uplink_rx_info_stats* pRxInfoStats = NULL;
   //if ( NULL != s_pRelayRxInfoStats )
   //   pRxInfoStats = (type_uplink_rx_info_stats*)(((u32*)s_pRelayRxInfoStats) + iRadioInterfaceIndex * sizeof(type_uplink_rx_info_stats));

   bool bPacketContainsDataToForward = false;

   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RUBY )
      bPacketContainsDataToForward = true;

   if ( g_pCurrentModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_VIDEO )
   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
   {
      if ( relay_current_vehicle_must_send_relayed_video_feeds() )
         bPacketContainsDataToForward = true;
   }

   if ( g_pCurrentModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_TELEMETRY )
   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
      bPacketContainsDataToForward = true;

   if ( g_pCurrentModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_COMMANDS )
   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
      bPacketContainsDataToForward = true;
  
   
   // Ruby telemetry and FC telemetry is always forwarded on the relay link
   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
   if ( (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED) ||
        (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_SHORT) ||
        (uPacketType == PACKET_TYPE_FC_TELEMETRY) ||
        (uPacketType == PACKET_TYPE_FC_TELEMETRY_EXTENDED) )
   {
      if ( (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED) ||
           (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_SHORT) )
         _process_received_ruby_telemetry_from_relayed_vehicle(pBufferData, iTotalLength);
      bPacketContainsDataToForward = true;
   }

   if ( uPacketType == PACKET_TYPE_RUBY_PING_CLOCK_REPLY )
       memcpy(pBufferData+sizeof(t_packet_header)+2*sizeof(u8)+sizeof(u32), &s_uLastLocalRadioLinkUsedForPingToRelayedVehicle, sizeof(u8));

   if ( ! bPacketContainsDataToForward )
      return;

   relay_send_packet_to_controller(pBufferData, iBufferLength);
}


void relay_on_relay_params_changed()
{
   if ( NULL == g_pCurrentModel )
      return;

   log_line("[Relay] Processing notification that relay link or vehicle id was updated by user...");
   
   if ( g_pCurrentModel->relay_params.uRelayedVehicleId != s_uLastReceivedRelayedVehicleID )
   {
      log_line("[Relay] Relayed VID changed from %u to %u, reset relay state.", s_uLastReceivedRelayedVehicleID, g_pCurrentModel->relay_params.uRelayedVehicleId);
      s_bHasEverReceivedDataFromRelayedVehicle = false;
      s_uLastReceivedRelayedVehicleID = MAX_U32;
   }

   int iRelayRadioLink = g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId;
   if ( (iRelayRadioLink < 0) || (iRelayRadioLink >= g_pCurrentModel->radioLinksParams.links_count) )
   {
      // Relaying is disabled. Set all radio links and interfaces as active

      log_line("[Relay] Relaying was disabled. Set all radio links and interfaces as usable as regular radio links.");

      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      {
         g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
         g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
      }
   }
   else
   {
      // Relaying is enabled on a radio link.

      log_line("[Relay] Relaying is enabled on radio link %d, mark it as relay link.", iRelayRadioLink+1);
      g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRelayRadioLink] |= RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY;
      
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] != iRelayRadioLink )
         {
            g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
            continue;
         }
         log_line("[Relay] Marked radio interface %d as used for relaying.", i+1);
         g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY;
      }

      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      {
         g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
      }
      g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRelayRadioLink] |= RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY;
   }

   saveCurrentModel();
   
   log_line("[Relay] Notify other processes to reload model (relay params changed)");
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

   log_line("[Relay] Done processing notification that relay parameters where updated by user. Notified all local components about new radio config.");
}

void relay_on_relay_mode_changed(u8 uOldMode, u8 uNewMode)
{
   log_line("[Relay] New relay mode: %d, %s", uNewMode, str_format_relay_mode(uNewMode));

   if ( ! (uOldMode & RELAY_MODE_PERMANENT_REMOTE) )
   if ( uOldMode & RELAY_MODE_REMOTE )
   if ( ! (uNewMode & RELAY_MODE_REMOTE) )
   {
      // To fix video_link_auto_keyframe_set_local_requested_value(0, DEFAULT_VIDEO_MIN_AUTO_KEYFRAME_INTERVAL, "relay mode changed");
   }

   g_iDebugShowKeyFramesAfterRelaySwitch = 6;
}

void relay_on_relay_flags_changed(u32 uNewFlags)
{
   log_line("[Relay] Relay flags changed to: %u, %s", uNewFlags, str_format_relay_flags(uNewFlags));
}

void relay_send_packet_to_controller(u8* pBufferData, int iBufferLength)
{
   if ( iBufferLength <= 0 )
   {
      log_softerror_and_alarm("[Relay] Tried to send an empty radio packet (%d bytes) from relayed vehicle to controller.", iBufferLength);
      return;
   }

   u8* pData = pBufferData;
   int iRemainingLength = iBufferLength;
   u32 uCountChainedPackets = 0;
   int iPacketLength = 0;
   u8 uPacketType = 0;
   u32 uStreamId = 0;
   u32 uSourceVehicleId = 0;
   bool bContainsPairConfirmation = false;

   while ( iRemainingLength > 0 )
   {
      t_packet_header* pPH = (t_packet_header*)pData;
      iPacketLength = pPH->total_length;
      uPacketType = pPH->packet_type;
      uSourceVehicleId = pPH->vehicle_id_src;
      uStreamId = (pPH->stream_packet_idx)>>PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
      
      if ( uPacketType == PACKET_TYPE_RUBY_PAIRING_CONFIRMATION )
         bContainsPairConfirmation = true;

      if ( (uStreamId < 0) || (uStreamId >= MAX_RADIO_STREAMS) )
         uStreamId = 0;

      iRemainingLength -= iPacketLength;
      pData += iPacketLength;
      uCountChainedPackets++;
   }

   if ( uSourceVehicleId != g_pCurrentModel->relay_params.uRelayedVehicleId )
   {
      return;
   } 
   // Send packet on all radio links to controller (that are not set as relay links)

   bool bPacketSent = false;

   for( int iRadioLinkId=0; iRadioLinkId<g_pCurrentModel->radioLinksParams.links_count; iRadioLinkId++ )
   {
      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == iRadioLinkId )
         continue;
        
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         continue;

      if ( !(g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;

      int iRadioInterfaceIndex = -1;
      for( int k=0; k<g_pCurrentModel->radioInterfacesParams.interfaces_count; k++ )
      {
         if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[k] == iRadioLinkId )
         {
            iRadioInterfaceIndex = k;
            break;
         }
      }
      if ( iRadioInterfaceIndex < 0 )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterfaceIndex);
      if ( ! pRadioHWInfo->openedForWrite )
         continue;
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;
      
      int nRateTx = g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[iRadioLinkId];
      radio_set_out_datarate(nRateTx, uPacketType, g_TimeNow);

      u32 radioFlags = g_pCurrentModel->radioInterfacesParams.interface_current_radio_flags[iRadioInterfaceIndex];
      radio_set_frames_flags(radioFlags, g_TimeNow);

      int totalLength = radio_build_new_raw_ieee_packet(iRadioLinkId, s_RadioRawPacketRelayed, pBufferData, iBufferLength, RADIO_PORT_ROUTER_DOWNLINK, 0);

      if ( (totalLength >0) && radio_write_raw_ieee_packet(iRadioInterfaceIndex, s_RadioRawPacketRelayed, totalLength, 0) )
      {           
         bPacketSent = true;
         g_SM_RadioStats.radio_links[iRadioLinkId].totalTxPackets++;
         g_SM_RadioStats.radio_links[iRadioLinkId].totalTxBytes += iBufferLength;
      }
      else
         log_softerror_and_alarm("[RelayTX] Failed to write to radio interface %d.", iRadioInterfaceIndex+1);
   }

   if ( ! bPacketSent )
   {
      log_softerror_and_alarm("[RelayTX] Packet not sent! No radio interface could send it.");
      if ( bContainsPairConfirmation )
         log_softerror_and_alarm("[RelayTX] Contained a pairing confirmation that was not relayed back.");
      return;
   }

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastRadioTxTime = g_TimeNow;

   if ( bContainsPairConfirmation )
     log_line("[RelayTX] Relayed back pairing confirmation from relayed vehicle VID %u to controller %u",
        uSourceVehicleId, g_uControllerId);
   //log_line("[RelayTX] Sent relayed packet to controller, stream %u, %d bytes, relayed VID: %u, current VID: %u", uStreamId, iBufferLength, uSourceVehicleId, g_pCurrentModel->uVehicleId);
}

void relay_send_single_packet_to_relayed_vehicle(u8* pBufferData, int iBufferLength)
{
   if ( (NULL == pBufferData) || (iBufferLength <= 0) )
   {
      log_softerror_and_alarm("[Relay] Tried to send an empty radio packet to relayed vehicle");
      return;
   }
   t_packet_header* pPH = (t_packet_header*)pBufferData;
   
   u32 uRelayedVehicleId = pPH->vehicle_id_dest;
   u32 uStreamId = (pPH->stream_packet_idx)>>PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   
   if ( (uStreamId < 0) || (uStreamId >= MAX_RADIO_STREAMS) )
      uStreamId = 0;

   if ( uRelayedVehicleId != g_pCurrentModel->relay_params.uRelayedVehicleId )
   {
      return;
   }

   // Send packet on all radio links to relayed vehicle

   bool bPacketSent = false;

   for( int iRadioLinkId=0; iRadioLinkId<g_pCurrentModel->radioLinksParams.links_count; iRadioLinkId++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;

      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY) )
         continue;

      if ( !(g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;

      int iRadioInterfaceIndex = -1;
      for( int k=0; k<g_pCurrentModel->radioInterfacesParams.interfaces_count; k++ )
         if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[k] == iRadioLinkId )
         {
            iRadioInterfaceIndex = k;
            break;
         }
      if ( iRadioInterfaceIndex < 0 )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterfaceIndex);
      if ( ! pRadioHWInfo->openedForWrite )
         continue;
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;
      
      int nRateTx = g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[iRadioLinkId];
      radio_set_out_datarate(nRateTx, pPH->packet_type, g_TimeNow);

      u32 radioFlags = g_pCurrentModel->radioInterfacesParams.interface_current_radio_flags[iRadioInterfaceIndex];
      radio_set_frames_flags(radioFlags, g_TimeNow);

      int totalLength = radio_build_new_raw_ieee_packet(iRadioLinkId, s_RadioRawPacketRelayed, pBufferData, iBufferLength, RADIO_PORT_ROUTER_UPLINK, 0);

      if ( (totalLength>0) && radio_write_raw_ieee_packet(iRadioInterfaceIndex, s_RadioRawPacketRelayed, totalLength, 0) )
      {           
         bPacketSent = true;
         g_SM_RadioStats.radio_links[iRadioLinkId].totalTxPackets++;
         g_SM_RadioStats.radio_links[iRadioLinkId].totalTxBytes += iBufferLength;
      }
      else
         log_softerror_and_alarm("[RelayTX] Failed to write to radio interface %d.", iRadioInterfaceIndex+1);
   }

   if ( ! bPacketSent )
   {
      log_softerror_and_alarm("[RelayTX] Packet not sent to relayed vehicle! No radio interface could send it.");
      return;
   }

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastRadioTxTime = g_TimeNow;
}

bool relay_current_vehicle_must_send_own_video_feeds()
{
   static bool sl_bCurrentRelayModeMustSentOwnVideoFeed = false;
   bool bMustSendOwnVideo = false;

   if ( (NULL == g_pCurrentModel) || g_bVideoPaused )
   {
      if ( sl_bCurrentRelayModeMustSentOwnVideoFeed )
      {
         sl_bCurrentRelayModeMustSentOwnVideoFeed = false;
         log_line("Flag to tell if this vehicle should send it's own video feeds changed to: false");
      }
      return false;
   }
   
   bMustSendOwnVideo = relay_vehicle_must_forward_own_video(g_pCurrentModel);

   if ( bMustSendOwnVideo != sl_bCurrentRelayModeMustSentOwnVideoFeed )
   {
      sl_bCurrentRelayModeMustSentOwnVideoFeed = bMustSendOwnVideo;
      log_line("Flag to tell if this vehicle should send it's own video feeds changed to: %s", bMustSendOwnVideo?"true":"false");
   }
   return bMustSendOwnVideo;
}

bool relay_current_vehicle_must_send_relayed_video_feeds()
{
   return relay_vehicle_must_forward_relayed_video(g_pCurrentModel);
}