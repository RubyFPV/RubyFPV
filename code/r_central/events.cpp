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
#include "../base/hardware.h"
#include "../base/hardware_audio.h"
#include "../base/plugins_settings.h"
#include "../base/ctrl_preferences.h"
#include "../radio/radiolink.h"
#include "../common/string_utils.h"
#include "../common/strings_loc.h"
#include "../utils/utils_controller.h"
#include <ctype.h>
#include "events.h"
#include "shared_vars.h"
#include "timers.h"
#include "local_stats.h"
#include "ruby_central.h"
#include "pairing.h"
#include "notifications.h"
#include "handle_commands.h"
#include "osd_common.h"
#include "osd.h"
#include "osd_stats.h"
#include "osd_warnings.h"
#include "osd_widgets.h"
#include "link_watch.h"
#include "ui_alarms.h"
#include "warnings.h"
#include "colors.h"
#include "menu.h"
#include "menu_update_vehicle.h"
#include "menu_confirmation.h"
#include "menu_confirmation_vehicle_board.h"
#include "menu_negociate_radio.h"
#include "menu_vehicle_simplesetup.h"
#include "process_router_messages.h"
#include "fonts.h"

static Model* s_pEventsLastRecvModelSettings = NULL;
static u32 s_uTimeToCheckNextPairingUIAction = 0;
static bool s_bEventTookPairingUIAction = false;
static bool s_bEventTookPairingUIActionUpdateVehicle = false;
static bool s_bEventTookPairingUIActionUpdateController = false;
static bool s_bEventTookPairingUIActionEncoderType = false;
static bool s_bEventTookPairingUIActionEncoderSlices = false;
static bool s_bEventTookPairingUIActionEncoderSpikes = false;
static bool s_bEventTookPairingUIActionNegociateRadio = false;
static bool s_bEventTookPairingUIActionUnitType = false;
static bool s_bEventTookPairingUIActionSimpleSetup = false;

void onModelAdded(u32 uModelId)
{
   log_line("[Events] Handling event new model added (vehicle UID: %u).", uModelId);
   osd_widgets_on_new_vehicle_added_to_controller(uModelId);
   Model* pModel = findModelWithId(uModelId, 115);
   if ( NULL != pModel )
      log_line("The newly added model controller's id: %u (this controller: %u), has negociated radio? %s",
         pModel->uControllerId, g_uControllerId, (pModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS)?"yes":"no");

   if ( NULL != g_pCurrentModel )
      log_line("Current model (VID: %u) has negocated radio? %s",
         g_pCurrentModel->uVehicleId, (g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS)?"yes":"no");

   log_line("[Events] Handled event new model added (vehicle UID: %u). Done.", uModelId);
}

void onModelDeleted(u32 uModelId)
{
   log_line("[Events] Handling event model deleted (vehicle UID: %u).", uModelId);
   deletePluginModelSettings(uModelId);
   save_PluginsSettings();
   g_bGotStatsVideoBitrate = false;
   g_bGotStatsVehicleTx = false;
   log_line("[Events] Handled event model deleted (vehicle UID: %u). Done.", uModelId);
}


void onMainVehicleChanged(bool bRemovePreviousVehicleState)
{
   log_line("[Events] Handling event Main Vehicle Changed...");

   if ( NULL != g_pPopupVideoOverloadAlarm )
   {
      if ( popups_has_popup(g_pPopupVideoOverloadAlarm) )
         popups_remove(g_pPopupVideoOverloadAlarm);
      g_pPopupVideoOverloadAlarm = NULL;
   }

   link_watch_reset();
   if ( NULL != g_pCurrentModel )
      vehicle_runtime_reset_has_received_fc_telemetry_info(g_pCurrentModel->uVehicleId);

   // Remove all the custom popups (above), before removing all popups (so that pointers to custom popups are invalidated beforehand);
   static bool s_bEventsFirstTimeMainVehicleChanged = true;

   if ( ! s_bEventsFirstTimeMainVehicleChanged )
   {
      popups_remove_all();
      menu_discard_all();
      warnings_remove_all();
      osd_warnings_reset();
   }

   s_bEventsFirstTimeMainVehicleChanged = false;
   g_bIsFirstConnectionToCurrentVehicle = true;
   
   if ( bRemovePreviousVehicleState )
      shared_vars_state_reset_all_vehicles_runtime_info();
   
   if ( NULL == g_pCurrentModel )
      log_softerror_and_alarm("[Events] New main vehicle is NULL.");
         
   //g_nTotalControllerCPUSpikes = 0;

   if ( bRemovePreviousVehicleState )
      local_stats_reset_all();

   osd_stats_init();
   
   handle_commands_reset_has_received_vehicle_core_plugins_info();

   g_uPersistentAllAlarmsVehicle = 0;
   g_uTimeLastRadioLinkOverloadAlarm = 0;
   g_bHasVideoDataOverloadAlarm = false;
   g_bHasVideoTxOverloadAlarm = false;
   g_iVehicleCorePluginsCount = 0;
   g_bChangedOSDStatsFontSize = false;
   g_bFreezeOSD = false;

   if ( NULL != g_pCurrentModel )
   {
      u32 scale = g_pCurrentModel->osd_params.osd_preferences[g_pCurrentModel->osd_params.iCurrentOSDScreen] & 0xFF;
      osd_setScaleOSD((int)scale);
   
      u32 scaleStats = (g_pCurrentModel->osd_params.osd_preferences[g_pCurrentModel->osd_params.iCurrentOSDScreen]>>16) & 0x0F;
      osd_setScaleOSDStats((int)scaleStats);

      if ( render_engine_uses_raw_fonts() )
         loadAllFonts(false);

      log_line("Vehicle radio links: %d", g_pCurrentModel->radioLinksParams.links_count );
      for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
      {
         char szBuff[128];
         str_get_radio_frame_flags_description(g_pCurrentModel->radioLinksParams.link_radio_flags[i], szBuff);
         log_line("Vechile radio link %d radio flags: %s", i+1, szBuff);
      }
   }

   for ( int i=0; i<g_iPluginsOSDCount; i++ )
   {
      if ( NULL == g_pPluginsOSD[i] )
         continue;
      if ( NULL != g_pPluginsOSD[i]->pFunctionOnNewVehicle )
         (*(g_pPluginsOSD[i]->pFunctionOnNewVehicle))(g_pCurrentModel->uVehicleId);
   }
   osd_widgets_on_main_vehicle_changed(g_pCurrentModel->uVehicleId);
   
   warnings_on_changed_vehicle();

   render_all(g_TimeNow);

   log_line("[Events] Handled event Main Vehicle Changed. Done.");
}

void onEventReboot()
{
   log_line("[Events] Handling event Reboot...");
   ruby_pause_watchdog("rebooting");
   bool bMigratedFirmware = false;
   if ( g_bDidAnUpdate )
   {
      char szFile[MAX_FILE_PATH_SIZE];
      strcpy(szFile, FOLDER_BINARIES);
      strcat(szFile, "ruby_start");
      if ( access(szFile, R_OK) != -1 )
      {
         strcpy(szFile, FOLDER_BINARIES);
         strcat(szFile, "onyxfpv_start");
         if ( access(szFile, R_OK) != -1 )
            bMigratedFirmware = true;
      }
   }
   if ( ! bMigratedFirmware )
      save_temp_local_stats();
   hardware_sleep_ms(50);
   //pairing_stop();

   log_line("[Events] Handled event Reboot. Done.");
}

void onEventBeforePairing()
{
   log_line("[Events] Handling event BeforePairing...");

   log_current_runtime_vehicles_info();

   onEventPairingDiscardAllUIActions();
   
   notification_add_start_pairing();

   if ( NULL != g_pCurrentModel )
      osd_set_current_layout_index_and_source_model(g_pCurrentModel, g_pCurrentModel->osd_params.iCurrentOSDScreen);
   else
      osd_set_current_layout_index_and_source_model(NULL, 0);

   osd_set_current_data_source_vehicle_index(0);
   shared_vars_osd_reset_before_pairing();

   g_iCurrentActiveVehicleRuntimeInfoIndex = 0;

   g_uTotalLocalAlarmDevRetransmissions = 0;
   g_bHasVideoDataOverloadAlarm = false;
   g_bHasVideoTxOverloadAlarm = false;
   g_bIsTestingAdaptiveVideo = false;

   g_bDidAnUpdate = false;
   g_nSucceededOTAUpdates = 0;
   g_bLinkWizardAfterUpdate = false;

   g_bGotStatsVideoBitrate = false;
   g_bGotStatsVehicleTx = false;
   g_bFreezeOSD = false;

   g_CurrentUploadingFile.uFileId = 0;
   g_CurrentUploadingFile.uTotalSegments = 0;
   g_CurrentUploadingFile.szFileName[0] = 0;
   for( int i=0; i<MAX_SEGMENTS_FILE_UPLOAD; i++ )
   {
      g_CurrentUploadingFile.pSegments[i] = NULL;
      g_CurrentUploadingFile.uSegmentsSize[i] = 0;
      g_CurrentUploadingFile.bSegmentsUploaded[i] = false;
   }
   g_bHasFileUploadInProgress = false;

   warnings_remove_all();
   alarms_reset_vehicle();

   shared_vars_state_reset_all_vehicles_runtime_info();

   // First vehicle is always the main vehicle, the next ones are relayed vehicles
   if ( NULL != g_pCurrentModel )
   {
      log_line("[Events] Add the current g_pCurrentModel as the first vehicle runtime info.");
      g_VehiclesRuntimeInfo[0].uVehicleId = g_pCurrentModel->uVehicleId;
      g_VehiclesRuntimeInfo[0].pModel = g_pCurrentModel;
      g_iCurrentActiveVehicleRuntimeInfoIndex = 0;

      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
      if ( g_pCurrentModel->relay_params.uRelayedVehicleId != 0 )
      {
         log_line("[Events] Add the current relayed vehicle (VID: %u) by the g_pCurrentModel as the second vehicle runtime info.", g_pCurrentModel->relay_params.uRelayedVehicleId);
         g_VehiclesRuntimeInfo[1].uVehicleId = g_pCurrentModel->relay_params.uRelayedVehicleId;
         g_VehiclesRuntimeInfo[1].pModel = findModelWithId(g_VehiclesRuntimeInfo[1].uVehicleId, 3);
      }
   }

   if ( (NULL != g_pCurrentModel) && g_pCurrentModel->audio_params.has_audio_device && g_pCurrentModel->audio_params.enabled )
   {
      ControllerSettings* pCS = get_ControllerSettings();
      hardware_enable_audio_output();
      hardware_set_audio_output_volume(pCS->iAudioOutputVolume);
   }

   log_current_runtime_vehicles_info();
   log_line("[Events] Current VID for vehicle runtime info[0] is (if any): %u", g_VehiclesRuntimeInfo[0].uVehicleId);
   log_line("[Events] Handled event BeforePairing. Done.");
}

void onEventPaired()
{
   log_line("[Events] Handling event Paired...");
   log_current_runtime_vehicles_info();
   if ( NULL != g_pCurrentModel )
      osd_set_current_layout_index_and_source_model(g_pCurrentModel, g_pCurrentModel->osd_params.iCurrentOSDScreen);
   else
      osd_set_current_layout_index_and_source_model(NULL, 0);

   link_watch_reset();

   log_line("[Events] Handled event Paired. Done.");
}

void onEventBeforePairingStop()
{
   log_line("[Events] Handling event Before Pairing Stop...");
   log_current_runtime_vehicles_info();

   ruby_stop_recording();

   onEventPairingDiscardAllUIActions();
   osd_remove_stats_flight_end();

   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
   {
      if ( ! (g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_PERMANENT_REMOTE) )
         g_pCurrentModel->relay_params.uCurrentRelayMode = RELAY_MODE_MAIN | RELAY_MODE_IS_RELAY_NODE;
      saveControllerModel(g_pCurrentModel);
   }

   log_current_runtime_vehicles_info();
   log_line("[Events] Handled event Before Pairing Stop.");
}

void onEventPairingStopped()
{
   log_line("[Events] Handling event Pairing Stopped...");
   log_current_runtime_vehicles_info();

   s_bEventTookPairingUIAction = false;
   g_bSwitchingRadioLink = false;
   
   alarms_remove_all();
   popups_remove_all();
   g_bIsRouterReady = false;
   g_RouterIsReadyTimestamp = 0;

   if ( NULL != g_pCurrentModel )
      reset_model_settings_download_buffers(g_pCurrentModel->uVehicleId);

   shared_vars_state_reset_all_vehicles_runtime_info();
   link_reset_reconfiguring_radiolink();
   
   g_bHasVideoDataOverloadAlarm = false;
   g_bHasVideoTxOverloadAlarm = false;
   g_bIsTestingAdaptiveVideo = false;
   g_bGotStatsVideoBitrate = false;
   g_bGotStatsVehicleTx = false;
   
   g_bHasVideoDecodeStatsSnapshot = false;

   if ( NULL != g_pPopupVideoOverloadAlarm )
   {
      if ( popups_has_popup(g_pPopupVideoOverloadAlarm) )
         popups_remove(g_pPopupVideoOverloadAlarm);
      g_pPopupVideoOverloadAlarm = NULL;
   }

   osd_warnings_reset();
   warnings_remove_all();
   log_current_runtime_vehicles_info();
   log_line("[Events] Handled event Pairing Stopped. Done.");
}

void onEventPairingStartReceivingData(u32 uVehicleId)
{
   log_line("[Events] Hadling event 'Started receiving data from a vehicle', for VID: %u", uVehicleId);
   log_current_runtime_vehicles_info();

   if ( NULL != g_pPopupLooking )
   {
      popups_remove(g_pPopupLooking);
      g_pPopupLooking = NULL;
      log_line("Removed popup looking for model (4).");
   }
   
   if ( NULL != g_pPopupLinkLost )
   {
      popups_remove(g_pPopupLinkLost);
      g_pPopupLinkLost = NULL;
      log_line("Removed popup link lost.");
   }

   if ( NULL != g_pPopupWrongModel )
   {
      popups_remove(g_pPopupWrongModel);
      g_pPopupWrongModel = NULL;
      log_line("Removed popup wrong model (3).");
   }

   t_structure_vehicle_info* pVRTInfo = NULL;
   if ( g_bSearching )
      pVRTInfo = &g_SearchVehicleRuntimeInfo;
   else
      pVRTInfo = &(g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex]);
   log_line("[Events] Got already telemetry for this VID %u ? Ruby telem: %s, FC telemetry: %s",
      uVehicleId, pVRTInfo->bGotRubyTelemetryInfo? "yes":"no",
      pVRTInfo->bGotFCTelemetry? "yes":"no");

   log_line("[Events] Ruby telemetry received VID: %u", pVRTInfo->headerRubyTelemetryExtended.uVehicleId);

   log_line("[Events] Is pairing initated with this vehicle? %s", pVRTInfo->bNotificationPairingRequestSent?"Yes":"No");
   log_line("[Events] Is pairing done and confirmed with this vehicle? %s", pVRTInfo->bPairedConfirmed?"Yes":"No");

   if ( g_bSearching )
   {
      log_line("[Events] Handled event 'Started receiving data from vehicle' (while searching)");
      return;
   }

   log_line("[Events] Mode 'Must sync settings on link recover' is set? %s", g_bSyncModelSettingsOnLinkRecover?"Yes":"No");

   if ( NULL == g_pCurrentModel )
      log_line("[Events] No current model active.");
   else
      log_line("[Events] Must sync current model settings: %s", g_pCurrentModel->b_mustSyncFromVehicle?"yes":"no");
   if ( g_bSyncModelSettingsOnLinkRecover )
   {
      log_line("Must sync model setings on link recover.");
      g_bSyncModelSettingsOnLinkRecover = false;
      if ( NULL != g_pCurrentModel )
         g_pCurrentModel->b_mustSyncFromVehicle = true;
   }

   log_current_runtime_vehicles_info();
   log_line("[Events] Handled event 'Started receiving data from vehicle'.");
}

void onEventArmed(u32 uVehicleId)
{
   log_line("[Events] Handling event OnArmed...");
   log_line("Vehicle %u is Armed", uVehicleId);
   local_stats_on_arm(uVehicleId);
   notification_add_armed(uVehicleId);
   osd_remove_stats_flight_end();
   Preferences* p = get_Preferences();
   if ( NULL != p && p->iStartVideoRecOnArm )
      ruby_start_recording();
   log_line("[Events] Handled event OnArmed. Done.");
}

void onEventDisarmed(u32 uVehicleId)
{
   log_line("[Events] Handling event OnDisarmed...");
   log_line("Vehicle %u is Disarmed", uVehicleId);
   local_stats_on_disarm(uVehicleId);
   notification_add_disarmed(uVehicleId);
   t_structure_vehicle_info* pRuntimeInfo = get_vehicle_runtime_info_for_vehicle_id(uVehicleId);
   if ( (NULL != pRuntimeInfo) && (NULL != pRuntimeInfo->pModel) )
   if ( pRuntimeInfo->pModel->osd_params.uFlags & OSD_BIT_FLAGS_SHOW_FLIGHT_END_STATS )
      osd_add_stats_flight_end();
   Preferences* p = get_Preferences();
   if ( NULL != p && p->iStopVideoRecOnDisarm )
      ruby_stop_recording();
   log_line("[Events] Handled event OnDisarmed. Done.");
}

bool _onEventCheck_NegociateRadioLinks(Model* pCurrentlyStoredModel, Model* pNewReceivedModel)
{
   log_line("Check if we must negociate radio links...");
   g_bMustNegociateRadioLinksFlag = false;

   if ( NULL != pNewReceivedModel )
   if ( ! is_sw_version_latest(pNewReceivedModel) )
   {
      log_line("Negociate radio links? No, vehicle's SW version %d.%d (b-%d) is different than controller's %d.%d", get_sw_version_major(pNewReceivedModel), get_sw_version_minor(pNewReceivedModel), get_sw_version_build(pNewReceivedModel), SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR);
      return false;
   }
   if ( (NULL == pNewReceivedModel) && (NULL != pCurrentlyStoredModel) )
   if ( ! is_sw_version_latest(pCurrentlyStoredModel) )
   {
      log_line("Negociate radio links? No, received vehicle is null, current vehicle's SW build version %d.%d different than controller's %d.%d", get_sw_version_major(pCurrentlyStoredModel), get_sw_version_minor(pCurrentlyStoredModel), SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR);
      return false;
   }

   if ( NULL != pNewReceivedModel )
   if ( pNewReceivedModel->uControllerId != g_uControllerId )
   {
      log_line("Negociate radio links? Yes, vehicle was paired with a different controller: %u (this controller: %u)", pNewReceivedModel->uControllerId, g_uControllerId);
      g_bMustNegociateRadioLinksFlag = true;
   }

   if ( NULL != pCurrentlyStoredModel )
   if ( !(pCurrentlyStoredModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS) )
   {
      log_line("Negociate radio links? Yes, currently stored model has not negociated radio links.");
      g_bMustNegociateRadioLinksFlag = true;
   }

   if ((NULL != g_pCurrentModel) && (NULL != pCurrentlyStoredModel) )
   if ( g_pCurrentModel->uVehicleId == pCurrentlyStoredModel->uVehicleId )
   if ( !(pCurrentlyStoredModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS) )
   {
      log_line("Negociate radio links? Yes, currently stored model (2) has not negociated radio links.");
      g_bMustNegociateRadioLinksFlag = true;
   }

   if ( NULL != pNewReceivedModel )
   if ( !(pNewReceivedModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS) )
   {
      log_line("Negociate radio links? Yes, received model has not negociated radio links.");
      g_bMustNegociateRadioLinksFlag = true;
   }

   if ( NULL != pNewReceivedModel )
   if ( !(pNewReceivedModel->radioRuntimeCapabilities.uFlagsRuntimeCapab & MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED) )
   {
      log_line("Negociate radio links? Yes, received model has no computed runtime radio links capabilities.");
      g_bMustNegociateRadioLinksFlag = true;
   }

   if ( (NULL != pCurrentlyStoredModel) && (NULL != pNewReceivedModel) )
   if ( pCurrentlyStoredModel->radioInterfacesParams.interfaces_count != pNewReceivedModel->radioInterfacesParams.interfaces_count )
   {
      log_line("Negociate radio links? Yes, received model and current model have different interfaces count.");
      g_bMustNegociateRadioLinksFlag = true;
   }
   if ( (NULL != pCurrentlyStoredModel) && (NULL != pNewReceivedModel) )
   for( int i=0; i<pCurrentlyStoredModel->radioInterfacesParams.interfaces_count; i++ )
   {
       if ( pCurrentlyStoredModel->radioInterfacesParams.interface_card_model[i] != pNewReceivedModel->radioInterfacesParams.interface_card_model[i] )
       {
          log_line("Negociate radio links? Yes, received model and current model have different interfaces model for radio interface %d: %d != %d", i+1, pCurrentlyStoredModel->radioInterfacesParams.interface_card_model[i], pNewReceivedModel->radioInterfacesParams.interface_card_model[i] );
          g_bMustNegociateRadioLinksFlag = true;
       }
       if ( pCurrentlyStoredModel->radioInterfacesParams.interface_radiotype_and_driver[i] != pNewReceivedModel->radioInterfacesParams.interface_radiotype_and_driver[i] )
       {
          log_line("Negociate radio links? Yes, received model and current model have different interfaces drivers for radio interface %d: %d != %d", i+1, pCurrentlyStoredModel->radioInterfacesParams.interface_radiotype_and_driver[i], pNewReceivedModel->radioInterfacesParams.interface_radiotype_and_driver[i] );
          g_bMustNegociateRadioLinksFlag = true;
       }
       if ( 0 != strcmp(pCurrentlyStoredModel->radioInterfacesParams.interface_szMAC[i], pNewReceivedModel->radioInterfacesParams.interface_szMAC[i]) )
       {
          log_line("Negociate radio links? Yes, received model and current model have different interfaces names for radio interface %d: (%s) != (%s)", i+1, pCurrentlyStoredModel->radioInterfacesParams.interface_szMAC[i], pNewReceivedModel->radioInterfacesParams.interface_szMAC[i] );
          g_bMustNegociateRadioLinksFlag = true;
       }
   }

   if ( g_bLinkWizardAfterUpdate )
   {
      log_line("Negociate radio links? Yes, an update was done. Must show radio link wizard.");
      g_bMustNegociateRadioLinksFlag = true;
   }
   //if ( g_bDidAnUpdate )
   //   g_bMustNegociateRadioLinksFlag = false;

   log_line("Check if we must negociate radio links result: %s", g_bMustNegociateRadioLinksFlag?"yes":"no");
   return g_bMustNegociateRadioLinksFlag;
}

bool _onEventCheck_CameraChanged(Model* pCurrentlyStoredModel, Model* pNewReceivedModel)
{
   if ( pCurrentlyStoredModel->uVehicleId != pNewReceivedModel->uVehicleId )
      return false;

   if ( pNewReceivedModel->iCurrentCamera != pCurrentlyStoredModel->iCurrentCamera )
      return true;
   if ( (pNewReceivedModel->iCurrentCamera >= 0) && (pNewReceivedModel->iCameraCount > 0) && (pCurrentlyStoredModel->iCurrentCamera >= 0) && (pCurrentlyStoredModel->iCameraCount > 0) )
   if ( (pNewReceivedModel->camera_params[pNewReceivedModel->iCurrentCamera].iCameraType != pCurrentlyStoredModel->camera_params[pCurrentlyStoredModel->iCurrentCamera].iCameraType ) ||
        (pNewReceivedModel->camera_params[pNewReceivedModel->iCurrentCamera].iForcedCameraType != pCurrentlyStoredModel->camera_params[pCurrentlyStoredModel->iCurrentCamera].iForcedCameraType ) )
      return true;

   return false;
}

bool _onEventCheck_RadioChanged(Model* pCurrentlyStoredModel, Model* pNewReceivedModel)
{
   if ( pCurrentlyStoredModel->uVehicleId != pNewReceivedModel->uVehicleId )
      return false;

   bool bRadioChanged = IsModelRadioConfigChanged(&(pCurrentlyStoredModel->radioLinksParams), &(pCurrentlyStoredModel->radioInterfacesParams),
       &(pNewReceivedModel->radioLinksParams), &(pNewReceivedModel->radioInterfacesParams));
   log_line("Received model has different radio config? %s", bRadioChanged?"yes":"no");
   
   if ( ! bRadioChanged )
      return false;

   if ( pCurrentlyStoredModel->radioLinksParams.links_count != pNewReceivedModel->radioLinksParams.links_count )
      return true;

   if ( pCurrentlyStoredModel->radioInterfacesParams.interfaces_count != pNewReceivedModel->radioInterfacesParams.interfaces_count )
      return true;

   for( int i=0; i<pCurrentlyStoredModel->radioLinksParams.links_count; i++ )
   {
      if ( pCurrentlyStoredModel->radioLinksParams.link_frequency_khz[i] != pNewReceivedModel->radioLinksParams.link_frequency_khz[i] )
         return true;
   }

   return false;
}

// Returns true if a big change is detected that requires re-pairing
bool _onEventCheckChangesToModel(Model* pCurrentlyStoredModel, Model* pNewReceivedModel)
{
   log_line("[Events] onEventCheckChangesToModel: currently stored model: %u, new received model: %u",
      (NULL == pCurrentlyStoredModel)?0:pCurrentlyStoredModel->uVehicleId,
      (NULL == pNewReceivedModel)?0:pNewReceivedModel->uVehicleId);

   log_line("[Events] onEventCheckChangesToModel: negociated radio? currently stored model: %s, new received model: %s",
      (NULL == pCurrentlyStoredModel)?"N/A" : ((pCurrentlyStoredModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS)?"yes":"no"),
      (NULL == pNewReceivedModel)?"N/A" : ((pNewReceivedModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS)?"yes":"no"));

   if ( (NULL == pCurrentlyStoredModel) || (NULL == pNewReceivedModel) )
   {
      g_bMustNegociateRadioLinksFlag = _onEventCheck_NegociateRadioLinks(pCurrentlyStoredModel, pNewReceivedModel);
      return false;
   }
   bool bCurrentModelIsInActiveRuntime = false;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == pCurrentlyStoredModel->uVehicleId )
      {
         bCurrentModelIsInActiveRuntime = true;
         break;
      }
   }

   // Check camera sensor change
   if ( pNewReceivedModel->iCurrentCamera != pCurrentlyStoredModel->iCurrentCamera )
      warnings_add(pCurrentlyStoredModel->uVehicleId, L("Current active camera changed on the vehicle"), g_idIconCamera);
   int iCurrentCamType = 0;
   int iNewCamType = 0;

   if ( pCurrentlyStoredModel->iCurrentCamera >= 0 )
      iCurrentCamType = pCurrentlyStoredModel->camera_params[pCurrentlyStoredModel->iCurrentCamera].iCameraType;
   if ( pNewReceivedModel->iCurrentCamera >= 0 )
      iNewCamType = pNewReceivedModel->camera_params[pNewReceivedModel->iCurrentCamera].iCameraType;

   if ( iCurrentCamType != iNewCamType )
   {
      char szBuff[256];
      char szCam1[128];
      char szCam2[128];
      str_get_hardware_camera_type_string_to_string(iCurrentCamType, szCam1);
      str_get_hardware_camera_type_string_to_string(iNewCamType, szCam2);
      sprintf(szBuff, L("Current camera sensor changed from %s to %s"), szCam1, szCam2);
      warnings_add(pCurrentlyStoredModel->uVehicleId, szBuff, g_idIconCamera);
   }

   osd_parameters_t oldOSDParams;
   video_parameters_t oldVideoParams;
   memcpy((u8*)&oldOSDParams, (u8*)&(pCurrentlyStoredModel->osd_params), sizeof(osd_parameters_t));
   memcpy(&oldVideoParams, &(pCurrentlyStoredModel->video_params), sizeof(video_parameters_t));

   g_bMustNegociateRadioLinksFlag = _onEventCheck_NegociateRadioLinks(pCurrentlyStoredModel, pNewReceivedModel);

   bool bCameraChanged = _onEventCheck_CameraChanged(pCurrentlyStoredModel, pNewReceivedModel);
   log_line("Camera did change on the VID %u ? %s", pNewReceivedModel->uVehicleId, (bCameraChanged?"Yes":"No"));
   
   bool bRadioChanged = _onEventCheck_RadioChanged(pCurrentlyStoredModel, pNewReceivedModel);
   if ( bRadioChanged )
   {
      if ( (pCurrentlyStoredModel == g_pCurrentModel) || bCurrentModelIsInActiveRuntime )
         log_line("Received model has different number of radio links or different frequencies. Must update local radio configuration now as it's the current model or it's an active relay node.");
      else
         log_line("Received model has different number of radio links or different frequencies but it's not the current model.");
   }
   else
      log_line("Received model has same radio config as the currently stored model.");

   if ( pCurrentlyStoredModel->uVehicleId == g_pCurrentModel->uVehicleId )
   if ( bRadioChanged )
      return true;

   if ( pCurrentlyStoredModel->uVehicleId == g_pCurrentModel->uVehicleId )
   if ( pCurrentlyStoredModel->audio_params.enabled != pNewReceivedModel->audio_params.enabled )
   {
      log_line("Audio enable flag changed.");
      return true;
   }
   return false;
}

// Returns true if actions where taken or needed to be taken

bool _onEventCheckNewlyPairedModelForUIActionsToTake()
{
   log_line("[Events] Checking for pairing UI actions to take...");

   if ( s_bEventTookPairingUIAction )
   {
      log_line("[Events] Checking for pairing UI actions to take: already has an action in progres.");
      return true;
   }

   // Remove vehicle will reboot message, if present
   Menu* pTopMenu = menu_get_top_menu();
   if ( (NULL != pTopMenu) && menu_has_menu(MENU_ID_CONFIRMATION+3*1000) )
   if ( pTopMenu->m_MenuId == MENU_ID_CONFIRMATION+3*1000 )
      remove_menu_from_stack(pTopMenu);

   s_uTimeToCheckNextPairingUIAction = 0;
   if ( (NULL == g_pCurrentModel) || (NULL == s_pEventsLastRecvModelSettings) )
   {
      log_softerror_and_alarm("[Events] Has invalid null models to check for pairing actions.");
      return false;
   }
   if ( g_pCurrentModel->uVehicleId != s_pEventsLastRecvModelSettings->uVehicleId )
   {
      log_softerror_and_alarm("[Events] Received pairing model has a different VID (%u) than current model (%u). No pairing action to do.", s_pEventsLastRecvModelSettings->uVehicleId, g_pCurrentModel->uVehicleId);
      return false;
   }

   int iCurrentModelActiveRuntimeIndex = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == g_pCurrentModel->uVehicleId )
      {
         iCurrentModelActiveRuntimeIndex = i;
         break;
      }
   }

   if ( ! s_bEventTookPairingUIActionUpdateController )
   if ( get_sw_version_build(s_pEventsLastRecvModelSettings) > SYSTEM_SW_BUILD_NUMBER )
   {
      char szBuff[256];
      char szBuff2[32];
      char szBuff3[32];
      char szBuff4[64];
      getSystemVersionString(szBuff2, s_pEventsLastRecvModelSettings->sw_version);
      getSystemVersionString(szBuff3, (SYSTEM_SW_VERSION_MAJOR<<8) | SYSTEM_SW_VERSION_MINOR);
      strcpy(szBuff4, s_pEventsLastRecvModelSettings->getVehicleTypeString());
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s has Ruby version %s (b-%u) and your controller %s (b-%u). You should update your controller.", szBuff4, szBuff2, get_sw_version_build(s_pEventsLastRecvModelSettings), szBuff3, SYSTEM_SW_BUILD_NUMBER);
      szBuff[0] = toupper(szBuff[0]);
      warnings_add(s_pEventsLastRecvModelSettings->uVehicleId, szBuff, 0, NULL, 10);
      MenuConfirmation* pMC = new MenuConfirmation(L("Update Info"), szBuff, 0, true);
      pMC->m_yPos = 0.3;
      pMC->setDefaultActionPositive();
      add_menu_to_stack(pMC);

      s_bEventTookPairingUIActionUpdateController = true;
      log_line("[Events] Added pairing UI action: controller update.");
      return true;
   }

   bool bMustUpdate = false;
   if ( ! is_sw_version_latest(s_pEventsLastRecvModelSettings) )
      bMustUpdate = true;

   if ( s_pEventsLastRecvModelSettings->isRunningOnOpenIPCHardware() )
   if ( hardware_board_is_goke(s_pEventsLastRecvModelSettings->hwCapabilities.uBoardType) )
      bMustUpdate = false;

   if ( bMustUpdate )
   {
      char szBuff[256];
      char szBuff2[32];
      char szBuff3[32];
      char szBuff4[64];
      getSystemVersionString(szBuff2, s_pEventsLastRecvModelSettings->sw_version);
      getSystemVersionString(szBuff3, (SYSTEM_SW_VERSION_MAJOR<<8) | SYSTEM_SW_VERSION_MINOR);
      strcpy(szBuff4, s_pEventsLastRecvModelSettings->getVehicleTypeString());
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), L("%s has Ruby version %s (b-%u) and your controller %s (b-%u). You should update your %s."), szBuff4, szBuff2, get_sw_version_build(s_pEventsLastRecvModelSettings), szBuff3, SYSTEM_SW_BUILD_NUMBER, szBuff4);
      szBuff[0] = toupper(szBuff[0]);
      warnings_add(s_pEventsLastRecvModelSettings->uVehicleId, szBuff, 0, NULL, 12);
      bool bArmed = false;
      if ( -1 != iCurrentModelActiveRuntimeIndex )
      if ( g_VehiclesRuntimeInfo[iCurrentModelActiveRuntimeIndex].bGotFCTelemetry )
      if ( g_VehiclesRuntimeInfo[iCurrentModelActiveRuntimeIndex].headerFCTelemetry.uFCFlags & FC_TELE_FLAGS_ARMED )
         bArmed = true;

      if ( ! s_bEventTookPairingUIActionUpdateVehicle )
      if ( ! bArmed )
      if ( ! isMenuOn() )
      if ( ! g_bMenuPopupUpdateVehicleShown )
      {
          add_menu_to_stack( new MenuUpdateVehiclePopup(-1) );
          g_bMenuPopupUpdateVehicleShown = true;
          s_bEventTookPairingUIActionUpdateVehicle = true;
          log_line("[Events] Added pairing UI action: vehicle update.");
          return true;
      }
   }

   if ( ! g_pCurrentModel->is_spectator )
   if ( hardware_board_is_sigmastar(s_pEventsLastRecvModelSettings->hwCapabilities.uBoardType) )
   if ( (s_pEventsLastRecvModelSettings->hwCapabilities.uBoardType & BOARD_SUBTYPE_MASK) == BOARD_SUBTYPE_OPENIPC_UNKNOWN )
   if ( ! menu_has_menu(MENU_ID_VEHICLE_BOARD) )
   if ( ! s_bEventTookPairingUIActionUnitType )
   {
      add_menu_to_stack( new MenuConfirmationVehicleBoard() );
      s_bEventTookPairingUIActionUnitType = true;
      log_line("[Events] Added pairing UI action: vehicle update.");
      return true;
   }

   if ( ! g_pCurrentModel->is_spectator )
   if ( is_sw_version_latest(s_pEventsLastRecvModelSettings) )
   if ( ! s_bEventTookPairingUIActionSimpleSetup )
   if ( s_pEventsLastRecvModelSettings->osd_params.uFlags & OSD_BIT_FLAGS_MUST_CHOOSE_PRESET )
   {
      MenuVehicleSimpleSetup* pMenu = new MenuVehicleSimpleSetup();
      pMenu->setPairingSetup();
      add_menu_to_stack(pMenu);
      log_line("[Events] Added pairing UI action: simple setup.");
      s_bEventTookPairingUIActionSimpleSetup = true;
      return true;
   }

   #if defined(HW_PLATFORM_RASPBERRY)
   if ( s_pEventsLastRecvModelSettings->video_params.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265 )
   if ( ! s_bEventTookPairingUIActionEncoderType )
   {
       char szVehicleType[64];
       char szText[128];
       strcpy(szVehicleType, s_pEventsLastRecvModelSettings->getVehicleTypeString());
       snprintf(szText, sizeof(szText)/sizeof(szText[0]), "Your %s generates H265 video but your controller supports only H264. Change the %s video encoder to H264 encoder", szVehicleType, szVehicleType);
       warnings_add(s_pEventsLastRecvModelSettings->uVehicleId, szText, g_idIconCamera, get_Color_IconWarning() );
       snprintf(szText, sizeof(szText)/sizeof(szText[0]), "Your %s generates H265 video but your controller supports only H264. Change teh %s video encoder to H264 encoder (from Menu->Vehicle Settings->Video)", szVehicleType, szVehicleType);
       MenuConfirmation* pMC = new MenuConfirmation(L("Unsuppoerted video codec"), szText, 0, true);
       pMC->m_yPos = 0.3;
       add_menu_to_stack(pMC);
       s_bEventTookPairingUIActionEncoderType = true;
       log_line("[Events] Added pairing UI action: vehicle has H265 on Pi controller.");
       return true;
   }

   // Check for H264 slices on OpenIPC-Raspberry pair
   if ( s_pEventsLastRecvModelSettings->isRunningOnOpenIPCHardware() )
   if ( s_pEventsLastRecvModelSettings->video_params.iH264Slices > 1 )
   if ( ! s_bEventTookPairingUIActionEncoderSlices )
   {
      char szBuff[256];
      strcpy(szBuff, L("Your vehicle has H264/H265 slices enabled and Raspberry Pi can't decode slices from OpenIPC hardware. You will have issues."));
      MenuConfirmation* pMC = new MenuConfirmation(L("Unsuppoerted video slices"), szBuff, 0, true);
      pMC->addTopLine(L("Set H264/H265 slices to 1, from [Menu]->[Vehicle]->[Video]->[Advanced settings]"));
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      s_bEventTookPairingUIActionEncoderSlices = true;
      log_line("[Events] Added pairing UI action: vehicle has H264 slices on Pi controller.");
      return true;
   }
   #endif

   if ( g_bMustNegociateRadioLinksFlag )
   {
      if ( ! g_pCurrentModel->is_spectator )
      if ( ! menu_has_menu(MENU_ID_NEGOCIATE_RADIO) )
      if ( ! menu_has_menu(MENU_ID_VEHICLE_BOARD) )
      if ( ! g_bAskedForNegociateRadioLink )
      if ( is_sw_version_latest(s_pEventsLastRecvModelSettings) )
      if ( ! s_bEventTookPairingUIActionNegociateRadio )
      {
         add_menu_to_stack(new MenuNegociateRadio());
         s_bEventTookPairingUIActionNegociateRadio = true;
         log_line("[Events] Added pairing UI action: negociate radio links.");
         return true;
      }
      log_line("[Events] Flag is set that we must negociate radio links, but checks fail: spectator? %s, has camera: %s, has other menus visible? %s, already asked for negociate? %s, sw too old? %s, already took pairing action to start radio link negociation? %s",
         g_pCurrentModel->is_spectator?"yes":"no",
         s_pEventsLastRecvModelSettings->hasCamera()?"yes":"no",
         (menu_has_menu(MENU_ID_NEGOCIATE_RADIO) || menu_has_menu(MENU_ID_VEHICLE_BOARD))?"yes":"no",
         g_bAskedForNegociateRadioLink?"yes":"no",
         is_sw_version_latest(s_pEventsLastRecvModelSettings)?"no":"yes",
         s_bEventTookPairingUIActionNegociateRadio?"yes":"no");
   }
   #if defined HW_PLATFORM_RASPBERRY
   if ( ! s_bEventTookPairingUIActionEncoderSpikes )
   //if ( ! g_bDidAnUpdate )
   if ( s_pEventsLastRecvModelSettings->isRunningOnOpenIPCHardware() )
   if ( getPreferencesDoNotShowAgain(ID_DONOT_SHOW_AGAIN_MIXED_PI_OPENIPC_HARDWARE) != 1 )
   {
      char szText1[256];
      char szText2[256];
      strcpy(szText1, L("You are using a Raspberry pi board connected to an OpenIPC board."));
      strcpy(szText2, L("You might experience spikes in the video feed on this hardware combination depending on the video settings."));
      MenuConfirmation* pMC = new MenuConfirmation(L("Hardware Info"), szText1, 0, true);
      pMC->m_yPos = 0.3;
      pMC->addTopLine(szText2);
      pMC->setUniqueId(ID_DONOT_SHOW_AGAIN_MIXED_PI_OPENIPC_HARDWARE);
      pMC->enableShowDoNotShowAgain();
      pMC->setIconId(g_idIconCPU);
      add_menu_to_stack(pMC);
      s_bEventTookPairingUIActionEncoderSpikes = true;
      log_line("[Events] Added pairing UI action: Pi-OpenIPC keyframe spikes.");
      return true;
   }
   #endif

   log_line("[Events] Checking for pairing UI actions to take: none taken.");
   return false;
}

void _onEventCheckModelAddNonBlockingPopupWarnings(Model* pModel, bool bUnsolicitedSync)
{
   if ( NULL == pModel )
      return;

   char szText[256];

   // Check supported cards

   if ( pModel->uVehicleId == g_pCurrentModel->uVehicleId )
   {
      int countUnsuported = 0;
      for( int i=0; i<pModel->radioInterfacesParams.interfaces_count; i++ )
         if ( 0 == (pModel->radioInterfacesParams.interface_radiotype_and_driver[i] & 0xFF0000) )
            countUnsuported++;
   
      if ( countUnsuported == pModel->radioInterfacesParams.interfaces_count )
      {
         sprintf(szText, L("No radio interface on your %s is fully supported."), pModel->getVehicleTypeString());
         Popup* p = new Popup(szText, 0.3,0.4, 0.5, 6);
         p->setIconId(g_idIconError, get_Color_IconError());
         popups_add_topmost(p);
      }
      else if ( countUnsuported > 0 )
      {
         sprintf(szText, L("Some radio interfaces on your %s are not fully supported."), pModel->getVehicleTypeString());
         Popup* p = new Popup(szText, 0.3,0.4, 0.5, 6);
         p->setIconId(g_idIconWarning, get_Color_IconWarning());
         popups_add_topmost(p);
      }
   }

   // Check unsuported UARTs

   if ( pModel->alarms & ALARM_ID_UNSUPORTED_USB_SERIAL )
   {
      sprintf(szText, "Your %s has an unsupported USB to Serial adapter. Use brand name serial adapters or ones with  CP2102 chipset. The ones with 340 chipset are not compatible.", pModel->getVehicleTypeString());
      warnings_add(pModel->uVehicleId, szText, g_idIconError);
   }

   // Check invalid audio

   if ( pModel->audio_params.enabled )
   {
      if ( ! pModel->audio_params.has_audio_device )
         warnings_add(pModel->uVehicleId, L("Your vehicle has audio enabled but no audio capture device"), g_idIconError);
      else if ( pModel->uVehicleId == g_pCurrentModel->uVehicleId )
      {
         if ( ! hardware_has_audio_playback() )
            warnings_add(pModel->uVehicleId, L("Your vehicle has audio enabled but your controller can't output audio."), g_idIconError);
      }
   }

   // Check forced camera types
   
   for( int i=0; i<pModel->iCameraCount; i++ )
   {
      log_line("Received camera %d name: [%s]", i, pModel->getCameraName(i));
      if ( pModel->camera_params[i].iForcedCameraType != CAMERA_TYPE_NONE )
      if ( pModel->camera_params[i].iForcedCameraType != pModel->camera_params[i].iCameraType )
      {
         char szBuff[256];
         char szBuff1[128];
         char szBuff2[128];
         str_get_hardware_camera_type_string_to_string(pModel->camera_params[i].iCameraType, szBuff1);
         str_get_hardware_camera_type_string_to_string(pModel->camera_params[i].iForcedCameraType, szBuff2);
         if ( pModel->iCameraCount > 1 )
            snprintf(szBuff, 255, "Your camera %d is autodetected as %s but you forced to work as %s", i+1, szBuff1, szBuff2);
         else
            snprintf(szBuff, 255, "Your camera is autodetected as %s but you forced to work as %s", szBuff1, szBuff2);
         warnings_add(pModel->uVehicleId, szBuff, g_idIconCamera, get_Color_IconWarning() );
      }
   }

   // Check for H264 slices on OpenIPC-Raspberry pair
   #if defined HW_PLATFORM_RASPBERRY
   if ( pModel->isRunningOnOpenIPCHardware() )
   if ( pModel->video_params.iH264Slices > 1 )
   {
      char szBuff[256];
      strcpy(szBuff, L("Your vehicle has H264/H265 slices enabled and Raspberry Pi can't decode slices from OpenIPC hardware. You will have issues."));
      warnings_add(pModel->uVehicleId, szBuff, g_idIconCamera, get_Color_IconError());
   }
   #endif
}


void onEventRelayModeChanged()
{
   log_line("[Events] Handling event OnRelayModeChanged...");
   log_line("[Events] New relay mode: %d (%s), main VID: %u, relayed VID: %u", g_pCurrentModel->relay_params.uCurrentRelayMode, str_format_relay_mode(g_pCurrentModel->relay_params.uCurrentRelayMode), g_pCurrentModel->uVehicleId, g_pCurrentModel->relay_params.uRelayedVehicleId);
   log_line("[Events] Handled of event OnRelayModeChanged complete.");

   Model* pModel = findModelWithId(g_pCurrentModel->relay_params.uRelayedVehicleId, 5);
   if ( NULL != pModel )
   {
      bool bDifferentResolution = false;
      if ( g_pCurrentModel->video_params.iVideoWidth != pModel->video_params.iVideoWidth )
         bDifferentResolution = true;
      if ( g_pCurrentModel->video_params.iVideoHeight != pModel->video_params.iVideoHeight )
         bDifferentResolution = true;

      if ( bDifferentResolution )
      {
         static u32 s_uTimeLastWarningRelayDifferentResolution = 0;
         if ( g_TimeNow > s_uTimeLastWarningRelayDifferentResolution + 1000*60 )
         {
            s_uTimeLastWarningRelayDifferentResolution = g_TimeNow;
            char szBuff[256];
            sprintf(szBuff, "The relay and relayed vehicles have different video streams resolutions (%d x %d and %d x %d). Set the same video resolution for both cameras to get best relaying performance.",
               g_pCurrentModel->video_params.iVideoWidth,
               g_pCurrentModel->video_params.iVideoHeight,      
               pModel->video_params.iVideoWidth,
               pModel->video_params.iVideoHeight );
            warnings_add(g_pCurrentModel->uVehicleId, szBuff, g_idIconCamera);
         }
      }
   }
}

bool onEventReceivedModelSettings(u32 uVehicleId, u8* pBuffer, int length, bool bUnsolicited)
{
   log_line("[Events] Handling event OnReceivedModelSettings for VID %u (%d bytes, expected: %s)...", uVehicleId, length, (bUnsolicited?"no":"yes"));

   if ( 0 == uVehicleId )
   {
      log_line("[Events] Received model settings for VID 0. Ignoring it.");
      return false;
   }

   if ( NULL == g_pCurrentModel )
   {
      log_line("[Events] Currently active model is NULL. Ignore received model settings.");
      return false;
   }
   else
      log_line("[Events] Currently active model VID: %d, mode: %s", g_pCurrentModel->uVehicleId, g_pCurrentModel->is_spectator?"spectator mode":"control mode");
   
   log_current_runtime_vehicles_info();

   log_line("[Events] Current vehicle radio links configuration:");
   g_pCurrentModel->logVehicleRadioInfo();

   Model* pCurrentlyStoredModel = NULL;
   bool bFoundInList = false;
   int iRuntimeIndex = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId != uVehicleId )
         continue;
      if ( NULL != g_VehiclesRuntimeInfo[i].pModel )
         log_line("[Events] Received model settings for runtime index %d, %s", i, g_VehiclesRuntimeInfo[i].pModel->getLongName());
      else
         log_line("[Events] Received model settings for runtime index %d, NULL model.", i);
      pCurrentlyStoredModel = g_VehiclesRuntimeInfo[i].pModel;
      bFoundInList = true;
      iRuntimeIndex = i;
      break;
   }

   if ( (! bFoundInList) || (-1 == iRuntimeIndex) )
   {
      log_softerror_and_alarm("[Events] Received model settings for unknown vehicle who's not in the runtime list. Ignoring it.");
      log_current_runtime_vehicles_info();
      return false;
   }

   if ( NULL == pCurrentlyStoredModel )
   {
      log_softerror_and_alarm("[Events] Received model settings for vehicle who has no model in the runtime list. Ignoring it.");
      log_current_runtime_vehicles_info();
      return false;
   }

   log_line("[Events] Found model (VID %u) in the runtime list at position %d, mode: %s", pCurrentlyStoredModel->uVehicleId, iRuntimeIndex, pCurrentlyStoredModel->is_spectator?"spectator mode":"control mode");
   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->uVehicleId == pCurrentlyStoredModel->uVehicleId) )
      log_line("[Events] Found model in storage (0x%X) is the same as current model (g_pCurrentModel = 0x%X).", pCurrentlyStoredModel, g_pCurrentModel);
   else
      log_line("[Events] Found model in storage is not the same as current model (current model VID: %u)", (NULL != g_pCurrentModel)?(g_pCurrentModel->uVehicleId): 0);

   log_line("[Events] Stored model (VID %u) radio links configuration:", pCurrentlyStoredModel->uVehicleId);
   pCurrentlyStoredModel->logVehicleRadioInfo();

   char szFile[MAX_FILE_PATH_SIZE];
   sprintf(szFile, "%s/last_recv_model.mdl", FOLDER_RUBY_TEMP);

   FILE* fd = fopen(szFile, "wb");
   if ( NULL != fd )
   {
       fwrite(pBuffer, 1, length, fd);
       fclose(fd);
       fd = NULL;
   }
   else
   {
      log_error_and_alarm("Failed to save received vehicle configuration to temp file (%s).", szFile);
      log_error_and_alarm("[Events]: Failed to process received model settings from vehicle.");
      return false;
   }

   sprintf(szFile, "%s/last_recv_model.bak", FOLDER_RUBY_TEMP);
   fd = fopen(szFile, "wb");
   if ( NULL != fd )
   {
       fwrite(pBuffer, 1, length, fd);
       fclose(fd);
       fd = NULL;
   }
   else
   {
      log_error_and_alarm("Failed to save received vehicle configuration to temp file (%s).", szFile);
      log_error_and_alarm("[Events]: Failed to process received model settings from vehicle.");
      return false;
   }

   if ( NULL != s_pEventsLastRecvModelSettings )
      delete s_pEventsLastRecvModelSettings;
   s_pEventsLastRecvModelSettings = new Model();
   if ( NULL == s_pEventsLastRecvModelSettings )
   {
      log_softerror_and_alarm("HCommands: Failed to create the new received vehicle model.");
      log_error_and_alarm("[Events]: Failed to process received model settings from vehicle, null model.");
      warnings_add_error_null_model(1);
      return false;    
   }
   sprintf(szFile, "%s/last_recv_model.mdl", FOLDER_RUBY_TEMP);
   if ( ! s_pEventsLastRecvModelSettings->loadFromFile(szFile, true) )
   {
      log_softerror_and_alarm("HCommands: Failed to load the received vehicle model file. Invalid file.");
      log_error_and_alarm("[Events]: Failed to process received model settings from vehicle.");
      warnings_add_error_null_model(1);
      return false;
   }

   log_line("[Events] Received model (VID %u) radio links configuration:", s_pEventsLastRecvModelSettings->uVehicleId);
   s_pEventsLastRecvModelSettings->logVehicleRadioInfo();

   log_line("Current (before update) local model (VID: %u) mode: %s, has negociated radio? %s, controller id: %u", pCurrentlyStoredModel->uVehicleId, pCurrentlyStoredModel->is_spectator?"spectator mode":"control mode", (pCurrentlyStoredModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS)?"yes":"no", pCurrentlyStoredModel->uControllerId);
   log_line("Current (before update) current model (g_pCurrentModel) (VID: %u) mode: %s, has negociated radio? %s", g_pCurrentModel->uVehicleId, g_pCurrentModel->is_spectator?"spectator mode":"control mode", (g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS)?"yes":"no");
   log_line("Current (before update) current camera index: %d, type: %s", g_pCurrentModel->iCurrentCamera,
      (g_pCurrentModel->iCurrentCamera < 0)?"N/A":str_get_hardware_camera_type_string(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraType));
   
   log_line("Current received temp model camera index: %d, type: %s", s_pEventsLastRecvModelSettings->iCurrentCamera,
      (s_pEventsLastRecvModelSettings->iCurrentCamera < 0)?"N/A":str_get_hardware_camera_type_string(s_pEventsLastRecvModelSettings->camera_params[s_pEventsLastRecvModelSettings->iCurrentCamera].iCameraType));
   log_line("Currently received temp model (VID: %u) controller ID: %u", s_pEventsLastRecvModelSettings->uVehicleId, s_pEventsLastRecvModelSettings->uControllerId);
   log_line("Currently received temp model (VID: %u) mode: %s. has negociated radio? %s", s_pEventsLastRecvModelSettings->uVehicleId, s_pEventsLastRecvModelSettings->is_spectator?"spectator mode":"control mode", (s_pEventsLastRecvModelSettings->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS)?"yes":"no");
   log_line("Currently received temp model osd layout: %d, enabled: %s", s_pEventsLastRecvModelSettings->osd_params.iCurrentOSDScreen, (s_pEventsLastRecvModelSettings->osd_params.osd_flags2[s_pEventsLastRecvModelSettings->osd_params.iCurrentOSDScreen] & OSD_FLAG2_LAYOUT_ENABLED)?"yes":"no");
   log_line("Currently received temp model developer flags: [%s]", str_get_developer_flags(s_pEventsLastRecvModelSettings->uDeveloperFlags));
   log_line("Currently received temp model has %d radio links.", s_pEventsLastRecvModelSettings->radioLinksParams.links_count );
   log_line("Currently received temp model has Ruby base version: %d.%d", (s_pEventsLastRecvModelSettings->hwCapabilities.uRubyBaseVersion >> 8) & 0xFF, s_pEventsLastRecvModelSettings->hwCapabilities.uRubyBaseVersion & 0xFF);
   log_line("Currently received temp model has %d cameras", s_pEventsLastRecvModelSettings->iCameraCount);
   
   for( int i=0; i<s_pEventsLastRecvModelSettings->radioLinksParams.links_count; i++ )
   {
      char szBuff[128];
      char szBuffC[128];

      str_get_radio_capabilities_description(s_pEventsLastRecvModelSettings->radioLinksParams.link_capabilities_flags[i], szBuffC);   
      str_get_radio_frame_flags_description(s_pEventsLastRecvModelSettings->radioLinksParams.link_radio_flags[i], szBuff);
      log_line("Currently received temp model info: radio link %d: %s, capabilities flags: %s, radio flags: %s", i+1, str_format_frequency(s_pEventsLastRecvModelSettings->radioLinksParams.link_frequency_khz[i]), szBuffC, szBuff);
   }

   if ( pCurrentlyStoredModel->uVehicleId != s_pEventsLastRecvModelSettings->uVehicleId )
   {
      Model* pTempModel = findModelWithId(s_pEventsLastRecvModelSettings->uVehicleId, 4);
      if ( NULL == pTempModel )
      {
         log_softerror_and_alarm("[Events] Received model settings for unknown VID %u (none in the list).", s_pEventsLastRecvModelSettings->uVehicleId);
         return false;
      }
      saveControllerModel(s_pEventsLastRecvModelSettings);
      warnings_add(s_pEventsLastRecvModelSettings->uVehicleId, L("Received vehicle settings."), g_idIconCheckOK);
      return true;
   }

   if ( bUnsolicited )
      warnings_add(pCurrentlyStoredModel->uVehicleId, L("Received vehicle settings."), g_idIconCheckOK);
   else
      warnings_add(pCurrentlyStoredModel->uVehicleId, L("Synchronized vehicle settings."), g_idIconCheckOK);

   log_line("The currently stored vehicle has Ruby version %d.%d (b-%d) and the controller %d.%d (b-%d)", get_sw_version_major(pCurrentlyStoredModel), get_sw_version_minor(pCurrentlyStoredModel), get_sw_version_build(pCurrentlyStoredModel), SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR, SYSTEM_SW_BUILD_NUMBER );

   bool bOldIsSpectator = pCurrentlyStoredModel->is_spectator;
   osd_parameters_t oldOSDParams;
   video_parameters_t oldVideoParams;
   memcpy((u8*)&oldOSDParams, (u8*)&(pCurrentlyStoredModel->osd_params), sizeof(osd_parameters_t));
   memcpy(&oldVideoParams, &(pCurrentlyStoredModel->video_params), sizeof(video_parameters_t));
   
   bool bMustRePair = _onEventCheckChangesToModel(pCurrentlyStoredModel, s_pEventsLastRecvModelSettings);
   
   pCurrentlyStoredModel->is_spectator = bOldIsSpectator;
   pCurrentlyStoredModel->b_mustSyncFromVehicle = false;
   log_line("[Events] Did set 'settings synchronized' flag to true for the vehicle (%u)", pCurrentlyStoredModel->uVehicleId);
   if ( pCurrentlyStoredModel->is_spectator )
   {
      log_line("Vehicle is spectator!");
      memcpy((u8*)&(pCurrentlyStoredModel->osd_params), (u8*)&oldOSDParams, sizeof(osd_parameters_t));
   }

   log_line("Current model (VID %u) mode: %s, has negociated radio? %s", g_pCurrentModel->uVehicleId, g_pCurrentModel->is_spectator?"spectator mode":"control mode", (g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS)?"yes":"no");
   log_line("Current model (VID %u) on time: %02d:%02d, total flights: %u", g_pCurrentModel->uVehicleId, g_pCurrentModel->m_Stats.uCurrentOnTime/60, g_pCurrentModel->m_Stats.uCurrentOnTime%60, g_pCurrentModel->m_Stats.uTotalFlights);
   log_line("Received model (VID %u) mode: %s", s_pEventsLastRecvModelSettings->uVehicleId, s_pEventsLastRecvModelSettings->is_spectator?"spectator mode":"control mode");
   log_line("Received model (VID %u) on time: %02d:%02d, flight time: %02d:%02d, total flights: %u", s_pEventsLastRecvModelSettings->uVehicleId, s_pEventsLastRecvModelSettings->m_Stats.uCurrentOnTime/60, s_pEventsLastRecvModelSettings->m_Stats.uCurrentOnTime%60, s_pEventsLastRecvModelSettings->m_Stats.uCurrentFlightTime/60, s_pEventsLastRecvModelSettings->m_Stats.uCurrentFlightTime%60, s_pEventsLastRecvModelSettings->m_Stats.uTotalFlights);
   
   if ( pCurrentlyStoredModel == g_pCurrentModel )
      log_line("[Events] Update received model to current model to storage...");
   else
      log_line("[Events] Update received model to storage...");
   
   s_pEventsLastRecvModelSettings->is_spectator = bOldIsSpectator;
   saveControllerModel(s_pEventsLastRecvModelSettings);
   setCurrentModel(pCurrentlyStoredModel->uVehicleId);
   g_pCurrentModel = getCurrentModel();

   log_line("[Events] Updated current local vehicle with received vehicle settings.");
   log_line("Currently stored model now (VID %u) radio links configuration:", g_pCurrentModel->uVehicleId);

   pCurrentlyStoredModel->logVehicleRadioInfo();

   save_ControllerInterfacesSettings();

   _onEventCheckModelAddNonBlockingPopupWarnings(pCurrentlyStoredModel, bUnsolicited);

   if ( pCurrentlyStoredModel->uVehicleId == g_pCurrentModel->uVehicleId )
   if ( g_bIsFirstConnectionToCurrentVehicle )
   {
      osd_add_stats_total_flights();
      g_bIsFirstConnectionToCurrentVehicle = false;
   }

   if ( bMustRePair )
   {
      char szText[128];
      snprintf(szText, sizeof(szText)/sizeof(szText[0]), L("Radio links configuration changed on your %s. Updating local radio configuration..."), pCurrentlyStoredModel->getVehicleTypeString());
      Popup* p = new Popup(szText, 0.15,0.5, 0.7, 5);
      p->setIconId(g_idIconRadio, get_Color_IconWarning());
      popups_add_topmost(p);

      log_line("[Events] Critical change in radio params on the received model settings. Restart pairing with the vehicle.");
      pairing_stop();
      hardware_sleep_ms(100);
      pairing_start_normal();
      if ( -1 != iRuntimeIndex )
      {
         u32 uVehicleId = g_VehiclesRuntimeInfo[iRuntimeIndex].uVehicleId;
         Model* pModelTmp = g_VehiclesRuntimeInfo[iRuntimeIndex].pModel;
         reset_vehicle_runtime_info(&(g_VehiclesRuntimeInfo[iRuntimeIndex]));
         g_VehiclesRuntimeInfo[iRuntimeIndex].uVehicleId = uVehicleId;
         g_VehiclesRuntimeInfo[iRuntimeIndex].pModel = pModelTmp;
      }
      log_line("[Events] Handing of event OnReceivedModelSettings complete.");
      return true;
   }

   log_line("[Events] No critical change that requires repairing on the received model settings.");
   log_line("Currenty stored model has negociated radio? %s", (pCurrentlyStoredModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS)?"yes":"no");
   log_line("Received model has negociated radio? %s", (s_pEventsLastRecvModelSettings->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS)?"yes":"no");

   log_line("[Events] Current controller model mode: %s", g_pCurrentModel->is_spectator?"spectator mode":"control mode");   
   log_line("[Events] Notify components to reload the new updated model. Updated model mode: %s", pCurrentlyStoredModel->is_spectator?"spectator mode":"control mode");
   send_model_changed_message_to_router(MODEL_CHANGED_SYNCHRONISED_SETTINGS_FROM_VEHICLE, 0);
   
   if ( (pCurrentlyStoredModel->video_params.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265) != (oldVideoParams.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265) )
   {
      log_line("Changed video codec. New codec: %s", (pCurrentlyStoredModel->video_params.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265)?"H265":"H264");
      send_model_changed_message_to_router(MODEL_CHANGED_VIDEO_CODEC, 0);
   }

   log_line("[Events] Handled of event OnReceivedModelSettings complete.");

   s_bEventTookPairingUIAction = _onEventCheckNewlyPairedModelForUIActionsToTake();
   if ( ! s_bEventTookPairingUIAction )
   {
      log_line("[Events] No pairing UI action to take.");
      if ( NULL != s_pEventsLastRecvModelSettings )
         delete s_pEventsLastRecvModelSettings;
      s_pEventsLastRecvModelSettings = NULL;
   }
   else
      log_line("[Events] Took a pairing UI action.");
   return true;
}

void onEventPairingDiscardAllUIActions()
{
   s_bEventTookPairingUIAction = false;
   s_bEventTookPairingUIActionUpdateVehicle = false;
   s_bEventTookPairingUIActionUpdateController = false;
   s_bEventTookPairingUIActionEncoderType = false;
   s_bEventTookPairingUIActionEncoderSlices = false;
   s_bEventTookPairingUIActionEncoderSpikes = false;
   s_bEventTookPairingUIActionNegociateRadio = false;
   s_bEventTookPairingUIActionUnitType = false;
   s_bEventTookPairingUIActionSimpleSetup = false;

   s_uTimeToCheckNextPairingUIAction = 0;
   if ( NULL != s_pEventsLastRecvModelSettings )
      delete s_pEventsLastRecvModelSettings;
   s_pEventsLastRecvModelSettings = NULL;
}

bool onEventPairingTookUIActions()
{
   return s_bEventTookPairingUIAction;
}

void onEventFinishedPairUIAction()
{
   if ( ! s_bEventTookPairingUIAction )
      return;
   u32 uTimeoutMs = 300;
   log_line("[Events] Finished pairing UI action. Will check again for next pairing UI actions in %u ms", uTimeoutMs);
   s_bEventTookPairingUIAction = false;
   s_uTimeToCheckNextPairingUIAction = g_TimeNow + uTimeoutMs;
}

void onEventCheckForPairPendingUIActionsToTake()
{
   if ( (0 == s_uTimeToCheckNextPairingUIAction) || s_bEventTookPairingUIAction )
      return;
   if ( g_TimeNow < s_uTimeToCheckNextPairingUIAction )
      return;
   s_uTimeToCheckNextPairingUIAction = 0;

   log_line("[Events] Timeout triggered to check for UI pairing actions. Check now.");
   if ( (NULL == g_pCurrentModel) || (NULL == s_pEventsLastRecvModelSettings) )
      return;
   if ( g_pCurrentModel->uVehicleId != s_pEventsLastRecvModelSettings->uVehicleId )
      return;
   s_bEventTookPairingUIAction = _onEventCheckNewlyPairedModelForUIActionsToTake();
   if ( ! s_bEventTookPairingUIAction )
   {
      log_line("[Events] No pairing UI action to take.");
      s_uTimeToCheckNextPairingUIAction = 0;
      if ( NULL != s_pEventsLastRecvModelSettings )
         delete s_pEventsLastRecvModelSettings;
      s_pEventsLastRecvModelSettings = NULL;
   }
   else
      log_line("[Events] Took a pairing UI action.");
}