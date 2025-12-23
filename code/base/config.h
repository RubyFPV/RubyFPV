#pragma once

#include "config_hw.h"
#include "alarms.h"
#include "flags.h"
#include "config_rc.h"
#include "config_file_names.h"
#include "config_obj_names.h"
#include "config_radio.h"
#include "config_video.h"
#include "config_timers.h"

#define ALIGN_STRUCT_SPEC_INFO __attribute__((aligned(4)))

//sig_atomic_t
//_Atomic
//#if defined (HW_PLATFORM_OPENIPC_CAMERA)
//#define _ATOMIC_PREFIX _Atomic
//#else
#define _ATOMIC_PREFIX volatile
//#endif

//#define DISABLE_ALL_LOGS 1
#define FEATURE_ENABLE_RC 1
#define FEATURE_RELAYING 1
//#define FEATURE_CHECK_LICENCES 1
//#define FEATURE_LOCAL_AUDIO_RECORDING 1
//#define FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
//#define LOG_RAW_TELEMETRY

#define MAX_RELAY_VEHICLES 5
// Should be Main vehicle + relay vehicles (above)

#define MAX_CONCURENT_VEHICLES 6

#define MAX_PLUGIN_NAME_LENGTH 64
#define MAX_OSD_PLUGINS 32
#define MAX_CAMERA_NAME_LENGTH 24
#define MAX_CAMERA_BIN_PROFILE_NAME 32

#define DEFAULT_VEHICLE_NAME ""
#ifdef HW_PLATFORM_OPENIPC_CAMERA
#define MAX_MODELS 2
#define MAX_MODELS_SPECTATOR 2
#else
#define MAX_MODELS 40
#define MAX_MODELS_SPECTATOR 20
#endif


#define SYSTEM_RT_INFO_UPDATE_INTERVAL_MS 5
#define SYSTEM_RT_INFO_INTERVALS 400
#define SYSTEM_RT_INFO_INTERVALS_FRAMES 150

#define DEFAULT_OVERVOLTAGE 3
#define DEFAULT_ARM_FREQ 900
#define DEFAULT_GPU_FREQ 400
#define DEFAULT_FREQ_OPENIPC_SIGMASTAR 1100
#define DEFAULT_FREQ_RADXA 1416

#define DEFAULT_IO_PRIORITY_ROUTER_CTRL 3 //(negative for disabled)
#define DEFAULT_IO_PRIORITY_ROUTER_VEHICLE 3 //(negative for disabled)
#define DEFAULT_IO_PRIORITY_RC 3
#define DEFAULT_IO_PRIORITY_VIDEO_TX 3
#define DEFAULT_IO_PRIORITY_VIDEO_RX 3

// Priorities are 2...139:  0,1: disabled, 1...100: RT, 101-139: nice
#define DEFAULT_PRIORITY_VEHICLE_THREAD_ROUTER 60
#define DEFAULT_PRIORITY_VEHICLE_THREAD_ROUTER_OIPC 90
#define DEFAULT_PRIORITY_VEHICLE_THREAD_RADIO_RX 55
#define DEFAULT_PRIORITY_VEHICLE_THREAD_RADIO_TX 80
#define DEFAULT_PRIORITY_VEHICLE_THREAD_VIDEO_CAPTURE 70
#define DEFAULT_PRIORITY_VEHICLE_THREAD_VIDEO_CAPTURE_OIPC 0
#define DEFAULT_PRIORITY_VEHICLE_THREAD_RX_RC 80
#define DEFAULT_PRIORITY_VEHICLE_THREAD_TX_TELEM 101
#define DEFAULT_PRIORITY_VEHICLE_THREAD_TX_TELEM_OIPC 105
#define DEFAULT_PRIORITY_VEHICLE_OTHERS 110

// Priorities are 2...139:  0,1: disabled, 1...100: RT, 101-139: nice
#define DEFAULT_PRIORITY_CTRL_THREAD_ROUTER 60
#define DEFAULT_PRIORITY_CTRL_THREAD_RADIO_RX 51
#define DEFAULT_PRIORITY_CTRL_THREAD_RADIO_TX 55
#define DEFAULT_PRIORITY_CTRL_THREAD_CENTRAL 101
#define DEFAULT_PRIORITY_CTRL_THREAD_TX_RC 90
#define DEFAULT_PRIORITY_CTRL_THREAD_VIDEO_RX 70
#define DEFAULT_PRIORITY_CTRL_THREAD_VIDEO_REC 101
#define DEFAULT_PRIORITY_CTRL_OTHERS 110

#define DEFAULT_MAVLINK_SYS_ID_VEHICLE 1
#define DEFAULT_MAVLINK_SYS_ID_CONTROLLER 255

//#if defined(HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA)
#define DEFAULT_FC_TELEMETRY_SERIAL_SPEED 57600
//#endif
//#ifdef HW_PLATFORM_OPENIPC_CAMERA
//#define DEFAULT_FC_TELEMETRY_SERIAL_SPEED 115200
//#endif

#define DEFAULT_TELEMETRY_SEND_RATE 4 // Times per second. How often the Ruby/FC telemetry gets sent from vehicle to controller

#define RAW_TELEMETRY_MAX_BUFFER 512  // bytes
#define RAW_TELEMETRY_SEND_TIMEOUT 200 // miliseconds. how much to wait until to send whatever is in a telemetry serial buffer to the radio
#define RAW_TELEMETRY_MIN_SEND_LENGTH 255 // minimum data length to send right away to radio

#define AUXILIARY_DATA_LINK_SEND_TIMEOUT 100 // miliseconds. how much to wait until to send whatever is in a data link serial buffer to the radio
#define AUXILIARY_DATA_LINK_MIN_SEND_LENGTH 255 // minimum data length to send right away to radio

#define DEFAULT_PING_FREQUENCY 2

#define DEFAULT_UPLOAD_PACKET_CONFIRMATION_FREQUENCY 10

#define DEFAULT_RADXA_DISPLAY_WIDTH 1280
#define DEFAULT_RADXA_DISPLAY_HEIGHT 720
#define DEFAULT_RADXA_DISPLAY_REFRESH 60
#define DEFAULT_MPP_BUFFERS_SIZE 32

#define DEFAULT_OSD_RADIO_GRAPH_REFRESH_PERIOD_MS 50
#define DEFAULT_MSPOSD_RECORDING_COLS 53
#define DEFAULT_MSPOSD_RECORDING_ROWS 20


#ifdef __cplusplus
extern "C" {
#endif 

void getSystemVersionString(char* p, u32 swversion);

int config_file_get_value(const char* szPropName);
void config_file_set_value(const char* szFile, const char* szPropName, int value);
void config_file_force_value(const char* szFile, const char* szPropName, int value);
void config_file_add_value(const char* szFile, const char* szPropName, int value);

void save_simple_config_fileU(const char* fileName, u32 value);
u32 load_simple_config_fileU(const char* fileName, u32 defaultValue);

void save_simple_config_fileI(const char* fileName, int value);
int load_simple_config_fileI(const char* fileName, int defaultValue);

FILE* try_open_base_version_file(char* szOutputFile);
void get_Ruby_BaseVersion(int* pMajor, int* pMinor);
void get_Ruby_UpdatedVersion(int* pMajor, int* pMinor);

#ifdef __cplusplus
}  
#endif 