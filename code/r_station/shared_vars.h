#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../base/controller_rt_info.h"
#include "../base/vehicle_rt_info.h"
#include "../base/models.h"
#include "../base/shared_mem.h"
#include "../base/shared_mem_controller_only.h"
#include "../base/utils.h"

#include "shared_vars_state.h"

#include "processor_rx_video.h"

extern bool g_bQuit;
extern bool g_bDebugState;

extern Model* g_pCurrentModel;
extern ControllerSettings* g_pControllerSettings; 
extern ControllerInterfacesSettings* g_pControllerInterfaces;
extern u32 g_uControllerId;

extern bool g_bSearching;
extern u32  g_uSearchFrequency;
extern u32  g_uAcceptedFirmwareType;
extern bool g_bUpdateInProgress;

// Router

extern ProcessorRxVideo* g_pVideoProcessorRxList[MAX_VIDEO_PROCESSORS];

extern controller_runtime_info g_SMControllerRTInfo;
extern controller_runtime_info* g_pSMControllerRTInfo;
extern controller_debug_video_runtime_info g_SMControllerDebugVideoRTInfo;
extern controller_debug_video_runtime_info* g_pSMControllerDebugVideoRTInfo;

extern shared_mem_radio_stats_rx_hist* g_pSM_HistoryRxStats;
extern shared_mem_radio_stats_rx_hist g_SM_HistoryRxStats;

extern shared_mem_video_frames_stats g_SM_VideoFramesStatsOutput;
extern shared_mem_video_frames_stats* g_pSM_VideoFramesStatsOutput;
//extern shared_mem_video_frames_stats g_SM_VideoInfoStatsRadioIn;
//extern shared_mem_video_frames_stats* g_pSM_VideoInfoStatsRadioIn;

extern shared_mem_router_vehicles_runtime_info g_SM_RouterVehiclesRuntimeInfo;
extern shared_mem_router_vehicles_runtime_info* g_pSM_RouterVehiclesRuntimeInfo;

extern shared_mem_video_stream_stats_rx_processors g_SM_VideoDecodeStats;
extern shared_mem_video_stream_stats_rx_processors* g_pSM_VideoDecodeStats;

extern shared_mem_radio_rx_queue_info* g_pSM_RadioRxQueueInfo;
extern shared_mem_radio_rx_queue_info g_SM_RadioRxQueueInfo;

extern shared_mem_radio_stats g_SM_RadioStats;
extern shared_mem_radio_stats* g_pSM_RadioStats;

extern shared_mem_process_stats* g_pProcessStats;
extern shared_mem_process_stats* g_pProcessStatsCentral;

extern shared_mem_ctrl_ping_stats* g_pSMDbgPingStats;
extern shared_mem_ctrl_ping_stats g_SMDbgPingStats;

extern int g_fIPCFromCentral;
extern int g_fIPCToCentral;
extern int g_fIPCToTelemetry;
extern int g_fIPCFromTelemetry;
extern int g_fIPCToRC;
extern int g_fIPCFromRC;

extern t_sik_radio_state g_SiKRadiosState;

extern bool g_bFirstModelPairingDone;
extern bool g_bOSDPluginsNeedTelemetryStreams;

// Used for Atheros cards data rates set;
extern u32 g_uLastInterceptedCommandCounterToSetRadioFlags;
extern u32 g_uLastRadioLinkIndexForSentSetRadioLinkFlagsCommand;
extern int g_iLastRadioLinkDataRateSentForSetRadioLinkFlagsCommand;

extern int g_iDebugShowKeyFramesAfterRelaySwitch;

extern int g_iGetSiKConfigAsyncResult;
extern int g_iGetSiKConfigAsyncRadioInterfaceIndex;
extern u8 g_uGetSiKConfigAsyncVehicleLinkIndex;
