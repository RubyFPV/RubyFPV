#pragma once


#define OSD_FLAG_SHOW_VIDEO_MBPS ((u32)(((u32)0x01)))
#define OSD_FLAG_TOTAL_DISTANCE ((u32)(((u32)0x01)<<1))
#define OSD_FLAG_EXTENDED_VIDEO_DECODE_STATS ((u32)(((u32)0x01)<<2))
#define OSD_FLAG_SHOW_STATS_VIDEO_H264_FRAMES_INFO ((u32)(((u32)0x01)<<3))
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
#define OSD_FLAG_SHOW_FLIGHT_MODE_CHANGE ((u32)(((u32)0x01)<<31))

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
#define OSD_FLAG2_SHOW_RADIO_LINK_QUALITY_NUMBERS ((u32)(((u32)0x01)<<17))
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
// Deprecated #define OSD_FLAG2_SHOW_BACKGROUND_BARS ((u32)(((u32)0x01)<<28))
#define OSD_FLAG2_FLASH_OSD_ON_TELEMETRY_DATA_LOST ((u32)(((u32)0x01)<<29))


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
//#define OSD_FLAG3_SHOW_RADIO_RX_GRAPH_CONTROLLER  ((u32)(((u32)0x01)<<12))
#define OSD_FLAG3_HIGHLIGHT_CHANGING_ELEMENTS  ((u32)(((u32)0x01)<<13))
#define OSD_FLAG3_LAYOUT_ENABLED_PLUGINS_ONLY ((u32)(((u32)0x01)<<14))
#define OSD_FLAG3_LAYOUT_STATS_AUTO_WIDGETS_MARGINS ((u32)(((u32)0x01)<<15))
#define OSD_FLAG3_SHOW_VEHICLE_DEV_STATS ((u32)(((u32)0x01)<<16))
#define OSD_FLAG3_RENDER_MSP_OSD ((u32)(((u32)0x01)<<17))
#define OSD_FLAG3_SHOW_RADIO_LINK_QUALITY_NUMBERS_DBM ((u32)(((u32)0x01)<<18))
#define OSD_FLAG3_SHOW_RADIO_LINK_QUALITY_NUMBERS_SNR ((u32)(((u32)0x01)<<19))
#define OSD_FLAG3_SHOW_RADIO_LINK_QUALITY_NUMBERS_PERCENT ((u32)(((u32)0x01)<<20))


#define OSD_PREFERENCES_OSD_TRANSPARENCY_BITMASK ((u32)0x0000FF00)
#define OSD_PREFERENCES_OSD_TRANSPARENCY_SHIFT 8
#define OSD_PREFERENCES_BIT_FLAG_SHOW_CONTROLLER_LINK_LOST_ALARM ((u32)(((u32)0x01)<<24))
#define OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_TOP ((u32)(((u32)0x01)<<25))
#define OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_BOTTOM ((u32)(((u32)0x01)<<26))
#define OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_LEFT ((u32)(((u32)0x01)<<27))
#define OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_RIGHT ((u32)(((u32)0x01)<<28))
#define OSD_PREFERENCES_BIT_FLAG_DONT_SHOW_FC_MESSAGES ((u32)(((u32)0x01)<<29))

#define INSTRUMENTS_FLAG_SPEED_TO_SIDES ((u32)(((u32)0x01))) // bit 0
#define INSTRUMENTS_FLAG_SHOW_HORIZONT ((u32)(((u32)0x01)<<1)) // bit 1
#define INSTRUMENTS_FLAG_SHOW_SPEEDALT ((u32)(((u32)0x01)<<2)) // bit 2
#define INSTRUMENTS_FLAG_SHOW_HEADING  ((u32)(((u32)0x01)<<3)) // bit 3
#define INSTRUMENTS_FLAG_SHOW_ALTGRAPH ((u32)(((u32)0x01)<<4)) // bit 4
#define INSTRUMENTS_FLAG_SHOW_INSTRUMENTS ((u32)(((u32)0x01)<<5))

// First usable bit for OSD plugins show/hide flag
#define INSTRUMENTS_FLAG_SHOW_FIRST_OSD_PLUGIN ((u32)(((u32)0x01)<<8))
#define INSTRUMENTS_FLAG_SHOW_ALL_OSD_PLUGINS_MASK ((u32)(((u32)0xFFFFFF)<<8))

#define OSD_BIT_FLAGS_MASK_MSPOSD_FONT ((u32)(((u32)0x07)))
#define OSD_BIT_FLAGS_SHOW_FLIGHT_END_STATS ((u32)(((u32)0x01)<<3))
#define OSD_BIT_FLAGS_SHOW_TEMPS_F ((u32)(((u32)0x01)<<4))
#define OSD_BIT_FLAGS_MUST_CHOOSE_PRESET ((u32)(((u32)0x01)<<5))

#define OSD_PRESET_NONE 0
#define OSD_PRESET_MINIMAL 1
#define OSD_PRESET_COMPACT 2
#define OSD_PRESET_DEFAULT 3
#define OSD_PRESET_CUSTOM 4
