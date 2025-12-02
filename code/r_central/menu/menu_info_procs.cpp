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

#include <ctype.h>
#include "menu.h"
#include "menu_info_procs.h"
#include "../osd/osd_common.h"

MenuInfoProcs::MenuInfoProcs(const char* szInfo)
:Menu(MENU_ID_PROC_INFO, L("Processes Priorities Info"), NULL)
{
   m_xPos = 0.15;
   m_yPos = 0.1;
   m_Width = 0.78;
   m_Height = 0.88;
   addMenuItem(new MenuItem(L("Ok")));

   strncpy(m_szInfo, szInfo, 4095);
}


MenuInfoProcs::~MenuInfoProcs()
{
}

void MenuInfoProcs::onShow()
{
   Menu::onShow();
}

void MenuInfoProcs::appendInfo(const char* szInfo)
{
   if ( (NULL == szInfo) || (0 == szInfo[0]) )
      return;
   if ( strlen(m_szInfo) + strlen(szInfo) >= (int)(sizeof(m_szInfo)/sizeof(m_szInfo[0])) - 1 )
      return;
   strcat(m_szInfo, szInfo);
}


float MenuInfoProcs::_parseAddDbgProcsInfoLine(float yPos, const char* szDbgInfoLine)
{
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);

   float fTabStops[] = {0.0, 
     0.12,
     0.15, 0.2, 0.25, 0.28, 0.33, 0.4, 0.45, 0.5, 0.55, 0.6, 0.65, 0.7, 0.73, 0.76};

   int iCountTabStops = sizeof(fTabStops)/sizeof(fTabStops[0]);
   int iCurrentTab = 0;

   char* pTmp = (char*)szDbgInfoLine;
   char* pWord = (char*)szDbgInfoLine;
   float x = m_RenderXPos+m_sfMenuPaddingX;

   while ( *pTmp != 0 )
   {
      if ( (*pTmp == 10) || (*pTmp == 13) || (*pTmp == ' ') || (*pTmp == '\t') )
      {
         *pTmp = 0;
         if ( (0 < strlen(pWord)) && (isalnum(*pWord) || (ispunct(*pWord))) )
         {
            g_pRenderEngine->drawText(x + fTabStops[iCurrentTab], yPos, g_idFontMenu, pWord);
            if ( iCurrentTab < iCountTabStops-1 )
               iCurrentTab++;
         }
         pTmp++;
         pWord = pTmp;
         continue;
      }
      pTmp++;
   }
   if ( (0 < strlen(pWord)) && (isalnum(*pWord) || (ispunct(*pWord))) )
   {
      g_pRenderEngine->drawText(x + fTabStops[iCurrentTab], yPos, g_idFontMenu, pWord);
      if ( iCurrentTab < iCountTabStops-1 )
         iCurrentTab++;
   }
   return height_text;
}


void MenuInfoProcs::Render()
{
   RenderPrepare();   
   float yTop = RenderFrameAndTitle();
   float w = m_RenderWidth-2.0*m_sfMenuPaddingX;
   RenderItem(0,m_RenderYPos+m_RenderHeight-m_sfMenuPaddingY-m_pMenuItems[0]->getItemHeight(getUsableWidth()), w*0.48);
   RenderEnd(yTop);

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float x = m_RenderXPos+m_sfMenuPaddingX;
   float y = m_RenderYPos + m_RenderHeaderHeight + m_sfMenuPaddingY;

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);


   char szTmp[4096];
   strncpy(szTmp, m_szInfo, 4095);
   char* pTmp = szTmp;
   char* pLine = szTmp;
   int iCountLines = 0;
   while ( *pTmp != 0 )
   {
      if ( ((*pTmp) == 10) || ((*pTmp) == 13) )
      {
         *pTmp = 0;
         pTmp++;

         if ( iCountLines < 2 )
            g_pRenderEngine->drawText(x, y, g_idFontMenu, pLine);
         else
            _parseAddDbgProcsInfoLine(y, pLine);
         y += height_text;
         iCountLines++;
         if ( ((*pTmp) == 10) || ((*pTmp) == 13) )
         {
            *pTmp = 0;
            pTmp++;
         }
         pLine = pTmp;
         continue;
      }
      pTmp++;
   }

   if ( *pLine != 0 )
      _parseAddDbgProcsInfoLine(y, pLine);
}

void MenuInfoProcs::onSelectItem()
{
   menu_stack_pop(1);
   return;
}