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

#include "colors.h"
#include "../base/base.h"
#include "../base/config.h"
#include "../base/ctrl_preferences.h"


double COLOR_DEV[4] = { 170,220,255,1.0 };

double COLOR_POPUP_BG[4] = { 5,5,5, 0.8 };
double COLOR_POPUP_BORDER[4] = { 90,20,40,0.9 };
double COLOR_POPUP_TEXT[4] = { 255,250,230,0.85 };

double COLOR_POPUP_INV_BG[4] = { 180,180,180, 0.8 };
double COLOR_POPUP_INV_BORDER[4] = { 100,100,100,0.7 };
double COLOR_POPUP_INV_TEXT[4] = { 30,30,30,0.8 };

double COLOR_MENU_BG[4] = { 20,20,20, 0.8 };
double COLOR_MENU_BG_TITLE[4] = {90,20,30,0.9};
double COLOR_MENU_BG_TOOLTIP[4] = {90,20,30,0.9};
double COLOR_MENU_BORDER[4] = { 120,50,70, 0.9 };
double COLOR_MENU_TEXT[4] = { 233,230,225,0.96 };

double COLOR_MENU_INV_BG[4] = { 185,185,180, 0.94 };
double COLOR_MENU_INV_BORDER[4] = { 60,60,60,0.7 };
double COLOR_MENU_INV_TEXT[4] = { 20,20,20,0.96 };

double COLOR_MENU_ITEM_BG_SELECTED[4] = { 217,213,206,0.8 };
double COLOR_MENU_ITEM_TEXT_SELECTED[4] = { 2,2,2,0.95 };
double COLOR_MENU_ITEM_TEXT_DISABLED[4] = { 80,80,80,0.8 };

double COLOR_MENU_INV_ITEM_BG_SELECTED[4] = { 80,80,80,0.8 };
double COLOR_MENU_INV_ITEM_TEXT_SELECTED[4] = { 255,244,225, 0.9 };
double COLOR_MENU_INV_ITEM_TEXT_DISABLED[4] = { 80,80,80,0.5 };

double COLOR_ICON_NORMAL[4] = { 255,255,255, 0.95 };
double COLOR_ICON_WARNING[4] = { 255,255,0, 0.95 };
double COLOR_ICON_ERROR[4] = { 255,70,90, 0.98 };
double COLOR_ICON_SUCCES[4] = { 100,150,100, 0.95 };

double COLOR_OSD_ELEMENT_CHANGED[4] = {140,180,255,1};
double COLOR_OSD_BACKGROUND[4] = {0,0,0, 0.7};
double COLOR_OSD_TEXT[4] = { 255,250,230,1.0 };
double COLOR_OSD_TEXT_OUTLINE[4] = { 0,0,0,0.9 };
double COLOR_OSD_HIGH_WARNING[4] = {255,0,0,0.9};

double COLOR_OSD_INV_BACKGROUND[4] = {220,220,220, 0.7};
double COLOR_OSD_INV_TEXT[4] = { 3,3,3,0.98 };
double COLOR_OSD_INV_TEXT_OUTLINE[4] = { 55,55,55,0.2 };
double COLOR_OSD_INV_HIGH_WARNING[4] = {255,0,0,0.9};

double COLOR_OSD_CHANED_VALUE[4] = { 120,180,120, 0.95 };
double COLOR_OSD_CHANED_VALUE_INV[4] = { 180,250,180, 0.95 };

const double* get_Color_Dev()
{
   return COLOR_DEV;
}

const double* get_Color_OSDBackground()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_OSD_TEXT;
   else
      return COLOR_OSD_BACKGROUND;
}

const double* get_Color_OSDText()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_OSD_INV_TEXT;
   else
      return COLOR_OSD_TEXT;
}

const double* get_Color_OSDTextOutline()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_OSD_INV_TEXT_OUTLINE;
   else
      return COLOR_OSD_TEXT_OUTLINE;

}

const double* get_Color_OSDWarning()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_OSD_INV_HIGH_WARNING;
   else
      return COLOR_OSD_HIGH_WARNING;
}

const double* get_Color_OSDChangedValue()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_OSD_CHANED_VALUE_INV;
   else
      return COLOR_OSD_CHANED_VALUE;
}

const double* get_Color_PopupBg()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_POPUP_INV_BG;
   else
      return COLOR_POPUP_BG;
}

const double* get_Color_PopupBorder()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_POPUP_INV_BORDER;
   else
      return COLOR_POPUP_BORDER;
}

const double* get_Color_PopupText()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_POPUP_INV_TEXT;
   else
      return COLOR_POPUP_TEXT;
}

const double* get_Color_MenuBg()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_MENU_INV_BG;
   else
      return COLOR_MENU_BG;
}

const double* get_Color_MenuBgTitle()
{
   return COLOR_MENU_BG_TITLE;
}

const double* get_Color_MenuBgTooltip()
{
   return COLOR_MENU_BG_TOOLTIP;
}


const double* get_Color_MenuBorder()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_MENU_INV_BORDER;
   else
      return COLOR_MENU_BORDER;
}

const double* get_Color_MenuText()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_MENU_INV_TEXT;
   else
      return COLOR_MENU_TEXT;
}

const double* get_Color_MenuItemSelectedBg()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_MENU_INV_ITEM_BG_SELECTED;
   else
      return COLOR_MENU_ITEM_BG_SELECTED;
}

const double* get_Color_MenuItemSelectedText()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_MENU_INV_ITEM_TEXT_SELECTED;
   else
      return COLOR_MENU_ITEM_TEXT_SELECTED;
}

const double* get_Color_MenuItemDisabledText()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_MENU_INV_ITEM_TEXT_DISABLED;
   else
      return COLOR_MENU_ITEM_TEXT_DISABLED;
}

const double* get_Color_IconNormal() { return COLOR_ICON_NORMAL; }
const double* get_Color_IconWarning() { return COLOR_ICON_WARNING; }
const double* get_Color_IconError() { return COLOR_ICON_ERROR; }
const double* get_Color_IconSucces() { return COLOR_ICON_SUCCES; }

const double* get_Color_OSDElementChanged() { return COLOR_OSD_ELEMENT_CHANGED; }


void set_Color_OSDText(float r, float g, float b, float a)
{
   COLOR_OSD_TEXT[0] = r;
   COLOR_OSD_TEXT[1] = g;
   COLOR_OSD_TEXT[2] = b;
   COLOR_OSD_TEXT[3] = a;
}

void set_Color_OSDOutline(float r, float g, float b, float a)
{
   COLOR_OSD_TEXT_OUTLINE[0] = r;
   COLOR_OSD_TEXT_OUTLINE[1] = g;
   COLOR_OSD_TEXT_OUTLINE[2] = b;
   COLOR_OSD_TEXT_OUTLINE[3] = a;
}