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
#include "../base/hardware_camera.h"
#include "../base/hardware_procs.h"
#include <math.h>
#include "shared_vars.h"
#include "timers.h"
#include "video_sources.h"
#include "ruby_rt_vehicle.h"

int _try_process_live_changes(int iNewCameraIndex, type_camera_parameters* pNewCamParams, int iCurrentCameraIndex, type_camera_parameters* pCurrentCamParams)
{
   if ( (NULL == pNewCamParams) || (NULL == pCurrentCamParams) )
      return -1;

   int iCurCamProfile = pCurrentCamParams[iCurrentCameraIndex].iCurrentProfile;
   int iNewCamProfile = pNewCamParams[iNewCameraIndex].iCurrentProfile;
   camera_profile_parameters_t* pCurCamProfile = &(pCurrentCamParams[iCurrentCameraIndex].profiles[iCurCamProfile]);
   camera_profile_parameters_t* pNewCamProfile = &(pNewCamParams[iNewCameraIndex].profiles[iNewCamProfile]);

   int iCountUpdates = 0;
   if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
   {
      u32 uCurrentFlags = pCurCamProfile->uFlags;
      u32 uNewFlags = pNewCamProfile->uFlags;
      if ( (uCurrentFlags & CAMERA_FLAG_IR_FILTER_OFF) != (uNewFlags & CAMERA_FLAG_IR_FILTER_OFF) )
      {
         hardware_camera_maj_set_irfilter_off(uNewFlags & CAMERA_FLAG_IR_FILTER_OFF, true);
         pNewCamProfile->uFlags = uCurrentFlags;
         iCountUpdates++;
      }
      if ( (uCurrentFlags & CAMERA_FLAG_OPENIPC_DAYLIGHT_OFF) != (uNewFlags & CAMERA_FLAG_OPENIPC_DAYLIGHT_OFF) )
      {
         hardware_camera_maj_set_daylight_off(uNewFlags & CAMERA_FLAG_OPENIPC_DAYLIGHT_OFF, true);
         pNewCamProfile->uFlags = uCurrentFlags;
         iCountUpdates++;
      }
   }

   if ( pNewCamProfile->brightness != pCurCamProfile->brightness )
   {
      if ( g_pCurrentModel->isRunningOnOpenIPCHardware() && (! hardware_board_is_goke(g_pCurrentModel->hwCapabilities.uBoardType)) )
      {
         hardware_camera_maj_set_brightness(pNewCamProfile->brightness);
         pNewCamProfile->brightness = pCurCamProfile->brightness;
         iCountUpdates++;
      }
      else if ( g_pCurrentModel->isActiveCameraCSICompatible() )
      {
         video_source_csi_send_control_message( RASPIVID_COMMAND_ID_BRIGHTNESS, pNewCamProfile->brightness, 0 );
         pNewCamProfile->brightness = pCurCamProfile->brightness;
         iCountUpdates++;
      }
   }

   if ( pNewCamProfile->contrast != pCurCamProfile->contrast )
   {
      if ( g_pCurrentModel->isRunningOnOpenIPCHardware() && (! hardware_board_is_goke(g_pCurrentModel->hwCapabilities.uBoardType)) )
      {
         hardware_camera_maj_set_contrast(pNewCamProfile->contrast);
         pNewCamProfile->contrast = pCurCamProfile->contrast;
         iCountUpdates++;
      }
      else if ( g_pCurrentModel->isActiveCameraCSICompatible() )
      {
         video_source_csi_send_control_message( RASPIVID_COMMAND_ID_CONTRAST, pNewCamProfile->contrast, 0 );
         pNewCamProfile->contrast = pCurCamProfile->contrast;
         iCountUpdates++;
      }
   }

   if ( pNewCamProfile->saturation != pCurCamProfile->saturation )
   {
      if ( g_pCurrentModel->isRunningOnOpenIPCHardware() && (! hardware_board_is_goke(g_pCurrentModel->hwCapabilities.uBoardType)) )
      {
         hardware_camera_maj_set_saturation(pNewCamProfile->saturation);
         pNewCamProfile->saturation = pCurCamProfile->saturation;
         iCountUpdates++;
      }
      else if ( g_pCurrentModel->isActiveCameraCSICompatible() )
      {
         video_source_csi_send_control_message( RASPIVID_COMMAND_ID_SATURATION, pNewCamProfile->saturation, 0 );
         pNewCamProfile->saturation = pCurCamProfile->saturation;
         iCountUpdates++;
      }
   }

   if ( pNewCamProfile->sharpness != pCurCamProfile->sharpness )
   {
      if ( g_pCurrentModel->isActiveCameraCSICompatible() )
      {
         video_source_csi_send_control_message( RASPIVID_COMMAND_ID_SHARPNESS, pNewCamProfile->sharpness, 0 );
         pNewCamProfile->sharpness = pCurCamProfile->sharpness;
         iCountUpdates++;
      }
   }

   if ( pNewCamProfile->hue != pCurCamProfile->hue )
   {
      if ( g_pCurrentModel->isRunningOnOpenIPCHardware() && (! hardware_board_is_goke(g_pCurrentModel->hwCapabilities.uBoardType)) )
      {
         hardware_camera_maj_set_hue(pNewCamProfile->hue);
         pNewCamProfile->hue = pCurCamProfile->hue;
         iCountUpdates++;
      }
   }

   return iCountUpdates;
}

void process_camera_params_changed(u8* pPacketBuffer, int iPacketLength)
{
   if ( (NULL == pPacketBuffer) || (iPacketLength < (int)sizeof(t_packet_header)) )
      return;

   int iOldCameraIndex = g_pCurrentModel->iCurrentCamera;
   u8 uNewCameraIndex = 0;
   type_camera_parameters oldCamParams;
   type_camera_parameters newCamParams;

   memcpy((u8*)(&oldCamParams), &(g_pCurrentModel->camera_params[iOldCameraIndex]), sizeof(type_camera_parameters));
   memcpy((u8*)&newCamParams, pPacketBuffer + sizeof(t_packet_header) + sizeof(u8), sizeof(type_camera_parameters));
   memcpy(&uNewCameraIndex, pPacketBuffer + sizeof(t_packet_header), sizeof(u8));

   // Do not save model. Saved by rx_commands. Just update to the new values.
   memcpy(&(g_pCurrentModel->camera_params[uNewCameraIndex]), (u8*)&newCamParams, sizeof(type_camera_parameters));
   g_pCurrentModel->iCurrentCamera = uNewCameraIndex;

   oldCamParams.szCameraBinProfileName[MAX_CAMERA_BIN_PROFILE_NAME-1] = 0;
   newCamParams.szCameraBinProfileName[MAX_CAMERA_BIN_PROFILE_NAME-1] = 0;

   log_line("Active camera changed from camera %d to camera %d", iOldCameraIndex, (int)uNewCameraIndex);
   log_line("Active camera profile changed from profile %d to profile %d", oldCamParams.iCurrentProfile, newCamParams.iCurrentProfile);
   log_line("Active camera type changed from (%d, forced %d) to (%d, forced %d)", oldCamParams.iCameraType, oldCamParams.iForcedCameraType, newCamParams.iCameraType, newCamParams.iForcedCameraType);
   log_line("Active camera bin file changed from (%d, %s) to (%d, %s)", oldCamParams.iCameraBinProfile, oldCamParams.szCameraBinProfileName, newCamParams.iCameraBinProfile, newCamParams.szCameraBinProfileName);
   
   // Do changes on the fly, if possible
   int iCountUpdates = _try_process_live_changes((int)uNewCameraIndex, &newCamParams, iOldCameraIndex, &oldCamParams);
   if ( iCountUpdates > 0 )
      log_line("Did %d live updates to camera.", iCountUpdates);
   else
      log_line("No live updates where done to camera.");
   // Do all the other remaining changes if any are remaining
   if ( 0 != memcmp(&newCamParams, &oldCamParams, sizeof(type_camera_parameters)) )
      video_sources_on_changed_camera_params(&(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), &oldCamParams);

   #if defined HW_PLATFORM_RASPBERRY
   if ( g_pCurrentModel->isActiveCameraVeye() )
       g_uTimeToSaveVeyeCameraParams = g_TimeNow + 5000;
   #endif
}


void process_camera_periodic_loop()
{
   // Save lastest camera params to flash (on camera)
   if ( 0 != g_uTimeToSaveVeyeCameraParams )
   if ( g_TimeNow > g_uTimeToSaveVeyeCameraParams )
   {
      #ifdef HW_PLATFORM_RASPBERRY
      if ( g_pCurrentModel->isActiveCameraVeye() )
      {
         log_line("Saving Veye camera parameters to flash memory.");
         int nBus = hardware_i2c_get_device_bus_number(I2C_DEVICE_ADDRESS_CAMERA_VEYE);
         char szComm[256];
         if ( g_pCurrentModel->isActiveCameraVeye307() )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f paramsave -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
         else if ( g_pCurrentModel->isActiveCameraVeye() )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f paramsave -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         hw_execute_bash_command(szComm, NULL);
      }
      #endif
      g_uTimeToSaveVeyeCameraParams = 0;
   }
}