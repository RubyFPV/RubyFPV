#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/commands.h"
#include "../base/msp.h"

#define MAX_SEGMENTS_FILE_UPLOAD 10000 // About 10 Mbytes of data maximum

typedef struct
{
   u32 uFileId;
   char szFileName[128];
   command_packet_upload_file_segment currentUploadSegment;

   u32 uTotalSegments;
   u8* pSegments[MAX_SEGMENTS_FILE_UPLOAD];
   u32 uSegmentsSize[MAX_SEGMENTS_FILE_UPLOAD];
   bool bSegmentsUploaded[MAX_SEGMENTS_FILE_UPLOAD];

   u32 uTimeLastUploadSegment;
   u32 uLastSegmentIndexUploaded;
} ALIGN_STRUCT_SPEC_INFO t_structure_file_upload;

extern t_structure_file_upload g_CurrentUploadingFile;
extern bool g_bHasFileUploadInProgress;

typedef struct 
{
   u32 uVehicleId;
   Model* pModel;
   
   // Ruby telemetry

   bool bGotRubyTelemetryInfo;
   bool bGotRubyTelemetryInfoShort;
   bool bGotRubyTelemetryExtraInfo;
   bool bGotRubyTelemetryExtraInfoRetransmissions;
   bool bGotStatsVehicleRxCards;

   int  iFrequencyRubyTelemetryFull;
   int  iFrequencyRubyTelemetryShort;
   u32  uTimeLastRecvRubyTelemetry;
   u32  uTimeLastRecvRubyTelemetryExtended;
   u32  uTimeLastRecvRubyTelemetryShort;
   u32  uTimeLastRecvAnyRubyTelemetry;
   u32  uTimeLastRecvVehicleRxStats;
   bool bRubyTelemetryLost;

   type_u32_couters vehicleDebugRouterCounters;

   t_packet_header_ruby_telemetry_extended_v6 headerRubyTelemetryExtended;
   t_packet_header_ruby_telemetry_extended_extra_info headerRubyTelemetryExtraInfo;
   t_packet_header_ruby_telemetry_extended_extra_info_retransmissions headerRubyTelemetryExtraInfoRetransmissions;
   t_packet_header_ruby_telemetry_short headerRubyTelemetryShort;
   type_msp_parse_state mspState;
   t_packet_header_relay_radio_info headerRelayRadioLinksInfo;
   shared_mem_radio_stats_radio_interface SMVehicleRxStats[MAX_RADIO_INTERFACES];

   // FC telemetry

   bool bGotFCTelemetry;
   bool bGotFCTelemetryShort;
   bool bGotFCTelemetryFull;
   bool bGotFCTelemetryExtra;
   bool bFCTelemetrySourcePresent;
   int  iFrequencyFCTelemetryFull;
   int  iFrequencyFCTelemetryShort;
   u32  uTimeLastRecvFCTelemetry;
   u32  uTimeLastRecvFCTelemetryFull;
   u32  uTimeLastRecvFCTelemetryShort;
   u8   uLastFCFlightMode;
   u8   uLastFCFlags;

   t_packet_header_fc_telemetry headerFCTelemetry;
   t_packet_header_fc_extra     headerFCTelemetryExtra;
   t_packet_header_fc_rc_channels headerFCTelemetryRCChannels;

   char szLastMessageFromFC[FC_MESSAGE_MAX_LENGTH];
   u32 uTimeLastMessageFromFC;
   
   // Other info

   bool bLinkLost;
   bool bRCFailsafeState;
   bool bWarningBatteryVoltage;
   int iComputedBatteryCellCount;
   bool bNotificationPairingRequestSent;
   bool bPairedConfirmed;
   bool bIsAdaptiveVideoPaused;

   bool bIsArmed;
   bool bHomeSet;
   double fHomeLat, fHomeLon;
   double fHomeLastLat, fHomeLastLon;

   // Temporary info

   u8 pSegmentsModelSettingsDownload[20][256];
   u8 uSegmentsModelSettingsSize[20];
   u32 uSegmentsModelSettingsIds[20];
   u8 uSegmentsModelSettingsCount;
   bool bWaitingForModelSettings;
   u32 uTimeLastReceivedModelSettings;
   u8  uTmpLastThrottledFlags;
   
   int tmp_iCountRubyTelemetryPacketsFull;
   int tmp_iCountRubyTelemetryPacketsShort;
   int tmp_iCountFCTelemetryPacketsFull;
   int tmp_iCountFCTelemetryPacketsShort;
   u32 tmp_uTimeLastTelemetryFrequencyComputeTime;

} ALIGN_STRUCT_SPEC_INFO t_structure_vehicle_info;

extern int s_StartSequence;
extern Model* g_pCurrentModel;
extern u32 g_uActiveControllerModelVID;

// First vehicle is always the main vehicle, the next ones are relayed vehicles
extern t_structure_vehicle_info g_SearchVehicleRuntimeInfo;
extern t_structure_vehicle_info g_UnexpectedVehicleRuntimeInfo;
extern t_structure_vehicle_info g_VehiclesRuntimeInfo[MAX_CONCURENT_VEHICLES];
extern int g_iCurrentActiveVehicleRuntimeInfoIndex;

extern bool g_bSearching;
extern bool g_bSearchFoundVehicle;
extern int g_iSearchFrequency;
extern int g_iSearchFirmwareType;
extern int g_iSearchSiKAirDataRate;
extern int g_iSearchSiKECC;
extern int g_iSearchSiKLBT;
extern int g_iSearchSiKMCSTR;

extern bool g_bIsReinit;
extern bool g_bIsRouterReady;

extern bool g_bFirstModelPairingDone;
extern bool g_bIsFirstConnectionToCurrentVehicle;
extern bool g_bIsVideoLost;
extern bool g_bAdaptiveVideoIsPaused;

extern bool g_bDidAnUpdate;
extern bool g_bUpdateInProgress;
extern bool g_bLinkWizardAfterUpdate;
extern int g_nFailedOTAUpdates;
extern int g_nSucceededOTAUpdates;

extern bool g_bSyncModelSettingsOnLinkRecover;

extern int g_nTotalControllerCPUSpikes;

extern bool g_bGotStatsVideoBitrate;
extern bool g_bGotStatsVehicleTx;

extern bool g_bHasVideoDecodeStatsSnapshot;

extern u32 g_uLastControllerAlarmIOFlags;

extern bool g_bChangedOSDStatsFontSize;

extern bool g_bReconfiguringRadioLinks;
extern bool g_bConfiguringRadioLink;
extern bool g_bSwitchingFavoriteVehicle;

void reset_vehicle_runtime_info(t_structure_vehicle_info* pInfo);
void reset_vehicle_telemetry_runtime_info(t_structure_vehicle_info* pInfo);
void shared_vars_state_reset_all_vehicles_runtime_info();
void reset_model_settings_download_buffers(u32 uVehicleId);
t_structure_vehicle_info* get_vehicle_runtime_info_for_vehicle_id(u32 uVehicleId);
t_packet_header_ruby_telemetry_extended_v6* get_received_relayed_vehicle_telemetry_info();
void log_current_runtime_vehicles_info();
bool vehicle_runtime_has_received_fc_telemetry(u32 uVehicleId);
u32  vehicle_runtime_get_time_last_received_ruby_telemetry(u32 uVehicleId);
void vehicle_runtime_reset_has_received_fc_telemetry_info(u32 uVehicleId);
