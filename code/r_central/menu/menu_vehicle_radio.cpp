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
#include "../osd/osd_common.h"
#include "menu_vehicle_radio.h"
#include "menu_item_select.h"
#include "menu_confirmation.h"
#include "menu_item_text.h"
#include "menu_item_legend.h"
#include "menu_vehicle_radio_link.h"
#include "menu_vehicle_radio_link_sik.h"
#include "menu_radio_config.h"
#include "menu_tx_raw_power.h"
#include "menu_negociate_radio.h"
#include "menu_vehicle_radio_pit.h"
#include "menu_vehicle_radio_rt_capab.h"
#include "../../base/tx_powers.h"
#include "../../common/models_connect_frequencies.h"
#include "../../utils/utils_controller.h"
#include "../link_watch.h"
#include "../launchers_controller.h"
#include "../../base/encr.h"

int s_iTempGenNewFrequency = 0;
int s_iTempGenNewFrequencyLink = 0;

MenuVehicleRadioConfig::MenuVehicleRadioConfig(void)
:Menu(MENU_ID_VEHICLE_RADIO_CONFIG, L("Vehicle Radio Configuration"), NULL)
{
   m_Width = 0.39;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.21;
   m_bControllerHasKey = false;
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      m_IndexFreq[i] = -1;
      m_IndexConfigureLinks[i] = -1;
      m_IndexTxPowers[i] = -1;
   }
   m_IndexDevRuntimeInfo = -1;
   m_IndexShowTxPower = -1;
   m_pPopupRadioInterface = NULL;
   m_iLastLinkAddedPopupFor = -1;
   m_bDisablePopupCreation = false;
}

MenuVehicleRadioConfig::~MenuVehicleRadioConfig()
{
   if ( NULL != m_pPopupRadioInterface )
   {
      popups_remove(m_pPopupRadioInterface);
      m_pPopupRadioInterface = NULL;
      log_line("MenuVehicleRadio: Removed popup for tx power");
   }
   m_iLastLinkAddedPopupFor = -1;
}

void MenuVehicleRadioConfig::onAddToStack()
{
   Menu::onAddToStack();

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
}

void MenuVehicleRadioConfig::populate()
{
   int iTmp = getSelectedMenuItemIndex();
   removeAllItems();
   m_IndexDevRuntimeInfo = -1;
   m_IndexShowTxPower = -1;

   char szBuff[256];
   int len = 64;
   int res = lpp(szBuff, len);
   if ( res )
      m_bControllerHasKey = true;
   else
      m_bControllerHasKey = false;

   if ( 0 == g_pCurrentModel->radioLinksParams.links_count )
   {
      addMenuItem( new MenuItemText(L("No radio interfaces detected on this vehicle!")));
      return;
   }

   populateFrequencies();
   populateTxPowers();

   m_IndexPitMode = addMenuItem(new MenuItem(L("Pit Mode / Auto Power"), L("Enable auto adjustment to radio tx powers based on various conditions.")));
   m_pMenuItems[m_IndexPitMode]->showArrow();

   m_pItemsSelect[4] = new MenuItemSelect(L("Disable Uplinks"), L("Disable all uplinks, makes the system a one way system. Except for initial pairing and synching and sending commands to the vehicle. No video retransmissions happen, adaptive video is also disabled."));
   m_pItemsSelect[4]->addSelection(L("No"));
   m_pItemsSelect[4]->addSelection(L("Yes"));
   m_pItemsSelect[4]->setIsEditable();
   m_IndexDisableUplink = addMenuItem(m_pItemsSelect[4]);

   m_pItemsSelect[3] = new MenuItemSelect(L("Prioritize Uplink"), L("Prioritize Uplink data over Downlink data. Enable it when uplink data resilience and consistentcy is more important than downlink data."));
   m_pItemsSelect[3]->addSelection(L("No"));
   m_pItemsSelect[3]->addSelection(L("Yes"));
   m_pItemsSelect[3]->setIsEditable();
   m_IndexPrioritizeUplink = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSelect[3]->setEnabled(true);
   m_pItemsSelect[3]->setSelectedIndex(0);
   if ( g_pCurrentModel->uModelFlags & MODEL_FLAG_PRIORITIZE_UPLINK )
      m_pItemsSelect[3]->setSelectedIndex(1); 

   m_pItemsSelect[4]->setSelectedIndex(0);
   if ( g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_DOWNLINK_ONLY )
   {
      m_pItemsSelect[4]->setSelectedIndex(1);
      m_pItemsSelect[3]->setSelectedIndex(0);
      m_pItemsSelect[3]->setEnabled(false);
   }

   /*
   m_pItemsSelect[2] = new MenuItemSelect("Radio Encryption", "Changes the encryption used for the radio links. You can encrypt the video data, or telemetry data, or everything, including the ability to search for and find this vehicle (unless your controller has the right pass phrase).");
   m_pItemsSelect[2]->addSelection("None");
   m_pItemsSelect[2]->addSelection("Video Stream Only");
   m_pItemsSelect[2]->addSelection("Data Streams Only");
   m_pItemsSelect[2]->addSelection("Video and Data Streams");
   m_pItemsSelect[2]->addSelection("All Streams and Data");
   m_pItemsSelect[2]->setIsEditable();
   m_IndexEncryption = addMenuItem(m_pItemsSelect[2]);
   */
   m_IndexEncryption = -1;
   /*
   if ( -1 != m_IndexEncryption )
   {
      m_pItemsSelect[2]->setSelectedIndex(0);
      if ( g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_VIDEO )
         m_pItemsSelect[2]->setSelectedIndex(1); 

      if ( g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_DATA )
         m_pItemsSelect[2]->setSelectedIndex(2); 

      if ( g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_DATA )
      if ( g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_VIDEO )
         m_pItemsSelect[2]->setSelectedIndex(3); 

      if ( g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_ALL )
         m_pItemsSelect[2]->setSelectedIndex(4); 

      if ( ! m_bControllerHasKey )
      {
         m_pItemsSelect[2]->setSelectedIndex(0);
         //m_pItemsSelect[2]->setEnabled(false);
      }
   }
   */

   m_IndexOptimizeLinks = addMenuItem(new MenuItem(L("Optmize Radio Links Wizard"), L("Runs a process to optimize radio links parameters.")));
   m_pMenuItems[m_IndexOptimizeLinks]->showArrow();

   m_IndexRadioConfig = addMenuItem(new MenuItem(L("Full Radio Config"), L("Full radio configuration")));
   m_pMenuItems[m_IndexRadioConfig]->showArrow();

   ControllerSettings* pCS = get_ControllerSettings();
   m_IndexDevRuntimeInfo = -1;
   if ( (NULL != pCS) && pCS->iDeveloperMode )
   {
      m_IndexDevRuntimeInfo = addMenuItem( new MenuItem("Radio Runtime Capabilities") );
      m_pMenuItems[m_IndexDevRuntimeInfo]->showArrow();
      m_pMenuItems[m_IndexDevRuntimeInfo]->setTextColor(get_Color_Dev());
   }

   m_SelectedIndex = iTmp;
   if ( m_SelectedIndex >= m_ItemsCount )
      m_SelectedIndex = m_ItemsCount-1;
}

void MenuVehicleRadioConfig::populateFrequencies()
{
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      m_IndexFreq[i] = -1;

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
   log_line("MenuVehicleRadio: All supported radio bands by controller: %s", szBuff);

   for( int iRadioLinkId=0; iRadioLinkId<g_pCurrentModel->radioLinksParams.links_count; iRadioLinkId++ )
   {
      m_IndexFreq[iRadioLinkId] = -1;
      
      int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(iRadioLinkId);
      if ( -1 == iRadioInterfaceId )
      {
         log_softerror_and_alarm("MenuVehicleRadio: Invalid radio link. No radio interfaces assigned to radio link %d.", iRadioLinkId+1);
         continue;
      }

      str_get_supported_bands_string(g_pCurrentModel->radioInterfacesParams.interface_supported_bands[iRadioInterfaceId], szBuff);
      log_line("MenuVehicleRadio: Vehicle radio interface %d (used on vehicle radio link %d) supported bands: %s", 
         iRadioInterfaceId+1, iRadioLinkId+1, szBuff);

      if ( g_pCurrentModel->radioInterfacesParams.interface_card_model[iRadioInterfaceId] == CARD_MODEL_SERIAL_RADIO_ELRS )
      {
         m_SupportedChannelsCount[iRadioLinkId] = 1;
         m_SupportedChannels[iRadioLinkId][0] = g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLinkId];
      }
      else
         m_SupportedChannelsCount[iRadioLinkId] = getSupportedChannels( uControllerAllSupportedBands & g_pCurrentModel->radioInterfacesParams.interface_supported_bands[iRadioInterfaceId], 1, &(m_SupportedChannels[iRadioLinkId][0]), 100);

      log_line("MenuVehicleRadio: Suported channels count by controller and vehicle radio interface %d: %d",
          iRadioInterfaceId+1, m_SupportedChannelsCount[iRadioLinkId]);

      char szTmp[128];
      strcpy(szTmp, L("Radio Link Frequency"));
      if ( g_pCurrentModel->radioLinksParams.links_count > 1 )
         sprintf(szTmp, L("Radio Link %d Frequency"), iRadioLinkId+1 );
      if ( ! is_vehicle_radio_link_used(g_pCurrentModel, &g_SM_RadioStats, iRadioLinkId) )
         strcat(szTmp, " (Not used)");
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
         //snprintf(szTitle, sizeof(szTitle)/sizeof(szTitle[0]), "(D) %s", szTmp);
         //strcat(szTooltip, " This radio link is not connected to the controller.");
         snprintf(szTitle, sizeof(szTitle)/sizeof(szTitle[0]), "%s", szTmp);
         strcpy(szTooltip, L("Change the radio link frequency."));
      }
      else
      {
         //if ( 1 == g_pCurrentModel->radioLinksParams.links_count )
         {
            snprintf(szTitle, sizeof(szTitle)/sizeof(szTitle[0]), "%s", szTmp);
            strcpy(szTooltip, L("Change the radio link frequency."));
         }
         /*
         else
         {
            snprintf(szTitle, sizeof(szTitle)/sizeof(szTitle[0]), "(C) %s", szTmp);
            strcat(szTooltip, " This radio link is connected to the controller.");
         }
         */
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
         log_line("MenuVehicleRadio: Added %d frequencies to selector for radio link %d", m_SupportedChannelsCount[iRadioLinkId], iRadioLinkId+1);
         m_SupportedChannelsCount[iRadioLinkId] = getSupportedChannels( uControllerAllSupportedBands & g_pCurrentModel->radioInterfacesParams.interface_supported_bands[iRadioInterfaceId], 0, &(m_SupportedChannels[iRadioLinkId][0]), 100);
         log_line("MenuVehicleRadio: %d frequencies supported for radio link %d", m_SupportedChannelsCount[iRadioLinkId], iRadioLinkId+1);
         
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
      m_IndexFreq[iRadioLinkId] = addMenuItem(m_pItemsSelect[20+iRadioLinkId]);
   }
}

void MenuVehicleRadioConfig::populateTxPowers()
{
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      m_IndexTxPowers[i] = -1;
   
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
      if ( ! is_vehicle_radio_link_used(g_pCurrentModel, &g_SM_RadioStats, iLink) )
         strcat(szTitle, " (Not used)");        
      m_pItemsSelect[40+iLink] = createMenuItemTxPowers(szTitle, false, bBoost2W, bBoost4W, iVehicleLinkPowerMaxMw);
      m_IndexTxPowers[iLink] = addMenuItem(m_pItemsSelect[40+iLink]);
      selectMenuItemTxPowersValue(m_pItemsSelect[40+iLink], false, bBoost2W, bBoost4W, &(iLinkPowersMw[0]), iCountLinkInterfaces, iVehicleLinkPowerMaxMw);

   }

   /*
   char szText[256];
   strcpy(szText, L("The Tx power is for the radio downlink(s).\nMaximum selectable Tx power is computed based on detected radio interfaces on the vehicle: "));
   char szBuff[256];
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

   m_pItemsSelect[5] = new MenuItemSelect(L("Show Tx Power in OSD"), L("Shows radio tx power in the current OSD screen."));
   m_pItemsSelect[5]->addSelection(L("No"));
   m_pItemsSelect[5]->addSelection(L("Yes"));
   m_pItemsSelect[5]->setIsEditable();
   m_IndexShowTxPower = addMenuItem(m_pItemsSelect[5]);

   m_pItemsSelect[5]->setSelectedIndex(0);
   if ( g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_FLAG2_SHOW_TX_POWER )
      m_pItemsSelect[5]->setSelectedIndex(1);
}

void MenuVehicleRadioConfig::onShow()
{
   log_line("MenuVehicleRadio: onShow");

   PopupRadioInterface* pTmpPopup = m_pPopupRadioInterface;
   m_bDisablePopupCreation = true;
   int iTmp = getSelectedMenuItemIndex();

   valuesToUI();
   Menu::onShow();
   invalidate();

   m_SelectedIndex = iTmp;
   if ( m_SelectedIndex < 0 )
      m_SelectedIndex = 0;
   if ( m_SelectedIndex >= m_ItemsCount )
      m_SelectedIndex = m_ItemsCount-1;

   m_pPopupRadioInterface = pTmpPopup;
   m_bDisablePopupCreation = false;
}


void MenuVehicleRadioConfig::valuesToUI()
{
   populate();
}

void MenuVehicleRadioConfig::Render()
{
   RenderPrepare();
   
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yTop);
}

void MenuVehicleRadioConfig::onFocusedItemChanged()
{
   Menu::onFocusedItemChanged();
   log_line("MenuVehicleRadio: onFocusedItemChanged");
   if ( m_bDisablePopupCreation )
      return;
   bool bAnyRadioLinkFocused = false;
   for( int iLink=0; iLink<g_pCurrentModel->radioLinksParams.links_count; iLink++ )
   {
      if ( (m_IndexTxPowers[iLink] != -1) && (m_IndexTxPowers[iLink] == m_SelectedIndex) )
      {
         bAnyRadioLinkFocused = true;
         if ( m_iLastLinkAddedPopupFor != iLink )
         {
            if ( NULL != m_pPopupRadioInterface )
            {
               popups_remove(m_pPopupRadioInterface);
               log_line("MenuVehicleRadio: Removed popup for tx power");
            }
            log_line("MenuVehicleRadio: Added popup for tx power for radio link %d", iLink+1);
            m_iLastLinkAddedPopupFor = iLink;
            m_pPopupRadioInterface = new PopupRadioInterface(iLink);
            popups_add_bottom(m_pPopupRadioInterface);
         }
         break;
      }
   }
   if ( ! bAnyRadioLinkFocused )
   {
      if ( NULL != m_pPopupRadioInterface )
      {
         popups_remove(m_pPopupRadioInterface);
         log_line("MenuVehicleRadio: Removed popup for tx power");
      }
      m_pPopupRadioInterface = NULL;
      m_iLastLinkAddedPopupFor = -1;
   }
}

int MenuVehicleRadioConfig::onBack()
{
   if ( NULL != m_pPopupRadioInterface )
   {
      popups_remove(m_pPopupRadioInterface);
      m_pPopupRadioInterface = NULL;
      log_line("MenuVehicleRadio: Removed popup for tx power");
   }

   return Menu::onBack();
}

void MenuVehicleRadioConfig::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   valuesToUI();
}

void MenuVehicleRadioConfig::sendNewRadioLinkFrequency(int iVehicleLinkIndex, u32 uNewFreqKhz)
{
   if ( (iVehicleLinkIndex < 0) || (iVehicleLinkIndex >= MAX_RADIO_INTERFACES) )
      return;

   log_line("MenuVehicleRadio: Changing radio link %d frequency to %u khz (%s)", iVehicleLinkIndex+1, uNewFreqKhz, str_format_frequency(uNewFreqKhz));

   if ( ! is_vehicle_radio_link_used(g_pCurrentModel, &g_SM_RadioStats, iVehicleLinkIndex) )
   {
      if ( link_is_reconfiguring_radiolink() )
      {
         add_menu_to_stack(new MenuConfirmation("Configuration In Progress","Another radio link configuration change is in progress. Please wait.", 0, true));
         valuesToUI();
         return;
      }
      log_line("MenuVehicleRadio: User changed unused radio link %d frequency to %s", iVehicleLinkIndex+1, str_format_frequency(uNewFreqKhz));

      u32 param = uNewFreqKhz & 0xFFFFFF;
      param = param | (((u32)iVehicleLinkIndex)<<24);
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RADIO_LINK_FREQUENCY, param, NULL, 0) )
         valuesToUI();
      else
      {
         link_set_is_reconfiguring_radiolink(iVehicleLinkIndex, false, true, true);
         warnings_add_configuring_radio_link(iVehicleLinkIndex, "Changing frequency");
      }
      return;
   }

   type_radio_links_parameters newRadioLinkParams;
   memcpy((u8*)&newRadioLinkParams, (u8*)&(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));
   
   newRadioLinkParams.link_frequency_khz[iVehicleLinkIndex] = uNewFreqKhz;
   
   if ( 0 == memcmp((u8*)&newRadioLinkParams, (u8*)&(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters)) )
   {
      log_line("MenuVehicleRadio: No change in radio link frequency. Do not send command.");
      return;
   }

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
   PH.total_length = sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE + sizeof(type_radio_links_parameters);

   u8 buffer[1024];
   static int s_iMenuRadioTestNumberCount = 0;
   s_iMenuRadioTestNumberCount++;
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   buffer[sizeof(t_packet_header)] = PACKET_TYPE_TEST_RADIO_LINK_PROTOCOL_VERSION;
   buffer[sizeof(t_packet_header)+1] = PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE;
   buffer[sizeof(t_packet_header)+2] = (u8)iVehicleLinkIndex;
   buffer[sizeof(t_packet_header)+3] = (u8)s_iMenuRadioTestNumberCount;
   buffer[sizeof(t_packet_header)+4] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START;
   memcpy(buffer + sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE, &newRadioLinkParams, sizeof(type_radio_links_parameters));
   send_packet_to_router(buffer, PH.total_length);

   link_set_is_reconfiguring_radiolink(iVehicleLinkIndex);
   warnings_add_configuring_radio_link(iVehicleLinkIndex, "Changing Frequency");
}


void MenuVehicleRadioConfig::computeSendPowerToVehicle(int iVehicleLinkIndex)
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

   log_line("MenuVehicleRadio: Setting radio link %d tx power to %s", iVehicleLinkIndex+1, m_pItemsSelect[iIndexSelector]->getSelectionIndexText(iIndex));

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
      log_line("MenuVehicleRadio: Setting tx raw power for card %d from %d to: %d (%d mw)",
         i+1, g_pCurrentModel->radioInterfacesParams.interface_raw_power[i], uCommandParams[i+1], iCardNewPowerMw);
      if ( uCommandParams[i+1] != g_pCurrentModel->radioInterfacesParams.interface_raw_power[i] )
         bDifferent = true;
   }

   if ( ! bDifferent )
   {
      log_line("MenuVehicleRadio: No change");
      return;
   }

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_TX_POWERS, 0, uCommandParams, uCommandParams[0]+1) )
       valuesToUI();
}


void MenuVehicleRadioConfig::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_IndexDisableUplink == m_SelectedIndex )
   {
      u32 uFlags = g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags;
      if ( 0 == m_pItemsSelect[4]->getSelectedIndex() )
         uFlags &= ~(MODEL_RADIOLINKS_FLAGS_DOWNLINK_ONLY);
      if ( 1 == m_pItemsSelect[4]->getSelectedIndex() )
         uFlags |= MODEL_RADIOLINKS_FLAGS_DOWNLINK_ONLY;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RADIO_LINKS_FLAGS, uFlags, NULL, 0) )
         valuesToUI();

   }

   if ( m_IndexPrioritizeUplink == m_SelectedIndex )
   {
      u32 uFlags = g_pCurrentModel->uModelFlags;
      uFlags &= (~MODEL_FLAG_PRIORITIZE_UPLINK);
      if ( 1 == m_pItemsSelect[3]->getSelectedIndex() )
         uFlags |= MODEL_FLAG_PRIORITIZE_UPLINK;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_MODEL_FLAGS, uFlags, NULL, 0) )
         valuesToUI();
   }

   if ( (-1 != m_IndexEncryption) && (m_IndexEncryption == m_SelectedIndex) )
   {
      if ( ! m_bControllerHasKey )
      {
         MenuConfirmation* pMC = new MenuConfirmation(L("Missing Pass Code"), "You have not set a pass code on the controller. You need first to set a pass code on the controller from Menu->Controller->Encryption.", -1, true);
         pMC->m_yPos = 0.3;
         add_menu_to_stack(pMC);
         m_pItemsSelect[2]->setSelectedIndex(0);
         return;
      }
      u8 params[128];
      int len = 0;
      params[0] = MODEL_ENC_FLAGS_NONE; // flags
      params[1] = 0; // pass length

      if ( 0 == m_pItemsSelect[2]->getSelectedIndex() )
         params[0] = MODEL_ENC_FLAGS_NONE;
      if ( 1 == m_pItemsSelect[2]->getSelectedIndex() )
         params[0] = MODEL_ENC_FLAG_ENC_VIDEO;
      if ( 2 == m_pItemsSelect[2]->getSelectedIndex() )
         params[0] = MODEL_ENC_FLAG_ENC_DATA ;
      if ( 3 == m_pItemsSelect[2]->getSelectedIndex() )
         params[0] = MODEL_ENC_FLAG_ENC_VIDEO | MODEL_ENC_FLAG_ENC_DATA;
      if ( 4 == m_pItemsSelect[2]->getSelectedIndex() )
         params[0] = MODEL_ENC_FLAG_ENC_ALL;

      if ( 0 != m_pItemsSelect[2]->getSelectedIndex() )
      {
         char szFile[128];
         strcpy(szFile, FOLDER_CONFIG);
         strcat(szFile, FILE_CONFIG_ENCRYPTION_PASS);
         FILE* fd = fopen(szFile, "r");
         if ( NULL == fd )
            return;
         len = fread(&params[2], 1, 124, fd);
         fclose(fd);
      }
      params[1] = (int)len;
      log_line("Send e type: %d, len: %d", (int)params[0], (int)params[1]);

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_ENCRYPTION_PARAMS, 0, params, len+2) )
         valuesToUI();
      return;
   }

   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
      if ( (m_IndexTxPowers[i] != -1) && (m_IndexTxPowers[i] == m_SelectedIndex) )
      {
         computeSendPowerToVehicle(i);
         return;
      }
   }

   for( int n=0; n<g_pCurrentModel->radioLinksParams.links_count; n++ )
   if ( m_IndexFreq[n] == m_SelectedIndex )
   {
      int index = m_pItemsSelect[20+n]->getSelectedIndex();
      u32 freq = m_SupportedChannels[n][index];
      int band = getBand(freq);      
      if ( freq == g_pCurrentModel->radioLinksParams.link_frequency_khz[n] )
         return;

      if ( link_is_reconfiguring_radiolink() )
      {
         add_menu_to_stack(new MenuConfirmation(L("Configuration In Progress"), L("Another radio link configuration change is in progress. Please wait."), 0, true));
         valuesToUI();
         return;
      }

      u32 uVehicleRadioInterfacesFrequencies[MAX_RADIO_INTERFACES];
      int iVehicleRadioInterfacesCapab[MAX_RADIO_INTERFACES];
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      {
         uVehicleRadioInterfacesFrequencies[i] = g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[i];
         iVehicleRadioInterfacesCapab[i] = g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i];
      }
      int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(n);
      uVehicleRadioInterfacesFrequencies[iRadioInterfaceId] = freq;

      const char* szError = controller_validate_radio_settings( g_pCurrentModel, uVehicleRadioInterfacesFrequencies, iVehicleRadioInterfacesCapab, NULL, NULL, NULL);
      
         if ( NULL != szError && 0 != szError[0] )
         {
            log_line(szError);
            add_menu_to_stack(new MenuConfirmation(L("Invalid option"), szError, 0, true));
            valuesToUI();
            return;
         }

         bool supportedOnController = false;
         bool allSupported = true;
         for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
         {
            radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
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
            add_menu_to_stack(new MenuConfirmation("Invalid option",szBuff, 0, true));
            valuesToUI();
            return;
         }
         if ( (! allSupported) && (1 == g_pCurrentModel->radioLinksParams.links_count) )
         {
            char szBuff[256];
            sprintf(szBuff, "Not all radio interfaces on your controller support %s frequency. Some radio interfaces on the controller will not be used to communicate with this vehicle.", str_format_frequency(freq));
            add_menu_to_stack(new MenuConfirmation("Confirmation",szBuff, 0, true));
         }

      sendNewRadioLinkFrequency(n, freq);
   }

   if ( m_IndexRadioConfig == m_SelectedIndex )
   {
      MenuRadioConfig* pM = new MenuRadioConfig();
      pM->m_bGoToFirstRadioLinkOnShow = true;
      add_menu_to_stack(pM);
      return;
   }

   if ( m_IndexOptimizeLinks == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuNegociateRadio());
      return;
   }

   if ( (-1 != m_IndexShowTxPower) && (m_IndexShowTxPower == m_SelectedIndex) )
   {
      osd_parameters_t params;
      memcpy(&params, &(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));
      int iScreenIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;

      if ( 0 == m_pItemsSelect[5]->getSelectedIndex() )
         params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_TX_POWER;
      else
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_TX_POWER;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_OSD_PARAMS, 0, (u8*)&params, sizeof(osd_parameters_t)) )
         valuesToUI();
      return;
   }

   if ( m_IndexPitMode == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleRadioLinkPITModes());
      return;
   }

   if ( (-1 != m_IndexDevRuntimeInfo) && (m_IndexDevRuntimeInfo == m_SelectedIndex) )
   {
      add_menu_to_stack(new MenuVehicleRadioRuntimeCapabilities());
      return;    
   }
}