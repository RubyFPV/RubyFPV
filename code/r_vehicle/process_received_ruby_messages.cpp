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

#include "process_received_ruby_messages.h"
#include "shared_vars.h"
#include "../base/models_list.h"
#include "../base/ruby_ipc.h"
#include "../base/hardware_cam_maj.h"
#include "../base/hardware_radio_sik.h"
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "../common/relay_utils.h"
#include "../radio/radiolink.h"
#include "../radio/radio_rx.h"
#include "../radio/radio_tx.h"
#include "../radio/radio_duplicate_det.h"

#include "timers.h"
#include "packets_utils.h"
#include "processor_tx_video.h"
#include "processor_relay.h"
#include "events.h"
#include "test_link_params.h"
#include "video_sources.h"
#include "adaptive_video.h"
#include "ruby_rt_vehicle.h"
#include "negociate_radio.h"
#include "radio_links.h"

u32 s_uResendPairingConfirmationCounter = 0;

int _process_received_ping_messages(int iInterfaceIndex, u8* pPacketBuffer)
{
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   
   if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK )
   if ( pPH->total_length >= sizeof(t_packet_header) + 5*sizeof(u8) )
   {
      u8 uLocalRadioLinkId = (u8) g_pCurrentModel->radioInterfacesParams.interface_link_id[iInterfaceIndex];
      
      u8 uPingId = 0;
      u8 uSenderLocalRadioLinkId = 0;
      u8 uDummy1 = 0;
      u8 uDummy2 = 0;
      g_TimeNow = get_current_timestamp_ms();
      memcpy( &uPingId, pPacketBuffer + sizeof(t_packet_header), sizeof(u8));
      memcpy( &uSenderLocalRadioLinkId, pPacketBuffer + sizeof(t_packet_header)+sizeof(u8), sizeof(u8));
      memcpy( &uDummy1, pPacketBuffer + sizeof(t_packet_header)+2*sizeof(u8), sizeof(u8));
      memcpy( &uDummy2, pPacketBuffer + sizeof(t_packet_header)+3*sizeof(u8), sizeof(u8));

      log_line("Recv Ping id %d for vehiclke link %d, for VID %u (this is VID %u)", uPingId, uLocalRadioLinkId+1, pPH->vehicle_id_dest, g_pCurrentModel->uVehicleId);
      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_RUBY_PING_CLOCK_REPLY, STREAM_ID_DATA);
      PH.packet_flags |= PACKET_FLAGS_BIT_HIGH_PRIORITY;
      PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      PH.vehicle_id_dest = g_uControllerId;
      PH.total_length = sizeof(t_packet_header) + 3*sizeof(u8) + sizeof(u32);
      u8 packet[MAX_PACKET_TOTAL_SIZE];
      memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
      memcpy(packet+sizeof(t_packet_header), &uPingId, sizeof(u8));
      memcpy(packet+sizeof(t_packet_header)+sizeof(u8), &g_TimeNow, sizeof(u32));
      memcpy(packet+sizeof(t_packet_header)+sizeof(u8)+sizeof(u32), &uSenderLocalRadioLinkId, sizeof(u8));
      memcpy(packet+sizeof(t_packet_header)+2*sizeof(u8)+sizeof(u32), &uLocalRadioLinkId, sizeof(u8));

      if ( PH.packet_flags & PACKET_FLAGS_BIT_HIGH_PRIORITY )
         send_packet_to_radio_interfaces(packet, PH.total_length, -1);
      else
         packets_queue_inject_packet_first(&g_QueueRadioPacketsOut, packet);
      
      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
      if ( (g_pCurrentModel->relay_params.uRelayedVehicleId != 0) && (g_pCurrentModel->relay_params.uRelayedVehicleId != g_pCurrentModel->uVehicleId) )
      {
         memcpy(packet, pPacketBuffer, pPH->total_length);
         t_packet_header* pPHForward = (t_packet_header*)&packet[0];
         pPHForward->vehicle_id_dest = g_pCurrentModel->relay_params.uRelayedVehicleId;
         relay_send_single_packet_to_relayed_vehicle(packet, pPHForward->total_length);
         //if ( pPHForward->packet_flags & PACKET_FLAGS_BIT_HIGH_PRIORITY )
         //   send_packet_to_radio_interfaces(packet, pPHForward->total_length, -1);
         //else
         //   packets_queue_inject_packet_first(&g_QueueRadioPacketsOut, packet);
      }
   
      u8 uPingFlags = pPacketBuffer[sizeof(t_packet_header) + 4 * sizeof(u8)];
      bool bOSDTelem = false;
      if ( uPingFlags & 0x01 )
         bOSDTelem = true;
      if ( g_bOSDPluginsNeedTelemetryStreams != bOSDTelem )
      {
         g_bOSDPluginsNeedTelemetryStreams = bOSDTelem;
         log_line("Set flag that OSD plugins require full telmetry: %s", g_bOSDPluginsNeedTelemetryStreams?"on":"off");

         radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_OSD_PLUGINS_NEED_TELEMETRY, STREAM_ID_DATA);
         PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
         PH.vehicle_id_dest = g_bOSDPluginsNeedTelemetryStreams?0x01:0x00;
         PH.total_length = sizeof(t_packet_header);

         memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
        
         // Forward to other components
         ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, packet, PH.total_length);
      }
   }
   return 0;
}

int process_received_ruby_message_from_controller(int iInterfaceIndex, u8* pPacketBuffer)
{
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;

   if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK )
   {
      return _process_received_ping_messages(iInterfaceIndex, pPacketBuffer);
   }

   if ( pPH->packet_type == PACKET_TYPE_TEST_RADIO_LINK )
   {
      test_link_process_received_message(iInterfaceIndex, pPacketBuffer);
      return 0;
   }


   if ( pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_REQUEST )
   {
      bool bUpdated = false;
      u32 uResendCount = 0;
      u32 uDeveloperFlags = 0xFFFFFFFF;
      u32 uOldDeveloperFlags = 0xFFFFFFFF;
      u32 uControllerBoardType = g_pCurrentModel->uControllerBoardType;
      if ( pPH->total_length >= sizeof(t_packet_header) + sizeof(u32) )
         memcpy(&uResendCount, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));
      if ( pPH->total_length >= sizeof(t_packet_header) + 2*sizeof(u32) )
      {
         memcpy(&uDeveloperFlags, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32), sizeof(u32));
         uOldDeveloperFlags = g_pCurrentModel->uDeveloperFlags;
         g_pCurrentModel->uDeveloperFlags = uDeveloperFlags;
         bUpdated = true;
      }
      else
         log_line("No developer flags in the pairing request.");

      if ( pPH->total_length >= sizeof(t_packet_header) + 3*sizeof(u32) )
      {
         memcpy(&uControllerBoardType, pPacketBuffer + sizeof(t_packet_header) + 2*sizeof(u32), sizeof(u32));
         log_line("Pairing request: received controller board type: %s", str_get_hardware_board_name(uControllerBoardType));
         if ( g_pCurrentModel->uControllerBoardType != uControllerBoardType )
         {
            log_line("Pairing request: controller board type has been updated.");
            g_pCurrentModel->uControllerBoardType = uControllerBoardType;
            bUpdated = true;
         }
      }

      log_line("Pairing request: Currently stored controller ID: %u / %u", g_uControllerId, g_pCurrentModel->uControllerId);
      log_line("Pairing request: Received pairing request from controller (received resend count: %u). From CID %u to VID %u (%s). Developer mode before: %s",
         uResendCount, pPH->vehicle_id_src, pPH->vehicle_id_dest, (pPH->vehicle_id_dest == g_pCurrentModel->uVehicleId)?"self":"not self", (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE)?"on":"off");

      if ( NULL != g_pCurrentModel )
         bUpdated |= g_pCurrentModel->onControllerIdUpdated(pPH->vehicle_id_src);

      if ( g_uControllerId != pPH->vehicle_id_src )
      {
         log_line("Update controller ID stored on file from this: %u to this one: %u", g_uControllerId, pPH->vehicle_id_src);
         u32 uOldControllerId = g_uControllerId;
         g_uControllerId = pPH->vehicle_id_src;
         char szFile[128];
         strcpy(szFile, FOLDER_CONFIG);
         strcat(szFile, FILE_CONFIG_CONTROLLER_ID);
         FILE* fd = fopen(szFile, "w");
         if ( NULL != fd )
         {
            fprintf(fd, "%u\n", g_uControllerId);
            fclose(fd);
         }

         radio_duplicate_detection_remove_data_for_vid(uOldControllerId);
         radio_stats_remove_received_info_for_vid(&g_SM_RadioStats, uOldControllerId);
         bUpdated = true;
      }
      else
         log_line("Pairinig request: controller ID has not changed.");

      g_bReceivedPairingRequest = true;

      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
      if ( g_pCurrentModel->relay_params.uRelayedVehicleId != 0 )
      if ( g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_REMOTE )
      {
         u32 uOldRelayMode = g_pCurrentModel->relay_params.uCurrentRelayMode;
         g_pCurrentModel->relay_params.uCurrentRelayMode = RELAY_MODE_MAIN | RELAY_MODE_IS_RELAY_NODE;
         onEventRelayModeChanged(uOldRelayMode, g_pCurrentModel->relay_params.uCurrentRelayMode, "stop");
         bUpdated = true;
      }

      if ( (uDeveloperFlags & DEVELOPER_FLAGS_USE_PCAP_RADIO_TX ) != (uOldDeveloperFlags & DEVELOPER_FLAGS_USE_PCAP_RADIO_TX) )
         bUpdated = true;

      if ( bUpdated )
         saveCurrentModel();

      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_RUBY_PAIRING_CONFIRMATION, STREAM_ID_DATA);
      PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      PH.vehicle_id_dest = g_uControllerId;
      PH.total_length = sizeof(t_packet_header) + sizeof(u32) + sizeof(u16);

      s_uResendPairingConfirmationCounter++;

      u16 uVersion = (SYSTEM_SW_VERSION_MAJOR << 8) | SYSTEM_SW_VERSION_MINOR;
      u8 packet[MAX_PACKET_TOTAL_SIZE];
      memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
      memcpy(packet+sizeof(t_packet_header), &s_uResendPairingConfirmationCounter, sizeof(u32));
      memcpy(packet+sizeof(t_packet_header)+sizeof(u32), &uVersion, sizeof(u16));
      packets_queue_add_packet(&g_QueueRadioPacketsOut, packet);
      send_radio_config_to_controller();

      if ( (uDeveloperFlags != 0xFFFFFFFF) )
         checkDeveloperFlagsChanges(uOldDeveloperFlags, uDeveloperFlags);

      // Forward to other components
      ruby_ipc_channel_send_message(s_fIPCRouterToCommands, pPacketBuffer, pPH->total_length);
      ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, pPacketBuffer, pPH->total_length);
      if ( g_pCurrentModel->rc_params.rc_enabled )
         ruby_ipc_channel_send_message(s_fIPCRouterToRC, pPacketBuffer, pPH->total_length);
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_SIK_CONFIG )
   {
      u8 uVehicleLinkId = *(pPacketBuffer + sizeof(t_packet_header));
      u8 uCommandId = *(pPacketBuffer + sizeof(t_packet_header) + sizeof(u8));

      log_line("Received SiK config message for local radio link %d, command: %d", (int) uVehicleLinkId+1, (int) uCommandId);

      bool bInvalidParams = false;
      int iLocalRadioInterfaceIndex = -1;
      if ( uVehicleLinkId >= g_pCurrentModel->radioLinksParams.links_count )
         bInvalidParams = true;

      if ( ! bInvalidParams )
      {
         for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
         {
            if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == uVehicleLinkId )
            if ( hardware_radio_index_is_sik_radio(i) )
            {
               iLocalRadioInterfaceIndex = i;
               break;
            }
         }
      }

      if ( -1 == iLocalRadioInterfaceIndex )
         bInvalidParams = true;

      if ( bInvalidParams )
      {
         char szBuff[256];
         strcpy(szBuff, "Invalid radio interface selected.");

         t_packet_header PH;
         radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_SIK_CONFIG, STREAM_ID_DATA);
         PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
         PH.vehicle_id_dest = g_uControllerId;
         PH.total_length = sizeof(t_packet_header) + strlen(szBuff)+3*sizeof(u8);

         u8 packet[MAX_PACKET_TOTAL_SIZE];
         memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
         memcpy(packet+sizeof(t_packet_header), &uVehicleLinkId, sizeof(u8));
         memcpy(packet+sizeof(t_packet_header) + sizeof(u8), &uCommandId, sizeof(u8));
         memcpy(packet+sizeof(t_packet_header) + 2*sizeof(u8), szBuff, strlen(szBuff)+1);
         packets_queue_add_packet(&g_QueueRadioPacketsOut, packet);
         return 0;
      }

      char szBuff[256];
      strcpy(szBuff, "SiK config: done.");

      if ( uCommandId == 1 )
      {
         radio_rx_pause_interface(iLocalRadioInterfaceIndex, "SiK config");
         radio_tx_pause_radio_interface(iLocalRadioInterfaceIndex, "SiK config");
      }
      if ( uCommandId == 2 )
      {
         radio_rx_resume_interface(iLocalRadioInterfaceIndex);
         radio_tx_resume_radio_interface(iLocalRadioInterfaceIndex);
      }

      if ( uCommandId == 0 )
      {
         log_line("Received message to get local SiK radio config for vehicle radio link %d", (int) uVehicleLinkId+1);
         g_iGetSiKConfigAsyncResult = 0;
         g_iGetSiKConfigAsyncRadioInterfaceIndex = iLocalRadioInterfaceIndex;
         g_uGetSiKConfigAsyncVehicleLinkIndex = uVehicleLinkId;
         hardware_radio_sik_get_all_params_async(iLocalRadioInterfaceIndex, &g_iGetSiKConfigAsyncResult);
         return 0;
      }

      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_SIK_CONFIG, STREAM_ID_DATA);
      PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      PH.vehicle_id_dest = g_uControllerId;
      PH.total_length = sizeof(t_packet_header) + strlen(szBuff)+3*sizeof(u8);

      u8 packet[MAX_PACKET_TOTAL_SIZE];
      memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
      memcpy(packet+sizeof(t_packet_header), &uVehicleLinkId, sizeof(u8));
      memcpy(packet+sizeof(t_packet_header) + sizeof(u8), &uCommandId, sizeof(u8));
      memcpy(packet+sizeof(t_packet_header) + 2*sizeof(u8), szBuff, strlen(szBuff)+1);
      packets_queue_add_packet(&g_QueueRadioPacketsOut, packet);

      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_VEHICLE_RECORDING )
   {
      log_line("Received recording command radio packet.");
      if ( pPH->total_length != sizeof(t_packet_header) + 8 * sizeof(u8) )
      {
         log_softerror_and_alarm("Received radio packet for recording of unexpected size. Received %d bytes, expected %d bytes.",
          pPH->total_length, sizeof(t_packet_header) + 8 * sizeof(u8));
         return 0;
      }
      if ( NULL != g_pProcessorTxAudio )
      {
         u8 uCommand = pPacketBuffer[sizeof(t_packet_header)];
         if ( uCommand == 1 || uCommand == 5 )
            g_pProcessorTxAudio->startLocalRecording();
         if ( uCommand == 2 || uCommand == 6 )
            g_pProcessorTxAudio->stopLocalRecording();
      }
      return 0;
   }
   

   if ( pPH->packet_type == PACKET_TYPE_NEGOCIATE_RADIO_LINKS )
      return negociate_radio_process_received_radio_link_messages(pPacketBuffer);

   log_line("Received unprocessed Ruby message from controller, message type: %d", pPH->packet_type);

   return 0;
}

