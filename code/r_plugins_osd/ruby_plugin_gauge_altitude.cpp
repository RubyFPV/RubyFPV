// Licence: you can copy, edit, change or do whatever you wish with the code in this file

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "../public/render_engine_ui.h"
#include "../public/telemetry_info.h"
#include "../public/settings_info.h"
#include "../public/utils/core_plugins_utils.h"

#include "ruby_plugins_common.cpp"

PLUGIN_VAR RenderEngineUI* g_pEngine = NULL;
PLUGIN_VAR const char* g_szPluginName = "Ruby Gauge Altitude";
PLUGIN_VAR const char* g_szUID = "782AST-23432Q-WE31A-RGAlt"; // This needs to be a unique number/id/string

PLUGIN_VAR const char* g_szOption1 = "Show Altitude";
PLUGIN_VAR const char* g_szOption2 = "Show Vertical Speed";

#ifdef __cplusplus
extern "C" {
#endif


void init(void* pEngine)
{
  char szBuff[256];
  sprintf(szBuff,"Plugin altitude Init., engine: 0x%p", pEngine );
  core_plugin_util_log_line(szBuff);
  g_pEngine = (RenderEngineUI*)pEngine;
  sprintf(szBuff,"Plugin altitude Init., engine new: 0x%p", g_pEngine );
  core_plugin_util_log_line(szBuff);
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
   if ( 0 == settingIndex || 1 == settingIndex )
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
  char szBuff[256];
  sprintf(szBuff,"Plugin altitude w, engine: 0x%p", g_pEngine );
  core_plugin_util_log_line(szBuff);
   if ( NULL == g_pEngine )
      return 0.28;
   return 0.28/g_pEngine->getAspectRatio();
}

float getDefaultHeight()
{
  char szBuff[256];
  sprintf(szBuff,"Plugin altitude h, engine: 0x%p", g_pEngine );
  core_plugin_util_log_line(szBuff);
   if ( NULL == g_pEngine )
      return 0.28;
   return 0.28;
}

void _render_vspeed_only(vehicle_and_telemetry_info_t* pTelemetryInfo, plugin_settings_info_t2* pCurrentSettings, float xPos, float yPos, float fWidth, float fHeight)
{
   char szBuff[64];
   double color[4];

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

   int nMaxVSpeed = 15;
   int nVSpeed = 0;

   float fStartAngle = 180.0;
   float fEndAngle = -180.0;
   float fStepAngle = -360.0/(float)nMaxVSpeed/2.0;

   // Draw the gauge and gradations for vertical speed

   for( float fAngle = fStartAngle; fAngle>fEndAngle+1.0; fAngle += fStepAngle )
   {
      float x = xCenter + fRadius*0.93 * cos(fAngle*0.017453) / g_pEngine->getAspectRatio();
      float y = yCenter - fRadius*0.93 * sin(fAngle*0.017453);
      float dInner = 0.84;
      float dText = 0.68;

      if ( (nVSpeed%2) )
      {
         dInner = 0.9;
      }

      float xinner = xCenter + fRadius * dInner * cos(fAngle*0.017453) / g_pEngine->getAspectRatio();
      float yinner = yCenter - fRadius * dInner * sin(fAngle*0.017453);

      float xtext = xCenter + fRadius * dText * cos(fAngle*0.017453) / g_pEngine->getAspectRatio();
      float ytext = yCenter - fRadius * dText * sin(fAngle*0.017453);

      g_pEngine->drawLine(x,y,xinner,yinner);

      if ( (nVSpeed%5) == 0 )
      {
         if ( nVSpeed == nMaxVSpeed )
            sprintf(szBuff, "+/-%d", nVSpeed);
         else if ( nVSpeed > nMaxVSpeed )
            sprintf(szBuff, "%d", - nMaxVSpeed - (nMaxVSpeed-nVSpeed));
         else
            sprintf(szBuff, "%d", nVSpeed);
         if ( fHeight > 0.2 )
         {
            float w = g_pEngine->textWidth(fontId, szBuff);
            g_pEngine->drawText(xtext-0.5*w,ytext-0.5*height_text, fontId, szBuff);
         }
         else
         {
            float w = g_pEngine->textWidth(fontIdSmall, szBuff);
            g_pEngine->drawText(xtext-0.5*w,ytext-0.5*height_text_small, fontIdSmall, szBuff);
         }
      }
      nVSpeed++;
   }

   // Show vspeed value

   float fVSpeed = ((float)pTelemetryInfo->vspeed)/100.0 - 1000.0;

   // Make it move smoothly

   static float s_fPluginVSpeedSmooth = 0.0;
   s_fPluginVSpeedSmooth = 0.6*s_fPluginVSpeedSmooth + 0.4 * fVSpeed;

   sprintf(szBuff, "%1.f m/s", s_fPluginVSpeedSmooth);

   g_pEngine->setStrokeSize(pCurrentSettings->fOutlineThicknessPx);

   show_value_centered(g_pEngine, xCenter,yCenter-fRadius*0.14-height_text_small, "V-SPEED", fontIdSmall);

   float wText = g_pEngine->textWidth(fontId, szBuff);

   // Use a smaller font is the plugin size is small
   if ( fHeight < 0.36 )
   {
      show_value_centered(g_pEngine, xCenter,yCenter+fRadius*0.12, szBuff, fontIdSmall);
      wText = g_pEngine->textWidth(fontIdSmall, szBuff);
   }
   else
      show_value_centered(g_pEngine, xCenter,yCenter+fRadius*0.18, szBuff, fontId);

   g_pEngine->setStrokeSize(1.0);
   g_pEngine->setFill(0,0,0,0.1);
   g_pEngine->setStroke(0,0,0,0.7);
   g_pEngine->setStrokeSize(1.0);

   if ( fHeight < 0.36 )
      g_pEngine->drawRoundRect(xCenter-wText*0.6, yCenter+fRadius*0.12, wText*1.2, height_text_small, 0.05);
   else
      g_pEngine->drawRoundRect(xCenter-wText*0.6, yCenter+fRadius*0.18, wText*1.2, height_text, 0.05);

   // Draw center

   g_pEngine->setStrokeSize(1.0);
   g_pEngine->setFill(0,0,0,0);
   g_pEngine->setStroke(0,0,0,0.7);
   g_pEngine->setStrokeSize(1.0);
   g_pEngine->drawCircle(xCenter, yCenter, g_pEngine->getPixelHeight()*12.0);

   g_pEngine->setColors(g_pEngine->getColorOSDInstruments());

   // Draw needle

   float fValue = s_fPluginVSpeedSmooth;
   if ( fValue > nMaxVSpeed-0.5 )
      fValue = nMaxVSpeed-0.5;
   if ( fValue < -nMaxVSpeed+0.5 )
      fValue = -nMaxVSpeed+0.5;

   float fNeedleAngle = 0.0;
   if ( fValue >= 0.0 )
      fNeedleAngle = fStartAngle - 180.0 * fValue / (float) nMaxVSpeed;
   else
      fNeedleAngle = - 180.0 - 180.0 * fValue / (float) nMaxVSpeed;

   float fNeedleAngleLeft = fNeedleAngle + 90.0;
   float fNeedleAngleRight = fNeedleAngle - 90.0;
   float fNeedleAngleBottom = fNeedleAngle + 180.0;

   float xneedle[4];
   float yneedle[4];

   xneedle[0] = xCenter + fRadius* 0.7 * cos(fNeedleAngle*0.017453) / g_pEngine->getAspectRatio();
   yneedle[0] = yCenter - fRadius* 0.7 * sin(fNeedleAngle*0.017453);

   xneedle[1] = xCenter + fRadius* 0.05 * cos(fNeedleAngleLeft*0.017453) / g_pEngine->getAspectRatio();
   yneedle[1] = yCenter - fRadius* 0.05 * sin(fNeedleAngleLeft*0.017453);

   xneedle[2] = xCenter + fRadius* 0.2 * cos(fNeedleAngleBottom*0.017453) / g_pEngine->getAspectRatio();
   yneedle[2] = yCenter - fRadius* 0.2 * sin(fNeedleAngleBottom*0.017453);

   xneedle[3] = xCenter + fRadius* 0.05 * cos(fNeedleAngleRight*0.017453) / g_pEngine->getAspectRatio();
   yneedle[3] = yCenter - fRadius* 0.05 * sin(fNeedleAngleRight*0.017453);

   memcpy(color, g_pEngine->getColorOSDInstruments(), 4*sizeof(double));
   color[3] = 0.9;
   if ( fValue < 0.0 )
   {
      color[0] = 250; color[1] = 50; color[2] = 50; color[3] = 0.7;
   }
   g_pEngine->setColors(color);
   //g_pEngine->setStroke(0,0,0,0.8);
   g_pEngine->setStrokeSize(0.0);
   g_pEngine->fillPolygon(xneedle, yneedle,4);

   g_pEngine->setFill(0,0,0,0.01);
   g_pEngine->setStroke(0,0,0,0.8);
   g_pEngine->setStrokeSize(1.0);
   g_pEngine->drawPolyLine(xneedle, yneedle,4);

   // Draw center

   g_pEngine->setFill(255,255,255,0.8);
   g_pEngine->setStroke(0,0,0,0.8);
   g_pEngine->setStrokeSize(1.0);
   g_pEngine->drawCircle(xCenter, yCenter, g_pEngine->getPixelHeight()*6.0);
   g_pEngine->fillCircle(xCenter, yCenter, g_pEngine->getPixelHeight()*2.0);

   g_pEngine->setColors(g_pEngine->getColorOSDInstruments());
}

void render(vehicle_and_telemetry_info_t* pTelemetryInfo, plugin_settings_info_t2* pCurrentSettings, float xPos, float yPos, float fWidth, float fHeight)
{
   if ( NULL == g_pEngine || NULL == pTelemetryInfo || NULL == pCurrentSettings )
      return;

   if ( 0 == pCurrentSettings->nSettingsValues[0] )
   if ( 1 == pCurrentSettings->nSettingsValues[1] )
   {
      _render_vspeed_only(pTelemetryInfo, pCurrentSettings, xPos, yPos, fWidth, fHeight);
      return;
   }

   char szBuff[64];
   double color[4];

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


   float fRadiusAltitude = fRadius;
   if ( 1 == pCurrentSettings->nSettingsValues[1] )
      fRadiusAltitude -= fRadius*0.18;

   float fStartAngle = 90.0;
   float fEndAngle = -270.0;
   float fStepAngle = -18.0;

   // Draw the gauge and gradations for altitude

   if ( 1 == pCurrentSettings->nSettingsValues[1] )
   {
      double color[4];
      memcpy(color, g_pEngine->getColorOSDInstruments(), 4*sizeof(double));
      color[3] = 0.7;
      g_pEngine->setColors(color);
      g_pEngine->setStrokeSize(2.0);
      g_pEngine->drawCircle(xCenter, yCenter, fRadiusAltitude);
   }

   g_pEngine->setColors(g_pEngine->getColorOSDInstruments());
   g_pEngine->setStrokeSize(4.0);

   int value = 0;

   for( float fAngle = fStartAngle; fAngle>fEndAngle+1.0; fAngle += fStepAngle )
   {
      float x = xCenter + fRadiusAltitude*0.95 * cos(fAngle*0.017453) / g_pEngine->getAspectRatio();
      float y = yCenter - fRadiusAltitude*0.95 * sin(fAngle*0.017453);
      float dInner = 0.84;
      float dText = 0.68;

      if ( (value%2) )
      {
         dInner = 0.9;
      }

      float xinner = xCenter + fRadiusAltitude * dInner * cos(fAngle*0.017453) / g_pEngine->getAspectRatio();
      float yinner = yCenter - fRadiusAltitude * dInner * sin(fAngle*0.017453);

      float xtext = xCenter + fRadiusAltitude * dText * cos(fAngle*0.017453) / g_pEngine->getAspectRatio();
      float ytext = yCenter - fRadiusAltitude * dText * sin(fAngle*0.017453);

      g_pEngine->drawLine(x,y,xinner,yinner);

      if ( (value%2) == 0 )
      {
         sprintf(szBuff, "%d", value/2);
         if ( fRadiusAltitude > 0.1 )
         {
            float w = g_pEngine->textWidth(fontId, szBuff);
            g_pEngine->drawText(xtext-0.5*w,ytext-0.5*height_text, fontId, szBuff);
         }
         else
         {
            float w = g_pEngine->textWidth(fontIdSmall, szBuff);
            g_pEngine->drawText(xtext-0.5*w,ytext-0.5*height_text_small, fontIdSmall, szBuff);
         }
      }
      value++;
   }

   // Show altitude value

   float fAltitude = ((float)pTelemetryInfo->altitude)/100.0 - 1000.0;

   // Make it move smoothly

   static float s_fPluginAltitudeSmooth = 0.0;
   s_fPluginAltitudeSmooth = 0.8*s_fPluginAltitudeSmooth + 0.2 * fAltitude;

   if ( fabs(s_fPluginAltitudeSmooth) < 10.0 )
      sprintf(szBuff, "%1.f m", s_fPluginAltitudeSmooth);
   else
      sprintf(szBuff, "%d m", (int)s_fPluginAltitudeSmooth);

   g_pEngine->setStrokeSize(pCurrentSettings->fOutlineThicknessPx);

   show_value_centered(g_pEngine, xCenter,yCenter-fRadiusAltitude*0.15-height_text_small, "ALT", fontIdSmall);

   float wText = g_pEngine->textWidth(fontId, szBuff);

   // Use a smaller font is the plugin size is small
   if ( fRadiusAltitude < 0.15 )
   {
      show_value_centered(g_pEngine, xCenter,yCenter+fRadiusAltitude*0.14, szBuff, fontIdSmall);
      wText = g_pEngine->textWidth(fontIdSmall, szBuff);
   }
   else
      show_value_centered(g_pEngine, xCenter,yCenter+fRadiusAltitude*0.18, szBuff, fontId);

   g_pEngine->setStrokeSize(1.0);
   g_pEngine->setFill(0,0,0,0.1);
   g_pEngine->setStroke(0,0,0,0.7);
   g_pEngine->setStrokeSize(1.0);

   if ( fRadiusAltitude < 0.15 )
      g_pEngine->drawRoundRect(xCenter-wText*0.6, yCenter+fRadiusAltitude*0.14, wText*1.2, height_text_small, 0.05);
   else
      g_pEngine->drawRoundRect(xCenter-wText*0.6, yCenter+fRadiusAltitude*0.18, wText*1.2, height_text, 0.05);

   // Draw center

   g_pEngine->setStrokeSize(1.0);
   g_pEngine->setFill(0,0,0,0);
   g_pEngine->setStroke(0,0,0,0.7);
   g_pEngine->setStrokeSize(1.0);
   g_pEngine->drawCircle(xCenter, yCenter, g_pEngine->getPixelHeight()*12.0);

   g_pEngine->setColors(g_pEngine->getColorOSDInstruments());

   // Draw needle (10 meters value)

   float fNormValue = s_fPluginAltitudeSmooth / 10.0 / 10.0;
   float fNeedleAngle = fStartAngle - (fStartAngle - fEndAngle) * fNormValue;
   float fNeedleAngleLeft = fNeedleAngle + 90.0;
   float fNeedleAngleRight = fNeedleAngle - 90.0;
   float fNeedleAngleBottom = fNeedleAngle + 180.0;

   float xneedle[5];
   float yneedle[5];
   float fLengthNeedle = fRadiusAltitude - height_text_small;
   if ( fRadiusAltitude > 0.1 )
      fLengthNeedle = fRadiusAltitude - height_text;

   xneedle[0] = fLengthNeedle * 0.84 * cos(fNeedleAngle*0.017453);
   yneedle[0] = fLengthNeedle * 0.84 * sin(fNeedleAngle*0.017453);

   xneedle[1] = fLengthNeedle * 0.6 * cos(fNeedleAngle*0.017453) + fLengthNeedle * 0.05 * cos(fNeedleAngleLeft*0.017453);
   yneedle[1] = fLengthNeedle * 0.6 * sin(fNeedleAngle*0.017453) + fLengthNeedle * 0.05 * sin(fNeedleAngleLeft*0.017453);

   xneedle[2] = fLengthNeedle * 0.05 * cos(fNeedleAngleLeft*0.017453);
   yneedle[2] = fLengthNeedle * 0.05 * sin(fNeedleAngleLeft*0.017453);

   xneedle[3] = fLengthNeedle * 0.05 * cos(fNeedleAngleRight*0.017453);
   yneedle[3] = fLengthNeedle * 0.05 * sin(fNeedleAngleRight*0.017453);

   xneedle[4] = fLengthNeedle * 0.6 * cos(fNeedleAngle*0.017453) + fLengthNeedle * 0.05 * cos(fNeedleAngleRight*0.017453);
   yneedle[4] = fLengthNeedle * 0.6 * sin(fNeedleAngle*0.017453) + fLengthNeedle * 0.05 * sin(fNeedleAngleRight*0.017453);

   for( int i=0; i<5; i++ )
   {
      xneedle[i] = xCenter + xneedle[i] / g_pEngine->getAspectRatio();
      yneedle[i] = yCenter - yneedle[i];
   }

   memcpy(color, g_pEngine->getColorOSDInstruments(), 4*sizeof(double));
   color[3] = 0.9;
   g_pEngine->setColors(color);
   g_pEngine->setStrokeSize(0.0);
   g_pEngine->fillPolygon(xneedle, yneedle,5);

   xneedle[0] = fLengthNeedle * 0.05 * cos(fNeedleAngleLeft*0.017453);
   yneedle[0] = fLengthNeedle * 0.05 * sin(fNeedleAngleLeft*0.017453);

   xneedle[1] = fLengthNeedle * 0.3 * cos(fNeedleAngleBottom*0.017453) + fLengthNeedle * 0.06 * cos(fNeedleAngleLeft*0.017453);
   yneedle[1] = fLengthNeedle * 0.3 * sin(fNeedleAngleBottom*0.017453) + fLengthNeedle * 0.06 * sin(fNeedleAngleLeft*0.017453);

   xneedle[2] = fLengthNeedle * 0.3 * cos(fNeedleAngleBottom*0.017453) + fLengthNeedle * 0.06 * cos(fNeedleAngleRight*0.017453);
   yneedle[2] = fLengthNeedle * 0.3 * sin(fNeedleAngleBottom*0.017453) + fLengthNeedle * 0.06 * sin(fNeedleAngleRight*0.017453);

   xneedle[3] = fLengthNeedle * 0.05 * cos(fNeedleAngleRight*0.017453);
   yneedle[3] = fLengthNeedle * 0.05 * sin(fNeedleAngleRight*0.017453);


   for( int i=0; i<4; i++ )
   {
      xneedle[i] = xCenter + xneedle[i] / g_pEngine->getAspectRatio();
      yneedle[i] = yCenter - yneedle[i];
   }
   g_pEngine->setFill(0,0,0,0.8);
   g_pEngine->setStroke(0,0,0,0.8);
   g_pEngine->setStrokeSize(0.0);
   g_pEngine->fillPolygon(xneedle, yneedle,4);


   // Draw needle (100 meters value)

   fNormValue = s_fPluginAltitudeSmooth / 100.0 / 10.0;
   fNeedleAngle = fStartAngle - (fStartAngle - fEndAngle) * fNormValue;
   fNeedleAngleLeft = fNeedleAngle + 90.0;
   fNeedleAngleRight = fNeedleAngle - 90.0;
   fNeedleAngleBottom = fNeedleAngle + 180.0;

   fLengthNeedle = fRadiusAltitude - height_text_small;
   if ( fRadiusAltitude > 0.1 )
      fLengthNeedle = fRadiusAltitude - height_text;

   xneedle[0] = fLengthNeedle * 0.6 * cos(fNeedleAngle*0.017453);
   yneedle[0] = fLengthNeedle * 0.6 * sin(fNeedleAngle*0.017453);

   xneedle[1] = fLengthNeedle * 0.4 * cos(fNeedleAngle*0.017453) + fLengthNeedle * 0.1 * cos(fNeedleAngleLeft*0.017453);
   yneedle[1] = fLengthNeedle * 0.4 * sin(fNeedleAngle*0.017453) + fLengthNeedle * 0.1 * sin(fNeedleAngleLeft*0.017453);

   xneedle[2] = fLengthNeedle * 0.05 * cos(fNeedleAngleLeft*0.017453);
   yneedle[2] = fLengthNeedle * 0.05 * sin(fNeedleAngleLeft*0.017453);

   xneedle[3] = fLengthNeedle * 0.05 * cos(fNeedleAngleRight*0.017453);
   yneedle[3] = fLengthNeedle * 0.05 * sin(fNeedleAngleRight*0.017453);

   xneedle[4] = fLengthNeedle * 0.4 * cos(fNeedleAngle*0.017453) + fLengthNeedle * 0.1 * cos(fNeedleAngleRight*0.017453);
   yneedle[4] = fLengthNeedle * 0.4 * sin(fNeedleAngle*0.017453) + fLengthNeedle * 0.1 * sin(fNeedleAngleRight*0.017453);

   for( int i=0; i<5; i++ )
   {
      xneedle[i] = xCenter + xneedle[i] / g_pEngine->getAspectRatio();
      yneedle[i] = yCenter - yneedle[i];
   }

   memcpy(color, g_pEngine->getColorOSDInstruments(), 4*sizeof(double));
   color[3] = 0.9;
   g_pEngine->setColors(color);
   g_pEngine->setStrokeSize(0.0);
   g_pEngine->fillPolygon(xneedle, yneedle,5);

   xneedle[0] = fLengthNeedle * 0.05 * cos(fNeedleAngleLeft*0.017453);
   yneedle[0] = fLengthNeedle * 0.05 * sin(fNeedleAngleLeft*0.017453);

   xneedle[1] = fLengthNeedle * 0.2 * cos(fNeedleAngleBottom*0.017453) + fLengthNeedle * 0.07 * cos(fNeedleAngleLeft*0.017453);
   yneedle[1] = fLengthNeedle * 0.2 * sin(fNeedleAngleBottom*0.017453) + fLengthNeedle * 0.07 * sin(fNeedleAngleLeft*0.017453);

   xneedle[2] = fLengthNeedle * 0.2 * cos(fNeedleAngleBottom*0.017453) + fLengthNeedle * 0.07 * cos(fNeedleAngleRight*0.017453);
   yneedle[2] = fLengthNeedle * 0.2 * sin(fNeedleAngleBottom*0.017453) + fLengthNeedle * 0.07 * sin(fNeedleAngleRight*0.017453);

   xneedle[3] = fLengthNeedle * 0.05 * cos(fNeedleAngleRight*0.017453);
   yneedle[3] = fLengthNeedle * 0.05 * sin(fNeedleAngleRight*0.017453);


   for( int i=0; i<4; i++ )
   {
      xneedle[i] = xCenter + xneedle[i] / g_pEngine->getAspectRatio();
      yneedle[i] = yCenter - yneedle[i];
   }
   g_pEngine->setFill(0,0,0,0.8);
   g_pEngine->setStroke(0,0,0,0.8);
   g_pEngine->setStrokeSize(0.0);
   g_pEngine->fillPolygon(xneedle, yneedle,4);


   // Draw center

   g_pEngine->setFill(255,255,255,0.8);
   g_pEngine->setStroke(0,0,0,0.8);
   g_pEngine->setStrokeSize(1.0);
   g_pEngine->drawCircle(xCenter, yCenter, g_pEngine->getPixelHeight()*6.0);
   g_pEngine->fillCircle(xCenter, yCenter, g_pEngine->getPixelHeight()*2.0);


   // Draw vertical speed layer

   if ( 1 == pCurrentSettings->nSettingsValues[1] )
   {
      g_pEngine->setColors(g_pEngine->getColorOSDInstruments());
      g_pEngine->setStrokeSize(2.0);

      int nMaxVSpeed = 15;
      int nVSpeed = 0;

      fStartAngle = 180.0;
      fEndAngle = -180.0;
      fStepAngle = -360.0/(float)nMaxVSpeed/2.0;

      // Draw gradations for vertical speed

      for( float fAngle = fStartAngle; fAngle>fEndAngle+1.0; fAngle += fStepAngle )
      {
         float x = xCenter + fRadius*0.93 * cos(fAngle*0.017453) / g_pEngine->getAspectRatio();
         float y = yCenter - fRadius*0.93 * sin(fAngle*0.017453);
         float xinner = xCenter + (fRadiusAltitude + fRadius*0.07) * cos(fAngle*0.017453) / g_pEngine->getAspectRatio();
         float yinner = yCenter - (fRadiusAltitude + fRadius*0.07) * sin(fAngle*0.017453);
         if ( (nVSpeed % 5) == 0 )
         {
            x = xCenter + fRadius*0.97 * cos(fAngle*0.017453) / g_pEngine->getAspectRatio();
            y = yCenter - fRadius*0.97 * sin(fAngle*0.017453);
            xinner = xCenter + (fRadiusAltitude + fRadius*0.03) * cos(fAngle*0.017453) / g_pEngine->getAspectRatio();
            yinner = yCenter - (fRadiusAltitude + fRadius*0.03) * sin(fAngle*0.017453);
         }
         g_pEngine->drawLine(x,y,xinner,yinner);

         if ( (nVSpeed%5) == 0 )
         {
            if ( nVSpeed == nMaxVSpeed )
               sprintf(szBuff, "(%d)", nVSpeed);
            else if ( nVSpeed > nMaxVSpeed )
               sprintf(szBuff, "%d", - nMaxVSpeed - (nMaxVSpeed-nVSpeed));
            else
               sprintf(szBuff, "%d", nVSpeed);
            float w = g_pEngine->textWidth(fontIdSmall, szBuff);
            g_pEngine->drawText((x+xinner)*0.5-0.5*w, (y+yinner)*0.5-0.5*height_text_small, fontIdSmall, szBuff);
         }
         nVSpeed++;
      }

      // Draw vspeed

      float fVSpeed = ((float)pTelemetryInfo->vspeed)/100.0 - 1000.0;

      // Make it move smoothly

      static float s_fPluginVSpeedSmooth = 0.0;
      s_fPluginVSpeedSmooth = 0.6*s_fPluginVSpeedSmooth + 0.4 * fVSpeed;

      float fValue = s_fPluginVSpeedSmooth;
      if ( fValue > nMaxVSpeed-0.5 )
         fValue = nMaxVSpeed-0.5;
      if ( fValue < -nMaxVSpeed+0.5 )
         fValue = -nMaxVSpeed+0.5;

      float fNeedleAngle = 0.0;
      if ( fValue >= 0.0 )
         fNeedleAngle = fStartAngle - 180.0 * fValue / (float) nMaxVSpeed;
      else
         fNeedleAngle = - 180.0 - 180.0 * fValue / (float) nMaxVSpeed;

      float xneedle = xCenter + (fRadius* 0.95 * 0.5 + (fRadiusAltitude + fRadius*0.05)*0.5) * cos(fNeedleAngle*0.017453) / g_pEngine->getAspectRatio();
      float yneedle = yCenter - (fRadius* 0.95 * 0.5 + (fRadiusAltitude + fRadius*0.05)*0.5) * sin(fNeedleAngle*0.017453);

      g_pEngine->setStroke(0,0,0,0.8);
      g_pEngine->setStrokeSize(1.0);

      if ( fValue < 0.0 )
      {
         color[0] = 250; color[1] = 50; color[2] = 50; color[3] = 0.8;
         g_pEngine->setColors(color);
      }

      g_pEngine->fillCircle(xneedle, yneedle, fRadius*0.05);
      g_pEngine->drawCircle(xneedle, yneedle, fRadius*0.05);
   }

   g_pEngine->setColors(g_pEngine->getColorOSDInstruments());
}

#ifdef __cplusplus
}
#endif
