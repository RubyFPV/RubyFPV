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
#include "../base/encr.h"
#include "../base/models_list.h"
#include "../base/shared_mem.h"
#include "../base/ruby_ipc.h"
#include "../base/commands.h"
#include "../base/hardware_procs.h"
#include "../base/hardware_camera.h"
#include "../base/hardware_radio.h"
#include "../base/hardware_radio_sik.h"
#include "../base/hardware_serial.h"
#include "../base/vehicle_settings.h"
#include "../base/radio_utils.h"
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "process_local_packets.h"
#include "processor_tx_video.h"

#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"
#include "../radio/radiopacketsqueue.h"
#include "../radio/fec.h"
#include "../radio/radio_rx.h"
#include "../radio/radio_tx.h"
#include "../radio/radio_duplicate_det.h"
#include "packets_utils.h"
#include "shared_vars.h"
#include "timers.h"
#include "ruby_rt_vehicle.h"
#include "../utils/utils_vehicle.h"
#include "launchers_vehicle.h"
#include "radio_links.h"
#include "processor_tx_video.h"
#include "processor_relay.h"
#include "process_cam_params.h"
#include "events.h"
#include "adaptive_video.h"
#include "video_sources.h"
#include "radio_links.h"

u32 _get_previous_frequency_switch(int nLink)
{
   u32 nCurrentFreq = g_pCurrentModel->radioLinksParams.link_frequency_khz[nLink];
   u32 nNewFreq = nCurrentFreq;

   int nBand = getBand(nCurrentFreq);
   int nChannel = getChannelIndexForFrequency(nBand,nCurrentFreq);

   int maxLoop = getChannels23Count() + getChannels24Count() + getChannels25Count() + getChannels58Count();

   while ( maxLoop > 0 )
   {
      maxLoop--;
      nChannel--;
      if ( nChannel < 0 )
      {
         if ( nBand == RADIO_HW_SUPPORTED_BAND_23 )
         {
            nBand = RADIO_HW_SUPPORTED_BAND_58;
            nChannel = getChannels58Count();
            continue;
         }
         if ( nBand == RADIO_HW_SUPPORTED_BAND_24 )
         {
            nBand = RADIO_HW_SUPPORTED_BAND_23;
            nChannel = getChannels23Count();
            continue;
         }
         if ( nBand == RADIO_HW_SUPPORTED_BAND_25 )
         {
            nBand = RADIO_HW_SUPPORTED_BAND_24;
            nChannel = getChannels24Count();
            continue;
         }
         if ( nBand == RADIO_HW_SUPPORTED_BAND_58 )
         {
            nBand = RADIO_HW_SUPPORTED_BAND_25;
            nChannel = getChannels25Count();
            continue;
         }
         continue;
      }
      if ( nBand == RADIO_HW_SUPPORTED_BAND_23 )
      if ( g_pCurrentModel->functions_params.uChannels23FreqSwitch[nLink] & (((u32)0x01)<<nChannel) )
      {
         nNewFreq = getChannels23()[nChannel];
         return nNewFreq;
      }
      if ( nBand == RADIO_HW_SUPPORTED_BAND_24 )
      if ( g_pCurrentModel->functions_params.uChannels24FreqSwitch[nLink] & (((u32)0x01)<<nChannel) )
      {
         nNewFreq = getChannels24()[nChannel];
         return nNewFreq;
      }
      if ( nBand == RADIO_HW_SUPPORTED_BAND_25 )
      if ( g_pCurrentModel->functions_params.uChannels25FreqSwitch[nLink] & (((u32)0x01)<<nChannel) )
      {
         nNewFreq = getChannels25()[nChannel];
         return nNewFreq;
      }
      if ( nBand == RADIO_HW_SUPPORTED_BAND_58 )
      if ( g_pCurrentModel->functions_params.uChannels58FreqSwitch[nLink] & (((u32)0x01)<<nChannel) )
      {
         nNewFreq = getChannels58()[nChannel];
         return nNewFreq;
      }
   }
   return nNewFreq;   
}


u32 _get_next_frequency_switch(int nLink)
{
   u32 nCurrentFreq = g_pCurrentModel->radioLinksParams.link_frequency_khz[nLink];
   u32 nNewFreq = nCurrentFreq;

   int nBand = getBand(nCurrentFreq);
   int nChannel = getChannelIndexForFrequency(nBand,nCurrentFreq);
   int nCurrentBandChannelsCount = 0;

   if ( nBand == RADIO_HW_SUPPORTED_BAND_23 )
      for( int i=0; i<getChannels23Count(); i++ )
         if ( nCurrentFreq == getChannels23()[i] )
         {
            nChannel = i;
            nCurrentBandChannelsCount = getChannels23Count();
            break;
         }

   if ( nBand == RADIO_HW_SUPPORTED_BAND_24 )
      for( int i=0; i<getChannels24Count(); i++ )
         if ( nCurrentFreq == getChannels24()[i] )
         {
            nChannel = i;
            nCurrentBandChannelsCount = getChannels24Count();
            break;
         }

   if ( nBand == RADIO_HW_SUPPORTED_BAND_25 )
      for( int i=0; i<getChannels25Count(); i++ )
         if ( nCurrentFreq == getChannels25()[i] )
         {
            nChannel = i;
            nCurrentBandChannelsCount = getChannels25Count();
            break;
         }

   if ( nBand == RADIO_HW_SUPPORTED_BAND_58 )
      for( int i=0; i<getChannels58Count(); i++ )
         if ( nCurrentFreq == getChannels58()[i] )
         {
            nChannel = i;
            nCurrentBandChannelsCount = getChannels58Count();
            break;
         }

   int maxLoop = getChannels23Count() + getChannels24Count() + getChannels25Count() + getChannels58Count();

   while ( maxLoop > 0 )
   {
      maxLoop--;
      nChannel++;
      if ( nChannel >= nCurrentBandChannelsCount )
      {
         if ( nBand == RADIO_HW_SUPPORTED_BAND_23 )
         {
            nBand = RADIO_HW_SUPPORTED_BAND_24;
            nChannel = -1;
            nCurrentBandChannelsCount = getChannels24Count();
            continue;
         }
         
         if ( nBand == RADIO_HW_SUPPORTED_BAND_24 )
         {
            nBand = RADIO_HW_SUPPORTED_BAND_25;
            nChannel = -1;
            nCurrentBandChannelsCount = getChannels25Count();
            continue;
         }

         if ( nBand == RADIO_HW_SUPPORTED_BAND_25 )
         {
            nBand = RADIO_HW_SUPPORTED_BAND_58;
            nChannel = -1;
            nCurrentBandChannelsCount = getChannels58Count();
            continue;
         }

         if ( nBand == RADIO_HW_SUPPORTED_BAND_58 )
         {
            nBand = RADIO_HW_SUPPORTED_BAND_23;
            nChannel = -1;
            nCurrentBandChannelsCount = getChannels23Count();
            continue;
         }
         continue;
      }

      if ( nBand == RADIO_HW_SUPPORTED_BAND_23 )
      if ( g_pCurrentModel->functions_params.uChannels23FreqSwitch[nLink] & (((u32)0x01)<<nChannel) )
      {
         nNewFreq = getChannels23()[nChannel];
         return nNewFreq;
      }
      if ( nBand == RADIO_HW_SUPPORTED_BAND_24 )
      if ( g_pCurrentModel->functions_params.uChannels24FreqSwitch[nLink] & (((u32)0x01)<<nChannel) )
      {
         nNewFreq = getChannels24()[nChannel];
         return nNewFreq;
      }
      if ( nBand == RADIO_HW_SUPPORTED_BAND_25 )
      if ( g_pCurrentModel->functions_params.uChannels25FreqSwitch[nLink] & (((u32)0x01)<<nChannel) )
      {
         nNewFreq = getChannels25()[nChannel];
         return nNewFreq;
      }
      if ( nBand == RADIO_HW_SUPPORTED_BAND_58 )
      if ( g_pCurrentModel->functions_params.uChannels58FreqSwitch[nLink] & (((u32)0x01)<<nChannel) )
      {
         nNewFreq = getChannels58()[nChannel];
         return nNewFreq;
      }
   }
   return nNewFreq;
}

void _process_local_notification_model_changed(t_packet_header* pPH, int changeType, int fromComponentId, int iExtraParam)
{
   log_line("Received local notification to reload model. Change type: %d (%s), from component id: %d (%s)", changeType, str_get_model_change_type(changeType), fromComponentId, str_get_component_id(fromComponentId));
   log_line("Developer mode is now: %s", (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE)?"on":"off");

   if ( (changeType == MODEL_CHANGED_STATS) && (fromComponentId == PACKET_COMPONENT_TELEMETRY) )
   {
      log_line("Received event from telemetry component that model stats where updated. Updating local copy. Signal other components too.");
      char szFile[128];
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
      if ( ! g_pCurrentModel->loadFromFile(szFile, false) )
         log_error_and_alarm("Can't load current model vehicle.");
      ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)pPH, pPH->total_length);
      return;
   }

   if ( changeType == MODEL_CHANGED_SERIAL_PORTS )
   {
      char szFile[128];
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
      if ( ! g_pCurrentModel->loadFromFile(szFile, false) )
         log_error_and_alarm("Can't load current model vehicle.");
      hardware_serial_reload_ports_settings();
      ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)pPH, pPH->total_length);
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      return;
   }

   type_radio_interfaces_parameters oldRadioInterfacesParams;
   type_radio_links_parameters oldRadioLinksParams;
   rc_parameters_t oldRCParams;
   audio_parameters_t oldAudioParams;
   type_relay_parameters oldRelayParams;
   camera_profile_parameters_t oldCameraProfileParams;
   video_parameters_t oldVideoParams;
   type_video_link_profile oldVideoLinkProfiles[MAX_VIDEO_LINK_PROFILES];
   type_processes_priorities oldProcesses;

   memcpy(&oldRadioInterfacesParams, &(g_pCurrentModel->radioInterfacesParams), sizeof(type_radio_interfaces_parameters));
   memcpy(&oldRadioLinksParams, &(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));
   memcpy(&oldRCParams, &(g_pCurrentModel->rc_params), sizeof(rc_parameters_t));
   memcpy(&oldAudioParams, &(g_pCurrentModel->audio_params), sizeof(audio_parameters_t));
   memcpy(&oldRelayParams, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));
   memcpy(&oldCameraProfileParams, &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile]), sizeof(camera_profile_parameters_t));
   memcpy(&oldVideoParams, &(g_pCurrentModel->video_params), sizeof(video_parameters_t));
   memcpy(&(oldVideoLinkProfiles[0]), &(g_pCurrentModel->video_link_profiles[0]), MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile));
   memcpy(&oldProcesses, &g_pCurrentModel->processesPriorities, sizeof(type_processes_priorities));
   
   u32 uOldDevFlags = g_pCurrentModel->uDeveloperFlags;
   u32 old_ef = g_pCurrentModel->enc_flags;
   bool bMustSignalOtherComponents = true;
   int iPreviousRadioGraphsRefreshIntervalMs = g_pCurrentModel->osd_params.iRadioInterfacesGraphRefreshIntervalMs;

   if ( changeType == MODEL_CHANGED_RADIO_POWERS )
   {
      log_line("Tx powers before updating local model:");
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         log_line("Radio interface %d current tx power raw: %d", i+1, g_pCurrentModel->radioInterfacesParams.interface_raw_power[i]);
   }

   // Reload model first

   load_VehicleSettings();

   VehicleSettings* pVS = get_VehicleSettings();
   if ( NULL != pVS )
      radio_rx_set_timeout_interval(pVS->iDevRxLoopTimeout);
     
   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
   if ( ! g_pCurrentModel->loadFromFile(szFile, false) )
      log_error_and_alarm("Can't load current model vehicle.");

   if ( changeType == MODEL_CHANGED_DEVELOPER_FLAGS )
   {
      log_line("Received notification that developer flags changed. Developer mode changed from %s to %s",
        (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE)?"on":"off",
        (uOldDevFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE)?"on":"off");

      log_line("Old developer flags: %s", str_get_developer_flags(uOldDevFlags));
      log_line("New developer flags: %s", str_get_developer_flags(g_pCurrentModel->uDeveloperFlags));
      checkDeveloperFlagsChanges(uOldDevFlags, g_pCurrentModel->uDeveloperFlags);

      ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)pPH, pPH->total_length);
      if ( g_pCurrentModel->rc_params.rc_enabled )
         ruby_ipc_channel_send_message(s_fIPCRouterToRC, (u8*)pPH, pPH->total_length);
      return;
   }
   
   if ( (g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_BYPASS_SOCKETS_BUFFERS) != (oldRadioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_BYPASS_SOCKETS_BUFFERS) )
   {
      log_line("Radio bypass socket buffers changed. Reinit radio interfaces...");
      radio_links_restart(true);
   }

   if ( iPreviousRadioGraphsRefreshIntervalMs != g_pCurrentModel->osd_params.iRadioInterfacesGraphRefreshIntervalMs )
      radio_stats_set_graph_refresh_interval(&g_SM_RadioStats, g_pCurrentModel->osd_params.iRadioInterfacesGraphRefreshIntervalMs);

   if ( changeType == MODEL_CHANGED_VIDEO_PARAMETERS )
   {
      video_sources_on_changed_video_params(&oldCameraProfileParams, &oldVideoParams, &oldVideoLinkProfiles[0]);
      return;
   }

   if ( changeType == MODEL_CHANGED_THREADS_PRIORITIES )
   {
      log_line("Received notification to adjust threads priorities. Restart now? %s", iExtraParam?"yes":"no");
      log_line("Different settings? %s", (0 == memcmp((u8*)&oldProcesses, (u8*)&(g_pCurrentModel->processesPriorities), sizeof(type_processes_priorities)))?"no":"yes");
      log_line("Old thread priorities: router %d, radio rx: %d, radio tx: %d",
         oldProcesses.iThreadPriorityRouter, oldProcesses.iThreadPriorityRadioRx, oldProcesses.iThreadPriorityRadioTx);
      log_line("New thread priorities: router %d, radio rx: %d, radio tx: %d",
         g_pCurrentModel->processesPriorities.iThreadPriorityRouter, g_pCurrentModel->processesPriorities.iThreadPriorityRadioRx, g_pCurrentModel->processesPriorities.iThreadPriorityRadioTx);
      
      if ( iExtraParam )
      {
        log_line("Router and processes are marked for restart. Signal restart all procs and quit it.");

        if ( packets_queue_has_packets(&g_QueueRadioPacketsOut) )
           process_and_send_packets(false);
      
        sem_t* pSem = sem_open(SEMAPHORE_RESTART_VEHICLE_PROCS, O_CREAT, S_IWUSR | S_IRUSR, 0);
        if ( (NULL == pSem) || (SEM_FAILED == pSem) )
           log_softerror_and_alarm("Failed to open semaphore to signal restart: %s", SEMAPHORE_RESTART_VEHICLE_PROCS);
        else
        {
           sem_post(pSem);
           sem_close(pSem);
           log_line("Signaled semaphore to restart all procs.");
        }
      }
      else
      {
         log_line("Set new router raw priority: %d and radio rx/tx threads raw priorities: %d/%d (adjustments enabled: %d)",
            g_pCurrentModel->processesPriorities.iThreadPriorityRouter, g_pCurrentModel->processesPriorities.iThreadPriorityRadioRx, g_pCurrentModel->processesPriorities.iThreadPriorityRadioTx,
            (g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS)?1:0);
         if ( g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS )
         {
           radio_rx_set_custom_thread_raw_priority(g_pCurrentModel->processesPriorities.iThreadPriorityRadioRx);
           radio_tx_set_custom_thread_raw_priority(g_pCurrentModel->processesPriorities.iThreadPriorityRadioTx);
           if ( oldProcesses.iThreadPriorityRouter != g_pCurrentModel->processesPriorities.iThreadPriorityRouter )
              hw_set_current_thread_raw_priority("[router]", g_pCurrentModel->processesPriorities.iThreadPriorityRouter);
         }
      }
      return;
   }

   if ( changeType == MODEL_CHANGED_OVERCLOCKING )
   {
      log_line("Received local notification that overclocking was updated: new ARM freq: %d, new GPU freq: %d, new overvoltage: %d",
         g_pCurrentModel->processesPriorities.iFreqARM,
         g_pCurrentModel->processesPriorities.iFreqGPU,
         g_pCurrentModel->processesPriorities.iOverVoltage);

      video_sources_stop_capture();
      #if defined(HW_PLATFORM_OPENIPC_CAMERA)
      log_line("Setting CPU speed for OpenIPC hardware to %d Mhz...", g_pCurrentModel->processesPriorities.iFreqARM);
      hardware_set_oipc_cpu_freq(g_pCurrentModel->processesPriorities.iFreqARM);
      #endif
      video_sources_start_capture();
      return;
   }

   if ( changeType == MODEL_CHANGED_CONTROLLER_TELEMETRY )
   {
      return;
   }

   if ( changeType == MODEL_CHANGED_SIK_PACKET_SIZE )
   {
      log_line("Received local notification that SiK packet size was changed to %d bytes", g_pCurrentModel->radioLinksParams.iSiKPacketSize);
      radio_tx_set_sik_packet_size(g_pCurrentModel->radioLinksParams.iSiKPacketSize);
      return;
   }

   if ( changeType == MODEL_CHANGED_OSD_PARAMS )
   {
      log_line("Received local notification that model OSD params have been changed.");
   }

   if ( changeType == MODEL_CHANGED_ROTATED_RADIO_LINKS )
   {
      log_line("Received local notification to rotate vehicle radio links.");
      g_pCurrentModel->rotateRadioLinksOrder();
      saveCurrentModel();

      int iTmp = 0;

      // Rotate info in radio stats: rotate radio links ids
      for( int i=0; i<g_SM_RadioStats.countLocalRadioLinks; i++ )
      {
         iTmp = g_SM_RadioStats.radio_links[i].matchingVehicleRadioLinkId;
         g_SM_RadioStats.radio_links[i].lastTxInterfaceIndex = -1;
         if ( g_SM_RadioStats.radio_links[i].matchingVehicleRadioLinkId >= 0 )
         {
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

      ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)pPH, pPH->total_length);
      ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)pPH, pPH->total_length);
      if ( g_pCurrentModel->rc_params.rc_enabled )
         ruby_ipc_channel_send_message(s_fIPCRouterToRC, (u8*)pPH, pPH->total_length);
      return;
   }

   if ( changeType == MODEL_CHANGED_AUDIO_PARAMS )
   {
      log_line("Received notification that audio parameters have changed.");

      if ( oldAudioParams.quality != g_pCurrentModel->audio_params.quality )
      {
         #if defined (HW_PLATFORM_OPENIPC_CAMERA)
         video_source_majestic_clear_audio_buffers();
         hardware_camera_maj_set_audio_quality(4000*(1+g_pCurrentModel->audio_params.quality));
         #endif
      }
      else if ( oldAudioParams.volume != g_pCurrentModel->audio_params.volume )
      {
         #if defined (HW_PLATFORM_OPENIPC_CAMERA)
         video_source_majestic_clear_audio_buffers();
         hardware_camera_maj_set_audio_volume(g_pCurrentModel->audio_params.volume);
         #endif
      }

      if ( g_pCurrentModel->audio_params.has_audio_device && g_pCurrentModel->audio_params.enabled )
      if ( ! vehicle_is_audio_capture_started() )
      {
         vehicle_launch_audio_capture(g_pCurrentModel);
         if ( NULL != g_pProcessorTxAudio )
            g_pProcessorTxAudio->openAudioStream();
      }

      if ( (!g_pCurrentModel->audio_params.has_audio_device) || (!g_pCurrentModel->audio_params.enabled) )
      if ( vehicle_is_audio_capture_started() )
      {
         if ( NULL != g_pProcessorTxAudio )
            g_pProcessorTxAudio->closeAudioStream();
         vehicle_stop_audio_capture(g_pCurrentModel);
      }

      if ( (oldAudioParams.uPacketLength != g_pCurrentModel->audio_params.uPacketLength) ||
           (oldAudioParams.uECScheme != g_pCurrentModel->audio_params.uECScheme) )
      {
         if ( NULL != g_pProcessorTxAudio )
            g_pProcessorTxAudio->resetState(g_pCurrentModel);
      }
      return;
   }

   if ( changeType == MODEL_CHANGED_SWAPED_RADIO_INTERFACES )
   {
      log_line("Received notification to reload model due to radio interfaces swap command.");
      if ( ! g_pCurrentModel->canSwapEnabledHighCapacityRadioInterfaces() )
      {
         log_line("No high capacity radio interfaces that have same capabilities that can be swapped. No swap done.");
         send_alarm_to_controller(ALARM_ID_GENERIC, ALARM_ID_GENERIC_TYPE_SWAP_RADIO_INTERFACES_NOT_POSSIBLE, 0, 10);
         return;
      }
      
      // Swap model radio interfaces and update local radio stats info structures

      if ( ! g_pCurrentModel->swapEnabledHighCapacityRadioInterfaces() )
      {
         log_line("No high capacity radio interfaces that have same capabilities that can be swapped. No swap done.");
         send_alarm_to_controller(ALARM_ID_GENERIC, ALARM_ID_GENERIC_TYPE_SWAP_RADIO_INTERFACES_FAILED, 0, 10);
         return;
      }
      saveCurrentModel();

      int iInterface1 = g_pCurrentModel->getLastSwappedRadioInterface1();
      int iInterface2 = g_pCurrentModel->getLastSwappedRadioInterface2();

      if ( (iInterface1 < 0) || (iInterface1 >= hardware_get_radio_interfaces_count()) )
      {
         log_line("Invalid radio interfaces indexes to swap.");
         send_alarm_to_controller(ALARM_ID_GENERIC, ALARM_ID_GENERIC_TYPE_SWAP_RADIO_INTERFACES_FAILED, 0, 10);
         return;
      }

      if ( (iInterface2 < 0) || (iInterface2 >= hardware_get_radio_interfaces_count()) )
      {
         log_line("Invalid radio interfaces indexes to swap.");
         send_alarm_to_controller(ALARM_ID_GENERIC, ALARM_ID_GENERIC_TYPE_SWAP_RADIO_INTERFACES_FAILED, 0, 10);
         return;
      }

      int iTmp = g_SM_RadioStats.radio_interfaces[iInterface1].assignedVehicleRadioLinkId;
      g_SM_RadioStats.radio_interfaces[iInterface1].assignedVehicleRadioLinkId = g_SM_RadioStats.radio_interfaces[iInterface2].assignedVehicleRadioLinkId;
      g_SM_RadioStats.radio_interfaces[iInterface2].assignedVehicleRadioLinkId = iTmp;

      iTmp = g_SM_RadioStats.radio_interfaces[iInterface1].assignedLocalRadioLinkId;
      g_SM_RadioStats.radio_interfaces[iInterface1].assignedLocalRadioLinkId = g_SM_RadioStats.radio_interfaces[iInterface2].assignedLocalRadioLinkId;
      g_SM_RadioStats.radio_interfaces[iInterface2].assignedLocalRadioLinkId = iTmp;

      int iRadioLinkId = g_pCurrentModel->radioInterfacesParams.interface_link_id[iInterface1];
      if ( (iRadioLinkId >= 0) && (iRadioLinkId < g_pCurrentModel->radioLinksParams.links_count) )
      if ( radio_utils_set_interface_frequency(g_pCurrentModel, iInterface1, iRadioLinkId, g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLinkId], g_pProcessStats, 0) )
         radio_stats_set_card_current_frequency(&g_SM_RadioStats, iInterface1, g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLinkId]);

      iRadioLinkId = g_pCurrentModel->radioInterfacesParams.interface_link_id[iInterface2];
      if ( (iRadioLinkId >= 0) && (iRadioLinkId < g_pCurrentModel->radioLinksParams.links_count) )
      if ( radio_utils_set_interface_frequency(g_pCurrentModel, iInterface2, iRadioLinkId, g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLinkId], g_pProcessStats, 0) )
         radio_stats_set_card_current_frequency(&g_SM_RadioStats, iInterface2, g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLinkId]);

      if ( NULL != g_pProcessStats )
      {
         g_TimeNow = get_current_timestamp_ms();
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }

      ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)pPH, pPH->total_length);
      ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)pPH, pPH->total_length);
      if ( g_pCurrentModel->rc_params.rc_enabled )
         ruby_ipc_channel_send_message(s_fIPCRouterToRC, (u8*)pPH, pPH->total_length);
      
      send_alarm_to_controller(ALARM_ID_GENERIC, ALARM_ID_GENERIC_TYPE_SWAPPED_RADIO_INTERFACES, 0, 10);
      return;
      
      /*
      radio_links_close_rxtx_radio_interfaces();
      if ( NULL != g_pProcessStats )
      {
         g_TimeNow = get_current_timestamp_ms();
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }

      configure_radio_interfaces_for_current_model(g_pCurrentModel, g_pProcessStats);

      radio_links_open_rxtx_radio_interfaces();

      if ( NULL != g_pProcessStats )
      {
         g_TimeNow = get_current_timestamp_ms();
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }
      */
      log_line("Completed reinitializing radio interfaces due to local notification that radio interfaces where swaped.");
      return;
   }

   if ( changeType == MODEL_CHANGED_RELAY_PARAMS )
   {
      log_line("Received notification from commands that relay params or mode changed.");
      log_line("Old relay params: VID: %u, freq: %s, on vehicle's radio link %d, relay flags: (%s)", oldRelayParams.uRelayedVehicleId, str_format_frequency(oldRelayParams.uRelayFrequencyKhz), oldRelayParams.isRelayEnabledOnRadioLinkId, str_format_relay_flags(oldRelayParams.uRelayCapabilitiesFlags));
      log_line("New relay params: VID: %u, freq: %s, on vehicle's radio link %d, relay flags: (%s)", g_pCurrentModel->relay_params.uRelayedVehicleId, str_format_frequency(g_pCurrentModel->relay_params.uRelayFrequencyKhz), g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId, str_format_relay_flags(g_pCurrentModel->relay_params.uRelayCapabilitiesFlags));
      u8 uOldRelayMode = oldRelayParams.uCurrentRelayMode;
      oldRelayParams.uCurrentRelayMode = g_pCurrentModel->relay_params.uCurrentRelayMode;
      
      // Only relay mode changed?

      if ( 0 == memcmp(&oldRelayParams, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters)) )
      {
         if ( uOldRelayMode != g_pCurrentModel->relay_params.uCurrentRelayMode )
            onEventRelayModeChanged(uOldRelayMode, g_pCurrentModel->relay_params.uCurrentRelayMode, "command");
         else
            log_line("No change in relay flags or relay mode detected. Ignoring notification.");
         return;
      }

      oldRelayParams.uCurrentRelayMode = uOldRelayMode;
      
      u32 uOldRelayFlags = oldRelayParams.uRelayCapabilitiesFlags;
      oldRelayParams.uRelayCapabilitiesFlags = g_pCurrentModel->relay_params.uRelayCapabilitiesFlags;

      // Only relay flags changed?
      
      if ( 0 == memcmp(&oldRelayParams, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters)) )
      {
         if ( uOldRelayFlags != g_pCurrentModel->relay_params.uRelayCapabilitiesFlags )
         {
            log_line("Changed relay flags from %u to %u.", uOldRelayFlags, g_pCurrentModel->relay_params.uRelayCapabilitiesFlags);
            relay_on_relay_flags_changed(g_pCurrentModel->relay_params.uRelayCapabilitiesFlags);
         }
         else
            log_line("No change in relay flags or relay mode detected. Ignoring notification.");
         return;
      }
      oldRelayParams.uRelayCapabilitiesFlags = uOldRelayFlags;
      
      
      // Relay link changed?
      if ( oldRelayParams.isRelayEnabledOnRadioLinkId != g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId )
      {
         log_line("Relay radio link has changed. Update relay params and state and signal router to update relay state.");
         relay_on_relay_params_changed();
         radio_links_restart(true);
      }

      // Relayed vehicle ID changed?
      if ( oldRelayParams.uRelayedVehicleId != g_pCurrentModel->relay_params.uRelayedVehicleId )
      {
         log_line("Changed relayed vehicle id, from %u to %u. Update relay params and state and signal router to update relay state.", oldRelayParams.uRelayedVehicleId, g_pCurrentModel->relay_params.uRelayedVehicleId);
         relay_on_relay_params_changed();
         radio_links_restart(true);
      }
      return;
   }

   if ( changeType == MODEL_CHANGED_RADIO_LINK_CAPABILITIES )
   {
      log_line("Received local notification (type %d) that link capabilities flags changed, that do not require additional processing besides reloading the current model.", (int)changeType);
      bMustSignalOtherComponents = false;
      return;
   }

   if ( changeType == MODEL_CHANGED_RADIO_LINK_PARAMS )
   {
      log_line("Received local notification (type %d) that radio link params have changed on model's radio link %d.", (int)changeType, iExtraParam+1);
      bMustSignalOtherComponents = false;
      return;
   }

   if ( changeType == MODEL_CHANGED_RADIO_POWERS )
   {
      log_line("Received local notification that radio powers have changed.");
      log_line("Tx powers after updating local model:");
      int iSikRadioIndexToUpdate = -1;
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         log_line("Radio interface %d current tx power raw: %d (old %d)", i+1, g_pCurrentModel->radioInterfacesParams.interface_raw_power[i], oldRadioInterfacesParams.interface_raw_power[i]);
         if ( hardware_radio_index_is_sik_radio(i) )
         if ( g_pCurrentModel->radioInterfacesParams.interface_raw_power[i] != oldRadioInterfacesParams.interface_raw_power[i] )
            iSikRadioIndexToUpdate = i;
      }

      if ( -1 != iSikRadioIndexToUpdate )
      {
         log_line("SiK radio interface %d tx power was changed from %d to %d. Updating SiK radio interfaces...", 
            iSikRadioIndexToUpdate+1, oldRadioInterfacesParams.interface_raw_power[iSikRadioIndexToUpdate], g_pCurrentModel->radioInterfacesParams.interface_raw_power[iSikRadioIndexToUpdate] );
         
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

         close_and_mark_sik_interfaces_to_reopen();
         g_SiKRadiosState.bConfiguringToolInProgress = true;
         g_SiKRadiosState.uTimeStartConfiguring = g_TimeNow;
         
         send_alarm_to_controller(ALARM_ID_GENERIC_STATUS_UPDATE, ALARM_FLAG_GENERIC_STATUS_RECONFIGURING_RADIO_INTERFACE, 0, 10);

         char szCommand[128];
         sprintf(szCommand, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_SIK_CONFIG_FINISHED);
         hw_execute_bash_command(szCommand, NULL);

         sprintf(szCommand, "./ruby_sik_config none 0 -power %d &", g_pCurrentModel->radioInterfacesParams.interface_raw_power[iSikRadioIndexToUpdate]);
         hw_execute_bash_command(szCommand, NULL);
         return;
      }
   }

   if ( changeType == MODEL_CHANGED_RADIO_DATARATES )
   {
      int iRadioLink = iExtraParam;
      if ( 0 == memcmp((u8*)&oldRadioLinksParams, (u8*)&g_pCurrentModel->radioLinksParams, sizeof(type_radio_links_parameters) ) )
      {
         log_line("Received local notification that radio data rates have changed on radio link %d but no data rate was changed. Ignoring it.", iRadioLink + 1);
         return;
      }
      log_line("Received local notification that radio data rates have changed on radio link %d", iRadioLink+1);
      log_line("New radio data rates for link %d: v: %d, d: %d/%d, u: %d/%d",
            iRadioLink+1, g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[iRadioLink],
            g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[iRadioLink],
            g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[iRadioLink],
            g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[iRadioLink],
            g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[iRadioLink]);

      // If downlink data rate for an Atheros card has changed, update it.
      
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iRadioLink )
            continue;
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
            continue;
         if ( ! hardware_radio_driver_is_atheros_card(pRadioHWInfo->iRadioDriver) )
            continue;
         int nRateTx = g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[iRadioLink];
         update_atheros_card_datarate(g_pCurrentModel, i, nRateTx, g_pProcessStats);
         g_TimeNow = get_current_timestamp_ms();
      }
      return;
   }
   
   if ( changeType == MODEL_CHANGED_SIK_FREQUENCY )
   {
      log_line("Received local notification that a SiK radio frequency was changed. Updating SiK interfaces...");
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( hardware_radio_index_is_sik_radio(i) )
            flag_update_sik_interface(i);
      }
      bMustSignalOtherComponents = false;
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      return;
   }

   if ( changeType == MODEL_CHANGED_RESET_RADIO_LINK )
   {
      int iLink = iExtraParam;
      log_line("Received local notification that radio link %d was reseted.", iLink+1);
      bMustSignalOtherComponents = true;
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      return;
   }

   if ( changeType == MODEL_CHANGED_RADIO_LINK_FRAMES_FLAGS )
   {
      int iLink = iExtraParam;
      log_line("Received local notification that radio flags or datarates changed on radio link %d. Update radio frames type right away.", iLink+1);
      
      u32 radioFlags = g_pCurrentModel->radioLinksParams.link_radio_flags[iLink];
      log_line("Radio link %d new datarates: %d/%d", iLink+1, g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[iLink], g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[iLink]);
      log_line("Radio link %d new radio flags: %s", iLink+1, str_get_radio_frame_flags_description2(radioFlags));
      
      if( g_pCurrentModel->radioLinkIsSiKRadio(iLink) )
      {
         log_line("Radio flags or radio datarates changed on a SiK radio link (link %d).", iLink+1);
         for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         {
            if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == iLink )
            if ( hardware_radio_index_is_sik_radio(i) )
               flag_update_sik_interface(i);
         }
      }
      else if( g_pCurrentModel->radioLinkIsELRSRadio(iLink) )
      {
         log_line("Radio datarates changed on a ELRS radio link (link %d).", iLink+1);
      }
      else
      {
         log_line("Radio link %d is regular 2.4/5.8 link.", iLink+1);
      }

      bMustSignalOtherComponents = false;
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      return;
   }

   if ( changeType == MODEL_CHANGED_RC_PARAMS )
   {
      if ( oldRCParams.rc_enabled != g_pCurrentModel->rc_params.rc_enabled )
      {
         if ( g_pCurrentModel->rc_params.rc_enabled )
         {
            log_line("RC link is enabled. Slow down telemetry packets frequency on slow links.");
            radio_reset_packets_default_frequencies(1);
         }
         else
            radio_reset_packets_default_frequencies(0);
      }

      // If RC was disabled, stop Rx Rc process
      if ( oldRCParams.rc_enabled && (! g_pCurrentModel->rc_params.rc_enabled) )
      {
         vehicle_stop_rx_rc();
         ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)pPH, pPH->total_length);
         return;
      }

      // If RC was enabled, start Rx Rc process
      if ( (! oldRCParams.rc_enabled) && g_pCurrentModel->rc_params.rc_enabled )
      {
         vehicle_launch_rx_rc(g_pCurrentModel);
         ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)pPH, pPH->total_length);
         return;       
      }

      if ( g_pCurrentModel->rc_params.rc_enabled )
         ruby_ipc_channel_send_message(s_fIPCRouterToRC, (u8*)pPH, pPH->total_length);
      ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)pPH, pPH->total_length);

      return;
   }

   if ( old_ef != g_pCurrentModel->enc_flags )
   {
      if ( g_pCurrentModel->enc_flags == MODEL_ENC_FLAGS_NONE )
         rpp();
      else
      {
         if ( ! lpp(NULL, 0) )
         {
            g_pCurrentModel->enc_flags = MODEL_ENC_FLAGS_NONE;
            saveCurrentModel();
         }
      }
      bMustSignalOtherComponents = false;
   }

   // Signal other components about the model change

   if ( bMustSignalOtherComponents )
   {
      if ( fromComponentId == PACKET_COMPONENT_COMMANDS )
      {
         ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)pPH, pPH->total_length);
         if ( g_pCurrentModel->rc_params.rc_enabled )
            ruby_ipc_channel_send_message(s_fIPCRouterToRC, (u8*)pPH, pPH->total_length);
      }
      if ( fromComponentId == PACKET_COMPONENT_TELEMETRY )
      {
         ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)pPH, pPH->total_length);
         if ( g_pCurrentModel->rc_params.rc_enabled )
            ruby_ipc_channel_send_message(s_fIPCRouterToRC, (u8*)pPH, pPH->total_length);
      }
   }

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
}

void process_local_control_packet(t_packet_header* pPH)
{
   if ( NULL != g_pProcessStats )
   {
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }
 
   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SEND_MODEL_SETTINGS )
   {
      log_line("Router received local notification to send model settings to controller.");
      log_line("Tell rx_commands to do it.");

      g_bHasSentVehicleSettingsAtLeastOnce = true;
      
      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SEND_MODEL_SETTINGS, STREAM_ID_DATA);
      PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
      PH.vehicle_id_dest = g_uControllerId;
      PH.total_length = sizeof(t_packet_header);

      ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)&PH, PH.total_length);

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_PAUSE_VIDEO )
   {
      log_line("Received controll message: Video is paused.");
      g_bVideoPaused = true;
   }
   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_RESUME_VIDEO )
   {
      log_line("Received controll message: Video is resumed.");
      g_bVideoPaused = false;
   }


   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_VEHICLE_CALIBRATION_FILE )
   {
      if ( pPH->total_length != (int)(sizeof(t_packet_header) + sizeof(t_packet_header_command_upload_calib_file)) )
      {
         log_softerror_and_alarm("Received invalid size (%d bytes) for camera calibration file segment data, expected %d bytes",
            pPH->total_length, (int)(sizeof(t_packet_header) + sizeof(t_packet_header_command_upload_calib_file)) );
         return;
      }
      u8* pData = (u8*)pPH;
      t_packet_header_command_upload_calib_file* pCalibFileSegment = (t_packet_header_command_upload_calib_file*)(pData + sizeof(t_packet_header));
      bool bValidName = false;
      for( int i=0; i<MAX_CAMERA_BIN_PROFILE_NAME; i++ )
      {
         if ( pCalibFileSegment->szFileName[i] == 0 )
         {
            bValidName = true;
            break;
         }
      }
      if ( ! bValidName )
      {
         if ( 0 != pCalibFileSegment->calibration_file_type )
         {
            log_softerror_and_alarm("Received invalid calibration filename. Reseting it to default.");
            pCalibFileSegment->calibration_file_type = 0;
         }
         pCalibFileSegment->szFileName[0] = 0;
      }
      log_line("Received message to set new camera calibration file: type: %d, filename: [%s]",
         pCalibFileSegment->calibration_file_type,
         pCalibFileSegment->szFileName);

      if ( 0 == pCalibFileSegment->calibration_file_type )
      {
         type_camera_parameters oldCamParams;
         memcpy((u8*)(&oldCamParams), &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), sizeof(type_camera_parameters));
         oldCamParams.szCameraBinProfileName[MAX_CAMERA_BIN_PROFILE_NAME-1] = 0;

         g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraBinProfile = 0;
         g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName[0] = 0;
         saveCurrentModel();

         log_line("Notify other processes to reload model (reset calibration file)");
         t_packet_header PH;
         radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED, STREAM_ID_DATA);
         PH.vehicle_id_src = PACKET_COMPONENT_COMMANDS | (MODEL_CHANGED_CAMERA_CALIBRATION_FILE<<8);
         PH.total_length = sizeof(t_packet_header);

         ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)&PH, PH.total_length);
         if ( g_pCurrentModel->rc_params.rc_enabled )
            ruby_ipc_channel_send_message(s_fIPCRouterToRC, (u8*)&PH, PH.total_length);

         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

         if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
            video_sources_on_changed_camera_params(&(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), &oldCamParams);
         return;
      }
      else if ( pCalibFileSegment->is_last_segment )
      {
         type_camera_parameters oldCamParams;
         memcpy((u8*)(&oldCamParams), &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), sizeof(type_camera_parameters));
         oldCamParams.szCameraBinProfileName[MAX_CAMERA_BIN_PROFILE_NAME-1] = 0;

         g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraBinProfile = pCalibFileSegment->calibration_file_type;
         strncpy(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName, pCalibFileSegment->szFileName, MAX_CAMERA_BIN_PROFILE_NAME);
         g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName[MAX_CAMERA_BIN_PROFILE_NAME-1] = 0;
         saveCurrentModel();

         log_line("Notify other processes to reload model (calibration file uploaded)");
         t_packet_header PH;
         radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED, STREAM_ID_DATA);
         PH.vehicle_id_src = PACKET_COMPONENT_COMMANDS | (MODEL_CHANGED_CAMERA_CALIBRATION_FILE<<8);
         PH.total_length = sizeof(t_packet_header);

         ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)&PH, PH.total_length);
         if ( g_pCurrentModel->rc_params.rc_enabled )
            ruby_ipc_channel_send_message(s_fIPCRouterToRC, (u8*)&PH, PH.total_length);

         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

         if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
            video_sources_on_changed_camera_params(&(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), &oldCamParams);
         return;
      }
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SET_SIK_RADIO_SERIAL_SPEED )
   {
      int iRadioIndex = (int) pPH->vehicle_id_src;
      int iBaudRate = (int) pPH->vehicle_id_dest;
      log_line("Received local message to change Sik Radio (radio interface %d) local baud rate (Must communicate at: %d bps).", iRadioIndex+1, iBaudRate);

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioIndex);
      if ( NULL == pRadioHWInfo || (!hardware_radio_is_sik_radio(pRadioHWInfo)) )
      {
         log_softerror_and_alarm("Radio interface %d is not a SiK radio.", iRadioIndex+1);
         return;
      }
      hw_serial_port_info_t* pSerialPort = hardware_get_serial_port_info_from_serial_port_name(pRadioHWInfo->szDriver);
      if ( NULL == pSerialPort )
      {
         log_softerror_and_alarm("Failed to get serial port hardware info for serial port [%s].", pRadioHWInfo->szDriver);
         return;
      }
      log_line("New serial port [%s] baudrate: %ld bps", pRadioHWInfo->szDriver, pSerialPort->lPortSpeed);

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

      close_and_mark_sik_interfaces_to_reopen();
      g_SiKRadiosState.bConfiguringToolInProgress = true;
      g_SiKRadiosState.uTimeStartConfiguring = g_TimeNow;
      
      send_alarm_to_controller(ALARM_ID_GENERIC_STATUS_UPDATE, ALARM_FLAG_GENERIC_STATUS_RECONFIGURING_RADIO_INTERFACE, 0, 10);

      char szCommand[128];
      sprintf(szCommand, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_SIK_CONFIG_FINISHED);
      hw_execute_bash_command(szCommand, NULL);

      sprintf(szCommand, "./ruby_sik_config %s %d -serialspeed %d &", pRadioHWInfo->szDriver, iBaudRate, (int)pSerialPort->lPortSpeed);
      hw_execute_bash_command(szCommand, NULL);
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_VEHICLE_APPLY_ALL_VIDEO_SETTINGS )
   {
      log_line("Received controll message to apply all video settings.");
      video_sources_apply_all_parameters();
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_FORCE_CAMERA_TYPE )
   {
      log_line("Received controll message that camera type was forced to a value. Parameter: %u", pPH->vehicle_id_dest);
      video_sources_stop_capture();

      if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
      {
         char szSensor[64];
         szSensor[0] = 0;
         switch( g_pCurrentModel->getActiveCameraType() )
         {
            case CAMERA_TYPE_OPENIPC_IMX307: strcpy(szSensor, "imx307"); break;
            case CAMERA_TYPE_OPENIPC_IMX335: strcpy(szSensor, "imx335"); break;
            case CAMERA_TYPE_OPENIPC_IMX415: strcpy(szSensor, "imx415"); break;
         }
         char szComm[256];
         sprintf(szComm, "fw_setenv sensor %s", szSensor);
         hw_execute_bash_command(szComm, NULL);

         //if ( 0 == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraBinProfile )
         hardware_camera_set_default_oipc_calibration(g_pCurrentModel->getActiveCameraType());
         hardware_reboot();
         return;
      }

      if ( g_pCurrentModel->hasCamera() )
      if ( g_pCurrentModel->isActiveCameraHDMI() )
         hardware_sleep_ms(800);
      video_sources_start_capture();
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED )
   {
      u8 fromComponentId = (pPH->vehicle_id_src & 0xFF);
      u8 changeType = (pPH->vehicle_id_src >> 8 ) & 0xFF;
      u8 uExtraParam = (pPH->vehicle_id_src >> 16 ) & 0xFF;
      _process_local_notification_model_changed(pPH, (int)changeType, (int)fromComponentId, (int)uExtraParam);
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_REINITIALIZE_RADIO_LINKS )
   {
      log_line("Received local request to reinitialize radio interfaces from a controller command...");

      char szFile[128];
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
      if ( ! g_pCurrentModel->loadFromFile(szFile, false) )
         log_error_and_alarm("Can't load current model vehicle.");
      if ( NULL != g_pProcessStats )
      {
         g_TimeNow = get_current_timestamp_ms();
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }

      radio_links_restart(true);
      log_line("Completed signaling async reinitialization of radio interfaces due to local request from a controller command.");
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_REBOOT )
   {
      log_line("Received reboot request command. Save model too? %s", (pPH->vehicle_id_dest != 0)?"yes":"no");
      // Signal other components about the reboot request
      if ( pPH->vehicle_id_src == PACKET_COMPONENT_COMMANDS )
      {
         log_line("Received reboot request from RX Commands. Sending it to telemetry.");
         adaptive_video_save_state();
         ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)pPH, pPH->total_length);
         g_uTimeRequestedReboot = g_TimeNow;
      }
      else
         log_line("Received reboot request. Do nothing.");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SET_CAMERA_PARAMS )
   {
      log_line("Router received message to update camera params.");
      process_camera_params_changed((u8*)pPH, pPH->total_length);
      
      ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)pPH, pPH->total_length);
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_BROADCAST_VEHICLE_STATS )
   {
      memcpy((u8*)(&(g_pCurrentModel->m_Stats)),(u8*)(((u8*)pPH) + sizeof(t_packet_header)), sizeof(type_vehicle_stats_info));
      // Signal other components about the vehicle stats
      if ( pPH->vehicle_id_src == PACKET_COMPONENT_TELEMETRY )
         ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)pPH, pPH->total_length);

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_APPLY_SIK_PARAMS )
   {
      log_line("Received local request to apply all SiK radio parameters.");

   }
}


