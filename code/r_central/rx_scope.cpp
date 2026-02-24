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

#include "rx_scope.h"
#include "../base/base.h"
#include "../base/config.h"
//#include "../radio/radiolink.h"

#include "../renderer/render_engine.h"
#include "pairing.h"
#include "shared_vars.h"
#include "keyboard.h"

typedef struct
{
   int received_bytes;
   int received_video;
   int received_video_retransmission;
   int received_telemetry;
   int received_commands;
} scope_rx_slice_info_t;

#define RXSCOPE_SLICES 2000

static scope_rx_slice_info_t s_RXScopeRXSlices[RXSCOPE_SLICES];

static bool s_bRXScopeStarted = false;
static bool s_bRXScopeWasPairingStarted = false;
static int  s_RXScopeInterfaceRx = -1;
static int  s_RXScopePage = 0;
static int  s_RXScopeZoom = 0;
static bool s_RXScopePaused = false;

static u32 s_RXScopeTimeNowMicroSeconds = 0;
static u32 s_RXScopeTimeNowMiliSeconds = 0;
static u32 s_RXScopeTimeLastRenderMicroSeconds = 0;
static u32 s_RXScopeTimeLastFPSComputeMicroSeconds = 0;
static u32 s_RXScopeDataFramesCount = 0;
static u32 s_RXScopeRenderFramesCount = 0;
static u32 s_RXScopeDataFPS = 0;
static u32 s_RXScopeRenderFPS = 0;

static int s_RXScopeSliceInterval = 1000; // in microseconds
static int s_RXScopeSlicesCount = 1000;
static int s_RXScopeCurrentSliceIndex = -1;

static int s_RXScopeThisFramePackets = 0;
static int s_RXScopeThisFrameKb = 0;

static int s_RXScopeLastFramePackets = 0;
static int s_RXScopeLastFrameKb = 0;

static int s_RXScopeLastTimeHardwareLoop = 0;
static int s_RXScopeLastTimeZoomChange = 0;

void rx_scope_start()
{
   if ( s_bRXScopeStarted )
      return;

   s_bRXScopeWasPairingStarted = pairing_isStarted();
   pairing_stop();

   s_RXScopeInterfaceRx = 0;
   // Deprecated
   //radio_open_interface_for_read(0, RADIO_PORT_ROUTER_DOWNLINK);

   for( int i=0; i<RXSCOPE_SLICES; i++ )
   {
      s_RXScopeRXSlices[i].received_bytes = 0;
      s_RXScopeRXSlices[i].received_video = 0;
      s_RXScopeRXSlices[i].received_video_retransmission = 0;
      s_RXScopeRXSlices[i].received_telemetry = 0;
      s_RXScopeRXSlices[i].received_commands = 0;
   }
   s_RXScopeCurrentSliceIndex = -1;

   s_RXScopeSliceInterval = 1; // ms
   s_RXScopeSlicesCount = 1000;

   s_RXScopePaused = false;

   s_RXScopeTimeLastFPSComputeMicroSeconds = get_current_timestamp_micros();
   s_RXScopeDataFramesCount = 0;
   s_RXScopeRenderFramesCount = 0;
   s_RXScopeDataFPS = 0;
   s_RXScopeRenderFPS = 0;

   s_RXScopeThisFramePackets = 0;
   s_RXScopeThisFrameKb = 0;
   s_RXScopeLastFramePackets = 0;
   s_RXScopeLastFrameKb = 0;

   s_bRXScopeStarted = true;
}

void rx_scope_stop()
{
   if ( ! s_bRXScopeStarted )
      return;

   // Deprecated
   //radio_close_interface_for_read(s_RXScopeInterfaceRx);
   s_RXScopeInterfaceRx = -1;

   s_bRXScopeStarted = false;

   if ( s_bRXScopeWasPairingStarted )
      pairing_start_normal();
}

bool rx_scope_is_started()
{
   return s_bRXScopeStarted;
}

void rx_scope_render_bg()
{
/*
   int startX = s_RXScopeRenderMarginXStart;
   int startY = s_RXScopeRenderMarginYStart;
   int endX = getScreenWidth() - s_RXScopeRenderMarginXEnd;
   int endY = getScreenHeight() - s_RXScopeRenderMarginYEnd;

   g_pRenderEngine->setFill(0,0,0,1);
   g_pRenderEngine->setStroke(0,0,0,1);
   g_pRenderEngine->setStrokeWidth(0);
   Rect(0, 0, getScreenWidth(), getScreenHeight());

   double cc[4] = { 80,30,40,0.88 };
   char szBuff[256];

   render_set_colors(cc);
   float text_scale = toScreenX(0.02)*0.6;
   float width_text = TextWidth(SYSTEM_NAME, *render_getFontMenu(), text_scale);

   Text(toScreenX(0.91), toScreenX(0.05), SYSTEM_NAME, *render_getFontMenu(), text_scale);
   sprintf(szBuff, "%d.%d", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR);
   if ( szBuff[strlen(szBuff)-1] == '0' && szBuff[strlen(szBuff)-2] != '.' )
      szBuff[strlen(szBuff)-1] = 0;
   Text(toScreenX(0.915)+width_text, toScreenX(0.05), szBuff, *render_getFontMenu(), text_scale*0.8);

   float yPos = toScreenY(0.08);

   Fill(255,255,255,1);
   Stroke(0,0,0,1);
   StrokeWidth(1);
   text_scale = toScreenX(0.015);
   Text(toScreenX(0.03), yPos, "RX Scope", *render_getFontMenu(), text_scale);
   yPos -= text_scale * 1.2;

   text_scale = toScreenX(0.01);
   sprintf(szBuff, "[Menu/Cancel]: Close");
   Text(toScreenX(0.03), yPos, szBuff, *render_getFontMenu(), text_scale);
   yPos -= text_scale * 1.3;

   sprintf(szBuff, "[Up/Down]: Page");
   Text(toScreenX(0.03), yPos, szBuff, *render_getFontMenu(), text_scale);
   yPos -= text_scale * 1.3;

   char szPage[64];
   sprintf(szPage, "%d", s_RXScopePage );
   if ( s_RXScopePage == 0 )
      strcpy(szPage, "1");
   if ( s_RXScopePage == 1 )
      strcpy(szPage, "2");
   if ( s_RXScopePage == 2 )
      strcpy(szPage, "3");
   if ( s_RXScopePage == 3 )
      strcpy(szPage, "4");

   sprintf(szBuff, "Page: %s", szPage);
   Text(toScreenX(0.03), yPos, szBuff, *render_getFontMenu(), text_scale);
   yPos -= text_scale * 1.3;

   yPos = toScreenY(0.1);
   text_scale = toScreenX(0.008);
   sprintf(szBuff, "Data FPS: %d", s_RXScopeDataFPS);
   Text(toScreenX(0.2), yPos, szBuff, *render_getFontMenu(), text_scale);
   yPos -= text_scale * 1.3;

   sprintf(szBuff, "Render FPS: %d", s_RXScopeRenderFPS);
   Text(toScreenX(0.2), yPos, szBuff, *render_getFontMenu(), text_scale);
   yPos -= text_scale * 1.3;

   yPos = toScreenY(0.1);
   text_scale = toScreenX(0.008);
   sprintf(szBuff, "kBytes/sec: %d", s_RXScopeLastFrameKb);
   Text(toScreenX(0.36), yPos, szBuff, *render_getFontMenu(), text_scale);
   yPos -= text_scale * 1.3;

   sprintf(szBuff, "Packets/sec: %d", s_RXScopeLastFramePackets);
   Text(toScreenX(0.36), yPos, szBuff, *render_getFontMenu(), text_scale);
   yPos -= text_scale * 1.3;

   text_scale = toScreenX(0.008);

   Fill(255,255,255,1);
   Stroke(255,255,255,1);
   StrokeWidth(1);

   Line(startX, startY, endX, startY);
   Line(startX-1, startY, startX-1, endY);

   Text(startX+(endX-startX)/2-50, startY-text_scale*4.2, "ms", *render_getFontMenu(), text_scale);

   for( int i=0; i<=10; i++ )
   {
      sprintf(szBuff, "%d", i*100);
      if ( s_RXScopeZoom == 1 )
        sprintf(szBuff, "%d", i*50);
      if ( s_RXScopeZoom == 2 )
        sprintf(szBuff, "%d", i*25);
      if ( s_RXScopeZoom == 3 )
        sprintf(szBuff, "%d", i*10);

      float width_text = TextWidth(szBuff, *render_getFontMenu(), text_scale);
      int x = startX + (endX-startX)*i/10;
      Text(x-width_text*0.5, startY-text_scale*2.2, szBuff, *render_getFontMenu(), text_scale);
      Line(x, startY-10, x, startY);
      if ( i != 10 )
         Line(x+(endX-startX)/20, startY-10, x+(endX-startX)/20, startY);
   }


   TextEnd(startX-60, startY+(endY-startY)/2-text_scale*0.4, "kBytes", *render_getFontMenu(), text_scale);
   for( int i=0; i<=s_RXScopeYMax; i+=500 )
   {
      sprintf(szBuff, "%.1f", i/1000.0);
      int y = startY + s_RXScopeRenderBottomBand + (endY-startY - s_RXScopeRenderBottomBand)*i/s_RXScopeYMax;
      TextEnd(startX-22, y-text_scale*0.4, szBuff, *render_getFontMenu(), text_scale);
      Line(startX-10, y, startX, y);
   }
*/
}

void rx_scope_render()
{
/*
   render_start();

   rx_scope_render_bg();

   int startX = s_RXScopeRenderMarginXStart;
   int endX = getScreenWidth() - s_RXScopeRenderMarginXEnd;
   int startY = s_RXScopeRenderMarginYStart + s_RXScopeRenderBottomBand;
   int endY = getScreenHeight() - s_RXScopeRenderMarginYEnd;

   float posX = startX;
   int slicesToRender = s_RXScopeSlicesCount;
   if ( s_RXScopeZoom == 1 )
      slicesToRender /= 2;
   if ( s_RXScopeZoom == 2 )
      slicesToRender /= 4;
   if ( s_RXScopeZoom == 3 )
      slicesToRender /= 10;


   float sliceWidth = (float)(endX-startX)/(float)slicesToRender;
   //if ( sliceWidth < 2.0 )
   //   sliceWidth = 2.0;


   if ( -1 != s_RXScopeCurrentSliceIndex )
   if ( s_RXScopeCurrentSliceIndex < slicesToRender )
   {
      Fill(255,255,0, 0.5);
      Stroke(255,255,0, 0.5);
      StrokeWidth(1);
      int posX = 1 + startX + (endX - startX) * s_RXScopeCurrentSliceIndex / slicesToRender;
      Line(posX, startY, posX, endY);
   }

   Fill(255,255,255,1);
   Stroke(255,255,255,0);
   StrokeWidth(0);


   for ( int i=0; i<slicesToRender; i++ )
   {
      if ( 0 < s_RXScopeRXSlices[i].received_bytes )
      {
         int lineH = (endY-startY) * s_RXScopeRXSlices[i].received_bytes/s_RXScopeYMax;
         if ( lineH > endY-startY )
            lineH = endY-startY;
         Fill(255,255,255,1);
         Rect(posX, startY, sliceWidth-1, lineH);
      }
      if ( s_RXScopeRXSlices[i].received_commands > 0 )
      {
         Fill(255,255,0,1);
         Rect(posX, startY-s_RXScopeRenderBottomBand, sliceWidth-1, s_RXScopeRenderBottomBand-10);
      }
      if ( s_RXScopeRXSlices[i].received_telemetry > 0 )
      {
         Fill(0,0,255,1);
         Rect(posX, startY-s_RXScopeRenderBottomBand, sliceWidth-1, s_RXScopeRenderBottomBand-10);
      }
      if ( s_RXScopeRXSlices[i].received_video_retransmission > 0 )
      {
         Fill(0,255,0,1);
         Rect(posX, startY-s_RXScopeRenderBottomBand, sliceWidth-1, s_RXScopeRenderBottomBand-10);
      }

      posX += sliceWidth;
   }

   render_end();
*/
}

void rx_scope_read_data()
{
   if ( -1 == s_RXScopeInterfaceRx )
      return;

   int miliSec = (s_RXScopeTimeNowMicroSeconds/1000) % 1000;
   int sliceIndex = miliSec / s_RXScopeSliceInterval;
   if ( sliceIndex < 0 ) sliceIndex = 0;
   if ( sliceIndex > s_RXScopeSlicesCount ) sliceIndex = s_RXScopeSlicesCount;

   if ( sliceIndex != s_RXScopeCurrentSliceIndex )
   {
      s_RXScopeRXSlices[sliceIndex].received_bytes = 0;
      s_RXScopeRXSlices[sliceIndex].received_video = 0;
      s_RXScopeRXSlices[sliceIndex].received_video_retransmission = 0;
      s_RXScopeRXSlices[sliceIndex].received_telemetry = 0;
      s_RXScopeRXSlices[sliceIndex].received_commands = 0;

      int clearIndex = s_RXScopeCurrentSliceIndex+1;
      if ( clearIndex > s_RXScopeSlicesCount )
         clearIndex = 0;
      while ( clearIndex != sliceIndex )
      {
         s_RXScopeRXSlices[clearIndex].received_bytes = 0;
         s_RXScopeRXSlices[clearIndex].received_video = 0;
         s_RXScopeRXSlices[clearIndex].received_video_retransmission = 0;
         s_RXScopeRXSlices[clearIndex].received_telemetry = 0;
         s_RXScopeRXSlices[clearIndex].received_commands = 0;
         clearIndex++;
         if ( clearIndex > s_RXScopeSlicesCount )
            clearIndex = 0;
      }
   }

   if ( sliceIndex < s_RXScopeCurrentSliceIndex )
   {
      s_RXScopeLastFramePackets = s_RXScopeThisFramePackets;
      s_RXScopeLastFrameKb = s_RXScopeThisFrameKb;
      s_RXScopeThisFramePackets = 0;
      s_RXScopeThisFrameKb = 0;
   }

   s_RXScopeCurrentSliceIndex = sliceIndex;


   fd_set readset;
   struct timeval to;
   to.tv_sec = 0;
   to.tv_usec = 400;

   int maxfd = -1;
   FD_ZERO(&readset);

   radio_hw_info_t* pNICInfo = hardware_get_radio_info(s_RXScopeInterfaceRx);

   FD_SET(pNICInfo->runtimeInterfaceInfoRx.selectable_fd, &readset);
   if ( pNICInfo->runtimeInterfaceInfoRx.selectable_fd > maxfd )
      maxfd = pNICInfo->runtimeInterfaceInfoRx.selectable_fd;

   int n = select(maxfd+1, &readset, NULL, NULL, &to);
   if ( n == 0 )
      return;
   if ( n < 0 )
      return;

   if (! FD_ISSET(pNICInfo->runtimeInterfaceInfoRx.selectable_fd, &readset))
      return;

   u8* rxBuffer = NULL;
   int rxLength = 0;
   t_packet_header* pPH = NULL;

   // Deprecated
   //rxBuffer = radio_process_wlan_data_in(s_RXScopeInterfaceRx, &rxLength);
   if ( NULL == rxBuffer )
      return;
   if ( rxLength < (int)(sizeof(t_packet_header)) )
      return;

   pPH = (t_packet_header*)rxBuffer;

   if ( ! radio_packet_check_crc(rxBuffer, rxLength) )
      return;

   //log_line("Received a packet type: %d", pdph->packetType);

   s_RXScopeThisFramePackets++;
   s_RXScopeThisFrameKb += rxLength;
   s_RXScopeRXSlices[sliceIndex].received_bytes += rxLength;

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
      s_RXScopeRXSlices[sliceIndex].received_telemetry++;
   else if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
      s_RXScopeRXSlices[sliceIndex].received_commands++;
   else if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RUBY )
      s_RXScopeRXSlices[sliceIndex].received_commands++;
   else if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
   {
      if ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
         s_RXScopeRXSlices[sliceIndex].received_video_retransmission++;
      else
         s_RXScopeRXSlices[sliceIndex].received_video++;
   }
}

void rx_scope_on_page_change()
{
}

void rx_scope_loop()
{
   if ( ! s_bRXScopeStarted )
      return;

   keyboard_consume_input_events();

   s_RXScopeTimeNowMicroSeconds = get_current_timestamp_micros();
   s_RXScopeTimeNowMiliSeconds = get_current_timestamp_ms();

   if ( s_RXScopeLastTimeHardwareLoop < (int)s_RXScopeTimeNowMiliSeconds - 20 )
   {
      s_RXScopeLastTimeHardwareLoop = s_RXScopeTimeNowMiliSeconds;
   }

   if ( ! s_RXScopePaused )
      rx_scope_read_data();

   s_RXScopeDataFramesCount++;
   if ( s_RXScopeTimeNowMicroSeconds > s_RXScopeTimeLastFPSComputeMicroSeconds + 500000 ||
        s_RXScopeTimeNowMicroSeconds < s_RXScopeTimeLastFPSComputeMicroSeconds )
   {
      s_RXScopeTimeLastFPSComputeMicroSeconds = s_RXScopeTimeNowMicroSeconds;
      s_RXScopeDataFPS = 0;
      if ( s_RXScopeDataFramesCount > 0 )
         s_RXScopeDataFPS = s_RXScopeDataFramesCount * 2;
      s_RXScopeDataFramesCount = 0;

      s_RXScopeRenderFPS = 0;
      if ( s_RXScopeRenderFramesCount > 0 )
         s_RXScopeRenderFPS = s_RXScopeRenderFramesCount * 2;
      s_RXScopeRenderFramesCount = 0;
   }

   if ( s_RXScopeTimeNowMicroSeconds > s_RXScopeTimeLastRenderMicroSeconds + 95000 ||
         s_RXScopeTimeNowMicroSeconds < s_RXScopeTimeLastRenderMicroSeconds )
   {
      s_RXScopeTimeLastRenderMicroSeconds = s_RXScopeTimeNowMicroSeconds;
      rx_scope_render();
      s_RXScopeRenderFramesCount++;
   }

   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_BACK )
      rx_scope_stop();

   if ( s_RXScopeLastTimeZoomChange > (int)s_RXScopeTimeNowMiliSeconds - 100 )
      return;

   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_MENU )
   {
      s_RXScopePaused = ! s_RXScopePaused;
      s_RXScopeLastTimeZoomChange = s_RXScopeTimeNowMiliSeconds;
   }

   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_PLUS )
   {
      s_RXScopePage++;
      if ( s_RXScopePage > 3 )
         s_RXScopePage = 0;
      rx_scope_on_page_change();

      s_RXScopeLastTimeZoomChange = s_RXScopeTimeNowMiliSeconds;
      s_RXScopeZoom++;
      if ( s_RXScopeZoom > 3 )
         s_RXScopeZoom = 0;
   }
   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_MINUS )
   {
      s_RXScopePage--;
      if ( s_RXScopePage < 0 )
         s_RXScopePage = 3;
      rx_scope_on_page_change();

      s_RXScopeLastTimeZoomChange = s_RXScopeTimeNowMiliSeconds;
      s_RXScopeZoom--;
      if ( s_RXScopeZoom < 0 )
         s_RXScopeZoom = 3;
   }
}
