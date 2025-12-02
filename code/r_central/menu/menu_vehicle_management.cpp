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

#include "../osd/osd_common.h"
#include "menu.h"
#include "menu_vehicle_management.h"
#include "menu_vehicle_general.h"
#include "menu_vehicle_expert.h"
#include "menu_confirmation.h"
#include "menu_vehicle_import.h"
#include "menu_vehicle_management_plugins.h"

#include <ctype.h>
#include "../link_watch.h"
#include "../../base/hardware_files.h"

MenuVehicleManagement::MenuVehicleManagement(void)
:Menu(MENU_ID_VEHICLE_MANAGEMENT, L("Vehicle Management"), NULL)
{
   m_Width = 0.17;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.18;
   m_fIconSize = 1.0;
   m_bWaitingForVehicleInfo = false;
}

MenuVehicleManagement::~MenuVehicleManagement()
{
}

void MenuVehicleManagement::onShow()
{
   m_Height = 0.0;
   removeAllItems();

   m_IndexHWInfo = addMenuItem(new MenuItem(L("Get Hardware Info"), L("Gets the hardware capabilities of the vehicle.")));
   m_IndexConfig = addMenuItem(new MenuItem(L("Get Config Info"), L("Gets the current configuration of the vehicle.")));
   m_IndexModules = addMenuItem(new MenuItem(L("Get Modules Info"), L("Gets the current detected and loaded modules on the vehicle.")));
   m_IndexPlugins = addMenuItem(new MenuItem(L("Core Plugins"), L("Manage the core plugins on this vehicle.")));
   m_IndexExport = addMenuItem(new MenuItem(L("Export Model Settings"),L("Exports the model settings to a USB stick.")));
   m_IndexImport = addMenuItem(new MenuItem(L("Import Model Settings"), L("Imports the model settings from a USB stick.")));
   m_IndexUpdate = addMenuItem(new MenuItem(L("Update Software"), L("Updates the software on the vehicle.")));
   m_IndexReset  = addMenuItem(new MenuItem(L("Reset to defaults"), L("Resets all parameters for this vehicle to the default configuration (except for frequency, vehicle ID and name).")));
   m_IndexFactoryReset = addMenuItem(new MenuItem(L("Factory Reset"), L("Resets the vehicle as it comes after a fresh install. All settings (including vehicle name, frequency, etc) will be reset to default values.")));
   m_IndexReboot = addMenuItem(new MenuItem(L("Restart"), L("Restarts the vehicle.")));
   m_IndexDelete = addMenuItem(new MenuItem(L("Delete"), L("Delete this vehicle from your control list.")));

   bool bConnected = false;
   if ( g_bIsRouterReady && link_has_received_main_vehicle_ruby_telemetry() )
      bConnected = true;

   if ( g_pCurrentModel->b_mustSyncFromVehicle )
      bConnected = false;

   if ( ! bConnected )
   {
      m_pMenuItems[m_IndexHWInfo]->setEnabled(false);
      m_pMenuItems[m_IndexConfig]->setEnabled(false);
      m_pMenuItems[m_IndexModules]->setEnabled(false);
      m_pMenuItems[m_IndexImport]->setEnabled(false);
      m_pMenuItems[m_IndexUpdate]->setEnabled(false);
      m_pMenuItems[m_IndexReset]->setEnabled(false);
      m_pMenuItems[m_IndexReboot]->setEnabled(false);
   }
   
   Menu::onShow();
}

void MenuVehicleManagement::Render()
{
   RenderPrepare();

   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

bool MenuVehicleManagement::periodicLoop()
{
   if ( ! m_bWaitingForVehicleInfo )
      return false;

   if ( ! handle_commands_is_command_in_progress() )
   {
      if ( handle_commands_has_received_vehicle_core_plugins_info() )
      {
         add_menu_to_stack(new MenuVehicleManagePlugins());
         m_bWaitingForVehicleInfo = false;
         return true;
      }
   }
   return false;
}
     
void MenuVehicleManagement::onReturnFromChild(int iChildMenuId, int returnValue)
{
   log_line("MenuVehicleManagement: on return from child id %d, result: %d", iChildMenuId, returnValue);

   Menu::onReturnFromChild(iChildMenuId, returnValue);

   // Delete model
   if ( (1 == iChildMenuId/1000) && (1 == returnValue) )
   {
      if ( NULL != g_pCurrentModel )
         pairing_stop();
      menu_discard_all();
      u32 uVehicleId = g_pCurrentModel->uVehicleId;
      deleteModel(g_pCurrentModel);
      g_pCurrentModel = NULL;
      notification_add_model_deleted();
      onModelDeleted(uVehicleId);
      return;
   }

   // Upload software
   if ( (1 == returnValue) && ( (2 == iChildMenuId/1000) || (4 == iChildMenuId/1000)) )
   {
      if ( uploadSoftware() )
      {
         onEventPairingDiscardAllUIActions();
         
         char szTextW[256];
         sprintf(szTextW, L("Your %s was updated. It will reboot now."), g_pCurrentModel->getVehicleTypeString());
         MenuConfirmation* pm = new MenuConfirmation( L("Upload Succeeded"), szTextW, 3, true);
         pm->m_xPos = 0.4; pm->m_yPos = 0.4;
         pm->m_Width = 0.36;
         pm->m_bDisableStacking = true;
         pm->disablePairingUIActions();
         pm->setTimeoutMs(6000);
         add_menu_to_stack(pm);
      }
      return;
   }

   if ( 3 == iChildMenuId/1000 )
   {
      menu_discard_all();
      g_bSyncModelSettingsOnLinkRecover = true;
      return;
   }

   if ( (10 == iChildMenuId/1000) && (1 == returnValue) )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_REBOOT, 0, NULL, 0) )
         valuesToUI();
      else
         menu_discard_all();
      return;
   }

   // Reset to default
   if ( (20 == iChildMenuId/1000) && (1 == returnValue) )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_RESET_ALL_TO_DEFAULTS, 0, NULL, 0) )
         valuesToUI();
      else
      {
         g_bSyncModelSettingsOnLinkRecover = true;
         //Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE+3*1000,"Reset Complete",NULL);
         //pm->m_xPos = 0.4; pm->m_yPos = 0.4;
         //pm->m_Width = 0.36;
         //pm->addTopLine("Your vehicle was reseted to default settings. It will reboot now.");
         MenuConfirmation* pM = new MenuConfirmation( L("Reset Complete"), L("Your vehicle was reseted to default settings. It will reboot now."), 3, true);
         add_menu_to_stack(pM);
      }
      return;
   }

   // Factory reset
   if ( (21 == iChildMenuId/1000) && (1 == returnValue) )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_FACTORY_RESET, 0, NULL, 0) )
         valuesToUI();
      else
      {
         g_bSyncModelSettingsOnLinkRecover = true;
      }
      return;
   }
}

void MenuVehicleManagement::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( NULL == g_pCurrentModel )
   {
      Popup* p = new Popup( L("Vehicle is offline"), 0.3, 0.3, 0.5, 4 );
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      valuesToUI();
      return;
   }

   if ( g_pCurrentModel->is_spectator )
   {
      Popup* p = new Popup( L("Vehicle settings can not be changed on a spectator vehicle."), 0.3, 0.3, 0.5, 4 );
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      valuesToUI();
      return;
   }

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_IndexHWInfo == m_SelectedIndex )
   {
      handle_commands_send_to_vehicle(COMMAND_ID_GET_MODULES_INFO, 1, NULL, 0);
   }

   if ( m_IndexConfig == m_SelectedIndex )
   {
      handle_commands_send_to_vehicle(COMMAND_ID_GET_CURRENT_VIDEO_CONFIG, 0, NULL, 0);
   }

   if ( m_IndexModules == m_SelectedIndex )
   {
      handle_commands_send_to_vehicle(COMMAND_ID_GET_MODULES_INFO, 0, NULL, 0);
   }

   if ( m_IndexPlugins == m_SelectedIndex )
   {
      if ( hardware_board_is_openipc(g_pCurrentModel->hwCapabilities.uBoardType) )
      {
         addUnsupportedMessageOpenIPC(NULL);
         return;
      }
      if ( handle_commands_has_received_vehicle_core_plugins_info() )
      {
         add_menu_to_stack(new MenuVehicleManagePlugins());
         return;
      }

      handle_commands_reset_has_received_vehicle_core_plugins_info();
      m_bWaitingForVehicleInfo = false;

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_GET_CORE_PLUGINS_INFO, 0, NULL, 0) )
         valuesToUI();
      else
         m_bWaitingForVehicleInfo = true;
      return;
   }

   if ( m_IndexExport == m_SelectedIndex )
   {
      int iMountRes = hardware_try_mount_usb();
      if ( 1 != iMountRes )
      {
         log_line("No USB memory stick available.");
         Popup* p = new Popup(L("Please insert a USB memory stick."), 0.28, 0.32, 0.32, 3);
         if ( 0 != iMountRes )
            p->setTitle(L("USB memory stick invalid. Please try again."));
         p->setCentered();
         p->setIconId(g_idIconInfo, get_Color_IconWarning());
         popups_add_topmost(p);
         return;
      }
      char szFile[256];
      char szModelName[256];

      strcpy(szModelName, g_pCurrentModel->getLongName());
      str_sanitize_filename(szModelName);

      snprintf(szFile, sizeof(szFile)/sizeof(szFile[0]), "%s/ruby_model_%s_%u.txt", FOLDER_USB_MOUNT, szModelName, g_pCurrentModel->uVehicleId);
      g_pCurrentModel->saveToFile(szFile, false);
   
      hardware_unmount_usb();
      ruby_signal_alive();
      sync();
      ruby_signal_alive();
      Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE+10*1000, L("Export Succeeded"), NULL);
      pm->m_xPos = 0.4; pm->m_yPos = 0.4;
      pm->m_Width = 0.36;
      pm->addTopLine(L("Your vehicle settings have been stored to the USB stick. You can now remove the USB stick."));
      add_menu_to_stack(pm);
      return;
   }

   if ( m_IndexImport == m_SelectedIndex )
   {
      if ( checkIsArmed() )
         return;
      int iMountRes = hardware_try_mount_usb();
      if ( 1 != iMountRes )
      {
         log_line("No USB memory stick available.");
         Popup* p = new Popup(L("Please insert a USB memory stick."), 0.28, 0.32, 0.32, 3);
         if ( 0 != iMountRes )
            p->setTitle(L("USB memory stick invalid. Please try again."));
         p->setCentered();
         p->setIconId(g_idIconInfo, get_Color_IconWarning());
         popups_add_topmost(p);
         return;
      }

      MenuVehicleImport* pM = new MenuVehicleImport();
      pM->buildSettingsFileList();
      if ( 0 == pM->getSettingsFilesCount() )
      {
         hardware_unmount_usb();
         ruby_signal_alive();
         sync();
         ruby_signal_alive();
         delete pM;
         Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE+10*1000,"No settings files",NULL);
         pm->m_xPos = 0.4; pm->m_yPos = 0.4;
         pm->m_Width = 0.36;
         pm->addTopLine(L("There are no vehicle settings files on the USB stick."));
         add_menu_to_stack(pm);
         return;
      }
      add_menu_to_stack(pM);
   }

   if ( m_IndexUpdate == m_SelectedIndex )
   {
      bool bSupportsOTA = false;
      char szTextW[256];
      if ( hardware_board_is_raspberry(g_pCurrentModel->hwCapabilities.uBoardType) )
         bSupportsOTA = true;
      if ( g_pCurrentModel->hwCapabilities.uHWFlags & MODEL_HW_CAP_FLAG_OTA )
         bSupportsOTA = true;

      if ( ! bSupportsOTA )
      {
         addMessageWithTitle(0, L("Can't update"), L("This vehicle does not support OTA (over the air) updates."));
         return;
      }
      
      if ( checkIsArmed() )
         return;
      if ( (! pairing_isStarted()) || (NULL == g_pCurrentModel) || (! link_is_vehicle_online_now(g_pCurrentModel->uVehicleId)) )
      {
         sprintf(szTextW, "Please connect to your %s first, if you want to update the software on the vehicle.", g_pCurrentModel->getVehicleTypeString());
         addMessage(szTextW);
         return;
      }

      char szBuff2[64];
      getSystemVersionString(szBuff2, g_pCurrentModel->sw_version);

      if ( is_sw_version_latest(g_pCurrentModel) )
      {
         char szBuff[256];
         sprintf(szBuff, "Your %s already has the latest version of the software (version %s). Do you still want to upgrade?", g_pCurrentModel->getVehicleTypeString(), szBuff2);
         MenuConfirmation* pMC = new MenuConfirmation( L("Upgrade Confirmation"), szBuff, 4);
         add_menu_to_stack(pMC);
         //pMC->addTopLine(" ");
         //pMC->addTopLine("Note: Do not keep the vehicle very close to the controller as the radio power might be too powerfull and generate noise.");
         return;
      }
      char szBuff[512];
      char szBuff3[64];
      getSystemVersionString(szBuff3, (SYSTEM_SW_VERSION_MAJOR<<8) | SYSTEM_SW_VERSION_MINOR);
      sprintf(szBuff, "Your %s has software version %s (b-%d) and software version %s (b-%d) is available on the controller. Do you want to upgrade?", g_pCurrentModel->getVehicleTypeString(), szBuff2, (int)get_sw_version_build(g_pCurrentModel), szBuff3, SYSTEM_SW_BUILD_NUMBER);
      MenuConfirmation* pMC = new MenuConfirmation( L("Upgrade Confirmation"), szBuff, 2);
      add_menu_to_stack(pMC);
   }
        
   if ( m_IndexReset == m_SelectedIndex )
   {
      if ( checkIsArmed() )
         return;
      char szBuff[256];
      sprintf(szBuff, "Are you sure you want to reset all parameters for %s?", g_pCurrentModel->getLongName());
      MenuConfirmation* pMC = new MenuConfirmation(L("Confirmation"), szBuff, 20);
      add_menu_to_stack(pMC);
   }
 
   if ( m_IndexFactoryReset == m_SelectedIndex )
   {
      if ( checkIsArmed() )
         return;
      char szBuff[256];
      sprintf(szBuff, "Factory reset %s", g_pCurrentModel->getLongName());
      MenuConfirmation* pMC = new MenuConfirmation(L("Confirmation"), szBuff, 21);
      pMC->addTopLine(L("All parameters (including vehicle name, radio frequency, etc) and state will be reset to default values as after a fresh instalation."));
      pMC->addTopLine(L("You will need to search and pair with the vehicle again after that."));
      sprintf(szBuff, "Are you sure you want to factory reset %s?", g_pCurrentModel->getLongName());
      pMC->addTopLine(szBuff);
      add_menu_to_stack(pMC);
   }

   if ( m_IndexDelete == m_SelectedIndex )
   {
      if ( checkIsArmed() )
         return;
      char szBuff[64];
      sprintf(szBuff, "Are you sure you want to delete %s?", g_pCurrentModel->getLongName());
      add_menu_to_stack(new MenuConfirmation(L("Confirmation"), szBuff, 1));
   }

   if ( m_IndexReboot == m_SelectedIndex )
   {
      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotFCTelemetry )
      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.uFCFlags & FC_TELE_FLAGS_ARMED )
      {
         char szTextW[256];
         sprintf(szTextW, "Your %s is armed. Are you sure you want to reboot it?", g_pCurrentModel->getVehicleTypeString());
         MenuConfirmation* pMC = new MenuConfirmation(L("Warning! Reboot Confirmation"), szTextW, 10);
         if ( g_pCurrentModel->rc_params.rc_enabled )
         {
            pMC->addTopLine(" ");
            pMC->addTopLine(L("Warning: You have the RC link enabled, the vehicle flight controller will go into RC failsafe mode during reboot."));
         }
         add_menu_to_stack(pMC);
         return;
      }
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_REBOOT, 0, NULL, 0) )
         valuesToUI();
      else
         menu_discard_all();
   }
}

