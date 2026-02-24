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
#include <math.h>
#include "popup_camera_params.h"
#include "../renderer/render_engine.h"
#include "colors.h"
#include "menu.h"
#include "shared_vars.h"
#include "timers.h"
#include "ruby_central.h"
#include "handle_commands.h"
#include "pairing.h"
#include "osd_common.h"

PopupCameraParams::PopupCameraParams()
:Popup("", 0.2, 0.2, 5)
{
   setBottomAlign(true);
   setMaxWidth(0.3);
   m_fFixedWidth = 0.2;
   m_bCentered = true;
   m_bInvalidated = true;
   m_bCanAdjust = false;
   m_bCanSendQuick = true;
   m_bHasPendingCameraChanges = false;
   m_bHasPendingAudioChanges = false;
   m_uTimeLastChange = get_current_timestamp_ms();
   m_bHasSharpness = false;
   m_bHasHue = false;
   m_bHasAGC = false;
   m_iIndexParamBrightness = 0;
   m_iIndexParamContrast = 1;
   m_iIndexParamSaturation = 2;
   m_iIndexParamSharpness = -1;
   m_iIndexParamHue = -1;
   m_iIndexParamAGC = -1;
   m_iIndexVolume = -1;
   m_iTotalParams = 3;
   m_iParamToAdjust = 0;
   setIconId(g_idIconCamera, get_Color_PopupText());

   if ( NULL != g_pCurrentModel )
   {
      memcpy(&m_PendingCameraChanges, &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), sizeof(type_camera_parameters));
      memcpy(&m_PendingAudioChanges, &(g_pCurrentModel->audio_params), sizeof(audio_parameters_t));
   }
}

PopupCameraParams::~PopupCameraParams()
{
}

void PopupCameraParams::onShow()
{
   log_line("Popup Camera Params: Show");

   setTimeout(5);
   removeAllLines();

   m_bCanSendQuick = false;
   m_bHasSharpness = false;
   m_bHasHue = false;
   m_bHasAGC = false;
   m_bHasVolume = false;

   m_iIndexParamBrightness = 0;
   m_iIndexParamContrast = 1;
   m_iIndexParamSaturation = 2;
   m_iTotalParams = 3;
   m_iParamToAdjust = 0;

   m_iIndexParamSharpness = -1;
   m_iIndexParamHue = -1;
   m_iIndexParamAGC = -1;
   m_iIndexVolume = -1;

   if ( (NULL == g_pCurrentModel) || (!pairing_isStarted()) ||
        ( g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].bGotFCTelemetry && (g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].uTimeLastRecvAnyRubyTelemetry+2000 < g_TimeNow) ) )
   {
      m_fFixedWidth = 0.3;
      setTitle("Camera Adjustment");
      addLine("You must be connected to a vehicle to be able to adjust camera parameters.");
      Popup::onShow();
      log_line("Popup Camera Params: Show");
      m_bCanAdjust = false;
      return;
   }

   m_bCanSendQuick = true;
   if ( hardware_board_is_goke(g_pCurrentModel->hwCapabilities.uBoardType) )
      m_bCanSendQuick = false;

   if ( !g_pCurrentModel->hasCamera() )
   {
      m_fFixedWidth = 0.3;
      setTitle("Camera Adjustment");
      addLine("Can't adjust camera parameters as this vehicle has no camera");
      Popup::onShow();
      log_line("Popup Camera Params: Show");
      m_bCanAdjust = false;
      return;
   }

   if ( g_pCurrentModel->isActiveCameraHDMI() )
   {
      m_fFixedWidth = 0.3;
      setTitle("Camera Adjustment");
      addLine("Can't adjust camera parameters for HDMI cameras.");
      Popup::onShow();
      log_line("Popup Camera Params: Show");
      m_bCanAdjust = false;
      return;
   }

   // Brightness, contrast, saturation are always present. Don't check them

   if ( g_pCurrentModel->isActiveCameraVeye327290() || g_pCurrentModel->isActiveCameraCSI() )
   {
      m_bHasSharpness = true;
      m_iIndexParamSharpness = m_iTotalParams;
      m_iTotalParams++;
   }

   if ( g_pCurrentModel->isActiveCameraVeye307() ||
        g_pCurrentModel->isActiveCameraOpenIPC() )
   {
      m_bHasHue = true;
      m_iIndexParamHue = m_iTotalParams;
      m_iTotalParams++;
   }

   if ( g_pCurrentModel->isActiveCameraVeye327290() )
   {
      m_bHasAGC = true;
      m_iIndexParamAGC = m_iTotalParams;
      m_iTotalParams++;
   }

   /*
   if ( g_pCurrentModel->isAudioCapableAndEnabled() )
   {
      m_bHasVolume = true;
      m_iIndexVolume = m_iTotalParams;
      m_iTotalParams++;
   }
   */

   m_fFixedWidth = 0.2;
   m_bCanAdjust = true;
   Popup::onShow();
   invalidate();
}

void PopupCameraParams::Render()
{
   float height_text = g_pRenderEngine->textHeight(g_idFontMenuSmall);
   float x = m_RenderXPos+POPUP_MARGINS*g_pRenderEngine->textHeight(g_idFontMenuSmall)/g_pRenderEngine->getAspectRatio();
   float y = m_RenderYPos+0.8*POPUP_MARGINS*g_pRenderEngine->textHeight(g_idFontMenuSmall);
   float dx = m_fIconWidth + 0.8*g_pRenderEngine->textHeight(g_idFontMenuSmall) / g_pRenderEngine->getAspectRatio();

   if ( ! m_bHasPendingCameraChanges )
      memcpy(&m_PendingCameraChanges, &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), sizeof(type_camera_parameters));
   if ( ! m_bHasPendingAudioChanges )
      memcpy(&m_PendingAudioChanges, &(g_pCurrentModel->audio_params), sizeof(audio_parameters_t));

   if ( m_bHasPendingCameraChanges && (! m_bCanSendQuick) )
   if ( get_current_timestamp_ms() > m_uTimeLastChange + 700 )
   {
      memcpy( &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), &m_PendingCameraChanges, sizeof(type_camera_parameters));
      handle_commands_send_single_oneway_command(2, COMMAND_ID_SET_CAMERA_PARAMETERS, g_pCurrentModel->iCurrentCamera, (u8*)(&m_PendingCameraChanges), sizeof(type_camera_parameters));
      m_bHasPendingCameraChanges = false;
   }

   if ( m_bCanAdjust )
   {
      computeSize();
      m_RenderHeight = 4.0*height_text;
      m_RenderYPos = 0.84 - m_RenderHeight;
   }
   Popup::Render();

   if ( (! m_bCanAdjust) || (NULL == g_pCurrentModel) )
      return;

   char szBuff[32];
   char szText[64];
   int value = 0;
   int value_min = 0;
   int value_max = 100;

   int iProfile = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;
   camera_profile_parameters_t* pCamProfile = &(m_PendingCameraChanges.profiles[iProfile]);

   if ( m_iParamToAdjust == m_iIndexParamBrightness )
   {
      value = pCamProfile->brightness;
      value_min = 0;
      value_max = 100;
      strcpy(szText, "Brightness");
   }
   if ( m_iParamToAdjust == m_iIndexParamContrast )
   {
      value = pCamProfile->contrast;
      value_min = 0;
      value_max = 100;
      strcpy(szText, "Contrast");
   }
   if ( m_iParamToAdjust == m_iIndexParamSaturation )
   {
      value = pCamProfile->saturation-100;
      value_min = -100;
      value_max = 100;
      strcpy(szText, "Saturation");
   }
   if ( m_iParamToAdjust == m_iIndexParamSharpness )
   {
      value = pCamProfile->sharpness-100;
      value_min = -100;
      value_max = 100;
      if ( g_pCurrentModel->isActiveCameraVeye327290() )
      {
         value_min = 0;
         value_max = 10;
      }
      strcpy(szText, "Sharpness");
   }
   if ( m_iParamToAdjust == m_iIndexParamHue )
   {
      value = pCamProfile->hue;
      value_min = 0;
      value_max = 100;
      strcpy(szText, "Hue");
   }
   if ( m_iParamToAdjust == m_iIndexParamAGC )
   {
      value = pCamProfile->drc;
      value_min = 0;
      value_max = 15;
      strcpy(szText, "Automatic Gain Control");
   }

   if ( m_iParamToAdjust == m_iIndexVolume )
   {
      value = g_pCurrentModel->audio_params.volume;
      value_min = 0;
      value_max = 100;
      strcpy(szText, "Volume");
   }

   sprintf(szBuff, "%d", value);
   strcat(szText, ":");
   //strcat(szText, szBuff);

   g_pRenderEngine->setColors(get_Color_PopupText());
   g_pRenderEngine->drawText(x+dx, y, g_idFontMenuSmall, szText);
   float fw = g_pRenderEngine->textWidth(g_idFontMenuSmall, szText);
   g_pRenderEngine->drawText(x+dx+fw + 0.02, y-0.2*height_text, g_idFontMenu, szBuff);
   y += height_text*1.4;

   float sliderHeight = 0.4 * height_text;
   float sliderWidth = m_RenderWidth - m_fIconWidth - 2*POPUP_MARGINS*g_pRenderEngine->textHeight(g_idFontMenuSmall)/g_pRenderEngine->getAspectRatio();
   float xPosSlider = x + dx;
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(0);


   g_pRenderEngine->setColors(get_Color_MenuText(), 0.2);
   g_pRenderEngine->setStrokeSize(1);

   g_pRenderEngine->drawRoundRect(xPosSlider, y+height_text-1.3*sliderHeight, sliderWidth, sliderHeight, 0.002);

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(1);

   float yBottom = y + height_text - 0.9*sliderHeight;
   g_pRenderEngine->drawLine(xPosSlider, yBottom, xPosSlider, yBottom-1.6*sliderHeight);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth, yBottom, xPosSlider+sliderWidth, yBottom-1.6*sliderHeight);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth/2, yBottom, xPosSlider+sliderWidth/2, yBottom-1.6*sliderHeight);

   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.1, yBottom, xPosSlider+sliderWidth*0.1, yBottom-sliderHeight*0.8);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.2, yBottom, xPosSlider+sliderWidth*0.2, yBottom-sliderHeight*0.8);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.3, yBottom, xPosSlider+sliderWidth*0.3, yBottom-sliderHeight*0.8);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.4, yBottom, xPosSlider+sliderWidth*0.4, yBottom-sliderHeight*0.8);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.6, yBottom, xPosSlider+sliderWidth*0.6, yBottom-sliderHeight*0.8);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.7, yBottom, xPosSlider+sliderWidth*0.7, yBottom-sliderHeight*0.8);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.8, yBottom, xPosSlider+sliderWidth*0.8, yBottom-sliderHeight*0.8);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.9, yBottom, xPosSlider+sliderWidth*0.9, yBottom-sliderHeight*0.8);

   float size = sliderHeight*1.8;
   xPosSlider = xPosSlider + (float)(sliderWidth-size)*(value-value_min)/(float)(value_max-value_min);

   g_pRenderEngine->drawRoundRect(xPosSlider, yBottom-size*0.6, size, size, 0.002);

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuBorder());
   g_pRenderEngine->setStrokeSize(0);
}


void PopupCameraParams::handleRotaryEvents(bool bCW, bool bCCW, bool bFastCW, bool bFastCCW, bool bSelect, bool bCancel)
{
   if ( bCW || bCCW || bFastCW || bFastCCW || bSelect || bCancel )
   {
      m_uTimeLastChange = get_current_timestamp_ms();
      setTimeout(5);
   }

   if ( bCancel )
   {
      popups_remove(this);
      return;
   }

   if ( (NULL == g_pCurrentModel) || (!pairing_isStarted()) )
      return;
   if ( ! m_bCanAdjust )
      return;

   if ( bSelect )
   {
       m_iParamToAdjust++;
       if ( m_iParamToAdjust >= m_iTotalParams )
          m_iParamToAdjust = 0;
   }

   bool bHasMoved = false;

   if ( bCW || bCCW || bFastCW || bFastCCW )
      bHasMoved = true;

   if ( ! bHasMoved )
      return;

   if ( ! m_bHasPendingCameraChanges )
      memcpy(&m_PendingCameraChanges, &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), sizeof(type_camera_parameters));
   if ( ! m_bHasPendingAudioChanges )
      memcpy(&m_PendingAudioChanges, &(g_pCurrentModel->audio_params), sizeof(audio_parameters_t));

   int iProfile = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;

   if ( m_iParamToAdjust == m_iIndexParamBrightness )
   {
      if ( (bCCW || bFastCCW) && m_PendingCameraChanges.profiles[iProfile].brightness > 0 )
         m_PendingCameraChanges.profiles[iProfile].brightness--;
      if ( bFastCCW && m_PendingCameraChanges.profiles[iProfile].brightness > 0 )
         m_PendingCameraChanges.profiles[iProfile].brightness--;

      if ( (bCW || bFastCW) && m_PendingCameraChanges.profiles[iProfile].brightness < 100 )
         m_PendingCameraChanges.profiles[iProfile].brightness++;
      if ( bFastCW && m_PendingCameraChanges.profiles[iProfile].brightness < 100 )
         m_PendingCameraChanges.profiles[iProfile].brightness++;

      if ( m_PendingCameraChanges.profiles[iProfile].brightness != g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iProfile].brightness )
         m_bHasPendingCameraChanges = true;
   }

   if ( m_iParamToAdjust == m_iIndexParamContrast )
   {
      if ( (bCCW || bFastCCW) && m_PendingCameraChanges.profiles[iProfile].contrast > 0 )
         m_PendingCameraChanges.profiles[iProfile].contrast--;
      if ( bFastCCW && m_PendingCameraChanges.profiles[iProfile].contrast > 0 )
         m_PendingCameraChanges.profiles[iProfile].contrast--;

      if ( (bCW || bFastCW) && m_PendingCameraChanges.profiles[iProfile].contrast < 100 )
         m_PendingCameraChanges.profiles[iProfile].contrast++;
      if ( bFastCW && m_PendingCameraChanges.profiles[iProfile].contrast < 100 )
         m_PendingCameraChanges.profiles[iProfile].contrast++;

      if ( m_PendingCameraChanges.profiles[iProfile].contrast != g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iProfile].contrast )
         m_bHasPendingCameraChanges = true;
   }

   if ( m_iParamToAdjust == m_iIndexParamSaturation )
   {
      if ( (bCCW || bFastCCW) && m_PendingCameraChanges.profiles[iProfile].saturation > 0 )
         m_PendingCameraChanges.profiles[iProfile].saturation--;
      if ( bFastCCW && m_PendingCameraChanges.profiles[iProfile].saturation > 0 )
         m_PendingCameraChanges.profiles[iProfile].saturation--;

      if ( (bCW || bFastCW) && m_PendingCameraChanges.profiles[iProfile].saturation < 200 )
         m_PendingCameraChanges.profiles[iProfile].saturation++;
      if ( bFastCW && m_PendingCameraChanges.profiles[iProfile].saturation < 200 )
         m_PendingCameraChanges.profiles[iProfile].saturation++;

      if ( m_PendingCameraChanges.profiles[iProfile].saturation != g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iProfile].saturation )
         m_bHasPendingCameraChanges = true;
   }

   if ( m_iParamToAdjust == m_iIndexParamSharpness )
   {
      int value_min = -100;
      int value_max = 100;
      if ( g_pCurrentModel->isActiveCameraVeye327290() )
      {
         value_min = 0;
         value_max = 10;
      }

      if ( (bCCW || bFastCCW) && m_PendingCameraChanges.profiles[iProfile].sharpness > 100+value_min )
         m_PendingCameraChanges.profiles[iProfile].sharpness--;
      if ( bFastCCW && m_PendingCameraChanges.profiles[iProfile].sharpness > 100+value_min )
         m_PendingCameraChanges.profiles[iProfile].sharpness--;

      if ( (bCW || bFastCW) && m_PendingCameraChanges.profiles[iProfile].sharpness < 100 + value_max )
         m_PendingCameraChanges.profiles[iProfile].sharpness++;
      if ( bFastCW && m_PendingCameraChanges.profiles[iProfile].sharpness < 100 + value_max )
         m_PendingCameraChanges.profiles[iProfile].sharpness++;

      log_line("PopupCamera: sharpness changed to: %d", m_PendingCameraChanges.profiles[iProfile].sharpness);

      if ( m_PendingCameraChanges.profiles[iProfile].sharpness != g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iProfile].sharpness )
         m_bHasPendingCameraChanges = true;
   }

   if ( m_iParamToAdjust == m_iIndexParamHue )
   {
      if ( (bCCW || bFastCCW) && m_PendingCameraChanges.profiles[iProfile].hue > 0 )
         m_PendingCameraChanges.profiles[iProfile].hue -= 1;
      if ( bFastCCW && m_PendingCameraChanges.profiles[iProfile].hue > 0 )
         m_PendingCameraChanges.profiles[iProfile].hue -= 1;

      if ( (bCW || bFastCW) && m_PendingCameraChanges.profiles[iProfile].hue < 100 )
         m_PendingCameraChanges.profiles[iProfile].hue += 1;
      if ( bFastCW && m_PendingCameraChanges.profiles[iProfile].hue < 100 )
         m_PendingCameraChanges.profiles[iProfile].hue += 1;

      log_line("PopupCamera: hue (R) changed to: %f", m_PendingCameraChanges.profiles[iProfile].hue);

      if ( fabs(m_PendingCameraChanges.profiles[iProfile].hue - g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iProfile].hue) >= 1.0 )
         m_bHasPendingCameraChanges = true;
   }

   if ( m_iParamToAdjust == m_iIndexParamAGC )
   {
      if ( (bCCW || bFastCCW) && m_PendingCameraChanges.profiles[iProfile].drc > 0 )
         m_PendingCameraChanges.profiles[iProfile].drc -= 1;
      if ( bFastCCW && m_PendingCameraChanges.profiles[iProfile].drc > 0 )
         m_PendingCameraChanges.profiles[iProfile].drc -= 1;

      if ( (bCW || bFastCW) && m_PendingCameraChanges.profiles[iProfile].drc < 16 )
         m_PendingCameraChanges.profiles[iProfile].drc += 1;
      if ( bFastCW && m_PendingCameraChanges.profiles[iProfile].drc < 16 )
         m_PendingCameraChanges.profiles[iProfile].drc += 1;

      if ( m_PendingCameraChanges.profiles[iProfile].drc != g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iProfile].drc )
         m_bHasPendingCameraChanges = true;
   }
   if ( (-1 != m_iIndexVolume) && (m_iParamToAdjust == m_iIndexVolume) )
   {
      if ( (bCCW || bFastCCW) && m_PendingAudioChanges.volume > 0 )
         m_PendingAudioChanges.volume--;
      if ( bFastCCW && m_PendingAudioChanges.volume > 0 )
         m_PendingAudioChanges.volume--;

      if ( (bCW || bFastCW) && m_PendingAudioChanges.volume < 100 )
         m_PendingAudioChanges.volume++;
      if ( bFastCW && m_PendingAudioChanges.volume < 100 )
         m_PendingAudioChanges.volume++;

      if ( m_PendingAudioChanges.volume != g_pCurrentModel->audio_params.volume )
         m_bHasPendingAudioChanges = true;
   }

   if ( m_bHasPendingCameraChanges && m_bCanSendQuick )
   {
      memcpy( &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), &m_PendingCameraChanges, sizeof(type_camera_parameters));
      handle_commands_send_single_oneway_command(2, COMMAND_ID_SET_CAMERA_PARAMETERS, g_pCurrentModel->iCurrentCamera, (u8*)(&m_PendingCameraChanges), sizeof(type_camera_parameters));
      m_bHasPendingCameraChanges = false;
   }

   if ( m_bHasPendingAudioChanges && m_bCanSendQuick )
   {
      memcpy( &(g_pCurrentModel->audio_params), &m_PendingAudioChanges, sizeof(audio_parameters_t));
      handle_commands_send_single_oneway_command(2, COMMAND_ID_SET_AUDIO_PARAMS, 0, (u8*)&m_PendingAudioChanges, sizeof(audio_parameters_t));
      m_bHasPendingAudioChanges = false;
   }
}

