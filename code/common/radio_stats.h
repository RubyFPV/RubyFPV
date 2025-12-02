#pragma once
#include "../base/base.h"
#include "../base/shared_mem.h"

#ifdef __cplusplus
extern "C" {
#endif  

void shared_mem_radio_stats_rx_hist_reset(shared_mem_radio_stats_rx_hist* pStats);
void shared_mem_radio_stats_rx_hist_update(shared_mem_radio_stats_rx_hist* pStats, int iInterfaceIndex, u8* pPacket, u32 uTimeNow);

void radio_stats_reset(shared_mem_radio_stats* pSMRS, int graphRefreshInterval);
void radio_stats_reset_received_info(shared_mem_radio_stats* pSMRS);
void radio_stats_remove_received_info_for_vid(shared_mem_radio_stats* pSMRS, u32 uVehicleId);
void radio_stats_reset_rx_signal_info(shared_mem_radio_stats* pSMRS);
void radio_stats_reset_interfaces_rx_info(shared_mem_radio_stats* pSMRS, const char* szReason);
void radio_stats_set_graph_refresh_interval(shared_mem_radio_stats* pSMRS, int graphRefreshInterval);

void radio_stats_enable_history_monitor(int iEnable);

void radio_stats_log_info(shared_mem_radio_stats* pSMRS, u32 uTimeNow);
void radio_stats_log_tx_info(shared_mem_radio_stats* pSMRS, u32 uVehicleId, u32 uControllerId, u32 uTimeNow);
int  radio_stats_periodic_update(shared_mem_radio_stats* pSMRS, u32 timeNow);

void radio_stats_set_tx_card_for_radio_link(shared_mem_radio_stats* pSMRS, int iLocalRadioLink, int iTxCard);
void radio_stats_set_card_current_frequency(shared_mem_radio_stats* pSMRS, int iRadioInterface, u32 freqKhz);
void radio_stats_set_bad_data_on_current_rx_interval(shared_mem_radio_stats* pSMRS, int iRadioInterface);

int  radio_stats_update_on_new_radio_packet_received(shared_mem_radio_stats* pSMRS, u32 timeNow, int iInterfaceIndex, u8* pPacketBuffer, int iPacketLength, int iIsShortPacket, int iDataIsOk);
int  radio_stats_update_on_unique_packet_received(shared_mem_radio_stats* pSMRS, u32 timeNow, int iInterfaceIndex, u8* pPacketBuffer, int iPacketLength);
void radio_stats_update_on_packet_sent_on_radio_interface(shared_mem_radio_stats* pSMRS, u32 timeNow, int interfaceIndex, int iPacketLength);
void radio_stats_update_on_packet_sent_on_radio_link(shared_mem_radio_stats* pSMRS, u32 timeNow, int iLocalLinkIndex, int iStreamIndex, int iPacketLength);
void radio_stats_update_on_packet_sent_for_radio_stream(shared_mem_radio_stats* pSMRS, u32 timeNow, u32 uVehicleId, int iStreamIndex, u8 uPacketType, int iPacketLength);

void radio_stats_set_tx_radio_datarate_for_packet(shared_mem_radio_stats* pSMRS, int iInterfaceIndex, int iLocalRadioLinkIndex, int iDataRate, int iIsVideoPacket);

int radio_stats_get_reset_stream_lost_packets_flags(shared_mem_radio_stats* pSMRS, u32 uVehicleId, u32 uStreamIndex);

u32 radio_stats_get_time_last_received_packet_on_stream(shared_mem_radio_stats* pSMRS, u32 uVehicleId, u32 uStreamIndex);
#ifdef __cplusplus
}  
#endif
