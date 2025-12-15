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

#include "menu.h"
#include "menu_quick_menu.h"
#include "menu_search.h"
#include "menu_vehicles.h"
#include "menu_spectator.h"
#include "menu_vehicle.h"
#include "menu_preferences_buttons.h"
#include "menu_controller.h"
#include "menu_controller_peripherals.h"
#include "menu_vehicle_simplesetup.h"
#include "menu_storage.h"
#include "menu_system.h"
#include "menu_radio_config.h"
#include "menu_item_text.h"
#include "../quickactions.h"

#include "osd_common.h"
#include "../launchers_controller.h"
#include "../link_watch.h"
#include <ctype.h>

int MenuQuickMenu::iPrevSelectedItem = 0;

MenuQuickMenu::MenuQuickMenu(void)
:Menu(MENU_ID_QUICK_MENU, L("Quick Action Menu"), NULL)

{
   m_Width = 0.164;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.30;
}

MenuQuickMenu::~MenuQuickMenu()
{
   log_line("Quick Menu Closed.");
}

void MenuQuickMenu::onShow()
{
   //log_line("MenuQuickMenu: onShow...");
   
   addItems();
   Menu::onShow();

   if ( MenuQuickMenu::iPrevSelectedItem >= 0 )
   {
      m_SelectedIndex = MenuQuickMenu::iPrevSelectedItem;
      onFocusedItemChanged();
   }
   //log_line("Entered root menu.");
}

void MenuQuickMenu::addItems()
{
   removeAllItems();

   Preferences* pP = get_Preferences();
   
   //m_iIndexController =
   //addMenuItem(new MenuItem(L("Take Picture")));
   //addMenuItem(new MenuItem(L("Video Recording")));
   //addMenuItem(new MenuItem(L("Toggle OSD Off")));

   m_pItemAction.clear();

   int m_index = 0;
   if(pP->uEnabledQuickMenu & MenuQuickMenu::CycleOSDScreen)
   {
      m_index = addMenuItem(new MenuItem(L("Cycle OSD screen")));
      m_pItemAction[m_index] = MenuQuickMenu::CycleOSDScreen;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::CycleOSDSize)
   {
      m_index = addMenuItem(new MenuItem(L("Cycle OSD size")));
      m_pItemAction[m_index] = MenuQuickMenu::CycleOSDSize;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::TakePicture)
   {
      m_index = addMenuItem(new MenuItem(L("Take Picture")));
      m_pItemAction[m_index] = MenuQuickMenu::TakePicture;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::VideoRecording)
   {
      m_index = addMenuItem(new MenuItem(L("Video Recording")));
      m_pItemAction[m_index] = MenuQuickMenu::VideoRecording;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::ToggleOSD)
   {
      m_index = addMenuItem(new MenuItem(L("Toggle OSD Off")));
      m_pItemAction[m_index] = MenuQuickMenu::ToggleOSD;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::ToggleStatsOff)
   {
      if(g_bToglleStatsOff)
         m_index = addMenuItem(new MenuItem(L("Toggle Stats On")));
      else
         m_index = addMenuItem(new MenuItem(L("Toggle Stats Off")));
      m_pItemAction[m_index] = MenuQuickMenu::ToggleStatsOff;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::ToggleAllOff)
   {
      if(g_bToglleAllOSDOff)
         m_index = addMenuItem(new MenuItem(L("Toggle All On")));
      else
         m_index = addMenuItem(new MenuItem(L("Toggle All Off")));
      m_pItemAction[m_index] = MenuQuickMenu::ToggleAllOff;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::RelaySwitch)
   {
      m_index = addMenuItem(new MenuItem(L("Relay Switch")));
      m_pItemAction[m_index] = MenuQuickMenu::RelaySwitch;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::SwitchCameraProfile)
   {
      m_index = addMenuItem(new MenuItem(L("Switch Camera Profile")));
      m_pItemAction[m_index] = MenuQuickMenu::SwitchCameraProfile;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::RCOutputOnOff)
   {
      m_index = addMenuItem(new MenuItem(L("RC Output On/Off")));
      m_pItemAction[m_index] = MenuQuickMenu::RCOutputOnOff;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::RotaryEncoderFunction)
   {
      m_index = addMenuItem(new MenuItem(L("Rotary Encoder Function")));
      m_pItemAction[m_index] = MenuQuickMenu::RotaryEncoderFunction;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::FreezeOSD)
   {
      m_index = addMenuItem(new MenuItem(L("Freeze OSD")));
      m_pItemAction[m_index] = MenuQuickMenu::FreezeOSD;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::CycleFavoriteVehicles)
   {
      m_index = addMenuItem(new MenuItem(L("Cycle Favorite Vehicles")));
      m_pItemAction[m_index] = MenuQuickMenu::CycleFavoriteVehicles;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::PITMode)
   {
      m_index = addMenuItem(new MenuItem(L("PIT Mode")));
      m_pItemAction[m_index] = MenuQuickMenu::PITMode;
   }

   
   // m_pMenuItems[m_ItemsCount-1]->setExtraHeight(m_sfMenuPaddingY);
   // char szBuff[256];
   // char szBuff2[64];
   // getSystemVersionString(szBuff2, (SYSTEM_SW_VERSION_MAJOR<<8) | SYSTEM_SW_VERSION_MINOR);
   // sprintf(szBuff, "Version %s (b-%d)", szBuff2, SYSTEM_SW_BUILD_NUMBER);
   //
   // addMenuItem(new MenuItemText(szBuff, true, 0.01 * Menu::getScaleFactor()));
   // sprintf(szBuff, "Running on: %s", str_get_hardware_board_name_short(hardware_getBoardType()));
   // addMenuItem(new MenuItemText(szBuff, true, 0.01 * Menu::getScaleFactor()));
}



void MenuQuickMenu::Render()
{
   RenderPrepare();
   float yEnd = RenderFrameAndTitle();
   float y = yEnd;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yEnd);

//    RenderPrepare();
//
//    if ( Menu::getRenderMode() != 1 )
//       m_RenderHeight -= 1.14 * g_pRenderEngine->textHeight(g_idFontMenu);
//
//    float yTop = RenderFrameAndTitle();
//    float y = yTop;
//
//    if ( Menu::getRenderMode() != 1 )
//       m_RenderHeight += 1.14 * g_pRenderEngine->textHeight(g_idFontMenu);
//
//    bool bTmp1 = m_bEnableScrolling;
//    bool bTmp2 = m_bHasScrolling;
//    m_bEnableScrolling = false;
//    m_bHasScrolling = false;
//
//    int iItem = 0;
//    float yTopItems = m_RenderYPos - 0.02*m_sfMenuPaddingY - 1.2*g_pRenderEngine->textHeight(g_idFontMenu) - 1.5*m_sfMenuPaddingY;
//    yTopItems += RenderItem(iItem, yTopItems);
//    iItem++;
//    //yTopItems += RenderItem(iItem, yTopItems);
//    //iItem++;
//
//    m_bEnableScrolling = bTmp1;
//    m_bHasScrolling = bTmp2;
//
//    for(; iItem<m_ItemsCount; iItem++ )
//    {
//       if ( iItem == m_ItemsCount-2 )
//          y += 0.5*m_sfMenuPaddingY;
//       y += RenderItem(iItem,y);
//    }
//    RenderEnd(yTop);
}

void MenuQuickMenu::onSelectItem()
{
   Menu::onSelectItem();
   if (-1 == m_SelectedIndex)
     return;

   if ( (!pairing_isStarted()) || (! g_bIsRouterReady) )
   {
      warnings_add(0, "Please connect to a vehicle first, to execute Quick Actions.");
      return;
   }

   MenuQuickMenu::iPrevSelectedItem = m_SelectedIndex;

   t_quick_menu_actions  action = m_pItemAction[m_SelectedIndex];

   switch(action)
   {
      case None:
         break;
      case CycleOSDScreen:
         executeQuickActionCycleOSD();
         break;
      case CycleOSDSize:
         executeQuickActionOSDSize();
         break;
      case TakePicture:
         //warnings_add(0, "QuickMenu TakePicture");
         //log_line("QuickMenu TakePicture.");
         executeQuickActionTakePicture();
         break;
      case VideoRecording:
         //warnings_add(0, "QuickMenu VideoRecording");
         //log_line("QuickMenu VideoRecording.");
         executeQuickActionRecord();
         break;
      case ToggleOSD:
         if ( quickActionCheckVehicle("toggle the OSD") )
            g_bToglleOSDOff = ! g_bToglleOSDOff;
         break;
      case ToggleStatsOff:
         if ( quickActionCheckVehicle("toggle the statistics") )
            g_bToglleStatsOff = ! g_bToglleStatsOff;
         break;
      case ToggleAllOff:
         if ( quickActionCheckVehicle("toggle all info on/off") )
            g_bToglleAllOSDOff = ! g_bToglleAllOSDOff;
         break;
      case RelaySwitch:
         executeQuickActionRelaySwitch();
         break;
      case SwitchCameraProfile:
         executeQuickActionCameraProfileSwitch();
         break;
      case RCOutputOnOff:
         executeQuickActionToggleRCEnabled();
         break;
      case RotaryEncoderFunction:
         g_pControllerSettings->nRotaryEncoderFunction++;
         if ( g_pControllerSettings->nRotaryEncoderFunction > 2 )
            g_pControllerSettings->nRotaryEncoderFunction = 1;
         save_ControllerSettings();
         if ( 0 == g_pControllerSettings->nRotaryEncoderFunction )
            warnings_add(0, "Rotary Encoder function changed to: None");
         if ( 1 == g_pControllerSettings->nRotaryEncoderFunction )
            warnings_add(0, "Rotary Encoder function changed to: Menu Navigation");
         if ( 2 == g_pControllerSettings->nRotaryEncoderFunction )
            warnings_add(0, "Rotary Encoder function changed to: Camera Adjustment");
         break;
      case FreezeOSD:
         g_bFreezeOSD = ! g_bFreezeOSD;
         break;
      case CycleFavoriteVehicles:
         executeQuickActionSwitchFavoriteVehicle();
         break;
      case PITMode:
         //warnings_add(0, "QuickMenu PITMode");
         log_line("QuickMenu PITMode.");
         executeQuickActionSwitchPITMode();
         break;
   }
   // if ( m_iIndexController == m_SelectedIndex )
   //    add_menu_to_stack(new MenuController());

   menu_discard_all();
}


void MenuQuickMenu::executeQuickActionToggleRCEnabled()
{
   if ( (NULL != g_pCurrentModel) && g_pCurrentModel->is_spectator )
   {
      warnings_add(0, "Can't enable RC while in spectator mode.");
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


void MenuQuickMenu::executeQuickActionCameraProfileSwitch()
{
   if ( g_pCurrentModel->is_spectator )
   {
      warnings_add(0, "Can't switch camera profile for spectator vehicles.");
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


void MenuQuickMenu::executeQuickActionOSDSize()
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



/*
void executeQuickActions()
{
   Preferences* p = get_Preferences();
   if ( NULL == p )
      return;
   if ( g_bIsReinit || g_bSearching )
      return;
   if ( NULL == g_pCurrentModel )
   {
      Popup* p = new Popup("You must be connected to a vehicle to execute Quick Actions.", 0.1,0.8, 0.54, 5);
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      return;
   }

   log_force_full_log();

   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA1  )
      log_line("Pressed button QA1");
   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA2  )
      log_line("Pressed button QA2");
   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA3  )
      log_line("Pressed button QA3");

   log_line("Current assigned QA actions: button1: %d, button2: %d, button3: %d",
    p->iActionQuickButton1,p->iActionQuickButton2,p->iActionQuickButton3);

   log_regular_mode();


   if ( ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA1) && quickActionOSDSize == p->iActionQuickButton1) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA2) && quickActionOSDSize == p->iActionQuickButton2) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA3) && quickActionOSDSize == p->iActionQuickButton3) )
   {
      if ( ! quickActionCheckVehicle("change OSD size") )
         return;
      p->iScaleOSD++;
      if ( p->iScaleOSD > 3 )
         p->iScaleOSD = -1;
      save_Preferences();
      osd_apply_preferences();
      return;
   }


   #ifdef FEATURE_ENABLE_RC
   if ( NULL != g_pCurrentModel )
   if ( ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA1) && quickActionRCEnable == p->iActionQuickButton1) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA2) && quickActionRCEnable == p->iActionQuickButton2) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA3) && quickActionRCEnable == p->iActionQuickButton3) )
   {
      if ( (NULL != g_pCurrentModel) && g_pCurrentModel->is_spectator )
      {
         warnings_add(0, "Can't enable RC while in spectator mode.");
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
      return;
   }
   #endif

   if ( ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA1) && quickActionCameraProfileSwitch == p->iActionQuickButton1) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA2) && quickActionCameraProfileSwitch == p->iActionQuickButton2) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA3) && quickActionCameraProfileSwitch == p->iActionQuickButton3) )
   {
      if ( g_pCurrentModel->is_spectator )
      {
         warnings_add(0, "Can't switch camera profile for spectator vehicles.");
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

   if ( ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA1) && quickActionRotaryFunction == p->iActionQuickButton1) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA2) && quickActionRotaryFunction == p->iActionQuickButton2) ||
        ((keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_QA3) && quickActionRotaryFunction == p->iActionQuickButton3) )
   {
      g_pControllerSettings->nRotaryEncoderFunction++;
      if ( g_pControllerSettings->nRotaryEncoderFunction > 2 )
         g_pControllerSettings->nRotaryEncoderFunction = 1;
      save_ControllerSettings();
      if ( 0 == g_pControllerSettings->nRotaryEncoderFunction )
         warnings_add(0, "Rotary Encoder function changed to: None");
      if ( 1 == g_pControllerSettings->nRotaryEncoderFunction )
         warnings_add(0, "Rotary Encoder function changed to: Menu Navigation");
      if ( 2 == g_pControllerSettings->nRotaryEncoderFunction )
         warnings_add(0, "Rotary Encoder function changed to: Camera Adjustment");

      return;
   }

}
*/

