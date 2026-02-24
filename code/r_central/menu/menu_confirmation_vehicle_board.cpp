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
#include "../../base/config.h"
#include "../../base/hardware.h"
#include "../../base/hardware_procs.h"
#include "../local_stats.h"

#include "menu.h"
#include "menu_confirmation_vehicle_board.h"
#include "menu_negociate_radio.h"
#include "../ruby_central.h"
#include "../events.h"

MenuConfirmationVehicleBoard::MenuConfirmationVehicleBoard()
:Menu(MENU_ID_VEHICLE_BOARD, L("Select your vehicle board type"), NULL)
{
   m_xPos = 0.35; m_yPos = 0.35;
   m_Width = 0.30;
   char szText[256];
   sprintf(szText, L("Select your %s board type"), g_pCurrentModel->getVehicleTypeString());
   setTitle(szText);
   sprintf(szText, "The hardware detected on the %s has multiple variants.", g_pCurrentModel->getVehicleTypeString());
   addTopLine(szText);
   sprintf(szText, "Select the type of your %s board:", g_pCurrentModel->getVehicleTypeString());
   addTopLine(szText);
   addTopLine(L("(You can later change it from vehicle menu)"));

   for( int i=0; i<hardware_get_board_subtypes_count(); i++ )
   {
      const char* szBoard = str_get_hardware_board_name( (BOARD_TYPE_OPENIPC_SIGMASTAR_338Q & BOARD_TYPE_MASK) | (hardware_get_board_subtype_at_index(i)<<BOARD_SUBTYPE_SHIFT));
      addMenuItem(new MenuItem(szBoard));
   }
   m_SelectedIndex = 0;
}

MenuConfirmationVehicleBoard::~MenuConfirmationVehicleBoard()
{
}

void MenuConfirmationVehicleBoard::onShow()
{
   Menu::onShow();
   m_SelectedIndex = 0;
   u32 uSubType = 0;
   if ( NULL != g_pCurrentModel )
   {
     uSubType = (g_pCurrentModel->hwCapabilities.uBoardType & BOARD_SUBTYPE_MASK) >> BOARD_SUBTYPE_SHIFT;
     int iIndex = hardware_get_board_subtype_index(uSubType);
     if ( iIndex >= 0 )
        m_SelectedIndex = iIndex;
   }
}

int MenuConfirmationVehicleBoard::onBack()
{
   int iRet = Menu::onBack();

   if ( onEventPairingTookUIActions() )
      onEventFinishedPairUIAction();
   return iRet;
}


void MenuConfirmationVehicleBoard::onSelectItem()
{
   log_line("Menu Confirmation Vehicle Board: selected item: %d", m_SelectedIndex);

   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( NULL == g_pCurrentModel )
   {
      menu_stack_pop(0);
      return;
   }

   u32 uBoardType = g_pCurrentModel->hwCapabilities.uBoardType;
   uBoardType &= ~(u32)BOARD_SUBTYPE_MASK;
   uBoardType |= (hardware_get_board_subtype_at_index(m_SelectedIndex)) << BOARD_SUBTYPE_SHIFT;

   send_pause_adaptive_to_router(6000);
   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VEHICLE_BOARD_TYPE, uBoardType, NULL, 0) )
      valuesToUI();
}