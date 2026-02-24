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
#include "menu_tx_raw_power.h"
#include "menu_objects.h"
#include "menu_item_text.h"
#include "menu_item_section.h"
#include "menu_item_legend.h"
#include "menu_confirmation.h"
#include "../../utils/utils_controller.h"
#include "../../base/tx_powers.h"

MenuTXRawPower::MenuTXRawPower()
:Menu(MENU_ID_TX_RAW_POWER, "Custom Radio Power Levels", NULL)
{
   m_Width = 0.42;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.18;
   m_bShowVehicleSide = true;
}

MenuTXRawPower::~MenuTXRawPower()
{
}

void MenuTXRawPower::onShow()
{
   int iTmp = getSelectedMenuItemIndex();

   if ( m_bShowVehicleSide )
      setTitle(L("Custom Radio Power Levels"));
   else
      setTitle(L("Controller's Radio Interfaces Tx Powers"));
   addItems();
   Menu::onShow();

   if ( iTmp > 0 )
      m_SelectedIndex = iTmp;
   if ( m_SelectedIndex >= m_ItemsCount )
      m_SelectedIndex = m_ItemsCount-1;
}

void MenuTXRawPower::valuesToUI()
{
   addItems();
}


void MenuTXRawPower::addItems()
{
   int iTmp = getSelectedMenuItemIndex();

   removeAllItems();

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      m_iIndexVehicleCards[i] = -1;
      m_iIndexVehicleCardsBoostMode[i] = -1;
      m_IndexControllerTxPowerRadioLinks[i] = -1;
      m_pItemSelectVehicleCards[i] = NULL;
      m_pItemSelectVehicleCardsBoostMode[i] = NULL;
      m_pItemSelectControllerCards[i] = NULL;
   }
   m_IndexControllerTxPowerMode = -1;
   m_IndexControllerTxPowerSingle = -1;

   if ( m_bShowVehicleSide )
      addItemsVehicle();
   addItemsController();

   if ( iTmp > 0 )
      m_SelectedIndex = iTmp;
   if ( m_SelectedIndex >= m_ItemsCount )
      m_SelectedIndex = m_ItemsCount-1;
}

void MenuTXRawPower::addItemsVehicle()
{
   addMenuItem(new MenuItemSection("Vehicle's Radio Interfaces Tx Powers"));

   if ( NULL == g_pCurrentModel )
   {
      addMenuItem(new MenuItemText("Not connected to any vehicle", true));
      return;
   }

   for( int iLink=0; iLink<g_pCurrentModel->radioLinksParams.links_count; iLink++ )
   {
      if ( g_pCurrentModel->radioLinksParams.links_count > 1 )
      {
         char szBuff[128];
         sprintf(szBuff, L("Radio link %d:"), iLink+1);
         addMenuItem(new MenuItemText(szBuff));
      }
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( ! g_pCurrentModel->radioInterfaceIsWiFiRadio(i) )
            continue;
         if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] != iLink )
            continue;
         int iCardModel = g_pCurrentModel->radioInterfacesParams.interface_card_model[i];
         int iPowerRaw = g_pCurrentModel->radioInterfacesParams.interface_raw_power[i];

         m_pItemSelectVehicleCards[i] = createItemCard(true, g_pCurrentModel->hwCapabilities.uBoardType, g_pCurrentModel->radioLinksParams.links_count, iLink, i, iCardModel, iPowerRaw);
         if ( NULL != m_pItemSelectVehicleCards[i] )
            m_iIndexVehicleCards[i] = addMenuItem(m_pItemSelectVehicleCards[i]);

         char szBuff[128];
         sprintf(szBuff, L("Radio Interface %d Output Boost Mode"), i+1);
         m_pItemSelectVehicleCardsBoostMode[i] = new MenuItemSelect(szBuff, L("If this radio interface has an output RF power booster connected to it, select the one you have."));
         m_pItemSelectVehicleCardsBoostMode[i]->addSelection(L("None"));
         m_pItemSelectVehicleCardsBoostMode[i]->addSelection(L("2W Booster"));
         m_pItemSelectVehicleCardsBoostMode[i]->addSelection(L("4W Booster"));
         m_pItemSelectVehicleCardsBoostMode[i]->setIsEditable();

         m_pItemSelectVehicleCardsBoostMode[i]->setSelectedIndex(0);
         if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_2W )
            m_pItemSelectVehicleCardsBoostMode[i]->setSelectedIndex(1);
         if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_4W )
            m_pItemSelectVehicleCardsBoostMode[i]->setSelectedIndex(2);

         m_iIndexVehicleCardsBoostMode[i] = addMenuItem(m_pItemSelectVehicleCardsBoostMode[i]);
      }
   }
}

void MenuTXRawPower::addItemsController()
{
   ControllerSettings* pCS = get_ControllerSettings();
   if ( m_bShowVehicleSide )
      addMenuItem(new MenuItemSection(L("Controller's Radio Interfaces Tx Powers")));

   m_pItemsSelect[0] = new MenuItemSelect(L("Controller Tx Power Mode"), L("Sets the radio transmission power mode selector for the controller side: custom fixed power or auto computed for each radio link based on vechile radio links tx powers."));
   m_pItemsSelect[0]->addSelection(L("Fixed uplink Tx power"));
   m_pItemsSelect[0]->addSelection(L("Match current vehicle"));
   m_pItemsSelect[0]->setIsEditable();
   m_pItemsSelect[0]->setSelectedIndex(1-pCS->iFixedTxPower);
   m_pItemsSelect[0]->setExtraHeight(0.2*g_pRenderEngine->textHeight(g_idFontMenu));
   m_IndexControllerTxPowerMode = addMenuItem(m_pItemsSelect[0]);

   if ( !pCS->iFixedTxPower )
   {
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         m_IndexControllerTxPowerRadioLinks[i] = -1;

      MenuItemLegend* pLegend = new MenuItemLegend(L("Note"), L("Controller is currently set to Auto Tx power mode for the uplink. Set it to Fixed power mode in the Controller menu if you want to be able to customize the uplink tx power."), 0);
      addMenuItem(pLegend);

      char szText[256];
      for( int iLink=0; iLink<g_pCurrentModel->radioLinksParams.links_count; iLink++ )
      {
         char szTitle[64];
         strcpy(szTitle, L("Radio Uplink"));
         if ( g_pCurrentModel->radioLinksParams.links_count > 1 )
            sprintf(szTitle, L("Radio Uplink %d"), iLink+1);

         int iCountInterfacesForLink = 0;
         for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
         {
            if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iLink )
               continue;
            if ( ! hardware_radio_index_is_wifi_radio(i) )
               continue;
            radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
            if ( (! pRadioHWInfo->isConfigurable) || (! pRadioHWInfo->isSupported) )
               continue;

            t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
            if ( NULL == pCRII )
               continue;
            iCountInterfacesForLink++;
         }

         szText[0] = 0;
         for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
         {
            if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iLink )
               continue;
            if ( ! hardware_radio_index_is_wifi_radio(i) )
               continue;
            radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
            if ( (! pRadioHWInfo->isConfigurable) || (! pRadioHWInfo->isSupported) )
               continue;

            t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
            if ( NULL == pCRII )
               continue;

            int iCardModel = pCRII->cardModel;
            int iCardPowerMwNow = tx_powers_convert_raw_to_mw(hardware_getBoardType(), iCardModel, pCRII->iRawPowerLevel);
            char szBuff[128];
            if ( iCountInterfacesForLink < 2 )
            {
               if ( iCardPowerMwNow >= 1000 )
                  sprintf(szBuff, "Radio Interface %s: %.1f W", str_get_radio_card_model_string_short(pRadioHWInfo->iCardModel), (float)iCardPowerMwNow/1000.0);
               else
                  sprintf(szBuff, "Radio Interface %s: %d mW", str_get_radio_card_model_string_short(pRadioHWInfo->iCardModel), iCardPowerMwNow);
            }
            else
            {
               if ( iCardPowerMwNow >= 1000 )
                  sprintf(szBuff, "Radio Interface %d (%s): %.1f W", i+1, str_get_radio_card_model_string_short(pRadioHWInfo->iCardModel), (float)iCardPowerMwNow/1000.0);
               else
                  sprintf(szBuff, "Radio Interface %d (%s): %d mW", i+1, str_get_radio_card_model_string_short(pRadioHWInfo->iCardModel), iCardPowerMwNow);
            }
            if ( 0 != szText[0] )
               strcat(szText, "\n");
            strcat(szText, szBuff);
         }

         pLegend = new MenuItemLegend(szTitle, szText, 0);
         addMenuItem(pLegend);
      }
      return;
   }

   // Fixed powers

   int iCountRadioLinks = 1;
   if ( m_bShowVehicleSide && (NULL != g_pCurrentModel) )
      iCountRadioLinks = g_pCurrentModel->radioLinksParams.links_count;

   for( int iLink=0; iLink<iCountRadioLinks; iLink++ )
   {
      if ( iCountRadioLinks > 1 )
      {
         char szBuff[128];
         sprintf(szBuff, L("Radio link %d:"), iLink+1);
         addMenuItem(new MenuItemText(szBuff));
      }
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( ! hardware_radio_index_is_wifi_radio(i) )
            continue;
         if ( hardware_radio_index_is_sik_radio(i) )
            continue;
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( (! pRadioHWInfo->isConfigurable) || (! pRadioHWInfo->isSupported) )
            continue;

         t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
         if ( NULL == pCRII )
            continue;

         m_pItemSelectControllerCards[i] = createItemCard(false, hardware_getBoardType(), iCountRadioLinks, iLink, i, pCRII->cardModel, pCRII->iRawPowerLevel);
         if ( NULL != m_pItemSelectControllerCards[i] )
            m_IndexControllerTxPowerRadioLinks[i] = addMenuItem(m_pItemSelectControllerCards[i]);
      }
   }
}

MenuItemSelect* MenuTXRawPower::createItemCard(bool bVehicle, u32 uBoardType, int iCountLinks, int iRadioLinkIndex, int iCardIndex, int iCardModel, int iRawPower)
{
   char szText[128];
   char szTooltip[256];
   sprintf(szText, "Radio Interface %d (%s)", iCardIndex+1, str_get_radio_card_model_string(iCardModel));
   sprintf(szTooltip, "Adjust the output tx power of %s's radio interface %d", bVehicle?"vehicle":"controller", iCardIndex+1);
   MenuItemSelect* pItemSelect = new MenuItemSelect(szText, szTooltip);

   int iMaxRawPowerForCard = tx_powers_get_max_usable_power_raw_for_card(uBoardType, iCardModel);
   int iIndexSelected = -1;
   int iMinRawDiff = 10000;

   int iRawValuesCount = 0;
   const int* piRawValues = tx_powers_get_raw_radio_power_values(&iRawValuesCount);

   for( int i=0; i<iRawValuesCount; i++ )
   {
      if ( piRawValues[i] > iMaxRawPowerForCard )
         sprintf(szText, "%d -> !!!", piRawValues[i]);
      else
      {
         int iMwValue = tx_powers_convert_raw_to_mw(uBoardType, iCardModel, piRawValues[i]);
         sprintf(szText, "%d -> %d mW", piRawValues[i], iMwValue);

         if ( bVehicle && (NULL != g_pCurrentModel) )
         {
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iCardIndex] & RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_4W )
            {
               int iMwValueB = tx_powers_get_mw_boosted_value_from_mw(iMwValue,0,1);
               if ( iMwValue > MAX_BOOST_INPUT_SIGNAL )
                  sprintf(szText, "%d -> %d mW -> !!!", piRawValues[i], iMwValue);
               else if ( iMwValue < MIN_BOOST_INPUT_SIGNAL )
                  sprintf(szText, "%d -> %d mW -> 0!", piRawValues[i], iMwValue);
               else
               {
                  if ( iMwValueB >= 1000 )
                     sprintf(szText, "%d -> %d mW -> %.1f W", piRawValues[i], iMwValue, ((float)iMwValueB)/1000.0);
                  else
                     sprintf(szText, "%d -> %d mW -> %d mW", piRawValues[i], iMwValue, iMwValueB);
               }
            }
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iCardIndex] & RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_2W )
            {
               int iMwValueB = tx_powers_get_mw_boosted_value_from_mw(iMwValue,1,0);
               if ( iMwValue > MAX_BOOST_INPUT_SIGNAL )
                  sprintf(szText, "%d -> %d mW -> !!!", piRawValues[i], iMwValue);
               else if ( iMwValue < MIN_BOOST_INPUT_SIGNAL )
                  sprintf(szText, "%d -> %d mW -> 0!", piRawValues[i], iMwValue);
               else
               {
                  if ( iMwValueB >= 1000 )
                     sprintf(szText, "%d -> %d mW -> %.1f W", piRawValues[i], iMwValue, ((float)iMwValueB)/1000.0);
                  else
                     sprintf(szText, "%d -> %d mW -> %d mW", piRawValues[i], iMwValue, iMwValueB);
               }
            }
         }
      }
      pItemSelect->addSelection(szText);
      int iDiff = iRawPower - piRawValues[i];
      if ( iDiff < 0 )
         iDiff = - iDiff;

      if ( iDiff < iMinRawDiff )
      {
         iMinRawDiff = iDiff;
         iIndexSelected = pItemSelect->getSelectionsCount()-1;
      }
   }

   pItemSelect->setIsEditable();
   if ( -1 != iIndexSelected )
      pItemSelect->setSelectedIndex(iIndexSelected);
   return pItemSelect;
}

void MenuTXRawPower::computeSendPowerToVehicle(int iCardIndex)
{
   if ( (iCardIndex < 0) || (iCardIndex >= g_pCurrentModel->radioInterfacesParams.interfaces_count) )
   {
      addMessage("Invalid card.");
      return;
   }
   int iRawValuesCount = 0;
   const int* piRawValues = tx_powers_get_raw_radio_power_values(&iRawValuesCount);

   int iPowerIndex = m_pItemSelectVehicleCards[iCardIndex]->getSelectedIndex();
   if ( (iPowerIndex < 0) || (iPowerIndex >= iRawValuesCount) )
   {
      addMessage("Invalid value");
      return;
   }
   int iRawPowerToSet = piRawValues[iPowerIndex];
   log_line("MenuTXRawPower: Setting raw tx power for card %d to: %d", iCardIndex+1, iRawPowerToSet);

   u8 uCommandParams[MAX_RADIO_INTERFACES+1];
   uCommandParams[0] = g_pCurrentModel->radioInterfacesParams.interfaces_count;

   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      uCommandParams[i+1] = g_pCurrentModel->radioInterfacesParams.interface_raw_power[i];
      if ( i == iCardIndex )
         uCommandParams[i+1] = iRawPowerToSet;

      if ( ! hardware_radio_index_is_wifi_radio(i) )
         continue;
      if ( hardware_radio_index_is_sik_radio(i) )
         continue;

      int iCardModel = g_pCurrentModel->radioInterfacesParams.interface_card_model[i];
      int iCardMaxPowerMw = tx_powers_get_max_usable_power_mw_for_card(g_pCurrentModel->hwCapabilities.uBoardType, iCardModel);
      int iCardPowerMw = tx_powers_convert_raw_to_mw(g_pCurrentModel->hwCapabilities.uBoardType, iCardModel, uCommandParams[i+1]);

      log_line("MenuTXRawPower: Setting tx raw power for card %d from %d to: %d (%d mw, max is %d mw)",
         i+1, g_pCurrentModel->radioInterfacesParams.interface_raw_power[i], uCommandParams[i+1], iCardPowerMw, iCardMaxPowerMw);
   }

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_TX_POWERS, 0, uCommandParams, uCommandParams[0]+1) )
       valuesToUI();
}

void MenuTXRawPower::computeApplyControllerPower(int iCardIndex)
{
   if ( (iCardIndex < 0) || (iCardIndex >= hardware_get_radio_interfaces_count()) )
   {
      addMessage("Invalid card.");
      return;
   }

   int iRawValuesCount = 0;
   const int* piRawValues = tx_powers_get_raw_radio_power_values(&iRawValuesCount);

   int iPowerIndex = m_pItemSelectControllerCards[iCardIndex]->getSelectedIndex();
   if ( (iPowerIndex < 0) || (iPowerIndex >= iRawValuesCount) )
   {
      addMessage("Invalid value");
      return;
   }
   int iRawPowerToSet = piRawValues[iPowerIndex];

   if ( ! hardware_radio_index_is_wifi_radio(iCardIndex) )
      return;

   if ( hardware_radio_index_is_sik_radio(iCardIndex) )
      return;

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iCardIndex);
   if ( (! pRadioHWInfo->isConfigurable) || (! pRadioHWInfo->isSupported) )
      return;

   t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
   if ( NULL == pCRII )
      return;

   pCRII->iRawPowerLevel = iRawPowerToSet;

   save_ControllerInterfacesSettings();
   send_model_changed_message_to_router(MODEL_CHANGED_RADIO_POWERS, 0);
   valuesToUI();
}

void MenuTXRawPower::Render()
{
   RenderPrepare();

   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yTop);
}

int MenuTXRawPower::onBack()
{
   return Menu::onBack();
}

void MenuTXRawPower::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);
   valuesToUI();
}

void MenuTXRawPower::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( (-1 != m_IndexControllerTxPowerMode) && (m_IndexControllerTxPowerMode == m_SelectedIndex) )
   {
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->iFixedTxPower = 1 - m_pItemsSelect[0]->getSelectedIndex();
      save_ControllerSettings();
      send_model_changed_message_to_router(MODEL_CHANGED_RADIO_POWERS, 0);
      valuesToUI();
      return;
   }

   if ( m_bShowVehicleSide )
   {
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( (m_iIndexVehicleCards[i] != -1) && (m_iIndexVehicleCards[i] == m_SelectedIndex) )
         if ( NULL != m_pItemSelectVehicleCards[i] )
         {
            computeSendPowerToVehicle(i);
            return;
         }

         if ( (m_iIndexVehicleCardsBoostMode[i] != -1) && (m_iIndexVehicleCardsBoostMode[i] == m_SelectedIndex) )
         if ( NULL != m_pItemSelectVehicleCardsBoostMode[i] )
         {
            u32 uNewFlags = g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i];
            uNewFlags &= ~RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_2W;
            uNewFlags &= ~RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_4W;
            if ( 1 == m_pItemSelectVehicleCardsBoostMode[i]->getSelectedIndex() )
               uNewFlags |= RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_2W;
            if ( 2 == m_pItemSelectVehicleCardsBoostMode[i]->getSelectedIndex() )
               uNewFlags |= RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_4W;

            if ( uNewFlags == g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] )
               return;

            u32 uParam = i;
            uParam = uParam | ((uNewFlags << 8) & 0xFFFFFF00);
            if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RADIO_INTERFACE_CAPABILITIES, uParam, NULL, 0) )
               valuesToUI();
            return;
         }
      }
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( (m_IndexControllerTxPowerRadioLinks[i] != -1) && (m_IndexControllerTxPowerRadioLinks[i] == m_SelectedIndex) )
      if ( NULL != m_pItemSelectControllerCards[i] )
      {
         computeApplyControllerPower(i);
         return;
      }
   }
}
