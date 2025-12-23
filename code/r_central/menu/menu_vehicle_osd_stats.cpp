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
#include "menu_vehicle_osd_stats.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "menu_item_section.h"
#include "../osd/osd_common.h"

MenuVehicleOSDStats::MenuVehicleOSDStats(void)
:Menu(MENU_ID_VEHICLE_OSD, "OSD Statistics Panels", NULL)
{
   m_Width = 0.36;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.22;

   char szBuff[256];
   sprintf(szBuff, L("OSD Statistics Panels (%s)"), str_get_osd_screen_name(g_pCurrentModel->osd_params.iCurrentOSDScreen));
   setTitle(szBuff);
   
   m_IndexDevStatsVideo = -1;
   m_IndexDevVehicleVideoBitrateHistory = -1;
   m_IndexDevStatsRadio = -1;
   m_IndexDevFullRXStats = -1;
   m_IndexShowControllerAdaptiveInfoStats = -1;
   m_IndexTelemetryStats = -1;
   m_IndexAudioDecodeStats = -1;
   m_IndexDevStatsVehicleTx = -1;
   m_IndexSnapshot = -1;
   m_IndexStatsVideoH264FramesInfo = -1;
   m_IndexRadioRxDbgPingStats = -1;

   m_pItemsSelect[0] = new MenuItemSelect(L("Font Size"), L("Increase/decrease OSD font size for stats windows for current OSD screen."));  
   m_pItemsSelect[0]->addSelection(L("Smallest"));
   m_pItemsSelect[0]->addSelection(L("Smaller"));
   m_pItemsSelect[0]->addSelection(L("Small"));
   m_pItemsSelect[0]->addSelection(L("Normal"));
   m_pItemsSelect[0]->addSelection(L("Large"));
   m_pItemsSelect[0]->addSelection(L("Larger"));
   m_pItemsSelect[0]->addSelection(L("Largest"));
   m_pItemsSelect[0]->setIsEditable();
   m_IndexFontSize = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[21] = new MenuItemSelect(L("Transparency"), L("Change how transparent the statistics background is."));  
   m_pItemsSelect[21]->addSelection(L("Max"));
   m_pItemsSelect[21]->addSelection(L("Medium"));
   m_pItemsSelect[21]->addSelection(L("Normal"));
   m_pItemsSelect[21]->addSelection(L("Minimum"));
   m_pItemsSelect[21]->setIsEditable();
   m_IndexTransparency = addMenuItem(m_pItemsSelect[21]);


   m_pItemsSelect[7] = new MenuItemSelect(L("Stats panels layout"), L("Changes the layout of the statistics panels."));  
   m_pItemsSelect[7]->addSelection(L("Auto (Left)"));
   m_pItemsSelect[7]->addSelection(L("Auto (Right)"));
   m_pItemsSelect[7]->addSelection(L("Auto (Top)"));
   m_pItemsSelect[7]->addSelection(L("Auto (Bottom)"));
   m_pItemsSelect[7]->setIsEditable();
   m_IndexPanelsDirection = addMenuItem(m_pItemsSelect[7]);

   m_pItemsSelect[32] = new MenuItemSelect(L("Add margins for widgets"), L("When arranging stats on the screen, allow room for widgets if any are overlaping with stats."));
   m_pItemsSelect[32]->addSelection(L("No"));
   m_pItemsSelect[32]->addSelection(L("Auto"));
   m_pItemsSelect[32]->setIsEditable();
   m_IndexFitWidgets = addMenuItem(m_pItemsSelect[32]);

   addSeparator();

   m_pItemsSelect[1] = new MenuItemSelect(L("Radio: Links stats"), L("Show statistics about the radio links health."));  
   m_pItemsSelect[1]->addSelection(L("Off"));
   m_pItemsSelect[1]->addSelection(L("On"));
   m_pItemsSelect[1]->setUseMultiViewLayout();
   m_IndexStatsRadioLinks = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSelect[2] = new MenuItemSelect(L("Radio: Interfaces stats"), L("Show statistics about the radio interfaces present on the controller and how they perform."));  
   m_pItemsSelect[2]->addSelection(L("Off"));
   m_pItemsSelect[2]->addSelection(L("Minimal"));
   m_pItemsSelect[2]->addSelection(L("Compact"));
   m_pItemsSelect[2]->addSelection(L("Full"));
   m_pItemsSelect[2]->setIsEditable();
   m_IndexStatsRadioInterfaces = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSelect[17] = new MenuItemSelect(L("    Displayed Radio Cards"), L("Choose what radio cards statistics to see: from controller radio cards or vehicle radio cards or both."));  
   m_pItemsSelect[17]->addSelection(L("Controller Only"));
   m_pItemsSelect[17]->addSelection(L("Controller & Vehicle"));
   m_pItemsSelect[17]->setIsEditable();
   m_IndexVehicleRadioRxStats = addMenuItem(m_pItemsSelect[17]);


   m_pItemsSelect[8] = new MenuItemSelect(L("    Graphs Resolution"), L("The resolution of the graphs, in miliseconds / bar."));  
   m_pItemsSelect[8]->addSelection("10 ms/bar");
   m_pItemsSelect[8]->addSelection("20 ms/bar");
   m_pItemsSelect[8]->addSelection("50 ms/bar");
   m_pItemsSelect[8]->addSelection("100 ms/bar");
   m_pItemsSelect[8]->addSelection("200 ms/bar");
   m_pItemsSelect[8]->addSelection("500 ms/bar");
   m_pItemsSelect[8]->setIsEditable();
   m_IndexRadioRefreshInterval = addMenuItem(m_pItemsSelect[8]);

   m_pItemsSelect[27] = new MenuItemSelect(L("Radio: Packets Rx History (Controller)"), L("Shows the history of received radio packets for each radio interface on the controller."));
   m_pItemsSelect[27]->addSelection(L("Off"));
   m_pItemsSelect[27]->addSelection(L("On"));
   m_pItemsSelect[27]->setUseMultiViewLayout();
   m_IndexRadioRxHistoryController = addMenuItem(m_pItemsSelect[27]);

   m_pItemsSelect[29] = new MenuItemSelect(L("    Show Large"), L("Shows a big history of received radio packets for selected radio interface on the controller."));
   m_pItemsSelect[29]->addSelection(L("None"));
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      char szTmp[128];
      szTmp[0] = 0;
      controllerGetCardUserDefinedNameOrType(pRadioHWInfo, szTmp);
      sprintf(szBuff, "Int %d, %s, %s", i+1, pRadioHWInfo->szUSBPort, szTmp);
      m_pItemsSelect[29]->addSelection(szBuff);
   }
   for( int i=1; i<hardware_get_radio_interfaces_count(); i++ )
   {
      sprintf(szBuff, "Int 1 & %d", i+1);
      m_pItemsSelect[29]->addSelection(szBuff);
   }
   m_pItemsSelect[29]->setIsEditable();
   m_IndexRadioRxHistoryControllerBig = addMenuItem(m_pItemsSelect[29]);

   m_pItemsSelect[28] = new MenuItemSelect(L("Radio: Packets Rx History (Vehicle)"), L("Shows the history of received radio packets for each radio interface on this vehicle."));
   m_pItemsSelect[28]->addSelection(L("Off"));
   m_pItemsSelect[28]->addSelection(L("On"));
   m_pItemsSelect[28]->setUseMultiViewLayout();
   m_IndexRadioRxHistoryVehicle = addMenuItem(m_pItemsSelect[28]);

   m_pItemsSelect[30] = new MenuItemSelect(L("    Show Large"), L("Shows a big history of received radio packets for selected radio interface on the vehicle."));
   m_pItemsSelect[30]->addSelection(L("None"));
   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      sprintf(szBuff, "Int %d, %s, %s", i+1, g_pCurrentModel->radioInterfacesParams.interface_szPort[i], str_get_radio_card_model_string_short(g_pCurrentModel->radioInterfacesParams.interface_card_model[i]));
      m_pItemsSelect[30]->addSelection(szBuff);
   }
   for( int i=1; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      sprintf(szBuff, "Int 1 & %d", i+1);
      m_pItemsSelect[30]->addSelection(szBuff);
   }
   m_pItemsSelect[30]->setIsEditable();
   m_IndexRadioRxHistoryVehicleBig = addMenuItem(m_pItemsSelect[30]);

   if ( g_pControllerSettings->iDeveloperMode )
   {
      m_pItemsSelect[11] = new MenuItemSelect(L("Radio: Extended Radio Stats"), L("Shows the extended developer radio stats."));
      m_pItemsSelect[11]->addSelection(L("Off"));
      m_pItemsSelect[11]->addSelection(L("On"));
      m_pItemsSelect[11]->setUseMultiViewLayout();
      m_pItemsSelect[11]->setTextColor(get_Color_Dev());
      m_IndexDevStatsRadio = addMenuItem(m_pItemsSelect[11]);

      m_pItemsSelect[12] = new MenuItemSelect(L("Radio: Full Radio RX Stats"), L("Shows full statistics about radio stack state."));
      m_pItemsSelect[12]->addSelection(L("Off"));
      m_pItemsSelect[12]->addSelection(L("On"));
      m_pItemsSelect[12]->setUseMultiViewLayout();
      m_pItemsSelect[12]->setTextColor(get_Color_Dev());
      m_IndexDevFullRXStats = addMenuItem(m_pItemsSelect[12]);

      m_pItemsSelect[20] = new MenuItemSelect(L("Radio: Vehicle Tx Stats"), L("Shows graphs about vehicle transmissions."));
      m_pItemsSelect[20]->addSelection(L("Off"));
      m_pItemsSelect[20]->addSelection(L("On"));
      m_pItemsSelect[20]->setUseMultiViewLayout();
      m_pItemsSelect[20]->setTextColor(get_Color_Dev());
      m_IndexDevStatsVehicleTx = addMenuItem(m_pItemsSelect[20]);

      m_pItemsSelect[31] = new MenuItemSelect(L("Radio: Ping Stats"), L("Shows graphs about ping roundtrip stats."));
      m_pItemsSelect[31]->addSelection(L("Off"));
      m_pItemsSelect[31]->addSelection(L("On"));
      m_pItemsSelect[31]->setUseMultiViewLayout();
      m_pItemsSelect[31]->setTextColor(get_Color_Dev());
      m_IndexRadioRxDbgPingStats = addMenuItem(m_pItemsSelect[31]);
   }

   addSeparator();

   m_pItemsSelect[35] = new MenuItemSelect(L("Video Frame Stats"), L("Show statistics about the video stream frames and video stream quality."));  
   m_pItemsSelect[35]->addSelection(L("Off"));
   m_pItemsSelect[35]->addSelection(L("On"));
   m_pItemsSelect[35]->setUseMultiViewLayout();
   m_IndexVideoFramesStats = addMenuItem(m_pItemsSelect[35]);


   m_pItemsSelect[3] = new MenuItemSelect(L("Video: Stream Stats"), L("Show statistics about the video stream rx & decode process and stream quality."));  
   m_pItemsSelect[3]->addSelection(L("Off"));
   m_pItemsSelect[3]->addSelection(L("On"));
   m_pItemsSelect[3]->setUseMultiViewLayout();
   m_IndexStatsDecode = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSelect[4] = new MenuItemSelect(L("    Display Style"), L("Selects the style and how much information to show in the video stream stats."));  
   m_pItemsSelect[4]->addSelection(L("Minimal"));
   m_pItemsSelect[4]->addSelection(L("Compact"));
   m_pItemsSelect[4]->addSelection(L("Normal"));
   m_pItemsSelect[4]->addSelection(L("Extended"));
   m_pItemsSelect[4]->setIsEditable();
   m_IndexStatsVideoExtended = addMenuItem(m_pItemsSelect[4]);

   m_pItemsSelect[9] = new MenuItemSelect(L("    Graphs Resolution"), L("The resolution of the graphs, in miliseconds / bar."));  
   m_pItemsSelect[9]->addSelection("10 ms/bar");
   m_pItemsSelect[9]->addSelection("20 ms/bar");
   m_pItemsSelect[9]->addSelection("50 ms/bar");
   m_pItemsSelect[9]->addSelection("100 ms/bar");
   //m_pItemsSelect[9]->addSelection("200 ms/bar");
   //m_pItemsSelect[9]->addSelection("500 ms/bar");
   m_pItemsSelect[9]->setIsEditable();
   m_IndexVideoRefreshInterval = addMenuItem(m_pItemsSelect[9]);

   if ( g_pControllerSettings->iDeveloperMode )
   {
      m_pItemsSelect[13] = new MenuItemSelect(L("    Show snapshot on retransmissions"), L("Shows a snapshot of buffers and state when a video block is discarded."));  
      m_pItemsSelect[13]->addSelection(L("Off"));
      m_pItemsSelect[13]->addSelection(L("On"));
      m_pItemsSelect[13]->setTextColor(get_Color_Dev());
      m_IndexSnapshot = addMenuItem(m_pItemsSelect[13]);

      m_pItemsSelect[22] = new MenuItemSelect(L("    Snapshot closes on"), L("Select how the snapshot window is closed."));  
      m_pItemsSelect[22]->addSelection(L("Back Key"));
      m_pItemsSelect[22]->addSelection(L("Timeout Period"));
      m_pItemsSelect[22]->addSelection(L("Next Snapshot"));
      m_pItemsSelect[22]->setTextColor(get_Color_Dev());
      m_pItemsSelect[22]->setIsEditable();
      m_IndexSnapshotTimeout = addMenuItem(m_pItemsSelect[22]);
   }
   else
   {
      m_IndexSnapshot = -1;
      m_IndexSnapshotTimeout = -1;
   }

   m_pItemsSelect[19] = new MenuItemSelect(L("Video: Bitrate History"), L("Shows history graph on video bitrate and radio datarates."));
   m_pItemsSelect[19]->addSelection(L("Off"));
   m_pItemsSelect[19]->addSelection(L("On"));
   m_pItemsSelect[19]->setUseMultiViewLayout();
   m_IndexDevVehicleVideoBitrateHistory = addMenuItem(m_pItemsSelect[19]);


   m_pItemsSelect[33] = new MenuItemSelect(L("    Graphs Resolution"), L("The resolution of the graphs, in miliseconds / bar."));  
   m_pItemsSelect[33]->addSelection("500 ms/bar");
   m_pItemsSelect[33]->addSelection("200 ms/bar");
   m_pItemsSelect[33]->addSelection("100 ms/bar");
   m_pItemsSelect[33]->addSelection("50 ms/bar");
   m_pItemsSelect[33]->setIsEditable();
   m_IndexRefreshIntervalVideoBitrateHistory = addMenuItem(m_pItemsSelect[33]);

   if ( g_pControllerSettings->iDeveloperMode )
   {
      m_pItemsSelect[23] = new MenuItemSelect(L("Video: H264/H265 Frame Stats"), L("Show statistics about the video stream H264/H65 frames and auto adjustments."));
      m_pItemsSelect[23]->addSelection(L("Off"));
      m_pItemsSelect[23]->addSelection(L("On"));
      m_pItemsSelect[23]->setUseMultiViewLayout();
      m_pItemsSelect[23]->setTextColor(get_Color_Dev());
      m_IndexStatsVideoH264FramesInfo = addMenuItem(m_pItemsSelect[23]);
      
      m_pItemsSelect[10] = new MenuItemSelect(L("Video: Retransmissions Stats"), L("Shows the extended developer video retransmissions stats."));
      m_pItemsSelect[10]->addSelection(L("Off"));
      m_pItemsSelect[10]->addSelection(L("On"));
      m_pItemsSelect[10]->setUseMultiViewLayout();
      m_pItemsSelect[10]->setTextColor(get_Color_Dev());
      m_IndexDevStatsVideo = addMenuItem(m_pItemsSelect[10]);

      m_pItemsSelect[24] = new MenuItemSelect(L("Video: Controller Adaptive Video Info Stats"), L(""));
      m_pItemsSelect[24]->addSelection(L("Off"));
      m_pItemsSelect[24]->addSelection(L("On"));
      m_pItemsSelect[24]->setUseMultiViewLayout();
      m_pItemsSelect[24]->setTextColor(get_Color_Dev());
      m_IndexShowControllerAdaptiveInfoStats = addMenuItem(m_pItemsSelect[24]);
   }

   addSeparator();

   m_IndexVehicleDevStats = -1;
   if ( g_pControllerSettings->iDeveloperMode )
   {
      m_pItemsSelect[34] = new MenuItemSelect(L("Vehicle Dev Stats"), L("Show developer statistics from vehicle state."));  
      m_pItemsSelect[34]->addSelection(L("Off"));
      m_pItemsSelect[34]->addSelection(L("On"));
      m_pItemsSelect[34]->setUseMultiViewLayout();
      m_IndexVehicleDevStats = addMenuItem(m_pItemsSelect[34]);
   }
   m_pItemsSelect[26] = new MenuItemSelect(L("Audio: Decoding Stats"), L("Show statistics about the audio decoding process and quality."));  
   m_pItemsSelect[26]->addSelection(L("Off"));
   m_pItemsSelect[26]->addSelection(L("On"));
   m_pItemsSelect[26]->setUseMultiViewLayout();
   m_IndexAudioDecodeStats = addMenuItem(m_pItemsSelect[26]);

   m_pItemsSelect[25] = new MenuItemSelect(L("Telemetry: FC Stats"), L("Shows detailed information about telemetry data received."));
   m_pItemsSelect[25]->addSelection(L("Off"));
   m_pItemsSelect[25]->addSelection(L("On"));
   m_pItemsSelect[25]->setUseMultiViewLayout();
   m_IndexTelemetryStats = addMenuItem(m_pItemsSelect[25]);

   m_pItemsSelect[6] = new MenuItemSelect(L("RC: Vehicle Stats"), L("Show statistics received from vehicle about the RC link status and health."));  
   m_pItemsSelect[6]->addSelection(L("Off"));
   m_pItemsSelect[6]->addSelection(L("On"));
   m_pItemsSelect[6]->setUseMultiViewLayout();
   m_IndexStatsRC = addMenuItem(m_pItemsSelect[6]);
   #ifndef FEATURE_ENABLE_RC
   m_pMenuItems[m_IndexStatsRC]->setEnabled(false);
   #endif

   m_pItemsSelect[5] = new MenuItemSelect(L("Efficiency Stats"), L("Show statistics about power usage efficiency."));  
   m_pItemsSelect[5]->addSelection(L("Off"));
   m_pItemsSelect[5]->addSelection(L("On"));
   m_pItemsSelect[5]->setUseMultiViewLayout();
   m_IndexStatsEff = addMenuItem(m_pItemsSelect[5]);
}

MenuVehicleOSDStats::~MenuVehicleOSDStats()
{
}

void MenuVehicleOSDStats::valuesToUI()
{
   int iScreenIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;
   Preferences* pP = get_Preferences();

   m_pItemsSelect[0]->setSelectedIndex(((g_pCurrentModel->osd_params.osd_preferences[iScreenIndex])>>16) & 0x0F);
   m_pItemsSelect[21]->setSelectedIndex(((g_pCurrentModel->osd_params.osd_preferences[iScreenIndex])>>20) & 0x0F);
   m_pItemsSelect[1]->setSelection((g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_STATS_RADIO_LINKS)?1:0);
   m_pItemsSelect[32]->setSelection((g_pCurrentModel->osd_params.osd_flags3[iScreenIndex] & OSD_FLAG3_LAYOUT_STATS_AUTO_WIDGETS_MARGINS)?1:0);
   
   if ( ! (g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_STATS_RADIO_INTERFACES) )
      m_pItemsSelect[2]->setSelection(0);
   else if ( g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_MINIMAL_RADIO_INTERFACES_STATS )
      m_pItemsSelect[2]->setSelection(1);
   else if ( g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_RADIO_INTERFACES_COMPACT )
      m_pItemsSelect[2]->setSelection(2);
   else
      m_pItemsSelect[2]->setSelection(3);

   if ( g_pCurrentModel->osd_params.iRadioInterfacesGraphRefreshIntervalMs <= 10 )
      m_pItemsSelect[8]->setSelection(0);
   else if ( g_pCurrentModel->osd_params.iRadioInterfacesGraphRefreshIntervalMs <= 20 )
      m_pItemsSelect[8]->setSelection(1);
   else if ( g_pCurrentModel->osd_params.iRadioInterfacesGraphRefreshIntervalMs <= 50 )
      m_pItemsSelect[8]->setSelection(2);
   else if ( g_pCurrentModel->osd_params.iRadioInterfacesGraphRefreshIntervalMs <= 100 )
      m_pItemsSelect[8]->setSelection(3);
   else if ( g_pCurrentModel->osd_params.iRadioInterfacesGraphRefreshIntervalMs <= 200 )
      m_pItemsSelect[8]->setSelection(4);
   else
      m_pItemsSelect[8]->setSelection(5);

   if ( g_pControllerSettings->nGraphVideoRefreshInterval <= 10 )
      m_pItemsSelect[9]->setSelection(0);
   else if ( g_pControllerSettings->nGraphVideoRefreshInterval <= 20 )
      m_pItemsSelect[9]->setSelection(1);
   else if ( g_pControllerSettings->nGraphVideoRefreshInterval <= 50 )
      m_pItemsSelect[9]->setSelection(2);
   else if ( g_pControllerSettings->nGraphVideoRefreshInterval <= 100 )
      m_pItemsSelect[9]->setSelection(3);
   else if ( g_pControllerSettings->nGraphVideoRefreshInterval <= 200 )
      m_pItemsSelect[9]->setSelection(4);
   else
      m_pItemsSelect[9]->setSelection(5);

   if ( -1 != m_IndexRadioRxDbgPingStats )
      m_pItemsSelect[31]->setSelectedIndex(g_pControllerSettings->iDbgPingGraphs);

   if ( g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_STATS_RADIO_INTERFACES )
   {
      m_pItemsSelect[8]->setEnabled(true);
      m_pItemsSelect[17]->setEnabled(true);
   }
   else
   {
      m_pItemsSelect[8]->setEnabled(false);
      m_pItemsSelect[17]->setEnabled(false);
   }

   m_pItemsSelect[27]->setSelectedIndex((g_pCurrentModel->osd_params.osd_flags3[iScreenIndex] & OSD_FLAG3_SHOW_RADIO_RX_HISTORY_CONTROLLER)?1:0);
   m_pItemsSelect[28]->setSelectedIndex((g_pCurrentModel->osd_params.osd_flags3[iScreenIndex] & OSD_FLAG3_SHOW_RADIO_RX_HISTORY_VEHICLE)?1:0);
   
   if ( m_pItemsSelect[27]->getSelectedIndex() == 0 )
      m_pItemsSelect[29]->setEnabled(false);
   else
      m_pItemsSelect[29]->setEnabled(true);

   if ( m_pItemsSelect[28]->getSelectedIndex() == 0 )
      m_pItemsSelect[30]->setEnabled(false);
   else
      m_pItemsSelect[30]->setEnabled(true);

   m_pItemsSelect[29]->setSelectedIndex(0);
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_pControllerSettings->uShowBigRxHistoryInterface & (1<<i) )
         m_pItemsSelect[29]->setSelectedIndex(i+1);
   }
   if ( g_pControllerSettings->uShowBigRxHistoryInterface & 1 )
   {
      for( int i=1; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_pControllerSettings->uShowBigRxHistoryInterface & (1<<i) )
            m_pItemsSelect[29]->setSelectedIndex(i+hardware_get_radio_interfaces_count());
      }   
   }

   m_pItemsSelect[30]->setSelectedIndex(0);
   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( g_pControllerSettings->uShowBigRxHistoryInterface & ((1<<i)<<16) )
         m_pItemsSelect[30]->setSelectedIndex(i+1);
   }
   if ( g_pControllerSettings->uShowBigRxHistoryInterface & (1<<16) )
   {
      for( int i=1; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( g_pControllerSettings->uShowBigRxHistoryInterface & ((1<<i)<<16) )
            m_pItemsSelect[30]->setSelectedIndex(i+g_pCurrentModel->radioInterfacesParams.interfaces_count);
      }   
   }

   m_pItemsSelect[25]->setSelectedIndex((g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_TELEMETRY_STATS)?1:0);
   m_pItemsSelect[26]->setSelectedIndex((g_pCurrentModel->osd_params.osd_flags3[iScreenIndex] & OSD_FLAG3_SHOW_AUDIO_DECODE_STATS)?1:0);
   m_pItemsSelect[17]->setSelectedIndex((g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_VEHICLE_RADIO_INTERFACES_STATS)?1:0);

   if ( -1 != m_IndexStatsVideoH264FramesInfo )
   {
      if ( g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_STATS_VIDEO_H264_FRAMES_INFO )
      {
         //if ( 0 == g_pControllerSettings->iShowVideoStreamInfoCompactType )
         //   m_pItemsSelect[23]->setSelectedIndex(3);
         //else if ( 1 == g_pControllerSettings->iShowVideoStreamInfoCompactType )
         //   m_pItemsSelect[23]->setSelectedIndex(2);
         //else
            m_pItemsSelect[23]->setSelectedIndex(1);
      }
      else
         m_pItemsSelect[23]->setSelectedIndex(0);
   }

   if ( g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_STATS_VIDEO )
   {
      m_pItemsSelect[4]->setEnabled(true);
      m_pItemsSelect[9]->setEnabled(true);
      if ( g_pControllerSettings->iDeveloperMode )
      {
         if ( -1 != m_IndexSnapshot )
            m_pItemsSelect[13]->setEnabled(true);
         if ( -1 != m_IndexSnapshotTimeout )
            m_pItemsSelect[22]->setEnabled(true);
      }
   }
   else
   {
      m_pItemsSelect[4]->setEnabled(false);
      m_pItemsSelect[9]->setEnabled(false);
      //if ( g_pControllerSettings->iDeveloperMode )
      {
         if ( -1 != m_IndexSnapshot )
            m_pItemsSelect[13]->setEnabled(false);
         if ( -1 != m_IndexSnapshotTimeout )
            m_pItemsSelect[22]->setEnabled(false);
      }
   }

   m_pItemsSelect[35]->setSelection((g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_VIDEO_FRAMES_STATS)?1:0);
   m_pItemsSelect[3]->setSelection((g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_STATS_VIDEO)?1:0);
   m_pItemsSelect[4]->setSelectedIndex(2); // Normal
   if ( g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_MINIMAL_VIDEO_DECODE_STATS )
      m_pItemsSelect[4]->setSelectedIndex(0);
   if ( g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_COMPACT_VIDEO_DECODE_STATS )
      m_pItemsSelect[4]->setSelectedIndex(1);
   if ( g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_EXTENDED_VIDEO_DECODE_STATS )
      m_pItemsSelect[4]->setSelectedIndex(3);

   m_pItemsSelect[5]->setSelection((g_pCurrentModel->osd_params.osd_flags[iScreenIndex] & OSD_FLAG_SHOW_EFFICIENCY_STATS)?1:0);
   m_pItemsSelect[6]->setSelection((g_pCurrentModel->osd_params.osd_flags2[iScreenIndex] & OSD_FLAG2_SHOW_STATS_RC)?1:0);

   m_pItemsSelect[19]->setSelectedIndex(0);
   m_pItemsSelect[33]->setEnabled(false);
   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->osd_params.osd_flags3[iScreenIndex] & OSD_FLAG3_SHOW_VIDEO_BITRATE_HISTORY )
   {
      m_pItemsSelect[19]->setSelectedIndex(1);
      m_pItemsSelect[33]->setEnabled(true);
      if ( g_pCurrentModel->telemetry_params.iVideoBitrateHistoryGraphSampleInterval >= 500 )
         m_pItemsSelect[33]->setSelectedIndex(0);
      else if ( g_pCurrentModel->telemetry_params.iVideoBitrateHistoryGraphSampleInterval >= 200 )
         m_pItemsSelect[33]->setSelectedIndex(1);
      else if ( g_pCurrentModel->telemetry_params.iVideoBitrateHistoryGraphSampleInterval >= 100 )
         m_pItemsSelect[33]->setSelectedIndex(2);
      else if ( g_pCurrentModel->telemetry_params.iVideoBitrateHistoryGraphSampleInterval >= 50 )
         m_pItemsSelect[33]->setSelectedIndex(3);
   }

   if ( g_pControllerSettings->iDeveloperMode )
   {
      m_pItemsSelect[20]->setSelectedIndex(0);
      if ( NULL != g_pCurrentModel )
      if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_TX_GAP )
         m_pItemsSelect[20]->setSelectedIndex(1);

      m_pItemsSelect[10]->setSelectedIndex(pP->iDebugShowDevVideoStats);
      m_pItemsSelect[11]->setSelectedIndex(pP->iDebugShowDevRadioStats);
      m_pItemsSelect[12]->setSelectedIndex(pP->iDebugShowFullRXStats);
      if ( -1 != m_IndexSnapshot )
         m_pItemsSelect[13]->setSelectedIndex(pP->iDebugShowVideoSnapshotOnDiscard);

      if ( -1 != m_IndexSnapshotTimeout )
      {
         m_pItemsSelect[22]->setSelectedIndex(g_pControllerSettings->iVideoDecodeStatsSnapshotClosesOnTimeout);
         if ( pP->iDebugShowVideoSnapshotOnDiscard )
            m_pItemsSelect[22]->setEnabled(true);
         else
            m_pItemsSelect[22]->setEnabled(false);
      }
      m_pItemsSelect[24]->setSelectedIndex( (g_pCurrentModel->osd_params.osd_flags3[iScreenIndex] & OSD_FLAG3_SHOW_CONTROLLER_ADAPTIVE_VIDEO_INFO)?1:0);
   }

   if ( g_pCurrentModel->osd_params.osd_preferences[iScreenIndex] & OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_LEFT )
      m_pItemsSelect[7]->setSelectedIndex(0);
   else if ( g_pCurrentModel->osd_params.osd_preferences[iScreenIndex] & OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_RIGHT )
      m_pItemsSelect[7]->setSelectedIndex(1);
   else if ( g_pCurrentModel->osd_params.osd_preferences[iScreenIndex] & OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_TOP )
      m_pItemsSelect[7]->setSelectedIndex(2);
   else if ( g_pCurrentModel->osd_params.osd_preferences[iScreenIndex] & OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_BOTTOM )
      m_pItemsSelect[7]->setSelectedIndex(3);
   else   
      m_pItemsSelect[7]->setSelectedIndex(0);

   if ( -1 != m_IndexVehicleDevStats )
   if ( g_pControllerSettings->iDeveloperMode )
   {
      m_pItemsSelect[34]->setSelectedIndex(0);
      if ( g_pCurrentModel->osd_params.osd_flags3[iScreenIndex] & OSD_FLAG3_SHOW_VEHICLE_DEV_STATS )
         m_pItemsSelect[34]->setSelectedIndex(1);
   }
}

void MenuVehicleOSDStats::onShow()
{
   int iTemp = m_SelectedIndex;
   Menu::onShow();
   valuesToUI();
   if ( iTemp >= 0 )
      m_SelectedIndex = iTemp;
}

void MenuVehicleOSDStats::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuVehicleOSDStats::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   Preferences* pP = get_Preferences();
   bool sendToVehicle = false;
   osd_parameters_t params;
   memcpy(&params, &(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));
   int iScreenIndex = g_pCurrentModel->osd_params.iCurrentOSDScreen;

   g_bChangedOSDStatsFontSize = false;
   
   if ( m_IndexFontSize == m_SelectedIndex )
   {
      params.osd_preferences[iScreenIndex] &= ~(0x0F0000);
      params.osd_preferences[iScreenIndex] |= ((u32)m_pItemsSelect[0]->getSelectedIndex())<<16;
      sendToVehicle = true;
      g_bChangedOSDStatsFontSize = true;
   }

   if ( m_IndexTransparency == m_SelectedIndex )
   {
      params.osd_preferences[iScreenIndex] &= ~(0xF00000);
      params.osd_preferences[iScreenIndex] |= ((u32)m_pItemsSelect[21]->getSelectedIndex())<<20;
      sendToVehicle = true;
   }

   if ( m_IndexPanelsDirection == m_SelectedIndex )
   {
      params.osd_preferences[iScreenIndex] &= ~OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_LEFT;
      params.osd_preferences[iScreenIndex] &= ~OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_RIGHT;
      params.osd_preferences[iScreenIndex] &= ~OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_TOP;
      params.osd_preferences[iScreenIndex] &= ~OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_BOTTOM;
      if ( 0 == m_pItemsSelect[7]->getSelectedIndex() )
         params.osd_preferences[iScreenIndex] |= OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_LEFT;
      else if ( 1 == m_pItemsSelect[7]->getSelectedIndex() )
         params.osd_preferences[iScreenIndex] |= OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_RIGHT;
      else if ( 2 == m_pItemsSelect[7]->getSelectedIndex() )
         params.osd_preferences[iScreenIndex] |= OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_TOP;
      else if ( 3 == m_pItemsSelect[7]->getSelectedIndex() )
         params.osd_preferences[iScreenIndex] |= OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_BOTTOM;
      sendToVehicle = true;
   }

   if ( m_IndexFitWidgets == m_SelectedIndex )
   {
      params.osd_flags3[iScreenIndex] &= ~OSD_FLAG3_LAYOUT_STATS_AUTO_WIDGETS_MARGINS;
      if ( 1 == m_pItemsSelect[32]->getSelectedIndex() )
         params.osd_flags3[iScreenIndex] |= OSD_FLAG3_LAYOUT_STATS_AUTO_WIDGETS_MARGINS;
      sendToVehicle = true;
   }

   if ( m_IndexStatsRadioLinks == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[1]->getSelectedIndex() )
         params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_STATS_RADIO_LINKS;
      else
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_STATS_RADIO_LINKS;

      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexStatsRadioInterfaces == m_SelectedIndex )
   {
      params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_STATS_RADIO_INTERFACES;
      params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_MINIMAL_RADIO_INTERFACES_STATS;
      params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_RADIO_INTERFACES_COMPACT;
      if ( m_pItemsSelect[2]->getSelectedIndex() > 0 )
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_STATS_RADIO_INTERFACES;

      if ( 1 == m_pItemsSelect[2]->getSelectedIndex() )
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_MINIMAL_RADIO_INTERFACES_STATS;
      if ( 2 == m_pItemsSelect[2]->getSelectedIndex() )
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_RADIO_INTERFACES_COMPACT;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexVehicleRadioRxStats == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[17]->getSelectedIndex() )
         params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_VEHICLE_RADIO_INTERFACES_STATS;
      else
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_VEHICLE_RADIO_INTERFACES_STATS;

      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;    
   }

   if ( m_IndexRadioRefreshInterval == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[8]->getSelectedIndex() )
         params.iRadioInterfacesGraphRefreshIntervalMs = 10;
      if ( 1 == m_pItemsSelect[8]->getSelectedIndex() )
         params.iRadioInterfacesGraphRefreshIntervalMs = 20;
      if ( 2 == m_pItemsSelect[8]->getSelectedIndex() )
         params.iRadioInterfacesGraphRefreshIntervalMs = 50;
      if ( 3 == m_pItemsSelect[8]->getSelectedIndex() )
         params.iRadioInterfacesGraphRefreshIntervalMs = 100;
      if ( 4 == m_pItemsSelect[8]->getSelectedIndex() )
         params.iRadioInterfacesGraphRefreshIntervalMs = 200;
      if ( 5 == m_pItemsSelect[8]->getSelectedIndex() )
         params.iRadioInterfacesGraphRefreshIntervalMs = 500;
      sendToVehicle = true;
   }

   if ( m_IndexRadioRxHistoryController == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[27]->getSelectedIndex() )
         params.osd_flags3[iScreenIndex] &= ~OSD_FLAG3_SHOW_RADIO_RX_HISTORY_CONTROLLER;
      else
         params.osd_flags3[iScreenIndex] |= OSD_FLAG3_SHOW_RADIO_RX_HISTORY_CONTROLLER;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexRadioRxHistoryVehicle == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[28]->getSelectedIndex() )
         params.osd_flags3[iScreenIndex] &= ~OSD_FLAG3_SHOW_RADIO_RX_HISTORY_VEHICLE;
      else
         params.osd_flags3[iScreenIndex] |= OSD_FLAG3_SHOW_RADIO_RX_HISTORY_VEHICLE;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexRadioRxHistoryControllerBig == m_SelectedIndex )
   {
      int index = m_pItemsSelect[29]->getSelectedIndex();
      g_pControllerSettings->uShowBigRxHistoryInterface &= 0xFFFF0000;
      if ( index <= hardware_get_radio_interfaces_count() )
         g_pControllerSettings->uShowBigRxHistoryInterface |= (1<<(index-1));
      else
      {
         g_pControllerSettings->uShowBigRxHistoryInterface |= (1);
         g_pControllerSettings->uShowBigRxHistoryInterface |= (1<<(index-hardware_get_radio_interfaces_count()));
      }
      save_ControllerSettings();
      valuesToUI();
      return;
   }

   if ( m_IndexRadioRxHistoryVehicleBig == m_SelectedIndex )
   {
      int index = m_pItemsSelect[30]->getSelectedIndex();
      g_pControllerSettings->uShowBigRxHistoryInterface &= 0x0000FFFF;
      if ( index <= g_pCurrentModel->radioInterfacesParams.interfaces_count )
         g_pControllerSettings->uShowBigRxHistoryInterface |= ((1<<(index-1))<<16);
      else
      {
         g_pControllerSettings->uShowBigRxHistoryInterface |= (1<<16);
         g_pControllerSettings->uShowBigRxHistoryInterface |= (1<<(index-g_pCurrentModel->radioInterfacesParams.interfaces_count))<<16;
      }
      
      save_ControllerSettings();
      valuesToUI();
      return;
   }

   if ( (-1 != m_IndexRadioRxDbgPingStats) && (m_IndexRadioRxDbgPingStats == m_SelectedIndex) )
   {
      g_pControllerSettings->iDbgPingGraphs = m_pItemsSelect[31]->getSelectedIndex();
      save_ControllerSettings();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      return;      
   }

   if ( m_IndexVideoRefreshInterval == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[9]->getSelectedIndex() )
         g_pControllerSettings->nGraphVideoRefreshInterval = 10;
      if ( 1 == m_pItemsSelect[9]->getSelectedIndex() )
         g_pControllerSettings->nGraphVideoRefreshInterval = 20;
      if ( 2 == m_pItemsSelect[9]->getSelectedIndex() )
         g_pControllerSettings->nGraphVideoRefreshInterval = 50;
      if ( 3 == m_pItemsSelect[9]->getSelectedIndex() )
         g_pControllerSettings->nGraphVideoRefreshInterval = 100;
      if ( 4 == m_pItemsSelect[9]->getSelectedIndex() )
         g_pControllerSettings->nGraphVideoRefreshInterval = 200;
      if ( 5 == m_pItemsSelect[9]->getSelectedIndex() )
         g_pControllerSettings->nGraphVideoRefreshInterval = 500;
      save_ControllerSettings();
      invalidate();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      return;
   }

   if ( m_IndexVideoFramesStats == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[35]->getSelectedIndex() )
         params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_VIDEO_FRAMES_STATS;
      else
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_VIDEO_FRAMES_STATS;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexStatsDecode == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[3]->getSelectedIndex() )
         params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_STATS_VIDEO;
      else
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_STATS_VIDEO;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexStatsVideoExtended == m_SelectedIndex )
   {
      params.osd_flags[iScreenIndex] &= ~OSD_FLAG_EXTENDED_VIDEO_DECODE_STATS;
      params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_COMPACT_VIDEO_DECODE_STATS;
      params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_MINIMAL_VIDEO_DECODE_STATS;
      if ( 0 == m_pItemsSelect[4]->getSelectedIndex() )
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_MINIMAL_VIDEO_DECODE_STATS;
      else if ( 1 == m_pItemsSelect[4]->getSelectedIndex() )
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_COMPACT_VIDEO_DECODE_STATS;
      else if ( 3 == m_pItemsSelect[4]->getSelectedIndex() )
         params.osd_flags[iScreenIndex] |= OSD_FLAG_EXTENDED_VIDEO_DECODE_STATS;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( (-1 != m_IndexStatsVideoH264FramesInfo) && (m_IndexStatsVideoH264FramesInfo == m_SelectedIndex) )
   {
      if ( 0 == m_pItemsSelect[23]->getSelectedIndex() )
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SHOW_STATS_VIDEO_H264_FRAMES_INFO;
      else
      {
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_STATS_VIDEO_H264_FRAMES_INFO;
         //if ( 3 == m_pItemsSelect[23]->getSelectedIndex() )
         //   g_pControllerSettings->iShowVideoStreamInfoCompactType = 0;
         //else if ( 2 == m_pItemsSelect[23]->getSelectedIndex() )
         //   g_pControllerSettings->iShowVideoStreamInfoCompactType = 1;
         //else
         //   g_pControllerSettings->iShowVideoStreamInfoCompactType = 2;
         save_ControllerSettings();
      }
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;    
   }


   if ( m_IndexSnapshot == m_SelectedIndex )
   {
      pP->iDebugShowVideoSnapshotOnDiscard = m_pItemsSelect[13]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      return;
   }

   if ( m_IndexSnapshotTimeout == m_SelectedIndex )
   {
      g_pControllerSettings->iVideoDecodeStatsSnapshotClosesOnTimeout = m_pItemsSelect[22]->getSelectedIndex();
      save_ControllerSettings();
      invalidate();
      valuesToUI();
      return;
   }

   if ( m_IndexStatsEff == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[5]->getSelectedIndex() )
         params.osd_flags[iScreenIndex] &= ~OSD_FLAG_SHOW_EFFICIENCY_STATS;
      else
         params.osd_flags[iScreenIndex] |= OSD_FLAG_SHOW_EFFICIENCY_STATS;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexTelemetryStats == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[25]->getSelectedIndex() )
         params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_TELEMETRY_STATS;
      else
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_TELEMETRY_STATS;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexAudioDecodeStats == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[26]->getSelectedIndex() )
         params.osd_flags3[iScreenIndex] &= ~OSD_FLAG3_SHOW_AUDIO_DECODE_STATS;
      else
         params.osd_flags3[iScreenIndex] |= OSD_FLAG3_SHOW_AUDIO_DECODE_STATS;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexStatsRC == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[6]->getSelectedIndex() )
         params.osd_flags2[iScreenIndex] &= ~OSD_FLAG2_SHOW_STATS_RC;
      else
         params.osd_flags2[iScreenIndex] |= OSD_FLAG2_SHOW_STATS_RC;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexDevStatsVideo == m_SelectedIndex )
   {
      pP->iDebugShowDevVideoStats = m_pItemsSelect[10]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
      return;
   }

   if ( m_IndexShowControllerAdaptiveInfoStats == m_SelectedIndex )
   {
      params.osd_flags3[iScreenIndex] &= ~OSD_FLAG3_SHOW_CONTROLLER_ADAPTIVE_VIDEO_INFO;
      if ( 1 == m_pItemsSelect[24]->getSelectedIndex() )
         params.osd_flags3[iScreenIndex] |= OSD_FLAG3_SHOW_CONTROLLER_ADAPTIVE_VIDEO_INFO;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }
   
   if ( -1 != m_IndexVehicleDevStats )
   if ( g_pControllerSettings->iDeveloperMode )
   if ( m_IndexVehicleDevStats == m_SelectedIndex )
   {
      params.osd_flags3[iScreenIndex] &= ~OSD_FLAG3_SHOW_VEHICLE_DEV_STATS;
      if ( 1 == m_pItemsSelect[34]->getSelectedIndex() )
         params.osd_flags3[iScreenIndex] |= OSD_FLAG3_SHOW_VEHICLE_DEV_STATS;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexDevVehicleVideoBitrateHistory == m_SelectedIndex )
   {
      if ( NULL == g_pCurrentModel )
      {
         addMessageNeedsVehcile("You need to be connected to a vehicle to change this.", 1);
         valuesToUI();
         return;
      }

      params.osd_flags3[iScreenIndex] &= ~OSD_FLAG3_SHOW_VIDEO_BITRATE_HISTORY;
      if ( 1 == m_pItemsSelect[19]->getSelectedIndex() )
         params.osd_flags3[iScreenIndex] |= OSD_FLAG3_SHOW_VIDEO_BITRATE_HISTORY;
      params.osd_layout_preset[iScreenIndex] = OSD_PRESET_CUSTOM;
      sendToVehicle = true;
   }

   if ( m_IndexRefreshIntervalVideoBitrateHistory == m_SelectedIndex )
   {
      telemetry_parameters_t params;
      memcpy(&params, &g_pCurrentModel->telemetry_params, sizeof(telemetry_parameters_t));
   
      int iIndex = m_pItemsSelect[33]->getSelectedIndex();
      if ( 0 == iIndex )
         params.iVideoBitrateHistoryGraphSampleInterval = 500;
      if ( 1 == iIndex )
         params.iVideoBitrateHistoryGraphSampleInterval = 200;
      if ( 2 == iIndex )
         params.iVideoBitrateHistoryGraphSampleInterval = 100;
      if ( 3 == iIndex )
         params.iVideoBitrateHistoryGraphSampleInterval = 50;
  
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_TELEMETRY_PARAMETERS, 0, (u8*)&params, sizeof(telemetry_parameters_t)) )
         valuesToUI();
      return;
   }

   if ( m_IndexDevStatsVehicleTx == m_SelectedIndex )
   {
      if ( NULL == g_pCurrentModel )
      {
         addMessageNeedsVehcile("You need to be connected to a vehicle to change this.", 1);
         valuesToUI();
         return;
      }

      if ( 0 == m_pItemsSelect[20]->getSelectedIndex() )
         g_pCurrentModel->uDeveloperFlags &= (~DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_TX_GAP);
      else
         g_pCurrentModel->uDeveloperFlags |= DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_TX_GAP;
      if ( ! handle_commands_send_developer_flags(g_pCurrentModel->uDeveloperFlags) )
         valuesToUI();  
      return;    
   }


   if ( m_IndexDevStatsRadio == m_SelectedIndex )
   {
      pP->iDebugShowDevRadioStats = m_pItemsSelect[11]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
      return;
   }

   if ( m_IndexDevFullRXStats == m_SelectedIndex )
   {
      pP->iDebugShowFullRXStats = m_pItemsSelect[12]->getSelectedIndex();
      save_Preferences();
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
   }
}
