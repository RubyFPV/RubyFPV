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

#include "notifications.h"
#include "popup.h"
#include "../base/base.h"
#include "../base/models.h"
#include "../common/string_utils.h"

#include "colors.h"
#include "osd_common.h"
#include "warnings.h"
#include "shared_vars.h"

static bool s_bIsRCFailsafeNotificationTriggered[MAX_CONCURENT_VEHICLES];

static Popup* s_pPopupNotificationPairing = NULL;

void notification_add_armed(u32 uVehicleId)
{
   int iRuntimeInfoIndex = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
         iRuntimeInfoIndex = i;

   if ( (-1 == iRuntimeInfoIndex) || (g_VehiclesRuntimeInfo[iRuntimeInfoIndex].pModel == NULL) )
      return;

   int layoutIndex = g_VehiclesRuntimeInfo[iRuntimeInfoIndex].pModel->osd_params.iCurrentOSDScreen;
   if ( ! (g_VehiclesRuntimeInfo[iRuntimeInfoIndex].pModel->osd_params.osd_flags[layoutIndex] & OSD_FLAG_SHOW_FLIGHT_MODE_CHANGE) )
      return;

   char szBuff[64];
   strcpy(szBuff, "ARMED");
   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0) )
      sprintf(szBuff, "%s ARMED", g_VehiclesRuntimeInfo[iRuntimeInfoIndex].pModel->getShortName());

   Popup* p = new Popup(true, szBuff, 3);
   p->setFont(g_idFontOSD);
   popups_add(p);
}

void notification_add_disarmed(u32 uVehicleId)
{
   int iRuntimeInfoIndex = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
         iRuntimeInfoIndex = i;

   if ( (-1 == iRuntimeInfoIndex) || (g_VehiclesRuntimeInfo[iRuntimeInfoIndex].pModel == NULL) )
      return;

   int layoutIndex = g_VehiclesRuntimeInfo[iRuntimeInfoIndex].pModel->osd_params.iCurrentOSDScreen;
   if ( ! (g_VehiclesRuntimeInfo[iRuntimeInfoIndex].pModel->osd_params.osd_flags[layoutIndex] & OSD_FLAG_SHOW_FLIGHT_MODE_CHANGE) )
      return;

   char szBuff[64];
   strcpy(szBuff, "DISARMED");
   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0) )
      sprintf(szBuff, "%s DISARMED", g_VehiclesRuntimeInfo[iRuntimeInfoIndex].pModel->getShortName());

   Popup* p = new Popup(true, szBuff, 3);
   p->setFont(g_idFontOSD);
   popups_add(p);
}

void notification_add_flight_mode(u32 uVehicleId, u32 flightMode)
{
   char szBuff[128];

   int iRuntimeInfoIndex = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
         iRuntimeInfoIndex = i;

   if ( (-1 == iRuntimeInfoIndex) || (g_VehiclesRuntimeInfo[iRuntimeInfoIndex].pModel == NULL) )
      return;

   int layoutIndex = g_VehiclesRuntimeInfo[iRuntimeInfoIndex].pModel->osd_params.iCurrentOSDScreen;
   if ( ! (g_VehiclesRuntimeInfo[iRuntimeInfoIndex].pModel->osd_params.osd_flags[layoutIndex] & OSD_FLAG_SHOW_FLIGHT_MODE_CHANGE) )
      return;

   sprintf(szBuff, "Flight mode changed to: %s", model_getShortFlightMode(flightMode));
   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0) )
      sprintf(szBuff, "%s: Flight mode changed to: %s", g_VehiclesRuntimeInfo[iRuntimeInfoIndex].pModel->getShortName(), model_getShortFlightMode(flightMode));
   popups_add(new Popup(true, szBuff, 3));
}

void notification_add_rc_failsafe(u32 uVehicleId)
{
   int iRuntimeInfoIndex = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
         iRuntimeInfoIndex = i;

   if ( -1 == iRuntimeInfoIndex )
      return;

   if ( s_bIsRCFailsafeNotificationTriggered[iRuntimeInfoIndex] )
      return;
   s_bIsRCFailsafeNotificationTriggered[iRuntimeInfoIndex] = true;

   warnings_add(uVehicleId, "RC FAILSAFE triggered", g_idIconJoystick, get_Color_IconError(), 6);
}

void notification_add_rc_failsafe_cleared(u32 uVehicleId)
{
   int iRuntimeInfoIndex = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
         iRuntimeInfoIndex = i;

   if ( -1 == iRuntimeInfoIndex )
      return;

   if ( ! s_bIsRCFailsafeNotificationTriggered[iRuntimeInfoIndex] )
      return;
   s_bIsRCFailsafeNotificationTriggered[iRuntimeInfoIndex] = false;

   warnings_add(uVehicleId, "RC FAILSAFE cleared.", g_idIconJoystick, get_Color_IconSucces(), 6);
}

void notification_add_model_deleted()
{
   popups_add(new Popup(true, "Model deleted!", 3));
}

#include "../base/ctrl_settings.h"
static const char* _recording_target_suffix()
{
   ControllerSettings* pCS = get_ControllerSettings();
   if ( NULL == pCS )
      return "";
   if ( pCS->iRecordingTarget == 1 ) return " (Onboard SD)";
   if ( pCS->iRecordingTarget == 2 ) return " (Both: Ground + Onboard SD)";
   return " (Ground)";
}

void notification_add_recording_start()
{
   char szBuff[128];
   snprintf(szBuff, sizeof(szBuff), "%s%s", L("Video recording started"), _recording_target_suffix());
   Popup* p = new Popup(true, szBuff, 3);
   p->setIconId(g_idIconCamera, get_Color_IconNormal());
   popups_add_topmost(p);
}

void notification_add_recording_end()
{
   char szBuff[128];
   snprintf(szBuff, sizeof(szBuff), "%s%s", L("Video recording stopped"), _recording_target_suffix());
   Popup* p = new Popup(true, szBuff, 4);
   p->setIconId(g_idIconCamera, get_Color_IconNormal());
   popups_add_topmost(p);
}

void notification_add_rc_output_enabled()
{
   warnings_add(0, "RC Output to FC is Enabled!", g_idIconJoystick, get_Color_IconSucces(), 4);
}

void notification_add_rc_output_disabled()
{
   warnings_add(0, "RC Output to FC is Disabled!", g_idIconJoystick, get_Color_IconWarning(), 4);
}

void notification_add_frequency_changed(int nLinkId, u32 uFreq)
{
   char szBuff[64];
   szBuff[0] = 0;
   if ( NULL != g_pCurrentModel && 1 == g_pCurrentModel->radioLinksParams.links_count )
      sprintf(szBuff,"Radio link switched to %s", str_format_frequency(uFreq));
   else
      sprintf(szBuff,"Radio link %d switched to %s", nLinkId+1, str_format_frequency(uFreq));

   if ( 0 != szBuff[0] )
      warnings_add(g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].uVehicleId, szBuff, g_idIconInfo, get_Color_IconSucces(), 4);
}

void notification_add_start_pairing()
{
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      s_bIsRCFailsafeNotificationTriggered[i] = false;

   if ( NULL != s_pPopupNotificationPairing )
      notification_remove_start_pairing();

   char szMessage[256];
   strcpy(szMessage, "Configuring radio interfaces and activating model...");
   if ( (NULL != g_pCurrentModel) && (!g_bSearching) )
   {
      strcpy(szMessage, "Configuring radio interfaces (radio links: ");
      int iCount = 0;
      for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
      {
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            continue;
         if ( iCount != 0 )
            strcat(szMessage, ", ");
         strcat(szMessage, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[i]) );
         iCount++;
      }
      strcat(szMessage, ") and activating model...");
   }
   s_pPopupNotificationPairing = new Popup(true, szMessage, 10);
   s_pPopupNotificationPairing->setBottomAlign(true);
   popups_add_topmost(s_pPopupNotificationPairing);
}

void notification_remove_start_pairing()
{
  if ( NULL != s_pPopupNotificationPairing )
  {
     popups_remove(s_pPopupNotificationPairing);
     s_pPopupNotificationPairing = NULL;
  }
}
