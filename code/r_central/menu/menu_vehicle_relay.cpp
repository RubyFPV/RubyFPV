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
#include "menu_vehicle.h"
#include "menu_vehicle_general.h"
#include "menu_vehicle_video.h"
#include "menu_vehicle_camera.h"
#include "menu_vehicle_osd.h"
#include "menu_vehicle_expert.h"
#include "menu_confirmation.h"
#include "menu_vehicle_telemetry.h"
#include "menu_vehicle_relay.h"
#include "../osd/osd_common.h"
#include "../../common/models_connect_frequencies.h"
#include <ctype.h>

MenuVehicleRelay::MenuVehicleRelay(void)
:Menu(MENU_ID_VEHICLE_RELAY, "Relay Settings", NULL)
{
   m_Width = 0.44;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.3;
   m_bIsConfigurable = false;
   m_iCountVehiclesIDs = 0;
   m_szRelayError[0] = 0;
   m_szRelayError2[0] = 0;

   m_fHeightHeader = 0.16;
   addExtraHeightAtStart(m_fHeightHeader);

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      m_bCurrentVehicleRelayCapableLinks[i] = false;

   m_iCountCurrentVehicleRelayCapableLinks = 0;

   m_IndexBack = -1;
   hardware_load_radio_info();
}

MenuVehicleRelay::~MenuVehicleRelay()
{
}

void MenuVehicleRelay::onShow()
{
   m_Height = 0.0;
   char szBuff[256];
   removeAllItems();
   removeAllTopLines();

   m_IndexRadioLink = -1;
   m_IndexQAButton = -1;
   m_IndexVehicle = -1;
   m_IndexBack = -1;
   m_bIsConfigurable = false;

   int countCardsCapableOnVehicle = 0;
   u32 uTotalVehicleSupportedBands = 0;
   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( 0 != g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] )
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY )
      {
         uTotalVehicleSupportedBands |= g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i];
         countCardsCapableOnVehicle++;
      }
   }

   if ( (g_pCurrentModel->radioLinksParams.links_count < 2) || (countCardsCapableOnVehicle < 2) )
   {
      addTopLine("You can not enable relay functionality on this vehicle!");
      addTopLine("You need at least two high capacity radio interfaces on the vehicle to be able to relay from this vehicle.");
      m_IndexBack = addMenuItem(new MenuItem("Ok"));
      Menu::onShow();
      return;
   }

   m_bIsConfigurable = true;

   sprintf(szBuff, "Selects the vehicle to relay using the currently active vehicle: %s.", g_pCurrentModel->getLongName());
   m_pItemsSelect[0] = new MenuItemSelect(L("Vehicle To Relay"), szBuff);  
   m_pItemsSelect[0]->addSelection(L("None"));
   for( int i=0; i<getControllerModelsCount(); i++ )
   {
      Model *pModel = getModelAtIndex(i);
      if ( NULL == pModel )
         continue;
      if ( pModel->uVehicleId == g_pCurrentModel->uVehicleId )
         continue;

      u32 uAllBands = 0;
      for( int k=0; k<pModel->radioInterfacesParams.interfaces_count; k++ )
      {
         if ( 0 != pModel->radioInterfacesParams.interface_supported_bands[k] )
         if ( pModel->radioInterfacesParams.interface_capabilities_flags[k] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY )
         if ( ! (pModel->radioInterfacesParams.interface_capabilities_flags[k] & RADIO_HW_CAPABILITY_FLAG_DISABLED) )
            uAllBands |= pModel->radioInterfacesParams.interface_supported_bands[k];
      }
      if ( (uTotalVehicleSupportedBands & uAllBands) == 0 )
         continue;
      m_uVehiclesID[m_iCountVehiclesIDs] = pModel->uVehicleId;
      m_iCountVehiclesIDs++;

      char szName[128];
      strncpy(szName, pModel->getLongName(), 127);
      szName[0] = toupper(szName[0]);

      sprintf(szBuff,"%s, %s", szName, str_format_frequency(pModel->radioLinksParams.link_frequency_khz[0]));
      m_pItemsSelect[0]->addSelection(szBuff);
   }

   m_pItemsSelect[0]->setIsEditable();
   m_IndexVehicle = addMenuItem(m_pItemsSelect[0]);
   
   m_pItemsSelect[1] = new MenuItemSelect("Relay Radio Link", "Selects which radio link from current vehicle to use to connect to the relayed vehicle.");
   m_pItemsSelect[1]->addSelection("None");
   m_pItemsSelect[1]->setIsEditable();
   m_IndexRadioLink = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSelect[2] = new MenuItemSelect("Relay Type", "Selects which radio streams to relay. If everything selected then video, telemetry and commands are relayed.");
   m_pItemsSelect[2]->addSelection("Video Only");
   m_pItemsSelect[2]->addSelection("Telemetry Only");
   m_pItemsSelect[2]->addSelection("Video & Telemetry");
   m_pItemsSelect[2]->addSelection("Everything");
   m_pItemsSelect[2]->setIsEditable();
   m_IndexRelayType = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSelect[3] = new MenuItemSelect("Relay Switching", "Selects the QA button to be used to switch between this vehicle and the relayed one or enable permanent relaying of the vehicle.");  
   m_pItemsSelect[3]->addSelection(L("Disabled"));
   m_pItemsSelect[3]->addSelection(L("QA Button 1"));
   m_pItemsSelect[3]->addSelection(L("QA Button 2"));
   m_pItemsSelect[3]->addSelection(L("QA Button 3"));
   m_pItemsSelect[3]->addSelection(L("Permanent Relaying"));
   m_pItemsSelect[3]->setIsEditable();
   m_IndexQAButton = addMenuItem(m_pItemsSelect[3]);

   addSection("Relaying Options");

   m_pItemsSelect[4] = new MenuItemSelect("OSD Switch", "Choose if the OSD layout should change when switching between relayed vehicles.");
   m_pItemsSelect[4]->addSelection("Keep Main OSD layout");
   m_pItemsSelect[4]->addSelection("Switch OSD Layout");
   m_pItemsSelect[4]->setUseMultiViewLayout();
   m_IndexOSDSwitch = addMenuItem(m_pItemsSelect[4]);

   m_pItemsSelect[5] = new MenuItemSelect("OSD Merge", "Merging OSD will fuse the OSD info from all relayed vehicles into the current OSD screen.");
   m_pItemsSelect[5]->addSelection("Off");
   m_pItemsSelect[5]->addSelection("On");
   m_pItemsSelect[5]->setUseMultiViewLayout();
   m_IndexOSDMerge = addMenuItem(m_pItemsSelect[5]);

   Menu::onShow();

   if ( g_pCurrentModel->rc_params.rc_enabled )
   if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotFCTelemetry )
   if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.uFCFlags & FC_TELE_FLAGS_ARMED )
      addMessage("RC Link is enabled and vehicle is Armed. We do not recomend chaning the relay settings while Armed. It has an impact on your radio links.");
}


void MenuVehicleRelay::valuesToUI()
{
   Preferences* pP = get_Preferences();
   
   if ( (NULL == pP) || (!m_bIsConfigurable) )
      return;

   m_pItemsSelect[0]->setSelection(0);
   bool bCheckOk = true;

   if ( g_pCurrentModel->relay_params.uRelayedVehicleId != 0 )
   if ( g_pCurrentModel->relay_params.uRelayFrequencyKhz != 0 )
   {
      Model *pRelayedModel = findModelWithId(g_pCurrentModel->relay_params.uRelayedVehicleId, 18);
      if ( NULL == pRelayedModel )
      {
         log_softerror_and_alarm("MenuVehicleRelay: valuestoUI: Relay is enabled on inexistent VID: %u. Disabling relaying.", g_pCurrentModel->relay_params.uRelayedVehicleId);
         bCheckOk = false;
      }

      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId < 0 )
         bCheckOk = false;

      if ( bCheckOk )
         bCheckOk = _check_if_vehicle_can_be_relayed(g_pCurrentModel->relay_params.uRelayedVehicleId);

      if ( ! bCheckOk )
      {
         log_softerror_and_alarm("MenuVehicleRelay: valuestoUI: Relay checks failed. Disabling relaying.");
         type_relay_parameters params;
         memcpy((u8*)&params, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));
         g_pCurrentModel->resetRelayParamsToDefaults(&params); 
         handle_commands_send_to_vehicle(COMMAND_ID_SET_RELAY_PARAMETERS, 0, (u8*)&params, sizeof(type_relay_parameters));
      }
      else
      {
         for( int i=0; i<m_iCountVehiclesIDs; i++ )
         {
            if ( m_uVehiclesID[i] != g_pCurrentModel->relay_params.uRelayedVehicleId )
               continue;
            m_pItemsSelect[0]->setSelectedIndex(i+1);
            break;
         }
      }
   }

   if ( 0 == m_pItemsSelect[0]->getSelectedIndex() )
   {
      m_pItemsSelect[1]->removeAllSelections();
      m_pItemsSelect[1]->addSelection("None");
      m_pItemsSelect[1]->setSelection(0);
      m_pItemsSelect[1]->setEnabled(false);
      m_pItemsSelect[2]->setEnabled(false);
      m_pItemsSelect[3]->setEnabled(false);
      m_pItemsSelect[4]->setEnabled(false);
      m_pItemsSelect[5]->setEnabled(false);
      return;
   }

   m_pItemsSelect[1]->setEnabled(true);
   m_pItemsSelect[2]->setEnabled(true);
   m_pItemsSelect[3]->setEnabled(true);
   m_pItemsSelect[4]->setEnabled(true);
   m_pItemsSelect[5]->setEnabled(true);


   m_pItemsSelect[1]->removeAllSelections();
   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
      if ( m_bCurrentVehicleRelayCapableLinks[i] )
      {
         char szBuff[128];
         sprintf(szBuff, "Radio Link %d", i+1);
         m_pItemsSelect[1]->addSelection(szBuff);

         if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == i )
            m_pItemsSelect[1]->setSelectedIndex(i);
      }
   }
   if ( 0 < m_pItemsSelect[1]->getSelectionsCount() )
      m_pItemsSelect[1]->setIsEditable();

   m_pItemsSelect[2]->setSelectedIndex(0);
   if ( 0 == g_pCurrentModel->relay_params.uRelayCapabilitiesFlags )
      m_pItemsSelect[2]->setEnabled(false);
   else
   {
      m_pItemsSelect[2]->setEnabled(true);
      if ( (g_pCurrentModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_VIDEO) &&
           (g_pCurrentModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_TELEMETRY) &&
           (g_pCurrentModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_COMMANDS) )
         m_pItemsSelect[2]->setSelectedIndex(3);
      else if ( (g_pCurrentModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_VIDEO) &&
           (g_pCurrentModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_TELEMETRY) )
         m_pItemsSelect[2]->setSelectedIndex(2);
      else if ( g_pCurrentModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_VIDEO )
         m_pItemsSelect[2]->setSelectedIndex(0);
      else if ( g_pCurrentModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_TELEMETRY )
         m_pItemsSelect[2]->setSelectedIndex(1);
   }

   m_pItemsSelect[3]->setSelection(0);

   if ( pP->iActionQuickButton1 == quickActionRelaySwitch )
       m_pItemsSelect[3]->setSelectedIndex(1);
   if ( pP->iActionQuickButton2 == quickActionRelaySwitch )
       m_pItemsSelect[3]->setSelectedIndex(2);
   if ( pP->iActionQuickButton3 == quickActionRelaySwitch )
       m_pItemsSelect[3]->setSelectedIndex(3);
   if ( g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_PERMANENT_REMOTE )
       m_pItemsSelect[3]->setSelectedIndex(4);

   m_pItemsSelect[4]->setSelectedIndex(0);
   if ( g_pCurrentModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_SWITCH_OSD )
      m_pItemsSelect[4]->setSelectedIndex(1);

   m_pItemsSelect[5]->setSelectedIndex(0);
   if ( g_pCurrentModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_MERGE_OSD )
      m_pItemsSelect[5]->setSelectedIndex(1);
}


void MenuVehicleRelay::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle(); 

   _drawHeader(yTop - m_fHeightHeader - 0.4*m_sfMenuPaddingY);
   float y = yTop;
 
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehicleRelay::_drawHeader(float yPos)
{
   if ( ! m_bIsConfigurable )
      return;

   char szBuff[128];
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float height_text_large = g_pRenderEngine->textHeight(g_idFontMenuLarge);
   float hPixel = g_pRenderEngine->getPixelHeight();
   
   float fXStart = m_xPos + m_sfMenuPaddingX;
   float fXEnd = m_xPos + m_Width - 2.0 * m_sfMenuPaddingX;
   float fMarginY = 0.02;
   float hIconCtrlr = m_fHeightHeader - height_text_large - fMarginY;
   float hIcon = hIconCtrlr*0.7;
   float wIcon = hIcon/g_pRenderEngine->getAspectRatio();
   float fXMid = fXStart + 0.5*(m_Width - 2.0*m_sfMenuPaddingX);
   float fSpacingX = 0.02/g_pRenderEngine->getAspectRatio();
   float yMidLine = yPos + 0.5*hIconCtrlr;

   u32 uIdIconVehicle = g_idIconDrone;
   if ( NULL != g_pCurrentModel )
      uIdIconVehicle = osd_getVehicleIcon( g_pCurrentModel->vehicle_type );
   
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   float xPos = fXStart;
   g_pRenderEngine->drawIcon(xPos, yPos, hIconCtrlr/g_pRenderEngine->getAspectRatio(), hIconCtrlr, g_idIconController);
   g_pRenderEngine->drawText(xPos, yPos + hIconCtrlr, g_idFontMenu, "Controller");

   xPos += hIconCtrlr/g_pRenderEngine->getAspectRatio() + fSpacingX;

   g_pRenderEngine->drawLine(xPos, yMidLine, fXMid - 0.5 * wIcon - fSpacingX, yMidLine);
   g_pRenderEngine->drawLine(xPos, yMidLine + hPixel, fXMid - 0.5 * wIcon - fSpacingX, yMidLine + hPixel);

   float fXMidLine = ((fXMid - 0.5 * wIcon - fSpacingX) + xPos) * 0.5;

   //log_line("MenuVehicleRelay: Draw header for current relay params: relayed VID: %u, relay radio link: %d, freq: %s",
   //   g_pCurrentModel->relay_params.uRelayedVehicleId, g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId+1, str_format_frequency(g_pCurrentModel->relay_params.uRelayFrequencyKhz));

   int iLinkDrawCount = 0;
   for( int iLink=0; iLink<g_pCurrentModel->radioLinksParams.links_count; iLink++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iLink] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY) )
         continue;

      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == iLink )
         continue;

      bool bUsedOnController = false;
      for( int iInterface=0; iInterface < hardware_get_radio_interfaces_count(); iInterface++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[iInterface].assignedVehicleRadioLinkId == iLink )
         {
            bUsedOnController = true;
            break;
         }
      }

      if ( ! bUsedOnController )
         continue;

      sprintf(szBuff, "%d: %s", iLink+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iLink]) );
      float fW = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
      if ( 0 == iLinkDrawCount )
         g_pRenderEngine->drawText(fXMidLine - 0.5*fW, yMidLine - height_text*1.1, g_idFontMenu, szBuff);
      else if ( 1 == iLinkDrawCount )
         g_pRenderEngine->drawText(fXMidLine - 0.5*fW, yMidLine + 0.1*height_text, g_idFontMenu, szBuff);    

      iLinkDrawCount++;
   }
   
   xPos = fXMid - wIcon * 0.5;

   // ---------------------------------------------
   // Draw current vehicle

   g_pRenderEngine->drawIcon(xPos, yMidLine - 0.5*hIcon, wIcon, hIcon, uIdIconVehicle);

   if ( NULL == g_pCurrentModel )
      strcpy(szBuff, "No Vehicle Active");
   else
      strcpy(szBuff, g_pCurrentModel->getLongName());

   szBuff[0] = toupper(szBuff[0]);
   float fW = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
   g_pRenderEngine->drawText(fXMid - fW*0.5, yMidLine + 0.6*hIcon, g_idFontMenu, szBuff);

   xPos = fXMid + wIcon*0.5 + fSpacingX;

   fXMidLine = ((fXEnd - wIcon - fSpacingX) + xPos) * 0.5;

   // ------------------------------------------------
   // Draw relayed vehicle

   if ( (g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId < 0) ||
        (g_pCurrentModel->relay_params.uRelayedVehicleId == 0) ||
        (g_pCurrentModel->relay_params.uRelayFrequencyKhz == 0) )
   {   
      strcpy(szBuff, "No Relayed");
      float fW = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
      g_pRenderEngine->drawText(fXMidLine - 0.5*fW, yMidLine - height_text*0.9, g_idFontMenu, szBuff);

      strcpy(szBuff, "Vehicle Selected");
      fW = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
      g_pRenderEngine->drawText(fXMidLine - 0.5*fW, yMidLine + height_text*0.1, g_idFontMenu, szBuff);

      g_pRenderEngine->drawIcon( fXEnd -wIcon, yMidLine - hIcon*0.5, wIcon, hIcon, g_idIconX);
      return;
   }
/*
   Model* pRelayedModel = findModelWithId(g_pCurrentModel->relay_params.uRelayedVehicleId, 19);
   
   if ( NULL == pRelayedModel )
   {
      strcpy(szBuff, "Invalid Relayed");
      float fW = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
      g_pRenderEngine->drawText(xPos + 0.5*fSpacing - 0.5*fW, yMidLine - height_text*0.9, g_idFontMenu, szBuff);

      strcpy(szBuff, "Vehicle Selected");
      fW = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
      g_pRenderEngine->drawText(xPos + 0.5*fSpacing - 0.5*fW, yMidLine + height_text*0.1, g_idFontMenu, szBuff);

      xPos += fSpacing + 0.02/g_pRenderEngine->getAspectRatio();
      g_pRenderEngine->drawIcon(xPos, yMidLine - hIcon*0.5, hIcon/g_pRenderEngine->getAspectRatio(), hIcon, g_idIconX);  
      return;
   }

   uIdIconVehicle = g_idIconDrone;
   if ( NULL != pRelayedModel )
      uIdIconVehicle = osd_getVehicleIcon( pRelayedModel->vehicle_type );

   g_pRenderEngine->drawLine(xPos, yMidLine, xPos+fSpacing, yMidLine);

   strcpy(szBuff, str_format_frequency(g_pCurrentModel->relay_params.uRelayFrequencyKhz) );
   fW = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
   g_pRenderEngine->drawText(xPos + 0.5*fSpacing - 0.5*fW, yMidLine - height_text*1.1, g_idFontMenu, szBuff);

   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
   {
      sprintf(szBuff, "Link %d", g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId+1);

      fW = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
      g_pRenderEngine->drawText(xPos + 0.5*fSpacing - 0.5*fW, yMidLine + 0.1*height_text, g_idFontMenu, szBuff);
   }

   u32 uRelayedVehicleFreq = 0;
   for( int i=0; i<pRelayedModel->radioLinksParams.links_count; i++ )
   {
      if ( pRelayedModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( ! (pRelayedModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY) )
         continue;
      uRelayedVehicleFreq = pRelayedModel->radioLinksParams.link_frequency_khz[i];
      break;
   }

   if ( uRelayedVehicleFreq != g_pCurrentModel->relay_params.uRelayFrequencyKhz )
   {
      sprintf(szBuff, "Now at %s !", str_format_frequency(uRelayedVehicleFreq) );
      fW = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
      g_pRenderEngine->drawText(xPos + 0.5*fSpacing - 0.5*fW, yMidLine + height_text*0.1, g_idFontMenu, szBuff);
   }
   xPos += fSpacing + 0.02/g_pRenderEngine->getAspectRatio();
   
   g_pRenderEngine->drawIcon(xPos, yMidLine - 0.5*hIcon, hIcon/g_pRenderEngine->getAspectRatio(), hIcon, uIdIconVehicle);

   if ( NULL == pRelayedModel )
      strcpy(szBuff, "No Relay Vehicle");
   else
      strcpy(szBuff, pRelayedModel->getLongName());

   szBuff[0] = toupper(szBuff[0]);
   fW = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
   g_pRenderEngine->drawText(xPos-fW*0.5 + 0.5*hIcon/g_pRenderEngine->getAspectRatio(), yMidLine + 0.6*hIcon, g_idFontMenu, szBuff);
   */
}

int MenuVehicleRelay::onBack()
{
   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
   if ( m_pItemsSelect[3]->getSelectedIndex() <= 0 )
   {
      addMessage("You have not assigned a Quick Action button to do relay switching. You will have no way to switch between vehicles.");
      return 1;
   }
   return Menu::onBack();
}

bool MenuVehicleRelay::_check_if_vehicle_can_be_relayed(u32 uVehicleId)
{
   m_szRelayError[0] = 0;
   m_szRelayError2[0] = 0;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      m_bCurrentVehicleRelayCapableLinks[i] = false;

   m_iCountCurrentVehicleRelayCapableLinks = 0;

   log_line("MenuVehicleRelay: Checking if vehicle VID %u can be relayed.", uVehicleId);

   Model *pRelayedModel = findModelWithId(uVehicleId, 20);
   if ( NULL == pRelayedModel )
   {
      strcpy(m_szRelayError, "Invalid vehicle selected.");
      strcpy(m_szRelayError2, "That vehicle is invalid. Something went wrong. Please contact developers.");
      return false;
   }

   // Find first usable high capacity radio link of the relayed vehicle
   u32 uFrequencyRelayedVehicle = 0;

   for( int i=0; i<pRelayedModel->radioLinksParams.links_count; i++ )
   {
      if ( pRelayedModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( ! ( pRelayedModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY) )
         continue;
      uFrequencyRelayedVehicle = pRelayedModel->radioLinksParams.link_frequency_khz[i];
      break;
   }

   if ( 0 == uFrequencyRelayedVehicle )
   {
      strcpy(m_szRelayError, "No usable radio links");
      snprintf(m_szRelayError2, sizeof(m_szRelayError2)/sizeof(m_szRelayError2[0]), "Your %s has no usable high capacity radio links to connect to as a relayed node.", 
         pRelayedModel->getLongName());
      return false;
   }

   u32 uMainControllerFreq = get_model_main_connect_frequency(g_pCurrentModel->uVehicleId);

   char szNameRelayVehicle[128];
   char szNameRelayedVehicle[128];
   char szFreqMainRelayVehicle[128];
   char szFreqRelayedVehicle[128];

   strcpy(szNameRelayVehicle, g_pCurrentModel->getLongName());
   strcpy(szNameRelayedVehicle, pRelayedModel->getLongName());
   strcpy(szFreqMainRelayVehicle, str_format_frequency(uMainControllerFreq));
   strcpy(szFreqRelayedVehicle, str_format_frequency(uFrequencyRelayedVehicle));

   // Can't relay vehicles that use the same freq as main connect freq
   if ( uMainControllerFreq == uFrequencyRelayedVehicle )
   {
      strcpy(m_szRelayError, "Conflicting frequencies");
      snprintf(m_szRelayError2, sizeof(m_szRelayError2)/sizeof(m_szRelayError2[0]), "Your %s uses the same frequency (%s) as your controller and current vehicle (%s). You need first to connect to %s and change it's frequency or change the current vehicle frequency.", 
         szNameRelayedVehicle, szFreqRelayedVehicle, szFreqMainRelayVehicle, szNameRelayedVehicle);
      return false;
   }

   // First compute capabilities of current vehicle radio links and conflicts with relayed vehicle
   bool bVehicleLinkIsHighCapacity[MAX_RADIO_INTERFACES];
   bool bVehicleLinkSupportsRelayedFrequency[MAX_RADIO_INTERFACES];
   bool bVehicleLinkConflictsFrequencies[MAX_RADIO_INTERFACES];

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      m_bCurrentVehicleRelayCapableLinks[i] = false;
      bVehicleLinkIsHighCapacity[i] = false;
      bVehicleLinkSupportsRelayedFrequency[i] = false;
      bVehicleLinkConflictsFrequencies[i] = true;
   }

   for( int iLink=0; iLink<g_pCurrentModel->radioLinksParams.links_count; iLink++ )
   {
      m_bCurrentVehicleRelayCapableLinks[iLink] = false;
      bVehicleLinkIsHighCapacity[iLink] = false;
      bVehicleLinkSupportsRelayedFrequency[iLink] = false;
      bVehicleLinkConflictsFrequencies[iLink] = false;

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      
      u32 uLinkSupportedBands = 0;
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] != iLink )
            continue;
         if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            continue;
         if ( ! (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY) )
            continue;
         bVehicleLinkIsHighCapacity[iLink] = true;
         uLinkSupportedBands |= g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i];
      }

      if ( ! bVehicleLinkIsHighCapacity[iLink] )
         continue;

      // Does this vehicle link supports the relayed vehicle frequency?
      if ( getBand(uFrequencyRelayedVehicle) & uLinkSupportedBands )
            bVehicleLinkSupportsRelayedFrequency[iLink] = true;

      if ( ! bVehicleLinkSupportsRelayedFrequency[iLink] )
         continue;

      m_bCurrentVehicleRelayCapableLinks[iLink] = true;

      // If this link is the main vehicle link, skip it
      if ( g_pCurrentModel->radioLinksParams.link_frequency_khz[iLink] == uMainControllerFreq )
      {
         bVehicleLinkConflictsFrequencies[iLink] = true;
         continue;
      }

      m_iCountCurrentVehicleRelayCapableLinks++;
   }

   log_line("MenuVehicleRelay: Relaying capable links count: %d", m_iCountCurrentVehicleRelayCapableLinks);
   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
      log_line("MenuVehicleRelay: Current vehicle radio link %d: is high capacity: %s, supports %s frequency: %s, is relay capable: %s, usable: %s",
       i+1, (bVehicleLinkIsHighCapacity[i]?"Yes":"No"),
       str_format_frequency(uFrequencyRelayedVehicle),
       (bVehicleLinkSupportsRelayedFrequency[i]?"Yes":"No"),
       (m_bCurrentVehicleRelayCapableLinks[i]?"Yes":"No"),
       bVehicleLinkConflictsFrequencies[i]?"no (main controller link freq)":"yes"
       );  
   }

   if ( 0 == m_iCountCurrentVehicleRelayCapableLinks )
   {
      log_line("MenuVehicleRelay: No radio links can be enabled as relay links.");
      bool bNoHighCapacity = true;
      bool bSupportsFrequency = false;
      bool bFrequencyConflict = false;
      for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
      {
         if ( bVehicleLinkIsHighCapacity[i] )
            bNoHighCapacity = false;
         if ( bVehicleLinkSupportsRelayedFrequency[i] )
            bSupportsFrequency = true;
         if ( bVehicleLinkConflictsFrequencies[i] )
            bFrequencyConflict = true;
      }

      sprintf(m_szRelayError, "Radio Links Validation Error");
      sprintf(m_szRelayError2, "%s can't be relayed.", szNameRelayedVehicle);
      if ( bNoHighCapacity )
         sprintf(m_szRelayError2, "No high datarate capacity links present on the current vehicle.");
      else if ( ! bSupportsFrequency )
         sprintf(m_szRelayError2, "No radio links on current vehicle supports the relayed vehicle radio frequency of %s", szFreqRelayedVehicle);
      else if ( bFrequencyConflict )
         snprintf(m_szRelayError2, sizeof(m_szRelayError2)/sizeof(m_szRelayError2[0]), "There would be a conflict of frequencies (both vehicles would use the same frequency %s) if relay would be enabled to %s", szFreqRelayedVehicle, szNameRelayedVehicle);
      else
         sprintf(m_szRelayError2, "The remaining current vehicle's radio links would not be able to connect to controller anymore.");
      return false;
   }

   log_line("MenuVehicleRelay: %d radio links can be enabled as relay links.", m_iCountCurrentVehicleRelayCapableLinks );
   return true;
}

void MenuVehicleRelay::_onSelectedVehicle(int iVehicleIndex)
{
   if ( 0 == iVehicleIndex )
   {
      type_relay_parameters params;
      memcpy((u8*)&params, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));
      
      if ( (params.isRelayEnabledOnRadioLinkId >= 0) ||
           (params.uRelayedVehicleId != 0) ||
           (params.uRelayFrequencyKhz != 0) ||
           (params.uCurrentRelayMode != 0) )
      {
         g_pCurrentModel->resetRelayParamsToDefaults(&params);
         
         if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RELAY_PARAMETERS, 0, (u8*)&params, sizeof(type_relay_parameters)) )
            valuesToUI();
      }
      valuesToUI();
      return;
   }
   if ( ! _check_if_vehicle_can_be_relayed(m_uVehiclesID[iVehicleIndex-1]) )
   {
      if ( 0 != m_szRelayError2[0] )
         addMessageWithTitle(0, m_szRelayError, m_szRelayError2);
      else
         addMessage(m_szRelayError);
      valuesToUI();
      return;
   }

   Model *pRelayedModel = findModelWithId(m_uVehiclesID[iVehicleIndex-1], 21);
   if ( NULL == pRelayedModel )
   {
      addMessage("Invalid vehicle selected.");
      return;
   }

   type_relay_parameters params;
   memcpy((u8*)&params, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));

   params.uRelayedVehicleId = m_uVehiclesID[iVehicleIndex-1];
   params.uRelayFrequencyKhz = 0;
   params.isRelayEnabledOnRadioLinkId = -1;

   for( int i=0; i<pRelayedModel->radioLinksParams.links_count; i++ )
   {
      if ( pRelayedModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( ! ( pRelayedModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY) )
         continue;
      params.uRelayFrequencyKhz = pRelayedModel->radioLinksParams.link_frequency_khz[i];
      break;
   }

   u32 uMainControllerFreq = get_model_main_connect_frequency(g_pCurrentModel->uVehicleId);
   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
      if ( m_bCurrentVehicleRelayCapableLinks[i] )
      if ( g_pCurrentModel->radioLinksParams.link_frequency_khz[i] != uMainControllerFreq )
      {
         params.isRelayEnabledOnRadioLinkId = i;
         break;
      }
   }

   if ( (params.isRelayEnabledOnRadioLinkId < 0) || (params.uRelayFrequencyKhz == 0) )
   {
      addMessageWithTitle(0, "Invalid params", "Something went wrong. Please contact developers.");
      return;
   }

   params.uRelayCapabilitiesFlags = RELAY_CAPABILITY_TRANSPORT_VIDEO | RELAY_CAPABILITY_TRANSPORT_TELEMETRY | RELAY_CAPABILITY_TRANSPORT_COMMANDS | RELAY_CAPABILITY_SWITCH_OSD | RELAY_CAPABILITY_MERGE_OSD;
   params.uCurrentRelayMode = RELAY_MODE_MAIN | RELAY_MODE_IS_RELAY_NODE;

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RELAY_PARAMETERS, 0, (u8*)&params, sizeof(type_relay_parameters)) )
      valuesToUI();
}


void MenuVehicleRelay::onSelectItem()
{
   Preferences* pP = get_Preferences();
   
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( (m_IndexBack != -1) && (m_IndexBack == m_SelectedIndex) )
   {
      menu_stack_pop(0);
      return;
   }

   if ( (NULL == pP) || (!m_bIsConfigurable) )
      return;

   if ( m_IndexVehicle == m_SelectedIndex )
   {
      int iIndexVehicle = m_pItemsSelect[0]->getSelectedIndex();
      _onSelectedVehicle(iIndexVehicle);
      return;
   }

   if ( m_IndexRadioLink == m_SelectedIndex )
   {
      if ( g_bReconfiguringRadioLinks )
      {
         addMessage("Please wait for the radio links reconfiguration to complete.");
         valuesToUI();
         return;
      }

      int iNewRadioLinkToUse = -1;
      int iCount = -1;
      for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
      {
         if ( m_bCurrentVehicleRelayCapableLinks[i] )
         {
            iCount++;
            if ( iCount == m_pItemsSelect[1]->getSelectedIndex() )
            {
               iNewRadioLinkToUse = i;
               break;
            }
         }
      }
      if ( iNewRadioLinkToUse == -1 )
      {
         log_softerror_and_alarm("MenuVehicleRelay: Tried to use an invalid radio link for relaying.");
         return;
      }
      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == iNewRadioLinkToUse )
         return;

      Model *pRelayedModel = findModelWithId(g_pCurrentModel->relay_params.uRelayedVehicleId, 22);
      if ( NULL == pRelayedModel )
      {
         addMessage("Invalid vehicle selected.");
         return;
      }

      type_relay_parameters params;
      memcpy((u8*)&params, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));

      params.uRelayFrequencyKhz = 0;
      for( int i=0; i<pRelayedModel->radioLinksParams.links_count; i++ )
      {
         if ( pRelayedModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            continue;
         if ( ! ( pRelayedModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY) )
            continue;
         params.uRelayFrequencyKhz = pRelayedModel->radioLinksParams.link_frequency_khz[i];
         break;
      }
      params.isRelayEnabledOnRadioLinkId = iNewRadioLinkToUse;

      if ( 0 == params.uRelayFrequencyKhz )
      {
         addMessage("No radio links from relayed vehicle can be relayed.");
         return;
      }

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RELAY_PARAMETERS, 0, (u8*)&params, sizeof(type_relay_parameters)) )
         valuesToUI();
      return;
   }

   if ( m_IndexQAButton == m_SelectedIndex )
   {
      type_relay_parameters params;
      memcpy((u8*)&params, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));

      int iIndex = m_pItemsSelect[3]->getSelectedIndex();

      if ( iIndex == 4 )
         params.uCurrentRelayMode |= RELAY_MODE_PERMANENT_REMOTE;
      else
         params.uCurrentRelayMode &= ~RELAY_MODE_PERMANENT_REMOTE;

      if ( iIndex == 1 )
         pP->iActionQuickButton1 = quickActionRelaySwitch;
      else if ( iIndex == 2 )
         pP->iActionQuickButton2 = quickActionRelaySwitch;
      else if ( iIndex == 3 )
         pP->iActionQuickButton3 = quickActionRelaySwitch;

      if ( pP->iActionQuickButton1 == quickActionRelaySwitch )
      if ( iIndex != 1 )
         pP->iActionQuickButton1 = quickActionVideoRecord;

      if ( pP->iActionQuickButton2 == quickActionRelaySwitch )
      if ( iIndex != 2 )
         pP->iActionQuickButton2 = quickActionCycleOSD;

      if ( pP->iActionQuickButton3 == quickActionRelaySwitch )
      if ( iIndex != 3 )
         pP->iActionQuickButton3 = quickActionTakePicture;

      save_ControllerSettings();
      save_Preferences();

      if ( 0 != memcmp(&params, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters)) )
      {
         if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RELAY_PARAMETERS, 0, (u8*)&params, sizeof(type_relay_parameters)) )
            valuesToUI();
      }
      return;
   }

   if ( m_IndexRelayType == m_SelectedIndex )
   {
      type_relay_parameters params;
      memcpy((u8*)&params, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));

      params.uRelayCapabilitiesFlags &= ~(RELAY_CAPABILITY_TRANSPORT_TELEMETRY | RELAY_CAPABILITY_TRANSPORT_VIDEO | RELAY_CAPABILITY_TRANSPORT_COMMANDS);
      
      if ( 0 == m_pItemsSelect[2]->getSelectedIndex() )
         params.uRelayCapabilitiesFlags |= RELAY_CAPABILITY_TRANSPORT_VIDEO;
      else if ( 1 == m_pItemsSelect[2]->getSelectedIndex() )
         params.uRelayCapabilitiesFlags |= RELAY_CAPABILITY_TRANSPORT_TELEMETRY;
      else if ( 2 == m_pItemsSelect[2]->getSelectedIndex() )
         params.uRelayCapabilitiesFlags |= RELAY_CAPABILITY_TRANSPORT_VIDEO | RELAY_CAPABILITY_TRANSPORT_TELEMETRY;
      else if ( 3 == m_pItemsSelect[2]->getSelectedIndex() )
         params.uRelayCapabilitiesFlags |= RELAY_CAPABILITY_TRANSPORT_VIDEO | RELAY_CAPABILITY_TRANSPORT_TELEMETRY | RELAY_CAPABILITY_TRANSPORT_COMMANDS;

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RELAY_PARAMETERS, 0, (u8*)&params, sizeof(type_relay_parameters)) )
         valuesToUI();
      return;
   }

   if ( m_IndexOSDSwitch == m_SelectedIndex )
   {
      type_relay_parameters params;
      memcpy((u8*)&params, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));

      params.uRelayCapabilitiesFlags &= (~RELAY_CAPABILITY_SWITCH_OSD);
      if ( 1 == m_pItemsSelect[4]->getSelectedIndex() )
         params.uRelayCapabilitiesFlags |= RELAY_CAPABILITY_SWITCH_OSD;
      
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RELAY_PARAMETERS, 0, (u8*)&params, sizeof(type_relay_parameters)) )
         valuesToUI();
      return;
   }

   if ( m_IndexOSDMerge == m_SelectedIndex )
   {
      type_relay_parameters params;
      memcpy((u8*)&params, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));

      params.uRelayCapabilitiesFlags &= (~RELAY_CAPABILITY_MERGE_OSD);
      if ( 1 == m_pItemsSelect[5]->getSelectedIndex() )
         params.uRelayCapabilitiesFlags |= RELAY_CAPABILITY_MERGE_OSD;
      
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RELAY_PARAMETERS, 0, (u8*)&params, sizeof(type_relay_parameters)) )
         valuesToUI();
      return;
   }
}
