#pragma once

#define SHORT_PACKET_START_BYTE_REG_PACKET 0xAA
#define SHORT_PACKET_START_BYTE_START_PACKET 0x0F
#define SHORT_PACKET_START_BYTE_END_PACKET 0x10

// Short packets (t_packet_header_short) are sent only on low bandwidth radio links

// Short packets usually have 24 bytes usable payload
typedef struct
{
   u8 start_header; // 0xAA, 0x0F, 0x10
   u8 crc; // Computed for everything after this crc
   u8 packet_id;
   u8 last_ack_packet_id;
   u8 data_length; // max 240
} __attribute__((packed)) t_packet_header_short;

#ifdef __cplusplus
extern "C" {
#endif

void radio_packets_short_init();
void radio_packet_short_init(t_packet_header_short* pPHS);
u8 radio_packets_short_get_next_id_for_radio_interface(int iInterfaceIndex);
int radio_buffer_is_valid_short_packet(u8* pBuffer, int iLength);
#ifdef __cplusplus
}
#endif
