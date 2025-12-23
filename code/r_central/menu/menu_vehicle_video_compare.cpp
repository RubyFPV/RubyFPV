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

#include "../../base/hardware_i2c.h"
#include "menu.h"
#include "menu_vehicle_video_compare.h"
#include "menu_confirmation.h"
#include "menu_item_text.h"

MenuVehicleVideoCompare::MenuVehicleVideoCompare(void)
:Menu(MENU_ID_VEHICLE_VIDEO_COMPARE, L("Video Profiles"), NULL)
{
   m_Width = 0.32;
   m_Height = 0.0;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.2;

   float fColWidth = g_pRenderEngine->textWidth(g_idFontMenu, "100% NAAB22");
   float fStartWidth = g_pRenderEngine->textWidth(g_idFontMenu, "Some Long Random Text Here Again");
   m_Width = 5.0 * fColWidth + fStartWidth;

   float fTextHeight = g_pRenderEngine->textHeight(g_idFontMenu);
   addExtraHeightAtStart(13.0 * (fTextHeight  + Menu::getSelectionPaddingY()*1.5));

   m_fCurrentYPos = 0.0;
   m_iCurrentLine = 0;

   m_iIndexOk = addMenuItem(new MenuItem(L("Ok")));
}

MenuVehicleVideoCompare::~MenuVehicleVideoCompare()
{
}

void MenuVehicleVideoCompare::onShow()
{      
   Menu::onShow();
}

void MenuVehicleVideoCompare::valuesToUI()
{
}

void MenuVehicleVideoCompare::addRenderLine(const char* szTitle, const char* szVal1, const char* szVal2, const char* szVal3, const char* szVal4, const char* szVal5 )
{
   float fStartWidth = g_pRenderEngine->textWidth(g_idFontMenu, "Some Long Random Text Here Again");
   float fColWidth = g_pRenderEngine->textWidth(g_idFontMenu, "100% NAAB22");
   float fTextHeight = g_pRenderEngine->textHeight(g_idFontMenu);

   m_fCurrentYPos += 0.5 * 1.5 * Menu::getSelectionPaddingY();

   float fXPos = m_RenderXPos + m_sfMenuPaddingX + fStartWidth;
   float fTextWidth = 0.0;
   g_pRenderEngine->drawTextLeft(fXPos - m_sfMenuPaddingX, m_fCurrentYPos, g_idFontMenu, szTitle);

   fTextWidth = g_pRenderEngine->textWidth(g_idFontMenu, szVal1);
   g_pRenderEngine->drawText(fXPos + 0.5 * (fColWidth - fTextWidth), m_fCurrentYPos, g_idFontMenu, szVal1);
   fXPos += fColWidth;

   fTextWidth = g_pRenderEngine->textWidth(g_idFontMenu, szVal2);
   g_pRenderEngine->drawText(fXPos + 0.5 * (fColWidth - fTextWidth), m_fCurrentYPos, g_idFontMenu, szVal2);
   fXPos += fColWidth;

   fTextWidth = g_pRenderEngine->textWidth(g_idFontMenu, szVal3);
   g_pRenderEngine->drawText(fXPos + 0.5 * (fColWidth - fTextWidth), m_fCurrentYPos, g_idFontMenu, szVal3);
   fXPos += fColWidth;

   fTextWidth = g_pRenderEngine->textWidth(g_idFontMenu, szVal4);
   g_pRenderEngine->drawText(fXPos + 0.5 * (fColWidth - fTextWidth), m_fCurrentYPos, g_idFontMenu, szVal4);
   fXPos += fColWidth;

   fTextWidth = g_pRenderEngine->textWidth(g_idFontMenu, szVal5);
   g_pRenderEngine->drawText(fXPos + 0.5 * (fColWidth - fTextWidth), m_fCurrentYPos, g_idFontMenu, szVal5);
   fXPos += fColWidth;

   m_fCurrentYPos += fTextHeight + 0.5 * 1.5 * Menu::getSelectionPaddingY();
   m_iCurrentLine++;
}

void MenuVehicleVideoCompare::Render()
{
   RenderPrepare();   
   float yTop = RenderFrameAndTitle();

   float fTableTop = m_RenderYPos + m_RenderTitleHeight + m_sfMenuPaddingY*2.0;
   float fColWidth = g_pRenderEngine->textWidth(g_idFontMenu, "100% NAAB22");
   float fStartWidth = g_pRenderEngine->textWidth(g_idFontMenu, "Some Long Random Text Here Again");
   float x = m_RenderXPos + m_sfMenuPaddingX + fStartWidth - 2.0*g_pRenderEngine->getPixelWidth();

   bool bAlphaEnabled = g_pRenderEngine->isAlphaBlendingEnabled();
   g_pRenderEngine->enableAlphaBlending();
   g_pRenderEngine->setFill(255,100,100,0.2);
   g_pRenderEngine->setStroke(250,250,250, 0.3);
   //g_pRenderEngine->drawRect(x + fColWidth * g_pCurrentModel->video_params.iCurrentVideoProfile, fTableTop, fColWidth, m_RenderHeight - (fTableTop - m_RenderYPos) - m_sfMenuPaddingY);
   g_pRenderEngine->drawRect(x + fColWidth * g_pCurrentModel->video_params.iCurrentVideoProfile + 8.0 * g_pRenderEngine->getPixelWidth(), fTableTop - 0.5 * m_sfMenuPaddingY, fColWidth - 16.0 * g_pRenderEngine->getPixelWidth(), m_fExtraHeightStart + 0.2 * m_sfMenuPaddingY);
   g_pRenderEngine->setAlphaBlendingEnabled(bAlphaEnabled);
   g_pRenderEngine->setColors(get_Color_MenuText());

   m_fCurrentYPos = fTableTop;
   m_iCurrentLine = 0;
   addRenderLine(L("Setting Name"), "HQ", "HP", "LR", "USER", "Custom" );

   char szVals[6][64];

   m_fCurrentYPos += g_pRenderEngine->getPixelHeight()*4.0;   
   g_pRenderEngine->drawLine(m_RenderXPos + m_sfMenuPaddingX, m_fCurrentYPos, m_RenderXPos + m_RenderWidth - m_sfMenuPaddingX, m_fCurrentYPos);
   m_fCurrentYPos += m_sfMenuPaddingY * 0.5;

   for( int i=0; i<5; i++ )
   {
      if ( g_pCurrentModel->video_link_profiles[i].iDefaultFPS > 0 )
         sprintf(szVals[i], "%d", g_pCurrentModel->video_link_profiles[i].iDefaultFPS);
      else
         strcpy(szVals[i], "Any");
   }
   addRenderLine(L("Default FPS"), szVals[0], szVals[1], szVals[2], szVals[3], szVals[4] );


   for( int i=0; i<5; i++ )
   {
      strcpy(szVals[i], "Off");
      if ( g_pCurrentModel->video_link_profiles[i].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS )
      if ( ! g_pCurrentModel->isVideoLinkFixedOneWay() )
      {
         if ( g_pCurrentModel->video_link_profiles[i].uProfileFlags & VIDEO_PROFILE_FLAG_RETRANSMISSIONS_AGGRESIVE )
            strcpy(szVals[i], "Aggresive");
         else
            strcpy(szVals[i], "Regular");
      }
   }
   addRenderLine(L("Retransmissions"), szVals[0], szVals[1], szVals[2], szVals[3], szVals[4] );

   for( int i=0; i<5; i++ )
   {
      sprintf(szVals[i], "%d", (g_pCurrentModel->video_link_profiles[i].uProfileFlags & VIDEO_PROFILE_FLAG_MASK_RETRANSMISSIONS_GUARD_MASK) >> 8);
   }   
   addRenderLine(L("Retransmissions Sensitivity (ms)"), szVals[0], szVals[1], szVals[2], szVals[3], szVals[4] );

   for( int i=0; i<5; i++ )
   {
      if ( g_pCurrentModel->video_link_profiles[i].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK )
      {
         if ( g_pCurrentModel->video_link_profiles[i].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO )
            strcpy(szVals[i], "Medium");
         else
            strcpy(szVals[i], "Full");
      }
      else
         strcpy(szVals[i], "Off");
   }
   addRenderLine(L("Adaptive Video Strength"), szVals[0], szVals[1], szVals[2], szVals[3], szVals[4] );

   for( int i=0; i<5; i++ )
   {
      int ikf = g_pCurrentModel->video_link_profiles[i].iKeyframeMS;
      if ( ikf < 0 )
         ikf = -ikf;
      if ( g_pCurrentModel->video_link_profiles[i].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME )
         sprintf(szVals[i], "A %d ms", ikf);
      else
         sprintf(szVals[i], "M %d ms", ikf);
   }
   addRenderLine(L("Video Keyframes"), szVals[0], szVals[1], szVals[2], szVals[3], szVals[4] );

   for( int i=0; i<5; i++ )
   {
      if ( g_pCurrentModel->video_link_profiles[i].uProfileFlags & VIDEO_PROFILE_FLAG_USE_HIGHER_DATARATE )
      {
         if ( 0 == (g_pCurrentModel->video_link_profiles[i].uProfileFlags & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK) >> VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT )
            strcpy(szVals[i], "0");
         else
            sprintf(szVals[i], "+%d", (g_pCurrentModel->video_link_profiles[i].uProfileFlags & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK) >> VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT);
      }
      else
         strcpy(szVals[i], "No");
   }
   addRenderLine(L("Boost Radio Rates"), szVals[0], szVals[1], szVals[2], szVals[3], szVals[4] );

   for( int i=0; i<5; i++ )
   {
      sprintf(szVals[i], "%d%%", g_pCurrentModel->video_link_profiles[i].iECPercentage);
   }
   addRenderLine(L("EC Percentage"), szVals[0], szVals[1], szVals[2], szVals[3], szVals[4] );

   for( int i=0; i<5; i++ )
   {
      if ( g_pCurrentModel->video_link_profiles[i].uProfileFlags & VIDEO_PROFILE_FLAG_USE_LOWER_DR_FOR_EC_PACKETS )
         strcpy(szVals[i], "Yes");
      else
         strcpy(szVals[i], "No");
   }
   addRenderLine(L("Lower Radio Rates on EC"), szVals[0], szVals[1], szVals[2], szVals[3], szVals[4] );

   for( int i=0; i<5; i++ )
   {
      if ( g_pCurrentModel->video_link_profiles[i].iDefaultLinkLoad > 0)
         sprintf(szVals[i], "%d%%", g_pCurrentModel->video_link_profiles[i].iDefaultLinkLoad);
      else
         sprintf(szVals[i], "%d%%", (int)g_pCurrentModel->radioLinksParams.uMaxLinkLoadPercent[0]);
   }
   addRenderLine(L("Max Radio Link Load"), szVals[0], szVals[1], szVals[2], szVals[3], szVals[4] );

   for( int i=0; i<5; i++ )
   {
      if ( g_pCurrentModel->video_link_profiles[i].iIPQuantizationDelta > -100 )
         sprintf(szVals[i], "%d", g_pCurrentModel->video_link_profiles[i].iIPQuantizationDelta);
      else
         strcpy(szVals[i], "Auto");
   }
   addRenderLine(L("H264/H265 Quant Delta"), szVals[0], szVals[1], szVals[2], szVals[3], szVals[4] );

   for( int i=0; i<5; i++ )
   {
      int iNoise = g_pCurrentModel->video_link_profiles[i].uProfileFlags & VIDEO_PROFILE_FLAGS_MASK_NOISE;
      if ( 3 != iNoise )
         sprintf(szVals[i], "%d", iNoise);
      else
         strcpy(szVals[i], "Disabled");
   }
   addRenderLine(L("Noise Reduction"), szVals[0], szVals[1], szVals[2], szVals[3], szVals[4] );

   g_pRenderEngine->drawLine( x, fTableTop, x, m_fCurrentYPos);

   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehicleVideoCompare::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_iIndexOk == m_SelectedIndex )
   {
      menu_stack_pop(0);
      return;
   }
}
