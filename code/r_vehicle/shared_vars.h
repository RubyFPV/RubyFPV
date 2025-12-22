#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/shared_mem.h"
#include "../base/vehicle_rt_info.h"
#include "../base/utils.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"
#include "../radio/radiopacketsqueue.h"
#include "processor_tx_audio.h"
#include "processor_tx_video.h"
#include "video_tx_buffers.h"

#define ROUTER_STATE_RUNNING 1
#define ROUTER_STATE_NEEDS_RESTART_VIDEO_CAPTURE (1<<1)

typedef struct
{
   u32 timeLastLogWrongRxPacket;
   int lastReceivedDBM;
   int lastReceivedDBMNoise;
   int lastReceivedSNR;
   int lastReceivedDataRate;
   u32 uTimeLastCapture;
}
type_uplink_rx_info_stats;

extern bool g_bQuit;
extern bool g_bRouterReady;

extern Model* g_pCurrentModel;
extern shared_mem_process_stats* g_pProcessStats;

extern t_packet_queue s_QueueControlPackets;
 
extern int s_fIPCRouterToCommands;
extern int s_fIPCRouterFromCommands;
extern int s_fIPCRouterToTelemetry;
extern int s_fIPCRouterFromTelemetry;
extern int s_fIPCRouterToRC;
extern int s_fIPCRouterFromRC;

extern int s_fInputVideoStream;

extern bool g_bVideoPaused;

extern u16 s_countTXVideoPacketsOutTemp;
extern u16 s_countTXDataPacketsOutTemp;
extern u16 s_countTXCompactedPacketsOutTemp;

// Router
extern type_u32_couters g_CoutersMainLoop;

extern u32 s_debugVideoBlocksInCount;

extern t_packet_queue g_QueueRadioPacketsOut;
extern VideoTxPacketsBuffer* g_pVideoTxBuffers;
extern ProcessorTxVideo* g_pProcessorTxVideo;
extern ProcessorTxAudio* g_pProcessorTxAudio;

extern bool g_bRadioReinitialized;
extern shared_mem_radio_stats g_SM_RadioStats;
extern shared_mem_radio_stats_rx_hist* g_pSM_HistoryRxStats;
extern shared_mem_radio_stats_rx_hist g_SM_HistoryRxStats;

extern bool g_bVehicleArmed;
extern int g_iVehicleSOCTemperatureC;

extern type_uplink_rx_info_stats g_UplinkInfoRxStats[MAX_RADIO_INTERFACES];

extern t_sik_radio_state g_SiKRadiosState;

extern bool g_bReinitializeRadioInProgress;
extern bool g_bReceivedPairingRequest;
extern bool g_bHasFastUplinkFromController;
extern bool g_bHasSlowUplinkFromController;
extern bool g_bHadEverLinkToController;
extern bool g_bHasSentVehicleSettingsAtLeastOnce;
extern bool g_bOSDPluginsNeedTelemetryStreams;

extern u32 g_uControllerId;

extern t_packet_header_ruby_telemetry_extended_extra_info_retransmissions g_PHTE_Retransmissions;
extern t_packet_header_vehicle_tx_history g_PHVehicleTxStats;
extern shared_mem_dev_video_bitrate_history g_SM_DevVideoBitrateHistory;

//extern shared_mem_video_frames_stats g_VideoInfoStatsCameraOutput;
//extern shared_mem_video_frames_stats* g_pSM_VideoInfoStatsCameraOutput;
//extern shared_mem_video_frames_stats g_VideoInfoStatsRadioOut;
//extern shared_mem_video_frames_stats* g_pSM_VideoInfoStatsRadioOut;

extern int g_iDebugShowKeyFramesAfterRelaySwitch;

extern int g_iGetSiKConfigAsyncResult;
extern int g_iGetSiKConfigAsyncRadioInterfaceIndex;
extern u8 g_uGetSiKConfigAsyncVehicleLinkIndex;

extern bool g_bLongTaskStarted;

