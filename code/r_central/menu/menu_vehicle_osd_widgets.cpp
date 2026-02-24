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
#include "menu_vehicle_osd_widgets.h"
#include "menu_vehicle_osd_widget.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"

#include "../osd/osd_common.h"
#include "../osd/osd_widgets.h"

#include <sys/types.h>
#include <dirent.h>
#include <string.h>


MenuVehicleOSDWidgets::MenuVehicleOSDWidgets(void)
:Menu(MENU_ID_VEHICLE_OSD_WIDGETS, "OSD Widgets", NULL)
{
   m_Width = 0.22;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.2;

   for( int i=0; i<MAX_MENU_OSD_WIDGETS; i++ )
   {
      m_pItemsSelect[i] = NULL;
      m_IndexWidgets[i] = -1;
   }

   addMenuItem(new MenuItemSection("New widgets & gauges"));

   for( int i=0; i<osd_widgets_get_count(); i++ )
   {
      if ( (i >= MAX_OSD_PLUGINS) || (i >= MAX_MENU_OSD_WIDGETS) )
         break;
      type_osd_widget* pWidget = osd_widgets_get(i);
      if ( NULL == pWidget )
         continue;

      char szName[64];
      strcpy(szName, "No Name");
      if ( 0 != pWidget->info.szName[0] )
         strncpy(szName, pWidget->info.szName, sizeof(szName)/sizeof(szName[0])-1);
      m_pItemsSelect[i] = new MenuItemSelect(szName, "Shows/hide/configure this widget/gauge on current OSD layout");
      m_pItemsSelect[i]->addSelection("No");
      m_pItemsSelect[i]->addSelection("Yes");
      m_pItemsSelect[i]->addSelection("Configure...");
      m_pItemsSelect[i]->setIsEditable();
      m_IndexWidgets[i] = addMenuItem(m_pItemsSelect[i]);
   }

   addMenuItem(new MenuItemSection("Old gauges"));

   m_pItemsSelect[MAX_MENU_OSD_WIDGETS] = new MenuItemSelect("Show Speed/Alt", "Shows the speed and altitude instruments");
   m_pItemsSelect[MAX_MENU_OSD_WIDGETS]->addSelection("No");
   m_pItemsSelect[MAX_MENU_OSD_WIDGETS]->addSelection("Yes");
   m_pItemsSelect[MAX_MENU_OSD_WIDGETS]->setIsEditable();
   m_IndexShowSpeedAlt = addMenuItem(m_pItemsSelect[MAX_MENU_OSD_WIDGETS]);

   m_pItemsSelect[MAX_MENU_OSD_WIDGETS+1] = new MenuItemSelect("   Side Speed/Alt", "Shows the instruments for speed and altitude to the edge of the screen");
   m_pItemsSelect[MAX_MENU_OSD_WIDGETS+1]->addSelection("No");
   m_pItemsSelect[MAX_MENU_OSD_WIDGETS+1]->addSelection("Yes");
   m_pItemsSelect[MAX_MENU_OSD_WIDGETS+1]->setIsEditable();
   m_IndexSpeedToSides = addMenuItem(m_pItemsSelect[MAX_MENU_OSD_WIDGETS+1]);

   m_pItemsSelect[MAX_MENU_OSD_WIDGETS+2] = new MenuItemSelect("Show Heading", "Shows the heading instrument");
   m_pItemsSelect[MAX_MENU_OSD_WIDGETS+2]->addSelection("No");
   m_pItemsSelect[MAX_MENU_OSD_WIDGETS+2]->addSelection("Yes");
   m_pItemsSelect[MAX_MENU_OSD_WIDGETS+2]->setIsEditable();
   m_IndexShowHeading = addMenuItem(m_pItemsSelect[MAX_MENU_OSD_WIDGETS+2]);

   m_pItemsSelect[MAX_MENU_OSD_WIDGETS+3] = new MenuItemSelect("Show Altitude Graph/Glide", "Shows the altitude graph and glide efficiency.");
   m_pItemsSelect[MAX_MENU_OSD_WIDGETS+3]->addSelection("No");
   m_pItemsSelect[MAX_MENU_OSD_WIDGETS+3]->addSelection("Yes");
   m_pItemsSelect[MAX_MENU_OSD_WIDGETS+3]->setIsEditable();
   m_IndexShowAltGraph = addMenuItem(m_pItemsSelect[MAX_MENU_OSD_WIDGETS+3]);

}

MenuVehicleOSDWidgets::~MenuVehicleOSDWidgets()
{
}

void MenuVehicleOSDWidgets::valuesToUI()
{
   int layoutIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;

   //log_dword("start: osd flags", g_pCurrentModel->osd_params.osd_flags[layoutIndex]);
   //log_dword("start: instruments flags", g_pCurrentModel->osd_params.instruments_flags[layoutIndex]);
   //log_line("current layout: %d, show instr: %d", (g_pCurrentModel->osd_params.osd_flags[layoutIndex] & OSD_FLAG_AHI_STYLE_MASK)>>3, g_pCurrentModel->osd_params.instruments_flags[layoutIndex] & INSTRUMENTS_FLAG_SHOW_INSTRUMENTS);

   m_pItemsSelect[MAX_MENU_OSD_WIDGETS]->setSelection(((g_pCurrentModel->osd_params.instruments_flags[layoutIndex]) & INSTRUMENTS_FLAG_SHOW_SPEEDALT)?1:0);
   m_pItemsSelect[MAX_MENU_OSD_WIDGETS+1]->setSelection(((g_pCurrentModel->osd_params.instruments_flags[layoutIndex]) & INSTRUMENTS_FLAG_SPEED_TO_SIDES)?1:0);

   m_pItemsSelect[MAX_MENU_OSD_WIDGETS+2]->setSelection(((g_pCurrentModel->osd_params.instruments_flags[layoutIndex]) & INSTRUMENTS_FLAG_SHOW_HEADING)?1:0);
   m_pItemsSelect[MAX_MENU_OSD_WIDGETS+3]->setSelection(((g_pCurrentModel->osd_params.instruments_flags[layoutIndex]) & INSTRUMENTS_FLAG_SHOW_ALTGRAPH)?1:0);


   if ( m_pItemsSelect[MAX_MENU_OSD_WIDGETS]->isEnabled() && 1 == m_pItemsSelect[MAX_MENU_OSD_WIDGETS]->getSelectedIndex() )
      m_pItemsSelect[MAX_MENU_OSD_WIDGETS+1]->setEnabled(true);
   else
      m_pItemsSelect[MAX_MENU_OSD_WIDGETS+1]->setEnabled(false);


   for( int i=0; i<osd_widgets_get_count(); i++ )
   {
      if ( (i >= MAX_OSD_PLUGINS) || (i >= MAX_MENU_OSD_WIDGETS) )
         break;
      type_osd_widget* pWidget = osd_widgets_get(i);
      if ( NULL == pWidget )
         continue;
      int iIndexModel = osd_widget_add_to_model(pWidget, g_pCurrentModel->uVehicleId);

      if ( pWidget->display_info[iIndexModel][osd_get_current_layout_index()].bShow )
         m_pItemsSelect[i]->setSelectedIndex(1);
      else
         m_pItemsSelect[i]->setSelectedIndex(0);
   }
}

void MenuVehicleOSDWidgets::onShow()
{
   int iTemp = m_SelectedIndex;
   Menu::onShow();
   valuesToUI();
   if ( iTemp >= 0 )
      m_SelectedIndex = iTemp;
}

bool MenuVehicleOSDWidgets::periodicLoop()
{
   return false;
}

void MenuVehicleOSDWidgets::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


int MenuVehicleOSDWidgets::onBack()
{
   return Menu::onBack();
}


void MenuVehicleOSDWidgets::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   for( int i=0; i<osd_widgets_get_count(); i++ )
   {
      if ( (i >= MAX_OSD_PLUGINS) || (i >= MAX_MENU_OSD_WIDGETS) )
         break;
      type_osd_widget* pWidget = osd_widgets_get(i);
      if ( NULL == pWidget )
         continue;

      if ( m_IndexWidgets[i] == m_SelectedIndex )
      {
         int iIndexModel = osd_widget_add_to_model(pWidget, g_pCurrentModel->uVehicleId);
         if ( -1 == iIndexModel )
            return;
         int iValue = m_pItemsSelect[i]->getSelectedIndex();
         if ( 0 == iValue )
         {
             pWidget->display_info[iIndexModel][osd_get_current_layout_index()].bShow = false;
             osd_widgets_save();
         }
         if ( 1 == iValue )
         {
             pWidget->display_info[iIndexModel][osd_get_current_layout_index()].bShow = true;
             osd_widgets_save();
         }
         if ( 2 == iValue )
         {
            MenuVehicleOSDWidget* pMenu = new MenuVehicleOSDWidget(i);
            add_menu_to_stack(pMenu);
            return;
         }
         return;
      }
   }

   osd_parameters_t params;
   memcpy(&params, &(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));
   bool sendToVehicle = false;
   int layoutIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;

   if ( m_IndexShowSpeedAlt == m_SelectedIndex )
   {
      u32 idx = m_pItemsSelect[MAX_MENU_OSD_WIDGETS]->getSelectedIndex();
      params.instruments_flags[layoutIndex] &= ~INSTRUMENTS_FLAG_SHOW_SPEEDALT;
      if ( idx > 0 )
         params.instruments_flags[layoutIndex] |= INSTRUMENTS_FLAG_SHOW_SPEEDALT;
      sendToVehicle = true;
   }

   if ( m_IndexSpeedToSides == m_SelectedIndex )
   {
      u32 idx = m_pItemsSelect[MAX_MENU_OSD_WIDGETS+1]->getSelectedIndex();
      params.instruments_flags[layoutIndex] &= ~INSTRUMENTS_FLAG_SPEED_TO_SIDES;
      if ( idx > 0 )
         params.instruments_flags[layoutIndex] |= INSTRUMENTS_FLAG_SPEED_TO_SIDES;
      sendToVehicle = true;
   }

   if ( m_IndexShowHeading == m_SelectedIndex )
   {
      u32 idx = m_pItemsSelect[MAX_MENU_OSD_WIDGETS+2]->getSelectedIndex();
      params.instruments_flags[layoutIndex] &= ~INSTRUMENTS_FLAG_SHOW_HEADING;
      if ( idx > 0 )
         params.instruments_flags[layoutIndex] |= INSTRUMENTS_FLAG_SHOW_HEADING;
      sendToVehicle = true;
   }

   if ( m_IndexShowAltGraph == m_SelectedIndex )
   {
      u32 idx = m_pItemsSelect[MAX_MENU_OSD_WIDGETS+3]->getSelectedIndex();
      params.instruments_flags[layoutIndex] &= ~INSTRUMENTS_FLAG_SHOW_ALTGRAPH;
      if ( idx > 0 )
         params.instruments_flags[layoutIndex] |= INSTRUMENTS_FLAG_SHOW_ALTGRAPH;
      sendToVehicle = true;
   }

   if ( g_pCurrentModel->is_spectator )
   {
      memcpy(&(g_pCurrentModel->osd_params), &params, sizeof(osd_parameters_t));
      saveControllerModel(g_pCurrentModel);
      valuesToUI();
   }
   else if ( sendToVehicle )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_OSD_PARAMS, 0, (u8*)&params, sizeof(osd_parameters_t)) )
         valuesToUI();
      return;
   }
}
