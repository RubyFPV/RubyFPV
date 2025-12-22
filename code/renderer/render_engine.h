#pragma once

#include "../base/base.h"

#define MAX_FONT_CHARS 256
#define MAX_FONT_KERINGS 1024
#define MAX_RAW_FONTS 100
#define MAX_RAW_IMAGES 100
#define MAX_RAW_ICONS 100


typedef struct
{
   int charId;
   int imgXOffset;
   int imgYOffset;
   int width;
   int height;
   int xOffset;
   int yOffset;
   int xAdvance;
} RenderEngineRawFontChar;

typedef struct
{
   char szName[128];
   int iFamilyId;
   bool bBold;
   //struct _fbg_img* pImage;
   void* pImageObject;
   int lineHeight;
   int baseLine;
   int charCount;
   int charIdFirst;
   int charIdLast;
   RenderEngineRawFontChar chars[MAX_FONT_CHARS];

   int keringsCount;
   int kerings[MAX_FONT_KERINGS][3]; // first char, second char, kering distance

   float dxLetters; // percent -1...0...1 of font height

} RenderEngineRawFont;


class RenderEngine
{
   public:
     RenderEngine();
     virtual ~RenderEngine();

     virtual bool initEngine();
     virtual bool uninitEngine();
     int getScreenWidth();
     int getScreenHeight();
     float getAspectRatio();
     float getPixelWidth();
     float getPixelHeight();
     virtual void* getDrawContext();

     float setGlobalAlfa(float alfa);
     float getGlobalAlfa();
     bool isAlphaBlendingEnabled();
     void setAlphaBlendingEnabled(bool bEnable);
     void enableAlphaBlending();
     void disableAlphaBlending();
     void setClearBufferByte(u8 uClearByte);

     virtual void highlightFirstWordOfLine(bool bHighlight);
     virtual bool drawBackgroundBoundingBoxes(bool bEnable);

     virtual void setColors(const double* color);
     virtual void setColors(const double* color, float fAlfaScale);
     virtual void setFill(const double* pColor);
     virtual void setFill(float r, float g, float b, float a);
     virtual void setStroke(const double* color);
     virtual void setStroke(const double* color, float fStrokeSize); 
     virtual void setStroke(float r, float g, float b, float a);
     virtual float getStrokeSize();
     virtual void setStrokeSize(float fStrokeSize);
     virtual void setFontColor(u32 fontId, double* color);
     virtual void enableFontScaling(bool bEnable);
     virtual void setFontBackgroundBoundingBoxFillColor(const double* color);
     virtual void setFontBackgroundBoundingBoxStrikeColor(const double* color);
     virtual void clearFontBackgroundBoundingBoxStrikeColor();
     virtual void setBackgroundBoundingBoxPadding(float fPadding);
     virtual void setFontBackgroundBoundingBoxSameTextColor(bool bSameColor);
     virtual bool getFontBackgroundBoundingBoxSameTextColor();

     virtual int loadRawFont(int iFamilyId, const char* szFontFile, int iBold);
     virtual void freeRawFont(u32 idFont);
     virtual void setFontOutlineColor(u32 idFont, u8 r, u8 g, u8 b, u8 a);
     virtual u32 loadImage(const char* szFile);
     virtual void freeImage(u32 idImage);
     virtual u32 loadIcon(const char* szFile);
     virtual void freeIcon(u32 idIcon);

     virtual int getImageWidth(u32 uImageId);
     virtual int getImageHeight(u32 uImageId);
     virtual void changeImageHue(u32 uImageId, u8 r, u8 g, u8 b);

     virtual void startFrame();
     virtual void endFrame();
     virtual bool isFrameStarted();

     virtual void rotate180();

     virtual void drawImage(float xPos, float yPos, float fWidth, float fHeight, u32 imageId);
     virtual void drawImageAlpha(float xPos, float yPos, float fWidth, float fHeight, u32 imageId, u8 uAlpha);
     virtual void bltImage(float xPosDest, float yPosDest, float fWidthDest, float fHeightDest, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, u32 uImageId);
     virtual void bltSprite(float xPosDest, float yPosDest, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, u32 uImageId);
     virtual void drawIcon(float xPos, float yPos, float fWidth, float fHeight, u32 iconId);
     virtual void bltIcon(float xPosDest, float yPosDest, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, u32 iconId);

     virtual float getRawFontHeight(u32 fontId);
     virtual float textHeight(u32 fontId);
     virtual float textWidth(u32 fontId, const char* szText);
     virtual float textRawHeight(u32 fontId);
     virtual float textRawWidth(u32 fontId, const char* szText);
     virtual float textRawWidthScaled(u32 fontId, float fScale, const char* szText);
     virtual void drawText(float xPos, float yPos, u32 uFontId, const char* szText);
     virtual void drawTextNoOutline(float xPos, float yPos, u32 fontId, const char* szText);
     virtual void drawTextLeft(float xPos, float yPos, u32 fontId, const char* szText);
     virtual void drawTextScaled(float xPos, float yPos, u32 fontId, float fScale, const char* szText);
     virtual void drawTextLeftScaled(float xPos, float yPos, u32 fontId, float fScale, const char* szText);
     virtual float getMessageWidth(const char* text, float max_width, u32 fontId);
     virtual float getMessageHeight(const char* text, float line_spacing_percent, float max_width, u32 fontId);
     virtual float drawMessageLines(float xPos, float yPos, const char* text, float line_spacing_percent, float max_width, u32 fontId);
     
     virtual void drawLine(float x1, float y1, float x2, float y2);
     virtual void drawRect(float xPos, float yPos, float fWidth, float fHeight);
     virtual void drawRoundRect(float xPos, float yPos, float fWidth, float fHeight, float fCornerRadius);
     virtual void drawRoundRectMenu(float xPos, float yPos, float fWidth, float fHeight, float fCornerRadius);
     virtual void drawTriangle(float x1, float y1, float x2, float y2, float x3, float y3);
     virtual void fillTriangle(float x1, float y1, float x2, float y2, float x3, float y3);
     virtual void drawPolyLine(float* x, float* y, int count);
     virtual void fillPolygon(float* x, float* y, int count);

     virtual void fillCircle(float x, float y, float r);
     virtual void drawCircle(float x, float y, float r);
     virtual void drawArc(float x, float y, float r, float a1, float a2);

     bool rectIntersect(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2);

   protected:
      virtual int _getRawFontIndexFromId(u32 fontId);
      virtual RenderEngineRawFont* _getRawFontFromId(u32 fontId);
      virtual u32 _getRawFontId(RenderEngineRawFont* pRawFont);
      virtual void* _loadRawFontImageObject(const char* szFileName);
      virtual void _freeRawFontImageObject(void* pImageObject);

      virtual float _get_raw_space_width(RenderEngineRawFont* pFont);
      virtual float _get_raw_char_width(RenderEngineRawFont* pFont, int ch);
      virtual void _drawSimpleTextBoundingBox(RenderEngineRawFont* pFont, const char* szText, float xPos, float yPos, float fScale);
      virtual void _drawSimpleText(RenderEngineRawFont* pFont, const char* szText, float xPos, float yPos);
      virtual void _drawSimpleTextScaled(RenderEngineRawFont* pFont, const char* szText, float xPos, float yPos, float fScale);

      bool m_bStartedFrame;
      int m_iRenderDepth;
      int m_iRenderWidth;
      int m_iRenderHeight;
      float m_fPixelWidth;
      float m_fPixelHeight;

      u8 m_uClearBufferByte;
      float m_fGlobalAlfa;
      bool m_bEnableAlphaBlending;
      bool m_bEnableFontScaling;
      bool m_bHighlightFirstWord;
      bool m_bDrawBackgroundBoundingBoxes;
      bool m_bDrawBackgroundBoundingBoxesTextUsesSameStrokeColor;
      
      float m_fBoundingBoxPadding;

      bool m_bDisableTextOutline;

      double m_ColorTextBackgroundBoundingBoxStrike[4];
      bool  m_bDrawStrikeOnTextBackgroundBoundingBoxes;

      u8 m_ColorFill[4];
      u8 m_ColorStroke[4];
      u8 m_ColorTextBoundingBoxBgFill[4];
      u8 m_uTextFontMixColor[4];
      float m_fStrokeSizePx;

      RenderEngineRawFont* m_pRawFonts[MAX_RAW_FONTS];
      u32 m_RawFontIds[MAX_RAW_FONTS];
      u32 m_CurrentRawFontId;
      int m_iCountRawFonts;
};



RenderEngine* render_init_engine();
RenderEngine* renderer_engine();
bool render_engine_uses_raw_fonts();

void render_free_engine();

extern "C" {

void render_engine_test();
}  

