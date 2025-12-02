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
#include "menu_text.h"
#include "menu_system_all_params.h"
#include "../../radio/radiolink.h"
#include "../launchers_controller.h"


MenuSystemAllParams::MenuSystemAllParams(void)
:Menu(MENU_ID_SYSTEM_ALL_PARAMS, "All System Parameters (controller + vehicle)", NULL)
{
   m_Width = 0.8;
   m_Height = 0.91;
   m_xPos = 0.12;
   m_yPos = 0.05;
   m_bGotIP = false;
   m_szIP[0] = 0;
}

void MenuSystemAllParams::onShow()
{
   Menu::onShow();

   ControllerSettings* pCS = get_ControllerSettings();
   if ( NULL != pCS && (0 != pCS->iDeveloperMode) )
      setTitle("All System Parameters (controller + vehicle), Developer Mode");
}


void MenuSystemAllParams::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   FILE* fd = NULL;
   char szBuff[256];
   char szFile[256];
   char szOutput[1024];

   u32 uBoardType = 0;

   float height_text = g_pRenderEngine->textHeight(g_idFontMenuSmall);   

   float xPos = m_xPos+m_sfMenuPaddingX;
   float yPos = y;
   float width = m_RenderWidth-2*m_sfMenuPaddingX;
   float height = m_RenderHeight-2*m_sfMenuPaddingY;

   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_CONFIG_BOARD_TYPE);
   fd = fopen(szFile,"r");
   if ( NULL != fd )
   {
      fscanf(fd,"%u", &uBoardType);
      fclose(fd);
   }
         
   sprintf(szBuff, "Controller board: %s, no vehicle selected.", str_get_hardware_board_name(uBoardType));
   if ( NULL != g_pCurrentModel )
      sprintf(szBuff, "Controller board: %s, vehicle board: %s, vehicle name: %s, %s", str_get_hardware_board_name(uBoardType), str_get_hardware_board_name(g_pCurrentModel->hwCapabilities.uBoardType), g_pCurrentModel->getLongName(), g_pCurrentModel->is_spectator?"(Spectator Mode)":"(Control Mode)");

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText(), 0.3);
   g_pRenderEngine->drawText(xPos, yPos, g_idFontMenuSmall, szBuff);
   yPos += 1.9*height_text;
   g_pRenderEngine->setColors(get_Color_MenuText());


   float yPosCol2 = yPos;

   if ( ! m_bGotIP )
   {
      m_szIP[0] = 0;
      if ( uBoardType != BOARD_TYPE_PIZERO && uBoardType != BOARD_TYPE_PIZEROW && uBoardType != BOARD_TYPE_PI3APLUS )
      {
         szOutput[0] = 0;
         //hw_execute_bash_command_raw("ifconfig | grep -A1 eth | head -2 | tail -1", szOutput);
         hw_execute_bash_command_raw("ip addr | grep eth | grep inet", szOutput);
         log_line("Out: [%s]", szOutput);
         
         int cSpaces = 0;
         int cOther = 0;
         for( int i=0; i<(int)strlen(szOutput); i++ )
         {
            if ( szOutput[i] != ' ' )
               cOther++;

            if ( szOutput[i] == ' ' )
            if ( cOther )
               cSpaces++;
            if ( szOutput[i] == 10 || szOutput[i] == 13 || cSpaces >= 2 )
            {
               szOutput[i] = 0;
               break;
            }
         }
         log_line("Out: [%s]", szOutput);
         char* p = &(szOutput[0]);
         if ( NULL != strstr(szOutput, "inet") )
         {
            p = strstr(szOutput, "inet");
            p += 5;
         }

         while ( (*p == ' ') && (*p != 0) )
            p++;
         if ( (*p != 0) && (strlen(p)>4) )
            strcpy(m_szIP, p);
         m_bGotIP = true;
      }
      else
         strcpy(m_szIP, "No ETH");
      m_bGotIP = true;
   }
   snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Controller IP: %s", m_szIP);
   g_pRenderEngine->drawText(xPos, yPos, g_idFontMenuSmall, szBuff);
   yPos += 2.0*height_text;

   yPos += renderRadioInfo(xPos, yPos, width, height, 0.9);
   yPos += renderDataRates(xPos, yPos, width, 0.8);
   yPos += renderProcesses(xPos, yPos, width, 0.8);
   yPos += renderSoftware(xPos, yPos, width, 0.8);


   float fScale = 0.86;
   float dxCol = 0.35*m_sfScaleFactor;

   g_pRenderEngine->drawLine(xPos+dxCol - 0.02*m_sfScaleFactor, yPosCol2, xPos+dxCol - 0.02*m_sfScaleFactor, m_RenderHeight-(yPosCol2-m_yPos+0.04*m_sfScaleFactor));
   yPosCol2 += renderVehicleCamera(xPos + dxCol, yPosCol2, width - dxCol, fScale);
   yPosCol2 += renderVehicleRC(xPos + dxCol, yPosCol2, width - dxCol, fScale);
   yPosCol2 += renderControllerParams(xPos + dxCol, yPosCol2, width - dxCol, fScale);
   yPosCol2 += renderCPUParams(xPos + dxCol, yPosCol2, width - dxCol, fScale);
   yPosCol2 += renderDeveloperFlags(xPos + dxCol, yPosCol2, width - dxCol, fScale);
}

float MenuSystemAllParams::renderVehicleCamera(float xPos, float yPos, float width, float fScale)
{
   if ( NULL == g_pCurrentModel )
      return 0.0;
   float y0 = yPos;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenuSmall);
   
   char szCamera[256];
   char szVideo[256];
   char szBuff[1024];
   char szTemp[1024];

   g_pCurrentModel->getCameraFlags(szCamera);
   g_pCurrentModel->getVideoFlags(szVideo, g_pCurrentModel->video_params.iCurrentVideoProfile, 0,0);
   sprintf(szBuff, "Vehicle camera flags: %s", szCamera );

   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenuSmall);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   sprintf(szBuff, "Vehicle video flags: %s", szVideo );

   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenuSmall);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   sprintf(szBuff, "EC: %d/%d/%d", g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iBlockDataPackets, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iBlockECs, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].video_data_length);

   g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenuSmall);

   strcpy(szTemp, str_get_video_profile_name(g_pCurrentModel->video_params.iCurrentVideoProfile));

   snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "S: %s/%d/%d/%d", szTemp, 
              (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS)?1:0,
              0,
              (((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags)>>8)&0xFF)*5);
   yPos += g_pRenderEngine->drawMessageLines(xPos+ 0.16*m_sfScaleFactor, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenuSmall);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "H264: %d/%d/%d", g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].h264quantization, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].h264profile, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].h264level);
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenuSmall);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   g_pRenderEngine->setColors(get_Color_MenuText());

   yPos += 2.0* MENU_TEXTLINE_SPACING * height_text;
   return yPos-y0;
}

float MenuSystemAllParams::renderVehicleRC(float xPos, float yPos, float width, float fScale)
{
   if ( NULL == g_pCurrentModel )
      return 0.0;
   float y0 = yPos;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenuSmall);

   char szBuff[1024];

   sprintf(szBuff, "RC: %s, ch %d / fps %d / output enabled: %d / fs %d ms", g_pCurrentModel->rc_params.rc_enabled?"Enabled":"Disabled", g_pCurrentModel->rc_params.channelsCount, g_pCurrentModel->rc_params.rc_frames_per_second, (g_pCurrentModel->rc_params.flags & RC_FLAGS_OUTPUT_ENABLED), g_pCurrentModel->rc_params.rc_failsafe_timeout_ms);
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenuSmall);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   sprintf(szBuff, "RC Failsafe Type: %d, (value: %d)", (g_pCurrentModel->rc_params.failsafeFlags & 0xFF), ((g_pCurrentModel->rc_params.failsafeFlags >> 8) & 0xFFFF));
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenuSmall);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   bool bSI = false;
   if ( g_pCurrentModel->osd_params.show_stats_rc || (g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.iCurrentOSDScreen] & OSD_FLAG_SHOW_HID_IN_OSD) )
      bSI = true;
   sprintf(szBuff, "RC: send info back: %s, show on OSD: %d/%d", bSI?"Yes":"No", g_pCurrentModel->osd_params.show_stats_rc, (g_pCurrentModel->osd_params.osd_flags[0] & OSD_FLAG_SHOW_HID_IN_OSD));
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenuSmall);
   yPos += 2.0*MENU_TEXTLINE_SPACING * height_text;

   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, "Telemetry", MENU_TEXTLINE_SPACING, width, g_idFontMenuSmall);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   char szTelemetryType[64];
   sprintf(szTelemetryType, "%d", g_pCurrentModel->telemetry_params.fc_telemetry_type);
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_NONE )
      strcpy(szTelemetryType, "None");
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MAVLINK )
      strcpy(szTelemetryType, "MAVLink");
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_LTM )
      strcpy(szTelemetryType, "LTM");
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_TRANSPARENT )
      strcpy(szTelemetryType, "Transp.");
   sprintf(szBuff, "  %s / rate %d", szTelemetryType, g_pCurrentModel->telemetry_params.iUpdateRateHz);

   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenuSmall);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   if ( NULL != g_pCurrentModel )
   {
      sprintf(szBuff, "  Vehicle MAVLink Id: %d, Controller MAVLink Id: %d", g_pCurrentModel->telemetry_params.vehicle_mavlink_id, g_pCurrentModel->telemetry_params.controller_mavlink_id );
      yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenuSmall);
      yPos += MENU_TEXTLINE_SPACING * height_text;
   }

   g_pRenderEngine->setColors(get_Color_MenuText());

   yPos += 2.0 * MENU_TEXTLINE_SPACING * height_text;
   return yPos - y0;
}

float MenuSystemAllParams::renderRadioInfo(float xPos, float yPos, float width, float height, float fScale)
{
   char szBuff[256];
   float height_text = g_pRenderEngine->textHeight(g_idFontMenuSmall);   

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText(), 0.6);
   g_pRenderEngine->drawText(xPos, yPos, g_idFontMenuSmall, "Controller Radio Interfaces:");
   yPos += 2.0*height_text;

   g_pRenderEngine->setColors(get_Color_MenuText());

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      sprintf(szBuff, "Radio Interface %d: USB port %s: %s, driver: %s", i+1, pNICInfo->szUSBPort, pNICInfo->szDescription, str_get_radio_driver_description(pNICInfo->iRadioDriver));
      g_pRenderEngine->drawText(xPos, yPos, g_idFontMenuSmall, szBuff);
      yPos += 1.4*height_text;

      if ( pNICInfo->lastFrequencySetFailed )
         sprintf(szBuff, "   Now at: 0 Mhz, bands:");
      else
         sprintf(szBuff, "   Now at: %s, bands:", str_format_frequency(pNICInfo->uCurrentFrequencyKhz));
      if ( pNICInfo->supportedBands & 1 )
         strcat(szBuff, "2.3, ");
      if ( pNICInfo->supportedBands & 2 )
         strcat(szBuff, "2.4, ");
      if ( pNICInfo->supportedBands & 4 )
         strcat(szBuff, "2.5, ");
      if ( pNICInfo->supportedBands & 8 )
         strcat(szBuff, "5.8, ");
      
      g_pRenderEngine->drawText(xPos, yPos, g_idFontMenuSmall, szBuff);
      yPos += 1.4*height_text;

      sprintf(szBuff, "   Flags: ");
      if ( controllerIsCardTXOnly(pNICInfo->szMAC) )
         strcat(szBuff, "TX Only, ");
      else if ( controllerIsCardRXOnly(pNICInfo->szMAC) )
         strcat(szBuff, "RX Only, ");
      else
         strcat(szBuff, "TX/RX, ");
      
      if ( controllerIsCardDisabled(pNICInfo->szMAC) )
         strcat(szBuff, "Disabled");
      else
         strcat(szBuff, "Enabled");

      if ( pNICInfo->openedForWrite )
         strcat(szBuff, ", [Current TX Card]");
      if ( controllerIsCardTXPreferred(pNICInfo->szMAC) )
         strcat(szBuff, ", [Preffered TX Card]");
      g_pRenderEngine->drawText(xPos, yPos, g_idFontMenuSmall, szBuff);
      yPos += 1.8*height_text;
   }

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText(), 0.6);

   g_pRenderEngine->drawText(xPos, yPos, g_idFontMenuSmall, "Vehicle Radio Interfaces:");
   yPos += 2.0*height_text;

   g_pRenderEngine->setColors(get_Color_MenuText());

   if ( NULL == g_pCurrentModel )
   {
      g_pRenderEngine->drawText(xPos, yPos, g_idFontMenuSmall, "No vehicle selected.");
      yPos += 1.4*height_text;
   }
   else
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         {
            sprintf(szBuff, "Radio Interface %d: USB port %s,  %s, driver %s", i+1, g_pCurrentModel->radioInterfacesParams.interface_szPort[i], str_get_radio_type_description(g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[i]), str_get_radio_driver_description(g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[i]));
            g_pRenderEngine->drawText(xPos, yPos, g_idFontMenuSmall, szBuff);
            yPos += 1.4*height_text;
            sprintf(szBuff, "   Now at %s, bands: ", str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[i]));
            if ( g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_433 )
               strcat(szBuff, "433 ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_868 )
               strcat(szBuff, "868 ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_915 )
               strcat(szBuff, "915 ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_23 )
               strcat(szBuff, "2.3 ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_24 )
               strcat(szBuff, "2.4 ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_25 )
               strcat(szBuff, "2.5 ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_58 )
               strcat(szBuff, "5.8 ");
            g_pRenderEngine->drawText(xPos, yPos, g_idFontMenuSmall, szBuff);
            yPos += 1.4*height_text;

            sprintf(szBuff, "   Flags: ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
               strcat(szBuff, "TX/RX, ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
            if ( !( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
               strcat(szBuff, "TX Only, ");
            if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
               strcat(szBuff, "RX Only, ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
               strcat(szBuff, "Video, ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
               strcat(szBuff, "Data, ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
               strcat(szBuff, "Relay");

            g_pRenderEngine->drawText(xPos, yPos, g_idFontMenuSmall, szBuff);
            yPos += 1.8*height_text;
         }

   return 5.0*height_text + (1.8*height_text+ 2*1.4*height_text)*(2+3);
}

float MenuSystemAllParams::renderDataRates(float xPos, float yPos, float width, float fScale)
{
   float y0 = yPos;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenuSmall);

   char szBuff[1024];

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText(), 0.6);
   g_pRenderEngine->drawText(xPos, yPos, g_idFontMenuSmall, "Radio data rates and power:");
   yPos += 2.0*height_text;

   g_pRenderEngine->setColors(get_Color_MenuText());

   sprintf(szBuff, "Controller TX power: N/A");

   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenuSmall);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   if ( NULL == g_pCurrentModel )
      return 0.0;

   sprintf(szBuff, "Vehicle TX power (RTL8812AU): %d", g_pCurrentModel->radioInterfacesParams.interface_raw_power[0]);

   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenuSmall);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   g_pRenderEngine->setColors(get_Color_MenuText());
   return yPos - y0;
}

float MenuSystemAllParams::renderProcesses(float xPos, float yPos, float width, float fScale)
{
   ControllerSettings* pCS = get_ControllerSettings();
   
   float y0 = yPos;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenuSmall);

   char szBuff[1024];
   char szBuff2[1024];

   yPos += MENU_TEXTLINE_SPACING * height_text;

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText(), 0.6);
   g_pRenderEngine->drawText(xPos, yPos, g_idFontMenuSmall, "Processes Priorities:");

   g_pRenderEngine->setColors(get_Color_MenuText());

   snprintf(szBuff, 1023, "Controller router/video/UI: %d/%d/%d", pCS->iThreadPriorityRouter, pCS->iThreadPriorityVideo, -pCS->iThreadPriorityCentral);
   if ( NULL == g_pCurrentModel )
      strncpy(szBuff2, "No vehicle", 1023);
   else
      snprintf(szBuff2, 1023, "Vehicle router/video/telemetry/RC: %d/%d/%d/%d", g_pCurrentModel->processesPriorities.iThreadPriorityRouter, g_pCurrentModel->processesPriorities.iThreadPriorityVideoCapture, g_pCurrentModel->processesPriorities.iThreadPriorityTelemetry, g_pCurrentModel->processesPriorities.iThreadPriorityRC);

   g_pRenderEngine->drawMessageLines(xPos+0.16*m_sfScaleFactor, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenuSmall);
   yPos += g_pRenderEngine->drawMessageLines(xPos+0.41*m_sfScaleFactor, yPos, szBuff2, MENU_TEXTLINE_SPACING, width, g_idFontMenuSmall);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   g_pRenderEngine->setColors(get_Color_MenuText());
   return yPos - y0;
}


float MenuSystemAllParams::renderSoftware(float xPos, float yPos, float width, float fScale)
{   
   float y0 = yPos;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenuSmall);

   char szBuff[64];
   char szBuff2[64];

   yPos += MENU_TEXTLINE_SPACING * height_text;

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText(), 0.6);
   g_pRenderEngine->drawText(xPos, yPos, g_idFontMenuSmall, "Software:");

   g_pRenderEngine->setColors(get_Color_MenuText());

   snprintf(szBuff, 63, "Controller version %d.%d (b-%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR, SYSTEM_SW_BUILD_NUMBER);
   if ( NULL == g_pCurrentModel )
      strncpy(szBuff2, "No vehicle", 63);
   else
      snprintf(szBuff2, 63, "Vehicle version: %d.%d (b-%d)", get_sw_version_major(g_pCurrentModel), get_sw_version_minor(g_pCurrentModel), get_sw_version_build(g_pCurrentModel));

   g_pRenderEngine->drawMessageLines(xPos+0.16*m_sfScaleFactor, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenuSmall);
   yPos += g_pRenderEngine->drawMessageLines(xPos+0.32*m_sfScaleFactor, yPos, szBuff2, MENU_TEXTLINE_SPACING, width, g_idFontMenuSmall);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   g_pRenderEngine->setColors(get_Color_MenuText());
   return yPos - y0;
}

float MenuSystemAllParams::renderDeveloperFlags(float xPos, float yPos, float width, float fScale)
{
   float y0 = yPos;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenuSmall);   

   char szBuff[1024];

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText(), 0.6);
   g_pRenderEngine->drawText(xPos, yPos, g_idFontMenuSmall, "Developer Flags:");
   yPos += 2.0*height_text;

   g_pRenderEngine->setColors(get_Color_MenuText());

   if ( NULL != g_pCurrentModel )
      sprintf(szBuff, "LiveLog: %d, RSFS: %d, RTW: %d ms", (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LIVE_LOG)?1:0, (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_RADIO_SILENCE_FAILSAFE)?1:0, (( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & 0xFF00 ) >> 8 ) * 5);
   else
      sprintf(szBuff, "No vehicle selected.");
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenuSmall);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   if ( NULL != g_pCurrentModel )
   {
      strcpy(szBuff, "Set_RxTx_Sync_Type Sync Type: ");
      if ( g_pCurrentModel->rxtx_sync_type == RXTX_SYNC_TYPE_NONE )
         strcat(szBuff, "None");
      if ( g_pCurrentModel->rxtx_sync_type == RXTX_SYNC_TYPE_BASIC )
         strcat(szBuff, "Basic");
      if ( g_pCurrentModel->rxtx_sync_type == RXTX_SYNC_TYPE_ADV )
         strcat(szBuff, "Advanced");
      yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenuSmall);
      yPos += MENU_TEXTLINE_SPACING * height_text;
   }
   g_pRenderEngine->setColors(get_Color_MenuText());
   return yPos - y0;
}

float MenuSystemAllParams::renderControllerParams(float xPos, float yPos, float width, float fScale)
{
   ControllerSettings* pCS = get_ControllerSettings();
   
   float y0 = yPos;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenuSmall);

   char szBuff[1024];

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText(), 0.6);
   g_pRenderEngine->drawText(xPos, yPos, g_idFontMenuSmall, "Controller Params:");
   yPos += 2.0*height_text;

   g_pRenderEngine->setColors(get_Color_MenuText());

   sprintf(szBuff, "USB Tethering: Video: %s", pCS->iVideoForwardUSBType?"yes":"no");
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenuSmall);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   g_pRenderEngine->setColors(get_Color_MenuText());
   return yPos - y0;
}

float MenuSystemAllParams::renderCPUParams(float xPos, float yPos, float width, float fScale)
{
   ControllerSettings* pCS = get_ControllerSettings();
   float y0 = yPos;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenuSmall);

   char szBuff[1024];

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText(), 0.6);
   g_pRenderEngine->drawText(xPos, yPos, g_idFontMenuSmall, "CPU Params:");
   yPos += 2.0*height_text;

   g_pRenderEngine->setColors(get_Color_MenuText());

   sprintf(szBuff, "Controller: CPU: %d Mhz, GPU: %d Mhz, OverVoltage: %d, IOPriority: %d, %d", pCS->iFreqARM, pCS->iFreqGPU, pCS->iOverVoltage, pCS->ioNiceRouter, pCS->ioNiceRxVideo);
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenuSmall);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   if ( NULL != g_pCurrentModel )
      sprintf(szBuff, "Vehicle: CPU: %d Mhz, GPU: %d Mhz, OverVoltage: %d, IOPriority: %d, %d", g_pCurrentModel->processesPriorities.iFreqARM, g_pCurrentModel->processesPriorities.iFreqGPU, g_pCurrentModel->processesPriorities.iOverVoltage, g_pCurrentModel->processesPriorities.ioNiceRouter, g_pCurrentModel->processesPriorities.ioNiceVideo);
   else
      sprintf(szBuff, "Vehicle: None");
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, MENU_TEXTLINE_SPACING, width, g_idFontMenuSmall);
   yPos += MENU_TEXTLINE_SPACING * height_text;

   g_pRenderEngine->setColors(get_Color_MenuText());
   return yPos - y0;
}

void MenuSystemAllParams::onSelectItem()
{
   Menu::onSelectItem();
}

