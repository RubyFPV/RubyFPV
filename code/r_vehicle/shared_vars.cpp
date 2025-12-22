#include "shared_vars.h"

bool g_bQuit = false;
bool g_bRouterReady = false;
Model* g_pCurrentModel = NULL;

shared_mem_process_stats* g_pProcessStats = NULL;

t_packet_queue s_QueueControlPackets;

int s_fIPCRouterToCommands = -1;
int s_fIPCRouterFromCommands = -1;
int s_fIPCRouterToTelemetry = -1;
int s_fIPCRouterFromTelemetry = -1;
int s_fIPCRouterToRC = -1;
int s_fIPCRouterFromRC = -1;

int s_fInputVideoStream = -1;

bool g_bVideoPaused = false;

u16 s_countTXVideoPacketsOutTemp = 0;
u16 s_countTXDataPacketsOutTemp = 0;
u16 s_countTXCompactedPacketsOutTemp = 0;

// Router

type_u32_couters g_CoutersMainLoop;

u32 s_debugVideoBlocksInCount = 0;

t_packet_queue g_QueueRadioPacketsOut;
VideoTxPacketsBuffer* g_pVideoTxBuffers = NULL;
ProcessorTxVideo* g_pProcessorTxVideo = NULL;
ProcessorTxAudio* g_pProcessorTxAudio = NULL;
bool g_bRadioReinitialized = false;

shared_mem_radio_stats g_SM_RadioStats;
shared_mem_radio_stats_rx_hist* g_pSM_HistoryRxStats = NULL;
shared_mem_radio_stats_rx_hist g_SM_HistoryRxStats;

type_uplink_rx_info_stats g_UplinkInfoRxStats[MAX_RADIO_INTERFACES];

bool g_bVehicleArmed = false;
int g_iVehicleSOCTemperatureC = 0;

t_sik_radio_state g_SiKRadiosState;

bool g_bReinitializeRadioInProgress = false;
bool g_bReceivedPairingRequest = false;
bool g_bHasFastUplinkFromController = false;
bool g_bHasSlowUplinkFromController = false;
bool g_bHadEverLinkToController = false;
bool g_bHasSentVehicleSettingsAtLeastOnce = false;
bool g_bOSDPluginsNeedTelemetryStreams = false;

u32  g_uControllerId = 0;

t_packet_header_ruby_telemetry_extended_extra_info_retransmissions g_PHTE_Retransmissions;
t_packet_header_vehicle_tx_history g_PHVehicleTxStats;
shared_mem_dev_video_bitrate_history g_SM_DevVideoBitrateHistory;

//shared_mem_video_frames_stats g_VideoInfoStatsCameraOutput;
//shared_mem_video_frames_stats* g_pSM_VideoInfoStatsRadioOut = NULL;
//shared_mem_video_frames_stats g_VideoInfoStatsRadioOut;
//shared_mem_video_frames_stats* g_pSM_VideoInfoStatsCameraOutput = NULL;

int g_iDebugShowKeyFramesAfterRelaySwitch = 0;

int g_iGetSiKConfigAsyncResult = 0;
int g_iGetSiKConfigAsyncRadioInterfaceIndex = -1;
u8 g_uGetSiKConfigAsyncVehicleLinkIndex = 0;

bool g_bLongTaskStarted = false;
