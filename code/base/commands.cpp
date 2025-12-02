/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga
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

#include "base.h"
#include "config.h"
#include "commands.h"
#include "../radio/radiopackets2.h"

const char* commands_get_description(u8 command_type)
{
   static char szCommandDesc[64];
   szCommandDesc[0] = 0;

   command_type = command_type & COMMAND_TYPE_MASK;

   switch (command_type)
   {
      case COMMAND_ID_SET_VEHICLE_TYPE: strcpy(szCommandDesc, "Set_Vehicle_Type"); break;
      case COMMAND_ID_SET_RADIO_LINK_FREQUENCY: strcpy(szCommandDesc, "Set_Radio_Link_Frequency"); break;
      case COMMAND_ID_SET_RADIO_LINK_FLAGS: strcpy(szCommandDesc, "Set_Radio_Link_Flags"); break;
      case COMMAND_ID_SET_RADIO_LINK_FLAGS_CONFIRMATION: strcpy(szCommandDesc, "RadioLink_Flags_Changed_Confirmation"); break;
      case COMMAND_ID_SET_RADIO_CARD_MODEL: strcpy(szCommandDesc, "Set_Radio_Card_Model"); break;
      case COMMAND_ID_SET_RADIO_INTERFACE_CAPABILITIES: strcpy(szCommandDesc, "Set_Radio_Capabilities_Flags"); break;
      case COMMAND_ID_SET_RADIO_LINK_DATARATES: strcpy(szCommandDesc, "Set_RadioLink_Datarates"); break;
      case COMMAND_ID_SET_MODEL_FLAGS: strcpy(szCommandDesc, "Set_Model_Flags"); break;
      case COMMAND_ID_SET_SIK_PACKET_SIZE: strcpy(szCommandDesc, "Set_SiK_Packet_Size"); break;
      case COMMAND_ID_RESET_RADIO_LINK: strcpy(szCommandDesc, "Reset_Radio_Link"); break;
      case COMMAND_ID_SET_PIT_AUTO_TX_POWERS_FLAGS: strcpy(szCommandDesc, "SetPITAutoTxPowersFlags"); break;
      case COMMAND_ID_GET_USB_INFO: strcpy(szCommandDesc, "Get_USB_Info"); break;
      case COMMAND_ID_SET_RADIO_LINK_CAPABILITIES: strcpy(szCommandDesc, "Set_Radio_Link_Capabilities"); break;
      case COMMAND_ID_GET_USB_INFO2: strcpy(szCommandDesc, "Get_USB_Info2"); break;
      case COMMAND_ID_SET_CAMERA_PROFILE: strcpy(szCommandDesc, "Set_Camera_Profile"); break;
      case COMMAND_ID_SET_CAMERA_PARAMETERS: strcpy(szCommandDesc, "Set_Camera_Params"); break;
      case COMMAND_ID_SET_CURRENT_CAMERA: strcpy(szCommandDesc, "Set_Current_Camera"); break;
      case COMMAND_ID_SET_OVERCLOCKING_PARAMS: strcpy(szCommandDesc, "SetOverclockingParams"); break;
      case COMMAND_ID_FORCE_CAMERA_TYPE: strcpy(szCommandDesc, "Set_Force_Camera_Type"); break;
      case COMMAND_ID_SET_OSD_CURRENT_LAYOUT: strcpy(szCommandDesc, "Set_Current_OSD_Layout"); break;
      case COMMAND_ID_GET_CORE_PLUGINS_INFO: strcpy(szCommandDesc, "Get_Core_Plugins_Info"); break;
      case COMMAND_ID_SET_THREADS_PRIORITIES: strcpy(szCommandDesc, "Set_Threads_Priorities"); break;
      case COMMAND_ID_GET_SIK_CONFIG: strcpy(szCommandDesc, "Get_SiK_Config"); break;
      case COMMAND_ID_SET_RADIO_LINKS_FLAGS: strcpy(szCommandDesc, "Set_Radio_Links_Flags"); break;
      case COMMAND_ID_ROTATE_RADIO_LINKS: strcpy(szCommandDesc, "Rotate_Radio_Links"); break;
      case COMMAND_ID_SWAP_RADIO_INTERFACES: strcpy(szCommandDesc, "Swap_Radio_Interfaces"); break;
      case COMMAND_ID_SET_RELAY_PARAMETERS: strcpy(szCommandDesc, "Set_Relay_Parameters"); break;
      case COMMAND_ID_SET_SERIAL_PORTS_INFO: strcpy(szCommandDesc, "Set_Serial_Ports_Info"); break;
      case COMMAND_ID_SET_AUDIO_PARAMS: strcpy(szCommandDesc, "Set_Audio_Params"); break;
      case COMMAND_ID_SET_TEMPERATURE_THRESHOLD: strcpy(szCommandDesc, "Set_Temperature_Threshold"); break;
      case COMMAND_ID_RESET_ALL_TO_DEFAULTS: strcpy(szCommandDesc, "ResetToDefaults"); break;
      case COMMAND_ID_SET_TX_PIT_MODE: strcpy(szCommandDesc, "SetTxPitMode"); break;
      case COMMAND_ID_SET_VIDEO_PARAMETERS: strcpy(szCommandDesc, "Set_Video_Params"); break;
      case COMMAND_ID_SET_TELEMETRY_PARAMETERS: strcpy(szCommandDesc, "Set_Telemetry_Params"); break;
      case COMMAND_ID_SET_TX_POWERS: strcpy(szCommandDesc, "Set_TxPowers"); break;
      case COMMAND_ID_SET_GPS_INFO: strcpy(szCommandDesc, "Set_GPS_Info"); break;
      case COMMAND_ID_SET_FUNCTIONS_TRIGGERS_PARAMS: strcpy(szCommandDesc, "Set_Functions_Triggers_Params"); break;
      case COMMAND_ID_SET_RC_PARAMS: strcpy(szCommandDesc, "Set_RcParams"); break;
      case COMMAND_ID_SET_VEHICLE_NAME: strcpy(szCommandDesc, "Set_VehicleName"); break;
      case COMMAND_ID_REBOOT: strcpy(szCommandDesc, "Reboot"); break;
      case COMMAND_ID_FACTORY_RESET: strcpy(szCommandDesc, "Factory Reset"); break;
      case COMMAND_ID_SET_ENABLE_DHCP: strcpy(szCommandDesc, "Set_Enable_DHCP"); break;
      case COMMAND_ID_SET_ALL_PARAMS: strcpy(szCommandDesc, "Set_All_Params"); break;
      case COMMAND_ID_GET_ALL_PARAMS_ZIP: strcpy(szCommandDesc, "Get_All_Params_Zip"); break;
      case COMMAND_ID_SET_OSD_PARAMS: strcpy(szCommandDesc, "SetOSDParams"); break;
      case COMMAND_ID_SET_ALARMS_PARAMS: strcpy(szCommandDesc, "SetAlarmsParams"); break;
      case COMMAND_ID_GET_CURRENT_VIDEO_CONFIG: strcpy(szCommandDesc, "Get_Current_Video_Config"); break;
      case COMMAND_ID_GET_MODULES_INFO: strcpy(szCommandDesc, "Get_Modules_Info"); break;
      case COMMAND_ID_GET_MEMORY_INFO: strcpy(szCommandDesc, "Get_Memory_Info"); break;
      case COMMAND_ID_GET_CPU_INFO: strcpy(szCommandDesc, "Get_CPU_Info"); break;
      case COMMAND_ID_SET_RC_CAMERA_PARAMS: strcpy(szCommandDesc, "Set_Camera_RC_Params"); break;
      case COMMAND_ID_UPLOAD_SW_TO_VEHICLE63: strcpy(szCommandDesc, "Upload_SW_To_Vehicle_2"); break;
      case COMMAND_ID_UPLOAD_FILE_SEGMENT: strcpy(szCommandDesc, "Upload_File_Segment"); break;
      case COMMAND_ID_SET_RXTX_SYNC_TYPE: strcpy(szCommandDesc, "Set_RxTx_Sync_Type"); break;
      case COMMAND_ID_RESET_CPU_SPEED: strcpy(szCommandDesc, "Reset_CPU_Speed"); break;
      case COMMAND_ID_SET_DEVELOPER_FLAGS: strcpy(szCommandDesc, "Set_Developer_Flags"); break;
      case COMMAND_ID_RESET_ALL_DEVELOPER_FLAGS: strcpy(szCommandDesc, "Reset_All_Developer_Flags"); break;
      case COMMAND_ID_DEBUG_GET_TOP: strcpy(szCommandDesc, "Debug_Get_TOP"); break;
      case COMMAND_ID_SET_ENCRYPTION_PARAMS: strcpy(szCommandDesc, "Set Encryption Params"); break;
      case COMMAND_ID_UPLOAD_CALIBRATION_FILE: strcpy(szCommandDesc, "Upload_Calibration_File"); break;
      case COMMAND_ID_DOWNLOAD_FILE: strcpy(szCommandDesc, "Download_File"); break;
      case COMMAND_ID_DOWNLOAD_FILE_SEGMENT: strcpy(szCommandDesc, "Download_File_Segment"); break;
      case COMMAND_ID_CLEAR_LOGS: strcpy(szCommandDesc, "Clear_Logs"); break;
      case COMMAND_ID_SET_VEHICLE_BOARD_TYPE: strcpy(szCommandDesc, "Set_Vehicle_Board_Type"); break;
      case COMMAND_ID_SET_TELEMETRY_TYPE_AND_PORT: strcpy(szCommandDesc, "Set_TelemetryTypeAndPort"); break;
      case COMMAND_ID_GET_CPU_PROCS_INFO: strcpy(szCommandDesc, "Get_CPU_Procs_Info"); break;
      default:
         sprintf(szCommandDesc, "%d", command_type);
         break;
   }
   return szCommandDesc;
}
