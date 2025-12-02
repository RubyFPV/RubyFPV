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

#include "../../base/plugins_settings.h"
#include "../../base/core_plugins_settings.h"
#include "../../base/hardware_files.h"

#include "menu.h"
#include "menu_controller_plugins.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_confirmation.h"

#include "osd_plugins.h"
#include "../process_router_messages.h"

#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

MenuControllerPlugins::MenuControllerPlugins(void)
:Menu(MENU_ID_CONTROLLER_PLUGINS, "Controller Plugins", NULL)
{
   m_Width = 0.24;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.2;

   readPlugins();
}

MenuControllerPlugins::~MenuControllerPlugins()
{
}

void MenuControllerPlugins::readPlugins()
{
   removeAllItems();
   m_SelectedIndex = 0;

   addMenuItem(new MenuItemSection("Core Plugins"));

   m_iCountCorePlugins = get_CorePluginsCount();

   if ( 0 == m_iCountCorePlugins )
      addMenuItem( new MenuItemText("No core plugins installed on this controller.") );
   else
   {
      for( int i=0; i<m_iCountCorePlugins; i++ )
      {
         char* szName = get_CorePluginName(i);
         m_pItemsSelectCore[i] = new MenuItemSelect(szName, "Enables, disables or removes this plugin");
         m_pItemsSelectCore[i]->addSelection("Disabled");
         m_pItemsSelectCore[i]->addSelection("Enabled");
         m_pItemsSelectCore[i]->addSelection("Delete");
         m_pItemsSelectCore[i]->setIsEditable();
         m_IndexCorePlugins[i] = addMenuItem(m_pItemsSelectCore[i]);
         log_line("Added Core Plugin %d of %d at menu index %d.", i+1, m_iCountCorePlugins, m_IndexCorePlugins[i]);
      }
   }
   addMenuItem(new MenuItemSection("OSD Plugins"));

   m_iCountOSDPlugins = osd_plugins_get_count();

   for( int i=0; i<m_iCountOSDPlugins; i++ )
   {
      if ( i >= MAX_OSD_PLUGINS )
         break;

      char* szName = osd_plugins_get_name(i);
      m_pItemsSelect[i] = new MenuItemSelect(szName, "Enables, disables or removes this plugin");
      m_pItemsSelect[i]->addSelection("Disabled");
      m_pItemsSelect[i]->addSelection("Enabled");
      m_pItemsSelect[i]->addSelection("Delete");
      m_pItemsSelect[i]->setIsEditable();
      m_IndexOSDPlugins[i] = addMenuItem(m_pItemsSelect[i]);
      log_line("Added OSD Plugin %d of %d at menu index %d.", i+1, m_iCountOSDPlugins, m_IndexOSDPlugins[i]);
   }

   if ( 0 == m_iCountOSDPlugins )
   {
      addMenuItem(new MenuItemText("No plugins installed on the system."));
   }

   addMenuItem(new MenuItemSection("Manage Plugins"));

   m_IndexImport = addMenuItem(new MenuItem("Import Plugins", "Import new plugins from a USB memory stick."));
   m_pMenuItems[m_IndexImport]->showArrow();
   m_IndexSelectedPlugin = -1;
}

void MenuControllerPlugins::valuesToUI()
{
   log_line("MenuControllerPlugins update UI values...");
   for( int i=0; i<m_iCountOSDPlugins; i++ )
   {
      if ( i >= MAX_OSD_PLUGINS )
         break;

      char* szUID = osd_plugins_get_uid(i);
      SinglePluginSettings* pPlugin = pluginsGetByUID(szUID);
      if ( NULL == pPlugin )
         continue;

      if ( pPlugin->nEnabled )
         m_pItemsSelect[i]->setSelectedIndex(1);
      else
         m_pItemsSelect[i]->setSelectedIndex(0);
   }

   for( int i=0; i<m_iCountCorePlugins; i++ )
   {
      if ( i >= MAX_CORE_PLUGINS_COUNT )
         break;

      char* szGUID = get_CorePluginGUID(i);
      CorePluginSettings* pSettings = get_CorePluginSettings(szGUID);
      if ( NULL == pSettings )
         continue;

      if ( pSettings->iEnabled )
         m_pItemsSelectCore[i]->setSelectedIndex(1);
      else
         m_pItemsSelectCore[i]->setSelectedIndex(0);
   }
   log_line("MenuControllerPlugins updated UI values.");
}

void MenuControllerPlugins::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuControllerPlugins::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   if ( (2 == iChildMenuId/1000) && (1 == returnValue) && (-1 != m_IndexSelectedPlugin) )
   {
      char* szUID = osd_plugins_get_uid(m_IndexSelectedPlugin);
      log_line("User confirmed deletion of OSD plugin %d: %s, %s", m_IndexSelectedPlugin+1, osd_plugins_get_short_name(m_IndexSelectedPlugin),szUID );
      delete_PluginByUID(szUID);
      save_PluginsSettings();
      osd_plugins_delete(m_IndexSelectedPlugin);
      readPlugins();
      valuesToUI();
      log_line("Plugin deletion complete.");
      return;
   }
   
   if ( (5 == iChildMenuId/1000) && (1 == returnValue) && (-1 != m_IndexSelectedPlugin) )
   {
      char* szGUID = get_CorePluginGUID(m_IndexSelectedPlugin);
      char* szName = get_CorePluginName(m_IndexSelectedPlugin);
      log_line("User confirmed deletion of core plugin %d: %s, %s", m_IndexSelectedPlugin+1, szName, szGUID );
      delete_CorePlugin(szGUID);
      readPlugins();
      valuesToUI();

      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROLLER_RELOAD_CORE_PLUGINS, STREAM_ID_DATA);
      PH.vehicle_id_src = PACKET_COMPONENT_COMMANDS;
      PH.vehicle_id_dest = PACKET_COMPONENT_COMMANDS;
      PH.total_length = sizeof(t_packet_header);
 
      u8 buffer[MAX_PACKET_TOTAL_SIZE];
      memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
      send_packet_to_router(buffer, PH.total_length);
      return;
   }
   
   if ( (1 == iChildMenuId/1000) && (1 == returnValue) )
   {
      importFromUSB();
      return;
   }
}


void MenuControllerPlugins::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( (m_iCountOSDPlugins > 0) && (m_SelectedIndex >= m_IndexOSDPlugins[0]) && (m_SelectedIndex <= m_IndexOSDPlugins[m_iCountOSDPlugins-1]) )
   {
      m_IndexSelectedPlugin = m_SelectedIndex - m_IndexOSDPlugins[0];
      log_line("MenuControllerPlugins selected OSD plugin %d", m_IndexSelectedPlugin+1);
      int iAction = m_pItemsSelect[m_IndexSelectedPlugin]->getSelectedIndex();
      if ( iAction == 0 || iAction == 1 )
      {
         char* szUID = osd_plugins_get_uid(m_IndexSelectedPlugin);
         SinglePluginSettings* pPlugin = pluginsGetByUID(szUID);
         if ( NULL == pPlugin )
         {
            log_softerror_and_alarm("MenuControllerPlugins failed to get plugins settings for OSD plugin %d of %d", m_IndexSelectedPlugin+1, m_iCountOSDPlugins );
            return;
         }
         if ( iAction == 0 )
            pPlugin->nEnabled = 0;
         if ( iAction == 1 )
            pPlugin->nEnabled = 1;
         save_PluginsSettings();
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

   if ( (m_iCountCorePlugins > 0) && (m_SelectedIndex >= m_IndexCorePlugins[0]) && (m_SelectedIndex <= m_IndexCorePlugins[m_iCountCorePlugins-1]) )
   {
      m_IndexSelectedPlugin = m_SelectedIndex - m_IndexCorePlugins[0];
      log_line("MenuControllerPlugins selected core plugin %d", m_IndexSelectedPlugin+1);

      int iAction = m_pItemsSelectCore[m_IndexSelectedPlugin]->getSelectedIndex();
      if ( iAction == 0 || iAction == 1 )
      {
         CorePluginSettings* pSettings = get_CorePluginSettings(get_CorePluginGUID(m_IndexSelectedPlugin));
         if ( NULL == pSettings )
         {
            log_softerror_and_alarm("MenuControllerPlugins failed to get plugins settings for core plugin %d of %d", m_IndexSelectedPlugin+1, m_iCountCorePlugins );
            return;
         }
         if ( iAction == 0 )
            pSettings->iEnabled = 0;
         if ( iAction == 1 )
            pSettings->iEnabled = 1;
         save_CorePluginsSettings();
         valuesToUI();
         return;
      }
      if ( iAction == 2 )
      {
         MenuConfirmation* pMC = new MenuConfirmation("Delete Plugin","Are you sure you want to delete this plugin?",5);
         pMC->m_yPos = 0.3;
         add_menu_to_stack(pMC);
         return;
      }
      return;
   }

   if ( m_IndexImport == m_SelectedIndex )
   {
      MenuConfirmation* pMC = new MenuConfirmation("Import Plugins","Insert an USB stick containing your plugins and then press Ok to start the import process.",1, true);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      return;
   }
   
}

void MenuControllerPlugins::importFromUSB()
{
   int iMountRes = hardware_try_mount_usb();
   if ( 1 != iMountRes )
   {
      if ( 0 == iMountRes )
         addMessage("No USB memory stick detected. Please insert a USB stick.");
      else
         addMessage("USB memory stick detected but could not be mounted. Please try again.");
      return;
   }

   DIR *d;
   FILE* fd;
   struct dirent *dir;
   char szFile[1024];
   char szComm[1024];
   log_line("Searching for plugins...");
   
   d = opendir(FOLDER_USB_MOUNT);
   if (!d)
   {
      log_softerror_and_alarm("Failed to open USB mount dir to search for plugins.");
      addMessage("Failed to read the USB memory stick.");
      hardware_unmount_usb();
      return;
   }

   int countImportedTotal = 0;
   int countInvalidOSD = 0;
   int countImportedOSD = 0;
   int countImportedCore = 0;

   while ((dir = readdir(d)) != NULL)
   {
      if ( strlen(dir->d_name) < 4 )
         continue;

      sprintf(szFile, "%s/%s", FOLDER_USB_MOUNT, dir->d_name);
      long lSize = 0;
      fd = fopen(szFile, "rb");
      if ( NULL != fd )
      {
         fseek(fd, 0, SEEK_END);
         lSize = ftell(fd);
         fseek(fd, 0, SEEK_SET);
         fclose(fd);
      }
      else
         log_softerror_and_alarm("Failed to open file [%s] for checking it's size.", szFile);
      if ( lSize < 3 )
         continue;

      if ( NULL != strstr(dir->d_name, ".png") )
      {
         sprintf(szComm, "cp -rf %s/%s %s", FOLDER_USB_MOUNT, dir->d_name, FOLDER_OSD_PLUGINS);
         hw_execute_bash_command(szComm, NULL);
         continue;
      }

      if ( NULL == strstr(dir->d_name, ".so") )
         continue;
      sprintf(szFile, "%s/%s", FOLDER_USB_MOUNT, dir->d_name);
      void* pLibrary = NULL;
      pLibrary = dlopen(szFile, RTLD_LAZY | RTLD_GLOBAL);
      if ( NULL == pLibrary )
      {
         log_softerror_and_alarm("Invalid plugin library on USB stick: %s. Skipped.", dir->d_name);
         countInvalidOSD++;
         continue;
      }

      void (*pFunctionOSDInit)(void*);
      void (*pFunctionOSDRender)(vehicle_and_telemetry_info_t*, plugin_settings_info_t2*, float, float, float, float);
      char* (*pFunctionOSDGetName)(void);
      char* (*pFunctionOSDGetUID)(void);
   
      pFunctionOSDInit = (void (*)(void*)) dlsym(pLibrary, "init");
      pFunctionOSDRender = (void (*)(vehicle_and_telemetry_info_t*, plugin_settings_info_t2*, float, float, float, float)) dlsym(pLibrary, "render");
      pFunctionOSDGetName = (char* (*)(void)) dlsym(pLibrary, "getName");
      pFunctionOSDGetUID = (char* (*)(void)) dlsym(pLibrary, "getUID");
      
      if ( (NULL != pFunctionOSDInit) && (NULL != pFunctionOSDRender) && (NULL != pFunctionOSDGetName) && (NULL != pFunctionOSDGetUID) )
      {
         dlclose(pLibrary);
         sprintf(szComm, "cp -rf %s/%s %s", FOLDER_USB_MOUNT, dir->d_name, FOLDER_OSD_PLUGINS);
         hw_execute_bash_command(szComm, NULL);
      
         log_line("Found OSD plugin: [%s]", dir->d_name);
         countImportedOSD++;
         countImportedTotal++;
         continue;
      }
      else
         countInvalidOSD++;

      int (*pFunctionCoreInit)(u32, u32);
      u32 (*pFunctionCoreRequestCapab)(void);
      const char* (*pFunctionCoreGetName)(void);
      const char* (*pFunctionCoreGetUID)(void);
   
      pFunctionCoreInit = (int (*)(u32, u32)) dlsym(pLibrary, "core_plugin_init");
      pFunctionCoreRequestCapab = (u32 (*)(void)) dlsym(pLibrary, "core_plugin_on_requested_capabilities");
      pFunctionCoreGetName = (const char* (*)(void)) dlsym(pLibrary, "core_plugin_get_name");
      pFunctionCoreGetUID = (const char* (*)(void)) dlsym(pLibrary, "core_plugin_get_guid");

      if ( (NULL != pFunctionCoreInit) && (NULL != pFunctionCoreRequestCapab) && (NULL != pFunctionCoreGetName) && (NULL != pFunctionCoreGetUID) )
      {
         countInvalidOSD--;
         dlclose(pLibrary);
         sprintf(szComm, "cp -rf %s/%s %s", FOLDER_USB_MOUNT, dir->d_name, FOLDER_CORE_PLUGINS);
         hw_execute_bash_command(szComm, NULL);
      
         log_line("Found Core plugin: [%s]", dir->d_name);
         countImportedCore++;
         countImportedTotal++;
         continue;
      }
      dlclose(pLibrary);
   }
   
   closedir(d);

   log_line("Searching for plugins complete.");
   hardware_unmount_usb();
   char szBuff[128];

   if ( 0 == countImportedTotal )
   {
      if ( 0 == countInvalidOSD )
         addMessage("No plugins found.");
      else
      {
         sprintf(szBuff, "No valid plugins found. Found %d invalid plugins.", countInvalidOSD);
         addMessage(szBuff);
      }

      hardware_unmount_usb();
      return;
   }
   if ( 1 == countImportedTotal )
   {
      if ( 1 == countImportedOSD )
         strcpy(szBuff, "Found and imported one OSD plugin.");
      else if ( 1 == countImportedCore )
         strcpy(szBuff, "Found and imported one core plugin.");
      else
         strcpy(szBuff, "Found and imported one plugin.");
   }
   else
   {
      if ( 0 < countImportedOSD && 0 < countImportedCore )
         sprintf(szBuff, "Found and imported %d plugins (%d OSD plugins, %d core plugins).", countImportedTotal, countImportedOSD, countImportedCore);
      else if ( 0 < countImportedOSD )
         sprintf(szBuff, "Found and imported %d plugins (%d OSD plugins).", countImportedTotal, countImportedOSD);
      else if ( 0 < countImportedCore )
         sprintf(szBuff, "Found and imported %d plugins (%d core plugins).", countImportedTotal, countImportedCore);
      else
         sprintf(szBuff, "Found and imported %d plugins.", countImportedTotal);
   }
   addMessage(szBuff);
   hardware_unmount_usb();

   if ( countImportedOSD > 0 )
   {
      osd_plugins_load();

      for( int i=osd_plugins_get_count()-countImportedOSD; i<osd_plugins_get_count(); i++ )
      {
         if ( i < 0 || i >= MAX_OSD_PLUGINS )
            break;
         if ( NULL == g_pCurrentModel )
            break;
         int layoutIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;
         g_pCurrentModel->osd_params.instruments_flags[layoutIndex] |= (INSTRUMENTS_FLAG_SHOW_FIRST_OSD_PLUGIN << i);
      }

      handle_commands_send_single_oneway_command(5, COMMAND_ID_SET_OSD_PARAMS, 0, (u8*)&(g_pCurrentModel->osd_params), sizeof(osd_parameters_t), 5);

      readPlugins();
   }

   if ( countImportedTotal > 0 )
      valuesToUI();

   if ( countImportedCore > 0 )
   {
      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROLLER_RELOAD_CORE_PLUGINS, STREAM_ID_DATA);
      PH.vehicle_id_src = PACKET_COMPONENT_COMMANDS;
      PH.vehicle_id_dest = PACKET_COMPONENT_COMMANDS;
      PH.total_length = sizeof(t_packet_header);
      u8 buffer[MAX_PACKET_TOTAL_SIZE];
      memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
      send_packet_to_router(buffer, PH.total_length);
   }
}
