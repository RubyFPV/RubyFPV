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
#include "render_engine_ovg.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "VG/openvg.h"
#include "VG/vgu.h"
#include "fontinfo.h"
#include "shapes.h"
#include <math.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <utime.h>
#include <unistd.h>
#include <getopt.h>
#include <endian.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <fontinfo.h>
#include <time.h>

RenderEngineOVG::RenderEngineOVG()
:RenderEngine()
{
   log_line("RendererOVG: Init started.");

   m_iCountImages = 0;
   m_iCountFonts = 0;
   m_CurrentFontId = 1;
   m_CurrentImageId = 1;

   //InitShapes(&sWidth, &sHeight);
   init(&m_iRenderWidth, &m_iRenderHeight);
   log_line("RendererOVG: Display size is: %d x %d", m_iRenderWidth, m_iRenderHeight);

   m_fPixelWidth = 1.0/(float)m_iRenderWidth;
   m_fPixelHeight = 1.0/(float)m_iRenderHeight;

   log_line("RendererOVG: Render init done.");
}


RenderEngineOVG::~RenderEngineOVG()
{
}


void RenderEngineOVG::setColors(const double* color)
{
   setColors(color, 1.0);
}

void RenderEngineOVG::setColors(const double* color, float fAlfaScale)
{
   float fAlpha = color[3]*m_fGlobalAlfa*fAlfaScale;
   if ( fAlpha > 1.0 )
      fAlpha = 1.0;
   Fill(color[0], color[1], color[2], fAlpha);
   Stroke(color[0], color[1], color[2], fAlpha);
   StrokeWidth(0);
}

void RenderEngineOVG::setFill(float r, float g, float b, float a)
{
   Fill(r,g,b,a);
}

void RenderEngineOVG::setStroke(const double* color)
{
   setStroke(color, 1.0);
}

void RenderEngineOVG::setStroke(const double* color, float fStrokeWidth)
{
   float fAlpha = color[3]*m_fGlobalAlfa;
   if ( fAlpha > 1.0 )
      fAlpha = 1.0;
   Stroke(color[0], color[1], color[2], fAlpha);
   StrokeWidth(fStrokeWidth);
}

void RenderEngineOVG::setStroke(float r, float g, float b, float a)
{
   Stroke(r,g,b,a);
}

void RenderEngineOVG::setStrokeWidth(float fWidth)
{
   StrokeWidth(fWidth);
}

int RenderEngineOVG::loadFont(const char* szFontFile)
{
   if ( m_iCountFonts > 9 )
      return 0;
   m_Fonts[m_iCountFonts] = LoadTTFFile(szFontFile);
   if ( ! m_Fonts[m_iCountFonts] )
   {
      log_softerror_and_alarm("RendererOVG: Failed to load font [%s].", szFontFile);
      return 0;
   }
   m_CurrentFontId++;
   m_FontIds[m_iCountFonts] = m_CurrentFontId;
   m_iCountFonts++;
   return m_CurrentFontId;
}

void RenderEngineOVG::freeFont(u32 idFont)
{
   int indexFont = -1;
   for( int i=0; i<m_iCountFonts; i++ )
      if ( m_FontIds[i] == idFont )
      {
         indexFont = i;
         break;
      }
   if ( -1 == indexFont )
      return;
   free(m_Fonts[indexFont]);

   for( int i=indexFont; i<m_iCountFonts-1; i++ )
   {
      m_Fonts[i] = m_Fonts[i+1];
      m_FontIds[i] = m_FontIds[i+1];
   }
   m_iCountFonts--;
}

u32 RenderEngineOVG::loadImage(const char* szFile)
{
   if ( m_iCountImages > 2 )
      return 0;
   m_Images[m_iCountImages] = CreateImageFromJpeg(szFile);
   m_CurrentImageId++;
   m_ImageIds[m_iCountImages] = m_CurrentImageId;

   m_iCountImages++;
   return m_CurrentImageId;
}

void RenderEngineOVG::freeImage(u32 idImage)
{
   int indexImage = -1;
   for( int i=0; i<m_iCountImages; i++ )
      if ( m_ImageIds[i] == idImage )
      {
         indexImage = i;
         break;
      }
   if ( -1 == indexImage )
      return;

   for( int i=indexImage; i<m_iCountImages-1; i++ )
   {
      m_Images[i] = m_Images[i+1];
      m_ImageIds[i] = m_ImageIds[i+1];
   }
   m_iCountImages--;
}



u32 RenderEngineOVG::loadIcon(const char* szFile)
{
   return 0;
}

void RenderEngineOVG::freeIcon(u32 idIcon)
{
}

void RenderEngineOVG::startFrame()
{
   RenderEngine::startFrame();
   if ( 1 == m_iRenderDepth )
      Start(m_iRenderWidth, m_iRenderHeight);
}

void RenderEngineOVG::endFrame()
{
   RenderEngine::endFrame();
   if ( 0 == m_iRenderDepth )
      End();
}

void RenderEngineOVG::flipScreenOrientation()
{
   Rotate(180);
   Translate(-m_iRenderWidth,-m_iRenderHeight);
}

void RenderEngineOVG::resetScreenOrientation()
{
   Translate(m_iRenderWidth,m_iRenderHeight);
   Rotate(-180);
}

void RenderEngineOVG::translate(float dx, float dy)
{
   Translate(dx,dy);
}

void RenderEngineOVG::rotate(float angle)
{
   Rotate(angle);
}


void RenderEngineOVG::drawImage(float xPos, float yPos, float fWidth, float fHeight, u32 imageId)
{
   int indexImage = -1;
   for( int i=0; i<m_iCountImages; i++ )
      if ( m_ImageIds[i] == imageId )
      {
         indexImage = i;
         break;
      }
   if ( -1 == indexImage )
      return;

   DrawImageAtFit(0, 0, fWidth*m_iRenderWidth, fHeight*m_iRenderHeight, m_Images[indexImage]);
}

float RenderEngineOVG::textWidth(u32 fontId, const char* szText)
{
   int indexFont = -1;
   for( int i=0; i<m_iCountFonts; i++ )
      if ( m_FontIds[i] == fontId )
      {
         indexFont = i;
         break;
      }
   if ( -1 == indexFont )
      return 0.0;

   float fWidth = TextWidth(szText, m_Fonts[indexFont], 1.0*m_iRenderHeight);
   fWidth = fWidth/(float)m_iRenderWidth;
   return fWidth;
}

void RenderEngineOVG::drawText(float xPos, float yPos, u32 fontId, const char* szText)
{
   int indexFont = -1;
   for( int i=0; i<m_iCountFonts; i++ )
      if ( m_FontIds[i] == fontId )
      {
         indexFont = i;
         break;
      }
   if ( -1 == indexFont )
      return;

   Text(xPos*m_iRenderWidth, m_iRenderHeight - yPos*m_iRenderHeight, szText, m_Fonts[indexFont], 1.0*m_iRenderHeight);
}

void RenderEngineOVG::drawTextLeft(float xPos, float yPos, u32 fontId, const char* szText)
{
   int indexFont = -1;
   for( int i=0; i<m_iCountFonts; i++ )
      if ( m_FontIds[i] == fontId )
      {
         indexFont = i;
         break;
      }
   if ( -1 == indexFont )
      return;

   TextEnd(xPos*m_iRenderWidth, m_iRenderHeight - yPos*m_iRenderHeight, szText, m_Fonts[indexFont], 1.0*m_iRenderHeight);
}

float RenderEngineOVG::getMessageHeight(const char* text, float line_spacing_percent, float max_width, u32 fontId)
{
   int indexFont = -1;
   for( int i=0; i<m_iCountFonts; i++ )
      if ( m_FontIds[i] == fontId )
      {
         indexFont = i;
         break;
      }
   if ( -1 == indexFont )
      return 0.0;

   if ( NULL == text || strlen(text) >= 1023 )
      return 0.0;

   float fTextHeight = textHeight(fontId);

   char szText[1024];
   strcpy(szText, text );
   const char* szTokens = " \n";
   char* szContext = szText;
   char* szWord = NULL;
   char* szPrevWord = NULL;
   float height_text = fTextHeight;
   float h = 0, w = 0;
   float line_width = 0;
   float space_width = TextWidth("_", m_Fonts[indexFont], fTextHeight*m_iRenderHeight);
   int countWords = 0;
   int countLineWords = 0;

   while ( (szWord = strtok_r(szContext, szTokens, &szContext)) )
   {
      countWords++;
      countLineWords++;
      w = TextWidth(szWord, m_Fonts[indexFont], fTextHeight*m_iRenderHeight);
      line_width += w;
      if ( countLineWords > 1 )
         line_width += space_width;
      if ( line_width/(float)m_iRenderWidth > max_width-0.01 || ( NULL != szPrevWord && '\n' == szPrevWord[strlen(szPrevWord)-1]) )
      {
         h += height_text*(1.0+line_spacing_percent);
         line_width = w;
         countLineWords = 1;
      }
      szPrevWord = szWord;
   }
   if ( countWords > 0 )
      h += height_text;

   return h;
}

float RenderEngineOVG::drawMessageLines(float xPos, float yPos, const char* text, float line_spacing_percent, float max_width, u32 fontId)
{
   int indexFont = -1;
   for( int i=0; i<m_iCountFonts; i++ )
      if ( m_FontIds[i] == fontId )
      {
         indexFont = i;
         break;
      }
   if ( -1 == indexFont )
      return 0.0;

   if ( NULL == text || strlen(text) >= 1023 )
      return 0.0;

   float fTextHeight = textHeight(fontId);

   char szText[1024];
   strcpy(szText, text );
   const char* szTokens = " \n";
   char* szContext = szText;
   char* szWord = NULL;
   char* szPrevWord = NULL;

   float height_text = fTextHeight;
   float wWord = 0;
   float h = 0;

   float line_width = 0;
   float space_width = TextWidth("_", m_Fonts[indexFont], fTextHeight*m_iRenderHeight);

   int countWords = 0;
   int countLineWords = 0;

   while ( (szWord = strtok_r(szContext, szTokens, &szContext)) )
   {
      countWords++;
      countLineWords++;
      wWord = TextWidth(szWord, m_Fonts[indexFont], fTextHeight*m_iRenderHeight);
      line_width += wWord;
      if ( countLineWords > 1 )
         line_width += space_width;
      if ( line_width/(float)m_iRenderWidth > max_width-0.01 || ( NULL != szPrevWord && '\n' == szPrevWord[strlen(szPrevWord)-1]) )
      {
         h += height_text*(1+line_spacing_percent);
         line_width = wWord;
         countLineWords = 1;
      }

      Text(xPos*m_iRenderWidth + line_width-wWord, m_iRenderHeight - (yPos+h+height_text)*m_iRenderHeight, szWord, m_Fonts[indexFont], fTextHeight*m_iRenderHeight);

      szPrevWord = szWord;
   }

   if ( countWords > 0 )
      h += height_text;

   return h;
}


void RenderEngineOVG::drawLine(float x1, float y1, float x2, float y2)
{
   Line(x1*m_iRenderWidth, m_iRenderHeight - y1*m_iRenderHeight, x2*m_iRenderWidth, m_iRenderHeight - y2*m_iRenderHeight);
}

void RenderEngineOVG::drawRect(float xPos, float yPos, float fWidth, float fHeight)
{
   Rect(xPos*m_iRenderWidth, m_iRenderHeight - yPos*m_iRenderHeight - fHeight*m_iRenderHeight, m_iRenderWidth*fWidth, m_iRenderHeight*fHeight);
}

void RenderEngineOVG::drawRoundRect(float xPos, float yPos, float fWidth, float fHeight, float fCornerRadius)
{
   Roundrect(xPos*m_iRenderWidth, m_iRenderHeight - yPos*m_iRenderHeight - fHeight*m_iRenderHeight, m_iRenderWidth*fWidth, m_iRenderHeight*fHeight, fCornerRadius*m_iRenderHeight, fCornerRadius*m_iRenderHeight);
}

void RenderEngineOVG::drawTriangle(float x1, float y1, float x2, float y2, float x3, float y3)
{
   float x[3];
   float y[3];
   x[0] = x1*m_iRenderWidth; x[1] = x2*m_iRenderWidth; x[2] = x3*m_iRenderWidth;
   y[0] = m_iRenderHeight-y1*m_iRenderHeight; y[1] = m_iRenderHeight-y2*m_iRenderHeight; y[2] = m_iRenderHeight-y3*m_iRenderHeight;
   Polygon(x, y, 3);
}

void RenderEngineOVG::drawPolyLine(float* x, float* y, int count)
{
   float xt[100];
   float yt[100];
   for( int i=0; i<count; i++ )
   {
      xt[i] = x[i]*m_iRenderWidth;
      yt[i] = m_iRenderHeight - y[i]*m_iRenderHeight;
   }
   Polyline(xt,yt,count);
}

void RenderEngineOVG::fillPolygon(float* x, float* y, int count)
{
   float xt[100];
   float yt[100];
   for( int i=0; i<count; i++ )
   {
      xt[i] = x[i]*m_iRenderWidth;
      yt[i] = m_iRenderHeight - y[i]*m_iRenderHeight;
   }
   Polygon(xt,yt,count);
}
