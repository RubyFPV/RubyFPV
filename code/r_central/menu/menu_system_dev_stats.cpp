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
#include "menu_objects.h"
#include "menu_controller.h"
#include "menu_text.h"
#include "menu_system_dev_stats.h"
#include "menu_system_dev_logs.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"
#include "../../radio/radiolink.h"
#include "../../base/utils.h"
#include "../rx_scope.h"

#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>


MenuSystemDevStats::MenuSystemDevStats(void)
:Menu(MENU_ID_SYSTEM_DEV_STATS, "Developer Stats Windows", NULL)
{
   m_Width = 0.36;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.16;
   
   m_pItemsSelect[0] = new MenuItemSelect("Show Retransmissions Stats", "Shows the extended developer video retransmissions stats.");
   m_pItemsSelect[0]->addSelection("Off");
   m_pItemsSelect[0]->addSelection("On");
   m_pItemsSelect[0]->setUseMultiViewLayout();
   m_IndexDevStatsVideo = addMenuItem(m_pItemsSelect[0]);


   m_pItemsSelect[1] = new MenuItemSelect("Show Extended Radio Stats", "Shows the extended developer radio stats.");
   m_pItemsSelect[1]->addSelection("Off");
   m_pItemsSelect[1]->addSelection("On");
   m_pItemsSelect[1]->setUseMultiViewLayout();
   m_IndexDevStatsRadio = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSelect[2] = new MenuItemSelect("Show Full Radio RX Stats", "Shows full statistics about radio stack state.");
   m_pItemsSelect[2]->addSelection("Off");
   m_pItemsSelect[2]->addSelection("On");
   m_pItemsSelect[2]->setUseMultiViewLayout();
   m_IndexDevFullRXStats = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSelect[7] = new MenuItemSelect("Show Controller Adaptive Video Info Stats", "");
   m_pItemsSelect[7]->addSelection("Off");
   m_pItemsSelect[7]->addSelection("On");
   m_pItemsSelect[7]->setUseMultiViewLayout();
   m_IndexShowControllerAdaptiveInfoStats = addMenuItem(m_pItemsSelect[7]);

   m_pItemsSelect[3] = new MenuItemSelect("Show Vehicle Tx Stats", "Shows graphs about vehicle transmissions.");
   m_pItemsSelect[3]->addSelection("Off");
   m_pItemsSelect[3]->addSelection("On");
   m_pItemsSelect[3]->setUseMultiViewLayout();
   m_IndexDevStatsVehicleTx = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSelect[8] = new MenuItemSelect("Show Vehicle Stats", "Shows info about vehicle state.");
   m_pItemsSelect[8]->addSelection("Off");
   m_pItemsSelect[8]->addSelection("On");
   m_pItemsSelect[8]->setUseMultiViewLayout();
   m_IndexDevStatsVehicle = addMenuItem(m_pItemsSelect[8]);

   m_pItemsSelect[4] = new MenuItemSelect("Show Video Bitrate History", "Shows history graph on video bitrate and radio datarates.");
   m_pItemsSelect[4]->addSelection("Off");
   m_pItemsSelect[4]->addSelection("On");
   m_pItemsSelect[4]->setUseMultiViewLayout();
   m_IndexDevVehicleVideoBitrateHistory = addMenuItem(m_pItemsSelect[4]);

   for( int i=0; i<m_ItemsCount; i++ )
      m_pMenuItems[i]->setTextColor(get_Color_Dev());
}

void MenuSystemDevStats::valuesToUI()
{
   Preferences* pP = get_Preferences();

   if ( NULL == g_pCurrentModel )
      return;

   int layoutIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;

   m_pItemsSelect[0]->setSelectedIndex(pP->iDebugShowDevVideoStats);
   m_pItemsSelect[1]->setSelectedIndex(pP->iDebugShowDevRadioStats);
   m_pItemsSelect[2]->setSelectedIndex(pP->iDebugShowFullRXStats);
   
   m_pItemsSelect[3]->setSelectedIndex(0);
   if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_TX_GAP )
      m_pItemsSelect[3]->setSelectedIndex(1);

   m_pItemsSelect[4]->setSelectedIndex(0);
   if ( g_pCurrentModel->osd_params.osd_flags3[layoutIndex] & OSD_FLAG3_SHOW_VIDEO_BITRATE_HISTORY )
      m_pItemsSelect[4]->setSelectedIndex(1);

   m_pItemsSelect[7]->setSelectedIndex(0);
   if ( g_pCurrentModel->osd_params.osd_flags3[layoutIndex] & OSD_FLAG3_SHOW_CONTROLLER_ADAPTIVE_VIDEO_INFO )
      m_pItemsSelect[7]->setSelectedIndex(1);
   

   m_pItemsSelect[8]->setSelectedIndex(0);
   if ( g_pCurrentModel->osd_params.osd_flags3[layoutIndex] & OSD_FLAG3_SHOW_VEHICLE_DEV_STATS )
      m_pItemsSelect[8]->setSelectedIndex(1);
}

void MenuSystemDevStats::onShow()
{
   Menu::onShow();
}

void MenuSystemDevStats::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yTop);
}


void MenuSystemDevStats::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( NULL == g_pCurrentModel )
   {
      addMessageNeedsVehcile("You need to be connected to a vehicle to change this.", 1);
      valuesToUI();
      return;
   }

   Preferences* pP = get_Preferences();

   if ( m_IndexDevStatsVideo == m_SelectedIndex )
   {
      pP->iDebugShowDevVideoStats = m_pItemsSelect[0]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
   }

   if ( m_IndexDevStatsRadio == m_SelectedIndex )
   {
      pP->iDebugShowDevRadioStats = m_pItemsSelect[1]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
   }

   if ( m_IndexDevFullRXStats == m_SelectedIndex )
   {
      pP->iDebugShowFullRXStats = m_pItemsSelect[2]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
   }

   if ( m_IndexShowControllerAdaptiveInfoStats == m_SelectedIndex )
   {
      osd_parameters_t params;
      memcpy(&params, &(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));
      int layoutIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;

      params.osd_flags3[layoutIndex] &= ~OSD_FLAG3_SHOW_CONTROLLER_ADAPTIVE_VIDEO_INFO;
      if ( 1 == m_pItemsSelect[7]->getSelectedIndex() )
         params.osd_flags3[layoutIndex] |= OSD_FLAG3_SHOW_CONTROLLER_ADAPTIVE_VIDEO_INFO;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_OSD_PARAMS, 0, (u8*)&params, sizeof(osd_parameters_t)) )
         valuesToUI();
      return;
   }
   
   if ( m_IndexDevStatsVehicleTx == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[3]->getSelectedIndex() )
         g_pCurrentModel->uDeveloperFlags &= (~DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_TX_GAP);
      else
         g_pCurrentModel->uDeveloperFlags |= DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_TX_GAP;
      if ( ! handle_commands_send_developer_flags(g_pCurrentModel->uDeveloperFlags) )
         valuesToUI();  
      return;    
   }

   if ( m_IndexDevStatsVehicle == m_SelectedIndex )
   {
      osd_parameters_t params;
      memcpy(&params, &(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));
      int layoutIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;

      params.osd_flags3[layoutIndex] &= ~OSD_FLAG3_SHOW_VEHICLE_DEV_STATS;
      if ( 1 == m_pItemsSelect[8]->getSelectedIndex() )
         params.osd_flags3[layoutIndex] |= OSD_FLAG3_SHOW_VEHICLE_DEV_STATS;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_OSD_PARAMS, 0, (u8*)&params, sizeof(osd_parameters_t)) )
         valuesToUI();
      return;    
   }

   if ( m_IndexDevVehicleVideoBitrateHistory == m_SelectedIndex )
   {
      osd_parameters_t params;
      memcpy(&params, &(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));
      int layoutIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;

      params.osd_flags3[layoutIndex] &= ~OSD_FLAG3_SHOW_VIDEO_BITRATE_HISTORY;
      if ( 1 == m_pItemsSelect[4]->getSelectedIndex() )
         params.osd_flags3[layoutIndex] |= OSD_FLAG3_SHOW_VIDEO_BITRATE_HISTORY;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_OSD_PARAMS, 0, (u8*)&params, sizeof(osd_parameters_t)) )
         valuesToUI();
   }
}

