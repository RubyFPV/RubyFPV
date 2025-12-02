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
#include "../ruby_central.h"

#include "menu.h"
#include "menu_confirmation_sdcard_update.h"
#include "../ruby_central.h"
#include "../events.h"
#include "../osd/osd_common.h"

MenuConfirmationSDCardUpdate::MenuConfirmationSDCardUpdate()
:Menu(MENU_ID_CONFIRMATION_SDCARD_UPDATE, L("Update from SD card"), NULL)
{
   m_xPos = 0.35; m_yPos = 0.35;
   m_Width = 0.3;
   m_bDoingUpdate = false;
   m_bUpdateFinished = false;

   setIconId(g_idIconSDCard);
   addTopLine(L("A Ruby update is present on the SD card."));
   addTopLine(L("Do you want to update Ruby from the SD card?"));
   addMenuItem(new MenuItem(L("No")));
   addMenuItem(new MenuItem(L("Yes")));
   m_SelectedIndex = 1;
}

MenuConfirmationSDCardUpdate::~MenuConfirmationSDCardUpdate()
{
   log_line("Closed MenuConfirmationSDCardUpdate.");
}

void MenuConfirmationSDCardUpdate::onShow()
{
   Menu::onShow();
   m_SelectedIndex = 1;
}

bool MenuConfirmationSDCardUpdate::periodicLoop()
{
   if ( m_bDoingUpdate )
   if ( m_bUpdateFinished )
   {
      removeAllTopLines();
      addTopLine(L("Update from SD card finished."));
      char szBuff[256];
      char szOutput[256];
      hw_execute_bash_command("./ruby_start -ver", szOutput);
      removeTrailingNewLines(szOutput);
      sprintf(szBuff, L("Your controller was updated to version: %s"), szOutput);
      addTopLine(szBuff);
      addTopLine(L("Your controller will reboot now."));
      m_bDoingUpdate = false;
      addMenuItem(new MenuItem(L("Ok")));
      m_SelectedIndex = 0;
   }
   return Menu::periodicLoop();
}

void MenuConfirmationSDCardUpdate::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   //if ( 10 == iChildMenuId/1000 )
}

int MenuConfirmationSDCardUpdate::onBack()
{
   if ( m_bDoingUpdate )
      return 0;
   return Menu::onBack();
}


void* _thread_sdcard_update(void *argument)
{
   bool* pFinished = (bool*) argument;
   hw_log_current_thread_attributes("sdcard update");
   for( int i=0; i<10; i++ )
      hardware_sleep_ms(50);

   ruby_central_has_sdcard_update(true);

   if ( NULL != pFinished )
      *pFinished = true;
   return NULL;
}

void MenuConfirmationSDCardUpdate::onSelectItem()
{
   log_line("MenuSDCardUpdate: selected item: %d", m_SelectedIndex);

   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( m_bDoingUpdate )
      return;
   if ( m_bUpdateFinished )
   {
      if ( 0 == m_SelectedIndex )
      {
         menu_stack_pop(0);
         onEventReboot();
         hardware_reboot();
      }
      return;
   }

   if ( 0 == m_SelectedIndex )
   {
      menu_stack_pop(0);
      return;
   }

   if ( 1 == m_SelectedIndex )
   {
      m_bDoingUpdate = true;
      m_bUpdateFinished = false;
      pthread_attr_t attr;
      hw_init_worker_thread_attrs(&attr, CORE_AFFINITY_OTHERS, -1, SCHED_OTHER, 0, "update from sd card");
      if ( 0 != pthread_create(&m_pThreadUpdate, &attr, &_thread_sdcard_update, (void*)&m_bUpdateFinished) )
      {
         log_softerror_and_alarm("MenuSDCardUpdate: Failed to create update thread.");
         m_bDoingUpdate = false;
         addMessage(L("Failed to do the SD card update."));
      }
      pthread_attr_destroy(&attr);

      removeAllItems();
      removeAllTopLines();
      addTopLine(L("Doing the SD card update. Please wait..."));
   }
}