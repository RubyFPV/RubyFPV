// Licence: you can copy, edit, change or do whatever you wish with the code in this file

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "../public/render_engine_ui.h"
#include "../public/telemetry_info.h"
#include "../public/settings_info.h"
#include "../public/utils/osd_plugins_utils.h"

#include "ruby_plugins_common.cpp"

PLUGIN_VAR RenderEngineUI* g_pEngine = NULL;
PLUGIN_VAR const char* g_szPluginName = "Ruby Gauge Speed";
PLUGIN_VAR const char* g_szUID = "932AST-23432Q-WE30A-RGSpeed"; // This needs to be a unique number/id/string

PLUGIN_VAR const char* g_szOption1 = "Max Speed";
PLUGIN_VAR const char* g_szOption11 = "50";
PLUGIN_VAR const char* g_szOption12 = "100";
PLUGIN_VAR const char* g_szOption13 = "150";
PLUGIN_VAR const char* g_szOption14 = "200";
PLUGIN_VAR const char* g_szOption2 = "Show Throttle";

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
   return 2;
}

char* getPluginSettingName(int settingIndex)
{
   if ( 0 == settingIndex )
      return (char*)g_szOption1;
   if ( 1 == settingIndex )
      return (char*)g_szOption2;
   return NULL;
}

int getPluginSettingType(int settingIndex)
{
   if ( 0 == settingIndex )
      return PLUGIN_SETTING_TYPE_ENUM;
   if ( 1 == settingIndex )
      return PLUGIN_SETTING_TYPE_BOOL;

   return PLUGIN_SETTING_TYPE_INT;
}

int getPluginSettingMinValue(int settingIndex)
{
   return 0;
}

int getPluginSettingMaxValue(int settingIndex)
{
   return 200;
}

int getPluginSettingDefaultValue(int settingIndex)
{
   if ( 0 == settingIndex )
      return 1; // 100km/h
   return 1;
}

int getPluginSettingOptionsCount(int settingIndex)
{
   if ( 0 == settingIndex )
      return 4;
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
      if ( 2 == optionIndex )
         szRet = (char*)g_szOption13;
      if ( 3 == optionIndex )
         szRet = (char*)g_szOption14;
   }
   return szRet;
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

   float xCenter = xPos + 0.5*fWidth;
   float yCenter = yPos + 0.5*fHeight;
   float fRadius = fHeight/2.0;

   draw_shadow(g_pEngine, xCenter, yCenter, fRadius);

   g_pEngine->setColors(g_pEngine->getColorOSDInstruments());

   u32 fontId = g_pEngine->getFontIdRegular();
   float height_text = g_pEngine->textHeight(fontId);

   u32 fontIdSmall = g_pEngine->getFontIdSmall();
   float height_text_small = g_pEngine->textHeight(fontIdSmall);

   g_pEngine->setStroke(0,0,0,fBackgroundAlpha);
   g_pEngine->setFill(0,0,0,fBackgroundAlpha);
   g_pEngine->setStrokeSize(1.0);
   g_pEngine->fillCircle(xCenter, yCenter, fRadius);
   g_pEngine->setColors(g_pEngine->getColorOSDInstruments());
   g_pEngine->setStrokeSize(4.0);

   float fStartAngle = 240.0;
   float fEndAngle = -60.0;
   float fStepAngle = 20.0;
   int nMaxSpeed = 50;
   int ndSpeed = 10;

   if ( pCurrentSettings->nSettingsValues[0] == 1 )
      nMaxSpeed = 100;
   if ( pCurrentSettings->nSettingsValues[0] == 2 )
      nMaxSpeed = 150;
   if ( pCurrentSettings->nSettingsValues[0] == 3 )
      nMaxSpeed = 200;

   if ( pCurrentSettings->nSettingsValues[0] < 2 )
      ndSpeed = 5;

   int nGradations = nMaxSpeed/ndSpeed;

   fStepAngle = (fStartAngle - fEndAngle)/(float) nGradations;

   int nSpeed = 0;

   // Draw the gauge and gradations

   double colorRed[4] = {250,50,50,0.9};

   for( float fAngle = fStartAngle; fAngle>=fEndAngle-1.0; fAngle -= fStepAngle )
   {
      float x = xCenter + fRadius*0.95 * cos(fAngle*0.017453) / g_pEngine->getAspectRatio();
      float y = yCenter - fRadius*0.95 * sin(fAngle*0.017453);
      float dInner = 0.84;
      float dText = 0.68;
      if ( (nSpeed%10) )
      {
         dInner = 0.9;
      }
      float xinner = xCenter + fRadius * dInner * cos(fAngle*0.017453) / g_pEngine->getAspectRatio();
      float yinner = yCenter - fRadius * dInner * sin(fAngle*0.017453);

      float xtext = xCenter + fRadius * dText * cos(fAngle*0.017453) / g_pEngine->getAspectRatio();
      float ytext = yCenter - fRadius * dText * sin(fAngle*0.017453);

      g_pEngine->drawLine(x,y,xinner,yinner);
      if ( nSpeed > nMaxSpeed*0.75 )
      {
         g_pEngine->setStroke(colorRed, 4.0);
         g_pEngine->drawLine(x,y,xinner,yinner);
         g_pEngine->setColors(g_pEngine->getColorOSDInstruments());
      }

      if ( (nSpeed%10) == 0 )
      {
         sprintf(szBuff, "%d", nSpeed);
         float w = g_pEngine->textWidth(fontIdSmall, szBuff);
         g_pEngine->drawText(xtext-0.5*w,ytext-0.5*height_text_small, fontIdSmall, szBuff);
      }
      nSpeed += ndSpeed;
   }

   // Show speed value

   float hspeed = ((float)pTelemetryInfo->hspeed)/100.0 - 1000.0;
   hspeed *= 3.6; // from m/s to km/h

   if ( NULL != pCurrentSettings->pExtraInfo )
   {
      plugin_settings_info_t2_extra* pExtraInfo2 = (plugin_settings_info_t2_extra*) pCurrentSettings->pExtraInfo;
      hspeed = plugin_osd_convertKm(hspeed, pExtraInfo2->iMeasureUnitsType);
   }
   // Make it move smoothly
   static float s_fPluginSpeedSmooth = 0.0;
   s_fPluginSpeedSmooth = 0.8*s_fPluginSpeedSmooth + 0.2 * hspeed;

   sprintf(szBuff, "%d km/h", (int)s_fPluginSpeedSmooth);

   if ( NULL != pCurrentSettings->pExtraInfo )
   {
      plugin_settings_info_t2_extra* pExtraInfo2 = (plugin_settings_info_t2_extra*) pCurrentSettings->pExtraInfo;

      if ( pExtraInfo2->iMeasureUnitsType == 1 || pExtraInfo2->iMeasureUnitsType == 3 )
         sprintf(szBuff, "%d mi/h", (int) s_fPluginSpeedSmooth);
   }
   g_pEngine->setStrokeSize(pCurrentSettings->fOutlineThicknessPx);
   // Use a smaller font is the plugin size is small
   if ( fHeight < 0.3 )
      show_value_centered(g_pEngine, xCenter,yCenter-fRadius*0.12-height_text_small, szBuff, fontIdSmall);
   else
      show_value_centered(g_pEngine, xCenter,yCenter-fRadius*0.2-height_text, szBuff, fontId);

   // Show throttle value

   if ( 1 == pCurrentSettings->nSettingsValues[1] )
   {
      sprintf(szBuff, "TH: %d%%", pTelemetryInfo->throttle);
      g_pEngine->setStrokeSize(pCurrentSettings->fOutlineThicknessPx);
      // Use a smaller font is the plugin size is small
      if ( fHeight < 0.3 )
         show_value_centered(g_pEngine, xCenter,yCenter+fRadius*0.05, szBuff, fontIdSmall);
      else
         show_value_centered(g_pEngine, xCenter,yCenter+fRadius*0.15, szBuff, fontId);
   }

   // Draw needle and center

   if ( s_fPluginSpeedSmooth > nMaxSpeed )
      s_fPluginSpeedSmooth = nMaxSpeed;

   float fNeedleAngle = fStartAngle - (fStartAngle - fEndAngle) * s_fPluginSpeedSmooth / (float) nMaxSpeed;
   float fNeedleAngleLeft = fNeedleAngle + 90.0;
   float fNeedleAngleRight = fNeedleAngle - 90.0;
   float fNeedleAngleBottom = fNeedleAngle + 180.0;

   float xneedle[4];
   float yneedle[4];

   xneedle[0] = xCenter + fRadius* 0.8 * cos(fNeedleAngle*0.017453) / g_pEngine->getAspectRatio();
   yneedle[0] = yCenter - fRadius* 0.8 * sin(fNeedleAngle*0.017453);

   xneedle[1] = xCenter + fRadius* 0.05 * cos(fNeedleAngleLeft*0.017453) / g_pEngine->getAspectRatio();
   yneedle[1] = yCenter - fRadius* 0.05 * sin(fNeedleAngleLeft*0.017453);

   xneedle[2] = xCenter + fRadius* 0.3 * cos(fNeedleAngleBottom*0.017453) / g_pEngine->getAspectRatio();
   yneedle[2] = yCenter - fRadius* 0.3 * sin(fNeedleAngleBottom*0.017453);

   xneedle[3] = xCenter + fRadius* 0.05 * cos(fNeedleAngleRight*0.017453) / g_pEngine->getAspectRatio();
   yneedle[3] = yCenter - fRadius* 0.05 * sin(fNeedleAngleRight*0.017453);

   double color[4];
   memcpy(color, g_pEngine->getColorOSDInstruments(), 4*sizeof(double));
   color[3] = 0.9;


   g_pEngine->setStrokeSize(3.0);
   g_pEngine->setColors(g_pEngine->getColorOSDOutline());
   g_pEngine->drawPolyLine(xneedle, yneedle,4);
   g_pEngine->drawCircle(xCenter, yCenter, fRadius*0.1);
   g_pEngine->drawCircle(xCenter, yCenter+g_pEngine->getPixelHeight(), fRadius*0.1);

   g_pEngine->setColors(color);
   g_pEngine->setStrokeSize(0.0);
   g_pEngine->fillPolygon(xneedle, yneedle,4);
   g_pEngine->fillCircle(xCenter, yCenter, fRadius*0.12);

   g_pEngine->setColors(g_pEngine->getColorOSDInstruments());
}

#ifdef __cplusplus
}
#endif
