#pragma once
#include "radiopackets2.h"

#define MAX_PACKETS_IN_QUEUE 64

typedef struct
{
   u32 uTimeAdded;
   u8  has_radio_header;
   u8  packet_buffer[MAX_PACKET_TOTAL_SIZE];
   u16 packet_length;
} ALIGN_STRUCT_SPEC_INFO t_packet_queue_item;

typedef struct
{
   t_packet_queue_item packets_queue[MAX_PACKETS_IN_QUEUE];
   int queue_start_pos; // position of first element in queue
   int queue_end_pos; // position of first free element in queue (after the last one)
   u32 timeFirstPacket;
} ALIGN_STRUCT_SPEC_INFO t_packet_queue;

#ifdef __cplusplus
extern "C" {
#endif  

void packets_queue_init(t_packet_queue* pQueue);

int packets_queue_is_empty(t_packet_queue* pQueue);
int packets_queue_has_packets(t_packet_queue* pQueue);

int packets_queue_inject_packet_first(t_packet_queue* pQueue, u8* pBuffer);
int packets_queue_inject_packet_first_mark_time(t_packet_queue* pQueue, u8* pBuffer);
int packets_queue_add_packet(t_packet_queue* pQueue, u8* pBuffer);
int packets_queue_add_packet_mark_time(t_packet_queue* pQueue, u8* pBuffer);
int packets_queue_add_packet2(t_packet_queue* pQueue, u8* pBuffer, int length, int has_radio_header, int iMarkTime);
u8* packets_queue_pop_packet(t_packet_queue* pQueue, int* pLength);
u8* packets_queue_pop_packet_and_time(t_packet_queue* pQueue, int* pLength, u32* puTimeAdded);
u8* packets_queue_peek_packet(t_packet_queue* pQueue, int index, int* pLength);

#ifdef __cplusplus
}  
#endif

