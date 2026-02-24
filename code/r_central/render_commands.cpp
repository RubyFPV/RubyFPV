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
#include "../base/config.h"

#include "colors.h"
#include "../renderer/render_engine.h"
#include "render_commands.h"
#include "menu.h"
#include "shared_vars.h"
#include "timers.h"
#include "osd_common.h"
#include "warnings.h"


double COLOR_RADIO_BARS[4] = { 180,180,180, 0.9 };
double COLOR_RADIO_BARS_HIGH[4] = { 250,250,250, 1 };
double s_deltaColorHigh = 70;
double s_delatAlfaColorHigh = 0.1;

static u32 s_lastTimeRenderAnimCommands = 0;
static float s_fBarHighPosX = 0.0;
static int s_ProgressPercent = -1;
static bool s_bUploadNewMethod = true;
static char s_szRenderCommandsCustomStatus[256];

void render_commands_init()
{
   s_szRenderCommandsCustomStatus[0] = 0;
}


void render_animation_bars( float xPos, float yPos, float fWidth, float fHeight, bool bCentered )
{
   int numBars = 12;
   float fBarWidth = 0.64*fWidth/numBars;
   float fBarSpacing = 0.36*fWidth/numBars;
   float height_text = g_pRenderEngine->textHeight(g_idFontOSD);
   char* szMsgWarnings = warnings_get_last_message_configure_radio_link();

   if ( bCentered )
   {
      xPos = (1.0 - fWidth)*0.5;
      yPos -= fHeight*0.5;
   }

   s_fBarHighPosX += fWidth * (g_TimeNow-s_lastTimeRenderAnimCommands)/1000.0;
   if ( s_fBarHighPosX >= fWidth + 2*fBarWidth )
     s_fBarHighPosX = -2*fBarWidth;

   g_pRenderEngine->setStrokeSize(0);

   char szMsgToDisplay[256];
   szMsgToDisplay[0] = 0;

   if ( (NULL != szMsgWarnings) && (0 != szMsgWarnings[0]) )
      strcpy(szMsgToDisplay, szMsgWarnings);
   if ( 0 != s_szRenderCommandsCustomStatus[0] )
      strcpy(szMsgToDisplay, s_szRenderCommandsCustomStatus);

   if ( 0 != szMsgToDisplay[0] )
   {
      float fTextWidth = g_pRenderEngine->textWidth(g_idFontOSD, szMsgToDisplay);
      fTextWidth += g_pRenderEngine->textWidth(g_idFontOSD, "**");
      if ( fWidth > fTextWidth )
         fTextWidth = fWidth;

      float xPosText = xPos;
      if ( bCentered )
         xPosText = (1.0 - fTextWidth)*0.5;

      float fMargin = height_text*0.5;

      g_pRenderEngine->setFill(0,0,0,0.7);
      g_pRenderEngine->drawRoundRect(xPosText-fMargin, yPos - height_text*1.5 - fMargin, fTextWidth + 2.0*fMargin, fHeight + height_text * 1.5 + 2.0*fMargin, 0.02);
      //g_pRenderEngine->setGlobalAlfa(fAlpha);
   }

   for( int i=0; i<numBars; i++ )
   {
      g_pRenderEngine->setFill(COLOR_RADIO_BARS[0], COLOR_RADIO_BARS[1], COLOR_RADIO_BARS[2], COLOR_RADIO_BARS[3]);

      float fDistance = i*(fBarWidth+fBarSpacing) + 0.5 * fBarWidth - s_fBarHighPosX;
      if ( fDistance < 0 )
         fDistance = -fDistance;
      float fWeight = fDistance/(0.28*fWidth);
      if ( fWeight > 1.0 )
         fWeight = 1.0;
      fWeight = 1.0 - fWeight;
      fWeight *= fWeight;

      float fBarHeight = 0.4*fHeight;
      fBarHeight += 0.6 * fHeight * fWeight;

      g_pRenderEngine->setFill(COLOR_RADIO_BARS[0]+s_deltaColorHigh*fWeight, COLOR_RADIO_BARS[1]+s_deltaColorHigh*fWeight, COLOR_RADIO_BARS[2]+s_deltaColorHigh*fWeight, COLOR_RADIO_BARS[3]+s_delatAlfaColorHigh*fWeight);
      g_pRenderEngine->drawRoundRect(xPos+i*(fBarWidth+fBarSpacing), yPos+0.5*(fHeight-fBarHeight), fBarWidth, fBarHeight, 0.006);
   }

   static int sl_iCountUploadTextDotsCount = 0;
   sl_iCountUploadTextDotsCount++;

   if ( s_ProgressPercent > 0 )
   {
      if ( 0 != szMsgToDisplay[0] )
      {
         float fTextWidth = g_pRenderEngine->textWidth(g_idFontOSD, szMsgToDisplay);
         fTextWidth += g_pRenderEngine->textWidth(g_idFontOSD, "**");
         char szTmp[256];
         strcpy(szTmp, szMsgToDisplay);
         for( int i=0; i<((sl_iCountUploadTextDotsCount/10) % 3); i++ )
            strcat(szTmp, ".");
         if ( bCentered )
            g_pRenderEngine->drawText((1.0 - fTextWidth)*0.5, yPos-height_text*1.5, g_idFontOSD, szTmp);
         else
            g_pRenderEngine->drawText(xPos, yPos-height_text, g_idFontOSD, szTmp);
      }

      char szBuff[32];
      sprintf(szBuff, "%d%%", s_ProgressPercent);
      if ( ! s_bUploadNewMethod )
         sprintf(szBuff, "* %d%%", s_ProgressPercent);
      float height_text = osd_getFontHeightBig();
      g_pRenderEngine->drawText(xPos+fWidth+0.01, yPos+height_text*0.1, g_idFontOSDBig, szBuff);
   }
   else if ( 0 != szMsgToDisplay[0] )
   {
      float fTextWidth = g_pRenderEngine->textWidth(g_idFontOSD, szMsgToDisplay);
      fTextWidth += g_pRenderEngine->textWidth(g_idFontOSD, "**");

      char szTmp[256];
      strcpy(szTmp, szMsgToDisplay);
      for( int i=0; i<((sl_iCountUploadTextDotsCount/10) % 3); i++ )
         strcat(szTmp, ".");
      if ( bCentered )
         g_pRenderEngine->drawText((1.0 - fTextWidth)*0.5, yPos-height_text*1.5, g_idFontOSD, szTmp);
      else
         g_pRenderEngine->drawText(xPos, yPos-height_text, g_idFontOSD, szTmp);
   }
}


void render_commands()
{
   render_animation_bars(0.5, 0.8, 0.12, 0.06, true );
   s_lastTimeRenderAnimCommands = g_TimeNow;
}

void render_commands_set_progress_percent(int percent, bool bNewMethod)
{
   s_ProgressPercent = percent;
   s_bUploadNewMethod = bNewMethod;
}

void render_commands_set_custom_status(const char* szStatus)
{
   if ( (NULL == szStatus) || (0 == szStatus[0]) )
   {
      s_szRenderCommandsCustomStatus[0] = 0;
      log_line("Cleared custom upload status message.");
      return;
   }
   strcpy(s_szRenderCommandsCustomStatus, szStatus);
   log_line("Set custom upload status message: (%s)", s_szRenderCommandsCustomStatus);
}