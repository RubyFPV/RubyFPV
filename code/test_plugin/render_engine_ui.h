#pragma once

// The drawing functions use a coordonate system from 0 to 1.
// That is, no matter what the screen resolution is, the top left corner of the screen is (0,0) and the bottom right corner of the screen is (1,1);
// Angles are in degrees;
// Colors are an array of 4 double values: r,g,b are from 0 to 255 and a (alpha) is from 0 to 1
// Supported image file formats are: PNG


class RenderEngineUI
{
   public:
     RenderEngineUI();
     virtual ~RenderEngineUI();

     int getScreenWidth(); // in pixels
     int getScreenHeight(); // in pixels
     float getAspectRatio();
     float getPixelWidth(); // screen drawing functions uses coordonates from 0 to 1. So a pixel width is 1/screen_width_in_pixels
     float getPixelHeight();

     // There are some predefined fonts and colors that you can and should use, to have a consistent user interface along with other plugins and Ruby OSD:
     // 3 fonts, the actual size is customized by enduser;
     // 4 colors, to be used for regular OSD text, for OSD instruments, for line outline and for warning messages/icons;

     unsigned int getFontIdSmall();
     unsigned int getFontIdRegular();
     unsigned int getFontIdBig();
     unsigned int loadFontSize(float fSize);

     const double* getColorOSDText(); // returns 4 double values: r,g,b,a.  r,g,b=0...255, a=0...1
     const double* getColorOSDInstruments();
     const double* getColorOSDOutline();
     const double* getColorOSDWarning();

     unsigned int loadImage(const char* szFile); // supports PNG files
     void freeImage(unsigned int idImage);
     unsigned int loadIcon(const char* szFile); // supports PNG files
     void freeIcon(unsigned int idIcon);

     void highlightFirstWordOfLine(bool bHighlight);
     bool drawBackgroundBoundingBoxes(bool bEnable);

     void setColors(double* color); // parameter is an array with 4 double values: r,g,b,a.  r,g,b=0...255, a=0...1
     void setColors(double* color, float fAlfaScale);
     void setFill(float r, float g, float b, float a);
     void setStroke(double* color);
     void setStroke(double* color, float fStrokeSize);
     void setStroke(float r, float g, float b, float a);
     float getStrokeSize();
     void setStrokeSize(float fStrokeSize);
     void setFontColor(unsigned int fontId, double* color);

     void drawImage(float xPos, float yPos, float fWidth, float fHeight, unsigned int imageId);
     void drawIcon(float xPos, float yPos, float fWidth, float fHeight, unsigned int iconId);

     float textHeight(unsigned int fontId);
     float textWidth(unsigned int fontId, const char* szText);
     void drawText(float xPos, float yPos, unsigned int fontId, const char* szText);
     void drawTextLeft(float xPos, float yPos, unsigned int fontId, const char* szText);
     float getMessageHeight(const char* text, float line_spacing_percent, float max_width, unsigned int fontId);
     float drawMessageLines(const char* text, float xPos, float yPos, float line_spacing_percent, float max_width, unsigned int fontId);

     void drawLine(float x1, float y1, float x2, float y2);
     void drawRect(float xPos, float yPos, float fWidth, float fHeight);
     void drawRoundRect(float xPos, float yPos, float fWidth, float fHeight, float fCornerRadius);
     void drawTriangle(float x1, float y1, float x2, float y2, float x3, float y3);
     void drawPolyLine(float* x, float* y, int count);
     void fillPolygon(float* x, float* y, int count);

     void fillCircle(float x, float y, float r);
     void drawCircle(float x, float y, float r);
     void drawArc(float x, float y, float r, float a1, float a2);
};

