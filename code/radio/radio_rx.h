#pragma once

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include <pthread.h>
#include <semaphore.h>

#if defined (HW_PLATFORM_OPENIPC)
#define MAX_RX_PACKETS_QUEUE_REG 500
#define MAX_RX_PACKETS_QUEUE_HIP 200
#else
#define MAX_RX_PACKETS_QUEUE_REG 2000
#define MAX_RX_PACKETS_QUEUE_HIP 1000
#endif

typedef struct
{
   u32 uVehicleId;
   u32 uDetectedFirmwareType;
   u32 uTotalRxPackets;
   u32 uTotalRxPacketsBad;
   u32 uTotalRxPacketsLost;
   u32 uTmpRxPackets;
   u32 uTmpRxPacketsBad;
   u32 uTmpRxPacketsLost;
   int iMaxRxPacketsPerSec;
   int iMinRxPacketsPerSec;
   u32 uLastRxRadioLinkPacketIndex[MAX_RADIO_INTERFACES]; // per radio interface

} ALIGN_STRUCT_SPEC_INFO t_radio_rx_state_vehicle;

typedef struct
{
   u8* pPacketsBuffers[MAX_RX_PACKETS_QUEUE_REG];
   int iPacketsLengths[MAX_RX_PACKETS_QUEUE_REG];
   u8  uPacketsAreShort[MAX_RX_PACKETS_QUEUE_REG];
   u8  uPacketsRxInterface[MAX_RX_PACKETS_QUEUE_REG];
   int iQueueSize;
   _ATOMIC_PREFIX int iCurrentPacketIndexToWrite; // Where next packet will be added
   _ATOMIC_PREFIX int iCurrentPacketIndexToConsume; // Where the first packet to read/consume is
   int iStatsMaxPacketsInQueue;
   int iStatsMaxPacketsInQueueLastMinute;

   pthread_mutex_t mutexLock;
   sem_t* pSemaphoreWrite;
   sem_t* pSemaphoreRead;
} ALIGN_STRUCT_SPEC_INFO t_radio_rx_state_packets_queue;

typedef struct
{
   int iRadioInterfacesBroken[MAX_RADIO_INTERFACES];
   int iRadioInterfacesRxTimeouts[MAX_RADIO_INTERFACES];
   int iRadioInterfacesRxBadPackets[MAX_RADIO_INTERFACES];

   t_radio_rx_state_vehicle vehicles[MAX_CONCURENT_VEHICLES];
   t_radio_rx_state_packets_queue queue_high_priority;
   t_radio_rx_state_packets_queue queue_reg_priority;

   u32 uMaxLoopTime;
   u32 uAcceptedFirmwareType;
   u32 uTimeLastMinuteStatsUpdate;
   u32 uTimeLastStatsUpdate;
} ALIGN_STRUCT_SPEC_INFO t_radio_rx_state;

typedef struct
{
   u8* pPacketData;
   int iPacketLength;
   int iPacketIsShort;
   int iPacketRxInterface;
} ALIGN_STRUCT_SPEC_INFO type_received_radio_packet;

#ifdef __cplusplus
extern "C" {
#endif

void * _thread_radio_rx(void *argument);
int radio_rx_start_rx_thread(shared_mem_radio_stats* pSMRadioStats, int iSearchMode, u32 uAcceptedFirmwareType);
void radio_rx_stop_rx_thread();

void radio_rx_set_cpu_affinity(int iCPUCore);
void radio_rx_set_custom_thread_raw_priority(int iRawPriority);
void radio_rx_set_timeout_interval(int iMiliSec);

void radio_rx_pause_interface(int iInterfaceIndex, const char* szReason);
void radio_rx_resume_interface(int iInterfaceIndex);
void radio_rx_mark_quit();
void radio_rx_reset_signal_info();
void radio_rx_set_dev_mode(int iDevMode);
void radio_rx_set_packet_counter_output(u8* pCounterOutputHighPriority, u8* pCounterOutputData, u8* pCounterMissingPackets, u8* pCounterMissingPacketsMaxGap);
void radio_rx_set_air_gap_track_output(u8* pCounterRxAirgap);

int radio_rx_detect_firmware_type_from_packet(u8* pPacketBuffer, int nPacketLength);

int radio_rx_any_interface_broken();
int radio_rx_any_interface_bad_packets();
int radio_rx_get_interface_bad_packets_error_and_reset(int iInterfaceIndex);
int radio_rx_any_rx_timeouts();
int radio_rx_get_timeout_count_and_reset(int iInterfaceIndex);
void radio_rx_reset_interfaces_broken_state();

u32 radio_rx_get_and_reset_max_loop_time();
u32 radio_rx_get_and_reset_max_loop_time_read();
u32 radio_rx_get_and_reset_max_loop_time_queue();

u32 radio_rx_get_current_frame_start_time();
u32 radio_rx_get_current_frame_end_time();
u16 radio_rx_get_current_frame_number();
u8* radio_rx_wait_get_next_received_high_prio_packet(u32 uTimeoutMicroSec, int* pLength, int* pIsShortPacket, int* pRadioInterfaceIndex);
u8* radio_rx_wait_get_next_received_reg_prio_packet(u32 uTimeoutMicroSec, int* pLength, int* pIsShortPacket, int* pRadioInterfaceIndex);

#ifdef __cplusplus
}  
#endif
