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
#include "menu_system_dev_logs.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"
#include "../popup_log.h"
#include "../link_watch.h"
#include "../../radio/radiolink.h"
#include "../../base/utils.h"
#include "../../base/hardware_files.h"
#include "../rx_scope.h"

#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

MenuSystemDevLogs::MenuSystemDevLogs(void)
:Menu(MENU_ID_DEV_LOGS, L("Developer Log Settings"), NULL)
{
   m_Width = 0.34;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.16;
   
   m_pItemsSelect[0] = new MenuItemSelect(L("Log System (Controller)"), L("Sets the type of log system to use: directly to files or using a service. Requires a reboot after the change."));
   m_pItemsSelect[0]->addSelection(L("Files"));
   m_pItemsSelect[0]->addSelection(L("Service"));
   m_pItemsSelect[0]->setIsEditable();
   m_IndexLogServiceController = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[1] = new MenuItemSelect("Log System (Vehicle)", "Sets the type of log system to use: directly to files or using a service or disabled completly. Requires a reboot after the change.");
   m_pItemsSelect[1]->addSelection(L("Files"));
   m_pItemsSelect[1]->addSelection(L("Service"));
   m_pItemsSelect[1]->addSelection(L("Disabled"));
   m_pItemsSelect[1]->setIsEditable();
   m_IndexLogServiceVehicle = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSelect[6] = new MenuItemSelect(L("Vehicle Log Level"), L("How much logs are saved on current vehicle."));
   m_pItemsSelect[6]->addSelection(L("Full"));
   m_pItemsSelect[6]->addSelection(L("Errors"));
   m_pItemsSelect[6]->setIsEditable();
   m_IndexLogLevelVehicle = addMenuItem(m_pItemsSelect[6]);

   m_pItemsSelect[7] = new MenuItemSelect(L("Controller Log Level"), L("How much logs are saved on this controller."));
   m_pItemsSelect[7]->addSelection(L("Full"));
   m_pItemsSelect[7]->addSelection(L("Errors"));
   m_pItemsSelect[7]->setIsEditable();
   m_IndexLogLevelController = addMenuItem(m_pItemsSelect[7]);

   m_pItemsSelect[2] = new MenuItemSelect(L("Enable Vehicle Live Log"), L("Gets a stream of the live log for the current vehicle."));
   m_pItemsSelect[2]->addSelection(L("Disabled"));
   m_pItemsSelect[2]->addSelection(L("Enabled"));
   m_pItemsSelect[2]->setIsEditable();
   m_IndexEnableLiveLog = addMenuItem(m_pItemsSelect[2]);

   m_IndexGetVehicleLogs = addMenuItem( new MenuItem(L("Get Vehicle Logs")) );
   m_IndexZipAllLogs = addMenuItem( new MenuItem(L("Export all logs"), L("Exports all controller logs and all vehicle logs (that are already downloaded) to a USB memort stick.")) );
   m_pMenuItems[m_IndexZipAllLogs]->showArrow();

   m_IndexClearControllerLogs = addMenuItem( new MenuItem(L("Clear controller logs")) );
   m_pMenuItems[m_IndexClearControllerLogs]->showArrow();

   m_IndexClearVehicleLogs = addMenuItem( new MenuItem(L("Clear vehicle logs")) );
   m_pMenuItems[m_IndexClearVehicleLogs]->showArrow();

   //for( int i=0; i<m_ItemsCount; i++ )
   //   m_pMenuItems[i]->setTextColor(get_Color_Dev());
}

void MenuSystemDevLogs::valuesToUI()
{
   Preferences* pP = get_Preferences();

   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, LOG_USE_PROCESS);
   if ( access(szFile, R_OK) != -1 )
      m_pItemsSelect[0]->setSelectedIndex(1);
   else
      m_pItemsSelect[0]->setSelectedIndex(0);
         
   if ( NULL == g_pCurrentModel )
   {
      m_pItemsSelect[6]->setEnabled(false);
      m_pItemsSelect[1]->setEnabled(false);
      m_pItemsSelect[1]->setSelectedIndex(0);
   }
   else
   {
      m_pItemsSelect[1]->setEnabled(true);
      if ( g_pCurrentModel->uModelFlags & MODEL_FLAG_USE_LOGER_SERVICE )
         m_pItemsSelect[1]->setSelectedIndex(1);
      else if ( g_pCurrentModel->uModelFlags & MODEL_FLAG_DISABLE_ALL_LOGS )
         m_pItemsSelect[1]->setSelectedIndex(2);
      else
         m_pItemsSelect[1]->setSelectedIndex(0);

      m_pItemsSelect[2]->setEnabled(true);
      m_pItemsSelect[2]->setSelection( (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LIVE_LOG)?1:0 );

      if ( g_pCurrentModel->uModelFlags & MODEL_FLAG_DISABLE_ALL_LOGS )
         m_pItemsSelect[6]->setEnabled(false);
      else
         m_pItemsSelect[6]->setEnabled(true);
      m_pItemsSelect[6]->setSelectedIndex(0);
      if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS )
         m_pItemsSelect[6]->setSelectedIndex(1);
   }

   m_pItemsSelect[7]->setSelectedIndex(0);
   if ( pP->nLogLevel != 0 )
      m_pItemsSelect[7]->setSelectedIndex(1);
}

void MenuSystemDevLogs::onShow()
{
   Menu::onShow();
}


void MenuSystemDevLogs::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yTop);
}

void MenuSystemDevLogs::exportVehicleLogs(char* szVehicleFolder)
{
   if ( NULL == szVehicleFolder || 0 == szVehicleFolder[0] )
      return;

   DIR *d;
   struct dirent *dir;
   char szFolder[256];
   char szComm[256];

   sprintf(szFolder, "%s/%s/", FOLDER_MEDIA, szVehicleFolder);
   d = opendir(szFolder);
   if (d)
   {
      while ((dir = readdir(d)) != NULL)
      {
         if ( strlen(dir->d_name) < 4 )
            continue;
         ruby_signal_alive();
         sprintf(szComm, "mkdir -p %s/Ruby/%s/", FOLDER_USB_MOUNT, szVehicleFolder);
         hw_execute_bash_command(szComm, NULL);

         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s/%s %s/Ruby/%s/%s", szFolder, dir->d_name, FOLDER_USB_MOUNT, szVehicleFolder, dir->d_name);
         hw_execute_bash_command(szComm, NULL);
      }
      closedir(d);
   }
}

void MenuSystemDevLogs::exportAllLogs()
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
   ruby_signal_alive();

   char szOutput[1024];
   char szComm[1024];
   char szBuff[1024];
   hw_execute_bash_command_raw("which zip", szOutput);
   if ( 0 == strlen(szOutput) || NULL == strstr(szOutput, "zip") )
   {
      hardware_unmount_usb();
      addMessage("Failed, could not find zip program.");
      return;
   }

   ruby_signal_alive();
   ruby_pause_watchdog("export all logs");

   Popup* p = new Popup("Exporting. Please wait...",0.3,0.4, 0.5, 15);
   popups_add_topmost(p);
   ruby_processing_loop(true);
   g_TimeNow = get_current_timestamp_ms();
   render_all(g_TimeNow);
   ruby_signal_alive();
      
   hw_execute_bash_command("mkdir -p tmp/exportcontrollerlogs", NULL);
   hw_execute_bash_command("chmod 777 tmp/exportcontrollerlogs", NULL);
   hw_execute_bash_command("rm -rf tmp/exportcontrollerlogs/* 2>/dev/null", NULL);
   sprintf(szComm, "zip tmp/exportcontrollerlogs/ruby_controller_logs_controller_id_%u_%d.zip logs/*", g_uControllerId, g_iBootCount);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szBuff, "%s/Ruby", FOLDER_USB_MOUNT);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "mkdir -p %s", szBuff );
   hw_execute_bash_command(szComm, NULL);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf tmp/exportcontrollerlogs/ruby_controller_logs_controller_id_%u_%d.zip %s/Ruby/ruby_controller_logs_controller_id_%u_%d.zip", g_uControllerId, g_iBootCount, FOLDER_USB_MOUNT, g_uControllerId, g_iBootCount);
   hw_execute_bash_command(szComm, NULL);
   hw_execute_bash_command("rm -rf tmp/exportcontrollerlogs 2>/dev/null", NULL);

   DIR *d;
   struct dirent *dir;
   d = opendir(FOLDER_MEDIA);
   if (d)
   {
      while ((dir = readdir(d)) != NULL)
      {
         if ( strlen(dir->d_name) < 4 )
            continue;
         ruby_signal_alive();

         if ( strncmp(dir->d_name, "vehicle-", 8) != 0 )
            continue;
         exportVehicleLogs(dir->d_name);
      }
      closedir(d);
   }

   hardware_unmount_usb();
   ruby_signal_alive();
   sync();
   ruby_signal_alive();
   ruby_resume_watchdog("finished export all logs");
   popups_remove(p);
   addMessage("Done. All logs have been copied to the USB memory stick on a folder named Ruby. You can now remove the USB memory stick.");
}

void MenuSystemDevLogs::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   // Export controller logs to USB zip file
   if ( (1 == iChildMenuId/1000) && (1 == returnValue) )
   {
      exportAllLogs();
      return;
   }   

   // Delete controller logs
   if ( (2 == iChildMenuId/1000) && (1 == returnValue) )
   {
      hw_execute_bash_command("rm -rf logs/*", NULL);
      addMessage("Done. All controller logs have been cleared.");
      return;
   }   

   // Delete vehicle logs
   if ( (3 == iChildMenuId/1000) && (1 == returnValue) )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_CLEAR_LOGS, 0, NULL, 0) )
         valuesToUI();
      else
         addMessage("Done. All vehicle logs have been cleared.");
      return;
   }   
}


void MenuSystemDevLogs::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   Preferences* pP = get_Preferences();

   if ( m_IndexLogServiceController == m_SelectedIndex )
   {
      char szBuff[128];
      if ( 0 == m_pItemsSelect[0]->getSelectedIndex() )
         sprintf(szBuff, "rm -rf %s%s", FOLDER_CONFIG, LOG_USE_PROCESS);
      else
         sprintf(szBuff, "touch %s%s", FOLDER_CONFIG, LOG_USE_PROCESS);
      hw_execute_bash_command(szBuff, NULL);
      return;
   }

   if ( m_IndexLogServiceVehicle == m_SelectedIndex )
   {
      if ( (NULL == g_pCurrentModel) ||
           (!pairing_isStarted()) ||
           (!link_is_vehicle_online_now(g_pCurrentModel->uVehicleId)) )
      {
         addMessage("You must be connected to a vehicle.");
         valuesToUI();
         return;
      }
      if ( g_pCurrentModel->is_spectator )
      {
         addMessage("Can't change log settings on a spectator vehicle.");
         valuesToUI();
         return;
      }
      int val = m_pItemsSelect[1]->getSelectedIndex();
      u32 uFlags = g_pCurrentModel->uModelFlags;
      if ( 0 == val )
      {
         uFlags &= ~ (MODEL_FLAG_USE_LOGER_SERVICE | MODEL_FLAG_DISABLE_ALL_LOGS);
      }
      if ( 1 == val )
      {
        uFlags |= MODEL_FLAG_USE_LOGER_SERVICE;
        uFlags &= ~ MODEL_FLAG_DISABLE_ALL_LOGS;
      }
      if ( 2 == val )
      {
         uFlags |= MODEL_FLAG_DISABLE_ALL_LOGS;
      }
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_MODEL_FLAGS, uFlags, NULL, 0) )
         valuesToUI();  
      return;
   }

   if ( m_IndexEnableLiveLog == m_SelectedIndex )
   {
      if ( (NULL == g_pCurrentModel) ||
           (!pairing_isStarted()) ||
           (!link_is_vehicle_online_now(g_pCurrentModel->uVehicleId)) )
      {
         addMessage("You must be connected to a vehicle.");
         valuesToUI();
         return;
      }
      if ( g_pCurrentModel->is_spectator )
      {
         addMessage("Can't change log settings on a spectator vehicle.");
         valuesToUI();
         return;
      }
      u32 uDeveloperFlags = g_pCurrentModel->uDeveloperFlags;
      u32 enable = m_pItemsSelect[2]->getSelectedIndex();
      if ( 0 == enable )
      {
         uDeveloperFlags &= (~DEVELOPER_FLAGS_BIT_LIVE_LOG);
         popup_log_vehicle_remove();
      }
      else
         uDeveloperFlags |= DEVELOPER_FLAGS_BIT_LIVE_LOG;

      if ( ! handle_commands_send_developer_flags(uDeveloperFlags) )
         valuesToUI();
   }

   if ( m_IndexGetVehicleLogs == m_SelectedIndex )
   {
      if ( (NULL == g_pCurrentModel) ||
           (!pairing_isStarted()) ||
           (!link_is_vehicle_online_now(g_pCurrentModel->uVehicleId)) )
      {
         addMessage("You must be connected to a vehicle.");
         valuesToUI();
         return;
      }
      if ( g_pCurrentModel->is_spectator )
      {
         addMessage("Can't change log settings on a spectator vehicle.");
         valuesToUI();
         return;
      }
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_DOWNLOAD_FILE, FILE_ID_VEHICLE_LOGS_ARCHIVE, NULL, 0) )
         valuesToUI();
      else
         menu_discard_all();
   }

   if ( m_IndexLogLevelVehicle == m_SelectedIndex )
   {
      if ( (NULL == g_pCurrentModel) ||
           (!pairing_isStarted()) ||
           (!link_is_vehicle_online_now(g_pCurrentModel->uVehicleId)) )
      {
         addMessage("You must be connected to a vehicle.");
         valuesToUI();
         return;
      }
      if ( g_pCurrentModel->is_spectator )
      {
         addMessage("Can't change log settings on a spectator vehicle.");
         return;
      }
      int iLogNow = 0;
      if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS )
         iLogNow = 1;
      int log = m_pItemsSelect[6]->getSelectedIndex();
      if ( 0 == log )
         g_pCurrentModel->uDeveloperFlags &= (~DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS);
      else
         g_pCurrentModel->uDeveloperFlags |= DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS;

      if ( ! handle_commands_send_developer_flags(g_pCurrentModel->uDeveloperFlags) )
         valuesToUI();
      else
      {
         if ( iLogNow != log )
            addMessage("You need to restart your vehicle for the changes to take effect.");
      }
   }

   if ( m_IndexLogLevelController == m_SelectedIndex )
   {
      int nLogNow = pP->nLogLevel;
      pP->nLogLevel = m_pItemsSelect[7]->getSelectedIndex();
      save_Preferences();
      valuesToUI();

      if ( nLogNow != pP->nLogLevel )
         addMessage("You need to restart your controller for the changes to take effect.");
   }

   if ( m_IndexZipAllLogs == m_SelectedIndex )
   {
      MenuConfirmation* pMC = new MenuConfirmation("Export all logs","Insert an USB memory stick and press [Ok] to export the logs to the memory stick.",1, true);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      return;
   }

   if ( m_IndexClearControllerLogs == m_SelectedIndex )
   {
      MenuConfirmation* pMC = new MenuConfirmation("Clear controller logs", "Are you sure you want to delete all the controller logs?",2);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      return;
   }

   if ( m_IndexClearVehicleLogs == m_SelectedIndex )
   {
      if ( (NULL == g_pCurrentModel) ||
           (!pairing_isStarted()) ||
           (!link_is_vehicle_online_now(g_pCurrentModel->uVehicleId)) )
      {
         addMessage("You must be connected to a vehicle.");
         valuesToUI();
         return;
      }
      if ( g_pCurrentModel->is_spectator )
      {
         addMessage("Can't change log settings on a spectator vehicle.");
         valuesToUI();
         return;
      }

      MenuConfirmation* pMC = new MenuConfirmation("Clear vehicle logs", "Are you sure you want to delete all the vehicle logs?",3);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      return;
   }
}

