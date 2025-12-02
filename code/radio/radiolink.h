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

#pragma once
#include "../base/base.h"
#include "../base/hardware.h"
#include "../base/shared_mem.h"
#include "radioflags.h"
#include "radiotap.h"
#include "radiopackets2.h"
#include <time.h>
#include <sys/resource.h>

#define MAX_PACKET_LENGTH_PCAP 4096

#define RADIO_PROCESSING_ERROR_NO_ERROR 0x00
#define RADIO_PROCESSING_ERROR_CODE_INVALID_CRC_RECEIVED 0x01
#define RADIO_PROCESSING_ERROR_CODE_PACKET_RECEIVED_TOO_SMALL 0x02
#define RADIO_PROCESSING_ERROR_INVALID_PARAMETERS 0x0E
#define RADIO_PROCESSING_ERROR_INVALID_RECEIVED_PACKET 0x0F

#define RADIO_READ_ERROR_NO_ERROR 0
#define RADIO_READ_ERROR_TIMEDOUT 1
#define RADIO_READ_ERROR_INTERFACE_BROKEN 2
#define RADIO_READ_ERROR_READ_ERROR 3


#ifdef __cplusplus
extern "C" {
#endif  

void radio_init_link_structures();
void radio_link_cleanup();
void radio_enable_crc_gen(int enable);
void radio_set_debug_flag(int iDebugFlag);
void radio_set_link_clock_delta(int iVehicleBehindMilisec);
int  radio_get_link_clock_delta();
void radio_set_use_pcap_for_tx(int iEnablePCAPTx);
void radio_set_bypass_socket_buffers(int iBypass);
int  radio_set_out_datarate(int rate_bps, u8 uPacketType, u32 uTimeNow); // positive: classic in bps, negative: MCS; returns 1 if it was changed
u32  radio_get_current_frames_flags();
u32  radio_get_current_frames_flags_datarate();
void radio_set_frames_flags(u32 frameFlags, u32 uTimeNow); // frame type, MSC Flags
u32 radio_get_received_frames_type();

void radio_reset_packets_default_frequencies(int iRCEnabled);
int radio_can_send_packet_on_slow_link(int iLinkId, int iPacketType, int iFromController, u32 uTimeNow);
int radio_get_last_second_sent_bps_for_radio_interface(int iInterfaceIndex, u32 uTimeNow);
int radio_get_last_500ms_sent_bps_for_radio_interface(int iInterfaceIndex, u32 uTimeNow);

// Tells caller if a radio interface is broken (maybe unplugged)
int radio_interfaces_broken();

int radio_open_interface_for_read(int interfaceIndex, int portNumber);
int radio_open_interface_for_write(int interfaceIndex);
void radio_close_interfaces_for_read();
void radio_close_interface_for_read(int interfaceIndex);
void radio_close_interface_for_write(int interfaceIndex);

u8* radio_process_wlan_data_in(int interfaceNumber, int* piOutPacketLength, int* piOutRxDatarate, u32 uTimeNow);
int radio_get_last_read_error_code();

// returns 0 for failure, total length of packet for success
int packet_process_and_check(int interfaceNb, u8* pPacketBuffer, int iBufferLength, int* pbCRCOk);
int get_last_processing_error_code();

u32 radio_get_next_radio_link_packet_index(int iLocalRadioLinkId);
int radio_build_new_raw_ieee_packet(int iLocalRadioLinkId, u8* pRawPacket, u8* pPacketData, int nInputLength, int portNb, int bEncrypt);
int radio_write_raw_ieee_packet(int interfaceIndex, u8* pData, int dataLength, int iRepeatCount);
int radio_write_serial_packet(int interfaceIndex, u8* pData, int dataLength, u32 uTimeNow);
int radio_write_sik_packet(int interfaceIndex, u8* pData, int dataLength, u32 uTimeNow);

#ifdef __cplusplus
}  
#endif
