/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga petrusoroaga@yahoo.com
    Phase 2: Onboard SD recording menu added by the waybeam-integration branch.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete)
          must retain the above copyright notice, this list of conditions and
          the following disclaimer in the documentation and/or other materials
          provided with the distribution.
        * Neither the name of the organization nor the names of its
          contributors may be used to endorse or promote products derived from
          this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DAMAGE.
*/

#include "menu.h"
#include "menu_vehicle_onboard_recording.h"
#include "menu_item_select.h"
#include "menu_item_text.h"
#include "../shared_vars.h"
#include "../ruby_central.h"
#include "../vehicle_backend_cache.h"
#include "../../base/commands.h"
#include "../../base/ctrl_settings.h"
#include "../handle_commands.h"

// Quality preset: { label, kbps }
static const char* s_szQualityLabels[4] = { "Low — 8 Mbps", "Medium — 16 Mbps", "High — 25 Mbps", "Max — 40 Mbps" };
static const unsigned s_uQualityKbps[4] = { 8000, 16000, 25000, 40000 };

MenuVehicleOnboardRecording::MenuVehicleOnboardRecording(void)
: Menu(MENU_ID_VEHICLE_ONBOARD_RECORDING, L("Onboard Recording"), NULL)
{
   m_Width = 0.36;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.16;
   m_bBackendSupported = false;

   m_pItemsSelect[0] = new MenuItemSelect(L("Recording Target"), L("Where the video is saved when you press the record button. Ground = laptop/goggles SD (classic Ruby). Onboard SD = drone's micro SD card (waybeam only)."));
   m_pItemsSelect[0]->addSelection(L("Ground"));
   m_pItemsSelect[0]->addSelection(L("Onboard SD"));
   m_pItemsSelect[0]->setIsEditable();
   m_IndexTarget = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[1] = new MenuItemSelect(L("Recording Quality"), L("Recording bitrate for the onboard SD stream. Higher = better quality but fills the card faster."));
   for ( int i = 0; i < 4; i++ )
      m_pItemsSelect[1]->addSelection(s_szQualityLabels[i]);
   m_pItemsSelect[1]->setIsEditable();
   m_IndexQuality = addMenuItem(m_pItemsSelect[1]);
}

void MenuVehicleOnboardRecording::onShow()
{
   removeAllTopLines();

   // Probe and refresh backend on open.
   m_bBackendSupported = false;
   if ( NULL != g_pCurrentModel )
   {
      // Probe on EVERY open: the backend can change between boots/repairs, so a
      // stale cached value must be refreshed. The reply lands in handle_commands
      // and overwrites the cache unconditionally.
      handle_commands_send_to_vehicle(COMMAND_ID_GET_VIDEO_BACKEND, 0, NULL, 0);

      u8 uBackend = vehicle_backend_cache_get(g_pCurrentModel->uVehicleId);
      if ( uBackend == VEHICLE_BACKEND_UNKNOWN )
      {
         // Heuristic gate for the first view, before the reply lands.
         m_bBackendSupported = vehicle_backend_likely_supports_waybeam(g_pCurrentModel);
         addTopLine(L("Detecting video encoder backend..."));
      }
      else
      {
         m_bBackendSupported = (uBackend == VEHICLE_BACKEND_WAYBEAM);
         if ( ! m_bBackendSupported )
            addTopLine(L("Onboard SD recording is not supported by this vehicle. It requires the waybeam encoder (OpenIPC SigmaStar boards)."));
      }
   }

   addTopLine(L("Onboard recordings are saved as .ts video under /mnt/mmcblk0p1/ruby/ on the drone's SD card, with a matching .osd telemetry sidecar when available. The GS also keeps its own .osd sidecar as backup."));
   addTopLine(L("Changing these settings requires the vehicle encoder to restart; stream will pause briefly."));

   Menu::onShow();
}

void MenuVehicleOnboardRecording::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for ( int i = 0; i < m_ItemsCount; i++ )
      y += RenderItem(i, y);
   RenderEnd(yTop);
}

void MenuVehicleOnboardRecording::valuesToUI()
{
   ControllerSettings* pCS = get_ControllerSettings();
   if ( NULL == pCS )
      return;

   int iTarget = pCS->iRecordingTarget;
   if ( iTarget < 0 || iTarget > 1 ) iTarget = 0;
   int iQuality = pCS->iOnboardRecordingQuality;
   if ( iQuality < 0 || iQuality > 3 ) iQuality = 1;

   m_pItemsSelect[0]->setSelection(iTarget);
   m_pItemsSelect[1]->setSelection(iQuality);

   m_pItemsSelect[0]->setEnabled(m_bBackendSupported);
   m_pItemsSelect[1]->setEnabled(m_bBackendSupported && (iTarget == 1));
}

void MenuVehicleOnboardRecording::_pushSettingsToVehicle()
{
   if ( NULL == g_pCurrentModel )
      return;
   ControllerSettings* pCS = get_ControllerSettings();
   if ( NULL == pCS )
      return;

   command_packet_onboard_recording params;
   memset(&params, 0, sizeof(params));
   params.uTarget = (u8)((pCS->iRecordingTarget == 1) ? 1 : 0);
   int iQ = pCS->iOnboardRecordingQuality;
   if ( iQ < 0 || iQ > 3 ) iQ = 1;
   params.uQualityIdx = (u8)iQ;
   params.uBitrateKbps = s_uQualityKbps[iQ];

   handle_commands_send_to_vehicle(COMMAND_ID_SET_ONBOARD_RECORDING, 0, (u8*)&params, sizeof(params));
}

void MenuVehicleOnboardRecording::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;
   if ( ! m_bBackendSupported )
      return; // grayed out — nothing to do

   ControllerSettings* pCS = get_ControllerSettings();
   if ( NULL == pCS )
      return;

   if ( g_bIsVideoRecording )
   {
      addMessage(L("Stop the current recording before changing onboard recording settings."));
      valuesToUI();
      return;
   }

   if ( m_SelectedIndex == m_IndexTarget )
   {
      int iNew = m_pItemsSelect[0]->getSelectedIndex();
      if ( iNew != pCS->iRecordingTarget )
      {
         pCS->iRecordingTarget = iNew;
         save_ControllerSettings();
         _pushSettingsToVehicle();
      }
      valuesToUI();
      return;
   }

   if ( m_SelectedIndex == m_IndexQuality )
   {
      int iNew = m_pItemsSelect[1]->getSelectedIndex();
      if ( iNew != pCS->iOnboardRecordingQuality )
      {
         pCS->iOnboardRecordingQuality = iNew;
         save_ControllerSettings();
         // Push to vehicle only if onboard is currently active; otherwise it's
         // stored locally and applied on next toggle to Onboard.
         if ( pCS->iRecordingTarget == 1 )
            _pushSettingsToVehicle();
      }
      valuesToUI();
      return;
   }
}
