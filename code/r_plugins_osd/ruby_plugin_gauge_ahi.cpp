// Licence: you can copy, edit, change or do whatever you wish with the code in this file

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "../public/render_engine_ui.h"
#include "../public/telemetry_info.h"
#include "../public/settings_info.h"

#include "ruby_plugins_common.cpp"

PLUGIN_VAR RenderEngineUI* g_pEngine = NULL;
PLUGIN_VAR const char* g_szPluginName = "Ruby Gauge AHI";
PLUGIN_VAR const char* g_szUID = "342AST-25672Q-WE31-RGAHI"; // This needs to be a unique number/id/string

PLUGIN_VAR const char* g_szOption1 = "Show Heading";

#ifdef __cplusplus
extern "C" {
#endif


void init(void* pEngine)
{
  g_pEngine = (RenderEngineUI*)pEngine;
}

char* getName()
{
   return (char*)g_szPluginName;
}

int getVersion()
{
   return 1;
}

char* getUID()
{
   return (char*)g_szUID;
}

int getPluginSettingsCount()
{
   return 1;
}

char* getPluginSettingName(int settingIndex)
{
   if ( 0 == settingIndex )
      return (char*)g_szOption1;
   return NULL;
}

int getPluginSettingType(int settingIndex)
{
   if ( 0 == settingIndex )
      return PLUGIN_SETTING_TYPE_BOOL;

   return PLUGIN_SETTING_TYPE_INT;
}

int getPluginSettingMinValue(int settingIndex)
{
   return 0;
}

int getPluginSettingMaxValue(int settingIndex)
{
   return 1;
}

int getPluginSettingDefaultValue(int settingIndex)
{
   if ( 0 == settingIndex )
      return 1;
   return 1;
}

int getPluginSettingOptionsCount(int settingIndex)
{
   return 0;
}

char* getPluginSettingOptionName(int settingIndex, int optionIndex)
{
   return NULL;
}

float getDefaultWidth()
{
   if ( NULL == g_pEngine )
      return 0.24;
   return 0.24/g_pEngine->getAspectRatio();
}

float getDefaultHeight()
{
   if ( NULL == g_pEngine )
      return 0.24;
   return 0.24;
}

void render(vehicle_and_telemetry_info_t* pTelemetryInfo, plugin_settings_info_t2* pCurrentSettings, float xPos, float yPos, float fWidth, float fHeight)
{
   if ( NULL == g_pEngine || NULL == pTelemetryInfo || NULL == pCurrentSettings )
      return;


   char szBuff[64];

   float fBackgroundAlpha = pCurrentSettings->fBackgroundAlpha;
   double colorSky[4] = {20,70,150,0.95};

   float xCenter = xPos + 0.5*fWidth;
   float yCenter = yPos + 0.5*fHeight;
   float fRadius = fHeight*0.5;
   float fHeadingSize = fRadius * 0.16;

   draw_shadow(g_pEngine, xCenter, yCenter, fRadius);

   g_pEngine->setColors(g_pEngine->getColorOSDInstruments());

   //u32 fontId = g_pEngine->getFontIdRegular();
   //float height_text = g_pEngine->textHeight(fontId);

   u32 fontIdSmall = g_pEngine->getFontIdSmall();
   float height_text_small = g_pEngine->textHeight(fontIdSmall);

   g_pEngine->setStroke(0,0,0,fBackgroundAlpha);
   g_pEngine->setFill(0,0,0,fBackgroundAlpha);
   g_pEngine->setStrokeSize(0.0);
   g_pEngine->fillCircle(xCenter, yCenter, fRadius);
   g_pEngine->setColors(g_pEngine->getColorOSDInstruments());

   fRadius = fRadius*0.98;

   float xp[64];
   float yp[64];
   int points = fRadius*8.0/(g_pEngine->getPixelHeight()*30.0);

   if ( points < 10 )
      points = 10;
   if ( points > 63 )
      points = 63;

   float fAngleStart = 180.0;
   float fAngleEnd = 0.0;

   float fPitchAngle = pTelemetryInfo->pitch;
   if ( fPitchAngle < -80.0 ) fPitchAngle = -80.0;
   if ( fPitchAngle > 80.0 ) fPitchAngle = 80.0;
   float fHorizontShift = fPitchAngle/90.0;

   float fRollAngle = pTelemetryInfo->roll;
   if ( fRollAngle < -80.0 ) fRollAngle = -80.0;
   if ( fRollAngle > 80.0 ) fRollAngle = 80.0;

   fAngleStart += asin(fHorizontShift) * 57.2958;
   fAngleEnd -= asin(fHorizontShift) * 57.2958;

   //float xHorizontLeft = xCenter + fRadius*cos(fAngleStart*0.017453)/g_pEngine->getAspectRatio();
   float yHorizontLeft = yCenter - fRadius*sin(fAngleStart*0.017453);
   //float xHorizontRight = xCenter + fRadius*cos(fAngleEnd*0.017453)/g_pEngine->getAspectRatio();
   //float yHorizontRight = yCenter - fRadius*sin(fAngleEnd*0.017453);

   // Sky

   float dAngle = (fAngleStart - fAngleEnd)/(float)points;
   float angle = fAngleStart;
   for( int i=0; i<=points; i++ )
   {
      xp[i] = xCenter + fRadius*cos(angle*0.017453)/g_pEngine->getAspectRatio();
      yp[i] = yCenter - fRadius*sin(angle*0.017453);
      angle -= dAngle;
   }

   rotatePoints(g_pEngine, xp, yp, -fRollAngle, points+1, xCenter, yCenter, 1.0 );
   g_pEngine->setFill(colorSky[0],colorSky[1],colorSky[2],colorSky[3]);
   g_pEngine->setStroke(colorSky[0],colorSky[1],colorSky[2],colorSky[3]);
   g_pEngine->setStrokeSize(0.0);
   g_pEngine->fillPolygon(xp,yp, points+1);

   // Ground

   dAngle = (360.0 - fAngleStart + fAngleEnd)/(float)points;
   angle = fAngleEnd;
   for( int i=0; i<=points; i++ )
   {
      xp[i] = xCenter + fRadius*cos(angle*0.017453)/g_pEngine->getAspectRatio();
      yp[i] = yCenter - fRadius*sin(angle*0.017453);
      angle -= dAngle;
   }

   rotatePoints(g_pEngine, xp, yp, -fRollAngle, points+1, xCenter, yCenter, 1.0 );
   g_pEngine->setFill(40,30,10,0.97);
   g_pEngine->setStroke(40,30,10,0.97);
   g_pEngine->setStrokeSize(0.0);
   g_pEngine->fillPolygon(xp,yp, points+1);


   // Show heading?
   if ( 1 == pCurrentSettings->nSettingsValues[0] )
   {
      fRadius -= fHeadingSize;
   }

   // Roll Gradations

   double colorGradations[4] = {200,200,200,0.96};
   g_pEngine->setColors(colorGradations);
   g_pEngine->setStroke(colorGradations, 3.0);

   for (int iAngle = 180; iAngle >= 0; iAngle -= 5)
   {
      float x0 = xCenter + (fRadius-g_pEngine->getPixelHeight()*3.0) * cos(((float)iAngle)*0.017453)/g_pEngine->getAspectRatio();
      float y0 = yCenter - (fRadius-g_pEngine->getPixelHeight()*3.0) * sin(((float)iAngle)*0.017453);
      float x1 = xCenter + (fRadius*0.84+g_pEngine->getPixelHeight()*3.0) * cos(((float)iAngle)*0.017453)/g_pEngine->getAspectRatio();
      float y1 = yCenter - (fRadius*0.84+g_pEngine->getPixelHeight()*3.0) * sin(((float)iAngle)*0.017453);
      if ( (iAngle % 10) != 0 )
      {
         x1 = xCenter + (fRadius*0.88+g_pEngine->getPixelHeight()*3.0) * cos(((float)iAngle)*0.017453)/g_pEngine->getAspectRatio();
         y1 = yCenter - (fRadius*0.88+g_pEngine->getPixelHeight()*3.0) * sin(((float)iAngle)*0.017453);
      }
      rotate_point(g_pEngine, x0, y0, xCenter, yCenter, -fRollAngle, &x0, &y0);
      rotate_point(g_pEngine, x1, y1, xCenter, yCenter, -fRollAngle, &x1, &y1);
      g_pEngine->drawLine(x0,y0,x1,y1);
   }

   g_pEngine->setStroke(colorGradations, 2.0);
   xp[0] = xCenter;
   yp[0] = yCenter-fRadius*0.84;
   xp[1] = xp[0] - fRadius*0.04;
   yp[1] = yp[0] - fRadius*0.12;
   xp[2] = xp[0] + fRadius*0.04;
   yp[2] = yp[0] - fRadius*0.12;
   rotatePoints(g_pEngine, xp, yp, -fRollAngle, 3, xCenter, yCenter, 1.0);
   g_pEngine->fillPolygon(xp,yp,3);

   g_pEngine->setStroke(colorGradations, 1.0);
   for( float fDist=0.0; fDist<fRadius*0.85; fDist += g_pEngine->getPixelHeight()*10.0 )
   {
      float x0 = xCenter;
      float y0 = yCenter - fDist;
      float x1 = xCenter;
      float y1 = yCenter - fDist - g_pEngine->getPixelHeight() * 4.0;
      rotate_point(g_pEngine, x0, y0, xCenter, yCenter, -fRollAngle, &x0, &y0);
      rotate_point(g_pEngine, x1, y1, xCenter, yCenter, -fRollAngle, &x1, &y1);
      g_pEngine->drawLine(x0,y0,x1,y1);

      x0 = xCenter - fDist/g_pEngine->getAspectRatio();
      y0 = yCenter;
      x1 = xCenter - (fDist + g_pEngine->getPixelHeight() * 4.0)/g_pEngine->getAspectRatio();
      y1 = yCenter;
      rotate_point(g_pEngine, x0, y0, xCenter, yCenter, -fRollAngle, &x0, &y0);
      rotate_point(g_pEngine, x1, y1, xCenter, yCenter, -fRollAngle, &x1, &y1);
      g_pEngine->drawLine(x0,y0,x1,y1);

      x0 = xCenter + fDist/g_pEngine->getAspectRatio();
      y0 = yCenter;
      x1 = xCenter + (fDist + g_pEngine->getPixelHeight() * 4.0)/g_pEngine->getAspectRatio();
      y1 = yCenter;
      rotate_point(g_pEngine, x0, y0, xCenter, yCenter, -fRollAngle, &x0, &y0);
      rotate_point(g_pEngine, x1, y1, xCenter, yCenter, -fRollAngle, &x1, &y1);
      g_pEngine->drawLine(x0,y0,x1,y1);
   }

   for( float fDist=0.0; fDist<fRadius*0.85; fDist += g_pEngine->getPixelHeight()*6.0 )
   {
      float x0 = xCenter - fDist/g_pEngine->getAspectRatio();
      float y0 = yCenter - fDist;
      float x1 = xCenter - (fDist - g_pEngine->getPixelHeight() * 2.0)/g_pEngine->getAspectRatio();
      float y1 = yCenter - fDist - g_pEngine->getPixelHeight() * 2.0;
      if ( (x1-xCenter)*(x1-xCenter) + (y1-yCenter)*(y1-yCenter) < fRadius*0.85*fRadius*0.85 )
      {
         rotate_point(g_pEngine, x0, y0, xCenter, yCenter, -fRollAngle, &x0, &y0);
         rotate_point(g_pEngine, x1, y1, xCenter, yCenter, -fRollAngle, &x1, &y1);
         g_pEngine->drawLine(x0,y0,x1,y1);
      }
      x0 = xCenter + fDist/g_pEngine->getAspectRatio();
      y0 = yCenter - fDist;
      x1 = xCenter + (fDist + g_pEngine->getPixelHeight() * 2.0)/g_pEngine->getAspectRatio();
      y1 = yCenter - fDist - g_pEngine->getPixelHeight() * 2.0;
      if ( (x1-xCenter)*(x1-xCenter) + (y1-yCenter)*(y1-yCenter) < fRadius*0.85*fRadius*0.85 )
      {
         rotate_point(g_pEngine, x0, y0, xCenter, yCenter, -fRollAngle, &x0, &y0);
         rotate_point(g_pEngine, x1, y1, xCenter, yCenter, -fRollAngle, &x1, &y1);
         g_pEngine->drawLine(x0,y0,x1,y1);
      }
   }



   // Pitch Gradations

   g_pEngine->setColors(colorGradations);
   g_pEngine->setStroke(colorGradations, 3.0);

   for( int i=0; i<80; i+= 10 )
   {
      float dy = fRadius * (float)i/90.0;
      float dx = fRadius * 0.14;
      if ( (i % 20) == 0 )
         dx = fRadius * 0.34;

      float x0 = xCenter - dx/g_pEngine->getAspectRatio();
      float x1 = xCenter + dx/g_pEngine->getAspectRatio();
      float y0 = yCenter - dy + (yHorizontLeft-yCenter);
      float y1 = yCenter - dy + (yHorizontLeft-yCenter);
      if ( fabs(y0 - yCenter) < fRadius*0.86 )
      {
         rotate_point(g_pEngine, x0, y0, xCenter, yCenter, -fRollAngle, &x0, &y0);
         rotate_point(g_pEngine, x1, y1, xCenter, yCenter, -fRollAngle, &x1, &y1);
         g_pEngine->drawLine(x0,y0,x1,y1);
      }
      x0 = xCenter - dx/g_pEngine->getAspectRatio();
      x1 = xCenter + dx/g_pEngine->getAspectRatio();
      y0 = yCenter + dy + (yHorizontLeft-yCenter);
      y1 = yCenter + dy + (yHorizontLeft-yCenter);
      if ( fabs(y0 - yCenter) < fRadius*0.86 )
      {
         rotate_point(g_pEngine, x0, y0, xCenter, yCenter, -fRollAngle, &x0, &y0);
         rotate_point(g_pEngine, x1, y1, xCenter, yCenter, -fRollAngle, &x1, &y1);
         g_pEngine->drawLine(x0,y0,x1,y1);
      }
   }

   // Plane shadow

   double colorShadow[4] = {20,20,20,0.1};
   float fShadowDy = g_pEngine->getPixelHeight()*3.0;
   float fShadowDx = g_pEngine->getPixelHeight()*2.0/g_pEngine->getAspectRatio();
   g_pEngine->setColors(colorShadow);
   g_pEngine->setStroke(colorShadow, 3.0);

   g_pEngine->drawLine(xCenter-fRadius*0.7/g_pEngine->getAspectRatio() + fShadowDx, yCenter + fShadowDy, xCenter-fRadius*0.2/g_pEngine->getAspectRatio() + fShadowDx, yCenter + fShadowDy);
   g_pEngine->drawLine(xCenter-fRadius*0.2/g_pEngine->getAspectRatio() + fShadowDx, yCenter + fShadowDy, xCenter-fRadius*0.2/g_pEngine->getAspectRatio() + fShadowDx, yCenter + fRadius*0.1 + fShadowDy);
   g_pEngine->drawLine(xCenter-fRadius*0.7/g_pEngine->getAspectRatio() + fShadowDx, yCenter-g_pEngine->getPixelHeight() + fShadowDy, xCenter-fRadius*0.2/g_pEngine->getAspectRatio() + fShadowDx, yCenter-g_pEngine->getPixelHeight() + fShadowDy);

   g_pEngine->drawLine(xCenter+fRadius*0.7/g_pEngine->getAspectRatio() + fShadowDx, yCenter + fShadowDy, xCenter+fRadius*0.2/g_pEngine->getAspectRatio() + fShadowDx, yCenter + fShadowDy);
   g_pEngine->drawLine(xCenter+fRadius*0.2/g_pEngine->getAspectRatio() + fShadowDx, yCenter + fShadowDy, xCenter+fRadius*0.2/g_pEngine->getAspectRatio() + fShadowDx, yCenter + fRadius*0.1 + fShadowDy);
   g_pEngine->drawLine(xCenter+fRadius*0.7/g_pEngine->getAspectRatio() + fShadowDx, yCenter-g_pEngine->getPixelHeight() + fShadowDy, xCenter+fRadius*0.2/g_pEngine->getAspectRatio() + fShadowDx, yCenter-g_pEngine->getPixelHeight() + fShadowDy);

   g_pEngine->drawCircle(xCenter + fShadowDx, yCenter + fShadowDy, g_pEngine->getPixelHeight()*2.0);
   xp[0] = xCenter + fShadowDx;
   yp[0] = yCenter-fRadius*0.8 + fShadowDy;
   xp[1] = xp[0] - fRadius*0.05;
   yp[1] = yp[0] + fRadius*0.12;
   xp[2] = xp[0] + fRadius*0.05;
   yp[2] = yp[0] + fRadius*0.12;
   g_pEngine->fillPolygon(xp,yp,3);

   // Plane

   double colorPlane[4] = {200,150,20,0.9};
   g_pEngine->setColors(colorPlane);
   g_pEngine->setStroke(colorPlane, 3.0);

   g_pEngine->drawLine(xCenter-fRadius*0.7/g_pEngine->getAspectRatio(), yCenter, xCenter-fRadius*0.2/g_pEngine->getAspectRatio(), yCenter);
   g_pEngine->drawLine(xCenter-fRadius*0.2/g_pEngine->getAspectRatio(), yCenter, xCenter-fRadius*0.2/g_pEngine->getAspectRatio(), yCenter + fRadius*0.1);
   g_pEngine->drawLine(xCenter-fRadius*0.7/g_pEngine->getAspectRatio(), yCenter-g_pEngine->getPixelHeight(), xCenter-fRadius*0.2/g_pEngine->getAspectRatio(), yCenter-g_pEngine->getPixelHeight());

   g_pEngine->drawLine(xCenter+fRadius*0.7/g_pEngine->getAspectRatio(), yCenter, xCenter+fRadius*0.2/g_pEngine->getAspectRatio(), yCenter);
   g_pEngine->drawLine(xCenter+fRadius*0.2/g_pEngine->getAspectRatio(), yCenter, xCenter+fRadius*0.2/g_pEngine->getAspectRatio(), yCenter + fRadius*0.1);
   g_pEngine->drawLine(xCenter+fRadius*0.7/g_pEngine->getAspectRatio(), yCenter-g_pEngine->getPixelHeight(), xCenter+fRadius*0.2/g_pEngine->getAspectRatio(), yCenter-g_pEngine->getPixelHeight());

   g_pEngine->drawCircle(xCenter, yCenter, g_pEngine->getPixelHeight()*2.0);
   xp[0] = xCenter;
   yp[0] = yCenter-fRadius*0.8;
   xp[1] = xp[0] - fRadius*0.05;
   yp[1] = yp[0] + fRadius*0.12;
   xp[2] = xp[0] + fRadius*0.05;
   yp[2] = yp[0] + fRadius*0.12;
   g_pEngine->fillPolygon(xp,yp,3);

   // Show roll, pitch values

   sprintf(szBuff, "%d", -(int)(pTelemetryInfo->roll));
   float wText = g_pEngine->textWidth(fontIdSmall, szBuff);

   g_pEngine->setFill(0,0,0,0.3);
   g_pEngine->setStroke(0,0,0,0);
   g_pEngine->drawRoundRect(xCenter - wText*0.7, yCenter - fRadius*0.64, wText*1.4, height_text_small*1.2, 0.1);

   g_pEngine->setColors(colorPlane);
   g_pEngine->setStroke(colorPlane, 3.0);
   show_value_centered(g_pEngine, xCenter, yCenter - fRadius*0.64, szBuff, fontIdSmall);

   sprintf(szBuff, "%d", (int)(pTelemetryInfo->pitch));
   wText = g_pEngine->textWidth(fontIdSmall, szBuff);

   g_pEngine->setFill(0,0,0,0.3);
   g_pEngine->setStroke(0,0,0,0);
   g_pEngine->drawRoundRect(xCenter+fRadius*0.35 - wText*0.7, yCenter - height_text_small*1.1 , wText*1.4, height_text_small*1.2, 0.1);
   g_pEngine->drawRoundRect(xCenter-fRadius*0.35 - wText*0.7, yCenter - height_text_small*1.1 , wText*1.4, height_text_small*1.2, 0.1);

   g_pEngine->setColors(colorPlane);
   g_pEngine->setStroke(colorPlane, 3.0);
   show_value_centered(g_pEngine, xCenter+fRadius*0.35, yCenter - height_text_small*1.1, szBuff, fontIdSmall);
   show_value_centered(g_pEngine, xCenter-fRadius*0.35, yCenter - height_text_small*1.1, szBuff, fontIdSmall);

   // Show heading?

   if ( 0 == pCurrentSettings->nSettingsValues[0] )
   {
      g_pEngine->setColors(g_pEngine->getColorOSDInstruments());
      return;
   }

   // Show heading

   fRadius = fHeight*0.5;
   fRadius = fRadius*0.98;
   float fRadiusInner = fRadius - fHeadingSize + g_pEngine->getPixelHeight()*2.0;

   g_pEngine->setColors(colorPlane);
   g_pEngine->setStroke(colorPlane, 2.0);
   g_pEngine->drawCircle(xCenter, yCenter, fRadiusInner);

   g_pEngine->setColors(g_pEngine->getColorOSDInstruments());

   g_pEngine->setStroke(colorGradations, 3.0);

   for( float fAngle = 0; fAngle<360; fAngle += 10 )
   {
      float fFinalAngle = fAngle + 90.0 + (float) pTelemetryInfo->heading;
      float x0 = xCenter + (fRadius-g_pEngine->getPixelHeight()*3.0) * cos(fFinalAngle*0.017453)/g_pEngine->getAspectRatio();
      float y0 = yCenter - (fRadius-g_pEngine->getPixelHeight()*3.0) * sin(fFinalAngle*0.017453);

      float x1 = xCenter + (fRadiusInner+g_pEngine->getPixelHeight()*3.0 + fHeadingSize*0.5) * cos(fFinalAngle*0.017453)/g_pEngine->getAspectRatio();
      float y1 = yCenter - (fRadiusInner+g_pEngine->getPixelHeight()*3.0 + fHeadingSize*0.5) * sin(fFinalAngle*0.017453);
      if ( (((int)fAngle) % 90) == 0 )
      {
         x1 = xCenter + (fRadiusInner+g_pEngine->getPixelHeight()*3.0) * cos(fFinalAngle*0.017453)/g_pEngine->getAspectRatio();
         y1 = yCenter - (fRadiusInner+g_pEngine->getPixelHeight()*3.0) * sin(fFinalAngle*0.017453);
      }
      g_pEngine->drawLine(x0,y0,x1,y1);

      if ( (((int)fAngle) % 90) == 0 )
      {
         szBuff[0] = 0;
         if ( ((int)fAngle) == 0 )
            strcpy(szBuff, "N");
         if ( ((int)fAngle) == 90 )
            strcpy(szBuff, "W");
         if ( ((int)fAngle) == 180 )
            strcpy(szBuff, "S");
         if ( ((int)fAngle) == 270 )
            strcpy(szBuff, "E");
         show_value_centered(g_pEngine, (x0+x1)*0.5, (y0+y1)*0.5 - height_text_small*0.4, szBuff, fontIdSmall);
      }
   }

   // Show heading value

   sprintf(szBuff, "%d", (int)(pTelemetryInfo->heading));
   wText = g_pEngine->textWidth(fontIdSmall, szBuff);

   g_pEngine->setFill(0,0,0,0.3);
   g_pEngine->setStroke(0,0,0,0);
   g_pEngine->drawRoundRect(xCenter - wText*0.7, yPos - height_text_small, wText*1.4, height_text_small*1.2, 0.1);

   g_pEngine->setColors(colorPlane);
   g_pEngine->setStroke(colorPlane, 3.0);
   show_value_centered(g_pEngine, xCenter, yPos - height_text_small, szBuff, fontIdSmall);

}

#ifdef __cplusplus
}
#endif
