/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga petrusoroaga@yahoo.com
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

#include "render_engine.h"
#include "../base/config_hw.h"
#include <math.h>

#if defined (HW_PLATFORM_RASPBERRY)
//#include "render_engine_ovg.h"
#include "render_engine_raw.h"
#endif

#if defined (HW_PLATFORM_RADXA)
#include "render_engine_cairo.h"
#endif

#include "../base/base.h"
#include "../base/hardware.h"

static RenderEngine* s_pRenderEngine = NULL;
static bool s_bRenderEngineSupportsRawFonts = false;

RenderEngine* render_init_engine()
{
   log_line("[RenderEngine] Renderer Engine Init...");
   if ( NULL == s_pRenderEngine )
   {
      #if defined (HW_PLATFORM_RASPBERRY)
      s_bRenderEngineSupportsRawFonts = true;
      s_pRenderEngine = new RenderEngineRaw();
      #endif
      #if defined (HW_PLATFORM_RADXA)
      s_bRenderEngineSupportsRawFonts = true;
      s_pRenderEngine = new RenderEngineCairo();
      #endif

      if (NULL != s_pRenderEngine )
         s_pRenderEngine->initEngine();
   }
   return s_pRenderEngine;
}

bool render_engine_uses_raw_fonts()
{
   return s_bRenderEngineSupportsRawFonts;  
}

void render_free_engine()
{
   if ( NULL != s_pRenderEngine )
   {
      s_pRenderEngine->uninitEngine();
      delete s_pRenderEngine;
   }
   s_pRenderEngine = NULL;
}

extern "C" {
void render_engine_test()
{
   printf("\nTest engine\n");
}
}  

RenderEngine* renderer_engine()
{
   if ( NULL == s_pRenderEngine )
      s_pRenderEngine = render_init_engine();
   return s_pRenderEngine;
}

RenderEngine::RenderEngine()
{
   m_bStartedFrame = false;
   m_iRenderDepth = 0;
   m_iRenderWidth = 0;
   m_iRenderHeight = 0;
   m_fPixelWidth = 0.0;
   m_fPixelHeight = 0.0;
   m_uClearBufferByte = 0;
   m_fGlobalAlfa = 1.0;
   m_bEnableRectBlending = true;
   m_bEnableFontScaling = false;
   m_bHighlightFirstWord = false;
   m_bDrawBackgroundBoundingBoxes = false;
   m_bDrawBackgroundBoundingBoxesTextUsesSameStrokeColor = false;
   m_fBoundingBoxPadding = 0.0;

   m_bDrawStrikeOnTextBackgroundBoundingBoxes = false;
   m_bDisableTextOutline = false;

   m_uTextFontMixColor[0] = m_uTextFontMixColor[1] = m_uTextFontMixColor[2] = m_uTextFontMixColor[3] = 0xFF;
   m_ColorFill[0] = m_ColorFill[1] = m_ColorFill[2] = m_ColorFill[3] = 0;
   m_ColorStroke[0] = m_ColorStroke[1] = m_ColorStroke[2] = m_ColorStroke[3] = 0;
   m_ColorTextBoundingBoxBgFill[0] = m_ColorTextBoundingBoxBgFill[1] = m_ColorTextBoundingBoxBgFill[2] = m_ColorTextBoundingBoxBgFill[3] = 0;
   m_fStrokeSizePx = 0.0;

   m_CurrentRawFontId = 0;
   m_iCountRawFonts = 0;
}


RenderEngine::~RenderEngine()
{
}

bool RenderEngine::initEngine()
{
   log_line("[RenderEngineRaw] RenderEngineBase::initEngine");
   return true;
}

bool RenderEngine::uninitEngine()
{
   log_line("[RenderEngineRaw] RenderEngineBase::uninitEngine");
   return true;
}

int RenderEngine::getScreenWidth()
{
   return m_iRenderWidth;
}

int RenderEngine::getScreenHeight()
{
   return m_iRenderHeight;
}

float RenderEngine::getAspectRatio()
{
   return (float)m_iRenderWidth/(float)m_iRenderHeight;
}

float RenderEngine::getPixelWidth()
{
   return m_fPixelWidth;
}

float RenderEngine::getPixelHeight()
{
   return m_fPixelHeight;
}

void* RenderEngine::getDrawContext()
{
   return NULL;
}


void RenderEngine::highlightFirstWordOfLine(bool bHighlight)
{
   m_bHighlightFirstWord = bHighlight;
}

bool RenderEngine::drawBackgroundBoundingBoxes(bool bEnable)
{
   bool b = m_bDrawBackgroundBoundingBoxes;
   m_bDrawBackgroundBoundingBoxes = bEnable;
   return b;
}

void RenderEngine::setBackgroundBoundingBoxPadding(float fPadding)
{
   m_fBoundingBoxPadding = fPadding;
}

void RenderEngine::setFontBackgroundBoundingBoxSameTextColor(bool bSameColor)
{
   m_bDrawBackgroundBoundingBoxesTextUsesSameStrokeColor = bSameColor;
}

bool RenderEngine::getFontBackgroundBoundingBoxSameTextColor()
{
   return m_bDrawBackgroundBoundingBoxesTextUsesSameStrokeColor;
}

void RenderEngine::setFontBackgroundBoundingBoxStrikeColor(const double* color)
{
   memcpy(m_ColorTextBackgroundBoundingBoxStrike, color, 4*sizeof(double));
   m_bDrawStrikeOnTextBackgroundBoundingBoxes = true;

   float fAlpha = color[3]*m_fGlobalAlfa;
   if ( fAlpha > 1.0 )
      fAlpha = 1.0;
   if ( fAlpha < 0.0 )
      fAlpha = 0.0;

   m_ColorTextBackgroundBoundingBoxStrike[3] = fAlpha * 255;
}

void RenderEngine::clearFontBackgroundBoundingBoxStrikeColor()
{
   m_bDrawStrikeOnTextBackgroundBoundingBoxes = false;
}

float RenderEngine::setGlobalAlfa(float alfa)
{
   float f = m_fGlobalAlfa;
   m_fGlobalAlfa = alfa;
   return f;
}
 
float RenderEngine::getGlobalAlfa()
{
   return m_fGlobalAlfa;
}

bool RenderEngine::isRectBlendingEnabled()
{
   return m_bEnableRectBlending;
}

void RenderEngine::setRectBlendingEnabled(bool bEnable)
{
   m_bEnableRectBlending = bEnable;
}

void RenderEngine::enableRectBlending()
{
   m_bEnableRectBlending = true;
}

void RenderEngine::disableRectBlending()
{
   m_bEnableRectBlending = false;
}

void RenderEngine::setClearBufferByte(u8 uClearByte)
{
   m_uClearBufferByte = uClearByte;
}

void RenderEngine::setColors(const double* color)
{
   setColors(color, 1.0);
}

void RenderEngine::setColors(const double* color, float fAlfaScale)
{
   m_ColorFill[0] = color[0];
   m_ColorFill[1] = color[1];
   m_ColorFill[2] = color[2];

   m_ColorStroke[0] = color[0];
   m_ColorStroke[1] = color[1];
   m_ColorStroke[2] = color[2];

   m_uTextFontMixColor[0] = color[0];
   m_uTextFontMixColor[1] = color[1];
   m_uTextFontMixColor[2] = color[2];

   float fAlpha = color[3]*m_fGlobalAlfa*fAlfaScale;
   if ( fAlpha > 1.0 )
      fAlpha = 1.0;
   if ( fAlpha < 0.0 )
      fAlpha = 0.0;
   m_ColorFill[3] = fAlpha * 255;
   m_ColorStroke[3] = fAlpha * 255;
   m_uTextFontMixColor[3] = 255 * fAlpha;
}

void RenderEngine::setFill(const double* pColor)
{
   m_ColorFill[0] = pColor[0];
   m_ColorFill[1] = pColor[1];
   m_ColorFill[2] = pColor[2];

   m_uTextFontMixColor[0] = pColor[0];
   m_uTextFontMixColor[1] = pColor[1];
   m_uTextFontMixColor[2] = pColor[2];

   float fAlpha = pColor[3]*m_fGlobalAlfa;
   if ( fAlpha > 1.0 )
      fAlpha = 1.0;
   if ( fAlpha < 0.0 )
      fAlpha = 0.0;
   m_ColorFill[3] = fAlpha * 255;
   m_uTextFontMixColor[3] = 255 * fAlpha;
}

void RenderEngine::setFill(float r, float g, float b, float a)
{
   m_ColorFill[0] = r;
   m_ColorFill[1] = g;
   m_ColorFill[2] = b;
   m_uTextFontMixColor[0] = r;
   m_uTextFontMixColor[1] = g;
   m_uTextFontMixColor[2] = b;

   float fAlpha = a*m_fGlobalAlfa;
   if ( fAlpha > 1.0 )
      fAlpha = 1.0;
   if ( fAlpha < 0.0 )
      fAlpha = 0.0;
   m_ColorFill[3] = fAlpha * 255;
   m_uTextFontMixColor[3] = 255 * fAlpha;
}

void RenderEngine::setStroke(const double* color)
{
   setStroke(color, 1.0);
}

void RenderEngine::setStroke(const double* color, float fStrokeSize)
{
   m_ColorStroke[0] = color[0];
   m_ColorStroke[1] = color[1];
   m_ColorStroke[2] = color[2];

   float fAlpha = color[3]*m_fGlobalAlfa;
   if ( fAlpha > 1.0 )
      fAlpha = 1.0;
   if ( fAlpha < 0.0 )
      fAlpha = 0.0;

   m_ColorStroke[3] = fAlpha * 255;
   m_fStrokeSizePx = fStrokeSize;

   // Is it in pixel size? convert to pixels;
   if ( fStrokeSize < 0.5 )
      m_fStrokeSizePx = fStrokeSize / m_fPixelWidth;
}

void RenderEngine::setStroke(float r, float g, float b, float a)
{
   m_ColorStroke[0] = r;
   m_ColorStroke[1] = g;
   m_ColorStroke[2] = b;

   float fAlpha = a*m_fGlobalAlfa;
   if ( fAlpha > 1.0 )
      fAlpha = 1.0;
   if ( fAlpha < 0.0 )
      fAlpha = 0.0;

   m_ColorStroke[3] = fAlpha * 255;
}

float RenderEngine::getStrokeSize()
{
   return m_fStrokeSizePx;
}

void RenderEngine::setStrokeSize(float fStrokeSize)
{
   m_fStrokeSizePx = fStrokeSize;

   // Is it not in pixel size? convert to pixels;
   if ( fStrokeSize < 0.5 )
      m_fStrokeSizePx = fStrokeSize / m_fPixelWidth;
}

void RenderEngine::setFontColor(u32 fontId, double* color)
{
}

void RenderEngine::setFontBackgroundBoundingBoxFillColor(const double* color)
{
   m_ColorTextBoundingBoxBgFill[0] = color[0];
   m_ColorTextBoundingBoxBgFill[1] = color[1];
   m_ColorTextBoundingBoxBgFill[2] = color[2];

   float fAlpha = color[3]*m_fGlobalAlfa;
   if ( fAlpha > 1.0 )
      fAlpha = 1.0;
   if ( fAlpha < 0.0 )
      fAlpha = 0.0;

   m_ColorTextBoundingBoxBgFill[3] = fAlpha * 255;
}

void RenderEngine::enableFontScaling(bool bEnable)
{
   m_bEnableFontScaling = bEnable;
}


int RenderEngine::_getRawFontIndexFromId(u32 fontId)
{
   for( int i=0; i<m_iCountRawFonts; i++ )
   {
      if ( m_RawFontIds[i] == fontId )
         return i;
   }
   log_softerror_and_alarm("[RenderEngineRaw] Tried to access inexistent font id %u, currently loaded %d fonts", fontId, m_iCountRawFonts);
   return -1;
}

RenderEngineRawFont* RenderEngine::_getRawFontFromId(u32 fontId)
{
   int index = _getRawFontIndexFromId(fontId);
   if ( index < 0 )
      return NULL;
   return m_pRawFonts[index];
}


u32 RenderEngine::_getRawFontId(RenderEngineRawFont* pRawFont)
{
   if ( NULL == pRawFont )
   {
      log_softerror_and_alarm("[RenderEngineRaw] Tried to find invalid NULL font object, currently loaded %d fonts", m_iCountRawFonts);
      return 0;
   }
   for( int i=0; i<m_iCountRawFonts; i++ )
   {
      if ( m_pRawFonts[i] == pRawFont )
         return m_RawFontIds[i];
   }
   log_softerror_and_alarm("[RenderEngineRaw] Tried to find inexistent font object, currently loaded %d fonts", m_iCountRawFonts);
   return 0;
}

float RenderEngine::_get_raw_space_width(RenderEngineRawFont* pFont)
{
   if ( NULL == pFont )
      return 0.0;

   return pFont->lineHeight*0.25*m_fPixelHeight + pFont->lineHeight * pFont->dxLetters * m_fPixelWidth;
}
   
float RenderEngine::_get_raw_char_width(RenderEngineRawFont* pFont, int ch)
{
   if ( NULL == pFont )
      return 0.0;

   if ( ch == ' ' )
      return _get_raw_space_width(pFont);
   
   float fWidth = 0.0;

   if ( (ch >= pFont->charIdFirst) && (ch <= pFont->charIdLast) )
   {
      fWidth = pFont->chars[ch-pFont->charIdFirst].xAdvance * m_fPixelWidth;
      fWidth += pFont->dxLetters * pFont->lineHeight*m_fPixelWidth;
   }
   return fWidth;
}

void RenderEngine::_drawSimpleText(RenderEngineRawFont* pFont, const char* szText, float xPos, float yPos)
{

}

void RenderEngine::_drawSimpleTextScaled(RenderEngineRawFont* pFont, const char* szText, float xPos, float yPos, float fScale)
{

}

void RenderEngine::_freeRawFontImageObject(void* pImageObject)
{
}

void* RenderEngine::_loadRawFontImageObject(const char* szFileName)
{
   return NULL;
}

int RenderEngine::loadRawFont(int iFamilyId, const char* szFontFile, int iBold)
{
   if ( m_iCountRawFonts >= MAX_RAW_FONTS )
   {
      log_error_and_alarm("[RenderEngineRaw] Too many fonts (%d). Can't load more.", m_iCountRawFonts);
      return -1;
   }
   if ( access( szFontFile, R_OK ) == -1 )
   {
      log_error_and_alarm("[RenderEngineRaw] Can't access font file (%s).", szFontFile);
      return -2;
   }
   char szFile[256];
   FILE* fd = fopen(szFontFile, "r");
   if ( NULL == fd )
   {
      log_error_and_alarm("[RenderEngineRaw] Can't read font file (%s).", szFontFile);
      return -3;
   }
   m_pRawFonts[m_iCountRawFonts] = (RenderEngineRawFont*) malloc(sizeof(RenderEngineRawFont));
   m_pRawFonts[m_iCountRawFonts]->iFamilyId = iFamilyId;
   m_pRawFonts[m_iCountRawFonts]->bBold = (iBold?true:false);
   strncpy(m_pRawFonts[m_iCountRawFonts]->szName, szFontFile, 127);
   sprintf(szFile, "%s", szFontFile);
   szFile[strlen(szFile)-3] = 'p';
   szFile[strlen(szFile)-2] = 'n';
   szFile[strlen(szFile)-1] = 'g';

   log_line("[RenderEngineRaw] Loading font: %s", szFile);
   m_pRawFonts[m_iCountRawFonts]->pImageObject = _loadRawFontImageObject(szFile);
   #if defined (HW_PLATFORM_RADXA)
   #else
   if ( NULL == m_pRawFonts[m_iCountRawFonts]->pImageObject )
   {
      log_softerror_and_alarm("[RenderEngineRaw] Failed to load font: %s (can't load font image)", szFile);
      fclose(fd);
      return -4;
   }
   #endif
   if ( 2 != fscanf(fd, "%d %d", &(m_pRawFonts[m_iCountRawFonts]->lineHeight), &(m_pRawFonts[m_iCountRawFonts]->baseLine) ) )
   {
      log_error_and_alarm("[RenderEngineRaw] Invalid font file (%s).", szFontFile);
      fclose(fd);
      return -5;
   }
   if ( 1 != fscanf(fd, "%d", &(m_pRawFonts[m_iCountRawFonts]->charCount) ) )
   {
      log_error_and_alarm("[RenderEngineRaw] Invalid font file (%s).", szFontFile);
      fclose(fd);
      return -5;
   }
   if ( m_pRawFonts[m_iCountRawFonts]->charCount < 0 || m_pRawFonts[m_iCountRawFonts]->charCount >= MAX_FONT_CHARS )
   {
      log_error_and_alarm("[RenderEngineRaw] Invalid font file (%s).", szFontFile);
      fclose(fd);
      return -5;
   }
   log_line("[RenderEngineRaw] Loaded font: size: lineheight: %d, baseline: %d, count: %d", m_pRawFonts[m_iCountRawFonts]->lineHeight, m_pRawFonts[m_iCountRawFonts]->baseLine, m_pRawFonts[m_iCountRawFonts]->charCount);
   for( int i=0; i<m_pRawFonts[m_iCountRawFonts]->charCount; i++ )
   {
      if ( 5 != fscanf(fd, "%d %d %d %d %d", &(m_pRawFonts[m_iCountRawFonts]->chars[i].charId),
                              &(m_pRawFonts[m_iCountRawFonts]->chars[i].imgXOffset),
                              &(m_pRawFonts[m_iCountRawFonts]->chars[i].imgYOffset),
                              &(m_pRawFonts[m_iCountRawFonts]->chars[i].width),
                              &(m_pRawFonts[m_iCountRawFonts]->chars[i].height) ) )
      {
         log_softerror_and_alarm("[RenderEngineRaw] Failed to load font, invalid c-description.");
         fclose(fd);
         return 0;
      }
      if ( 3 != fscanf(fd, "%d %d %d", &(m_pRawFonts[m_iCountRawFonts]->chars[i].xOffset),
                              &(m_pRawFonts[m_iCountRawFonts]->chars[i].yOffset),
                              &(m_pRawFonts[m_iCountRawFonts]->chars[i].xAdvance) ) )
      {
         log_softerror_and_alarm("[RenderEngineRaw] Failed to load font, invalid cc-description.");
         fclose(fd);
         return 0;
      }
      int page, ch;
      if ( 2 != fscanf(fd, "%d %d", &page, &ch ) )
      {
         log_softerror_and_alarm("[RenderEngineRaw] Failed to load font, invalid p-description.");
         fclose(fd);
         return 0;
      }
     //printf("loaded char: %d %d %d %d %d\n", m_pFonts[m_iCountFonts]->chars[i].charId, 
     //         m_pFonts[m_iCountFonts]->chars[i].imgXOffset, m_pFonts[m_iCountFonts]->chars[i].imgYOffset, m_pFonts[m_iCountFonts]->chars[i].width, m_pFonts[m_iCountFonts]->chars[i].height );
   }

   /*
   if ( 1 != fscanf(fd, "%d", &(m_pFonts[m_iCountFonts]->keringsCount) ) )
   {
      log_softerror_and_alarm("Failed to load font, invalid k-description.");
      return 0;
   }
   if ( m_pFonts[m_iCountFonts]->keringsCount < 0 || m_pFonts[m_iCountFonts]->keringsCount >= MAX_FONT_KERINGS )
   {
      log_softerror_and_alarm("Failed to load font, invalid k-description.");
      return 0;
   }
   for( int i=0; i<m_pFonts[m_iCountFonts]->keringsCount; i++ )
   {
      if ( 3 != fscanf(fd, "%d %d %d", &(m_pFonts[m_iCountFonts]->kerings[i][0]),
                              &(m_pFonts[m_iCountFonts]->kerings[i][1]),
                              &(m_pFonts[m_iCountFonts]->kerings[i][2]) ) )
      {
         log_softerror_and_alarm("Failed to load font, invalid k-description.");
         return 0;
      }
   }
   */
   fclose(fd);

   m_pRawFonts[m_iCountRawFonts]->charIdFirst = m_pRawFonts[m_iCountRawFonts]->chars[0].charId;
   m_pRawFonts[m_iCountRawFonts]->charIdLast = m_pRawFonts[m_iCountRawFonts]->chars[m_pRawFonts[m_iCountRawFonts]->charCount-1].charId;
   m_pRawFonts[m_iCountRawFonts]->dxLetters = 0.0;
   m_CurrentRawFontId++;
   m_RawFontIds[m_iCountRawFonts] = m_CurrentRawFontId;

   log_line("[RenderEngineRaw] Loaded font %s, id: %u (%d of max %d)",
       szFile, m_RawFontIds[m_iCountRawFonts], m_iCountRawFonts+1, MAX_RAW_FONTS);
   log_line("[RenderEngineRaw] Font %s: chars: %d to %d, count %d",
       szFile, m_pRawFonts[m_iCountRawFonts]->charIdFirst, m_pRawFonts[m_iCountRawFonts]->charIdLast, m_pRawFonts[m_iCountRawFonts]->charCount);

   log_line("[RenderEngineRaw] Font %s: kering %d, id: %u; %d currently loaded fonts",
       szFile, m_pRawFonts[m_iCountRawFonts]->keringsCount, m_RawFontIds[m_iCountRawFonts], m_iCountRawFonts+1);

   m_iCountRawFonts++;
   return (int)m_CurrentRawFontId;
}

void RenderEngine::freeRawFont(u32 idFont)
{
   int indexFont = _getRawFontIndexFromId(idFont);
   if ( -1 == indexFont )
   {
      log_softerror_and_alarm("[RenderEngineRaw] Tried to delete invalid raw font id %u, not in the list (%d raw fonts)", idFont, m_iCountRawFonts);
      return;
   }

   _freeRawFontImageObject(m_pRawFonts[indexFont]->pImageObject);
   free(m_pRawFonts[indexFont]);

   for( int i=indexFont; i<m_iCountRawFonts-1; i++ )
   {
      m_pRawFonts[i] = m_pRawFonts[i+1];
      m_RawFontIds[i] = m_RawFontIds[i+1];
   }
   m_iCountRawFonts--;
   log_line("[RenderEngineRaw] Unloaded font id %u, remaining fonts: %d", idFont, m_iCountRawFonts);
}

void RenderEngine::setFontOutlineColor(u32 idFont, u8 r, u8 g, u8 b, u8 a)
{
 
}

u32 RenderEngine::loadImage(const char* szFile)
{
   return 0;
}

void RenderEngine::freeImage(u32 idImage)
{
}

u32 RenderEngine::loadIcon(const char* szFile)
{
   return 0;
}

void RenderEngine::freeIcon(u32 idIcon)
{
}

int RenderEngine::getImageWidth(u32 uImageId)
{
   return 0;
}

int RenderEngine::getImageHeight(u32 uImageId)
{
   return 0;
}

void RenderEngine::changeImageHue(u32 uImageId, u8 r, u8 g, u8 b)
{

}

     
void RenderEngine::startFrame()
{
   if ( m_bStartedFrame  )
      return;
   m_bStartedFrame = true;
   m_iRenderDepth++;
}

void RenderEngine::endFrame()
{
   if ( ! m_bStartedFrame )
      return;

   m_bStartedFrame = false;
   m_iRenderDepth--;
}

bool RenderEngine::isFrameStarted()
{
   return m_bStartedFrame;
}


void RenderEngine::rotate180()
{
}

void RenderEngine::drawImage(float xPos, float yPos, float fWidth, float fHeight, u32 imageId)
{
}

void RenderEngine::drawImageAlpha(float xPos, float yPos, float fWidth, float fHeight, u32 imageId, u8 uAlpha)
{
 
}

void RenderEngine::bltImage(float xPosDest, float yPosDest, float fWidthDest, float fHeightDest, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, u32 uImageId)
{

}

void RenderEngine::bltSprite(float xPosDest, float yPosDest, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, u32 uImageId)
{

}


void RenderEngine::drawIcon(float xPos, float yPos, float fWidth, float fHeight, u32 iconId)
{
}

void RenderEngine::bltIcon(float xPosDest, float yPosDest, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, u32 iconId)
{

}


float RenderEngine::getRawFontHeight(u32 fontId)
{
   RenderEngineRawFont* pFont = _getRawFontFromId(fontId);
   if ( NULL == pFont )
      return 0.02;
   return pFont->lineHeight * m_fPixelHeight;
}

float RenderEngine::textHeight(u32 fontId)
{
   return textRawHeight(fontId);
}

float RenderEngine::textWidth(u32 fontId, const char* szText)
{
   return textRawWidth(fontId, szText);
}

float RenderEngine::textRawHeight(u32 fontId)
{
   RenderEngineRawFont* pFont = _getRawFontFromId(fontId);
   if ( NULL == pFont )
      return 10.0 * m_fPixelHeight;

   return pFont->lineHeight * m_fPixelHeight;
}

float RenderEngine::textRawWidth(u32 fontId, const char* szText)
{
   if ( NULL == szText )
      return 0.0;

   return textRawWidthScaled(fontId, 1.0, szText);
}

float RenderEngine::textRawWidthScaled(u32 fontId, float fScale, const char* szText)
{
   if ( NULL == szText )
      return 0.0;

   RenderEngineRawFont* pFont = _getRawFontFromId(fontId);
   if ( NULL == pFont )
      return 0.0;

   float fWidth = 0.0;
   char* p = (char*)szText;

   while ( (*p) != 0 )
   {
      fWidth += _get_raw_char_width(pFont, (*p));
      p++;
   }

   return fWidth * fScale;
}

void RenderEngine::_drawSimpleTextBoundingBox(RenderEngineRawFont* pFont, const char* szText, float xPos, float yPos, float fScale)
{
   u32 uFontId = _getRawFontId(pFont);
   float fTextWidth = textRawWidthScaled(uFontId, fScale, szText);
   float fTextHeight = textRawHeight(uFontId);
   float space_width = _get_raw_space_width(pFont);

   if ( (fTextWidth < 0.0001) || (fTextHeight < 0.0001) )
      return;

   float xBoundingStart = xPos;
   float yBoundingStart = yPos;
   float xBoundingEnd = xPos + fTextWidth;
   float yBoundingEnd = yPos + fTextHeight;

   xBoundingStart -= 0.7 * space_width;
   xBoundingEnd += 0.7 * space_width;
   yBoundingStart += 1 * m_fPixelHeight;
   yBoundingEnd -= 2 * m_fPixelHeight;
   yBoundingStart += pFont->lineHeight*0.03 * m_fPixelHeight;
   yBoundingEnd -= pFont->lineHeight*0.01 * m_fPixelHeight;

   u8 tmp_ColorFill[4];
   u8 tmp_ColorStroke[4];
   float tmp_fStrokeSize = m_fStrokeSizePx;

   memcpy(tmp_ColorFill, m_ColorFill, 4*sizeof(u8));
   memcpy(tmp_ColorStroke, m_ColorStroke, 4*sizeof(u8));
   memcpy(m_ColorFill, m_ColorTextBoundingBoxBgFill, 4*sizeof(u8));
   m_ColorStroke[0] = 0; m_ColorStroke[1] = 0; m_ColorStroke[2] = 0; m_ColorStroke[3] = 0;
   m_fStrokeSizePx = 0.0;
   if ( m_bDrawStrikeOnTextBackgroundBoundingBoxes )
   {
      m_ColorStroke[0] = m_ColorTextBackgroundBoundingBoxStrike[0];
      m_ColorStroke[1] = m_ColorTextBackgroundBoundingBoxStrike[1];
      m_ColorStroke[2] = m_ColorTextBackgroundBoundingBoxStrike[2];
      m_ColorStroke[3] = m_ColorTextBackgroundBoundingBoxStrike[3];
      m_fStrokeSizePx = 1.0;
   }
   drawRoundRect(xBoundingStart - m_fBoundingBoxPadding/getAspectRatio(), yBoundingStart - m_fBoundingBoxPadding, (xBoundingEnd - xBoundingStart) + 2.0*m_fBoundingBoxPadding/getAspectRatio(), (yBoundingEnd - yBoundingStart) + 2.0*m_fBoundingBoxPadding, 5.0 );

   memcpy(m_ColorFill, tmp_ColorFill, 4*sizeof(u8));
   memcpy(m_ColorStroke, tmp_ColorStroke, 4*sizeof(u8));
   m_fStrokeSizePx = tmp_fStrokeSize;
}

void RenderEngine::drawText(float xPos, float yPos, u32 fontId, const char* szText)
{
   drawTextScaled(xPos, yPos, fontId, 1.0, szText);
}

void RenderEngine::drawTextNoOutline(float xPos, float yPos, u32 fontId, const char* szText)
{
   bool bTmp = m_bDisableTextOutline;
   m_bDisableTextOutline = true;
   drawTextScaled(xPos, yPos, fontId, 1.0, szText);
   m_bDisableTextOutline = bTmp;
}

void RenderEngine::drawTextLeft(float xPos, float yPos, u32 fontId, const char* szText)
{
   drawTextLeftScaled(xPos, yPos, fontId, 1.0, szText);
}


void RenderEngine::drawTextScaled(float xPos, float yPos, u32 fontId, float fScale, const char* szText)
{
   if ( NULL == szText )
      return;

   RenderEngineRawFont* pFont = _getRawFontFromId(fontId);
   if ( NULL == pFont )
   {
      log_error_and_alarm("[RenderEngineRaw] Tried to drawTextScaled using a NULL font object");
      return;
   }
   if ( yPos < 0 )
      return;

   m_uTextFontMixColor[0] = m_ColorFill[0];
   m_uTextFontMixColor[1] = m_ColorFill[1];
   m_uTextFontMixColor[2] = m_ColorFill[2];
   m_uTextFontMixColor[3] = m_ColorFill[3];

   //y += fScale*(pFont->lineHeight-pFont->baseLine )/3;

   if ( yPos + fScale * (float)pFont->lineHeight * m_fPixelHeight >= 1.0 )
      return;

   if ( fabs(fScale-1.0) > m_fPixelWidth )
       _drawSimpleTextScaled(pFont, szText, xPos, yPos, fScale);
   else
       _drawSimpleText(pFont, szText, xPos, yPos);
}

void RenderEngine::drawTextLeftScaled(float xPos, float yPos, u32 fontId, float fScale, const char* szText)
{
   if ( NULL == szText )
      return;

   RenderEngineRawFont* pFont = _getRawFontFromId(fontId);
   if ( NULL == pFont )
   {
      log_error_and_alarm("[RenderEngineRaw] Tried to drawTextLeftScaled using a NULL font object");
      return;
   }
   if ( yPos < 0 )
      return;

   m_uTextFontMixColor[0] = m_ColorFill[0];
   m_uTextFontMixColor[1] = m_ColorFill[1];
   m_uTextFontMixColor[2] = m_ColorFill[2];
   m_uTextFontMixColor[3] = m_ColorFill[3];

   //y += fScale*(pFont->lineHeight-pFont->baseLine )/3;

   if ( yPos + fScale*pFont->lineHeight * m_fPixelHeight >= 1.0 )
      return;

   float wText = textRawWidthScaled(fontId, fScale, szText);

   if ( fabs(fScale-1.0) > m_fPixelWidth )
      _drawSimpleTextScaled(pFont, szText, xPos-wText, yPos, fScale);
   else
      _drawSimpleText(pFont, szText, xPos-wText, yPos);
 }

float RenderEngine::getMessageWidth(const char* text, float max_width, u32 fontId)
{
   if ( (NULL == text) || (0 == text[0]) )
      return 0.0;

   RenderEngineRawFont* pFont = _getRawFontFromId(fontId);
   if ( NULL == pFont )
      return 0.0;

   float fMaxLineWidth = 0.0;

   char szText[256];
   memset(szText, 0, sizeof(szText));
   strncpy(szText, text, sizeof(szText)-2);
   //const char* szTokens = " ，。\n";
   char* szParse = szText;
   char* szWord = NULL;
   float line_width = 0;
   int countWords = 0;
   int countLineWords = 0;
   int countLines = 0;

   float space_width = _get_raw_space_width(pFont);

   while ( *szParse )
   {
      szWord = szParse;
      while ( *szParse )
      {
         if ( ((*szParse) == ' ') || ((*szParse) == '\n') )
         {
            *szParse = 0;
            szParse++;
            break;
         }
         szParse++;
      }
      if ( NULL == szWord )
         break;
      if ( 0 == szWord[0] )
         continue;
      countWords++;
      countLineWords++;
      float fWidthWord = textRawWidthScaled(fontId, 1.0, szWord);

      if ( (line_width+fWidthWord) <= max_width + 0.0001 )
      {
         line_width += fWidthWord;
         if ( countLineWords > 1 )
            line_width += space_width;
         if ( line_width > fMaxLineWidth )
            fMaxLineWidth = line_width;
      }
      else
      {
         countLines++;
         line_width = fWidthWord;
         if ( line_width > fMaxLineWidth )
            fMaxLineWidth = line_width;
         countLineWords = 1;
      }

      if ( (0 != szWord[0]) && (szWord[strlen(szWord)-1] == '\n') )
      {
         countLines++;
         
         line_width = 0.0;
         countLineWords = 0;
      }
   }

   return fMaxLineWidth;
}

float RenderEngine::getMessageHeight(const char* text, float line_spacing_percent, float max_width, u32 fontId)
{
   if ( (NULL == text) || (0 == text[0]) )
      return 0.0;

   float fTextHeight = textHeight(fontId);
   RenderEngineRawFont* pFont = _getRawFontFromId(fontId);
   if ( NULL == pFont )
      return 0.05;

   float height = 0.0;

   char szText[256];
   memset(szText, 0, sizeof(szText));
   strncpy(szText, text, sizeof(szText)-2);
   //const char* szTokens = " ，。\n";
   char* szParse = szText;
   char* szWord = NULL;
   float line_width = 0;
   int countWords = 0;
   int countLineWords = 0;
   int countLines = 0;

   float space_width = _get_raw_space_width(pFont);

   while ( *szParse )
   {
      szWord = szParse;
      while ( *szParse )
      {
         if ( ((*szParse) == ' ') || ((*szParse) == '\n') )
         {
            *szParse = 0;
            szParse++;
            break;
         }
         szParse++;
      }
      if ( NULL == szWord )
         break;
      if ( 0 == szWord[0] )
         continue;
      countWords++;
      countLineWords++;
      float fWidthWord = textRawWidthScaled(fontId, 1.0, szWord);

      if ( (line_width+fWidthWord) <= max_width + 0.0001 )
      {
         line_width += fWidthWord;
         if ( countLineWords > 1 )
            line_width += space_width;
      }
      else
      {
         height += fTextHeight;
         if ( 0 != countLines )
            height += fTextHeight*line_spacing_percent;
         countLines++;
         line_width = fWidthWord;
         countLineWords = 1;
      }

      if ( (0 != szWord[0]) && (szWord[strlen(szWord)-1] == '\n') )
      {
         height += fTextHeight;
         if ( 0 != countLines )
            height += fTextHeight*line_spacing_percent;
         countLines++;
         
         line_width = 0.0;
         countLineWords = 0;
      }
   }
   if ( (countLineWords > 0) && (line_width > 0.0001) )
   {
      height += fTextHeight;
      if ( 0 != countLines )
         height += fTextHeight*line_spacing_percent;
      countLines++;
   }

   return height;
}

float RenderEngine::drawMessageLines(float xPos, float yPos, const char* text, float line_spacing_percent, float max_width, u32 fontId)
{
   if ( (NULL == text) || (0 == text[0]) )
      return 0.0;

   RenderEngineRawFont* pFont = _getRawFontFromId(fontId);
   if ( NULL == pFont )
   {
      log_error_and_alarm("[RenderEngineRaw] Tried to drawMesageLines using a NULL font object");
      return 0.05;
   }

   if ( yPos < 0 )
      return 0.0;

   float fTextHeight = textHeight(fontId);
   float fScale = 1.0;
 
   m_uTextFontMixColor[0] = m_ColorFill[0];
   m_uTextFontMixColor[1] = m_ColorFill[1];
   m_uTextFontMixColor[2] = m_ColorFill[2];
   m_uTextFontMixColor[3] = m_ColorFill[3];

   if ( m_bHighlightFirstWord )
   {
      m_uTextFontMixColor[0] = 200;
      m_uTextFontMixColor[1] = 200;
      m_uTextFontMixColor[2] = 0;
   }

   //y += fScale*(pFont->lineHeight-pFont->baseLine )/3;

   if ( yPos + pFont->lineHeight*fScale * m_fPixelHeight >= 1.0 )
      return 0.0;

   float height = 0.0;
   char szText[256];
   memset(szText, 0, sizeof(szText));
   strncpy(szText, text, sizeof(szText)-3);
   //const char* szTokens = " ，。\n";
   char* szParse = szText;
   char* szWord = NULL;
   float line_width = 0;
   float space_width = _get_raw_space_width(pFont);
   int countWords = 0;
   int countLineWords = 0;
   int countLines = 0;

   float xTmp = xPos;
   float yTmp = yPos;

   auto isChineseChar = [](const char* ptr) {
      return (ptr[0] & 0xE0) == 0xE0; // The first byte range of Chinese UTF-8 is 0xE0-0xEF
   };
   char outputBuffer[256];
   bool AddSpace = false;
   while (*szParse)
   {
      //If have space
      while (*szParse == ' ' || *szParse == '\n')
      {
         AddSpace = true;
         szParse++;
      }
      if (!*szParse)
         break;

      char *szWordStart = szParse;
      memset(outputBuffer, 0, sizeof(outputBuffer));

      if (isChineseChar(szParse))
      {
         // CN
         strncpy(outputBuffer, szParse, 3);
         szParse += 3;
      }
      else
      {
         // Latin or outhers
         int i = 0;
         while (*szParse && !isChineseChar(szParse) && *szParse != ' ' && *szParse != '\n')
         {
            outputBuffer[i++] = *szParse++;
         }
      }
      
      if (strlen(outputBuffer) > 0)
         szWord = outputBuffer;
      if (NULL == szWord)
         break;
      if (0 == szWord[0])
         continue;

      countWords++;
      countLineWords++;

      float fWidthWord = textRawWidthScaled(fontId, 1.0, szWord);
      if ( (line_width+fWidthWord) <= max_width+0.0002 )
      {
         // Draw on same line
         if ( countLineWords > 1 && AddSpace == true)
         {
            line_width += space_width;
            xTmp += space_width;
            AddSpace = false;
         }
         //if ( m_bEnableFontScaling )
         //   _drawSimpleTextScaled(pFont, szWord, xTmp, yTmp, 0.7);
         //else
            _drawSimpleText(pFont, szWord, xTmp, yTmp);

         line_width += fWidthWord;
         xTmp += fWidthWord;
      }
      else
      {
         // Draw on new line
         height += fTextHeight;
         yTmp += fTextHeight;
         if ( 0 != countLines )
         {
            height += fTextHeight*line_spacing_percent;
            yTmp += fTextHeight*line_spacing_percent;
         }
         countLines++;
         
         xTmp = xPos;
         line_width = fWidthWord;
         countLineWords = 1;
         if ( yTmp + pFont->lineHeight*fScale * m_fPixelHeight >= 1.0 )
            return height;

         //if ( m_bEnableFontScaling )
         //   _drawSimpleTextScaled(pFont, szWord, xTmp, yTmp, 0.7);
         //else
            _drawSimpleText(pFont, szWord, xTmp, yTmp);

         xTmp += fWidthWord;
      }
      if ( (0 != szWord[0]) && (szWord[strlen(szWord)-1] == '\n') )
      {
         height += fTextHeight;
         yTmp += fTextHeight;
         if ( 0 != countLines )
         {
            height += fTextHeight*line_spacing_percent;
            yTmp += fTextHeight*line_spacing_percent;
         }
         countLines++;
         
         xTmp = xPos;
         line_width = 0;
         countLineWords = 0;
         if ( yTmp + pFont->lineHeight*fScale * m_fPixelHeight >= 1.0 )
            return height;
      }

      if ( 1 == countWords && m_bHighlightFirstWord )
      {
         m_uTextFontMixColor[0] = m_ColorFill[0];
         m_uTextFontMixColor[1] = m_ColorFill[1];
         m_uTextFontMixColor[2] = m_ColorFill[2];
      }
   }
   if ( countLineWords > 0 )
   {
      height += fTextHeight;
      if ( 0 != countLines )
         height += fTextHeight*line_spacing_percent;
      countLines++;
   }

   return height;
}

void RenderEngine::drawLine(float x1, float y1, float x2, float y2)
{
}

void RenderEngine::drawRect(float xPos, float yPos, float fWidth, float fHeight)
{
}

void RenderEngine::drawRoundRect(float xPos, float yPos, float fWidth, float fHeight, float fCornerRadius)
{
}

void RenderEngine::drawTriangle(float x1, float y1, float x2, float y2, float x3, float y3)
{
}

void RenderEngine::fillTriangle(float x1, float y1, float x2, float y2, float x3, float y3)
{
}

void RenderEngine::drawPolyLine(float* x, float* y, int count)
{
}

void RenderEngine::fillPolygon(float* x, float* y, int count)
{
}

void RenderEngine::fillCircle(float x, float y, float r)
{
}

void RenderEngine::drawCircle(float x, float y, float r)
{
}

void RenderEngine::drawArc(float x, float y, float r, float a1, float a2)
{
}

bool RenderEngine::rectIntersect(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2)
{
   if ( (x1 + w1 < x2) || (x2+w2 < x1) )
      return false;

   if ( (y1 + h1 < y2) || (y2+h2 < y1) )
      return false;

   return true;
}
