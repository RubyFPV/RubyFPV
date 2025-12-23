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

#include "osd_common.h"
#include "../../base/base.h"
#include "../../base/config.h"
#include "../../../mavlink/common/mavlink.h"
#include "../../base/ctrl_interfaces.h"
#include "../../base/ctrl_settings.h"
#include "../../common/string_utils.h"
#include "osd.h"
#include "osd_stats.h"
#include "osd_lean.h"
#include "osd_warnings.h"
#include "osd_widgets.h"
#include "osd_gauges.h"
#include <math.h>
#include <ctype.h>

#include "../popup.h"
#include "../colors.h"
#include "../fonts.h"
#include "../../renderer/render_engine.h"
#include "../shared_vars.h"
#include "../pairing.h"
#include "../link_watch.h"
#include "osd_ahi.h"
#include "../local_stats.h"
#include "../launchers_controller.h"


u32 g_idIconRuby = 0;
u32 g_idIconOpenIPC = 0;
u32 g_idIconDrone = 0;
u32 g_idIconPlane = 0;
u32 g_idIconCar = 0;

u32 g_idIconArrowUp = 0;
u32 g_idIconArrowDown = 0;
u32 g_idIconAlarm = 0;
u32 g_idIconGPS = 0;
u32 g_idIconHome = 0;
u32 g_idIconCamera = 0;
u32 g_idIconClock = 0;
u32 g_idIconHeading = 0;
u32 g_idIconThrotle = 0;
u32 g_idIconJoystick = 0;
u32 g_idIconTemperature = 0;
u32 g_idIconCPU = 0;
u32 g_idIconCheckOK = 0;
u32 g_idIconShield = 0;
u32 g_idIconInfo = 0;
u32 g_idIconWarning = 0;
u32 g_idIconError = 0;
u32 g_idIconUplink = 0;
u32 g_idIconUplink2 = 0;
u32 g_idIconRadio = 0;
u32 g_idIconWind = 0;
u32 g_idIconController = 0;
u32 g_idIconX = 0;
u32 g_idIconFavorite = 0;
u32 g_idIconSDCard = 0;

u32 g_idImgMSPOSDBetaflight = 0;
u32 g_idImgMSPOSDINAV = 0;
u32 g_idImgMSPOSDArdupilot = 0;
u32 g_idImgMSPOSDPitLab = 0;

float sfScreenXMargin = 0.02;
float sfScreenYMargin = 0.02;
float sfSpacingH = 0.02;
float sfSpacingV = 0.02;
float sfBarHeight = 0.05;

float sfScaleOSD = 1.0;
float sfScaleOSDStats = 1.0;
float sfOSDOutlineThickness = OSD_STRIKE_WIDTH;
int s_iOSDTransparency = 1;

float g_fOSDStatsForcePanelWidth = 0.0;
float g_fOSDStatsBgTransparency = 1.0;

u32 g_uOSDElementChangeTimeout = 2000;
u32 g_uOSDElementChangeBlinkInterval = 100;
bool g_bOSDElementChangeNotification = true;

int s_iCurrentOSDScreenIndex = 0;
Model* s_pCurrentOSDLayoutSourceModel = NULL;

int s_iCurrentOSDVehicleDataSourceRuntimeIndex = 0;

double OSD_COLOR_FRAME_NORMAL[4] = { 255,255,255, 0.7 };
double OSD_COLOR_FRAME_MISSING[4] = {255,70,70, 0.9};
double OSD_COLOR_FRAME_EC[4] = {90,255,110, 0.9};
double OSD_COLOR_FRAME_ECMAX[4] = {255,250,40, 1.0};
double OSD_COLOR_FRAME_NAL_I[4] = {80,80,255, 0.94};
double OSD_COLOR_FRAME_NAL_OTHER[4] = {0,0,100, 0.94};
double OSD_COLOR_FRAME_RETR[4] = {255,170, 0, 0.9};
double OSD_COLOR_FRAME_RETR_DISCARD[4] = {70,100,255,1.0};

const double* osdGetColorVideoFrameNormal()
{
   return OSD_COLOR_FRAME_NORMAL;
}
const double* osdGetColorVideoFrameI()
{
   return OSD_COLOR_FRAME_NAL_I;
}
const double* osdGetColorVideoFrameO()
{
   return OSD_COLOR_FRAME_NAL_OTHER;
}
const double* osdGetColorVideoFrameEC()
{
   return OSD_COLOR_FRAME_EC;
}
const double* osdGetColorVideoFrameECMax()
{
   return OSD_COLOR_FRAME_ECMAX;
}
const double* osdGetColorVideoFrameRetr()
{
   return OSD_COLOR_FRAME_RETR;
}
const double* osdGetColorVideoFrameRetrDiscard()
{
   return OSD_COLOR_FRAME_RETR_DISCARD;
}
const double* osdGetColorVideoFrameMissing()
{
   return OSD_COLOR_FRAME_MISSING;
}

float osd_getSetScreenScale(int iOSDScreenSize)
{
   float dxPixels = 0.0;
   float dyPixels = 0.0;

   //if ( 0 != iOSDScreenSize )
   {
      dxPixels = 2.0 * g_pRenderEngine->getPixelWidth();
      dyPixels = 2.0 * g_pRenderEngine->getPixelHeight();
   }

   float fScreenScale = 1.0;
   if ( iOSDScreenSize == 1 ) fScreenScale = 0.98;
   if ( iOSDScreenSize == 2 ) fScreenScale = 0.96;
   if ( iOSDScreenSize == 3 ) fScreenScale = 0.94;
   if ( iOSDScreenSize == 4 ) fScreenScale = 0.92;
   if ( iOSDScreenSize == 5 ) fScreenScale = 0.90;
   if ( iOSDScreenSize == 6 ) fScreenScale = 0.88;
   if ( iOSDScreenSize == 7 ) fScreenScale = 0.86;

   osd_setMarginX(dxPixels + 0.5*(1.0-fScreenScale));
   osd_setMarginY(dyPixels + 0.5*(1.0-fScreenScale)*g_pRenderEngine->getAspectRatio()*0.8);
   return fScreenScale;
}


float osd_getFontHeight()
{
   return g_pRenderEngine->textHeight(g_idFontOSD);
}

float osd_getFontHeightBig()
{
   return g_pRenderEngine->textHeight(g_idFontOSDBig);
}

float osd_getFontHeightSmall()
{
   return g_pRenderEngine->textHeight(g_idFontOSDSmall);
}


float osd_getMarginX() { return sfScreenXMargin; }
float osd_getMarginY() { return sfScreenYMargin; }
void osd_setMarginX(float x) { sfScreenXMargin = x; }
void osd_setMarginY(float y) { sfScreenYMargin = y; }

float osd_getBarHeight()
{
   return osd_getFontHeightBig() * 1.32 * 1.1;
}

float osd_getSecondBarHeight()
{
   return (osd_getFontHeight() + 1.0 * osd_getSpacingV())*1.1;
};

float osd_getVerticalBarWidth()
{
   return 4.6*osd_getFontHeightBig()/g_pRenderEngine->getAspectRatio();
}

float osd_getSpacingH()
{
   return 0.6*osd_getFontHeight();
}
float osd_getSpacingV()
{
   return 0.16*osd_getFontHeightBig();
}

float osd_getScaleOSD() { return sfScaleOSD; }
float osd_setScaleOSD(int nValue)
{
   float fOSDScale = 1.2;
   if ( nValue >= 2)
     fOSDScale = fOSDScale + (((float)nValue)-2.0)/4.4;
   else
     fOSDScale = fOSDScale + (((float)nValue)-2.0)/5.0;
   sfScaleOSD = fOSDScale;
   return sfScaleOSD; 
}

float osd_getScaleOSDStats() { return sfScaleOSDStats; }
float osd_setScaleOSDStats(int nValue)
{
   sfScaleOSDStats = 1.34;
   if ( nValue >= 3) // 3 is normal
     sfScaleOSDStats = sfScaleOSDStats + 0.15 * ((float)nValue-3.0);
   else
     sfScaleOSDStats = sfScaleOSDStats - 0.18 * (3.0 - (float)nValue);
   
   return sfScaleOSDStats; 
}

void osd_setOSDOutlineThickness(float fValue) { sfOSDOutlineThickness = fValue; }


float _osd_convertKm(float km)
{
   Preferences* p = get_Preferences();

   if ( p->iUnits == prefUnitsMetric || p->iUnits == prefUnitsMeters )
      return km;
   if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
      return km*0.621371;
   return km;
}


float _osd_convertMeters(float m)
{
   Preferences* p = get_Preferences();

   if ( p->iUnits == prefUnitsMetric || p->iUnits == prefUnitsMeters )
      return m;
   if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
      return m*3.28084;
   return m;
}


float _osd_convertHeightMeters(float m)
{
   Preferences* p = get_Preferences();

   if ( (p->iUnitsHeight == prefUnitsMetric) || (p->iUnitsHeight == prefUnitsMeters) )
      return m;
   if ( (p->iUnitsHeight == prefUnitsImperial) || (p->iUnitsHeight == prefUnitsFeets) )
      return m*3.28084;
   return m;
}

bool osd_load_resources()
{
   g_idIconRuby = g_pRenderEngine->loadIcon("res/icon_ruby.png");
   g_idIconOpenIPC = g_pRenderEngine->loadIcon("res/openipc.png");
   g_idIconDrone = g_pRenderEngine->loadIcon("res/icon_v_drone.png");
   g_idIconPlane = g_pRenderEngine->loadIcon("res/icon_v_plane.png");
   g_idIconCar = g_pRenderEngine->loadIcon("res/icon_v_car.png");

   g_idIconArrowUp = g_pRenderEngine->loadIcon("res/icon_arrow_up.png");
   g_idIconArrowDown = g_pRenderEngine->loadIcon("res/icon_arrow_down.png");
   g_idIconAlarm = g_pRenderEngine->loadIcon("res/icon_alarm.png");
   g_idIconCheckOK = g_pRenderEngine->loadIcon("res/icon_checkok.png");
   g_idIconInfo = g_pRenderEngine->loadIcon("res/icon_info.png");
   g_idIconWarning = g_pRenderEngine->loadIcon("res/icon_warning.png");
   g_idIconError = g_pRenderEngine->loadIcon("res/icon_error.png");

   g_idIconGPS = g_pRenderEngine->loadIcon("res/icon_gps.png");
   g_idIconHome = g_pRenderEngine->loadIcon("res/icon_home.png");
   g_idIconCamera = g_pRenderEngine->loadIcon("res/icon_camera.png");
   g_idIconClock = g_pRenderEngine->loadIcon("res/icon_clock.png");
   g_idIconHeading = g_pRenderEngine->loadIcon("res/icon_heading.png");
   g_idIconThrotle = g_pRenderEngine->loadIcon("res/icon_gauge.png");
   g_idIconTemperature = g_pRenderEngine->loadIcon("res/icon_temp.png");
   g_idIconCPU = g_pRenderEngine->loadIcon("res/icon_cpu.png");
   g_idIconJoystick = g_pRenderEngine->loadIcon("res/icon_joystick.png");
   g_idIconShield = g_pRenderEngine->loadIcon("res/icon_shield.png");
   g_idIconUplink = g_pRenderEngine->loadIcon("res/icon_uplink.png");
   g_idIconUplink2 = g_pRenderEngine->loadIcon("res/icon_uplink2.png");
   g_idIconRadio = g_pRenderEngine->loadIcon("res/icon_radio.png");
   g_idIconWind = g_pRenderEngine->loadIcon("res/icon_wind.png");
   g_idIconController = g_pRenderEngine->loadIcon("res/icon_controller.png");
   g_idIconX = g_pRenderEngine->loadIcon("res/icon_x.png");
   g_idIconFavorite = g_pRenderEngine->loadIcon("res/favorite.png");
   g_idIconSDCard = g_pRenderEngine->loadIcon("res/icon_sdcard.png");

   osd_reload_msp_resources();
   osd_stats_init();
   return true;
}

void osd_reload_msp_resources()
{
   if ( g_idImgMSPOSDBetaflight > 0 )
      g_pRenderEngine->freeImage(g_idImgMSPOSDBetaflight);
   if ( g_idImgMSPOSDINAV > 0 )
      g_pRenderEngine->freeImage(g_idImgMSPOSDINAV);
   if ( g_idImgMSPOSDArdupilot > 0 )
      g_pRenderEngine->freeImage(g_idImgMSPOSDArdupilot);
   if ( g_idImgMSPOSDPitLab > 0 )
      g_pRenderEngine->freeImage(g_idImgMSPOSDPitLab);

   log_line("Loading MSP OSD images for screen surface height: %d px", g_pRenderEngine->getScreenHeight());
   if ( g_pRenderEngine->getScreenHeight() > 800 )
   {
      g_idImgMSPOSDBetaflight = g_pRenderEngine->loadImage("res/msp_osd_betaflight.png");
      g_idImgMSPOSDINAV = g_pRenderEngine->loadImage("res/msp_osd_inav.png");
      g_idImgMSPOSDArdupilot = g_pRenderEngine->loadImage("res/msp_osd_ardu.png");
      g_idImgMSPOSDPitLab = g_pRenderEngine->loadImage("res/msp_osd_pitlab.png");
   }
   else
   {
      g_idImgMSPOSDBetaflight = g_pRenderEngine->loadImage("res/msp_osd_betaflight720.png");
      g_idImgMSPOSDINAV = g_pRenderEngine->loadImage("res/msp_osd_inav720.png");
      g_idImgMSPOSDArdupilot = g_pRenderEngine->loadImage("res/msp_osd_ardu720.png");
      g_idImgMSPOSDPitLab = g_pRenderEngine->loadImage("res/msp_osd_pitlab720.png");
   }
   Preferences* p = get_Preferences();
   if ( NULL != p )
   {
      g_pRenderEngine->changeImageHue(g_idImgMSPOSDBetaflight, p->iColorOSD[0], p->iColorOSD[1], p->iColorOSD[2]);
      g_pRenderEngine->changeImageHue(g_idImgMSPOSDINAV, p->iColorOSD[0], p->iColorOSD[1], p->iColorOSD[2]);
      g_pRenderEngine->changeImageHue(g_idImgMSPOSDArdupilot, p->iColorOSD[0], p->iColorOSD[1], p->iColorOSD[2]);
      g_pRenderEngine->changeImageHue(g_idImgMSPOSDPitLab, p->iColorOSD[0], p->iColorOSD[1], p->iColorOSD[2]);
   }
}

void osd_apply_preferences()
{
   Preferences* p = get_Preferences();

   float fO = 1.07;
   if ( p->iOSDOutlineThickness == -3 )
     fO = 0;
   if ( p->iOSDOutlineThickness == -2 )
     fO = 0.2;
   if ( p->iOSDOutlineThickness == -1 )
     fO = 0.4;
   if ( p->iOSDOutlineThickness == 0 )
     fO = 0.7;
   if ( p->iOSDOutlineThickness > 0 )
     fO = fO + ((float)p->iOSDOutlineThickness)*0.4;
   log_line("Apply OSD outline thickness: %f", fO);
   osd_setOSDOutlineThickness(fO);

   float fA = 0.85;
   if ( p->iScaleAHI > 0 )
     fA = fA + ((float)p->iScaleAHI)/12.0;
   if ( p->iScaleAHI < 0 )
     fA = fA + ((float)p->iScaleAHI)/16.0;
   osd_set_ahi_scale(fA);

   log_line("Applied UI scaling: OSD: %.2f, AHI: %.2f", fO, fA);
}

void osd_set_transparency(int value)
{
   s_iOSDTransparency = value;
   switch ( value )
   {
      case 0: g_pRenderEngine->setGlobalAlfa(0.85); break;
      case 1: g_pRenderEngine->setGlobalAlfa(0.9); break;
      case 2: g_pRenderEngine->setGlobalAlfa(0.95); break;
      case 3:
      case 4: g_pRenderEngine->setGlobalAlfa(1.0); break;
   }
}


void osd_set_colors()
{
   const double* pc1 = get_Color_OSDText();
   g_pRenderEngine->setColors(pc1);
   const double* pc = get_Color_OSDTextOutline();
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], pc[3]);
   g_pRenderEngine->setStrokeSize(sfOSDOutlineThickness);
}

void osd_set_colors_text(const double* pColorText)
{
   if ( NULL == pColorText )
      return;
   g_pRenderEngine->setColors(pColorText);
   const double* pc = get_Color_OSDTextOutline();
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], pc[3]);
   g_pRenderEngine->setStrokeSize(sfOSDOutlineThickness); 
}

void osd_set_colors_alpha(float alpha)
{
   const double* pC = get_Color_OSDText();
   const double* pCO = get_Color_OSDTextOutline();

   g_pRenderEngine->setFill(pC[0], pC[1], pC[2], pC[3]*alpha);
   g_pRenderEngine->setStroke(pCO[0], pCO[1], pCO[2], pCO[3]*alpha);
   g_pRenderEngine->setStrokeSize(sfOSDOutlineThickness);
}


void osd_set_colors_background_fill()
{
   Preferences* p = get_Preferences();
   float alfa = 0.9;
   switch ( s_iOSDTransparency )
   {
      case 0: alfa = 0.0; break;
      case 1: alfa = 0.15; break;
      case 2: alfa = 0.3; break;
      case 3: alfa = 0.7; break;
      case 4: alfa = 1.0; break;
   }

   if ( p->iInvertColorsOSD )
   {
   switch ( s_iOSDTransparency )
   {
      case 0: alfa = 0.5; break;
      case 1: alfa = 0.62; break;
      case 2: alfa = 0.67; break;
      case 3: alfa = 0.75; break;
      case 4: alfa = 1.0; break;
   }
   }

   double pC[4];
   memcpy(pC, get_Color_OSDBackground(), 4*sizeof(double));
   if ( s_iOSDTransparency == 4 )
      pC[3] = 1.0;
   g_pRenderEngine->setColors(pC, alfa);
}

void osd_set_colors_background_fill(float fAlpha)
{
   double pC[4];
   memcpy(pC, get_Color_OSDBackground(), 4*sizeof(double));
   if ( s_iOSDTransparency == 4 )
      pC[3] = 1.0;
   g_pRenderEngine->setColors(pC, fAlpha);
}

float osd_course_to(double lat1, double long1, double lat2, double long2)
{
    double dlon = (long2-long1)*0.017453292519;
    lat1 = (lat1)*0.017453292519;
    lat2 = (lat2)*0.017453292519;
    double a1 = sin(dlon) * cos(lat2);
    double a2 = sin(lat1) * cos(lat2) * cos(dlon);
    a2 = cos(lat1) * sin(lat2) - a2;
    a2 = atan2(a1, a2);
    if (a2 < 0.0) a2 += 2.0*3.141592653589793;
    return a2*180.0/3.141592653589793;
    
    /*
    double dlon = long2 - long1;
    double x = cos(lat2)*sin(dlon);
    double y = cos(lat1)*sin(lat2) - cos(lat2)*sin(lat1)*cos(dlon);
    double a = atan2(x,y);
    if (a < 0.0) a += 2.0*3.141592653589793;
    return a*180.0/3.141592653589793;
    */
}

float osd_show_value(float x, float y, const char* szValue, u32 fontId)
{
   float w = 0;
   g_pRenderEngine->drawText(x,y, fontId, szValue);
   w += g_pRenderEngine->textWidth(fontId, szValue);
   return w;
}

float osd_show_value_sufix(float x, float y, const char* szValue, const char* szSufix, u32 fontId, u32 fontIdSufix )
{
   float height_text = g_pRenderEngine->textHeight(fontId);
   float height_text_s = g_pRenderEngine->textHeight(fontIdSufix);
   float w = 0;
   g_pRenderEngine->drawText(x,y, fontId, szValue);
   w += g_pRenderEngine->textWidth(fontId, szValue);
   if ( (NULL != szSufix) && (0 != szSufix[0]) )
   {
      if ( ' ' == *szSufix )
         szSufix++;
      w += height_text_s*0.18;
      g_pRenderEngine->drawText(x+w,y+(height_text-height_text_s)*0.6, fontIdSufix, szSufix);
      w += g_pRenderEngine->textWidth(fontIdSufix, szSufix);
   }
   return w;
}

float osd_show_value_sufix_left(float x, float y, const char* szValue, const char* szSufix, u32 fontId, u32 fontIdSufix )
{
   float height_text = g_pRenderEngine->textHeight(fontId);
   float height_text_s = g_pRenderEngine->textHeight(fontIdSufix);

   float w = g_pRenderEngine->textWidth(fontId, szValue);
   float ws = g_pRenderEngine->textWidth(fontIdSufix, szSufix);

   g_pRenderEngine->drawText(x-ws,y+(height_text-height_text_s)*0.6, fontIdSufix, szSufix);
   x -= w + ws + height_text_s*0.18;
   g_pRenderEngine->drawText(x,y, fontId, szValue);
   return w + ws + height_text_s*0.18;
}

float osd_show_value_left(float x, float y, const char* szValue, u32 fontId)
{
   float w = g_pRenderEngine->textWidth(fontId, szValue);
   g_pRenderEngine->drawText(x-w,y, fontId, szValue);
   return w;
}


float osd_show_value_centered(float x, float y, const char* szValue, u32 fontId)
{
   float w = g_pRenderEngine->textWidth(fontId, szValue);

   g_pRenderEngine->drawText(x-0.5*w,y, fontId, szValue);
   
   return w;
}

u32 osd_getVehicleIcon(int vehicle_type)
{
   u32 idIcon = g_idIconRuby;

   if ( (vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_DRONE )
      idIcon = g_idIconDrone;
   if ( (vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_AIRPLANE )
      idIcon = g_idIconPlane;
   if ( (vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_CAR )
      idIcon = g_idIconCar;
   return idIcon;
}


void osd_rotatePoints(float *x, float *y, float angle, int points, float center_x, float center_y, float fScale)
{
    double cosAngle = cos(angle * 0.017453292519);
    double sinAngle = sin(angle * 0.017453292519);

    float tmp_x = 0;
    float tmp_y = 0;
    for( int i=0; i<points; i++ )
    {
       tmp_x = center_x + (x[i]*cosAngle-y[i]*sinAngle)*fScale/g_pRenderEngine->getAspectRatio();
       tmp_y = center_y + (x[i]*sinAngle + y[i]*cosAngle)*fScale;
       x[i] = tmp_x;
       y[i] = tmp_y;
    }
}


void osd_rotate_point(float x, float y, float xCenter, float yCenter, float angle, float* px, float* py)
{
   if ( NULL == px || NULL == py )
      return;
   float s = sin(angle*3.14159/180.0);
   float c = cos(angle*3.14159/180.0);

   x = (x-xCenter)*g_pRenderEngine->getAspectRatio();
   y = (y-yCenter);
   *px = x * c - y * s;
   *py = x * s + y * c;

   *px /= g_pRenderEngine->getAspectRatio();
   *px += xCenter;
   *py += yCenter;
}

float osd_show_video_profile_mode(float xPos, float yPos, u32 uFontId, bool bLeft)
{
   float fWidth = 0;
   char szBuff[64];
   szBuff[0] = 0;
   
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   u32 uActiveVehicleId = osd_get_current_data_source_vehicle_id();
   shared_mem_video_stream_stats* pVDS = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, uActiveVehicleId);
   if ( (NULL == pVDS) || (NULL == pActiveModel) )
      return 0.0;

   strcpy(szBuff, str_get_video_profile_name(pVDS->PHVS.uCurrentVideoLinkProfile));
   
   if ( pVDS->iAdaptiveVideoLevelNow > 0 )
   {
      if ( pVDS->bIsOnLowestAdaptiveLevel )
      {
         if ( pActiveModel->video_link_profiles[pActiveModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO )
            strcat(szBuff, "-M");
         else
            strcat(szBuff, "-L");
      }
      else
      {
         char szTmp[16];
         sprintf(szTmp, "-%d", pVDS->iAdaptiveVideoLevelNow);
         strcat(szBuff, szTmp);
      }
   }
   else if ( pVDS->PHVS.uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_ON_LOWER_BITRATE )
      strcat(szBuff, "-");
     
   if ( pVDS->uCurrentVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ONE_WAY_FIXED_VIDEO )
      strcat(szBuff, "-1Way");
   if (((pVDS->PHVS.uVideoStreamIndexAndType >> 4) & 0x0F) == VIDEO_TYPE_H265 )
      strcat(szBuff, " H265");
   
   fWidth = g_pRenderEngine->textWidth(uFontId, szBuff);

   if ( bLeft )
      g_pRenderEngine->drawTextLeft(xPos, yPos,uFontId, szBuff);
   else
      g_pRenderEngine->drawText(xPos, yPos, uFontId, szBuff);    
   return fWidth;
}

float osd_convertTemperature(float c, bool bToF)
{
   if ( bToF )
      return c*1.8 + 32.0;
   return c;
}

float osd_render_relay(float xCenter, float yBottom, bool bHorizontal)
{
   if ( NULL == g_pCurrentModel || ( g_pCurrentModel->is_spectator) )
      return 0.0;
   if ( (g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId < 0) || (g_pCurrentModel->relay_params.uRelayedVehicleId == 0) || (g_pCurrentModel->relay_params.uRelayedVehicleId == g_pCurrentModel->uVehicleId) )
      return 0.0;

   const char* szTextMain = "Main Vehicle";
   const char* szTextRelay = "Relayed Vehicle";

   const double* pc1 = get_Color_OSDText();
   g_pRenderEngine->setColors(pc1);
   g_pRenderEngine->setStroke(pc1[0], pc1[1], pc1[2], pc1[3]);
   g_pRenderEngine->setStrokeSize(sfOSDOutlineThickness);

   float height_text = osd_getFontHeight();
   float height_text_small = osd_getFontHeightSmall();

   float fPaddingY = height_text_small*0.4;
   float fPaddingX = 2.0*fPaddingY/g_pRenderEngine->getAspectRatio();
   float fHeight = height_text_small*2.0 + height_text_small + 2.0*fPaddingY;
   float fWidth = 0.0;
   float yPos = yBottom-fHeight;
   
   Model *pRelayedModel = findModelWithId(g_pCurrentModel->relay_params.uRelayedVehicleId, 30);
   if ( (NULL == pRelayedModel) || (pRelayedModel->uVehicleId == g_pCurrentModel->uVehicleId) )
   {
      fHeight = height_text + 2.0*fPaddingY;
      yPos = yBottom - fHeight;
      fWidth = g_pRenderEngine->textWidth(g_idFontOSD, "RELAY: Invalid target relayed vehicle.");
      fWidth += 2.0*fPaddingX;
      g_pRenderEngine->setFill(0,0,0,0.2);
      g_pRenderEngine->drawRoundRect(xCenter-fWidth*0.5, yPos, fWidth, fHeight, 0.01);
      osd_set_colors();
      g_pRenderEngine->drawText(xCenter - fWidth*0.5 + fPaddingX, yPos + fPaddingY, g_idFontOSD, "RELAY: Invalid target relayed vehicle.");
      return fWidth;
   }


   g_pRenderEngine->setColors(pc1);
   g_pRenderEngine->setStroke(pc1[0], pc1[1], pc1[2], pc1[3]);
   g_pRenderEngine->setStrokeSize(sfOSDOutlineThickness);

   bool bRelayedVehicleIsOnline = link_is_relayed_vehicle_online();

   char szName1[128];
   char szName2[128];
   strncpy(szName1, g_pCurrentModel->getLongName(), 127);
   szName1[0] = toupper(szName1[0]);

   strncpy(szName2, pRelayedModel->getLongName(), 127);
   szName2[0] = toupper(szName2[0]);

   float fWidthName1 = g_pRenderEngine->textWidth(g_idFontOSDSmall, szName1);
   float fWidthName2 = g_pRenderEngine->textWidth(g_idFontOSDSmall, szName2);

   float fWidthTextMain = g_pRenderEngine->textWidth(g_idFontOSDSmall, szTextMain);
   float fWidthTextRelay = g_pRenderEngine->textWidth(g_idFontOSDSmall, szTextRelay);
   
   float fWidthLeft = fWidthName1;
   if ( fWidthTextMain > fWidthLeft )
      fWidthLeft = fWidthTextMain;

   float fWidthRight = fWidthName2;
   if ( fWidthTextRelay > fWidthRight )
      fWidthRight = fWidthTextRelay;
   
   float xLeft = xCenter - fWidthLeft - 2.0 * fPaddingX;
   float xRight = xCenter + fWidthRight + 2.0 * fPaddingX;

   if ( g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_PERMANENT_REMOTE )
   {
      xLeft = xCenter - 0.5 * fWidthRight - fPaddingX;
      xRight = xCenter + 0.5 * fWidthRight + fPaddingX;
   }
   fWidth = xRight - xLeft;

   g_pRenderEngine->setFill(0,0,0,0.1);
   g_pRenderEngine->setStrokeSize(1.0);
   g_pRenderEngine->drawRoundRect(xLeft, yPos, fWidth, fHeight, 0.01);
   if ( ! (g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_PERMANENT_REMOTE) )
      g_pRenderEngine->drawLine(xCenter, yPos, xCenter, yBottom - g_pRenderEngine->getPixelHeight());

   bool bMainVehicleActive = true;
   if ( g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_PERMANENT_REMOTE )
      bMainVehicleActive = false;
   if ( (g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_REMOTE) && bRelayedVehicleIsOnline )
      bMainVehicleActive = false;

   double pCA[4];
   memcpy(pCA,get_Color_IconSucces(), 4*sizeof(double));
   for( int i=0; i<3; i++ )
   {
      if ( pCA[i] > 70 )
         pCA[i] -= 70;
      else
         pCA[i] = 0;
   }
   if ( g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_PERMANENT_REMOTE )
   {
      g_pRenderEngine->setFill(pCA[0],pCA[1],pCA[2],pCA[3]);
      g_pRenderEngine->drawRoundRect(xLeft, yPos, fWidth, fHeight, 0.01);
   }
   else if ( bMainVehicleActive )
   {
      g_pRenderEngine->setFill(pCA[0],pCA[1],pCA[2],pCA[3]);
      g_pRenderEngine->drawRoundRect(xLeft, yPos, xCenter-xLeft, fHeight, 0.01);
   }
   else
   {
      g_pRenderEngine->setFill(pCA[0],pCA[1],pCA[2],pCA[3]);
      g_pRenderEngine->drawRoundRect(xCenter, yPos, xRight-xCenter, fHeight, 0.01);
   }
 
   osd_set_colors();
   float fInactiveAlpha = 0.7;

   yPos += fPaddingY;

   if ( g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_PERMANENT_REMOTE )
   {
      osd_set_colors();
      g_pRenderEngine->drawText(xLeft + fPaddingX + 0.5*(fWidthRight - fWidthTextRelay), yPos, g_idFontOSDSmall, szTextRelay);
   }
   else if ( bMainVehicleActive )
   {
      g_pRenderEngine->drawText(xLeft + fPaddingX + 0.5*(fWidthLeft - fWidthTextMain), yPos, g_idFontOSDSmall, szTextMain);
      float fa = g_pRenderEngine->setGlobalAlfa(fInactiveAlpha);
      osd_set_colors();
      g_pRenderEngine->drawText(xCenter + fPaddingX + 0.5*(fWidthRight - fWidthTextRelay), yPos, g_idFontOSDSmall, szTextRelay);
      g_pRenderEngine->setGlobalAlfa(fa);
      osd_set_colors();
   }
   else
   {
      float fa = g_pRenderEngine->setGlobalAlfa(fInactiveAlpha);
      osd_set_colors();
      g_pRenderEngine->drawText(xLeft + fPaddingX + 0.5*(fWidthLeft - fWidthTextMain), yPos, g_idFontOSDSmall, szTextMain);
      g_pRenderEngine->setGlobalAlfa(fa);
      osd_set_colors();
      g_pRenderEngine->drawText(xCenter + fPaddingX + 0.5*(fWidthRight - fWidthTextRelay), yPos, g_idFontOSDSmall, szTextRelay);
   }

   yPos += height_text_small*1.2;

   if ( g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_PERMANENT_REMOTE )
   {
      osd_set_colors();
      g_pRenderEngine->drawText(xLeft + fPaddingX, yPos, g_idFontOSDSmall, szName2);
   }
   else if ( bMainVehicleActive )
   {
      g_pRenderEngine->drawText(xLeft + fPaddingX, yPos, g_idFontOSDSmall, szName1);
      float fa = g_pRenderEngine->setGlobalAlfa(fInactiveAlpha);
      osd_set_colors();
      g_pRenderEngine->drawText(xCenter + fPaddingX, yPos, g_idFontOSDSmall, szName2);
      g_pRenderEngine->setGlobalAlfa(fa);
      osd_set_colors();
   }
   else
   {
      float fa = g_pRenderEngine->setGlobalAlfa(fInactiveAlpha);
      osd_set_colors();
      g_pRenderEngine->drawText(xLeft + fPaddingX, yPos, g_idFontOSDSmall, szName1);
      g_pRenderEngine->setGlobalAlfa(fa);
      osd_set_colors();
      g_pRenderEngine->drawText(xCenter + fPaddingX, yPos, g_idFontOSDSmall, szName2);
   }
 
   yPos += height_text_small;

   float fRadius = height_text_small*0.34;
   if ( bRelayedVehicleIsOnline )
   {
      float fa = g_pRenderEngine->setGlobalAlfa(fInactiveAlpha);
      osd_set_colors();
      float fwt = g_pRenderEngine->textWidth(g_idFontOSDSmall, "Online");
      float fxHighlight = xCenter + 0.5*(fWidthRight - fwt);
      if ( g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_PERMANENT_REMOTE )
         fxHighlight = xLeft + 0.5*(fWidthRight - fwt);
      if ( g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_PERMANENT_REMOTE )

      g_pRenderEngine->drawText(fxHighlight, yPos, g_idFontOSDSmall, "Online");
      const double* pc = get_Color_IconSucces();
      g_pRenderEngine->setFill(pc[0], pc[1], pc[2], pc[3]);
      g_pRenderEngine->fillCircle(fxHighlight - height_text_small/g_pRenderEngine->getAspectRatio(), yPos + height_text_small*0.5, fRadius);
   
      g_pRenderEngine->setGlobalAlfa(fa);
      osd_set_colors();
   }
   else
   {
      float fwt = g_pRenderEngine->textWidth(g_idFontOSDSmall, "Offline");
      float fxHighlight = xCenter + 0.5*(fWidthRight - fwt);
      if ( g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_PERMANENT_REMOTE )
         fxHighlight = xLeft + 0.5*(fWidthRight - fwt);
      g_pRenderEngine->drawText(fxHighlight, yPos, g_idFontOSDSmall, "Offline");
      const double* pc = get_Color_IconError();
      g_pRenderEngine->setFill(pc[0], pc[1], pc[2], pc[3]);
      g_pRenderEngine->fillCircle(fxHighlight - height_text_small/g_pRenderEngine->getAspectRatio(), yPos + height_text_small*0.5, fRadius);
   }

   osd_set_colors();
   return fWidth;
}

int osd_get_current_layout_index()
{
   return s_iCurrentOSDScreenIndex;
}

Model* osd_get_current_layout_source_model()
{
   return s_pCurrentOSDLayoutSourceModel;
}

void osd_set_current_layout_index_and_source_model(Model* pModel, int iLayout)
{
   s_iCurrentOSDScreenIndex = iLayout;
   s_pCurrentOSDLayoutSourceModel = pModel;
   if ( NULL == pModel )
      return;
   /*
   int k=0; 
   while ( (k < 10) && (! (pModel->osd_params.osd_flags2[s_iCurrentOSDScreenIndex] & OSD_FLAG2_LAYOUT_ENABLED)) )
   {
      k++;
      s_iCurrentOSDScreenIndex++;
      if ( s_iCurrentOSDScreenIndex >= osdLayoutLast )
         s_iCurrentOSDScreenIndex = osdLayout1;
   }
   */
}

void osd_set_current_data_source_vehicle_index(int iIndex)
{
   s_iCurrentOSDVehicleDataSourceRuntimeIndex = iIndex;
}

int osd_get_current_data_source_vehicle_index()
{
   return s_iCurrentOSDVehicleDataSourceRuntimeIndex;
}

Model* osd_get_current_data_source_vehicle_model()
{
   Model* pModel = g_pCurrentModel;
   if ( (s_iCurrentOSDVehicleDataSourceRuntimeIndex >= 0) && (s_iCurrentOSDVehicleDataSourceRuntimeIndex <MAX_CONCURENT_VEHICLES) )
      pModel = g_VehiclesRuntimeInfo[s_iCurrentOSDVehicleDataSourceRuntimeIndex].pModel;
   if ( NULL == pModel )
      pModel = g_pCurrentModel;

   return pModel;
}

u32 osd_get_current_data_source_vehicle_id()
{
   u32 uVehicleId = 0;
   if ( NULL != g_pCurrentModel )
      uVehicleId = g_pCurrentModel->uVehicleId;
   if ( (s_iCurrentOSDVehicleDataSourceRuntimeIndex >= 0) && (s_iCurrentOSDVehicleDataSourceRuntimeIndex <MAX_CONCURENT_VEHICLES) )
   if ( NULL != g_VehiclesRuntimeInfo[s_iCurrentOSDVehicleDataSourceRuntimeIndex].pModel )
      uVehicleId = g_VehiclesRuntimeInfo[s_iCurrentOSDVehicleDataSourceRuntimeIndex].pModel->uVehicleId;
   return uVehicleId;
}
