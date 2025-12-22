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
#include "menu_root.h"
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


MenuRoot::MenuRoot(void)
:Menu(MENU_ID_ROOT, SYSTEM_NAME, NULL)
{
   m_Width = 0.164;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.42;
   //m_bFullWidthSelection = true;

   if ( 1 == Menu::getRenderMode() )
      addExtraHeightAtStart(0.2);
}

MenuRoot::~MenuRoot()
{
   log_line("Menu Closed.");
}

void MenuRoot::onShow()
{
   int iPrevSelectedItem = m_SelectedIndex;
   log_line("MenuRoot: onShow...");
   
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

void MenuRoot::addItems()
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


void MenuRoot::RenderVehicleInfo()
{
   Preferences* pP = get_Preferences();

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float iconHeight = 3.0*height_text;
   float iconWidth = iconHeight/g_pRenderEngine->getAspectRatio();

   float xPos = m_RenderXPos;
   float yPos = m_RenderYPos;
   float width = m_RenderWidth*1.1 + iconWidth + m_sfMenuPaddingX;
   float height = 2 * m_sfMenuPaddingY;
   float maxTextWidth = width-iconWidth-m_sfMenuPaddingX;
   float lineSpacing = 0.2*m_sfMenuPaddingY;
   float hText1 = 0;
   float hText2 = 0;
   float hText4 = 0;
   float hText5 = 0;
   char szBuff[256];
   char szLine1[128];
   char szLine2[128];
   char szLine4[128];
   char szLine5[128];
   char szRunType[32];

   szLine1[0] = 0;
   szLine2[0] = 0;
   szLine4[0] = 0;
   szLine5[0] = 0;
   szRunType[0] = 0;

   bool bConnected = false;
   if ( link_has_received_main_vehicle_ruby_telemetry() )
      bConnected = true;
   if ( g_bFirstModelPairingDone && (0 == getControllerModelsCount()) && (0 == getControllerModelsSpectatorCount()) )
      bConnected = false;
   if ( NULL == g_pCurrentModel )
      bConnected = false;
   if ( 0 == g_uActiveControllerModelVID )
      bConnected = false;

   if ( (NULL == g_pCurrentModel) || (0 == g_uActiveControllerModelVID) ||
        (g_bFirstModelPairingDone && (0 == getControllerModelsCount()) && (0 == getControllerModelsSpectatorCount())) )
   {
      hText1 = g_pRenderEngine->getMessageHeight(L("Not paired with any vehicle"), MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      height += hText1;
      hText2 = g_pRenderEngine->getMessageHeight(L("Search for a vehicle to find one and connect to or select one from your paired vehicles."), MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      height += hText2 + lineSpacing;
   }
   else
   {
      if ( (NULL != g_pCurrentModel) && link_is_vehicle_online_now(g_pCurrentModel->uVehicleId) )
      {
         sprintf(szLine1, L("Connected to %s"), g_pCurrentModel->getLongName() );
         height += g_pRenderEngine->textHeight(g_idFontMenuSmall);
         height += g_pRenderEngine->textHeight(g_idFontMenuSmall);
      }
      else
         sprintf(szLine1, L("Looking for %s"), g_pCurrentModel->getLongName() );

      hText1 = g_pRenderEngine->getMessageHeight(szLine1, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      height += hText1;

      if ( g_pCurrentModel->is_spectator )
         sprintf(szLine2, "(%s)", L("Spectator mode"));
      else
         sprintf(szLine2, "(%s)", L("Full control mode"));

      hText2 = g_pRenderEngine->getMessageHeight(szLine2, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      height += hText2 + lineSpacing;

      if ( bConnected )
      {
         strcpy(szRunType, "runs");
         if ( (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_DRONE ||
              (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_AIRPLANE ||
              (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_HELI )
            strcpy(szRunType, "flights");
      
         //sprintf(szLine3, "Total %s: %d", szRunType, g_pCurrentModel->stats_TotalFlights);
         //hText3 = g_pRenderEngine->getMessageHeight(szLine3, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
         //height += hText3 + lineSpacing;

         int sec = (g_pCurrentModel->m_Stats.uTotalFlightTime)%60;
         int min = (g_pCurrentModel->m_Stats.uTotalFlightTime/60)%60;
         int hours = (g_pCurrentModel->m_Stats.uTotalFlightTime/3600);

         strcpy(szRunType, "run time");
         if ( (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_DRONE ||
              (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_AIRPLANE ||
              (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_HELI )
            strcpy(szRunType, "flight time");
         sprintf(szLine4, "Total %s: %dh:%02dm:%02ds", szRunType, hours, min, sec);
         hText4 = g_pRenderEngine->getMessageHeight(szLine4, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
         height += hText4 + lineSpacing;

         if ( pP->iUnits == prefUnitsImperial )
            sprintf(szLine5, "Odometer: %.1f Mi", _osd_convertKm(g_pCurrentModel->m_Stats.uTotalFlightDistance/100.0/1000.0));
         else
            sprintf(szLine5, "Odometer: %.1f Km", _osd_convertKm(g_pCurrentModel->m_Stats.uTotalFlightDistance/100.0/1000.0));
         hText5 = g_pRenderEngine->getMessageHeight(szLine5, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
         height += hText5 + lineSpacing;
      }

      if ( height < iconHeight + 2*m_sfMenuPaddingY )
         height = iconHeight + 2*m_sfMenuPaddingY;
   }

   height += 2.0*height_text;
   //height += 1.2*height_text;

   yPos -= height + 0.5*m_sfMenuPaddingY;

   g_pRenderEngine->setColors(get_Color_MenuBg(), 0.9);
   g_pRenderEngine->setStroke(get_Color_MenuBorder());
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, MENU_ROUND_MARGIN*m_sfMenuPaddingY);

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   xPos += m_sfMenuPaddingX;
   yPos += m_sfMenuPaddingY;

   if ( (NULL == g_pCurrentModel) || (0 == g_uActiveControllerModelVID) ||
        (g_bFirstModelPairingDone && (0 == getControllerModelsCount()) && (0 == getControllerModelsSpectatorCount())) )
   {
      g_pRenderEngine->drawMessageLines( xPos, yPos, L("Not paired with any vehicle"), MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      yPos += hText1 + lineSpacing;
      g_pRenderEngine->drawMessageLines( xPos, yPos, L("Search for a vehicle to find one and connect to or select one of your paired vehicles."), MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
   }
   else
   {
      if ( iconWidth > 0.001 )
      {
         u32 idIcon = osd_getVehicleIcon( g_pCurrentModel->vehicle_type );

         g_pRenderEngine->setColors(get_Color_MenuText(), 0.7);
         g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
         g_pRenderEngine->drawIcon(xPos+width - iconWidth -2.2*m_sfMenuPaddingX , yPos + height - iconHeight - 2.0*height_text - Menu::getMenuPaddingY(), iconWidth, iconHeight, idIcon);
      }
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

      g_pRenderEngine->drawMessageLines(xPos, yPos, szLine1, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      yPos += hText1;
      yPos += lineSpacing;
      g_pRenderEngine->drawMessageLines(xPos, yPos, szLine2, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      yPos += hText2 + lineSpacing;
      if ( bConnected )
      {
         //g_pRenderEngine->drawMessageLines(xPos, yPos, szLine3, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
         //yPos += hText3 + lineSpacing;
         g_pRenderEngine->drawMessageLines(xPos, yPos, szLine4, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
         yPos += hText4 + lineSpacing;
         g_pRenderEngine->drawMessageLines(xPos, yPos, szLine5, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
         yPos += hText5 + lineSpacing;
      }

      if ( (NULL != g_pCurrentModel) && link_is_vehicle_online_now(g_pCurrentModel->uVehicleId) )
      {
         sprintf(szBuff, "Running ver %d.%d on:", get_sw_version_major(g_pCurrentModel), get_sw_version_minor(g_pCurrentModel) );
         g_pRenderEngine->drawText(xPos, yPos, g_idFontMenuSmall, szBuff);
         yPos += g_pRenderEngine->textHeight(g_idFontMenuSmall);
         sprintf(szBuff, "%s", str_get_hardware_board_name(g_pCurrentModel->hwCapabilities.uBoardType));
         g_pRenderEngine->drawText(xPos, yPos, g_idFontMenuSmall, szBuff);
         yPos += g_pRenderEngine->textHeight(g_idFontMenuSmall);
      }
   }
}

void MenuRoot::Render()
{
   RenderPrepare();

   if ( Menu::getRenderMode() != 1 )
      m_RenderHeight -= 1.14 * g_pRenderEngine->textHeight(g_idFontMenu);

   float yTop = RenderFrameAndTitle();
   float y = yTop;

   if ( Menu::getRenderMode() != 1 )
      m_RenderHeight += 1.14 * g_pRenderEngine->textHeight(g_idFontMenu);
   
   RenderVehicleInfo();

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

void MenuRoot::onSelectItem()
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
