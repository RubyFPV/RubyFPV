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
#include "menu_vehicle_osd.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "menu_item_section.h"

#include "menu_vehicle_osd_plugins.h"
#include "menu_vehicle_osd_widgets.h"
#include "menu_vehicle_osd_stats.h"
#include "menu_vehicle_osd_elements.h"
#include "menu_vehicle_instruments_general.h"
#include "menu_vehicle_alarms.h"
#include "menu_preferences_ui.h"

#include "../osd/osd_common.h"


MenuVehicleOSD::MenuVehicleOSD(void)
:Menu(MENU_ID_VEHICLE_OSD, L("OSD / Instruments Settings"), NULL)
{
   m_Width = 0.30;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.15;
   m_bShowCompact = false;

   m_IndexAlarms = -1;
   m_IndexPlugins = -1;
   m_IndexOSDShowMSPOSD = -1;
   //m_IndexOSDReset = -1;

}

MenuVehicleOSD::~MenuVehicleOSD()
{
}

void MenuVehicleOSD::showCompact()
{
   m_bShowCompact = true;
}

void MenuVehicleOSD::addItems()
{
   int iTmp = m_SelectedIndex;
   removeAllItems();
   removeAllTopLines();

   m_IndexSettings = -1;
   m_IndexOSDController = -1;

   for( int i=0; i<20; i++ )
      m_pItemsSelect[i] = NULL;

   if ( ! m_bShowCompact )
   {
      m_IndexSettings = addMenuItem(new MenuItem(L("General OSD Settings"), L("Sets global settings for all OSD/instruments/gauges")));
      m_pMenuItems[m_IndexSettings]->showArrow();

      m_IndexOSDController = addMenuItem( new MenuItem(L("OSD Size, Color, Fonts"), L("Sets colors, size and fonts for OSD.")));
      m_pMenuItems[m_IndexOSDController]->showArrow();

      addMenuItem(new MenuItemSection(L("OSD Layouts and Settings")));
   }

   int iScreenIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;

   m_pItemsSelect[0] = new MenuItemSelect(L("Screen"), L("Changes the active screen of the OSD elements and what information you see on this screen."));
   m_pItemsSelect[0]->addSelection("Screen 1");
   m_pItemsSelect[0]->addSelection("Screen 2");
   m_pItemsSelect[0]->addSelection("Screen 3");
   m_pItemsSelect[0]->addSelection("Lean");
   m_pItemsSelect[0]->addSelection("Lean Extended");
   m_pItemsSelect[0]->setIsEditable();
   m_IndexOSDScreen = addMenuItem(m_pItemsSelect[0]);


   m_pItemsSelect[1] = new MenuItemSelect(L("Enabled"), L("Enables or disables this screen"));
   m_pItemsSelect[1]->addSelection(L("No"));
   m_pItemsSelect[1]->addSelection(L("Yes"));
   m_pItemsSelect[1]->addSelection(L("Only plugins"));
   m_pItemsSelect[1]->setIsEditable();
   m_IndexOSDEnabled = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSelect[7] = new MenuItemSelect(L("Layout"), L("Set the default layout of this OSD screen (what elements are shown on this screen)."));
   m_pItemsSelect[7]->addSelection(L("No elements"));
   m_pItemsSelect[7]->addSelection(L("Minimal"));
   m_pItemsSelect[7]->addSelection(L("Compact"));
   m_pItemsSelect[7]->addSelection(L("Default"));
   m_pItemsSelect[7]->addSelection(L("Custom"), (g_pCurrentModel->osd_params.osd_layout_preset[iScreenIndex] == OSD_PRESET_CUSTOM)?true:false);
   m_pItemsSelect[7]->setIsEditable();
   m_IndexOSDLayout = addMenuItem(m_pItemsSelect[7]);

   m_IndexOSDFontSize = -1;
   m_IndexBgOnTexts = -1;
   m_IndexOSDTransparency = -1;
   m_IndexHighlightChangeElements = -1;
   m_IndexDontShowFCMessages = -1;
   m_IndexOSDShowMSPOSD = -1;

   if ( ! m_bShowCompact )
   {
      m_pItemsSelect[2] = new MenuItemSelect(L("Font Size"), L("Increase/decrease OSD font size for current screen."));
      m_pItemsSelect[2]->addSelection(L("Smallest"));
      m_pItemsSelect[2]->addSelection(L("Smaller"));
      m_pItemsSelect[2]->addSelection(L("Small"));
      m_pItemsSelect[2]->addSelection(L("Normal"));
      m_pItemsSelect[2]->addSelection(L("Large"));
      m_pItemsSelect[2]->addSelection(L("Larger"));
      m_pItemsSelect[2]->addSelection(L("Largest"));
      m_pItemsSelect[2]->addSelection(L("X-Large"));
      m_pItemsSelect[2]->setIsEditable();
      m_IndexOSDFontSize = addMenuItem(m_pItemsSelect[2]);

      m_pItemsSelect[4] = new MenuItemSelect(L("Background Type"), L("Change if and how the background behind OSD elements is shown."));
      m_pItemsSelect[4]->addSelection(L("None"));
      m_pItemsSelect[4]->addSelection(L("On text only"));
      m_pItemsSelect[4]->addSelection(L("As Bars"));
      m_pItemsSelect[4]->setIsEditable();
      m_IndexBgOnTexts = addMenuItem(m_pItemsSelect[4]);

      m_pItemsSelect[3] = new MenuItemSelect(L("Background Transparency"), L("Change how transparent the OSD background is for current screen."));
      m_pItemsSelect[3]->addSelection(L("Max"));
      m_pItemsSelect[3]->addSelection(L("Medium"));
      m_pItemsSelect[3]->addSelection(L("Normal"));
      m_pItemsSelect[3]->addSelection(L("Minimum"));
      m_pItemsSelect[3]->addSelection(L("None"));
      m_pItemsSelect[3]->setIsEditable();
      m_IndexOSDTransparency = addMenuItem(m_pItemsSelect[3]);

      m_pItemsSelect[5] = new MenuItemSelect(L("Highlight Changing Elements"), L("Highlight when important OSD elements change (i.e. radio modulation schemes)."));
      m_pItemsSelect[5]->addSelection(L("No"));
      m_pItemsSelect[5]->addSelection(L("Yes"));
      m_pItemsSelect[5]->setIsEditable();
      m_IndexHighlightChangeElements = addMenuItem(m_pItemsSelect[5]);

      if ( (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MAVLINK) ||
           (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_LTM) )
      {
         m_pItemsSelect[6] = new MenuItemSelect(L("Don't show FC messages"), L("Do not show messages/texts from flight controller."));
         m_pItemsSelect[6]->addSelection(L("No"));
         m_pItemsSelect[6]->addSelection(L("Yes"));
         m_pItemsSelect[6]->setIsEditable();
         m_IndexDontShowFCMessages = addMenuItem(m_pItemsSelect[6]);
      }
      if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP) )
      {
         m_pItemsSelect[11] = new MenuItemSelect(L("Show MSP OSD Elements"), L("Render the MSP OSD Elements"));
         m_pItemsSelect[11]->addSelection(L("No"));
         m_pItemsSelect[11]->addSelection(L("Yes"));
         m_pItemsSelect[11]->setIsEditable();
         m_IndexOSDShowMSPOSD = addMenuItem(m_pItemsSelect[11]);
      }
   }

   m_IndexOSDElements = addMenuItem(new MenuItem(L("Layout OSD Elements"), L("Configure which OSD elements show up on current screen")));
   m_pMenuItems[m_IndexOSDElements]->showArrow();

   m_IndexOSDWidgets = -1;
   m_IndexOSDPlugins = -1;

   if ( ! m_bShowCompact )
   {
      m_IndexOSDWidgets = addMenuItem(new MenuItem(L("Layout OSD Widgets & Gauges"), L("Configure which OSD widgets and gauges show up on current screen and where to show them.")));
      m_pMenuItems[m_IndexOSDWidgets]->showArrow();

      m_IndexOSDPlugins = addMenuItem(new MenuItem(L("Layout OSD Plugins"), L("Show/Hide/Configure the custom OSD plugins that are installed on the controller.")));
      m_pMenuItems[m_IndexOSDPlugins]->showArrow();
   }

   m_IndexOSDStats = addMenuItem(new MenuItem(L("Layout OSD Stats Windows"), L("Configure which statistics windows show up on current screen.")));
   m_pMenuItems[m_IndexOSDStats]->showArrow();

   //m_IndexOSDReset = addMenuItem(new MenuItem("Reset OSD Screen", "Resets this OSD screen to default layout and style."));

   m_IndexShowFull = -1;
   if ( m_bShowCompact )
      m_IndexShowFull = addMenuItem(new MenuItem(L("Show all OSD settings"), L("")));

   valuesToUI();
   if ( iTmp >= 0 )
   {
      m_SelectedIndex = iTmp;
      onFocusedItemChanged();
   }
}

void MenuVehicleOSD::valuesToUI()
{
   int iScreenIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;

   if ( NULL != m_pItemsSelect[0] )
      m_pItemsSelect[0]->setSelectedIndex(iScreenIndex);
   if ( -1 != m_IndexOSDFontSize )
      m_pItemsSelect[2]->setSelectedIndex(g_pCurrentModel->osd_params.osd_preferences[iScreenIndex] & 0xFF);
   if ( -1 != m_IndexOSDTransparency )
      m_pItemsSelect[3]->setSelectedIndex(((g_pCurrentModel->osd_params.osd_preferences[iScreenIndex]) & OSD_PREFERENCES_OSD_TRANSPARENCY_BITMASK) >> OSD_PREFERENCES_OSD_TRANSPARENCY_SHIFT);

   if ( -1 != m_IndexBgOnTexts )
   {
      m_pItemsSelect[4]->setSelectedIndex(0);
      if ( g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_BACKGROUND_ON_TEXTS_ONLY )
         m_pItemsSelect[4]->setSelectedIndex(1);
      else if ( g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_BGBARS )
         m_pItemsSelect[4]->setSelectedIndex(2);
   }
   if ( -1 != m_IndexHighlightChangeElements )
      m_pItemsSelect[5]->setSelectedIndex((g_pCurrentModel->osd_params.osd_flags3[iScreenIndex] & OSD_FLAG3_HIGHLIGHT_CHANGING_ELEMENTS) ? 1:0);

   if ( -1 != m_IndexDontShowFCMessages )
      m_pItemsSelect[6]->setSelectedIndex((g_pCurrentModel->osd_params.osd_preferences[iScreenIndex] & OSD_PREFERENCES_BIT_FLAG_DONT_SHOW_FC_MESSAGES) ? 1:0);

   if ( -1 != m_IndexOSDLayout )
   {
      m_pItemsSelect[7]->setSelectedIndex(g_pCurrentModel->osd_params.osd_layout_preset[iScreenIndex]);
      if ( g_pCurrentModel->osd_params.osd_layout_preset[iScreenIndex] == OSD_PRESET_CUSTOM )
         m_pItemsSelect[7]->setSelectionIndexEnabled(4);
      else
         m_pItemsSelect[7]->setSelectionIndexDisabled(4);
   }
   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP) )
   if ( -1 != m_IndexOSDShowMSPOSD )
   {
      if ( g_pCurrentModel->osd_params.osd_flags3[iScreenIndex] & OSD_FLAG3_RENDER_MSP_OSD )
         m_pItemsSelect[11]->setSelectedIndex(1);
      else
         m_pItemsSelect[11]->setSelectedIndex(0);
   }

   if ( g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_LAYOUT_ENABLED )
   {
      if ( NULL != m_pItemsSelect[1] )
         m_pItemsSelect[1]->setSelectedIndex(1);
      if ( NULL != m_pItemsSelect[2] )
         m_pItemsSelect[2]->setEnabled(true);
      if ( NULL != m_pItemsSelect[4] )
         m_pItemsSelect[4]->setEnabled(true);
      if ( (NULL != m_pItemsSelect[3]) && (NULL != m_pItemsSelect[4]) )
      {
         if ( m_pItemsSelect[4]->getSelectedIndex() != 0 )
            m_pItemsSelect[3]->setEnabled(true);
         else
            m_pItemsSelect[3]->setEnabled(false);
      }
      if ( NULL != m_pItemsSelect[5] )
         m_pItemsSelect[5]->setEnabled(true);
      if ( NULL != m_pItemsSelect[6] )
         m_pItemsSelect[6]->setEnabled(true);
      if ( NULL != m_pItemsSelect[7] )
         m_pItemsSelect[7]->setEnabled(true);
      if ( -1 != m_IndexOSDElements )
         m_pMenuItems[m_IndexOSDElements]->setEnabled(true);
      if ( -1 != m_IndexOSDWidgets )
         m_pMenuItems[m_IndexOSDWidgets]->setEnabled(true);
      if ( -1 != m_IndexOSDPlugins )
         m_pMenuItems[m_IndexOSDPlugins]->setEnabled(true);
      if ( -1 != m_IndexOSDStats )
         m_pMenuItems[m_IndexOSDStats]->setEnabled(true);

      if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP) )
      if ( -1 != m_IndexOSDShowMSPOSD )
         m_pItemsSelect[11]->setEnabled(true);
   }
   else if ( g_pCurrentModel->osd_params.osd_flags3[iScreenIndex] & OSD_FLAG3_LAYOUT_ENABLED_PLUGINS_ONLY )
   {
      if ( NULL != m_pItemsSelect[1] )
         m_pItemsSelect[1]->setSelectedIndex(2);
      if ( NULL != m_pItemsSelect[2] )
         m_pItemsSelect[2]->setEnabled(true);
      if ( NULL != m_pItemsSelect[4] )
         m_pItemsSelect[4]->setEnabled(true);

      if ( (NULL != m_pItemsSelect[3]) && (NULL != m_pItemsSelect[4]) )
      {
         if ( m_pItemsSelect[4]->getSelectedIndex() != 0 )
            m_pItemsSelect[3]->setEnabled(true);
         else
            m_pItemsSelect[3]->setEnabled(false);
      }
      if ( NULL != m_pItemsSelect[5] )
         m_pItemsSelect[5]->setEnabled(false);
      if ( NULL != m_pItemsSelect[6] )
         m_pItemsSelect[6]->setEnabled(false);
      if ( NULL != m_pItemsSelect[7] )
         m_pItemsSelect[7]->setEnabled(false);
      if ( -1 != m_IndexOSDElements )
         m_pMenuItems[m_IndexOSDElements]->setEnabled(false);
      if ( -1 != m_IndexOSDWidgets )
         m_pMenuItems[m_IndexOSDWidgets]->setEnabled(true);
      if ( -1 != m_IndexOSDPlugins )
         m_pMenuItems[m_IndexOSDPlugins]->setEnabled(true);
      if ( -1 != m_IndexOSDStats )
         m_pMenuItems[m_IndexOSDStats]->setEnabled(false);
      if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP) )
      if ( -1 != m_IndexOSDShowMSPOSD )
         m_pItemsSelect[11]->setEnabled(false);
   }
   else
   {
      if ( NULL != m_pItemsSelect[1] )
         m_pItemsSelect[1]->setSelectedIndex(0);
      if ( NULL != m_pItemsSelect[2] )
         m_pItemsSelect[2]->setEnabled(false);
      if ( NULL != m_pItemsSelect[3] )
         m_pItemsSelect[3]->setEnabled(false);
      if ( NULL != m_pItemsSelect[4] )
         m_pItemsSelect[4]->setEnabled(false);
      if ( NULL != m_pItemsSelect[5] )
         m_pItemsSelect[5]->setEnabled(false);
      if ( NULL != m_pItemsSelect[6] )
         m_pItemsSelect[6]->setEnabled(false);
      if ( NULL != m_pItemsSelect[7] )
         m_pItemsSelect[7]->setEnabled(false);
      if ( -1 != m_IndexOSDElements )
         m_pMenuItems[m_IndexOSDElements]->setEnabled(false);
      if ( -1 != m_IndexOSDWidgets )
         m_pMenuItems[m_IndexOSDWidgets]->setEnabled(false);
      if ( -1 != m_IndexOSDPlugins )
         m_pMenuItems[m_IndexOSDPlugins]->setEnabled(false);
      if ( -1 != m_IndexOSDStats )
         m_pMenuItems[m_IndexOSDStats]->setEnabled(false);
      if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP) )
      if ( -1 != m_IndexOSDShowMSPOSD )
         m_pItemsSelect[11]->setEnabled(false);
   }
}

void MenuVehicleOSD::onShow()
{
   int iTmp = getSelectedMenuItemIndex();

   addItems();
   Menu::onShow();

   if ( iTmp >= 0 )
      m_SelectedIndex = iTmp;
   onFocusedItemChanged();
}

void MenuVehicleOSD::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);
   valuesToUI();
}

void MenuVehicleOSD::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuVehicleOSD::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_IndexSettings == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleInstrumentsGeneral());
      return;
   }

   if ( m_IndexOSDController == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuPreferencesUI(true));
      return;
   }

   if ( m_IndexAlarms == m_SelectedIndex )
   {
      //add_menu_to_stack(new MenuVehicleAlarms());
      return;
   }

   if ( -1 != m_IndexPlugins )
   if ( m_IndexPlugins == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleOSDPlugins());
      return;
   }


   if ( m_IndexOSDElements == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleOSDElements());
      return;
   }

   if ( m_IndexOSDWidgets == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleOSDWidgets());
      return;
   }

   if ( m_IndexOSDPlugins == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleOSDPlugins());
      return;
   }

   if ( m_IndexOSDStats == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleOSDStats());
      return;
   }


   bool sendToVehicle = false;
   osd_parameters_t params;
   memcpy(&params, &(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));
   int iScreenIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;

   if ( m_IndexOSDScreen == m_SelectedIndex )
   {
      params.iCurrentOSDScreen = m_pItemsSelect[0]->getSelectedIndex();
      sendToVehicle = true;

      u32 scale = params.osd_preferences[params.iCurrentOSDScreen] & 0xFF;
      osd_setScaleOSD((int)scale);
      osd_apply_preferences();
      applyFontScaleChanges();
      g_bHasVideoDecodeStatsSnapshot = false;
   }

   if ( m_IndexOSDEnabled == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[1]->getSelectedIndex() )
      {
         params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_LAYOUT_ENABLED;
         params.osd_flags3[iScreenIndex] &= ~OSD_FLAG3_LAYOUT_ENABLED_PLUGINS_ONLY;
      }
      else if ( 1 == m_pItemsSelect[1]->getSelectedIndex() )
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_LAYOUT_ENABLED;
      else
      {
         params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_LAYOUT_ENABLED;
         params.osd_flags3[iScreenIndex] |= OSD_FLAG3_LAYOUT_ENABLED_PLUGINS_ONLY;
      }
      sendToVehicle = true;
   }

   if ( m_IndexOSDLayout == m_SelectedIndex )
   {
      params.osd_layout_preset[iScreenIndex] = m_pItemsSelect[7]->getSelectedIndex();
      sendToVehicle = true;

      if ( params.osd_layout_preset[iScreenIndex] < OSD_PRESET_DEFAULT )
      {
         Preferences* pP = get_Preferences();
         ControllerSettings* pCS = get_ControllerSettings();
         pP->iShowControllerCPUInfo = 0;
         pCS->iShowVoltage = 0;
         save_Preferences();
         save_ControllerSettings();
      }
   }

   if ( m_IndexOSDFontSize == m_SelectedIndex )
   {
      params.osd_preferences[iScreenIndex] &= 0xFFFFFF00;
      params.osd_preferences[iScreenIndex] |= (u32)m_pItemsSelect[2]->getSelectedIndex();
      sendToVehicle = true;

      u32 scale = params.osd_preferences[iScreenIndex] & 0xFF;
      osd_setScaleOSD((int)scale);
      osd_apply_preferences();
      applyFontScaleChanges();
   }

   if ( m_IndexOSDTransparency == m_SelectedIndex )
   {
      params.osd_preferences[iScreenIndex] &= 0xFFFF00FF;
      params.osd_preferences[iScreenIndex] |= ((((u32)m_pItemsSelect[3]->getSelectedIndex()) << OSD_PREFERENCES_OSD_TRANSPARENCY_SHIFT) & OSD_PREFERENCES_OSD_TRANSPARENCY_BITMASK);
      sendToVehicle = true;
   }

   if ( m_IndexBgOnTexts == m_SelectedIndex )
   {
      params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_BACKGROUND_ON_TEXTS_ONLY;
      params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_BGBARS;
      if ( 1 == m_pItemsSelect[4]->getSelectedIndex() )
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_BACKGROUND_ON_TEXTS_ONLY;
      if ( 2 == m_pItemsSelect[4]->getSelectedIndex() )
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_BGBARS;
      sendToVehicle = true;
   }

   if ( m_IndexHighlightChangeElements == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[5]->getSelectedIndex() )
         params.osd_flags3[iScreenIndex] &= ~OSD_FLAG3_HIGHLIGHT_CHANGING_ELEMENTS;
      else
         params.osd_flags3[iScreenIndex] |= OSD_FLAG3_HIGHLIGHT_CHANGING_ELEMENTS;
      sendToVehicle = true;
   }

   if ( m_IndexDontShowFCMessages == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[6]->getSelectedIndex() )
         params.osd_preferences[iScreenIndex] &= ~OSD_PREFERENCES_BIT_FLAG_DONT_SHOW_FC_MESSAGES;
      else
         params.osd_preferences[iScreenIndex] |= OSD_PREFERENCES_BIT_FLAG_DONT_SHOW_FC_MESSAGES;
      sendToVehicle = true;
   }

   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP) )
   if ( (-1 != m_IndexOSDShowMSPOSD) && (m_IndexOSDShowMSPOSD == m_SelectedIndex) )
   {
      if ( 0 == m_pItemsSelect[11]->getSelectedIndex() )
         params.osd_flags3[iScreenIndex] &= ~OSD_FLAG3_RENDER_MSP_OSD;
      else
         params.osd_flags3[iScreenIndex] |= OSD_FLAG3_RENDER_MSP_OSD;
      sendToVehicle = true;
   }

   /*
   if ( (-1 != m_IndexOSDReset) && (m_IndexOSDReset == m_SelectedIndex) )
   {
      osd_parameters_t paramsTemp;
      memcpy(&paramsTemp, &(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));
      g_pCurrentModel->resetOSDFlags(iScreenIndex);
      memcpy(&params, &(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));
      memcpy(&(g_pCurrentModel->osd_params), &paramsTemp, sizeof(osd_parameters_t));
      sendToVehicle = true;
   }
   */

   if ( (-1 != m_IndexShowFull) && (m_IndexShowFull == m_SelectedIndex) )
   {
      m_bShowCompact = false;
      m_SelectedIndex = 0;
      onFocusedItemChanged();
      addItems();
      return;
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
