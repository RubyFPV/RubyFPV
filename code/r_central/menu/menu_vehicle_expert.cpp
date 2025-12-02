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
#include "menu_vehicle_expert.h"
#include "menu_vehicle_cpu_priorities.h"
#include "menu_item_select.h"
#include "menu_confirmation.h"
#include "menu_item_section.h"

MenuVehicleExpert::MenuVehicleExpert(void)
:Menu(MENU_ID_VEHICLE_EXPERT, "Expert Settings", NULL)
{
   m_Width = 0.32;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.13;
   float fSliderWidth = 0.10;
   setSubTitle("Change advanced vehicle settings, for expert users.");
   
   addTopInfo();

   m_pItemsSelect[2] = new MenuItemSelect("Enable CPU Overclocking", "Enables overclocking of the main ARM CPU.");  
   m_pItemsSelect[2]->addSelection("No");
   m_pItemsSelect[2]->addSelection("Yes");
   m_pItemsSelect[2]->setIsEditable();
   m_IndexCPUEnabled = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSlider[7] = new MenuItemSlider("CPU Speed (Mhz)", "Sets the main CPU frequency. Requires a reboot.", 700,1600, 1050, fSliderWidth);
   m_pItemsSlider[7]->setStep(25);
   m_IndexCPUSpeed = addMenuItem(m_pItemsSlider[7]);

   m_pItemsSelect[3] = new MenuItemSelect("Enable GPU Overclocking", "Enables overclocking of the GPU cores.");  
   m_pItemsSelect[3]->addSelection("No");
   m_pItemsSelect[3]->addSelection("Yes");
   m_pItemsSelect[3]->setIsEditable();
   m_IndexGPUEnabled = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSlider[8] = new MenuItemSlider("GPU Speed (Mhz)", "Sets the GPU frequency. Requires a reboot.", 200,1000,600, fSliderWidth);
   m_pItemsSlider[8]->setStep(25);
   m_IndexGPUSpeed = addMenuItem(m_pItemsSlider[8]);

   m_pItemsSelect[4] = new MenuItemSelect("Enable Overvoltage", "Enables overvotage on the CPU and GPU cores. You need to increase voltage as you increase the frequency of the CPU or GPU.");  
   m_pItemsSelect[4]->addSelection("No");
   m_pItemsSelect[4]->addSelection("Yes");
   m_IndexVoltageEnabled = addMenuItem(m_pItemsSelect[4]);

   m_pItemsSlider[9] = new MenuItemSlider("Overvoltage (steps)", "Sets the overvoltage value, in 0.025V increments. Requires a reboot.", 1,8,4, fSliderWidth);
   m_IndexVoltage = addMenuItem(m_pItemsSlider[9]);

   addMenuItem(new MenuItemSection("Other Settings"));

   m_pItemsSelect[5] = new MenuItemSelect("Enable ETH and DHCP", "Enables local network and DHCP on the vehicle.");  
   m_pItemsSelect[5]->addSelection("No");
   m_pItemsSelect[5]->addSelection("Yes");
   m_pItemsSelect[5]->setUseMultiViewLayout();
   m_IndexDHCP = addMenuItem(m_pItemsSelect[5]);

   m_IndexPriorities = -1;
   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE) )
   {
      m_IndexPriorities = addMenuItem(new MenuItem(L("Processes Priorities"), L("Sets vehicle processes priorities.")));
      m_pMenuItems[m_IndexPriorities]->showArrow();
      m_pMenuItems[m_IndexPriorities]->setTextColor(get_Color_Dev());
   }
   m_IndexReset = addMenuItem(new MenuItem("Reset CPU Freq", "Restarts the vehicle CPU and GPU frequencies to default values."));
   m_IndexReboot = addMenuItem(new MenuItem("Restart", "Restarts the vehicle."));
}

void MenuVehicleExpert::valuesToUI()
{
   // CPU

   m_pItemsSelect[2]->setSelection(g_pCurrentModel->processesPriorities.iFreqARM > 0);
   if ( g_pCurrentModel->processesPriorities.iFreqARM > 0 )
      m_pItemsSlider[7]->setCurrentValue(g_pCurrentModel->processesPriorities.iFreqARM);
   else
      m_pItemsSlider[7]->setCurrentValue(-g_pCurrentModel->processesPriorities.iFreqARM);
   m_pItemsSlider[7]->setEnabled(g_pCurrentModel->processesPriorities.iFreqARM > 0);

   m_pItemsSelect[3]->setSelection(g_pCurrentModel->processesPriorities.iFreqGPU > 0);
   if ( g_pCurrentModel->processesPriorities.iFreqGPU > 0 )
      m_pItemsSlider[8]->setCurrentValue(g_pCurrentModel->processesPriorities.iFreqGPU);
   else
      m_pItemsSlider[8]->setCurrentValue(-g_pCurrentModel->processesPriorities.iFreqGPU);
   m_pItemsSlider[8]->setEnabled(g_pCurrentModel->processesPriorities.iFreqGPU > 0);

   m_pItemsSelect[4]->setSelection(g_pCurrentModel->processesPriorities.iOverVoltage > 0);
   if ( g_pCurrentModel->processesPriorities.iOverVoltage > 0 )
      m_pItemsSlider[9]->setCurrentValue(g_pCurrentModel->processesPriorities.iOverVoltage);
   else
      m_pItemsSlider[9]->setCurrentValue(-g_pCurrentModel->processesPriorities.iOverVoltage);
   m_pItemsSlider[9]->setEnabled(g_pCurrentModel->processesPriorities.iOverVoltage > 0);   


   m_pItemsSelect[5]->setSelection(0);
   if ( g_pCurrentModel->enableDHCP )
      m_pItemsSelect[5]->setSelection(1);
}

void MenuVehicleExpert::addTopInfo()
{
   //char szBuffer[2048];
   //char szOutput[1024];
   //char szOutput2[1024];
      
   //addTopLine(" ");
   //addTopLine("Note: Changing overclocking settings requires a reboot.");
   //addTopLine(" ");
}

void MenuVehicleExpert::onShow()
{
   Menu::onShow();
}

void MenuVehicleExpert::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuVehicleExpert::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   if ( (10 == iChildMenuId/1000) && (1 == returnValue) )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_REBOOT, 0, NULL, 0) )
         valuesToUI();
      else
         menu_discard_all();
      return;
   }
}

void MenuVehicleExpert::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      log_line("MenuCPU: Command in progress");
      handle_commands_show_popup_progress();
      return;
   }

   if ( ! menu_check_current_model_ok_for_edit() )
   {
      log_line("MenuCPU: Can't edit model.");
      return;
   }

   log_line("MenuCPU: Selected item %d", m_SelectedIndex);

   if ( m_IndexDHCP == m_SelectedIndex )
   {
      int val = 1;
      if ( m_pItemsSelect[5]->getSelectedIndex() == 0 )
         val = 0;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_ENABLE_DHCP, val , NULL, 0) )
         valuesToUI();
      return;
   }

   if ( (-1 != m_IndexPriorities) && (m_IndexPriorities == m_SelectedIndex) )
   {
      add_menu_to_stack(new MenuVehicleCPUPriorities());
      return;
   }

   if ( m_IndexReboot == m_SelectedIndex )
   {
      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotFCTelemetry )
      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.uFCFlags & FC_TELE_FLAGS_ARMED )
      {
         char szTextW[256];
         if ( NULL != g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].pModel )
            sprintf(szTextW, "Your %s is armed. Are you sure you want to reboot the controller?", g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].pModel->getVehicleTypeString());
         MenuConfirmation* pMC = new MenuConfirmation("Warning! Reboot Confirmation", szTextW, 10);
         if ( g_pCurrentModel->rc_params.rc_enabled )
         {
            pMC->addTopLine(" ");
            pMC->addTopLine("Warning: You have the RC link enabled, the vehicle flight controller might not go into failsafe mode during reboot.");
         }
         add_menu_to_stack(pMC);
         return;
      }
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_REBOOT, 0, NULL, 0) )
         valuesToUI();
      else
         menu_discard_all();
      return;
   }

   if ( g_pCurrentModel->processesPriorities.iFreqARM == 0 )
      g_pCurrentModel->processesPriorities.iFreqARM = 900;
   if ( g_pCurrentModel->processesPriorities.iFreqGPU == 0 )
      g_pCurrentModel->processesPriorities.iFreqGPU = 400;

   bool sendUpdate = false;
   command_packet_overclocking_params params;
   params.freq_arm = g_pCurrentModel->processesPriorities.iFreqARM;
   params.freq_gpu = g_pCurrentModel->processesPriorities.iFreqGPU;
   params.overvoltage = g_pCurrentModel->processesPriorities.iOverVoltage;

   if ( m_IndexCPUEnabled == m_SelectedIndex )
   {
      params.freq_arm = -g_pCurrentModel->processesPriorities.iFreqARM;
      sendUpdate = true;
   }
   if ( m_IndexGPUEnabled == m_SelectedIndex )
   {
      params.freq_gpu = -g_pCurrentModel->processesPriorities.iFreqGPU;
      sendUpdate = true;
   }
   if ( m_IndexVoltageEnabled == m_SelectedIndex )
   {
      params.overvoltage = -g_pCurrentModel->processesPriorities.iOverVoltage;
      sendUpdate = true;
   }

   if ( m_IndexCPUSpeed == m_SelectedIndex )
   {
      params.freq_arm = m_pItemsSlider[7]->getCurrentValue();
      sendUpdate = true;
   }

   if ( m_IndexGPUSpeed == m_SelectedIndex )
   {
      params.freq_gpu = m_pItemsSlider[8]->getCurrentValue();
      sendUpdate = true;
   }

   if ( m_IndexVoltage == m_SelectedIndex )
   {
      params.overvoltage = m_pItemsSlider[9]->getCurrentValue();
      sendUpdate = true;
   }

   if ( m_IndexReset == m_SelectedIndex )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_RESET_CPU_SPEED, 0, NULL, 0) )
         valuesToUI();
   }
   
   if ( sendUpdate )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_OVERCLOCKING_PARAMS, 0, (u8*)(&params), sizeof(command_packet_overclocking_params)) )
         valuesToUI();
   }

}
