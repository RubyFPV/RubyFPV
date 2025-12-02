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
#include "menu_controller.h"
#include "menu_text.h"
#include "menu_confirmation.h"
#include "menu_controller_cpu_priorities.h"
#include "menu_info_procs.h"
#include "menu_item_section.h"
#include "../process_router_messages.h"
#include "../../base/hardware_files.h"
#include "../ruby_central.h"
#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>


MenuControllerCPUPriorities::MenuControllerCPUPriorities(void)
:Menu(MENU_ID_CONTROLLER_CPU_PRIORITIES, L("Processes Settings"), NULL)
{
   m_Width = 0.42;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.15;
   
   float fSliderWidth = 0.12;

   for( int i=0; i<15; i++ )
      m_pItemsSlider[i] = NULL;

   m_pItemsSelect[0] = new MenuItemSelect(L("Enable Auto Cores Affinities"), L("Automatically adjust the work load on each CPU core."));
   m_pItemsSelect[0]->addSelection(L("No"));
   m_pItemsSelect[0]->addSelection(L("Yes"));
   m_pItemsSelect[0]->setIsEditable();
   m_iIndexAffinities = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[1] = new MenuItemSelect("Enable Priorities Adjustment", "Enable adjustment of processes priorities or use default priorities.");
   m_pItemsSelect[1]->addSelection(L("No"));
   m_pItemsSelect[1]->addSelection(L("Yes"));
   m_pItemsSelect[1]->setIsEditable();
   m_iIndexPrioritiesAdjustment = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSlider[0] = new MenuItemSlider("Core Priority",  "Sets the priority for the Ruby core functionality. Lower values means higher priority. Zero means disabled.", 0,139,100, fSliderWidth);
   m_iIndexPriorityRouter = addMenuItem(m_pItemsSlider[0]);

   m_pItemsSlider[10] = new MenuItemSlider("Core I/O Priority", "Sets the I/O priority value for the core Ruby components, 1 is highest priority, 7 is lowest priority. Zero means disabled.", 0,7,4, fSliderWidth);
   m_iIndexIONiceRouter = addMenuItem(m_pItemsSlider[10]);


   m_pItemsSlider[1] = new MenuItemSlider("Rx Priority", "Sets the priority for the Ruby radio Rx threads. Lower values means higher priority. Zero means disabled.", 0,139,100, fSliderWidth);
   m_iIndexPriorityRadioRx = addMenuItem(m_pItemsSlider[1]);

   m_pItemsSlider[2] = new MenuItemSlider("Tx Priority", "Sets the priority for the Ruby radio Tx threads. Lower values means higher priority. Zero means disabled.", 0,139,100, fSliderWidth);
   m_iIndexPriorityRadioTx = addMenuItem(m_pItemsSlider[2]);

   m_pItemsSlider[3] = new MenuItemSlider("UI", "Sets the priority for the Ruby UI. Lower values means higher priority. Zero means disabled.", 0,139,100, fSliderWidth);
   m_iIndexPriorityCentral = addMenuItem(m_pItemsSlider[3]);

   m_pItemsSlider[4] = new MenuItemSlider("Video", "Sets the priority for the Ruby video threads. Lower values means higher priority. Zero means disabled.", 0,139,100, fSliderWidth);
   m_iIndexPriorityVideoRx = addMenuItem(m_pItemsSlider[4]);

   m_pItemsSlider[11] = new MenuItemSlider("Video I/O Priority", "Sets the I/O priority value for RX video pipeline, 1 is highest priority, 7 is lowest priority. Zero means disabled.", 0,7,4, fSliderWidth);
   m_iIndexIONiceRxVideo = addMenuItem(m_pItemsSlider[11]);


   m_pItemsSlider[5] = new MenuItemSlider("Video Recording", "Sets the priority for the Ruby video recording threads. Lower values means higher priority. Zero means disabled.", 0,139,100, fSliderWidth);
   m_iIndexPriorityVideoRec = addMenuItem(m_pItemsSlider[5]);

   m_pItemsSlider[6] = new MenuItemSlider("RC", "Sets the priority for the Ruby RC threads. Lower values means higher priority. Zero means disabled.", 0,139,100, fSliderWidth);
   m_iIndexPriorityRC = addMenuItem(m_pItemsSlider[6]);

   m_pItemsSlider[7] = new MenuItemSlider("Others", "Sets the priority for other Ruby threads. Lower values means higher priority. Zero means disabled.", 0,139,100, fSliderWidth);
   m_iIndexPriorityOthers = addMenuItem(m_pItemsSlider[7]);

   m_IndexQuery = -1;
   if ( g_pControllerSettings->iDeveloperMode )
   {
      m_IndexQuery = addMenuItem(new MenuItem(L("List Priorities"), L("Lists all controller's processes priorities.")));
      m_pMenuItems[m_IndexQuery]->showArrow();
      m_pMenuItems[m_IndexQuery]->setTextColor(get_Color_Dev());
   }
   m_IndexResetPriorities = addMenuItem(new MenuItem(L("Reset Priorities"), L("Resets all controller processes priorities.")));
}

void MenuControllerCPUPriorities::valuesToUI()
{
   m_pItemsSelect[0]->setSelectedIndex(g_pControllerSettings->iCoresAdjustment);
   m_pItemsSelect[1]->setSelectedIndex(g_pControllerSettings->iPrioritiesAdjustment);

   m_pItemsSlider[0]->setCurrentValue(g_pControllerSettings->iThreadPriorityRouter);
   m_pItemsSlider[1]->setCurrentValue(g_pControllerSettings->iThreadPriorityRadioRx);
   m_pItemsSlider[2]->setCurrentValue(g_pControllerSettings->iThreadPriorityRadioTx);
   m_pItemsSlider[3]->setCurrentValue(g_pControllerSettings->iThreadPriorityCentral);
   m_pItemsSlider[4]->setCurrentValue(g_pControllerSettings->iThreadPriorityVideo);
   m_pItemsSlider[5]->setCurrentValue(g_pControllerSettings->iThreadPriorityVideoRecording);
   m_pItemsSlider[6]->setCurrentValue(g_pControllerSettings->iThreadPriorityRC);
   m_pItemsSlider[7]->setCurrentValue(g_pControllerSettings->iThreadPriorityOthers);
   
   m_pItemsSlider[10]->setCurrentValue(g_pControllerSettings->ioNiceRouter);
   m_pItemsSlider[11]->setCurrentValue(g_pControllerSettings->ioNiceRxVideo);

   for( int i=0; i<15; i++ )
   {
      if ( NULL == m_pItemsSlider[i] )
         continue;
      m_pItemsSlider[i]->setEnabled(g_pControllerSettings->iPrioritiesAdjustment);

      if ( 0 == m_pItemsSlider[i]->getCurrentValue() )
         m_pItemsSlider[i]->setSufix(L("Disabled"));
      else
         m_pItemsSlider[i]->setSufix("");
   }
}


void MenuControllerCPUPriorities::onShow()
{
   Menu::onShow();
}

void MenuControllerCPUPriorities::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuControllerCPUPriorities::onItemValueChanged(int itemIndex)
{
   Menu::onItemValueChanged(itemIndex);

   if ( (itemIndex >= m_iIndexPriorityRouter) && (itemIndex <= m_iIndexPriorityOthers) )
   {
      MenuItemSlider* pSlider = (MenuItemSlider*)m_pMenuItems[itemIndex];
      if ( pSlider->getCurrentValue() < 1 )
         pSlider->setSufix(L("Disabled"));
      else
         pSlider->setSufix("");
   }
}

void MenuControllerCPUPriorities::onItemEndEdit(int itemIndex)
{
   Menu::onItemEndEdit(itemIndex);
}

void MenuControllerCPUPriorities::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   // Restart pairing only
   if ( (5 == iChildMenuId/1000) && (1 == returnValue) )
   {
      pairing_stop();
      hardware_sleep_ms(200);
      if ( 0 < hardware_get_radio_interfaces_count() )
         pairing_start_normal();
      return;
   }

   // Restart all procs
   if ( (4 == iChildMenuId/1000) && (1 == returnValue) )
   {
      pairing_stop();
      ruby_shutdown_ui();
      sem_t* ps = sem_open(SEMAPHORE_WATCHDOG_CONTROLLER_RESTART, O_CREAT, S_IWUSR | S_IRUSR, 0);
      if ( (NULL != ps) && (SEM_FAILED != ps) )
      {
         sem_post(ps);
         sem_close(ps);
      }
      else
         log_softerror_and_alarm("Failed to open and signal semaphore %s", SEMAPHORE_WATCHDOG_CONTROLLER_RESTART);
      exit(0);
   }
}


void MenuControllerCPUPriorities::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( m_iIndexAffinities == m_SelectedIndex )
   {
      if ( g_pControllerSettings->iCoresAdjustment != m_pItemsSelect[0]->getSelectedIndex() )
      {
         MenuConfirmation* pMC = new MenuConfirmation( L("Restart Confirmation"), L("You need to restart the controller processes. Do you want to restart them now?"), 4);
         add_menu_to_stack(pMC);
      }
      g_pControllerSettings->iCoresAdjustment = m_pItemsSelect[0]->getSelectedIndex();
      save_ControllerSettings();
      valuesToUI();
      return;
   }

   if ( m_iIndexPrioritiesAdjustment == m_SelectedIndex )
   {
      if ( g_pControllerSettings->iPrioritiesAdjustment != m_pItemsSelect[1]->getSelectedIndex() )
      {
         MenuConfirmation* pMC = new MenuConfirmation( L("Restart Confirmation"), L("You need to restart the controller processes. Do you want to restart them now?"), 4);
         add_menu_to_stack(pMC);
      }
      g_pControllerSettings->iPrioritiesAdjustment = m_pItemsSelect[1]->getSelectedIndex();
      save_ControllerSettings();
      valuesToUI();
      return;
   }

   if ( m_iIndexPriorityRouter == m_SelectedIndex )
   {
      if ( g_pControllerSettings->iThreadPriorityRouter != m_pItemsSlider[0]->getCurrentValue() )
      {
      //   MenuConfirmation* pMC = new MenuConfirmation( L("Restart Confirmation"), L("You need to restart the controller processes. Do you want to restart them now?"), 5);
      //   add_menu_to_stack(pMC);
      }
      g_pControllerSettings->iThreadPriorityRouter = m_pItemsSlider[0]->getCurrentValue();
      save_ControllerSettings();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      return;
   }

   if ( m_iIndexPriorityRadioRx == m_SelectedIndex )
   {
      if ( g_pControllerSettings->iThreadPriorityRadioRx != m_pItemsSlider[1]->getCurrentValue() )
      {
         //MenuConfirmation* pMC = new MenuConfirmation( L("Restart Confirmation"), L("You need to restart the controller processes. Do you want to restart them now?"), 5);
         //add_menu_to_stack(pMC);
      }
      g_pControllerSettings->iThreadPriorityRadioRx = m_pItemsSlider[1]->getCurrentValue();
      save_ControllerSettings();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      return;
   }

   if ( m_iIndexPriorityRadioTx == m_SelectedIndex )
   {
      if ( g_pControllerSettings->iThreadPriorityRadioTx != m_pItemsSlider[2]->getCurrentValue() )
      {
         //MenuConfirmation* pMC = new MenuConfirmation( L("Restart Confirmation"), L("You need to restart the controller processes. Do you want to restart them now?"), 5);
         //add_menu_to_stack(pMC);
      }
      g_pControllerSettings->iThreadPriorityRadioTx = m_pItemsSlider[2]->getCurrentValue();
      save_ControllerSettings();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      return;
   }
   

   if ( m_iIndexPriorityCentral == m_SelectedIndex )
   {
      if ( g_pControllerSettings->iThreadPriorityCentral != m_pItemsSlider[3]->getCurrentValue() )
      {
         MenuConfirmation* pMC = new MenuConfirmation( L("Restart Confirmation"), L("You need to restart the controller processes. Do you want to restart them now?"), 4);
         add_menu_to_stack(pMC);
      }
      g_pControllerSettings->iThreadPriorityCentral = m_pItemsSlider[3]->getCurrentValue();
      save_ControllerSettings();
      valuesToUI();
      return;
   }

   if ( m_iIndexPriorityVideoRx == m_SelectedIndex )
   {
      if ( g_pControllerSettings->iThreadPriorityVideo != m_pItemsSlider[4]->getCurrentValue() )
      {
         MenuConfirmation* pMC = new MenuConfirmation( L("Restart Confirmation"), L("You need to restart the controller processes. Do you want to restart them now?"), 5);
         add_menu_to_stack(pMC);
      }
      g_pControllerSettings->iThreadPriorityVideo = m_pItemsSlider[4]->getCurrentValue();
      save_ControllerSettings();
      valuesToUI();
      return;
   }

   if ( m_iIndexPriorityVideoRec == m_SelectedIndex )
   {
      if ( g_pControllerSettings->iThreadPriorityVideoRecording != m_pItemsSlider[5]->getCurrentValue() )
      {
         MenuConfirmation* pMC = new MenuConfirmation( L("Restart Confirmation"), L("You need to restart the controller processes. Do you want to restart them now?"), 5);
         add_menu_to_stack(pMC);
      }
      g_pControllerSettings->iThreadPriorityVideoRecording = m_pItemsSlider[5]->getCurrentValue();
      save_ControllerSettings();
      valuesToUI();
      return;
   }

   if ( m_iIndexPriorityRC == m_SelectedIndex )
   {
      if ( g_pControllerSettings->iThreadPriorityRC != m_pItemsSlider[6]->getCurrentValue() )
      {
         MenuConfirmation* pMC = new MenuConfirmation( L("Restart Confirmation"), L("You need to restart the controller processes. Do you want to restart them now?"), 5);
         add_menu_to_stack(pMC);
      }
      g_pControllerSettings->iThreadPriorityRC = m_pItemsSlider[6]->getCurrentValue();
      save_ControllerSettings();
      valuesToUI();
      return;
   }

   if ( m_iIndexPriorityOthers == m_SelectedIndex )
   {
      if ( g_pControllerSettings->iThreadPriorityOthers != m_pItemsSlider[7]->getCurrentValue() )
      {
         MenuConfirmation* pMC = new MenuConfirmation( L("Restart Confirmation"), L("You need to restart the controller processes. Do you want to restart them now?"), 5);
         add_menu_to_stack(pMC);
      }
      g_pControllerSettings->iThreadPriorityOthers = m_pItemsSlider[7]->getCurrentValue();
      save_ControllerSettings();
      valuesToUI();
      return;
   }

   if ( m_iIndexIONiceRouter == m_SelectedIndex )
   {
      if ( g_pControllerSettings->ioNiceRouter != m_pItemsSlider[10]->getCurrentValue() )
      {
         MenuConfirmation* pMC = new MenuConfirmation( L("Restart Confirmation"), L("You need to restart the controller processes. Do you want to restart them now?"), 5);
         add_menu_to_stack(pMC);
      }
      g_pControllerSettings->ioNiceRouter = m_pItemsSlider[10]->getCurrentValue();
      save_ControllerSettings();
      valuesToUI();
      return;
   }

   if ( m_iIndexIONiceRxVideo == m_SelectedIndex )
   {
      if ( g_pControllerSettings->ioNiceRxVideo != m_pItemsSlider[11]->getCurrentValue() )
      {
         MenuConfirmation* pMC = new MenuConfirmation( L("Restart Confirmation"), L("You need to restart the controller processes. Do you want to restart them now?"), 5);
         add_menu_to_stack(pMC);
      }
      g_pControllerSettings->ioNiceRxVideo = m_pItemsSlider[11]->getCurrentValue();
      save_ControllerSettings();
      valuesToUI();
      return;
   }

   if ( (-1 != m_IndexQuery) && (m_IndexQuery == m_SelectedIndex) )
   {
      char szOutput[4096];
      hw_execute_bash_command("./ruby_start -dbgproc -g", szOutput);
      add_menu_to_stack( new MenuInfoProcs(szOutput));
      return;
   }
  
   if ( m_IndexResetPriorities == m_SelectedIndex )
   {
      reset_ControllerPriorities();
      save_ControllerSettings();
      valuesToUI();
      addMessage("Controller processes priorities have been reset.");
      MenuConfirmation* pMC = new MenuConfirmation( L("Restart Confirmation"), L("You need to restart the controller processes. Do you want to restart them now?"), 4);
      add_menu_to_stack(pMC);
      return;
   }
}
