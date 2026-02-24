#pragma once
// Version 1.3

#include <stdint.h>

#define ALIGN_STRUCT_SPEC_INFO __attribute__((aligned(4)))

#define MAX_U32 0xFFFFFFFF
#define MAX_VEHICLE_NAME_LENGTH 16


#define MODEL_TYPE_GENERIC 0
#define MODEL_TYPE_DRONE 1
#define MODEL_TYPE_AIRPLANE 2
#define MODEL_TYPE_HELI 3
#define MODEL_TYPE_CAR 4
#define MODEL_TYPE_BOAT 5
#define MODEL_TYPE_ROBOT 6
#define MODEL_TYPE_RELAY 7
#define MODEL_TYPE_LAST 8

#define FLIGHT_MODE_ARMED 128 // last bit set to 1, remaining 7 bits are a flight mode as int value
#define FLIGHT_MODE_MANUAL 1
#define FLIGHT_MODE_STAB 2
#define FLIGHT_MODE_ALTH 3
#define FLIGHT_MODE_LOITER 4
#define FLIGHT_MODE_RTL 5
#define FLIGHT_MODE_LAND 6
#define FLIGHT_MODE_POSHOLD 7
#define FLIGHT_MODE_AUTOTUNE 8
#define FLIGHT_MODE_ACRO 9
#define FLIGHT_MODE_FBWA 10
#define FLIGHT_MODE_FBWB 11
#define FLIGHT_MODE_CRUISE 12
#define FLIGHT_MODE_TAKEOFF 13
#define FLIGHT_MODE_RATE 15
#define FLIGHT_MODE_HORZ 16
#define FLIGHT_MODE_CIRCLE 17
#define FLIGHT_MODE_AUTO 18
#define FLIGHT_MODE_QSTAB 20
#define FLIGHT_MODE_QHOVER 21
#define FLIGHT_MODE_QLOITER 22
#define FLIGHT_MODE_QLAND 23
#define FLIGHT_MODE_QRTL 24

#define FC_TELE_FLAGS_ARMED 1
#define FC_TELE_FLAGS_POS_CURRENT 2 // received lon, lat are current position
#define FC_TELE_FLAGS_POS_HOME 4    // received lon, lat are home position
#define FC_TELE_FLAGS_HAS_GPS_FIX 8  // has a 3d gps fix
#define FC_TELE_FLAGS_RC_FAILSAFE 16   // RC failsafe is triggered
//#define FC_TELE_FLAGS_NO_FC_TELEMETRY 32 // No telemetry from flight controller
#define FC_TELE_FLAGS_HAS_MESSAGE 64 // Set if there is also a message structure from flight controller;
#define FC_TELE_FLAGS_HAS_ATTITUDE 128 // Set if the attitude was received
#define FC_MESSAGE_MAX_LENGTH 101

typedef enum MAVLINK_GPS_FIX_TYPE
{
   MAVLINK_GPS_FIX_TYPE_NO_GPS=0, /* No GPS connected | */
   MAVLINK_GPS_FIX_TYPE_NO_FIX=1, /* No position information, GPS is connected | */
   MAVLINK_GPS_FIX_TYPE_2D_FIX=2, /* 2D position | */
   MAVLINK_GPS_FIX_TYPE_3D_FIX=3, /* 3D position | */
   MAVLINK_GPS_FIX_TYPE_DGPS=4, /* DGPS/SBAS aided 3D position | */
   MAVLINK_GPS_FIX_TYPE_RTK_FLOAT=5, /* RTK float, 3D position | */
   MAVLINK_GPS_FIX_TYPE_RTK_FIXED=6, /* RTK Fixed, 3D position | */
   MAVLINK_GPS_FIX_TYPE_STATIC=7, /* Static fixed, typically used for base stations | */
   MAVLINK_GPS_FIX_TYPE_PPP=8, /* PPP, 3D position. | */
   MAVLINK_GPS_FIX_TYPE_ENUM_END=9, /*  | */
} MAVLINK_GPS_FIX_TYPE;

#ifdef __cplusplus
extern "C" {
#endif


typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

typedef struct
{
   u8  pi_temperature; // celsius
   u8  cpu_load; // percent 0...100
   u16 cpu_mhz;
   u8  throttled; // throttle flags as defined by Raspberry
   u8  rssi_dbm; // 200 offset. that is: real rssi value = rssi_dbm - 200 (dbm is negative);
   u8  rssi_quality; // 0...100
   char vehicle_name[MAX_VEHICLE_NAME_LENGTH];
   u8  vehicle_type;

   u8 uFCFlags; // bitmask with flags
   u8 flight_mode;
   u32 arm_time; // in miliseconds
   u8 throttle; // percent 0...100
   u16 voltage; // 1/100 volts
   u16 current; // 1/100 amps
   u16 voltage2; // 1/100 volts
   u16 current2; // 1/100 amps
   u16 mah;  // total consumed current
   u32 altitude; // 1/100 meters  -1000 m
   u32 altitude_abs; // 1/100 meters -1000 m
   u32 distance; // 1/100 meters
   u32 total_distance; // 1/100 meters

   u32 vspeed; // 1/100 meters -1000 m
   u32 aspeed; // airspeed (1/100 meters - 1000 m)
   u32 hspeed; // 1/100 meters -1000 m
   float roll; // angle in degrees
   float pitch; // angle in degrees

   u8 satelites;
   u8 gps_fix_type; // enum from MAVLink libraries, see above
   u16 hdop; // 1/100
   u16 heading; // angle in degrees

   int32_t latitude; // 1/10000000;
   int32_t longitude; // 1/10000000;
   int isHomeSet; // 0 or 1
   int32_t home_latitude; // 1/10000000;
   int32_t home_longitude; // 1/10000000;

   u8 rc_rssi; // percent 0...100
   u8 extra_info[12];

   float fCameraFOVHorizontal;
   float fCameraFOVVertical;

   float fMaxCurrent;  // amps
   float fMinVoltage;  // volts
   float fMaxAltitude; // meters
   float fMaxDistance; // meters
   float fTotalCurrent; // mAh
   u32 u32VehicleOnTime; // milisec
   u32 u32VehicleArmTime; // milisec

   // Parameters

   int ahi_warning_angle; // set angle for warnings, in degrees

   void* pExtraInfo; // Extended info, see the structure below: vehicle_and_telemetry_info2_t, valid starting with version 6.8

} ALIGN_STRUCT_SPEC_INFO vehicle_and_telemetry_info_t;


typedef struct
{
   u32 uTimeNow; // Controller time now
   u32 uTimeNowVehicle; // Vehicle time now
   u32 uVehicleId;
   u8  uIsSpectatorMode; // 0/1
   u8  uIsRelaing;
   u32 uRelayedVehicleId;
   u8  uActiveCamera; // 0..N (camera or video stream)
   u8  uTotalCameras; // total number of cameras (cameras or video streams)

   // Info for the second GPS, if present. Otherways the values are all 0.
   u8 satelites2;
   u8 gps_fix_type2; // enum from MAVLink libraries, see above
   u16 hdop2; // 1/100
   u16 heading2; // angle in degrees

   u16 uWindHeading; // angle in degree
   float fWindSpeed; // in m/sec
   u16 uThrottleInput;
   u16 uThrottleOutput;

} ALIGN_STRUCT_SPEC_INFO vehicle_and_telemetry_info2_t;

#ifdef __cplusplus
}
#endif
