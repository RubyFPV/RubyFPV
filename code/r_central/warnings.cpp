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

#include "../base/alarms.h"
#include "../base/base.h"
#include "../base/config.h"
#include "../radio/radiolink.h"
#include "warnings.h"
#include "popup.h"
#include "../renderer/render_engine.h"
#include "colors.h"
#include "menu/menu.h"
#include "menu/menu_confirmation.h"
#include "osd/osd.h"
#include "osd/osd_common.h"
#include "shared_vars.h"

#include <ctype.h>
#include "link_watch.h"
#include "ruby_central.h"
#include "popup_log.h"
#include "timers.h"

#define MAX_WARNINGS 8

Popup* s_PopupSwitchingRadioLink = NULL;

MenuConfirmation* s_pMenuUnsupportedVideo = NULL;
Popup* s_PopupConfigureRadioLink = NULL;
u32 s_uTimeLastPopupConfigureRadioLinkUpdate = 0;
char s_szLastMessageConfigureRadioLink[256];

Popup* s_PopupsWarnings[MAX_WARNINGS];
int s_PopupsWarningsCount = 0;
float s_fBottomYStartWarnings = 0.78;

u32 s_TimeLastWarningAdded = 0;
u32 s_TimeLastWarningRadioInitialized = 0;

u32 s_TimeLastWarningTooMuchData = 0;
u32 s_TimeLastWarningVehicleOverloaded = 0;

bool s_bIsLinkLostToControllerAlarm[MAX_CONCURENT_VEHICLES];
bool s_bIsLinkRecoveredToControllerAlarm[MAX_CONCURENT_VEHICLES];

void warnings_on_changed_vehicle()
{
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      s_bIsLinkRecoveredToControllerAlarm[i] = false;
      s_bIsLinkLostToControllerAlarm[i] = false;
   }
   popups_remove(s_PopupConfigureRadioLink);
   s_PopupConfigureRadioLink = NULL;

   remove_menu_from_stack(s_pMenuUnsupportedVideo);
   s_pMenuUnsupportedVideo = NULL;
}

void warnings_remove_all()
{
   for( int i=0; i<s_PopupsWarningsCount; i++ )
   {
      popups_remove(s_PopupsWarnings[i]);
      delete s_PopupsWarnings[i];
      s_PopupsWarnings[i] = NULL;
   }
   s_PopupsWarningsCount = 0;

   popups_remove(s_PopupSwitchingRadioLink);
   s_PopupSwitchingRadioLink = NULL;

   popups_remove(s_PopupConfigureRadioLink);
   s_PopupConfigureRadioLink = NULL;
   s_szLastMessageConfigureRadioLink[0] = 0;
}

void warnings_periodic_loop()
{
   if ( NULL != s_PopupSwitchingRadioLink )
   {
      if ( s_PopupSwitchingRadioLink->isExpired() )
      {
         popups_remove(s_PopupSwitchingRadioLink);
         s_PopupSwitchingRadioLink = NULL;
         s_szLastMessageConfigureRadioLink[0] = 0;
      }
   }

   if ( NULL != s_PopupConfigureRadioLink )
   {
      if ( s_PopupConfigureRadioLink->isExpired() )
      {
         popups_remove( s_PopupConfigureRadioLink );
         s_PopupConfigureRadioLink = NULL;
         s_szLastMessageConfigureRadioLink[0] = 0;
         link_reset_reconfiguring_radiolink();
      }
      else if ( (g_TimeNow >= s_uTimeLastPopupConfigureRadioLinkUpdate + 300) && (s_PopupConfigureRadioLink->getTimeout() > 3.0) )
      {
         int iLine = s_PopupConfigureRadioLink->getLinesCount()-1;
         char* szLine = s_PopupConfigureRadioLink->getLine(iLine);
         if ( NULL != szLine )
         {
            int iDots = 0;
            int iPos = strlen(szLine)-1;
            while ( (iPos > 0) && (szLine[iPos] == '.') )
            {
               iPos--;
               iDots++;
            }
            iDots++;
            if ( iDots > 4 )
            {
               char szBuff[256];
               strcpy(szBuff, szLine);
               szBuff[iPos+1] = 0;
               s_PopupConfigureRadioLink->setLine(iLine, szBuff);
            }
            else
            {
               char szBuff[256];
               strcpy(szBuff, szLine);
               strcat(szBuff, ".");
               s_PopupConfigureRadioLink->setLine(iLine, szBuff);
            }
         }
      }
   }
}

void warnings_replace(const char* szSource, const char* szDest)
{
   if ( (NULL == szSource) || (0 == szSource[0]) || (NULL == szDest) || (0 == szDest[0]) )
      return;
   for( int i=0; i<s_PopupsWarningsCount; i++ )
   {
      if ( NULL == s_PopupsWarnings[i] )
         continue;
      if ( NULL == strstr(s_PopupsWarnings[i]->getTitle(), szSource) )
         continue;

      s_PopupsWarnings[i]->setTitle(szDest);
      return;
   }
}

void warnings_add(u32 uVehicleId, const char* szTitle)
{
   warnings_add(uVehicleId, szTitle, 0, NULL);
}

void warnings_add(u32 uVehicleId, const char* szTitle, u32 iconId)
{
   warnings_add(uVehicleId, szTitle, iconId, NULL);
}

void warnings_add(u32 uVehicleId, const char* szTitle, u32 iconId, const double* pColorIcon, int timeout)
{
   if ( (NULL == szTitle) || (0 == szTitle[0]) )
      return;

   log_line("Adding warning popup (%d) for VID: %u: %s", s_PopupsWarningsCount, uVehicleId, szTitle);

   char szComposedTitle[256];
   strncpy(szComposedTitle, szTitle, 254);

   int iRuntimeInfoIndex = -1;
   int iCountVehicles = 0;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
         iRuntimeInfoIndex = i;

      if ( g_VehiclesRuntimeInfo[i].uVehicleId != 0 )
         iCountVehicles++;
   }
   if ( (0 != uVehicleId) && (iRuntimeInfoIndex != -1) && (iCountVehicles > 1) )
   if ( NULL != g_VehiclesRuntimeInfo[iRuntimeInfoIndex].pModel )
      sprintf(szComposedTitle, "%s: %s", g_VehiclesRuntimeInfo[iRuntimeInfoIndex].pModel->getShortName(), szTitle);

   szComposedTitle[0] = toupper(szComposedTitle[0]);
   if ( s_PopupsWarningsCount > 0 )
   if ( 0 == strcmp(szComposedTitle, s_PopupsWarnings[0]->getTitle()) )
   if ( s_TimeLastWarningAdded+2000 > g_TimeNow )
   {
      log_line("Warning popup not added due to too soon duplicate with last one.");
      return;
   }

   s_TimeLastWarningAdded = g_TimeNow;

   Preferences* pP = get_Preferences();
   if ( pP->iDebugShowFullRXStats )
      return;
   if ( pP->iPersistentMessages )
      timeout = 12;
   if ( g_pControllerSettings->iEnableDebugStats )
      timeout = 3;

   float dySpacing = 0.012;

   // No more room?
   if ( s_PopupsWarningsCount >= MAX_WARNINGS )
   {
      if ( s_PopupsWarnings[MAX_WARNINGS-1] != NULL )
      {
         popups_remove(s_PopupsWarnings[MAX_WARNINGS-1]);
         delete s_PopupsWarnings[MAX_WARNINGS-1];
         s_PopupsWarnings[MAX_WARNINGS-1] = NULL;
         s_PopupsWarningsCount--;
      }
   }

   // Add it to the bottom of screen, start of the array

   float xPos = 0.01 + osd_getMarginX();
   if ( NULL != g_pCurrentModel && g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT )
      xPos += osd_getVerticalBarWidth();

   Popup* p = new Popup(szComposedTitle, xPos, s_fBottomYStartWarnings, 0.7, timeout);

   //if ( g_pRenderEngine->textWidth(g_idFontMenu, szComposedTitle) > 0.5 )
   //   p->setFont(g_idFontMenuSmall);
   //else
      p->setFont(g_idFontMenu);

   //if ( pP->iPersistentMessages )
   //   p->setNoBackground();
   p->setBackgroundAlpha(0.5);
   p->setCustomPadding(0.6);

   if ( 0 != iconId )
   {
      if ( NULL != pColorIcon )
         p->setIconId(iconId, pColorIcon);
      else
         p->setIconId(iconId, get_Color_IconNormal());
   }

   if ( (-1 != iRuntimeInfoIndex) && (0 != uVehicleId) && (MAX_U32 != uVehicleId) )
   if ( NULL != g_VehiclesRuntimeInfo[iRuntimeInfoIndex].pModel )
   {
      p->setIconId2(osd_getVehicleIcon(g_VehiclesRuntimeInfo[iRuntimeInfoIndex].pModel->vehicle_type), get_Color_OSDText() );
   }

   popups_add(p);
   p->invalidate();
   float fHeightPopup = p->getRenderHeight();
   p->m_yPos -= fHeightPopup;
   p->invalidate();


   // Move it all up

   if ( s_PopupsWarningsCount > 0 )
   for( int i=s_PopupsWarningsCount; i>0; i-- )
   {
      s_PopupsWarnings[i] = s_PopupsWarnings[i-1];
      s_PopupsWarnings[i]->m_yPos -= dySpacing + fHeightPopup;
      s_PopupsWarnings[i]->invalidate();
   }

   s_PopupsWarnings[0] = p;
   s_PopupsWarningsCount++;

   // Remove expired ones

   if ( s_PopupsWarningsCount > 0 )
   for( int i = s_PopupsWarningsCount-1; i>=0; i-- )
   {
      if ( s_PopupsWarnings[i]->isExpired() )
      {
         float hPopup = s_PopupsWarnings[i]->getRenderHeight();
         popups_remove(s_PopupsWarnings[i]);
         delete s_PopupsWarnings[i];
         s_PopupsWarnings[i] = NULL;
         // Move down all above the expired one
         for( int j=i; j<s_PopupsWarningsCount-1; j++ )
         {
            s_PopupsWarnings[j] = s_PopupsWarnings[j+1];
            s_PopupsWarnings[j]->m_yPos += dySpacing + hPopup;
            s_PopupsWarnings[j]->invalidate();
         }
         s_PopupsWarningsCount--;
      }
   }

   log_line("Added warning popup: %s", szComposedTitle);
   popup_log_add_entry(szComposedTitle);
}

void warnings_add_error_null_model(int code)
{
   char szBuff[128];
   sprintf(szBuff, L("Fatal error: Invalid model! (code: %x)"), code);
   popup_log_add_entry(szBuff);
   Popup* p = new Popup(szBuff, 0.34, 0.68, 5);
   p->setIconId(g_idIconError, get_Color_IconError());
   popups_add(p);
   render_all(g_TimeNow);
}

void warnings_add_radio_reinitialized()
{
   if ( g_TimeNow > s_TimeLastWarningRadioInitialized + 70000 )
   {
      log_line("Added warning radio initialized.");
      s_TimeLastWarningRadioInitialized = g_TimeNow;
      Popup* p = new Popup(L("Your vehicle radio modules have been reinitialized"), 0.5, 0.5, 0.6, 10.0);
      p->setCentered();
      p->addLine(L("Your vehicle radio modules have been restarted due to a harware issue. Please check your wiring and power supply on the vehicle."));
      p->setFont(g_idFontOSD);
      p->setIconId(g_idIconWarning, get_Color_IconWarning());
      popups_add_topmost(p);
   }
}

void warnings_add_too_much_data(u32 uVehicleId, int current_kbps, int max_kbps)
{
   if ( (g_pCurrentModel == NULL) || g_pCurrentModel->is_spectator )
      return;

   if ( s_TimeLastWarningTooMuchData != 0 )
   if ( g_TimeNow < s_TimeLastWarningTooMuchData + 10000 )
      return;
   s_TimeLastWarningTooMuchData = g_TimeNow;

   char szBuff[256];
   sprintf(szBuff, L("You are sending too much data (%d Mbps) over the radio link (max capacity %d Mbps). Lower your video bitrate or increase your radio data rate."), current_kbps/1000, max_kbps/1000);
   warnings_add(uVehicleId, szBuff, g_idIconWarning);
}

void warnings_add_vehicle_overloaded(u32 uVehicleId, bool bEC, bool bTx, u32 uECMsPerSec, u32 uTxMsPerSec)
{
   if ( (g_pCurrentModel != NULL) && g_pCurrentModel->is_spectator )
      return;

   if ( s_TimeLastWarningVehicleOverloaded != 0 )
   if ( g_TimeNow < s_TimeLastWarningVehicleOverloaded + 10000 )
      return;
   s_TimeLastWarningVehicleOverloaded = g_TimeNow;
   char szBuff[256];
   if ( bEC )
      sprintf(szBuff, L("Your vehicle is overloaded. Can't keep up processing data for sending error correction (took %u ms / sec). Lower your video bitrate or error correction rate."), uECMsPerSec);
   if ( bTx )
      sprintf(szBuff, L("Your vehicle is overloaded. Can't keep up transmiting radio data (took %u ms / sec). Lower your video bitrate or error correction rate."), uTxMsPerSec);
   warnings_add(uVehicleId, szBuff, g_idIconWarning);
}

void warnings_add_link_to_controller_lost(u32 uVehicleId)
{
   int iRuntimeInfoIndex = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
         iRuntimeInfoIndex = i;
   if ( -1 == iRuntimeInfoIndex )
      return;

   if ( s_bIsLinkLostToControllerAlarm[iRuntimeInfoIndex] )
      return;
   s_bIsLinkLostToControllerAlarm[iRuntimeInfoIndex] = true;
   s_bIsLinkRecoveredToControllerAlarm[iRuntimeInfoIndex] = false;
   warnings_add(uVehicleId, L("Vehicle: Radio link to controller is lost"), g_idIconRadio, get_Color_IconError());
}

void warnings_add_link_to_controller_recovered(u32 uVehicleId)
{
   int iRuntimeInfoIndex = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
         iRuntimeInfoIndex = i;
   if ( -1 == iRuntimeInfoIndex )
      return;

   if ( s_bIsLinkRecoveredToControllerAlarm[iRuntimeInfoIndex] )
      return;
   s_bIsLinkRecoveredToControllerAlarm[iRuntimeInfoIndex] = true;
   s_bIsLinkLostToControllerAlarm[iRuntimeInfoIndex] = false;
   warnings_add(uVehicleId, L("Vehicle: Radio link to controller recovered."), g_idIconRadio, get_Color_IconSucces());
}

Popup* warnings_get_configure_radio_link_popup()
{
   return s_PopupConfigureRadioLink;
}

char* warnings_get_last_message_configure_radio_link()
{
   return s_szLastMessageConfigureRadioLink;
}

void warnings_add_configuring_radio_link(int iRadioLink, const char* szMessage)
{
   if ( (NULL == szMessage) || ( 0 == szMessage[0] ) )
      return;

   strcpy(s_szLastMessageConfigureRadioLink, szMessage);
   
   if ( NULL != s_PopupConfigureRadioLink )
      return;

   log_line("Add popup for starting configuring radio link %d", iRadioLink+1);
   char szBuff[128];
   sprintf(szBuff, "Configuring Radio Link %d", iRadioLink+1);
   /*
   s_PopupConfigureRadioLink = new Popup(true, szBuff, 50);
   if ( (NULL != szMessage) && (0 != szMessage[0]) )
      s_PopupConfigureRadioLink->addLine(szMessage);
   else
      s_PopupConfigureRadioLink->addLine("Configuring");
   s_PopupConfigureRadioLink->addLine("Processing");
   s_PopupConfigureRadioLink->setFont(g_idFontOSDSmall);
   s_PopupConfigureRadioLink->setIconId(g_idIconRadio, get_Color_MenuText());
   s_PopupConfigureRadioLink->setFixedWidth(0.24);

   popups_add_topmost(s_PopupConfigureRadioLink);
   */
}

void warnings_add_configuring_radio_link_line(const char* szLine)
{
   if ( (NULL == szLine) || ( 0 == szLine[0] ) )
      return;

   strcpy(s_szLastMessageConfigureRadioLink, szLine);

   if ( NULL == s_PopupConfigureRadioLink )
      return;
   /*
   s_PopupConfigureRadioLink->removeLastLine();
   s_PopupConfigureRadioLink->addLine(szLine);
   s_PopupConfigureRadioLink->addLine("Processing");
   log_line("Configure radio link popup: added line %d: [%s]", s_PopupConfigureRadioLink->getLinesCount()-1, szLine);
   */
}

void warnings_remove_configuring_radio_link(bool bSucceeded)
{
   s_szLastMessageConfigureRadioLink[0] = 0;
   if ( NULL == s_PopupConfigureRadioLink )
   {
      link_reset_reconfiguring_radiolink();
      return;
   }

   if ( NULL != s_PopupConfigureRadioLink )
   if ( s_PopupConfigureRadioLink->getTimeout() > 3.0 )
   {
      if ( bSucceeded )
      {
         s_PopupConfigureRadioLink->removeLastLine();
         s_PopupConfigureRadioLink->addLine(L("Completed"));
         s_PopupConfigureRadioLink->setTimeout(1.8);
      }
      else
      {
         s_PopupConfigureRadioLink->removeLastLine();
         s_PopupConfigureRadioLink->addLine(L("Failed!"));
         s_PopupConfigureRadioLink->setTimeout(3.2);
      }
   }
}

void warnings_add_unsupported_video(u32 uVehicleId, u32 uType)
{
   #if defined (HW_PLATFORM_RASPBERRY)
   static u32 s_uTimeLastWarningUnsupportedVideo = 0;

   if ( g_TimeNow > s_uTimeLastWarningUnsupportedVideo + 9000 )
   {
      s_uTimeLastWarningUnsupportedVideo = g_TimeNow;

      char szText[256];
      strcpy(szText, L("Your vehicle is sending H265 video. Your controller does not support H265 video decoding. Switch your vehicle to H264 video encoding from vehicle's Video menu."));
      
      if ( NULL == s_pMenuUnsupportedVideo )
      {
         s_pMenuUnsupportedVideo = new MenuConfirmation(L("Unsupported Video Type"), szText, 0, true);
         s_pMenuUnsupportedVideo->m_yPos = 0.3;
         s_pMenuUnsupportedVideo->setIconId(g_idIconCamera);
         add_menu_to_stack(s_pMenuUnsupportedVideo);

      }
      else
         warnings_add(uVehicleId, szText, g_idIconCamera, get_Color_IconError());
   }
   #endif
}

void warnings_add_switching_radio_link(int iVehicleRadioLinkId, u32 uNewFreqKhz)
{
   if ( NULL != s_PopupSwitchingRadioLink )
      popups_remove(s_PopupSwitchingRadioLink);

   char szBuff[256];
   sprintf(szBuff, L("Switching to vehicle radio link %d (%s)"), iVehicleRadioLinkId+1, str_format_frequency(uNewFreqKhz));
   s_PopupSwitchingRadioLink = new Popup(true, szBuff, 50);
   s_PopupSwitchingRadioLink->setFont(g_idFontMenuLarge);
   s_PopupSwitchingRadioLink->addLine(L("Please wait..."));
   s_PopupSwitchingRadioLink->setIconId(g_idIconRadio, get_Color_MenuText());
   s_PopupSwitchingRadioLink->setFixedWidth(0.24);

   popups_add_topmost(s_PopupSwitchingRadioLink);
}


void warnings_remove_switching_radio_link(int iVehicleRadioLinkId, u32 uNewFreqKhz, bool bSucceeded)
{
   if ( NULL == s_PopupSwitchingRadioLink )
      return;

   if ( bSucceeded )
   {
      s_PopupSwitchingRadioLink->removeLastLine();
      char szBuff[256];
      sprintf(szBuff, L("Switched to vehicle radio link %d (%s)"), iVehicleRadioLinkId+1, str_format_frequency(uNewFreqKhz));
      s_PopupSwitchingRadioLink->setTitle(szBuff);
      s_PopupSwitchingRadioLink->setTimeout(2.5);
   }
   else
   {
      s_PopupSwitchingRadioLink->removeLastLine();
      s_PopupSwitchingRadioLink->addLine(L("Failed!"));
      s_PopupSwitchingRadioLink->setTimeout(3.5);
   }
}

void warnings_add_input_device_added(char* szDeviceName)
{
   char szBuff[128];
   if ( (NULL == szDeviceName) || (0 == szDeviceName[0]) )
      strcpy(szBuff, L("A new USB input device was detected"));
   else
      sprintf(szBuff, "A new USB input device (%s) was detected", szDeviceName);

   Popup* pPopup = new Popup(szBuff, 0.6, 0.15, 0.38, 5.0);
   pPopup->setFont(g_idFontMenuSmall);
   popups_add_topmost(pPopup);
}

void warnings_add_input_device_removed(char* szDeviceName)
{
   char szBuff[128];
   if ( (NULL == szDeviceName) || (0 == szDeviceName[0]) )
      strcpy(szBuff, L("A USB input device was removed"));
   else
      sprintf(szBuff, "USB input device (%s) was removed", szDeviceName);

   Popup* pPopup = new Popup(szBuff, 0.6, 0.15, 0.38, 4.0);
   pPopup->setFont(g_idFontMenuSmall);
   popups_add_topmost(pPopup);
}


void warnings_add_input_device_unknown_key(int iKeyCode)
{
   char szBuff[64];
   sprintf(szBuff, "Unkown key code %d", iKeyCode);
   Popup* pPopup = new Popup(szBuff, 0.8, 0.15, 0.18, 3.0);
   pPopup->setFont(g_idFontMenuSmall);
   popups_add_topmost(pPopup);
}