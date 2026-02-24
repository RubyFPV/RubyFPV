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

#include "menu_vehicle_osd_plugins.h"
#include "menu_vehicle_osd_plugin.h"
#include "menu_item_select.h"
#include "menu_item_text.h"
#include "menu_item_section.h"
#include "menu_vehicle_instruments_general.h"

#include "../osd/osd_plugins.h"


MenuVehicleOSDPlugins::MenuVehicleOSDPlugins(void)
:Menu(MENU_ID_OSD_PLUGINS, "OSD Plugins Settings", NULL)
{
   m_Width = 0.28;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.30;

   char szBuff[128];
   sprintf(szBuff, "OSD Plugins Settings (%s)", str_get_osd_screen_name(g_pCurrentModel->osd_params.iCurrentOSDScreen));
   setTitle(szBuff);

   for( int i=0; i<MAX_OSD_CUSTOM_PLUGINS; i++ )
   {
      m_pItemsSelect[i] = NULL;
      m_IndexPlugins[i] = -1;
   }

   for( int i=0; i<osd_plugins_get_count(); i++ )
   {
      if ( (i >= MAX_OSD_PLUGINS) || (i >= MAX_OSD_CUSTOM_PLUGINS) )
         break;
      char* szName = osd_plugins_get_short_name(i);
      m_pItemsSelect[i] = new MenuItemSelect(szName, "Shows/hide/configure this plugin on current OSD layout");
      m_pItemsSelect[i]->addSelection("No");
      m_pItemsSelect[i]->addSelection("Yes");
      m_pItemsSelect[i]->addSelection("Configure...");
      m_pItemsSelect[i]->setIsEditable();
      m_IndexPlugins[i] = addMenuItem(m_pItemsSelect[i]);
   }

   addSeparator();
   addMenuItem(new MenuItemText("If you want to add additional OSD plugins, import them from [Menu] -> [Controller Settings] -> [Management] -> [Manage Plugins]", true));
}

MenuVehicleOSDPlugins::~MenuVehicleOSDPlugins()
{
}

void MenuVehicleOSDPlugins::valuesToUI()
{
   int layoutIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;

   //log_dword("start: osd flags", g_pCurrentModel->osd_params.osd_flags[layoutIndex]);
   //log_dword("start: instruments flags", g_pCurrentModel->osd_params.instruments_flags[layoutIndex]);
   //log_line("current layout: %d, show instr: %d", (g_pCurrentModel->osd_params.osd_flags[layoutIndex] & OSD_FLAG_AHI_STYLE_MASK)>>3, g_pCurrentModel->osd_params.instruments_flags[layoutIndex] & INSTRUMENTS_FLAG_SHOW_INSTRUMENTS);

   for( int i=0; i<osd_plugins_get_count(); i++ )
   {
      if ( (i >= MAX_OSD_PLUGINS) || ( i >= MAX_OSD_CUSTOM_PLUGINS) )
         break;
      u32 flags = g_pCurrentModel->osd_params.instruments_flags[layoutIndex];
      if ( flags & (INSTRUMENTS_FLAG_SHOW_FIRST_OSD_PLUGIN << i) )
         m_pItemsSelect[i]->setSelectedIndex(1);
      else
         m_pItemsSelect[i]->setSelectedIndex(0);
   }
}

void MenuVehicleOSDPlugins::onShow()
{
   int iTemp = m_SelectedIndex;
   Menu::onShow();
   valuesToUI();
   if ( iTemp >= 0 )
      m_SelectedIndex = iTemp;
}


void MenuVehicleOSDPlugins::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yTop);
}

void MenuVehicleOSDPlugins::onItemValueChanged(int itemIndex)
{
   Menu::onItemValueChanged(itemIndex);
}

void MenuVehicleOSDPlugins::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   osd_parameters_t params;
   memcpy(&params, &(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));
   bool sendToVehicle = false;
   int layoutIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;

   for( int i=0; i<osd_plugins_get_count(); i++ )
   {
      if ( (i >= MAX_OSD_PLUGINS) || (i >= MAX_OSD_CUSTOM_PLUGINS) )
         break;

      if ( m_IndexPlugins[i] != -1 )
      if ( m_SelectedIndex == m_IndexPlugins[i] )
      {
         if ( m_pItemsSelect[i]->getSelectedIndex() == 0 )
            params.instruments_flags[layoutIndex] &= ~(INSTRUMENTS_FLAG_SHOW_FIRST_OSD_PLUGIN << i);
         else if ( m_pItemsSelect[i]->getSelectedIndex() == 1 )
            params.instruments_flags[layoutIndex] |= (INSTRUMENTS_FLAG_SHOW_FIRST_OSD_PLUGIN << i);
         else
         {
            params.instruments_flags[layoutIndex] |= (INSTRUMENTS_FLAG_SHOW_FIRST_OSD_PLUGIN << i);
            memcpy(&(g_pCurrentModel->osd_params), &params, sizeof(osd_parameters_t));
            valuesToUI();
            MenuVehicleOSDPlugin* pMenu = new MenuVehicleOSDPlugin(i);
            add_menu_to_stack(pMenu);
            //return;
         }
         sendToVehicle = true;
      }
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