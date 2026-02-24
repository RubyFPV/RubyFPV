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
#include "menu_vehicle_management.h"
#include "menu_vehicle_general.h"
#include "menu_vehicle_expert.h"
#include "menu_confirmation.h"
#include "menu_vehicle_management_plugins.h"
#include <ctype.h>
#include "../osd/osd_common.h"

MenuVehicleManagePlugins::MenuVehicleManagePlugins(void)
:Menu(MENU_ID_VEHICLE_MANAGE_PLUGINS, "Manage Vehicle Core Plugins", NULL)
{
   m_Width = 0.24;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.18;
   m_fIconSize = 1.0;
   m_pPopup = NULL;
   m_bWaitingForVehicleInfo = false;
   m_iCountPlugins = 0;
   m_IndexSelectedPlugin = -1;
}

MenuVehicleManagePlugins::~MenuVehicleManagePlugins()
{
   if ( NULL != m_pPopup )
   {
      popups_remove(m_pPopup);
      delete m_pPopup;
      m_pPopup = NULL;
   }
}

void MenuVehicleManagePlugins::onShow()
{
   m_Height = 0.0;
   populateInfo();
   Menu::onShow();
}

void MenuVehicleManagePlugins::populateInfo()
{
   removeAllItems();
   removeAllTopLines();

   m_iCountPlugins = 0;
   m_IndexSelectedPlugin = -1;

   if ( 0 == g_iVehicleCorePluginsCount )
   {
      addTopLine("No core plugins installed on this vehicle.");
      char szBuff[256];
      if ( 0 == get_CorePluginsCount() )
         sprintf(szBuff, "No core plugins installed on this controller.");
      else if ( 1 == get_CorePluginsCount() )
         sprintf(szBuff, "1 core plugin available on this controller.");
      else
         sprintf(szBuff, "%d core plugins available on this controller.", get_CorePluginsCount());
      addTopLine(szBuff);
   }
   else
   {
      m_iCountPlugins = g_iVehicleCorePluginsCount;
      for( int i=0; i<m_iCountPlugins; i++ )
      {
         char* szName = g_listVehicleCorePlugins[i].szName;
         m_pItemsSelect[i] = new MenuItemSelect(szName, "Enables, disables or removes this plugin.");
         m_pItemsSelect[i]->addSelection("Disabled");
         m_pItemsSelect[i]->addSelection("Enabled");
         m_pItemsSelect[i]->addSelection("Delete");
         m_pItemsSelect[i]->setIsEditable();
         m_IndexPlugins[i] = addMenuItem(m_pItemsSelect[i]);

         if ( g_listVehicleCorePlugins[i].iEnabled )
            m_pItemsSelect[i]->setSelectedIndex(1);
         else
            m_pItemsSelect[i]->setSelectedIndex(0);
      }
   }

   m_IndexSync = addMenuItem(new MenuItem("Sync Plugins From Controller", "Gets all plugins from controller and install them on this vehicle."));

   if ( 0 == g_iVehicleCorePluginsCount && 0 == get_CorePluginsCount() )
      m_pMenuItems[m_IndexSync]->setEnabled(false);
}

void MenuVehicleManagePlugins::Render()
{
   RenderPrepare();

   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


bool MenuVehicleManagePlugins::periodicLoop()
{
   static int s_iStaticMenuManagePluginsCounter = 0;
   s_iStaticMenuManagePluginsCounter++;

   if ( NULL == m_pPopup )
   {
      if ( ! m_bWaitingForVehicleInfo )
         return false;

      if ( ! handle_commands_is_command_in_progress() )
      {
         if ( handle_commands_has_received_vehicle_core_plugins_info() )
         {
            m_bWaitingForVehicleInfo = false;
            populateInfo();
            Popup* p = new Popup("Core plugins uploaded.", 0.3, 0.3, 0.5, 4 );
            popups_add_topmost(p);
         }
      }
      return false;
   }

   char szBuff[128];
   sprintf(szBuff, "Uploading core plugins. Please wait..");
   for( int i=0; i<(s_iStaticMenuManagePluginsCounter%3); i++ )
      strcat(szBuff, ".");
   m_pPopup->setTitle(szBuff);

   if ( g_bHasFileUploadInProgress )
      return true;

   popups_remove(m_pPopup);
   delete m_pPopup;
   m_pPopup = NULL;

   if ( g_CurrentUploadingFile.uTotalSegments == 0 )
   {
      Popup* p = new Popup("Failed to upload plugins. Error from vehicle.", 0.3, 0.3, 0.5, 4 );
      popups_add_topmost(p);
   }
   else
   {
      handle_commands_reset_has_received_vehicle_core_plugins_info();
      m_bWaitingForVehicleInfo = false;

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_GET_CORE_PLUGINS_INFO, 0, NULL, 0) )
      {
         Popup* p = new Popup("Core plugins uploaded.", 0.3, 0.3, 0.5, 4 );
         popups_add_topmost(p);

         valuesToUI();
      }
      else
         m_bWaitingForVehicleInfo = true;
   }

   valuesToUI();

   return false;
}

int MenuVehicleManagePlugins::onBack()
{
   if ( NULL != m_pPopup )
      return 1;

   return Menu::onBack();
}


void MenuVehicleManagePlugins::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   if ( (2 == iChildMenuId/1000) && (1 == returnValue) && (-1 != m_IndexSelectedPlugin) )
   {
      for( int i=m_IndexSelectedPlugin; i<g_iVehicleCorePluginsCount; i++ )
         memcpy((u8*)&(g_listVehicleCorePlugins[i]), (u8*)&(g_listVehicleCorePlugins[i+1]), sizeof(CorePluginSettings));
      g_iVehicleCorePluginsCount--;

      populateInfo();
      valuesToUI();
      return;
   }
}


void MenuVehicleManagePlugins::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( NULL == g_pCurrentModel )
   {
      Popup* p = new Popup("Vehicle is offline", 0.3, 0.3, 0.5, 4 );
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      valuesToUI();
      return;
   }

   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->is_spectator) )
   {
      Popup* p = new Popup("Vehicle Settings can not be changed on a spectator vehicle.", 0.3, 0.3, 0.5, 4 );
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      valuesToUI();
      return;
   }

   if ( m_IndexSync == m_SelectedIndex )
   {
      char szComm[256];

      strcpy(szComm, "rm -rf tmp/core_plugins.zip 2>&1");
      hw_execute_bash_command(szComm, NULL);

      strcpy(szComm, "zip tmp/core_plugins.zip plugins/core/*");
      hw_execute_bash_command(szComm, NULL);

      if ( NULL != m_pPopup )
      {
         popups_remove(m_pPopup);
         delete m_pPopup;
      }
      m_pPopup = new Popup("Uploading core plugins. Please wait...", 0.3, 0.3, 0.5, 200 );
      popups_add_topmost(m_pPopup);

      handle_commands_initiate_file_upload(FILE_ID_CORE_PLUGINS_ARCHIVE, "tmp/core_plugins.zip");
   }

   for( int i=0; i<m_iCountPlugins; i++ )
   {
      if ( m_SelectedIndex != m_IndexPlugins[i] )
         continue;
      m_IndexSelectedPlugin = i;
      int iAction = m_pItemsSelect[i]->getSelectedIndex();
      if ( iAction == 0 || iAction == 1 )
      {
         if ( iAction == 0 )
            g_listVehicleCorePlugins->iEnabled = 0;
         if ( iAction == 1 )
            g_listVehicleCorePlugins->iEnabled = 1;
         valuesToUI();
         return;
      }
      if ( iAction == 2 )
      {
         MenuConfirmation* pMC = new MenuConfirmation("Delete Plugin","Are you sure you want to delete this plugin?",2);
         pMC->m_yPos = 0.3;
         add_menu_to_stack(pMC);
         return;
      }
      return;
   }
}

