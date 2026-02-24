#pragma once

#include "render_engine.h"
#include "fontinfo.h"

class RenderEngineOVG: public RenderEngine
{
   public:
     RenderEngineOVG();
     virtual ~RenderEngineOVG();

     virtual void setColors(const double* color);
     virtual void setColors(const double* color, float fAlfaScale);
     virtual void setFill(float r, float g, float b, float a);
     virtual void setStroke(const double* color);
     virtual void setStroke(const double* color, float fStrokeWidth);
     virtual void setStroke(float r, float g, float b, float a);
     virtual void setStrokeWidth(float fWidth);

     virtual int loadFont(const char* szFontFile);
     virtual void freeFont(u32 idFont);
     virtual u32 loadImage(const char* szFile);
     virtual void freeImage(u32 idImage);
     virtual u32 loadIcon(const char* szFile);
     virtual void freeIcon(u32 idIcon);

     virtual void startFrame();
     virtual void endFrame();

     virtual void flipScreenOrientation();
     virtual void resetScreenOrientation();
     virtual void translate(float dx, float dy);
     virtual void rotate(float angle);

     virtual void drawImage(float xPos, float yPos, float fWidth, float fHeight, u32 imageId);

     virtual float textWidth(u32 fontId, const char* szText);
     virtual void drawText(float xPos, float yPos, u32 fontId, const char* szText);
     virtual void drawTextLeft(float xPos, float yPos, u32 fontId, const char* szText);

     virtual float getMessageHeight(const char* text, float line_spacing_percent, float max_width, u32 fontId);
     virtual float drawMessageLines(float xPos, float yPos, const char* text, float line_spacing_percent, float max_width, u32 fontId);

     virtual void drawLine(float x1, float y1, float x2, float y2);
     virtual void drawRect(float xPos, float yPos, float fWidth, float fHeight);
     virtual void drawRoundRect(float xPos, float yPos, float fWidth, float fHeight, float fCornerRadius);
     virtual void drawTriangle(float x1, float y1, float x2, float y2, float x3, float y3);
     virtual void drawPolyLine(float* x, float* y, int count);
     virtual void fillPolygon(float* x, float* y, int count);

   private:

      Fontinfo m_Fonts[10];
      u32 m_FontIds[10];
      u32 m_CurrentFontId;
      int m_iCountFonts;

      VGImage m_Images[3];
      u32 m_ImageIds[3];
      u32 m_CurrentImageId;
      int m_iCountImages;
};
