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
#include "menu_controller_expert.h"
#include "menu_controller_cpu_priorities.h"
#include "menu_item_section.h"
#include "../process_router_messages.h"
#include "../../base/hardware_files.h"
#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>


MenuControllerExpert::MenuControllerExpert(void)
:Menu(MENU_ID_CONTROLLER_EXPERT, L("CPU and Processes Settings"), NULL)
{
   m_Width = 0.34;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.15;

   readConfigFile();
   addTopInfo();

   m_IndexCPUEnabled = -1;
   m_IndexCPUSpeed = -1;
   m_IndexGPUEnabled = -1;
   m_IndexGPUSpeed = -1;
   m_IndexVoltageEnabled = -1;
   m_IndexVoltage = -1;
   m_IndexReset = -1;
   m_iIndexPriorities = -1;

   #if defined(HW_PLATFORM_RASPBERRY)
   float fSliderWidth = 0.12;
   m_pItemsSelect[2] = new MenuItemSelect(L("Enable CPU Overclocking"), L("Enables overclocking of the main CPU."));
   m_pItemsSelect[2]->addSelection(L("No"));
   m_pItemsSelect[2]->addSelection(L("Yes"));
   m_pItemsSelect[2]->setIsEditable();
   m_IndexCPUEnabled = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSlider[5] = new MenuItemSlider(L("CPU Speed (Mhz)"), L("Sets the main CPU frequency. Requires a reboot."), 700,1600, 1050, fSliderWidth);
   m_pItemsSlider[5]->setStep(25);
   m_IndexCPUSpeed = addMenuItem(m_pItemsSlider[5]);

   m_pItemsSelect[3] = new MenuItemSelect(L("Enable GPU Overclocking"), L("Enables overclocking of the GPU cores."));  
   m_pItemsSelect[3]->addSelection(L("No"));
   m_pItemsSelect[3]->addSelection(L("Yes"));
   m_pItemsSelect[3]->setIsEditable();
   m_IndexGPUEnabled = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSlider[6] = new MenuItemSlider(L("GPU Speed (Mhz)"), L("Sets the GPU frequency. Requires a reboot."), 200,1000,600, fSliderWidth);
   m_pItemsSlider[6]->setStep(25);
   m_IndexGPUSpeed = addMenuItem(m_pItemsSlider[6]);

   m_pItemsSelect[4] = new MenuItemSelect(L("Enable Overvoltage"), L("Enables overvotage on the CPU and GPU cores. You need to increase voltage as you increase speed."));  
   m_pItemsSelect[4]->addSelection(L("No"));
   m_pItemsSelect[4]->addSelection(L("Yes"));
   m_IndexVoltageEnabled = addMenuItem(m_pItemsSelect[4]);

   m_pItemsSlider[7] = new MenuItemSlider(L("Overvoltage (steps)"), L("Sets the overvoltage value, in 0.025V increments. Requires a reboot."), 1,8,4, fSliderWidth);
   m_IndexVoltage = addMenuItem(m_pItemsSlider[7]);

   m_IndexReset = addMenuItem(new MenuItem(L("Reset CPU Freq"), L("Resets the controller CPU and GPU frequencies to default values.")));
   #endif

   m_iIndexPriorities = -1;
   ControllerSettings* pCS = get_ControllerSettings();
   if ( (NULL != pCS) && pCS->iDeveloperMode )
   {
      m_iIndexPriorities = addMenuItem(new MenuItem(L("Processes Priorities"), L("Sets controller processes priorities.")));
      m_pMenuItems[m_iIndexPriorities]->showArrow();
      m_pMenuItems[m_iIndexPriorities]->setTextColor(get_Color_Dev());
   }
   m_IndexVersions = addMenuItem(new MenuItem(L("Modules versions"), L("Get all modules versions.")));
   m_IndexReboot = addMenuItem(new MenuItem(L("Restart"), L("Restarts the controller.")));
}

void MenuControllerExpert::valuesToUI()
{
   ControllerSettings* pcs = get_ControllerSettings();
   if ( NULL == pcs )
   {
      log_softerror_and_alarm("Failed to get pointer to controller settings structure");
      return;
   }

   // CPU

   #if defined(HW_PLATFORM_RASPBERRY)
   m_pItemsSelect[2]->setSelection(pcs->iFreqARM > 0);
   if ( pcs->iFreqARM <= 0 )
      m_pItemsSlider[5]->setCurrentValue(m_iDefaultARMFreq);
   else
      m_pItemsSlider[5]->setCurrentValue(pcs->iFreqARM);
   m_pItemsSlider[5]->setEnabled(pcs->iFreqARM > 0);

   m_pItemsSelect[3]->setSelection(pcs->iFreqGPU > 0);
   if ( pcs->iFreqGPU <= 0 )
      m_pItemsSlider[6]->setCurrentValue(m_iDefaultGPUFreq);
   else
      m_pItemsSlider[6]->setCurrentValue(pcs->iFreqGPU);
   m_pItemsSlider[6]->setEnabled(pcs->iFreqGPU > 0);

   m_pItemsSelect[4]->setSelection(pcs->iOverVoltage > 0);
   if ( pcs->iOverVoltage <= 0 )
      m_pItemsSlider[7]->setCurrentValue(m_iDefaultVoltage);
   else
      m_pItemsSlider[7]->setCurrentValue(pcs->iOverVoltage);
   m_pItemsSlider[7]->setEnabled(pcs->iOverVoltage > 0);
   #endif
}


void MenuControllerExpert::readConfigFile()
{
   ruby_signal_alive();

   ControllerSettings* pcs = get_ControllerSettings();
   if ( NULL == pcs )
   {
      log_softerror_and_alarm("Failed to get pointer to controller settings structure");
      return;
   }

   ruby_signal_alive();

   #if defined(HW_PLATFORM_RASPBERRY)
   m_iDefaultARMFreq = config_file_get_value("arm_freq");
   if ( m_iDefaultARMFreq < 0 )
      m_iDefaultARMFreq = - m_iDefaultARMFreq;

   m_iDefaultGPUFreq = config_file_get_value("gpu_freq");
   if ( m_iDefaultGPUFreq < 0 )
      m_iDefaultGPUFreq = - m_iDefaultGPUFreq;

   m_iDefaultVoltage = config_file_get_value("over_voltage");
   if ( m_iDefaultVoltage < 0 )
      m_iDefaultVoltage = - m_iDefaultVoltage;

   pcs->iFreqARM = config_file_get_value("arm_freq");
   ruby_signal_alive();
   pcs->iFreqGPU = config_file_get_value("gpu_freq");
   ruby_signal_alive();
   pcs->iOverVoltage = config_file_get_value("over_voltage");
   ruby_signal_alive();

   save_ControllerSettings();
   #endif

   ruby_signal_alive();
}

void MenuControllerExpert::writeConfigFile()
{
   ControllerSettings* pcs = get_ControllerSettings();
   if ( NULL == pcs )
   {
      log_softerror_and_alarm("Failed to get pointer to controller settings structure");
      return;
   }

   #if defined(HW_PLATFORM_RASPBERRY)
   ruby_signal_alive();
   hardware_mount_boot();
   hardware_sleep_ms(50);
   ruby_signal_alive();
   hw_execute_bash_command("cp /boot/config.txt config.txt", NULL);

   config_file_set_value("config.txt", "over_voltage", pcs->iOverVoltage);
   config_file_set_value("config.txt", "over_voltage_sdram", pcs->iOverVoltage);
   config_file_set_value("config.txt", "over_voltage_min", pcs->iOverVoltage);
   ruby_signal_alive();

   config_file_set_value("config.txt", "arm_freq", pcs->iFreqARM);
   config_file_set_value("config.txt", "arm_freq_min", pcs->iFreqARM);
   ruby_signal_alive();

   config_file_set_value("config.txt", "gpu_freq", pcs->iFreqGPU);
   config_file_set_value("config.txt", "gpu_freq_min", pcs->iFreqGPU);
   config_file_set_value("config.txt", "core_freq_min", pcs->iFreqGPU);
   config_file_set_value("config.txt", "h264_freq_min", pcs->iFreqGPU);
   config_file_set_value("config.txt", "isp_freq_min", pcs->iFreqGPU);
   config_file_set_value("config.txt", "v3d_freq_min", pcs->iFreqGPU);
   ruby_signal_alive();

   config_file_set_value("config.txt", "sdram_freq", pcs->iFreqGPU);
   config_file_set_value("config.txt", "sdram_freq_min", pcs->iFreqGPU);
   ruby_signal_alive();

   hw_execute_bash_command("cp config.txt /boot/config.txt", NULL);
   #endif
   ruby_signal_alive();
}

void MenuControllerExpert::addTopInfo()
{
   char szBuffer[2048];
   char szOutput[1024];
   char szOutput2[1024];

   log_line("Menu Controller Expert: adding info...");

   u32 board_type = hardware_getBoardType();
   const char* szBoard = str_get_hardware_board_name(board_type);
   ruby_signal_alive();

   hw_execute_bash_command_raw("nproc --all", szOutput);
   szOutput[strlen(szOutput)-1] = 0;

   int speed = hardware_get_cpu_speed();
   sprintf(szOutput2, "%d Mhz", speed);

   snprintf(szBuffer, sizeof(szBuffer)/sizeof(szBuffer[0]), "%s, %s CPU Cores, %s", szBoard, szOutput, szOutput2);
   addTopLine(szBuffer);

   #if defined(HW_PLATFORM_RASPBERRY)
   hw_execute_bash_command_raw("vcgencmd measure_clock core", szOutput);
   szOutput[0] = 'F';
   szOutput[strlen(szOutput)-1] = 0;
   sprintf(szBuffer, "GPU %s Hz", szOutput);
   addTopLine(szBuffer);
   #endif

   /*
   hw_execute_bash_command_raw("vcgencmd measure_clock h264", szOutput);
   szOutput[0] = 'F';
   szOutput[strlen(szOutput)-1] = 0;
   sprintf(szBuffer, "H264 %s Hz", szOutput);
   addTopLine(szBuffer);

   hw_execute_bash_command_raw("vcgencmd measure_clock isp", szOutput);
   szOutput[0] = 'F';
   szOutput[strlen(szOutput)-1] = 0;
   sprintf(szBuffer, "ISP %s Hz", szOutput);
   addTopLine(szBuffer);
   */

   #if defined(HW_PLATFORM_RASPBERRY)
   hw_execute_bash_command_raw("vcgencmd measure_volts core", szOutput);
   sprintf(szBuffer, "CPU Voltage: %s", szOutput);
   addTopLine(szBuffer);
   ruby_signal_alive();

   char szTmp[256];

   hw_execute_bash_command_raw("vcgencmd measure_volts sdram_c", szOutput);
   sprintf(szBuffer, "SDRAM C%s", szOutput);
   szBuffer[strlen(szBuffer)-1] = 0;
   hw_execute_bash_command_raw("vcgencmd measure_volts sdram_i", szOutput);

   snprintf(szTmp, sizeof(szTmp)/sizeof(szTmp[0]), ", I%s", szOutput);
   strcat(szBuffer, szTmp);
   szBuffer[strlen(szBuffer)-1] = 0;
   ruby_signal_alive();

   hw_execute_bash_command_raw("vcgencmd measure_volts sdram_p", szOutput);
   snprintf(szTmp, sizeof(szTmp)/sizeof(szTmp[0]), ", P%s", szOutput);
   strcat(szBuffer, szTmp);
   addTopLine(szBuffer);
   
   addTopLine(" ");
   addTopLine("Note: Changing overclocking settings requires a reboot.");
   addTopLine(" ");
   #endif
   
   log_line("Menu Controller Expert: added info.");
}


void MenuControllerExpert::onShow()
{
   Menu::onShow();
}

void MenuControllerExpert::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuControllerExpert::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   if ( (1 == iChildMenuId/1000) && (1 == returnValue) )
   {
      onEventReboot();
      hardware_reboot();
   }
}


void MenuControllerExpert::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   ControllerSettings* pcs = get_ControllerSettings();
   if ( NULL == pcs )
   {
      log_softerror_and_alarm("Failed to get pointer to controller settings structure");
      return;
   }

   if ( m_IndexReboot == m_SelectedIndex )
   {
      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotFCTelemetry )
      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.uFCFlags & FC_TELE_FLAGS_ARMED )
      {
         char szText[256];
         if ( NULL != g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].pModel )
            sprintf(szText, "Your %s is armed. Are you sure you want to reboot the controller?", g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].pModel->getVehicleTypeString());
         else
            strcpy(szText, "Your vehicle is armed. Are you sure you want to reboot the controller?");
         MenuConfirmation* pMC = new MenuConfirmation("Warning! Reboot Confirmation", szText, 1);
         if ( g_pCurrentModel->rc_params.rc_enabled )
         {
            pMC->addTopLine(" ");
            pMC->addTopLine("Warning: You have the RC link enabled, the vehicle flight controller might not go into failsafe mode during reboot.");
         }
         add_menu_to_stack(pMC);
         return;
      }

      onEventReboot();
      hardware_reboot();
      return;
   }

   bool bUpdatedConfig = false;

   if ( m_IndexCPUEnabled == m_SelectedIndex )
   {
      if ( m_iDefaultARMFreq <= 0 )
         m_iDefaultARMFreq = 900;
      if ( 1 == m_pItemsSelect[2]->getSelectedIndex() )
         pcs->iFreqARM = m_iDefaultARMFreq;
      else
         pcs->iFreqARM = -m_iDefaultARMFreq;
      valuesToUI();
      bUpdatedConfig = true;
   }

   if ( m_IndexGPUEnabled == m_SelectedIndex )
   {
      if ( m_iDefaultGPUFreq <= 0 )
         m_iDefaultGPUFreq = 400;

      if ( 1 == m_pItemsSelect[3]->getSelectedIndex() )
         pcs->iFreqGPU = m_iDefaultGPUFreq;
      else
         pcs->iFreqGPU = -m_iDefaultGPUFreq;
      valuesToUI();
      bUpdatedConfig = true;
   }

   if ( m_IndexVoltageEnabled == m_SelectedIndex )
   {
      if ( 1 == m_pItemsSelect[4]->getSelectedIndex() )
         pcs->iOverVoltage = m_iDefaultVoltage;
      else
         pcs->iOverVoltage = -m_iDefaultVoltage;
      valuesToUI();
      bUpdatedConfig = true;
   }

   if ( m_IndexCPUSpeed == m_SelectedIndex )
   {
      pcs->iFreqARM = m_pItemsSlider[5]->getCurrentValue();
      m_iDefaultARMFreq = pcs->iFreqARM;
      valuesToUI();
      bUpdatedConfig = true;
   }

   if ( m_IndexGPUSpeed == m_SelectedIndex )
   {
      pcs->iFreqGPU = m_pItemsSlider[6]->getCurrentValue();
      m_iDefaultGPUFreq = pcs->iFreqGPU;
      valuesToUI();
      bUpdatedConfig = true;
   }

   if ( m_IndexVoltage == m_SelectedIndex )
   {
      pcs->iOverVoltage = m_pItemsSlider[7]->getCurrentValue();
      m_iDefaultVoltage = pcs->iOverVoltage;
      valuesToUI();
      bUpdatedConfig = true;
   }

   if ( (m_iIndexPriorities != -1) && (m_iIndexPriorities == m_SelectedIndex) )
   {
      add_menu_to_stack(new MenuControllerCPUPriorities());
      return;
   }

   if ( m_IndexVersions == m_SelectedIndex )
   {
      char szComm[256];
      char szBuff[1024];
      char szOutput[1024];

      Menu* pMenu = new Menu(0,"All Modules Versions",NULL);
      pMenu->m_xPos = 0.32;
      pMenu->m_yPos = 0.17;
      pMenu->m_Width = 0.6;
      
      hw_execute_bash_command_raw_silent("./ruby_start -ver", szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ruby_start: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_controller -ver", szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ruby_controller: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_central -ver", szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ruby_central: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_rt_station -ver", szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ruby_rt_station: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_rx_telemetry -ver", szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ruby_rx_telemetry: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_tx_rc -ver", szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ruby_tx_rc: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_i2c -ver", szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ruby_i2c: %s", szOutput);
      pMenu->addTopLine(szBuff);

      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "./%s -ver", VIDEO_PLAYER_PIPE);
      hw_execute_bash_command_raw_silent(szComm, szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s: %s", VIDEO_PLAYER_PIPE, szOutput);
      pMenu->addTopLine(szBuff);

      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "./%s -ver", VIDEO_PLAYER_SM);
      hw_execute_bash_command_raw_silent(szComm, szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s: %s", VIDEO_PLAYER_SM, szOutput);
      pMenu->addTopLine(szBuff);

      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "./%s -ver", VIDEO_PLAYER_OFFLINE);
      hw_execute_bash_command_raw_silent(szComm, szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s: %s", VIDEO_PLAYER_OFFLINE, szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_update_worker -ver", szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ruby_update_worker: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_rt_vehicle -ver", szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ruby_rt_vehicle: %s", szOutput);
      pMenu->addTopLine(szBuff);

      hw_execute_bash_command_raw_silent("./ruby_tx_telemetry -ver", szOutput);
      removeTrailingNewLines(szOutput);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "ruby_tx_telemetry: %s", szOutput);
      pMenu->addTopLine(szBuff);

      add_menu_to_stack(pMenu);
      return;
   }

   if ( m_IndexReset == m_SelectedIndex )
   {
      u32 board_type = hardware_getBoardType() & BOARD_TYPE_MASK;
      pcs->iFreqARM = 900;
      if ( board_type == BOARD_TYPE_PIZERO2 )
         pcs->iFreqARM = 1000;
      else if ( board_type == BOARD_TYPE_PI3B )
         pcs->iFreqARM = 1200;
      else if ( board_type == BOARD_TYPE_PI3BPLUS || board_type == BOARD_TYPE_PI4B )
         pcs->iFreqARM = 1400;
      else if ( board_type != BOARD_TYPE_PIZERO && board_type != BOARD_TYPE_PIZEROW && board_type != BOARD_TYPE_NONE 
               && board_type != BOARD_TYPE_PI2B && board_type != BOARD_TYPE_PI2BV11 && board_type != BOARD_TYPE_PI2BV12 )
         pcs->iFreqARM = 1200;

      pcs->iFreqGPU = 400;
      pcs->iOverVoltage = 3;
      m_iDefaultARMFreq = pcs->iFreqARM;
      m_iDefaultGPUFreq = pcs->iFreqGPU;
      m_iDefaultVoltage = pcs->iOverVoltage;
      valuesToUI();
      bUpdatedConfig = true;
   }
   save_ControllerSettings();
   if ( bUpdatedConfig )
      writeConfigFile();
}
