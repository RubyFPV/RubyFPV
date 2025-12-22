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
#include "menu_objects.h"
#include "menu_text.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"
#include "menu_controller_update.h"
#include "menu_controller_update_net.h"
#include "../../base/ctrl_settings.h"
#include "../../base/hardware_files.h"

#include <time.h>
#include <sys/resource.h>

MenuControllerUpdate::MenuControllerUpdate(void)
:Menu(MENU_ID_CONTROLLER_UPDATE, L("Controller Update"), NULL)
{
   m_Width = 0.24;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.18;
   m_bDoDefaultHandling = true;
   m_bWaitingForUserFinishUpdateConfirmation = false;
   m_iMustStartUpdate = 0;
}

void MenuControllerUpdate::onShow()
{
   if ( ! m_bDoDefaultHandling )
   {
      Menu::onShow();
      return;
   }

   int iTmp = getSelectedMenuItemIndex();

   addItems();
   Menu::onShow();

   if ( iTmp > 0 )
   {
      m_SelectedIndex = iTmp;
      if ( m_SelectedIndex >= m_ItemsCount )
         m_SelectedIndex = m_ItemsCount-1;
      onFocusedItemChanged();
   }
}

void MenuControllerUpdate::addItems()
{
   removeAllItems();
   removeAllTopLines();

   m_IndexUpdateUSB = addMenuItem(new MenuItem(L("Update Software from USB"), L("Updates software on this controller using a USB memory stick.")));
   m_IndexUpdateNet = addMenuItem(new MenuItem(L("Update Software from Internet"), L("Updates software on this controller from the internet.")));
}

void MenuControllerUpdate::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

bool MenuControllerUpdate::periodicLoop()
{
   if ( ! m_bDoDefaultHandling )
      return false;

   if ( m_bWaitingForUserFinishUpdateConfirmation )
      log_line("Waiting for user confirmation to complete the update procedure...");

   if ( m_iMustStartUpdate > 0 )
   {
      m_iMustStartUpdate--;
      if ( m_iMustStartUpdate == 0 )
      {
         updateControllerSoftware(NULL);
         return true;
      }
   }
   return false;
}

void MenuControllerUpdate::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   log_line("MenuControllerUpdate: Returned from child id: %d, return value: %d", iChildMenuId, returnValue);
   if ( (1 == iChildMenuId/1000) && (1 == returnValue) )
   {
      m_iMustStartUpdate = 1;
      log_line("Signaled event to start controller update.");
      return;
   }
   if ( 3 == iChildMenuId/1000 )
   {
      m_bWaitingForUserFinishUpdateConfirmation = false;
      log_line("Closed message update. Will reboot now.");
      menu_discard_all();
      popups_remove_all();
      ruby_processing_loop(true);
      render_all(g_TimeNow);
      ruby_signal_alive();

      onEventReboot();
      hardware_reboot();
      return;
   }

   if ( 5 == iChildMenuId/1000 ) // Update failed.
   {
      log_line("Not waiting for user confirmation anymore");
      m_bWaitingForUserFinishUpdateConfirmation = false;

      //log_line("Closed message update. Will reboot now.");
      //onEventReboot();
      //hw_execute_bash_command("sudo reboot -f", NULL);
      return;
   }
}

void MenuControllerUpdate::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( m_IndexUpdateUSB == m_SelectedIndex )
   {
      if ( checkIsArmed() )
         return;
      char szBuff[128];
      char szBuff2[64];
      getSystemVersionString(szBuff2, (SYSTEM_SW_VERSION_MAJOR<<8) | SYSTEM_SW_VERSION_MINOR);

      sprintf(szBuff, "Your controller has software version %s (b-%d)", szBuff2, SYSTEM_SW_BUILD_NUMBER);

      MenuConfirmation* pMC = new MenuConfirmation(L("Update Controller Software"), L("Insert an USB stick containing the Ruby update archive file and then press Ok to start the update process."), 1, true);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      return;
   }

   if ( m_IndexUpdateNet == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuControllerUpdateNet());
      return;
   }
}

void MenuControllerUpdate::updateControllerSoftware(const char* szUpdateFile)
{
   Popup* p = NULL;
   if ( (NULL == szUpdateFile) || (0 == szUpdateFile[0]) )
      p = new Popup(L("Updating. Please wait"), 0.36,0.4, 0.5, 90);
   else
      p = new Popup(L("Downloading. Please wait"), 0.36,0.4, 0.5, 90);
   popups_add_topmost(p);

   ruby_processing_loop(true);
   render_all(g_TimeNow);
   ruby_signal_alive();

   int iMountRes = 1;
   if ( (NULL == szUpdateFile) || (0 == szUpdateFile[0]) )
      iMountRes = hardware_try_mount_usb();
   if ( 1 != iMountRes )
   {
      popups_remove(p);
      ruby_signal_alive();
      ruby_processing_loop(true);
      render_all(g_TimeNow);
      ruby_signal_alive();

      if ( 0 == iMountRes )
         addMessage(L("No USB memory stick detected. Please insert a USB stick."));
      else
      {
         if ( -1 == iMountRes )
            iMountRes = hardware_try_mount_usb();
         if ( 1 != iMountRes )
            addMessage(L("USB memory stick detected but could not be mounted. Please try again."));
      }
      ruby_signal_alive();
      ruby_processing_loop(true);
      render_all(g_TimeNow);
      ruby_signal_alive();
      if ( 1 != iMountRes )
         return;
   }
   ruby_signal_alive();

   log_line("Starting controller update procedure.");
   g_bUpdateInProgress = true;
   popups_remove(p);

   ruby_pause_watchdog("starting controller update procedure");
   pairing_stop();
   
   if ( (NULL == szUpdateFile) || (0 == szUpdateFile[0]) )
      p = new Popup(L("Updating. Please wait"), 0.36,0.4, 0.5, 90);
   else
      p = new Popup(L("Downloading. Please wait"), 0.36,0.4, 0.5, 90);
   popups_add_topmost(p);

   // Execute ruby_update_worker twice as the ruby_update_worker might have been updated itself too
   char szComm[256];
   char szOutput[4096];
   char szFile[MAX_FILE_PATH_SIZE];

   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_TEMP_UPDATE_CONTROLLER_PROGRESS);

   FILE* fd = NULL;
   int iCounter[2];
   int iResult[2];
   iCounter[0] = iCounter[1] = 0;
   iResult[0] = iResult[1] = -1000;

   for( int iRepeatCount=0; iRepeatCount<2; iRepeatCount++ )
   {
      char szLine[256];
      char szTitle[256];
      szTitle[0] = 0;
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s", szFile);
      hw_execute_bash_command(szComm, NULL);
      hw_execute_ruby_process(NULL, "ruby_update_worker", szUpdateFile, NULL);
      ruby_signal_alive();

      int iLastPartialStatus = -1000;
      u32 uTimeWorkerFinished = 0;

      log_line("Waiting for update process to start and finish, repeat count: %d ...", iRepeatCount);
      bool bFoundProcess = false;
      bool bFinishedProcess = false;
      do
      {
         g_TimeNow = get_current_timestamp_ms();
         szTitle[0] = 0;
         fd = fopen(szFile, "r");
         if ( fd != NULL )
         {
            int iPartialResult = -1;
            szLine[0] = 0;
            if ( NULL != fgets(szLine, 255, fd) )
            if ( 1 != sscanf(szLine, "%d", &iPartialResult) )
               iPartialResult = -10;

            if ( iPartialResult != iLastPartialStatus )
               uTimeWorkerFinished = 0;
            iLastPartialStatus = iPartialResult;
            szLine[0] = 0;
            if ( (NULL == fgets(szLine, 255, fd)) || (strlen(szLine)<5) )
            {
               log_line("Did not found an update status string.");

               if ( (NULL == szUpdateFile) || (0 == szUpdateFile[0]) )
                  strcpy(szTitle, L("Updating. Please wait"));
               else
                  strcpy(szTitle, L("Downloading. Please wait"));
            }
            else
            {
               removeTrailingNewLines(szLine);
               strcpy(szTitle, szLine);
               log_line("Found partial update status: (%s)", szTitle);
            }
            if ( (iPartialResult >= 0) )
            {
               log_line("Found progress update status from worker: %d", iPartialResult);
               bFoundProcess = true;
            }
            fclose(fd);
            fd = NULL;
         }

         if ( (0 == iRepeatCount) || (0 == szTitle[0]) )
         {
            if ( (NULL == szUpdateFile) || (0 == szUpdateFile[0]) )
               strcpy(szTitle, L("Updating. Please wait"));
            else
               strcpy(szTitle, L("Downloading. Please wait"));
         }
         if ( 0 == ((iCounter[iRepeatCount]/2) % 3) )
            strcat(szTitle, ".");
         if ( 1 == ((iCounter[iRepeatCount]/2) % 3) )
            strcat(szTitle, "..");
         if ( 2 == ((iCounter[iRepeatCount]/2) % 3) )
            strcat(szTitle, "...");
         log_line("Set update popup title for run %d: (%s)", iRepeatCount, szTitle);
         p->setTitle(szTitle);
         ruby_processing_loop(true);
         render_all_with_menus(g_TimeNow, false);
         ruby_signal_alive();
         hardware_sleep_ms(50);
         if ( bFoundProcess )
            log_line("Waiting for update worker to finish (%d)...", iCounter[iRepeatCount]);
         else
            log_line("Waiting for update worker to start (%d)...", iCounter[iRepeatCount]);
         iCounter[iRepeatCount]++;

         if ( bFoundProcess )
         {
             if ( (0 == hw_process_exists("ruby_update_worker")) && (0 == hw_process_exists("unzip")) )
             {
                log_line("Update worker process finished (test 1)");
                hardware_sleep_ms(100);
                if ( (0 == hw_process_exists("ruby_update_worker")) && (0 == hw_process_exists("unzip")) )
                {
                   hw_execute_bash_command_raw("ps -ae | grep ruby_update_worker | grep -v grep", szOutput);
                   removeTrailingNewLines(szOutput);
                   char* p = removeLeadingWhiteSpace(szOutput);
                   log_line("Update worker process finished (test 2), ps list: [%s]", p);
                   if (0 == uTimeWorkerFinished )
                   {
                      uTimeWorkerFinished = g_TimeNow;
                      log_line("update worker process is finished at time now.");
                   }
                   else
                   {
                      u32 uDeltaStop = g_TimeNow - uTimeWorkerFinished;
                      if ( uDeltaStop < 7000 )
                         log_line("waiting to see if update worker is really finished, waiting since %u ms ago", uDeltaStop);
                      else
                      {
                         log_line("update worker process is finished since %u ms ago. Finish this update run.", uDeltaStop);
                         bFinishedProcess = true;
                      }
                   }
                }
                else
                   uTimeWorkerFinished = 0;
             }
             else
                uTimeWorkerFinished = 0;
         }

         if ( ! bFoundProcess )
         {
            if ( 0 != hw_process_exists("ruby_update_worker") )
            {
               log_line("Found update worker process.");
               bFoundProcess = true;
            }
         }

         if ( bFoundProcess )
         if ( bFinishedProcess )
         {
            log_line("Update run %d finished. Read final status.", iRepeatCount);
            fd = fopen(szFile, "r");
            if ( fd != NULL )
            {
               szLine[0] = 0;
               if ( NULL != fgets(szLine, 255, fd) )
               {
                  if ( 1 != sscanf(szLine, "%d", &iResult[iRepeatCount]) )
                     iResult[iRepeatCount] = -10;
                  else
                     log_line("Run %d: Found final update status: %d", iRepeatCount, iResult[iRepeatCount]);
               }
               szLine[0] = 0;
               if ( (NULL == fgets(szLine, 255, fd)) || (strlen(szLine)<5) )
                  log_line("Run %d: Did not found  final update status string.", iRepeatCount);
               else
               {
                  removeTrailingNewLines(szLine);
                  strcpy(szTitle, szLine);
                  log_line("Run %d: Found final update status: (%s)", iRepeatCount, szTitle);
               }
               if ( (iResult[iRepeatCount] == 1) || (iResult[iRepeatCount] < 0) )
                  log_line("Run %d: Found final update status from worker: %d", iRepeatCount, iResult[iRepeatCount]);
               fclose(fd);
               fd = NULL;
            }
            break;
         }
      }
      while ( (iCounter[iRepeatCount] < 500) && (! bFinishedProcess) );
      
      log_line("Done waiting for update worker process run %d (on counter %d).", iRepeatCount, iCounter[iRepeatCount]);
      if ( iResult[iRepeatCount] < 0 )
         break;
   }

   int iFinalResult = -1000;
   char szFinalResult[256];
   strcpy(szFinalResult, "Unknown result.");
   if ( access(szFile, R_OK) != -1 )
   {
      fd = fopen(szFile, "r");
      if ( fd != NULL )
      {
         szOutput[0] = 0;
         if ( NULL != fgets(szOutput, 255, fd) )
         if ( 1 != sscanf(szOutput, "%d", &iFinalResult) )
            iFinalResult = -900;

         szOutput[0] = 0;
         if ( (NULL == fgets(szOutput, 255, fd)) || (strlen(szOutput)<5) )
            log_line("Did not found an update final status string.");
         else
         {
            removeTrailingNewLines(szOutput);
            log_line("Found final update status: (%s)", szOutput);
            strcpy(szFinalResult, szOutput);
         }
         fclose(fd);
         fd = NULL;
      }
   }
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s", szFile);
   hw_execute_bash_command(szComm, NULL);

   if ( iFinalResult == -1000 )
      iFinalResult = iResult[1];
   if ( iFinalResult == -1000 )
      iFinalResult = iResult[0];

   log_line("Finished update workers. Loop counter[0]: %d, counter[1]: %d, result[0]: %d, result[1]: %d",
       iCounter[0], iCounter[1], iResult[0], iResult[1]);
   log_line("Final result: %d, (%s)", iFinalResult, szFinalResult);

   hardware_unmount_usb();
   ruby_processing_loop(true);
   render_all(g_TimeNow);
   ruby_signal_alive();
   g_bUpdateInProgress = false;
   ruby_resume_watchdog("finished controller update procedure");

   popups_remove(p);
   ruby_processing_loop(true);
   render_all(g_TimeNow);
   ruby_signal_alive();
   hw_execute_bash_command("sync", NULL);

   if ( iCounter[1] >= 500 )
   {
      m_bWaitingForUserFinishUpdateConfirmation = true;
      MenuConfirmation* pMC = new MenuConfirmation(L("Update Failed"), L("Update timedout and failed."), 5, true);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      ruby_processing_loop(true);
      render_all(g_TimeNow);
      ruby_signal_alive();
      log_line("Exit from main update procedure (1).");
      return;
   }

   if ( iFinalResult <= 0 )
   {
      m_bWaitingForUserFinishUpdateConfirmation = true;
      MenuConfirmation* pMC = NULL;
      
      if ( iFinalResult == -1 )
         pMC = new MenuConfirmation(L("Update Failed"), L("No update archive file found on the USB memory stick. No update done."), 5, true);
      else if ( iFinalResult == -2 )
         pMC = new MenuConfirmation(L("Update Failed"), "Can't do update. Can't access the controller files.",5, true);
      else if ( iFinalResult == -3 )
         pMC = new MenuConfirmation(L("Update Failed"), "Update failed. Can't access the controller files.",5, true);
      else if ( iFinalResult == -4 )
         pMC = new MenuConfirmation(L("Update Info"), "Files unchanged. Either you applyed the same update twice, either the update failed to write the new files.",5, true);
      else if ( iFinalResult == -10 )
         pMC = new MenuConfirmation(L("Update Failed"), "The update archive file found on the USB memory stick looks to be invalid.",5, true);
      else if ( iFinalResult == -11 )
         pMC = new MenuConfirmation(L("Download Failed"), "Failed to download update from internet. Try again or use a USB memory stick to do the update.",5, true);
      else if ( iFinalResult < 0 )
      {
         char szMsg[256];
         sprintf(szMsg, "The update procedure failed, error code %d.", iFinalResult);
         pMC = new MenuConfirmation(L("Update Failed"), szMsg, 5, true);
         if ( iFinalResult < 0 )
            pMC->addTopLine(szFinalResult);
      }
      else
         pMC = new MenuConfirmation(L("Update Failed"), "Update failed.", 5, true);

      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      ruby_processing_loop(true);
      render_all(g_TimeNow);
      ruby_signal_alive();
      log_line("Exit from main update procedure (2).");
      return;
   }

   m_bWaitingForUserFinishUpdateConfirmation = true;
   g_bDidAnUpdate = true;
   MenuConfirmation* pMC = new MenuConfirmation(L("Update Complete"), L("Update complete. You can now remove the USB stick. The system will reboot now."), 3, true);
   pMC->m_yPos = 0.3;
   pMC->addTopLine(L("(If it does not reboot, you can power it off and on)"));
   if ( 0 < strlen(szOutput) )
   {
      char* pSt = szOutput;
      while (*pSt)
      {
         char* pEnd = pSt;
         while ( (*pEnd != 0) && (*pEnd != 10) && (*pEnd != 13) )
            pEnd++;
         *pEnd = 0;
         if ( pEnd > pSt )
            pMC->addTopLine(pSt);
         pSt = pEnd+1;
      }
   }

   add_menu_to_stack(pMC);
   ruby_processing_loop(true);
   render_all(g_TimeNow);
   ruby_signal_alive();
   log_line("Exit from main update procedure (normal exit).");
}
