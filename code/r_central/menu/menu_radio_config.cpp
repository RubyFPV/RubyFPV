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

#include "../render_commands.h"
#include "../../utils/utils_controller.h"
#include "../../utils/utils_vehicle.h"
#include "../../base/tx_powers.h"
#include "osd.h"
#include "osd_common.h"
#include "../popup.h"
#include "menu.h"
#include "menu_radio_config.h"
#include "menu_confirmation.h"
#include "menu_item_text.h"
#include "menu_controller_radio_interface.h"
#include "menu_controller_radio_interface_sik.h"
#include "menu_vehicle_radio_link.h"
#include "menu_vehicle_radio_link_sik.h"
#include "menu_vehicle_radio_interface.h"
#include "menu_diagnose_radio_link.h"

#include <ctype.h>
#include <math.h>
#include "../process_router_messages.h"
#include "../link_watch.h"
#include "../pairing.h"

#define MRC_ID_SET_PREFFERED_TX 1


#define MRC_ID_CONFIGURE_RADIO_LINK 30
#define MRC_ID_CONFIGURE_RADIO_INTERFACE_CONTROLLER 31
#define MRC_ID_CONFIGURE_RADIO_INTERFACE_VEHICLE 32
#define MRC_ID_SWITCH_RADIO_LINK 34
#define MRC_ID_SWAP_VEHICLE_RADIO_INTERFACES 35
#define MRC_ID_ROTATE_RADIO_LINKS 36
#define MRC_ID_DISABLE_UPLINKS 37
#define MRC_ID_DIAGNOSE_RADIO_LINK 40

MenuRadioConfig::MenuRadioConfig(void)
:Menu(MENU_ID_RADIO_CONFIG, "Radio Configuration", NULL)
{
   m_Width = 0.17;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.16;
   m_bDisableStacking = true;
   m_bDisableBackgroundAlpha = true;
   m_fHeaderHeight = 0.0;
   m_fTotalHeightRadioConfig = 0.0;
   m_bComputedHeights = false;
   m_bGoToFirstRadioLinkOnShow = false;
   m_bHasSwapInterfacesCommand = false;
   m_bHasRotateRadioLinksOrderCommand = false;
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      m_bHasSwitchRadioLinkCommand[i] = false;
      m_bHasDiagnoseRadioLinkCommand[i] = false;
   }
   m_szCurrentTooltip[0] = 0;
   for( int i=0; i<50; i++ )
      m_szTooltips[i][0] = 0;

   m_bShowOnlyControllerUnusedInterfaces = false;
   m_pItemSelectTxCard = NULL;
   m_iTxCardSelectedIndex = -1;
   m_iIndexCurrentItem = 0;

   m_iIdFontRegular = g_idFontMenu;
   m_iIdFontSmall = g_idFontMenuSmall;
   m_iIdFontLarge = g_idFontMenuLarge;
}

MenuRadioConfig::~MenuRadioConfig()
{
}

void MenuRadioConfig::onShow()
{
   int iTmp = getSelectedMenuItemIndex();
   int iTmpCurItem = m_iIndexCurrentItem;

   m_Height = 0.0;
   m_bComputedHeights = false;
   log_line("MenuRadioConfig: Entering menu radio config onShow event...");
   removeAllItems();
      
   m_fFooterHeight = 1.0 * g_pRenderEngine->textHeight(m_iIdFontRegular) + m_sfMenuPaddingY;
   m_szCurrentTooltip[0] = 0;

   m_bShowOnlyControllerUnusedInterfaces = false;

   if ( (0 == getControllerModelsCount()) || (NULL == g_pCurrentModel) || (! g_bFirstModelPairingDone) )
      m_bShowOnlyControllerUnusedInterfaces = true;

   m_iCurrentRadioLink = -1;
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      m_fYPosAutoTx[i] = 0.5;
      m_fYPosCtrlRadioInterfaces[i] = 0.2;
      m_fYPosVehicleRadioInterfaces[i] = 0.2;
      m_fYPosRadioLinks[i] = 0.2;
   }

   m_uBandsSiKVehicle = 0;
   m_uBandsSiKController = 0;

   m_bTmpHasVideoStreamsEnabled = false;
   m_bTmpHasDataStreamsEnabled = false;

   m_iCountVehicleRadioLinks = 0;
   if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone && (!m_bShowOnlyControllerUnusedInterfaces) )
      m_iCountVehicleRadioLinks = g_pCurrentModel->radioLinksParams.links_count;

   log_line("MenuRadioConfig: Vehicle has %d radio links.", m_iCountVehicleRadioLinks);

   computeMenuItems();

   if ( m_iIndexCurrentItem >= m_iIndexMaxItem )
      m_iIndexCurrentItem = m_iIndexMaxItem-1;
     
   if ( (0 < g_SM_RadioStats.countVehicleRadioLinks) || (0 < g_SM_RadioStats.countLocalRadioLinks) )
      computeHeights();

   setTooltipText();

   Menu::onShow();

   if ( m_bGoToFirstRadioLinkOnShow )
   {
      m_iIndexCurrentItem = m_iHeaderItemsCount;
      m_bGoToFirstRadioLinkOnShow = false;
      setTooltipText();
   }

   if ( ! (menu_is_menu_on_top(this)) )
   if ( ! (menu_has_menu(MENU_ID_TX_RAW_POWER)) )
   {
      Menu* pTopMenu = menu_get_top_menu();
      if ( NULL != pTopMenu )
         log_line("MenuRadioConfig: Top menu is: (%s), menu id: %d", pTopMenu->m_szTitle, pTopMenu->m_MenuId);

      // Do not close radio interface card model autodetect confirmation message or set video bitrate confirmation message from vehicle radio link
      bool bDoNotClose = false;
      if ( (NULL != pTopMenu) && (pTopMenu->getId() == (MENU_ID_SIMPLE_MESSAGE + 34*1000)) )
         bDoNotClose = true;
      if ( (NULL != pTopMenu) && (pTopMenu->getId() == (MENU_ID_SIMPLE_MESSAGE + 12*1000)) )
         bDoNotClose = true;
      if ( (NULL != pTopMenu) && (pTopMenu->getId() == (MENU_ID_SIMPLE_MESSAGE + 11*1000)) )
         bDoNotClose = true;
      if ( (NULL != pTopMenu) && (pTopMenu->getId() == MENU_ID_VEHICLE_RADIO_INTERFACE) )
         bDoNotClose = true;
 
      if ( ! bDoNotClose )
      {
         log_line("MenuRadioConfig: Is not on top, close the top menu.");
         menu_stack_pop(0);
      }
   }

   m_SelectedIndex = iTmp;
   if ( m_SelectedIndex < 0 )
      m_SelectedIndex = 0;
   if ( m_SelectedIndex >= m_ItemsCount )
      m_SelectedIndex = m_ItemsCount-1;

   m_iIndexCurrentItem = iTmpCurItem;
   if ( m_iIndexCurrentItem < 0 )
      m_iIndexCurrentItem =0;
   if ( m_iIndexCurrentItem >= m_iIndexMaxItem )
      m_iIndexCurrentItem = 0;

   log_line("MenuRadioConfig: Done handling onShow event.");
}

void MenuRadioConfig::setTooltip(int iItemIndex, const char* szTooltip)
{
   if ( iItemIndex < 0 || iItemIndex >= 50 )
      return;
   if ( NULL == szTooltip )
      m_szTooltips[iItemIndex][0] = 0;
   else
      strcpy(m_szTooltips[iItemIndex], szTooltip);
}

void MenuRadioConfig::setTooltipText()
{
   if ( m_iIndexCurrentItem >= 0 && m_iIndexCurrentItem < 50 )
      strcpy(m_szCurrentTooltip, m_szTooltips[m_iIndexCurrentItem]);
   else
      m_szCurrentTooltip[0] = 0;
}

void MenuRadioConfig::computeMenuItems()
{
   log_line("MenuRadioConfig: Compute Menu Items...");
   
   if ( NULL != m_pItemSelectTxCard )
      removeMenuItem(m_pItemSelectTxCard);

   m_pItemSelectTxCard = NULL;
   m_iTxCardSelectedIndex = -1;

   m_bHasSwapInterfacesCommand = false;
   m_bHasRotateRadioLinksOrderCommand = false;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      m_bHasSwitchRadioLinkCommand[i] = false;
      m_bHasDiagnoseRadioLinkCommand[i] = false;
   }

   if ( g_pCurrentModel->radioLinksParams.links_count > 1 )
      m_bHasRotateRadioLinksOrderCommand = true;

   m_iHeaderItemsCount = 0;

   // Check controller interfaces

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      log_line("MenuRadioConfig: Detected controller radio interface %d type: %s", i+1, str_get_radio_type_description(pRadioHWInfo->iRadioType));
      if ( hardware_radio_index_is_sik_radio(i) )
      {
         if ( NULL != pRadioHWInfo )
            m_uBandsSiKController |= pRadioHWInfo->supportedBands;
      }
   }

   if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone && (!m_bShowOnlyControllerUnusedInterfaces) )
   {
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         log_line("MenuRadioConfig: Detected vehicle radio interface %d type: %s", i+1, str_get_radio_type_description(g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[i]));

         if ( (g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[i] & 0xFF) == RADIO_TYPE_SIK )
            m_uBandsSiKVehicle |= g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i];
      }
   }

   // Check if swap vehicle radio interfaces is possible
   ControllerSettings* pCS = get_ControllerSettings();
   m_bHasSwapInterfacesCommand = false;
   if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone && (!m_bShowOnlyControllerUnusedInterfaces) )  
   if ( pCS->iDeveloperMode )
   if ( g_pCurrentModel->canSwapEnabledHighCapacityRadioInterfaces() )
       m_bHasSwapInterfacesCommand = true;

   // Check to see to which radio links to add the swhich radio link commands
   
   if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone )
   for( int iVehicleRadioLink=0; iVehicleRadioLink<m_iCountVehicleRadioLinks; iVehicleRadioLink++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         continue;
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;
      if ( (! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO)) &&
           (! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA)) )
         continue;

      int iCountInterfacesAssignedToThisLink = 0;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
            continue;

         t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
         if ( NULL == pCardInfo )
            continue;

         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId == iVehicleRadioLink )
            iCountInterfacesAssignedToThisLink++;
      }

      int iCountInterfacesAssignableToThisLink = controller_count_asignable_radio_interfaces_to_vehicle_radio_link(g_pCurrentModel, iVehicleRadioLink);
      if ( (0 == iCountInterfacesAssignedToThisLink) && (0 < iCountInterfacesAssignableToThisLink) )
      {
         m_bHasSwitchRadioLinkCommand[iVehicleRadioLink] = true;
         log_line("MenuRadioConfig: Radio link %d (of %d) has switch command.", iVehicleRadioLink+1, m_iCountVehicleRadioLinks);
      }
   }

   for( int iVehicleRadioLink=0; iVehicleRadioLink<m_iCountVehicleRadioLinks; iVehicleRadioLink++ )
   {
      m_bHasDiagnoseRadioLinkCommand[iVehicleRadioLink] = false;
      //if ( g_pCurrentModel->radioLinkIsSiKRadio(iVehicleRadioLink) )
      //   m_bHasDiagnoseRadioLinkCommand[iVehicleRadioLink] = true;
   }
   computeMaxItemIndexAndCommands();
}

void MenuRadioConfig::Render()
{
   float height_text = g_pRenderEngine->textHeight(m_iIdFontRegular);
   bool bbSameStroke = g_pRenderEngine->getFontBackgroundBoundingBoxSameTextColor();
   g_pRenderEngine->setFontBackgroundBoundingBoxFillColor(get_Color_MenuItemSelectedBg());
   g_pRenderEngine->setFontBackgroundBoundingBoxStrikeColor(get_Color_MenuItemSelectedText());
   g_pRenderEngine->setBackgroundBoundingBoxPadding(0.007);
   g_pRenderEngine->setFontBackgroundBoundingBoxSameTextColor(true);

   if ( ! m_bComputedHeights )
   {
      if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone && (!m_bShowOnlyControllerUnusedInterfaces) )
         m_iCountVehicleRadioLinks = g_pCurrentModel->radioLinksParams.links_count;

      computeHeights();
      computeMaxItemIndexAndCommands();
   }

   float xStart = 0.05;
   float xEnd = 0.72;
   float fWidth = xEnd - xStart;

   float yStart = 0.04;
   if ( m_fTotalHeightRadioConfig < 0.8 )
      yStart += 0.1;

   g_pRenderEngine->setColors(get_Color_MenuBg());
   g_pRenderEngine->drawRoundRect(xStart, yStart, fWidth, m_fTotalHeightRadioConfig, MENU_ROUND_MARGIN*m_sfMenuPaddingY);
   
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   g_pRenderEngine->setFill(0,0,0,0);
   g_pRenderEngine->setStroke(get_Color_MenuBorder());
   g_pRenderEngine->drawRoundRect(xStart, yStart, fWidth, m_fTotalHeightRadioConfig, MENU_ROUND_MARGIN*m_sfMenuPaddingY);
   g_pRenderEngine->setColors(get_Color_MenuText());

   if ( 1 )
   {
      g_pRenderEngine->setColors(get_Color_MenuBgTooltip());
      float yTooltip = yStart + m_fTotalHeightRadioConfig - m_fFooterHeight;
      g_pRenderEngine->drawRoundRect(xStart, yTooltip, fWidth, m_fFooterHeight, MENU_ROUND_MARGIN*m_sfMenuPaddingY);

      g_pRenderEngine->setColors(get_Color_MenuBg());
      g_pRenderEngine->setStroke(get_Color_MenuBorder());
      g_pRenderEngine->drawLine(xStart, yTooltip, xStart+fWidth, yTooltip);

      yTooltip += 0.4*m_sfMenuPaddingY;

      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->drawMessageLines(xStart+m_sfMenuPaddingX, yTooltip, m_szCurrentTooltip, MENU_TEXTLINE_SPACING, fWidth-2.0*m_sfMenuPaddingX, m_iIdFontRegular);
   }

   if ( m_iIndexCurrentItem < 0 )
      m_iIndexCurrentItem = 0;

   yStart += m_sfMenuPaddingY;
   m_fHeaderHeight = drawRadioHeader(xStart, xEnd, yStart);
   yStart += m_fHeaderHeight;
   computeHeights();

   // Draw radio links

   float yEnd = drawRadioLinks(xStart, xEnd, yStart);
   g_pRenderEngine->clearFontBackgroundBoundingBoxStrikeColor();

   
   if ( NULL != m_pItemSelectTxCard )
   {
      float xPos = m_RenderXPos + g_pRenderEngine->textWidth(m_iIdFontRegular, "TxCard");

      m_pItemSelectTxCard->setLastRenderPos(xPos, m_fYPosAutoTx[m_iCurrentRadioLink]-height_text);
      m_pItemSelectTxCard->getItemHeight(0.5);
      m_pItemSelectTxCard->Render(xPos, m_fYPosAutoTx[m_iCurrentRadioLink]-height_text, true, 0.0);
   }

   // Draw bottom items

   float xLeft = m_RenderXPos + m_RenderWidth - 2.0*m_sfMenuPaddingX;
   yEnd -= height_text;
   if ( m_bHasRotateRadioLinksOrderCommand )
   {
      bool bBBox = false;
      if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_ROTATE_RADIO_LINKS )
            bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
      
      float fWidthText = g_pRenderEngine->textWidth(m_iIdFontRegular, "Rotate Radio Links Order");
      g_pRenderEngine->drawTextLeft(xLeft, yEnd, m_iIdFontRegular, "Rotate Radio Links Order");

      if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_ROTATE_RADIO_LINKS )
         g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

      xLeft -= fWidthText + 2.0 * m_sfMenuPaddingX;
   }

   if ( m_bHasSwapInterfacesCommand )
   {
      bool bBBox = false;
      if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SWAP_VEHICLE_RADIO_INTERFACES )
            bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
      
      float fWidthText = g_pRenderEngine->textWidth(m_iIdFontRegular, "Swap Vehicle's Radio Interfaces");
      g_pRenderEngine->drawTextLeft(xLeft, yEnd, m_iIdFontRegular, "Swap Vehicle's Radio Interfaces");

      if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SWAP_VEHICLE_RADIO_INTERFACES )
         g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

      xLeft -= fWidthText + 2.0 * m_sfMenuPaddingX;
   }
   g_pRenderEngine->setFontBackgroundBoundingBoxSameTextColor(bbSameStroke);
}


void MenuRadioConfig::showProgressInfo()
{
   ruby_pause_watchdog("controller update radio config progress");
   m_pPopupProgress = new Popup("Updating Radio Configuration. Please wait...",0.3,0.4, 0.5, 15);
   popups_add_topmost(m_pPopupProgress);

   g_pRenderEngine->startFrame();
   render_background_and_paddings(false);
   popups_render();
   popups_render_topmost();
   g_pRenderEngine->endFrame();
}

void MenuRadioConfig::hideProgressInfo()
{
   popups_remove(m_pPopupProgress);
   m_pPopupProgress = NULL;
}


void MenuRadioConfig::onMoveUp(bool bIgnoreReversion)
{
   if ( g_bSwitchingRadioLink )
      return;
   if ( NULL != m_pItemSelectTxCard )
   {
      Menu::onMoveUp(bIgnoreReversion);
      setTooltipText();
      return;
   }

   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      setTooltipText();
      return;
   }

   m_iIndexCurrentItem--;
   if ( m_iIndexCurrentItem < 0 )
      m_iIndexCurrentItem = m_iIndexMaxItem-1;

   setTooltipText();
}

void MenuRadioConfig::onMoveDown(bool bIgnoreReversion)
{
   if ( g_bSwitchingRadioLink )
      return;
   if ( NULL != m_pItemSelectTxCard )
   {
      Menu::onMoveDown(bIgnoreReversion);
      setTooltipText();
      return;
   }

   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      setTooltipText();
      return;
   }
   m_iIndexCurrentItem++;
   if ( m_iIndexCurrentItem >= m_iIndexMaxItem )
      m_iIndexCurrentItem = 0;

   setTooltipText();
}

void MenuRadioConfig::onMoveLeft(bool bIgnoreReversion)
{
   if ( g_bSwitchingRadioLink )
      return;
   onMoveUp(bIgnoreReversion);
}

void MenuRadioConfig::onMoveRight(bool bIgnoreReversion)
{
   if ( g_bSwitchingRadioLink )
      return;
   onMoveDown(bIgnoreReversion);
}

void MenuRadioConfig::onFocusedItemChanged()
{
   setTooltipText();
}

void MenuRadioConfig::onItemValueChanged(int itemIndex)
{
}

void MenuRadioConfig::onSelectedPreferredTxCard(int iVehicleRadioLink)
{
   int iSelection = m_pItemSelectTxCard->getSelectedIndex();
   log_line("MenuRadioConfig: Changed auto tx card selection index for vehicle radio link %d from %d to %d", iVehicleRadioLink+1, m_iTxCardSelectedIndex, iSelection);

   if ( m_iTxCardSelectedIndex == iSelection )
   {
      log_line("MenuRadioConfig: No change in auto tx.");
      removeMenuItem(m_pItemSelectTxCard);
      m_pItemSelectTxCard = NULL;
      m_iTxCardSelectedIndex = -1;
      return;
   }

   // Remove all preferred cards for this link first
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iVehicleRadioLink )
         continue;
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL != pRadioHWInfo )
         controllerRemoveCardTXPreferred(pRadioHWInfo->szMAC);
   }
     
   if ( iSelection > 0 )
   {
      int iCount = 0;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iVehicleRadioLink )
            continue;
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         iCount++;
         if ( iCount == iSelection )
         {
            controllerSetCardTXPreferred(pRadioHWInfo->szMAC);
            break;
         }
      }
   }
   removeMenuItem(m_pItemSelectTxCard);
   m_pItemSelectTxCard = NULL;
   m_iTxCardSelectedIndex = -1;

   send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_UPDATED_RADIO_TX_POWERS, 0);
}

void MenuRadioConfig::onItemEndEdit(int itemIndex)
{
   if ( NULL == m_pItemSelectTxCard )
      return;

   onSelectedPreferredTxCard(m_iCurrentRadioLink);
}

void MenuRadioConfig::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);
   computeMaxItemIndexAndCommands();
}

void MenuRadioConfig::onClickAutoTx(int iRadioLink)
{
   m_iCurrentRadioLink = iRadioLink;
   m_pItemSelectTxCard = new MenuItemSelect("");
   m_pItemSelectTxCard->addSelection("Auto");

   int iSelectedIndex = 0; // auto option
   int iMinPriority = 10000;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId != iRadioLink )
         continue;
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
      if ( NULL == pCardInfo )
         continue;

      if ( controllerIsCardDisabled(pRadioHWInfo->szMAC) )
         continue;

      if ( controllerIsCardRXOnly(pRadioHWInfo->szMAC) )
         continue;

      char szBuff[256];
      char szName[128];
   
      strcpy(szName, "NoName");
   
      char szCardName[64];
      controllerGetCardUserDefinedNameOrShortType(pRadioHWInfo, szCardName);
      if ( NULL != szCardName && 0 != szCardName[0] )
         strcpy(szName, szCardName);
      else if ( NULL != pCardInfo )
         strcpy(szName, str_get_radio_card_model_string(pCardInfo->cardModel) );

      sprintf(szBuff, "Int. %d, Port %s, %s", i+1, pRadioHWInfo->szUSBPort, szName);
      m_pItemSelectTxCard->addSelection(szBuff);


      int iCardPriority = controllerIsCardTXPreferred(pRadioHWInfo->szMAC);
      if ( (iCardPriority <= 0) || (iCardPriority > iMinPriority) )
         continue;
      iMinPriority = iCardPriority;
      iSelectedIndex = m_pItemSelectTxCard->getSelectionsCount()-1;
   }


   addMenuItem(m_pItemSelectTxCard);
   m_pItemSelectTxCard->setIsEditable();
   m_pItemSelectTxCard->setPopupSelectorToRight();
   m_pItemSelectTxCard->setSelectedIndex(iSelectedIndex);
   m_pItemSelectTxCard->beginEdit();
}


void MenuRadioConfig::onSelectItem()
{
   if ( g_bSwitchingRadioLink )
      return;

   bool bConnectedToVehicle = false;
   if ( link_has_received_main_vehicle_ruby_telemetry() )
   if ( pairing_isStarted() )
   if ( NULL != g_pCurrentModel )
   if ( NULL != get_vehicle_runtime_info_for_vehicle_id(g_pCurrentModel->uVehicleId) )
   if ( ! get_vehicle_runtime_info_for_vehicle_id(g_pCurrentModel->uVehicleId)->bLinkLost )
      bConnectedToVehicle = true;

   char szConnectTitle[32];
   char szConnectMsg[128];
   strcpy(szConnectTitle, "Vehicle radio link");
   strcpy(szConnectMsg, "To change parameters, you must be connected to the vehicle and have an active radio link to the vehicle.");

   log_line("MenuRadioConfig: Selected item index %d, command: %d, extra param: %d. Connected to vehicle? %s",
      m_iIndexCurrentItem, (int)(m_uCommandsIds[m_iIndexCurrentItem] & 0xFF),
      (int)(m_uCommandsIds[m_iIndexCurrentItem] >> 8),
      bConnectedToVehicle?"yes":"no");

   if ( NULL != m_pItemSelectTxCard )
   {
      if ( ! bConnectedToVehicle )
      {
         addMessageWithTitle(0, szConnectTitle, szConnectMsg);
         return;
      }
      Menu::onSelectItem();
      return;
   }

   log_line("MenuRadioConfig: Clicked on current item %d", m_iIndexCurrentItem);

   float height_text = g_pRenderEngine->textHeight(m_iIdFontRegular);
   
   if ( m_bHasRotateRadioLinksOrderCommand )
   if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_ROTATE_RADIO_LINKS )
   {
      if ( ! bConnectedToVehicle )
      {
         addMessageWithTitle(0, szConnectTitle, szConnectMsg);
         return;
      }
      log_line("User selected command to rotate vehicle's radio links.");
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_ROTATE_RADIO_LINKS, 0, NULL, 0) )
         valuesToUI();
      return;
   }

   if ( m_bHasSwapInterfacesCommand )
   if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SWAP_VEHICLE_RADIO_INTERFACES )
   {
      if ( ! bConnectedToVehicle )
      {
         addMessageWithTitle(0, szConnectTitle, szConnectMsg);
         return;
      }
      log_line("User selected command to swap vehicle's radio interfaces.");
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SWAP_RADIO_INTERFACES, 0, NULL, 0) )
         valuesToUI();
      return;
   }

   if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_DIAGNOSE_RADIO_LINK )
   {
      if ( ! bConnectedToVehicle )
      {
         addMessageWithTitle(0, szConnectTitle, szConnectMsg);
         return;
      }
      int iVehicleRadioLinkId = (int)(m_uCommandsIds[m_iIndexCurrentItem] >> 8);
      log_line("MenuRadioConfig: Selected item to diagnose vehicle radio link %d", iVehicleRadioLinkId+1);

      if ( (iVehicleRadioLinkId < 0) || (iVehicleRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count) )
         return;

      MenuDiagnoseRadioLink* pMenu = new MenuDiagnoseRadioLink(iVehicleRadioLinkId);
      add_menu_to_stack(pMenu);
      return;
   }

   if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_CONFIGURE_RADIO_LINK )
   {
      if ( ! bConnectedToVehicle )
      {
         addMessageWithTitle(0, szConnectTitle, szConnectMsg);
         return;
      }
      int iVehicleRadioLinkId = (int)(m_uCommandsIds[m_iIndexCurrentItem] >> 8);
      log_line("MenuRadioConfig: Selected item to configure vehicle radio link %d", iVehicleRadioLinkId+1);

      if ( (iVehicleRadioLinkId < 0) || (iVehicleRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count) )
         return;

      Menu* pMenu = NULL;
      if ( g_pCurrentModel->radioLinkIsSiKRadio(iVehicleRadioLinkId) )
         pMenu = new MenuVehicleRadioLinkSiK(iVehicleRadioLinkId);
      else if ( g_pCurrentModel->radioLinkIsELRSRadio(iVehicleRadioLinkId) )
         pMenu = new MenuVehicleRadioLinkELRS(iVehicleRadioLinkId);
      else
         pMenu = new MenuVehicleRadioLink(iVehicleRadioLinkId);
        
      int iCountInterfacesAssignedToThisLink = 0;
      for( int i=0; i<g_SM_RadioStats.countLocalRadioInterfaces; i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId == iVehicleRadioLinkId )
            iCountInterfacesAssignedToThisLink++;
      }

      if ( 0 != iCountInterfacesAssignedToThisLink )
         pMenu->m_xPos = m_RenderXPos + m_RenderWidth*0.2;
      else
         pMenu->m_xPos = m_RenderXPos + m_RenderWidth*0.3;
      pMenu->m_yPos = m_fYPosRadioLinks[iVehicleRadioLinkId] - 5.0*height_text;
      if ( pMenu->m_yPos < 0.03 )
         pMenu->m_yPos = 0.03;
      if ( pMenu->m_yPos > 0.8 )
         pMenu->m_yPos = 0.8;
      add_menu_to_stack(pMenu);
      return;
   }

   if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_CONFIGURE_RADIO_INTERFACE_CONTROLLER )
   {
      int iRadioInterface = (int)(m_uCommandsIds[m_iIndexCurrentItem] >> 8);
      log_line("MenuRadioConfig: Configuring controller radio interface %d", iRadioInterface+1);
      Menu* pMenu = NULL;
      if ( hardware_radio_index_is_elrs_radio(iRadioInterface) )
      {
         addMessage(0, "You need to use ELRS configurator to configure your ELRS radio module.");
         return;
      }
      if ( hardware_radio_index_is_sik_radio(iRadioInterface) )
         pMenu = new MenuControllerRadioInterfaceSiK(iRadioInterface);
      else
         pMenu = new MenuControllerRadioInterface(iRadioInterface);
      pMenu->m_xPos = m_RenderXPos + m_RenderWidth*0.4;
      pMenu->m_yPos = m_fYPosCtrlRadioInterfaces[iRadioInterface] - 5.0*height_text;
      if ( pMenu->m_yPos < 0.03 )
         pMenu->m_yPos = 0.03;
      if ( pMenu->m_yPos > 0.8 )
         pMenu->m_yPos = 0.8;
      add_menu_to_stack(pMenu);
      return;  
   }

   if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_CONFIGURE_RADIO_INTERFACE_VEHICLE )
   {
      if ( ! bConnectedToVehicle )
      {
         addMessageWithTitle(0, szConnectTitle, szConnectMsg);
         return;
      }
      int iVehicleRadioInterface = (int)(m_uCommandsIds[m_iIndexCurrentItem] >> 8);
      log_line("MenuRadioConfig: Configuring vehicle radio interface %d", iVehicleRadioInterface+1);

      if ( (iVehicleRadioInterface < 0) || (iVehicleRadioInterface >= g_pCurrentModel->radioInterfacesParams.interfaces_count) )
         return;
      int iVehicleRadioLink = g_pCurrentModel->radioInterfacesParams.interface_link_id[iVehicleRadioInterface];
      if ( (iVehicleRadioLink < 0) || (iVehicleRadioLink >= g_pCurrentModel->radioLinksParams.links_count) )
         return;
      MenuVehicleRadioInterface* pMenu = new MenuVehicleRadioInterface(iVehicleRadioInterface);
      pMenu->m_xPos = m_RenderXPos + m_RenderWidth*0.8;
      pMenu->m_yPos = m_fYPosRadioLinks[iVehicleRadioLink] + m_fHeightLinks[iVehicleRadioLink]*0.5 - 5.0*height_text;
      if ( pMenu->m_yPos < 0.03 )
         pMenu->m_yPos = 0.03;
      if ( pMenu->m_yPos > 0.8 )
         pMenu->m_yPos = 0.8;
      add_menu_to_stack(pMenu);
      return;  
   }

   if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_PREFFERED_TX )
   {
      int iLink = (int)(m_uCommandsIds[m_iIndexCurrentItem] >> 8);
      onClickAutoTx(iLink);
      return;
   }

   if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SWITCH_RADIO_LINK )
   {
      int iLink = (int)(m_uCommandsIds[m_iIndexCurrentItem] >> 8);

      if ( (iLink < 0) || (iLink >= g_pCurrentModel->radioLinksParams.links_count) )
         return;

      g_bSwitchingRadioLink = true;
      warnings_add_switching_radio_link(iLink, g_pCurrentModel->radioLinksParams.link_frequency_khz[iLink]);
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_SWITCH_RADIO_LINK, iLink);
      return;
   }
}

float MenuRadioConfig::computeHeights()
{
   float height_text = g_pRenderEngine->textHeight(m_iIdFontRegular);
   float height_text_large = g_pRenderEngine->textHeight(m_iIdFontLarge);
   float fPaddingInnerY = 0.02;

   float hTotalHeight = 0.0;

   hTotalHeight += m_fHeaderHeight;
   hTotalHeight += 2.0 * m_sfMenuPaddingY;

   if ( (0 < g_SM_RadioStats.countVehicleRadioLinks) || (0 < g_SM_RadioStats.countLocalRadioLinks) )
   if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone && (!m_bShowOnlyControllerUnusedInterfaces) )
   {
      for( int iLink=0; iLink<m_iCountVehicleRadioLinks; iLink++ )
      {
          int countInterfaces = 0;
          for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
             if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId == iLink )
                countInterfaces++;
          if ( countInterfaces == 0 )
             countInterfaces = 1;

          m_fHeightLinks[iLink] = height_text_large;
          m_fHeightLinks[iLink] += 2.0*fPaddingInnerY + height_text*2.0;
          m_fHeightLinks[iLink] += countInterfaces * height_text*3.0 + (countInterfaces-1)*height_text*1.0;

          if ( countInterfaces > 1 )
             m_fHeightLinks[iLink] += height_text*2.0;

          hTotalHeight += m_fHeightLinks[iLink];
          if ( iLink != 0 )
             hTotalHeight += height_text*2.5;
      }
   }
   
   int iCountUnusedControllerInterfaces = 0;
   if ( (NULL == g_pCurrentModel) || (!g_bFirstModelPairingDone) || m_bShowOnlyControllerUnusedInterfaces )
      iCountUnusedControllerInterfaces = hardware_get_radio_interfaces_count();
   else
   {
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId >= 0 )
            continue;
         iCountUnusedControllerInterfaces++;
      }
   }
   hTotalHeight += iCountUnusedControllerInterfaces * height_text*2.0;
   if ( iCountUnusedControllerInterfaces > 0 )
   {
      hTotalHeight += height_text*4.0;
      if ( iCountUnusedControllerInterfaces > 1 )
         hTotalHeight += (iCountUnusedControllerInterfaces-1)*height_text*1.5;
   }

   if ( 0 == hardware_get_radio_interfaces_count() )
      hTotalHeight += height_text*1.5;

   hTotalHeight += m_fFooterHeight;
   if ( m_iCountVehicleRadioLinks < 2 )
      hTotalHeight += height_text;

   if ( hTotalHeight > 1.0 - 2.0 * 0.02 )
      hTotalHeight = 1.0 - 2.0 * 0.02;
   //log_line("Menu radio config: computed heights.");
   m_bComputedHeights = true;
   m_fTotalHeightRadioConfig = hTotalHeight;
   return hTotalHeight;
}

void MenuRadioConfig::computeMaxItemIndexAndCommands()
{
   m_iHeaderItemsCount = 0;
   m_iIndexMaxItem = 0;

   if ( (NULL == g_pCurrentModel) || (0 == g_SM_RadioStats.countVehicleRadioLinks) || m_bShowOnlyControllerUnusedInterfaces )
   {
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         setTooltip(m_iIndexMaxItem, "Configure this controller's radio interface.");
         m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_CONFIGURE_RADIO_INTERFACE_CONTROLLER + (((u32)i) << 8);
         m_iIndexMaxItem++;
      }
      if ( m_iIndexCurrentItem >= m_iIndexMaxItem )
         m_iIndexCurrentItem = m_iIndexMaxItem-1;

      return;
   }
   
   if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone )
   for( int iLink=0; iLink<m_iCountVehicleRadioLinks; iLink++ )
   {
       setTooltip(m_iIndexMaxItem, "Configure this radio link.");
       m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_CONFIGURE_RADIO_LINK + (((u32)iLink) << 8);
       m_iIndexMaxItem++;

       if ( m_bHasDiagnoseRadioLinkCommand[iLink] )
       {
          setTooltip(m_iIndexMaxItem, "Test this radio link.");
          m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_DIAGNOSE_RADIO_LINK + (((u32)iLink) << 8);
          m_iIndexMaxItem++;
       }
       int iCountInterfacesAssignedToThisLink = 0;
       for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
       {
          if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId == iLink )
          {
             setTooltip(m_iIndexMaxItem, "Configure this controller's radio interface.");
             m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_CONFIGURE_RADIO_INTERFACE_CONTROLLER + (((u32)i) << 8);
             m_iIndexMaxItem++;
             iCountInterfacesAssignedToThisLink++;
          }
       }
       if ( (0 == iCountInterfacesAssignedToThisLink) && m_bHasSwitchRadioLinkCommand[iLink] )
       {
          setTooltip(m_iIndexMaxItem, "Switch to and connect to this vehicle radio link.");
          m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_SWITCH_RADIO_LINK + (((u32)iLink) << 8);
          m_iIndexMaxItem++;
       }

       if ( iCountInterfacesAssignedToThisLink > 1 )
       {
          setTooltip(m_iIndexMaxItem, "Set the preffered Tx card (on the controller side) to be used for sending data to vehicle on this radio link.");
          m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_SET_PREFFERED_TX + (((u32)iLink) << 8);
          m_iIndexMaxItem++;
       }

       int iVehicleRadioInterfaceForThisLink = -1;
       for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
       {
          if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == iLink )
          {
             iVehicleRadioInterfaceForThisLink = i;
             break;
          }
       }
       if ( -1 != iVehicleRadioInterfaceForThisLink )
       {
          setTooltip(m_iIndexMaxItem, "Configure this vehicle's radio interface.");
          m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_CONFIGURE_RADIO_INTERFACE_VEHICLE + (((u32)iVehicleRadioInterfaceForThisLink) << 8);
          m_iIndexMaxItem++;
       }
   }

   // Controller's unused interfaces

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId >= 0 )
         continue;
      setTooltip(m_iIndexMaxItem, "Configure this radio interface.");
      m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_CONFIGURE_RADIO_INTERFACE_CONTROLLER + (((u32)i) << 8);
      m_iIndexMaxItem++;
   }

   if ( m_bHasSwapInterfacesCommand )
   {
      setTooltip(m_iIndexMaxItem, "Swap vehicle's radio interfaces assignment to radio links.");
      m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_SWAP_VEHICLE_RADIO_INTERFACES;
      m_iIndexMaxItem++;
   }

   if ( m_bHasRotateRadioLinksOrderCommand )
   {
      setTooltip(m_iIndexMaxItem, "Rotate vehicle's radio links order. This doesn't change any radio links parameters.");
      m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_ROTATE_RADIO_LINKS;
      m_iIndexMaxItem++;
   }

   if ( m_iIndexCurrentItem >= m_iIndexMaxItem )
      m_iIndexCurrentItem = m_iIndexMaxItem-1;

   //log_line("Menu radio config: computed max indexes.");
}


float MenuRadioConfig::drawRadioLinks(float xStart, float xEnd, float yStart)
{
   float height_text = g_pRenderEngine->textHeight(m_iIdFontRegular);
   float fWidth = (xEnd - xStart);
   float fXMid = xStart + fWidth*0.5;
   
   xStart += m_sfMenuPaddingX;
   xEnd -= m_sfMenuPaddingX;
   fWidth -= 2.0 * m_sfMenuPaddingX;

   m_RenderXPos = xStart;
   m_RenderWidth = fWidth;
   
   computeMaxItemIndexAndCommands();

   m_bTmpHasVideoStreamsEnabled = false;
   m_bTmpHasDataStreamsEnabled = false;

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   float yPos = yStart;

   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      yPos += height_text*4.0;
      g_pRenderEngine->drawText(xStart, yPos, m_iIdFontLarge, "No radio interfaces on controller!");
      return yPos;   
   }

   // ---------------------------------
   // Draw each radio link

   for( int iLink = 0; iLink < m_iCountVehicleRadioLinks; iLink++ )
   {
      drawOneRadioLink(xStart, xEnd, yPos, iLink );
      yPos += m_fHeightLinks[iLink] + height_text*2.0;
   }

   if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone && (!m_bShowOnlyControllerUnusedInterfaces) )
   {
      if ( ! m_bTmpHasVideoStreamsEnabled )
      {
         g_pRenderEngine->setColors(get_Color_IconError());
         g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
         g_pRenderEngine->drawText(xStart, yPos-height_text*0.3, m_iIdFontRegular, "Warning: No radio links are setup to transmit video streams.");
         yPos += height_text;
         g_pRenderEngine->setColors(get_Color_MenuText());
         g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      }
      if ( ! m_bTmpHasDataStreamsEnabled )
      {
         g_pRenderEngine->setColors(get_Color_IconError());
         g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
         g_pRenderEngine->drawText(xStart, yPos-height_text*0.3, m_iIdFontRegular, "Warning: No radio links are setup to transmit data streams.");
         yPos += height_text;
         g_pRenderEngine->setColors(get_Color_MenuText());
         g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      }
   }

   // Show unused interfaces on the controller (disabled or not usable for current vehicle)

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   int iCountUnusedControllerInterfaces = 0;
   if ( (NULL == g_pCurrentModel) || (!g_bFirstModelPairingDone) || m_bShowOnlyControllerUnusedInterfaces )
      iCountUnusedControllerInterfaces = hardware_get_radio_interfaces_count();
   else
   {
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId >= 0 )
            continue;
         iCountUnusedControllerInterfaces++;
      }
   }
   if ( 0 == iCountUnusedControllerInterfaces )
      return yPos;

   if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone && (! m_bShowOnlyControllerUnusedInterfaces) )
   {
      g_pRenderEngine->drawText(xStart, yPos, m_iIdFontRegular, "Unused controller radio interfaces:");
      yPos += height_text;
      g_pRenderEngine->drawText(xStart, yPos, m_iIdFontRegular, "  (disabled or not usable for current vehicle radio configuration)");
      yPos += height_text*1.5;
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( (NULL == g_pCurrentModel) || ( g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId < 0) || (!g_bFirstModelPairingDone) || m_bShowOnlyControllerUnusedInterfaces )
         yPos += drawRadioInterfaceController(fXMid, xEnd, yPos, -1, i);
   }

   g_pRenderEngine->clearFontBackgroundBoundingBoxStrikeColor();

   return yPos;
}

float MenuRadioConfig::drawRadioHeader(float xStart, float xEnd, float yStart)
{
   float height_text = g_pRenderEngine->textHeight(m_iIdFontRegular);
   float height_text_large = g_pRenderEngine->textHeight(m_iIdFontLarge);
   float hIconBig = height_text*3.0;
   float yPos = yStart;
   float xMid = (xStart+xEnd)*0.5;
   float xMidMargin = 0.14/g_pRenderEngine->getAspectRatio();
   char szBuff[128];

   float ftw = g_pRenderEngine->textWidth(m_iIdFontLarge, "Current Radio Configuration");
   g_pRenderEngine->drawText(xStart + (xEnd-xStart - ftw)*0.5, yPos, m_iIdFontLarge, "Current Radio Configuration");
   yPos += height_text_large*1.4;

   float yTmp = yPos;

   u32 uIdIconVehicle = g_idIconDrone;
   if ( NULL != g_pCurrentModel )
      uIdIconVehicle = osd_getVehicleIcon( g_pCurrentModel->vehicle_type );
   //g_pRenderEngine->drawIcon(xStart + fWidth*0.2-hIconBig*0.5/g_pRenderEngine->getAspectRatio(), yPos, hIconBig/g_pRenderEngine->getAspectRatio(), hIconBig, g_idIconJoystick);
   //g_pRenderEngine->drawIcon(xStart + fWidth*0.8-hIconBig*0.5/g_pRenderEngine->getAspectRatio(), yPos, hIconBig/g_pRenderEngine->getAspectRatio(), hIconBig, uIdIconVehicle);
   
   float fScale = 1.7;
   g_pRenderEngine->drawIcon(xMid - xMidMargin - hIconBig*fScale/g_pRenderEngine->getAspectRatio(), yPos-hIconBig*0.3, hIconBig*fScale/g_pRenderEngine->getAspectRatio(), hIconBig*fScale, g_idIconController);
   if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone && (!m_bShowOnlyControllerUnusedInterfaces) )
      g_pRenderEngine->drawIcon(xMid + xMidMargin, yPos, hIconBig/g_pRenderEngine->getAspectRatio(), hIconBig, uIdIconVehicle);
   
   float xLeft = xMid - xMidMargin - hIconBig*fScale/g_pRenderEngine->getAspectRatio() - 0.02/g_pRenderEngine->getAspectRatio();
   float xRight = xMid + xMidMargin + hIconBig/g_pRenderEngine->getAspectRatio() + 0.02/g_pRenderEngine->getAspectRatio();

   g_pRenderEngine->drawTextLeft(xLeft, yPos + (hIconBig-height_text)*0.5, m_iIdFontRegular, "Controller");

   if ( (NULL == g_pCurrentModel) || ( ! g_bFirstModelPairingDone) || m_bShowOnlyControllerUnusedInterfaces )
      strcpy(szBuff, "No Vehicle Active");
   else
      strcpy(szBuff, g_pCurrentModel->getLongName());

   szBuff[0] = toupper(szBuff[0]);
   g_pRenderEngine->drawText(xRight, yPos + (hIconBig-height_text)*0.5, m_iIdFontRegular, szBuff);
   
   yPos += hIconBig + height_text*1.5;
   
   if ( (NULL == g_pCurrentModel) || (! g_bFirstModelPairingDone) || m_bShowOnlyControllerUnusedInterfaces )
   {
      g_pRenderEngine->drawMessageLines(xMid + xMidMargin*0.6, yPos, "You have no active model. No radio links are created.", MENU_TEXTLINE_SPACING, (xEnd - xMid) - m_sfMenuPaddingX - xMidMargin*0.6, m_iIdFontRegular);
      yPos += height_text*1.2;
      return yPos - yStart;
   }

   float fAlpha = g_pRenderEngine->setGlobalAlfa(0.5);
   for( float fy = yTmp; fy<yPos - height_text; fy += 0.02 )
      g_pRenderEngine->drawLine(xMid, fy, xMid, fy+0.01);
   g_pRenderEngine->setGlobalAlfa(fAlpha);
   yPos += height_text*0.3;
   return yPos - yStart;
}


void MenuRadioConfig::drawOneRadioLinkCapabilities(float xStart, float xEnd, float yStart, int iVehicleRadioLink, bool bIsLinkActive, bool bIsRelayLink)
{
   char szDescription[128];
   char szDescriptionSmall[128];
   char szCapabilities[128];
   char szDRVideo[128];
   char szDRDataUp[128];
   char szDRDataDown[128];

   int iCountInterfacesAssignableToThisLink = controller_count_asignable_radio_interfaces_to_vehicle_radio_link(g_pCurrentModel, iVehicleRadioLink);
   int iCountInterfacesAssignedToThisLink = 0;
   int iRadioInterfaceIndex = -1;

   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == iVehicleRadioLink )
         iRadioInterfaceIndex = i;
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;

      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
      if ( NULL == pCardInfo )
         continue;

      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId == iVehicleRadioLink )
         iCountInterfacesAssignedToThisLink++;
   }

   szDescription[0] = 0;
   szDescriptionSmall[0] = 0;
   szCapabilities[0] = 0;

   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      strcpy(szCapabilities, "RELAY LINK");
   else
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
         strcpy(szCapabilities, "Downlink Only");
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         strcpy(szCapabilities, "Uplink Only");
   }

   if ( 0 < strlen(szCapabilities) )
      strcat(szCapabilities, ",  ");

   char szTmpRadioLink[256];
   strcpy(szTmpRadioLink, szCapabilities);

   bool bDataOnlyRadioLink = false;
   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
   if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
      strcat(szCapabilities, "Video Only,");
      
   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
   if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) )
   {
      bDataOnlyRadioLink = true;
      strcat(szCapabilities, "Data Only,");
   }

   bool bUsesHT40 = false;
   if ( g_pCurrentModel->radioLinksParams.link_radio_flags[iVehicleRadioLink] & RADIO_FLAG_HT40_VEHICLE )
      bUsesHT40 = true;
   str_getDataRateDescription(g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[iVehicleRadioLink], bUsesHT40, szDRVideo);
   str_getDataRateDescription(g_pCurrentModel->radioLinksParams.downlink_datarate_data_bps[iVehicleRadioLink], bUsesHT40, szDRDataDown);
   str_getDataRateDescription(g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[iVehicleRadioLink], 0, szDRDataUp);

   bool bShowLinkRed = false;

   if ( 0 == iCountInterfacesAssignedToThisLink )
   {
      szDescription[0] = 0;
      szDescriptionSmall[0] = 0;
      if ( bDataOnlyRadioLink )
         strcpy(szDescription, "Data Only. ");
      if ( !bIsLinkActive )
      {
         strcat(szDescription, "This radio link is disabled.");
         bShowLinkRed = true;
      }
      else if ( bIsRelayLink )
         strcat(szDescription, "This is a relay radio link.");
      else if ( 0 < iCountInterfacesAssignableToThisLink )
      {
         strcat(szDescription, "Not connected.");
         strcpy(szDescriptionSmall, "Ready to connect to this link.");
      }
      else
      {
         strcat(szDescription, "Not connected.");
         strcpy(szDescriptionSmall, "No controller interfaces can connect to this link.");
      }
   }
   else
   {
      if ( g_pCurrentModel->radioLinkIsSiKRadio(iVehicleRadioLink) && (-1 != iRadioInterfaceIndex) )
      {
         char szTmp[64];
         str_format_bitrate(hardware_radio_sik_get_air_baudrate_in_bytes(iRadioInterfaceIndex)*8, szTmp);

         if ( 0 != szCapabilities[0] )
            snprintf(szDescription, sizeof(szDescription)/sizeof(szDescription[0]), "%s Data Rate: %s", szCapabilities, szTmp);
         else
            snprintf(szDescription, sizeof(szDescription)/sizeof(szDescription[0]), "Data Rate: %s", szTmp);
      }
      else
      {
         szDescription[0] = 0;
         if ( 0 != szCapabilities[0] )
         {
            strcat(szDescription, szCapabilities);
            strcat(szDescription, " ");
         }
         char szTmp[256];
         if ( bDataOnlyRadioLink )
            snprintf(szTmp, sizeof(szTmp)/sizeof(szTmp[0]), "Data Rates: Data: %s, Uplink: %s", szDRDataDown, szDRDataUp);
         else
            snprintf(szTmp, sizeof(szTmp)/sizeof(szTmp[0]), "Data Rates: Video: %s, Data: %s, Uplink: %s", szDRVideo, szDRDataDown, szDRDataUp);
         strcat(szDescription, szTmp);
      }
   }

   if ( bShowLinkRed )
   {
      g_pRenderEngine->setColors(get_Color_IconError());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   }

   float fLine1W = g_pRenderEngine->textWidth(m_iIdFontRegular, szDescription);
   float xLine1 = xStart + 0.5*((xEnd-xStart) - fLine1W);
   g_pRenderEngine->drawText(xLine1, yStart, m_iIdFontRegular, szDescription);
   if ( 0 != szDescriptionSmall[0] )
   {
      float fLine2W = g_pRenderEngine->textWidth(m_iIdFontSmall, szDescriptionSmall);
      float xLine2 = xStart + 0.5*((xEnd-xStart) - fLine2W);
      g_pRenderEngine->drawText(xLine2, yStart + 0.94*(g_pRenderEngine->textHeight(m_iIdFontRegular)), m_iIdFontSmall, szDescriptionSmall);
   }
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   if ( bDataOnlyRadioLink )
   {
      double cY[4] = {255,240,100,1.0};

      g_pRenderEngine->setColors(cY);
      g_pRenderEngine->drawText(xLine1, yStart, m_iIdFontRegular, "Data Only");

      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   }
}

float MenuRadioConfig::drawOneRadioLink(float xStart, float xEnd, float yStart, int iVehicleRadioLink)
{
   float height_text = g_pRenderEngine->textHeight(m_iIdFontRegular);
   float height_text_small = g_pRenderEngine->textHeight(m_iIdFontSmall);
   float height_text_large = g_pRenderEngine->textHeight(m_iIdFontLarge);
   float hIcon = height_text*2.4;
   float fPaddingInnerY = 0.02;
   float fPaddingInnerX = fPaddingInnerY/g_pRenderEngine->getAspectRatio();
   ControllerSettings* pCS = get_ControllerSettings();
   
   bool bBBox = false;
   bool bShowLinkRed = false;
   bool bShowRed = false;
   bool bHasAutoTxOption = false;

   double cY[4] = {255,240,100,1.0};
   
   char szTmp[64];
   char szBuff[128];
   char szError[128];
   
   float xLeftMax[MAX_RADIO_INTERFACES];
   float yMidVehicle = 0.0;
   float yMidController[MAX_RADIO_INTERFACES];
   float fWidth = xEnd - xStart;
   float xMid = (xStart + xEnd)*0.5;
   float xMidMargin = 0.11/g_pRenderEngine->getAspectRatio();
   float yPos = yStart;

   szBuff[0] = 0;
   szTmp[0] = 0;
   szError[0] = 0;

   m_fYPosRadioLinks[iVehicleRadioLink] = yPos;

   int iVehicleRadioInterfaceForThisLink = -1;
   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == iVehicleRadioLink )
      {
         iVehicleRadioInterfaceForThisLink = i;
         break;
      }
   }

   int iCountInterfacesAssignedToThisLink = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;

      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
      if ( NULL == pCardInfo )
         continue;

      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId == iVehicleRadioLink )
         iCountInterfacesAssignedToThisLink++;
   }

   int iCountInterfacesAssignableToThisLink = controller_count_asignable_radio_interfaces_to_vehicle_radio_link(g_pCurrentModel, iVehicleRadioLink);

   int iCurrentMenuItemCommand = (int)(m_uCommandsIds[m_iIndexCurrentItem] & 0xFF);
   int iCurrentMenuItemExtraParam = (int)(m_uCommandsIds[m_iIndexCurrentItem] >> 8);

   if ( iCountInterfacesAssignedToThisLink > 1 )
      bHasAutoTxOption = true;

   bool bIsLinkActive = true;
   bool bIsRelayLink = false;
   
   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
   {
      strcpy(szError, "Disabled");
      bShowLinkRed = true;
      bIsLinkActive = false;
   }
   if ( -1 == iVehicleRadioInterfaceForThisLink )
   {
      strcpy(szError, "No radio interface");
      bShowLinkRed = true;
      bIsLinkActive = false;    
   }
   else if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceForThisLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
   {
      strcpy(szError, "Disabled");
      bShowLinkRed = true;
      bIsLinkActive = false;
   }
   if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA ) )
   if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO ) )
   {
      strcpy(szError, "No data streams enabled on this radio link.");
      bShowLinkRed = true;
      bIsLinkActive = false;
   }
   
   if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX ) )
   if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX ) )
   {
      strcpy(szError, "Both Uplink and Downlink capabilities are disabled.");
      bShowLinkRed = true;
      bIsLinkActive = false;
   }

   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
   {
      bIsRelayLink = true;
   }
   else
   {
     
   }

   if ( ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO ) &&
        ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA ) )
   {
      if ( bIsLinkActive )
      {
         m_bTmpHasVideoStreamsEnabled = true;
         m_bTmpHasDataStreamsEnabled = true;
      }
   }
   else if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
   {
      if ( bIsLinkActive )
         m_bTmpHasVideoStreamsEnabled = true;
   }
   else if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
   {
      //bDataOnlyRadioLink = true;
      if ( bIsLinkActive )
         m_bTmpHasDataStreamsEnabled = true;
   }

   if ( bIsRelayLink )
       sprintf(szBuff, "RELAY Link-%d  %s", iVehicleRadioLink+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iVehicleRadioLink]));
   else if ( bIsLinkActive )
       sprintf(szBuff, "Radio Link-%d  %s", iVehicleRadioLink+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iVehicleRadioLink]));
   else
      sprintf(szBuff, "Radio Link-%d  %s", iVehicleRadioLink+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iVehicleRadioLink]));
      

   // -------------------------------------------------------
   // Begin - Draw link name/title

   float fTitleWidth = g_pRenderEngine->textWidth(m_iIdFontLarge, szBuff);
   float fTitleXpos = xStart + 0.5*((xEnd-xStart) - fTitleWidth);
   float fxTitleEnd = 0.0;

   if ( (iCurrentMenuItemCommand == MRC_ID_CONFIGURE_RADIO_LINK) && (iCurrentMenuItemExtraParam == iVehicleRadioLink) )
      bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   if ( bShowLinkRed )
   {
      g_pRenderEngine->setColors(get_Color_IconError());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   }

   g_pRenderEngine->drawText(fTitleXpos, yPos-height_text*0.3, m_iIdFontLarge, szBuff);
   fxTitleEnd = fTitleXpos + fTitleWidth + height_text;

   if ( bShowLinkRed && bIsLinkActive )
   {
      g_pRenderEngine->drawText(fxTitleEnd, yPos-height_text*0.2, m_iIdFontRegular, szError);
      fxTitleEnd += 0.01 + g_pRenderEngine->textWidth(m_iIdFontRegular, szError);
   }

   if ( (iCurrentMenuItemCommand == MRC_ID_CONFIGURE_RADIO_LINK) && (iCurrentMenuItemExtraParam == iVehicleRadioLink) )
      g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

   if ( m_bHasDiagnoseRadioLinkCommand[iVehicleRadioLink] )
   {
      if ( (iCurrentMenuItemCommand == MRC_ID_DIAGNOSE_RADIO_LINK) && (iCurrentMenuItemExtraParam == iVehicleRadioLink) )
         bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);

      g_pRenderEngine->drawText(fxTitleEnd, yPos-height_text*0.2, m_iIdFontRegular, "Test");
      
      if ( (iCurrentMenuItemCommand == MRC_ID_DIAGNOSE_RADIO_LINK) && (iCurrentMenuItemExtraParam == iVehicleRadioLink) )
         g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

   }

   // End - Draw link name/title
   // -------------------------------------------------------

   yPos += height_text_large;

   // ------------------------------------------------------
   // Begin - Draw link container rectangle

   double pColor[4];
   memcpy(pColor,get_Color_MenuBg(), 4*sizeof(double));
   pColor[0] += 10;
   pColor[1] += 10;
   pColor[2] += 10;
   g_pRenderEngine->setColors(pColor);

   memcpy(pColor,get_Color_MenuText(), 4*sizeof(double));
   pColor[0] -= 20;
   pColor[1] -= 20;
   pColor[2] -= 20;
   g_pRenderEngine->setStroke(pColor);
   g_pRenderEngine->setStrokeSize(2.0);

   if ( bShowLinkRed )
   {
      g_pRenderEngine->setStroke(get_Color_IconError());
      g_pRenderEngine->setStrokeSize(2.0);
   }

   if ( 0 == iCountInterfacesAssignedToThisLink )
      g_pRenderEngine->drawRoundRect(xMid, yPos, fWidth - (xMid - xStart), m_fHeightLinks[iVehicleRadioLink]-height_text_large, MENU_ROUND_MARGIN*m_sfMenuPaddingY);
   else
      g_pRenderEngine->drawRoundRect(xStart, yPos, fWidth, m_fHeightLinks[iVehicleRadioLink]-height_text_large, MENU_ROUND_MARGIN*m_sfMenuPaddingY);
   
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   // End - Draw link container rectangle
   // ------------------------------------------------------

   yPos += fPaddingInnerY * 0.7;

   drawOneRadioLinkCapabilities(xStart + fPaddingInnerX, xEnd - fPaddingInnerX, yPos, iVehicleRadioLink, bIsLinkActive, bIsRelayLink);

   yPos += height_text*2.0;
   yPos += fPaddingInnerY * 0.1;
   
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   //--------------------------------------------------
   // Draw vehicle interfaces

   float y1 = yPos;

   if ( bHasAutoTxOption )
      yPos += (m_fHeightLinks[iVehicleRadioLink]-height_text_large - 2.0*fPaddingInnerY-4.0*height_text) * 0.5 - height_text*1.5;
   else
      yPos += (m_fHeightLinks[iVehicleRadioLink]-height_text_large - 2.0*fPaddingInnerY-2.0*height_text) * 0.5 - height_text*1.5;

   int iVehicleRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(iVehicleRadioLink);
      
   yMidVehicle = yPos + height_text*1.5;
   szTmp[0] = 0;
   if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
   {
      strcat(szTmp, " Disabled");
      bShowRed = true;
   }
   if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
   if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
   {
      strcat(szTmp, " Can't Uplink or Downlink");
      bShowRed = true;
   }
   
   g_pRenderEngine->drawIcon(xMid + xMidMargin, yPos, hIcon/g_pRenderEngine->getAspectRatio(), hIcon, g_idIconRadio);

   float xiRight = xMid + xMidMargin + hIcon/g_pRenderEngine->getAspectRatio() + fPaddingInnerX;
   sprintf(szBuff, "Interface %d, Port %s", iVehicleRadioInterfaceId+1, g_pCurrentModel->radioInterfacesParams.interface_szPort[iVehicleRadioInterfaceId]);
   
   if ( (iCurrentMenuItemCommand == MRC_ID_CONFIGURE_RADIO_INTERFACE_VEHICLE) && (iCurrentMenuItemExtraParam == iVehicleRadioInterfaceId) )
      bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
    
   g_pRenderEngine->drawText(xiRight, yPos, m_iIdFontRegular, szBuff);

   if ( (iCurrentMenuItemCommand == MRC_ID_CONFIGURE_RADIO_INTERFACE_VEHICLE) && (iCurrentMenuItemExtraParam == iVehicleRadioInterfaceId) )
      g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

   yPos += height_text*1.2;

   sprintf(szBuff, "Type: %s", str_get_radio_card_model_string(g_pCurrentModel->radioInterfacesParams.interface_card_model[iVehicleRadioInterfaceId]));
   g_pRenderEngine->drawText(xiRight, yPos, m_iIdFontRegular, szBuff);
   yPos += height_text;

   szBuff[0] = 0;

   if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA ))
   if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO ))
   {
      bShowRed = true;
      strcat(szTmp, " No streams enabled");
   }

   if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
   if ( ! (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_RX ) )
      strcat(szBuff, "Downlink Only");

   if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
   if ( ! (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX ) )
      strcat(szBuff, "Uplink Only");

   if ( ! (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
   if ( ! (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_RX ) )
   {
      bShowRed = true;
      strcat(szTmp, " Disabled");
   }
   
   g_pRenderEngine->drawText(xiRight, yPos,  m_iIdFontRegular, szBuff);
   yPos += height_text;

   //g_pRenderEngine->drawLine(xiRight - fPaddingInnerX, yStartVehicle + height_text*0.5, xiRight- fPaddingInnerX, yPos - height_text*0.5);

   //------------------------------------------------
   // Draw controller interfaces
   
   float yTopInterfaces = y1;
   yPos = y1;
   iCountInterfacesAssignedToThisLink = 0;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      xLeftMax[i] = 0.0;
      yMidController[i] = 0.0;
      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iVehicleRadioLink )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;

      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
      if ( NULL == pCardInfo )
         continue;

      iCountInterfacesAssignedToThisLink++;

      if ( iCountInterfacesAssignedToThisLink != 1 )
         yPos += height_text*1.0;

      m_fYPosCtrlRadioInterfaces[i] = yPos;
      y1 = yPos;
      yMidController[i] = yPos + height_text*1.5;

      bool bSelected = false;
      if ( (iCurrentMenuItemCommand == MRC_ID_CONFIGURE_RADIO_INTERFACE_CONTROLLER) && (iCurrentMenuItemExtraParam == i) )
         bSelected = true;

      xLeftMax[i] = xMid - xMidMargin - drawRadioInterfaceCtrlInfo(xMid - xMidMargin, 1.0, yPos, iVehicleRadioLink, i, bSelected);
      yPos += height_text*1.2;
      yPos += height_text*1.1;
      yPos += height_text;

   }

   if ( 0 == iCountInterfacesAssignedToThisLink )
   {
      yPos -= height_text*0.5;

      if ( bIsRelayLink )
      {
         strcpy(szBuff, "This is a Relay link between vehicles.");
         yPos += g_pRenderEngine->drawMessageLines(xStart + 0.02 + fPaddingInnerX, yPos, szBuff, MENU_TEXTLINE_SPACING, (xMid-xMidMargin-xStart - 0.02 - fPaddingInnerX), m_iIdFontRegular);
      }
      else if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      {
         strcpy(szBuff, "This vehicle radio link is disabled.");
         yPos += g_pRenderEngine->drawMessageLines(xStart + 0.02 + fPaddingInnerX, yPos, szBuff, MENU_TEXTLINE_SPACING, (xMid-xMidMargin-xStart - 0.02 - fPaddingInnerX), m_iIdFontRegular);
      }
      else if ( -1 == iVehicleRadioInterfaceForThisLink )
      {
         strcpy(szBuff, "There is no radio interface on vehicle assigned to this radio link.");
         yPos += g_pRenderEngine->drawMessageLines(xStart + 0.02 + fPaddingInnerX, yPos, szBuff, MENU_TEXTLINE_SPACING, (xMid-xMidMargin-xStart - 0.02 - fPaddingInnerX), m_iIdFontRegular);
      }
      else if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceForThisLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      {
         strcpy(szBuff, "The vehicle radio interface assigned to this radio link is disabled.");
         yPos += g_pRenderEngine->drawMessageLines(xStart + 0.02 + fPaddingInnerX, yPos, szBuff, MENU_TEXTLINE_SPACING, (xMid-xMidMargin-xStart - 0.02 - fPaddingInnerX), m_iIdFontRegular);
      }
      else
      {
         if ( 1 == g_SM_RadioStats.countLocalRadioInterfaces )
            strcpy(szBuff, "No more controller interfaces available for this vehicle radio link.");
         else
         {
            strcpy(szBuff, "No controller interfaces that can handle this vehicle radio link.");
            g_pRenderEngine->setColors(get_Color_IconError());
            g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
         }
         if ( m_bHasSwitchRadioLinkCommand[iVehicleRadioLink] )
            yPos -= height_text * 0.4;
         yPos += g_pRenderEngine->drawMessageLines(xStart + 0.02 + fPaddingInnerX, yPos, szBuff, MENU_TEXTLINE_SPACING, (xMid-xMidMargin-xStart - 0.02 - fPaddingInnerX), m_iIdFontRegular); 
         g_pRenderEngine->setColors(get_Color_MenuText());
         g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      }
   }

   if ( (0 == iCountInterfacesAssignedToThisLink) && (iCountInterfacesAssignableToThisLink > 0) )
   {
      yPos += height_text*0.8;

      if ( (iCurrentMenuItemCommand == MRC_ID_SWITCH_RADIO_LINK) && (iCurrentMenuItemExtraParam == iVehicleRadioLink) )
          bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
     
       g_pRenderEngine->drawText(xStart + 0.02 + fPaddingInnerX, yPos, m_iIdFontLarge, "Switch to this radio link");
       
       if ( (iCurrentMenuItemCommand == MRC_ID_SWITCH_RADIO_LINK) && (iCurrentMenuItemExtraParam == iVehicleRadioLink) )
          g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);
       
       yPos += 1.0*height_text_large;
   }

   //--------------------------------------------------
   // Draw auto tx card

   if ( iCountInterfacesAssignedToThisLink > 1 )
   {
      yPos += 0.5 * height_text;
      m_fYPosAutoTx[iVehicleRadioLink] = yPos;

      sprintf(szBuff, "Preferred Tx Card: Auto");
      int iMinPriority = 10000;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iVehicleRadioLink )
            continue;

         radio_hw_info_t* pNICInfo2 = hardware_get_radio_info(i);
         if ( NULL == pNICInfo2 )
            continue;

         if ( controllerIsCardRXOnly(pNICInfo2->szMAC) )
            continue;

         int iCardPriority = controllerIsCardTXPreferred(pNICInfo2->szMAC);
         if ( (iCardPriority <= 0) || (iCardPriority > iMinPriority) )
            continue;
         iMinPriority = iCardPriority;
         char szCardName[64];
         controllerGetCardUserDefinedNameOrShortType(pNICInfo2, szCardName);
         sprintf(szBuff, "Preferred Tx Card: Interface %d, %s Port %s", i+1, szCardName, pNICInfo2->szUSBPort);
         if ( ! controllerIsCardInternal(pNICInfo2->szMAC) )
            strcat(szBuff, " (Ext)");
      }

      if ( (iCurrentMenuItemCommand == MRC_ID_SET_PREFFERED_TX) && (iCurrentMenuItemExtraParam == iVehicleRadioLink) )
         bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
    
      g_pRenderEngine->drawText(xStart + 0.02 + fPaddingInnerX, yPos, m_iIdFontRegular, szBuff);
      
      if ( (iCurrentMenuItemCommand == MRC_ID_SET_PREFFERED_TX) && (iCurrentMenuItemExtraParam == iVehicleRadioLink) )
         g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);
      
      yPos += 1.5*height_text;
   }

   //---------------------------------------
   // Draw mid separator

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   float fAlpha = g_pRenderEngine->setGlobalAlfa(0.2);
   for( float y = yTopInterfaces; y<yPos-0.01; y += 0.02 )
      g_pRenderEngine->drawLine(xMid, y, xMid, y+0.01);
   g_pRenderEngine->setGlobalAlfa(fAlpha);
   
   //--------------------------------------------------
   // Draw arrows

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      
   float xLeftMaxMax = 0.0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      if ( xLeftMax[i] > xLeftMaxMax )
         xLeftMaxMax = xLeftMax[i];

   float dyArrow = 0.0;
   float arrowSpacing = 0.0;
   if ( iCountInterfacesAssignedToThisLink > 1 )
   {
      arrowSpacing = height_text*1.6/(iCountInterfacesAssignedToThisLink-1);
      dyArrow = - height_text*0.8;
   }

   int iArrowCount = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iVehicleRadioLink )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;

      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
      if ( NULL == pCardInfo )
         continue;

      int iVehicleRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(iVehicleRadioLink);
      iArrowCount++;
      bShowRed = false;

      if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         bShowRed = true;
      if ( !(pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
      if ( !(pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
         bShowRed = true;

      bool bCanUplink = false;
      bool bCanDownlink = false;
      bool bCanExchangeData = false;
      bool bCanExchangeVideo = false;

      if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
         bCanUplink = true;

      if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
         bCanDownlink = true;   

      if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
         bCanExchangeData = true;

      if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
         bCanExchangeVideo = true;

      if ( (! bCanExchangeData) && (! bCanExchangeVideo) )
      {
         bShowRed = true;
         bCanUplink = false;
         bCanDownlink = false;
      }
      //if ( bShowRed )
      //   continue;

      if ( bShowRed )
      {
         g_pRenderEngine->setColors(get_Color_IconError());
         g_pRenderEngine->setStrokeSize(1);
      }
      else
      {
         g_pRenderEngine->setColors(get_Color_MenuText());
         g_pRenderEngine->setStrokeSize(1);
      }

      //g_pRenderEngine->drawLine(xLeftMaxMax + fPaddingInnerX, yMidController[i] - height_text, xLeftMaxMax + fPaddingInnerX, yMidController[i] + height_text);

      float xLineCtrl = xMid - xMidMargin + fPaddingInnerX*0.2;
      float xLineVeh = xMid + xMidMargin - fPaddingInnerX*0.2;
      float yLineCtrl = yMidController[i]-dyArrow*0.2;
      float yLineVeh = yMidVehicle + dyArrow*0.5;

      //if ( bCanExchangeData && bCanExchangeVideo )
      //{
      //   g_pRenderEngine->drawLine(xLineStart + 6.0*g_pRenderEngine->getPixelWidth(), yLineStart - 1.0*g_pRenderEngine->getPixelHeight(), xLineEnd - 6.0*g_pRenderEngine->getPixelWidth(), yLineEnd - 1.0*g_pRenderEngine->getPixelHeight() );
      //   g_pRenderEngine->drawLine(xLineStart + 6.0*g_pRenderEngine->getPixelWidth(), yLineStart + 1.0*g_pRenderEngine->getPixelHeight(), xLineEnd - 6.0*g_pRenderEngine->getPixelWidth(), yLineEnd + 1.0*g_pRenderEngine->getPixelHeight());
      //}
      //else

      if ( ! bShowRed )
      if ( bCanExchangeData != bCanExchangeVideo )
      {
         g_pRenderEngine->setColors(cY);
         g_pRenderEngine->setStrokeSize(1.0);
      }
      
      float xLineMid = (xLineCtrl + xLineVeh)/2.0;
      float yLineMid = (yLineCtrl + yLineVeh)/2.0;
      float xLineMarginCtrl = xLineMid - height_text*0.5;
      float xLineMarginVeh = xLineMid + height_text*0.5;
      float yLineMarginCtrl = yLineCtrl + (yLineMid-yLineCtrl) * (xLineMarginCtrl-xLineCtrl) / (xLineMid - xLineCtrl);
      float yLineMarginVeh = yLineVeh + (yLineMid-yLineVeh) * (xLineMarginVeh-xLineVeh) / (xLineMid - xLineVeh);
      
      //g_pRenderEngine->drawLine(xLineCtrl, yLineCtrl, xLineVeh, yLineVeh );
      g_pRenderEngine->drawLine(xLineCtrl, yLineCtrl, xLineMarginCtrl, yLineMarginCtrl);
      g_pRenderEngine->drawLine(xLineVeh, yLineVeh, xLineMarginVeh, yLineMarginVeh);
 
      char szTxPower[64];
      if ( 1 == iArrowCount )
      {
         int iVehicleMw = get_vehicle_radio_link_current_tx_power_mw(g_pCurrentModel, iVehicleRadioLink);
         if ( iVehicleMw < 1000 )
            sprintf(szTxPower, "%d mW", iVehicleMw);
         else
            sprintf(szTxPower, "%.1f W", (float)iVehicleMw/1000.0);
         g_pRenderEngine->drawTextLeft(xLineVeh - height_text*0.2, yLineMarginVeh - height_text*1.2, g_idFontMenuSmall, szTxPower);
      }
      int iCardMw = tx_powers_convert_raw_to_mw(hardware_getBoardType(), pCardInfo->cardModel, pCardInfo->iRawPowerLevel);
      if ( ! pCS->iFixedTxPower )
         sprintf(szTxPower, "Auto");
      else if ( iCardMw < 1000 )
         sprintf(szTxPower, "%d mW", iCardMw);
      else
         sprintf(szTxPower, "%.1f W", (float)iCardMw/1000.0);
      float xLine1 = xLineCtrl + height_text*0.2;
      float yLine1 = yLineCtrl - height_text*1.2;
      if ( iArrowCount <= iCountInterfacesAssignedToThisLink/2 )
         g_pRenderEngine->drawText(xLineCtrl + height_text*0.2, yLineCtrl - height_text*1.2, g_idFontMenuSmall, szTxPower);
      else
      {
         g_pRenderEngine->drawText(xLineCtrl + height_text*0.2, yLineMarginCtrl - height_text*1.2, g_idFontMenuSmall, szTxPower);
         yLine1 = yLineMarginCtrl - height_text*1.2;
      }
      if ( ! pCS->iFixedTxPower )
      {
         int iCardPowerMwNow = tx_power_compute_uplink_power_for_model_link(g_pCurrentModel, iVehicleRadioLink, i, pCardInfo->cardModel);
         if ( iCardPowerMwNow < 1000 )
            sprintf(szTxPower, "%d mW", iCardPowerMwNow);
         else
            sprintf(szTxPower, "%.1f W", (float)iCardPowerMwNow/1000.0);
         g_pRenderEngine->drawText(xLine1, yLine1 + height_text_small, m_iIdFontSmall, szTxPower);
      }
      if ( bCanUplink )
      {
         float dx = xLineVeh - xLineCtrl;
         float dy = yLineVeh - yLineCtrl;
         float fLength = sqrtf(dx*dx+dy*dy);
         float x1 = xLineMarginCtrl - dx*0.01/fLength;
         float y1 = yLineMarginCtrl - dy*0.01/fLength;
         float xp[3], yp[3];
         xp[0] = xLineMarginCtrl;
         yp[0] = yLineMarginCtrl;
         osd_rotate_point(x1, y1, xLineMarginCtrl, yLineMarginCtrl, 26, &xp[1], &yp[1]);
         osd_rotate_point(x1, y1, xLineMarginCtrl, yLineMarginCtrl, -26, &xp[2], &yp[2]);
         g_pRenderEngine->drawTriangle(xp[0], yp[0], xp[1], yp[1], xp[2], yp[2]);
         g_pRenderEngine->fillTriangle(xp[0], yp[0], xp[1], yp[1], xp[2], yp[2]);
      }
      if ( bCanDownlink )
      {
         float dx = xLineVeh - xLineCtrl;
         float dy = yLineVeh - yLineCtrl;
         float fLength = sqrtf(dx*dx+dy*dy);         
         float x1 = xLineMarginVeh + dx*0.01/fLength;
         float y1 = yLineMarginVeh + dy*0.01/fLength;
         float xp[3], yp[3];
         xp[0] = xLineMarginVeh;
         yp[0] = yLineMarginVeh;
         osd_rotate_point(x1, y1, xLineMarginVeh, yLineMarginVeh, 26, &xp[1], &yp[1]);
         osd_rotate_point(x1, y1, xLineMarginVeh, yLineMarginVeh, -26, &xp[2], &yp[2]);
         g_pRenderEngine->drawTriangle(xp[0], yp[0], xp[1], yp[1], xp[2], yp[2]);
         g_pRenderEngine->fillTriangle(xp[0], yp[0], xp[1], yp[1], xp[2], yp[2]);
      }

      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
     
      dyArrow += arrowSpacing;
   }

   return yPos - yStart;
}


float MenuRadioConfig::drawRadioInterfaceController(float xStart, float xEnd, float yStart, int iRadioLink, int iRadioInterface)
{
   float height_text = g_pRenderEngine->textHeight(m_iIdFontRegular);

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   int iCurrentMenuItemCommand = (int)(m_uCommandsIds[m_iIndexCurrentItem] & 0xFF);
   int iCurrentMenuItemExtraParam = (int)(m_uCommandsIds[m_iIndexCurrentItem] >> 8);

   bool bSelected = false;
   
   if ( (iCurrentMenuItemCommand == MRC_ID_CONFIGURE_RADIO_INTERFACE_CONTROLLER) && (iCurrentMenuItemExtraParam == iRadioInterface) )
      bSelected = true;

   if ( (!m_bShowOnlyControllerUnusedInterfaces) && (NULL != g_pCurrentModel) && g_bFirstModelPairingDone && (g_SM_RadioStats.radio_interfaces[iRadioInterface].assignedLocalRadioLinkId >= 0) )
      return 0.0;

   drawRadioInterfaceCtrlInfo(xStart - 0.03/g_pRenderEngine->getAspectRatio(), 1.0, yStart, iRadioLink, iRadioInterface, bSelected);
   float yPos = yStart;
   yPos += height_text*1.2;
   yPos += height_text*1.1;
   yPos += height_text;

   yPos += height_text*1.5;
   return yPos - yStart;
}

float MenuRadioConfig::drawRadioInterfaceCtrlInfo(float xStart, float xEnd, float yStart, int iRadioLink, int iRadioInterface, bool bSelected)
{
   float height_text = g_pRenderEngine->textHeight(m_iIdFontRegular);
   float hIcon = height_text*2.4;
   float fPaddingInnerY = 0.02;
   float fPaddingInnerX = fPaddingInnerY/g_pRenderEngine->getAspectRatio();
   
   float yPos = yStart;
   float fMaxWidth = 0.0;

   char szBuff[128];
   char szError[128];

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   bool bShowRed = false;
   bool bBBox = false;

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterface);
   if ( NULL == pRadioHWInfo )
   {
      sprintf(szBuff, "Can't get info about radio interface %d.", iRadioInterface+1);
      g_pRenderEngine->drawTextLeft(xStart, yPos, m_iIdFontRegular, szBuff);
      return g_pRenderEngine->textWidth(m_iIdFontRegular, szBuff);
   }

   t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
   if ( NULL == pCardInfo )
   {
      sprintf(szBuff, "Can't get info about radio interface %d.", iRadioInterface+1);
      g_pRenderEngine->drawTextLeft(xStart, yPos, m_iIdFontRegular, szBuff);
      return g_pRenderEngine->textWidth(m_iIdFontRegular, szBuff);
   }

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   
   bShowRed = false;

   if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      bShowRed = true;
   if ( !(pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
   if ( !(pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
      bShowRed = true;
      
   m_fYPosCtrlRadioInterfaces[iRadioInterface] = yPos;

   g_pRenderEngine->drawIcon(xStart - hIcon/g_pRenderEngine->getAspectRatio(), yPos, hIcon/g_pRenderEngine->getAspectRatio(), hIcon, g_idIconRadio);
      
   float xTextLeft = xStart - hIcon/g_pRenderEngine->getAspectRatio() - fPaddingInnerX;
   sprintf(szBuff, "Interface %d, Port %s", iRadioInterface+1, pRadioHWInfo->szUSBPort);
   if ( ! controllerIsCardInternal(pRadioHWInfo->szMAC) )
      strcat(szBuff, " (Ext)");

   if ( bSelected )
      bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
      
   g_pRenderEngine->drawTextLeft(xTextLeft, yPos, m_iIdFontRegular, szBuff);
   fMaxWidth = g_pRenderEngine->textWidth(m_iIdFontRegular, szBuff);

   if ( bSelected )
      g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

   yPos += height_text*1.2;

   //char szBands[128];
   //str_get_supported_bands_string(pRadioHWInfo->supportedBands, szBands);
   //snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Type: %s,  %s", str_get_radio_card_model_string(pCardInfo->cardModel), szBands);
   //if ( strlen(str_get_radio_card_model_string(pCardInfo->cardModel)) > 10 )
   //   snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s,  %s", str_get_radio_card_model_string(pCardInfo->cardModel), szBands);
   snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Type: %s", str_get_radio_card_model_string(pCardInfo->cardModel));
   g_pRenderEngine->drawTextLeft(xTextLeft, yPos, m_iIdFontSmall, szBuff);
   if ( g_pRenderEngine->textWidth(m_iIdFontSmall, szBuff) > fMaxWidth )
      fMaxWidth = g_pRenderEngine->textWidth(m_iIdFontSmall, szBuff);
   yPos += height_text*1.1;

   szBuff[0] = 0;
   szError[0] = 0;

   if ( (!(pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA)) &&
        (!(pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO)) )
   {
      bShowRed = true;
      strcpy(szError, "No streams enabled");
   }

   if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
   if ( ! (pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
      strcat(szBuff, "Uplink Only");
   if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
   if ( ! (pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
      strcat(szBuff, "Downlink Only");
   if ( ! (pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
   if ( ! (pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
   {
      bShowRed = true;
      strcpy(szError, "Neither Uplink or Downlink enabled");
   }

   if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_DISABLED )
   {
      bShowRed = true;
      strcpy(szError, "Card Is Disabled");
   }

   float ftw = g_pRenderEngine->textWidth(m_iIdFontRegular, szBuff);
   g_pRenderEngine->drawTextLeft(xTextLeft, yPos, m_iIdFontRegular, szBuff);
   if ( ftw > fMaxWidth )
      fMaxWidth = ftw;

   if ( bShowRed && (0 != szError[0]) )
   {
      g_pRenderEngine->setColors(get_Color_IconError());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      g_pRenderEngine->drawTextLeft(xTextLeft - ftw - 0.01, yPos, m_iIdFontRegular, szError);
      if ( ftw + 0.01 + g_pRenderEngine->textWidth(m_iIdFontRegular, szError) > fMaxWidth )
         fMaxWidth = ftw + 0.01 + g_pRenderEngine->textWidth(m_iIdFontRegular, szError);
   }

   yPos += height_text;
   yPos += height_text*1.5;

   return (xTextLeft-xStart) + fMaxWidth;
}
