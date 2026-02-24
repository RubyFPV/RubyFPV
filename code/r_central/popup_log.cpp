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
#include "popup_log.h"
#include "../renderer/render_engine.h"
#include "osd_common.h"
#include "colors.h"
#include "menu.h"
#include "shared_vars.h"
#include "timers.h"

PopupLog s_PopupVehicleLog;
PopupLog s_PopupLog;
int static s_PopupShowFlag = 0;

PopupLog::PopupLog()
:Popup(NULL,0.4,0.65, 0.48, 4)
{
   setTitle("Log");
   m_fMaxWidth = 0.5;
   m_MaxRenderLines = 6;
   m_TotalLinesAdded = 0;
   m_bShowLineNumbers = true;
   m_bBottomAlign = true;
   setRenderBelowMenu(true);
}

PopupLog::~PopupLog()
{
}

void PopupLog::showLineNumbers(bool bShow)
{
   m_bShowLineNumbers = bShow;
   m_bInvalidated = true;
}

void PopupLog::setMaxRenderLines(int count)
{
   m_MaxRenderLines = count;
   m_bInvalidated = true;
}

void PopupLog::addLine(const char* szLine)
{
   if ( NULL == szLine )
      return;
   m_TotalLinesAdded++;
   char szBuff[1024];
   if ( m_bShowLineNumbers )
      snprintf(szBuff, 1023, "%d. %s", m_TotalLinesAdded, szLine);
   else
      snprintf(szBuff, 1023, "%s", szLine);

   if ( m_LinesCount < m_MaxRenderLines )
   {
      m_szLines[m_LinesCount] = (char*)malloc(strlen(szBuff)+1);
      strcpy(m_szLines[m_LinesCount], szBuff);
      m_LinesCount++;
   }
   else
   {
      free (m_szLines[0]);
      for( int i=0; i<m_MaxRenderLines-1; i++ )
         m_szLines[i] = m_szLines[i+1];
      m_szLines[m_MaxRenderLines-1] = (char*)malloc(strlen(szBuff)+1);
      strcpy(m_szLines[m_MaxRenderLines-1], szBuff);
   }
   m_bInvalidated = true;
}

void PopupLog::computeSize()
{
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);

   m_RenderWidth = (m_fMaxWidth-2*Menu::getSelectionPaddingX());
   m_RenderHeight = 0;

   if ( 0 != m_szTitle[0] )
   {
      m_RenderHeight = g_pRenderEngine->getMessageHeight(m_szTitle, POPUP_LINE_SPACING, m_RenderWidth, g_idFontMenu);
      m_RenderHeight += height_text * POPUP_LINE_SPACING*1.5;
   }

   for ( int i=0; i<m_LinesCount; i++ )
   {
      m_RenderHeight += g_pRenderEngine->getMessageHeight(m_szLines[i], POPUP_LINE_SPACING, m_RenderWidth, g_idFontMenu);
      m_RenderHeight += height_text*POPUP_LINE_SPACING;
   }

   m_RenderWidth += 2*Menu::getSelectionPaddingX();
   m_RenderHeight += 1.7*Menu::getSelectionPaddingY();

   m_xPos = (1.0-m_RenderWidth)/2.0;

   if ( m_bBottomAlign )
      m_yPos = 0.84 - m_RenderHeight;
   else
      m_yPos = 0.18;
   m_bInvalidated = false;
}

void PopupLog::Render()
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

   float alfaOrg = g_pRenderEngine->setGlobalAlfa(0.84);

   g_pRenderEngine->setColors(get_Color_PopupBg());
   g_pRenderEngine->setStroke(get_Color_PopupBorder());

   g_pRenderEngine->drawRoundRect(m_xPos, m_yPos, m_RenderWidth, m_RenderHeight, POPUP_ROUND_MARGIN);

   g_pRenderEngine->setColors(get_Color_PopupText());

   float y = m_yPos+Menu::getSelectionPaddingY();
   if ( 0 != m_szTitle[0] )
   {
      float x = m_xPos+Menu::getSelectionPaddingX();
      y += POPUP_TITLE_SCALE * g_pRenderEngine->drawMessageLines(x,y, m_szTitle, POPUP_LINE_SPACING, m_RenderWidth-2*Menu::getSelectionPaddingX(), g_idFontMenu);
      y += height_text * POPUP_LINE_SPACING * 1.5;
   }
   else
      y += height_text*0.9;

   g_pRenderEngine->highlightFirstWordOfLine(true);
   for (int i = 0; i<m_LinesCount; i++)
   {
      y += g_pRenderEngine->drawMessageLines(m_xPos+Menu::getSelectionPaddingX(), y, m_szLines[i], POPUP_LINE_SPACING, m_RenderWidth-2*Menu::getSelectionPaddingX(), g_idFontMenu);
      y += height_text*POPUP_LINE_SPACING;
   }
   g_pRenderEngine->highlightFirstWordOfLine(false);

   g_pRenderEngine->setGlobalAlfa(alfaOrg);
}


void popup_log_add_entry(const char* szLine)
{
   s_PopupLog.setFont(g_idFontOSDSmall);
   s_PopupLog.addLine(szLine);
   if ( s_PopupShowFlag != 0 )
      popups_add(&s_PopupLog);
}

void popup_log_add_entry(const char* szLine, int param)
{
   char szBuff[1024];
   snprintf(szBuff, 1023, szLine, param);
   s_PopupLog.setFont(g_idFontOSDSmall);
   s_PopupLog.addLine(szBuff);
   if ( s_PopupShowFlag != 0 )
      popups_add(&s_PopupLog);
}

void popup_log_add_entry(const char* szLine, const char* szParam)
{
   char szBuff[1024];
   snprintf(szBuff, 1023, szLine, szParam);
   s_PopupLog.setFont(g_idFontOSDSmall);
   s_PopupLog.addLine(szBuff);
   if ( s_PopupShowFlag != 0 )
      popups_add(&s_PopupLog);
}

void popup_log_set_show_flag(int flag)
{
   s_PopupShowFlag = flag;
   log_line("Set log popup window flags to: %d", s_PopupShowFlag);

   s_PopupLog.setFont(g_idFontOSDSmall);

   // Off
   if ( 0 == s_PopupShowFlag )
   {
      s_PopupLog.setTimeout(1.0);
      popups_remove(&s_PopupLog);
   }

   // On on new content
   if ( 1 == s_PopupShowFlag )
      s_PopupLog.setTimeout(5);

   // Always on
   if ( 2 == s_PopupShowFlag )
   {
      s_PopupLog.setTimeout(5000);
      popups_add(&s_PopupLog);
   }
}

void popup_log_vehicle_remove()
{
  s_PopupVehicleLog.setTimeout(1.0);
}

void popup_log_add_vehicle_entry(const char* szLine)
{
   s_PopupVehicleLog.setTitle("Vehicle Log");
   s_PopupVehicleLog.setBottomAlign(true);
   s_PopupVehicleLog.setFont(g_idFontOSDSmall);
   s_PopupVehicleLog.setMaxWidth(0.9);
   s_PopupVehicleLog.showLineNumbers(false);
   s_PopupVehicleLog.setMaxRenderLines(14);
   s_PopupVehicleLog.setTimeout(10);

   s_PopupVehicleLog.addLine(szLine);
   popups_add(&s_PopupVehicleLog);
}

