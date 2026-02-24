#pragma once
#include "base.h"

// Lower 2 bytes: alarms from vehicle
// Higher 2 bytes: alarms on controller

//---------------------------------------------------------
// Vehicle alarms

#define ALARM_ID_UNSUPORTED_USB_SERIAL  ((u32)(((u32)0x01)))
#define ALARM_ID_RECEIVED_INVALID_RADIO_PACKET  ((u32)(((u32)0x01)<<1))
#define ALARM_ID_RECEIVED_INVALID_RADIO_PACKET_RECONSTRUCTED  ((u32)(((u32)0x01)<<2))
#define ALARM_ID_VIDEO_CAPTURE_MALFUNCTION  ((u32)(((u32)0x01)<<3))
#define ALARM_ID_LINK_TO_CONTROLLER_LOST  ((u32)(((u32)0x01)<<4))
#define ALARM_ID_LINK_TO_CONTROLLER_RECOVERED  ((u32)(((u32)0x01)<<5))
#define ALARM_ID_VEHICLE_VIDEO_DATA_OVERLOAD ((u32)(((u32)0x01)<<6)) // param: low word: video total bitrate in kb, high word: current radio datarate in kb
#define ALARM_ID_VEHICLE_VIDEO_TX_OVERLOAD ((u32)(((u32)0x01)<<7)) // param: low word: current tx time in ms/sec, threshold for max tx time in ms/sec, param2: 1 - local overload, 0 - overall overload
#define ALARM_ID_VEHICLE_CPU_LOOP_OVERLOAD ((u32)(((u32)0x01)<<8)) // param: low word: loop miliseconds average, highword: loop miliseconds spike
#define ALARM_ID_VEHICLE_RX_TIMEOUT ((u32)(((u32)0x01)<<9))
#define ALARM_ID_VEHICLE_LOW_STORAGE_SPACE ((u32)(((u32)0x01)<<10))
#define ALARM_ID_VEHICLE_STORAGE_WRITE_ERRROR ((u32)(((u32)0x01)<<11))
#define ALARM_ID_VEHICLE_CPU_RX_LOOP_OVERLOAD ((u32)(((u32)0x01)<<12)) // param: loop miliseconds spike
#define ALARM_ID_RADIO_INTERFACE_DOWN ((u32)(((u32)0x01)<<13)) // flags: interface index that is not working
#define ALARM_ID_RADIO_INTERFACE_REINITIALIZED ((u32)(((u32)0x01)<<14)) // flags: interface index that was reinitialized
#define ALARM_ID_VEHICLE_VIDEO_CAPTURE_RESTARTED ((u32)(((u32)0x01)<<15))

//--------------------------------------------------------
// Controller alarms

#define ALARM_ID_RADIO_LINK_DATA_OVERLOAD ((u32)(((u32)0x01)<<16)) // param1: low 3 bytes: total send bitrate in bytes/sec, high byte: radio interface, param2: current radio datarate in bytes/sec
#define ALARM_ID_CONTROLLER_RECEIVED_INVALID_RADIO_PACKET  ((u32)(((u32)0x01)<<17))
#define ALARM_ID_CONTROLLER_NO_INTERFACES_FOR_RADIO_LINK  ((u32)(((u32)0x01)<<18))
#define ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD ((u32)(((u32)0x01)<<19)) // param: low word: loop miliseconds average, highword: loop miliseconds spike
#define ALARM_ID_CONTROLLER_RX_TIMEOUT  ((u32)(((u32)0x01)<<20)) // param: which interface timedout
#define ALARM_ID_CONTROLLER_IO_ERROR  ((u32)(((u32)0x01)<<21))
#define ALARM_ID_CONTROLLER_LOW_STORAGE_SPACE ((u32)(((u32)0x01)<<22))
#define ALARM_ID_CONTROLLER_STORAGE_WRITE_ERRROR ((u32)(((u32)0x01)<<23))
#define ALARM_ID_CONTROLLER_PAIRING_COMPLETED ((u32)(((u32)0x01)<<24))
#define ALARM_ID_FIRMWARE_OLD ((u32)(((u32)0x01)<<25))
#define ALARM_ID_CONTROLLER_CPU_RX_LOOP_OVERLOAD ((u32)(((u32)0x01)<<26)) // param: loop miliseconds spike
#define ALARM_ID_UNSUPPORTED_VIDEO_TYPE ((u32)(((u32)0x01)<<27))  // flags1: vehicle id, flags2: video type
#define ALARM_ID_DEVELOPER_ALARM ((u32)(((u32)0x01)<<28))
#define ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD_RECORDING ((u32)(((u32)0x01)<<29)) // param: low word: loop miliseconds average, highword: loop miliseconds spike

//-----------------------------------------------
// Generic alarm

#define ALARM_ID_GENERIC ((u32)(((u32)0x01)<<30)) // first param is the generic alarm id
#define ALARM_ID_GENERIC_STATUS_UPDATE ((u32)(((u32)0x01)<<31))

#define ALARM_ID_GENERIC_TYPE_UNKNOWN_VIDEO 1
#define ALARM_ID_GENERIC_TYPE_RECEIVED_DEPRECATED_MESSAGE 2
#define ALARM_ID_GENERIC_TYPE_SWAP_RADIO_INTERFACES_NOT_POSSIBLE 3
#define ALARM_ID_GENERIC_TYPE_SWAP_RADIO_INTERFACES_FAILED 4
#define ALARM_ID_GENERIC_TYPE_SWAPPED_RADIO_INTERFACES 5
#define ALARM_ID_GENERIC_TYPE_RELAYED_TELEMETRY_LOST 6
#define ALARM_ID_GENERIC_TYPE_RELAYED_TELEMETRY_RECOVERED 7
#define ALARM_ID_GENERIC_TYPE_WRONG_OPENIPC_KEY 9
#define ALARM_ID_GENERIC_TYPE_MISSED_TELEMETRY_DATA 10
#define ALARM_ID_GENERIC_TYPE_UNKNOWN_VEHICLE 11


#define ALARM_FLAG_GENERIC_STATUS_RECONFIGURING_RADIO_INTERFACE ((u32)(((u32)0x01)<<1))
#define ALARM_FLAG_GENERIC_STATUS_RECONFIGURED_RADIO_INTERFACE ((u32)(((u32)0x01)<<2))
#define ALARM_FLAG_GENERIC_STATUS_RECONFIGURED_RADIO_INTERFACE_FAILED ((u32)(((u32)0x01)<<3))

#define ALARM_FLAG_GENERIC_STATUS_REASIGNING_RADIO_LINKS_START ((u32)(((u32)0x01)<<4))
#define ALARM_FLAG_GENERIC_STATUS_REASIGNING_RADIO_LINKS_END ((u32)(((u32)0x01)<<5))
#define ALARM_FLAG_GENERIC_STATUS_SENT_PAIRING_REQUEST ((u32)(((u32)0x01)<<6))

#define ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT ((u32)(((u32)0x01)<<1))
#define ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT_TRUNCATED ((u32)(((u32)0x01)<<2))
#define ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT_WOULD_BLOCK ((u32)(((u32)0x01)<<3))
#define ALARM_FLAG_IO_ERROR_VIDEO_USB_OUTPUT ((u32)(((u32)0x01)<<4))
#define ALARM_FLAG_IO_ERROR_VIDEO_USB_OUTPUT_TRUNCATED ((u32)(((u32)0x01)<<5))
#define ALARM_FLAG_IO_ERROR_VIDEO_USB_OUTPUT_WOULD_BLOCK ((u32)(((u32)0x01)<<6))
#define ALARM_FLAG_IO_ERROR_VIDEO_MPP_DECODER_STALLED ((u32)(((u32)0x01)<<7))

#define ALARM_FLAG_DEVELOPER_ALARM_RETRANSMISSIONS_OFF 1
#define ALARM_FLAG_DEVELOPER_ALARM_UDP_SKIPPED 2

#define ALARM_FLAG_VIDEO_CAPTURE_MALFUNCTION 0
#define ALARM_FLAG_VIDEO_CAPTURE_PARAMETERS_UPDATE 1

#ifdef __cplusplus
extern "C" {
#endif

void alarms_to_string(u32 uAlarms, u32 uFlags1, u32 uFlags2, char* szOutput);

#ifdef __cplusplus
}
#endif
