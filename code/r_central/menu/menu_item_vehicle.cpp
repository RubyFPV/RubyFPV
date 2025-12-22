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

#include "menu_item_vehicle.h"
#include "menu_objects.h"
#include "menu.h"
#include "osd_common.h"

MenuItemVehicle::MenuItemVehicle(const char* title)
:MenuItem(title)
{
   m_iVehicleIndex = -1;
   m_bIsSpectator = false;
}

MenuItemVehicle::MenuItemVehicle(const char* title, const char* tooltip)
:MenuItem(title, tooltip)
{
   m_iVehicleIndex = -1;
   m_bIsSpectator = false;
}

MenuItemVehicle::~MenuItemVehicle()
{
}

void MenuItemVehicle::setVehicleIndex(int vehicleIndex, bool bIsSpectator)
{
   m_iVehicleIndex = vehicleIndex;
   m_bIsSpectator = bIsSpectator;
}


void MenuItemVehicle::beginEdit()
{
   MenuItem::beginEdit();
}

void MenuItemVehicle::endEdit(bool bCanceled)
{
   MenuItem::endEdit(bCanceled);
}


float MenuItemVehicle::getItemHeight(float maxWidth)
{
   m_RenderTitleHeight = g_pRenderEngine->textHeight(g_idFontMenu);
   m_RenderHeight = m_RenderTitleHeight*1.2;
   if ( (-1 != m_iVehicleIndex) && (NULL != g_pCurrentModel) && (!g_pCurrentModel->is_spectator) )
   {
      Model *pModel = getModelAtIndex(m_iVehicleIndex);
      if ( pModel->uVehicleId == g_pCurrentModel->uVehicleId )
      {
         m_RenderTitleHeight *= 2.0;
         m_RenderHeight = m_RenderTitleHeight*1.1;
      }
   }
   m_RenderHeight += m_fExtraHeight;
   return m_RenderHeight;
}


float MenuItemVehicle::getTitleWidth(float maxWidth)
{
   if ( m_RenderTitleWidth > 0.001 )
      return m_RenderTitleWidth;

   if ( g_pControllerSettings->iDeveloperMode )
   {
      m_RenderTitleWidth = maxWidth;
      return m_RenderTitleWidth;
   }

   m_RenderTitleWidth = g_pRenderEngine->textWidth(g_idFontMenu, m_pszTitle);

   m_RenderTitleWidth += getItemHeight(0.0);
   if ( m_bShowArrow )
      m_RenderTitleWidth += 2.0*g_pRenderEngine->textHeight(g_idFontMenu);
   return m_RenderTitleWidth;
}

float MenuItemVehicle::getValueWidth(float maxWidth)
{
   return 0.0;
}


void MenuItemVehicle::Render(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   if ( m_iVehicleIndex == -1 )
   {
      m_RenderLastY = yPos;
      m_RenderLastX = xPos;
      RenderBaseTitle(xPos, yPos, bSelected, fWidthSelection);
      return;
   }

   Model *pModel = NULL;

   if ( m_bIsSpectator )
      pModel = getSpectatorModel(m_iVehicleIndex);
   else
      pModel = getModelAtIndex(m_iVehicleIndex);
   
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   u32 idIcon = 0;
   if ( NULL != pModel )
      idIcon = osd_getVehicleIcon(pModel->vehicle_type);
   
   bool bIsCurrentVehicle = false;
   bool bIsCurrentSpectatorVehicle = false;
   if ( !m_bIsSpectator )
   if ( (NULL != pModel) && (NULL != g_pCurrentModel) && (!g_pCurrentModel->is_spectator) && (g_pCurrentModel->uVehicleId == pModel->uVehicleId) )
      bIsCurrentVehicle = true;
   if ( m_bIsSpectator )
   if ( (NULL != pModel) && (NULL != g_pCurrentModel) && g_pCurrentModel->is_spectator && (g_pCurrentModel->uVehicleId == pModel->uVehicleId) )
      bIsCurrentSpectatorVehicle = true;

   float fIconHeight = height_text;
   if ( bIsCurrentVehicle )
      fIconHeight = height_text*1.9;
   float fIconWidth = fIconHeight/g_pRenderEngine->getAspectRatio();

    double pC[4];
   memcpy(pC, get_Color_MenuBg(), 4*sizeof(double));
   pC[0] += 40; pC[1] += 35; pC[2] += 30;
         
   if ( bIsCurrentVehicle )
   {
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setFill(pC[0], pC[1], pC[2], pC[3]);
      g_pRenderEngine->setStrokeSize(1);

      float maxWidth = fWidthSelection + Menu::getSelectionPaddingX();
      float h2 = getItemHeight(0.0) + 0.5*Menu::getSelectionPaddingY();

      g_pRenderEngine->drawRoundRect(xPos-0.5*Menu::getSelectionPaddingX(), yPos, maxWidth, h2, 0.1*Menu::getMenuPaddingY());
      g_pRenderEngine->setColors(get_Color_MenuText());
   }
   else if ( bIsCurrentSpectatorVehicle )
   {
      g_pRenderEngine->setFill(pC[0], pC[1], pC[2], pC[3]);
      g_pRenderEngine->setStrokeSize(1);

      float maxWidth = fWidthSelection + Menu::getSelectionPaddingX();
      float h2 = getItemHeight(0.0) + 0.6*Menu::getSelectionPaddingY();

      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->drawRoundRect(xPos-0.5*Menu::getSelectionPaddingX(), yPos-0.4*Menu::getSelectionPaddingY(), maxWidth, h2, 0.1*Menu::getMenuPaddingY());
      g_pRenderEngine->setColors(get_Color_MenuText());
   }

   g_pRenderEngine->setColors(get_Color_MenuText(), 0.7);
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   if ( bIsCurrentVehicle )
      g_pRenderEngine->drawIcon(xPos, yPos + height_text*0.2, fIconWidth, fIconHeight, idIcon);
   else
      g_pRenderEngine->drawIcon(xPos, yPos + height_text*0.02, fIconWidth, fIconHeight, idIcon);
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   float yText = yPos;
   if ( bIsCurrentVehicle )
      yText += height_text*0.6;

   RenderBaseTitle(xPos+fIconWidth + height_text*0.5, yText, bSelected, fWidthSelection - fIconWidth - height_text*0.8);

   if ( g_pControllerSettings->iDeveloperMode )
   {
      if ( bSelected && (!m_bIsEditing) )
         g_pRenderEngine->setColors(get_Color_MenuItemSelectedText());
      else
      {
         if ( m_bEnabled )
            g_pRenderEngine->setColors(get_Color_Dev());
         else
            g_pRenderEngine->setColors(get_Color_MenuItemDisabledText());
      }
      char szId[64];
      if ( NULL != pModel )
         sprintf(szId, "VID: %u", pModel->uVehicleId);
      else
         strcpy(szId, "VID: N/A");
      g_pRenderEngine->drawTextLeft(xPos + fWidthSelection - 1.4*Menu::getSelectionPaddingX(), yText, g_idFontMenu, szId);
   }
}

void MenuItemVehicle::RenderCondensed(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   MenuItem::RenderCondensed(xPos, yPos, bSelected, fWidthSelection);
}
