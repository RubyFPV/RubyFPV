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
   m_Width = 0.19;
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
   
   m_pItemAction.clear();

   int m_index = 0;
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

   if(pP->uEnabledQuickMenu & MenuQuickMenu::ToggleOSD)
   {
      if(g_bToglleOSDOff)
         m_index = addMenuItem(new MenuItem(L("Toggle OSD On")));
      else
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
}

void MenuQuickMenu::Render()
{
   RenderPrepare();
   float yEnd = RenderFrameAndTitle();
   float y = yEnd;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yEnd);
}

void MenuQuickMenu::onSelectItem()
{
   Menu::onSelectItem();
   if (-1 == m_SelectedIndex)
     return;

   if ( (!pairing_isStarted()) || (! g_bIsRouterReady) )
   {
      warnings_add(0, L("Please connect to a vehicle first, to execute Quick Actions."));
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
         executeQuickActionTakePicture();
         break;
      case VideoRecording:
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

   menu_discard_all();
}

