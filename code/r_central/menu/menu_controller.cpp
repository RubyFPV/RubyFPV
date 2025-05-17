/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga petrusoroaga@yahoo.com
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
#include "menu_item_section.h"
#include "menu_confirmation.h"
#include "menu_controller_expert.h"
#include "menu_controller_peripherals.h"
#include "menu_controller_video.h"
#include "menu_controller_telemetry.h"
#include "menu_controller_network.h"
#include "menu_controller_plugins.h"
#include "menu_controller_encryption.h"
#include "menu_controller_recording.h"
#include "menu_controller_dev.h"
#include "menu_preferences_buttons.h"
#include "menu_preferences_ui.h"
#include "menu_preferences.h"
#include "menu_controller_radio.h"
#include "menu_controller_update.h"
#include "../../base/ctrl_settings.h"

#include <time.h>
#include <sys/resource.h>

MenuController::MenuController(void)
:Menu(MENU_ID_CONTROLLER, L("Controller Settings"), NULL)
{
   m_Width = 0.18;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.18;

   m_bShownHDMIChangeNotif = false;
}

void MenuController::onShow()
{
   int iTmp = getSelectedMenuItemIndex();

   addItems();
   Menu::onShow();

   if ( iTmp > 0 )
   {
      m_SelectedIndex = iTmp;
      if ( m_SelectedIndex >= m_ItemsCount )
         m_SelectedIndex = m_ItemsCount-1;
      onFocusedItemChanged();
   }
}

void MenuController::addItems()
{
   removeAllItems();
   removeAllTopLines();
   
   setTitle(L("Controller Settings"));
   m_IndexVideo = addMenuItem(new MenuItem(L("Audio & Video Output"), L("Change Audio and Video Output Settings (HDMI, USB Tethering, Audio output device)")));
   //m_pMenuItems[m_IndexVideo]->showArrow();

   m_IndexTelemetry = addMenuItem(new MenuItem(L("Telemetry Input/Output"), L("Change the Telemetry Input/Output settings on the controller ports.")));
   //m_pMenuItems[m_IndexTelemetry]->showArrow();

   m_IndexRadio = addMenuItem(new MenuItem(L("Radio"), L("Configure the radio interfaces on the controller.")));
   //m_pMenuItems[m_IndexTelemetry]->showArrow();

   m_IndexCPU = addMenuItem(new MenuItem(L("CPU and Processes"), L("Set CPU Overclocking, Processes Priorities")));
   //m_pMenuItems[m_IndexCPU]->showArrow();

   m_IndexPorts = addMenuItem(new MenuItem(L("Peripherals & Ports"), L("Change controller peripherals settings (serial ports, USB devices, HID, I2C devices, etc)")));
   //m_pMenuItems[m_IndexPorts]->showArrow();

   //m_pItemsSelect[1] = new MenuItemSelect("Show CPU Info in OSD", "Shows Controller CPU information (load, frequency, temperature) on the OSD, near the vehicle CPU info, when using OSD Full Layout.");
   //m_pItemsSelect[1]->addSelection("No");
   //m_pItemsSelect[1]->addSelection("Yes");
   //m_pItemsSelect[1]->setUseMultiViewLayout();
   //m_IndexShowCPUInfo = addMenuItem(m_pItemsSelect[1]);

   //m_pItemsSelect[2] = new MenuItemSelect("Show Voltage/Current in OSD", "Shows Controller voltage/current information on the OSD, near the vehicle CPU info, when using OSD Full Layout.");
   //m_pItemsSelect[2]->addSelection("No");
   //m_pItemsSelect[2]->addSelection("Yes");
   //m_pItemsSelect[2]->setUseMultiViewLayout();
   //m_IndexShowVoltage = addMenuItem(m_pItemsSelect[2]);
   
   m_IndexNetwork = addMenuItem(new MenuItem(L("Local Network Settings"), L("Change the local network settings on the controller (DHCP/Fixed IP)")));
   //m_pMenuItems[m_IndexNetwork]->showArrow();

   m_IndexEncryption = -1;
   //m_IndexEncryption = addMenuItem(new MenuItem("Encryption", "Change the encryption global settings"));
   //m_pMenuItems[m_IndexEncryption]->showArrow();
   //m_pMenuItems[m_IndexEncryption]->setEnabled(false);

   m_IndexButtons = addMenuItem(new MenuItem(L("Buttons"), L("Change buttons actions.")));
   m_IndexPreferences = -1;
   //m_IndexPreferences = addMenuItem(new MenuItem("Preferences", "Change preferences about messages."));
   m_IndexPreferencesUI = addMenuItem(new MenuItem(L("User Interface"), L("Change user interface preferences: language, fonts, colors, sizes, display units.")));

   m_IndexRecording = addMenuItem(new MenuItem(L("Recording Settings"), L("Change the recording settings")));
   //m_pMenuItems[m_IndexRecording]->showArrow();

   ControllerSettings* pCS = get_ControllerSettings();
   m_IndexDeveloper = -1;
   if ( (NULL != pCS) && pCS->iDeveloperMode )
   {
      m_IndexDeveloper = addMenuItem( new MenuItem(L("Developer Settings")) );
      m_pMenuItems[m_IndexDeveloper]->showArrow();
      m_pMenuItems[m_IndexDeveloper]->setTextColor(get_Color_Dev());
   }

   addMenuItem(new MenuItemSection(L("Management")));

   m_IndexPlugins = addMenuItem(new MenuItem(L("Manage Plugins"), L("Configure, add and remove controller software plugins.")));
   m_IndexUpdate = addMenuItem(new MenuItem(L("Update Software"), L("Updates software on this controller using a USB memory stick.")));
   m_IndexReboot = addMenuItem(new MenuItem(L("Restart Controller"), L("Restarts the controller.")));
}

void MenuController::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuController::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   log_line("MenuController: Returned from child id: %d, return value: %d", iChildMenuId, returnValue);
   
   if ( (10 == iChildMenuId/1000) && (1 == returnValue) )
   {
      onEventReboot();
      hardware_reboot();
      return;
   }

   if ( (11 == iChildMenuId/1000) && (1 == returnValue) )
   {
      onEventReboot();
      hardware_reboot();
      return;
   }

   log_line("Add HDMI changed confirmation dialog.");
   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_TEMP_HDMI_CHANGED);
   if ( ! m_bShownHDMIChangeNotif )
   if ( access(szFile, R_OK) != -1 )
   {
      m_bShownHDMIChangeNotif = true;
      MenuConfirmation* pMC = new MenuConfirmation("HDMI Output Configuration Changed","You updated the HDMI output configuration.", 11);
      pMC->addTopLine("If the new resolution is not supported by your display, Ruby will reboot again with the old display configuration.");
      pMC->addTopLine("Do you want to reboot now for the new changes to take effect?");
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      return;
   }
}

void MenuController::onSelectItem()
{
   Menu::onSelectItem();

   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   ControllerSettings* pCS = get_ControllerSettings();
   if ( NULL == pCS )
   {
      log_softerror_and_alarm("Failed to get pointer to controller settings structure");
      return;
   }

   Preferences* pp = get_Preferences();
   if ( NULL == pp )
   {
      log_softerror_and_alarm("Failed to get pointer to preferences structure");
      return;
   }

   if ( m_IndexNetwork == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuControllerNetwork());
      return;
   }
  
   if ( m_IndexCPU == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuControllerExpert());
      return;
   }

   if ( m_IndexPorts == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuControllerPeripherals());
      return;
   }

   if ( m_IndexVideo == m_SelectedIndex )
   {
      m_bShownHDMIChangeNotif = false;
      add_menu_to_stack(new MenuControllerVideo());
      return;
   }

   if ( m_IndexTelemetry == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuControllerTelemetry());
      return;
   }

   if ( m_IndexRadio == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuControllerRadio());
      return;
   }

   if ( m_IndexRecording == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuControllerRecording());
      return;
   }

   if ( m_IndexPlugins == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuControllerPlugins());
      return;
   }

   if ( (-1 != m_IndexEncryption) && (m_IndexEncryption == m_SelectedIndex) )
   {
      add_menu_to_stack(new MenuControllerEncryption());
      return;
   }

   if ( m_IndexButtons == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuButtons()); 
      return;
   }

   /*
   if ( m_IndexPreferences == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuPreferences()); 
      return;    
   }
   */
   if ( m_IndexPreferencesUI == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuPreferencesUI()); 
      return;
   }

   if ( m_IndexUpdate == m_SelectedIndex )
   {
      if ( checkIsArmed() )
         return;

      add_menu_to_stack(new MenuControllerUpdate());
      return;
   }

   if ( m_IndexReboot == m_SelectedIndex )
   {
      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotFCTelemetry )
      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.uFCFlags & FC_TELE_FLAGS_ARMED )
      {
         char szText[256];
         if ( NULL != g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].pModel )
            sprintf(szText, "Your %s is armed. Are you sure you want to reboot the controller?", g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].pModel->getVehicleTypeString());
         else
            strcpy(szText, "Your vehicle is armed. Are you sure you want to reboot the controller?");
         MenuConfirmation* pMC = new MenuConfirmation(L("Warning! Reboot Confirmation"), szText, 10);
         if ( g_pCurrentModel->rc_params.rc_enabled )
         {
            pMC->addTopLine(" ");
            pMC->addTopLine(L("Warning: You have the RC link enabled, the vehicle flight controller might not go into failsafe mode during reboot."));
         }
         add_menu_to_stack(pMC);
         return;
      }
      onEventReboot();
      hardware_reboot();
   }

   if ( (m_IndexDeveloper != -1) && (m_IndexDeveloper == m_SelectedIndex) )
   {
      add_menu_to_stack(new MenuControllerDev());
      return;
   }
}
