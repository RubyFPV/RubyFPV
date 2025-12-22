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

#include "render_engine_raw.h"
#include "fbg_dispmanx.h"
#include "fbgraphics.h"
#include <math.h>

RenderEngineRaw::RenderEngineRaw()
:RenderEngine()
{
   log_line("RendererRAW: Init started.");

   m_pFBG = fbg_dispmanxSetup(0, VC_IMAGE_RGBA32);

   m_iRenderWidth = m_pFBG->width;
   m_iRenderHeight = m_pFBG->height;
   log_line("Initialized graphics to resolution: %d x %d, line width: %d bytes, components: %d, frame buffer size: %d bytes", m_iRenderWidth, m_iRenderHeight, m_pFBG->line_length, m_pFBG->components, m_pFBG->size);
   m_fPixelWidth = 1.0/(float)m_iRenderWidth;
   m_fPixelHeight = 1.0/(float)m_iRenderHeight;

   m_iCountImages = 0;
   m_iCountIcons = 0;
   m_CurrentImageId = 1;
   m_CurrentIconId = 1;

   log_line("RendererRAW: Render init done.");
}


RenderEngineRaw::~RenderEngineRaw()
{
   log_line("Free graphics engine resources.");
   if ( NULL != m_pFBG )
   {
      log_line("Free graphics engine instance.");
      fbg_close(m_pFBG);
   }
}

void* RenderEngineRaw::_loadRawFontImageObject(const char* szFileName)
{
   struct _fbg_img* pImage = fbg_loadPNG(m_pFBG, szFileName);
   return (void*) pImage;
}

void RenderEngineRaw::_freeRawFontImageObject(void* pImageObject)
{
   if ( NULL == pImageObject )
      return;
   fbg_freeImage((struct _fbg_img*)pImageObject);
}


void RenderEngineRaw::setFontOutlineColor(u32 idFont, u8 r, u8 g, u8 b, u8 a)
{
   int indexFont = _getRawFontIndexFromId(idFont);
   if ( -1 == indexFont )
   {
      log_softerror_and_alarm("Tried to update invalid raw font id %u, not in the list (%d raw fonts)", idFont, m_iCountRawFonts);
      return;
   }

   if ( (r<30) && (g<30) && (b<30) )
      return;
   RenderEngineRawFont* pFont = m_pRawFonts[indexFont];
   struct _fbg_img* pImg = (struct _fbg_img*) pFont->pImageObject;
   unsigned char *img_data_pointer_row = (unsigned char *)(pImg->data);
   u8 r0,g0,b0,a0;

   for( int y=0; y<(int)pImg->height; y++ )
   {
      unsigned char *img_data_pointer = img_data_pointer_row;
      for( int x=0; x<(int)pImg->width; x++ )
      {
         r0 = *img_data_pointer;
         g0 = *(img_data_pointer+1);
         b0 = *(img_data_pointer+2);
         a0 = *(img_data_pointer+3);
         if ( (r0 < 50) && (g0 < 50) && (b0 < 50) && (a0 > 0) )
         {
            *img_data_pointer = r;
            *(img_data_pointer+1) = g;
            *(img_data_pointer+2) = b;
            *(img_data_pointer+3) = a0;
         }
         img_data_pointer += fbg_getcomponents(m_pFBG);
      }
      img_data_pointer_row += fbg_getcomponents(m_pFBG) * pImg->width;
   }
}

u32 RenderEngineRaw::loadImage(const char* szFile)
{
   if ( m_iCountImages > MAX_RAW_IMAGES )
      return 0;

   if ( access( szFile, R_OK ) == -1 )
      return 0;

   if ( NULL != strstr(szFile, ".png") )
   {
      m_pImages[m_iCountImages] = fbg_loadPNG(m_pFBG, szFile);
   }
   else
   {
      m_pImages[m_iCountImages] = fbg_loadJPEG(m_pFBG, szFile);
   }
   if ( NULL != m_pImages[m_iCountImages] )
      log_line("Loaded image %s, id: %u", szFile, m_CurrentImageId+1);
   else
      log_softerror_and_alarm("Failed to load image %s", szFile);

   m_CurrentImageId++;
   m_ImageIds[m_iCountImages] = m_CurrentImageId;
   m_iCountImages++;
   return m_CurrentImageId;
}

void RenderEngineRaw::freeImage(u32 idImage)
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

   fbg_freeImage(m_pImages[indexImage]);

   for( int i=indexImage; i<m_iCountImages-1; i++ )
   {
      m_pImages[i] = m_pImages[i+1];
      m_ImageIds[i] = m_ImageIds[i+1];
   }
   m_iCountImages--;
}


u32 RenderEngineRaw::loadIcon(const char* szFile)
{
   if ( m_iCountIcons > MAX_RAW_ICONS )
      return 0;

   if ( access( szFile, R_OK ) == -1 )
      return 0;

   if ( NULL != strstr(szFile, ".png") )
   {
      m_pIcons[m_iCountIcons] = fbg_loadPNG(m_pFBG, szFile);
      m_pIconsMip[m_iCountIcons][0] = fbg_loadPNG(m_pFBG, szFile);
      m_pIconsMip[m_iCountIcons][1] = fbg_loadPNG(m_pFBG, szFile);
   }
   else
   {
      m_pIcons[m_iCountIcons] = fbg_loadJPEG(m_pFBG, szFile);
      m_pIconsMip[m_iCountIcons][0] = fbg_loadJPEG(m_pFBG, szFile);
      m_pIconsMip[m_iCountIcons][1] = fbg_loadJPEG(m_pFBG, szFile);
   }
   if ( NULL != m_pIcons[m_iCountIcons] )
      log_line("Loaded icon %s, id: %u", szFile, m_CurrentIconId+1);
   else
      log_softerror_and_alarm("Failed to load icon %s", szFile);

   _buildMipImage(m_pIcons[m_iCountIcons], m_pIconsMip[m_iCountIcons][0]);
   _buildMipImage(m_pIconsMip[m_iCountIcons][0], m_pIconsMip[m_iCountIcons][1]);

   m_CurrentIconId++;
   m_IconIds[m_iCountIcons] = m_CurrentIconId;
   m_iCountIcons++;
   return m_CurrentIconId;
}

void RenderEngineRaw::freeIcon(u32 idIcon)
{
   int indexIcon = -1;
   for( int i=0; i<m_iCountIcons; i++ )
      if ( m_IconIds[i] == idIcon )
      {
         indexIcon = i;
         break;
      }
   if ( -1 == indexIcon )
      return;

   fbg_freeImage(m_pIcons[indexIcon]);
   fbg_freeImage(m_pIconsMip[indexIcon][0]);
   fbg_freeImage(m_pIconsMip[indexIcon][1]);

   for( int i=indexIcon; i<m_iCountIcons-1; i++ )
   {
      m_pIcons[i] = m_pIcons[i+1];
      m_pIconsMip[i][0] = m_pIconsMip[i+1][0];
      m_pIconsMip[i][1] = m_pIconsMip[i+1][1];
      m_IconIds[i] = m_IconIds[i+1];
   }
   m_iCountIcons--;
}

int RenderEngineRaw::getImageWidth(u32 uImageId)
{
   if ( uImageId < 1 )
      return 0;

   int indexImage = -1;
   for( int i=0; i<m_iCountImages; i++ )
   {
      if ( m_ImageIds[i] == uImageId )
      {
         indexImage = i;
         break;
      }
   }
   if ( -1 == indexImage )
      return 0;

   
   return m_pImages[indexImage]->width;
}

int RenderEngineRaw::getImageHeight(u32 uImageId)
{
   if ( uImageId < 1 )
      return 0;

   int indexImage = -1;
   for( int i=0; i<m_iCountImages; i++ )
   {
      if ( m_ImageIds[i] == uImageId )
      {
         indexImage = i;
         break;
      }
   }
   if ( -1 == indexImage )
      return 0;

   
   return m_pImages[indexImage]->height;
}

void RenderEngineRaw::changeImageHue(u32 uImageId, u8 r, u8 g, u8 b)
{
   if ( uImageId < 1 )
      return;

   int indexImage = -1;
   for( int i=0; i<m_iCountImages; i++ )
   {
      if ( m_ImageIds[i] == uImageId )
      {
         indexImage = i;
         break;
      }
   }
   if ( -1 == indexImage )
      return;

   fbg_imageChangeHue(m_pFBG, m_pImages[indexImage], r, g, b);
}

void RenderEngineRaw::_buildMipImage(struct _fbg_img* pSrc, struct _fbg_img* pDest)
{
   pDest->width = pSrc->width/2;
   pDest->height = pSrc->height/2;
   u32 r[4];
   u32 g[4];
   u32 b[4];
   u32 a[4];
   u32 res;
   for( unsigned int y=0; y<pDest->height; y++ )
   for( unsigned int x=0; x<pDest->width; x++ )
   {
      unsigned char* pixelDest = pDest->data + x*4 + y * pDest->width * 4;
      unsigned char* pixelSrc = pSrc->data + x*2*4 + y*2 * pSrc->width * 4;
      r[0] = *pixelSrc;
      pixelSrc++;
      g[0] = *pixelSrc;
      pixelSrc++;
      b[0] = *pixelSrc;
      pixelSrc++;
      a[0] = *pixelSrc;
      pixelSrc++;

      r[1] = *pixelSrc;
      pixelSrc++;
      g[1] = *pixelSrc;
      pixelSrc++;
      b[1] = *pixelSrc;
      pixelSrc++;
      a[1] = *pixelSrc;
      pixelSrc++;

      pixelSrc = pSrc->data + x*2*4 + (y*2+1) * pSrc->width * 4;
      r[2] = *pixelSrc;
      pixelSrc++;
      g[2] = *pixelSrc;
      pixelSrc++;
      b[2] = *pixelSrc;
      pixelSrc++;
      a[2] = *pixelSrc;
      pixelSrc++;

      r[3] = *pixelSrc;
      pixelSrc++;
      g[3] = *pixelSrc;
      pixelSrc++;
      b[3] = *pixelSrc;
      pixelSrc++;
      a[3] = *pixelSrc;
      pixelSrc++;
    
      res = sqrtl((r[0]*r[0] + r[1]*r[1] + r[2]*r[2] + r[3]*r[3])/4);
      if ( res < 0 ) res = 0;
      if ( res > 255 ) res = 255;
      *pixelDest = res;
      pixelDest++;

      res = sqrtl((g[0]*g[0] + g[1]*g[1] + g[2]*g[2] + g[3]*g[3])/4);
      if ( res < 0 ) res = 0;
      if ( res > 255 ) res = 255;
      *pixelDest = res;
      pixelDest++;

      res = sqrtl((b[0]*b[0] + b[1]*b[1] + b[2]*b[2] + b[3]*b[3])/4);
      if ( res < 0 ) res = 0;
      if ( res > 255 ) res = 255;
      *pixelDest = res;
      pixelDest++;

      res = sqrtl((a[0]*a[0] + a[1]*a[1] + a[2]*a[2] + a[3]*a[3])/4);
      if ( res < 0 ) res = 0;
      if ( res > 255 ) res = 255;
      *pixelDest = res;
      pixelDest++;
   }

}

void RenderEngineRaw::startFrame()
{
   RenderEngine::startFrame();
   fbg_clear(m_pFBG, m_uClearBufferByte);
}

void RenderEngineRaw::endFrame()
{
   fbg_draw(m_pFBG);
   fbg_flip(m_pFBG);
   RenderEngine::endFrame();
}

void RenderEngineRaw::rotate180()
{
   unsigned char pixel[4];

   for( int y=0; y<m_pFBG->height/2; y++ )
   for( int x=0; x<m_pFBG->width; x++ )
   {
       unsigned char *scr_pointer1 = (unsigned char *)(m_pFBG->back_buffer + (y * m_pFBG->line_length + x * m_pFBG->components));
       unsigned char *scr_pointer2 = (unsigned char *)(m_pFBG->back_buffer + ((m_pFBG->height-y-1) * m_pFBG->line_length + (m_pFBG->width-x-1) * m_pFBG->components));
       memcpy(pixel, scr_pointer1, 4);
       memcpy(scr_pointer1, scr_pointer2, 4);
       memcpy(scr_pointer2, pixel, 4);
   }
}

void RenderEngineRaw::drawImage(float xPos, float yPos, float fWidth, float fHeight, u32 imageId)
{
   if ( imageId < 1 )
      return;

   int indexImage = -1;
   for( int i=0; i<m_iCountImages; i++ )
      if ( m_ImageIds[i] == imageId )
      {
         indexImage = i;
         break;
      }
   if ( -1 == indexImage )
      return;
   int x = xPos*m_iRenderWidth;
   int y = yPos*m_iRenderHeight;
   int w = fWidth*m_iRenderWidth;
   int h = fHeight*m_iRenderHeight;

   if ( x < 0 || y < 0 || x+w > m_iRenderWidth || y+h > m_iRenderHeight )
      return;

   m_pFBG->mix_color.r = 255;
   m_pFBG->mix_color.g = 255;
   m_pFBG->mix_color.b = 255;
   m_pFBG->mix_color.a = 255;

   fbg_imageDraw(m_pFBG, m_pImages[indexImage], x,y,w,h, 0, 0, m_pImages[indexImage]->width, m_pImages[indexImage]->height);
}

void RenderEngineRaw::drawImageAlpha(float xPos, float yPos, float fWidth, float fHeight, u32 imageId, u8 uAlpha)
{
   if ( imageId < 1 )
      return;

   int indexImage = -1;
   for( int i=0; i<m_iCountImages; i++ )
      if ( m_ImageIds[i] == imageId )
      {
         indexImage = i;
         break;
      }
   if ( -1 == indexImage )
      return;
   int x = xPos*m_iRenderWidth;
   int y = yPos*m_iRenderHeight;
   int w = fWidth*m_iRenderWidth;
   int h = fHeight*m_iRenderHeight;

   if ( x < 0 || y < 0 || x+w > m_iRenderWidth || y+h > m_iRenderHeight )
      return;

   m_pFBG->mix_color.r = 255;
   m_pFBG->mix_color.g = 255;
   m_pFBG->mix_color.b = 255;
   m_pFBG->mix_color.a = uAlpha;

   fbg_imageDrawAlpha(m_pFBG, m_pImages[indexImage], x,y,w,h, 0, 0, m_pImages[indexImage]->width, m_pImages[indexImage]->height);
}

void RenderEngineRaw::bltImage(float xPosDest, float yPosDest, float fWidthDest, float fHeightDest, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, u32 uImageId)
{
   if ( uImageId < 1 )
      return;

   int indexImage = -1;
   for( int i=0; i<m_iCountImages; i++ )
   {
      if ( m_ImageIds[i] == uImageId )
      {
         indexImage = i;
         break;
      }
   }
   if ( -1 == indexImage )
      return;
   if ( NULL == m_pImages[indexImage] )
      return;
  
   int xDest = xPosDest*m_iRenderWidth;
   int yDest = yPosDest*m_iRenderHeight;
   int wDest = fWidthDest*m_iRenderWidth;
   int hDest = fHeightDest*m_iRenderHeight;

   if ( (xDest < 0) || (yDest < 0) || (xDest+wDest >= m_iRenderWidth) || (yDest+hDest >= m_iRenderHeight) )
      return;

   m_pFBG->mix_color.r = 255;
   m_pFBG->mix_color.g = 255;
   m_pFBG->mix_color.b = 255;
   m_pFBG->mix_color.a = 255;

   fbg_imageDraw(m_pFBG, m_pImages[indexImage], xDest,yDest,wDest,hDest, iSrcX, iSrcY, iSrcWidth, iSrcHeight);
  
}

void RenderEngineRaw::bltSprite(float xPosDest, float yPosDest, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, u32 uImageId)
{
   if ( uImageId < 1 )
      return;

   int indexImage = -1;
   for( int i=0; i<m_iCountImages; i++ )
   {
      if ( m_ImageIds[i] == uImageId )
      {
         indexImage = i;
         break;
      }
   }
   if ( -1 == indexImage )
      return;
   if ( NULL == m_pImages[indexImage] )
      return;
  
   int xDest = xPosDest*m_iRenderWidth;
   int yDest = yPosDest*m_iRenderHeight;
   int wDest = iSrcWidth;
   int hDest = iSrcHeight;

   if ( (xDest < 0) || (yDest < 0) || (xDest+wDest >= m_iRenderWidth) || (yDest+hDest >= m_iRenderHeight) )
      return;

   m_pFBG->mix_color.r = 255;
   m_pFBG->mix_color.g = 255;
   m_pFBG->mix_color.b = 255;
   m_pFBG->mix_color.a = 255;
   fbg_imageDraw(m_pFBG, m_pImages[indexImage], xDest,yDest,wDest,hDest, iSrcX, iSrcY, iSrcWidth, iSrcHeight);
}

     
void RenderEngineRaw::drawIcon(float xPos, float yPos, float fWidth, float fHeight, u32 iconId)
{
   if ( iconId < 1 )
      return;

   int indexIcon = -1;
   for( int i=0; i<m_iCountIcons; i++ )
      if ( m_IconIds[i] == iconId )
      {
         indexIcon = i;
         break;
      }
   if ( -1 == indexIcon )
      return;

   int x = xPos*m_iRenderWidth;
   int y = yPos*m_iRenderHeight;
   int w = fWidth*m_iRenderWidth;
   int h = fHeight*m_iRenderHeight;

   if ( x < 0 || y < 0 || x+w >= m_iRenderWidth || y+h >= m_iRenderHeight )
      return;

   fbg_enable_alpha(m_pFBG, m_bEnableAlphaBlending?1:0);
   m_pFBG->mix_color.r = m_ColorFill[0];
   m_pFBG->mix_color.g = m_ColorFill[1];
   m_pFBG->mix_color.b = m_ColorFill[2];
   m_pFBG->mix_color.a = m_ColorFill[3];

   if ( fWidth*m_iRenderWidth <= m_pIcons[indexIcon]->width/4 ||
        fHeight*m_iRenderHeight <= m_pIcons[indexIcon]->height/4 ) 
      fbg_imageDrawAlpha(m_pFBG, m_pIconsMip[indexIcon][1], x,y, fWidth*m_iRenderWidth, fHeight*m_iRenderHeight, 0,0, m_pIconsMip[indexIcon][1]->width, m_pIconsMip[indexIcon][1]->height);
   else if ( fWidth*m_iRenderWidth <= m_pIcons[indexIcon]->width/2 ||
        fHeight*m_iRenderHeight <= m_pIcons[indexIcon]->height/2 ) 
      fbg_imageDrawAlpha(m_pFBG, m_pIconsMip[indexIcon][0], x,y, fWidth*m_iRenderWidth, fHeight*m_iRenderHeight, 0,0, m_pIconsMip[indexIcon][0]->width, m_pIconsMip[indexIcon][0]->height);
   else
      fbg_imageDrawAlpha(m_pFBG, m_pIcons[indexIcon], x,y, fWidth*m_iRenderWidth, fHeight*m_iRenderHeight, 0,0, m_pIcons[indexIcon]->width, m_pIcons[indexIcon]->height);
}


void RenderEngineRaw::bltIcon(float xPosDest, float yPosDest, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, u32 iconId)
{

}

void RenderEngineRaw::_drawSimpleText(RenderEngineRawFont* pFont, const char* szText, float xPos, float yPos)
{
   if ( NULL == pFont || NULL == szText || 0 == szText[0] )
      return;

   if ( yPos < 0 )
      return;
   if ( xPos >= 1.0 )
      return;
   if ( yPos + pFont->lineHeight * m_fPixelHeight >= 1.0 )
      return;

   fbg_enable_alpha(m_pFBG, m_bEnableAlphaBlending?1:0);
   m_pFBG->mix_color.r = m_uTextFontMixColor[0];
   m_pFBG->mix_color.g = m_uTextFontMixColor[1];
   m_pFBG->mix_color.b = m_uTextFontMixColor[2];
   m_pFBG->mix_color.a = m_uTextFontMixColor[3];

   int tmp = m_pFBG->disableFontOutline;
   m_pFBG->disableFontOutline = m_bDisableTextOutline?1:0;

   if ( m_bDrawBackgroundBoundingBoxes )
   {
      _drawSimpleTextBoundingBox(pFont, szText, xPos, yPos, 1.0);
      if ( m_bDrawBackgroundBoundingBoxesTextUsesSameStrokeColor )
      {
         m_pFBG->mix_color.r = m_ColorTextBackgroundBoundingBoxStrike[0];
         m_pFBG->mix_color.g = m_ColorTextBackgroundBoundingBoxStrike[1];
         m_pFBG->mix_color.b = m_ColorTextBackgroundBoundingBoxStrike[2];
         m_pFBG->mix_color.a = m_ColorTextBackgroundBoundingBoxStrike[3];
      }
   }

   float xTmp = xPos;
   while ( *szText )
   {
      float fWidthCh = _get_raw_char_width(pFont, *szText);
      if ( (fWidthCh < 0.0001) || ( (*szText) < pFont->charIdFirst || (*szText) > pFont->charIdLast ) )
      {
         szText++;
         continue;
      }
      if ( xTmp < 0 )
      {
         xTmp += fWidthCh;
         szText++;
         continue;
      }
      if ( xTmp + fWidthCh >= 1.0 )
         break;
      
      int xImg = pFont->chars[(*szText)-pFont->charIdFirst].imgXOffset;
      int yImg = pFont->chars[(*szText)-pFont->charIdFirst].imgYOffset;
      int wImg = pFont->chars[(*szText)-pFont->charIdFirst].width;
      int hImg = pFont->chars[(*szText)-pFont->charIdFirst].height;
      //unsigned char *img_pointer = (unsigned char *)(pFont->pImage->data + (yImg * pFont->pImage->width * m_pFBG->components + xImg * m_pFBG->components));

      if ( (*szText) != ' ' )
         fbg_imageClipAColor(m_pFBG, (struct _fbg_img*) pFont->pImageObject, xTmp*m_iRenderWidth, yPos*m_iRenderHeight, xImg, yImg, wImg, hImg);

      xTmp += fWidthCh;
      szText++;
   }

   m_pFBG->disableFontOutline = tmp;
}

void RenderEngineRaw::_drawSimpleTextScaled(RenderEngineRawFont* pFont, const char* szText, float xPos, float yPos, float fScale)
{
   if ( (NULL == pFont) || (NULL == szText) || (0 == szText[0]) )
      return;

   if ( yPos < 0 )
      return;
   if ( yPos + pFont->lineHeight * fScale * m_fPixelHeight >= 1.0 )
      return;

   fbg_enable_alpha(m_pFBG, m_bEnableAlphaBlending?1:0);
   m_pFBG->mix_color.r = m_uTextFontMixColor[0];
   m_pFBG->mix_color.g = m_uTextFontMixColor[1];
   m_pFBG->mix_color.b = m_uTextFontMixColor[2];
   m_pFBG->mix_color.a = m_uTextFontMixColor[3];

   while ( *szText )
   {
      float fWidthCh = _get_raw_char_width(pFont, *szText);
      if ( (fWidthCh < 0.0001) || ( (*szText) < pFont->charIdFirst || (*szText) > pFont->charIdLast ) )
      {
         szText++;
         continue;
      }
      if ( xPos < 0 )
      {
         xPos += fWidthCh;
         szText++;
         continue;
      }
      if ( xPos + fWidthCh * fScale >= 1.0 )
         break;

      int xImg = pFont->chars[(*szText)-pFont->charIdFirst].imgXOffset;
      int yImg = pFont->chars[(*szText)-pFont->charIdFirst].imgYOffset;
      int wImg = pFont->chars[(*szText)-pFont->charIdFirst].width;
      int hImg = pFont->chars[(*szText)-pFont->charIdFirst].height;
      //unsigned char *img_pointer = (unsigned char *)(pFont->pImage->data + (yImg * pFont->pImage->width * m_pFBG->components + xImg * m_pFBG->components));

      if ( m_bEnableAlphaBlending )
         fbg_imageDrawAlpha(m_pFBG, (struct _fbg_img*) pFont->pImageObject, xPos * m_iRenderWidth, yPos * m_iRenderHeight, wImg*fScale, hImg*fScale, xImg, yImg, wImg, hImg);
      else
         fbg_imageDrawAlphaMask(m_pFBG, (struct _fbg_img*) pFont->pImageObject, xPos * m_iRenderWidth, yPos * m_iRenderHeight, wImg*fScale, hImg*fScale, xImg, yImg, wImg, hImg);
      xPos += fWidthCh;
      szText++;
   }
}


void RenderEngineRaw::drawLine(float x1, float y1, float x2, float y2)
{
   fbg_enable_alpha(m_pFBG, m_bEnableAlphaBlending?1:0);
   // Clip horizontal or vertical lines
   if ( fabs(x2-x1) < 0.0001 )
   {
      if ( y1 < 0 )
         y1 = 0.0001;
      if ( y2 < 0 )
         y2 = 0.0001;
      if ( y1 > 1.0 - m_fPixelHeight )
         y1 = 1.0 - m_fPixelHeight;
      if ( y2 > 1.0 - m_fPixelHeight )
         y2 = 1.0 - m_fPixelHeight;

      if ( fabs (y2-y1) < 0.0001 )
         return;
   }
   if ( fabs(y2-y1) < 0.0001 )
   {
      if ( x1 < 0 )
         x1 = 0.0001;
      if ( x2 < 0 )
         x2 = 0.0001;
      if ( x1 > 1.0 - m_fPixelWidth)
         x1 = 1.0 - m_fPixelWidth;
      if ( x2 > 1.0 - m_fPixelWidth)
         x2 = 1.0 - m_fPixelWidth;
      if ( fabs(x2-x1) < 0.0001 )
         return;
   }

   if ( x1 < 0 || x2 < 0 )
      return;
   if ( y1 < 0 || y2 < 0 )
      return;
   if ( x1 >= 1.0 || x2 >= 1.0 )
      return;
   if ( y1 >= 1.0 || y2 >= 1.0 )
      return;

   if ( x1 > 1.0 - m_fPixelWidth )
      x1 = 1.0 - m_fPixelWidth;
   if ( x2 > 1.0 - m_fPixelWidth )
      x2 = 1.0 - m_fPixelWidth;

   if ( y1 > 1.0 - m_fPixelHeight )
      y1 = 1.0 - m_fPixelHeight;
   if ( y2 > 1.0 - m_fPixelHeight )
      y2 = 1.0 - m_fPixelHeight;

   if ( (x2-x1)*(x2-x1) + (y2-y1)*(y2-y1) < 0.5 * m_fPixelWidth * m_fPixelWidth )
      return;

   u8 alfa = m_ColorStroke[3];

   if ( m_fStrokeSizePx < 1.5 )
   {
      fbg_line(m_pFBG, x1*m_iRenderWidth, y1*m_iRenderHeight, x2*m_iRenderWidth, y2*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], alfa);
      return;
   }

   float xp1 = -(y2-y1);
   float yp1 = (x2-x1);
   float len = sqrtf(xp1*xp1+yp1*yp1);
   if ( fabs(len) > 0.0001 )
   {
   xp1 = xp1 / len;
   yp1 = yp1 / len;
   }
   float xp2 = (y2-y1);
   float yp2 = -(x2-x1);
   len = sqrtf(xp2*xp2+yp2*yp2);
   if ( fabs(len) > 0.0001 )
   {
   xp2 = xp2 / len;
   yp2 = yp2 / len;
   }
   if ( m_fStrokeSizePx < 2.5 )
   {
      xp1 *= 0.5*m_fPixelWidth;
      yp1 *= 0.5*m_fPixelHeight;

      xp2 *= 0.5*m_fPixelWidth;
      yp2 *= 0.5*m_fPixelHeight;

      if ( x1+xp1 >= 0.0 && x1+xp1 <= 1.0 - m_fPixelWidth )
      if ( x2+xp1 >= 0.0 && x2+xp1 <= 1.0 - m_fPixelWidth )
      if ( y1+yp1 >= 0.0 && y1+yp1 <= 1.0 - m_fPixelHeight )
      if ( y2+yp1 >= 0.0 && y2+yp1 <= 1.0 - m_fPixelHeight )
         fbg_line(m_pFBG, (x1+xp1)*m_iRenderWidth, (y1+yp1)*m_iRenderHeight, (x2+xp1)*m_iRenderWidth, (y2+yp1)*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], alfa);

      if ( x1+xp2 >= 0.0 && x1+xp2 <= 1.0 - m_fPixelWidth )
      if ( x2+xp2 >= 0.0 && x2+xp2 <= 1.0 - m_fPixelWidth )
      if ( y1+yp2 >= 0.0 && y1+yp2 <= 1.0 - m_fPixelHeight )
      if ( y2+yp2 >= 0.0 && y2+yp2 <= 1.0 - m_fPixelHeight )
         fbg_line(m_pFBG, (x1+xp2)*m_iRenderWidth, (y1+yp2)*m_iRenderHeight, (x2+xp2)*m_iRenderWidth, (y2+yp2)*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], alfa);
   }

   if ( m_fStrokeSizePx < 3.5 )
   {
      fbg_line(m_pFBG, x1*m_iRenderWidth, y1*m_iRenderHeight, x2*m_iRenderWidth, y2*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], alfa);

      xp1 *= 0.99*m_fPixelWidth;
      yp1 *= 0.99*m_fPixelHeight;

      xp2 *= 0.99*m_fPixelWidth;
      yp2 *= 0.99*m_fPixelHeight;

      if ( x1+xp1 >= 0.0 && x1+xp1 <= 1.0 - m_fPixelWidth )
      if ( x2+xp1 >= 0.0 && x2+xp1 <= 1.0 - m_fPixelWidth )
      if ( y1+yp1 >= 0.0 && y1+yp1 <= 1.0 - m_fPixelHeight )
      if ( y2+yp1 >= 0.0 && y2+yp1 <= 1.0 - m_fPixelHeight )
         fbg_line(m_pFBG, (x1+xp1)*m_iRenderWidth, (y1+yp1)*m_iRenderHeight, (x2+xp1)*m_iRenderWidth, (y2+yp1)*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], alfa);

      if ( x1+xp2 >= 0.0 && x1+xp2 <= 1.0 - m_fPixelWidth )
      if ( x2+xp2 >= 0.0 && x2+xp2 <= 1.0 - m_fPixelWidth )
      if ( y1+yp2 >= 0.0 && y1+yp2 <= 1.0 - m_fPixelHeight )
      if ( y2+yp2 >= 0.0 && y2+yp2 <= 1.0 - m_fPixelHeight )
         fbg_line(m_pFBG, (x1+xp2)*m_iRenderWidth, (y1+yp2)*m_iRenderHeight, (x2+xp2)*m_iRenderWidth, (y2+yp2)*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], alfa);
   }

   //if ( m_fStrokeSizePx < 4.5 )
   {
      float xp1b = xp1*1.5*m_fPixelWidth;
      float yp1b = yp1*1.5*m_fPixelHeight;
      float xp2b = xp2*1.5*m_fPixelWidth;
      float yp2b = yp2*1.5*m_fPixelHeight;

      if ( x1+xp1b >= 0.0 && x1+xp1b <= 1.0 - m_fPixelWidth )
      if ( x2+xp1b >= 0.0 && x2+xp1b <= 1.0 - m_fPixelWidth )
      if ( y1+yp1b >= 0.0 && y1+yp1b <= 1.0 - m_fPixelHeight )
      if ( y2+yp1b >= 0.0 && y2+yp1b <= 1.0 - m_fPixelHeight )
         fbg_line(m_pFBG, (x1+xp1b)*m_iRenderWidth, (y1+yp1b)*m_iRenderHeight, (x2+xp1b)*m_iRenderWidth, (y2+yp1b)*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], alfa);

      if ( x1+xp2b >= 0.0 && x1+xp2b <= 1.0 - m_fPixelWidth )
      if ( x2+xp2b >= 0.0 && x2+xp2b <= 1.0 - m_fPixelWidth )
      if ( y1+yp2b >= 0.0 && y1+yp2b <= 1.0 - m_fPixelHeight )
      if ( y2+yp2b >= 0.0 && y2+yp2b <= 1.0 - m_fPixelHeight )
         fbg_line(m_pFBG, (x1+xp2b)*m_iRenderWidth, (y1+yp2b)*m_iRenderHeight, (x2+xp2b)*m_iRenderWidth, (y2+yp2b)*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], alfa);

      xp1 *= 0.5*m_fPixelWidth;
      yp1 *= 0.5*m_fPixelHeight;
      xp2 *= 0.5*m_fPixelWidth;
      yp2 *= 0.5*m_fPixelHeight;

      if ( x1+xp1 >= 0.0 && x1+xp1 <= 1.0 - m_fPixelWidth )
      if ( x2+xp1 >= 0.0 && x2+xp1 <= 1.0 - m_fPixelWidth )
      if ( y1+yp1 >= 0.0 && y1+yp1 <= 1.0 - m_fPixelHeight )
      if ( y2+yp1 >= 0.0 && y2+yp1 <= 1.0 - m_fPixelHeight )
         fbg_line(m_pFBG, (x1+xp1)*m_iRenderWidth, (y1+yp1)*m_iRenderHeight, (x2+xp1)*m_iRenderWidth, (y2+yp1)*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], alfa);

      if ( x1+xp2 >= 0.0 && x1+xp2 <= 1.0 - m_fPixelWidth )
      if ( x2+xp2 >= 0.0 && x2+xp2 <= 1.0 - m_fPixelWidth )
      if ( y1+yp2 >= 0.0 && y1+yp2 <= 1.0 - m_fPixelHeight )
      if ( y2+yp2 >= 0.0 && y2+yp2 <= 1.0 - m_fPixelHeight )
         fbg_line(m_pFBG, (x1+xp2)*m_iRenderWidth, (y1+yp2)*m_iRenderHeight, (x2+xp2)*m_iRenderWidth, (y2+yp2)*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], alfa);
   }
}

void RenderEngineRaw::drawRect(float xPos, float yPos, float fWidth, float fHeight)
{
   int x = xPos*m_iRenderWidth;
   int y = yPos*m_iRenderHeight;
   int w = fWidth*m_iRenderWidth;
   int h = fHeight*m_iRenderHeight;

   fbg_enable_alpha(m_pFBG, m_bEnableAlphaBlending?1:0);

   if ( x >= m_iRenderWidth || y >= m_iRenderHeight)
      return;

   if ( x+w <= 0 || y+h <= 0 )
      return;

   if ( x < 0 )
   {
      w += x;
      x = 0;
   }
   if ( y < 0 )
   {
      h += y;
      y = 0;
   }
   if ( x+w >= m_iRenderWidth )
      w = m_iRenderWidth-x-1;
   if ( y+h >= m_iRenderHeight )
      h = m_iRenderHeight-y-1;

   if ( w <= 0 || h <= 0 )
      return;

   if ( 0 != m_ColorFill[3] )
   {
      if ( m_bEnableAlphaBlending )
         fbg_recta(m_pFBG, x,y, w,h, m_ColorFill[0], m_ColorFill[1], m_ColorFill[2], m_ColorFill[3]);
      else
         fbg_rect(m_pFBG, x,y, w,h, m_ColorFill[0], m_ColorFill[1], m_ColorFill[2], m_ColorFill[3]);
   }
   if ( (m_ColorStroke[0] != m_ColorFill[0]) ||
        (m_ColorStroke[1] != m_ColorFill[1]) ||
        (m_ColorStroke[2] != m_ColorFill[2]) ||
        (m_ColorStroke[3] != m_ColorFill[3]) )
   if ( (m_ColorStroke[3] > 2) && (m_fStrokeSizePx >= 0.9) )
   {
      fbg_hline(m_pFBG, x,y,w , m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      fbg_hline(m_pFBG, x,y+h-1,w , m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      fbg_vline(m_pFBG, x,y,h , m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      fbg_vline(m_pFBG, x+w-1,y,h , m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      if ( m_fStrokeSizePx >= 1.9 )
      {
         if ( x > 0 && y > 0 && x+w+1 < m_iRenderWidth )
            fbg_hline(m_pFBG, x-1,y-1,w+2 , m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
         if ( x > 0 && y+h < m_iRenderHeight && x+w+1 < m_iRenderWidth )
            fbg_hline(m_pFBG, x-1,y+h,w+2 , m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
         if ( x > 0 && y > 0 && y+h < m_iRenderHeight )
            fbg_vline(m_pFBG, x-1,y,h , m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
         if ( x+w<m_iRenderWidth && y > 0 && y+h < m_iRenderHeight )
            fbg_vline(m_pFBG, x+w,y,h , m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      }
   }
}

void RenderEngineRaw::drawRoundRect(float xPos, float yPos, float fWidth, float fHeight, float fCornerRadius)
{
   int x = xPos*m_iRenderWidth;
   int y = yPos*m_iRenderHeight;
   int w = fWidth*m_iRenderWidth;
   int h = fHeight*m_iRenderHeight;

   fbg_enable_alpha(m_pFBG, m_bEnableAlphaBlending?1:0);

   if ( (x >= m_iRenderWidth) || (y >= m_iRenderHeight) )
      return;

   if ( (x+w <= 0) || (y+h <= 0) )
      return;

   if ( x < 0 )
   {
      w += x;
      x = 0;
   }
   if ( y < 0 )
   {
      h += y;
      y = 0;
   }

   if ( x+w >= m_iRenderWidth )
      w = m_iRenderWidth-x-1;
   if ( y+h >= m_iRenderHeight )
      h = m_iRenderHeight-y-1;

   if ( (w < 6.0*m_fPixelWidth) || (h < 6.0*m_fPixelHeight) )
      return;

   if ( 0 != m_ColorFill[3] )
   {
      if ( m_bEnableAlphaBlending )
         fbg_recta(m_pFBG, x+3,y, w-5,h, m_ColorFill[0], m_ColorFill[1], m_ColorFill[2], m_ColorFill[3]);
      else
         fbg_rect(m_pFBG, x+3,y, w-5,h, m_ColorFill[0], m_ColorFill[1], m_ColorFill[2], m_ColorFill[3]);
   }

   fbg_vline(m_pFBG, x+2,y+1, h-2, m_ColorFill[0], m_ColorFill[1], m_ColorFill[2], m_ColorFill[3]);
   fbg_vline(m_pFBG, x+1,y+1, h-2 , m_ColorFill[0], m_ColorFill[1], m_ColorFill[2], m_ColorFill[3]);
   fbg_vline(m_pFBG, x,y+3, h-6, m_ColorFill[0], m_ColorFill[1], m_ColorFill[2], m_ColorFill[3]);

   fbg_vline(m_pFBG, x+w-2,y+1, h-2, m_ColorFill[0], m_ColorFill[1], m_ColorFill[2], m_ColorFill[3]);
   fbg_vline(m_pFBG, x+w-1,y+1, h-2, m_ColorFill[0], m_ColorFill[1], m_ColorFill[2], m_ColorFill[3]);
   fbg_vline(m_pFBG, x+w,y+3, h-6, m_ColorFill[0], m_ColorFill[1], m_ColorFill[2], m_ColorFill[3]);

   if ( (m_ColorStroke[0] != m_ColorFill[0]) ||
        (m_ColorStroke[1] != m_ColorFill[1]) ||
        (m_ColorStroke[2] != m_ColorFill[2]) ||
        (m_ColorStroke[3] != m_ColorFill[3]))
   if ( m_ColorStroke[3] > 0 && m_fStrokeSizePx >= 0.9 )
   {
      fbg_hline(m_pFBG, x+3,y,w-6, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      fbg_hline(m_pFBG, x+1,y+1, 2, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      fbg_hline(m_pFBG, x+w-4,y+1,2, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);

      fbg_hline(m_pFBG, x+3,y+h,w-6, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      fbg_hline(m_pFBG, x+1,y+h-1,2, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      fbg_hline(m_pFBG, x+w-4,y+h-1,2, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);

      fbg_vline(m_pFBG, x,y+3, h-6, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      fbg_vline(m_pFBG, x+1,y+1, 2, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      fbg_vline(m_pFBG, x+1,y+h-3, 2, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);

      fbg_vline(m_pFBG, x+w,y+3, h-6, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      fbg_vline(m_pFBG, x+w-1,y+1, 2, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      fbg_vline(m_pFBG, x+w-1,y+h-3, 2, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
   }
}

void RenderEngineRaw::drawRoundRectMenu(float xPos, float yPos, float fWidth, float fHeight, float fCornerRadius)
{
   drawRoundRect(xPos, yPos, fWidth, fHeight, fCornerRadius);
}

void RenderEngineRaw::drawTriangle(float x1, float y1, float x2, float y2, float x3, float y3)
{
   fbg_enable_alpha(m_pFBG, m_bEnableAlphaBlending?1:0);
   drawLine(x1,y1,x2,y2);
   drawLine(x2,y2,x3,y3);
   drawLine(x3,y3,x1,y1);
}


void RenderEngineRaw::fillTriangle(float x1, float y1, float x2, float y2, float x3, float y3)
{
   fbg_enable_alpha(m_pFBG, m_bEnableAlphaBlending?1:0);
   int ix1 = x1 * m_iRenderWidth;
   int ix2 = x2 * m_iRenderWidth;
   int ix3 = x3 * m_iRenderWidth;
   int iy1 = y1 * m_iRenderHeight;
   int iy2 = y2 * m_iRenderHeight;
   int iy3 = y3 * m_iRenderHeight;
   
   int iymin = iy1;
   int iymax = iy1;

   if ( iy2 < iymin ) iymin = iy2;
   if ( iy3 < iymin ) iymin = iy3;

   if ( iy2 > iymax ) iymax = iy2;
   if ( iy3 > iymax ) iymax = iy3;

   int ixmin = ix1;
   int ixmax = ix1;

   if ( ix2 < ixmin ) ixmin = ix2;
   if ( ix3 < ixmin ) ixmin = ix3;

   if ( ix2 > ixmax ) ixmax = ix2;
   if ( ix3 > ixmax ) ixmax = ix3;

   ixmin++;
   ixmax--;
   iymin++;
   iymax--;

   if ( iymin > iymax )
   {
      drawLine(x1,y1,x2,y2);
      drawLine(x2,y2,x3,y3);
      drawLine(x3,y3,x1,y1);
      return;
   }
   
   int ixpt, ixpt1, ixpt2;

   for( int iy=iymin; iy <= iymax; iy++ )
   {
      ixpt1 = -1;
      ixpt2 = -1;
      ixpt = -1;
      if ( (iy >= iy1) && (iy <= iy2) && (iy1 != iy2) )
         ixpt = ix1 + ((ix2-ix1)*(iy-iy1))/(iy2-iy1);
      if ( (iy >= iy2) && (iy <= iy1) && (iy1 != iy2) )
         ixpt = ix2 + ((ix1-ix2)*(iy-iy2))/(iy1-iy2);
      if ( -1 == ixpt1 )
         ixpt1 = ixpt;
      else if ( -1 == ixpt2 )
         ixpt2 = ixpt;

      if ( (iy >= iy2) && (iy <= iy3) && (iy2 != iy3) )
         ixpt = ix2 + ((ix3-ix2)*(iy-iy2))/(iy3-iy2);
      if ( (iy >= iy3) && (iy <= iy2) && (iy2 != iy3) )
         ixpt = ix3 + ((ix2-ix3)*(iy-iy3))/(iy2-iy3);
      if ( -1 == ixpt1 )
         ixpt1 = ixpt;
      else if ( -1 == ixpt2 )
         ixpt2 = ixpt;

      if ( (iy >= iy1) && (iy <= iy3) && (iy1 != iy3) )
         ixpt = ix1 + ((ix3-ix1)*(iy-iy1))/(iy3-iy1);
      if ( (iy >= iy3) && (iy <= iy1) && (iy1 != iy3) )
         ixpt = ix3 + ((ix1-ix3)*(iy-iy3))/(iy1-iy3);
      if ( -1 == ixpt1 )
         ixpt1 = ixpt;
      else if ( -1 == ixpt2 )
         ixpt2 = ixpt;

      if ( (ixpt1 < 0) || (ixpt2 < 0) || (ixpt1 >= m_iRenderWidth) || (ixpt2 >= m_iRenderWidth) )
         continue;

      if ( ixpt1 > ixpt2 )
      {
         ixpt = ixpt1;
         ixpt1 = ixpt2;
         ixpt2 = ixpt;
      }

      //if ( (ixpt1 < ixmin) || (ixpt2 > ixmax) )
      //   continue;

      if ( ixpt1 < ixmin )
         ixpt1 = ixmin;
      if ( ixpt2 > ixmax )
         ixpt2 = ixmax;
      //fbg_line(m_pFBG, ixpt1, iy, ixpt2, iy, m_ColorFill[0], m_ColorFill[1], m_ColorFill[2], m_ColorFill[3]);
      fbg_line(m_pFBG, ixpt1, iy, ixpt2, iy, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]/2);
   }

   drawLine(x1,y1,x2,y2);
   drawLine(x2,y2,x3,y3);
   drawLine(x3,y3,x1,y1);
}

void RenderEngineRaw::drawPolyLine(float* x, float* y, int count)
{
   fbg_enable_alpha(m_pFBG, m_bEnableAlphaBlending?1:0);
   for( int i=0; i<count-1; i++ )
      drawLine(x[i], y[i], x[i+1], y[i+1]);
   drawLine(x[count-1], y[count-1], x[0], y[0]);
}

void RenderEngineRaw::fillPolygon(float* x, float* y, int count)
{
   fbg_enable_alpha(m_pFBG, m_bEnableAlphaBlending?1:0);
   if ( count < 3 || count > 120 )
      return;
   float xIntersections[256];
   int countIntersections = 0;
   float yMin, yMax;
   float xMin, xMax;

   xMin = xMax = x[0];
   yMin = yMax = y[0];

   for( int i=1; i<count; i++ )
   {
      if ( x[i] < xMin ) xMin = x[i];
      if ( y[i] < yMin ) yMin = y[i];
      if ( x[i] > xMax ) xMax = x[i];
      if ( y[i] > yMax ) yMax = y[i];
   }

   for( float yLine = yMin; yLine <= yMax; yLine += m_fPixelHeight )
   {
      countIntersections = 0;
      for( int i=0; i<count; i++ )
      {
         int j = (i+1)%count;

         // Horizontal line
         if ( fabs(y[i]-yLine) < 0.3*m_fPixelHeight && fabs(y[j]-yLine) < 0.3*m_fPixelHeight )
            drawLine(x[i], y[i], x[j], y[j]);
         else if ( y[i] <= yLine && y[j] >= yLine )
         {
            float xInt = x[i] + (x[j]-x[i])*(yLine-y[i])/(y[j]-y[i]);
            xIntersections[countIntersections] = xInt;
            countIntersections++;
         }
         else if ( y[i] >= yLine && y[j] <= yLine )
         {
            float xInt = x[j] + (x[i]-x[j])*(yLine-y[j])/(y[i]-y[j]);
            xIntersections[countIntersections] = xInt;
            countIntersections++;
         }
      }

      // Sort intersections;
      for( int i=0; i<countIntersections-1; i++ )
      for( int j=i+1; j<countIntersections; j++ )
      {
         if ( xIntersections[i] > xIntersections[j] )
         {
            float tmp = xIntersections[i];
            xIntersections[i] = xIntersections[j];
            xIntersections[j] = tmp;
         }
      }

      // Remove duplicates if odd count
      if ( countIntersections > 2 )
      if ( countIntersections%2 )
      for( int i=0; i<countIntersections-1; i++ )
      {
         if ( fabs(xIntersections[i]-xIntersections[i+1]) < 0.0001 )
         {
            while ( i<countIntersections-1 )
            {
               xIntersections[i] = xIntersections[i+1];
               i++;
            }
            countIntersections--;
            break;
         }
      }

      // Draw lines
      for( int i=0; i<countIntersections; i+= 2 )
         if ( xIntersections[i] >= xMin && xIntersections[i] <= xMax )
         if ( xIntersections[i+1] >= xMin && xIntersections[i+1] <= xMax )
            drawLine(xIntersections[i], yLine, xIntersections[i+1], yLine);
   }

   for( int i=0; i<count-1; i++ )
      drawLine(x[i], y[i], x[i+1], y[i+1]);
   drawLine(x[count-1], y[count-1], x[0], y[0]);

}


void RenderEngineRaw::fillCircle(float x, float y, float r)
{
   fbg_enable_alpha(m_pFBG, m_bEnableAlphaBlending?1:0);
   u8 tmpColor[4];

   memcpy(tmpColor, m_ColorStroke, 4*sizeof(u8));
   memcpy(m_ColorStroke, m_ColorFill, 4*sizeof(u8));

   float yTop = (y - r);
   float yBottom = (y + r);

   float yi = yTop;
   while ( yi <= yBottom )
   {
      float xi = sqrt(r*r-(yi-y)*(yi-y));
      drawLine(x-xi/getAspectRatio(), yi, x+xi/getAspectRatio(), yi);
      //drawLine(x-5*m_fPixelWidth, yi, x+5*m_fPixelWidth, yi);
      yi += m_fPixelHeight;
   }

   memcpy(m_ColorStroke, tmpColor, 4*sizeof(u8));
}

void RenderEngineRaw::drawCircle(float x, float y, float r)
{
   fbg_enable_alpha(m_pFBG, m_bEnableAlphaBlending?1:0);
   float xp[180];
   float yp[180];

   int points = r*6.0/(m_fPixelHeight*12.0);
   if ( points < 12 )
      points = 12;
   if ( points > 180 )
      points = 180;
   float dAngle = 360*0.0174533/(float)points;
   float angle = 0.0;
   for( int i=0; i<points; i++ )
   {
      xp[i] = x + r*cos(angle)/getAspectRatio();
      yp[i] = y + r*sin(angle);
      angle += dAngle;
   }

   drawPolyLine(xp,yp,points);
}

void RenderEngineRaw::drawArc(float x, float y, float r, float a1, float a2)
{
   fbg_enable_alpha(m_pFBG, m_bEnableAlphaBlending?1:0);
   float xp[180];
   float yp[180];

   int points = ((a2-a1)/360.0)*r*8.0/(m_fPixelHeight*12.0);
   if ( points < 6 )
      points = 6;
   if ( points > 180 )
      points = 180;
   float dAngle = (a2-a1)*0.0174533/((float)points-1);
   float angle = a1*0.0174533;
   for( int i=0; i<points; i++ )
   {
      xp[i] = x + r*cos(angle)/getAspectRatio();
      yp[i] = y - r*sin(angle);
      angle += dAngle;
   }

   for( int i=0; i<points-1; i++ )
      drawLine(xp[i], yp[i], xp[i+1], yp[i+1]);
}

