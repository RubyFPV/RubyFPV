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

    Code contribution by: Jessica Severin
*/

#include "../base/base.h"
#include "../base/flags.h"
#include "../base/models_list.h"
#include "../base/ctrl_preferences.h"
#include "../common/string_utils.h"
#include "../common/favorites.h"
#include "quickactions.h"
#include "colors.h"
#include "ruby_central.h"
#include "events.h"
#include "pairing.h"
#include "handle_commands.h"
#include "link_watch.h"
#include "warnings.h"
#include "popup.h"
#include "osd_common.h"
#include "fonts.h"
#include "media.h"
#include "shared_vars.h"
#include "timers.h"
#include "process_router_messages.h"
#include "notifications.h"
#include "../radio/radiopackets2.h"

u32 s_uTimeLastQuickActionPress = 0;
u32 s_uLastQuickActionSwitchVideoProfile = 0;

bool quickActionCheckVehicle(const char* szText)
{
   bool bHasVehicle = false;
   if ( pairing_isStarted() && (NULL != g_pCurrentModel) )
   if ( link_is_vehicle_online_now(g_pCurrentModel->uVehicleId) )
   if ( link_has_received_main_vehicle_ruby_telemetry() )
      bHasVehicle = true;

   if ( bHasVehicle )
      return true;

   char szBuff[256];
   if ( NULL == szText || 0 == szText[0] )
      strcpy(szBuff, "You must be connected to a vehicle to use this Quick Action button function");
   else
      sprintf(szBuff, "You must be connected to a vehicle to %s", szText);

   Popup* p = new Popup(szBuff, 0.1,0.7, 0.54, 4);
   p->setCentered();
   p->setIconId(g_idIconInfo, get_Color_IconWarning());
   popups_add_topmost(p);
   return false;  
}

void executeQuickActionTakePicture()
{
   if ( get_current_timestamp_ms() < s_uTimeLastQuickActionPress + 500 )
      return;
   Preferences* p = get_Preferences();
   if ( NULL == p )
      return;

   s_uTimeLastQuickActionPress = get_current_timestamp_ms();
   media_take_screenshot(p->iAddOSDOnScreenshots);
}

void executeQuickActionRecord()
{
   if ( get_current_timestamp_ms() < s_uTimeLastQuickActionPress + 600 )
      return;
   s_uTimeLastQuickActionPress = get_current_timestamp_ms();

   if ( g_bIsVideoRecording )
   {
      ruby_stop_recording();
   }
   else
   {
      if ( ! link_is_vehicle_online_now(g_pCurrentModel->uVehicleId) )
      {
         Popup* p = new Popup("You must be connected to a vehicle to start video recording.", 0.1,0.8, 0.54, 5);
         p->setIconId(g_idIconError, get_Color_IconError());
         popups_add_topmost(p);
      }
      else
         ruby_start_recording();
   }
}

void executeQuickActionCycleOSD()
{
   //if ( ! quickActionCheckVehicle("cycle the OSD screens") )
   //   return;

   g_bHasVideoDecodeStatsSnapshot = false;

   Model* pModel = osd_get_current_layout_source_model();
   if ( NULL == pModel )
      return;
   log_line("Execute quick action to switch OSD screen for VID %u (%s): from layout %d to next one", pModel->uVehicleId, pModel->getShortName(), pModel->osd_params.iCurrentOSDScreen);

   int curentLayout = pModel->osd_params.iCurrentOSDScreen;
   int k=0; 
   while ( k < 10 )
   {
      k++;
      pModel->osd_params.iCurrentOSDScreen++;
      if ( pModel->osd_params.iCurrentOSDScreen >= osdLayoutLast )
         pModel->osd_params.iCurrentOSDScreen = osdLayout1;
      if ( pModel->osd_params.osd_flags2[pModel->osd_params.iCurrentOSDScreen] & OSD_FLAG2_LAYOUT_ENABLED )
         break; 
   }

   if ( curentLayout == pModel->osd_params.iCurrentOSDScreen )
   {
      char szBuff[128];
      sprintf(szBuff, "You have a single OSD screen enabled on %s. Enable more to be able to switch them.", pModel->getLongName());
      Popup* p = new Popup(szBuff, 0.1,0.7, 0.54, 4);
      p->setCentered();
      p->setIconId(g_idIconInfo, get_Color_IconWarning());
      popups_add_topmost(p);
      return;
   }

   osd_set_current_layout_index_and_source_model(pModel, pModel->osd_params.iCurrentOSDScreen);

   u32 scale = pModel->osd_params.osd_preferences[pModel->osd_params.iCurrentOSDScreen] & 0xFF;
   osd_setScaleOSD((int)scale);
   scale = (pModel->osd_params.osd_preferences[pModel->osd_params.iCurrentOSDScreen]>>16) & 0x0F;
   osd_setScaleOSDStats((int)scale);
   osd_apply_preferences();
   applyFontScaleChanges();

   saveControllerModel(pModel);
   save_Preferences();

   if ( pModel->osd_params.iCurrentOSDScreen == 0 )
      warnings_add(pModel->uVehicleId, "OSD Screen changed to Screen 1");
   if ( pModel->osd_params.iCurrentOSDScreen == 1 )
      warnings_add(pModel->uVehicleId, "OSD Screen changed to Screen 2");
   if ( pModel->osd_params.iCurrentOSDScreen == 2 )
      warnings_add(pModel->uVehicleId, "OSD Screen changed to Screen 3");
   if ( pModel->osd_params.iCurrentOSDScreen == 3 )
      warnings_add(pModel->uVehicleId, "OSD Screen changed to Screen Lean");
   if ( pModel->osd_params.iCurrentOSDScreen == 4 )
      warnings_add(pModel->uVehicleId, "OSD Screen changed to Screen Lean Extended");

   if ( pModel->is_spectator )
      return;

   handle_commands_abandon_command();
   handle_commands_send_single_oneway_command_to_vehicle(pModel->uVehicleId, 1, COMMAND_ID_SET_OSD_CURRENT_LAYOUT, (u32)pModel->osd_params.iCurrentOSDScreen, NULL, 0, 0);
   g_iMustSendCurrentActiveOSDLayoutCounter = 10; // send it 10 times, every 200 ms
   g_TimeLastSentCurrentActiveOSDLayout = g_TimeNow;

   send_model_changed_message_to_router(MODEL_CHANGED_OSD_PARAMS, 0);
}

void executeQuickActionRelaySwitch()
{
   if ( g_pCurrentModel->is_spectator )
   {
      warnings_add(0, "Can't execute relay switching while in spectator mode.");
      return;
   }
   if ( ! quickActionCheckVehicle("relay switch") )
      return;
   if ( get_current_timestamp_ms() < s_uTimeLastQuickActionPress + 500 )
      return;
   
   s_uTimeLastQuickActionPress = get_current_timestamp_ms();

   if ( ! link_is_vehicle_online_now(g_pCurrentModel->uVehicleId) )
   {
      Popup* p =new Popup("You must be connected to a vehicle to switch relaying!", 0.1,0.8, 0.54, 4);
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      return;
   }

   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId < 0 )
   {
      char szBuff[128];
      sprintf(szBuff, "Relaying is not enabled on this vehicle (%s)!", g_pCurrentModel->getLongName() );
      Popup* p = new Popup(szBuff, 0.1,0.8, 0.5, 4);
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      return;
   }

   // Switching to remote video stream?
   if ( ! (g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_REMOTE) )
   if ( ! link_is_relayed_vehicle_online() )
   {
      Popup* p = new Popup("Relayed vehicle is not online. Can't switch to relay it.", 0.1,0.8, 0.54, 4);
      p->setCentered();
      p->setBottomAlign(true);
      p->setBottomMargin(0.2);
      p->setIconId(g_idIconInfo, get_Color_IconWarning());
      popups_add_topmost(p);
      return;
   }
   
   type_relay_parameters newParams;
   memcpy((u8*)&newParams, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));
   u32 uOldRelayMode = newParams.uCurrentRelayMode;

   // Switch to relayed vehicle
   if ( newParams.uCurrentRelayMode & RELAY_MODE_MAIN )
   {
      newParams.uCurrentRelayMode &= ~RELAY_MODE_MAIN;
      newParams.uCurrentRelayMode |= RELAY_MODE_REMOTE;

      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( g_VehiclesRuntimeInfo[i].uVehicleId == g_pCurrentModel->relay_params.uRelayedVehicleId )
         {
            osd_set_current_data_source_vehicle_index(i);
            break;
         }
      }
   }
   // Switch to main vehicle
   else
   {
      newParams.uCurrentRelayMode &= ~RELAY_MODE_REMOTE;
      newParams.uCurrentRelayMode |= RELAY_MODE_MAIN;

      osd_set_current_data_source_vehicle_index(0);
   }

   if ( g_pCurrentModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_SWITCH_OSD )
   {
      Model * pModel = g_pCurrentModel;
      if ( newParams.uCurrentRelayMode & RELAY_MODE_REMOTE )
         pModel = findModelWithId(g_pCurrentModel->relay_params.uRelayedVehicleId, 50);
        
      if ( NULL != pModel )
      {
         osd_set_current_layout_index_and_source_model(pModel, pModel->osd_params.iCurrentOSDScreen);
         u32 scale = pModel->osd_params.osd_preferences[pModel->osd_params.iCurrentOSDScreen] & 0xFF;
         osd_setScaleOSD((int)scale);
         scale = (pModel->osd_params.osd_preferences[pModel->osd_params.iCurrentOSDScreen]>>16) & 0x0F;
         osd_setScaleOSDStats((int)scale);
         if ( render_engine_uses_raw_fonts() )
            applyFontScaleChanges();
      }
   }

   newParams.uCurrentRelayMode |= RELAY_MODE_IS_RELAY_NODE;

   log_line("Pressed QA button to switch relay mode from %d to %d (%s to %s)",
      uOldRelayMode, newParams.uCurrentRelayMode,
      str_format_relay_mode(uOldRelayMode), str_format_relay_mode(newParams.uCurrentRelayMode));

   handle_commands_abandon_command();
   handle_commands_send_to_vehicle(COMMAND_ID_SET_RELAY_PARAMETERS, 0, (u8*)&newParams, sizeof(type_relay_parameters));
}

void executeQuickActionSwitchFavoriteVehicle()
{
   if ( ! vehicle_is_favorite(g_pCurrentModel->uVehicleId) )
   {
      warnings_add(0, "This vehicle is not a favorite vehicle. Add it to favorites to be able to cycle through favorites.");
      return;
   }
   u32 uOldVehicleId = g_pCurrentModel->uVehicleId;
   u32 uVehicleId = get_next_favorite(g_pCurrentModel->uVehicleId);
   Model* pNewVehicle = findModelWithId(uVehicleId, 51);
   if ( NULL == pNewVehicle )
   {
      warnings_add(0, "Favorite vehicle does not exist. Check your favorites vehicles list.");
      return;
   }

   if ( uVehicleId == g_pCurrentModel->uVehicleId )
   {
      warnings_add(0, "You have a single favorite vehicle. Add at least two in order to be able to cycle through favorites.");
      return;
   }

   log_line("--------------------------------------------------------------------------------------------");
   log_line("Starting QA favorite action to switch current vehicle from VID %u to %u...", uOldVehicleId, uVehicleId);

   g_bSwitchingFavoriteVehicle = true;
   Model* pOldVehicle = g_pCurrentModel;

   setCurrentModel(pNewVehicle->uVehicleId);
   g_pCurrentModel = getCurrentModel();

   setControllerCurrentModel(g_pCurrentModel->uVehicleId);
   saveControllerModel(g_pCurrentModel);

   ruby_set_active_model_id(g_pCurrentModel->uVehicleId);
   
   onMainVehicleChanged(false);

   //shared_vars_state_reset_all_vehicles_runtime_info();
   //g_VehiclesRuntimeInfo[0].uVehicleId = g_pCurrentModel->uVehicleId;
   //g_VehiclesRuntimeInfo[0].pModel = g_pCurrentModel;
   //g_iCurrentActiveVehicleRuntimeInfoIndex = 0;
   
   int iIndexRuntime = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == g_pCurrentModel->uVehicleId )
      {
         iIndexRuntime = i;
         break;
      }
   }
   if ( -1 == iIndexRuntime )
   {
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( g_VehiclesRuntimeInfo[i].uVehicleId == 0 )
         {
            iIndexRuntime = i;
            break;
         }
      }
   }
   if ( -1 == iIndexRuntime )
   {
      iIndexRuntime = MAX_CONCURENT_VEHICLES-1;
      log_softerror_and_alarm("QuickAction: No more room in vehicles runtime info structure for new vehicle VID %u. Reuse last index: %d", g_pCurrentModel->uVehicleId, iIndexRuntime);
      char szTmp[256];
      szTmp[0] = 0;
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         char szT[32];
         sprintf(szT, "%u", g_VehiclesRuntimeInfo[i].uVehicleId);
         if ( 0 != i )
            strcat(szTmp, ", ");
         strcat(szTmp, szT);
      }
      log_softerror_and_alarm("QuickAction: Current vehicles in vehicles runtime info: [%s]", szTmp);
   }
   reset_vehicle_runtime_info(&(g_VehiclesRuntimeInfo[iIndexRuntime]));
   g_VehiclesRuntimeInfo[iIndexRuntime].uVehicleId = g_pCurrentModel->uVehicleId;
   g_VehiclesRuntimeInfo[iIndexRuntime].pModel = g_pCurrentModel;

   t_structure_vehicle_info tmp;
   memcpy( &tmp, &(g_VehiclesRuntimeInfo[0]), sizeof(t_structure_vehicle_info));
   memcpy( &(g_VehiclesRuntimeInfo[0]), &(g_VehiclesRuntimeInfo[iIndexRuntime]), sizeof(t_structure_vehicle_info));
   memcpy( &(g_VehiclesRuntimeInfo[iIndexRuntime]), &tmp, sizeof(t_structure_vehicle_info));
   g_iCurrentActiveVehicleRuntimeInfoIndex = 0;

   log_line("Send message to router to switch favorite vehicle from VID %u [%s] to %u [%s]", uOldVehicleId, pOldVehicle->getShortName(), uVehicleId, pNewVehicle->getShortName());
   send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_SWITCH_FAVORIVE_VEHICLE, uVehicleId);
   char szBuff[256];
   sprintf(szBuff, "Switching to favorite vehicle: %s", pNewVehicle->getLongName());
   warnings_add(0, szBuff); 

   log_line("Finished QA favorite action to switch current vehicle from VID %u to %u", uOldVehicleId, uVehicleId);
   log_line("--------------------------------------------------------------------------------------------");
}

void executeQuickActionSwitchPITMode()
{
   if ( g_pCurrentModel->is_spectator )
   {
      warnings_add(0, "Can't switch PIT mode while in spectator mode.");
      return;
   }
   if ( ! quickActionCheckVehicle("switch PIT mode") )
      return;
   if ( get_current_timestamp_ms() < s_uTimeLastQuickActionPress + 500 )
      return;

   u32 uPITFlags = g_pCurrentModel->radioInterfacesParams.uFlagsRadioInterfaces & 0xFF;

   if ( ! (uPITFlags && RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE) )
   {
      warnings_add(0, "Can't switch PIT mode, PIT mode is disabled. Enable it from menus.");
      return;
   }

   bool bIsInPITMode = false;
   if ( uPITFlags & RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE_MANUAL )
      bIsInPITMode = true;

   bIsInPITMode = ! bIsInPITMode;

   if ( bIsInPITMode )
       uPITFlags |= RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE_MANUAL;
   else
       uPITFlags &= ~RADIO_INTERFACES_FLAGS_PIT_MODE_ENABLE_MANUAL;

   u32 uCommandParam = uPITFlags;
   uCommandParam |= (g_pCurrentModel->hwCapabilities.uHWFlags & 0xFF00);
   if ( g_pCurrentModel->radioInterfacesParams.iAutoVehicleTxPower )
      uCommandParam |= ((u32)(0x01))<<16;
   if ( g_pCurrentModel->radioInterfacesParams.iAutoControllerTxPower )
      uCommandParam |= ((u32)(0x01))<<17;

   handle_commands_abandon_command();
   //handle_commands_send_single_oneway_command_to_vehicle(g_pCurrentModel->uVehicleId, 2, COMMAND_ID_SET_PIT_AUTO_TX_POWERS_FLAGS, uCommandParam, NULL, 0, 0);
   handle_commands_send_to_vehicle(COMMAND_ID_SET_PIT_AUTO_TX_POWERS_FLAGS, uCommandParam, NULL, 0);
}



void executeQuickActionToggleRCEnabled()
{
   if ( (NULL != g_pCurrentModel) && g_pCurrentModel->is_spectator )
   {
      warnings_add(0, L("Can't enable RC while in spectator mode."));
      return;
   }
   if ( ! quickActionCheckVehicle("enable/disable the RC link output") )
      return;

   rc_parameters_t params;
   memcpy(&params, &g_pCurrentModel->rc_params, sizeof(rc_parameters_t));

   if ( params.flags & RC_FLAGS_OUTPUT_ENABLED )
      params.flags &= (~RC_FLAGS_OUTPUT_ENABLED);
   else
      params.flags |= RC_FLAGS_OUTPUT_ENABLED;
   handle_commands_abandon_command();
   handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t));
}


void executeQuickActionCameraProfileSwitch()
{
   if ( g_pCurrentModel->is_spectator )
   {
      warnings_add(0, L("Can't switch camera profile for spectator vehicles."));
      return;
   }
   if ( handle_commands_is_command_in_progress() )
   {
      return;
   }

   int iProfileOrg = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;
   int iProfile = iProfileOrg;
   camera_profile_parameters_t* pProfile1 = &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iProfile]);
   iProfile++;
   if ( iProfile >= MODEL_CAMERA_PROFILES-1 )
      iProfile = 0;

   camera_profile_parameters_t* pProfile2 = &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iProfile]);

   //char szBuff[64];
   //sprintf(szBuff, "Switching to camera profile %s", model_getCameraProfileName(iProfile));
   //warnings_add(g_pCurrentModel->uVehicleId, szBuff);

   g_pCurrentModel->log_camera_profiles_differences(pProfile1, pProfile2, iProfileOrg, iProfile);

   handle_commands_send_to_vehicle(COMMAND_ID_SET_CAMERA_PROFILE, iProfile, NULL, 0);
   return;
}


void executeQuickActionOSDSize()
{
   if ( ! quickActionCheckVehicle("change OSD size") )
      return;

   Preferences* pP = get_Preferences();
   pP->iScaleOSD++;
   if ( pP->iScaleOSD > 3 )
      pP->iScaleOSD = -1;
   save_Preferences();
   osd_apply_preferences();
   return;
}

