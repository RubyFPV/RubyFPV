// Licence: you can copy, edit, change or do whatever you wish with the code in this file

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "../public/render_engine_ui.h"
#include "../public/telemetry_info.h"
#include "../public/settings_info.h"
#include "../public/utils/core_plugins_utils.h"

#include "ruby_plugins_common.cpp"

PLUGIN_VAR RenderEngineUI* g_pUIEngine = NULL;
PLUGIN_VAR const char* g_szPluginName = "Ruby Gauge Heading";
PLUGIN_VAR const char* g_szUID = "2398-2354-DJKS-RGH"; // This needs to be a unique number/id/string

PLUGIN_VAR const char* g_szOption1 = "Colors";
PLUGIN_VAR const char* g_szOption11 = "Default (Instruments)";
PLUGIN_VAR const char* g_szOption12 = "Solid (White)";
PLUGIN_VAR const char* g_szOption2 = "Show Home";
PLUGIN_VAR const char* g_szOption3 = "Show Home Distance";
PLUGIN_VAR const char* g_szOption4 = "Show Wind (Airplanes)";


PLUGIN_VAR u32 s_uFontIdCustomSize = 0;
PLUGIN_VAR u32 s_uFontIdCustomSizeLarger = 0;

#ifdef __cplusplus
extern "C" {
#endif


void init(void* pEngine)
{
  core_plugin_util_log_line("Plugin heading Init.");
  g_pUIEngine = (RenderEngineUI*)pEngine;
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
   return 4;
}

char* getPluginSettingName(int settingIndex)
{
   if ( 0 == settingIndex )
      return (char*)g_szOption1;
   if ( 1 == settingIndex )
      return (char*)g_szOption2;
   if ( 2 == settingIndex )
      return (char*)g_szOption3;
   if ( 3 == settingIndex )
      return (char*)g_szOption4;
   return NULL;
}

int getPluginSettingType(int settingIndex)
{
   if ( 0 == settingIndex )
      return PLUGIN_SETTING_TYPE_ENUM;
   if ( 1 == settingIndex )
      return PLUGIN_SETTING_TYPE_BOOL;
   if ( 2 == settingIndex )
      return PLUGIN_SETTING_TYPE_BOOL;
   if ( 3 == settingIndex )
      return PLUGIN_SETTING_TYPE_BOOL;
   return PLUGIN_SETTING_TYPE_INT;
}

int getPluginSettingDefaultValue(int settingIndex)
{
   if ( 0 == settingIndex )
      return 1;
   if ( 1 == settingIndex )
      return 1;
   if ( 2 == settingIndex )
      return 1;
   if ( 3 == settingIndex )
      return 0;
   return 1;
}


int getPluginSettingOptionsCount(int settingIndex)
{
   if ( 0 == settingIndex )
      return 2;
   return 0;
}

char* getPluginSettingOptionName(int settingIndex, int optionIndex)
{
   char* szRet = NULL;
   if ( 0 == settingIndex )
   {
      if ( 0 == optionIndex )
         szRet = (char*)g_szOption11;
      if ( 1 == optionIndex )
         szRet = (char*)g_szOption12;
   }
   return szRet;
}

float getDefaultWidth()
{
  core_plugin_util_log_line("Plugin heading width.");
   if ( NULL == g_pUIEngine )
      return 0.2;
   return 0.2/g_pUIEngine->getAspectRatio();
}

float getDefaultHeight()
{
  core_plugin_util_log_line("Plugin heading height.");
   if ( NULL == g_pUIEngine )
      return 0.2;
   return 0.2;
}


float _heading_course_to(double lat1, double long1, double lat2, double long2)
{
    double dlon = (long2-long1)*0.017453292519;

    lat1 = (lat1)*0.017453292519;
    lat2 = (lat2)*0.017453292519;

    double a1 = sin(dlon) * cos(lat2);
    double a2 = sin(lat1) * cos(lat2) * cos(dlon);

    a2 = cos(lat1) * sin(lat2) - a2;
    a2 = atan2(a1, a2);
    if (a2 < 0.0)
       a2 += 2.0*3.141592653589793;

    return a2*180.0/3.141592653589793;
}

void render(vehicle_and_telemetry_info_t* pTelemetryInfo, plugin_settings_info_t2* pCurrentSettings, float xPos, float yPos, float fWidth, float fHeight)
{
   if ( NULL == g_pUIEngine || NULL == pTelemetryInfo || NULL == pCurrentSettings )
      return;

   float fBackgroundAlpha = pCurrentSettings->fBackgroundAlpha;
   float xCenter = xPos + 0.5*fWidth;
   float yCenter = yPos + 0.5*fHeight;
   float fRadius = fHeight/2.0;

   float fFontSize = fRadius*0.17;
   float fFontSizeLarger = fRadius*0.24;
   float fStrokeSize = 4.0;
   float fStrokeSizeSmall = 2.0;

   s_uFontIdCustomSize = g_pUIEngine->loadFontSize(fFontSize);
   s_uFontIdCustomSizeLarger = g_pUIEngine->loadFontSize(fFontSizeLarger);

   if ( 0 == s_uFontIdCustomSize || MAX_U32 == s_uFontIdCustomSize )
      s_uFontIdCustomSize = g_pUIEngine->getFontIdSmall();

   if ( 0 == s_uFontIdCustomSizeLarger || MAX_U32 == s_uFontIdCustomSizeLarger )
      s_uFontIdCustomSizeLarger = g_pUIEngine->getFontIdRegular();

   char szBuff[64];
   double fColorWhite[4] = {230,210,200,1};
   double fColorRed[4] = {255,100,100,1};
   double fColorYellow[4] = {220,200,50,0.94};
   double fColorHome[4] = {150,170,255, 0.9};

   draw_shadow(g_pUIEngine, xCenter, yCenter, fRadius);

   if ( pCurrentSettings->nSettingsValues[0] == 1 )
      fBackgroundAlpha *= 1.5;

   g_pUIEngine->setStroke(0,0,0,fBackgroundAlpha);
   g_pUIEngine->setFill(0,0,0,fBackgroundAlpha);
   g_pUIEngine->setStrokeSize(1.0);
   g_pUIEngine->fillCircle(xCenter, yCenter, fRadius);

   // Draw the gauge and gradations

   if ( pCurrentSettings->nSettingsValues[0] == 0 )
      g_pUIEngine->setColors(g_pUIEngine->getColorOSDInstruments());
   else
      g_pUIEngine->setColors(fColorWhite);

   g_pUIEngine->setStrokeSize(1.0);
   g_pUIEngine->drawCircle( xCenter, yCenter, fRadius);

   int nGradationDelta = 5;
   if ( fRadius < 0.16 )
      nGradationDelta = 10;

   for( int i=0; i<360; i+= nGradationDelta )
   {
      float fAngle = 90 - i;
      float distInner = 0.85;

      float xMargin = xCenter + fRadius*0.95 * cos(fAngle*0.017453) / g_pUIEngine->getAspectRatio();
      float yMargin = yCenter - fRadius*0.95 * sin(fAngle*0.017453);

      float xinner = xCenter + fRadius * distInner * cos(fAngle*0.017453) / g_pUIEngine->getAspectRatio();
      float yinner = yCenter - fRadius * distInner * sin(fAngle*0.017453);

      g_pUIEngine->drawLine(xMargin,yMargin,xinner,yinner);
   }


   for( int i=0; i<360; i+= 5 )
   {
      float fAngle = 90 - i;
      float distText = 0.64;

      float xMargin = xCenter + fRadius*0.95 * cos(fAngle*0.017453) / g_pUIEngine->getAspectRatio();
      float yMargin = yCenter - fRadius*0.95 * sin(fAngle*0.017453);

      float xinner2 = xCenter + fRadius * 0.8 * cos(fAngle*0.017453) / g_pUIEngine->getAspectRatio();
      float yinner2 = yCenter - fRadius * 0.8 * sin(fAngle*0.017453);

      float xtext = xCenter + fRadius * distText * cos(fAngle*0.017453) / g_pUIEngine->getAspectRatio();
      float ytext = yCenter - fRadius * distText * sin(fAngle*0.017453);

      if ( (i%90) == 0 )
      {
         g_pUIEngine->setStroke(fColorRed);
         g_pUIEngine->setStrokeSize(fStrokeSize);
         g_pUIEngine->drawLine(xMargin,yMargin,xinner2,yinner2);

         if ( pCurrentSettings->nSettingsValues[0] == 0 )
            g_pUIEngine->setColors(g_pUIEngine->getColorOSDInstruments());
         else
            g_pUIEngine->setColors(fColorWhite);

         g_pUIEngine->setStrokeSize(1.0);
      }
      else if ( (i%45) == 0 )
      {
         if ( fRadius < 0.11 )
            g_pUIEngine->setStrokeSize(fStrokeSizeSmall);
         else
            g_pUIEngine->setStrokeSize(fStrokeSize);
         g_pUIEngine->drawLine(xMargin,yMargin,xinner2,yinner2);
         g_pUIEngine->setStrokeSize(1.0);
      }

      if ( (i % 45) == 0 )
      {
         if ( (i%90) == 0 )
         {
            if ( i == 0 )
               strcpy(szBuff, "N");
            if ( i == 90 )
               strcpy(szBuff, "E");
            if ( i == 270 )
               strcpy(szBuff, "W");
            if ( i == 180 )
               strcpy(szBuff, "S");
            float w = g_pUIEngine->textWidth(s_uFontIdCustomSizeLarger, szBuff);
            g_pUIEngine->drawText(xtext-0.5*w,ytext-0.5*fFontSizeLarger, s_uFontIdCustomSizeLarger, szBuff);
         }
         else
         {
            sprintf(szBuff, "%d", i);
            float w = g_pUIEngine->textWidth(s_uFontIdCustomSize, szBuff);
            g_pUIEngine->drawText(xtext-0.5*w,ytext-0.5*fFontSize, s_uFontIdCustomSize, szBuff);
         }
      }
   }

   // Show home

   if ( pCurrentSettings->nSettingsValues[1] == 1 )
   if ( pTelemetryInfo->isHomeSet )
   {
       static float heading_home = 0.0;

       // Received position is actual position, not home position
       if ( !(pTelemetryInfo->uFCFlags & FC_TELE_FLAGS_POS_HOME) )
       {
          heading_home = _heading_course_to(pTelemetryInfo->latitude/10000000.0f, pTelemetryInfo->longitude/10000000.0f, pTelemetryInfo->home_latitude/10000000.0f, pTelemetryInfo->home_longitude/10000000.0f);
          //if ( invert )
          //   heading_home = _heading_course_to(pTelemetryInfo->home_latitude/10000000.0f, pTelemetryInfo->home_longitude/10000000.0f, pTelemetryInfo->latitude/10000000.0f, pTelemetryInfo->longitude/10000000.0f);
       }

       float fAngle = 90.0 - heading_home;
       float distInner = 0.9;

       float xinner = xCenter + fRadius * distInner * cos(fAngle*0.017453) / g_pUIEngine->getAspectRatio();
       float yinner = yCenter - fRadius * distInner * sin(fAngle*0.017453);

       float xinner2 = xCenter + fRadius * distInner *0.4* cos((fAngle-180.0)*0.017453) / g_pUIEngine->getAspectRatio();
       float yinner2 = yCenter - fRadius * distInner *0.4* sin((fAngle-180.0)*0.017453);

       /*
       float xpt[3];
       float ypt[3];

       xpt[0] = xinner + fRadius * 0.1 * cos((180.0-fAngle)*0.017453) / g_pUIEngine->getAspectRatio();
       ypt[0] = yinner - fRadius * 0.1 * sin((180.0-fAngle)*0.017453);

       xpt[1] = xinner + fRadius * 0.1 * cos((fAngle+90.0)*0.017453) / g_pUIEngine->getAspectRatio();
       ypt[1] = yinner - fRadius * 0.1 * sin((fAngle+90.0)*0.017453);

       xpt[2] = xinner + fRadius * 0.1 * cos((fAngle-90.0)*0.017453) / g_pUIEngine->getAspectRatio();
       ypt[2] = yinner - fRadius * 0.1 * sin((fAngle-90.0)*0.017453);
       */

       g_pUIEngine->setStroke(fColorHome);
       g_pUIEngine->setFill(fColorHome[0], fColorHome[1], fColorHome[2], fColorHome[3]);
       if ( fRadius < 0.11 )
          g_pUIEngine->setStrokeSize(1.0);
       else
          g_pUIEngine->setStrokeSize(fStrokeSizeSmall);
       g_pUIEngine->drawLine(xCenter,yCenter,xinner,yinner);
       g_pUIEngine->drawLine(xCenter,yCenter,xinner2,yinner2);
       //g_pUIEngine->drawPolyLine(xpt, ypt, 3);
       //g_pUIEngine->fillPolygon(xpt, ypt, 3);
       g_pUIEngine->fillCircle(xinner, yinner, fRadius*0.05);

       if ( pCurrentSettings->nSettingsValues[0] == 0 )
          g_pUIEngine->setColors(g_pUIEngine->getColorOSDInstruments());
       else
          g_pUIEngine->setColors(fColorWhite);
   }


   // Show heading

   g_pUIEngine->setStroke(fColorYellow);
   g_pUIEngine->setFill(fColorYellow[0], fColorYellow[1], fColorYellow[2], fColorYellow[3]);
   g_pUIEngine->setStrokeSize(fStrokeSizeSmall);

   g_pUIEngine->fillCircle(xCenter, yCenter, 4.0*g_pUIEngine->getPixelHeight());

   float fHeadingAngle = 90.0-pTelemetryInfo->heading;
   float distMax = 0.8*fRadius;
   float distMin = 0.2*fRadius;
   float thickness = 0.18 * distMax;
   /*
   float xi = xCenter + fRadius * distMax * cos(fHeadingAngle*0.017453) / g_pUIEngine->getAspectRatio();
   float yi = yCenter - fRadius * distMax * sin(fHeadingAngle*0.017453);
   float xi2 = xCenter + fRadius * distMax*0.3 * cos((180.0-fHeadingAngle)*0.017453) / g_pUIEngine->getAspectRatio();
   float yi2 = yCenter + fRadius * distMax*0.3 * sin((180.0-fHeadingAngle)*0.017453);

   float xi3 = xCenter + fRadius * distMax*0.8 * cos((fHeadingAngle+8.0)*0.017453) / g_pUIEngine->getAspectRatio();
   float yi3 = yCenter - fRadius * distMax*0.8 * sin((fHeadingAngle+8.0)*0.017453);
   float xi4 = xCenter + fRadius * distMax*0.8 * cos((fHeadingAngle-8.0)*0.017453) / g_pUIEngine->getAspectRatio();
   float yi4 = yCenter - fRadius * distMax*0.8 * sin((fHeadingAngle-8.0)*0.017453);

   g_pUIEngine->drawLine(xCenter,yCenter,xi,yi);
   g_pUIEngine->drawLine(xCenter,yCenter,xi2,yi2);

   g_pUIEngine->drawLine(xi,yi, xi3, yi3);
   g_pUIEngine->drawLine(xi,yi, xi4, yi4);
   */

   float xpt[8];
   float ypt[8];
   float xopt[8];
   float yopt[8];

   xpt[0] = xCenter;
   ypt[0] = yCenter - distMax;

   xpt[1] = xpt[0] - thickness/g_pUIEngine->getAspectRatio();
   ypt[1] = ypt[0] + thickness;

   xpt[2] = xpt[1] + 0.6*thickness/g_pUIEngine->getAspectRatio();
   ypt[2] = ypt[1];

   xpt[3] = xpt[2];
   ypt[3] = ypt[2] + (distMax+distMin - thickness);

   xpt[4] = xpt[3] + 0.8*thickness/g_pUIEngine->getAspectRatio();
   ypt[4] = ypt[3];

   xpt[5] = xpt[4];
   ypt[5] = ypt[4] - (distMax+distMin - thickness);

   xpt[6] = xpt[5] + 0.6*thickness/g_pUIEngine->getAspectRatio();
   ypt[6] = ypt[5];

   xpt[7] = xpt[6] - thickness/g_pUIEngine->getAspectRatio();
   ypt[7] = ypt[6] - thickness;

   for( int i=0; i<8; i++ )
   {
       rotate_point(g_pUIEngine, xpt[i],ypt[i],xCenter, yCenter, fHeadingAngle+90, &xopt[i], &yopt[i]);
       yopt[i] = yCenter - (yopt[i] - yCenter);
   }
   g_pUIEngine->drawPolyLine(xopt, yopt, 7);

   if ( pCurrentSettings->nSettingsValues[0] == 0 )
      g_pUIEngine->setColors(g_pUIEngine->getColorOSDInstruments());
   else
      g_pUIEngine->setColors(fColorWhite);


   // Show wind

   if ( pCurrentSettings->nSettingsValues[3] == 1 )
   {
      g_pUIEngine->setStroke(fColorWhite);
      g_pUIEngine->setFill(fColorWhite[0], fColorWhite[1], fColorWhite[2], fColorWhite[3]);
      g_pUIEngine->setStrokeSize(fStrokeSize);

      float fWindAngle = 0.0;
      float fWindSpeed = 0.0;
      if ( NULL != pTelemetryInfo->pExtraInfo )
      {
         vehicle_and_telemetry_info2_t* pExtraInfo = (vehicle_and_telemetry_info2_t*)pTelemetryInfo->pExtraInfo;
         fWindAngle = (float) pExtraInfo->uWindHeading;
         // Convert from m/s to km/h
         fWindSpeed = pExtraInfo->fWindSpeed*3.6;
      }
      fWindAngle = 90.0-fWindAngle;
      //flip it
      fWindAngle += 180.0;
      if ( fWindAngle >= 360.0 )
         fWindAngle -= 360.0;

      int iWindSpeed10 = 1 + (int)(fWindSpeed/9.0);
      if ( iWindSpeed10 > 5 )
         iWindSpeed10 = 5;
      float distMargin = 0.74;
      float fSizeArrow = 0.045*fRadius;

      //float xmargin = xCenter + fRadius * distMargin * cos(fWindAngle*0.017453) / g_pUIEngine->getAspectRatio();
      //float ymargin = yCenter - fRadius * distMargin * sin(fWindAngle*0.017453);

      for( int i=0; i<iWindSpeed10; i++ )
      {
         float xc = xCenter + fRadius * (distMargin - 0.15*(float)i) * cos(fWindAngle*0.017453) / g_pUIEngine->getAspectRatio();
         float yc = yCenter - fRadius * (distMargin - 0.15*(float)i) * sin(fWindAngle*0.017453);

         float x1in = fSizeArrow;
         float y1in = -fSizeArrow*1.6;

         float x2in = -fSizeArrow;
         float y2in = -fSizeArrow*1.6;

         float x1,y1, x2, y2;
         rotate_point(g_pUIEngine, x1in, y1in, 0,0, -fWindAngle+90, &x1, &y1);
         rotate_point(g_pUIEngine, x2in, y2in, 0,0, -fWindAngle+90, &x2, &y2);

         g_pUIEngine->drawLine(xc,yc, xc+x1, yc+y1);
         g_pUIEngine->drawLine(xc,yc, xc+x2, yc+y2);
         //g_pUIEngine->fillCircle(xc, yc, 3.0*g_pUIEngine->getPixelHeight());
      }
   }

   if ( pCurrentSettings->nSettingsValues[0] == 0 )
      g_pUIEngine->setColors(g_pUIEngine->getColorOSDInstruments());
   else
      g_pUIEngine->setColors(fColorWhite);

   // Show home distance

   if ( pCurrentSettings->nSettingsValues[2] == 1 )
   {
       if ( pCurrentSettings->nSettingsValues[0] == 0 )
          g_pUIEngine->setColors(g_pUIEngine->getColorOSDInstruments());
       else
          g_pUIEngine->setColors(fColorWhite);

       float fDist = pTelemetryInfo->distance/100.0;
       if ( fDist > 1000000 || fDist < 0 )
          fDist = 0;
       u32 fontIdSmall = g_pUIEngine->getFontIdSmall();

       sprintf(szBuff, "Dist: %d m", (int)fDist);
       float w = g_pUIEngine->textWidth(fontIdSmall, szBuff);
       bool bOldValue = g_pUIEngine->drawBackgroundBoundingBoxes(true);
       g_pUIEngine->drawText(xCenter-0.5*w,yPos+fHeight, fontIdSmall, szBuff);
       g_pUIEngine->drawBackgroundBoundingBoxes(bOldValue);
   }

   g_pUIEngine->setColors(g_pUIEngine->getColorOSDInstruments());
}

#ifdef __cplusplus
}
#endif
