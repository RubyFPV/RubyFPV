#pragma once
#include "base.h"
#include "config.h"
#include "hardware_radio.h"

#define MAX_HISTORY_RADIO_STATS_RECV_SLICES 50
#define MAX_RADIO_STATS_INTERFACE_RX_HISTORY_SLICES 100

typedef struct
{
   u8 uHistPacketsTypes[MAX_RADIO_STATS_INTERFACE_RX_HISTORY_SLICES];
   u8 uHistPacketsCount[MAX_RADIO_STATS_INTERFACE_RX_HISTORY_SLICES];

   // type: radio packet type or:
   // 0x00 - none if count is 0. missing packets if count > 0
   // 0xFF - end of a second

   int iCurrentSlice;
   u32 uTimeLastUpdate;
} ALIGN_STRUCT_SPEC_INFO shared_mem_radio_stats_interface_rx_hist;

typedef struct
{
   shared_mem_radio_stats_interface_rx_hist interfaces_history[MAX_RADIO_INTERFACES];
} ALIGN_STRUCT_SPEC_INFO shared_mem_radio_stats_rx_hist;

typedef struct
{
   u32 uVehicleId;
   u32 totalRxBytes;
   u32 totalTxBytes;
   u32 rxBytesPerSec;
   u32 txBytesPerSec;
   u32 totalRxPackets;
   u32 totalTxPackets;
   u32 totalLostPackets;
   u32 rxPacketsPerSec;
   u32 txPacketsPerSec;
   u32 timeLastRxPacket;
   u32 timeLastTxPacket;
   u32 uLastRecvStreamPacketIndex;
   int iHasMissingStreamPacketsFlag;
   u32 tmpRxBytes;
   u32 tmpTxBytes;
   u32 tmpRxPackets;
   u32 tmpTxPackets;
} ALIGN_STRUCT_SPEC_INFO shared_mem_radio_stats_stream;

// Vehicle radio interfaces order is given by physical order. Can't be changed.
// Vehicle radio links order is auto/user defined.
// Vehicle radio interfaces assignment to radio links is auto/user defined.
// Vehicle radio interfaces parameters are checked/re-computed when the vehicle starts.

typedef struct
{
   int iAntennaCount;
   type_runtime_radio_rx_signal_info signalInfoAll;
   type_runtime_radio_rx_signal_info signalInfoVideo;
   type_runtime_radio_rx_signal_info signalInfoData;

} ALIGN_STRUCT_SPEC_INFO shared_mem_radio_stats_radio_interface_rx_signal_all;

typedef struct
{
   int assignedLocalRadioLinkId; // id of the radio link assigned to this radio interface
   int assignedVehicleRadioLinkId; // id of the radio link assigned to this radio interface
   u32 uCurrentFrequencyKhz;
   u8 openedForRead;
   u8 openedForWrite;
   shared_mem_radio_stats_radio_interface_rx_signal_all signalInfo;
   int lastRecvDataRate; // positive: bps, negative: MCS, 0 - never
   int lastRecvDataRateVideo;
   int lastRecvDataRateData;
   int lastSentDataRateVideo; // positive: bps, negative: MCS, 0 - never
   int lastSentDataRateData;
   u32 lastReceivedRadioLinkPacketIndex; // for lost packets calculation

   u32 totalRxBytes;
   u32 totalTxBytes;
   u32 rxBytesPerSec;
   u32 txBytesPerSec;
   u32 totalRxPackets;
   u32 totalRxPacketsBad;
   u32 totalRxPacketsLost;
   u32 totalTxPackets;
   u32 rxPacketsPerSec;
   u32 txPacketsPerSec;
   u32 timeLastRxPacket;
   u32 timeLastTxPacket;
   u32 timeNow;

   u32 tmpRxBytes;
   u32 tmpTxBytes;
   u32 tmpRxPackets;
   u32 tmpTxPackets;

   int rxQuality; // 0...100%
   int rxRelativeQuality; // higher value means better link; it's relative to the other radio interfaces

   u8 hist_rxPacketsCurrentIndex;
   u8 hist_rxPacketsCount[MAX_HISTORY_RADIO_STATS_RECV_SLICES];
   u8 hist_rxPacketsBadCount[MAX_HISTORY_RADIO_STATS_RECV_SLICES];
   u8 hist_rxPacketsLostCountVideo[MAX_HISTORY_RADIO_STATS_RECV_SLICES];
   u8 hist_rxPacketsLostCountData[MAX_HISTORY_RADIO_STATS_RECV_SLICES];
   u8 hist_rxGapMiliseconds[MAX_HISTORY_RADIO_STATS_RECV_SLICES];
   u32 hist_tmp_rxPacketsCount;
   u32 hist_tmp_rxPacketsBadCount;
   u32 hist_tmp_rxPacketsLostCountVideo;
   u32 hist_tmp_rxPacketsLostCountData;

} ALIGN_STRUCT_SPEC_INFO shared_mem_radio_stats_radio_interface;

typedef struct
{
   shared_mem_radio_stats_radio_interface_rx_signal_all signalInfo;
   int lastRecvDataRate; // positive: bps, negative: MCS, 0 - never
   int lastRecvDataRateVideo;
   int lastRecvDataRateData;
   int lastSentDataRateVideo; // positive: bps, negative: MCS, 0 - never
   int lastSentDataRateData;

   u32 totalRxBytes;
   u32 totalTxBytes;
   u32 rxBytesPerSec;
   u32 txBytesPerSec;
   u32 totalRxPackets;
   u32 totalRxPacketsBad;
   u32 totalRxPacketsLost;
   u32 totalTxPackets;
   u32 rxPacketsPerSec;
   u32 txPacketsPerSec;
   u32 timeLastRxPacket;
   u32 timeLastTxPacket;
   u32 timeNow;

   int rxQuality; // 0...100%
   int rxRelativeQuality; // higher value means better link; it's relative to the other radio interfaces

   u8 hist_rxPacketsCurrentIndex;
   u8 hist_rxPacketsCount[MAX_HISTORY_RADIO_STATS_RECV_SLICES];
   u8 hist_rxPacketsLostCountVideo[MAX_HISTORY_RADIO_STATS_RECV_SLICES];
   u8 hist_rxPacketsLostCountData[MAX_HISTORY_RADIO_STATS_RECV_SLICES];
   u8 hist_rxGapMiliseconds[MAX_HISTORY_RADIO_STATS_RECV_SLICES];

} ALIGN_STRUCT_SPEC_INFO shared_mem_radio_stats_radio_interface_compact;


typedef struct
{
   int matchingVehicleRadioLinkId;
   int refreshIntervalMs;
   u32 lastComputeTimePerSec;
   u32 totalRxBytes;
   u32 totalTxBytes;
   u32 rxBytesPerSec;
   u32 txBytesPerSec;
   u32 totalRxPackets;
   u32 totalTxPackets;
   u32 rxPacketsPerSec;
   u32 txPacketsPerSec;
   u32 totalTxPacketsUncompressed;
   u32 txUncompressedPacketsPerSec;
   u32 timeLastRxPacket;
   u32 timeLastTxPacket;

   u32 tmpRxBytes;
   u32 tmpTxBytes;
   u32 tmpRxPackets;
   u32 tmpTxPackets;
   u32 tmpUncompressedTxPackets;

   int lastTxInterfaceIndex;
   int lastSentDataRateVideo; // positive: bps, negative: MCS, 0 - never
   int lastSentDataRateData;

   u32 downlink_tx_time_per_sec;
   u32 tmp_downlink_tx_time_per_sec;

} ALIGN_STRUCT_SPEC_INFO shared_mem_radio_stats_radio_link;


typedef struct
{
   int countLocalRadioLinks;
   int countVehicleRadioLinks;
   int countLocalRadioInterfaces;
   int graphRefreshIntervalMs;
   int refreshIntervalMs;

   u32 lastComputeTime;
   u32 lastComputeTimeGraph;

   u32 all_downlinks_tx_time_per_sec;
   u32 tmp_all_downlinks_tx_time_per_sec;
   u32 timeLastRxPacket;
   u32 timeLastTxPacket;
   u32 uLastTimeReceivedAckFromAVehicle;

   int iMaxRxQuality;
   shared_mem_radio_stats_stream           radio_streams[MAX_CONCURENT_VEHICLES][MAX_RADIO_STREAMS];
   shared_mem_radio_stats_radio_interface  radio_interfaces[MAX_RADIO_INTERFACES];
   shared_mem_radio_stats_radio_link       radio_links[MAX_RADIO_INTERFACES];

} ALIGN_STRUCT_SPEC_INFO shared_mem_radio_stats;

