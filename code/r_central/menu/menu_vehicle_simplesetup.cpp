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

#include "../../base/base.h"
#include "../../base/config.h"
#include "../../base/tx_powers.h"
#include "menu.h"
#include "menu_confirmation.h"
#include "menu_vehicle_simplesetup.h"
#include "menu_item_text.h"
#include "menu_item_legend.h"
#include "menu_tx_raw_power.h"
#include "menu_vehicle.h"
#include "menu_vehicle_camera.h"
#include "menu_vehicle_video.h"
#include "menu_vehicle_osd.h"
#include "../osd/osd_common.h"
#include "../link_watch.h"
#include "../warnings.h"
#include "../launchers_controller.h"

MenuVehicleSimpleSetup::MenuVehicleSimpleSetup()
:Menu(MENU_ID_VEHICLE_SIMPLE_SETUP, L("Quick vehicle setup"), NULL)
{
   m_bDisableStacking = true;
   m_Width = 0.32;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.3;
   
   m_iIndexMenuOk = -1;
   m_iIndexMenuCancel = -1;
   m_iIndexFullConfig = -1;
   m_iIndexCamera = -1;
   m_iIndexVideo = -1;
   m_iIndexOSDLayout = -1;
   m_iIndexOSDSettings = -1;
   m_iIndexTelemetryDetect = -1;

   m_iCurrentSerialPortIndexUsedForTelemetry = -1;

   m_bPairingSetup = false;
   m_bTelemetrySetup = false;
   m_bSearchingTelemetry = false;
   m_iSearchTelemetryType = 0;
   m_iSearchTelemetryPort = 0;
   m_iSearchTelemetrySpeed = 0;
   m_uTimeStartCurrentTelemetrySearch = 0;

   m_pItemsSelect[0] = m_pItemsSelect[1] = m_pItemsSelect[2] = NULL;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      m_iIndexFreq[i] = -1;
      m_iIndexTxPowers[i] = -1;
   }   
}

MenuVehicleSimpleSetup::~MenuVehicleSimpleSetup()
{
}


void MenuVehicleSimpleSetup::onAddToStack()
{
   Menu::onAddToStack();

   /*
   if ( ! m_bPairingSetup )
      return;
   bool bShowTxWarning = false;
   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      int iCardModel = g_pCurrentModel->radioInterfacesParams.interface_card_model[i];
      if ( iCardModel < 0 )
         iCardModel = -iCardModel;
      int iPowerRaw = tx_powers_get_max_usable_power_raw_for_card(g_pCurrentModel->hwCapabilities.uBoardType, iCardModel);
      int iPowerMw = tx_powers_get_max_usable_power_mw_for_card(g_pCurrentModel->hwCapabilities.uBoardType, iCardModel);
      log_line("[MenuVehicleRadio] Max power for radio interface %d (%s): raw: %d, mw: %d, current raw power: %d",
         i+1, str_get_radio_card_model_string(iCardModel), iPowerRaw, iPowerMw, g_pCurrentModel->radioInterfacesParams.interface_raw_power[i]);

      if ( iCardModel == CARD_MODEL_RTL8812AU_DUAL_ANTENNA )
        bShowTxWarning = true;   
   }

   if ( bShowTxWarning )
   {
      char szText[256];
      if ( 1 == g_pCurrentModel->radioInterfacesParams.interfaces_count )
         sprintf(szText, L("Your radio interface (%s) has multiple cloned variants manufactured. Tx power varies depending on clone manufacturer, so a certain value can't be established."), str_get_radio_card_model_string(CARD_MODEL_RTL8812AU_DUAL_ANTENNA));
      else
         sprintf(szText, L("Some of your radio interfaces (%s) have multiple cloned variants manufactured. Tx power varies depending on clone manufacturer, so a certain value can't be established."), str_get_radio_card_model_string(CARD_MODEL_RTL8812AU_DUAL_ANTENNA));
         
      addMessage2(0, L("Variable Tx Power"),szText);
   }
   */
}

void MenuVehicleSimpleSetup::onShow()
{
   int iTmp = getSelectedMenuItemIndex();
   addItems();
   Menu::onShow();

   if ( iTmp > 0 )
   {
      m_SelectedIndex = iTmp;
      if ( m_SelectedIndex < 0 )
         m_SelectedIndex = 0;
      if ( m_SelectedIndex >= m_ItemsCount )
         m_SelectedIndex = m_ItemsCount-1;
   }
}

void MenuVehicleSimpleSetup::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   valuesToUI();
}


void MenuVehicleSimpleSetup::setPairingSetup()
{
   m_bPairingSetup = true;
   m_bTelemetrySetup = false;
   m_bSearchingTelemetry = true;
   if ( (NULL == g_pCurrentModel) || (g_pCurrentModel->hardwareInterfacesInfo.serial_port_count == 0) )
      m_bSearchingTelemetry = false;
   m_xPos = 0.27;
   m_Width = 0.46;
   setTitle(L("Quick Pairing Setup"));
   disableBackAction();
}

void MenuVehicleSimpleSetup::setTelemetryDetectionType()
{
   m_MenuId = MENU_ID_VEHICLE_TELEMETRY_DETECTION;
   m_bPairingSetup = false;
   m_bTelemetrySetup = true;
   m_bSearchingTelemetry = true;
   if ( (NULL == g_pCurrentModel) || (g_pCurrentModel->hardwareInterfacesInfo.serial_port_count == 0) )
      m_bSearchingTelemetry = false;
   m_xPos = 0.27;
   m_Width = 0.46;
   setTitle(L("Telemetry Detection Wizard"));
   disableBackAction();
}

void MenuVehicleSimpleSetup::addSearchItems()
{
   addRegularItems();
}

void MenuVehicleSimpleSetup::addRegularItems()
{
   float fVSpacing = getMenuFontHeight()*0.7;
   MenuItemText* pItem = NULL;

   if ( m_bPairingSetup )
   {
      //addTopLine(L("Welcome! Pairing your controller with your vehicle for the first time!"), dx);
      //addTopLine(L("Please select your desired options to apply to this vehicle:"), dx);
      addTopLine(L("First time pairing setup:"));
      addTopLine(L("(You can later change the values again from menus)"));
   }
   if ( NULL == g_pCurrentModel )
   {
      addMenuItem(new MenuItemText(L("Missing vehicle configuration.")));
      m_iIndexMenuOk = addMenuItem( new MenuItem(L("Cancel"), L("Closes this settings.")));
      return;
   }

   int iScreenIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;
   m_iCurrentSerialPortIndexUsedForTelemetry = -1;
   for( int i=0; i<g_pCurrentModel->hardwareInterfacesInfo.serial_port_count; i++ )
   {
       u32 uPortTelemetryType = g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & 0xFF;
       
       if ( g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & MODEL_SERIAL_PORT_BIT_SUPPORTED )
       if ( uPortTelemetryType == SERIAL_PORT_USAGE_TELEMETRY )
       {
          m_iCurrentSerialPortIndexUsedForTelemetry = i;
          break;
       }
   }

   if ( (! m_bPairingSetup) && (! m_bTelemetrySetup) )
   {
      addRadioItems();
      m_iIndexCamera = addMenuItem(new MenuItem(L("Camera settings"), L("Change camera settings: brightness, constrast, saturation, hue, etc.")));
      m_pMenuItems[m_iIndexCamera]->showArrow();

      m_iIndexVideo = addMenuItem(new MenuItem(L("Video settings"), L("Change video settings: resolution, FPS, bitrate, etc.")));
      m_pMenuItems[m_iIndexVideo]->showArrow();
   }
   
   m_iIndexOSDLayout = -1;
   m_pItemsSelect[2] = NULL;
   if ( m_bPairingSetup )
   {
      pItem = new MenuItemText(L("Select the OSD layout you want, how many OSD elements to show by default on your screen:"), true);
      pItem->setExtraHeight(fVSpacing*0.6);
      addMenuItem(pItem);

      m_pItemsSelect[2] = new MenuItemSelect(L("OSD Layout"), L("Set the default layout of this OSD screen (what elements are shown on this screen)."));
      m_pItemsSelect[2]->addSelection(L("None"));
      m_pItemsSelect[2]->addSelection(L("Minimal"));
      m_pItemsSelect[2]->addSelection(L("Compact"));
      m_pItemsSelect[2]->addSelection(L("Default"));
      m_pItemsSelect[2]->setUseMultiViewLayout();
      m_iIndexOSDLayout = addMenuItem(m_pItemsSelect[2]);
      m_pItemsSelect[2]->setSelectedIndex(g_pCurrentModel->osd_params.osd_layout_preset[iScreenIndex]);
      m_pItemsSelect[2]->setExtraHeight(fVSpacing);
   }

   m_iIndexOSDSettings = -1;
   if ( (! m_bPairingSetup) && ( ! m_bTelemetrySetup) )
   {
      m_iIndexOSDSettings = addMenuItem(new MenuItem(L("OSD settings"), L("Change OSD type, layout and settings.")));
      m_pMenuItems[m_iIndexOSDSettings]->showArrow();
   }

   if ( m_bPairingSetup || m_bTelemetrySetup )
   {
      pItem = new MenuItemText(L("If you have a flight controller, select the telemetry type used by the flight controller:"), true);
      pItem->setExtraHeight(fVSpacing*0.6);
      if ( m_bSearchingTelemetry )
         pItem->setEnabled(false);
      addMenuItem(pItem);
   }
   m_pItemsSelect[0] = new MenuItemSelect(L("FC Telemetry Type"), L("The telemetry type that is sent/received from the flight controller. This should match the telemetry type generated by the flight controller."));
   m_pItemsSelect[0]->addSelection(L("None"));
   m_pItemsSelect[0]->addSelection(L("MAVLink"));
   m_pItemsSelect[0]->addSelection(L("LTM"));
   m_pItemsSelect[0]->addSelection(L("MSP OSD"));
   if ( m_bPairingSetup || m_bTelemetrySetup )
      m_pItemsSelect[0]->setUseMultiViewLayout();
   else
      m_pItemsSelect[0]->setIsEditable();
   m_iIndexTelemetryType = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[0]->setSelection(0);
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_NONE )
      m_pItemsSelect[0]->setSelection(0);
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MAVLINK )
      m_pItemsSelect[0]->setSelection(1);
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_LTM )
      m_pItemsSelect[0]->setSelection(2);
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP )
      m_pItemsSelect[0]->setSelection(3);
   
   if ( m_bPairingSetup || m_bTelemetrySetup )
      m_pItemsSelect[0]->setExtraHeight(fVSpacing);
   if ( m_bSearchingTelemetry )
      m_pItemsSelect[0]->setEnabled(false);

   if ( m_bPairingSetup || m_bTelemetrySetup )
   {
      pItem = new MenuItemText(L("If you have a flight controller, select the air unit's serial port the flight controller is connected to:"), true);
      if ( m_bSearchingTelemetry )
         pItem->setEnabled(false);
      addMenuItem(pItem);
      char szText[256];
      sprintf(szText, L("(The default telemetry serial port speed it %d bps)"), DEFAULT_FC_TELEMETRY_SERIAL_SPEED );
      pItem = new MenuItemText(szText, true);
      pItem->setExtraHeight(fVSpacing*0.6);
      if ( m_bSearchingTelemetry )
         pItem->setEnabled(false);
      addMenuItem(pItem);
   }
   m_pItemsSelect[1] = new MenuItemSelect(L("Telemetry Port"), L("The Ruby vehicle port at which the flight controller telemetry connects to."));
   m_pItemsSelect[1]->addSelection(L("None"));
   for( int i=0; i<g_pCurrentModel->hardwareInterfacesInfo.serial_port_count; i++ )
      m_pItemsSelect[1]->addSelection(g_pCurrentModel->hardwareInterfacesInfo.serial_port_names[i]);
   if ( m_bPairingSetup || m_bTelemetrySetup )
      m_pItemsSelect[1]->setUseMultiViewLayout();
   else
      m_pItemsSelect[1]->setIsEditable();
   m_iIndexTelemetryPort = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSelect[1]->setSelectedIndex( 1 + m_iCurrentSerialPortIndexUsedForTelemetry );

   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_NONE )
      m_pItemsSelect[1]->setEnabled(false);

   if ( m_bPairingSetup || m_bTelemetrySetup )
      m_pItemsSelect[1]->setExtraHeight(fVSpacing);
   if ( m_bSearchingTelemetry )
      m_pItemsSelect[1]->setEnabled(false);

   if ( m_bPairingSetup )
      m_iIndexMenuOk = addMenuItem( new MenuItem(L("Looks Good! Done"), L("Applies the selections and closes this settings page.")));
   else if ( m_bTelemetrySetup )
      m_iIndexMenuOk = addMenuItem( new MenuItem(L("Done"), L("Applies the selections and closes this settings page.")));
   else
   {
      m_iIndexFullConfig = addMenuItem( new MenuItem(L("Full Vehicle Settings"), L("Configure/change all vehicle settings.")));
      m_pMenuItems[m_iIndexFullConfig]->showArrow();
   }
}

void MenuVehicleSimpleSetup::addRadioItems()
{
   char szBuff[128];
   char szTooltip[256];
   char szTitle[128];

   u32 uControllerAllSupportedBands = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      u32 cardFlags = controllerGetCardFlags(pRadioHWInfo->szMAC);
      if ( ! (cardFlags & RADIO_HW_CAPABILITY_FLAG_DISABLED) )
         uControllerAllSupportedBands |= pRadioHWInfo->supportedBands;
   }

   str_get_supported_bands_string(uControllerAllSupportedBands, szBuff);
   log_line("MenuVehicleSimpleSetup: All supported radio bands by controller: %s", szBuff);

   for( int iRadioLinkId=0; iRadioLinkId<g_pCurrentModel->radioLinksParams.links_count; iRadioLinkId++ )
   {
      m_iIndexFreq[iRadioLinkId] = -1;
      
      int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(iRadioLinkId);
      if ( -1 == iRadioInterfaceId )
      {
         log_softerror_and_alarm("MenuVehicleSimpleSetup: Invalid radio link. No radio interfaces assigned to radio link %d.", iRadioLinkId+1);
         continue;
      }

      str_get_supported_bands_string(g_pCurrentModel->radioInterfacesParams.interface_supported_bands[iRadioInterfaceId], szBuff);
      log_line("MenuVehicleSimpleSetup: Vehicle radio interface %d (used on vehicle radio link %d) supported bands: %s", 
         iRadioInterfaceId+1, iRadioLinkId+1, szBuff);

      if ( g_pCurrentModel->radioInterfacesParams.interface_card_model[iRadioInterfaceId] == CARD_MODEL_SERIAL_RADIO_ELRS )
      {
         m_SupportedChannelsCount[iRadioLinkId] = 1;
         m_SupportedChannels[iRadioLinkId][0] = g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLinkId];
      }
      else
         m_SupportedChannelsCount[iRadioLinkId] = getSupportedChannels( uControllerAllSupportedBands & g_pCurrentModel->radioInterfacesParams.interface_supported_bands[iRadioInterfaceId], 1, &(m_SupportedChannels[iRadioLinkId][0]), 100);

      log_line("MenuVehicleSimpleSetup: Suported channels count by controller and vehicle radio interface %d: %d",
          iRadioInterfaceId+1, m_SupportedChannelsCount[iRadioLinkId]);

      char szTmp[128];
      strcpy(szTmp, L("Radio Link Frequency"));
      if ( g_pCurrentModel->radioLinksParams.links_count > 1 )
         sprintf(szTmp, "Radio Link %d Frequency", iRadioLinkId+1 );

      strcpy(szTooltip, L("Sets the radio link frequency for this radio link."));
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), " Radio type: %s.", str_get_radio_card_model_string(g_pCurrentModel->radioInterfacesParams.interface_card_model[iRadioInterfaceId]));
      strcat(szTooltip, szBuff);
      
      int iCountInterfacesAssignedToThisLink = 0;
      for( int i=0; i<g_SM_RadioStats.countLocalRadioInterfaces; i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId == iRadioLinkId )
            iCountInterfacesAssignedToThisLink++;
      }

      if ( 0 == iCountInterfacesAssignedToThisLink )
      {
         snprintf(szTitle, sizeof(szTitle)/sizeof(szTitle[0]), "%s", szTmp);
         strcpy(szTooltip, L("Change the radio link frequency."));
      }
      else
      {
         snprintf(szTitle, sizeof(szTitle)/sizeof(szTitle[0]), "%s", szTmp);
         strcpy(szTooltip, L("Change the radio link frequency."));
      }

      m_pItemsSelect[20+iRadioLinkId] = new MenuItemSelect(szTitle, szTooltip);

      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == iRadioLinkId )
      {
         sprintf(szBuff, "Relay on %s", str_format_frequency(g_pCurrentModel->relay_params.uRelayFrequencyKhz));
         m_pItemsSelect[20+iRadioLinkId]->addSelection(szBuff);
         m_pItemsSelect[20+iRadioLinkId]->setEnabled(false);
      }
      else
      {
         Preferences* pP = get_Preferences();
         int iCountChCurrentColumn = 0;
         for( int ch=0; ch<m_SupportedChannelsCount[iRadioLinkId]; ch++ )
         {
            if ( (pP->iScaleMenus > 0) && (iCountChCurrentColumn > 14 ) )
            {
               m_pItemsSelect[20+iRadioLinkId]->addSeparator();
               iCountChCurrentColumn = 0;
            }
            if ( 0 == m_SupportedChannels[iRadioLinkId][ch] )
            {
               m_pItemsSelect[20+iRadioLinkId]->addSeparator();
               iCountChCurrentColumn = 0;
               continue;
            }
            iCountChCurrentColumn++;
            sprintf(szBuff,"%s", str_format_frequency(m_SupportedChannels[iRadioLinkId][ch]));
            m_pItemsSelect[20+iRadioLinkId]->addSelection(szBuff);
         }
         log_line("MenuVehicleSimpleSetup: Added %d frequencies to selector for radio link %d", m_SupportedChannelsCount[iRadioLinkId], iRadioLinkId+1);
         m_SupportedChannelsCount[iRadioLinkId] = getSupportedChannels( uControllerAllSupportedBands & g_pCurrentModel->radioInterfacesParams.interface_supported_bands[iRadioInterfaceId], 0, &(m_SupportedChannels[iRadioLinkId][0]), 100);
         log_line("MenuVehicleSimpleSetup: %d frequencies supported for radio link %d", m_SupportedChannelsCount[iRadioLinkId], iRadioLinkId+1);
         
         int selectedIndex = 0;
         for( int ch=0; ch<m_SupportedChannelsCount[iRadioLinkId]; ch++ )
         {
            if ( g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLinkId] == m_SupportedChannels[iRadioLinkId][ch] )
               break;
            selectedIndex++;
         }
         m_pItemsSelect[20+iRadioLinkId]->setSelection(selectedIndex);

         if ( 0 == m_SupportedChannelsCount[iRadioLinkId] )
         {
            sprintf(szBuff,"%s", str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLinkId]));
            m_pItemsSelect[20+iRadioLinkId]->addSelection(szBuff);
            m_pItemsSelect[20+iRadioLinkId]->setSelectedIndex(0);
            m_pItemsSelect[20+iRadioLinkId]->setEnabled(false);
         }

         m_pItemsSelect[20+iRadioLinkId]->setIsEditable();
         //if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         //   m_pItemsSelect[20+iRadioLinkId]->setEnabled(false);
         if ( g_pCurrentModel->radioInterfacesParams.interface_card_model[iRadioInterfaceId] == CARD_MODEL_SERIAL_RADIO_ELRS )
            m_pItemsSelect[20+iRadioLinkId]->setEnabled(false);
      }
      m_iIndexFreq[iRadioLinkId] = addMenuItem(m_pItemsSelect[20+iRadioLinkId]);
   }

   for( int iLink=0; iLink<g_pCurrentModel->radioLinksParams.links_count; iLink++ )
   {
      int iVehicleLinkPowerMaxMw = 0;
      int iLinkPowersMw[MAX_RADIO_INTERFACES];
      int iCountLinkInterfaces = 0;
      bool bBoost2W = false;
      bool bBoost4W = false;
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] != iLink )
            continue;
         if ( ! hardware_radio_type_is_ieee(g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[i] & 0xFF) )
            continue;

         int iCardModel = g_pCurrentModel->radioInterfacesParams.interface_card_model[i];
         int iCardRawPower = g_pCurrentModel->radioInterfacesParams.interface_raw_power[i];
         int iCardPowerMw = tx_powers_convert_raw_to_mw(g_pCurrentModel->hwCapabilities.uBoardType, iCardModel, iCardRawPower);
         iLinkPowersMw[iCountLinkInterfaces] = iCardPowerMw;
         iCountLinkInterfaces++;
         int iPowerMaxMw = tx_powers_get_max_usable_power_mw_for_card(g_pCurrentModel->hwCapabilities.uBoardType, iCardModel);
         if ( iPowerMaxMw > iVehicleLinkPowerMaxMw )
            iVehicleLinkPowerMaxMw = iPowerMaxMw;

         if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_2W )
            bBoost2W = true;
         if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_4W )
            bBoost4W = true;
      }

      char szTitle[128];
      strcpy(szTitle, L("Radio Link Tx Power (mW)"));
      if ( g_pCurrentModel->radioLinksParams.links_count > 1 )
         sprintf(szTitle, L("Radio Link %d Tx Power (mw)"), iLink+1);
      m_pItemsSelect[40+iLink] = createMenuItemTxPowers(szTitle, false, bBoost2W, bBoost4W, iVehicleLinkPowerMaxMw);
      m_iIndexTxPowers[iLink] = addMenuItem(m_pItemsSelect[40+iLink]);
      selectMenuItemTxPowersValue(m_pItemsSelect[40+iLink], false, bBoost2W, bBoost4W, &(iLinkPowersMw[0]), iCountLinkInterfaces, iVehicleLinkPowerMaxMw);

   }
   /*
   char szText[256];
   strcpy(szText, L("The Tx power is for the radio downlink(s).\nMaximum selectable Tx power is computed based on detected radio interfaces on the vehicle: "));
   szBuff[0] = 0;
   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( ! g_pCurrentModel->radioInterfaceIsWiFiRadio(i) )
         continue;
      int iCardModel = g_pCurrentModel->radioInterfacesParams.interface_card_model[i];
      if ( 0 != szBuff[0] )
         strcat(szBuff, ", ");
      //str_get_radio_driver_description(pRadioHWInfo->iRadioDriver);
      strcat(szBuff, str_get_radio_card_model_string_short(iCardModel));
   }
   strcat(szText, szBuff);
   MenuItemLegend* pLegend = new MenuItemLegend(L("Note"), szText, 0);
   pLegend->setExtraHeight(0.4*g_pRenderEngine->textHeight(g_idFontMenu));
   addMenuItem(pLegend);
   */
}

void MenuVehicleSimpleSetup::addItems()
{
   int iTmp = getSelectedMenuItemIndex();

   removeAllItems();
   removeAllTopLines();
   m_iIndexMenuOk = -1;
   m_iIndexMenuCancel = -1;
   m_iIndexOSDLayout = -1;
   m_iIndexOSDSettings = -1;
   m_iIndexTelemetryDetect = -1;
   m_iIndexTelemetryType = -1;
   m_iIndexTelemetryPort = -1;
   m_iIndexFullConfig = -1;
   m_iIndexCamera = -1;
   m_iIndexVideo = -1;
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      m_iIndexFreq[i] = -1;
      m_iIndexTxPowers[i] = -1;
   }   
   m_pItemsSelect[0] = m_pItemsSelect[1] = m_pItemsSelect[2] = NULL;

   if ( m_bSearchingTelemetry )
      addSearchItems();
   else
      addRegularItems();

   if ( iTmp > 0 )
   {
      m_SelectedIndex = iTmp;
      if ( m_SelectedIndex < 0 )
         m_SelectedIndex = 0;
      if ( m_SelectedIndex >= m_ItemsCount )
         m_SelectedIndex = m_ItemsCount-1;
   }
}

void MenuVehicleSimpleSetup::valuesToUI()
{
   int iTmp = getSelectedMenuItemIndex();
   
   addItems();

   if ( (iTmp >= 0) && (iTmp <m_ItemsCount) )
   {
      m_SelectedIndex = iTmp;
      onFocusedItemChanged();
   }
}

void MenuVehicleSimpleSetup::renderSearch()
{
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);

   float yPos = m_RenderYPos + m_RenderTitleHeight;
   if ( NULL != m_pItemsSelect[2] )
     yPos = m_pItemsSelect[2]->getItemRenderYPos();
   else if ( NULL != m_pItemsSelect[0] )
     yPos = m_pItemsSelect[0]->getItemRenderYPos();
   yPos += height_text*1.5;

   float xPos = m_RenderXPos + m_sfMenuPaddingX;
   float fWidth = m_RenderWidth - 2*m_sfMenuPaddingX ;
   float fHeight = (m_RenderYPos + m_RenderHeight - m_RenderFooterHeight - m_sfMenuPaddingY) - yPos;

   float fAlpha = g_pRenderEngine->setGlobalAlfa(0.9);
   bool bAlpha = g_pRenderEngine->isAlphaBlendingEnabled();
   g_pRenderEngine->enableAlphaBlending();
   g_pRenderEngine->setColors(get_Color_MenuBg());
   //g_pRenderEngine->setStroke(get_Color_MenuText());
   g_pRenderEngine->setStroke(0,0,0,0);
   g_pRenderEngine->drawRoundRect(xPos - g_pRenderEngine->getPixelWidth(), yPos - g_pRenderEngine->getPixelHeight(), fWidth + 2.0 * g_pRenderEngine->getPixelWidth(), fHeight + 2.0*g_pRenderEngine->getPixelHeight(), 0.01*Menu::getMenuPaddingY());
   g_pRenderEngine->setAlphaBlendingEnabled(bAlpha);
   g_pRenderEngine->setGlobalAlfa(fAlpha);

   yPos += fHeight*0.3;
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText());
   g_pRenderEngine->setFill(0,0,0,0);
   g_pRenderEngine->drawRoundRect(xPos + fWidth*0.2, yPos, fWidth-0.4*fWidth, fHeight*0.5, 0.01*Menu::getMenuPaddingY());
   g_pRenderEngine->setColors(get_Color_MenuText());

   yPos += m_sfMenuPaddingY;
   char szText[128];
   strcpy(szText, L("Autodetecting telemetry settings"));
   float fTextWidth = g_pRenderEngine->textWidth(g_idFontMenu, szText);
   g_pRenderEngine->drawText(xPos + (fWidth-fTextWidth)*0.5, yPos, g_idFontMenu, szText);

   yPos += height_text;
   strcpy(szText, L("Please wait"));
   fTextWidth = g_pRenderEngine->textWidth(g_idFontMenu, szText);
   for( int i=0; i<(m_iLoopCounter%4); i++ )
      strcat(szText, ".");
   g_pRenderEngine->drawText(xPos + (fWidth-fTextWidth)*0.5, yPos,  g_idFontMenu, szText);

   yPos += height_text*1.5;

   int iTotalSteps = g_pCurrentModel->hardwareInterfacesInfo.serial_port_count * 2 * 2;
   int iCurrentStep = m_iSearchTelemetryPort * 4 + m_iSearchTelemetrySpeed * 2 + m_iSearchTelemetryType;

   g_pRenderEngine->setStroke(get_Color_MenuText());
   g_pRenderEngine->setFill(0,0,0,0);
   g_pRenderEngine->drawRoundRect(xPos + fWidth*0.25, yPos, fWidth-0.5*fWidth, height_text, 0.01*Menu::getMenuPaddingY());
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setFill(get_Color_IconSucces());
   g_pRenderEngine->drawRoundRect(xPos + fWidth*0.25, yPos, (fWidth-0.5*fWidth) * (float)iCurrentStep / (float)iTotalSteps, height_text, 0.01*Menu::getMenuPaddingY());
   g_pRenderEngine->setColors(get_Color_MenuText());
}

void MenuVehicleSimpleSetup::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();

   /*
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float iconHeight = 3.0*height_text;
   float iconWidth = iconHeight/g_pRenderEngine->getAspectRatio();

   if ( m_bPairingSetup && (NULL != g_pCurrentModel) )
   {
      u32 idIcon = osd_getVehicleIcon( g_pCurrentModel->vehicle_type );
      float yBar = m_RenderYPos + m_RenderHeaderHeight + m_sfMenuPaddingY + m_fExtraHeightStart;
      //g_pRenderEngine->setColors(get_Color_MenuText(), 0.7);
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      g_pRenderEngine->drawIcon(m_RenderXPos + m_sfMenuPaddingX, yBar + 0.5*(yTop-yBar - iconHeight - height_text), iconWidth, iconHeight, idIcon);
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   }
   */
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);

   if ( m_bSearchingTelemetry )
      renderSearch();
}

bool MenuVehicleSimpleSetup::periodicLoop()
{
   Menu::periodicLoop();
   if ( ! m_bSearchingTelemetry )
      return false;

   if ( 0 == m_uTimeStartCurrentTelemetrySearch )
   {
      m_iSearchTelemetryType = 0;
      m_iSearchTelemetryPort = 0;
      m_iSearchTelemetrySpeed = 0;
      m_uTimeStartCurrentTelemetrySearch = g_TimeNow;
      send_pause_adaptive_to_router(10000);
      sendTelemetrySearchToVehicle();
      return true;
   }

   if ( g_TimeNow > m_uTimeStartCurrentTelemetrySearch + 500 )
   {
      bool bHasFCTelemetry = vehicle_runtime_has_received_fc_telemetry(g_pCurrentModel->uVehicleId);
      u32 uTimeLastRubyTelemetry = vehicle_runtime_get_time_last_received_ruby_telemetry(g_pCurrentModel->uVehicleId);
      log_line("MenuVehicleSimpleSetup: Has FC telemetry? %s, last Ruby telemetry time: %u ms ago",
         bHasFCTelemetry?"yes":"no", g_TimeNow - uTimeLastRubyTelemetry);
      if ( bHasFCTelemetry && (uTimeLastRubyTelemetry > m_uTimeStartCurrentTelemetrySearch + 500) )
      {
         log_line("MenuVehicleSimpleSetup: Received vehicle FC telemetry.");
         m_bSearchingTelemetry = false;
         enableBackAction();
         m_SelectedIndex = m_iIndexMenuOk;
         send_pause_adaptive_to_router(0);
         addItems();
         return true;
      }
   }

   if ( g_TimeNow < m_uTimeStartCurrentTelemetrySearch+2000 )
      return true;

   m_iSearchTelemetryType++;
   if ( m_iSearchTelemetryType >= 2 )
   {
      m_iSearchTelemetryType = 0;
      m_iSearchTelemetrySpeed++;
      if ( m_iSearchTelemetrySpeed >= 2 )
      {
         m_iSearchTelemetrySpeed = 0;
         m_iSearchTelemetryPort++;
         if ( m_iSearchTelemetryPort >= g_pCurrentModel->hardwareInterfacesInfo.serial_port_count )
         {
            // Finished all options
            m_bSearchingTelemetry = false;
            enableBackAction();
            m_SelectedIndex = m_iIndexMenuOk;
            send_pause_adaptive_to_router(0);
            addItems();
            return true;
         }
      }
   }
   m_uTimeStartCurrentTelemetrySearch = g_TimeNow;
   sendTelemetrySearchToVehicle();
   return true;
}

void MenuVehicleSimpleSetup::sendTelemetrySearchToVehicle()
{
   int iSerialPortsOrder[MAX_MODEL_SERIAL_PORTS];

   for( int i=0; i<MAX_MODEL_SERIAL_PORTS; i++ )
      iSerialPortsOrder[i] = i;

   if ( 4 == g_pCurrentModel->hardwareInterfacesInfo.serial_port_count )
   {
      iSerialPortsOrder[0] = 0;
      iSerialPortsOrder[1] = 2;
      iSerialPortsOrder[2] = 1;
      iSerialPortsOrder[3] = 3;
   }
   u32 uParam = 0;
   
   uParam = (u8)TELEMETRY_TYPE_MSP;
   if ( 1 == m_iSearchTelemetryType )
      uParam = (u8)TELEMETRY_TYPE_MAVLINK;
   uParam &= 0x0F;

   uParam |= (((u32)(iSerialPortsOrder[m_iSearchTelemetryPort])) & 0x0F) << 4;

   if ( m_iSearchTelemetrySpeed == 0 )
      uParam |= ((u32)(57600)) << 8;
   else
      uParam |= ((u32)(115200)) << 8;
   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_TELEMETRY_TYPE_AND_PORT, uParam, NULL, 0) )
      valuesToUI();

   vehicle_runtime_reset_has_received_fc_telemetry_info(g_pCurrentModel->uVehicleId);
}

void MenuVehicleSimpleSetup::sendTelemetryTypeToVehicle()
{
   telemetry_parameters_t params;
   memcpy(&params, &g_pCurrentModel->telemetry_params, sizeof(telemetry_parameters_t));
   
   params.fc_telemetry_type = TELEMETRY_TYPE_NONE;
   if ( 1 == m_pItemsSelect[0]->getSelectedIndex() )
      params.fc_telemetry_type = TELEMETRY_TYPE_MAVLINK;
   if ( 2 == m_pItemsSelect[0]->getSelectedIndex() )
      params.fc_telemetry_type = TELEMETRY_TYPE_LTM;
   if ( 3 == m_pItemsSelect[0]->getSelectedIndex() )
      params.fc_telemetry_type = TELEMETRY_TYPE_MSP;

   if ( params.fc_telemetry_type == g_pCurrentModel->telemetry_params.fc_telemetry_type )
      return;

   // Remove serial port used, if telemetry is set to None.
   // Same thing will be done by vehicle when it gets the telemetry change command.
   if ( (params.fc_telemetry_type == TELEMETRY_TYPE_NONE) && (-1 != m_iCurrentSerialPortIndexUsedForTelemetry) )
   {
      // Remove serial port usage (set it to none)
      g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[m_iCurrentSerialPortIndexUsedForTelemetry] &= 0xFFFFFF00;
   }

   // Assign a default serial port if none is set
   if ( (params.fc_telemetry_type != TELEMETRY_TYPE_NONE) && (-1 == m_iCurrentSerialPortIndexUsedForTelemetry) )
   {
      log_line("MenuVehicleSimpleSetup: Try to find a default serial port for telemetry...");
      for( int i=0; i<g_pCurrentModel->hardwareInterfacesInfo.serial_port_count; i++ )
      {
         u32 uPortUsage = g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & 0xFF;
         
         if ( g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & MODEL_SERIAL_PORT_BIT_SUPPORTED )
         if ( uPortUsage == SERIAL_PORT_USAGE_NONE )
         {
            m_iCurrentSerialPortIndexUsedForTelemetry = i;
            break;
         }
      }
      if ( -1 == m_iCurrentSerialPortIndexUsedForTelemetry )
        log_line("MenuVehicleSimpleSetup: Could not find a default serial port for telemetry.");
      else
        log_line("MenuVehicleSimpleSetup: Found a default serial port for telemetry. Serial port index %d", m_iCurrentSerialPortIndexUsedForTelemetry);

      if ( -1 != m_iCurrentSerialPortIndexUsedForTelemetry )
      {
         type_vehicle_hardware_interfaces_info new_info;
         memcpy((u8*)&new_info, (u8*)&(g_pCurrentModel->hardwareInterfacesInfo), sizeof(type_vehicle_hardware_interfaces_info));

         if ( new_info.serial_port_speed[m_iCurrentSerialPortIndexUsedForTelemetry] <= 0 )
            new_info.serial_port_speed[m_iCurrentSerialPortIndexUsedForTelemetry] = DEFAULT_FC_TELEMETRY_SERIAL_SPEED;

         new_info.serial_port_supported_and_usage[m_iCurrentSerialPortIndexUsedForTelemetry] &= 0xFFFFFF00;
         if ( 1 == m_pItemsSelect[0]->getSelectedIndex() )
            new_info.serial_port_supported_and_usage[m_iCurrentSerialPortIndexUsedForTelemetry] |= SERIAL_PORT_USAGE_TELEMETRY;
         if ( 2 == m_pItemsSelect[0]->getSelectedIndex() )
            new_info.serial_port_supported_and_usage[m_iCurrentSerialPortIndexUsedForTelemetry] |= SERIAL_PORT_USAGE_TELEMETRY;
         if ( 3 == m_pItemsSelect[0]->getSelectedIndex() )
            new_info.serial_port_supported_and_usage[m_iCurrentSerialPortIndexUsedForTelemetry] |= SERIAL_PORT_USAGE_TELEMETRY;

         // We don't need to send the serial ports configuration to vehicle as the vehicle
         // does the same assignment if no serial port is set when telemetry changes with
         // the command below to update telemetry info.

         memcpy((u8*)&(g_pCurrentModel->hardwareInterfacesInfo), (u8*)&new_info, sizeof(type_vehicle_hardware_interfaces_info));
      }
   }
   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_TELEMETRY_PARAMETERS, 0, (u8*)&params, sizeof(telemetry_parameters_t)) )
      valuesToUI();
}

void MenuVehicleSimpleSetup::sendTelemetryPortToVehicle()
{
   int iSerialPort = m_pItemsSelect[1]->getSelectedIndex();
   if ( iSerialPort != 0 )
   if ( (g_pCurrentModel->hardwareInterfacesInfo.serial_port_supported_and_usage[iSerialPort-1] & 0xFF) == SERIAL_PORT_USAGE_SIK_RADIO )
   {
      MenuConfirmation* pMC = new MenuConfirmation("Can't use serial port", "The serial port you selected can't be used for telemetry connection to flight controller as it's used for a SiK radio interface.",1, true);
      pMC->m_yPos = 0.3;
      pMC->disablePairingUIActions();
      add_menu_to_stack(pMC);
      valuesToUI();
      return;
   }

   if ( 0 == iSerialPort )
   {
      if ( m_iCurrentSerialPortIndexUsedForTelemetry != -1 )
      {
         type_vehicle_hardware_interfaces_info new_info;
         memcpy((u8*)&new_info, (u8*)&(g_pCurrentModel->hardwareInterfacesInfo), sizeof(type_vehicle_hardware_interfaces_info));
         new_info.serial_port_supported_and_usage[m_iCurrentSerialPortIndexUsedForTelemetry] &= 0xFFFFFF00;
         new_info.serial_port_supported_and_usage[m_iCurrentSerialPortIndexUsedForTelemetry] |= SERIAL_PORT_USAGE_NONE;
         
         log_line("Sending disabling telemetry serial port selection to vehicle.");
         if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_SERIAL_PORTS_INFO, 0, (u8*)&new_info, sizeof(type_vehicle_hardware_interfaces_info)) )
            valuesToUI();
         return;
      }
   }
   else
   {
      type_vehicle_hardware_interfaces_info new_info;
      memcpy((u8*)&new_info, (u8*)&(g_pCurrentModel->hardwareInterfacesInfo), sizeof(type_vehicle_hardware_interfaces_info));

      u8 uCurrentUsage = new_info.serial_port_supported_and_usage[iSerialPort-1] & 0xFF;
      if ( uCurrentUsage == SERIAL_PORT_USAGE_DATA_LINK )
      {
         MenuConfirmation* pMC = new MenuConfirmation(L("User Data Link Disabled"), L("The serial port was used by your custom data link. It was reasigned to the telemetry link."), 1, true);
         pMC->m_yPos = 0.3;
         pMC->disablePairingUIActions();
         add_menu_to_stack(pMC);      
      }

      if ( m_iCurrentSerialPortIndexUsedForTelemetry != -1 )
      {
         new_info.serial_port_supported_and_usage[m_iCurrentSerialPortIndexUsedForTelemetry] &= 0xFFFFFF00;
         new_info.serial_port_supported_and_usage[m_iCurrentSerialPortIndexUsedForTelemetry] |= SERIAL_PORT_USAGE_NONE;
      }

      new_info.serial_port_supported_and_usage[iSerialPort-1] &= 0xFFFFFF00;
      if ( 1 == m_pItemsSelect[0]->getSelectedIndex() )
         new_info.serial_port_supported_and_usage[iSerialPort-1] |= SERIAL_PORT_USAGE_TELEMETRY;
      if ( 2 == m_pItemsSelect[0]->getSelectedIndex() )
         new_info.serial_port_supported_and_usage[iSerialPort-1] |= SERIAL_PORT_USAGE_TELEMETRY;
      if ( 3 == m_pItemsSelect[0]->getSelectedIndex() )
         new_info.serial_port_supported_and_usage[iSerialPort-1] |= SERIAL_PORT_USAGE_TELEMETRY;

      log_line("Sending new serial port to be used for telemetry to vehicle.");
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_SERIAL_PORTS_INFO, 0, (u8*)&new_info, sizeof(type_vehicle_hardware_interfaces_info)) )
         valuesToUI();
   }
}

void MenuVehicleSimpleSetup::sendOSDToVehicle()
{
   if ( NULL == m_pItemsSelect[2] )
      return;

   osd_parameters_t params;
   memcpy(&params, &(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));
   int iScreenIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;

   log_line("MenuVehicleSimpleSetup: Sending OSD info, has mavlink telem? %s", (m_pItemsSelect[0]->getSelectedIndex() == 1)?"yes":"no");
   if ( 1 == m_pItemsSelect[0]->getSelectedIndex() )
      params.uFlags |= OSD_BIT_FLAGS_SHOW_FLIGHT_END_STATS;
   else
      params.uFlags &= ~OSD_BIT_FLAGS_SHOW_FLIGHT_END_STATS;

   params.osd_layout_preset[iScreenIndex] = m_pItemsSelect[2]->getSelectedIndex();

   if ( params.osd_layout_preset[iScreenIndex] < OSD_PRESET_DEFAULT )
   {
      Preferences* pP = get_Preferences();
      ControllerSettings* pCS = get_ControllerSettings();
      pP->iShowControllerCPUInfo = 0;
      pCS->iShowVoltage = 0;
      save_Preferences();
      save_ControllerSettings();
   }
   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_OSD_PARAMS, 0, (u8*)&params, sizeof(osd_parameters_t)) )
      valuesToUI();
}


void MenuVehicleSimpleSetup::sendNewRadioLinkFrequency(int iVehicleLinkIndex, u32 uNewFreqKhz)
{
   if ( (iVehicleLinkIndex < 0) || (iVehicleLinkIndex >= MAX_RADIO_INTERFACES) )
      return;

   log_line("MenuVehicleSimpleSetup: Changing radio link %d frequency to %u khz (%s)", iVehicleLinkIndex+1, uNewFreqKhz, str_format_frequency(uNewFreqKhz));

   type_radio_links_parameters newRadioLinkParams;
   memcpy((u8*)&newRadioLinkParams, (u8*)&(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));
   
   newRadioLinkParams.link_frequency_khz[iVehicleLinkIndex] = uNewFreqKhz;
   
   if ( 0 == memcmp((u8*)&newRadioLinkParams, (u8*)&(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters)) )
   {
      log_line("MenuVehicleSimpleSetup: No change in radio link frequency. Do not send command.");
      return;
   }

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
   PH.total_length = sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE + sizeof(type_radio_links_parameters);

   u8 buffer[1024];
   static int s_iMenuSimpleSetupTestNumberCount = 0;
   s_iMenuSimpleSetupTestNumberCount++;
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   buffer[sizeof(t_packet_header)] = PACKET_TYPE_TEST_RADIO_LINK_PROTOCOL_VERSION;
   buffer[sizeof(t_packet_header)+1] = PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE;
   buffer[sizeof(t_packet_header)+2] = (u8)iVehicleLinkIndex;
   buffer[sizeof(t_packet_header)+3] = (u8)s_iMenuSimpleSetupTestNumberCount;
   buffer[sizeof(t_packet_header)+4] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START;
   memcpy(buffer + sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE, &newRadioLinkParams, sizeof(type_radio_links_parameters));
   send_packet_to_router(buffer, PH.total_length);

   link_set_is_reconfiguring_radiolink(iVehicleLinkIndex);
   warnings_add_configuring_radio_link(iVehicleLinkIndex, L("Changing Frequency"));
}


void MenuVehicleSimpleSetup::computeSendPowerToVehicle(int iVehicleLinkIndex)
{
   if ( (iVehicleLinkIndex < 0) || (iVehicleLinkIndex >= g_pCurrentModel->radioLinksParams.links_count) )
      return;

   int iIndexSelector = 40 + iVehicleLinkIndex;
   int iIndex = m_pItemsSelect[iIndexSelector]->getSelectedIndex();

   if ( iIndex == m_pItemsSelect[iIndexSelector]->getSelectionsCount() -1 )
   {
      add_menu_to_stack(new MenuTXRawPower());
      return;
   }

   log_line("MenuVehicleSimpleSetup: Setting radio link %d tx power to %s", iVehicleLinkIndex+1, m_pItemsSelect[iIndexSelector]->getSelectionIndexText(iIndex));

   int iUIPowerLevelsCount = 0;
   const int* piUIPowerLevelsMw = tx_powers_get_ui_levels_mw(&iUIPowerLevelsCount);
   int iPowerMwToSet = piUIPowerLevelsMw[iIndex];

   u8 uCommandParams[MAX_RADIO_INTERFACES+1];
   uCommandParams[0] = g_pCurrentModel->radioInterfacesParams.interfaces_count;
   bool bDifferent = false;
   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      uCommandParams[i+1] = g_pCurrentModel->radioInterfacesParams.interface_raw_power[i];

      if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] != iVehicleLinkIndex )
         continue;
      if ( ! hardware_radio_type_is_ieee(g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[i] & 0xFF) )
         continue;

      int iCardModel = g_pCurrentModel->radioInterfacesParams.interface_card_model[i];
      int iCardMaxPowerMw = tx_powers_get_max_usable_power_mw_for_card(g_pCurrentModel->hwCapabilities.uBoardType, iCardModel);
 
      int iCardNewPowerMw = iPowerMwToSet;
      if ( iCardNewPowerMw > iCardMaxPowerMw )
         iCardNewPowerMw = iCardMaxPowerMw;
      int iTxPowerRawToSet = tx_powers_convert_mw_to_raw(g_pCurrentModel->hwCapabilities.uBoardType, iCardModel, iCardNewPowerMw);
      uCommandParams[i+1] = iTxPowerRawToSet;
      log_line("MenuVehicleSimpleSetup: Setting tx raw power for card %d from %d to: %d (%d mw)",
         i+1, g_pCurrentModel->radioInterfacesParams.interface_raw_power[i], uCommandParams[i+1], iCardNewPowerMw);
      if ( uCommandParams[i+1] != g_pCurrentModel->radioInterfacesParams.interface_raw_power[i] )
         bDifferent = true;
   }

   if ( ! bDifferent )
   {
      log_line("MenuVehicleSimpleSetup: No change");
      return;
   }

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_TX_POWERS, 0, uCommandParams, uCommandParams[0]+1) )
       valuesToUI();
}

int MenuVehicleSimpleSetup::onBack()
{
   if ( isEditingItem() )
      return Menu::onBack();

   int iRet = Menu::onBack();
   if ( m_bPairingSetup )
   if ( onEventPairingTookUIActions() )
      onEventFinishedPairUIAction();

   menu_refresh_all_menus_except(this);
   return iRet;
}

void MenuVehicleSimpleSetup::onSelectItem()
{
   log_line("Menu Vehicle Simple Setup: selected item: %d", m_SelectedIndex);
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( NULL == g_pCurrentModel )
      return;

   if ( (-1 != m_iIndexCamera) && (m_iIndexCamera == m_SelectedIndex) )
   {
      MenuVehicleCamera* pMenuCam = new MenuVehicleCamera();
      pMenuCam->showCompact();
      add_menu_to_stack(pMenuCam);
      return;
   }

   if ( (-1 != m_iIndexVideo) && (m_iIndexVideo == m_SelectedIndex) )
   {
      MenuVehicleVideo* pMenuVid = new MenuVehicleVideo();
      add_menu_to_stack(pMenuVid);
      return;
   }

   if ( (-1 != m_iIndexOSDSettings) && (m_iIndexOSDSettings == m_SelectedIndex) )
   {
      MenuVehicleOSD* pMenuOSD = new MenuVehicleOSD();
      pMenuOSD->showCompact();
      add_menu_to_stack(pMenuOSD);
      return;
   }

   if ( m_iIndexTelemetryType == m_SelectedIndex )
   {
      sendTelemetryTypeToVehicle();
      return;
   }

   if ( m_iIndexTelemetryPort == m_SelectedIndex )
   {
      sendTelemetryPortToVehicle();
      return;
   }

   if ( (-1 != m_iIndexOSDLayout) && (m_iIndexOSDLayout == m_SelectedIndex) )
   {
      sendOSDToVehicle();
      return;
   }

   if ( (-1 != m_iIndexMenuOk) && (m_iIndexMenuOk == m_SelectedIndex) )
   {
      if ( m_bPairingSetup )
         sendOSDToVehicle();
   
      menu_refresh_all_menus_except(this);
      menu_stack_pop(0);
      if ( m_bPairingSetup )
      if ( onEventPairingTookUIActions() )
         onEventFinishedPairUIAction();
   }

   if ( m_bPairingSetup )
      return;

   if ( m_iIndexFullConfig == m_SelectedIndex )
   {
      if ( (NULL == g_pCurrentModel) || (0 == g_uActiveControllerModelVID) ||
        (g_bFirstModelPairingDone && (0 == getControllerModelsCount()) && (0 == getControllerModelsSpectatorCount())) )
      {
         addMessage2(0, L("Not paired with any vehicle"), L("Search for vehicles to find one and connect to."));
         return;
      }
      add_menu_to_stack(new MenuVehicle());
      return;
   }

   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
      if ( (m_iIndexTxPowers[i] != -1) && (m_iIndexTxPowers[i] == m_SelectedIndex) )
      {
         computeSendPowerToVehicle(i);
         return;
      }
      if ( m_iIndexFreq[i] == m_SelectedIndex )
      {
         int index = m_pItemsSelect[20+i]->getSelectedIndex();
         u32 freq = m_SupportedChannels[i][index];
         int band = getBand(freq);      
         if ( freq == g_pCurrentModel->radioLinksParams.link_frequency_khz[i] )
            return;

         if ( link_is_reconfiguring_radiolink() )
         {
            add_menu_to_stack(new MenuConfirmation(L("Configuration In Progress"), L("Another radio link configuration change is in progress. Please wait."), 0, true));
            valuesToUI();
            return;
         }

         u32 nicFreq[MAX_RADIO_INTERFACES];
         for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
            nicFreq[k] = g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[k];

         int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(i);
         nicFreq[iRadioInterfaceId] = freq;

         int nicFlags[MAX_RADIO_INTERFACES];
         for( int n=0; n<MAX_RADIO_INTERFACES; n++ )
            nicFlags[n] = g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[n];

         const char* szError = controller_validate_radio_settings( g_pCurrentModel, nicFreq, nicFlags, NULL, NULL, NULL);
         
         if ( NULL != szError && 0 != szError[0] )
         {
            log_line(szError);
            add_menu_to_stack(new MenuConfirmation(L("Invalid option"), szError, 0, true));
            valuesToUI();
            return;
         }

         bool supportedOnController = false;
         bool allSupported = true;
         for( int n=0; n<hardware_get_radio_interfaces_count(); n++ )
         {
            radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(n);
            if ( controllerIsCardDisabled(pRadioHWInfo->szMAC) )
               continue;
            if ( band & pRadioHWInfo->supportedBands )
               supportedOnController = true;
            else
               allSupported = false;
         }
         if ( ! supportedOnController )
         {
            char szBuff[128];
            sprintf(szBuff, "%s frequency is not supported by your controller.", str_format_frequency(freq));
            add_menu_to_stack(new MenuConfirmation(L("Invalid option"), szBuff, 0, true));
            valuesToUI();
            return;
         }
         if ( (! allSupported) && (1 == g_pCurrentModel->radioLinksParams.links_count) )
         {
            char szBuff[256];
            sprintf(szBuff, "Not all radio interfaces on your controller support %s frequency. Some radio interfaces on the controller will not be used to communicate with this vehicle.", str_format_frequency(freq));
            add_menu_to_stack(new MenuConfirmation(L("Confirmation"), szBuff, 0, true));
         }

         sendNewRadioLinkFrequency(i, freq);
         return;
      }
   }
}