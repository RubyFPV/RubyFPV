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

#include "menu_vehicle_osd_instruments.h"
#include "menu_vehicle_osd_plugins.h"
#include "menu_vehicle_osd_plugin.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_section.h"
#include "menu_vehicle_instruments_general.h"

#include "../osd/osd_plugins.h"


MenuVehicleOSDInstruments::MenuVehicleOSDInstruments(void)
:Menu(MENU_ID_OSD_AHI, L("Intruments/Gauges Settings"), NULL)
{
   m_Width = 0.28;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.30;

   char szBuff[256];
   sprintf(szBuff, "Instruments/Gauges Settings (%s)", str_get_osd_screen_name(g_pCurrentModel->osd_params.iCurrentOSDScreen));
   setTitle(szBuff);

   for( int i=0; i<50; i++ )
   {
      m_pItemsSelect[i] = NULL;
      m_IndexPlugins[i] = -1;
      m_IndexPluginsConfigure[i] = -1;
   }

   m_pItemsSelect[6] = new MenuItemSelect(L("Show Speed/Alt"), L("Shows the speed and altitude instruments"));
   m_pItemsSelect[6]->addSelection(L("No"));
   m_pItemsSelect[6]->addSelection(L("Yes"));
   m_pItemsSelect[6]->setUseMultiViewLayout();
   m_IndexAHIShowSpeedAlt = addMenuItem(m_pItemsSelect[6]);

   m_pItemsSelect[8] = new MenuItemSelect("   Side Speed/Alt", "Shows the instruments for speed and altitude to the edge of the screen");
   m_pItemsSelect[8]->addSelection("No");
   m_pItemsSelect[8]->addSelection("Yes");
   m_pItemsSelect[8]->setUseMultiViewLayout();
   m_IndexAHIToSides = addMenuItem(m_pItemsSelect[8]);

   m_pItemsSelect[9] = new MenuItemSelect(L("Show Heading"), L("Shows the heading instrument."));
   m_pItemsSelect[9]->addSelection(L("No"));
   m_pItemsSelect[9]->addSelection(L("Yes"));
   m_pItemsSelect[9]->setUseMultiViewLayout();
   m_IndexAHIShowHeading = addMenuItem(m_pItemsSelect[9]);

   m_pItemsSelect[10] = new MenuItemSelect(L("Show Altitude Graph/Glide"), L("Shows the altitude graph and glide efficiency."));
   m_pItemsSelect[10]->addSelection(L("No"));
   m_pItemsSelect[10]->addSelection(L("Yes"));
   m_pItemsSelect[10]->setUseMultiViewLayout();
   m_IndexAHIShowAltGraph = addMenuItem(m_pItemsSelect[10]);

   addMenuItem(new MenuItemSection("Plugins"));

   for( int i=0; i<osd_plugins_get_count(); i++ )
   {
      if ( i >= MAX_OSD_PLUGINS )
         break;
      char* szName = osd_plugins_get_short_name(i);
      m_pItemsSelect[20+i] = new MenuItemSelect(szName, "Show/hide this plugin on current OSD layout");
      m_pItemsSelect[20+i]->addSelection("No");
      m_pItemsSelect[20+i]->addSelection("Yes");
      m_pItemsSelect[20+i]->setUseMultiViewLayout();
      m_IndexPlugins[20+i] = addMenuItem(m_pItemsSelect[20+i]);

      char szDesc[128];
      sprintf(szDesc, "Configure this plugin (%s)", szName);
      MenuItem* pItem = new MenuItem("   Configure", szDesc);
      pItem->showArrow();
      m_IndexPluginsConfigure[20+i] = addMenuItem(pItem);
   }
}

MenuVehicleOSDInstruments::~MenuVehicleOSDInstruments()
{
}

void MenuVehicleOSDInstruments::valuesToUI()
{
   int layoutIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;

   //log_dword("start: osd flags", g_pCurrentModel->osd_params.osd_flags[layoutIndex]);
   //log_dword("start: instruments flags", g_pCurrentModel->osd_params.instruments_flags[layoutIndex]);
   //log_line("current layout: %d, show instr: %d", (g_pCurrentModel->osd_params.osd_flags[layoutIndex] & OSD_FLAG_AHI_STYLE_MASK)>>3, g_pCurrentModel->osd_params.instruments_flags[layoutIndex] & INSTRUMENTS_FLAG_SHOW_INSTRUMENTS);

   m_pItemsSelect[6]->setSelection(((g_pCurrentModel->osd_params.instruments_flags[layoutIndex]) & INSTRUMENTS_FLAG_SHOW_SPEEDALT)?1:0);
   m_pItemsSelect[8]->setSelection(((g_pCurrentModel->osd_params.instruments_flags[layoutIndex]) & INSTRUMENTS_FLAG_SPEED_TO_SIDES)?1:0);

   m_pItemsSelect[9]->setSelection(((g_pCurrentModel->osd_params.instruments_flags[layoutIndex]) & INSTRUMENTS_FLAG_SHOW_HEADING)?1:0);
   m_pItemsSelect[10]->setSelection(((g_pCurrentModel->osd_params.instruments_flags[layoutIndex]) & INSTRUMENTS_FLAG_SHOW_ALTGRAPH)?1:0);


   if ( m_pItemsSelect[6]->isEnabled() && 1 == m_pItemsSelect[6]->getSelectedIndex() )
      m_pItemsSelect[8]->setEnabled(true);
   else
      m_pItemsSelect[8]->setEnabled(false);

   for( int i=0; i<osd_plugins_get_count(); i++ )
   {
      if ( i >= MAX_OSD_PLUGINS )
         break;
      u32 flags = g_pCurrentModel->osd_params.instruments_flags[layoutIndex];
      if ( flags & (INSTRUMENTS_FLAG_SHOW_FIRST_OSD_PLUGIN << i) )
      {
         m_pItemsSelect[20+i]->setSelectedIndex(1);
         m_pMenuItems[m_IndexPluginsConfigure[20+i]]->setEnabled(true);
      }
      else
      {
         m_pItemsSelect[20+i]->setSelectedIndex(0);
         m_pMenuItems[m_IndexPluginsConfigure[20+i]]->setEnabled(false);
      }
   }
}

void MenuVehicleOSDInstruments::onShow()
{
   int iTemp = m_SelectedIndex;
   Menu::onShow();
   valuesToUI();
   if ( iTemp >= 0 )
      m_SelectedIndex = iTemp;
}


void MenuVehicleOSDInstruments::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yTop);
}

void MenuVehicleOSDInstruments::onItemValueChanged(int itemIndex)
{
   Menu::onItemValueChanged(itemIndex);
}

void MenuVehicleOSDInstruments::onSelectItem()
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

   if ( m_IndexAHIShowSpeedAlt == m_SelectedIndex )
   {
      u32 idx = m_pItemsSelect[6]->getSelectedIndex();
      params.instruments_flags[layoutIndex] &= ~INSTRUMENTS_FLAG_SHOW_SPEEDALT;
      if ( idx > 0 )
         params.instruments_flags[layoutIndex] |= INSTRUMENTS_FLAG_SHOW_SPEEDALT;
      sendToVehicle = true;
   }

   if ( m_IndexAHIToSides == m_SelectedIndex )
   {
      u32 idx = m_pItemsSelect[8]->getSelectedIndex();
      params.instruments_flags[layoutIndex] &= ~INSTRUMENTS_FLAG_SPEED_TO_SIDES;
      if ( idx > 0 )
         params.instruments_flags[layoutIndex] |= INSTRUMENTS_FLAG_SPEED_TO_SIDES;
      sendToVehicle = true;
   }

   if ( m_IndexAHIShowHeading == m_SelectedIndex )
   {
      u32 idx = m_pItemsSelect[9]->getSelectedIndex();
      params.instruments_flags[layoutIndex] &= ~INSTRUMENTS_FLAG_SHOW_HEADING;
      if ( idx > 0 )
         params.instruments_flags[layoutIndex] |= INSTRUMENTS_FLAG_SHOW_HEADING;
      sendToVehicle = true;
   }

   if ( m_IndexAHIShowAltGraph == m_SelectedIndex )
   {
      u32 idx = m_pItemsSelect[10]->getSelectedIndex();
      params.instruments_flags[layoutIndex] &= ~INSTRUMENTS_FLAG_SHOW_ALTGRAPH;
      if ( idx > 0 )
         params.instruments_flags[layoutIndex] |= INSTRUMENTS_FLAG_SHOW_ALTGRAPH;
      sendToVehicle = true;
   }

   for( int i=0; i<osd_plugins_get_count(); i++ )
   {
      if ( i >= MAX_OSD_PLUGINS )
         break;

      if ( m_IndexPlugins[20+i] != -1 )
      if ( m_SelectedIndex == m_IndexPlugins[20+i] )
      {
         if ( m_pItemsSelect[20+i]->getSelectedIndex() == 1 )
            params.instruments_flags[layoutIndex] |= (INSTRUMENTS_FLAG_SHOW_FIRST_OSD_PLUGIN << i);
         else
            params.instruments_flags[layoutIndex] &= ~(INSTRUMENTS_FLAG_SHOW_FIRST_OSD_PLUGIN << i);
         sendToVehicle = true;
      }
      if ( m_IndexPluginsConfigure[20+i] != -1 )
      if ( m_SelectedIndex == m_IndexPluginsConfigure[20+i] )
      {
         MenuVehicleOSDPlugin* pMenu = new MenuVehicleOSDPlugin(i);
         add_menu_to_stack(pMenu);
         return;
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