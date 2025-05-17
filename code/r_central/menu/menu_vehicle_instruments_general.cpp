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
#include "menu_vehicle_instruments_general.h"
#include "menu_vehicle_osd_plugins.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "../osd/osd_common.h"

MenuVehicleInstrumentsGeneral::MenuVehicleInstrumentsGeneral(void)
:Menu(MENU_ID_VEHICLE_INSTRUMENTS_GENERAL, L("Intruments/Gauges General Settings"), NULL)
{
   m_Width = 0.32;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.30;

   for( int i=0; i<20; i++ )
      m_pItemsSelect[i] = NULL;

   m_pItemsSelect[10] = new MenuItemSelect(L("Display Units"), L("Changes how the OSD displays data: in metric system or imperial system."));  
   m_pItemsSelect[10]->addSelection(L("Metric (km/h)"));
   m_pItemsSelect[10]->addSelection(L("Metric (m/s)"));
   m_pItemsSelect[10]->addSelection(L("Imperial (mi/h)"));
   m_pItemsSelect[10]->addSelection(L("Imperial (ft/s)"));
   m_pItemsSelect[10]->setIsEditable();
   m_IndexUnits = addMenuItem(m_pItemsSelect[10]);

   m_pItemsSelect[12] = new MenuItemSelect(L("Display Units (Heights)"), L("Changes how the OSD displays heights: in metric system or imperial system."));  
   //m_pItemsSelect[12]->addSelection("Metric (km)");
   m_pItemsSelect[12]->addSelection(L("Metric (m)"));
   //m_pItemsSelect[12]->addSelection("Imperial (mi)");
   m_pItemsSelect[12]->addSelection(L("Imperial (ft)"));
   m_pItemsSelect[12]->setIsEditable();
   m_IndexUnitsHeight = addMenuItem(m_pItemsSelect[12]);

   m_pItemsSelect[13] = new MenuItemSelect(L("Temperature Display"), L("Changes how the OSD displays temperatures: in F or C."));
   m_pItemsSelect[13]->addSelection("C");
   m_pItemsSelect[13]->addSelection("F");
   m_pItemsSelect[13]->setIsEditable();
   m_IndexTemp = addMenuItem(m_pItemsSelect[13]);


   m_pItemsSelect[1] = new MenuItemSelect(L("Instruments Size"), L("Increase/decrease instruments sizes."));  
   m_pItemsSelect[1]->addSelection("Smallest");
   m_pItemsSelect[1]->addSelection("Smaller");
   m_pItemsSelect[1]->addSelection("Small");
   m_pItemsSelect[1]->addSelection("Normal");
   m_pItemsSelect[1]->addSelection("Large");
   m_pItemsSelect[1]->addSelection("Larger");
   m_pItemsSelect[1]->addSelection("Largest");
   m_pItemsSelect[1]->addSelection("XLarge");
   m_IndexAHISize = addMenuItem(m_pItemsSelect[1]);
 
   m_pItemsSelect[2] = new MenuItemSelect(L("Instruments Line Thickness"), L("Sets the thickness of instruments elements"));
   m_pItemsSelect[2]->addSelection("Smallest");
   m_pItemsSelect[2]->addSelection("Small");
   m_pItemsSelect[2]->addSelection("Normal");
   m_pItemsSelect[2]->addSelection("Big");
   m_pItemsSelect[2]->addSelection("Biggest");
   m_IndexAHIStrokeSize = addMenuItem(m_pItemsSelect[2]);
   
   m_pItemsSelect[9] = new MenuItemSelect(L("Flash OSD on Telemetry Lost"), L("Flashes the OSD whenever a telemetry data packet is lost."));
   m_pItemsSelect[9]->addSelection(L("No"));
   m_pItemsSelect[9]->addSelection(L("Yes"));
   m_pItemsSelect[9]->setIsEditable();
   m_iIndexFlashOSDOnTelemLost = addMenuItem(m_pItemsSelect[9]);

   m_pItemsSelect[3] = new MenuItemSelect(L("Altitude type"), L("Shows the vehicle altitude relative to sea level or relative to the starting position."));  
   m_pItemsSelect[3]->addSelection(L("Absolute"));
   m_pItemsSelect[3]->addSelection(L("Relative"));
   m_pItemsSelect[3]->setIsEditable();
   m_IndexAltitudeType = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSelect[6] = new MenuItemSelect(L("Vertical Speed"), L("Shows the vertical speed reported by the flight controller or the one computed locally on the controller based on altitude variations."));
   m_pItemsSelect[6]->addSelection(L("Reported by FC"));
   m_pItemsSelect[6]->addSelection(L("Computed"));
   m_pItemsSelect[6]->setIsEditable();
   m_IndexLocalVertSpeed = addMenuItem(m_pItemsSelect[6]);

   m_pItemsSelect[4] = new MenuItemSelect(L("Revert Pitch"), L("Reverses the rotation direction of the pitch axis. Impacts only on the AHI gauge."));  
   m_pItemsSelect[4]->addSelection(L("No"));
   m_pItemsSelect[4]->addSelection(L("Yes"));
   m_pItemsSelect[4]->setUseMultiViewLayout();
   m_IndexRevertPitch = addMenuItem(m_pItemsSelect[4]);

   m_pItemsSelect[5] = new MenuItemSelect(L("Revert Roll"), L("Reverses the rotation direction of the roll axis. Impacts only on the AHI gauge."));  
   m_pItemsSelect[5]->addSelection(L("No"));
   m_pItemsSelect[5]->addSelection(L("Yes"));
   m_pItemsSelect[5]->setUseMultiViewLayout();
   m_IndexRevertRoll = addMenuItem(m_pItemsSelect[5]);

   m_pItemsSelect[7] = new MenuItemSelect(L("Main Speed"), L("Shows the air speed or ground speed as the main speed indicator."));  
   m_pItemsSelect[7]->addSelection(L("Ground"));
   m_pItemsSelect[7]->addSelection(L("Air"));
   m_pItemsSelect[7]->setUseMultiViewLayout();
   m_IndexAHIShowAirSpeed = addMenuItem(m_pItemsSelect[7]);

   m_pItemsSelect[8] = new MenuItemSelect(L("Battery Cells"), L("How many cells (in series) does the vehicle battery has."));  
   m_pItemsSelect[8]->addSelection(L("Auto Detect"));
   m_pItemsSelect[8]->addSelection("1 cell");
   m_pItemsSelect[8]->addSelection("2 cells");
   m_pItemsSelect[8]->addSelection("3 cells");
   m_pItemsSelect[8]->addSelection("4 cells");
   m_pItemsSelect[8]->addSelection("5 cells");
   m_pItemsSelect[8]->addSelection("6 cells");
   m_pItemsSelect[8]->addSelection("7 cells");
   m_pItemsSelect[8]->addSelection("8 cells");
   m_pItemsSelect[8]->addSelection("9 cells");
   m_pItemsSelect[8]->addSelection("10 cells");
   m_pItemsSelect[8]->addSelection("11 cells");
   m_pItemsSelect[8]->addSelection("12 cells");
   m_pItemsSelect[8]->setIsEditable();
   m_IndexBatteryCells = addMenuItem(m_pItemsSelect[8]);

   m_pItemsSelect[11] = new MenuItemSelect(L("Show Flight End Stats"), L("When a flight ends, show a summary stats window about the flight."));
   m_pItemsSelect[11]->addSelection(L("No"));
   m_pItemsSelect[11]->addSelection(L("Yes"));
   m_pItemsSelect[11]->setUseMultiViewLayout();
   m_IndexFlightEndStats = addMenuItem(m_pItemsSelect[11]);
}

MenuVehicleInstrumentsGeneral::~MenuVehicleInstrumentsGeneral()
{
}

void MenuVehicleInstrumentsGeneral::valuesToUI()
{
   Preferences* p = get_Preferences();
   m_nOSDIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;

   if ( p->iUnits == prefUnitsMetric )
      m_pItemsSelect[10]->setSelection(0);
   if ( p->iUnits == prefUnitsMeters )
      m_pItemsSelect[10]->setSelection(1);
   if ( p->iUnits == prefUnitsImperial )
      m_pItemsSelect[10]->setSelection(2);
   if ( p->iUnits == prefUnitsFeets )
      m_pItemsSelect[10]->setSelection(3);

   if ( p->iUnitsHeight == prefUnitsMetric )
      m_pItemsSelect[12]->setSelection(0);
   if ( p->iUnitsHeight == prefUnitsMeters )
      m_pItemsSelect[12]->setSelection(0);
   if ( p->iUnitsHeight == prefUnitsImperial )
      m_pItemsSelect[12]->setSelection(1);
   if ( p->iUnitsHeight == prefUnitsFeets )
      m_pItemsSelect[12]->setSelection(1);


   //log_dword("start: osd flags", g_pCurrentModel->osd_params.osd_flags[m_nOSDIndex]);
   //log_dword("start: instruments flags", g_pCurrentModel->osd_params.instruments_flags[m_nOSDIndex]);
   //log_line("current layout: %d, show instr: %d", (g_pCurrentModel->osd_params.osd_flags[m_nOSDIndex] & OSD_FLAG_AHI_STYLE_MASK)>>3, g_pCurrentModel->osd_params.instruments_flags[m_nOSDIndex] & INSTRUMENTS_FLAG_SHOW_INSTRUMENTS);

   m_pItemsSelect[1]->setSelection(p->iScaleAHI+3);
   m_pItemsSelect[2]->setSelection(p->iAHIStrokeSize+2);
   m_pItemsSelect[3]->setSelection((g_pCurrentModel->osd_params.osd_flags2[m_nOSDIndex] & OSD_FLAG2_RELATIVE_ALTITUDE)?1:0);
   m_pItemsSelect[6]->setSelection((g_pCurrentModel->osd_params.osd_flags2[m_nOSDIndex] & OSD_FLAG2_SHOW_LOCAL_VERTICAL_SPEED)?1:0);
   m_pItemsSelect[9]->setSelection((g_pCurrentModel->osd_params.osd_flags2[m_nOSDIndex] & OSD_FLAG2_FLASH_OSD_ON_TELEMETRY_DATA_LOST)?1:0);

   m_pItemsSelect[4]->setSelection(((g_pCurrentModel->osd_params.osd_flags[m_nOSDIndex]) & OSD_FLAG_REVERT_PITCH)?1:0);
   m_pItemsSelect[5]->setSelection(((g_pCurrentModel->osd_params.osd_flags[m_nOSDIndex]) & OSD_FLAG_REVERT_ROLL)?1:0);

   m_pItemsSelect[7]->setSelection(((g_pCurrentModel->osd_params.osd_flags[m_nOSDIndex]) & OSD_FLAG_AIR_SPEED_MAIN)?1:0);

   m_pItemsSelect[8]->setSelection(g_pCurrentModel->osd_params.battery_cell_count);

   m_pItemsSelect[11]->setSelection((g_pCurrentModel->osd_params.uFlags & OSD_BIT_FLAGS_SHOW_FLIGHT_END_STATS)?1:0);
   m_pItemsSelect[13]->setSelection((g_pCurrentModel->osd_params.uFlags & OSD_BIT_FLAGS_SHOW_TEMPS_F)?1:0);
}


void MenuVehicleInstrumentsGeneral::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yTop);
}

void MenuVehicleInstrumentsGeneral::onItemValueChanged(int itemIndex)
{
   Menu::onItemValueChanged(itemIndex);
}

void MenuVehicleInstrumentsGeneral::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   m_nOSDIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;

   Preferences* p = get_Preferences();
   osd_parameters_t params;
   memcpy(&params, &(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));
   bool sendToVehicle = false;


   if ( m_IndexUnits == m_SelectedIndex )
   {
      int nSel = m_pItemsSelect[10]->getSelectedIndex();
      if ( 0 == nSel )
         p->iUnits = prefUnitsMetric;
      if ( 1 == nSel )
         p->iUnits = prefUnitsMeters;
      if ( 2 == nSel )
         p->iUnits = prefUnitsImperial;
      if ( 3 == nSel )
         p->iUnits = prefUnitsFeets;

      save_Preferences();
      valuesToUI();
      return;
   }

   if ( m_IndexUnitsHeight == m_SelectedIndex )
   {
      int nSel = m_pItemsSelect[12]->getSelectedIndex();
      if ( 0 == nSel )
         p->iUnitsHeight = prefUnitsMeters;
      if ( 1 == nSel )
         p->iUnitsHeight = prefUnitsFeets;
      save_Preferences();
      valuesToUI();
      return;
   }

   if ( m_IndexAHISize == m_SelectedIndex )
   {
      p->iScaleAHI = m_pItemsSelect[1]->getSelectedIndex()-3;
      save_Preferences();
      valuesToUI();
      osd_apply_preferences();
   }

   if ( m_IndexAHIStrokeSize == m_SelectedIndex )
   {
      p->iAHIStrokeSize = m_pItemsSelect[2]->getSelectedIndex()-2;
      save_Preferences();
      valuesToUI();
      osd_apply_preferences();
   }

   if ( m_iIndexFlashOSDOnTelemLost == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[9]->getSelectedIndex() )
         params.osd_flags2[m_nOSDIndex] &= ~OSD_FLAG2_FLASH_OSD_ON_TELEMETRY_DATA_LOST;
      else
         params.osd_flags2[m_nOSDIndex] |= OSD_FLAG2_FLASH_OSD_ON_TELEMETRY_DATA_LOST;
      sendToVehicle = true;
   }

   if ( m_IndexAltitudeType == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[3]->getSelectedIndex() )
      {
         params.altitude_relative = false;
         for( int i=0; i<MODEL_MAX_OSD_SCREENS; i++ )
            params.osd_flags2[i] &= ~OSD_FLAG2_RELATIVE_ALTITUDE;
      }
      else
      {
         params.altitude_relative = true;
         for( int i=0; i<MODEL_MAX_OSD_SCREENS; i++ )
            params.osd_flags2[i] |= OSD_FLAG2_RELATIVE_ALTITUDE;
      }
      sendToVehicle = true;
   }
   
   if ( m_IndexLocalVertSpeed == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[6]->getSelectedIndex() )
         for( int i=0; i<MODEL_MAX_OSD_SCREENS; i++ )
            params.osd_flags2[i] &= ~OSD_FLAG2_SHOW_LOCAL_VERTICAL_SPEED;
      else
         for( int i=0; i<MODEL_MAX_OSD_SCREENS; i++ )
            params.osd_flags2[i] |= OSD_FLAG2_SHOW_LOCAL_VERTICAL_SPEED;
      sendToVehicle = true;
   }

   if ( m_IndexRevertPitch == m_SelectedIndex )
   {
      u32 idx = m_pItemsSelect[4]->getSelectedIndex();
      for( int i=0; i<MODEL_MAX_OSD_SCREENS; i++ )
         params.osd_flags[i] &= ~OSD_FLAG_REVERT_PITCH;
      if ( idx > 0 )
         for( int i=0; i<MODEL_MAX_OSD_SCREENS; i++ )
            params.osd_flags[i] |= OSD_FLAG_REVERT_PITCH;
      sendToVehicle = true;
   }

   if ( m_IndexRevertRoll == m_SelectedIndex )
   {
      u32 idx = m_pItemsSelect[5]->getSelectedIndex();
      for( int i=0; i<MODEL_MAX_OSD_SCREENS; i++ )
         params.osd_flags[i] &= ~OSD_FLAG_REVERT_ROLL;
      if ( idx > 0 )
         for( int i=0; i<MODEL_MAX_OSD_SCREENS; i++ )
            params.osd_flags[i] |= OSD_FLAG_REVERT_ROLL;
      sendToVehicle = true;
   } 

   if ( m_IndexAHIShowAirSpeed == m_SelectedIndex )
   {
      u32 idx = m_pItemsSelect[7]->getSelectedIndex();
      for( int i=0; i<MODEL_MAX_OSD_SCREENS; i++ )
         params.osd_flags[i] &= ~OSD_FLAG_AIR_SPEED_MAIN;
      if ( idx > 0 )
         for( int i=0; i<MODEL_MAX_OSD_SCREENS; i++ )
            params.osd_flags[i] |= OSD_FLAG_AIR_SPEED_MAIN;
      sendToVehicle = true;
   }

   if ( m_IndexBatteryCells == m_SelectedIndex )
   {
      params.battery_cell_count = m_pItemsSelect[8]->getSelectedIndex();
      sendToVehicle = true;
   }

   if ( m_IndexTemp == m_SelectedIndex )
   {
      if ( 1 == m_pItemsSelect[13]->getSelectedIndex() )
         params.uFlags |= OSD_BIT_FLAGS_SHOW_TEMPS_F;
      else
         params.uFlags &= ~OSD_BIT_FLAGS_SHOW_TEMPS_F;
      sendToVehicle = true;
   }

   if ( m_IndexFlightEndStats == m_SelectedIndex )
   {
      if ( 1 == m_pItemsSelect[11]->getSelectedIndex() )
         params.uFlags |= OSD_BIT_FLAGS_SHOW_FLIGHT_END_STATS;
      else
         params.uFlags &= ~OSD_BIT_FLAGS_SHOW_FLIGHT_END_STATS;
      sendToVehicle = true;
   }

   if ( g_pCurrentModel->is_spectator )
   {
      memcpy(&(g_pCurrentModel->osd_params), &params, sizeof(osd_parameters_t));
      saveControllerModel(g_pCurrentModel);
   }
   else if ( sendToVehicle )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_OSD_PARAMS, 0, (u8*)&params, sizeof(osd_parameters_t)) )
         valuesToUI();
      return;
   }
}