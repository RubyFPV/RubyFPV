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
#include "menu_vehicle_rc_expo.h"
#include "menu_confirmation.h"
#include "../../base/utils.h"


MenuVehicleRCExpo::MenuVehicleRCExpo(void)
:Menu(MENU_ID_VEHICLE_RC_EXPO, "Channels Expo", NULL)
{
   m_Width = 0.28;
   m_Height = 0.0;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.24;
   m_bDisableStacking = true;

   setColumnsCount(1);
   char szBuff[128];

   m_ChannelCount = g_pCurrentModel->rc_params.channelsCount;
   for( int i=0; i<m_ChannelCount; i++ )
   {
      g_pCurrentModel->get_rc_channel_name(i, szBuff);
      //m_ItemsChannels[i].pItemTitle = new MenuItem(szBuff);
      //m_ItemsChannels[i].m_IndexTitle = addMenuItem(m_ItemsChannels[i].pItemTitle);

      m_ItemsChannels[i].pItemValue = new MenuItemRange(szBuff, "Set an exponential curve value for this RC channel.", 0, 90, 0, 1 );
      m_ItemsChannels[i].pItemValue->setSufix("%");
      m_ItemsChannels[i].m_IndexValue = addMenuItem(m_ItemsChannels[i].pItemValue);
   }
   m_IndexComputedChannel = 0;
   computeDisplayCurve(0);
}

MenuVehicleRCExpo::~MenuVehicleRCExpo()
{
}

void MenuVehicleRCExpo::onShow()
{
   Menu::onShow();
   int index = m_SelectedIndex/m_iColumnsCount;
   computeDisplayCurve(index);
}

void MenuVehicleRCExpo::valuesToUI()
{
   for( int i=0; i<m_ChannelCount; i++ )
      m_ItemsChannels[i].pItemValue->setCurrentValue(g_pCurrentModel->rc_params.rcChExpo[i]);

   int index = m_SelectedIndex/m_iColumnsCount;
   computeDisplayCurve(index);
}

void MenuVehicleRCExpo::computeDisplayCurve(int nChannel)
{
   m_IndexComputedChannel = nChannel;

   for( int i=0; i<DISPLAY_CURVE_VALUES; i++ )
   {
      float fPercent = (float)i/(float)(DISPLAY_CURVE_VALUES-1);
      m_iDisplayCurveValues[i] = compute_output_rc_value(g_pCurrentModel, m_IndexComputedChannel, 0, fPercent, 2);
   }
}

void MenuVehicleRCExpo::Render()
{
   RenderPrepare();

   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   float yBottom = y;
   int index = m_SelectedIndex/m_iColumnsCount;
   if ( index != m_IndexComputedChannel )
      computeDisplayCurve(index);

   RenderEnd(yTop);

   // Draw channel graph (xy plot)

   float fWidthLegendValue = g_pRenderEngine->textWidth(g_idFontMenuSmall, "000%");
   float height_text_small = g_pRenderEngine->textHeight(g_idFontMenuSmall);
   float fWidthGraph = 0.07 * m_sfScaleFactor;
   float fHeightGraph = fWidthGraph * g_pRenderEngine->getAspectRatio();
   float fMarginText = 0.2*height_text_small;
   float xPosGraph = m_RenderXPos + getSelectionWidth() + 2.0*Menu::getMenuPaddingX() + fWidthLegendValue + fMarginText;
   float yPosGraph = (yBottom + yTop) * 0.5 - fHeightGraph * 0.5;

   float fAlfa = g_pRenderEngine->setGlobalAlfa(0.5);
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(1);
   g_pRenderEngine->drawLine(xPosGraph, yPosGraph, xPosGraph+fWidthGraph, yPosGraph);
   g_pRenderEngine->drawLine(xPosGraph, yPosGraph+fHeightGraph, xPosGraph+fWidthGraph, yPosGraph+fHeightGraph);
   g_pRenderEngine->drawLine(xPosGraph, yPosGraph, xPosGraph, yPosGraph+fHeightGraph);
   g_pRenderEngine->drawLine(xPosGraph+fWidthGraph, yPosGraph, xPosGraph+fWidthGraph, yPosGraph+fHeightGraph);

   for( float x = xPosGraph; x<=xPosGraph+fWidthGraph-0.01/g_pRenderEngine->getAspectRatio(); x += 0.01/g_pRenderEngine->getAspectRatio() )
      g_pRenderEngine->drawLine(x, yPosGraph+fHeightGraph*0.5, x + 0.005/g_pRenderEngine->getAspectRatio(), yPosGraph+fHeightGraph*0.5);

   for( float y = yPosGraph; y<=yPosGraph+fHeightGraph-0.01; y += 0.01)
      g_pRenderEngine->drawLine(xPosGraph + fWidthGraph*0.5, y, xPosGraph + fWidthGraph*0.5, y + 0.005);

   g_pRenderEngine->setGlobalAlfa(fAlfa);
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(1);

   g_pRenderEngine->drawText(xPosGraph, yPosGraph+fHeightGraph, g_idFontMenuSmall, "0%");
   g_pRenderEngine->drawTextLeft(xPosGraph+fWidthGraph, yPosGraph+fHeightGraph, g_idFontMenuSmall, "100%");

   char szBuff[32];
   sprintf(szBuff, "%d", g_pCurrentModel->rc_params.rcChMax[m_IndexComputedChannel]);
   g_pRenderEngine->drawTextLeft(xPosGraph-fMarginText, yPosGraph, g_idFontMenuSmall, szBuff);
   sprintf(szBuff, "%d", g_pCurrentModel->rc_params.rcChMid[m_IndexComputedChannel]);
   g_pRenderEngine->drawTextLeft(xPosGraph-fMarginText, yPosGraph+fHeightGraph*0.5 - height_text_small*0.5, g_idFontMenuSmall, szBuff);
   sprintf(szBuff, "%d", g_pCurrentModel->rc_params.rcChMin[m_IndexComputedChannel]);
   g_pRenderEngine->drawTextLeft(xPosGraph-fMarginText, yPosGraph+fHeightGraph-height_text_small, g_idFontMenuSmall, szBuff);

   if ( g_pCurrentModel->rc_params.rcChMax[m_IndexComputedChannel] < g_pCurrentModel->rc_params.rcChMin[m_IndexComputedChannel]+0.001 )
      return;

   for( int i=0; i<DISPLAY_CURVE_VALUES-1; i++ )
   {
      float percent1 = ((float)i)/(float)(DISPLAY_CURVE_VALUES-1);
      float percent2 = ((float)i+1)/(float)(DISPLAY_CURVE_VALUES-1);
      float x1 = xPosGraph + fWidthGraph*percent1;
      float x2 = xPosGraph + fWidthGraph*percent2;
      float y1 = yPosGraph + fHeightGraph - fHeightGraph * (m_iDisplayCurveValues[i] - g_pCurrentModel->rc_params.rcChMin[m_IndexComputedChannel])/((g_pCurrentModel->rc_params.rcChMax[m_IndexComputedChannel]-g_pCurrentModel->rc_params.rcChMin[m_IndexComputedChannel]));
      float y2 = yPosGraph + fHeightGraph - fHeightGraph * (m_iDisplayCurveValues[i+1] - g_pCurrentModel->rc_params.rcChMin[m_IndexComputedChannel])/((g_pCurrentModel->rc_params.rcChMax[m_IndexComputedChannel]-g_pCurrentModel->rc_params.rcChMin[m_IndexComputedChannel]));
      g_pRenderEngine->drawLine(x1,y1,x2,y2);
   }
}

void MenuVehicleRCExpo::onItemValueChanged(int itemIndex)
{
   Menu::onItemValueChanged(itemIndex);

   rc_parameters_t params;
   memcpy(&params, &g_pCurrentModel->rc_params, sizeof(rc_parameters_t));

   int fsvalue = m_ItemsChannels[m_IndexComputedChannel].pItemValue->getCurrentValue();
   g_pCurrentModel->rc_params.rcChExpo[m_IndexComputedChannel] = fsvalue;

   computeDisplayCurve(m_IndexComputedChannel);
   memcpy(&g_pCurrentModel->rc_params, &params, sizeof(rc_parameters_t));
}

void MenuVehicleRCExpo::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;


   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   int index = m_SelectedIndex/m_iColumnsCount;
   int fsvalue = m_ItemsChannels[index].pItemValue->getCurrentValue();

   rc_parameters_t params;
   memcpy(&params, &g_pCurrentModel->rc_params, sizeof(rc_parameters_t));

   params.rcChExpo[index] = fsvalue;

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t)) )
      valuesToUI();
}