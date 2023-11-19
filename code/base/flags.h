#pragma once

#define COMMAND_RESPONSE_FLAGS_OK 1
#define COMMAND_RESPONSE_FLAGS_FAILED 2
#define COMMAND_RESPONSE_FLAGS_UNKNOWN_COMMAND 4
#define COMMAND_RESPONSE_FLAGS_FAILED_INVALID_PARAMS 8


#define OSD_FLAG_SHOW_VIDEO_MBPS ((u32)(((u32)0x01)))
#define OSD_FLAG_TOTAL_DISTANCE ((u32)(((u32)0x01)<<1))
#define OSD_FLAG_EXTENDED_VIDEO_DECODE_STATS ((u32)(((u32)0x01)<<2))
#define OSD_FLAG_SHOW_STATS_VIDEO_INFO ((u32)(((u32)0x01)<<3))
#define OSD_FLAG_SHOW_EFFICIENCY_STATS ((u32)(((u32)0x01)<<5))
#define OSD_FLAG_REVERT_PITCH ((u32)(((u32)0x01)<<6)) // bit 7
#define OSD_FLAG_REVERT_ROLL ((u32)(((u32)0x01)<<7)) // bit 8
#define OSD_FLAG_SCRAMBLE_GPS ((u32)(((u32)0x01)<<8))
#define OSD_FLAG_SHOW_AHI_HEADING ((u32)(((u32)0x01)<<9))
#define OSD_FLAG_SHOW_HID_IN_OSD ((u32)(((u32)0x01)<<10)) // bit 11
#define OSD_FLAG_SHOW_TIME_LOWER ((u32)(((u32)0x01)<<11)) // bit 12
#define OSD_FLAG_AIR_SPEED_MAIN ((u32)(((u32)0x01)<<12)) // bit 13
#define OSD_FLAG_SHOW_SIGNAL_BARS ((u32)(((u32)0x01)<<13)) // bit 14
#define OSD_FLAG_SIGNAL_BARS_MASK ((u32)(((u32)0x03)<<14)) // bit 15-16

#define OSD_FLAG_SHOW_DISTANCE ((u32)(((u32)0x01)<<16))
#define OSD_FLAG_SHOW_ALTITUDE ((u32)(((u32)0x01)<<17))
#define OSD_FLAG_SHOW_GPS_INFO ((u32)(((u32)0x01)<<18))
#define OSD_FLAG_SHOW_RADIO_LINKS ((u32)(((u32)0x01)<<19))
#define OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS ((u32)(((u32)0x01)<<20))
#define OSD_FLAG_SHOW_HOME ((u32)(((u32)0x01)<<21))
#define OSD_FLAG_SHOW_BATTERY ((u32)(((u32)0x01)<<22))
#define OSD_FLAG_SHOW_VIDEO_MODE ((u32)(((u32)0x01)<<23))
#define OSD_FLAG_SHOW_VIDEO_MODE_EXTENDED ((u32)(((u32)0x01)<<24))
#define OSD_FLAG_SHOW_CPU_INFO ((u32)(((u32)0x01)<<25))
#define OSD_FLAG_SHOW_PITCH ((u32)(((u32)0x01)<<26))
#define OSD_FLAG_SHOW_THROTTLE ((u32)(((u32)0x01)<<27))
#define OSD_FLAG_SHOW_FLIGHT_MODE ((u32)(((u32)0x01)<<28))
#define OSD_FLAG_SHOW_TIME ((u32)(((u32)0x01)<<29))
#define OSD_FLAG_SHOW_RADIO_INTERFACES_INFO ((u32)(((u32)0x01)<<30))

#define OSD_FLAG2_SHOW_BGBARS ((u32)(((u32)0x01)))
#define OSD_FLAG2_SHOW_BATTERY_CELLS ((u32)(((u32)0x01)<<1))
#define OSD_FLAG2_RELATIVE_ALTITUDE ((u32)(((u32)0x01)<<2))
#define OSD_FLAG2_SHOW_GPS_POS ((u32)(((u32)0x01)<<3))
#define OSD_FLAG2_SHOW_AHI_GRADATIONS ((u32)(((u32)0x01)<<4))
#define OSD_FLAG2_AHI_GRADATIONS_10 ((u32)(((u32)0x01)<<5))
#define OSD_FLAG2_SHOW_STATS_RADIO_LINKS ((u32)(((u32)0x01)<<6))
#define OSD_FLAG2_SHOW_STATS_RADIO_INTERFACES ((u32)(((u32)0x01)<<7))
#define OSD_FLAG2_SHOW_STATS_VIDEO ((u32)(((u32)0x01)<<8))
#define OSD_FLAG2_SHOW_STATS_RC ((u32)(((u32)0x01)<<9))
//#define OSD_FLAG_NOT_USED //OSD_FLAG2_SHOW_OSD_GRID ((u32)(((u32)0x01)<<10))
#define OSD_FLAG2_SHOW_LOCAL_VERTICAL_SPEED ((u32)(((u32)0x01)<<11))
#define OSD_FLAG2_LAYOUT_LEFT_RIGHT ((u32)(((u32)0x01)<<12))
#define OSD_FLAG2_LAYOUT_ENABLED ((u32)(((u32)0x01)<<13))
#define OSD_FLAG2_SHOW_GROUND_SPEED ((u32)(((u32)0x01)<<14))
#define OSD_FLAG2_SHOW_AIR_SPEED ((u32)(((u32)0x01)<<15))
#define OSD_FLAG2_SHOW_RC_RSSI ((u32)(((u32)0x01)<<16))
#define OSD_FLAG2_SHOW_RADIO_LINK_QUALITY_PERCENTAGE ((u32)(((u32)0x01)<<17))
#define OSD_FLAG2_SHOW_RADIO_LINK_QUALITY_BARS ((u32)(((u32)0x01)<<18))
#define OSD_FLAG2_SHOW_RADIO_LINK_INTERFACES_EXTENDED ((u32)(((u32)0x01)<<19))
#define OSD_FLAG2_SHOW_BACKGROUND_ON_TEXTS_ONLY ((u32)(((u32)0x01)<<20))
#define OSD_FLAG2_SHOW_ADAPTIVE_VIDEO_GRAPH ((u32)(((u32)0x01)<<21))
#define OSD_FLAG2_SHOW_COMPACT_VIDEO_DECODE_STATS ((u32)(((u32)0x01)<<22))
#define OSD_FLAG2_SHOW_VEHICLE_RADIO_INTERFACES_STATS ((u32)(((u32)0x01)<<23))
#define OSD_FLAG2_SHOW_RADIO_INTERFACES_COMPACT ((u32)(((u32)0x01)<<24))
#define OSD_FLAG2_SHOW_TELEMETRY_STATS ((u32)(((u32)0x01)<<25))
#define OSD_FLAG2_SHOW_MINIMAL_VIDEO_DECODE_STATS ((u32)(((u32)0x01)<<26))
#define OSD_FLAG2_SHOW_MINIMAL_RADIO_INTERFACES_STATS ((u32)(((u32)0x01)<<27))

#define OSD_FLAG3_SHOW_GRID_CROSSHAIR ((u32)(((u32)0x01)<<1))
#define OSD_FLAG3_SHOW_GRID_DIAGONAL ((u32)(((u32)0x01)<<2))
#define OSD_FLAG3_SHOW_GRID_SQUARES ((u32)(((u32)0x01)<<3))
#define OSD_FLAG3_SHOW_WIND ((u32)(((u32)0x01)<<4))
#define OSD_FLAG3_SHOW_FC_TEMPERATURE ((u32)(((u32)0x01)<<5))
#define OSD_FLAG3_SHOW_CONTROLLER_ADAPTIVE_VIDEO_INFO ((u32)(((u32)0x01)<<6))
#define OSD_FLAG3_SHOW_GRID_THIRDS_SMALL ((u32)(((u32)0x01)<<7))
#define OSD_FLAG3_SHOW_VIDEO_BITRATE_HISTORY ((u32)(((u32)0x01)<<8))
#define OSD_FLAG3_SHOW_AUDIO_DECODE_STATS ((u32)(((u32)0x01)<<9))
#define OSD_FLAG3_SHOW_RADIO_RX_HISTORY_CONTROLLER  ((u32)(((u32)0x01)<<10))
#define OSD_FLAG3_SHOW_RADIO_RX_HISTORY_VEHICLE  ((u32)(((u32)0x01)<<11))
#define OSD_FLAG3_SHOW_RADIO_RX_GRAPH_CONTROLLER  ((u32)(((u32)0x01)<<12))


#define OSD_PREFERENCES_BIT_FLAG_SHOW_CONTROLLER_LINK_LOST_ALARM ((u32)(((u32)0x01)<<24))
#define OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_TOP ((u32)(((u32)0x01)<<25))
#define OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_BOTTOM ((u32)(((u32)0x01)<<26))
#define OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_LEFT ((u32)(((u32)0x01)<<27))
#define OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_RIGHT ((u32)(((u32)0x01)<<28))

#define INSTRUMENTS_FLAG_SPEED_TO_SIDES ((u32)(((u32)0x01))) // bit 0
#define INSTRUMENTS_FLAG_SHOW_HORIZONT ((u32)(((u32)0x01)<<1)) // bit 1
#define INSTRUMENTS_FLAG_SHOW_SPEEDALT ((u32)(((u32)0x01)<<2)) // bit 2
#define INSTRUMENTS_FLAG_SHOW_HEADING  ((u32)(((u32)0x01)<<3)) // bit 3
#define INSTRUMENTS_FLAG_SHOW_ALTGRAPH ((u32)(((u32)0x01)<<4)) // bit 4
#define INSTRUMENTS_FLAG_SHOW_INSTRUMENTS ((u32)(((u32)0x01)<<5))

// First usable bit for OSD plugins show/hide flag
#define INSTRUMENTS_FLAG_SHOW_FIRST_OSD_PLUGIN ((u32)(((u32)0x01)<<8))
#define INSTRUMENTS_FLAG_SHOW_ALL_OSD_PLUGINS_MASK ((u32)(((u32)0xFFFFFF)<<8))


#define TELEMETRY_FLAGS_RXTX ((u32)(((u32)0x01)))
#define TELEMETRY_FLAGS_RXONLY ((u32)(((u32)0x01)<<1))
#define TELEMETRY_FLAGS_REQUEST_DATA_STREAMS ((u32)(((u32)0x01)<<2))
#define TELEMETRY_FLAGS_SPECTATOR_ENABLE ((u32)(((u32)0x01)<<3))
#define TELEMETRY_FLAGS_SEND_FULL_PACKETS_TO_CONTROLLER ((u32)(((u32)0x01)<<10))
//#define TELEMETRY_FLAGS_SEND_ON_LOWEST_DATARATE ((u32)(((u32)0x01)<<11))
#define TELEMETRY_FLAGS_ALLOW_ANY_VEHICLE_SYSID ((u32)(((u32)0x01)<<12))

// Encryption flags for models

#define MODEL_ENC_FLAGS_NONE      ((u32)((u32)0x0))
#define MODEL_ENC_FLAG_ENC_BEACON ((u32)(((u32)0x01)))
#define MODEL_ENC_FLAG_ENC_DATA   ((u32)(((u32)0x01)<<1))
#define MODEL_ENC_FLAG_ENC_VIDEO  ((u32)(((u32)0x01)<<2))
#define MODEL_ENC_FLAG_ENC_ALL    ((u32)(((u32)0x01)<<3))

// raspivid commands
#define RASPIVID_COMMAND_ID_BRIGHTNESS 1
#define RASPIVID_COMMAND_ID_CONTRAST   2
#define RASPIVID_COMMAND_ID_SATURATION 3
#define RASPIVID_COMMAND_ID_SHARPNESS  4

#define RASPIVID_COMMAND_ID_FPS  10
#define RASPIVID_COMMAND_ID_KEYFRAME 11
#define RASPIVID_COMMAND_ID_SLICES 12

#define RASPIVID_COMMAND_ID_QUANTIZATION  50
#define RASPIVID_COMMAND_ID_QUANTIZATION_INIT 51
#define RASPIVID_COMMAND_ID_QUANTIZATION_MIN 52
#define RASPIVID_COMMAND_ID_QUANTIZATION_MAX  53
#define RASPIVID_COMMAND_ID_VIDEO_BITRATE 55
// bitrate param is in 100kb increments


// Local control message model changed type

#define MODEL_CHANGED_GENERIC 0
#define MODEL_CHANGED_RADIO_LINK_FRAMES_FLAGS 1
#define MODEL_CHANGED_FREQUENCY 2
#define MODEL_CHANGED_RADIO_LINK_CAPABILITIES 3
#define MODEL_CHANGED_RADIO_POWERS 4
#define MODEL_CHANGED_ADAPTIVE_VIDEO_FLAGS 5
#define MODEL_CHANGED_EC_SCHEME 6
#define MODEL_CHANGED_CAMERA_PARAMS 7
#define MODEL_CHANGED_STATS 8
#define MODEL_CHANGED_VIDEO_BITRATE 9
#define MODEL_CHANGED_USER_SELECTED_VIDEO_PROFILE 10
#define MODEL_CHANGED_VIDEO_H264_QUANTIZATION 11
#define MODEL_CHANGED_SIK_PACKET_SIZE 12

#define MODEL_CHANGED_SIK_FREQUENCY 17

#define MODEL_CHANGED_DEFAULT_MAX_ADATIVE_KEYFRAME 20
#define MODEL_CHANGED_VIDEO_KEYFRAME 21
#define MODEL_CHANGED_SWAPED_RADIO_INTERFACES 22
#define MODEL_CHANGED_ROTATED_RADIO_LINKS 23

#define MODEL_CHANGED_CONTROLLER_TELEMETRY 25
#define MODEL_CHANGED_RELAY_PARAMS 26
#define MODEL_CHANGED_SERIAL_PORTS 30

#define MODEL_CHANGED_AUDIO_PARAMS 31
#define MODEL_CHANGED_OSD_PARAMS 32
#define MODEL_CHANGED_VEHICLE_GENERIC_PARAMS 33
#define MODEL_CHANGED_RADIO_DATARATES 34

#define MODEL_CHANGED_SYNCHRONISED_SETTINGS_FROM_VEHICLE 55

// Model & developer flags

#define MODEL_FLAG_PRIORITIZE_UPLINK ((u32)(((u32)0x01)))
#define MODEL_FLAG_USE_LOGER_SERVICE ((u32)(((u32)0x01)<<1))

#define DEVELOPER_FLAGS_BIT_LIVE_LOG ((u32)(((u32)0x01)))
#define DEVELOPER_FLAGS_BIT_RADIO_SILENCE_FAILSAFE ((u32)(((u32)0x01)<<1))
#define DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS ((u32)(((u32)0x01)<<2))
#define DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_LINK_STATS ((u32)(((u32)0x01)<<3))
#define DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_LINK_GRAPHS ((u32)(((u32)0x01)<<4))
#define DEVELOPER_FLAGS_BIT_INJECT_VIDEO_FAULTS ((u32)(((u32)0x01)<<5))
#define DEVELOPER_FLAGS_BIT_DISABLE_VIDEO_OVERLOAD_CHECK ((u32)(((u32)0x01)<<6))
#define DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_TX_GAP ((u32)(((u32)0x01)<<7))
//#define DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_VIDEO_BITRATE_HISTORY ((u32)(((u32)0x01)<<16))
#define DEVELOPER_FLAGS_BIT_INJECT_RECOVERABLE_VIDEO_FAULTS ((u32)(((u32)0x01)<<17))

// Video link profiles: 5 for link quality management, 1 for PIP

#define MAX_VIDEO_LINK_PROFILES 8
#define VIDEO_PROFILE_BEST_PERF 0
#define VIDEO_PROFILE_HIGH_QUALITY 1
#define VIDEO_PROFILE_USER 2
#define VIDEO_PROFILE_MQ 3
#define VIDEO_PROFILE_LQ 4
#define VIDEO_PROFILE_PIP 5


#define ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS ((u32)(((u32)0x01)<<3))
#define ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE ((u32)(((u32)0x01)<<4))
#define ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS ((u32)(((u32)0x01)<<5))
#define ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO ((u32)(((u32)0x01)<<6))
#define ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST ((u32)(((u32)0x01)<<7))

#define ENCODING_EXTRA_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT (0xFF<<16)
#define ENCODING_EXTRA_FLAG_RETRANSMISSIONS_DUPLICATION_PERCENT_AUTO (0xFF<<16)

#define ENCODING_EXTRA_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO ((u32)(((u32)0x01)<<24))
#define ENCODING_EXTRA_FLAG_ENABLE_VIDEO_ADAPTIVE_QUANTIZATION ((u32)(((u32)0x01)<<25))
#define ENCODING_EXTRA_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH_HIGH ((u32)(((u32)0x01)<<26))


#define VIDEO_FLAG_FILL_H264_SPT_TIMINGS     ((u32)((u32)0x01))
#define VIDEO_FLAG_IGNORE_TX_SPIKES          ((u32)(((u32)0x01)<<1))
#define VIDEO_FLAG_ENABLE_LOCAL_HDMI_OUTPUT  ((u32)(((u32)0x01)<<2))


#define RXTX_SYNC_TYPE_NONE 0
#define RXTX_SYNC_TYPE_BASIC 1
#define RXTX_SYNC_TYPE_ADV 2
#define RXTX_SYNC_TYPE_LAST 3


// Any of those can be set, multiple can be set
#define RELAY_CAPABILITY_TRANSPORT_TELEMETRY ((u32)1)
#define RELAY_CAPABILITY_TRANSPORT_VIDEO     (((u32)1)<<1)
#define RELAY_CAPABILITY_TRANSPORT_COMMANDS  (((u32)1)<<2)
#define RELAY_CAPABILITY_SWITCH_OSD         (((u32)1)<<3)
#define RELAY_CAPABILITY_MERGE_OSD          (((u32)1)<<4)

// Any of those can be set, multiple can be set
#define RELAY_MODE_MAIN       (((u32)1))
#define RELAY_MODE_REMOTE     (((u32)1)<<1)
#define RELAY_MODE_PIP_MAIN   (((u32)1)<<2)
#define RELAY_MODE_PIP_REMOTE (((u32)1)<<3)
#define RELAY_MODE_IS_RELAY_NODE   (((u32)1)<<4)
#define RELAY_MODE_IS_RELAYED_NODE (((u32)1)<<5)
