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

#include "menu.h"
#include "menu_confirmation_import_key.h"
#include "../ruby_central.h"


MenuConfirmationImportKey::MenuConfirmationImportKey(const char* szTitle, const char* szText, int id)
:Menu(MENU_ID_IMPORT_ENC_KEY+id*1000, szTitle, NULL)
{
   m_bDisableStacking = false;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.35;
   m_Width = 0.3;
   addTopLine(szText);
   addTopLine("Copy your custom encryption key (usually named gs.key) to a memory stick, insert the memory stick into this controller and then select 'Import'");
   addMenuItem(new MenuItem("Import from memory stick"));
   m_SelectedIndex = 0;
}

MenuConfirmationImportKey::~MenuConfirmationImportKey()
{
}

void MenuConfirmationImportKey::onShow()
{
   Menu::onShow();
   m_SelectedIndex = 0;
}

void MenuConfirmationImportKey::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);
   if ( iChildMenuId/1000 == 1 )
   {
      log_line("Closing import menu.");
      menu_stack_pop(0);
      return;
   }
}

void MenuConfirmationImportKey::onSelectItem()
{
   log_line("Menu Confirmation Import: selected item: %d", m_SelectedIndex);

   if ( 0 != m_SelectedIndex )
      return;

   if ( 1 != hardware_try_mount_usb() )
   {
      addMessage("No USB memory stick detected. Please insert a USB stick.");
      return;
   }

   log_line("Searching for OpenIPC keys on memory stick...");
   ruby_pause_watchdog("searching keys on memory stick");

   char szComm[256];
   char szOutput[1024];
   char szInputFile[256];
   char szOutputFile[256];
   sprintf(szComm, "find %s/gs.key 2>/dev/null", FOLDER_USB_MOUNT);
   hw_execute_bash_command(szComm, szOutput);

   if ( (strlen(szOutput) < 5) || (NULL == strstr(szOutput, "gs.key")) )
   {
      sprintf(szComm, "find %s/openipc_default.key 2>/dev/null", FOLDER_USB_MOUNT);
      hw_execute_bash_command(szComm, szOutput);
      if ( (strlen(szOutput) < 5) || (NULL == strstr(szOutput, "openipc_default.key")) )
      {
         hardware_unmount_usb();
         ruby_resume_watchdog("searching keys on memory stick");
         addMessage("No file named `gs.key` or `openipc_default.key` found on the memory stick.");
         return;
      }
   }

   strcpy(szInputFile, szOutput);

   strcpy(szOutputFile, FILE_DEFAULT_OPENIPC_KEYS);

   sprintf(szOutput, "%s.bk", FILE_DEFAULT_OPENIPC_KEYS);
   if ( access(szOutput, R_OK) == -1 )
   {
      sprintf(szComm, "cp -rf %s %s.bk", FILE_DEFAULT_OPENIPC_KEYS, FILE_DEFAULT_OPENIPC_KEYS);
      hw_execute_bash_command(szComm, NULL);
   }

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s", szOutputFile);
   hw_execute_bash_command(szComm, NULL);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s %s", szInputFile, szOutputFile);
   hw_execute_bash_command(szComm, NULL);

   if ( access(szOutputFile, R_OK ) == -1 )
   {
      hardware_unmount_usb();
      ruby_resume_watchdog("searching keys on memory stick");
      addMessage("Could not copy the key found on the memory stick.");
      return;
   }

   addMessage(1, "Encryption key found on memory stick and it was imported.");
   hardware_unmount_usb();
   ruby_resume_watchdog("searching keys on memory stick");
}
