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
#include "../base/hdmi.h"
#include "../base/ctrl_preferences.h"
#include "render_engine.h"
#include "../public/render_engine_ui.h"
#include "../r_central/colors.h"

RenderEngine* s_pRenderEngineUI = NULL;
u32 s_uRenderEngineUIFontIdSmall = 0;
u32 s_uRenderEngineUIFontIdRegular = 0;
u32 s_uRenderEngineUIFontIdBig = 0;
u32 s_uRenderEngineUIFontsListSizes[100];

RenderEngineUI::RenderEngineUI()
{
   for( int i=0; i<100; i++ )
      s_uRenderEngineUIFontsListSizes[i] = 0;

   if ( NULL == s_pRenderEngineUI )
      s_pRenderEngineUI = renderer_engine();
}

RenderEngineUI::~RenderEngineUI()
{
}

int RenderEngineUI::getScreenWidth()
{
   if ( NULL == s_pRenderEngineUI )
      return 0;
   return s_pRenderEngineUI->getScreenWidth();
}

int RenderEngineUI::getScreenHeight()
{
   if ( NULL == s_pRenderEngineUI )
      return 0;
   return s_pRenderEngineUI->getScreenHeight();
}

float RenderEngineUI::getAspectRatio()
{
   if ( NULL == s_pRenderEngineUI )
      return 0.0;
   return s_pRenderEngineUI->getAspectRatio();
}

float RenderEngineUI::getPixelWidth()
{
   if ( NULL == s_pRenderEngineUI )
      return 0.0;
   return s_pRenderEngineUI->getPixelWidth();
}

float RenderEngineUI::getPixelHeight()
{
   if ( NULL == s_pRenderEngineUI )
      return 0.0;
   return s_pRenderEngineUI->getPixelHeight();
}

unsigned int RenderEngineUI::getFontIdSmall()
{
   if ( NULL == s_pRenderEngineUI )
      return 0;
   return s_uRenderEngineUIFontIdSmall;
}

unsigned int RenderEngineUI::getFontIdRegular()
{
   if ( NULL == s_pRenderEngineUI )
      return 0;
   return s_uRenderEngineUIFontIdRegular;
}

unsigned int RenderEngineUI::getFontIdBig()
{
   if ( NULL == s_pRenderEngineUI )
      return 0;
   return s_uRenderEngineUIFontIdBig;
}

unsigned int RenderEngineUI::loadFontSize(float fSize)
{
   fSize *= hdmi_get_current_resolution_height();
   int nSize = fSize/2;
   nSize *= 2;
   if ( nSize < 14 )
      nSize = 14;
   if ( nSize > 56 )
      nSize = 56;

   if ( s_uRenderEngineUIFontsListSizes[nSize] == MAX_U32 )
      return 0;
   if ( s_uRenderEngineUIFontsListSizes[nSize] != 0 )
      return s_uRenderEngineUIFontsListSizes[nSize];

   Preferences* pP = get_Preferences();

   char szFont[32];
   char szFileName[256];

   strcpy(szFont, "raw_bold");
   if ( pP->iOSDFont == 0 )
      strcpy(szFont, "raw_bold");
   if ( pP->iOSDFont == 1 )
      strcpy(szFont, "rawobold");
   if ( pP->iOSDFont == 2 )
      strcpy(szFont, "ariobold");
   if ( pP->iOSDFont == 3 )
      strcpy(szFont, "bt_bold");

   sprintf(szFileName, "res/font_%s_%d.dsc", szFont, nSize );
   if ( access( szFileName, R_OK ) == -1 )
   {
      log_softerror_and_alarm("[RenderEngineOSD] Can't find font: [%s]. Using a default font instead.", szFileName );
      sprintf(szFileName, "res/font_rawobold_%d.dsc", nSize );
      if ( access( szFileName, R_OK ) == -1 )
      {
         log_softerror_and_alarm("[RenderEngineOSD] Can't find font: [%s]. Using a default size instead.", szFileName );
         nSize = 56;
         sprintf(szFileName, "res/font_rawobold_%d.dsc", nSize );
      }
   }

   s_uRenderEngineUIFontsListSizes[nSize] = s_pRenderEngineUI->loadRawFont(pP->iOSDFont, szFileName, 0);
   if ( 0 == s_uRenderEngineUIFontsListSizes[nSize] )
   {
      log_softerror_and_alarm("[RenderEngineOSD] Can't load font %s (%s)", szFont, szFileName);
      s_uRenderEngineUIFontsListSizes[nSize] = MAX_U32;
      return 0;
   }
   log_line("[RenderEngineOSD] Loaded font: %s (%s)", szFont, szFileName);
   return s_uRenderEngineUIFontsListSizes[nSize];
}

const double* RenderEngineUI::getColorOSDText()
{
   return get_Color_OSDText();
}

const double* RenderEngineUI::getColorOSDInstruments()
{
   Preferences* pP = get_Preferences();
   if ( NULL == pP )
      return getColorOSDText();

   static double s_ColorRenderUIInstruments[4];

   s_ColorRenderUIInstruments[0] = pP->iColorAHI[0];
   s_ColorRenderUIInstruments[1] = pP->iColorAHI[1];
   s_ColorRenderUIInstruments[2] = pP->iColorAHI[2];
   s_ColorRenderUIInstruments[3] = pP->iColorAHI[3]/100.0;
   return &s_ColorRenderUIInstruments[0];
}

const double* RenderEngineUI::getColorOSDOutline()
{
   static double s_ColorRenderUIOutline[4] = { 0,0,0, 0.4 };
   return &s_ColorRenderUIOutline[0];
}

const double* RenderEngineUI::getColorOSDWarning()
{
   return get_Color_IconError();
}

unsigned int RenderEngineUI::loadImage(const char* szFile)
{
   if ( NULL == s_pRenderEngineUI )
      return 0;
   return s_pRenderEngineUI->loadImage(szFile);
}

void RenderEngineUI::freeImage(unsigned int idImage)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->freeImage(idImage);
}

unsigned int RenderEngineUI::loadIcon(const char* szFile)
{
   if ( NULL == s_pRenderEngineUI )
      return 0;
   return s_pRenderEngineUI->loadIcon(szFile);
}

void RenderEngineUI::freeIcon(unsigned int idIcon)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->freeIcon(idIcon);
}


void RenderEngineUI::highlightFirstWordOfLine(bool bHighlight)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->highlightFirstWordOfLine(bHighlight);
}

bool RenderEngineUI::drawBackgroundBoundingBoxes(bool bEnable)
{
   if ( NULL == s_pRenderEngineUI )
      return false;
   return s_pRenderEngineUI->drawBackgroundBoundingBoxes(bEnable);
}

void RenderEngineUI::setColors(const double* color)
{
   if ( NULL == s_pRenderEngineUI )
      return ;
   s_pRenderEngineUI->setColors(color);
}

void RenderEngineUI::setColors(const double* color, float fAlfaScale)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->setColors(color, fAlfaScale);
}

void RenderEngineUI::setFill(float r, float g, float b, float a)
{
   if ( NULL == s_pRenderEngineUI )
      return ;
   s_pRenderEngineUI->setFill(r,g,b,a);
}

void RenderEngineUI::setStroke(const double* color)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->setStroke(color);
}

void RenderEngineUI::setStroke(const double* color, float fStrokeSize)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->setStroke(color, fStrokeSize);
}

void RenderEngineUI::setStroke(float r, float g, float b, float a)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->setStroke(r,g,b,a);
}

float RenderEngineUI::getStrokeSize()
{
   if ( NULL == s_pRenderEngineUI )
      return 0.0;
   return s_pRenderEngineUI->getStrokeSize();
}

void RenderEngineUI::setStrokeSize(float fStrokeSize)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->setStrokeSize(fStrokeSize);
}

void RenderEngineUI::setFontColor(unsigned int fontId, double* color)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->setFontColor(fontId, color);
}

void RenderEngineUI::drawImage(float xPos, float yPos, float fWidth, float fHeight, unsigned int imageId)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->drawImage(xPos, yPos, fWidth, fHeight, imageId);
}

void RenderEngineUI::drawIcon(float xPos, float yPos, float fWidth, float fHeight, unsigned int iconId)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->drawIcon(xPos, yPos, fWidth, fHeight, iconId);
}

float RenderEngineUI::textHeight(unsigned int fontId)
{
   if ( NULL == s_pRenderEngineUI )
      return 0.0;
   return s_pRenderEngineUI->textRawHeight(fontId);
}

float RenderEngineUI::textWidth(unsigned int fontId, const char* szText)
{
   if ( NULL == s_pRenderEngineUI )
      return 0.0;
   return s_pRenderEngineUI->textRawWidth(fontId, szText);
}

void RenderEngineUI::drawText(float xPos, float yPos, unsigned int fontId, const char* szText)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->drawText(xPos, yPos, fontId, szText);
}

void RenderEngineUI::drawTextLeft(float xPos, float yPos, unsigned int fontId, const char* szText)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->drawTextLeft(xPos, yPos, fontId, szText);
}

float RenderEngineUI::getMessageHeight(const char* text, float line_spacing_percent, float max_width, unsigned int fontId)
{
   if ( NULL == s_pRenderEngineUI )
      return 0.0;
   return s_pRenderEngineUI->getMessageHeight(text, line_spacing_percent, max_width, fontId);
}

float RenderEngineUI::drawMessageLines(const char* text, float xPos, float yPos, float line_spacing_percent, float max_width, unsigned int fontId)
{
   if ( NULL == s_pRenderEngineUI )
      return 0.0;
   return s_pRenderEngineUI->drawMessageLines(xPos, yPos, text, line_spacing_percent, max_width, fontId);
}

void RenderEngineUI::drawLine(float x1, float y1, float x2, float y2)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->drawLine(x1,y1,x2,y2);
}

void RenderEngineUI::drawRect(float xPos, float yPos, float fWidth, float fHeight)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->drawRect(xPos,yPos,fWidth, fHeight);
}

void RenderEngineUI::drawRoundRect(float xPos, float yPos, float fWidth, float fHeight, float fCornerRadius)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->drawRoundRect(xPos,yPos,fWidth, fHeight, fCornerRadius);
}

void RenderEngineUI::drawTriangle(float x1, float y1, float x2, float y2, float x3, float y3)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->drawTriangle(x1,y1,x2,y2,x3,y3);
}

void RenderEngineUI::drawPolyLine(float* x, float* y, int count)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->drawPolyLine(x,y,count);
}

void RenderEngineUI::fillPolygon(float* x, float* y, int count)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->fillPolygon(x,y,count);
}

void RenderEngineUI::fillCircle(float x, float y, float r)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->fillCircle(x,y,r);
}

void RenderEngineUI::drawCircle(float x, float y, float r)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->drawCircle(x,y,r);
}

void RenderEngineUI::drawArc(float x, float y, float r, float a1, float a2)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->drawArc(x,y,r,a1,a2);
}
