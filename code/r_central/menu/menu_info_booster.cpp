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

#include "menu.h"
#include "menu_info_booster.h"
#include "../osd/osd_common.h"

const char* s_szInfoBoost1 = "A new radio card has been detected.\n If you have a booster or a low noise amplifier connected to the antenna of that card please set it (the new radio card) as Uplink only or Downlink only from [Menu]->[Radio Configuration]";
const char* s_szInfoBoost1B = "A new radio card (%s) has been detected.\n If you have a booster or a low noise amplifier connected to the antenna of that card please set it (the new radio card) as Uplink only or Downlink only from [Menu]->[Radio Configuration]";
const char* s_szInfoBoost2 = "A booster should be set to UPLINK ONLY";
const char* s_szInfoBoost3 = "A LNA should be set to DOWNLINK ONLY";

MenuInfoBooster::MenuInfoBooster()
:Menu(MENU_ID_INFO_BOOSTER, "Info Booster", NULL)
{
   m_xPos = 0.18; m_yPos = 0.1;
   m_Width = 0.58;
   m_Height = 0.74;
   addMenuItem(new MenuItem("Ok"));

   m_iRadioCardIndex = -1;
   m_idImg1 = g_pRenderEngine->loadIcon("res/info_booster.png");
   m_idImg2 = g_pRenderEngine->loadIcon("res/info_booster_lna.png");
}


MenuInfoBooster::~MenuInfoBooster()
{
}


void MenuInfoBooster::onShow()
{
   Menu::onShow();
}


void MenuInfoBooster::Render()
{
   char szBuff[256];

   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float w = m_RenderWidth-2.0*m_sfMenuPaddingX;
   float dx = 0.1*m_sfMenuPaddingX;
   RenderItem(0,m_RenderYPos+m_RenderHeight-m_sfMenuPaddingY-m_pMenuItems[0]->getItemHeight(getUsableWidth()), w*0.48);
   RenderEnd(yTop);

   float x = m_RenderXPos+m_sfMenuPaddingX;
   float y = m_RenderYPos + m_RenderHeaderHeight + m_sfMenuPaddingY;

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   strcpy(szBuff, s_szInfoBoost1);
   radio_hw_info_t* pRadioInfo = hardware_get_radio_info(m_iRadioCardIndex);
   if ( NULL != pRadioInfo )
   {
      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioInfo->szMAC);
      if ( NULL != pCardInfo )
      {
         char szName[128];
         strcpy(szName, str_get_radio_card_model_string(pCardInfo->cardModel) );
         if ( (0 != szName[0]) && (NULL == strstr(szName, "Generic")) )
            sprintf(szBuff, s_szInfoBoost1B, szName);
      }
   }
   float h = g_pRenderEngine->getMessageHeight(szBuff, MENU_TEXTLINE_SPACING, getUsableWidth()-dx, g_idFontMenu);
   g_pRenderEngine->drawMessageLines(x+dx, y, szBuff, MENU_TEXTLINE_SPACING, getUsableWidth()-dx, g_idFontMenu);
   g_pRenderEngine->drawIcon(x, y, dx*0.7, dx*0.7*g_pRenderEngine->getAspectRatio(), g_idIconWarning);

   y += h;
   y += m_sfMenuPaddingY;

   float wImg = m_RenderWidth-4.0*m_sfMenuPaddingX;
   float hImg = wImg*g_pRenderEngine->getAspectRatio()*0.23;

   g_pRenderEngine->drawIcon(x+wImg*0.1, y+hImg*0.1, wImg*0.8, hImg*0.8, m_idImg1);
   y += hImg*0.9;

   dx = m_RenderWidth*0.24;
   h = g_pRenderEngine->getMessageHeight(s_szInfoBoost2, MENU_TEXTLINE_SPACING, getUsableWidth()-dx, g_idFontMenuLarge);
   g_pRenderEngine->drawMessageLines(x+dx, y, s_szInfoBoost2, MENU_TEXTLINE_SPACING, getUsableWidth()-dx, g_idFontMenuLarge);
   y += h;

   g_pRenderEngine->drawIcon(x+wImg*0.1, y+hImg*0.1, wImg*0.8, hImg*0.8, m_idImg2);
   y += hImg*0.9;

   h = g_pRenderEngine->getMessageHeight(s_szInfoBoost3, MENU_TEXTLINE_SPACING, getUsableWidth()-dx, g_idFontMenuLarge);
   g_pRenderEngine->drawMessageLines(x+dx, y, s_szInfoBoost3, MENU_TEXTLINE_SPACING, getUsableWidth()-dx, g_idFontMenuLarge);
   y += h;
}

void MenuInfoBooster::onSelectItem()
{
   menu_stack_pop(1);
   return;
}