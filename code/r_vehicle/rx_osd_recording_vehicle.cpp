/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga petrusoroaga@yahoo.com
    Phase 2: onboard OSD recorder addition by the waybeam-integration branch.

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

#include "rx_osd_recording_vehicle.h"
#include "telemetry_msp.h"
#include "timers.h"
#include "../base/config.h"
#include "../base/msp.h"
#include "../radio/radiopackets2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

static FILE*              s_pOSDFile = NULL;
static bool               s_bStarted = false;
static u32                s_uTimeStartedMs = 0;
static u32                s_uLastFrameWriteMs = 0;
static int                s_iFramesWritten = 0;
static bool               s_bHeaderWritten = false;
static type_msp_parse_state s_MSPState;
static bool               s_bMSPStateInit = false;

static void _ensure_msp_state_init()
{
   if ( s_bMSPStateInit )
      return;
   memset(&s_MSPState, 0, sizeof(s_MSPState));
   parse_msp_reset_state(&s_MSPState);
   s_MSPState.headerTelemetryMSP.uMSPOSDCols = DEFAULT_MSPOSD_RECORDING_COLS;
   s_MSPState.headerTelemetryMSP.uMSPOSDRows = DEFAULT_MSPOSD_RECORDING_ROWS;
   s_bMSPStateInit = true;
}

void rx_osd_recording_vehicle_feed_msp(u8* pData, int iLen, bool bAllowScreenUpdate)
{
   if ( NULL == pData || iLen <= 0 )
      return;
   _ensure_msp_state_init();
   memcpy(&(s_MSPState.headerTelemetryMSP), telemetry_msp_get_header(), sizeof(t_packet_header_telemetry_msp)); // sync whole MSP header each time
   parse_msp_incoming_data(&s_MSPState, pData, iLen, bAllowScreenUpdate);
}

void rx_osd_recording_vehicle_start(const char* szBasePath)
{
   if ( s_bStarted )
      return;
   if ( NULL == szBasePath || 0 == szBasePath[0] )
   {
      log_softerror_and_alarm("[OnboardOSD] Cannot start: empty base path.");
      return;
   }
   _ensure_msp_state_init();

   char szFile[512];
   snprintf(szFile, sizeof(szFile), "%s.osd", szBasePath);

   s_pOSDFile = fopen(szFile, "wb");
   if ( NULL == s_pOSDFile )
   {
      log_softerror_and_alarm("[OnboardOSD] Failed to create [%s]: errno=%d (%s)", szFile, errno, strerror(errno));
      return;
   }

   s_uTimeStartedMs = g_TimeNow;
   s_uLastFrameWriteMs = 0;
   s_iFramesWritten = 0;
   s_bHeaderWritten = false;
   s_bStarted = true;
   log_line("[OnboardOSD] Started onboard OSD recording to [%s]", szFile);
}

void rx_osd_recording_vehicle_stop()
{
   if ( NULL != s_pOSDFile )
   {
      fflush(s_pOSDFile);
      fclose(s_pOSDFile);
      s_pOSDFile = NULL;
   }
   if ( s_bStarted )
      log_line("[OnboardOSD] Stopped onboard OSD recording (%d frames written).", s_iFramesWritten);
   s_bStarted = false;
   s_bHeaderWritten = false;
   s_iFramesWritten = 0;
}

bool rx_osd_recording_vehicle_is_started()
{
   return s_bStarted;
}

static void _write_header()
{
   if ( NULL == s_pOSDFile || s_bHeaderWritten )
      return;
   u8 uHeader[40];
   memset(uHeader, 0, 40);
   // FC type comes from telemetry_msp's main parser state (s_PHTMSP), not
   // the writer's local parse state (which doesn't detect FC variant itself).
   switch ( telemetry_msp_get_fc_type_flag() )
   {
      case MSP_FLAGS_FC_TYPE_BETAFLIGHT: memcpy(uHeader, "BTFL", 4); break;
      case MSP_FLAGS_FC_TYPE_INAV:       memcpy(uHeader, "INAV", 4); break;
      case MSP_FLAGS_FC_TYPE_PITLAB:     memcpy(uHeader, "PITL", 4); break;
      case MSP_FLAGS_FC_TYPE_ARDUPILOT:  memcpy(uHeader, "ARDU", 4); break;
      default: break; // zero-padded when FC type unknown
   }
   // bytes 36-39: grid cols/rows (u16 each), read by external tools (Digital-FPV-OSD-Tool)
   // to auto-detect the frame stride instead of assuming a fixed default grid size.
   u16 uCols = DEFAULT_MSPOSD_RECORDING_COLS;
   u16 uRows = DEFAULT_MSPOSD_RECORDING_ROWS;
   memcpy(&(uHeader[36]), &uCols, sizeof(u16));
   memcpy(&(uHeader[38]), &uRows, sizeof(u16));
   fwrite(uHeader, 1, 40, s_pOSDFile);
   s_bHeaderWritten = true;
}

static void _write_frame()
{
   if ( NULL == s_pOSDFile )
      return;
   if ( ! s_bHeaderWritten )
      _write_header();

   // Skip duplicate frames (same iLastDrawFrameNumber as last write).
   if ( s_MSPState.iLastDrawFrameNumber == s_iFramesWritten && s_iFramesWritten > 0 )
      return;

   u32 uTimeMs = g_TimeNow - s_uTimeStartedMs;
   fwrite((u8*)&uTimeMs, 1, sizeof(u32), s_pOSDFile);

   u16 uBuffer[DEFAULT_MSPOSD_RECORDING_COLS * DEFAULT_MSPOSD_RECORDING_ROWS];
   int iBuffPos = 0;
   int iCols = s_MSPState.headerTelemetryMSP.uMSPOSDCols;
   if ( iCols <= 0 || iCols > 64 )
      iCols = DEFAULT_MSPOSD_RECORDING_COLS;
   int iRows = s_MSPState.headerTelemetryMSP.uMSPOSDRows;
   if ( iRows <= 0 || iRows > 24 )
      iRows = DEFAULT_MSPOSD_RECORDING_ROWS;
   // Real canvas (iCols/iRows) can be smaller than the padded output grid, e.g. a
   // 50x18 FC canvas padded into 60x22: pad cells with 0, don't read uScreenChars
   // at x>=iCols, which would alias into the next row's real data (real stride is iCols).
   for ( int y = 0; y < DEFAULT_MSPOSD_RECORDING_ROWS; y++ )
   for ( int x = 0; x < DEFAULT_MSPOSD_RECORDING_COLS; x++ )
      uBuffer[iBuffPos++] = ( (x < iCols) && (y < iRows) ) ? s_MSPState.uScreenChars[x + y * iCols] : 0;

   fwrite(uBuffer, 1, iBuffPos * sizeof(u16), s_pOSDFile);
   s_iFramesWritten = (s_MSPState.iLastDrawFrameNumber > 0) ? s_MSPState.iLastDrawFrameNumber : (s_iFramesWritten + 1);
   s_uLastFrameWriteMs = g_TimeNow;
}

static void _build_control_file_path(char* szOut, int iMax)
{
   snprintf(szOut, iMax, "%s%s", FOLDER_RUBY_TEMP, RX_OSD_REC_CONTROL_FILENAME);
}

static void _poll_control_file()
{
   // Polled every periodic_loop tick (no throttle). On tmpfs, stat() is a
   // single syscall — we only fopen() when the file's mtime indicates a
   // start signal we haven't seen yet, so steady-state cost is one stat
   // per tick. Fast enough that QA-press → first OSD frame is bounded by
   // the MSP loop cadence (~10 ms), not by polling.
   char szCtl[256];
   _build_control_file_path(szCtl, sizeof(szCtl));

   struct stat st;
   if ( 0 != stat(szCtl, &st) )
   {
      if ( s_bStarted )
         rx_osd_recording_vehicle_stop();
      return;
   }
   if ( s_bStarted )
      return; // already running, file presence just confirms; nothing to do

   FILE* fp = fopen(szCtl, "r");
   if ( NULL == fp )
      return;
   char szPath[256];
   szPath[0] = 0;
   if ( NULL == fgets(szPath, sizeof(szPath)-1, fp) )
      szPath[0] = 0;
   fclose(fp);
   for ( int i = 0; i < (int)sizeof(szPath); i++ )
      if ( szPath[i] == '\n' || szPath[i] == '\r' ) szPath[i] = 0;
   if ( 0 != szPath[0] )
      rx_osd_recording_vehicle_start(szPath);
}

void rx_osd_recording_vehicle_periodic_loop()
{
   _poll_control_file();

   if ( ! s_bStarted )
      return;
   if ( NULL == s_pOSDFile )
      return;

   // First frame: write header immediately so VueOSD sees a valid file even
   // if the session is power-cut a moment later.
   if ( ! s_bHeaderWritten )
   {
      _write_header();
      return;
   }

   // ~50 ms cadence matches the GS writer.
   if ( g_TimeNow < s_uLastFrameWriteMs + 50 )
      return;
   _write_frame();

   // fsync every ~30 seconds to minimize data loss on power cut.
   static u32 s_uLastFsyncMs = 0;
   if ( g_TimeNow >= s_uLastFsyncMs + 30000 )
   {
      fflush(s_pOSDFile);
      s_uLastFsyncMs = g_TimeNow;
   }
}

void rx_osd_recording_vehicle_signal_start(const char* szBasePath)
{
   if ( NULL == szBasePath || 0 == szBasePath[0] )
      return;
   char szCtl[256];
   _build_control_file_path(szCtl, sizeof(szCtl));
   FILE* fp = fopen(szCtl, "w");
   if ( NULL == fp )
   {
      log_softerror_and_alarm("[OnboardOSD] Failed to write control file %s (errno=%d)", szCtl, errno);
      return;
   }
   fprintf(fp, "%s\n", szBasePath);
   fclose(fp);
}

void rx_osd_recording_vehicle_signal_stop()
{
   char szCtl[256];
   _build_control_file_path(szCtl, sizeof(szCtl));
   unlink(szCtl);
}
