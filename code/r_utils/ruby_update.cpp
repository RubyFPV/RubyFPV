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

#include <stdlib.h>
#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>
#include <ctype.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hardware_camera.h"
#include "../base/models_list.h"
#include "../base/hardware_procs.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../radio/radioflags.h"


bool gbQuit = false;
bool s_isVehicle = false;

int iMajor = 0;
int iMinor = 0;
int iBuild = 0;
   
void getSystemType()
{
   if ( hardware_is_vehicle() )
   {
      log_line("Detected system as vehicle/relay.");
      s_isVehicle = true;
   }
   else
   {
      log_line("Detected system as controller.");
      s_isVehicle = false;
   }

   log_line("");
   if ( s_isVehicle )
      log_line("| System detected as vehicle/relay.");
   else
      log_line("| System detected as controller.");
   log_line("");
}  

void validate_camera(Model* pModel)
{
   if ( hardware_isCameraVeye() || hardware_isCameraHDMI() )
   {
      pModel->video_params.iVideoWidth = 1920;
      pModel->video_params.iVideoHeight = 1080;
      if ( pModel->video_params.iVideoFPS > 30 )
         pModel->video_params.iVideoFPS = 30;
   }
}

void update_openipc_cpu(Model* pModel)
{
   hardware_set_default_sigmastar_cpu_freq();
   if ( NULL != pModel )
      pModel->processesPriorities.iFreqARM = DEFAULT_FREQ_OPENIPC_SIGMASTAR;
}


void do_update_to_117()
{
   log_line("Doing update to 11.7");
 
   if ( ! s_isVehicle )
   {
      load_ControllerSettings();
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->nGraphVideoRefreshInterval = DEFAULT_OSD_RADIO_GRAPH_REFRESH_PERIOD_MS;
      pCS->iRenderFPS = 15;
      save_ControllerSettings();
      load_Preferences();
      Preferences* pP = get_Preferences();
      pP->iMenusStacked = 1;
      save_Preferences();
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   pModel->osd_params.iRadioInterfacesGraphRefreshIntervalMs = DEFAULT_OSD_RADIO_GRAPH_REFRESH_PERIOD_MS;

   for( int i=0; i<MODEL_MAX_OSD_SCREENS; i++ )
   {
      if ( pModel->osd_params.osd_layout_preset[i] >= OSD_PRESET_DEFAULT )
         pModel->osd_params.osd_flags2[i] |= OSD_FLAG2_SHOW_VIDEO_FRAMES_STATS;
      if ( pModel->osd_params.osd_flags2[i] & OSD_FLAG2_SHOW_STATS_VIDEO )
      {
         pModel->osd_params.osd_flags2[i] &= ~OSD_FLAG2_SHOW_STATS_VIDEO;
         pModel->osd_params.osd_flags2[i] |= OSD_FLAG2_SHOW_VIDEO_FRAMES_STATS;
      }
   }

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      pModel->video_link_profiles[i].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
      pModel->video_link_profiles[i].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HQ<<8);
      pModel->video_link_profiles[i].iBlockDataPackets = DEFAULT_VIDEO_BLOCK_PACKETS_HQ;
      pModel->video_link_profiles[i].iBlockECs = DEFAULT_VIDEO_BLOCK_ECS_HQ;
      pModel->convertECPercentageToData(&(pModel->video_link_profiles[i]));

      pModel->video_link_profiles[i].iKeyframeMS = DEFAULT_VIDEO_KEYFRAME_AUTO;
      pModel->video_link_profiles[i].uProfileFlags &= ~VIDEO_PROFILE_FLAGS_MASK_NOISE; // zero noise

      pModel->video_link_profiles[i].uProfileFlags |= VIDEO_PROFILE_FLAG_USE_HIGHER_DATARATE;
      pModel->video_link_profiles[i].uProfileFlags &= ~VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK;
      pModel->video_link_profiles[i].uProfileFlags |= (((u32)0x01) << VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT);

      pModel->video_link_profiles[i].iIPQuantizationDelta = DEFAULT_VIDEO_H264_IPQUANTIZATION_DELTA;
   }

   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_PERF].iKeyframeMS = DEFAULT_VIDEO_KEYFRAME_AUTO_HP;
   pModel->video_link_profiles[VIDEO_PROFILE_LONG_RANGE].iKeyframeMS = DEFAULT_VIDEO_KEYFRAME_AUTO_LR;

   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].uProfileFlags &= ~VIDEO_PROFILE_FLAGS_MASK_NOISE;
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].uProfileFlags |= 1; // 3d noise

   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_PERF].uProfileFlags |= VIDEO_PROFILE_FLAG_USE_HIGHER_DATARATE;
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_PERF].uProfileFlags &= ~VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK;
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_PERF].uProfileFlags |= (((u32)0x02) << VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT);

   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].iIPQuantizationDelta = DEFAULT_VIDEO_H264_IPQUANTIZATION_DELTA_HQ;
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_PERF].iIPQuantizationDelta = DEFAULT_VIDEO_H264_IPQUANTIZATION_DELTA_HP;
   pModel->video_link_profiles[VIDEO_PROFILE_LONG_RANGE].iIPQuantizationDelta = DEFAULT_VIDEO_H264_IPQUANTIZATION_DELTA_LR;

   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HQ<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_PERF].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_PERF].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_USER].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_USER].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);

   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].iBlockDataPackets = DEFAULT_VIDEO_BLOCK_PACKETS_HQ;
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].iBlockECs = DEFAULT_VIDEO_BLOCK_ECS_HQ;

   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_PERF].iBlockDataPackets = DEFAULT_VIDEO_BLOCK_PACKETS_HP;
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_PERF].iBlockECs = DEFAULT_VIDEO_BLOCK_ECS_HP;

   pModel->video_link_profiles[VIDEO_PROFILE_LONG_RANGE].iBlockDataPackets = DEFAULT_VIDEO_BLOCK_PACKETS_LR;
   pModel->video_link_profiles[VIDEO_PROFILE_LONG_RANGE].iBlockECs = DEFAULT_VIDEO_BLOCK_ECS_LR;

   pModel->video_link_profiles[VIDEO_PROFILE_LONG_RANGE].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK;
   pModel->video_link_profiles[VIDEO_PROFILE_LONG_RANGE].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO;

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      pModel->convertECPercentageToData(&(pModel->video_link_profiles[i]));
   }

   log_line("Updated model VID %u (%s) to v11.7", pModel->uVehicleId, pModel->getLongName());
}


void do_update_to_116()
{
   log_line("Doing update to 11.6");
 
   if ( ! s_isVehicle )
   {
      load_ControllerSettings();
      reset_ControllerPriorities();
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->nPingClockSyncFrequency = DEFAULT_PING_FREQUENCY;
      pCS->iStreamerOutputMode = 1;
      pCS->iWaitFullFrameForOutput = 0;
      save_ControllerSettings();
      //load_Preferences();
      //Preferences* pP = get_Preferences();
      //save_Preferences();
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   for( int i=0; i<pModel->radioLinksParams.links_count; i++ )
   {
      int iInterface = -1;
      for( int k=0; k<pModel->radioInterfacesParams.interfaces_count; k++ )
      {
         if ( pModel->radioInterfacesParams.interface_link_id[k] == i )
         {
            iInterface = k;
            break;
         }
      }
      radio_hw_info_t* pRadioHWInfo = NULL;
      if ( iInterface != -1 )
         pRadioHWInfo = hardware_get_radio_info(iInterface);

      if ( (pRadioHWInfo == NULL) || hardware_radio_is_wifi_radio(pRadioHWInfo) )
         pModel->resetRadioLinkDataRatesAndFlags(i);
   }

   pModel->radioLinksParams.uGlobalRadioLinksFlags &= ~(MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS);
   pModel->radioRuntimeCapabilities.uFlagsRuntimeCapab &= ~MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED;

   pModel->resetProcessesParams();
   pModel->resetAdaptiveVideoParams(-1);

   for( int i=0; i<MODEL_CAMERA_PROFILES; i++ )
      pModel->validate_fps_and_exposure_settings(&(pModel->camera_params[0].profiles[i]), true);

   // Must be done after radio links updates so max video bitrate for each video profile is computed correctly
   pModel->resetVideoLinkProfiles();
   pModel->validateVideoProfilesMaxVideoBitrate();

   pModel->telemetry_params.flags = TELEMETRY_FLAGS_REQUEST_DATA_STREAMS | TELEMETRY_FLAGS_SPECTATOR_ENABLE;
   pModel->telemetry_params.flags |= TELEMETRY_FLAGS_ALLOW_ANY_VEHICLE_SYSID;

   log_line("Updated model VID %u (%s) to v11.6", pModel->uVehicleId, pModel->getLongName());
}

void do_update_to_115()
{
   log_line("Doing update to 11.5");
 
   if ( ! s_isVehicle )
   {
      load_ControllerSettings();
      reset_ControllerPriorities();
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->nPingClockSyncFrequency = DEFAULT_PING_FREQUENCY;
      pCS->iStreamerOutputMode = 1;
      pCS->iWaitFullFrameForOutput = 0;
      save_ControllerSettings();
      //load_Preferences();
      //Preferences* pP = get_Preferences();
      //save_Preferences();
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   pModel->resetProcessesParams();
   pModel->uDeveloperFlags &= ~DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_STREAM_TIMINGS;

   if ( hardware_board_is_openipc(hardware_getBoardType()) )
   if ( hardware_board_is_sigmastar(hardware_getBoardType()) )
   {
      for( int k=0; k<MODEL_MAX_CAMERAS; k++ )
      for( int i=0; i<MODEL_CAMERA_PROFILES; i++ )
      {
         if ( pModel->camera_params[k].profiles[i].iShutterSpeed > DEFAULT_OIPC_SHUTTERSPEED )
            pModel->camera_params[k].profiles[i].iShutterSpeed = DEFAULT_OIPC_SHUTTERSPEED; //milisec
      }
   }

   if ( hardware_board_is_sigmastar(hardware_getBoardType()) )
   if ( pModel->video_params.iVideoHeight < 1200 )
   if ( pModel->video_params.iVideoFPS < DEFAULT_VIDEO_FPS_OIPC_SIGMASTAR )
      pModel->video_params.iVideoFPS = DEFAULT_VIDEO_FPS_OIPC_SIGMASTAR;

   pModel->video_params.iH264Slices = DEFAULT_VIDEO_H264_SLICES;
   if ( hardware_board_is_openipc(hardware_getBoardType()) )
      pModel->video_params.iH264Slices = DEFAULT_VIDEO_H264_SLICES_OIPC;

   pModel->video_params.lowestAllowedAdaptiveVideoBitrate = DEFAULT_LOWEST_ALLOWED_ADAPTIVE_VIDEO_BITRATE;

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      pModel->video_link_profiles[i].uProfileFlags &= ~VIDEO_PROFILE_FLAG_RETRANSMISSIONS_AGGRESIVE;
      pModel->video_link_profiles[i].uProfileFlags |= VIDEO_PROFILE_FLAG_USE_LOWER_DR_FOR_EC_PACKETS;
      pModel->video_link_profiles[i].uProfileFlags &= ~VIDEO_PROFILE_FLAG_MASK_RETRANSMISSIONS_GUARD_MASK;
      pModel->video_link_profiles[i].uProfileFlags |= (VIDEO_PROFILE_FLAG_MASK_RETRANSMISSIONS_GUARD_MASK & (((u32)DEFAULT_VIDEO_END_FRAME_DETECTION_BUFFER_MS)<<8));

      pModel->video_link_profiles[i].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK | VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO;
      pModel->video_link_profiles[i].iDefaultFPS = 0;
      pModel->video_link_profiles[i].iDefaultLinkLoad = 0;
      pModel->video_link_profiles[i].uDummyVP1 = 0;
      pModel->video_link_profiles[i].uDummyVP2 = 0;
   }
   pModel->resetAdaptiveVideoParams(-1);

   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].iDefaultFPS = DEFAULT_VIDEO_FPS_PROFILE_HQ;
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_PERF].iDefaultFPS = DEFAULT_VIDEO_FPS_PROFILE_HP;
   pModel->video_link_profiles[VIDEO_PROFILE_LONG_RANGE].iDefaultFPS = DEFAULT_VIDEO_FPS_PROFILE_LR;
   pModel->video_link_profiles[VIDEO_PROFILE_USER].iDefaultFPS = DEFAULT_VIDEO_FPS_PROFILE_HP;

   for( int i=0; i<MODEL_CAMERA_PROFILES; i++ )
      pModel->validate_fps_and_exposure_settings(&(pModel->camera_params[0].profiles[i]), true);

   log_line("Updated model VID %u (%s) to v11.5", pModel->uVehicleId, pModel->getLongName());
}


void do_update_to_114()
{
   log_line("Doing update to 11.4");
 
   if ( ! s_isVehicle )
   {
      load_Preferences();
      Preferences* pP = get_Preferences();
      pP->nLogLevel = 0;  
      save_Preferences();
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   log_line("Updated model VID %u (%s) to v11.4", pModel->uVehicleId, pModel->getLongName());
}

void do_update_to_113()
{
   log_line("Doing update to 11.3");
 
   if ( ! s_isVehicle )
   {
     
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   pModel->video_params.iH264Slices = DEFAULT_VIDEO_H264_SLICES;
   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   pModel->video_params.iH264Slices = DEFAULT_VIDEO_H264_SLICES_OIPC;
   #endif

   pModel->video_params.iVideoFPS = DEFAULT_VIDEO_FPS;
   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   pModel->video_params.iVideoFPS = DEFAULT_VIDEO_FPS_OIPC;
   if ( hardware_board_is_sigmastar(hardware_getBoardType()) )
      pModel->video_params.iVideoFPS = DEFAULT_VIDEO_FPS_OIPC_SIGMASTAR;
   #endif

   pModel->video_params.uVideoExtraFlags |= VIDEO_FLAG_ENABLE_FOCUS_MODE_BW;
   pModel->resetAdaptiveVideoParams(-1);

   if ( hardware_board_is_openipc(hardware_getBoardType()) )
   {
      for( int k=0; k<MODEL_MAX_CAMERAS; k++ )
      for( int i=0; i<MODEL_CAMERA_PROFILES; i++ )
         pModel->camera_params[k].profiles[i].iShutterSpeed = DEFAULT_OIPC_SHUTTERSPEED; //milisec
   }

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      pModel->video_link_profiles[i].uTargetVideoBitrateBPS = DEFAULT_VIDEO_BITRATE;
      if ( ((hardware_getBoardType() & BOARD_TYPE_MASK) == BOARD_TYPE_PIZERO) ||
           ((hardware_getBoardType() & BOARD_TYPE_MASK) == BOARD_TYPE_PIZEROW) ||
           hardware_board_is_goke(hardware_getBoardType()) )
         pModel->video_link_profiles[i].uTargetVideoBitrateBPS = DEFAULT_VIDEO_BITRATE_PI_ZERO;
      if ( hardware_board_is_sigmastar(hardware_getBoardType()) )
         pModel->video_link_profiles[i].uTargetVideoBitrateBPS = DEFAULT_VIDEO_BITRATE_OPIC_SIGMASTAR;
   }


   pModel->radioRuntimeCapabilities.uSupportedMCSFlags = RADIO_FLAGS_FRAME_TYPE_DATA;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      pModel->radioLinksParams.uMaxLinkLoadPercent[i] = DEFAULT_RADIO_LINK_LOAD_PERCENT;
   }

   log_line("Updated model VID %u (%s) to v11.3", pModel->uVehicleId, pModel->getLongName());
}


void do_update_to_112()
{
   log_line("Doing update to 11.2");
 
   if ( ! s_isVehicle )
   {
      load_ControllerSettings();
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->nPingClockSyncFrequency = DEFAULT_PING_FREQUENCY;
      pCS->iStreamerOutputMode = 0;
      pCS->iEasterEgg1 = 0;
      save_ControllerSettings();

      load_Preferences();
      Preferences* pP = get_Preferences();
      pP->iDebugMaxPacketSize = MAX_VIDEO_PACKET_DATA_SIZE;
      pP->iLanguage = 1;
      save_Preferences();

      for( int i=0; i<hardware_serial_get_ports_count(); i++ )
      {
         hw_serial_port_info_t* pInfo = hardware_get_serial_port_info(i);
         if ( NULL == pInfo )
            continue;
         if ( (pInfo->iPortUsage > 0) && (pInfo->iPortUsage < SERIAL_PORT_USAGE_DATA_LINK) )
         {
            pInfo->iPortUsage = SERIAL_PORT_USAGE_TELEMETRY;
            if ( pInfo->lPortSpeed <= 0 )
               pInfo->lPortSpeed = DEFAULT_FC_TELEMETRY_SERIAL_SPEED;
            hardware_serial_save_configuration();
         }
      }
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   for( int i=0; i<pModel->hardwareInterfacesInfo.serial_port_count; i++ )
   {
      u32 uUsage = pModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & 0xFF;
      if ( (uUsage > 0) && (uUsage < SERIAL_PORT_USAGE_DATA_LINK) )
      {
         pModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] = pModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & 0xFFFFFF00;
         pModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] |= SERIAL_PORT_USAGE_TELEMETRY;
      }
   }

   pModel->uDeveloperFlags &= ~DEVELOPER_FLAGS_BIT_ENABLE_DEVELOPER_MODE;

   u32 uPITTemp = (pModel->hwCapabilities.uHWFlags >> 8) & 0xFF;
   if ( (uPITTemp < 10) || (uPITTemp > 100) )
   {
      pModel->hwCapabilities.uHWFlags &= ~(0x0000FF00);
      pModel->hwCapabilities.uHWFlags |= (((u32)75) << 8);
   }

   pModel->osd_params.iRadioInterfacesGraphRefreshIntervalMs = DEFAULT_OSD_RADIO_GRAPH_REFRESH_PERIOD_MS;
   for( int i=0; i<MODEL_MAX_OSD_SCREENS; i++ )
   {
      if ( pModel->osd_params.osd_layout_preset[i] >= OSD_PRESET_COMPACT )
      if ( i < 3 )
         pModel->osd_params.osd_flags2[i] |= OSD_FLAG2_SHOW_TX_POWER;
   }

   pModel->video_params.uVideoExtraFlags = 0;
   pModel->video_params.iCurrentVideoProfile = VIDEO_PROFILE_HIGH_QUALITY;
   pModel->video_params.iVideoWidth = DEFAULT_VIDEO_WIDTH;
   pModel->video_params.iVideoHeight = DEFAULT_VIDEO_HEIGHT;

   pModel->video_params.iVideoFPS = DEFAULT_VIDEO_FPS;
   if ( hardware_board_is_openipc(hardware_getBoardType()) )
      pModel->video_params.iVideoFPS = DEFAULT_VIDEO_FPS_OIPC;
   if ( hardware_board_is_sigmastar(hardware_getBoardType()) )
      pModel->video_params.iVideoFPS = DEFAULT_VIDEO_FPS_OIPC_SIGMASTAR;

   if ( hardware_isCameraVeye() || hardware_isCameraHDMI() )
   {
      pModel->video_params.iVideoWidth = 1920;
      pModel->video_params.iVideoHeight = 1080;
      pModel->video_params.iVideoFPS = 30;
   }
   
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      pModel->video_link_profiles[i].iAdaptiveAdjustmentStrength = DEFAULT_VIDEO_PARAMS_ADJUSTMENT_STRENGTH;

      pModel->video_link_profiles[i].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK;
      pModel->video_link_profiles[i].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO;
   }

   for( int k=0; k<MODEL_MAX_CAMERAS; k++ )
   {
      pModel->camera_params[k].iCameraBinProfile = 0;
      pModel->camera_params[k].szCameraBinProfileName[0] = 0;
   }

   pModel->resetVideoLinkProfiles();

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      pModel->radioLinksParams.link_radio_flags[i] = RADIO_FLAGS_USE_LEGACY_DATARATES | RADIO_FLAGS_FRAME_TYPE_DATA;
      // Auto data rates
      pModel->radioLinksParams.downlink_datarate_video_bps[i] = 0;
      pModel->radioLinksParams.downlink_datarate_data_bps[i] = DEFAULT_RADIO_DATARATE_DATA;
      pModel->radioLinksParams.uplink_datarate_video_bps[i] = 0;
      pModel->radioLinksParams.uplink_datarate_data_bps[i] = DEFAULT_RADIO_DATARATE_DATA;
      pModel->radioLinksParams.uMaxLinkLoadPercent[i] = DEFAULT_RADIO_LINK_LOAD_PERCENT;
   }

   pModel->radioLinksParams.uGlobalRadioLinksFlags &= ~(MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS);
   pModel->radioRuntimeCapabilities.uFlagsRuntimeCapab = 0;
   pModel->validateRadioSettings();
   if ( hardware_board_is_openipc(hardware_getBoardType()) )
      hw_execute_bash_command("rm -rf /etc/init.d/S*majestic", NULL);
   log_line("Updated model VID %u (%s) to v11.2", pModel->uVehicleId, pModel->getLongName());
}


void do_update_to_111()
{
   log_line("Doing update to 11.1");
 
   if ( ! s_isVehicle )
   {
      load_Preferences();
      Preferences* pP = get_Preferences();
      pP->uEnabledAlarms &= ~ (ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD | ALARM_ID_CONTROLLER_CPU_RX_LOOP_OVERLOAD | ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD_RECORDING);
      pP->iVideoDestination = prefVideoDestination_Mem;
      save_Preferences();
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   hardware_camera_set_default_oipc_calibration(pModel->getActiveCameraType());
   pModel->resetAudioParams();
   #endif

   pModel->hwCapabilities.uHWFlags &= ~(0x0000FF00);
   pModel->hwCapabilities.uHWFlags |= (((u32)75) << 8);

   pModel->uModelPersistentStatusFlags = 0;
   pModel->radioInterfacesParams.uFlagsRadioInterfaces = 0;

   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].iBlockDataPackets = DEFAULT_VIDEO_BLOCK_PACKETS_HQ;
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_PERF].iBlockDataPackets = DEFAULT_VIDEO_BLOCK_PACKETS_HP;
   pModel->video_link_profiles[VIDEO_PROFILE_USER].iBlockDataPackets = DEFAULT_VIDEO_BLOCK_PACKETS_HQ;
   
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].iECPercentage = DEFAULT_VIDEO_EC_RATE_HQ;
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_PERF].iECPercentage = DEFAULT_VIDEO_EC_RATE_HP;
   pModel->video_link_profiles[VIDEO_PROFILE_USER].iECPercentage = DEFAULT_VIDEO_EC_RATE_HQ;
   
   for( int k=0; k<MODEL_MAX_CAMERAS; k++ )
   {
      pModel->camera_params[k].iCameraBinProfile = 0;
      pModel->camera_params[k].szCameraBinProfileName[0] = 0;
   }

   log_line("Updated model VID %u (%s) to v11.1", pModel->uVehicleId, pModel->getLongName());
}

void do_update_to_110()
{
   log_line("Doing update to 11.0");
 
   if ( ! s_isVehicle )
   {
     
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_PERF].iBlockDataPackets = DEFAULT_VIDEO_BLOCK_PACKETS_HP;
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].iECPercentage = DEFAULT_VIDEO_EC_RATE_HQ;
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_PERF].iECPercentage = DEFAULT_VIDEO_EC_RATE_HP;
   pModel->video_link_profiles[VIDEO_PROFILE_USER].iECPercentage = DEFAULT_VIDEO_EC_RATE_HQ;
   
   pModel->radioInterfacesParams.iAutoVehicleTxPower = 0;

   log_line("Updated model VID %u (%s) to v11.0", pModel->uVehicleId, pModel->getLongName());
}


void do_update_to_108()
{
   log_line("Doing update to 10.8");
 
   if ( ! s_isVehicle )
   {
      load_ControllerSettings();
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->iHDMIVSync = 1;
      save_ControllerSettings();
      load_Preferences();
      Preferences* pP = get_Preferences();
      pP->nLogLevel = 1;
      pP->iLanguage = 1;
      #if defined (HW_PLATFORM_RADXA)
      pP->iOSDFont = 1;
      pP->iOSDFontBold = 1;
      #endif
      save_Preferences();
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;
   
   u32 uBoardSubType = (pModel->hwCapabilities.uBoardType & BOARD_SUBTYPE_MASK) >> BOARD_SUBTYPE_SHIFT;
   if ( (uBoardSubType != 0) && (uBoardSubType < 10) )
      uBoardSubType *= 10;
   if ( uBoardSubType > 255 )
      uBoardSubType = 255;
   pModel->hwCapabilities.uBoardType &= ~(BOARD_SUBTYPE_MASK);
   pModel->hwCapabilities.uBoardType |= (uBoardSubType << BOARD_SUBTYPE_SHIFT);

   for( int i=0; i<MODEL_MAX_OSD_SCREENS; i++ )
   {
      pModel->osd_params.osd_flags3[i] &= ~(OSD_FLAG3_SHOW_RADIO_LINK_QUALITY_NUMBERS_DBM | OSD_FLAG3_SHOW_RADIO_LINK_QUALITY_NUMBERS_SNR | OSD_FLAG3_SHOW_RADIO_LINK_QUALITY_NUMBERS_PERCENT);
      if ( pModel->osd_params.osd_flags2[i] & OSD_FLAG2_SHOW_RADIO_LINK_QUALITY_NUMBERS )
         pModel->osd_params.osd_flags3[i] |= OSD_FLAG3_SHOW_RADIO_LINK_QUALITY_NUMBERS_DBM | OSD_FLAG3_SHOW_RADIO_LINK_QUALITY_NUMBERS_SNR;
   }
   log_line("Updated model VID %u (%s) to v10.8", pModel->uVehicleId, pModel->getLongName());
}


void do_update_to_107()
{
   log_line("Doing update to 10.7");
 
   if ( ! s_isVehicle )
   {
      load_Preferences();
      Preferences* pP = get_Preferences();
      pP->nLogLevel = 1;
      save_Preferences();
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   pModel->osd_params.uFlags |= OSD_BIT_FLAGS_MUST_CHOOSE_PRESET;

   log_line("Updated model VID %u (%s) to v10.7", pModel->uVehicleId, pModel->getLongName());
}


void do_update_to_106()
{
   log_line("Doing update to 10.6");
 
   if ( ! s_isVehicle )
   {
      load_ControllerSettings();
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->iDeveloperMode = 0;
      save_ControllerSettings();
      load_Preferences();
      Preferences* pP = get_Preferences();
      pP->nLogLevel = 1;
      save_Preferences();
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   hardware_camera_check_set_oipc_sensor();
   pModel->resetAudioParams();
   #endif

   pModel->uDeveloperFlags |= DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS;
   pModel->resetOSDStatsFlags();

   if ( hardware_board_is_sigmastar(hardware_getBoardType()) )
   {
      for( int i=0; i<MODEL_MAX_CAMERAS; i++ )
      for( int k=0; k<MODEL_CAMERA_PROFILES-1; k++ )
            pModel->camera_params[i].profiles[k].iShutterSpeed = DEFAULT_OIPC_SHUTTERSPEED; //milisec
   }

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      pModel->video_link_profiles[i].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO;
      pModel->video_link_profiles[i].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_AUTO_EC_SCHEME;
      pModel->video_link_profiles[i].uProfileFlags = 0;
   }
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_PERF].uProfileFlags = 0; // lowest

   pModel->video_params.iCurrentVideoProfile = VIDEO_PROFILE_HIGH_QUALITY;
   pModel->osd_params.uFlags &= ~(OSD_BIT_FLAGS_SHOW_TEMPS_F);
   pModel->osd_params.uFlags |= OSD_BIT_FLAGS_SHOW_FLIGHT_END_STATS;

   log_line("Updated model VID %u (%s) to v10.6", pModel->uVehicleId, pModel->getLongName());
}

void do_update_to_105()
{
   log_line("Doing update to 10.5");
 
   if ( ! s_isVehicle )
   {
      load_ControllerSettings();
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->iStreamerOutputMode = 0;
      pCS->iDeveloperMode = 0;
      save_ControllerSettings();
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   hardware_camera_check_set_oipc_sensor();
   pModel->resetAudioParams();
   #endif

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      pModel->video_link_profiles[i].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO;
      pModel->video_link_profiles[i].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_AUTO_EC_SCHEME;
   }
   log_line("Updated model VID %u (%s) to v10.5", pModel->uVehicleId, pModel->getLongName());
}


void do_update_to_104()
{
   log_line("Doing update to 10.4");
 
   if ( ! s_isVehicle )
   {
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   for( int i=0; i<MODEL_MAX_OSD_SCREENS; i++ )
      pModel->osd_params.osd_flags3[i] &= ~((u32)(((u32)0x01)<<12));


   pModel->osd_params.uFlags &= ~(OSD_BIT_FLAGS_SHOW_TEMPS_F);

   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HQ<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_PERF].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_PERF].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_USER].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_USER].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      pModel->video_link_profiles[i].uTargetVideoBitrateBPS = DEFAULT_VIDEO_BITRATE;
      if ( ((hardware_getBoardType() & BOARD_TYPE_MASK) == BOARD_TYPE_PIZERO) ||
           ((hardware_getBoardType() & BOARD_TYPE_MASK) == BOARD_TYPE_PIZEROW) ||
           hardware_board_is_goke(hardware_getBoardType()) )
         pModel->video_link_profiles[i].uTargetVideoBitrateBPS = DEFAULT_VIDEO_BITRATE_PI_ZERO;
      if ( hardware_board_is_sigmastar(hardware_getBoardType()) )
         pModel->video_link_profiles[i].uTargetVideoBitrateBPS = DEFAULT_VIDEO_BITRATE_OPIC_SIGMASTAR;
   }

   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_PERF].uTargetVideoBitrateBPS = DEFAULT_HP_VIDEO_BITRATE;
   
   pModel->validateRadioSettings();

   pModel->processesPriorities.iThreadPriorityRadioRx = DEFAULT_PRIORITY_VEHICLE_THREAD_RADIO_RX;
   pModel->processesPriorities.iThreadPriorityRadioTx = DEFAULT_PRIORITY_VEHICLE_THREAD_RADIO_TX;
   pModel->processesPriorities.iThreadPriorityRouter = DEFAULT_PRIORITY_VEHICLE_THREAD_ROUTER;

   log_line("Updated model VID %u (%s) to v10.4", pModel->uVehicleId, pModel->getLongName());
}


void do_generic_update()
{
   log_line("Doing generic update step");

   #if defined (HW_PLATFORM_RASPBERRY)
   if( access( "ruby_capture_raspi", R_OK ) != -1 )
      hw_execute_bash_command("cp -rf ruby_capture_raspi /opt/vc/bin/raspivid", NULL);

   if( access( "ruby_capture_veye", R_OK ) != -1 )
      hw_execute_bash_command("cp -rf ruby_capture_veye /usr/local/bin/veye_raspivid", NULL);

   if( access( "onyxfpv_capture_raspi", R_OK ) != -1 )
      hw_execute_bash_command("cp -rf onyxfpv_capture_raspi /opt/vc/bin/raspivid", NULL);

   if( access( "onyxfpv_capture_veye", R_OK ) != -1 )
      hw_execute_bash_command("cp -rf onyxfpv_capture_veye /usr/local/bin/veye_raspivid", NULL);
   #endif
   
   #if defined (HW_PLATFORM_RASPBERRY)
   hw_execute_bash_command("cp -rf ruby_update.log /boot/ 2>/dev/null", NULL);
   hw_execute_bash_command("cp -rf onyxfpv_update.log /boot/ 2>/dev/null", NULL);
   #endif
   #if defined (HW_PLATFORM_RADXA)
   hw_execute_bash_command("cp -rf ruby_update.log /config/ 2>/dev/null", NULL);
   hw_execute_bash_command("cp -rf onyxfpv_update.log /config/ 2>/dev/null", NULL);
   #endif
   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   hw_execute_bash_command("cp -rf ruby_update.log /root/ruby/ 2>/dev/null", NULL);
   hw_execute_bash_command("cp -rf onyxfpv_update.log /root/onyxfpv/ 2>/dev/null", NULL);
   #endif
}

void handle_sigint(int sig) 
{ 
   log_line("--------------------------");
   log_line("Caught signal to stop: %d", sig);
   log_line("--------------------------");
   gbQuit = true;
} 

int main(int argc, char *argv[])
{
   if ( strcmp(argv[argc-1], "-ver") == 0 )
   {
      printf("%d.%d (b-%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR, SYSTEM_SW_BUILD_NUMBER);
      return 0;
   }

   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   log_init("RubyUpdate");

   hardware_detectBoardAndSystemType();

   if ( argc >= 3 )
   {
      iMajor = atoi(argv[1]);
      iMinor = atoi(argv[2]);
      if ( iMinor >= 10 )
         iMinor /= 10;
      if ( (iMajor < 9) || ((iMajor == 9) && (iMinor <= 4)) )
         printf("There is a new driver added for RTL8812EU cards. Please do this update again to complete the drivers instalation.\n");
      return 0;
   }

   char szUpdateCommand[1024];

   szUpdateCommand[0] = 0;
   if ( argc >= 2 )
      strcpy(szUpdateCommand,argv[1]);

   log_line("Executing update commands with parameter: [%s]", szUpdateCommand);

   if ( NULL != strstr(szUpdateCommand, "pre" ) )
   {
      log_line("Pre-update step...");
      log_line("Done executing pre-update step.");
   }

   getSystemType();

   u32 uCurrentVersion = 0;
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_VERSION);
   FILE* fd = fopen(szFile, "r");
   if ( NULL != fd )
   {
      if ( 1 != fscanf(fd, "%u", &uCurrentVersion) )
         uCurrentVersion = 0;
      fclose(fd);
   }
   else
      log_softerror_and_alarm("Failed to read current version id from file: %s",FILE_CONFIG_CURRENT_VERSION);
 
   iMajor = (int)((uCurrentVersion >> 8) & 0xFF);
   iMinor = (int)(uCurrentVersion & 0xFF);
   iBuild = (int)(uCurrentVersion>>16);
   if ( uCurrentVersion == 0 )
   {
      char szVersionPresent[32];
      szVersionPresent[0] = 0;
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_INFO_LAST_UPDATE);
      fd = fopen(szFile, "r");
      if ( NULL != fd )
      {
         if ( 1 != fscanf(fd, "%s", szVersionPresent) )
            szVersionPresent[0] = 0;
         fclose(fd);
      }
      if ( 0 == szVersionPresent[0] )
      {
         fd = try_open_base_version_file(NULL);

         if ( NULL != fd )
         {
            if ( 1 != fscanf(fd, "%s", szVersionPresent) )
               szVersionPresent[0] = 0;
            fclose(fd);
         }
      }
      if ( 0 == szVersionPresent[0] )
         strcpy(szVersionPresent,"1.0");

      char* p = &szVersionPresent[0];
      while ( *p )
      {
         if ( isdigit(*p) )
           iMajor = iMajor * 10 + ((*p)-'0');
         if ( (*p) == '.' )
           break;
         p++;
      }
      if ( 0 != *p )
      {
         p++;
         while ( *p )
         {
            if ( isdigit(*p) )
               iMinor = iMinor * 10 + ((*p)-'0');
            if ( (*p) == '.' )
              break;
            p++;
         }
      }
      if ( iMinor > 9 )
         iMinor = iMinor/10;
   }

   if ( iMinor > 9 )
      iMinor = iMinor/10;

   log_line("Applying update on existing version: %d.%d (b-%d)", iMajor, iMinor, iBuild);
   log_line("Updating to version: %d.%d (b-%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR, SYSTEM_SW_BUILD_NUMBER);

   do_generic_update();

   loadAllModels();

   u32 uOldSwVersion = 0;
   Model* pModel = getCurrentModel();
   if ( NULL != pModel )
   {
      log_line("Current model software version: %d.%d, (b-%d)", get_sw_version_major(pModel), get_sw_version_minor(pModel), get_sw_version_build(pModel));
      uOldSwVersion = pModel->sw_version;
   }

   if ( (iMajor < 10) || (iMajor == 10 && iMinor <= 4) )
      do_update_to_104();
   if ( (iMajor < 10) || (iMajor == 10 && iMinor <= 5) )
      do_update_to_105();
   if ( (iMajor < 10) || (iMajor == 10 && iMinor <= 6) )
      do_update_to_106();
   if ( (iMajor < 10) || (iMajor == 10 && iMinor <= 7) )
      do_update_to_107();
   if ( (iMajor < 10) || (iMajor == 10 && iMinor <= 8) )
      do_update_to_108();
   if ( iMajor < 11 )
      do_update_to_110();
   if ( (iMajor < 11) || (iMajor == 11 && iMinor <= 1) )
      do_update_to_111();
   if ( (iMajor < 11) || (iMajor == 11 && iMinor <= 2) )
      do_update_to_112();
   if ( (iMajor < 11) || (iMajor == 11 && iMinor <= 3) )
      do_update_to_113();
   if ( (iMajor < 11) || (iMajor == 11 && iMinor <= 4) )
      do_update_to_114();
   if ( (iMajor < 11) || (iMajor == 11 && iMinor <= 5) )
      do_update_to_115();
   if ( (iMajor < 11) || (iMajor == 11 && iMinor <= 6) )
      do_update_to_116();
   if ( (iMajor < 11) || (iMajor == 11 && iMinor <= 7) )
      do_update_to_117();

   saveCurrentModel();

   // For vehicles older than 11.6, stop telemetry process first as it would fail to read the new model (before reboot) and try to save it in old format
   if ( (iMajor < 11) || ((iMajor == 11) && (iMinor < 6)) ||
        (uOldSwVersion == 0) ||
        (((uOldSwVersion>>8) & 0xFF) < 11) ||
        ( (((uOldSwVersion>>8) & 0xFF) == 11) && ((uOldSwVersion & 0xFF) < 6) ) )
   {
      log_line("Vehicle is older than 11.6, needs to restart telemetry.");
      char szOutput[4096];
      hw_execute_bash_command("ps -aef | grep tx_telemetry", szOutput);
      log_line("Running telemetry processes: [%s]", szOutput);
      hw_stop_process("ruby_tx_telemetry");
      hardware_sleep_ms(100);
      hw_execute_bash_command("ps -aef | grep tx_telemetry", szOutput);
      log_line("Running telemetry processes: [%s]", szOutput);
      saveCurrentModel();
      hw_execute_ruby_process(NULL, "ruby_tx_telemetry", "-log", NULL);
      // Wait for it to start
      for( int i=0; i<10; i++ )
      {
         log_line("Wait a little");
         hardware_sleep_ms(100);
      }
   }
   else
      log_line("Vehicle is newer than 11.5, no need to restart telemetry.");

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   hw_execute_bash_command("rm -rf /usr/sbin/ruby_update_* 2>/dev/null", NULL);
   hw_execute_bash_command("rm -rf /usr/sbin/ruby_alive 2>/dev/null", NULL);
   hw_execute_bash_command("rm -rf /usr/sbin/majestic 2>/dev/null", NULL);
   #endif
   
   log_line("Update finished.");
   return (0);
} 