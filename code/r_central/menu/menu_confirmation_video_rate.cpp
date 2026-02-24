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
#include "menu_confirmation_video_rate.h"
#include "../ruby_central.h"
#include "../events.h"

MenuConfirmationVideoRate::MenuConfirmationVideoRate(int iNewDataRate)
:Menu(MENU_ID_CONFIRMATION_VIDEO_RATE, L("Video Bitrate Adjustment"), NULL)
{
   m_xPos = 0.35; m_yPos = 0.35;
   m_Width = 0.3;

   m_iNewDataRate = iNewDataRate;
   m_SelectedIndex = 1;
}

MenuConfirmationVideoRate::~MenuConfirmationVideoRate()
{
   log_line("Closed MenuConfirmationVideoRate.");
}

void MenuConfirmationVideoRate::onShow()
{
   Menu::onShow();
   m_SelectedIndex = 1;
}

void MenuConfirmationVideoRate::onSelectItem()
{
   log_line("MenuConfirmationVideoRate: selected item: %d", m_SelectedIndex);

   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( 0 == m_SelectedIndex )
   {
      menu_stack_pop(0);
      return;
   }

   if ( 1 == m_SelectedIndex )
   {
      handle_commands_send_to_vehicle(COMMAND_ID_CLEAR_LOGS, 1, NULL, 0);
      menu_stack_pop(0);
   }
}