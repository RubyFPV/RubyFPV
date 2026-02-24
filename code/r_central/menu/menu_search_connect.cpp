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

#include "menu_search_connect.h"
#include "../../common/models_connect_frequencies.h"
#include "../osd/osd_common.h"
#include "menu.h"

MenuSearchConnect::MenuSearchConnect(void)
//:Menu(MENU_ID_SEARCH_CONNECT, "Found Vehicle", "")
:Menu(MENU_ID_CONFIRMATION+1000*1, L("Found Vehicle"), "") // Id confirmation to keep the parent menu on screen too
{
   m_iSearchModelTypes = 0;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   m_Width = 0.50 + 3.0*height_text;
   m_xPos = menu_get_XStartPos(this, m_Width);
   m_yPos = 0.32;
   m_bSpectatorOnlyMode = false;

   m_iIndexController = -1;
   m_iIndexSpectator = -1;
   m_iIndexSkip = -1;
}

void MenuSearchConnect::setSpectatorOnly()
{
   m_bSpectatorOnlyMode = true;
}

void MenuSearchConnect::setCurrentFrequency(u32 freq)
{
   m_CurrentSearchFrequency = freq;
}

void MenuSearchConnect::onShow()
{
   Menu::onShow();

   removeAllItems();
   m_iIndexController = -1;
   m_iIndexSpectator = -1;
   m_iIndexSkip = -1;

   int iCountLinks = 0;
   if ( g_SearchVehicleRuntimeInfo.bGotRubyTelemetryInfo )
   for( int i=0; i<g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.radio_links_count; i++ )
   {
      if ( ! (g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRelayLinks & (1<<i)) )
         iCountLinks++;
   }

   if ( iCountLinks > 1 )
      m_Width = 0.54;
   if ( (!m_bSpectatorOnlyMode) && (m_iSearchModelTypes == 0) )
   {
      m_iIndexController = addMenuItem(new MenuItem( L("Connect for control"), L("Connect to this vehicle as controller")));
      addExtraHeightAtEnd(g_pRenderEngine->textHeight(g_idFontMenu));
   }
   else
      addExtraHeightAtEnd(2.0*g_pRenderEngine->textHeight(g_idFontMenu));

   m_iIndexSpectator = addMenuItem(new MenuItem(L("View as spectator"), L("Connect to this vehicle as spectator")));
   m_iIndexSkip = addMenuItem(new MenuItem(L("Skip"), L("Skip this vehicle")));
}

void MenuSearchConnect::Render()
{
   RenderPrepare();
   float yTop = Menu::RenderFrameAndTitle();
   float y0 = yTop;
   float y = y0;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   char szBuff[128];
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);

   float xPos = m_xPos + 3*m_sfMenuPaddingX + getSelectionWidth();
   float fMaxWidth = getUsableWidth() - 3*m_sfMenuPaddingX - getSelectionWidth();
   y = y0;

   g_pRenderEngine->setColors(get_Color_MenuText());

   if ( g_SearchVehicleRuntimeInfo.bGotRubyTelemetryInfo )
   {
      g_pRenderEngine->setColors(get_Color_MenuText(), 0.8);
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      g_pRenderEngine->setColors(get_Color_MenuText());
   }

   sprintf(szBuff,"Found vehicle on %s", str_format_frequency(m_CurrentSearchFrequency));
   g_pRenderEngine->drawMessageLines(xPos, y, szBuff, MENU_TEXTLINE_SPACING, fMaxWidth, g_idFontMenu);

   y += height_text *(1.0+MENU_ITEM_SPACING);
   if ( ! g_SearchVehicleRuntimeInfo.bGotRubyTelemetryInfo )
   {
      sprintf(szBuff, L("Can't get vehicle info"));
      g_pRenderEngine->drawMessageLines(xPos, y, szBuff, MENU_TEXTLINE_SPACING, fMaxWidth, g_idFontMenu);
      y += height_text *(1.0+MENU_ITEM_SPACING);
      RenderEnd(yTop);
      return;
   }

   sprintf(szBuff,"Type: %s, Name: ", Model::getVehicleType(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_type));
   if ( 0 == g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_name[0] )
      strcat(szBuff, "No Name");
   else
      strncat(szBuff, (char*)g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_name, MAX_VEHICLE_NAME_LENGTH);

   g_pRenderEngine->drawMessageLines(xPos, y, szBuff, MENU_TEXTLINE_SPACING, fMaxWidth, g_idFontMenu);
   y += height_text *(1.0+MENU_ITEM_SPACING);

   char szLinks[256];
   szLinks[0] = 0;
   int iCountLinks = 0;
   if ( g_SearchVehicleRuntimeInfo.bGotRubyTelemetryInfo )
   for( int i=0; i<g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.radio_links_count; i++ )
   {
      // Not a relay link (bit N - relay link)
      if ( ! (g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRelayLinks & (1<<i)) )
      {
         iCountLinks++;
         if ( 0 != szLinks[0] )
            strcat(szLinks, ", ");
         char szTmp[32];
         sprintf(szTmp, "%s", str_format_frequency(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRadioFrequenciesKhz[i]) );
         strcat(szLinks, szTmp);
      }
   }

   if ( iCountLinks > 1 )
   {
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Vehicle has %d radio links on: %s", iCountLinks, szLinks);
      g_pRenderEngine->drawMessageLines(xPos, y, szBuff, MENU_TEXTLINE_SPACING, fMaxWidth, g_idFontMenu);
      y += g_pRenderEngine->getMessageHeight(szBuff, MENU_TEXTLINE_SPACING, fMaxWidth, g_idFontMenu);
      y += height_text * MENU_ITEM_SPACING;
   }

   if ( ((g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_type & MODEL_FIRMWARE_MASK) >> 5) == MODEL_FIRMWARE_TYPE_RUBY )
   {
      u8 vMaj = g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.rubyVersion;
      u8 vMin = g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.rubyVersion;
      vMaj = vMaj >> 4;
      vMin = vMin & 0x0F;
      //sprintf(szBuff,"Id: %u, ver: %d.%d", g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uVehicleId, vMaj, vMin);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Version: %d.%d", vMaj, vMin);
      g_pRenderEngine->drawMessageLines(xPos, y, szBuff, MENU_TEXTLINE_SPACING, fMaxWidth, g_idFontMenu);
      y += height_text *(1.0+MENU_ITEM_SPACING);
   }
   y += height_text *0.6;

   RenderEnd(yTop);
}

void MenuSearchConnect::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   // Add model as controller
   if ( m_iIndexController == m_SelectedIndex )
   {
      menu_stack_pop(2);
      warnings_remove_all();
      return;
   }

   // Add model as spectator
   if ( m_iIndexSpectator == m_SelectedIndex )
   {
      menu_stack_pop(1);
      warnings_remove_all();
      return;
   }

   // Skip vehicle
   if ( m_iIndexSkip == m_SelectedIndex )
   {
      menu_stack_pop(0);
      return;
   }
}
