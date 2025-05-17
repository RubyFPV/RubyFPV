/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga petrusoroaga@yahoo.com
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
#include "menu_system_alarms.h"

#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>

MenuSystemAlarms::MenuSystemAlarms(void)
:Menu(MENU_ID_SYSTEM_ALARMS, L("Configure System Alarms"), NULL)
{
   m_Width = 0.42;
   m_yPos = 0.28;
   m_xPos = menu_get_XStartPos(m_Width);

   m_pItemsSelect[0] = new MenuItemSelect(L("All Alarms"), L("Turn all alarms on or off."));
   m_pItemsSelect[0]->addSelection(L("Disabled"));
   m_pItemsSelect[0]->addSelection(L("Enabled"));
   m_pItemsSelect[0]->addSelection(L("Custom"));
   m_pItemsSelect[0]->setUseMultiViewLayout();
   m_IndexAllAlarms = addMenuItem( m_pItemsSelect[0]);

   addSeparator();

   m_pItemsSelect[8] = new MenuItemSelect(L("Video Stream Alarms"), L("Turn alarms about video stream output on or off."));
   m_pItemsSelect[8]->addSelection(L("Disabled"));
   m_pItemsSelect[8]->addSelection(L("Enabled"));
   m_pItemsSelect[8]->setUseMultiViewLayout();
   m_IndexAlarmControllerIOErrors = addMenuItem( m_pItemsSelect[8]);


   m_pItemsSelect[1] = new MenuItemSelect(L("Invalid Radio Data"), L("Turn this alarm on or off."));
   m_pItemsSelect[1]->addSelection(L("Disabled"));
   m_pItemsSelect[1]->addSelection(L("Enabled"));
   m_pItemsSelect[1]->setUseMultiViewLayout();
   m_IndexAlarmInvalidRadioPackets = addMenuItem( m_pItemsSelect[1]);

   m_pItemsSelect[5] = new MenuItemSelect(L("Controller Link Lost"), L("Turn this alarm on or off."));
   m_pItemsSelect[5]->addSelection(L("Disabled"));
   m_pItemsSelect[5]->addSelection(L("Enabled"));
   m_pItemsSelect[5]->setUseMultiViewLayout();
   m_IndexAlarmControllerLink = addMenuItem( m_pItemsSelect[5]);
   
   m_pItemsSelect[2] = new MenuItemSelect(L("Video Overload"), L("Turn this alarm on or off."));
   m_pItemsSelect[2]->addSelection(L("Disabled"));
   m_pItemsSelect[2]->addSelection(L("Enabled"));
   m_pItemsSelect[2]->setUseMultiViewLayout();
   m_IndexAlarmVideoDataOverload = addMenuItem( m_pItemsSelect[2]);

   m_pItemsSelect[3] = new MenuItemSelect(L("Vehicle CPU or Rx Overload"), L("Turn this alarm on or off."));
   m_pItemsSelect[3]->addSelection(L("Disabled"));
   m_pItemsSelect[3]->addSelection(L("Enabled"));
   m_pItemsSelect[3]->setUseMultiViewLayout();
   m_IndexAlarmVehicleCPUOverload = addMenuItem( m_pItemsSelect[3]);

   m_pItemsSelect[6] = new MenuItemSelect(L("Vehicle Radio Timeout"), L("Alarm when a vehicle radio interface times out reading a pending received packet. Turn this alarm on or off."));
   m_pItemsSelect[6]->addSelection(L("Disabled"));
   m_pItemsSelect[6]->addSelection(L("Enabled"));
   m_pItemsSelect[6]->setUseMultiViewLayout();
   m_IndexAlarmVehicleRxTimeout = addMenuItem( m_pItemsSelect[6]);

   addSeparator();

   m_pItemsSelect[10] = new MenuItemSelect(L("Recording SD Card Overload"), L("Alarm when recording to SD card slows down the system. Turn this alarm on or off."));
   m_pItemsSelect[10]->addSelection(L("Disabled"));
   m_pItemsSelect[10]->addSelection(L("Enabled"));
   m_pItemsSelect[10]->setUseMultiViewLayout();
   m_IndexAlarmControllerRecording = addMenuItem( m_pItemsSelect[10]);

   m_pItemsSelect[4] = new MenuItemSelect(L("Controller CPU or Rx Overload"), L("Turn this alarm on or off."));
   m_pItemsSelect[4]->addSelection(L("Disabled"));
   m_pItemsSelect[4]->addSelection(L("Enabled"));
   m_pItemsSelect[4]->setUseMultiViewLayout();
   m_IndexAlarmControllerCPUOverload = addMenuItem( m_pItemsSelect[4]);

   m_pItemsSelect[7] = new MenuItemSelect(L("Controller Radio Timeout"), L("Alarm when a controller radio interface times out reading a pending received packet. Turn this alarm on or off."));
   m_pItemsSelect[7]->addSelection(L("Disabled"));
   m_pItemsSelect[7]->addSelection(L("Enabled"));
   m_pItemsSelect[7]->setUseMultiViewLayout();
   m_IndexAlarmControllerRxTimeout = addMenuItem( m_pItemsSelect[7]);

   m_pItemsSelect[9] = new MenuItemSelect(L("Developer Alarms"), L("Disables/Enables developer alarms."));
   m_pItemsSelect[9]->addSelection(L("Disabled"));
   m_pItemsSelect[9]->addSelection(L("Enabled"));
   m_pItemsSelect[9]->setUseMultiViewLayout();
   m_IndexAlarmsDev = addMenuItem( m_pItemsSelect[9]);

   Preferences* pP = get_Preferences();

   m_bMenuSystemAlarmsIsOnCustomOption = true;
   if ( (pP->uEnabledAlarms == 0xFFFFFFFF) || (pP->uEnabledAlarms == 0) )
      m_bMenuSystemAlarmsIsOnCustomOption = false;
}

void MenuSystemAlarms::valuesToUI()
{
   
   Preferences* pP = get_Preferences();

   if ( m_bMenuSystemAlarmsIsOnCustomOption )
      m_pItemsSelect[0]->setSelectedIndex(2);
   else if ( 0 == pP->uEnabledAlarms )
      m_pItemsSelect[0]->setSelectedIndex(0);
   else if ( 0xFFFFFFFF == pP->uEnabledAlarms )
      m_pItemsSelect[0]->setSelectedIndex(1);
   else
      m_pItemsSelect[0]->setSelectedIndex(2);

   m_pItemsSelect[8]->setEnabled(true);
   m_pItemsSelect[8]->setSelectedIndex((pP->uEnabledAlarms & ALARM_ID_CONTROLLER_IO_ERROR)?1:0);

   m_pItemsSelect[1]->setEnabled(true);
   m_pItemsSelect[1]->setSelectedIndex((pP->uEnabledAlarms & ALARM_ID_RECEIVED_INVALID_RADIO_PACKET)?1:0);

   m_pItemsSelect[2]->setEnabled(true);
   m_pItemsSelect[2]->setSelectedIndex((pP->uEnabledAlarms & ALARM_ID_VEHICLE_VIDEO_DATA_OVERLOAD)?1:0);

   m_pItemsSelect[3]->setEnabled(true);
   m_pItemsSelect[3]->setSelectedIndex((pP->uEnabledAlarms & (ALARM_ID_VEHICLE_CPU_LOOP_OVERLOAD | ALARM_ID_VEHICLE_CPU_RX_LOOP_OVERLOAD))?1:0);

   m_pItemsSelect[4]->setEnabled(true);
   m_pItemsSelect[4]->setSelectedIndex((pP->uEnabledAlarms & (ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD | ALARM_ID_CONTROLLER_CPU_RX_LOOP_OVERLOAD))?1:0);

   m_pItemsSelect[5]->setEnabled(true);
   m_pItemsSelect[5]->setSelectedIndex((pP->uEnabledAlarms & ALARM_ID_LINK_TO_CONTROLLER_LOST)?1:0);

   m_pItemsSelect[6]->setEnabled(true);
   m_pItemsSelect[6]->setSelectedIndex((pP->uEnabledAlarms & ALARM_ID_VEHICLE_RX_TIMEOUT)?1:0);

   m_pItemsSelect[7]->setEnabled(true);
   m_pItemsSelect[7]->setSelectedIndex((pP->uEnabledAlarms & ALARM_ID_CONTROLLER_RX_TIMEOUT)?1:0);

   m_pItemsSelect[9]->setEnabled(true);
   m_pItemsSelect[9]->setSelectedIndex((pP->uEnabledAlarms & ALARM_ID_DEVELOPER_ALARM)?1:0);

   m_pItemsSelect[10]->setEnabled(true);
   m_pItemsSelect[10]->setSelectedIndex((pP->uEnabledAlarms & ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD_RECORDING)?1:0);

   if ( 0 == m_pItemsSelect[0]->getSelectedIndex() )
   {
      m_pItemsSelect[1]->setEnabled(false);
      m_pItemsSelect[2]->setEnabled(false);
      m_pItemsSelect[3]->setEnabled(false);
      m_pItemsSelect[4]->setEnabled(false);
      m_pItemsSelect[5]->setEnabled(false);
      m_pItemsSelect[6]->setEnabled(false);
      m_pItemsSelect[7]->setEnabled(false);
      m_pItemsSelect[8]->setEnabled(false);
      m_pItemsSelect[9]->setEnabled(false);
      m_pItemsSelect[10]->setEnabled(false);

      m_pItemsSelect[1]->setSelectedIndex(0);
      m_pItemsSelect[2]->setSelectedIndex(0);
      m_pItemsSelect[3]->setSelectedIndex(0);
      m_pItemsSelect[4]->setSelectedIndex(0);
      m_pItemsSelect[5]->setSelectedIndex(0);
      m_pItemsSelect[6]->setSelectedIndex(0);
      m_pItemsSelect[7]->setSelectedIndex(0);
      m_pItemsSelect[8]->setSelectedIndex(0);
      m_pItemsSelect[9]->setSelectedIndex(0);
      m_pItemsSelect[10]->setSelectedIndex(0);
   }   
}

void MenuSystemAlarms::onShow()
{
   Menu::onShow();
}


void MenuSystemAlarms::Render()
{
   RenderPrepare();
   float yEnd = RenderFrameAndTitle();
   float y = yEnd;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yEnd);
}

void MenuSystemAlarms::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   Preferences* pP = get_Preferences();

   if ( m_IndexAllAlarms == m_SelectedIndex )
   {
      m_bMenuSystemAlarmsIsOnCustomOption = false;
      if ( 0 == m_pItemsSelect[0]->getSelectedIndex() )
         pP->uEnabledAlarms = 0;
      else if ( 1 == m_pItemsSelect[0]->getSelectedIndex() )
         pP->uEnabledAlarms = 0xFFFFFFFF;
      else
        m_bMenuSystemAlarmsIsOnCustomOption = true;
   }

   if ( m_IndexAlarmControllerIOErrors == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[8]->getSelectedIndex() )
         pP->uEnabledAlarms &= ~ALARM_ID_CONTROLLER_IO_ERROR;
      else
         pP->uEnabledAlarms |= ALARM_ID_CONTROLLER_IO_ERROR;
   }

   if ( m_IndexAlarmInvalidRadioPackets == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[1]->getSelectedIndex() )
         pP->uEnabledAlarms &= ~(ALARM_ID_RECEIVED_INVALID_RADIO_PACKET | ALARM_ID_RECEIVED_INVALID_RADIO_PACKET_RECONSTRUCTED | ALARM_ID_CONTROLLER_RECEIVED_INVALID_RADIO_PACKET);
      else
         pP->uEnabledAlarms |= (ALARM_ID_RECEIVED_INVALID_RADIO_PACKET | ALARM_ID_RECEIVED_INVALID_RADIO_PACKET_RECONSTRUCTED | ALARM_ID_CONTROLLER_RECEIVED_INVALID_RADIO_PACKET);
   }

   if ( m_IndexAlarmControllerLink == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[5]->getSelectedIndex() )
         pP->uEnabledAlarms &= ~(ALARM_ID_LINK_TO_CONTROLLER_LOST | ALARM_ID_VEHICLE_VIDEO_TX_OVERLOAD);
      else
         pP->uEnabledAlarms |= (ALARM_ID_LINK_TO_CONTROLLER_LOST | ALARM_ID_LINK_TO_CONTROLLER_RECOVERED);
   }

   if ( m_IndexAlarmVideoDataOverload == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[2]->getSelectedIndex() )
         pP->uEnabledAlarms &= ~(ALARM_ID_VEHICLE_VIDEO_DATA_OVERLOAD | ALARM_ID_VEHICLE_VIDEO_TX_OVERLOAD);
      else
         pP->uEnabledAlarms |= (ALARM_ID_VEHICLE_VIDEO_DATA_OVERLOAD | ALARM_ID_VEHICLE_VIDEO_TX_OVERLOAD);
   }

   if ( m_IndexAlarmVehicleCPUOverload == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[3]->getSelectedIndex() )
         pP->uEnabledAlarms &= ~(ALARM_ID_VEHICLE_CPU_LOOP_OVERLOAD | ALARM_ID_VEHICLE_CPU_RX_LOOP_OVERLOAD);
      else
         pP->uEnabledAlarms |= (ALARM_ID_VEHICLE_CPU_LOOP_OVERLOAD | ALARM_ID_VEHICLE_CPU_RX_LOOP_OVERLOAD);
   }

   if ( m_IndexAlarmVehicleRxTimeout == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[6]->getSelectedIndex() )
         pP->uEnabledAlarms &= ~(ALARM_ID_VEHICLE_RX_TIMEOUT);
      else
         pP->uEnabledAlarms |= (ALARM_ID_VEHICLE_RX_TIMEOUT);
   }


   if ( m_IndexAlarmControllerCPUOverload == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[4]->getSelectedIndex() )
         pP->uEnabledAlarms &= ~(ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD | ALARM_ID_CONTROLLER_CPU_RX_LOOP_OVERLOAD);
      else
         pP->uEnabledAlarms |= (ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD | ALARM_ID_CONTROLLER_CPU_RX_LOOP_OVERLOAD);
   }

   if ( m_IndexAlarmControllerRxTimeout == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[7]->getSelectedIndex() )
         pP->uEnabledAlarms &= ~(ALARM_ID_CONTROLLER_RX_TIMEOUT);
      else
         pP->uEnabledAlarms |= (ALARM_ID_CONTROLLER_RX_TIMEOUT);
   }

   if ( m_IndexAlarmControllerRecording == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[10]->getSelectedIndex() )
         pP->uEnabledAlarms &= ~(ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD_RECORDING);
      else
         pP->uEnabledAlarms |= (ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD_RECORDING);
   }

   if ( m_IndexAlarmsDev == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[9]->getSelectedIndex() )
         pP->uEnabledAlarms &= ~(ALARM_ID_DEVELOPER_ALARM);
      else
         pP->uEnabledAlarms |= (ALARM_ID_DEVELOPER_ALARM);
   }

   save_Preferences();
   valuesToUI();
}

