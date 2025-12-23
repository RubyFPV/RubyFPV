#pragma once
#include "base.h"
#include "hardware.h"
#include "flags.h"
#include "config_rc.h"
#include "shared_mem.h"
#include "../radio/radiopackets2.h"

#define MODEL_TELEMETRY_TYPE_NONE 0
#define MODEL_TELEMETRY_TYPE_MAVLINK 1
#define MODEL_TELEMETRY_TYPE_LTM 2

#define MODEL_MAX_CAMERAS 4
#define MODEL_CAMERA_PROFILES 3

#define MODEL_MAX_OSD_SCREENS 5

#define CAMERA_FLAG_FORCE_MODE_1 1
#define CAMERA_FLAG_IR_FILTER_OFF ((u32)(((u32)0x01)<<2))
#define CAMERA_FLAG_OPENIPC_DAYLIGHT_OFF ((u32)(((u32)0x01) << 3))
#define CAMERA_FLAG_OPENIPC_3A_FPV ((u32)(((u32)0x01) << 4))

typedef struct
{
   u32 uFlags; // See CAMERA_FLAG_... above
   // bit 0: force mode 1
   // bit 1: AWB mode old
   // bit 2: 1: turn IR Cut off
   // bit 3: 1: turn off daylight mode for OpenIPC cameras, activate night mode
   // bit 4: 1: use sigmastar 3a, 0: use default 3a
   u8 flip_image;
   u8 brightness; // 0...100
   u8 contrast;   // 0...100
   u8 saturation; // 0...200,  100 is middle (0)
   u8 sharpness;
   u8 exposure;
   u8 whitebalance;
   u8 metering;
   u8 drc; // stands for AGC (gain) for IMX327 camera (0 to 0xF, 0xC default)
   float analogGain;
   float awbGainB;
   float awbGainR; // stands for HUE for IMX307 camera
   float fovV;
   float fovH;
   u8 vstab; // on/off
   u8 ev;    // -10 to 10, 0 default, translated to 1...21, default 11
   u16 iso; // 100 - 800, 0 for off
   short int iShutterSpeed; // in 1/x of a second, 0 for off, negative for auto, min is 30, max is 30000 (1/30 to 1/30000);
      // For IMX415 OpenIPC is exposure in miliseconds, negative is auto
   u8 wdr; // used for IMX327 camera for WDR mode
   u8 dayNightMode; // 0 - day mode, 1 - night mode, only for Veye cameras
   u8 hue; // 0...100
   u32 uDummyCamP;
} camera_profile_parameters_t;

typedef struct
{
   camera_profile_parameters_t profiles[MODEL_CAMERA_PROFILES];
   int iCurrentProfile;
   int iCameraType; // as detected by hardware. 0 for none
   int iForcedCameraType; // as set by user. 0 for none
   char szCameraName[MAX_CAMERA_NAME_LENGTH];
   int iCameraBinProfile; // 0 - default, 1 - user bin file
   char szCameraBinProfileName[MAX_CAMERA_BIN_PROFILE_NAME];

} type_camera_parameters;


typedef struct
{
   int iCurrentVideoProfile; // set by user on the controller
   int iVideoWidth;
   int iVideoHeight;
   int iVideoFPS;
   int iH264Slices;
   int iRemovePPSVideoFrames;
   int iInsertPPSVideoFrames;
   int iInsertSPTVideoFramesTimings;
   int uDummyV1;
   u32 lowestAllowedAdaptiveVideoBitrate;
   u32 uMaxAutoKeyframeIntervalMs; // in milisec
   u32 uVideoExtraFlags; // Check VIDEO_FLAG_* enum
    // bit 0: enable focus mode: B&W
    // bit 1: not used
    // bit 2: enable HDMI output on vehicle side (if possible)
    // bit 3: retransmissions are started fast/aggresive
    // bit 4: 1 to enable H265, 0 to enable H264
    // bit 5: 1 to enable new adaptive video algorithm, 0 - use default one
    // bit 6: enable focus mode: bars

} video_parameters_t;


typedef struct
{
   u32 uProfileFlags;
     // VIDEO_PROFILE_FLAGS_* constants in flags_video.h
     // byte 0:
     //   bit 0-1: 3d noise: 0,1, or 2 (auto) VIDEO_PROFILE_FLAGS_MASK_NOISE
     //   bit 2: use higher level radio data rates
     //   bit 3-4: higher datarate boost
     //   bit 5  - use lower DR for EC packets
     //   bit 6  - use lower DR for retr packets
     // byte 1:
     //   bit 0-4 - retransmissions guard time (ms)
     //   bit 5  - retransmissions: normal (0)/aggresive(1)
     //   bit 6  - lower QPDelta on low link quality
     //   bit 7  - high strength on lower QPDelta (see above, bit 6)

   u32 uProfileEncodingFlags; // same as radio video packet uProfileEncodingFlags
     // VIDEO_PROFILE_ENCODING_FLAG_* constants in flags_video.h
     // byte 0:
     //    bit 0..2  - scramble blocks count
     //    bit 3     - enables restransmission of missing packets
     //    bit 4     - enable adaptive video keyframe interval
     //    bit 5     - enable adaptive video link params
     //    bit 6     - use controller info too when adjusting video link params
     //    bit 7     - go lower adaptive video profile when controller link lost

     // byte 1:   - max time to wait for retransmissions (in ms*5)// affects rx buffers size
     // byte 2:   - retransmission duplication percent (0-100%), 0xFF = auto, bit 0..3 - regular packets duplication, bit 4..7 - retransmitted packets duplication
     // byte 3:
     //    bit 0  - use medium adaptive video
     //    bit 1  - enable video auto quantization
     //    bit 2  - video auto quantization strength
     //    bit 3  - one way video link
     //    bit 4  - video profile should use EC scheme as auto;
     //    bit 5,6 - EC scheme spreading factor (0...3)
     //    bit 7  - try to keep constant video bitrate when it fluctuates

   int iAdaptiveAdjustmentStrength; // 1..10 (from 10% to 100% strength)
   u32 uAdaptiveWeights;
   //    byte 0: 0...3: RSSI, 4...7: SNR, 0 for disabled
   //    byte 1: 0...3: retransmissions, 4...7 rx lost packets
   //    byte 2: 0...3: EC used, 4..7: max EC used
   //    byte 3: 0...3: time for metrics to be above level, in order to switch higher. In 100 ms intervals, multiplied with strength too.

   int h264profile; // 0 = baseline, 1 = main, 2 = high
   int h264level; //0 = 4.0, 1 = 4.1, 2 = 4.2
   int h264refresh; // 0 = cyclic, 1 = adaptive, 2 = both, 3 = cyclicrows
   int h264quantization; // 0 - auto, // pozitive - value to use, // negative - value when disabled (auto)
   int iIPQuantizationDelta;

   int iDefaultFPS; // Default FPS to set, if any (>0), when switching to this video profile
   int iDefaultLinkLoad; // In percentages, 0...100 (0 - use radio link setting)
   int iBlockDataPackets;
   int iBlockECs;
   int iECPercentage; // In percentages (0...100%)
   int video_data_length;
   int iKeyframeMS; // positive: fixed, negative: auto
   u32 uTargetVideoBitrateBPS; // in bits/second, 0 for auto
   u32 uDummyVP1;
   u32 uDummyVP2;
} type_video_link_profile;


typedef struct
{
   int iCurrentOSDScreen;
   bool voltage_alarm_enabled;
   float voltage_alarm;
   int  battery_show_per_cell;
   int  battery_cell_count; // 0 - autodetect
   int  battery_capacity_percent_alarm; // 0 for disabled
   bool altitude_relative;
   bool show_overload_alarm;
   bool show_stats_rx_detailed;
   bool show_stats_decode;
   bool show_stats_rc;
   bool show_full_stats;
   bool invert_home_arrow;
   int  home_arrow_rotate; // degrees
   bool show_instruments;
   int  ahi_warning_angle;
   bool show_gps_position;
   int  iRadioInterfacesGraphRefreshIntervalMs;
   u8   osd_layout_preset[MODEL_MAX_OSD_SCREENS]; // presets
     // 0 - none
     // 1 - minimal
     // 2 - compact
     // 3 - default
     // 4 - custom
   u32 osd_flags[MODEL_MAX_OSD_SCREENS]; // what OSD elements to display for each layout mode
   u32 osd_flags2[MODEL_MAX_OSD_SCREENS];
   u32 osd_flags3[MODEL_MAX_OSD_SCREENS];
   u32 instruments_flags[MODEL_MAX_OSD_SCREENS]; // each bit: what instrument to show
   u32 osd_preferences[MODEL_MAX_OSD_SCREENS];
     // byte 0: osd elements font size (0...6)
     // byte 1: transparency (0: max ... 4: none)
     // byte 2: bit 0...3  osd stats windows font size (0...6)
     //         bit 4...7  osd stats background transparency (0...3)
     // byte 3:
     //    bit 0: show controller link lost alarm
     //    bit 1: arrange osd stats windows top
     //    bit 2: arrange osd stats windows bottom
     //    bit 3: arrange osd stats windows left
     //    bit 4: arrange osd stats windows right
     //    bit 5: do not show messages (texts) from FC in OSD
     //

   u32 uFlags;
     // check OSD_BIT_FLAGS_xxx
     // bit 0,1,2: MSO OSD font: 0 - auto, 1 - BF, 2 - INAV
     // bit 3: show flight end stats
     // bit 4: show temps in F
     // bit 5: must choose OSD preset
} osd_parameters_t;


typedef struct
{
   int isRelayEnabledOnRadioLinkId; // negative: disabled, positive: radioLinkId 
   u32 uRelayFrequencyKhz;
   u32 uRelayedVehicleId;
   u32 uRelayCapabilitiesFlags; // see RELAY_CAPABILITY_* in flags.h
   u8  uCurrentRelayMode; // see RELAY_MODE_* in flags.h
} type_relay_parameters;

#define RC_TRANSLATION_TYPE_NONE 0
#define RC_TRANSLATION_TYPE_2000 1
#define RC_TRANSLATION_TYPE_4000 2

typedef struct
{
   bool rc_enabled;
   int rc_frames_per_second;
   int rc_failsafe_timeout_ms;
   int receiver_type;
   int inputType; // RC input type on the controller: 0 none, 1 usb, 2 ibus/sbus, see config_rc.h enum
   int inputSerialPort;
   long inputSerialPortSpeed;
   int outputSerialPort;
   long outputSerialPortSpeed;

   u32 rcChAssignment[MAX_RC_CHANNELS];
      // first byte:
          // bit 0: no assignment (0/1);
          // bit 1: 0 - axe, 1 - button;
          // bit 2: 0 - momentary, 1 - sticky button (togle);
          // bit 3: not used
          // bit 4..7 axe/button index (1 index based: first button/axe is 1)
          // each additional 4 bits: positive value (!=0) for each extra button (in increasing order of output RC value)
          // bit 31: toggle flag

   u16 rcChMid[MAX_RC_CHANNELS];
   u16 rcChMin[MAX_RC_CHANNELS];
   u16 rcChMax[MAX_RC_CHANNELS];
   u16 rcChFailSafe[MAX_RC_CHANNELS];
   u8 rcChExpo[MAX_RC_CHANNELS];  // expo (%)
   u8 rcChFlags[MAX_RC_CHANNELS]; 
          // bit 0: reversed
          // bit 1..3: failsafe type for this channel
          // bit 4: use linear range
          // bit 5: relative move

   u32 failsafeFlags; // first byte: what type of failsafe to execute, globally;
                      // 2nd-3rd byte: failsafe value (for that type of failsafe)
   int channelsCount;
   u32 hid_id; // USB HID id on the controller for RC control
   u32 flags;
          // bit 0: output to FC enabled
   u32 rcChAssignmentThrotleReverse;
   int iRCTranslationType;
} rc_parameters_t;


#define TELEMETRY_TYPE_NONE 0
#define TELEMETRY_TYPE_MAVLINK 1
#define TELEMETRY_TYPE_LTM 2
#define TELEMETRY_TYPE_MSP 3
#define TELEMETRY_TYPE_TRANSPARENT 10

typedef struct
{
   int fc_telemetry_type; // 0 = None, 1 = MAVLink, 2 == LTM, 3 == MSP
   int iVideoBitrateHistoryGraphSampleInterval;
   u32 uDummyT1;
   int iUpdateRateHz; // times per second
   int vehicle_mavlink_id;
   int controller_mavlink_id;
   u32 flags; // see flags.h for TELEMETRY_FLAGS_[ID] values
              // rx only, request data streams, spectator, send full mavlink/ltm packets to controller etc
              // usb uarts present (0,1,2)
} telemetry_parameters_t;

typedef struct
{
   bool has_audio_device;
   bool enabled;
   int  volume;
   int  quality; // 0-3
   u8  uECScheme; // high 4 bits data, low 4 bits ec
   u16 uPacketLength;
   u32 uFlags;
      // byte 0:
      //   bit 0,1: mic type: 0 - none, 1 - internal, 2 - external
      // byte 1:
      //   0...255 buffering size
   u32 uDummyA1;
} audio_parameters_t;


#define MAX_MODEL_I2C_BUSSES 6
#define MAX_MODEL_I2C_DEVICES 16
#define MAX_MODEL_SERIAL_PORTS 6
#define MAX_MODEL_SERIAL_PORT_NAME 16

#define MODEL_SERIAL_PORT_BIT_EXTRNAL_USB ((u32)(((u32)0x01)<<11))
#define MODEL_SERIAL_PORT_BIT_SUPPORTED ((u32)(((u32)0x01)<<12))
typedef struct
{
   int i2c_bus_count;
   int i2c_device_count;
   int i2c_bus_numbers[MAX_MODEL_I2C_BUSSES];
   int i2c_devices_address[MAX_MODEL_I2C_DEVICES];
   int i2c_devices_bus[MAX_MODEL_I2C_DEVICES];
   int radio_interface_count;

   int serial_port_count;
   char serial_port_names[MAX_MODEL_SERIAL_PORTS][MAX_MODEL_SERIAL_PORT_NAME];
   u32 serial_port_supported_and_usage[MAX_MODEL_SERIAL_PORTS];
     // byte 0: usage type; same as hardware_serial enum: SERIAL_PORT_USAGE_xxxx
     // byte 1: bits 0...3 usb or hardware port index
     //         bits 4 : 0 - hardware builtin, 1 - on usb
     //         bits 5 : supported 0/1

   int serial_port_speed[MAX_MODEL_SERIAL_PORTS];
} type_vehicle_hardware_interfaces_info;


typedef struct
{
   u32 uCurrentOnTime; // seconds
   u32 uCurrentFlightTime; // seconds
   u32 uCurrentFlightDistance; // in 1/100 meters (cm)
   u32 uCurrentFlightTotalCurrent; // 0.1 miliAmps (1/10000 amps);

   u32 uCurrentTotalCurrent; // 0.1 miliAmps (1/10000 amps);
   u32 uCurrentMaxAltitude; // meters
   u32 uCurrentMaxDistance; // meters
   u32 uCurrentMaxCurrent; // miliAmps (1/1000 amps)
   u32 uCurrentMinVoltage; // miliVolts (1/1000 volts)

   u32 uTotalFlights; // count

   u32 uTotalOnTime;  // seconds
   u32 uTotalFlightTime; // seconds
   u32 uTotalFlightDistance; // in 1/100 meters (cm)

   u32 uTotalTotalCurrent; // 0.1 miliAmps (1/10000 amps);
   u32 uTotalMaxAltitude; // meters
   u32 uTotalMaxDistance; // meters
   u32 uTotalMaxCurrent; // miliAmps (1/1000 amps)
   u32 uTotalMinVoltage; // miliVolts (1/1000 volts)
} type_vehicle_stats_info;



typedef struct
{
   bool bEnableRCTriggerFreqSwitchLink1;
   bool bEnableRCTriggerFreqSwitchLink2;
   bool bEnableRCTriggerFreqSwitchLink3;

   int iRCTriggerChannelFreqSwitchLink1;
   int iRCTriggerChannelFreqSwitchLink2;
   int iRCTriggerChannelFreqSwitchLink3;

   bool bRCTriggerFreqSwitchLink1_is3Position;
   bool bRCTriggerFreqSwitchLink2_is3Position;
   bool bRCTriggerFreqSwitchLink3_is3Position;

   u32 uChannels433FreqSwitch[3];
   u32 uChannels868FreqSwitch[3];
   u32 uChannels23FreqSwitch[3];
   u32 uChannels24FreqSwitch[3];
   u32 uChannels25FreqSwitch[3];
   u32 uChannels58FreqSwitch[3];

   u32 uDummyF[8];

} type_functions_parameters;

// Radio datarates:
// zero: auto or use radio link datarate
// negative: MCS data rates
// positive < 100: legacy 2.4/5.8 datarates, in Mbps
// positive > 100: bps bitrates (for low bandwidth radio cards). Ex: 200 = 100 bps; 9700 = 9600 bps, 1000000 = 999900 bps

typedef struct
{
   int iAutoVehicleTxPower;
   int iAutoControllerTxPower;
   u32 uFlagsRadioInterfaces;
      //  bit 0: tx pit mode enable
      //  bit 1: tx pit mode enable: manual (by user)
      //  bit 2: tx pit mode enable: arm/disarm
      //  bit 3: tx pit mode enable: temperature
   
   int iDummyR1;

   int  interfaces_count;
   int  interface_card_model[MAX_RADIO_INTERFACES]; // 0 or positive - autodetected, negative - user set
   int  interface_link_id[MAX_RADIO_INTERFACES];
   int  interface_raw_power[MAX_RADIO_INTERFACES];
   u32  interface_radiotype_and_driver[MAX_RADIO_INTERFACES]; // low byte: radio type, second byte = driver type, third byte: supported card flag
   u8   interface_supported_bands[MAX_RADIO_INTERFACES];  // bits 0-3: 2.3, 2.4, 2.5, 5.8 bands
   char interface_szMAC[MAX_RADIO_INTERFACES][MAX_MAC_LENGTH];
   char interface_szPort[MAX_RADIO_INTERFACES][MAX_RADIO_PORT_NAME_LENGTH]; // first byte - first char, sec byte - sec char
   u32  interface_capabilities_flags[MAX_RADIO_INTERFACES]; // what the card is used for: video/data/relay/tx/rx
   u32  interface_current_frequency_khz[MAX_RADIO_INTERFACES]; // current frequency for this card
   u32  interface_current_radio_flags[MAX_RADIO_INTERFACES]; // radio flags: legacy/MCS datarate type, frame type, STBC, LDP, MCS etc
} type_radio_interfaces_parameters;


typedef struct
{
   int links_count;
   int iSiKPacketSize;
   u32 uGlobalRadioLinksFlags; // See MODEL_RADIOLINKS_FLAGS_... in base/flags.h
   // bit 0 - none, 1 - disable uplinks
   // bit 1 - bypass sockets buffers
   // bit 2 - has negotiated links;

   u32 link_frequency_khz[MAX_RADIO_INTERFACES];
   u32 link_capabilities_flags[MAX_RADIO_INTERFACES]; // data/video/both? rxtx/rx only/tx only
   u32 link_radio_flags[MAX_RADIO_INTERFACES]; // radio flags: legacy/MCS datarate type, frame type, STBC, LDP, MCS, SIK flags, etc
   int downlink_datarate_video_bps[MAX_RADIO_INTERFACES]; // 0: auto, -100: lowest, positive: bps, negative (-1 or less): MCS rate
   int downlink_datarate_data_bps[MAX_RADIO_INTERFACES]; // 0: auto, -100: lowest, positive: bps, negative (-1 or less): MCS rate

   u8  uSerialPacketSize[MAX_RADIO_INTERFACES]; // packet size over air for serial radio links
   u32 uDummyR1[MAX_RADIO_INTERFACES];
   int uplink_datarate_video_bps[MAX_RADIO_INTERFACES]; // 0: auto, -100: lowest, positive: bps, negative (-1 or less): MCS rate
   int uplink_datarate_data_bps[MAX_RADIO_INTERFACES]; // 0: auto, -100: lowest, positive: bps, negative (-1 or less): MCS rate
   u8  uMaxLinkLoadPercent[MAX_RADIO_INTERFACES];

} type_radio_links_parameters;

#define MODEL_MAX_STORED_QUALITIES_LINKS 3
#define MODEL_MAX_STORED_QUALITIES_VALUES 9
typedef struct
{
   u8 uFlagsRuntimeCapab; // see flags.h MODEL_RUNTIME_*
   // bit 0: computed
   // bit 1: dirty

   int iMaxSupportedLegacyDataRate;
   int iMaxSupportedMCSDataRate;
   u32 uSupportedMCSFlags;
   float fQualitiesLegacy[MODEL_MAX_STORED_QUALITIES_LINKS][MODEL_MAX_STORED_QUALITIES_VALUES];
   float fQualitiesMCS[MODEL_MAX_STORED_QUALITIES_LINKS][MODEL_MAX_STORED_QUALITIES_VALUES];
   int iMaxTxPowerMwLegacy[MODEL_MAX_STORED_QUALITIES_LINKS][MODEL_MAX_STORED_QUALITIES_VALUES];
   int iMaxTxPowerMwMCS[MODEL_MAX_STORED_QUALITIES_LINKS][MODEL_MAX_STORED_QUALITIES_VALUES];

} type_radio_runtime_capabilities_parameters;

typedef struct
{
   u8 uAlarmMotorCurrentThreshold; // in 0.1 amp increments, most significant bit: 0 - alarm disabled, 1 - alarm enabled

} type_alarms_parameters;


typedef struct
{
   u8 uLogTypeFlags;
   // 0 - no log
   // 1 - log when armed
   // 2 - log when recording
   u32 uLogIntervalMilisec;
   u32 uLogParams; // bitmask
   u32 uLogParams2; // bitmask
   u32 uDummy;
} type_logging_parameters;

typedef struct
{
   u32 uProcessesFlags; // see enum in flags.h
   int iOverVoltage; // 0 or negative - disabled, negative - default value
   int iFreqARM; // 0 or negative - disabled; in Mhz
   int iFreqGPU;
      // Pi: 0 or negative - disabled; in Mhz
      // OIPC: 0 or negative - default; positive: boosted

   int iThreadPriorityRouter; // 0,1 - disabled, 2...100: rt, 101-139: nice, lower number - higher priority
   int iThreadPriorityRadioRx;
   int iThreadPriorityRadioTx;
   int iThreadPriorityVideoCapture;
   int iThreadPriorityRC;
   int iThreadPriorityTelemetry;
   int iThreadPriorityOthers;
   
   int ioNiceRouter; // 0 or negative - disabled;
   int ioNiceVideo; // 0 or negative - disabled;

   int iCoreRadioRx; // -1 for disabled
   int iCoreRouter;
   int iCoreVideoCapture;
   int iCoreTelemetry;
   int iCoreCommands;
   int iCoreRC;
   int iCoreOthers;
} type_processes_priorities;


// This is all readonly:
typedef struct
{
   u32 uBoardType; //byte 0: board_type;  byte 1: variant
   int iMaxTxVideoBlocksBuffer; // max blocks that can be cached on vehicle
   int iMaxTxVideoBlockPackets; // max packets in a video block
   u32 uHWFlags;
   // bit 0: OTA capable
   // byte 1: threshold temp (C)
   
   u32 uRubyBaseVersion;
   u32 uDummyHW1;
   u32 uDummyHW2;
} type_hardware_capabilities;

class Model
{
   public:
      Model();

      bool b_mustSyncFromVehicle; // non persistent

      u32 uModelFlags; // MODEL_FLAG_* flags
        //   bit 0: prioritize uplink
        //   bit 1: use logger service instead of files
      u32 uModelPersistentStatusFlags;
      u32 uModelRuntimeStatusFlags; // MODEL_RUNTIME_STATUS_FLAG_* flags
        // non persistent (not saved in model), can be send to controller through telemetry if needed
        // it's current state and updates are visible only in the current process
        // bit 0: currently in tx pit mode
        // bit 1: currently in tx pit mode (due to temperature)
        // bit 2: currently in tx auto power mode
      
      u32 uDeveloperFlags;
        // Check DEVELOPER_FLAGS_* in flags.h
        // byte 0:
        //    bit 0: enable live log
        //    bit 1: enable radio silence failsafe (reboot)
        //    bit 2: log only errors
        //    bit 3: enable vehicle developer video link stats
        //    bit 4: enable vehicle developer video link graphs
        //    bit 5: inject video faults (for testing only)
        //    bit 6: disable video tx overload logic
        //    bit 7: send back vehicle tx gap
        //     
        // byte 1:
        //    wifi guard delay (1..100 milisec)

        // byte 3:
        //    bit 0: send to controller video bitrate hystory telemetry packet
        //    bit 1: inject recoverable video faults

      type_hardware_capabilities hwCapabilities;
      
      char vehicle_name[MAX_VEHICLE_NAME_LENGTH];
      u32 uVehicleId;
      u32 uControllerId;
      u32 uControllerBoardType;
      u32 sw_version; // byte 0: minor version, byte 1 Major version, byte 2-3: build nb
      bool is_spectator;
      u8 vehicle_type;
         // semantic changed in version 8.0
         // bit 0...4 - vehicle type: car, drone, plane, etc
         // bit 5..7 - firmware type: Ruby, OpenIPC, etc
      int rxtx_sync_type;
      u32 alarms;

      type_vehicle_hardware_interfaces_info hardwareInterfacesInfo;
      type_processes_priorities processesPriorities;

      
      // Radio interfaces order is given by physical order. Can't be changed.
      // Radio links order is auto/user defined.
      // Radio interfaces assignment to radio links is auto/user defined.
      // Radio interfaces parameters are checked/re-computed when the vehicle starts.

      type_radio_interfaces_parameters radioInterfacesParams;
      type_radio_links_parameters radioLinksParams;
      type_radio_runtime_capabilities_parameters radioRuntimeCapabilities;
      type_logging_parameters loggingParams;
      bool enableDHCP;

      u32 camera_rc_channels;
           // each byte: pitch, roll, yaw camera RC channel (5 bits, 1 based index, 0 - unused), bit 6: 1 - absolute move, 0 - relative move;
           // 4th byte: bit 0...4: 5 bits for speed for relative move;
           // 4th byte: bit 5: relative(0)/absolute(1) move
           // last 2 bits should be 1 for consistency check

      u32 enc_flags;
      
      type_vehicle_stats_info m_Stats;

      int iGPSCount;
      
      type_camera_parameters camera_params[MODEL_MAX_CAMERAS];
      int iCameraCount;
      int iCurrentCamera;

      video_parameters_t video_params;
      type_video_link_profile video_link_profiles[MAX_VIDEO_LINK_PROFILES];
      osd_parameters_t osd_params;
      rc_parameters_t rc_params;
      telemetry_parameters_t telemetry_params;
      audio_parameters_t audio_params;
      type_functions_parameters functions_params;
      type_relay_parameters relay_params;
      type_alarms_parameters alarms_params;

      bool reloadIfChanged(bool bLoadStats);
      bool loadFromFile(const char* filename, bool bLoadStats = false);
      bool saveToFile(const char* filename, bool isOnController);
      int  getLoadedFileVersion();
      bool isRunningOnOpenIPCHardware();
      bool isRunningOnPiHardware();
      bool isRunningOnRadxaHardware();
      void populateHWInfo();
      bool populateVehicleSerialPorts();
      void resetRadioLinkDataRatesAndFlags(int iRadioLink);
      void addNewRadioLinkForRadioInterface(int iRadioInterfaceIndex, bool* pbDefault24Used, bool* pbDefault24_2Used, bool* pbDefault58Used, bool* pbDefault58_2Used);
      void populateRadioInterfacesInfoFromHardware();
      void populateDefaultRadioLinksInfoFromRadioInterfaces();
      bool check_update_radio_links();
      void check_detect_board_type();
      void resetToDefaults(bool generateId);
      void resetAllSettingsKeepPairing(bool bResetFreq);
      void resetAudioParams();
      void resetHWCapabilities();
      void resetProcessesParams();
      void disableProcessesParams();
      void resetRadioLinksParams();
      void resetRadioCapabilitiesRuntime(type_radio_runtime_capabilities_parameters* pRTInfo);
      void resetOSDFlags(int iScreen = -1);
      void resetOSDStatsFlags(int iScreen = -1);
      void resetOSDScreenToLayout(int iScreen, int iLayout);
      void checkUpdateOSDRadioLinksFlags(osd_parameters_t* pOSDParams);
      void resetTelemetryParams();
      void resetRCParams();
      void resetVideoParamsToDefaults();
      void resetAdaptiveVideoParams(int iVideoProfile);
      void resetVideoLinkProfiles();
      void resetVideoLinkProfilesBuiltIn();
      void resetVideoLinkProfile(int iProfile);
      void resetCameraToDefaults(int iCameraIndex);
      void resetCameraProfileToDefaults(camera_profile_parameters_t* pCamParams);
      void resetFunctionsParamsToDefaults();
      void resetRelayParamsToDefaults(type_relay_parameters* pRelayParams);

      void logVehicleRadioInfo();
      int logVehicleRadioLinkDifferences(const char* szPrefix, type_radio_links_parameters* pData1, type_radio_links_parameters* pData2);
      
      bool find_and_validate_camera_settings();
      bool validate_fps_and_exposure_settings(camera_profile_parameters_t* pCameraProfile, bool bFullForce);
      bool validate_settings();
      bool validateRadioSettings();
      bool validateProcessesParams();

      int getRadioInterfaceIndexForRadioLink(int iRadioLink);
      bool canSwapEnabledHighCapacityRadioInterfaces();
      bool swapEnabledHighCapacityRadioInterfaces();
      int getLastSwappedRadioInterface1();
      int getLastSwappedRadioInterface2();
      bool rotateRadioLinksOrder();

      bool radioInterfaceIsWiFiRadio(int iRadioInterfaceIndex);
      bool radioLinkIsWiFiRadio(int iRadioLinkIndex);
      bool radioLinkIsSiKRadio(int iRadioLinkIndex);
      bool radioLinkIsELRSRadio(int iRadioLinkIndex);
      int hasRadioCardsRTL8812AU();
      int hasRadioCardsRTL8812EU();
      int hasRadioCardsAtheros();
      bool isRadioDataRateSupportedByRadioLink(int iRadioLinkId, int iDataRate);

      bool hasCamera();
      char* getCameraName(int iCameraIndex);
      bool setCameraName(int iCameraIndex, const char* szCamName);
      u32 getActiveCameraType();
      bool isActiveCameraHDMI();
      bool isActiveCameraVeye();
      bool isActiveCameraVeye307();
      bool isActiveCameraVeye327290();
      bool isActiveCameraCSICompatible();
      bool isActiveCameraCSI();
      bool isActiveCameraOpenIPC();
      bool isActiveCameraSensorOpenIPCIMX415();

      void log_camera_profiles_differences(camera_profile_parameters_t* pCamProfile1, camera_profile_parameters_t* pCamProfile2, int iProfileIndex1, int iProfileIndex2);
      bool isVideoLinkFixedOneWay();
      bool isRadioLinkAdaptiveUsable(int iRadioLink);
      bool isAllVideoLinksFixedRate();
      int getInitialKeyframeIntervalMs(int iVideoProfile);
      int isVideoSettingsMatchingBuiltinVideoProfile(video_parameters_t* pVideoParams, type_video_link_profile* pVideoProfile);
      void logVideoSettingsDifferences(video_parameters_t* pNewVideoParams, type_video_link_profile* pNewVideoProfile, bool bReverseOrder = false);

      u32 getMaxVideoBitrateSupportedForCurrentRadioLinks();
      u32 getMaxVideoBitrateSupportedForRadioLinks(type_radio_links_parameters* pRadioLinksParams, video_parameters_t* pVideoParams, type_video_link_profile* pVideoProfiles);
      u32 getMaxVideoBitrateForRadioDatarate(int iRadioDatarateBPS, int iRadioLinkIndex);
      u32 getUsableVideoBitrateFromTotalBitrate(u32 uTotalBitrate, u32 uLoadPercent);
      int getRadioDataRateForVideoBitrate(u32 uVideoBitrateBPS, int iRadioLinkIndex, bool bLog);
      u32 getVideoProfileInitialVideoBitrate(int iVideoProfile);
      void setVideoProfilesDefaultVideoBitrates();
      bool validateVideoProfilesMaxVideoBitrate();
      int getCurrentVideoProfileMaxRetransmissionWindow();

      
      void getCameraFlags(char* szCameraFlags);
      u32  getVideoFlags(char* szVideoFlags, int iVideoProfile, u32 uOverwriteVideoBPS, int iOverwriteKeyframeMS);
      void populateVehicleTelemetryData_v6(t_packet_header_ruby_telemetry_extended_v6* pPHRTE);
      void populateFromVehicleTelemetryData_v3(t_packet_header_ruby_telemetry_extended_v3* pPHRTE);
      void populateFromVehicleTelemetryData_v4(t_packet_header_ruby_telemetry_extended_v4* pPHRTE);
      void populateFromVehicleTelemetryData_v5(t_packet_header_ruby_telemetry_extended_v5* pPHRTE);
      void populateFromVehicleTelemetryData_v6(t_packet_header_ruby_telemetry_extended_v6* pPHRTE);
      void setTelemetryTypeAndPort(int iTelemetryType, int iSerialPort, int iSerialSpeed);
      void syncModelSerialPortsToHardwareSerialPorts();

      void copy_video_link_profile(int from, int to);
      void convertECPercentageToData(type_video_link_profile* pVideoProfile);

      bool isAudioCapableAndEnabled();
      void constructLongName();
      const char* getShortName();
      const char* getLongName();
      u32 getVehicleFirmwareType();
      static const char* getVehicleType(u8 vtype);
      const char* getVehicleTypeString();
      int getSaveCount();
      void get_rc_channel_name(int nChannel, char* szOutput);

      void updateStatsMaxCurrentVoltage(t_packet_header_fc_telemetry* pPHFCT);
      void updateStatsEverySecond(t_packet_header_fc_telemetry* pPHFCT);

      void copy_radio_link_params(int iFrom, int iTo);
      void copy_radio_interface_params(int iFrom, int iTo);

      bool onControllerIdUpdated(u32 uNewControllerId);

   private:
      char vehicle_long_name[256];
      int iLoadedFileVersion;
      int iSaveCount;

      void generateUID();
      bool loadVersion10(FILE* fd); // from 7.6
      bool loadVersion11(FILE* fd); // from 11.5
      bool saveVersion11(FILE* fd, bool isOnController); // from 11.5
};

const char* model_getShortFlightMode(u8 mode);
const char* model_getLongFlightMode(u8 mode);
const char* model_getCameraProfileName(int profileIndex);

bool IsModelRadioConfigChanged(type_radio_links_parameters* pRadioLinks1, type_radio_interfaces_parameters* pRadioInterfaces1, type_radio_links_parameters* pRadioLinks2, type_radio_interfaces_parameters* pRadioInterfaces2);

u32 get_sw_version_major(Model* pModel);
u32 get_sw_version_minor(Model* pModel);
u32 get_sw_version_build(Model* pModel);
int is_sw_version_atleast(Model* pModel, int iMajor, int iMinor);
int is_sw_version_latest(Model* pModel);
