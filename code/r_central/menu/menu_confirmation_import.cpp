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
#include "../../base/ctrl_interfaces.h"
#include "../../base/ctrl_settings.h"
#include "../../base/hardware.h"
#include "../../base/hardware_procs.h"
#include "../../utils/utils_controller.h"
#include "../local_stats.h"

#include "menu.h"
#include "menu_confirmation_import.h"
#include "../ruby_central.h"
#include "../events.h"
#include "../pairing.h"


MenuConfirmationImport::MenuConfirmationImport(const char* szTitle, const char* szText, int id)
:Menu(MENU_ID_CONFIRMATION + id*1000, szTitle, NULL)
{
   m_xPos = 0.35; m_yPos = 0.35;
   m_Width = 0.3;
   addTopLine(szText);
   addMenuItem(new MenuItem(L("No")));
   addMenuItem(new MenuItem(L("Yes")));
   m_SelectedIndex = 0;
}

MenuConfirmationImport::~MenuConfirmationImport()
{
}

void MenuConfirmationImport::onShow()
{
   Menu::onShow();
   m_SelectedIndex = 0;
}

void MenuConfirmationImport::onSelectItem()
{
   log_line("Menu Confirmation Import: selected item: %d", m_SelectedIndex);

   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( 0 == m_SelectedIndex )
   {
      menu_stack_pop(1);
      return;
   }

   if ( 1 == m_SelectedIndex )
   {
      menu_stack_pop(0);
      int nResult = -1;
      pairing_stop();
      if ( controller_utils_usb_import_has_matching_controller_id_file() )
         nResult = controller_utils_import_all_from_usb(false);
      else if ( controller_utils_usb_import_has_any_controller_id_file() )
         nResult = controller_utils_import_all_from_usb(true);
      if ( nResult == 0 )
      {
         ruby_load_models();

         if ( ! load_Preferences() )
            save_Preferences();

         if ( ! load_ControllerSettings() )
            save_ControllerSettings();

         if ( ! load_ControllerInterfacesSettings() )
            save_ControllerInterfacesSettings();

         hardware_reboot();
      }
      else
         addMessage(L("Failed to import settings from USB memory stick."));
      return;
   }
}