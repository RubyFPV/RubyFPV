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

#include "../base/base.h"
#include "../base/tx_powers.h"
#include "popup_radio_int.h"
#include "../renderer/render_engine.h"
#include "osd_common.h"
#include "colors.h"
#include "menu.h"
#include "shared_vars.h"
#include "timers.h"
#include <math.h>

PopupRadioInterface::PopupRadioInterface(int iVehicleLinkIndex)
:Popup(NULL, 0.65, 0.2, 0.33, 400)
{
   char szBuff[256];
   char szTmp[64];
   m_fMaxWidth = 0.38;
   m_fHeightGraph = 0.14;
   m_iVehicleRadioLinkIndex = iVehicleLinkIndex;
   m_bCalibrated = false;

   if ( g_pCurrentModel->radioRuntimeCapabilities.uFlagsRuntimeCapab & MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED )
   if ( g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS )
      m_bCalibrated = true;

   m_iVehicleRadioInterfaceIndex = -1;
   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == m_iVehicleRadioLinkIndex )
      {
         m_iVehicleRadioInterfaceIndex = i;
         break;
      }
   }

   if ( -1 == m_iVehicleRadioInterfaceIndex )
   {
      addLine("Failed to get radio interface info.");
      return;
   }

   strcpy(szBuff, L("Vehicle's Radio Link"));
   strcat(szBuff, " ");
   sprintf(szTmp, "%d", m_iVehicleRadioLinkIndex+1);
   strcat(szBuff, szTmp);
   setTitle(szBuff);

   strcpy(szBuff, L("Radio type:"));
   strcat(szBuff, " ");
   strcat(szBuff, str_get_radio_card_model_string(g_pCurrentModel->radioInterfacesParams.interface_card_model[m_iVehicleRadioInterfaceIndex]));
   addLine(szBuff);

   strcpy(szBuff, L("Max power for this radio:"));
   strcat(szBuff, " ");
   int iRadioInterfacelModel = g_pCurrentModel->radioInterfacesParams.interface_card_model[m_iVehicleRadioInterfaceIndex];
   if ( iRadioInterfacelModel < 0 )
      iRadioInterfacelModel = -iRadioInterfacelModel;
   sprintf(szTmp, "%d mW", tx_powers_get_max_usable_power_mw_for_card(g_pCurrentModel->hwCapabilities.uBoardType, iRadioInterfacelModel));
   strcat(szBuff, szTmp);
   addLine(szBuff);

   m_iMaxMeasuredTxPowerMw = 1;
   if ( ! m_bCalibrated )
   {
      addLine("Radio not calibrated");
      addLine("You need to run the Radio Calibration Wizard so that the actual capabilities of your hardware gets computed for different modulations and radio configurations.");
   }
   else
   {
      m_iMaxMeasuredTxPowerMw = -1;
      int iMaxUsableMCSRate = 0;
      int iMaxUsableLegacyRate = 0;
      for( int i=0; i<MODEL_MAX_STORED_QUALITIES_VALUES; i++ )
      {
         if ( g_pCurrentModel->radioRuntimeCapabilities.iMaxTxPowerMwLegacy[m_iVehicleRadioLinkIndex][i] > 0 )
         {
            if ( i < getTestDataRatesCountLegacy() )
               iMaxUsableLegacyRate = getTestDataRatesLegacy()[i];
            if ( g_pCurrentModel->radioRuntimeCapabilities.iMaxTxPowerMwLegacy[m_iVehicleRadioLinkIndex][i] > m_iMaxMeasuredTxPowerMw )
               m_iMaxMeasuredTxPowerMw = g_pCurrentModel->radioRuntimeCapabilities.iMaxTxPowerMwLegacy[m_iVehicleRadioLinkIndex][i];
         }
         if ( g_pCurrentModel->radioRuntimeCapabilities.iMaxTxPowerMwMCS[m_iVehicleRadioLinkIndex][i] > 0 )
         {
            if ( i < getTestDataRatesCountMCS() )
               iMaxUsableMCSRate = getTestDataRatesMCS()[i];
            if ( g_pCurrentModel->radioRuntimeCapabilities.iMaxTxPowerMwMCS[m_iVehicleRadioLinkIndex][i] > m_iMaxMeasuredTxPowerMw )
               m_iMaxMeasuredTxPowerMw = g_pCurrentModel->radioRuntimeCapabilities.iMaxTxPowerMwMCS[m_iVehicleRadioLinkIndex][i];
          }
      }
      strcpy(szBuff, L("Max measured usable powers for this radio:"));
      strcat(szBuff, " ");
      if ( m_iMaxMeasuredTxPowerMw > 0 )
         sprintf(szTmp, "%d mW", m_iMaxMeasuredTxPowerMw);
      else
         sprintf(szTmp, "N/A");
      strcat(szBuff, szTmp);
      addLine(szBuff);

      strcpy(szBuff, L("Max usable rates on this radio:"));
      strcat(szBuff, " ");
      sprintf(szTmp, "MCS-%d / %s", -1-iMaxUsableMCSRate, str_getDataRateDescriptionAlternative(iMaxUsableLegacyRate));
      strcat(szBuff, szTmp);
      addLine(szBuff);
   }
}

PopupRadioInterface::~PopupRadioInterface()
{
}

void PopupRadioInterface::computeSize()
{
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float fMarginX = Menu::getMenuPaddingX();
   m_RenderWidth = m_fMaxWidth;
   m_RenderHeight = 0;
   
   if ( 0 != m_szTitle[0] )
   {
      m_RenderHeight = g_pRenderEngine->getMessageHeight(m_szTitle, POPUP_LINE_SPACING, m_fMaxWidth-2.0*fMarginX, g_idFontMenu);
      m_RenderHeight += height_text * POPUP_LINE_SPACING*1.5;
   }

   for ( int i=0; i<m_LinesCount; i++ )
   {
      m_RenderHeight += g_pRenderEngine->getMessageHeight(m_szLines[i], POPUP_LINE_SPACING, m_fMaxWidth-2.0*fMarginX, g_idFontMenu);
      m_RenderHeight += height_text*POPUP_LINE_SPACING;
   }

   m_RenderHeight += 1.7*Menu::getMenuPaddingY();

   m_xPos = (1.0-m_RenderWidth) - Menu::getMenuPaddingX();

   if ( m_bCalibrated )
      m_RenderHeight += m_fHeightGraph + 8.0 * height_text;
   m_bInvalidated = false;
}

void PopupRadioInterface::Render()
{
   if ( m_bInvalidated )
   {
      computeSize();
      m_bInvalidated = false;
   }
   if ( m_fTimeoutSeconds > 0.001 )
   if ((g_TimeNow-m_StartTime) > m_fTimeoutSeconds*1000.0)
      return;

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float height_text_small = g_pRenderEngine->textHeight(g_idFontMenuSmall);
   float fMarginX = Menu::getMenuPaddingX();
   float fPixelW = g_pRenderEngine->getPixelWidth();
   float fPixelH = g_pRenderEngine->getPixelHeight();

   g_pRenderEngine->setColors(get_Color_PopupBg());
   g_pRenderEngine->setStroke(get_Color_PopupBorder());

   g_pRenderEngine->drawRoundRect(m_xPos, m_yPos, m_RenderWidth, m_RenderHeight, POPUP_ROUND_MARGIN);
   
   g_pRenderEngine->setColors(get_Color_PopupText());
   
   float x = m_xPos + fMarginX;
   float y = m_yPos + Menu::getMenuPaddingY();
   if ( 0 != m_szTitle[0] )
   {
      y += POPUP_TITLE_SCALE * g_pRenderEngine->drawMessageLines(x,y, m_szTitle, POPUP_LINE_SPACING, m_RenderWidth-2.0*fMarginX, g_idFontMenu);
      y += height_text * POPUP_LINE_SPACING * 1.5;
   }
   else
      y += height_text*0.9;


   for (int i = 0; i<m_LinesCount; i++)
   {
      if ( (i == 2) && (! m_bCalibrated) )
         g_pRenderEngine->setColors(get_Color_IconError());
      y += height_text*POPUP_LINE_SPACING;
      float fHeightText = g_pRenderEngine->drawMessageLines(x, y, m_szLines[i], POPUP_LINE_SPACING, m_RenderWidth-2.0*fMarginX, g_idFontMenu);
      y += fHeightText;
      g_pRenderEngine->setColors(get_Color_PopupText());
   }

   if ( ! m_bCalibrated )
      return;

   double cBlue[] = {100,100,255,1};
   //double cRed[] = {255,50,50,1};
   double cGreen[] = {50,255,50,1};

   y += height_text;
   float fWidthText = g_pRenderEngine->textWidth(g_idFontMenu, L("Actual measured usable Tx powers:"));
   g_pRenderEngine->drawText(x, y, g_idFontMenu, L("Actual measured usable Tx powers:"));
   g_pRenderEngine->setColors(&cBlue[0]);
   g_pRenderEngine->drawText(x + fWidthText + 6.0 * fPixelW, y, g_idFontMenu, L("[Blue]"));
   g_pRenderEngine->setColors(get_Color_PopupText());
   y += height_text;

   fWidthText = g_pRenderEngine->textWidth(g_idFontMenu, L("Currently set Tx powers:"));
   g_pRenderEngine->drawText(x, y, g_idFontMenu, L("Currently set Tx powers:"));
   g_pRenderEngine->setColors(&cGreen[0]);
   g_pRenderEngine->drawText(x + fWidthText + 6.0 * fPixelW, y, g_idFontMenu, L("[Green]"));
   g_pRenderEngine->setColors(get_Color_PopupText());

   y += height_text*2.2;

   char szBuff[128];
   int iCountTestsLegacy = 5;
   int iCountTestsMCS = getTestDataRatesCountMCS();
   if ( iCountTestsMCS > MODEL_MAX_STORED_QUALITIES_VALUES )
      iCountTestsMCS = MODEL_MAX_STORED_QUALITIES_VALUES;
   int iMaxCountTests = iCountTestsLegacy;
   if ( iCountTestsMCS > iMaxCountTests )
      iMaxCountTests = iCountTestsMCS;

   int iRadioInterfacelModel = g_pCurrentModel->radioInterfacesParams.interface_card_model[m_iVehicleRadioInterfaceIndex];
   if ( iRadioInterfacelModel < 0 )
      iRadioInterfacelModel = -iRadioInterfacelModel;
   int iMaxPowerMw = tx_powers_get_max_usable_power_mw_for_card(g_pCurrentModel->hwCapabilities.uBoardType, iRadioInterfacelModel);

   float fWidthMargin = g_pRenderEngine->textWidth(g_idFontMenu, "999 mW");
   float fWidthGraph = m_RenderWidth - 2.0 * fMarginX - fWidthMargin;
   float fWidthBar = fWidthGraph / (float)(iMaxCountTests+1);

   g_pRenderEngine->drawLine(x + fWidthMargin, y, x + fWidthMargin, y + m_fHeightGraph);
   g_pRenderEngine->drawLine(x + fWidthMargin, y + m_fHeightGraph, x + fWidthMargin + fWidthGraph, y + m_fHeightGraph);

   g_pRenderEngine->drawTextLeft(x + fWidthMargin - 6.0 * fPixelW, y + m_fHeightGraph - height_text, g_idFontMenu, "0 mW");

   sprintf(szBuff, "%d mW", iMaxPowerMw);
   g_pRenderEngine->drawTextLeft(x + fWidthMargin - 6.0 * fPixelW, y - 0.6 * height_text, g_idFontMenu, szBuff);

   sprintf(szBuff, "%d mW", iMaxPowerMw/2);
   g_pRenderEngine->drawTextLeft(x + fWidthMargin - 6.0 * fPixelW, y + m_fHeightGraph*0.5 - 0.6 * height_text, g_idFontMenu, szBuff);

   if ( (0 != iMaxPowerMw) && (fabs(m_iMaxMeasuredTxPowerMw-iMaxPowerMw) > 10) )
   {
      float dy = m_fHeightGraph  * (float)m_iMaxMeasuredTxPowerMw/(float)iMaxPowerMw;
      if ( dy > m_fHeightGraph - height_text )
         dy = m_fHeightGraph - height_text;
      sprintf(szBuff, "%d mW", m_iMaxMeasuredTxPowerMw);
      g_pRenderEngine->drawTextLeft(x + fWidthMargin - 6.0 * fPixelW, y + m_fHeightGraph - dy, g_idFontMenu, szBuff);
   }

   for( float fx=x+fWidthMargin; fx<m_xPos + m_RenderWidth - 2.0 * fMarginX; fx += 0.012 )
   {
      g_pRenderEngine->drawLine(fx, y, fx + 0.004, y);
      g_pRenderEngine->drawLine(fx, y + m_fHeightGraph*0.5, fx + 0.004, y + m_fHeightGraph * 0.5);
   }
   for( int i=0; i<iMaxCountTests; i++ )
   {
      strcpy(szBuff, "N/A");
      if ( i < iCountTestsMCS )
         strcpy(szBuff, str_getDataRateDescriptionAlternative(-1-i));
      else if ( i < getTestDataRatesCountLegacy() )
         strcpy(szBuff, str_getDataRateDescriptionAlternative(getTestDataRatesLegacy()[i]));
      fWidthText = g_pRenderEngine->textWidth(g_idFontMenuSmall, szBuff);
      g_pRenderEngine->drawText(x + fWidthMargin + fWidthBar + i * fWidthBar - 0.5 * fWidthText, y + m_fHeightGraph + 3.0 * fPixelH + height_text_small * (i%2), g_idFontMenuSmall, szBuff);
   }

   g_pRenderEngine->setColors(&cBlue[0]);

   float fxPos = x + fWidthMargin + fWidthBar;
   float fPrevHeight = 0.0;
   float fPrevHeightTarget = 0.0;

   for( int i=0; i<iMaxCountTests; i++ )
   {
      int iTxPower = 0;
      if ( i < iCountTestsMCS )
         iTxPower = g_pCurrentModel->radioRuntimeCapabilities.iMaxTxPowerMwMCS[m_iVehicleRadioLinkIndex][i];
      else if ( i < getTestDataRatesCountLegacy() )
         iTxPower = g_pCurrentModel->radioRuntimeCapabilities.iMaxTxPowerMwLegacy[m_iVehicleRadioLinkIndex][i];
      float fHeight = 0.0;
      if ( (iTxPower > 0) && (iMaxPowerMw > 0) )
         fHeight = m_fHeightGraph * (float)iTxPower/(float)iMaxPowerMw;
      if ( fHeight < fPixelH*6.0 )
         fHeight = fPixelH*6.0;

      int iCardRawPower = g_pCurrentModel->radioInterfacesParams.interface_raw_power[m_iVehicleRadioInterfaceIndex];
      int iCardPowerMw = tx_powers_convert_raw_to_mw(g_pCurrentModel->hwCapabilities.uBoardType, iRadioInterfacelModel, iCardRawPower);
      if ( iCardPowerMw > iTxPower )
         iCardPowerMw = iTxPower;
      float fHeightTarget = 0.0;
      if ( (iCardPowerMw > 0) && (iMaxPowerMw > 0) )
         fHeightTarget = m_fHeightGraph * (float)iCardPowerMw/(float)iMaxPowerMw - 3.0 * fPixelH;
      if ( fHeightTarget < fPixelH*3.0 )
         fHeightTarget = fPixelH*3.0;

      g_pRenderEngine->setColors(get_Color_PopupText());
      if ( fHeight >  fPixelH*4.0 )
      {
         for( float fy = y + m_fHeightGraph; fy > y + m_fHeightGraph - fHeight - 0.01; fy -= 0.02)
            g_pRenderEngine->drawLine(fxPos, fy, fxPos, fy - 0.01);
      }

      if ( i != 0 )
      {
         g_pRenderEngine->setColors(&cBlue[0]);
         g_pRenderEngine->drawLine(fxPos - fWidthBar, y + m_fHeightGraph - fPrevHeight, fxPos, y + m_fHeightGraph - fHeight);
         g_pRenderEngine->drawLine(fxPos - fWidthBar, y + m_fHeightGraph - fPrevHeight + fPixelH, fxPos, y + m_fHeightGraph - fHeight + fPixelH);
         g_pRenderEngine->drawLine(fxPos - fWidthBar, y + m_fHeightGraph - fPrevHeight + 2.0*fPixelH, fxPos, y + m_fHeightGraph - fHeight + 2.0*fPixelH);
         g_pRenderEngine->setColors(&cGreen[0]);
         g_pRenderEngine->drawLine(fxPos - fWidthBar, y + m_fHeightGraph - fPrevHeightTarget, fxPos, y + m_fHeightGraph - fHeightTarget);
         g_pRenderEngine->drawLine(fxPos - fWidthBar, y + m_fHeightGraph - fPrevHeightTarget + fPixelH, fxPos, y + m_fHeightGraph - fHeightTarget + fPixelH);
         g_pRenderEngine->drawLine(fxPos - fWidthBar, y + m_fHeightGraph - fPrevHeightTarget + 2.0*fPixelH, fxPos, y + m_fHeightGraph - fHeightTarget + 2.0*fPixelH);
      }
      fPrevHeight = fHeight;
      fPrevHeightTarget = fHeightTarget;
      fxPos += fWidthBar;
   }
   g_pRenderEngine->setColors(get_Color_PopupText());

   y += m_fHeightGraph + 2.2 * height_text;

   fWidthText = g_pRenderEngine->textWidth(g_idFontMenu, L("Radio Modulations"));
   g_pRenderEngine->drawText(x + fWidthMargin + fWidthGraph * 0.48 - fWidthText * 0.5, y, g_idFontMenu, L("Radio Modulations"));
}

