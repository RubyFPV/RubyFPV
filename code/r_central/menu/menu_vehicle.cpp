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
#include "menu_vehicle.h"
#include "menu_vehicle_general.h"
#include "menu_vehicle_video.h"
#include "menu_vehicle_camera.h"
#include "menu_vehicle_osd.h"
#include "menu_vehicle_osd_stats.h"
#include "menu_vehicle_osd_instruments.h"
#include "menu_vehicle_expert.h"
#include "menu_confirmation.h"
#include "menu_vehicle_telemetry.h"
#include "menu_vehicle_rc.h"
#include "menu_vehicle_relay.h"
#include "menu_vehicle_management.h"
#include "menu_vehicle_audio.h"
#include "menu_vehicle_data_link.h"
#include "menu_vehicle_functions.h"
#include "menu_vehicle_alarms.h"
#include "menu_vehicle_peripherals.h"
#include "menu_vehicle_radio.h"
#include "menu_vehicle_cpu_oipc.h"
#include "menu_vehicle_dev.h"
#include "menu_item_text.h"
#include "../link_watch.h"

#include <ctype.h>

const char* s_szMenuVAlarmTemp = "Hardware alarm: temperature limit!";
const char* s_szMenuVAlarmThr = "Hardware alarm: Throttling!";
const char* s_szMenuVAlarmFreq = "Hardware alarm: Frequency capped!";
const char* s_szMenuVAlarmVoltage = "Hardware alarm: Undervoltage!";

MenuVehicle::MenuVehicle(void)
:Menu(MENU_ID_VEHICLE, L("Vehicle Settings"), NULL)
{
   m_Width = 0.18;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.16;
   m_fIconSize = 1.0;
   m_bWaitingForVehicleInfo = false;
}

MenuVehicle::~MenuVehicle()
{
}

void MenuVehicle::onShow()
{
   int iTmp = getSelectedMenuItemIndex();

   m_Height = 0.0;

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);

   addExtraHeightAtStart(4.0*height_text);
   addTopDescription();
   removeAllItems();

   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      addMenuItem(new MenuItemText(L("You are connected in spectator mode. Can't change vehicle settings.")));

   m_IndexGeneral = addMenuItem(new MenuItem(L("General"), L("Change general settings like name, type of vehicle and so on.")));
   //if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
   //   m_pMenuItems[m_IndexGeneral]->setEnabled(false);

   m_IndexRadio = addMenuItem(new MenuItem(L("Radio Links"), L("Change the radio links configuration of this vehicle.")));
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexRadio]->setEnabled(false);

   m_IndexAlarms = addMenuItem(new MenuItem(L("Warnings/Alarms"), L("Change which alarms and warnings to enable/disable.")));
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexAlarms]->setEnabled(false);

   m_IndexOSD = addMenuItem(new MenuItem(L("OSD / Instruments"), L("Change OSD type, layout and settings.")));

   m_IndexCamera = addMenuItem(new MenuItem(L("Camera"), L("Change camera parameters: brightness, contrast, sharpness and so on.")));

   //if ( NULL != g_pCurrentModel && (!g_pCurrentModel->hasCamera()) )
   //   m_pMenuItems[m_IndexCamera]->setEnabled(false);
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexCamera]->setEnabled(false);

   m_IndexVideo = addMenuItem(new MenuItem(L("Video"), L("Change video resolution, fps so on.")));
   
   //if ( NULL != g_pCurrentModel && (!g_pCurrentModel->hasCamera()) )
   //   m_pMenuItems[m_IndexVideo]->setEnabled(false);
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexVideo]->setEnabled(false);

   m_IndexAudio = addMenuItem(new MenuItem(L("Audio"), L("Change the audio settings")));
   if ( (NULL != g_pCurrentModel) && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexAudio]->setEnabled(false);

   m_IndexTelemetry = addMenuItem(new MenuItem(L("Telemetry"), L("Change telemetry parameters between flight controller and ruby vehicle.")));
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexTelemetry]->setEnabled(false);

   m_IndexDataLink = addMenuItem(new MenuItem(L("Auxiliary Data Link"), L("Create and configure a general purpose data link between the vehicle and the controller.")));
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexDataLink]->setEnabled(false);
 
   m_IndexRC = addMenuItem(new MenuItem(L("Remote Control"), L("Change your remote control type, channels, protocols and so on."))); 
   #ifndef FEATURE_ENABLE_RC
   m_pMenuItems[m_IndexRC]->setEnabled(false);
   #endif
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexRC]->setEnabled(false);

   m_IndexFunctions = -1;
   //m_IndexFunctions = addMenuItem(new MenuItem(L("Functions and Triggers"), L("Configure special functions and triggers.")));
   //if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
   //   m_pMenuItems[m_IndexFunctions]->setEnabled(false);

   /*
   if ( NULL != g_pCurrentModel && g_pCurrentModel->radioInterfacesParams.interfaces_count > 1 )
      index = addMenuItem(new MenuItem("Radio Links", "Change the function, frequencies, data rates and parameters of the radio links."));
   else
      index = addMenuItem(new MenuItem("Radio Link", "Change the frequency, data rate and parameters of the radio link."));
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[index]->setEnabled(false);
   */

   m_IndexRelay = addMenuItem(new MenuItem(L("Relaying"), L("Configure this vehicle as a relay.")));
   #ifdef FEATURE_RELAYING
   m_pMenuItems[m_IndexRelay]->setEnabled(true);
   #else
   m_pMenuItems[m_IndexRelay]->setEnabled(false);
   #endif
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexRelay]->setEnabled(false);

   m_IndexPeripherals = addMenuItem(new MenuItem(L("Peripherals"), L("Change vehicle's serial ports settings and other peripherals.")));
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexPeripherals]->setEnabled(false);

   m_IndexCPU = addMenuItem(new MenuItem(L("CPU Settings"), L("Change CPU overclocking settings and processes priorities.")));
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexCPU]->setEnabled(false);

   m_IndexManagement = addMenuItem(new MenuItem(L("Management"), L("Updates, deletes, back-up, restore the vehicle data and firmware.")));
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexManagement]->setEnabled(false);

   ControllerSettings* pCS = get_ControllerSettings();
   m_IndexDeveloper = -1;
   if ( (NULL != g_pCurrentModel) && (! g_pCurrentModel->is_spectator) && pCS->iDeveloperMode )
   {
      m_IndexDeveloper = addMenuItem( new MenuItem("Developer Settings") );
      m_pMenuItems[m_IndexDeveloper]->showArrow();
      m_pMenuItems[m_IndexDeveloper]->setTextColor(get_Color_Dev());
   }
   Menu::onShow();

   m_SelectedIndex = iTmp;
   if ( m_SelectedIndex < 0 )
      m_SelectedIndex = 0;
   if ( m_SelectedIndex >= m_ItemsCount )
      m_SelectedIndex = m_ItemsCount-1;
   onFocusedItemChanged();
}

void MenuVehicle::addTopDescription()
{
   removeAllTopLines();
   char szBuff[256];
   //sprintf(szBuff, "%s Settings", g_pCurrentModel->getLongName());
   strcpy(szBuff, L("Vehicle Settings"));
   szBuff[0] = toupper(szBuff[0]);
   setTitle(szBuff);
   //setSubTitle("Vehicle Settings");

   m_fIconSize = 2.6 * m_sfMenuPaddingY;

   m_Flags = 0xFFFF;

   return;

   if ( NULL != g_pCurrentModel && (g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotRubyTelemetryInfo) )
      m_Flags = g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.throttled;

   if ( 0xFFFF == m_Flags || 0 == m_Flags )
      addTopLine(L("No hardware alarms."));
   else
   {
      addTopLine("Has hardware alarms:");
      if ( m_Flags & 0b1000 )
         addTopLine(s_szMenuVAlarmTemp);
      if ( m_Flags & 0b0100 )
         addTopLine(s_szMenuVAlarmThr);
      if ( m_Flags & 0b0010 )
         addTopLine(s_szMenuVAlarmFreq);
      if ( m_Flags & 0b0001 )
         addTopLine(s_szMenuVAlarmVoltage);
   }
}

void MenuVehicle::Render()
{
   RenderPrepare();
   if ( m_bInvalidated )
      addTopDescription();

   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float iconHeight = 2.2*height_text;
   float iconWidth = iconHeight/g_pRenderEngine->getAspectRatio();

   float xPos = m_RenderXPos + m_sfMenuPaddingX;
   float yPos = m_RenderYPos + m_RenderTitleHeight + 2.0*m_sfMenuPaddingY;
  
   u32 idIcon = osd_getVehicleIcon( g_pCurrentModel->vehicle_type );

   /*
   bool bConnected = false;
   if ( link_has_received_main_vehicle_ruby_telemetry() )
      bConnected = true;
   if ( g_bFirstModelPairingDone && (0 == getControllerModelsCount()) && (0 == getControllerModelsSpectatorCount()) )
      bConnected = false;
   if ( NULL == g_pCurrentModel )
      bConnected = false;
   if ( 0 == g_uActiveControllerModelVID )
      bConnected = false;
   */
   
   char szLine1[128];
   char szLine2[128];
   char szLine3[128];
   szLine1[0] = 0;
   szLine2[0] = 0;
   szLine3[0] = 0;
   if ( (NULL != g_pCurrentModel) && link_is_vehicle_online_now(g_pCurrentModel->uVehicleId) )
   {
      sprintf(szLine1, L("Connected to:"));
      sprintf(szLine2, "%s", g_pCurrentModel->getLongName() );
      sprintf(szLine3, "Running ver %d.%d", get_sw_version_major(g_pCurrentModel), get_sw_version_minor(g_pCurrentModel));
   }
   else
   {
      sprintf(szLine1, L("Looking for:") );
      sprintf(szLine2, "%s", g_pCurrentModel->getLongName() );
   }
   szLine2[0] = toupper(szLine2[0]);

   g_pRenderEngine->setColors(get_Color_MenuText(), 0.7);
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   g_pRenderEngine->drawIcon(xPos, yPos, iconWidth, iconHeight, idIcon);
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   g_pRenderEngine->drawText(xPos + iconWidth + m_sfMenuPaddingX, yPos, g_idFontMenuSmall, szLine1);
   g_pRenderEngine->drawText(xPos + iconWidth + m_sfMenuPaddingX, yPos + height_text, g_idFontMenu, szLine2);
   g_pRenderEngine->drawText(xPos, yPos + iconHeight + 0.3*m_sfMenuPaddingY, g_idFontMenu, szLine3);
}


bool MenuVehicle::periodicLoop()
{
   if ( ! m_bWaitingForVehicleInfo )
      return false;

   if ( ! handle_commands_is_command_in_progress() )
   {
      if ( handle_commands_has_received_vehicle_core_plugins_info() )
      {
         if ( m_IndexPeripherals == m_SelectedIndex )
            add_menu_to_stack(new MenuVehiclePeripherals());
         if ( m_IndexTelemetry == m_SelectedIndex )
            add_menu_to_stack(new MenuVehicleTelemetry());
         if ( m_IndexDataLink == m_SelectedIndex )
            add_menu_to_stack(new MenuVehicleDataLink());
         m_bWaitingForVehicleInfo = false;
         return true;
      }
   }
   return false;
}


void MenuVehicle::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( NULL == g_pCurrentModel )
   {
      Popup* p = new Popup(true, L("Vehicle is offline"), 4 );
      p->setIconId(g_idIconWarning, get_Color_IconWarning());
      popups_add_topmost(p);
      valuesToUI();
      return;
   }

   if ( g_pCurrentModel->is_spectator )
   {
      if ( (m_IndexOSD != m_SelectedIndex) && (m_IndexGeneral != m_SelectedIndex) )
      {
         Popup* p = new Popup(true, L("Vehicle Settings can not be changed on a spectator vehicle."), 5 );
         p->setIconId(g_idIconError, get_Color_IconError());
         popups_add_topmost(p);
         valuesToUI();
         return;
      }
      if ( m_IndexGeneral == m_SelectedIndex )
         add_menu_to_stack(new MenuVehicleGeneral());
      if ( m_IndexOSD == m_SelectedIndex )
      {
         add_menu_to_stack(new MenuVehicleOSD());
      }
      return;
   }

   if ( m_IndexManagement == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleManagement());
      return;
   }

   if ( m_IndexPeripherals == m_SelectedIndex )
   {
      if ( handle_commands_has_received_vehicle_core_plugins_info() )
      {
         add_menu_to_stack(new MenuVehiclePeripherals());
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

   if ( (!pairing_isStarted()) || (NULL == g_pCurrentModel) || (!link_is_vehicle_online_now(g_pCurrentModel->uVehicleId)) )
   {
      Popup* p = new Popup(true, L("Can't change settings when not connected to the vehicle."), 5 );
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      valuesToUI();
      return;
   }

   if ( g_pCurrentModel->b_mustSyncFromVehicle && ( m_IndexManagement != m_SelectedIndex) )
   {
      Popup* p = new Popup(true, L("Vehicle Settings not synched yet"), 4 );
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      valuesToUI();
      return;
   }

   if ( m_IndexRadio == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleRadioConfig());
      return;
   }

   if ( m_IndexGeneral == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleGeneral());
      return;
   }
   if ( m_IndexCamera == m_SelectedIndex )
   {
      if ( (NULL == g_pCurrentModel) || (g_pCurrentModel->iCameraCount <=0) )
         addMessage(L("This vehicle has no cameras."));
      else if ( (NULL != g_pCurrentModel) && g_pCurrentModel->is_spectator )
         addMessage(L("You can't change camera settings for a spectator vehicle."));
      else
         add_menu_to_stack(new MenuVehicleCamera());
      return;
   }
   if ( m_IndexVideo == m_SelectedIndex )
   {
      if ( (NULL == g_pCurrentModel) || (g_pCurrentModel->iCameraCount <=0) )
         addMessage(L("This vehicle has no cameras or video streams."));
      else if ( (NULL != g_pCurrentModel) && g_pCurrentModel->is_spectator )
         addMessage(L("You can't change video settings for a spectator vehicle."));
      else
         add_menu_to_stack(new MenuVehicleVideo());
   }
   if ( m_IndexAudio == m_SelectedIndex )
   {

      if ( ! g_pCurrentModel->audio_params.has_audio_device )
      {
         addMessage(L("This vehicle doesn't have any audio capture device. Audio can't be used on this vehicle."));
         return;
      }
      add_menu_to_stack(new MenuVehicleAudio());
      return;
   }

   if ( m_IndexTelemetry == m_SelectedIndex )
   {
      if ( handle_commands_has_received_vehicle_core_plugins_info() )
      {
         add_menu_to_stack(new MenuVehicleTelemetry());
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
   if ( m_IndexDataLink == m_SelectedIndex )
   {
      if ( handle_commands_has_received_vehicle_core_plugins_info() )
      {
         add_menu_to_stack(new MenuVehicleDataLink());
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
   if ( m_IndexOSD == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleOSD());
      return;
   }
   if ( m_IndexRC == m_SelectedIndex )
      add_menu_to_stack(new MenuVehicleRC());

   if ( (-1 != m_IndexFunctions) && (m_IndexFunctions == m_SelectedIndex) )
   {
      add_menu_to_stack(new MenuVehicleFunctions());
      return;
   }

   if ( m_IndexRelay == m_SelectedIndex )
   {
      bool bCanUse = true;
      if ( g_pCurrentModel->radioInterfacesParams.interfaces_count < 2 )
         bCanUse = false;

      int countCardsOk = 0;
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( 0 != g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] )
         if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY )
            countCardsOk++;
      }
      if ( countCardsOk < 2 )
         bCanUse = false;

      if ( ! bCanUse )
      {
         MenuConfirmation* pMC = new MenuConfirmation(L("No Suitable Hardware"), L("You need at least two radio interfaces on the vehicle to be able to use the relay functionality."), 10, true);
         pMC->m_yPos = 0.3;
         add_menu_to_stack(pMC);
         return;
      }
      add_menu_to_stack(new MenuVehicleRelay());
   }

   if ( m_IndexCPU == m_SelectedIndex )
   {
      if ( hardware_board_is_openipc(g_pCurrentModel->hwCapabilities.uBoardType) )
      {
         add_menu_to_stack(new MenuVehicleCPU_OIPC());
         return;
      }

      add_menu_to_stack(new MenuVehicleExpert());
   }

   if ( m_IndexAlarms == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleAlarms());
      return;
   }
   if ( (m_IndexDeveloper != -1) && (m_IndexDeveloper == m_SelectedIndex) )
   {
      add_menu_to_stack(new MenuVehicleDev());
      return;
   }
}
