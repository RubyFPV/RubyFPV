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
#include "menu_vehicle_import.h"
#include "menu_confirmation.h"

#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include "../link_watch.h"
#include "../../base/hardware_files.h"

MenuVehicleImport::MenuVehicleImport(void)
:Menu(MENU_ID_VEHICLE_IMPORT, "Import Vehicle Settings", NULL)
{
   m_Width = 0.45;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.24;
   m_TempFilesCount = 0;
   buildSettingsFileList();
   m_bCreateNew = false;
}

MenuVehicleImport::~MenuVehicleImport()
{
   hardware_unmount_usb();
   sync();
   ruby_signal_alive();
   for( int i=0; i<m_TempFilesCount; i++ )
      free(m_szTempFiles[i]);
   m_TempFilesCount = 0;
}

int MenuVehicleImport::getSettingsFilesCount()
{
   return m_TempFilesCount;
}

void MenuVehicleImport::setCreateNew()
{
   m_bCreateNew = true;
}


void MenuVehicleImport::onShow()
{
   m_Height = 0.0;
   removeAllTopLines();
   removeAllItems();
   buildSettingsFileList();

   char szId[256];
   sprintf(szId, "%u", g_pCurrentModel->uVehicleId);
   bool found = false;
   int selectedIndex = -1;

   if ( ! m_bCreateNew )
   for( int i=0; i<m_TempFilesCount; i++ )
      if ( NULL != strstr(m_szTempFiles[i], szId) )
      {
         found = true;
         selectedIndex = i;
         break;
      }

   if ( found )
      addTopLine("Found matching settings file for current vehicle.");
   else if ( m_bCreateNew )
      addTopLine("Select the file you wish to import as a new vehicle.");
   else
      addTopLine("Select the file you wish to import. Your current settings for this vehicle will be overwritten with the ones from the file you import.");

   for( int i=0; i<m_TempFilesCount; i++ )
      addMenuItem(new MenuItem(m_szTempFiles[i]));

   if ( -1 != selectedIndex )
      m_SelectedIndex = selectedIndex;
   Menu::onShow();
}


void MenuVehicleImport::Render()
{
   RenderPrepare();
   
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehicleImport::onSelectItem()
{
   if ( m_SelectedIndex < 0 || m_SelectedIndex >= m_TempFilesCount )
      return;

   if ( ! m_bCreateNew )
   if ( (! pairing_isStarted()) || (NULL == g_pCurrentModel) || (! link_is_vehicle_online_now(g_pCurrentModel->uVehicleId)) )
   {
      Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"Not connected to vehicle",NULL);
      pm->m_xPos = 0.4; pm->m_yPos = 0.4;
      pm->m_Width = 0.36;
      pm->addTopLine("You can not import the settings if the vehicle is not running and connected to this controller.");
      add_menu_to_stack(pm);
      return;
   }

   Model m;
   char szFile[1024];
   sprintf(szFile, "%s/%s", FOLDER_USB_MOUNT, m_szTempFiles[m_SelectedIndex]);

   if ( ! m.loadFromFile(szFile, true) )
   {
      Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"Invalid file",NULL);
      pm->m_xPos = 0.4; pm->m_yPos = 0.4;
      pm->m_Width = 0.36;
      pm->addTopLine("This file is not a valid model settings file.");
      add_menu_to_stack(pm);
      return;
   }

   if ( m_bCreateNew )
   {
      Model* pModel = addNewModel(0, 0,0);
      pModel->loadFromFile(szFile, true);
      pModel->is_spectator = false;
      saveControllerModel(pModel);
      menu_discard_all();
      hardware_unmount_usb();
      ruby_signal_alive();
      sync();
      ruby_signal_alive();
      return;
   }

   u8 pData[2048];
   int length = 0;
   FILE* fd = fopen(szFile, "rb");
   if ( NULL != fd )
   {
      length = fread(pData, 1, 2000, fd);
      fclose(fd);
   }
   else
   {
      log_error_and_alarm("Failed to load current vehicle configuration from file: %s", szFile);
      return;
   }
 
   fd = fopen("tmp/tempVehicleSettings.txt", "wb");
   if ( NULL != fd )
   {
       fwrite(pData, 1, length, fd);
       fclose(fd);
       fd = NULL;
   } 

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_ALL_PARAMS, 0, pData, length) )
      valuesToUI();
   else
   {
      menu_discard_all();
      hardware_unmount_usb();
      ruby_signal_alive();
      sync();
      ruby_signal_alive();
   }
}

void MenuVehicleImport::buildSettingsFileList()
{
   DIR *d;
   struct dirent *dir;
   char szFolder[1024];

   for( int i=0; i<m_TempFilesCount; i++ )
      free(m_szTempFiles[i]);
   m_TempFilesCount = 0;

   sprintf(szFolder, "%s/", FOLDER_USB_MOUNT);
   log_line("Searching for model settings files on: %s", szFolder);
   d = opendir(szFolder);
   if (d)
   {
      while ((dir = readdir(d)) != NULL)
      {
         if ( strlen(dir->d_name) < strlen("ruby_model") )
            continue;
         if ( strncmp(dir->d_name, "ruby_model", strlen("ruby_model")) != 0 )
            continue;
         if ( dir->d_name[strlen(dir->d_name)-3] != 't' )
            continue;
         if ( dir->d_name[strlen(dir->d_name)-2] != 'x' )
            continue;
         if ( dir->d_name[strlen(dir->d_name)-1] != 't' )
            continue;
         if ( m_TempFilesCount >= MAX_SETTINGS_IMPORT_FILES )
            continue;
         m_szTempFiles[m_TempFilesCount] = (char*)malloc(strlen(dir->d_name)+1);
         if ( NULL == m_szTempFiles[m_TempFilesCount] )
            break;
         log_line("Add model settings file to list: %s", dir->d_name);
         strcpy(m_szTempFiles[m_TempFilesCount], dir->d_name);
         m_TempFilesCount++;
      }
      closedir(d);
   }
   else
      log_softerror_and_alarm("Failed to enumerate files on the USB stick");
}

void MenuVehicleImport::addMessage(const char* szMessage)
{
   Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"Info",NULL);
   pm->m_xPos = 0.4; pm->m_yPos = 0.4;
   pm->m_Width = 0.36;
   pm->addTopLine(szMessage);
   add_menu_to_stack(pm);
}
