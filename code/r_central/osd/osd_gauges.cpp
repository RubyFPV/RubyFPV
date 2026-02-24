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

#include "../../base/base.h"

#include "../../base/config.h"
#include "osd_common.h"
#include "osd_lean.h"
#include "osd.h"

#include "../colors.h"
#include "../../renderer/render_engine.h"
#include "../shared_vars.h"

void osd_show_HID()
{
   float xPos = osd_getMarginX();
   float yPos = 1.0-osd_getMarginY()-osd_getBarHeight() - 0.03*osd_getScaleOSD();
   yPos -= osd_getSecondBarHeight();

   bool bShowMAVLink = false;

   if ( NULL != g_pCurrentModel && (g_pCurrentModel->osd_params.osd_flags2[osd_get_current_layout_index()] & OSD_FLAG2_LAYOUT_LEFT_RIGHT) )
   {
      xPos += osd_getVerticalBarWidth() + osd_getSpacingH();
      yPos = 1.0-osd_getMarginY()-0.03*osd_getScaleOSD();
   }

   float height_text_small = osd_getFontHeightSmall();

   float fAxeSizeV = 0.1*osd_getScaleOSD();
   float fAxeSizeH = fAxeSizeV/g_pRenderEngine->getAspectRatio();
   float fCornerV = fAxeSizeV*0.05;
   float fCornerH = fAxeSizeV*0.05/g_pRenderEngine->getAspectRatio();
   float fAlphaBg = 0.2;
   float fBarThicknessV = fCornerV*2.2;
   float fBarThicknessH = fCornerH*2.2;

   float fBarLengthV = fAxeSizeV*0.8;
   float fBarLengthH = fAxeSizeH*0.8;

   float fSpacingV = fAxeSizeV*0.06;
   float fSpacingH = fAxeSizeH*0.06;
   float fSpacingH0 = fSpacingH;

   float fMarginV = 3.0*g_pRenderEngine->getPixelHeight();
   float fMarginH = 3.0*g_pRenderEngine->getPixelWidth();

   fSpacingH += fBarThicknessH + fSpacingH + fMarginH;
   yPos -= fAxeSizeV+fBarThicknessV+fMarginV;

   float fTextScale = 0.5;
   float height_text = g_pRenderEngine->textHeight(g_idFontOSD);

   osd_show_value(xPos+fCornerH, yPos+fCornerV, "AE", g_idFontOSDSmall);
   osd_show_value(xPos+fCornerH + fAxeSizeH + fSpacingH,yPos+fCornerV, "RT", g_idFontOSDSmall);

   g_pRenderEngine->setColors(get_Color_OSDText(), 0.8);
   g_pRenderEngine->setFill(0,0,0,fAlphaBg);
   g_pRenderEngine->setStrokeSize(2.0);

   g_pRenderEngine->drawRoundRect(xPos, yPos, fAxeSizeH, fAxeSizeV, fCornerV);
   g_pRenderEngine->drawRoundRect(xPos+fAxeSizeH+fSpacingH, yPos, fAxeSizeH, fAxeSizeV, fCornerV);

   g_pRenderEngine->drawRoundRect(xPos+0.5*(fAxeSizeH-fBarLengthH), yPos+fAxeSizeV+fMarginV, fBarLengthH, fBarThicknessV, fCornerV);
   g_pRenderEngine->drawRoundRect(xPos+fAxeSizeH+fMarginH, yPos+0.5*(fAxeSizeV-fBarLengthV), fBarThicknessH, fBarLengthV, fCornerV);

   g_pRenderEngine->drawRoundRect(xPos+0.5*(fAxeSizeH-fBarLengthH)+fAxeSizeH+fSpacingH, yPos+fAxeSizeV+fMarginV, fBarLengthH, fBarThicknessV, fCornerV);
   g_pRenderEngine->drawRoundRect(xPos+fAxeSizeH+fSpacingH+fAxeSizeH+fMarginH, yPos+0.5*(fAxeSizeV-fBarLengthV), fBarThicknessH, fBarLengthV, fCornerV);

   g_pRenderEngine->setColors(get_Color_OSDText(), 0.8);

   if ( NULL != g_pCurrentModel && ( ! (g_pCurrentModel->rc_params.uRCFlags & RC_FLAGS_ENABLED) ) )
   {
      osd_show_value(xPos+fAxeSizeH*0.08, yPos+fAxeSizeV-height_text*1.2, "DISABLED", g_idFontOSDSmall);
      osd_show_value(xPos+fAxeSizeH*0.08 + fAxeSizeH + fSpacingH, yPos+fAxeSizeV-height_text*1.2, "DISABLED", g_idFontOSDSmall);
      bShowMAVLink = true;
   }
   else if ( NULL != g_pCurrentModel && ((g_pCurrentModel->rc_params.uRCFlags & RC_FLAGS_OUTPUT_ENABLED) == 0 ) )
   {
      osd_show_value(xPos+fAxeSizeH*0.2, yPos+fAxeSizeV-height_text*1.3, "NO OUT", g_idFontOSDSmall);
      osd_show_value(xPos+fAxeSizeH*0.2 + fAxeSizeH + fSpacingH, yPos+fAxeSizeV-height_text*1.3, "NO OUT", g_idFontOSDSmall);
      bShowMAVLink = true;
   }
   else if ( g_SM_DownstreamInfoRC.recv_packets < 2 )
   {
      osd_show_value(xPos+fAxeSizeH*0.16, yPos+fAxeSizeV-height_text*1.3, "NO RC", g_idFontOSDSmall);
      osd_show_value(xPos+fAxeSizeH*0.16 + fAxeSizeH + fSpacingH, yPos+fAxeSizeV-height_text*1.3, "NO RC", g_idFontOSDSmall);
      bShowMAVLink = true;
   }

   if (g_SM_DownstreamInfoRC.is_failsafe )
   {
      osd_show_value(xPos+fAxeSizeH*0.08, yPos+fAxeSizeV-height_text*1.2, "FAILSAFE", g_idFontOSDSmall);
      osd_show_value(xPos+fAxeSizeH*0.08 + fAxeSizeH + fSpacingH, yPos-height_text*1.2, "FAILSAFE", g_idFontOSDSmall);
      if ( NULL != g_pCurrentModel && g_pCurrentModel->rc_params.failsafeFlags == RC_FAILSAFE_NOOUTPUT )
         bShowMAVLink = true;
      //else
      //   return;
   }

   if ( NULL == g_pCurrentModel )
      return;

   if ( bShowMAVLink )
   {
      float w = 1.4* g_pRenderEngine->textWidth(g_idFontOSDSmall, "RT");
      osd_show_value(xPos+fCornerH+w, yPos+fCornerV, "EXT", g_idFontOSDSmall);
      osd_show_value(xPos+fCornerH+w + fAxeSizeH + fSpacingH,yPos+fCornerV, "EXT", g_idFontOSDSmall);
   }

   float axeX1 = 0.5;
   float axeX2 = 0.5;
   float axeY1 = 0.5;
   float axeY2 = 0.5;
   float percentX1 = 0.0;
   float percentX2 = 0.0;
   float percentY1 = 0.0;
   float percentY2 = 0.0;
   float fDiv = 0.0;
   fDiv = (float)g_pCurrentModel->rc_params.rcChMax[0] - (float)g_pCurrentModel->rc_params.rcChMin[0];
   if ( fDiv > 0.0001 )
      percentX1 = ((float)g_SM_DownstreamInfoRC.rc_channels[0] - (float)g_pCurrentModel->rc_params.rcChMin[0]) / fDiv;
   else
      percentX1 = 0.0;

   fDiv = (float)g_pCurrentModel->rc_params.rcChMax[1] - (float)g_pCurrentModel->rc_params.rcChMin[1];
   if ( fDiv > 0.0001 )
      percentY1 = ((float)g_SM_DownstreamInfoRC.rc_channels[1] - (float)g_pCurrentModel->rc_params.rcChMin[1]) / fDiv;
   else
      percentY1 = 0.0;


   fDiv = (float)g_pCurrentModel->rc_params.rcChMax[3] - (float)g_pCurrentModel->rc_params.rcChMin[3];
   if ( fDiv > 0.0001 )
      percentX2 = ((float)g_SM_DownstreamInfoRC.rc_channels[3] - (float)g_pCurrentModel->rc_params.rcChMin[3]) / fDiv;
   else
      percentX2 = 0.0;

   fDiv = (float)g_pCurrentModel->rc_params.rcChMax[2] - (float)g_pCurrentModel->rc_params.rcChMin[2];
   if ( fDiv > 0.0001 )
      percentY2 = ((float)g_SM_DownstreamInfoRC.rc_channels[2] - (float)g_pCurrentModel->rc_params.rcChMin[2]) / fDiv;
   else
      percentY2 = 0.0;

   if ( bShowMAVLink && g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
   {
      percentX1 = ((float)g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetryRCChannels.channels[0]-1000.0)/1000.0;
      percentY1 = ((float)g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetryRCChannels.channels[1]-1000.0)/1000.0;
      percentX2 = ((float)g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetryRCChannels.channels[3]-1000.0)/1000.0;
      percentY2 = ((float)g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetryRCChannels.channels[2]-1000.0)/1000.0;
      if ( percentX1 < 0.0 ) percentX1 = 0.0;
      if ( percentX1 > 1.0 ) percentX1 = 1.0;
      if ( percentX2 < 0.0 ) percentX2 = 0.0;
      if ( percentX2 > 1.0 ) percentX2 = 1.0;
      if ( percentY1 < 0.0 ) percentY1 = 0.0;
      if ( percentY1 > 1.0 ) percentY1 = 1.0;
      if ( percentY2 < 0.0 ) percentY2 = 0.0;
      if ( percentY2 > 1.0 ) percentY2 = 1.0;
   }

   axeX1 = 0.08 + percentX1 * 0.84;
   axeY1 = 0.08 + percentY1 * 0.84;
   axeX2 = 0.08 + percentX2 * 0.84;
   axeY2 = 0.08 + percentY2 * 0.84;

   double pC[4];
   memcpy(pC, get_Color_OSDText(), 4*sizeof(double));
   g_pRenderEngine->setColors(pC);
   g_pRenderEngine->setFill(pC[0], pC[1], pC[2],0.3);
   g_pRenderEngine->setStrokeSize(2.0);

   g_pRenderEngine->drawRoundRect(xPos+axeX1*fAxeSizeH-fCornerH, yPos+(1.0-axeY1)*fAxeSizeV-fCornerV, fCornerH*2,fCornerV*2, fCornerV);
   g_pRenderEngine->drawRoundRect(xPos+fAxeSizeH+fSpacingH+axeX2*fAxeSizeH-fCornerH, yPos+(1.0-axeY2)*fAxeSizeV-fCornerV, fCornerH*2,fCornerV*2, fCornerV);

   g_pRenderEngine->setColors(get_Color_OSDText(),0.8);
   g_pRenderEngine->setStrokeSize(2.0);

   g_pRenderEngine->drawRoundRect(xPos+0.5*(fAxeSizeH-fBarLengthH), yPos+fAxeSizeV+fMarginV, fBarLengthH*percentX1, fBarThicknessV, fCornerV);
   g_pRenderEngine->drawRoundRect(xPos+fAxeSizeH+fMarginH, yPos+0.5*(fAxeSizeV-fBarLengthV)+fBarLengthV*(1.0-percentY1), fBarThicknessH, fBarLengthV*percentY1, fCornerV);

   g_pRenderEngine->drawRoundRect(xPos+0.5*(fAxeSizeH-fBarLengthH)+fAxeSizeH+fSpacingH, yPos+fAxeSizeV+fMarginV, fBarLengthH*percentX2, fBarThicknessV, fCornerV);
   g_pRenderEngine->drawRoundRect(xPos+fAxeSizeH+fSpacingH+fAxeSizeH+fMarginH, yPos+0.5*(fAxeSizeV-fBarLengthV)+fBarLengthV*(1.0-percentY2), fBarThicknessH, fBarLengthV*percentY2, fCornerV);

   if ( (g_pCurrentModel->rc_params.channelsCount <= 4) && (! bShowMAVLink) )
      return;
   xPos = xPos+2*fAxeSizeH+2*fSpacingH+fSpacingH0;
   yPos += fAxeSizeV*0.05;


   float fBarWidth = fAxeSizeH*0.6;
   float fBarHeight = fCornerV*2.2;
   float fTextWidth = 1.5*g_pRenderEngine->textWidth(g_idFontOSDSmall, "0");

   int maxCh = g_pCurrentModel->rc_params.channelsCount;
   if ( g_pCurrentModel->rc_params.channelsCount > 8 )
   {
      fBarHeight *= 0.7;
      fTextScale *= 0.8;
      yPos -= fBarHeight*0.2;
   }

   if ( bShowMAVLink )
      maxCh = 8;

   for( int i=4; i<=maxCh; i++ )
   {

      if ( (i >= g_pCurrentModel->rc_params.channelsCount) && (!bShowMAVLink) )
         break;

      g_pRenderEngine->setColors(get_Color_OSDText(), 0.8);
      char szBuff[16];
      sprintf(szBuff, "%d",i+1);
      osd_show_value(xPos,yPos-height_text_small*0.2, szBuff, g_idFontOSDSmall);

      g_pRenderEngine->setColors(get_Color_OSDText());
      g_pRenderEngine->setFill(0,0,0,fAlphaBg);
      g_pRenderEngine->setStrokeSize(2.0);

      g_pRenderEngine->drawRoundRect(xPos+fTextWidth, yPos, fBarWidth, fBarHeight,  fCornerV);
      fDiv = (float)g_pCurrentModel->rc_params.rcChMax[i] - (float)g_pCurrentModel->rc_params.rcChMin[i];
      float percent = 0.0;
      if ( fDiv > 0.0001 )
         percent = ((float)g_SM_DownstreamInfoRC.rc_channels[i] - (float)g_pCurrentModel->rc_params.rcChMin[i]) / fDiv;

      if ( bShowMAVLink && g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry )
      {
         percent = ((float)g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].headerFCTelemetryRCChannels.channels[i]-1000.0)/1000.0;
         if ( percent < 0.0 ) percent = 0.0;
         if ( percent > 1.0 ) percent = 1.0;
      }
      g_pRenderEngine->setColors(get_Color_OSDText(),0.8);
      g_pRenderEngine->setStrokeSize(2.0);

      if ( percent > 0.02 )
         g_pRenderEngine->drawRoundRect(xPos+fTextWidth, yPos, fBarWidth*percent, fBarHeight, fCornerV);
      yPos += fBarHeight + fSpacingV;
   }
}
