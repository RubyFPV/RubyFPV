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
#include "menu_vehicle_cpu_priorities.h"
#include "menu_item_select.h"
#include "menu_confirmation.h"
#include "menu_info_procs.h"
#include "menu_item_section.h"

MenuVehicleCPUPriorities::MenuVehicleCPUPriorities(void)
:Menu(MENU_ID_VEHICLE_CPU_PRIORITIES, L("Processes Settings"), NULL)
{
   m_Width = 0.42;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.13;
   float fSliderWidth = 0.10;
   setSubTitle(L("Change vehicle's processes settings, for expert users."));

   memcpy((u8*)&m_NewProcPriorities, (u8*)&(g_pCurrentModel->processesPriorities), sizeof(type_processes_priorities));

   m_iIndexBalanceIntCores = -1;
   m_iIndexAffinitiesBalance = -1;

   for( int i=0; i<15; i++ )
      m_pItemsSlider[i] = NULL;

   if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
   if ( hardware_board_is_sigmastar(g_pCurrentModel->hwCapabilities.uBoardType) )
   {
      m_pItemsSelect[6] = new MenuItemSelect(L("Balance CPU Interrupts"), L("Tries to balance the load on the CPU cores interrupts."));
      m_pItemsSelect[6]->addSelection(L("No"));
      m_pItemsSelect[6]->addSelection(L("Yes"));
      m_pItemsSelect[6]->setIsEditable();
      m_iIndexBalanceIntCores = addMenuItem(m_pItemsSelect[6]);
   }

   if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
   if ( hardware_board_is_sigmastar(g_pCurrentModel->hwCapabilities.uBoardType) )
   {
      m_pItemsSelect[3] = new MenuItemSelect(L("Balance CPU Cores"), L("Tries to balance the load on the CPU cores for video capture and Ruby processes."));
      m_pItemsSelect[3]->addSelection(L("No"));
      m_pItemsSelect[3]->addSelection(L("Yes"));
      m_pItemsSelect[3]->setIsEditable();
      m_iIndexAffinitiesBalance = addMenuItem(m_pItemsSelect[3]);
   }

   m_pItemsSelect[0] = new MenuItemSelect(L("Enable Auto Cores Affinities"), L("Automatically adjust the work load on each CPU core."));
   m_pItemsSelect[0]->addSelection(L("No"));
   m_pItemsSelect[0]->addSelection(L("Yes"));
   m_pItemsSelect[0]->setIsEditable();
   m_iIndexAffinities = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[1] = new MenuItemSelect(L("Enable Video Auto Cores Affinities"), L("Automatically adjust the work load on each CPU core for video capture processes."));
   m_pItemsSelect[1]->addSelection(L("No"));
   m_pItemsSelect[1]->addSelection(L("Yes"));
   m_pItemsSelect[1]->setIsEditable();
   m_iIndexAffinitiesVideoCapture = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSelect[2] = new MenuItemSelect("Enable Priorities Adjustment", "Enable adjustment of processes priorities or use default priorities.");
   m_pItemsSelect[2]->addSelection(L("No"));
   m_pItemsSelect[2]->addSelection(L("Yes"));
   m_pItemsSelect[2]->setIsEditable();
   m_iIndexPrioritiesAdjustment = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSlider[0] = new MenuItemSlider("Core Priority",  "Sets the priority for the Ruby core functionality. Lower values means higher priority. Zero means disabled.", 0,139,100, fSliderWidth);
   m_iIndexPriorityRouter = addMenuItem(m_pItemsSlider[0]);

   m_pItemsSlider[10] = new MenuItemSlider("Core I/O Priority", "Sets the I/O priority value for the core Ruby components, 1 is highest priority, 7 is lowest priority. Zero means disabled.", 0,7,4, fSliderWidth);
   m_iIndexIONiceRouter = addMenuItem(m_pItemsSlider[10]);


   m_pItemsSlider[1] = new MenuItemSlider("Rx Priority", "Sets the priority for the Ruby radio Rx threads. Lower values means higher priority. Zero means disabled.", 0,139,100, fSliderWidth);
   m_iIndexPriorityRadioRx = addMenuItem(m_pItemsSlider[1]);

   m_pItemsSlider[2] = new MenuItemSlider("Tx Priority", "Sets the priority for the Ruby radio Tx threads. Lower values means higher priority. Zero means disabled.", 0,139,100, fSliderWidth);
   m_iIndexPriorityRadioTx = addMenuItem(m_pItemsSlider[2]);

   m_pItemsSlider[3] = new MenuItemSlider("Video", "Sets the priority for the Ruby video threads. Lower values means higher priority. Zero means disabled.", 0,139,100, fSliderWidth);
   m_iIndexPriorityVideo = addMenuItem(m_pItemsSlider[3]);

   m_pItemsSlider[11] = new MenuItemSlider("Video I/O Priority", "Sets the I/O priority value for RX video pipeline, 1 is highest priority, 7 is lowest priority. Zero means disabled.", 0,7,4, fSliderWidth);
   m_iIndexIONiceVideo = addMenuItem(m_pItemsSlider[11]);

   m_pItemsSlider[4] = new MenuItemSlider("Telemetry", "Sets the priority for the Ruby telemetry threads. Lower values means higher priority. Zero means disabled.", 0,139,100, fSliderWidth);
   m_iIndexPriorityTelemetry = addMenuItem(m_pItemsSlider[4]);

   m_pItemsSlider[5] = new MenuItemSlider("RC", "Sets the priority for the Ruby RC threads. Lower values means higher priority. Zero means disabled.", 0,139,100, fSliderWidth);
   m_iIndexPriorityRC = addMenuItem(m_pItemsSlider[5]);

   m_pItemsSlider[6] = new MenuItemSlider("Others", "Sets the priority for other Ruby threads. Lower values means higher priority. Zero means disabled.", 0,139,100, fSliderWidth);
   m_iIndexPriorityOthers = addMenuItem(m_pItemsSlider[6]);

   m_IndexQuery = -1;
   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE) )
   {
      m_IndexQuery = addMenuItem(new MenuItem(L("List Priorities"), L("Lists all vehicle's processes priorities.")));
      m_pMenuItems[m_IndexQuery]->showArrow();
      m_pMenuItems[m_IndexQuery]->setTextColor(get_Color_Dev());
   }

   m_IndexRestart = addMenuItem(new MenuItem(L("Restart Processes"), L("Restarts all vehicle processes.")));
   m_IndexResetPriorities = addMenuItem(new MenuItem(L("Reset Priorities"), L("Resets all vehicle processes priorities.")));
}

void MenuVehicleCPUPriorities::valuesToUI()
{
   if ( -1 != m_iIndexBalanceIntCores )
   if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
   if ( hardware_board_is_sigmastar(g_pCurrentModel->hwCapabilities.uBoardType) )
   {
      if ( m_NewProcPriorities.uProcessesFlags & PROCESSES_FLAGS_BALANCE_INTERRUPTS_CORES )
         m_pItemsSelect[6]->setSelectedIndex(1);
      else
         m_pItemsSelect[6]->setSelectedIndex(0);
   }

   if ( -1 != m_iIndexAffinitiesBalance )
      m_pItemsSelect[3]->setSelectedIndex((m_NewProcPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_AFFINITY_CORES_BALANCE_OIPC)?1:0);

   m_pItemsSelect[0]->setSelectedIndex((m_NewProcPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_AFFINITY_CORES)?1:0);
   m_pItemsSelect[1]->setSelectedIndex((m_NewProcPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_AFFINITY_CORES_VIDEO_CAPTURE)?1:0);
   if ( m_NewProcPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_AFFINITY_CORES )
      m_pItemsSelect[1]->setEnabled(true);
   else
      m_pItemsSelect[1]->setEnabled(false);

   m_pItemsSelect[2]->setSelectedIndex((m_NewProcPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS)?1:0);

   m_pItemsSlider[0]->setCurrentValue(m_NewProcPriorities.iThreadPriorityRouter);
   m_pItemsSlider[1]->setCurrentValue(m_NewProcPriorities.iThreadPriorityRadioRx);
   m_pItemsSlider[2]->setCurrentValue(m_NewProcPriorities.iThreadPriorityRadioTx);
   m_pItemsSlider[3]->setCurrentValue(m_NewProcPriorities.iThreadPriorityVideoCapture);
   m_pItemsSlider[4]->setCurrentValue(m_NewProcPriorities.iThreadPriorityTelemetry);
   m_pItemsSlider[5]->setCurrentValue(m_NewProcPriorities.iThreadPriorityRC);
   m_pItemsSlider[6]->setCurrentValue(m_NewProcPriorities.iThreadPriorityOthers);
   
   m_pItemsSlider[10]->setCurrentValue(m_NewProcPriorities.ioNiceRouter);
   m_pItemsSlider[11]->setCurrentValue(m_NewProcPriorities.ioNiceVideo);
 
   for( int i=0; i<15; i++ )
   {
      if ( NULL == m_pItemsSlider[i] )
         continue;
      m_pItemsSlider[i]->setEnabled((m_NewProcPriorities.uProcessesFlags & PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS)?true:false);

      if ( 0 == m_pItemsSlider[i]->getCurrentValue() )
         m_pItemsSlider[i]->setSufix(L("Disabled"));
      else
         m_pItemsSlider[i]->setSufix("");
   }
}

void MenuVehicleCPUPriorities::onShow()
{
   Menu::onShow();
}

void MenuVehicleCPUPriorities::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehicleCPUPriorities::onItemValueChanged(int itemIndex)
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

void MenuVehicleCPUPriorities::onItemEndEdit(int itemIndex)
{
   Menu::onItemEndEdit(itemIndex);
}

void MenuVehicleCPUPriorities::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   // Restart processes?
   if ( 2 == iChildMenuId/1000 )
   {
      if ( (1 == returnValue) )
         handle_commands_send_to_vehicle(COMMAND_ID_SET_THREADS_PRIORITIES, 1, (u8*)&m_NewProcPriorities, sizeof(type_processes_priorities));
      else
         handle_commands_send_to_vehicle(COMMAND_ID_SET_THREADS_PRIORITIES, 0, (u8*)&m_NewProcPriorities, sizeof(type_processes_priorities));
      return;
   }
}

void MenuVehicleCPUPriorities::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      log_line("MenuVehicleCPUPriorities: Command in progress");
      handle_commands_show_popup_progress();
      return;
   }

   log_line("MenuVehicleCPUPriorities: Selected item %d", m_SelectedIndex);

   if ( (-1 != m_IndexQuery) && (m_IndexQuery == m_SelectedIndex) )
   {
      handle_commands_send_to_vehicle(COMMAND_ID_GET_CPU_PROCS_INFO, 0, NULL, 0);
      return;
   }

   bool bSkipAsk = false;

   if ( (-1 != m_iIndexBalanceIntCores) && (m_iIndexBalanceIntCores == m_SelectedIndex) )
   {
      if ( 1 == m_pItemsSelect[6]->getSelectedIndex() )
         m_NewProcPriorities.uProcessesFlags |= PROCESSES_FLAGS_BALANCE_INTERRUPTS_CORES;
      else
         m_NewProcPriorities.uProcessesFlags &= ~PROCESSES_FLAGS_BALANCE_INTERRUPTS_CORES;
   }

   if ( (-1 != m_iIndexAffinitiesBalance) && (m_iIndexAffinitiesBalance == m_SelectedIndex) )
   {
      if ( 1 == m_pItemsSelect[3]->getSelectedIndex() )
         m_NewProcPriorities.uProcessesFlags |= PROCESSES_FLAGS_ENABLE_AFFINITY_CORES_BALANCE_OIPC;
      else
         m_NewProcPriorities.uProcessesFlags &= ~PROCESSES_FLAGS_ENABLE_AFFINITY_CORES_BALANCE_OIPC;
   }

   if ( m_iIndexAffinities == m_SelectedIndex )
   {
      m_NewProcPriorities.uProcessesFlags &= ~PROCESSES_FLAGS_ENABLE_AFFINITY_CORES;
      if ( m_pItemsSelect[0]->getSelectedIndex() )
         m_NewProcPriorities.uProcessesFlags |= PROCESSES_FLAGS_ENABLE_AFFINITY_CORES;
   }

   if ( m_iIndexAffinitiesVideoCapture == m_SelectedIndex )
   {
      m_NewProcPriorities.uProcessesFlags &= ~PROCESSES_FLAGS_ENABLE_AFFINITY_CORES_VIDEO_CAPTURE;
      if ( m_pItemsSelect[1]->getSelectedIndex() )
         m_NewProcPriorities.uProcessesFlags |= PROCESSES_FLAGS_ENABLE_AFFINITY_CORES_VIDEO_CAPTURE;
   }

   if ( m_iIndexPrioritiesAdjustment == m_SelectedIndex )
   {
      m_NewProcPriorities.uProcessesFlags &= ~PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS;
      if ( m_pItemsSelect[2]->getSelectedIndex() )
         m_NewProcPriorities.uProcessesFlags |= PROCESSES_FLAGS_ENABLE_PRIORITIES_ADJUSTMENTS;
   }

   if ( m_iIndexPriorityRouter == m_SelectedIndex )
   {
      m_NewProcPriorities.iThreadPriorityRouter = m_pItemsSlider[0]->getCurrentValue();
      bSkipAsk = true;
   }
   if ( m_iIndexPriorityRadioRx == m_SelectedIndex )
   {
      m_NewProcPriorities.iThreadPriorityRadioRx = m_pItemsSlider[1]->getCurrentValue();
      bSkipAsk = true;
   }
   if ( m_iIndexPriorityRadioTx == m_SelectedIndex )
   {
      m_NewProcPriorities.iThreadPriorityRadioTx = m_pItemsSlider[2]->getCurrentValue();
      bSkipAsk = true;
   }

   if ( m_iIndexPriorityVideo == m_SelectedIndex )
      m_NewProcPriorities.iThreadPriorityVideoCapture = m_pItemsSlider[3]->getCurrentValue();
   if ( m_iIndexPriorityTelemetry == m_SelectedIndex )
      m_NewProcPriorities.iThreadPriorityTelemetry = m_pItemsSlider[4]->getCurrentValue();
   if ( m_iIndexPriorityRC == m_SelectedIndex )
      m_NewProcPriorities.iThreadPriorityRC = m_pItemsSlider[5]->getCurrentValue();
   if ( m_iIndexPriorityOthers == m_SelectedIndex )
      m_NewProcPriorities.iThreadPriorityOthers = m_pItemsSlider[6]->getCurrentValue();

   if ( m_iIndexIONiceRouter == m_SelectedIndex )
      m_NewProcPriorities.ioNiceRouter = m_pItemsSlider[10]->getCurrentValue();
   if ( m_iIndexIONiceVideo == m_SelectedIndex )
      m_NewProcPriorities.ioNiceVideo = m_pItemsSlider[11]->getCurrentValue();

   if ( m_IndexRestart == m_SelectedIndex )
   {
      memcpy((u8*)&m_NewProcPriorities, (u8*)&(g_pCurrentModel->processesPriorities), sizeof(type_processes_priorities));
      handle_commands_send_to_vehicle(COMMAND_ID_SET_THREADS_PRIORITIES, 1, (u8*)&m_NewProcPriorities, sizeof(type_processes_priorities));
      return;
   }

   if ( m_IndexResetPriorities == m_SelectedIndex )
   {
      type_processes_priorities tmp;
      memcpy((u8*)&tmp, (u8*)&(g_pCurrentModel->processesPriorities), sizeof(type_processes_priorities));
      g_pCurrentModel->resetProcessesParams();
      memcpy((u8*)&m_NewProcPriorities, (u8*)&(g_pCurrentModel->processesPriorities), sizeof(type_processes_priorities));
      memcpy((u8*)&(g_pCurrentModel->processesPriorities), (u8*)&tmp, sizeof(type_processes_priorities));
   }

   if ( 0 != memcmp((u8*)&m_NewProcPriorities, (u8*)&(g_pCurrentModel->processesPriorities), sizeof(type_processes_priorities)) )
   {
      if ( bSkipAsk )
      {
         handle_commands_send_to_vehicle(COMMAND_ID_SET_THREADS_PRIORITIES, 0, (u8*)&m_NewProcPriorities, sizeof(type_processes_priorities));
         return;
      }
      MenuConfirmation* pMC = new MenuConfirmation( L("Restart Confirmation"), L("You need to restart the vehicle processes. Do you want to restart them now?"), 2);
      add_menu_to_stack(pMC);
   }
}
