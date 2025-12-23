#pragma once
#include "../base/base.h"
#include "../base/shared_mem.h"
#include "../base/shared_mem_controller_only.h"
#include "../base/controller_rt_info.h"
#include "../base/vehicle_rt_info.h"
#include "../base/shared_mem_i2c.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiopackets_rc.h"

// These are received using router messages, not shared memory:

extern t_packet_header_vehicle_tx_history g_PHVehicleTxHistory;

// There are shared memory objects
extern shared_mem_process_stats* g_pProcessStatsCentral;
extern shared_mem_process_stats* g_pProcessStatsRouter;
extern shared_mem_process_stats* g_pProcessStatsTelemetry;
extern shared_mem_process_stats* g_pProcessStatsRC;
extern shared_mem_process_stats g_ProcessStatsRouter;
extern shared_mem_process_stats g_ProcessStatsTelemetry;
extern shared_mem_process_stats g_ProcessStatsRC;
extern shared_mem_ctrl_ping_stats* g_pSMDbgPingStats;
extern shared_mem_ctrl_ping_stats g_SMDbgPingStats;

extern controller_runtime_info g_SMControllerRTInfo;
extern controller_runtime_info* g_pSMControllerRTInfo;
extern controller_debug_video_runtime_info g_SMControllerDebugVideoRTInfo;
extern controller_debug_video_runtime_info* g_pSMControllerDebugVideoRTInfo;

extern t_packet_header_rc_info_downstream* g_pSM_DownstreamInfoRC; // RC Info received on ground from vehicle
extern t_packet_header_rc_info_downstream g_SM_DownstreamInfoRC;

extern shared_mem_router_vehicles_runtime_info* g_pSM_RouterVehiclesRuntimeInfo;
extern shared_mem_router_vehicles_runtime_info g_SM_RouterVehiclesRuntimeInfo;

extern shared_mem_radio_stats* g_pSM_RadioStats;
extern shared_mem_radio_stats g_SM_RadioStats;

extern shared_mem_radio_stats_rx_hist* g_pSM_HistoryRxStats;
extern shared_mem_radio_stats_rx_hist g_SM_HistoryRxStats;
extern shared_mem_radio_stats_rx_hist g_SM_HistoryRxStatsVehicle;

extern shared_mem_video_frames_stats* g_pSM_VideoFramesStatsOutput;
extern shared_mem_video_frames_stats g_SM_VideoFramesStatsOutput;
//extern shared_mem_video_frames_stats* g_pSM_VideoInfoStatsRadioIn;
//extern shared_mem_video_frames_stats g_SM_VideoInfoStatsRadioIn;
//extern shared_mem_video_frames_stats g_VideoInfoStatsFromVehicleCameraOut;
//extern shared_mem_video_frames_stats g_VideoInfoStatsFromVehicleRadioOut;

extern shared_mem_video_stream_stats_rx_processors* g_pSM_VideoDecodeStats;
extern shared_mem_video_stream_stats_rx_processors g_SM_VideoDecodeStats;

extern shared_mem_radio_rx_queue_info* g_pSM_RadioRxQueueInfo;
extern shared_mem_radio_rx_queue_info g_SM_RadioRxQueueInfo;

extern shared_mem_dev_video_bitrate_history g_SM_DevVideoBitrateHistory;

extern t_shared_mem_i2c_controller_rc_in* g_pSM_RCIn; // SBUS/IBUS RC In
extern t_shared_mem_i2c_controller_rc_in g_SM_RCIn;

extern t_shared_mem_i2c_current* g_pSMVoltage;
extern t_shared_mem_i2c_current g_SMVoltage;

extern t_shared_mem_i2c_rotary_encoder_buttons_events* g_pSMRotaryEncoderButtonsEvents;
