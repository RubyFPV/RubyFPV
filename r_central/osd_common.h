#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"


#define OSD_STRIKE_WIDTH 1.0

#define OSD_QUALITY_LEVEL_WARNING 50
#define OSD_QUALITY_LEVEL_CRITICAL 30

extern u32 g_idIconRuby;
extern u32 g_idIconDrone;
extern u32 g_idIconPlane;
extern u32 g_idIconCar;

extern u32 g_idIconArrowUp;
extern u32 g_idIconArrowDown;
extern u32 g_idIconAlarm;
extern u32 g_idIconGPS;
extern u32 g_idIconHome;
extern u32 g_idIconCamera;
extern u32 g_idIconClock;
extern u32 g_idIconHeading;
extern u32 g_idIconThrotle;
extern u32 g_idIconJoystick;
extern u32 g_idIconTemperature;
extern u32 g_idIconCPU;
extern u32 g_idIconCheckOK;
extern u32 g_idIconShield;
extern u32 g_idIconInfo;
extern u32 g_idIconWarning;
extern u32 g_idIconError;
extern u32 g_idIconUplink;
extern u32 g_idIconUplink2;
extern u32 g_idIconRadio;
extern u32 g_idIconWind;

extern int s_computedOSDCellCount;

extern float g_fOSDDbm[MAX_RADIO_INTERFACES];

extern float g_fOSDStatsForcePanelWidth;
extern float g_fOSDStatsBgTransparency;

float osd_getMarginX();
float osd_getMarginY();
void osd_setMarginX(float x);
void osd_setMarginY(float y);
float osd_getBarHeight();
float osd_getSecondBarHeight();
float osd_getVerticalBarWidth();

float osd_getFontHeight();
float osd_getFontHeightBig();
float osd_getFontHeightSmall();
float osd_getScaleOSD();
float osd_setScaleOSD(int nScale);
float osd_getScaleOSDStats();
float osd_setScaleOSDStats(int nScale);
void osd_setOSDOutlineThickness(float fValue);

float osd_getSpacingH();
float osd_getSpacingV();

void osd_set_transparency(int value);
void osd_set_colors();
void osd_set_colors_alpha(float alpha);
void osd_set_colors_background_fill();
void osd_set_colors_background_fill(float fAlpha);
float osd_show_value(float x, float y, const char* szValue, u32 fontId );
float osd_show_value_sufix(float x, float y, const char* szValue, const char* szSufix, u32 fontId, u32 fontIdSufix );
float osd_show_value_sufix_left(float x, float y, const char* szValue, const char* szSufix, u32 fontId, u32 fontIdSufix );
float osd_show_value_left(float x, float y, const char* szValue, u32 fontId );
float osd_show_value_centered(float x, float y, const char* szValue, u32 fontId);

float _osd_convertKm(float km);
float _osd_convertMeters(float m);
float _osd_convertKm(float km);
float _osd_convertMeters(float m);
float osd_convertTemperature(float c);

float osd_course_to(double lat1, double long1, double lat2, double long2);
void osd_rotatePoints(float *x, float *y, float angle, int points, float center_x, float center_y, float fScale);
void osd_rotate_point(float x, float y, float xCenter, float yCenter, float angle, float* px, float* py);

bool osd_load_resources();

float osd_getScaleOSD();
float osd_setScaleOSD(float f);

u32 osd_getVehicleIcon(int vehicletype);

float osd_show_video_profile_mode(float xPos, float yPos, u32 uFontId, bool bLeft);
