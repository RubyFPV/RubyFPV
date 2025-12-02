#pragma once
#include <stdarg.h>
#include "../base/core_plugins_settings.h"

#define COMMAND_ID_SET_VEHICLE_TYPE 2
// Has no particular input/response structure

#define COMMAND_ID_SET_VEHICLE_NAME 3

#define COMMAND_ID_SET_TX_POWERS 4
// byte 0: count interfaces
// N-bytes: power for each card (raw power values)

// Used only for low rate links, 433-915mhz bands and inactive links.
// For high capacity active links, use test link functionality to change frequency for 2.4/5.8ghz
#define COMMAND_ID_SET_RADIO_LINK_FREQUENCY 5
// byte 0..2: frequency, in Khz
// byte 3: link index

#define COMMAND_ID_SET_RADIO_LINK_CAPABILITIES 6
// has a u32 param
// byte 0: link index
// byte 1-3: link_capabilities_flags (only lowest 3 bytes of it) (tx/rx/relay - video/data)
// Capability flag RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY is never set, just read

#define COMMAND_ID_SET_RADIO_LINK_FLAGS 7
// Starting with 8.1 is used now only for SiK links
// u32 - link index
// u32 - link radio flags (MCS flags if any, STBC, adaptive rates, SIK flags, etc)
// int - datarate video  -1..-n MCS or 0..x classic, in bps
// int - datarate data 

#define COMMAND_ID_SET_RADIO_LINK_FLAGS_CONFIRMATION 8
// param is radio link id

#define COMMAND_ID_SET_PIT_AUTO_TX_POWERS_FLAGS 9
// param: byte 0: PIT flags
// param: byte 1: threshold temp
// param: byte 2.0: auto adjust vehicle power
// param: byte 2.1: auto adjust controller power

#define COMMAND_ID_SET_RADIO_INTERFACE_CAPABILITIES 10
// param: byte 0: radio interface index
// param: byte 1..4: 3 bytes of interface capabilities flags

#define COMMAND_ID_SET_RADIO_CARD_MODEL 11
// param: low byte: card index, second byte: user set card model (if 0xFF, then autodetect it again)
// response has the new card model in command_response_param if autodetect was requested

#define COMMAND_ID_SET_MODEL_FLAGS 12
// param u32 new model flags

#define COMMAND_ID_GET_USB_INFO 13  // no params
#define COMMAND_ID_GET_USB_INFO2 14  // no params

#define COMMAND_ID_GET_CPU_PROCS_INFO 15

//#define COMMAND_ID_SET_PROC_PRIORITY_VALUES 16 // deprecated in 11.5
// param u32: uProcessesFlags
// extra u8 values (7): iThreadPriorityRouter, iThreadPriorityRadioRx, iThreadPriorityRadioTx, iThreadPriorityVideoCapture, iThreadPriorityRC, iThreadPriorityTelemetry, iThreadPriorityOthers
// extra u8: restart now

//#define COMMAND_ID_SET_IONICE_VALUES 17 // deprecated in 11.5
#define COMMAND_ID_SET_ENABLE_DHCP 18

#define COMMAND_ID_SET_RADIO_LINK_DATARATES 19
// param: radio link index
// data: (int) datarate video downlink
//       (int) datarate data downlink
//       (int) datarate video uplink
//       (int) datarate data uplink

#define COMMAND_ID_REBOOT 20
#define COMMAND_ID_RESET_ALL_TO_DEFAULTS 21
#define COMMAND_ID_FACTORY_RESET 22

#define COMMAND_ID_SET_GPS_INFO 23
// param: gps present on vehicle: on/off

#define COMMAND_ID_SET_OSD_PARAMS 24
// an osd_parameters_t structure as input

#define COMMAND_ID_SET_OSD_CURRENT_LAYOUT 25
// the parameter is the layout index

#define COMMAND_ID_GET_CORE_PLUGINS_INFO 26
// request from vehicle info about core plugins present on vehicle
// returns the structure below
typedef struct
{
   CorePluginSettings listPlugins[MAX_CORE_PLUGINS_COUNT];
   int iCountPlugins;
   
} __attribute__((packed)) command_packet_core_plugins_response;

#define COMMAND_ID_RESET_RADIO_LINK 27
// param is radio link id

#define COMMAND_ID_SET_SERIAL_PORTS_INFO 28
// contains a type_vehicle_hardware_interfaces_info structure

#define COMMAND_ID_SET_SIK_PACKET_SIZE 29
// parameter is the sik packet size

#define COMMAND_ID_SET_CAMERA_PARAMETERS 30
// Has camera index as param and a camera_parameters_t struct as input data
// Has no particular response structure

#define COMMAND_ID_SET_CAMERA_PROFILE 31
// param is the new profile index

#define COMMAND_ID_SET_CURRENT_CAMERA 32
// param: camera index

#define COMMAND_ID_FORCE_CAMERA_TYPE 33
// param: camera type to set for current camera

#define COMMAND_ID_ROTATE_RADIO_LINKS 34

#define COMMAND_ID_SET_FUNCTIONS_TRIGGERS_PARAMS 35    // version 5.6
// A model type_functions_parameters structure parameters

// Deprecated in 11.2 #define COMMAND_ID_SET_CONTROLLER_TELEMETRY_OPTIONS 36

#define COMMAND_ID_SET_RELAY_PARAMETERS 37
// Param is a type_relay_parameters structure

#define COMMAND_ID_SET_VIDEO_PARAMETERS 39
// video params and video profiles (MAX_VIDEO_LINK_PROFILES)

// DEPRECATED 41

// DEPRECATED 43

// DEPRECATED 45
// param: 0 - auto, value: quantization

#define COMMAND_ID_SWAP_RADIO_INTERFACES 46
#define COMMAND_ID_GET_SIK_CONFIG 47
// param: local radio interface index

#define COMMAND_ID_SET_RADIO_LINKS_FLAGS 48
// param: uGlobalRadioLinksFlags from type_radio_links_parameters

#define COMMAND_ID_SET_TEMPERATURE_THRESHOLD 49
// param: temperature in C

#define COMMAND_ID_SET_ALARMS_PARAMS 50
// an type_alarms_parameters structure as input

#define COMMAND_ID_SET_THREADS_PRIORITIES 51
// param: 0/1 to restart vehicle processes
// params: model's type_processes_priorities structure

#define COMMAND_ID_SET_ENCRYPTION_PARAMS 54 // (added ruby 5.1)
// byte 0: flags, byte 1: pass key lenght; byte 2..N: pass key

#define COMMAND_ID_SET_TELEMETRY_TYPE_AND_PORT 55
// param:
//  byte 0: bit 0...3 telemetry type, bit 4...7 serial port
//  byte 1-3: serial speed

#define COMMAND_ID_SET_TX_PIT_MODE 56
// param: byte 0: pit mode flags, byte 1: 0/1 set pit mode off/on (if pit flag for manual set is true)

#define COMMAND_ID_SET_TELEMETRY_PARAMETERS 60
#define COMMAND_ID_SET_RC_CAMERA_PARAMS 61 // param is a u32 with camera control params

#define COMMAND_ID_SET_RC_PARAMS 70 // params are a rc_parameters_t struct

#define COMMAND_ID_SET_AUDIO_PARAMS 73  // (added v.4.4) an audio_parameters_t struct

#define COMMAND_ID_SET_RXTX_SYNC_TYPE 80 // param is rxtx sync type enum (0..2)

#define COMMAND_ID_RESET_CPU_SPEED 81 // (added v5.0)

#define COMMAND_ID_SET_ALL_PARAMS 99
// contains the model file

#define COMMAND_ID_GET_ALL_PARAMS_ZIP 100
// 11.1 or older:
// param:
//  byte 0:
//    bit 0: set developer mode on/off: 1/0
//    bit 1: enable telemetry output;
//    bit 2: enable telemetry input;
//    bit 3: (deprecated) enable developer vehicle video link stats
//    bit 4: enable developer vehicle video link graphs
//    bit 5: request sending of full mavlink/ltm telemetry packets
//    bit 6: send back response in small segments (150 bytes each, for low rate radio links)

//  byte 1:
//    MAVLink sys id of the controller
//  byte 2:
//    wifi guard delay (0..100)
//  byte 3:
//    bit 0..3: radio interfaces graph refresh interval: 1...6, same translation to miliseconds as for nGraphRadioRefreshInterval: 10,20,50,100,200,500 ms
//
// 11.2 or newer
//    no params or extra params
//
// Response has one of two types:
//   * the zip model settings, if single packet mode was set (more than 150 bytes)
//   * segments of the zip model settings, if multiple small segments response was requested (smaller than 150 bytes)
//     each segment has:
//               1 byte: file unique id
//               1 byte: segment number
//               1 byte: total segments
//               1 byte: segment size
//               N bytes - segment data (150 bytes)


#define COMMAND_ID_GET_CURRENT_VIDEO_CONFIG 101
#define COMMAND_ID_GET_MODULES_INFO 103
// 1 byte param: 0 - get modules, 1 - get hardware info

#define COMMAND_ID_GET_MEMORY_INFO 104
#define COMMAND_ID_GET_CPU_INFO 105
#define COMMAND_ID_SET_OVERCLOCKING_PARAMS 107
typedef struct
{
   int overvoltage; // 0 or negative - disabled, negative - default value
   int freq_arm; // 0 - disabled; in mhz
   int freq_gpu; // 0 - disabled; in mhz; (0/1 for OIPC boost)
   
} __attribute__((packed)) command_packet_overclocking_params;

#define COMMAND_ID_SET_VEHICLE_BOARD_TYPE 110
// param: u32 uBoardType, only subtype is used, rest is ignored


#define COMMAND_ID_DEBUG_GET_TOP 201

// Deprecated in 11.2 #define COMMAND_ID_ENABLE_LIVE_LOG 203
// param - 0/1 to enable live log

#define COMMAND_ID_SET_DEVELOPER_FLAGS 205
// u32 param - developer flags
// extra params (optional):
// u32 - radio rx loop timeout interval

#define COMMAND_ID_RESET_ALL_DEVELOPER_FLAGS 206

#define COMMAND_ID_UPLOAD_FILE_SEGMENT 208 // uploads a file segment to the vehicle. Has this structure then file segment data;

typedef struct
{
   char szFileName[128];
   u32 uFileId;
   u32 uTotalFileSize;
   u32 uTotalSegments;
   u32 uSegmentIndex;
   u32 uSegmentSize; // must be less that max packet size minus all the headers. Last segment will probablly be smaller.
} __attribute__((packed)) command_packet_upload_file_segment;


#define COMMAND_ID_UPLOAD_SW_TO_VEHICLE63 209
typedef struct
{
   int type; // 0: update zip, 1: generated tar file from controller
   u32 total_size; // total_size and block_length are zero to cancel an upload
   u32 file_block_index; // MAX_U32 to cancel an upload
   bool is_last_block;
   int block_length; // total_size and block_length are zero to cancel an upload
} __attribute__((packed)) command_packet_sw_package;

#define COMMAND_ID_UPLOAD_CALIBRATION_FILE 210
typedef struct
{
   int calibration_file_type; // 0: default, 1: user file, 2 - runcam, 3 - fpv1 (335/415), 4 - milos1 (335/415)
   char szFileName[64];
   int iUploadFileIndex;
   u32 total_file_size; // total_size and block_length are zero to cancel an upload
   u32 segment_index; // MAX_U32 to cancel an upload
   u32 segment_offset; // bytes offset from start of file
   int segment_length; // total_size and block_length are zero to cancel an upload
   bool is_last_segment;
} __attribute__((packed)) t_packet_header_command_upload_calib_file;


#define COMMAND_ID_DOWNLOAD_FILE 211 // has as param the ID of the file to download (high bit: request just status); has a response info about the file: t_packet_header_download_file_info

typedef struct
{
   u32 file_id; // what file was requested
   u8  szFileName[128]; // original filename
   u32 segments_count; // how many segments are in the file
   u32 segment_size; // how many bytes are in a segment
   u8 isReady; // 1 - file is ready: 0 - file is being preprocessed; 2 - error
} __attribute__((packed)) t_packet_header_download_file_info;

#define COMMAND_ID_DOWNLOAD_FILE_SEGMENT 212 // has as param: low word: file ID, high word: file segment; has as response data: dword: fileid and segment id then segment data


#define COMMAND_ID_CLEAR_LOGS 213

//------------------------------------------------------
const char* commands_get_description(u8 command_type);
