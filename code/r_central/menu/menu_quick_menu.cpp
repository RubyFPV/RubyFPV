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


const char* s_textRoot[] = { "Welcome to", SYSTEM_NAME, NULL };


MenuQuickMenu::MenuQuickMenu(void)
:Menu(MENU_ID_ROOT, SYSTEM_NAME, NULL)
{
   m_Width = 0.164;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.42;
   //m_bFullWidthSelection = true;

   if ( 1 == Menu::getRenderMode() )
      addExtraHeightAtStart(0.2);
}

MenuQuickMenu::~MenuQuickMenu()
{
   log_line("Menu Closed.");
}

void MenuQuickMenu::onShow()
{
   int iPrevSelectedItem = m_SelectedIndex;
   log_line("MenuQuickMenu: onShow...");
   
   load_Preferences();
   addItems();
   Menu::onShow();

   if ( iPrevSelectedItem >= 0 )
   {
      m_SelectedIndex = iPrevSelectedItem;
      onFocusedItemChanged();
   }
   log_line("Entered root menu.");
}

void MenuQuickMenu::addItems()
{
   removeAllItems();
   m_iIndexSpectator = -1;

   Preferences* pP = get_Preferences();
   
   if ( pP->iShowCompactMenus )
      m_iIndexSimpleSetup = addMenuItem(new MenuItem(L("Quick vehicle setup"), L("Quickly change the most common vehicle settings.")));
   else
      m_iIndexSimpleSetup = addMenuItem(new MenuItem(L("Vehicle Settings"), L("Change current vehicle settings.")));
   m_iIndexMyVehicles = addMenuItem(new MenuItem(L("My vehicles"), L("Manage my vehicles.")));
   m_iIndexSearch = addMenuItem(new MenuItem(L("Search"), L("Search for vehicles.")));
   addSeparator();
   //m_iIndexSpectator = addMenuItem(new MenuItem("Spectator Vehicles", "See the list of vehicles you recently connected to as a spectator."));

   if ( pP->iShowCompactMenus )
      m_iIndexVehicle = addMenuItem(new MenuItem(L("Vehicle settings"), L("Change vehicle settings.")));
   else
      m_iIndexVehicle = -1;
   m_iIndexController = addMenuItem(new MenuItem(L("Controller settings"), L("Change controller settings and user interface preferences.")));
   m_iIndexSystem = addMenuItem(new MenuItem(L("System"), L("Configure system options, shows detailed information about the system.")));
   addSeparator();
   m_iIndexMedia = addMenuItem(new MenuItem(L("Media & Storage"), L("Manage saved logs, screenshots and videos.")));
   
   m_pMenuItems[m_ItemsCount-1]->setExtraHeight(m_sfMenuPaddingY);
   char szBuff[256];
   char szBuff2[64];
   getSystemVersionString(szBuff2, (SYSTEM_SW_VERSION_MAJOR<<8) | SYSTEM_SW_VERSION_MINOR);
   sprintf(szBuff, "Version %s (b-%d)", szBuff2, SYSTEM_SW_BUILD_NUMBER);

   addMenuItem(new MenuItemText(szBuff, true, 0.01 * Menu::getScaleFactor()));
   sprintf(szBuff, "Running on: %s", str_get_hardware_board_name_short(hardware_getBoardType()));
   addMenuItem(new MenuItemText(szBuff, true, 0.01 * Menu::getScaleFactor()));
}



void MenuQuickMenu::Render()
{
   RenderPrepare();

   if ( Menu::getRenderMode() != 1 )
      m_RenderHeight -= 1.14 * g_pRenderEngine->textHeight(g_idFontMenu);

   float yTop = RenderFrameAndTitle();
   float y = yTop;

   if ( Menu::getRenderMode() != 1 )
      m_RenderHeight += 1.14 * g_pRenderEngine->textHeight(g_idFontMenu);
   
   bool bTmp1 = m_bEnableScrolling;
   bool bTmp2 = m_bHasScrolling;
   m_bEnableScrolling = false;
   m_bHasScrolling = false;

   int iItem = 0;
   float yTopItems = m_RenderYPos - 0.02*m_sfMenuPaddingY - 1.2*g_pRenderEngine->textHeight(g_idFontMenu) - 1.5*m_sfMenuPaddingY;
   yTopItems += RenderItem(iItem, yTopItems);
   iItem++;
   //yTopItems += RenderItem(iItem, yTopItems);
   //iItem++;
 
   m_bEnableScrolling = bTmp1;
   m_bHasScrolling = bTmp2;

   for(; iItem<m_ItemsCount; iItem++ )
   {
      if ( iItem == m_ItemsCount-2 )
         y += 0.5*m_sfMenuPaddingY;
      y += RenderItem(iItem,y);
   }
   RenderEnd(yTop);
}

void MenuQuickMenu::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( m_iIndexSimpleSetup == m_SelectedIndex )
   {
      if ( (NULL == g_pCurrentModel) || (0 == g_uActiveControllerModelVID) ||
        (g_bFirstModelPairingDone && (0 == getControllerModelsCount()) && (0 == getControllerModelsSpectatorCount())) )
      {
         addMessage2(0, L("Not paired with any vehicle"), L("Search for vehicles to find one and connect to."));
         return;
      }

      if ( (!pairing_isStarted()) || (NULL == g_pCurrentModel) || (!link_is_vehicle_online_now(g_pCurrentModel->uVehicleId)) )
      {
         addMessage2(0, "Not connected to a vehicle.", "Can't change settings when not connected to the vehicle. Connect to a vehicle first.");
         return;
      }

      Preferences* pP = get_Preferences();
      if ( pP->iShowCompactMenus )
         add_menu_to_stack(new MenuVehicleSimpleSetup());
      else
         add_menu_to_stack(new MenuVehicle());
      return;
   }

   if ( (-1 != m_iIndexVehicle) && (m_iIndexVehicle == m_SelectedIndex) )
   {
      if ( (NULL == g_pCurrentModel) || (0 == g_uActiveControllerModelVID) ||
        (g_bFirstModelPairingDone && (0 == getControllerModelsCount()) && (0 == getControllerModelsSpectatorCount())) )
      {
         addMessage2(0, L("Not paired with any vehicle"), L("Search for vehicles to find one and connect to."));
         return;
      }
      add_menu_to_stack(new MenuVehicle());
      return;
   }

   if ( m_iIndexMyVehicles == m_SelectedIndex )
         add_menu_to_stack(new MenuVehicles());

   if ( (-1 != m_iIndexSpectator) && (m_iIndexSpectator == m_SelectedIndex) )
         add_menu_to_stack(new MenuSpectator());

   if ( m_iIndexSearch == m_SelectedIndex )
         add_menu_to_stack(new MenuSearch());

   //if ( 4 == m_SelectedIndex )
   //   add_menu_to_stack(new MenuRadioConfig()); 

   if ( m_iIndexController == m_SelectedIndex )
      add_menu_to_stack(new MenuController()); 

   if ( m_iIndexMedia == m_SelectedIndex )
      add_menu_to_stack(new MenuStorage());

   if ( m_iIndexSystem == m_SelectedIndex )
      add_menu_to_stack(new MenuSystem());
}
