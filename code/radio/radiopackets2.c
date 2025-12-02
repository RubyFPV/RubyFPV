/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga petrusoroaga@yahoo.com
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

#include "../base/base.h"
#include "radiopackets2.h"
#include "radiolink.h"

void radio_packet_init(t_packet_header* pPH, u8 component, u8 packet_type, u32 uStreamId)
{
   if ( NULL == pPH )
      return;
   
   if ( uStreamId >= MAX_RADIO_STREAMS )
      uStreamId = MAX_RADIO_STREAMS-1;
   pPH->uCRC = 0;
   pPH->vehicle_id_src = 0;
   pPH->vehicle_id_dest = 0;
   pPH->radio_link_packet_index = 0;
   pPH->stream_packet_idx = uStreamId << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   pPH->packet_flags = component & PACKET_FLAGS_MASK_MODULE;
   pPH->packet_type = packet_type;
   pPH->total_length = sizeof(t_packet_header);
   pPH->packet_flags_extended = 0;
}

void radio_packet_compute_crc(u8* pBuffer, int length)
{
   u32 crc = base_compute_crc32(pBuffer + sizeof(u32), length-sizeof(u32)); 
   u32* p = (u32*)pBuffer;
   *p = crc;
}

int radio_packet_check_crc(u8* pBuffer, int length)
{
   u32 crc = base_compute_crc32(pBuffer + sizeof(u32), length-sizeof(u32)); 
   u32* p = (u32*)pBuffer;
   if ( *p != crc )
      return 0;
   return 1;
}

void radio_populate_ruby_telemetry_v6_from_ruby_telemetry_v3(t_packet_header_ruby_telemetry_extended_v6* pV6, t_packet_header_ruby_telemetry_extended_v3* pV3)
{
   if ( (NULL == pV6) || (NULL == pV3) )
      return;

   pV6->uRubyFlags = pV3->uRubyFlags;
   pV6->rubyVersion = pV3->rubyVersion;
   pV6->uVehicleId = pV3->uVehicleId;
   pV6->vehicle_type = pV3->vehicle_type;
   memcpy(pV6->vehicle_name, pV3->vehicle_name, MAX_VEHICLE_NAME_LENGTH);
   pV6->radio_links_count = pV3->radio_links_count;
   memcpy(pV6->uRadioFrequenciesKhz, pV3->uRadioFrequenciesKhz, MAX_RADIO_INTERFACES*sizeof(u32));

   pV6->uRelayLinks = pV3->uRelayLinks;
   
   pV6->downlink_tx_video_bitrate_bps = pV3->downlink_tx_video_bitrate_bps;
   pV6->downlink_tx_video_all_bitrate_bps = pV3->downlink_tx_video_all_bitrate_bps;
   pV6->downlink_tx_data_bitrate_bps = pV3->downlink_tx_data_bitrate_bps;

   pV6->downlink_tx_video_packets_per_sec = pV3->downlink_tx_video_packets_per_sec;
   pV6->downlink_tx_data_packets_per_sec = pV3->downlink_tx_data_packets_per_sec;
   pV6->downlink_tx_compacted_packets_per_sec = pV3->downlink_tx_compacted_packets_per_sec;

   pV6->temperatureC = pV3->temperatureC;
   pV6->cpu_load = pV3->cpu_load;
   pV6->cpu_mhz = pV3->cpu_mhz;
   pV6->throttled = pV3->throttled;

   memcpy(pV6->last_sent_datarate_bps, pV3->last_sent_datarate_bps, MAX_RADIO_INTERFACES*2*sizeof(int));
   memcpy(pV6->last_recv_datarate_bps, pV3->last_recv_datarate_bps, MAX_RADIO_INTERFACES*sizeof(int));
   memcpy(pV6->uplink_rssi_dbm, pV3->uplink_rssi_dbm, MAX_RADIO_INTERFACES*sizeof(u8));
   memcpy(pV6->uplink_link_quality, pV3->uplink_link_quality, MAX_RADIO_INTERFACES*sizeof(u8));
   memset(pV6->uplink_rssi_snr, 0, MAX_RADIO_INTERFACES*sizeof(u8));

   pV6->uplink_rc_rssi = pV3->uplink_rc_rssi;
   pV6->uplink_mavlink_rc_rssi = pV3->uplink_mavlink_rc_rssi;
   pV6->uplink_mavlink_rx_rssi = pV3->uplink_mavlink_rx_rssi;

   pV6->uExtraRubyFlags = pV3->extraFlags;
   pV6->extraSize = pV3->extraSize;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      pV6->iTxPowers[i] = 0;
}

void radio_populate_ruby_telemetry_v6_from_ruby_telemetry_v4(t_packet_header_ruby_telemetry_extended_v6* pV6, t_packet_header_ruby_telemetry_extended_v4* pV4)
{
   if ( (NULL == pV6) || (NULL == pV4) )
      return;

   pV6->uRubyFlags = pV4->uRubyFlags;
   pV6->rubyVersion = pV4->rubyVersion;
   pV6->uVehicleId = pV4->uVehicleId;
   pV6->vehicle_type = pV4->vehicle_type;
   memcpy(pV6->vehicle_name, pV4->vehicle_name, MAX_VEHICLE_NAME_LENGTH);
   
   pV6->radio_links_count = pV4->radio_links_count;
   memcpy(pV6->uRadioFrequenciesKhz, pV4->uRadioFrequenciesKhz, MAX_RADIO_INTERFACES*sizeof(u32));

   pV6->uRelayLinks = pV4->uRelayLinks;
   
   pV6->downlink_tx_video_bitrate_bps = pV4->downlink_tx_video_bitrate_bps;
   pV6->downlink_tx_video_all_bitrate_bps = pV4->downlink_tx_video_all_bitrate_bps;
   pV6->downlink_tx_data_bitrate_bps = pV4->downlink_tx_data_bitrate_bps;

   pV6->downlink_tx_video_packets_per_sec = pV4->downlink_tx_video_packets_per_sec;
   pV6->downlink_tx_data_packets_per_sec = pV4->downlink_tx_data_packets_per_sec;
   pV6->downlink_tx_compacted_packets_per_sec = pV4->downlink_tx_compacted_packets_per_sec;

   pV6->temperatureC = pV4->temperatureC;
   pV6->cpu_load = pV4->cpu_load;
   pV6->cpu_mhz = pV4->cpu_mhz;
   pV6->throttled = pV4->throttled;

   memcpy(pV6->last_sent_datarate_bps, pV4->last_sent_datarate_bps, MAX_RADIO_INTERFACES*2*sizeof(int));
   memcpy(pV6->last_recv_datarate_bps, pV4->last_recv_datarate_bps, MAX_RADIO_INTERFACES*sizeof(int));
   memcpy(pV6->uplink_rssi_dbm, pV4->uplink_rssi_dbm, MAX_RADIO_INTERFACES*sizeof(u8));
   memcpy(pV6->uplink_link_quality, pV4->uplink_link_quality, MAX_RADIO_INTERFACES*sizeof(u8));
   memset(pV6->uplink_rssi_snr, 0, MAX_RADIO_INTERFACES*sizeof(u8));

   pV6->uplink_rc_rssi = pV4->uplink_rc_rssi;
   pV6->uplink_mavlink_rc_rssi = pV4->uplink_mavlink_rc_rssi;
   pV6->uplink_mavlink_rx_rssi = pV4->uplink_mavlink_rx_rssi;

   pV6->uExtraRubyFlags = pV4->uExtraRubyFlags;
   pV6->extraSize = pV4->extraSize;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      pV6->iTxPowers[i] = 0;
}


void radio_populate_ruby_telemetry_v6_from_ruby_telemetry_v5(t_packet_header_ruby_telemetry_extended_v6* pV6, t_packet_header_ruby_telemetry_extended_v5* pV5)
{
   if ( (NULL == pV6) || (NULL == pV5) )
      return;

   pV6->uRubyFlags = pV5->uRubyFlags;
   pV6->rubyVersion = pV5->rubyVersion;
   pV6->uVehicleId = pV5->uVehicleId;
   pV6->vehicle_type = pV5->vehicle_type;
   memcpy(pV6->vehicle_name, pV5->vehicle_name, MAX_VEHICLE_NAME_LENGTH);
   
   pV6->radio_links_count = pV5->radio_links_count;
   memcpy(pV6->uRadioFrequenciesKhz, pV5->uRadioFrequenciesKhz, MAX_RADIO_INTERFACES*sizeof(u32));

   pV6->uRelayLinks = pV5->uRelayLinks;
   
   pV6->downlink_tx_video_bitrate_bps = pV5->downlink_tx_video_bitrate_bps;
   pV6->downlink_tx_video_all_bitrate_bps = pV5->downlink_tx_video_all_bitrate_bps;
   pV6->downlink_tx_data_bitrate_bps = pV5->downlink_tx_data_bitrate_bps;

   pV6->downlink_tx_video_packets_per_sec = pV5->downlink_tx_video_packets_per_sec;
   pV6->downlink_tx_data_packets_per_sec = pV5->downlink_tx_data_packets_per_sec;
   pV6->downlink_tx_compacted_packets_per_sec = pV5->downlink_tx_compacted_packets_per_sec;

   pV6->temperatureC = pV5->temperatureC;
   pV6->cpu_load = pV5->cpu_load;
   pV6->cpu_mhz = pV5->cpu_mhz;
   pV6->throttled = pV5->throttled;

   memcpy(pV6->last_sent_datarate_bps, pV5->last_sent_datarate_bps, MAX_RADIO_INTERFACES*2*sizeof(int));
   memcpy(pV6->last_recv_datarate_bps, pV5->last_recv_datarate_bps, MAX_RADIO_INTERFACES*sizeof(int));
   memcpy(pV6->uplink_rssi_dbm, pV5->uplink_rssi_dbm, MAX_RADIO_INTERFACES*sizeof(u8));
   memcpy(pV6->uplink_link_quality, pV5->uplink_link_quality, MAX_RADIO_INTERFACES*sizeof(u8));
   memcpy(pV6->uplink_rssi_snr, pV5->uplink_rssi_snr, MAX_RADIO_INTERFACES*sizeof(u8));
   memcpy(pV6->iTxPowers, pV5->iTxPowers, MAX_RADIO_INTERFACES*sizeof(int));

   pV6->uplink_rc_rssi = pV5->uplink_rc_rssi;
   pV6->uplink_mavlink_rc_rssi = pV5->uplink_mavlink_rc_rssi;
   pV6->uplink_mavlink_rx_rssi = pV5->uplink_mavlink_rx_rssi;

   pV6->uExtraRubyFlags = pV5->uExtraRubyFlags;
   pV6->extraSize = pV5->extraSize;
}

void radio_packets_log_sizes()
{
   /*
   char szBuff[128];
   sprintf(szBuff, "[Radio] Size of radio t_packet_header: %d bytes", (int)sizeof(t_packet_header));
   log_always(szBuff);
   sprintf(szBuff, "[Radio] Size of radio t_packet_header_command: %d bytes", (int)sizeof(t_packet_header_command));
   log_always(szBuff);
   sprintf(szBuff, "[Radio] Size of radio t_packet_header_command_response: %d bytes", (int)sizeof(t_packet_header_command_response));
   log_always(szBuff);
   sprintf(szBuff, "[Radio] Size of radio t_packet_header_video_segment: %d bytes", (int)sizeof(t_packet_header_video_segment));
   log_always(szBuff);
   sprintf(szBuff, "[Radio] Size of radio t_packet_header_video_segment_important: %d bytes", (int)sizeof(t_packet_header_video_segment_important));
   log_always(szBuff);
   sprintf(szBuff, "[Radio] Size of radio t_packet_header_ruby_telemetry_short: %d bytes", (int)sizeof(t_packet_header_ruby_telemetry_short));
   log_always(szBuff);
   sprintf(szBuff, "[Radio] Size of radio t_packet_header_ruby_telemetry_extended_v4: %d bytes", (int)sizeof(t_packet_header_ruby_telemetry_extended_v4));
   log_always(szBuff);
   sprintf(szBuff, "[Radio] Size of radio t_packet_header_ruby_telemetry_extended_v5: %d bytes", (int)sizeof(t_packet_header_ruby_telemetry_extended_v5));
   log_always(szBuff);
   sprintf(szBuff, "[Radio] Size of radio t_packet_header_ruby_telemetry_extended_extra_info: %d bytes", (int)sizeof(t_packet_header_ruby_telemetry_extended_extra_info));
   log_always(szBuff);
   sprintf(szBuff, "[Radio] Size of radio t_packet_header_ruby_telemetry_extended_extra_info_retransmissions: %d bytes", (int)sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions));
   log_always(szBuff);
   sprintf(szBuff, "[Radio] Size of radio t_packet_header_fc_telemetry: %d bytes", (int)sizeof(t_packet_header_fc_telemetry));
   log_always(szBuff);
   sprintf(szBuff, "[Radio] Size of radio t_packet_header_fc_extra: %d bytes", (int)sizeof(t_packet_header_fc_extra));
   log_always(szBuff);
   */
}
