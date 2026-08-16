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
#include "menu_vehicle_osd_elements.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "menu_item_section.h"
#include "../osd/osd_common.h"

MenuVehicleOSDElements::MenuVehicleOSDElements(void)
:Menu(MENU_ID_VEHICLE_OSD_ELEMENTS, L("OSD Elements"), NULL)
{
   m_Width = 0.56;
   //m_xPos = menu_get_XStartPos(m_Width);
   m_xPos = 0.15;
   m_yPos = 0.12;

   char szBuff[256];
   sprintf(szBuff, "OSD Elements (%s)", str_get_osd_screen_name(g_pCurrentModel->osd_params.iCurrentOSDScreen));
   setTitle(szBuff);
   
   setColumnsCount(2);
   enableColumnSelection(false);
   bool bUseMultiSelection = true;

   for( int i=0; i<50; i++ )
      m_pItemsSelect[i] = NULL;

   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP )
      addTopLine("To change the layout of the MSP OSD elements use your flight controller configurator software.");

   m_pItemsSelect[32] = new MenuItemSelect(L("OSD Elements Arrangement"), L("How the OSD elements are arranged on the screen: fluid around margins or only on the left/right sides of the screen."));  
   m_pItemsSelect[32]->addSelection(L("Fluid"));
   m_pItemsSelect[32]->addSelection(L("Left/Right"));
   //if ( bUseMultiSelection )
   //   m_pItemsSelect[32]->setUseMultiViewLayout();
   m_pItemsSelect[32]->setIsEditable();
   m_IndexOSDOrientation = addMenuItem(m_pItemsSelect[32]);

   m_pItemsSelect[25] = new MenuItemSelect(L("Background bars"), L("Shows background bars behind the OSD to increase readability"));  
   m_pItemsSelect[25]->addSelection(L("No"));
   m_pItemsSelect[25]->addSelection(L("Yes"));
   if ( bUseMultiSelection )
      m_pItemsSelect[25]->setUseMultiViewLayout();
   m_IndexShowBg = addMenuItem(m_pItemsSelect[25]);

   m_IndexVoltage = -1;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   {
      m_pItemsSelect[1] = new MenuItemSelect(L("Voltage/Current"), L("Shows the voltage/current on the OSD."));  
      m_pItemsSelect[1]->addSelection(L("No"));
      m_pItemsSelect[1]->addSelection(L("Yes"));
      if ( bUseMultiSelection )
         m_pItemsSelect[1]->setUseMultiViewLayout();
      m_IndexVoltage = addMenuItem(m_pItemsSelect[1]);
   }

   m_IndexVoltagePerCell = -1;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   {
      m_pItemsSelect[2] = new MenuItemSelect(L("Voltage per cell"), L("Shows also the voltage per cell of the battery."));  
      m_pItemsSelect[2]->addSelection(L("No"));
      m_pItemsSelect[2]->addSelection(L("Yes"));
      if ( bUseMultiSelection )
         m_pItemsSelect[2]->setUseMultiViewLayout();
      m_IndexVoltagePerCell = addMenuItem(m_pItemsSelect[2]);
   }

   m_pItemsSelect[31] = new MenuItemSelect(L("Controller Voltage & Current"), L("Shows controller voltage and current on the OSD."));  
   m_pItemsSelect[31]->addSelection(L("No"));
   m_pItemsSelect[31]->addSelection(L("Yes"));
   if ( bUseMultiSelection )
      m_pItemsSelect[31]->setUseMultiViewLayout();
   m_IndexControllerVoltage = addMenuItem(m_pItemsSelect[31]);

   m_IndexDistance = -1;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   {
      m_pItemsSelect[3] = new MenuItemSelect(L("Distance"), L("Shows the distance from home on the OSD."));
      m_pItemsSelect[3]->addSelection(L("No"));
      m_pItemsSelect[3]->addSelection(L("Yes"));
      if ( bUseMultiSelection )
         m_pItemsSelect[3]->setUseMultiViewLayout();
      m_IndexDistance = addMenuItem(m_pItemsSelect[3]);
   }

   m_IndexTotalDistance = -1;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   {
      m_pItemsSelect[10] = new MenuItemSelect(L("Total distance"), L("Shows on the OSD the total distance traveled, not just current distance from home."));  
      m_pItemsSelect[10]->addSelection(L("No"));
      m_pItemsSelect[10]->addSelection(L("Yes"));
      if ( bUseMultiSelection )
         m_pItemsSelect[10]->setUseMultiViewLayout();
      m_IndexTotalDistance = addMenuItem(m_pItemsSelect[10]);
   }

   m_IndexSpeed = -1;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   {
      m_pItemsSelect[33] = new MenuItemSelect(L("Speed"), L("Shows the ground and/or air speed on the OSD."));  
      m_pItemsSelect[33]->addSelection(L("None"));
      m_pItemsSelect[33]->addSelection(L("Ground Speed"));
      m_pItemsSelect[33]->addSelection(L("Air Speed"));
      m_pItemsSelect[33]->addSelection(L("Both"));
      m_pItemsSelect[33]->setIsEditable();
      m_IndexSpeed = addMenuItem(m_pItemsSelect[33]);
   }

   m_IndexAltitude = -1;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   {
      m_pItemsSelect[4] = new MenuItemSelect(L("Altitude"), L("Shows the altitude on the OSD."));  
      m_pItemsSelect[4]->addSelection(L("No"));
      m_pItemsSelect[4]->addSelection(L("Yes"));
      if ( bUseMultiSelection )
         m_pItemsSelect[4]->setUseMultiViewLayout();
      m_IndexAltitude = addMenuItem(m_pItemsSelect[4]);
   }

   m_IndexHome = -1;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   {
      m_pItemsSelect[6] = new MenuItemSelect(L("Home"), L("Shows the home heading on the OSD."));
      m_pItemsSelect[6]->addSelection(L("No"));
      m_pItemsSelect[6]->addSelection(L("Yes"));
      if ( bUseMultiSelection )
         m_pItemsSelect[6]->setUseMultiViewLayout();
      m_IndexHome = addMenuItem(m_pItemsSelect[6]);
   }

   m_IndexHomeInvert = -1;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   {
      m_pItemsSelect[7] = new MenuItemSelect(L("Mirror home arrow"), L("Flips the rotation of the home arrow."));  
      m_pItemsSelect[7]->addSelection(L("No"));
      m_pItemsSelect[7]->addSelection(L("Yes"));
      if ( bUseMultiSelection )
         m_pItemsSelect[7]->setUseMultiViewLayout();
      m_IndexHomeInvert = addMenuItem(m_pItemsSelect[7]);
   }

   m_IndexHomeRotate = -1;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   {
      m_pItemsSelect[8] = new MenuItemSelect(L("Home arrow rotation"), L("Changes the orientation of the home arrow by a set amount."));
      m_pItemsSelect[8]->addSelection("0°");
      m_pItemsSelect[8]->addSelection("90°");
      m_pItemsSelect[8]->addSelection("180°");
      m_pItemsSelect[8]->addSelection("270°");
      m_pItemsSelect[8]->setIsEditable();
      m_IndexHomeRotate = addMenuItem(m_pItemsSelect[8]);
   }

   m_IndexShowGPSInfo = -1;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   {
      m_pItemsSelect[26] = new MenuItemSelect(L("GPS Satellites"), L("Shows the GPS satellites information."));  
      m_pItemsSelect[26]->addSelection(L("No"));
      m_pItemsSelect[26]->addSelection(L("Yes"));
      if ( bUseMultiSelection )
         m_pItemsSelect[26]->setUseMultiViewLayout();
      m_IndexShowGPSInfo = addMenuItem(m_pItemsSelect[26]);
   }

   m_IndexShowGPSPos = -1;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   {   
      m_pItemsSelect[9] = new MenuItemSelect(L("GPS position"), L("Shows the current GPS position (long,lat)."));  
      m_pItemsSelect[9]->addSelection(L("No"));
      m_pItemsSelect[9]->addSelection(L("Yes"));
      m_pItemsSelect[9]->addSelection(L("Scrambled"));
      m_pItemsSelect[9]->setIsEditable();
      m_IndexShowGPSPos = addMenuItem(m_pItemsSelect[9]);
   }

   m_pItemsSelect[11] = new MenuItemSelect(L("Flight mode"), L("Shows flight mode on the OSD."));  
   m_pItemsSelect[11]->addSelection(L("No"));
   m_pItemsSelect[11]->addSelection(L("Yes"));
   m_pItemsSelect[11]->addSelection(L("Only when changing"));
   m_pItemsSelect[11]->addSelection(L("Both"));
   m_pItemsSelect[11]->setIsEditable();
   //if ( bUseMultiSelection )
   //   m_pItemsSelect[11]->setUseMultiViewLayout();
   m_IndexMode = addMenuItem(m_pItemsSelect[11]);

   m_pItemsSelect[12] = new MenuItemSelect(L("Time"), L("Shows the arm/flight time on the OSD."));  
   m_pItemsSelect[12]->addSelection(L("No"));
   m_pItemsSelect[12]->addSelection(L("Yes (Top)"));
   m_pItemsSelect[12]->addSelection(L("Yes (Bottom)"));
   //if ( bUseMultiSelection )
   //   m_pItemsSelect[12]->setUseMultiViewLayout();
   m_pItemsSelect[12]->setIsEditable();
   m_IndexTime = addMenuItem(m_pItemsSelect[12]);

   m_IndexWind = -1;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   {
      m_pItemsSelect[28] = new MenuItemSelect(L("Wind"), L("Shows the wind direction and speed on the OSD."));  
      m_pItemsSelect[28]->addSelection(L("No"));
      m_pItemsSelect[28]->addSelection(L("Yes"));
      if ( bUseMultiSelection )
         m_pItemsSelect[28]->setUseMultiViewLayout();
      m_IndexWind = addMenuItem(m_pItemsSelect[28]);
      if ( (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) != MODEL_TYPE_AIRPLANE )
      if ( (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) != MODEL_TYPE_DRONE )
         m_pItemsSelect[28]->setEnabled(false);
   }

   m_IndexTemperature = -1;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   {
      m_pItemsSelect[29] = new MenuItemSelect(L("Temperature"), L("Shows the temperature (if reported by the flight controller) on the OSD."));  
      m_pItemsSelect[29]->addSelection(L("No"));
      m_pItemsSelect[29]->addSelection(L("Yes"));
      if ( bUseMultiSelection )
         m_pItemsSelect[29]->setUseMultiViewLayout();
      m_IndexTemperature = addMenuItem(m_pItemsSelect[29]);
   }

   m_IndexThrottle = -1;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   {
      m_pItemsSelect[13] = new MenuItemSelect(L("Throttle"), L("Shows the throttle on the OSD."));  
      m_pItemsSelect[13]->addSelection(L("No"));
      m_pItemsSelect[13]->addSelection(L("Yes"));
      if ( bUseMultiSelection )
         m_pItemsSelect[13]->setUseMultiViewLayout();
      m_IndexThrottle = addMenuItem(m_pItemsSelect[13]);
   }

   m_IndexPitch = -1;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   {
      m_pItemsSelect[14] = new MenuItemSelect(L("Pitch"), L("Shows vehicle pitch on the OSD."));  
      m_pItemsSelect[14]->addSelection(L("No"));
      m_pItemsSelect[14]->addSelection(L("Yes"));
      if ( bUseMultiSelection )
         m_pItemsSelect[14]->setUseMultiViewLayout();
      m_IndexPitch = addMenuItem(m_pItemsSelect[14]);
   }

   m_pItemsSelect[15] = new MenuItemSelect(L("Vehicle CPU Info"), L("Shows vehicle CPU info on the OSD."));  
   m_pItemsSelect[15]->addSelection(L("No"));
   m_pItemsSelect[15]->addSelection(L("Yes"));
   if ( bUseMultiSelection )
      m_pItemsSelect[15]->setUseMultiViewLayout();
   m_IndexCPU = addMenuItem(m_pItemsSelect[15]);
   
   m_pItemsSelect[30] = new MenuItemSelect(L("Controller CPU Info"), L("Shows controller CPU info on the OSD."));  
   m_pItemsSelect[30]->addSelection(L("No"));
   m_pItemsSelect[30]->addSelection(L("Yes"));
   if ( bUseMultiSelection )
      m_pItemsSelect[30]->setUseMultiViewLayout();
   m_IndexControllerCPU = addMenuItem(m_pItemsSelect[30]);


   m_pItemsSelect[16] = new MenuItemSelect(L("Video Mode"), L("Shows current video mode on the OSD."));
   m_pItemsSelect[16]->addSelection(L("No"));
   m_pItemsSelect[16]->addSelection(L("Yes"));
   if ( bUseMultiSelection )
      m_pItemsSelect[16]->setUseMultiViewLayout();
   m_IndexVideo = addMenuItem(m_pItemsSelect[16]);

   m_pItemsSelect[17] = new MenuItemSelect(L("Video Mbps"), L("Shows the video link total Mbps and video Mbps generated by the camera."));
   m_pItemsSelect[17]->addSelection(L("No"));
   m_pItemsSelect[17]->addSelection(L("Yes"));
   if ( bUseMultiSelection )
      m_pItemsSelect[17]->setUseMultiViewLayout();
   m_IndexShowDetailedBPS = addMenuItem(m_pItemsSelect[17]);

   m_pItemsSelect[18] = new MenuItemSelect(L("Video Info (Extended)"), L("Shows video resolution and video FPS on the OSD."));
   m_pItemsSelect[18]->addSelection(L("No"));
   m_pItemsSelect[18]->addSelection(L("Yes"));
   if ( bUseMultiSelection )
      m_pItemsSelect[18]->setUseMultiViewLayout();
   m_IndexVideoExtended = addMenuItem(m_pItemsSelect[18]);

   m_pItemsSelect[19] = new MenuItemSelect(L("Radio Links"), L("Shows info about the radio links (Downlinks = controller Rx, from vehicle to controller; Uplinks = vehicle Rx, from controller to vehicle) in the OSD."));
   m_pItemsSelect[19]->addSelection(L("No"));
   m_pItemsSelect[19]->addSelection(L("Downlinks"));
   m_pItemsSelect[19]->addSelection(L("Uplinks"));
   m_pItemsSelect[19]->addSelection(L("Both"));
   m_pItemsSelect[19]->setIsEditable();
   m_IndexRadioLinks = addMenuItem(m_pItemsSelect[19]);

   m_pItemsSelect[20] = new MenuItemSelect(L("   Radio Links Info"), L("What info to show about the radio links (uplinks and downlinks) in the OSD."));
   m_pItemsSelect[20]->addSelection(L("Quality Bars"));
   m_pItemsSelect[20]->addSelection(L("Quality Numbers"));
   m_pItemsSelect[20]->addSelection(L("Both"));
   m_pItemsSelect[20]->setIsEditable();
   m_IndexRadioLinkBars = addMenuItem(m_pItemsSelect[20]);

   m_pItemsSelect[35] = new MenuItemSelect(L("   Radio Links Numbers"), L("What numerical info to show about the radio links (uplinks and downlinks) in the OSD."));
   m_pItemsSelect[35]->addSelection(L("dBm & Quality"));
   m_pItemsSelect[35]->addSelection(L("dBm & SNR"));
   m_pItemsSelect[35]->addSelection(L("SNR & Quality"));
   m_pItemsSelect[35]->setIsEditable();
   m_IndexRadioLinksNumbers = addMenuItem(m_pItemsSelect[35]);

   m_pItemsSelect[21] = new MenuItemSelect(L("   Radio Interfaces Info"), L("Shows information in OSD radio links about the radio interfaces associated with each radio link."));
   m_pItemsSelect[21]->addSelection(L("No"));
   m_pItemsSelect[21]->addSelection(L("Basic"));
   m_pItemsSelect[21]->addSelection(L("Full"));
   m_pItemsSelect[21]->setIsEditable();
   m_IndexRadioInterfaces = addMenuItem(m_pItemsSelect[21]);


   m_pItemsSelect[36] = new MenuItemSelect(L("Radio Tx Power"), L("Shows radio tx power in the current OSD screen."));
   m_pItemsSelect[36]->addSelection(L("No"));
   m_pItemsSelect[36]->addSelection(L("Yes"));
   if ( bUseMultiSelection )
      m_pItemsSelect[36]->setUseMultiViewLayout();
   m_IndexShowTxPower = addMenuItem(m_pItemsSelect[36]);


   m_IndexRCRSSI = -1;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   {
      m_pItemsSelect[34] = new MenuItemSelect(L("RC RSSI"), L("Shows the RC RSSI indicator on the OSD."));
      m_pItemsSelect[34]->addSelection(L("No"));
      m_pItemsSelect[34]->addSelection(L("Yes"));
      if ( bUseMultiSelection )
         m_pItemsSelect[34]->setUseMultiViewLayout();
      m_IndexRCRSSI = addMenuItem(m_pItemsSelect[34]);
   }

   m_pItemsSelect[22] = new MenuItemSelect(L("Signal strength bars"), L("Shows signal strength bars on the OSD."));  
   m_pItemsSelect[22]->addSelection(L("No"));
   m_pItemsSelect[22]->addSelection(L("Yes"));
   if ( bUseMultiSelection )
      m_pItemsSelect[22]->setUseMultiViewLayout();
   m_IndexSignalBars = addMenuItem(m_pItemsSelect[22]);

   m_pItemsSelect[23] = new MenuItemSelect(L("Signal bars position"), L("Sets the position on the screen of the signal strength bars."));
   m_pItemsSelect[23]->addSelection(L("Top"));
   m_pItemsSelect[23]->addSelection(L("Bottom"));
   m_pItemsSelect[23]->addSelection(L("Left"));
   m_pItemsSelect[23]->addSelection(L("Right"));
   m_pItemsSelect[23]->setIsEditable();
   m_IndexSignalBarsPosition = addMenuItem(m_pItemsSelect[23]);

   m_pItemsSelect[37] = new MenuItemSelect(L("Poor link indication"), L("How to indicate on screen that the radio link quality got poor: a colored outline that fades from yellow to red as the link degrades, or dark bars at the edges of the screen."));
   m_pItemsSelect[37]->addSelection(L("None"));
   m_pItemsSelect[37]->addSelection(L("Colour outline"));
   m_pItemsSelect[37]->addSelection(L("Black bars"));
   m_pItemsSelect[37]->setIsEditable();
   m_IndexLinkWarningOutline = addMenuItem(m_pItemsSelect[37]);

   m_IndexHIDOSD = -1;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP )
   {
      m_pItemsSelect[24] = new MenuItemSelect(L("RC Sticks and channels"), L("Shows the RC input sticks and RC channels on the OSD as they are actually reported back by the vehicle."));
      m_pItemsSelect[24]->addSelection(L("No"));
      m_pItemsSelect[24]->addSelection(L("Yes"));
      if ( bUseMultiSelection )
         m_pItemsSelect[24]->setUseMultiViewLayout();
      m_IndexHIDOSD = addMenuItem(m_pItemsSelect[24]);

      #ifndef FEATURE_ENABLE_RC
      m_pMenuItems[m_IndexHIDOSD]->setEnabled(false);
      #endif

      if ( g_pCurrentModel->is_spectator )
         m_pMenuItems[m_IndexHIDOSD]->setEnabled(false);
   }

   m_pItemsSelect[27] = new MenuItemSelect(L("Grid Overlay"), L("Shows a grid over the screen."));
   m_pItemsSelect[27]->addSelection(L("None"));
   m_pItemsSelect[27]->addSelection(L("Crosshair"));
   m_pItemsSelect[27]->addSelection(L("Diagonal Lines"));
   m_pItemsSelect[27]->addSelection(L("Thirds Lines"));
   m_pItemsSelect[27]->addSelection(L("Both"));
   m_pItemsSelect[27]->addSelection(L("Thirds Lines Small"));
   m_pItemsSelect[27]->setIsEditable();
   m_IndexGrid = addMenuItem(m_pItemsSelect[27]);
}

MenuVehicleOSDElements::~MenuVehicleOSDElements()
{
}

void MenuVehicleOSDElements::valuesToUI()
{
   Preferences* p = get_Preferences();
   ControllerSettings* pCS = get_ControllerSettings();
   
   int iScreenIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;

   m_pItemsSelect[25]->setSelection((g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_BGBARS)?1:0);

   if ( -1 != m_IndexVoltage )
      m_pItemsSelect[1]->setSelection((g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_BATTERY)?1:0);
   
   if ( -1 != m_IndexVoltagePerCell )
   {
      m_pItemsSelect[2]->setSelection((g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_BATTERY_CELLS)?1:0);
      if ( g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_BATTERY )
         m_pItemsSelect[2]->setEnabled(true);
      else
         m_pItemsSelect[2]->setEnabled(false);
   }

   if ( -1 != m_IndexDistance )
      m_pItemsSelect[3]->setSelection((g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_DISTANCE)?1:0);
   if ( -1 != m_IndexTotalDistance )
      m_pItemsSelect[10]->setSelection((g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_TOTAL_DISTANCE)?1:0);
   if ( -1 != m_IndexAltitude )
      m_pItemsSelect[4]->setSelection((g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_ALTITUDE)?1:0);
   if ( -1 != m_IndexHome )
      m_pItemsSelect[6]->setSelection((g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_HOME)?1:0);
   if ( -1 != m_IndexHomeInvert )
      m_pItemsSelect[7]->setSelection(g_pCurrentModel->osd_params.invert_home_arrow);

   if ( -1 != m_IndexHomeRotate )
   {
      if ( g_pCurrentModel->osd_params.home_arrow_rotate < 90 )
         m_pItemsSelect[8]->setSelection(0);
      else if ( g_pCurrentModel->osd_params.home_arrow_rotate < 180 )
         m_pItemsSelect[8]->setSelection(1);
      else if ( g_pCurrentModel->osd_params.home_arrow_rotate < 270 )
         m_pItemsSelect[8]->setSelection(2);
      else
         m_pItemsSelect[8]->setSelection(3);
   }

   if ( g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_HOME )
   {
      if ( -1 != m_IndexHomeInvert )
         m_pItemsSelect[7]->setEnabled(true);
      if ( -1 != m_IndexHomeRotate )
         m_pItemsSelect[8]->setEnabled(true);
   }
   else
   {
      if ( -1 != m_IndexHomeInvert )
         m_pItemsSelect[7]->setEnabled(false);
      if ( -1 != m_IndexHomeRotate )
         m_pItemsSelect[8]->setEnabled(false);
   }

   if ( -1 != m_IndexShowGPSInfo )
      m_pItemsSelect[26]->setSelection((g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_GPS_INFO)?1:0);
   if ( -1 != m_IndexShowGPSPos )
   {
      m_pItemsSelect[9]->setSelection((g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_GPS_POS)?1:0);
      if ( g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_GPS_POS )
      if ( g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SCRAMBLE_GPS )
         m_pItemsSelect[9]->setSelection(2);
   }

   m_pItemsSelect[11]->setSelection(0);
   if ( (g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_FLIGHT_MODE) &&
        (g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_FLIGHT_MODE_CHANGE) )
      m_pItemsSelect[11]->setSelection(3);
   else if ( g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_FLIGHT_MODE )
      m_pItemsSelect[11]->setSelection(1);
   else if ( g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_FLIGHT_MODE_CHANGE )
      m_pItemsSelect[11]->setSelection(2);

   if ( g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_TIME )
   {
      if ( g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_TIME_LOWER )
         m_pItemsSelect[12]->setSelection(2);
      else
         m_pItemsSelect[12]->setSelection(1);
   }
   else
      m_pItemsSelect[12]->setSelection(0);
   
   if ( -1 != m_IndexThrottle )
      m_pItemsSelect[13]->setSelection((g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_THROTTLE)?1:0);
   if ( -1 != m_IndexPitch )
      m_pItemsSelect[14]->setSelection((g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_PITCH)?1:0);
   m_pItemsSelect[15]->setSelection((g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_CPU_INFO)?1:0);
   m_pItemsSelect[16]->setSelection((g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_VIDEO_MODE)?1:0);
   m_pItemsSelect[17]->setSelection((g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_VIDEO_MBPS)?1:0);
   if ( g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_VIDEO_MODE )
      m_pItemsSelect[17]->setEnabled(true);
   else
      m_pItemsSelect[17]->setEnabled(false);

   m_pItemsSelect[18]->setSelection((g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_VIDEO_MODE_EXTENDED)?1:0);

   m_pItemsSelect[19]->setSelectedIndex(0);
   if ( (g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_RADIO_LINKS) &&
        (g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS) )
      m_pItemsSelect[19]->setSelectedIndex(3);
   else if ( g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_RADIO_LINKS )
      m_pItemsSelect[19]->setSelectedIndex(1);
   else if ( g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS )
      m_pItemsSelect[19]->setSelectedIndex(2);

   
   if ( (g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_RADIO_LINK_QUALITY_BARS) &&
        (g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_RADIO_LINK_QUALITY_NUMBERS) )
      m_pItemsSelect[20]->setSelectedIndex(2);
   else if ( g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_RADIO_LINK_QUALITY_BARS )
      m_pItemsSelect[20]->setSelectedIndex(0);
   else
      m_pItemsSelect[20]->setSelectedIndex(1);

   if ( m_pItemsSelect[20]->getSelectedIndex() == 0 )
      m_pItemsSelect[35]->setEnabled(false);
   else
      m_pItemsSelect[35]->setEnabled(true);

   if ( g_pCurrentModel->osd_params.osd_flags3[iScreenIndex] & OSD_FLAG3_SHOW_RADIO_LINK_QUALITY_NUMBERS_DBM )
   {
      if ( g_pCurrentModel->osd_params.osd_flags3[iScreenIndex] & OSD_FLAG3_SHOW_RADIO_LINK_QUALITY_NUMBERS_PERCENT )
         m_pItemsSelect[35]->setSelectedIndex(0);
      else
         m_pItemsSelect[35]->setSelectedIndex(1);
   }
   else
      m_pItemsSelect[35]->setSelectedIndex(2);
   if ( (g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_RADIO_INTERFACES_INFO) &&
        (g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_RADIO_LINK_INTERFACES_EXTENDED) )
      m_pItemsSelect[21]->setSelectedIndex(2);
   else if ( g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_RADIO_INTERFACES_INFO )
      m_pItemsSelect[21]->setSelectedIndex(1);
   else
      m_pItemsSelect[21]->setSelectedIndex(0);

   
   m_pItemsSelect[36]->setSelectedIndex(0);
   if ( g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_TX_POWER )
      m_pItemsSelect[36]->setSelectedIndex(1);


   m_pItemsSelect[22]->setSelectedIndex((g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_SIGNAL_BARS)?1:0);
   m_pItemsSelect[23]->setSelectedIndex((g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SIGNAL_BARS_MASK)>>14);
   if ( g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_SIGNAL_BARS )
      m_pItemsSelect[23]->setEnabled(true);
   else
      m_pItemsSelect[23]->setEnabled(false);

   if ( g_pCurrentModel->osd_params.osd_flags3[iScreenIndex] & OSD_FLAG3_SHOW_LINK_WARNING_BLACK_BARS )
      m_pItemsSelect[37]->setSelectedIndex(2);
   else if ( g_pCurrentModel->osd_params.osd_flags3[iScreenIndex] & OSD_FLAG3_SHOW_LINK_WARNING_OUTLINE )
      m_pItemsSelect[37]->setSelectedIndex(1);
   else
      m_pItemsSelect[37]->setSelectedIndex(0);

   if ( -1 != m_IndexHIDOSD )
      m_pItemsSelect[24]->setSelectedIndex((g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_HID_IN_OSD)?1:0);
   
   m_pItemsSelect[27]->setSelectedIndex(0);
   if ( g_pCurrentModel->osd_params.osd_flags3[iScreenIndex] & OSD_FLAG3_SHOW_GRID_CROSSHAIR )
      m_pItemsSelect[27]->setSelectedIndex(1);
   if ( g_pCurrentModel->osd_params.osd_flags3[iScreenIndex] & OSD_FLAG3_SHOW_GRID_DIAGONAL )
      m_pItemsSelect[27]->setSelectedIndex(2);
   if ( g_pCurrentModel->osd_params.osd_flags3[iScreenIndex] & OSD_FLAG3_SHOW_GRID_SQUARES )
      m_pItemsSelect[27]->setSelectedIndex(3);
   if ( g_pCurrentModel->osd_params.osd_flags3[iScreenIndex] & OSD_FLAG3_SHOW_GRID_DIAGONAL )
   if ( g_pCurrentModel->osd_params.osd_flags3[iScreenIndex] & OSD_FLAG3_SHOW_GRID_SQUARES )
      m_pItemsSelect[27]->setSelectedIndex(4);
   if ( g_pCurrentModel->osd_params.osd_flags3[iScreenIndex] & OSD_FLAG3_SHOW_GRID_THIRDS_SMALL )
      m_pItemsSelect[27]->setSelectedIndex(5);

   if ( -1 != m_IndexRCRSSI )
      m_pItemsSelect[34]->setSelection((g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_RC_RSSI)?1:0);

   m_pItemsSelect[30]->setSelection(p->iShowControllerCPUInfo);
   if ( hardware_i2c_has_current_sensor() )
   {
      m_pItemsSelect[31]->setSelection(pCS->iShowVoltage);
      m_pItemsSelect[31]->setEnabled(true);
   }
   else
   {
      m_pItemsSelect[31]->setSelection(0);
      m_pItemsSelect[31]->setEnabled(false);
   }

   m_pItemsSelect[32]->setSelectedIndex((g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_LAYOUT_LEFT_RIGHT)?1:0);
   if ( -1 != m_IndexSpeed )
   {
      m_pItemsSelect[33]->setSelectedIndex(0);
      if ( (g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_GROUND_SPEED) &&
           (g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_AIR_SPEED) )
         m_pItemsSelect[33]->setSelectedIndex(3);
      else if ( g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_GROUND_SPEED )
         m_pItemsSelect[33]->setSelectedIndex(1);
      else if ( g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_AIR_SPEED )
         m_pItemsSelect[33]->setSelectedIndex(2);
   }

   if ( g_pCurrentModel->osd_params.iCurrentOSDScreen == osdLayoutLean ||
        g_pCurrentModel->osd_params.iCurrentOSDScreen == osdLayoutLeanExtended )
   {
      for( int i=1; i<50; i++ )
         if ( NULL != m_pItemsSelect[i] )
            m_pItemsSelect[i]->setEnabled(false);
      if ( -1 != m_IndexWind )
         m_pItemsSelect[28]->setEnabled(false);
      if ( -1 != m_IndexTemperature )
         m_pItemsSelect[29]->setEnabled(false);
   }
   else
   {
      for( int i=1; i<50; i++ )
         if ( NULL != m_pItemsSelect[i] )
            m_pItemsSelect[i]->setEnabled(true);

      if ( -1 != m_IndexWind )
      {
         if ( ((g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) != MODEL_TYPE_AIRPLANE) && ( (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) != MODEL_TYPE_DRONE) )
            m_pItemsSelect[28]->setEnabled(false);
         else
            m_pItemsSelect[28]->setEnabled(true);
      }
      if ( -1 != m_IndexTemperature )
         m_pItemsSelect[29]->setEnabled(true);

      if ( (g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_RADIO_LINKS) ||
           (g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS) )
      {
         m_pItemsSelect[20]->setEnabled(true);
         m_pItemsSelect[21]->setEnabled(true);
      }
      else
      {
         m_pItemsSelect[20]->setEnabled(false);
         m_pItemsSelect[21]->setEnabled(false);
      }           
   }

   m_pItemsSelect[25]->setEnabled(true);

   if ( hardware_i2c_has_current_sensor() )
      m_pItemsSelect[31]->setEnabled(true);
   else
      m_pItemsSelect[31]->setEnabled(false);

   if ( g_pCurrentModel->osd_params.iCurrentOSDScreen == osdLayoutLean )
   {
      if ( -1 != m_IndexVoltage )
         m_pItemsSelect[1]->setEnabled(true);
      if ( -1 != m_IndexDistance )
         m_pItemsSelect[3]->setEnabled(true);
      if ( -1 != m_IndexAltitude )
         m_pItemsSelect[4]->setEnabled(true);
      if ( -1 != m_IndexHome )
         m_pItemsSelect[6]->setEnabled(true);
      if ( -1 != m_IndexHomeInvert )
         m_pItemsSelect[7]->setEnabled(true);
      if ( -1 != m_IndexHomeRotate )
         m_pItemsSelect[8]->setEnabled(true);
      if ( -1 != m_IndexTotalDistance )
         m_pItemsSelect[10]->setEnabled(true);
      m_pItemsSelect[11]->setEnabled(true);
      m_pItemsSelect[12]->setEnabled(true);
      if ( -1 != m_IndexThrottle )
         m_pItemsSelect[13]->setEnabled(true);
      m_pItemsSelect[19]->setEnabled(true);
      if ( -1 != m_IndexSpeed )
         m_pItemsSelect[33]->setEnabled(true);
   }
   else if ( g_pCurrentModel->osd_params.iCurrentOSDScreen == osdLayoutLeanExtended )
   {
      if ( -1 != m_IndexVoltage )
         m_pItemsSelect[1]->setEnabled(true);
      if ( -1 != m_IndexDistance )
         m_pItemsSelect[3]->setEnabled(true);
      if ( -1 != m_IndexAltitude )
         m_pItemsSelect[4]->setEnabled(true);
      if ( -1 != m_IndexHome )
         m_pItemsSelect[6]->setEnabled(true);
      if ( -1 != m_IndexHomeInvert )
         m_pItemsSelect[7]->setEnabled(true);
      if ( -1 != m_IndexHomeRotate )
         m_pItemsSelect[8]->setEnabled(true);
      if ( -1 != m_IndexTotalDistance )
         m_pItemsSelect[10]->setEnabled(true);
      m_pItemsSelect[11]->setEnabled(true);
      m_pItemsSelect[12]->setEnabled(true);
      if ( -1 != m_IndexThrottle )
         m_pItemsSelect[13]->setEnabled(true);
      if ( -1 != m_IndexSpeed )
         m_pItemsSelect[33]->setEnabled(true);
   }
   else
   {
      if ( (g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_RADIO_LINKS) ||
           (g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS) )
      {
         m_pItemsSelect[20]->setEnabled(true);
         m_pItemsSelect[21]->setEnabled(true);
      }
      else
      {
         m_pItemsSelect[20]->setEnabled(false);
         m_pItemsSelect[21]->setEnabled(false);
      }
   }

   if ( -1 != m_IndexWind )
      m_pItemsSelect[28]->setSelectedIndex((g_pCurrentModel->osd_params.osd_flags3[iScreenIndex] & OSD_FLAG3_SHOW_WIND)?1:0);
   if ( -1 != m_IndexTemperature )
      m_pItemsSelect[29]->setSelectedIndex((g_pCurrentModel->osd_params.osd_flags3[iScreenIndex] & OSD_FLAG3_SHOW_FC_TEMPERATURE)?1:0);

   if ( -1 != m_IndexHIDOSD )
   if ( g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexHIDOSD]->setEnabled(false);
}

void MenuVehicleOSDElements::onShow()
{
   int iTemp = m_SelectedIndex;
   Menu::onShow();
   valuesToUI();
   if ( iTemp >= 0 )
      m_SelectedIndex = iTemp;
}

void MenuVehicleOSDElements::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   float yStart = y;
   for( int i=0; i<m_ItemsCount/2; i++ )
      y += RenderItem(i,y);

   y = yStart;
   for( int i=m_ItemsCount/2; i<m_ItemsCount; i++ )
      y += RenderItem(i, y, getUsableWidth() + 2*m_sfMenuPaddingX);

   RenderEnd(yTop);
}


void MenuVehicleOSDElements::onSelectItem()
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
   int iScreenIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;

   Preferences* p = get_Preferences();
   ControllerSettings* pCS = get_ControllerSettings();

   if ( m_IndexOSDOrientation == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[32]->getSelectedIndex() )
         params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_LAYOUT_LEFT_RIGHT;
      else
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_LAYOUT_LEFT_RIGHT;
      sendToVehicle = true;
   }

   if ( m_IndexShowBg == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[25]->getSelectedIndex() )
         params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_BGBARS;
      else
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_BGBARS;
      sendToVehicle = true;
   }

   if ( m_IndexVoltage == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[1]->getSelectedIndex() )
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SHOW_BATTERY;
      else
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_BATTERY;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexVoltagePerCell == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[2]->getSelectedIndex() )
         params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_BATTERY_CELLS;
      else
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_BATTERY_CELLS;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexDistance == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[3]->getSelectedIndex() )
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SHOW_DISTANCE;
      else
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_DISTANCE;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexTotalDistance == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[10]->getSelectedIndex() )
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_TOTAL_DISTANCE;
      else
         params.osd_flags[iScreenIndex] |= OSD_FLAG_TOTAL_DISTANCE;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexSpeed == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[33]->getSelectedIndex() )
      {
         params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_GROUND_SPEED;
         params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_AIR_SPEED;
      }
      if ( 3 == m_pItemsSelect[33]->getSelectedIndex() )
      {
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_GROUND_SPEED;
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_AIR_SPEED;
      }
      if ( 1 == m_pItemsSelect[33]->getSelectedIndex() )
      {
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_GROUND_SPEED;
         params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_AIR_SPEED;
      }
      if ( 2 == m_pItemsSelect[33]->getSelectedIndex() )
      {
         params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_GROUND_SPEED;
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_AIR_SPEED;
      }
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexAltitude == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[4]->getSelectedIndex() )
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SHOW_ALTITUDE;
      else
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_ALTITUDE;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexHome == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[6]->getSelectedIndex() )
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SHOW_HOME;
      else
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_HOME;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexHomeInvert == m_SelectedIndex )
   {
      params.invert_home_arrow = m_pItemsSelect[7]->getSelectedIndex();
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexHomeRotate == m_SelectedIndex )
   {
      int index = m_pItemsSelect[8]->getSelectedIndex();
      if ( 0 == index )
         params.home_arrow_rotate = 0;
      if ( 1 == index )
         params.home_arrow_rotate = 90;
      if ( 2 == index )
         params.home_arrow_rotate = 180;
      if ( 3 == index )
         params.home_arrow_rotate = 270;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexShowGPSInfo == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[26]->getSelectedIndex() )
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SHOW_GPS_INFO;
      else
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_GPS_INFO;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexShowGPSPos == m_SelectedIndex )
   {
      if ( 0 != m_pItemsSelect[9]->getSelectedIndex() )
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_GPS_POS;
      else
         params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_GPS_POS;
      if ( 2 == m_pItemsSelect[9]->getSelectedIndex() )
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SCRAMBLE_GPS;
      else
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SCRAMBLE_GPS;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexMode == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[11]->getSelectedIndex() )
      {
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SHOW_FLIGHT_MODE;
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SHOW_FLIGHT_MODE_CHANGE;
      }
      else if ( 1 == m_pItemsSelect[11]->getSelectedIndex() )
      {
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_FLIGHT_MODE;
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SHOW_FLIGHT_MODE_CHANGE;
      }
      else if ( 2 == m_pItemsSelect[11]->getSelectedIndex() )
      {
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SHOW_FLIGHT_MODE;
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_FLIGHT_MODE_CHANGE;
      }
      else
      {
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_FLIGHT_MODE;
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_FLIGHT_MODE_CHANGE;
      }
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }
   
   if ( m_IndexTime == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[12]->getSelectedIndex() )
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SHOW_TIME;
      else
      {
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_TIME;
         if ( m_pItemsSelect[12]->getSelectedIndex() == 1 )
            params.osd_flags[iScreenIndex] &= (~OSD_FLAG_SHOW_TIME_LOWER);
         else
            params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_TIME_LOWER;
      }
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexWind == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[28]->getSelectedIndex() )
         params.osd_flags3[iScreenIndex] &= ~OSD_FLAG3_SHOW_WIND;
      else
         params.osd_flags3[iScreenIndex] |= OSD_FLAG3_SHOW_WIND;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexTemperature == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[29]->getSelectedIndex() )
         params.osd_flags3[iScreenIndex] &= ~OSD_FLAG3_SHOW_FC_TEMPERATURE;
      else
         params.osd_flags3[iScreenIndex] |= OSD_FLAG3_SHOW_FC_TEMPERATURE;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexThrottle == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[13]->getSelectedIndex() )
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SHOW_THROTTLE;
      else
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_THROTTLE;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexPitch == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[14]->getSelectedIndex() )
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SHOW_PITCH;
      else
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_PITCH;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexCPU == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[15]->getSelectedIndex() )
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SHOW_CPU_INFO;
      else
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_CPU_INFO;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexVideo == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[16]->getSelectedIndex() )
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SHOW_VIDEO_MODE;
      else
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_VIDEO_MODE;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexShowDetailedBPS == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[17]->getSelectedIndex() )
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SHOW_VIDEO_MBPS;
      else
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_VIDEO_MBPS;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexVideoExtended == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[18]->getSelectedIndex() )
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SHOW_VIDEO_MODE_EXTENDED;
      else
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_VIDEO_MODE_EXTENDED;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexRadioLinks == m_SelectedIndex )
   {
      params.osd_flags[iScreenIndex] &= ~(OSD_FLAG_SHOW_RADIO_LINKS | OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS);
      if ( 1 == m_pItemsSelect[19]->getSelectedIndex() )
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_RADIO_LINKS;
      else if ( 2 == m_pItemsSelect[19]->getSelectedIndex() )
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS;
      else if ( 3 == m_pItemsSelect[19]->getSelectedIndex() )
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_RADIO_LINKS | OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS;

      g_pCurrentModel->checkUpdateOSDRadioLinksFlags(&params);
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }
   
   if ( m_IndexRadioLinkBars == m_SelectedIndex )
   {
      params.osd_flags2[iScreenIndex] &= ~(OSD_FLAG2_SHOW_RADIO_LINK_QUALITY_BARS | OSD_FLAG2_SHOW_RADIO_LINK_QUALITY_NUMBERS);
      if ( 0 == m_pItemsSelect[20]->getSelectedIndex() )
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_RADIO_LINK_QUALITY_BARS;
      else if ( 1 == m_pItemsSelect[20]->getSelectedIndex() )
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_RADIO_LINK_QUALITY_NUMBERS;
      else
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_RADIO_LINK_QUALITY_BARS | OSD_FLAG2_SHOW_RADIO_LINK_QUALITY_NUMBERS;
      
      g_pCurrentModel->checkUpdateOSDRadioLinksFlags(&params);
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexRadioLinksNumbers == m_SelectedIndex )
   {
      params.osd_flags3[iScreenIndex] &= ~(OSD_FLAG3_SHOW_RADIO_LINK_QUALITY_NUMBERS_DBM | OSD_FLAG3_SHOW_RADIO_LINK_QUALITY_NUMBERS_SNR | OSD_FLAG3_SHOW_RADIO_LINK_QUALITY_NUMBERS_PERCENT);
      if ( 0 == m_pItemsSelect[35]->getSelectedIndex() )
         params.osd_flags3[iScreenIndex] |= OSD_FLAG3_SHOW_RADIO_LINK_QUALITY_NUMBERS_DBM | OSD_FLAG3_SHOW_RADIO_LINK_QUALITY_NUMBERS_PERCENT;
      else if ( 1 == m_pItemsSelect[35]->getSelectedIndex() )
         params.osd_flags3[iScreenIndex] |= OSD_FLAG3_SHOW_RADIO_LINK_QUALITY_NUMBERS_DBM | OSD_FLAG3_SHOW_RADIO_LINK_QUALITY_NUMBERS_SNR;
      else
         params.osd_flags3[iScreenIndex] |= OSD_FLAG3_SHOW_RADIO_LINK_QUALITY_NUMBERS_SNR | OSD_FLAG3_SHOW_RADIO_LINK_QUALITY_NUMBERS_PERCENT;

      g_pCurrentModel->checkUpdateOSDRadioLinksFlags(&params);
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexRadioInterfaces == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[21]->getSelectedIndex() )
      {
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SHOW_RADIO_INTERFACES_INFO;
         params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_RADIO_LINK_INTERFACES_EXTENDED;
      }
      else if ( 1 == m_pItemsSelect[21]->getSelectedIndex() )
      {
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_RADIO_INTERFACES_INFO;
         params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_RADIO_LINK_INTERFACES_EXTENDED;
      }
      else if ( 2 == m_pItemsSelect[21]->getSelectedIndex() )
      {
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_RADIO_INTERFACES_INFO;
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_RADIO_LINK_INTERFACES_EXTENDED;
      }

      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexShowTxPower == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[36]->getSelectedIndex() )
         params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_TX_POWER;
      else
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_TX_POWER;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexRCRSSI == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[34]->getSelectedIndex() )
         params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_RC_RSSI;
      else
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_RC_RSSI;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexSignalBars == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[22]->getSelectedIndex() )
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SHOW_SIGNAL_BARS;
      else
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_SIGNAL_BARS;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }
   if ( m_IndexSignalBarsPosition == m_SelectedIndex )
   {
      int index = m_pItemsSelect[23]->getSelectedIndex();
      params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SIGNAL_BARS_MASK;
      params.osd_flags[iScreenIndex] |= (index<<14);
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexLinkWarningOutline == m_SelectedIndex )
   {
      params.osd_flags3[iScreenIndex] &= ~(OSD_FLAG3_SHOW_LINK_WARNING_OUTLINE | OSD_FLAG3_SHOW_LINK_WARNING_BLACK_BARS);
      if ( 1 == m_pItemsSelect[37]->getSelectedIndex() )
         params.osd_flags3[iScreenIndex] |= OSD_FLAG3_SHOW_LINK_WARNING_OUTLINE;
      if ( 2 == m_pItemsSelect[37]->getSelectedIndex() )
         params.osd_flags3[iScreenIndex] |= OSD_FLAG3_SHOW_LINK_WARNING_BLACK_BARS;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }


   if ( m_IndexHIDOSD == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[24]->getSelectedIndex() )
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SHOW_HID_IN_OSD;
      else
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_HID_IN_OSD;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexGrid == m_SelectedIndex )
   {
      params.osd_flags3[iScreenIndex] &= ~(OSD_FLAG3_SHOW_GRID_CROSSHAIR | OSD_FLAG3_SHOW_GRID_DIAGONAL | OSD_FLAG3_SHOW_GRID_SQUARES | OSD_FLAG3_SHOW_GRID_THIRDS_SMALL);
      if ( 0 == m_pItemsSelect[27]->getSelectedIndex() )
         params.osd_flags3[iScreenIndex] &= ~(OSD_FLAG3_SHOW_GRID_CROSSHAIR | OSD_FLAG3_SHOW_GRID_DIAGONAL | OSD_FLAG3_SHOW_GRID_SQUARES);
      else if ( 1 == m_pItemsSelect[27]->getSelectedIndex() )
         params.osd_flags3[iScreenIndex] |= OSD_FLAG3_SHOW_GRID_CROSSHAIR;
      else if ( 2 == m_pItemsSelect[27]->getSelectedIndex() )
         params.osd_flags3[iScreenIndex] |= OSD_FLAG3_SHOW_GRID_DIAGONAL;
      else if ( 3 == m_pItemsSelect[27]->getSelectedIndex() )
         params.osd_flags3[iScreenIndex] |= OSD_FLAG3_SHOW_GRID_SQUARES;
      else if ( 4 == m_pItemsSelect[27]->getSelectedIndex() )
         params.osd_flags3[iScreenIndex] |= OSD_FLAG3_SHOW_GRID_DIAGONAL | OSD_FLAG3_SHOW_GRID_SQUARES;
      else if ( 5 == m_pItemsSelect[27]->getSelectedIndex() )
         params.osd_flags3[iScreenIndex] |= OSD_FLAG3_SHOW_GRID_THIRDS_SMALL;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexControllerCPU == m_SelectedIndex )
   {
      p->iShowControllerCPUInfo = m_pItemsSelect[30]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
      return;
   }

   if ( m_IndexControllerVoltage == m_SelectedIndex )
   {
      pCS->iShowVoltage = m_pItemsSelect[31]->getSelectedIndex();
      save_ControllerSettings();
      valuesToUI();
      return;
   }

   if ( g_pCurrentModel->is_spectator )
   {
      memcpy(&(g_pCurrentModel->osd_params), &params, sizeof(osd_parameters_t));
      saveControllerModel(g_pCurrentModel);
      valuesToUI();
   }
   else if ( sendToVehicle )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_OSD_PARAMS, 0, (u8*)&params, sizeof(osd_parameters_t)) )
         valuesToUI();
      return;
   }
}
