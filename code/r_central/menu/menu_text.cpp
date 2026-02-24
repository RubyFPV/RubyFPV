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
#include "menu_objects.h"
#include "menu_text.h"

MenuText::MenuText(void)
:Menu(MENU_ID_TEXT , "Info", NULL)
{
   m_xPos = 0.45; m_yPos = 0.2;
   m_Width = 0.45;
   m_LinesCount = 0;
   m_TopLineIndex = 0;
   m_fScaleFactor = 1.0;
   addMenuItem(new MenuItem("Done"));
}

MenuText::~MenuText()
{
   for( int i=0; i<m_LinesCount; i++ )
         free ( m_szLines[i] );
   m_LinesCount = 0;
   m_bInvalidated = true;
}

void MenuText::addTextLine(const char* szText)
{
   if ( NULL == szText )
      return;
   if ( m_LinesCount < M_MAX_TEXT_LINES - 1 )
   {
      m_szLines[m_LinesCount] = (char*)malloc(strlen(szText)+1);
      strcpy(m_szLines[m_LinesCount], szText);
      m_LinesCount++;
   }
   else
   {
      free (m_szLines[0]);
      for( int i=0; i<M_MAX_TEXT_LINES-1; i++ )
         m_szLines[i] = m_szLines[i+1];
      m_szLines[M_MAX_TEXT_LINES-1] = (char*)malloc(strlen(szText)+1);
      strcpy(m_szLines[M_MAX_TEXT_LINES-1], szText);
   }
   m_bInvalidated = true;

}

void MenuText::onShow()
{
   Menu::onShow();
}

void MenuText::computeRenderSizes()
{
   m_RenderWidth = m_Width;
   m_RenderHeight = 2.0*m_sfMenuPaddingY;
   if ( m_Height > 0.001 )
   {
      m_RenderHeight += m_Height;
      m_bInvalidated = false;
      return;
   }

       if ( 0 != m_szTitle[0] )
       {
          m_RenderHeight += g_pRenderEngine->getMessageHeight(m_szTitle, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
          m_RenderHeight += MENU_TEXTLINE_SPACING * g_pRenderEngine->textHeight(g_idFontMenu);
       }
       if ( 0 != m_szSubTitle[0] )
       {
          m_RenderHeight += g_pRenderEngine->getMessageHeight(m_szSubTitle, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
          m_RenderHeight += MENU_TEXTLINE_SPACING * g_pRenderEngine->textHeight(g_idFontMenu);
       }
       m_RenderHeight += m_sfMenuPaddingY;

       for (int i = 0; i<m_LinesCount; i++)
       {
           if ( 0 == strlen(m_szLines[i]) )
              m_RenderHeight += (1.0+MENU_TEXTLINE_SPACING) * g_pRenderEngine->textHeight(g_idFontMenu);
           else
           {
              m_RenderHeight += g_pRenderEngine->getMessageHeight(m_szLines[i], MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
              m_RenderHeight += MENU_TEXTLINE_SPACING * g_pRenderEngine->textHeight(g_idFontMenu);
           }
       }
       m_RenderHeight += 0.5*m_sfMenuPaddingY;

   m_bInvalidated = false;
}

void MenuText::Render()
{
   RenderPrepare();
   if ( m_bInvalidated )
   {
      computeRenderSizes();
      m_bInvalidated = false;
   }

   float yTop = RenderFrameAndTitle();
   float y = yTop;

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);

   for (int i = 0; i<m_LinesCount; i++)
   {
      y += g_pRenderEngine->drawMessageLines(m_xPos+m_sfMenuPaddingX, y, m_szLines[i], MENU_TEXTLINE_SPACING, m_RenderWidth-2*m_sfMenuPaddingX, g_idFontMenu);
      y += height_text*MENU_TEXTLINE_SPACING;
   }
   RenderEnd(yTop);
}

void MenuText::onSelectItem()
{
   Menu::onSelectItem();
   if ( 0 == m_SelectedIndex )
      menu_stack_pop(0);
}
