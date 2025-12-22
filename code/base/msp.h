#pragma once

#include "base.h"
#include "../radio/radiopackets2.h"

#define MSP_STATE_WAIT_HEADER1 0
#define MSP_STATE_WAIT_HEADER2 1
#define MSP_STATE_WAIT_DIR 2
#define MSP_STATE_WAIT_SIZE 3
#define MSP_STATE_WAIT_TYPE 4
#define MSP_STATE_PARSE_DATA 5
#define MSP_STATE_WAIT_CHECKSUM 6
#define MSP_STATE_ERROR 255

#define MSP_DIR_TO_FC 0
#define MSP_DIR_FROM_FC 1

#define MSP_CMD_API_VERSION 1
#define MSP_CMD_FC_VARIANT 2
#define MSP_CMD_FC_VERSION 3
#define MSP_CMD_NAME 10
#define MSP_CMD_STATUS 101
#define MSP_CMD_BATTERY_STATE 130
#define MSP_CMD_STATUS_EX 150
#define MSP_CMD_DISPLAYPORT 182
#define MSP_CMD_SET_OSD_CANVAS 188

typedef enum
{
    MSP_DISPLAYPORT_KEEPALIVE,
    MSP_DISPLAYPORT_CLOSE,
    MSP_DISPLAYPORT_CLEAR,
    MSP_DISPLAYPORT_DRAW_STRING,
    MSP_DISPLAYPORT_DRAW_SCREEN,
    MSP_DISPLAYPORT_SET_OPTIONS,
    MSP_DISPLAYPORT_DRAW_SYSTEM
} msp_osd_command_type;

// Max 64x24 chars
#define MAX_MSP_CHARS_BUFFER 1536

typedef enum {
    MSP_SD_OPTION_30_16,
    MSP_HD_OPTION_50_18,
    MSP_HD_OPTION_30_16,
    MSP_HD_OPTION_60_22
} msp_osd_hd_options;


typedef struct
{
   t_packet_header_telemetry_msp headerTelemetryMSP;

   u8  uMSPRawCommand[256]; // Max size is one byte long
   int iMSPRawCommandFilledBytes;
   int iMSPState;
   int iMSPDirection;
   u8  uMSPCommandPayload[256]; // Max size is one byte long
   int iMSPCommandPayloadSize;
   int iMSPParsedCommandPayloadBytes;
   u8  uMSPChecksum;
   u8  uMSPCommand;
   u8  uMSPPreviousCommand;
   u8  uMSPDisplayPortCommand;
   u8  uMSPDisplayPortPreviousCommand;
   u32 uLastMSPCommandReceivedTime;

   u16 uScreenChars[MAX_MSP_CHARS_BUFFER]; // Max 64x24
   u16 uScreenCharsTmp[MAX_MSP_CHARS_BUFFER]; // Max 64x24
   int iMaxDrawX;
   int iMaxDrawY;
   int iCountDrawnStrings;
   int iLastDrawFrameNumber;
   bool bEmptyBuffer;
} ALIGN_STRUCT_SPEC_INFO type_msp_parse_state;

void parse_msp_reset_state(type_msp_parse_state* pMSPState);
void parse_msp_incoming_data(type_msp_parse_state* pMSPState, u8* pData, int iDataLength, bool bAllowScreenUpdate);
