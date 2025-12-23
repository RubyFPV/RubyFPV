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

#include "../base/base.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../common/string_utils.h"
#include "ui_alarms.h"
#include "colors.h"
#include "osd/osd.h"
#include "osd/osd_common.h"
#include "osd/osd_stats_radio.h"
#include "popup.h"
#include "link_watch.h"
#include "shared_vars.h"
#include "timers.h"
#include "warnings.h"
#include "menu/menu.h"
#include "menu/menu_confirmation_delete_logs.h"

u32 g_uPersistentAllAlarmsVehicle = 0;
u32 g_uPersistentAllAlarmsLocal = 0;
u32 g_uTotalLocalAlarmDevRetransmissions = 0;

u32 s_TimeLastVehicleAlarm = 0;
u16 s_uLastVehicleAlarms = 0;

u32 s_TimeLastLocalAlarm = 0;
u16 s_uLastLocalAlarms = 0;

u32 s_TimeLastCPUOverloadAlarmVehicle = 0;

bool s_bAlarmVehicleLowSpaceMenuShown = false;
bool s_bAlarmWrongOpenIPCKey = false;
u32 s_uTimeLastAlarmWrongOpenIPCKey = 0;

#define MAX_POPUP_ALARMS 5
Popup* s_pPopupAlarms[MAX_POPUP_ALARMS];
// 0 is on top, count or MAX_POPUP_ALARMS is on bottom

void alarms_reset()
{
   for( int i=0; i<MAX_POPUP_ALARMS; i++ )
      s_pPopupAlarms[i] = NULL;
}

void alarms_reset_vehicle()
{
   s_bAlarmVehicleLowSpaceMenuShown = false;
}

void alarms_remove_all()
{
   s_bAlarmWrongOpenIPCKey = false;
   s_uTimeLastAlarmWrongOpenIPCKey = 0;
   for( int i=0; i<MAX_POPUP_ALARMS; i++ )
   {
      if ( popups_has_popup(s_pPopupAlarms[i]) )
         popups_remove(s_pPopupAlarms[i]);
      s_pPopupAlarms[i] = NULL;
   }
}

void alarms_render()
{
}

Popup* _get_next_available_alarm_popup(const char* szTitle, int timeout)
{
   float dY = 0.06 + g_pRenderEngine->textHeight(g_idFontMenuLarge);

   int iPosFree = -1;
   for( int i=0; i<MAX_POPUP_ALARMS; i++ )
   {
      if ( ! popups_has_popup(s_pPopupAlarms[i]) )
      {
         iPosFree = i;
         break;
      }
   }
   
   // No more room? remove one
   if ( iPosFree == -1 )
   {
      popups_remove(s_pPopupAlarms[0]);

      for( int i=0; i<MAX_POPUP_ALARMS-1; i++ )
         s_pPopupAlarms[i] = s_pPopupAlarms[i+1];

      s_pPopupAlarms[MAX_POPUP_ALARMS-1] = NULL;
      iPosFree = MAX_POPUP_ALARMS-1;
   }

   // Move all up on screen
   for( int i=0; i<iPosFree; i++ )
   {
      float fY = s_pPopupAlarms[i]->getBottomMargin();
      s_pPopupAlarms[i]->setBottomMargin(fY+dY);
   }

   Popup* pNew = new Popup(true, szTitle, timeout);
   pNew->setBottomAlign(true);
   pNew->setMaxWidth(0.76);
   pNew->setBottomMargin(0.0);

   s_pPopupAlarms[iPosFree] = pNew;   
   log_line("Add alarm popup on position %d", iPosFree);

   return pNew;
}

bool _alarms_must_skip(u32 uVehicleId, u32 uAlarms)
{
   Preferences* pP = get_Preferences();
   ControllerSettings* pCS = get_ControllerSettings();
   if ( g_bUpdateInProgress )
      return true;
  
   if ( pP->iDebugShowFullRXStats )
      return true;

   if ( ! (uAlarms & ALARM_ID_VEHICLE_LOW_STORAGE_SPACE) )
   if ( ! (pP->uEnabledAlarms & uAlarms) )
   {
      return true;
   }

   if ( uAlarms & ALARM_ID_DEVELOPER_ALARM )
   if ( ! pCS->iDeveloperMode )
      return false;

   return false;
}

void alarms_add_from_vehicle(u32 uVehicleId, u32 uAlarms, u32 uFlags1, u32 uFlags2)
{
   g_uPersistentAllAlarmsVehicle |= uAlarms;

   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* p = get_Preferences();
   if ( p->iDebugShowFullRXStats )
      return;

   char szAlarmPrefix[128];

   int iCountVehicles = 0;
   int iRuntimeIndex = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
         iRuntimeIndex = i;

      if ( g_VehiclesRuntimeInfo[i].uVehicleId != 0 )
         iCountVehicles++;
   }

   szAlarmPrefix[0] = 0;

   if ( (iRuntimeIndex != -1) && (iCountVehicles > 1) && (NULL != g_VehiclesRuntimeInfo[iRuntimeIndex].pModel) )
      sprintf(szAlarmPrefix, "%s:", g_VehiclesRuntimeInfo[iRuntimeIndex].pModel->getShortName());
   else if ( NULL != g_pCurrentModel )
      sprintf(szAlarmPrefix, "%s:", g_pCurrentModel->getShortName());

   bool bShowAsWarning = false;
   u32 uIconId = g_idIconAlarm;
   char szAlarmText[256];
   char szAlarmText2[256];
   char szAlarmText3[256];
   int iAlarmTimeoutSeconds = 7;

   char szAlarmDesc[2048];
   alarms_to_string(uAlarms, uFlags1, uFlags2, szAlarmDesc);
   snprintf(szAlarmText, sizeof(szAlarmText)/sizeof(szAlarmText[0]), "%s Generated alarm (%s)", szAlarmPrefix, szAlarmDesc);
   szAlarmText2[0] = 0;
   szAlarmText3[0] = 0;

   if ( _alarms_must_skip(uVehicleId, uAlarms) )
   {
      log_line(szAlarmText);
      log_line("This alarm from vehicle must be skipped. Do not show it in UI.");
      return;
   }

   if ( uAlarms & ALARM_ID_GENERIC )
   {
      if ( uFlags1 == ALARM_ID_GENERIC_TYPE_SWAP_RADIO_INTERFACES_NOT_POSSIBLE )
      {
         uIconId = g_idIconRadio;
         strcpy(szAlarmText, "Swapping vehicle's radio interfaces not possible with current radio configuration.");
      }
      if ( uFlags1 == ALARM_ID_GENERIC_TYPE_SWAP_RADIO_INTERFACES_FAILED )
      {
         uIconId = g_idIconRadio;
         strcpy(szAlarmText, "Swapping vehicle's radio interfaces faile");
      }
      if ( uFlags1 == ALARM_ID_GENERIC_TYPE_SWAPPED_RADIO_INTERFACES )
      {
         uIconId = g_idIconRadio;
         strcpy(szAlarmText, "Swapped vehicle's radio interfaces");
      }
      if ( uFlags1 == ALARM_ID_GENERIC_TYPE_RELAYED_TELEMETRY_LOST )
      {
         warnings_add(uVehicleId, "Telemetry from relayed vehicle lost!", g_idIconCPU, get_Color_IconError());
         return;
      }
      if ( uFlags1 == ALARM_ID_GENERIC_TYPE_RELAYED_TELEMETRY_RECOVERED )
      {
         warnings_add(uVehicleId, "Telemetry from relayed vehicle recovered", g_idIconCPU, get_Color_IconError());
         return;
      }
   }

   if ( uAlarms & ALARM_ID_FIRMWARE_OLD )
   {
      uIconId = g_idIconRadio;
      int iInterfaceIndex = uFlags1;
      sprintf(szAlarmText, "%s Radio interface %d firmware is too old!", szAlarmPrefix, iInterfaceIndex+1);
      if ( hardware_radio_index_is_sik_radio(iInterfaceIndex) )
         strcpy(szAlarmText2, "Update the SiK radio firmware to version 2.2");
   }

   if ( uAlarms & ALARM_ID_DEVELOPER_ALARM )
   {
      uIconId = g_idIconController;
      if ( (uFlags1 & 0xFF) == ALARM_FLAG_DEVELOPER_ALARM_RETRANSMISSIONS_OFF )
      {
         g_uTotalLocalAlarmDevRetransmissions++;
         strcpy(szAlarmText, L("Vehicle: Retransmissions are enabled but not requested/received from/to vehicle."));
         snprintf(szAlarmText2, sizeof(szAlarmText2)/sizeof(szAlarmText2[0]), L("Please notify the developers about this alarm (id %d)"), uFlags1);
         if ( (NULL != g_pCurrentModel) && g_pCurrentModel->isVideoLinkFixedOneWay() )
            strcpy(szAlarmText3, L("Vehicle is in one way video link mode."));
      }
      else if ( (uFlags1 & 0xFF) == ALARM_FLAG_DEVELOPER_ALARM_UDP_SKIPPED )
      {
         uIconId = g_idIconCamera;
         u32 uSkipped = (uFlags1 >> 8) & 0xFF;
         u32 uDelta1 = uFlags2 & 0xFF;
         u32 uDelta2 = (uFlags2 >> 8) & 0xFF;
         u32 uDelta3 = (uFlags2 >> 16) & 0xFF;
         sprintf(szAlarmText, "Video capture process skipped %u packets from H264/H265 encoder.", uSkipped);
         sprintf(szAlarmText2, "Please contact Ruby developers. Delta times: %u ms, %u ms, %u ms", uDelta1, uDelta2, uDelta3);
         bShowAsWarning = true;
      }
   }
   if ( uAlarms & ALARM_ID_VIDEO_CAPTURE_MALFUNCTION )
   {
      static u32 s_uTimeLastCaptureMalfunctionAlarm = 0;

      if ( g_TimeNow < s_uTimeLastCaptureMalfunctionAlarm + 10000 )
         return;
      s_uTimeLastCaptureMalfunctionAlarm = g_TimeNow;

      uIconId = g_idIconCamera;

      if ( 0 == (uFlags1 & 0xFF) )
      {
         strcpy(szAlarmText, "Video capture process on vehicle is malfunctioning.");
         strcpy(szAlarmText2, "Restarting it...");
      }
      else if ( 1 == (uFlags1 & 0xFF) )
      {
         strcpy(szAlarmText, "Video capture process on vehicle is not responding.");
         strcpy(szAlarmText2, "Vehicle will restart now...");
      }
      else
      {
         strcpy(szAlarmText, "Video capture process on vehicle is malfunctioning.");
         strcpy(szAlarmText2, "A generic error occured. Reinstall your vehicle firmware.");       
      }
   }


   if ( uAlarms & ALARM_ID_VEHICLE_CPU_RX_LOOP_OVERLOAD )
   {
      uIconId = g_idIconRadio;
      sprintf(szAlarmText, "%s Radio Rx process had a spike of %d ms (read: %u micros, queue: %u micros)", szAlarmPrefix, uFlags1, (uFlags2 & 0xFFFF), (uFlags2>>16));
   }

   if ( uAlarms & ALARM_ID_RADIO_INTERFACE_DOWN )
   {
       if ( (NULL != g_pCurrentModel) && ((int)uFlags1 < g_pCurrentModel->radioInterfacesParams.interfaces_count) )
       {
          const char* szCardModel = str_get_radio_card_model_string(g_pCurrentModel->radioInterfacesParams.interface_card_model[uFlags1]);
          if ( (NULL != szCardModel) && (0 != szCardModel[0]) )
             sprintf(szAlarmText, "%s Radio interface %u (%s) is not working", szAlarmPrefix, uFlags1, szCardModel);
          else
             sprintf(szAlarmText, "%s Radio interface %u (generic) is not working", szAlarmPrefix, uFlags1);
       }
       else
          sprintf(szAlarmText, "%s A radio interface is not working ", szAlarmPrefix);     
       strcpy(szAlarmText2, "Will try automatic recovery to restore it.");
   }

   if ( uAlarms & ALARM_ID_RADIO_INTERFACE_REINITIALIZED )
   {
       if ( (NULL != g_pCurrentModel) && ((int)uFlags1 < g_pCurrentModel->radioInterfacesParams.interfaces_count) )
       {
          const char* szCardModel = str_get_radio_card_model_string(g_pCurrentModel->radioInterfacesParams.interface_card_model[uFlags1]);
          if ( (NULL != szCardModel) && (0 != szCardModel[0]) )
             sprintf(szAlarmText, "%s Radio interface %u (%s) was reinitialized", szAlarmPrefix, uFlags1, szCardModel);
          else
             sprintf(szAlarmText, "%s Radio interface %u (generic) was reinitialized", szAlarmPrefix, uFlags1);
       }
       else
          sprintf(szAlarmText, "%s A radio interface was reinitialized on the vehicle.", szAlarmPrefix);     
   }

   if ( uAlarms & ALARM_ID_GENERIC_STATUS_UPDATE )
   {
      if ( uFlags1 == ALARM_FLAG_GENERIC_STATUS_RECONFIGURING_RADIO_INTERFACE )
         sprintf(szAlarmText, "%s Reconfiguring radio interface...", szAlarmPrefix);
      if ( uFlags1 == ALARM_FLAG_GENERIC_STATUS_RECONFIGURED_RADIO_INTERFACE )
         sprintf(szAlarmText, "%s Reconfigured radio interface. Completed.", szAlarmPrefix);
      if ( uFlags1 == ALARM_FLAG_GENERIC_STATUS_RECONFIGURED_RADIO_INTERFACE_FAILED )
         sprintf(szAlarmText, "%s Reconfiguring radio interface failed!", szAlarmPrefix);
   }

   if ( uAlarms & ALARM_ID_VEHICLE_VIDEO_CAPTURE_RESTARTED )
   {
      sprintf(szAlarmText, "%s The video capture was restarted (for unknown reason)", szAlarmPrefix);
      if ( ALARM_FLAG_VIDEO_CAPTURE_MALFUNCTION == uFlags1 )
         sprintf(szAlarmText2, "%s There was a malfunctioning of the capture program", szAlarmPrefix);
      else if ( ALARM_FLAG_VIDEO_CAPTURE_PARAMETERS_UPDATE == uFlags1 )
      {
         sprintf(szAlarmText2, "%s There where parameters changes that required restart of the capture program", szAlarmPrefix);
         warnings_add( uVehicleId, "Video capture was restarted due to changed parameters", g_idIconCamera);
         return;
      }   
   }

   if ( uAlarms & ALARM_ID_VEHICLE_LOW_STORAGE_SPACE )
   {
       sprintf(szAlarmText, "%s Is running low on free storage space. %u Mb free", szAlarmPrefix, uFlags1);  
       strcpy(szAlarmText2, "Try to delete your vehicle logs or check your SD card on the vehicle."); 
       if ( ! s_bAlarmVehicleLowSpaceMenuShown )
       {
          s_bAlarmVehicleLowSpaceMenuShown = true;
          MenuConfirmationDeleteLogs* pMenu = new MenuConfirmationDeleteLogs(uFlags1, uFlags2);
          add_menu_to_stack(pMenu);
          if ( hardware_is_running_on_runcam_vrx() )
             hardware_led_red_set_blinking_fast(10000);
       }
   }

   if ( uAlarms & ALARM_ID_VEHICLE_STORAGE_WRITE_ERRROR )
   {
       sprintf(szAlarmText, "%s The SD card has write errors", szAlarmPrefix);  
       strcpy(szAlarmText2, "It's recommended you replace the SD card."); 
   }

   if ( uAlarms & ALARM_ID_RECEIVED_INVALID_RADIO_PACKET )
   {
      sprintf(szAlarmText, "%s Invalid unrecoverable data detected over the radio links! Some radio interfaces are not functioning correctly!", szAlarmPrefix);
      strcpy(szAlarmText2, "Try to: swap the radio interfaces, change the USB ports or replace the radio interfaces!");
   }

   if ( uAlarms & ALARM_ID_RECEIVED_INVALID_RADIO_PACKET_RECONSTRUCTED )
   {
      sprintf(szAlarmText, "%s Invalid but recovered data detected over the radio links! Some radio interfaces are not functioning correctly!", szAlarmPrefix);
      if ( uAlarms & ALARM_ID_RECEIVED_INVALID_RADIO_PACKET )
         sprintf(szAlarmText, "%s Both invalid and recoverable data detected over the radio links! Some radio interfaces are not functioning correctly!", szAlarmPrefix);
      strcpy(szAlarmText2, "Try to: swap the radio interfaces, change the USB ports or replace the radio interfaces!");
   }

   if ( uAlarms & ALARM_ID_VEHICLE_VIDEO_DATA_OVERLOAD )
   {
      uIconId = g_idIconCamera;
      g_bHasVideoDataOverloadAlarm = true;
      g_TimeLastVideoDataOverloadAlarm = g_TimeNow;

      int kbpsSent = (int)(uFlags1 & 0xFFFF);
      int kbpsMax = (int)(uFlags1 >> 16);
      if ( NULL != g_pCurrentModel && (! g_pCurrentModel->osd_params.show_overload_alarm) )
         return;
      sprintf(szAlarmText, "%s Video link is overloaded (sending %.1f Mbps, over the safe %.1f Mbps limit for current radio rate)", szAlarmPrefix, ((float)kbpsSent)/1000.0, ((float)kbpsMax)/1000.0);
      strcpy(szAlarmText2, "Video bitrate was decreased temporarly.");
      strcpy(szAlarmText3, "Lower your video bitrate.");
   }

   if ( uAlarms & ALARM_ID_RADIO_LINK_DATA_OVERLOAD )
   {
      if ( g_TimeNow > g_uTimeLastRadioLinkOverloadAlarm + 5000 )
      {
         uIconId = g_idIconRadio;
         g_uTimeLastRadioLinkOverloadAlarm = g_TimeNow;

         int iRadioInterface = (int)(uFlags1 >> 24);
         int bpsSent = (int)(uFlags1 & 0xFFFFFF);
         int bpsMax = (int)(uFlags2);
         sprintf(szAlarmText, "%s Vehicle radio link %d is overloaded (sending %.1f kbytes/sec, max air rate for this link is %.1f kbytes/sec).",
            szAlarmPrefix, iRadioInterface+1, ((float)bpsSent)/1000.0, ((float)bpsMax)/1000.0);
         strcpy(szAlarmText2, "Lower the amount of data you send on this radio link.");
      }
      else
         return;
   }

   if ( uAlarms & ALARM_ID_VEHICLE_VIDEO_TX_OVERLOAD )
   {
      if ( g_TimeNow < g_TimeLastVideoTxOverloadAlarm + 5000 )
         return;
      if ( g_TimeNow < g_RouterIsReadyTimestamp + 2000 )
         return;

      uIconId = g_idIconCamera;
      g_bHasVideoTxOverloadAlarm = true;
      g_TimeLastVideoTxOverloadAlarm = g_TimeNow;

      int ms = (int)(uFlags1 & 0xFFFF);
      int msMax = (int)(uFlags1 >> 16);

      if ( (uFlags2 && (g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[0] < 0)) || ((g_pCurrentModel->radioLinksParams.links_count > 1) && (g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[1] < 0)) )
      {
         sprintf(szAlarmText, "%s Video link transmission is overloaded. Switch to default radio data rates or decrease video bitrate and radio data rate.", szAlarmPrefix);
         strcpy(szAlarmText2, "Not all radio cards do support MCS data rates properly.");
         szAlarmText3[0] = 0;
      }
      else
      {
         if ( (NULL != g_pCurrentModel) && (! g_pCurrentModel->osd_params.show_overload_alarm) )
            return;
         if ( uFlags2 )
            sprintf(szAlarmText, "%s Video link transmission had an Tx overload spike", szAlarmPrefix);
         else
         {
            sprintf(szAlarmText, "%s Video link transmission is overloaded (transmission took %d ms/sec, max safe limit is %d ms/sec)", szAlarmPrefix, ms, msMax);
            osd_stats_set_last_tx_overload_time_ms(ms);
         }
         strcpy(szAlarmText2, "Video bitrate was decreased temporarly.");
         strcpy(szAlarmText3, "Lower your video bitrate or switch frequencies.");
      }
   }

   if ( uAlarms & ALARM_ID_VEHICLE_CPU_LOOP_OVERLOAD )
   if ( !g_bUpdateInProgress )
   {
      bool bShow = false;
      if ( pCS->iDeveloperMode )
         bShow = true;
      else
      {
         u32 timeMax = uFlags1 >> 16;
         if ( timeMax > 100 )
            bShow = true;
      }

      if ( bShow && (g_TimeNow > s_TimeLastCPUOverloadAlarmVehicle + 10000) )
      {
         s_TimeLastCPUOverloadAlarmVehicle = g_TimeNow;
         u32 timeAvg = uFlags1 & 0xFFFF;
         u32 timeMax = uFlags1 >> 16;
         if ( timeMax > 0 )
         {
            sprintf(szAlarmText, "%s The CPU had a spike of %u miliseconds in it's loop processing", szAlarmPrefix, timeMax );
            sprintf(szAlarmText2, "If this is recurent, increase your CPU overclocking speed or reduce your data processing load (video bitrate).");
         }
         if ( timeAvg > 0 )
         {
            sprintf(szAlarmText, "%s The CPU had an overload of %u miliseconds in it's loop processing", szAlarmPrefix, timeAvg );
            sprintf(szAlarmText2, "Increase your CPU overclocking speed or reduce your data processing (video bitrate).");
         }
      }
      else
         return;
   }

   if ( uAlarms & ALARM_ID_VEHICLE_RX_TIMEOUT )
   {
      if ( g_bUpdateInProgress )
         return;
      sprintf(szAlarmText, "%s Radio interface %d timed out reading a radio packet %d times", szAlarmPrefix, 1+(int)uFlags1, (int)uFlags2 );
      bShowAsWarning = true;
   }

   if ( (g_TimeNow > s_TimeLastVehicleAlarm + 8000) || ((s_uLastVehicleAlarms != uAlarms) && (uAlarms != 0)) )
   {
      log_line("Adding UI for alarm from vehicle: [%s]", szAlarmText);
      s_TimeLastVehicleAlarm = g_TimeNow;
      s_uLastVehicleAlarms = uAlarms;

      if ( (uAlarms & ALARM_ID_VEHICLE_VIDEO_TX_OVERLOAD) || (uAlarms & ALARM_ID_VEHICLE_VIDEO_DATA_OVERLOAD) )
         bShowAsWarning = true;
      
      if ( bShowAsWarning )
      {
         warnings_add(0, szAlarmText, uIconId);
         if ( 0 != szAlarmText2[0] )
            warnings_add(0, szAlarmText2, uIconId);
      }
      else
      {
         Popup* p = _get_next_available_alarm_popup(szAlarmText, iAlarmTimeoutSeconds);
         p->setFont(g_idFontOSDSmall);
         p->setIconId(uIconId, get_Color_IconNormal());
         
         if ( 0 != szAlarmText2[0] )
            p->addLine(szAlarmText2);
         if ( 0 != szAlarmText3[0] )
            p->addLine(szAlarmText3);
         popups_add(p);

         if ( (uAlarms & ALARM_ID_VEHICLE_VIDEO_TX_OVERLOAD) || (uAlarms & ALARM_ID_VEHICLE_VIDEO_DATA_OVERLOAD) )
            g_pPopupVideoOverloadAlarm = p;
      }
   }
}

void alarms_add_from_local(u32 uAlarms, u32 uFlags1, u32 uFlags2)
{
   g_uPersistentAllAlarmsLocal |= uAlarms;

   if ( (uAlarms & ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD) || (uAlarms & ALARM_ID_CONTROLLER_IO_ERROR) )
      g_nTotalControllerCPUSpikes++;

   bool bShowAsWarning = false;
   bool bLargeFont = false;
   u32 uIconId = g_idIconAlarm;
   char szAlarmText[256];
   char szAlarmText2[256];
   char szAlarmText3[128];
   int iAlarmTimeoutSeconds = 15;

   char szAlarmDesc[2048];
   alarms_to_string(uAlarms, uFlags1, uFlags2, szAlarmDesc);
   snprintf(szAlarmText, sizeof(szAlarmText)/sizeof(szAlarmText[0]), "Triggered alarm(%s) on the controller", szAlarmDesc);
   szAlarmText2[0] = 0;
   szAlarmText3[0] = 0;

   if ( _alarms_must_skip(0, uAlarms) )
   {
      log_line(szAlarmText);
      log_line("This local alarm must be skipped. Do not show it in UI.");
      return;
   }


   if ( uAlarms & ALARM_ID_FIRMWARE_OLD )
   {
      uIconId = g_idIconRadio;
      int iInterfaceIndex = uFlags1;
      sprintf(szAlarmText, "Controller radio interface %d firmware is too old!", iInterfaceIndex+1);
      if ( hardware_radio_index_is_sik_radio(iInterfaceIndex) )
         strcpy(szAlarmText2, "Update the SiK radio firmware to version 2.2");
   }

   if ( uAlarms & ALARM_ID_CONTROLLER_CPU_RX_LOOP_OVERLOAD )
   {
      uIconId = g_idIconRadio;
      sprintf(szAlarmText, "Controller radio Rx process had a spike of %d ms (read: %u micros, queue: %u micros)", uFlags1, (uFlags2 & 0xFFFF), (uFlags2>>16));
   }

   if ( uAlarms & ALARM_ID_DEVELOPER_ALARM )
   {
      uIconId = g_idIconController;
      if ( (uFlags1 & 0xFF) == ALARM_FLAG_DEVELOPER_ALARM_RETRANSMISSIONS_OFF )
      {
         bLargeFont = true;
         iAlarmTimeoutSeconds = 7;
         g_uTotalLocalAlarmDevRetransmissions++;
         strcpy(szAlarmText, L("Retransmissions are enabled but not requested/received from/to vehicle."));
         snprintf(szAlarmText2, sizeof(szAlarmText2)/sizeof(szAlarmText2[0]), L("Please notify the developers about this alarm (id %d)"), uFlags1);
         if ( (NULL != g_pCurrentModel) && g_pCurrentModel->isVideoLinkFixedOneWay() )
            strcpy(szAlarmText3, L("Vehicle is in one way video link mode."));
      }
      else if ( (uFlags1 & 0xFF) == ALARM_FLAG_DEVELOPER_ALARM_UDP_SKIPPED )
      {
         uIconId = g_idIconCamera;
         u32 uSkipped = (uFlags1 >> 8) & 0xFF;
         u32 uDelta1 = uFlags2 & 0xFF;
         u32 uDelta2 = (uFlags2 >> 8) & 0xFF;
         u32 uDelta3 = (uFlags2 >> 16) & 0xFF;
         sprintf(szAlarmText, "Video capture process skipped %u packets from H264/H265 encoder.", uSkipped);
         sprintf(szAlarmText2, "Please contact Ruby developers. Delta times: %u ms, %u ms, %u ms", uDelta1, uDelta2, uDelta3);
         bShowAsWarning = true;
      }
   }

   if ( uAlarms & ALARM_ID_GENERIC )
   {
      if ( uFlags1 == ALARM_ID_GENERIC_TYPE_MISSED_TELEMETRY_DATA )
      {
         osd_start_flash_osd_elements();
         return;
      }

      if ( uFlags1 == ALARM_ID_GENERIC_TYPE_WRONG_OPENIPC_KEY )
      {
         if ( (! s_bAlarmWrongOpenIPCKey) || (g_TimeNow > s_uTimeLastAlarmWrongOpenIPCKey + 10000) )
         {
            s_bAlarmWrongOpenIPCKey = true;
            s_uTimeLastAlarmWrongOpenIPCKey = g_TimeNow;
            sprintf(szAlarmText, "Detected OpenIPC vehicle is using a different encryption key. Update your controller encryption key.");
         }
         else
            return;
      }
      if ( uFlags1 == ALARM_ID_GENERIC_TYPE_UNKNOWN_VIDEO )
      {
         uIconId = g_idIconCamera;
         sprintf(szAlarmText, "Receiving video stream from unknown vehicle (Id %u)",uFlags2);
      }
      if ( uFlags1 == ALARM_ID_GENERIC_TYPE_RECEIVED_DEPRECATED_MESSAGE )
      {
         static u32 s_uTimeLastAlarmDeprecatedMessage = 0;
         if ( g_TimeNow > s_uTimeLastAlarmDeprecatedMessage + 10000 )
         {
            s_uTimeLastAlarmDeprecatedMessage = g_TimeNow;
            sprintf(szAlarmText, "Received deprecated message from vehicle. Message type: %u", uFlags2);
         }
         else
            return;
      }
   }

   if ( uAlarms & ALARM_ID_RADIO_INTERFACE_DOWN )
   {  
       if ( (int)uFlags1 < hardware_get_radio_interfaces_count() )
       {
          radio_hw_info_t* pRadioInfo = hardware_get_radio_info(uFlags1);
          
          char szCardName[128];
          szCardName[0] = 0;
          controllerGetCardUserDefinedNameOrType(pRadioInfo, szCardName);
          if ( 0 == szCardName[0] )
             strcpy(szCardName, "generic");
                
          if ( (NULL != szCardName) && (0 != szCardName[0]) )
             sprintf(szAlarmText, "Radio interface %u (%s) on the controller is not working.", uFlags1+1, szCardName);
          else
             sprintf(szAlarmText, "Radio interface %u (generic) on the controller is not working.", uFlags1+1);
       }
       else if ( uFlags1 == 0xFF )
       {
          strcpy(szAlarmText, L("Some radio interfaces have broken."));
          strcpy(szAlarmText2, L("Will reinitialize radio interfaces..."));
       }
       else
          strcpy(szAlarmText, "A radio interface on the controller is not working.");     
   }

   if ( uAlarms & ALARM_ID_RADIO_INTERFACE_REINITIALIZED )
   {  
       if ( (int)uFlags1 < hardware_get_radio_interfaces_count() )
       {
          radio_hw_info_t* pRadioInfo = hardware_get_radio_info(uFlags1);
          
          char szCardName[128];
          controllerGetCardUserDefinedNameOrType(pRadioInfo, szCardName);
          if ( 0 == szCardName[0] )
             strcpy(szCardName, "generic");
                
          if ( (NULL != szCardName) && (0 != szCardName[0]) )
             sprintf(szAlarmText, "Radio interface %u (%s) was reinitialized on the controller.", uFlags1+1, szCardName);
          else
             sprintf(szAlarmText, "Radio interface %u (generic) was reinitialized on the controller.", uFlags1+1);
       }
       else
          strcpy(szAlarmText, "A radio interface was reinitialized on the controller.");     
   }

   if ( uAlarms & ALARM_ID_GENERIC_STATUS_UPDATE )
   {
      if ( uFlags1 == ALARM_FLAG_GENERIC_STATUS_REASIGNING_RADIO_LINKS_START )
      {
         strcpy(szAlarmText, "Reconfiguring radio links to vehicle...");
         g_bReconfiguringRadioLinks = true;
      }
      else if ( uFlags1 == ALARM_FLAG_GENERIC_STATUS_REASIGNING_RADIO_LINKS_END )
      {
         strcpy(szAlarmText, "Finished reconfiguring radio links to vehicle.");
         g_bReconfiguringRadioLinks = false;
      }

      if ( uFlags1 == ALARM_FLAG_GENERIC_STATUS_RECONFIGURING_RADIO_INTERFACE )
         strcpy(szAlarmText, "Reconfiguring radio interface...");
      if ( uFlags1 == ALARM_FLAG_GENERIC_STATUS_RECONFIGURED_RADIO_INTERFACE )
         strcpy(szAlarmText, "Reconfigured radio interface. Completed.");
      if ( uFlags1 == ALARM_FLAG_GENERIC_STATUS_RECONFIGURED_RADIO_INTERFACE_FAILED )
         strcpy(szAlarmText, "Reconfiguring radio interface failed!");
      if ( uFlags1 == ALARM_FLAG_GENERIC_STATUS_SENT_PAIRING_REQUEST )
      {
         Model* pModel = findModelWithId(uFlags2, 70);
         t_structure_vehicle_info* pRuntimeInfo = get_vehicle_runtime_info_for_vehicle_id(uFlags2);
         if ( (NULL == pModel) || (NULL == pRuntimeInfo) )
         {
            static bool s_bWarningUnknownVehiclePairing = false;
            if ( ! s_bWarningUnknownVehiclePairing )
               warnings_add(0, "Pairing with unknown vehicle...", g_idIconController, get_Color_IconWarning());
            s_bWarningUnknownVehiclePairing = true;
         }
         else if ( ! pRuntimeInfo->bNotificationPairingRequestSent )
         {
            pRuntimeInfo->bNotificationPairingRequestSent = true;
            char szBuff[256];
            sprintf(szBuff, "Pairing with %s...", pModel->getLongName());
            warnings_add(0, szBuff, g_idIconController);
         }
         return;
      }
   }

   if ( uAlarms & ALARM_ID_RADIO_LINK_DATA_OVERLOAD )
   {
      if ( g_TimeNow > g_uTimeLastRadioLinkOverloadAlarm + 5000 )
      {
         uIconId = g_idIconRadio;
         g_uTimeLastRadioLinkOverloadAlarm = g_TimeNow;

         int iRadioInterface = (int)(uFlags1 >> 24);
         int bpsSent = (int)(uFlags1 & 0xFFFFFF);
         int bpsMax = (int)(uFlags2);
         sprintf(szAlarmText, "Controller radio interface %d is overloaded (sending %.1f kbytes/sec, max air rate for this interface is %.1f kbytes/sec).",
            iRadioInterface+1, ((float)bpsSent)/1000.0, ((float)bpsMax)/1000.0);
         strcpy(szAlarmText2, "Lower the amount of data you send on this radio interface.");
      }
      else
         return;
   }

   if ( uAlarms & ALARM_ID_CONTROLLER_LOW_STORAGE_SPACE )
   {
       sprintf(szAlarmText, "Controller is running low on free storage space. %u Mb free.", uFlags1);  
       strcpy(szAlarmText2, "Try to delete your controller logs or some media files or check your SD card."); 
       if ( hardware_is_running_on_runcam_vrx() )
          hardware_led_red_set_blinking_fast(10000);
   }

   if ( uAlarms & ALARM_ID_CONTROLLER_STORAGE_WRITE_ERRROR )
   {
       strcpy(szAlarmText, "The SD card on the controller has write errors.");  
       strcpy(szAlarmText2, "It's recommended you replace the SD card."); 
   }

   if ( uAlarms & ALARM_ID_RECEIVED_INVALID_RADIO_PACKET )
   {
      strcpy(szAlarmText, "Invalid data detected over the radio links! Some radio interfaces are not functioning correctly on the controller!");
      strcpy(szAlarmText2, "Try to: swap the radio interfaces, change the USB ports or replace the radio interfaces!");
   }

   static u32 s_uLastTimeAlarmNoRadioInterfacesForRadioLink = 0;
   static u32 s_uLastAlarmNoRadioInterfacesForRadioLinkId = 0;
   
   if ( uAlarms & ALARM_ID_CONTROLLER_NO_INTERFACES_FOR_RADIO_LINK )
   {
      if ( s_uLastAlarmNoRadioInterfacesForRadioLinkId == uFlags1 )
      if ( g_TimeNow < s_uLastTimeAlarmNoRadioInterfacesForRadioLink + 1000 )
         return;

      s_uLastAlarmNoRadioInterfacesForRadioLinkId = uFlags1;
      s_uLastTimeAlarmNoRadioInterfacesForRadioLink = g_TimeNow;
      sprintf(szAlarmText, "No radio interfaces on this controller can handle the vehicle's radio link %d.", uFlags1+1);
      sprintf(szAlarmText2, "Add more radio interfaces to your controller or change radio links settings.");
   }
   
   if ( uAlarms & ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD )
   {
      if ( g_bUpdateInProgress )
         return;

      ControllerSettings* pCS = get_ControllerSettings();
      bool bShow = false;
      if ( pCS->iDeveloperMode )
         bShow = true;
      else
      {
         u32 timeMax = uFlags1 >> 16;
         if ( timeMax > 100 )
            bShow = true;
      }
      if ( link_is_reconfiguring_radiolink() || g_bSearching || g_bUpdateInProgress )
         bShow = false;
      if ( link_get_last_reconfiguration_end_time() != 0 )
      if ( g_TimeNow < link_get_last_reconfiguration_end_time() + 2000 )
         bShow = false;

      if ( ! bShow )
         return;

      u32 timeAvg = uFlags1 & 0xFFFF;
      u32 timeMax = uFlags1 >> 16;
      if ( timeMax > 0 )
      {
         sprintf(szAlarmText, "Your controller had a CPU spike of %u miliseconds in it's loop processing.", timeMax );
         sprintf(szAlarmText2, "If this is recurent, increase your CPU overclocking speed or reduce your data processing load (video bitrate).");
      }
      if ( timeAvg > 0 )
      {
         sprintf(szAlarmText, "Your controller had an CPU overload of %u miliseconds in it's loop processing.", timeAvg );
         sprintf(szAlarmText2, "Increase your CPU overclocking speed or reduce your data processing load (video bitrate).");
      }
   }

   if ( uAlarms & ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD_RECORDING )
   {
      if ( g_bUpdateInProgress )
         return;

      bool bShow = true;

      if ( link_is_reconfiguring_radiolink() )
         bShow = false;
      if ( link_get_last_reconfiguration_end_time() != 0 )
      if ( g_TimeNow < link_get_last_reconfiguration_end_time() + 2000 )
         bShow = false;
      
      static u32 s_TimeLastCPUOverloadAlarmRecording = 0;
      if ( bShow && ( g_TimeNow > s_TimeLastCPUOverloadAlarmRecording + 20000 ) )
      {
         s_TimeLastCPUOverloadAlarmRecording = g_TimeNow;
         uIconId = g_idIconSDCard;
         sprintf(szAlarmText, "Your controller had a CPU spike recording video to the SD card." );
         sprintf(szAlarmText2, "If this is recurent, switch to a faster SD card or disable this alarm from System->Alarms.");
      }
      else
         return;
   }

   if ( uAlarms & ALARM_ID_CONTROLLER_RX_TIMEOUT )
   {
      if ( g_bUpdateInProgress )
         return;
      sprintf(szAlarmText, "Controller radio interface %d timed out reading a radio packet %d times.", 1+(int)uFlags1, (int)uFlags2 );
      bShowAsWarning = true;
   }

   if ( uAlarms & ALARM_ID_UNSUPPORTED_VIDEO_TYPE )
   {
      warnings_add_unsupported_video(uFlags1, uFlags2);
      return;
      //sprintf(szAlarmText, "Your vehicle is using H265 video encoding. Your controller does not support H265 video decoding. Switch your vehicle to H264 video encoder from vehicle's Video menu.");
   }

   if ( uAlarms & ALARM_ID_CONTROLLER_IO_ERROR )
   {
      if ( g_uLastControllerAlarmIOFlags == uFlags1 )
         return;

      g_uLastControllerAlarmIOFlags = uFlags1;

      if ( 0 == uFlags1 )
      {
         warnings_add(0, "Controller IO Error Alarm cleared.");
         return;
      }
      strcpy(szAlarmText, "Controller IO Error!");
      szAlarmText2[0] = 0;
      if ( uFlags1 & ALARM_FLAG_IO_ERROR_VIDEO_USB_OUTPUT )
         strcpy(szAlarmText2, "Video USB output error.");
      if ( uFlags1 & ALARM_FLAG_IO_ERROR_VIDEO_USB_OUTPUT_TRUNCATED )
         strcpy(szAlarmText2, "Video USB output truncated.");
      if ( uFlags1 & ALARM_FLAG_IO_ERROR_VIDEO_USB_OUTPUT_WOULD_BLOCK )
         strcpy(szAlarmText2, "Video USB output full.");
      if ( uFlags1 & ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT )
         strcpy(szAlarmText2, "Video player output error.");
      if ( uFlags1 & ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT_TRUNCATED )
         strcpy(szAlarmText2, "Video player output truncated.");
      if ( uFlags1 & ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT_WOULD_BLOCK )
         strcpy(szAlarmText2, "Video player output full.");
      if ( uFlags1 & ALARM_FLAG_IO_ERROR_VIDEO_MPP_DECODER_STALLED )
      {
         bLargeFont = true;
         uIconId = g_idIconCamera;
         strcpy(szAlarmText, "Radxa MPP video streamer stalled!");
         strcpy(szAlarmText2, "If this is a recurent alarm, you can disable it from [Menu]->[System]->[Alarms].");
         strcpy(szAlarmText3, "Please notify the developers about this alarm.");
      }
   }

   if ( g_TimeNow > s_TimeLastLocalAlarm + 10000 || ((s_uLastLocalAlarms != uAlarms) && (uAlarms != 0)) )
   {
      log_line("Adding UI for alarm from controller: [%s]", szAlarmText);
      s_TimeLastLocalAlarm = g_TimeNow;
      s_uLastLocalAlarms = uAlarms;

      if ( uAlarms & ALARM_ID_CONTROLLER_PAIRING_COMPLETED )
      {
         log_line("Adding warning for pairing with vehicle id %u (SW version %d.%d)...", uFlags1, (uFlags2 >> 8) & 0xFF, uFlags2 & 0xFF);
         Model* pModel = findModelWithId(uFlags1, 71);
         char szBuff[256];
         if ( NULL == pModel )
         {
            sprintf(szBuff, L("Paired with a unknown vehicle (SW version %d.%d)"), (uFlags2 >> 8) & 0xFF, uFlags2 & 0xFF);
            warnings_add(0, szBuff, g_idIconController);
         }
         else
         {
            log_line("Paired with %s, VID: %u (SW version %d.%d)", pModel->getLongName(), pModel->uVehicleId, (uFlags2 >> 8) & 0xFF, uFlags2 & 0xFF);
            char szBuff[256];
            sprintf(szBuff, "Paired with %s (SW version %d.%d)", pModel->getLongName(), (uFlags2 >> 8) & 0xFF, uFlags2 & 0xFF);
            warnings_add(0, szBuff, g_idIconController);

            int iIndexRuntime = -1;
            for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
            {
               if ( g_VehiclesRuntimeInfo[i].uVehicleId == pModel->uVehicleId )
               {
                  iIndexRuntime = i;
                  break;
               }
            }
            bool bHasCamera = false;
            log_line("Vehicle has %d cameras.", pModel->iCameraCount);
            if ( pModel->iCameraCount > 0 )
            {
               bHasCamera = true;
               log_line("Vehicle has %d cameras.", pModel->iCameraCount);
            }

            if ( -1 != iIndexRuntime )
            if ( g_VehiclesRuntimeInfo[iIndexRuntime].bGotRubyTelemetryInfo )
            {
               if ( !( g_VehiclesRuntimeInfo[iIndexRuntime].headerRubyTelemetryExtended.uRubyFlags & FLAG_RUBY_TELEMETRY_VEHICLE_HAS_CAMERA) )
               {
                  bHasCamera = false;
                  log_line("Vehicle has camera flag not set in received telemetry.");
               }
               else
               {
                  bHasCamera = true;
                  log_line("Vehicle has camera flag set in received telemetry.");
               }
            }
            if ( ! bHasCamera )
               warnings_add(pModel->uVehicleId, "This vehicle has no camera or video streams.", g_idIconCamera, get_Color_IconError());
         }
         return;
      }

      if ( bShowAsWarning )
         warnings_add(0, szAlarmText, uIconId);
      else
      {
         Popup* p = _get_next_available_alarm_popup(szAlarmText, iAlarmTimeoutSeconds);
         if ( bLargeFont )
            p->setFont(g_idFontOSDWarnings);
         else
            p->setFont(g_idFontOSDSmall);
         p->setIconId(uIconId, get_Color_IconNormal());
         if ( 0 != szAlarmText2[0] )
            p->addLine(szAlarmText2);
         if ( 0 != szAlarmText3[0] )
            p->addLine(szAlarmText3);
         popups_add(p);
      }
   }
}
