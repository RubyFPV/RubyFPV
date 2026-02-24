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
#include "menu_vehicle_rc_failsafe.h"
#include "menu_confirmation.h"

MenuVehicleRCFailsafe::MenuVehicleRCFailsafe(void)
:Menu(MENU_ID_VEHICLE_RC_FAILSAFE, "Channels Failsafe Values", NULL)
{
   m_Width = 0.32;
   m_Height = 0.0;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.26;
   m_bDisableStacking = true;
   setColumnsCount(3);
   char szBuff[128];

   m_ChannelCount = g_pCurrentModel->rc_params.channelsCount;
   for( int i=0; i<m_ChannelCount; i++ )
   {
      g_pCurrentModel->get_rc_channel_name(i, szBuff);

      m_ItemsChannels[i].pItemTitle = new MenuItem(szBuff);
      m_ItemsChannels[i].m_IndexTitle = addMenuItem(m_ItemsChannels[i].pItemTitle);

      m_ItemsChannels[i].pItemType = new MenuItemSelect("Failsafe Type", "Sets what output will be generated for this channel when a failsafe event occurs.");
      m_ItemsChannels[i].pItemType->addSelection("Keep last");
      m_ItemsChannels[i].pItemType->addSelection("Below range");
      m_ItemsChannels[i].pItemType->addSelection("Value");
      m_ItemsChannels[i].pItemType->setIsEditable();
      m_ItemsChannels[i].pItemType->setCondensedOnly();
      m_ItemsChannels[i].m_IndexType = addMenuItem(m_ItemsChannels[i].pItemType);

      m_ItemsChannels[i].pItemValue = new MenuItemRange("Value:", "Set a fixed failsafe value for this RC channel.", 500, 2500, 1000, 1 );
      m_ItemsChannels[i].pItemValue->setCondensedOnly();
      m_ItemsChannels[i].m_IndexValue = addMenuItem(m_ItemsChannels[i].pItemValue);
   }
}

MenuVehicleRCFailsafe::~MenuVehicleRCFailsafe()
{
}

void MenuVehicleRCFailsafe::onShow()
{
   Menu::onShow();
}

void MenuVehicleRCFailsafe::valuesToUI()
{
   for( int i=0; i<m_ChannelCount; i++ )
   {
      m_ItemsChannels[i].pItemType->setSelection( ((g_pCurrentModel->rc_params.rcChFlags[i] >> 1) & 0x07) - 1 );
      m_ItemsChannels[i].pItemValue->setCurrentValue(g_pCurrentModel->rc_params.rcChFailSafe[i]);

     if ( m_ItemsChannels[i].pItemType->getSelectedIndex() == 2 )
        m_ItemsChannels[i].pItemValue->setEnabled(true);
     else
        m_ItemsChannels[i].pItemValue->setEnabled(false);
   }
}


void MenuVehicleRCFailsafe::Render()
{
   RenderPrepare();
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);

   float yTop = RenderFrameAndTitle();
   float y = yTop;
   float fFirstItemWidth = 0.08*Menu::getScaleFactor();
   float fSecItemWidth = 0.12*Menu::getScaleFactor();
   for( int i=0; i<m_ItemsCount; i++ )
   {
      m_pMenuItems[i]->invalidate();
      m_pMenuItems[i]->getItemHeight(getSelectionWidth());
      m_pMenuItems[i]->getTitleWidth(getSelectionWidth());
      m_pMenuItems[i]->getValueWidth(getSelectionWidth());
      if ( (i % m_iColumnsCount) == 0 )
         m_pMenuItems[i]->Render(m_RenderXPos + m_sfMenuPaddingX, y, i == m_SelectedIndex, fFirstItemWidth);
      else
      {
         float dx = fFirstItemWidth + m_sfMenuPaddingX;
         if ( (i % m_iColumnsCount) == 2 )
            dx += fSecItemWidth + m_sfMenuPaddingX;
         m_pMenuItems[i]->RenderCondensed(m_RenderXPos + m_sfMenuPaddingX + dx, y, i == m_SelectedIndex, 0);
      }
      if ( (i % m_iColumnsCount) == m_iColumnsCount-1 )
      {
         y += m_pMenuItems[i]->getItemHeight(getUsableWidth());
         y += MENU_ITEM_SPACING * height_text;
      }
   }
   RenderEnd(yTop);
}

void MenuVehicleRCFailsafe::onSelectItem()
{
   int subIndex = m_SelectedIndex % m_iColumnsCount;

   if ( subIndex == 0 )
   {
      Menu::onSelectItem();
      return;
   }

   Menu::onSelectItem();

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   int index = m_SelectedIndex/m_iColumnsCount;
   int fstype = m_ItemsChannels[index].pItemType->getSelectedIndex();
   int fsvalue = m_ItemsChannels[index].pItemValue->getCurrentValue();

   rc_parameters_t params;
   memcpy(&params, &g_pCurrentModel->rc_params, sizeof(rc_parameters_t));

   params.rcChFlags[index] &= ~(0x07<<1);
   params.rcChFlags[index] |= ((fstype+1)<<1);
   params.rcChFailSafe[index] = fsvalue;

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t)) )
      valuesToUI();
}