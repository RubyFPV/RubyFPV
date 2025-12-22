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

#include <stdlib.h>
#include <stdio.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/commands.h"
#include "../base/models.h"
#include "../base/models_list.h"
#include "../base/encr.h"
#include "../base/radio_utils.h"
#include "../base/hardware.h"
#include "../base/hardware_audio.h"
#include "../base/hardware_procs.h"
#include "../base/ruby_ipc.h"
#include "../common/string_utils.h"
#include "../common/radio_stats.h"
#include "../common/models_connect_frequencies.h"
#include "../common/relay_utils.h"
#include "../radio/radiolink.h"
#include "../radio/radiopacketsqueue.h"
#include "../radio/radio_rx.h"
#include "../radio/radio_tx.h"
#include "../radio/radio_duplicate_det.h"
#include "../utils/utils_controller.h"

#include "shared_vars.h"
#include "radio_links.h"
#include "ruby_rt_station.h"
#include "processor_rx_audio.h"
#include "processor_rx_video.h"
#include "rx_video_output.h"
#include "rx_video_recording.h"
#include "process_radio_in_packets.h"
#include "packets_utils.h"
#include "timers.h"
#include "adaptive_video.h"
#include "radio_links_sik.h"
#include "test_link_params.h"


bool _switch_to_vehicle_radio_link(int iVehicleRadioLinkId)
{
   if ( (iVehicleRadioLinkId < 0) || (iVehicleRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count) )
      return false;

   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      return false;
   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      return false;
   if (  !(g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
      return false;
   
   int iCountAssignableRadioInterfaces = controller_count_asignable_radio_interfaces_to_vehicle_radio_link(g_pCurrentModel, iVehicleRadioLinkId);
   if ( iCountAssignableRadioInterfaces < 1 )
      return false;

   // We must iterate local radio interfaces and switch one from a radio link to this radio link, if it supports this link
   // We must also update the radio state to reflect the new assigned radio links to local radio interfaces
   
   bool bSwitched = false;
   for( int iInterface=0; iInterface<hardware_get_radio_interfaces_count(); iInterface++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterface);
      if ( NULL == pRadioHWInfo )
         continue;

      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
      if ( NULL == pCardInfo )
         continue;

      if ( ! hardware_radio_supports_frequency(pRadioHWInfo, g_pCurrentModel->radioLinksParams.link_frequency_khz[iVehicleRadioLinkId]) )
         continue;

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         continue;

      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;

      if ( ! (pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
         continue;

      if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;

      // Change frequency of the card to the new radio link
      u32 uCurrentFrequencyKhz = pRadioHWInfo->uCurrentFrequencyKhz;
      Preferences* pP = get_Preferences();
      if ( ! radio_utils_set_interface_frequency(g_pCurrentModel, iInterface, iVehicleRadioLinkId, g_pCurrentModel->radioLinksParams.link_frequency_khz[iVehicleRadioLinkId], g_pProcessStats, pP->iDebugWiFiChangeDelay) )
      {
         log_softerror_and_alarm("Failed to change local radio interface %d to new frequency %s", iInterface, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iVehicleRadioLinkId]));
         log_line("Switching back local radio interface %d to previouse frequency %s", iInterface, str_format_frequency(uCurrentFrequencyKhz));
         radio_utils_set_interface_frequency(g_pCurrentModel, iInterface, iVehicleRadioLinkId, uCurrentFrequencyKhz, g_pProcessStats, pP->iDebugWiFiChangeDelay);
         continue;
      }
            
      radio_stats_set_card_current_frequency(&g_SM_RadioStats, iInterface, g_pCurrentModel->radioLinksParams.link_frequency_khz[iVehicleRadioLinkId]);

      int iLocalRadioLinkId = g_SM_RadioStats.radio_interfaces[iInterface].assignedLocalRadioLinkId;
      g_SM_RadioStats.radio_interfaces[iInterface].assignedVehicleRadioLinkId = iVehicleRadioLinkId;
      g_SM_RadioStats.radio_links[iLocalRadioLinkId].matchingVehicleRadioLinkId = iVehicleRadioLinkId;

      // Update controller's main connect frequency too if needed
      if ( uCurrentFrequencyKhz == get_model_main_connect_frequency(g_pCurrentModel->uVehicleId) )
         set_model_main_connect_frequency(g_pCurrentModel->uVehicleId, g_pCurrentModel->radioLinksParams.link_frequency_khz[iVehicleRadioLinkId]);
               
      bSwitched = true;
      break;
   }

   if ( ! bSwitched )
      return false;

   // Update the radio state to reflect the new assigned radio links to local radio interfaces

   if ( NULL != g_pSM_RadioStats )
      memcpy((u8*)g_pSM_RadioStats, (u8*)&g_SM_RadioStats, sizeof(shared_mem_radio_stats));
   return true;
}

void _compute_set_tx_power_settings_for_sik()
{
   ControllerInterfacesSettings* pCIS = get_ControllerInterfacesSettings();
   ControllerInterfacesSettings oldCIS;
   memcpy(&oldCIS, pCIS, sizeof(ControllerInterfacesSettings));

   load_ControllerSettings();
   load_ControllerInterfacesSettings();
   if ( ! reloadCurrentModel() )
      log_softerror_and_alarm("Failed to load current model.");

   int iSikRadioIndexToUpdate = -1;
   int iSikOldPower = -1;
   int iSikNewPower = -1;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( ! pRadioHWInfo->isConfigurable )
         continue;
      if ( !hardware_radio_index_is_sik_radio(i) )
         continue;

      t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
      if ( NULL == pCRII )
         continue;

      for( int k=0; k<oldCIS.radioInterfacesCount; k++ )
      {
         if ( 0 == strncmp(pRadioHWInfo->szMAC, oldCIS.listRadioInterfaces[k].szMAC, MAX_MAC_LENGTH) )
         if ( pCRII->iRawPowerLevel != oldCIS.listRadioInterfaces[k].iRawPowerLevel )
         {
            iSikOldPower = oldCIS.listRadioInterfaces[k].iRawPowerLevel;
            iSikRadioIndexToUpdate = i;
            break;
         }
      }
   }

   if ( iSikRadioIndexToUpdate == -1 )
      return;
 
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iSikRadioIndexToUpdate);
   t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
   iSikNewPower = pCRII->iRawPowerLevel;
   if ( (iSikNewPower < 0) || (iSikNewPower > 20) )
      iSikNewPower = 20;
   log_line("SiK Tx power changed on radio interface %d from %d to %d. Updating SiK radio interface params...",
      iSikRadioIndexToUpdate, iSikOldPower, iSikNewPower);
   if ( g_SiKRadiosState.bConfiguringToolInProgress )
   {
      log_softerror_and_alarm("Another SiK configuration is in progress. Ignoring this one.");
      return;
   }

   if ( g_SiKRadiosState.bConfiguringSiKThreadWorking )
   {
      log_softerror_and_alarm("SiK reinitialization thread is in progress. Ignoring this notification.");
      return;
   }

   if ( g_SiKRadiosState.bMustReinitSiKInterfaces )
   {
      log_softerror_and_alarm("SiK reinitialization is already flagged. Ignoring this notification.");
      return;
   }

   radio_links_close_and_mark_sik_interfaces_to_reopen();
   g_SiKRadiosState.bConfiguringToolInProgress = true;
   g_SiKRadiosState.uTimeStartConfiguring = g_TimeNow;
      
   char szCommand[128];
   sprintf(szCommand, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_SIK_CONFIG_FINISHED);
   hw_execute_bash_command(szCommand, NULL);

   sprintf(szCommand, "./ruby_sik_config none 0 -power %d &", iSikNewPower);
   hw_execute_bash_command(szCommand, NULL);

   // Do not need to notify other components. Just return.
}

bool _switch_to_vehicle(Model* pTmp)
{
   video_processors_cleanup();
   uninit_processing_audio();
   radio_rx_stop_rx_thread();
   radio_links_close_rxtx_radio_interfaces();

   // Reload new model state
   setCurrentModel(pTmp->uVehicleId);
   g_pCurrentModel = getCurrentModel();
   if ( ! reloadCurrentModel() )
      log_softerror_and_alarm("Failed to load current model.");

   reasign_radio_links(true);
   video_processors_init();
   if ( g_pCurrentModel->audio_params.has_audio_device && g_pCurrentModel->audio_params.enabled )
   if ( ! is_audio_processing_started() )
      init_processing_audio();
   return true;
}

void _onModelChangedVideoCodec()
{
   log_line("Received notification that video codec changed. New codec: %s", (g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265)?"H265":"H264");
   rx_video_output_signal_restart_streamer();
   // Reset local info so that we show the "Waiting for video feed" message
   log_line("Reset received video streams flags");
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      g_SM_RadioStats.radio_streams[i][STREAM_ID_VIDEO_1].totalRxBytes = 0;
}

void _process_local_notification_model_changed(t_packet_header* pPH, u8 uChangeType, int fromComponentId, int iExtraParam)
{
   type_radio_interfaces_parameters oldRadioInterfacesParams;
   type_radio_links_parameters oldRadioLinksParams;
   audio_parameters_t oldAudioParams;
   type_relay_parameters oldRelayParams;
   rc_parameters_t oldRCParams;
   video_parameters_t oldVideoParams;
   type_video_link_profile oldVideoLinkProfiles[MAX_VIDEO_LINK_PROFILES];
   osd_parameters_t oldOSDParams;

   ControllerInterfacesSettings* pCIS = get_ControllerInterfacesSettings();
   ControllerInterfacesSettings oldCIS;
   memcpy(&oldCIS, pCIS, sizeof(ControllerInterfacesSettings));

   memcpy(&oldRadioInterfacesParams, &(g_pCurrentModel->radioInterfacesParams), sizeof(type_radio_interfaces_parameters));
   memcpy(&oldRadioLinksParams, &(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));
   memcpy(&oldRCParams, &(g_pCurrentModel->rc_params), sizeof(rc_parameters_t));
   memcpy(&oldAudioParams, &(g_pCurrentModel->audio_params), sizeof(audio_parameters_t));
   memcpy(&oldRelayParams, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));
   memcpy(&oldVideoParams, &(g_pCurrentModel->video_params), sizeof(video_parameters_t));
   memcpy(&(oldVideoLinkProfiles[0]), &(g_pCurrentModel->video_link_profiles[0]), MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile));
   memcpy(&oldOSDParams, &(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));
            
   u32 oldEFlags = g_pCurrentModel->enc_flags;

   if ( uChangeType == MODEL_CHANGED_RADIO_POWERS )
   {
      log_line("Received local notification that radio powers have changed.");
      _compute_set_tx_power_settings_for_sik();
      return;
   }

   // Reload new model state
   if ( ! reloadCurrentModel() )
      log_softerror_and_alarm("Failed to load current model.");

   if ( uChangeType == MODEL_CHANGED_DEVELOPER_FLAGS )
   {
      log_line("Received local notification that current model developer flags have changed.");
      return;
   }

   if ( uChangeType == MODEL_CHANGED_SYNCHRONISED_SETTINGS_FROM_VEHICLE )
   {
      g_pCurrentModel->b_mustSyncFromVehicle = false;
      log_line("Model settings where synchronized from vehicle. Reset model must sync flag.");
   }

   if ( g_pCurrentModel->enc_flags != oldEFlags )
      lpp(NULL, 0);

   if ( uChangeType == MODEL_CHANGED_AUDIO_PARAMS )
   {
      log_line("Received notification that audio params have changed. Updating state and config...");
      if ( (!g_pCurrentModel->audio_params.has_audio_device) || (!g_pCurrentModel->audio_params.enabled) )
      if ( is_audio_processing_started() )
      {
         uninit_processing_audio();
         discardRetransmissionsInfoAndBuffersOnLengthyOp();
         log_line("Done processing notification that audio params have changed.");
         return;
      }
      if ( g_pCurrentModel->audio_params.has_audio_device && g_pCurrentModel->audio_params.enabled )
      if ( ! is_audio_processing_started() )
      {
         init_processing_audio();
         discardRetransmissionsInfoAndBuffersOnLengthyOp();
         log_line("Done processing notification that audio params have changed.");
         return;
      }

      if ( (oldAudioParams.uPacketLength != g_pCurrentModel->audio_params.uPacketLength) ||
           (oldAudioParams.uECScheme != g_pCurrentModel->audio_params.uECScheme) )
      {
         init_audio_rx_state();
         discardRetransmissionsInfoAndBuffersOnLengthyOp();
         log_line("Done processing notification that audio params have changed.");
         return;
      }
      if ( (oldAudioParams.uFlags & 0xFF00) != (g_pCurrentModel->audio_params.uFlags & 0xFF00) )
      {
         init_audio_rx_state();
         discardRetransmissionsInfoAndBuffersOnLengthyOp();
         log_line("Done processing notification that audio params have changed.");
         return;
      }
      log_line("Done processing notification that audio params have changed.");
      return;
   }

   if ( uChangeType == MODEL_CHANGED_VIDEO_PARAMETERS )
   {
      log_line("Processing received notification that video parameters have changed.");
      if ( (g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265) != (oldVideoParams.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265) )
      {
         log_line("Changed video codec. New codec: %s", (g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265)?"H265":"H264");
         _onModelChangedVideoCodec();
      }

      ProcessorRxVideo* pProcessorRxVideo = ProcessorRxVideo::getVideoProcessorForVehicleId(g_pCurrentModel->uVehicleId, 0);
      if ( NULL != pProcessorRxVideo )
         pProcessorRxVideo->setMustParseStream(true);

      bool bResetAdaptive = false;
      if ( g_pCurrentModel->video_params.iCurrentVideoProfile != oldVideoParams.iCurrentVideoProfile )
         bResetAdaptive = true;
      if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS !=
          oldVideoLinkProfiles[oldVideoParams.iCurrentVideoProfile].uTargetVideoBitrateBPS )
         bResetAdaptive = true;
      if ( (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAG_USE_HIGHER_DATARATE) !=
           (oldVideoLinkProfiles[oldVideoParams.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAG_USE_HIGHER_DATARATE) )
         bResetAdaptive = true;
      if ( (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK) !=
           (oldVideoLinkProfiles[oldVideoParams.iCurrentVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK) )
         bResetAdaptive = true;

      if ( bResetAdaptive )
         adaptive_video_reset_state(g_pCurrentModel->uVehicleId);
      else
         adaptive_video_on_vehicle_video_params_changed(g_pCurrentModel->uVehicleId, &oldVideoParams, &(oldVideoLinkProfiles[0]));
      
      rx_video_output_on_changed_video_params(&oldVideoParams, &oldVideoLinkProfiles[0], &(g_pCurrentModel->video_params), &(g_pCurrentModel->video_link_profiles[0]));
      log_line("Done processing received notification that video parameters have changed.");
      return;
   }

   if ( uChangeType == MODEL_CHANGED_SIK_PACKET_SIZE )
   {
      log_line("Received notification from central that SiK packet size whas changed to: %d bytes", iExtraParam);
      log_line("Current model new SiK packet size: %d", g_pCurrentModel->radioLinksParams.iSiKPacketSize); 
      radio_tx_set_sik_packet_size(g_pCurrentModel->radioLinksParams.iSiKPacketSize);
      return;
   }

   if ( uChangeType == MODEL_CHANGED_ROTATED_RADIO_LINKS )
   {
      log_line("Received notification from central that vehicle radio links have been rotated.");
      int iTmp = 0;

      // Rotate info in radio stats: rotate radio links ids
      for( int i=0; i<g_SM_RadioStats.countLocalRadioLinks; i++ )
      {
         g_SM_RadioStats.radio_links[i].lastTxInterfaceIndex = -1;
         if ( g_SM_RadioStats.radio_links[i].matchingVehicleRadioLinkId >= 0 )
         {
            iTmp = g_SM_RadioStats.radio_links[i].matchingVehicleRadioLinkId;
            g_SM_RadioStats.radio_links[i].matchingVehicleRadioLinkId++;
            if ( g_SM_RadioStats.radio_links[i].matchingVehicleRadioLinkId >= g_SM_RadioStats.countVehicleRadioLinks )
               g_SM_RadioStats.radio_links[i].matchingVehicleRadioLinkId = 0;
            log_line("Rotated radio stats local radio link %d from vehicle radio link %d to vehicle radio link %d",
               i+1, iTmp+1, g_SM_RadioStats.radio_links[i].matchingVehicleRadioLinkId);
         }
      }

      // Rotate info in radio stats: rotate radio interfaces assigned radio links
      for( int i=0; i<g_SM_RadioStats.countLocalRadioInterfaces; i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId >= 0 )
         {
            iTmp = g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId;
            g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId++;
            if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId >= g_SM_RadioStats.countVehicleRadioLinks )
               g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId = 0;
            log_line("Rotated radio stats local radio interface %d (assigned to local radio link %d) from vehicle radio link %d to vehicle radio link %d",
               i, g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId+1,
               iTmp, g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId);
         }
      }

      // Update the radio state to reflect the new radio links
      if ( NULL != g_pSM_RadioStats )
         memcpy((u8*)g_pSM_RadioStats, (u8*)&g_SM_RadioStats, sizeof(shared_mem_radio_stats));
   
      discardRetransmissionsInfoAndBuffersOnLengthyOp();
      return;
   }

   if ( uChangeType == MODEL_CHANGED_RESET_RADIO_LINK )
   {
      int iLink = (pPH->vehicle_id_src >> 16) & 0xFF;
      log_line("Received notification that radio link %d was reset.", iLink+1);
      return;
   }

   if ( uChangeType == MODEL_CHANGED_RADIO_LINK_FRAMES_FLAGS )
   {
      int iLink = (pPH->vehicle_id_src >> 16) & 0xFF;
      log_line("Received notification that radio link flags where changed for radio link %d.", iLink+1);
      
      u32 radioFlags = g_pCurrentModel->radioLinksParams.link_radio_flags[iLink];
      log_line("Radio link %d new datarates: %d/%d", iLink+1, g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[iLink], g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[iLink]);
      log_line("Radio link %d new radio flags: %s", iLink+1, str_get_radio_frame_flags_description2(radioFlags));
   
      if( (NULL != g_pCurrentModel) && g_pCurrentModel->radioLinkIsSiKRadio(iLink) )
      {
         log_line("Radio flags or radio datarates changed on a SiK radio link (link %d).", iLink+1);
         for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
         {
            if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId == iLink )
            if ( hardware_radio_index_is_sik_radio(i) )
               radio_links_flag_update_sik_interface(i);
         }
      }
      if( (NULL != g_pCurrentModel) && g_pCurrentModel->radioLinkIsELRSRadio(iLink) )
      {
         log_line("Radio datarates changed on a ELRS radio link (link %d).", iLink+1);
      }
      return;
   }

   if ( uChangeType == MODEL_CHANGED_RADIO_DATARATES )
   {
      int iRadioLink = iExtraParam;
      if ( 0 == memcmp((u8*)&oldRadioLinksParams, (u8*)&g_pCurrentModel->radioLinksParams, sizeof(type_radio_links_parameters) ) )
      {
         log_line("Received local notification that radio data rates have changed on radio link %d but no data rate was changed. Ignoring it.", iRadioLink + 1);
         return;
      }
      log_line("Received local notification that radio data rates have changed on radio link %d", iRadioLink+1);
      log_line("New radio data rates for link %d: v: %d, d: %d/%d, u: %d/%d",
            iRadioLink, g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[iRadioLink],
            g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[iRadioLink],
            g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[iRadioLink],
            g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[iRadioLink],
            g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[iRadioLink]);

      // If uplink data rate for an Atheros card has changed, update it.
      
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iRadioLink )
            continue;
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
            continue;
         if ( controllerIsCardDisabled(pRadioHWInfo->szMAC) )
            continue;

         int nRateTx = DEFAULT_RADIO_DATARATE_LOWEST;
         if ( g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[iRadioLink] == -100 )
            nRateTx = DEFAULT_RADIO_DATARATE_LOWEST;
         else if ( g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[iRadioLink] == 0 )
            nRateTx = DEFAULT_RADIO_DATARATE_LOWEST;
         else
            nRateTx = g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[iRadioLink];
         update_atheros_card_datarate(g_pCurrentModel, i, nRateTx, g_pProcessStats);
         g_TimeNow = get_current_timestamp_ms();
      }
      return;
   }

   if ( uChangeType == MODEL_CHANGED_RELAY_PARAMS )
   {
      log_line("Received notification from central that relay flags and params changed.");

      if ( NULL != g_pCurrentModel )
         radio_duplicate_detection_remove_data_for_all_except(g_pCurrentModel->uVehicleId);
      
      // If relayed vehicle changed, or relay was disabled, remove runtime info for the relayed vehicle

      if ( (g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId < 0) || (oldRelayParams.uRelayedVehicleId != g_pCurrentModel->relay_params.uRelayedVehicleId) )
      {
         // Remove old one
         if ( (oldRelayParams.uRelayedVehicleId != 0) && (oldRelayParams.uRelayedVehicleId != MAX_U32) )
         {
            log_line("Relayed vehicle id changed or relay was disabled. Remove runtimeinfo for old relayed vehicle id %u", oldRelayParams.uRelayedVehicleId);
            logCurrentVehiclesRuntimeInfo();

            bool bRuntimeFound = false;
            bool bProcessorFound = false;
            int iIndex = getVehicleRuntimeIndex(oldRelayParams.uRelayedVehicleId );
            if ( -1 != iIndex )
            {
               removeVehicleRuntimeInfo(iIndex);
               log_line("Removed runtime info at index %d", iIndex);
               bRuntimeFound = true;
            }
            for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
            {
               if ( g_pVideoProcessorRxList[i] != NULL )
               if ( g_pVideoProcessorRxList[i]->m_uVehicleId == oldRelayParams.uRelayedVehicleId )
               {
                  g_pVideoProcessorRxList[i]->uninit();
                  delete g_pVideoProcessorRxList[i];
                  g_pVideoProcessorRxList[i] = NULL;
                  for( int k=i; k<MAX_VIDEO_PROCESSORS-1; k++ )
                     g_pVideoProcessorRxList[k] = g_pVideoProcessorRxList[k+1];
                  log_line("Removed video processor at index %d", i);
                  bProcessorFound = true;
                  break;
               }
            }

            if ( ! bRuntimeFound )
               log_line("Did not find any runtime info for VID %u", oldRelayParams.uRelayedVehicleId);
            if ( ! bProcessorFound )
               log_line("Did not find any video processors for VID %u", oldRelayParams.uRelayedVehicleId);
         }
      }
      else if ( oldRelayParams.isRelayEnabledOnRadioLinkId != g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId )
      {
         log_line("Relay link id was changed (from %d to %d). Reasigning radio interfaces to current vehicle radio links...",
            oldRelayParams.isRelayEnabledOnRadioLinkId, g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId);
         reasign_radio_links(false);
      }
      return;
   }

   if ( uChangeType == MODEL_CHANGED_OSD_PARAMS )
   {
      log_line("Received notification from central that current model OSD params have changed.");
      if ( NULL != g_pCurrentModel )
      if ( oldOSDParams.iRadioInterfacesGraphRefreshIntervalMs != g_pCurrentModel->osd_params.iRadioInterfacesGraphRefreshIntervalMs)
         radio_stats_set_graph_refresh_interval(&g_SM_RadioStats, g_pCurrentModel->osd_params.iRadioInterfacesGraphRefreshIntervalMs);
      return;
   }

   if ( uChangeType == MODEL_CHANGED_VIDEO_CODEC )
   {
      _onModelChangedVideoCodec();
      return;
   }
   
   if ( uChangeType == MODEL_CHANGED_RESET_TO_DEFAULTS )
   {
      log_line("Received local notification that current model was reset to defaults or factory reseted.");
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( g_State.vehiclesRuntimeInfo[i].uVehicleId == g_pCurrentModel->uVehicleId )
         {
            g_State.vehiclesRuntimeInfo[i].bIsPairingDone = false;
            g_State.vehiclesRuntimeInfo[i].uPairingRequestTime = g_TimeNow + 5000;
         }
      }
   }

   // Signal other components about the model change if it's not from central or if settings where synchronized from vehicle
   // Signal other components too if the RC parameters where changed
   bool bNotify = false;

   if ( uChangeType == MODEL_CHANGED_RESET_TO_DEFAULTS )
      bNotify = true;
   if ( (pPH->vehicle_id_src == PACKET_COMPONENT_COMMANDS) ||
        (uChangeType == MODEL_CHANGED_SYNCHRONISED_SETTINGS_FROM_VEHICLE) )
      bNotify = true;
   if ( uChangeType == MODEL_CHANGED_RC_PARAMS )
      bNotify = true;
   if ( bNotify )
   {
      if ( -1 != g_fIPCToTelemetry )
         ruby_ipc_channel_send_message(g_fIPCToTelemetry, (u8*)pPH, pPH->total_length);
      if ( -1 != g_fIPCToRC )
         ruby_ipc_channel_send_message(g_fIPCToRC, (u8*)pPH, pPH->total_length);
   }
}

void process_local_control_packet(u8* pPacketBuffer)
{
   if ( NULL == pPacketBuffer )
      return;

   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;

   if ( pPH->packet_type == PACKET_TYPE_TEST_RADIO_LINK )
   {
      pPacketBuffer += sizeof(t_packet_header);
      int iHeaderSize = (int)pPacketBuffer[1];
      int iVehicleLinkIndex = pPacketBuffer[2];
      int iTestNb = pPacketBuffer[3];
      int iCmdType = pPacketBuffer[4];
      log_line("[TestLink] Received test link message from central, run %d, vehicle link %d. Command type: %s", iTestNb, (int)iVehicleLinkIndex+1, str_get_packet_test_link_command(iCmdType));
      if ( pPH->total_length != sizeof(t_packet_header) + iHeaderSize + sizeof(type_radio_links_parameters) )
      {
         test_link_send_status_message_to_central("Invalid radio parameters.");
         test_link_send_end_message_to_central(false);
         return;
      }
      if ( ! test_link_start(g_uControllerId, g_pCurrentModel->uVehicleId, iVehicleLinkIndex, (type_radio_links_parameters*)(pPacketBuffer + iHeaderSize)) )
         return;
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_PREFERENCES_UPDATED )
   {
      log_line("Received local message that preferences have been updated. Reload them.");
      load_Preferences();
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_TEST_ADAPTIVE_VIDEO )
   {
      adaptive_video_enable_test_mode(pPH->vehicle_id_src?true:false);
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATED_RADIO_TX_POWERS )
   {
      log_line("Received local notification that tx powers or auto tx cards have been updated.");
      load_ControllerInterfacesSettings();
      load_ControllerSettings(); 
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_VIDEO_RECORDING )
   {
      u8 uCmd = pPacketBuffer[sizeof(t_packet_header)];
      if ( 0 == uCmd )
      {
         log_line("Received local message to stop recording.");
         rx_video_recording_stop();
      }
      else if ( 1 == uCmd )
      {
         log_line("Received local message to start recording.");
         rx_video_recording_start();
      }
      else
         log_softerror_and_alarm("Received local message for recording with invalid param %d", uCmd);
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_OSD_PLUGINS_NEED_TELEMETRY )
   {
      g_bOSDPluginsNeedTelemetryStreams = pPH->vehicle_id_src?true:false;
      log_line("Received local message that OSD plugins %s", g_bOSDPluginsNeedTelemetryStreams?"need full telemetry stream":"don't need full telemetry stream");
      return;
   }

   if ( pPH->packet_type == PACEKT_TYPE_LOCAL_CONTROLLER_ADAPTIVE_VIDEO_PAUSE )
   {
      if ( 0xFFFFFFFF == pPH->vehicle_id_dest )
      {
         log_line("Received notification to reset adaptive video state for VID %u", pPH->vehicle_id_src);
         adaptive_video_reset_state(pPH->vehicle_id_src);
      }
      else
      {
         if ( 0 == pPH->vehicle_id_dest )
            log_line("Received notification to global resume adaptive video.");
         else
            log_line("Received notification to global pause adaptive video for %u ms", pPH->vehicle_id_dest);

         adaptive_video_pause(pPH->vehicle_id_dest);
      }
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_PAUSE_RESUME_AUDIO )
   {
      log_line("Received local notif to %s audio stream output.", pPH->vehicle_id_dest?"pause":"resume");
      if ( 0 == pPH->vehicle_id_dest )
      {
         if ( (! g_bSearching) && (NULL != g_pCurrentModel) )
         if ( hardware_has_audio_playback() )
            start_audio_player_and_pipe();
      }
      else
         stop_audio_player_and_pipe();
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROLLER_RELOAD_CORE_PLUGINS )
   {
      log_line("Router received a local message to reload core plugins.");
      refresh_CorePlugins(0);
      log_line("Router finished reloading core plugins.");
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_PAUSE_LOCAL_VIDEO_DISPLAY )
   {
      log_line("Router received local message to %s local video display", (0 == pPH->vehicle_id_dest)?"resume":"pause");
      if ( 0 == pPH->vehicle_id_dest )
      {
         rx_video_output_enable_streamer_output();
      }
      else
      {
         rx_video_output_disable_streamer_output();
         rx_video_output_stop_video_streamer();
      }
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_SWITCH_FAVORIVE_VEHICLE )
   {
      log_line("Received local request to switch to favorite vehicle VID %u (current VID: %u)", pPH->vehicle_id_dest, g_pCurrentModel->uVehicleId);
      Model* pTmp = findModelWithId(pPH->vehicle_id_dest, 100);
      if ( NULL == pTmp )
      {
         log_softerror_and_alarm("Tried to switch to invalid favorite model VID %u", pPH->vehicle_id_dest);
         return;
      }

      _switch_to_vehicle(pTmp);
      send_message_to_central(PACKET_TYPE_LOCAL_CONTROL_SWITCH_FAVORIVE_VEHICLE, pPH->vehicle_id_dest, false);
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_SIK_CONFIG )
   {
      u8 uVehicleLinkId = *(pPacketBuffer + sizeof(t_packet_header));
      u8 uCommandId = *(pPacketBuffer + sizeof(t_packet_header) + sizeof(u8));

      log_line("Received local message to configure SiK radio for vehicle radio link %d, parameter: %d", (int) uVehicleLinkId+1, (int)uCommandId);

      bool bInvalidParams = false;
      int iLocalRadioInterfaceIndex = -1;
      if ( uVehicleLinkId >= g_pCurrentModel->radioLinksParams.links_count )
         bInvalidParams = true;

      if ( ! bInvalidParams )
      {
         for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
         {
            if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId == uVehicleLinkId )
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
         radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_SIK_CONFIG, STREAM_ID_DATA);
         PH.vehicle_id_src = g_uControllerId;
         PH.vehicle_id_dest = g_uControllerId;
         PH.total_length = sizeof(t_packet_header) + strlen(szBuff)+3*sizeof(u8);

         u8 packet[MAX_PACKET_TOTAL_SIZE];
         memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
         memcpy(packet+sizeof(t_packet_header), &uVehicleLinkId, sizeof(u8));
         memcpy(packet+sizeof(t_packet_header) + sizeof(u8), &uCommandId, sizeof(u8));
         memcpy(packet+sizeof(t_packet_header) + 2*sizeof(u8), szBuff, strlen(szBuff)+1);
         radio_packet_compute_crc(packet, PH.total_length);
         if ( ! ruby_ipc_channel_send_message(g_fIPCToCentral, packet, PH.total_length) )
            log_ipc_send_central_error(packet, PH.total_length);
         return;
      }

      char szBuff[256];
      strcpy(szBuff, "SiK config: done.");

      if ( uCommandId == 1 )
      {
         radio_rx_pause_interface(iLocalRadioInterfaceIndex, "SiK configuration");
         radio_tx_pause_radio_interface(iLocalRadioInterfaceIndex, "SiK configuration");
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
         return;
      }

      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_SIK_CONFIG, STREAM_ID_DATA);
      PH.vehicle_id_src = g_uControllerId;
      PH.vehicle_id_dest = g_uControllerId;
      PH.total_length = sizeof(t_packet_header) + strlen(szBuff)+3*sizeof(u8);

      u8 packet[MAX_PACKET_TOTAL_SIZE];
      memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
      memcpy(packet+sizeof(t_packet_header), &uVehicleLinkId, sizeof(u8));
      memcpy(packet+sizeof(t_packet_header) + sizeof(u8), &uCommandId, sizeof(u8));
      memcpy(packet+sizeof(t_packet_header) + 2*sizeof(u8), szBuff, strlen(szBuff)+1);
      radio_packet_compute_crc(packet, PH.total_length);
      if ( ! ruby_ipc_channel_send_message(g_fIPCToCentral, packet, PH.total_length) )
         log_ipc_send_central_error(packet, PH.total_length);

      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROLLER_SEARCH_FREQ_CHANGED )
   {
      if ( ! g_bSearching )
      {
         log_softerror_and_alarm("Received local message to change the search frequency to %s, but no search is in progress!", str_format_frequency(pPH->vehicle_id_dest));
         return;
      }
      log_line("Received local message to change the search frequency to %s", str_format_frequency(pPH->vehicle_id_dest));
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
         radio_rx_pause_interface(i, "Controller search freq changed");
      log_line("Paused radio rx interfaces.");
      links_set_cards_frequencies_for_search((int)pPH->vehicle_id_dest, false, -1,-1,-1,-1 );
      hardware_save_radio_info();
      g_uSearchFrequency = pPH->vehicle_id_dest;
      log_line("Switched search frequency to %s. Broadcasting that router is ready.", str_format_frequency(pPH->vehicle_id_dest));
      broadcast_router_ready();
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
         radio_rx_resume_interface(i);
      log_line("Resumed radio rx interfaces.");
      radio_stats_reset_interfaces_rx_info(&g_SM_RadioStats, "Changed search frequency");
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_LINK_FREQUENCY_CHANGED )
   {
      u32* pI = (u32*)(((u8*)(pPH))+sizeof(t_packet_header));
      u32 uLinkId = *pI;
      pI++;
      u32 uNewFreq = *pI;
               
      log_line("Received control packet from central that a vehicle radio link frequency changed (radio link %u new freq: %s)", uLinkId+1, str_format_frequency(uNewFreq));
      if ( NULL != g_pCurrentModel && (uLinkId < (u32)g_pCurrentModel->radioLinksParams.links_count) )
      {
         g_pCurrentModel->radioLinksParams.link_frequency_khz[uLinkId] = uNewFreq;
         for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         {
            if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == (int)uLinkId )
               g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[i] = uNewFreq;
         }
         links_set_cards_frequencies_and_params((int)uLinkId);
         discardRetransmissionsInfoAndBuffersOnLengthyOp();
      }
      hardware_save_radio_info();
            
      log_line("Done processing control packet of frequency change.");
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_PASSPHRASE_CHANGED )
   {
      lpp(NULL,0);
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_SWITCH_RADIO_LINK )
   {
      int iVehicleRadioLinkId = (int) pPH->vehicle_id_dest;
      log_line("Received control message to switch to vehicle radio link %d", iVehicleRadioLinkId+1);

      pPH->vehicle_id_src = iVehicleRadioLinkId;
      if ( _switch_to_vehicle_radio_link(iVehicleRadioLinkId) )
         pPH->vehicle_id_dest = 1;
      else
         pPH->vehicle_id_dest = 0;
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

      log_line("Switching to vehicle radio link %d %s. Sending response to central.", iVehicleRadioLinkId+1, (pPH->vehicle_id_dest == 1)?"succeeded":"failed");
      if ( ! ruby_ipc_channel_send_message(g_fIPCToCentral, (u8*)pPH, pPH->total_length) )
         log_ipc_send_central_error((u8*)pPH, pPH->total_length);
      else
         log_line("Sent response to central.");
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_RELAY_MODE_SWITCHED )
   {
      g_pCurrentModel->relay_params.uCurrentRelayMode = pPH->vehicle_id_src;

      log_line("Received notification from central that relay mode changed to %d (%s)", g_pCurrentModel->relay_params.uCurrentRelayMode, str_format_relay_mode(g_pCurrentModel->relay_params.uCurrentRelayMode));
      int iCurrentFPS = g_pCurrentModel->video_params.iVideoFPS;
      int iDefaultKeyframeIntervalMs = DEFAULT_VIDEO_MIN_AUTO_KEYFRAME_INTERVAL;
      if ( ! relay_controller_must_display_main_video(g_pCurrentModel) )
         iDefaultKeyframeIntervalMs = DEFAULT_VIDEO_KEYFRAME_INTERVAL_WHEN_RELAYING;
      log_line("Set current keyframe to %d ms for FPS %d for current vehicle (VID: %u)", iDefaultKeyframeIntervalMs, iCurrentFPS, g_pCurrentModel->uVehicleId);
      // To fix video_link_keyframe_set_current_level_to_request(g_pCurrentModel->uVehicleId, iDefaultKeyframeIntervalMs);
      
      Model* pRelayedModel = findModelWithId(g_pCurrentModel->relay_params.uRelayedVehicleId, 102);
      if ( NULL != pRelayedModel )
      {
         iDefaultKeyframeIntervalMs = DEFAULT_VIDEO_MIN_AUTO_KEYFRAME_INTERVAL;
         if ( ! relay_controller_must_display_remote_video(g_pCurrentModel) )
            iDefaultKeyframeIntervalMs = DEFAULT_VIDEO_KEYFRAME_INTERVAL_WHEN_RELAYING;
         log_line("Set current keyframe to %d ms for remote vehicle (VID: %u)", iDefaultKeyframeIntervalMs, pRelayedModel->uVehicleId);
         // To fix video_link_keyframe_set_current_level_to_request(pModel->uVehicleId, iDefaultKeyframeIntervalMs);
      }

      g_iDebugShowKeyFramesAfterRelaySwitch = 5;
      log_line("Done processing notification from central that relay mode changed to %d (%s)", g_pCurrentModel->relay_params.uCurrentRelayMode, str_format_relay_mode(g_pCurrentModel->relay_params.uCurrentRelayMode));
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED )
   {
      u8 uComponent = (pPH->vehicle_id_src) && 0xFF;
      u8 uChangeType = (pPH->vehicle_id_src >> 8) & 0xFF;
      u8 uExtraParam = (pPH->vehicle_id_src >> 16) & 0xFF;
      log_line("Received local control packet from central to reload model, change type: %u (%s), extra value: %d.", uChangeType, str_get_model_change_type((int)uChangeType), uExtraParam);

      _process_local_notification_model_changed(pPH, uChangeType, (int)uComponent, (int)uExtraParam);
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED )
   {
      log_line("Received control packet to reload controller settings.");
      if ( pPH->vehicle_id_src == 0xFF )
      {
         log_line("Notification to enabled/disable debug stats.");
         load_ControllerSettings();
         g_pControllerSettings = get_ControllerSettings();
         return;
      }
      g_pControllerSettings = get_ControllerSettings();
      int iOldTxMode = g_pControllerSettings->iRadioTxUsesPPCAP;
      int iOldSocketBuffers = g_pControllerSettings->iRadioBypassSocketBuffers;
      int iOldThreadPriorityRouter = g_pControllerSettings->iThreadPriorityRouter;

      #if defined (HW_PLATFORM_RADXA)
      int iOldHDMIVSync = g_pControllerSettings->iHDMIVSync;
      #endif
      hw_serial_port_info_t oldSerialPorts[MAX_SERIAL_PORTS];
      for( int i=0; i<hardware_serial_get_ports_count(); i++ )
      {
         hw_serial_port_info_t* pPort = hardware_get_serial_port_info(i);
         if ( NULL != pPort )
            memcpy(&oldSerialPorts[i], pPort, sizeof(hw_serial_port_info_t));
      }
     
      hardware_serial_reload_ports_settings();
      load_ControllerSettings();
      g_pControllerSettings = get_ControllerSettings();

      #if defined (HW_PLATFORM_RADXA)
      if ( g_pControllerSettings->iHDMIVSync != iOldHDMIVSync )
      {
         g_TimeNow = get_current_timestamp_ms();
         g_TimeLastVideoParametersOrProfileChanged = g_TimeNow;
         rx_video_output_signal_restart_streamer();
         return;
      }
      #endif

      if ( g_pControllerSettings->iRadioTxUsesPPCAP != iOldTxMode )
      {
         log_line("Radio Tx mode (PPCAP/Socket) changed. Reinit radio interfaces...");
         reasign_radio_links(true);
      }
      if ( g_pControllerSettings->iRadioBypassSocketBuffers != iOldSocketBuffers )
      {
         log_line("Radio bypass socket buffers changed. Reinit radio interfaces...");
         reasign_radio_links(true);       
      }

      if ( NULL != g_pControllerSettings )
         radio_rx_set_timeout_interval(g_pControllerSettings->iDevRxLoopTimeout);

      for( int i=0; i<hardware_serial_get_ports_count(); i++ )
      {
         hw_serial_port_info_t* pPort = hardware_get_serial_port_info(i);
         if ( NULL == pPort )
            continue;

         if ( pPort->iPortUsage == SERIAL_PORT_USAGE_SIK_RADIO )
         if ( pPort->lPortSpeed != oldSerialPorts[i].lPortSpeed )
         {
            log_line("SiK radio interface changed local serial port speed from %d bps to %d bps",
               oldSerialPorts[i].lPortSpeed, pPort->lPortSpeed );

            if ( g_SiKRadiosState.bConfiguringToolInProgress )
            {
               log_softerror_and_alarm("Another SiK configuration is in progress. Ignoring this one.");
               continue;
            }

            if ( g_SiKRadiosState.bConfiguringSiKThreadWorking )
            {
               log_softerror_and_alarm("SiK reinitialization thread is in progress. Ignoring this notification.");
               continue;
            }
            if ( g_SiKRadiosState.bMustReinitSiKInterfaces )
            {
               log_softerror_and_alarm("SiK reinitialization is already flagged. Ignoring this notification.");
               continue;
            }

            radio_links_close_and_mark_sik_interfaces_to_reopen();
            g_SiKRadiosState.bConfiguringToolInProgress = true;
            g_SiKRadiosState.uTimeStartConfiguring = g_TimeNow;
            
            send_alarm_to_central(ALARM_ID_GENERIC_STATUS_UPDATE, ALARM_FLAG_GENERIC_STATUS_RECONFIGURING_RADIO_INTERFACE, 0);

            char szCommand[128];
            sprintf(szCommand, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_SIK_CONFIG_FINISHED);
            hw_execute_bash_command(szCommand, NULL);

            sprintf(szCommand, "./ruby_sik_config %s %d -serialspeed %d &", pPort->szPortDeviceName, (int)oldSerialPorts[i].lPortSpeed, (int)pPort->lPortSpeed);
            hw_execute_bash_command(szCommand, NULL);
         }
      }

      g_TimeNow = get_current_timestamp_ms();
      
      if ( NULL != g_pControllerSettings )
      {
         log_line("Set new radio rx/tx threads raw priorities: %d/%d (adjustments enabled: %d)", g_pControllerSettings->iThreadPriorityRadioRx, g_pControllerSettings->iThreadPriorityRadioTx, g_pControllerSettings->iPrioritiesAdjustment);
         if ( g_pControllerSettings->iPrioritiesAdjustment )
         {
           radio_rx_set_custom_thread_raw_priority(g_pControllerSettings->iThreadPriorityRadioRx);
           radio_tx_set_custom_thread_raw_priority(g_pControllerSettings->iThreadPriorityRadioTx);
           if ( iOldThreadPriorityRouter != g_pControllerSettings->iThreadPriorityRouter )
              hw_set_current_thread_raw_priority("[router]", g_pControllerSettings->iThreadPriorityRouter);
         }
      }

      if ( NULL != g_pSM_RadioStats )
         memcpy((u8*)g_pSM_RadioStats, (u8*)&g_SM_RadioStats, sizeof(shared_mem_radio_stats));

      if ( g_pCurrentModel->hasCamera() )
         rx_video_output_on_controller_settings_changed();

      for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
      {
         if ( NULL == g_pVideoProcessorRxList[i] )
            break;
         g_pVideoProcessorRxList[i]->onControllerSettingsChanged();
      }
      
      // Signal other components about the model change
      if ( pPH->vehicle_id_src == PACKET_COMPONENT_LOCAL_CONTROL )
      {
         if ( -1 != g_fIPCToTelemetry )
            ruby_ipc_channel_send_message(g_fIPCToTelemetry, (u8*)pPH, pPH->total_length);
         if ( -1 != g_fIPCToRC )
            ruby_ipc_channel_send_message(g_fIPCToRC, (u8*)pPH, pPH->total_length);
      }

      g_TimeNow = get_current_timestamp_ms();
      g_TimeLastVideoParametersOrProfileChanged = g_TimeNow;
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_STARTED ||
        pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED ||
        pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_FINISHED )
   {
      if ( -1 != g_fIPCToTelemetry )
         ruby_ipc_channel_send_message(g_fIPCToTelemetry, (u8*)pPH, pPH->total_length);
      if ( -1 != g_fIPCToRC )
         ruby_ipc_channel_send_message(g_fIPCToRC, (u8*)pPH, pPH->total_length);

      if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_STARTED )
         g_bUpdateInProgress = true;
      if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED ||
           pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_FINISHED )
         g_bUpdateInProgress = false;
      return;
   }
}
