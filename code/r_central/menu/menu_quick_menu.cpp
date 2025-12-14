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

#include "osd_common.h"
#include "../launchers_controller.h"
#include "../link_watch.h"
#include <ctype.h>

int MenuQuickMenu::iPrevSelectedItem = 0;

MenuQuickMenu::MenuQuickMenu(void)
:Menu(MENU_ID_QUICK_MENU, L("Quick Menu"), NULL)

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

   if(pP->uEnabledQuickMenu & MenuQuickMenu::CycleOSDScreen)
   {
      int m_index = addMenuItem(new MenuItem(L("Cycle OSD screen")));
      m_pItemAction[m_index] = MenuQuickMenu::CycleOSDScreen;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::CycleOSDSize)
   {
      int m_index = addMenuItem(new MenuItem(L("Cycle OSD size")));
      m_pItemAction[m_index] = MenuQuickMenu::CycleOSDSize;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::TakePicture)
   {
      int m_index = addMenuItem(new MenuItem(L("Take Picture")));
      m_pItemAction[m_index] = MenuQuickMenu::TakePicture;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::VideoRecording)
   {
      int m_index = addMenuItem(new MenuItem(L("Video Recording")));
      m_pItemAction[m_index] = MenuQuickMenu::VideoRecording;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::ToggleOSDOff)
   {
      int m_index = addMenuItem(new MenuItem(L("Toggle OSD Off")));
      m_pItemAction[m_index] = MenuQuickMenu::ToggleOSDOff;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::ToggleStatsOff)
   {
      int m_index = addMenuItem(new MenuItem(L("Toggle Stats Off")));
      m_pItemAction[m_index] = MenuQuickMenu::ToggleStatsOff;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::ToggleAllOff)
   {
      int m_index = addMenuItem(new MenuItem(L("Toggle All Off")));
      m_pItemAction[m_index] = MenuQuickMenu::ToggleAllOff;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::RelaySwitch)
   {
      int m_index = addMenuItem(new MenuItem(L("Relay Switch")));
      m_pItemAction[m_index] = MenuQuickMenu::RelaySwitch;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::SwitchCameraProfile)
   {
      int m_index = addMenuItem(new MenuItem(L("Switch Camera Profile")));
      m_pItemAction[m_index] = MenuQuickMenu::SwitchCameraProfile;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::RCOutputOnOff)
   {
      int m_index = addMenuItem(new MenuItem(L("RC Output On/Off")));
      m_pItemAction[m_index] = MenuQuickMenu::RCOutputOnOff;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::RotaryEncoderFunction)
   {
      int m_index = addMenuItem(new MenuItem(L("Rotary Encoder Function")));
      m_pItemAction[m_index] = MenuQuickMenu::RotaryEncoderFunction;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::FreezeOSD)
   {
      int m_index = addMenuItem(new MenuItem(L("Freeze OSD")));
      m_pItemAction[m_index] = MenuQuickMenu::FreezeOSD;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::CycleFavoriteVehicles)
   {
      int m_index = addMenuItem(new MenuItem(L("Cycle Favorite Vehicles")));
      m_pItemAction[m_index] = MenuQuickMenu::CycleFavoriteVehicles;
   }

   if(pP->uEnabledQuickMenu & MenuQuickMenu::PITMode)
   {
      int m_index = addMenuItem(new MenuItem(L("PIT Mode")));
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

   MenuQuickMenu::iPrevSelectedItem = m_SelectedIndex;

   t_quick_menu_actions  action = m_pItemAction[m_SelectedIndex];

   switch(action)
   {
      case None:
         break;
      case CycleOSDScreen:
         break;
      case CycleOSDSize:
         break;
      case TakePicture:
         warnings_add(0, "QuickMenu TakePicture");
         break;
      case VideoRecording:
         warnings_add(0, "QuickMenu VideoRecording");
         break;
      case ToggleOSDOff:
         break;
      case ToggleStatsOff:
         break;
      case ToggleAllOff:
         break;
      case RelaySwitch:
         break;
      case SwitchCameraProfile:
         break;
      case RCOutputOnOff:
         break;
      case RotaryEncoderFunction:
         break;
      case FreezeOSD:
         break;
      case CycleFavoriteVehicles:
         break;
      case PITMode:
         warnings_add(0, "QuickMenu PITMode");
         break;

   }




   // if ( m_iIndexController == m_SelectedIndex )
   //    add_menu_to_stack(new MenuController());

   menu_discard_all();
}
