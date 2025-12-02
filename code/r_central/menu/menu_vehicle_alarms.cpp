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
#include "menu_vehicle_alarms.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

MenuVehicleAlarms::MenuVehicleAlarms(void)
:Menu(MENU_ID_VEHICLE_ALARMS, L("Vehicle Alarms and Warnings Settings"), NULL)
{
   m_Width = 0.36;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.20;

   m_pItemsSelect[0] = new MenuItemSelect(L("Enable Voltage Alarm"), L("Shows an alarm on screen when vehicle battery voltage drops below a value."));  
   m_pItemsSelect[0]->addSelection(L("No"));
   m_pItemsSelect[0]->addSelection(L("Yes"));
   m_pItemsSelect[0]->setIsEditable();
   addMenuItem(m_pItemsSelect[0]);

   m_pItemsRange[0] = new MenuItemRange(L("Alarm voltage"), L("Voltage at which the alarm will trigger."), 2.0, 4.6, g_pCurrentModel->osd_params.voltage_alarm, 0.1 );  
   m_pItemsRange[0]->setSufix("V");
   addMenuItem(m_pItemsRange[0]);


   m_pItemsSelect[3] =  new MenuItemSelect(L("Enable Motor Alarm"), L("Shows an alarm on screen when throttle is active but motor is not responding (not consumming current). Set the threshold for current consumed by the vehicle when idle. Works only with MAVLink telemetry."));
   m_pItemsSelect[3]->addSelection("No");
   m_pItemsSelect[3]->addSelection("0.5 Amp threshold");
   m_pItemsSelect[3]->addSelection("1.0 Amp threshold");
   m_pItemsSelect[3]->addSelection("1.5 Amp threshold");
   m_pItemsSelect[3]->addSelection("2.0 Amp threshold");
   m_pItemsSelect[3]->addSelection("2.5 Amp threshold");
   m_pItemsSelect[3]->addSelection("3.0 Amp threshold");
   m_pItemsSelect[3]->addSelection("5.0 Amp threshold");
   m_pItemsSelect[3]->addSelection("10.0 Amp threshold");
   m_pItemsSelect[3]->setIsEditable();
   m_IndexAlarmMotorCurrent = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSelect[1] = new MenuItemSelect(L("Enable Overload Alarm"), L("Shows an alarm when the vehicle CPU is overloaded or the radio link is overloaded."));  
   m_pItemsSelect[1]->addSelection("No");
   m_pItemsSelect[1]->addSelection("Yes");
   m_IndexOverload = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSlider[0] = new MenuItemSlider(L("Threshold Temperature"), L("Sets the temperature at which to activate the PIT mode (if enabled) and also flashes the temperature in OSD."), 0,120,50, 0.12);
   m_iIndexTemperature = addMenuItem(m_pItemsSlider[0]);

   m_pItemsRange[1] = new MenuItemRange(L("Pitch/Roll warning angle"), L("Pitch and roll angle at witch to show a OSD warning indication about attitude (0 for disabled)."), 0, 80, g_pCurrentModel->osd_params.ahi_warning_angle, 5 );  
   m_pItemsRange[1]->setSufix("°");
   addMenuItem(m_pItemsRange[1]);

   m_pItemsSelect[2] = new MenuItemSelect(L("Link to Controller Lost Alarm"), L("Shows an alarm when the vehicle looses connection with the controller. That is, vehicle does not receive the uplink data for a period of time."));  
   m_pItemsSelect[2]->addSelection(L("Disabled"));
   m_pItemsSelect[2]->addSelection(L("Enabled"));
   m_pItemsSelect[2]->setIsEditable();
   m_IndexLinkLost = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSelect[4] = new MenuItemSelect(L("Flash OSD on Telemetry Lost"), L("Flashes the OSD whenever a telemetry data packet is lost."));
   m_pItemsSelect[4]->addSelection(L("No"));
   m_pItemsSelect[4]->addSelection(L("Yes"));
   m_pItemsSelect[4]->addSelection(L("On individual OSD screens"));
   m_pItemsSelect[4]->setIsEditable();
   m_iIndexFlashOSDOnTelemLost = addMenuItem(m_pItemsSelect[4]);
}

MenuVehicleAlarms::~MenuVehicleAlarms()
{
}

void MenuVehicleAlarms::valuesToUI()
{
   m_pItemsSelect[0]->setSelection(g_pCurrentModel->osd_params.voltage_alarm_enabled);

   m_pItemsSlider[0]->setCurrentValue((g_pCurrentModel->hwCapabilities.uHWFlags & 0xFF00) >> 8);

   m_pItemsRange[0]->setCurrentValue(g_pCurrentModel->osd_params.voltage_alarm);
   m_pItemsRange[1]->setCurrentValue(g_pCurrentModel->osd_params.ahi_warning_angle);

   if ( g_pCurrentModel->osd_params.voltage_alarm_enabled )
      m_pItemsRange[0]->setEnabled(true);
   else
      m_pItemsRange[0]->setEnabled(false);

   m_pItemsSelect[1]->setSelection(g_pCurrentModel->osd_params.show_overload_alarm);

   log_line("MenuAlarms: Current motor alarm value: %d/%d",
      g_pCurrentModel->alarms_params.uAlarmMotorCurrentThreshold & (1<<7),
      g_pCurrentModel->alarms_params.uAlarmMotorCurrentThreshold & 0x7F );
   int iCurrent = g_pCurrentModel->alarms_params.uAlarmMotorCurrentThreshold & 0x7F;
   if ( ! (g_pCurrentModel->alarms_params.uAlarmMotorCurrentThreshold & (1<<7)) )
      m_pItemsSelect[3]->setSelectedIndex(0);
   else if ( iCurrent <= 5 )
      m_pItemsSelect[3]->setSelectedIndex(1);
   else if ( iCurrent <= 10 )
      m_pItemsSelect[3]->setSelectedIndex(2);
   else if ( iCurrent <= 15 )
      m_pItemsSelect[3]->setSelectedIndex(3);
   else if ( iCurrent <= 20 )
      m_pItemsSelect[3]->setSelectedIndex(4);
   else if ( iCurrent <= 25 )
      m_pItemsSelect[3]->setSelectedIndex(5);
   else if ( iCurrent <= 30 )
      m_pItemsSelect[3]->setSelectedIndex(6);
   else if ( iCurrent <= 50 )
      m_pItemsSelect[3]->setSelectedIndex(7);
   else
      m_pItemsSelect[3]->setSelectedIndex(8);

   bool bHasFlashOn = false;
   bool bHasFlashOff = false;
   for( int i=0; i<MODEL_MAX_OSD_SCREENS; i++ )
   {
      if ( g_pCurrentModel->osd_params.osd_flags2[i] & OSD_FLAG2_FLASH_OSD_ON_TELEMETRY_DATA_LOST )
         bHasFlashOn = true;
      else
         bHasFlashOff = true;
   }

   if ( bHasFlashOn && bHasFlashOff )
      m_pItemsSelect[4]->setSelectedIndex(2);
   else if ( bHasFlashOn )
      m_pItemsSelect[4]->setSelectedIndex(1);
   else
      m_pItemsSelect[4]->setSelectedIndex(0);
}

void MenuVehicleAlarms::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuVehicleAlarms::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   bool sendToVehicle = false;
   osd_parameters_t params;
   memcpy(&params, &(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));

   if ( 0 == m_SelectedIndex )
   {
      params.voltage_alarm_enabled = m_pItemsSelect[0]->getSelectedIndex();
      sendToVehicle = true;
   }

   if ( 1 == m_SelectedIndex )
   {
      params.voltage_alarm = m_pItemsRange[0]->getCurrentValue();
      if ( params.voltage_alarm < 0 )
         params.voltage_alarm = 0;
      sendToVehicle = true;
   }

   if ( 2 == m_SelectedIndex )
   {
      params.ahi_warning_angle = m_pItemsRange[1]->getCurrentValue();
      if ( params.ahi_warning_angle < 0 )
         params.ahi_warning_angle = 0;
      sendToVehicle = true;
   }
   
   if ( m_iIndexTemperature == m_SelectedIndex )
   {
      u32 uCommandParam = (((u32)(m_pItemsSlider[0]->getCurrentValue())) & 0xFF);
      
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_TEMPERATURE_THRESHOLD, uCommandParam, NULL, 0) )
         valuesToUI();
      return;
   }

   if ( m_IndexOverload == m_SelectedIndex )
   {
      params.show_overload_alarm = (bool) m_pItemsSelect[1]->getSelectedIndex();
      sendToVehicle = true;
   }

   if ( m_IndexLinkLost == m_SelectedIndex )
   {
      if ( m_pItemsSelect[2]->getSelectedIndex() == 0 )
      {
         for( int i=0; i<MODEL_MAX_OSD_SCREENS; i++ )
            params.osd_preferences[i] &= ~(OSD_PREFERENCES_BIT_FLAG_SHOW_CONTROLLER_LINK_LOST_ALARM); // controller link lost alarm disabled
      }
      else
      {
         for( int i=0; i<MODEL_MAX_OSD_SCREENS; i++ )
            params.osd_preferences[i] |= OSD_PREFERENCES_BIT_FLAG_SHOW_CONTROLLER_LINK_LOST_ALARM; // controller link lost alarm enabled
      }
      sendToVehicle = true;
   }

   if ( m_IndexAlarmMotorCurrent == m_SelectedIndex )
   {
      int iIndex = m_pItemsSelect[3]->getSelectedIndex();

      type_alarms_parameters params;
      memcpy(&params, &(g_pCurrentModel->alarms_params), sizeof(type_alarms_parameters));

      if ( 0 == iIndex )
         params.uAlarmMotorCurrentThreshold &= 0x7F;
      else
      {
         if ( iIndex == 1 )
            params.uAlarmMotorCurrentThreshold = 5;
         if ( iIndex == 2 )
            params.uAlarmMotorCurrentThreshold = 10;
         if ( iIndex == 3 )
            params.uAlarmMotorCurrentThreshold = 15;
         if ( iIndex == 4 )
            params.uAlarmMotorCurrentThreshold = 20;
         if ( iIndex == 5 )
            params.uAlarmMotorCurrentThreshold = 25;
         if ( iIndex == 6 )
            params.uAlarmMotorCurrentThreshold = 30;
         if ( iIndex == 7 )
            params.uAlarmMotorCurrentThreshold = 50;
         if ( iIndex == 8 )
            params.uAlarmMotorCurrentThreshold = 100;

         params.uAlarmMotorCurrentThreshold |= (1<<7);
      }

      if ( g_pCurrentModel->is_spectator )
      {
         memcpy(&(g_pCurrentModel->alarms_params), &params, sizeof(type_alarms_parameters));
         saveControllerModel(g_pCurrentModel);
         valuesToUI();
         return;
      }

      log_line("MenuAlarms: Sending new motor alarm value: %d/%d",
         params.uAlarmMotorCurrentThreshold & (1<<7),
         params.uAlarmMotorCurrentThreshold & 0x7F );

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_ALARMS_PARAMS, 0, (u8*)&params, sizeof(type_alarms_parameters)) )
         valuesToUI();
      return;
   }

   if ( m_iIndexFlashOSDOnTelemLost == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[4]->getSelectedIndex() )
      {
         sendToVehicle = true;
         for( int i=0; i<MODEL_MAX_OSD_SCREENS; i++ )
            params.osd_flags2[i] &= ~OSD_FLAG2_FLASH_OSD_ON_TELEMETRY_DATA_LOST;
      }
      else if ( 1 == m_pItemsSelect[4]->getSelectedIndex() )
      {
         sendToVehicle = true;
         for( int i=0; i<MODEL_MAX_OSD_SCREENS; i++ )
            params.osd_flags2[i] |= OSD_FLAG2_FLASH_OSD_ON_TELEMETRY_DATA_LOST;
      }
   }

   if ( sendToVehicle )
   {
      if ( g_pCurrentModel->is_spectator )
      {
         memcpy(&(g_pCurrentModel->osd_params), &params, sizeof(osd_parameters_t));
         saveControllerModel(g_pCurrentModel);
         valuesToUI();
         return;
      }
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_OSD_PARAMS, 0, (u8*)&params, sizeof(osd_parameters_t)) )
         valuesToUI();
      return;
   }
}
