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
#include "menu_vehicle_cpu_oipc.h"
#include "menu_vehicle_cpu_priorities.h"
#include "menu_item_select.h"
#include "menu_confirmation.h"
#include "menu_item_section.h"

MenuVehicleCPU_OIPC::MenuVehicleCPU_OIPC(void)
:Menu(MENU_ID_VEHICLE_CPU_OIPC, "CPU Settings", NULL)
{
   m_Width = 0.36;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.13;
   float fSliderWidth = 0.10;
   setSubTitle("Change vehicle CPU settings, for expert users.");

   m_pItemsSlider[5] = new MenuItemSlider("CPU Speed (Mhz)", "Sets the main CPU frequency.", 700, 1200, 900, fSliderWidth);
   m_pItemsSlider[5]->setStep(25);
   m_IndexCPUSpeed = addMenuItem(m_pItemsSlider[5]);
   
   m_pItemsSelect[5] = new MenuItemSelect("GPU Boost", "Increases the video encoder clock speed.");
   m_pItemsSelect[5]->addSelection("Off");
   m_pItemsSelect[5]->addSelection("Medium");
   m_pItemsSelect[5]->addSelection("High");
   m_pItemsSelect[5]->addSelection("Custom");
   m_pItemsSelect[5]->setIsEditable();
   m_IndexGPUBoost = addMenuItem(m_pItemsSelect[5]);

   m_pItemsSelect[7] = new MenuItemSelect("GPU Freq Core 1", "Set custom freq for GPU core 1");
   m_pItemsSelect[7]->addSelection("384 Mhz");
   m_pItemsSelect[7]->addSelection("432 Mhz");
   m_pItemsSelect[7]->addSelection("480 Mhz");
   m_pItemsSelect[7]->setIsEditable();
   m_IndexGPUFreqCore1 = addMenuItem(m_pItemsSelect[7]);

   m_pItemsSelect[8] = new MenuItemSelect("GPU Freq Core 2", "Set custom freq for GPU core 2");
   m_pItemsSelect[8]->addSelection("320 Mhz");
   m_pItemsSelect[8]->addSelection("336 Mhz");
   m_pItemsSelect[8]->addSelection("348 Mhz");
   m_pItemsSelect[8]->addSelection("384 Mhz");
   m_pItemsSelect[8]->setIsEditable();
   m_IndexGPUFreqCore2 = addMenuItem(m_pItemsSelect[8]);

   m_IndexPriorities = -1;
   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE) )
   {
      m_IndexPriorities = addMenuItem(new MenuItem(L("Processes Priorities"), L("Sets vehicle processes priorities.")));
      m_pMenuItems[m_IndexPriorities]->showArrow();
      m_pMenuItems[m_IndexPriorities]->setTextColor(get_Color_Dev());
   }
}

void MenuVehicleCPU_OIPC::valuesToUI()
{
   if ( g_pCurrentModel->processesPriorities.iFreqARM > 0 )
      m_pItemsSlider[5]->setCurrentValue(g_pCurrentModel->processesPriorities.iFreqARM);

   m_pItemsSelect[5]->setSelectedIndex(0);
   if ( g_pCurrentModel->processesPriorities.iFreqGPU < 3 )
   {
      m_pItemsSelect[5]->setSelectedIndex(g_pCurrentModel->processesPriorities.iFreqGPU);
      m_pItemsSelect[7]->setEnabled(false);
      m_pItemsSelect[8]->setEnabled(false);

      if ( 0 == g_pCurrentModel->processesPriorities.iFreqGPU )
      {
         m_pItemsSelect[7]->setSelectedIndex(0);
         m_pItemsSelect[8]->setSelectedIndex(0);
      }
      if ( 1 == g_pCurrentModel->processesPriorities.iFreqGPU )
      {
         m_pItemsSelect[7]->setSelectedIndex(1);
         m_pItemsSelect[8]->setSelectedIndex(1);
      }
      if ( 2 == g_pCurrentModel->processesPriorities.iFreqGPU )
      {
         m_pItemsSelect[7]->setSelectedIndex(2);
         m_pItemsSelect[8]->setSelectedIndex(2);
      }
   }
   else
   {
      m_pItemsSelect[5]->setSelectedIndex(3);
      m_pItemsSelect[7]->setEnabled(true);
      m_pItemsSelect[8]->setEnabled(true);

      int iCore1 = (g_pCurrentModel->processesPriorities.iFreqGPU/10)%10;
      int iCore2 = (g_pCurrentModel->processesPriorities.iFreqGPU/100)%10;
      m_pItemsSelect[7]->setSelectedIndex(iCore1);
      m_pItemsSelect[8]->setSelectedIndex(iCore2);
   }
}

void MenuVehicleCPU_OIPC::onShow()
{
   Menu::onShow();
}


void MenuVehicleCPU_OIPC::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuVehicleCPU_OIPC::onReturnFromChild(int iChildMenuId, int returnValue)
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

void MenuVehicleCPU_OIPC::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( ! menu_check_current_model_ok_for_edit() )
      return;

   bool sendUpdate = false;
   command_packet_overclocking_params params;
   params.freq_arm = g_pCurrentModel->processesPriorities.iFreqARM;
   params.freq_gpu = g_pCurrentModel->processesPriorities.iFreqGPU;
   params.overvoltage = g_pCurrentModel->processesPriorities.iOverVoltage;
   
   if ( m_IndexCPUSpeed == m_SelectedIndex )
   {
      params.freq_arm = m_pItemsSlider[5]->getCurrentValue();
      sendUpdate = true;
   }
   if ( m_IndexGPUBoost == m_SelectedIndex )
   {
      params.freq_gpu = m_pItemsSelect[5]->getSelectedIndex();
      sendUpdate = true;
   }

   if ( (m_IndexGPUFreqCore1 == m_SelectedIndex) || (m_IndexGPUFreqCore2 == m_SelectedIndex) )
   {
      params.freq_gpu = 3;
      params.freq_gpu += m_pItemsSelect[7]->getSelectedIndex()*10;
      params.freq_gpu += m_pItemsSelect[8]->getSelectedIndex()*100;
      sendUpdate = true;
   }

   if ( (-1 != m_IndexPriorities) && (m_IndexPriorities == m_SelectedIndex) )
   {
      add_menu_to_stack(new MenuVehicleCPUPriorities());
      return;
   }

   if ( sendUpdate )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_OVERCLOCKING_PARAMS, 0, (u8*)(&params), sizeof(command_packet_overclocking_params)) )
         valuesToUI();
   }
}
