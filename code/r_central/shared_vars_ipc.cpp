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

#include "shared_vars_ipc.h"

// These are received using router messages, not shared memory:

t_packet_header_vehicle_tx_history g_PHVehicleTxHistory;


// There are shared memory objects
shared_mem_process_stats* g_pProcessStatsCentral = NULL;
shared_mem_process_stats* g_pProcessStatsRouter = NULL;
shared_mem_process_stats* g_pProcessStatsTelemetry = NULL;
shared_mem_process_stats* g_pProcessStatsRC = NULL;
shared_mem_process_stats g_ProcessStatsRouter;
shared_mem_process_stats g_ProcessStatsTelemetry;
shared_mem_process_stats g_ProcessStatsRC;
shared_mem_ctrl_ping_stats* g_pSMDbgPingStats = NULL;
shared_mem_ctrl_ping_stats g_SMDbgPingStats;

controller_runtime_info g_SMControllerRTInfo;
controller_runtime_info* g_pSMControllerRTInfo = NULL;
controller_debug_video_runtime_info g_SMControllerDebugVideoRTInfo;
controller_debug_video_runtime_info* g_pSMControllerDebugVideoRTInfo = NULL;

t_packet_header_rc_info_downstream* g_pSM_DownstreamInfoRC = NULL; // RC Info received on ground 
t_packet_header_rc_info_downstream g_SM_DownstreamInfoRC; // RC Info received on ground 

shared_mem_router_vehicles_runtime_info* g_pSM_RouterVehiclesRuntimeInfo = NULL;
shared_mem_router_vehicles_runtime_info g_SM_RouterVehiclesRuntimeInfo;

shared_mem_radio_stats* g_pSM_RadioStats = NULL;
shared_mem_radio_stats g_SM_RadioStats;

shared_mem_radio_stats_rx_hist* g_pSM_HistoryRxStats = NULL;
shared_mem_radio_stats_rx_hist g_SM_HistoryRxStats;
shared_mem_radio_stats_rx_hist g_SM_HistoryRxStatsVehicle;

shared_mem_video_frames_stats* g_pSM_VideoFramesStatsOutput = NULL;
shared_mem_video_frames_stats g_SM_VideoFramesStatsOutput;
//shared_mem_video_frames_stats* g_pSM_VideoInfoStatsRadioIn = NULL;
//shared_mem_video_frames_stats g_SM_VideoInfoStatsRadioIn;
//shared_mem_video_frames_stats g_VideoInfoStatsFromVehicleCameraOut;
//shared_mem_video_frames_stats g_VideoInfoStatsFromVehicleRadioOut;

shared_mem_video_stream_stats_rx_processors* g_pSM_VideoDecodeStats = NULL;
shared_mem_video_stream_stats_rx_processors g_SM_VideoDecodeStats;

shared_mem_radio_rx_queue_info* g_pSM_RadioRxQueueInfo = NULL;
shared_mem_radio_rx_queue_info g_SM_RadioRxQueueInfo;

shared_mem_dev_video_bitrate_history g_SM_DevVideoBitrateHistory;


t_shared_mem_i2c_controller_rc_in* g_pSM_RCIn = NULL; // SBUS/IBUS RC In
t_shared_mem_i2c_controller_rc_in g_SM_RCIn;

t_shared_mem_i2c_current* g_pSMVoltage = NULL;
t_shared_mem_i2c_current g_SMVoltage;

t_shared_mem_i2c_rotary_encoder_buttons_events* g_pSMRotaryEncoderButtonsEvents = NULL;
