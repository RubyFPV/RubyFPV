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
#include "menu_confirmation_delete_logs.h"
#include "../ruby_central.h"
#include "../events.h"

MenuConfirmationDeleteLogs::MenuConfirmationDeleteLogs(u32 uFreeSpaceMb, u32 uLogsSizeBytes)
:Menu(MENU_ID_CONFIRMATION_DELETE_LOGS, L("Delete vehicle logs"), NULL)
{
   m_xPos = 0.35; m_yPos = 0.35;
   m_Width = 0.3;

   m_uFreeSpaceMb = uFreeSpaceMb;
   m_uLogSizeBytes = uLogsSizeBytes;

   char szBuff[256];
   sprintf(szBuff, "Vehicle is running out of free storage space. %u Mb free storage available.", m_uFreeSpaceMb);
   addTopLine(szBuff);

   if ( m_uLogSizeBytes >= 1000000 )
      sprintf(szBuff, "Vehicle logs take up %u Mb of storage.", m_uLogSizeBytes/1000/1000);
   else
      sprintf(szBuff, "Vehicle logs take up %u bytes of storage.", m_uLogSizeBytes);
   addTopLine(szBuff);

   addTopLine(L("Do you want to delete the logs?"));
   addMenuItem(new MenuItem(L("No")));
   addMenuItem(new MenuItem(L("Yes")));
   m_SelectedIndex = 1;
}

MenuConfirmationDeleteLogs::~MenuConfirmationDeleteLogs()
{
   log_line("Closed MenuConfirmationDeleteLogs.");
}

void MenuConfirmationDeleteLogs::onShow()
{
   Menu::onShow();
   m_SelectedIndex = 1;
}

void MenuConfirmationDeleteLogs::onSelectItem()
{
   log_line("MenuConfirmationDeleteLogs: selected item: %d", m_SelectedIndex);

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