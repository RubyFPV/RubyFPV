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

#include "../../base/base.h"
#include "../../base/models.h"
#include "../../base/config.h"
#include "../../base/commands.h"
#include "../../common/string_utils.h"
#include "menu.h"
#include "menu_channels_select.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

#include "../shared_vars.h"
#include "../pairing.h"
#include "../ruby_central.h"

u32 s_uChannelsSelect433Band = 0;
u32 s_uChannelsSelect868Band = 0;
u32 s_uChannelsSelect23Band = 0;
u32 s_uChannelsSelect24Band = 0;
u32 s_uChannelsSelect25Band = 0;
u32 s_uChannelsSelect58Band = 0;

MenuChannelsSelect::MenuChannelsSelect(u32 uFrequencyBands, int iId)
:Menu(MENU_ID_CHANNELS_SELECT+iId*1000, L("Select Frequencies"), NULL)
{
   m_Width = 0.45;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.20;

   setColumnsCount(4);
   enableColumnSelection(false);

   addMenuItem(new MenuItem(L("Select All"), ""));
   addMenuItem(new MenuItem(L("Deselect All"), ""));

   m_uFrequencyBands = uFrequencyBands;
   char szBuff[64];

   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_23 )
      for( int i=0; i<getChannels23Count(); i++ )
      {
         u32 freq = getChannels23()[i];
         strcpy(szBuff, str_format_frequency(freq));
         MenuItemCheckbox* pItem = new MenuItemCheckbox(szBuff);
         pItem->setNotEditable();
         if ( s_uChannelsSelect23Band & (((u32)0x01)<<i) )
            pItem->setChecked(true);
         else
            pItem->setChecked(false);
         addMenuItem( pItem );
      }

   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_24 )
      for( int i=0; i<getChannels24Count(); i++ )
      {
         u32 freq = getChannels24()[i];
         strcpy(szBuff, str_format_frequency(freq));
         MenuItemCheckbox* pItem = new MenuItemCheckbox(szBuff);
         pItem->setNotEditable();
         if ( s_uChannelsSelect24Band & (((u32)0x01)<<i) )
            pItem->setChecked(true);
         else
            pItem->setChecked(false);
         addMenuItem( pItem );
      }

   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_25 )
      for( int i=0; i<getChannels25Count(); i++ )
      {
         u32 freq = getChannels25()[i];
         strcpy(szBuff,str_format_frequency(freq));
         MenuItemCheckbox* pItem = new MenuItemCheckbox(szBuff);
         pItem->setNotEditable();
         if ( s_uChannelsSelect25Band & (((u32)0x01)<<i) )
            pItem->setChecked(true);
         else
            pItem->setChecked(false);
         addMenuItem( pItem );
      }

   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_58 )
      for( int i=0; i<getChannels58Count(); i++ )
      {
         u32 freq = getChannels58()[i];
         strcpy(szBuff, str_format_frequency(freq));
         MenuItemCheckbox* pItem = new MenuItemCheckbox(szBuff);
         pItem->setNotEditable();
         if ( s_uChannelsSelect58Band & (((u32)0x01)<<i) )
            pItem->setChecked(true);
         else
            pItem->setChecked(false);
         addMenuItem( pItem );
      }
}

MenuChannelsSelect::~MenuChannelsSelect()
{
}

void MenuChannelsSelect::valuesToUI()
{
   int itemIndex = 2;

   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_23 )
      for( int i=0; i<getChannels23Count(); i++ )
      {
         MenuItemCheckbox* pItem = (MenuItemCheckbox*)m_pMenuItems[itemIndex];
         if ( s_uChannelsSelect23Band & (((u32)0x01)<<i) )
            pItem->setChecked(true);
         else
            pItem->setChecked(false);
         itemIndex++;
      }

   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_24 )
      for( int i=0; i<getChannels24Count(); i++ )
      {
         MenuItemCheckbox* pItem = (MenuItemCheckbox*)m_pMenuItems[itemIndex];
         if ( s_uChannelsSelect24Band & (((u32)0x01)<<i) )
            pItem->setChecked(true);
         else
            pItem->setChecked(false);
         itemIndex++;
      }

   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_25 )
      for( int i=0; i<getChannels25Count(); i++ )
      {
         MenuItemCheckbox* pItem = (MenuItemCheckbox*)m_pMenuItems[itemIndex];
         if ( s_uChannelsSelect25Band & (((u32)0x01)<<i) )
            pItem->setChecked(true);
         else
            pItem->setChecked(false);
         itemIndex++;
      }

   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_58 )
      for( int i=0; i<getChannels58Count(); i++ )
      {
         MenuItemCheckbox* pItem = (MenuItemCheckbox*)m_pMenuItems[itemIndex];
         if ( s_uChannelsSelect58Band & (((u32)0x01)<<i) )
            pItem->setChecked(true);
         else
            pItem->setChecked(false);
         itemIndex++;
      }
}

void MenuChannelsSelect::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   y += RenderItem(0,y);
   y += RenderItem(1,y);

   float yStart = y;
   int quarter = (m_ItemsCount-2)/4;
   for( int i=0; i<quarter; i++ )
      y += RenderItem(i+2,y);

   y = yStart;
   for( int i=quarter; i<2*quarter; i++ )
      y += RenderItem(i+2, y, getUsableWidth() + m_sfMenuPaddingX);

   y = yStart;
   for( int i=2*quarter; i<3*quarter; i++ )
      y += RenderItem(i+2, y, 2*(getUsableWidth() + m_sfMenuPaddingX));

   y = yStart;
   for( int i=3*quarter; i<m_ItemsCount-2; i++ )
      y += RenderItem(i+2, y, 3*(getUsableWidth() + m_sfMenuPaddingX));

   RenderEnd(yTop);
}

int MenuChannelsSelect::onBack()
{
   int itemIndex = 2;

   s_uChannelsSelect23Band = 0;
   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_23 )
      for( int i=0; i<getChannels23Count(); i++ )
      {
         MenuItemCheckbox* pItem = (MenuItemCheckbox*)m_pMenuItems[itemIndex];
         if ( pItem->isChecked() )
            s_uChannelsSelect23Band |= (((u32)0x01)<<i);
         itemIndex++;
      }

   s_uChannelsSelect24Band = 0;
   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_24 )
      for( int i=0; i<getChannels24Count(); i++ )
      {
         MenuItemCheckbox* pItem = (MenuItemCheckbox*)m_pMenuItems[itemIndex];
         if ( pItem->isChecked() )
            s_uChannelsSelect24Band |= (((u32)0x01)<<i);
         itemIndex++;
      }

   s_uChannelsSelect25Band = 0;
   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_25 )
      for( int i=0; i<getChannels25Count(); i++ )
      {
         MenuItemCheckbox* pItem = (MenuItemCheckbox*)m_pMenuItems[itemIndex];
         if ( pItem->isChecked() )
            s_uChannelsSelect25Band |= (((u32)0x01)<<i);
         itemIndex++;
      }

   s_uChannelsSelect58Band = 0;
   if ( m_uFrequencyBands & RADIO_HW_SUPPORTED_BAND_58 )
      for( int i=0; i<getChannels58Count(); i++ )
      {
         MenuItemCheckbox* pItem = (MenuItemCheckbox*)m_pMenuItems[itemIndex];
         if ( pItem->isChecked() )
            s_uChannelsSelect58Band |= (((u32)0x01)<<i);
         itemIndex++;
      }

   menu_stack_pop(1);
   return 1;
}

void MenuChannelsSelect::onSelectItem()
{
   Menu::onSelectItem();

   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( 0 == m_SelectedIndex )
   {
      s_uChannelsSelect433Band = MAX_U32;
      s_uChannelsSelect868Band = MAX_U32;
      s_uChannelsSelect23Band = MAX_U32;
      s_uChannelsSelect24Band = MAX_U32;
      s_uChannelsSelect25Band = MAX_U32;
      s_uChannelsSelect58Band = MAX_U32;
      valuesToUI();
   }
   if ( 1 == m_SelectedIndex )
   {
      s_uChannelsSelect433Band = 0;
      s_uChannelsSelect868Band = 0;
      s_uChannelsSelect23Band = 0;
      s_uChannelsSelect24Band = 0;
      s_uChannelsSelect25Band = 0;
      s_uChannelsSelect58Band = 0;
      valuesToUI();
   }
}
