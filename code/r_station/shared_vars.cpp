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

#include "shared_vars.h"
#include "timers.h"

bool g_bQuit = false;
bool g_bDebugState = false;
Model* g_pCurrentModel = NULL;

ControllerSettings* g_pControllerSettings = NULL; 
ControllerInterfacesSettings* g_pControllerInterfaces = NULL; 
u32 g_uControllerId = MAX_U32;

bool g_bSearching = false;
u32  g_uSearchFrequency = 0;
u32  g_uAcceptedFirmwareType = MODEL_FIRMWARE_TYPE_RUBY;
bool g_bUpdateInProgress = false;

// Router

ProcessorRxVideo* g_pVideoProcessorRxList[MAX_VIDEO_PROCESSORS];

controller_runtime_info g_SMControllerRTInfo;
controller_runtime_info* g_pSMControllerRTInfo = NULL;
controller_debug_video_runtime_info g_SMControllerDebugVideoRTInfo;
controller_debug_video_runtime_info* g_pSMControllerDebugVideoRTInfo = NULL;

shared_mem_radio_stats_rx_hist* g_pSM_HistoryRxStats = NULL;
shared_mem_radio_stats_rx_hist g_SM_HistoryRxStats;

shared_mem_video_frames_stats g_SM_VideoFramesStatsOutput;
shared_mem_video_frames_stats* g_pSM_VideoFramesStatsOutput = NULL;
//shared_mem_video_frames_stats g_SM_VideoInfoStatsRadioIn;
//shared_mem_video_frames_stats* g_pSM_VideoInfoStatsRadioIn = NULL;

shared_mem_router_vehicles_runtime_info g_SM_RouterVehiclesRuntimeInfo;
shared_mem_router_vehicles_runtime_info* g_pSM_RouterVehiclesRuntimeInfo = NULL;

shared_mem_video_stream_stats_rx_processors g_SM_VideoDecodeStats;
shared_mem_video_stream_stats_rx_processors* g_pSM_VideoDecodeStats = NULL;

shared_mem_radio_rx_queue_info* g_pSM_RadioRxQueueInfo = NULL;
shared_mem_radio_rx_queue_info g_SM_RadioRxQueueInfo;

shared_mem_radio_stats g_SM_RadioStats;
shared_mem_radio_stats* g_pSM_RadioStats = NULL;

shared_mem_process_stats* g_pProcessStats = NULL;
shared_mem_process_stats* g_pProcessStatsCentral = NULL;

shared_mem_ctrl_ping_stats* g_pSMDbgPingStats = NULL;
shared_mem_ctrl_ping_stats g_SMDbgPingStats;

int g_fIPCFromCentral = -1;
int g_fIPCToCentral = -1;
int g_fIPCToTelemetry = -1;
int g_fIPCFromTelemetry = -1;
int g_fIPCToRC = -1;
int g_fIPCFromRC = -1;

t_sik_radio_state g_SiKRadiosState;

bool g_bFirstModelPairingDone = false;
bool g_bOSDPluginsNeedTelemetryStreams = false;

u32 g_uLastInterceptedCommandCounterToSetRadioFlags = MAX_U32;
u32 g_uLastRadioLinkIndexForSentSetRadioLinkFlagsCommand = MAX_U32;
int g_iLastRadioLinkDataRateSentForSetRadioLinkFlagsCommand = 0;

int g_iDebugShowKeyFramesAfterRelaySwitch = 0;
int g_iGetSiKConfigAsyncResult = 0;
int g_iGetSiKConfigAsyncRadioInterfaceIndex = -1;
u8 g_uGetSiKConfigAsyncVehicleLinkIndex = 0;
