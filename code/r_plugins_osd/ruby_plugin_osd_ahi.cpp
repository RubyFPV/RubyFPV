// Written by Petru Soroaga
// Licence: you can copy, edit, change or do whatever you wish with this code

#include <stdio.h>
#include <math.h>

#include "../public/render_engine_ui.h"
#include "../public/telemetry_info.h"
#include "../public/settings_info.h"
#include "../public/utils/core_plugins_utils.h"

#include "ruby_plugins_common.cpp"

PLUGIN_VAR RenderEngineUI* g_pEngine = NULL;
PLUGIN_VAR const char* g_szPluginName = "AHI Ruby Plugin";
PLUGIN_VAR const char* g_szUID = "89930-2454352Q-ASDGF9J-RAHI";

PLUGIN_VAR const char* g_szOption1 = "Show Gradations";
PLUGIN_VAR const char* g_szOption2 = "Gradations Spacing";
PLUGIN_VAR const char* g_szOption21 = "5";
PLUGIN_VAR const char* g_szOption22 = "10";
PLUGIN_VAR const char* g_szOption23 = "15";
PLUGIN_VAR const char* g_szOption24 = "20";

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
      return PLUGIN_SETTING_TYPE_BOOL;
   if ( 1 == settingIndex )
      return PLUGIN_SETTING_TYPE_ENUM;

   return PLUGIN_SETTING_TYPE_INT;
}

int getPluginSettingMinValue(int settingIndex)
{
   return 0;
}

int getPluginSettingMaxValue(int settingIndex)
{
   return 10;
}

int getPluginSettingDefaultValue(int settingIndex)
{
   if ( 0 == settingIndex )
      return 1;
   return 0;
}

int getPluginSettingOptionsCount(int settingIndex)
{
   if ( 1 == settingIndex )
      return 4;
   return 0;
}

char* getPluginSettingOptionName(int settingIndex, int optionIndex)
{
   char* szRet = NULL;
   if ( 1 == settingIndex )
   {
      if ( 0 == optionIndex )
         szRet = (char*)g_szOption21;
      if ( 1 == optionIndex )
         szRet = (char*)g_szOption22;
      if ( 2 == optionIndex )
         szRet = (char*)g_szOption23;
      if ( 3 == optionIndex )
         szRet = (char*)g_szOption24;
   }
   return szRet;
}

float getDefaultWidth()
{
   return 0.3/g_pEngine->getAspectRatio();
}

float getDefaultHeight()
{
   return 0.3;
}

void render(vehicle_and_telemetry_info_t* pTelemetryInfo, plugin_settings_info_t2* pCurrentSettings, float xPos, float yPos, float fWidth, float fHeight)
{
   if ( NULL == g_pEngine || NULL == pTelemetryInfo )
      return;

   //bool bUseWarningColor = false;
   //if ( pTelemetryInfo->ahi_warning_angle > 1 )
   //if ( fabs(pTelemetryInfo->roll) >= pTelemetryInfo->ahi_warning_angle ||
   //     fabs(pTelemetryInfo->pitch) >= pTelemetryInfo->ahi_warning_angle )
   //   bUseWarningColor = true;

   char szBuff[64];

   float xCenter = xPos + 0.5*fWidth;
   float yCenter = yPos + 0.5*fHeight;
   float planeWidth = 0.2*fWidth;

   float xt[3] = {(float)(xCenter-planeWidth*0.2), (float)(xCenter+planeWidth*0.2), xCenter };
   float yt[3] = {(float)(yCenter+planeWidth*0.5), (float)(yCenter+planeWidth*0.5), (float)(yCenter+planeWidth*0.1)};

   g_pEngine->setColors(g_pEngine->getColorOSDOutline());
   g_pEngine->setStrokeSize(pCurrentSettings->fOutlineThicknessPx);
   g_pEngine->drawPolyLine(xt, yt, 3);
   g_pEngine->fillPolygon(xt, yt, 3);

   g_pEngine->setColors(g_pEngine->getColorOSDInstruments());
   if ( pTelemetryInfo->ahi_warning_angle > 1 )
   if ( fabs(pTelemetryInfo->roll) >= pTelemetryInfo->ahi_warning_angle )
      g_pEngine->setColors(g_pEngine->getColorOSDWarning());

   g_pEngine->setStrokeSize(0.0);
   g_pEngine->drawPolyLine(xt, yt, 3);
   g_pEngine->fillPolygon(xt, yt, 3);

   u32 fontId = g_pEngine->getFontIdRegular();
   float height_text = g_pEngine->textHeight(fontId);
   float width_text = g_pEngine->textWidth(fontId, "00");

   float dy = height_text*0.5;
   float dx = width_text;

   float height_ladder = (fHeight-2.0*dy);
   float width_ladder = height_ladder*0.5-2.0*dx;
   if ( height_ladder < 0.02 )
      height_ladder = 0.02;
   if ( width_ladder < 0.02 )
      width_ladder = 0.02;

   float range = 45;
   float space_text = 0.006;
   float fRatioH = height_ladder / range;

   int roll = -(pTelemetryInfo->roll);
   int pitch = pTelemetryInfo->pitch;

   if ( (fabs(roll) >= 179) && (fabs(pitch) >= 179) )
   {
      roll = 0;
      pitch = 0;
   }

   int iAnglePitch = pitch - range/2;
   int iAnglePitchMax = pitch + range/2;

   sprintf(szBuff, "%d", roll);

   g_pEngine->setColors(g_pEngine->getColorOSDOutline());
   g_pEngine->setStrokeSize(pCurrentSettings->fOutlineThicknessPx);
   show_value_centered(g_pEngine, xCenter,yCenter+planeWidth*0.7, szBuff, fontId);
   //g_pEngine->drawLine(0.2, 0.2, 0.7,0.7);
   g_pEngine->setColors(g_pEngine->getColorOSDInstruments());
   g_pEngine->setStrokeSize(0.0);
   show_value_centered(g_pEngine, xCenter,yCenter+planeWidth*0.7, szBuff, fontId);
   //g_pEngine->drawLine(0.2, 0.7, 0.7,0.2);

   while (iAnglePitch <= iAnglePitchMax)
   {
      float y = yCenter - (float)(iAnglePitch - pitch) * fRatioH;
      if (iAnglePitch == 0)
      {
        float xl, xr;
        float xt, yt;

        sprintf(szBuff, "0");
        g_pEngine->setColors(g_pEngine->getColorOSDOutline());
        g_pEngine->setStrokeSize(pCurrentSettings->fOutlineThicknessPx);
        xl = xCenter - width_ladder*0.5 - space_text;
        xr = xCenter + width_ladder*0.5 + space_text;
        rotate_point(g_pEngine, xl,y,xCenter, yCenter, roll, &xt, &yt);
        g_pEngine->drawTextLeft(xt, yt - height_text*0.5, fontId, const_cast<char*>(szBuff));
        rotate_point(g_pEngine, xr,y,xCenter, yCenter, roll, &xt, &yt);
        g_pEngine->drawText(xt, yt - height_text*0.5, fontId, const_cast<char*>(szBuff));

        g_pEngine->setColors(g_pEngine->getColorOSDInstruments());
        g_pEngine->setStrokeSize(0.0);
        rotate_point(g_pEngine, xl,y,xCenter, yCenter, roll, &xt, &yt);
        g_pEngine->drawTextLeft(xt, yt - height_text*0.5, fontId, const_cast<char*>(szBuff));
        rotate_point(g_pEngine, xr,y,xCenter, yCenter, roll, &xt, &yt);
        g_pEngine->drawText(xt, yt - height_text*0.5, fontId, const_cast<char*>(szBuff));

        xl = xCenter - width_ladder*0.5;
        xr = xCenter + width_ladder*0.5;
        float xtl, ytl;
        float xtr, ytr;

        if ( (NULL != pCurrentSettings) && (pCurrentSettings->nSettingsValues[0] != 0) )
        {
           rotate_point(g_pEngine, xl,y,xCenter, yCenter, roll, &xtl, &ytl);
           rotate_point(g_pEngine, xr,y,xCenter, yCenter, roll, &xtr, &ytr);
           g_pEngine->setColors(g_pEngine->getColorOSDOutline());
           g_pEngine->setStrokeSize(pCurrentSettings->fOutlineThicknessPx);
           g_pEngine->drawLine(xtl, ytl, xtr, ytr);

           g_pEngine->setColors(g_pEngine->getColorOSDInstruments());
           g_pEngine->setStrokeSize(pCurrentSettings->fLineThicknessPx);
           g_pEngine->drawLine(xtl, ytl, xtr, ytr);
        }
        iAnglePitch++;
        continue;
      }

      if ( (iAnglePitch % 5) != 0 )
      {
         iAnglePitch++;
         continue;
      }

      if ( (NULL != pCurrentSettings) && (pCurrentSettings->nSettingsValues[1] == 1) )
      if ( (iAnglePitch % 10) != 0 )
      {
         iAnglePitch++;
         continue;
      }

      if ( (NULL != pCurrentSettings) && (pCurrentSettings->nSettingsValues[1] == 2) )
      if ( (iAnglePitch % 15) != 0 )
      {
         iAnglePitch++;
         continue;
      }

      if ( (NULL != pCurrentSettings) && (pCurrentSettings->nSettingsValues[1] == 3) )
      if ( (iAnglePitch % 20) != 0 )
      {
         iAnglePitch++;
         continue;
      }

      float xl = xCenter - width_ladder *0.4 - space_text;
      float xr = xCenter + width_ladder *0.4 + space_text;
      float xtl, ytl;
      float xtr, ytr;
      rotate_point(g_pEngine, xl,y,xCenter, yCenter, roll, &xtl, &ytl);
      rotate_point(g_pEngine, xr,y,xCenter, yCenter, roll, &xtr, &ytr);

      sprintf(szBuff, "%d", iAnglePitch);

      g_pEngine->setColors(g_pEngine->getColorOSDOutline());
      g_pEngine->setStrokeSize(pCurrentSettings->fOutlineThicknessPx);
      g_pEngine->drawTextLeft(xtl, ytl-0.5*height_text, fontId, const_cast<char*>(szBuff));
      g_pEngine->drawText(xtr, ytr-0.5*height_text, fontId, const_cast<char*>(szBuff));

      g_pEngine->setColors(g_pEngine->getColorOSDInstruments());
      if ( pTelemetryInfo->ahi_warning_angle > 1 )
      if ( fabs(iAnglePitch) >= pTelemetryInfo->ahi_warning_angle )
         g_pEngine->setColors(g_pEngine->getColorOSDWarning());
      g_pEngine->setStrokeSize(0.0);

      g_pEngine->drawTextLeft(xtl, ytl-0.5*height_text, fontId, const_cast<char*>(szBuff));
      g_pEngine->drawText(xtr, ytr-0.5*height_text, fontId, const_cast<char*>(szBuff));

      if ( (NULL != pCurrentSettings) && (pCurrentSettings->nSettingsValues[0] != 0) )
      {
         xl = xCenter - width_ladder * 0.4;
         xr = xCenter + width_ladder * 0.4;
         float xl2 = xCenter - width_ladder * 0.4 + width_ladder*0.2;
         float xr2 = xCenter + width_ladder * 0.4 - width_ladder*0.2;
         float xtl2, ytl2;
         float xtr2, ytr2;
         rotate_point(g_pEngine, xl,y,xCenter, yCenter, roll, &xtl, &ytl);
         rotate_point(g_pEngine, xr,y,xCenter, yCenter, roll, &xtr, &ytr);

         rotate_point(g_pEngine, xl2,y,xCenter, yCenter, roll, &xtl2, &ytl2);
         rotate_point(g_pEngine, xr2,y,xCenter, yCenter, roll, &xtr2, &ytr2);

         if (iAnglePitch > 0)
         {
            g_pEngine->setColors(g_pEngine->getColorOSDInstruments());
            if ( pTelemetryInfo->ahi_warning_angle > 1 )
            if ( fabs(iAnglePitch) >= pTelemetryInfo->ahi_warning_angle )
               g_pEngine->setColors(g_pEngine->getColorOSDWarning());
            g_pEngine->setStrokeSize(pCurrentSettings->fLineThicknessPx);

            g_pEngine->drawLine(xtl, ytl, xtl2, ytl2);
            g_pEngine->drawLine(xtr2, ytr2, xtr, ytr);
         }
         if (iAnglePitch < 0)
         {
            g_pEngine->setColors(g_pEngine->getColorOSDInstruments());
            if ( pTelemetryInfo->ahi_warning_angle > 1 )
            if ( fabs(iAnglePitch) >= pTelemetryInfo->ahi_warning_angle )
               g_pEngine->setColors(g_pEngine->getColorOSDWarning());
            g_pEngine->setStrokeSize(pCurrentSettings->fLineThicknessPx);
            g_pEngine->drawLine(xtl, ytl, xtl2, ytl2);
            g_pEngine->drawLine(xtr2, ytr2, xtr, ytr);
         }
      }
      iAnglePitch++;
   }
}

#ifdef __cplusplus
}
#endif
