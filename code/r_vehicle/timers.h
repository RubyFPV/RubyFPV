#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../base/shared_mem.h"

// Globals

extern u32 g_TimeLastPeriodicCheck;

// Vehicle

extern u32 g_TimeLastCheckRadioSilenceFailsafe;

// Telemetry

extern u32 g_TimeLastCheckEverySecond;
extern u32 g_TimeLastRCSentToFC;
extern u32 g_TimeLastMAVLinkHeartbeatSent;

// RC TX

extern u32 g_TimeLastFrameReceived;
extern u32 g_TimeLastQualityMeasurement;

// Commands


// Router

extern u32 g_TimeFirstReceivedRadioPacketFromController;
extern u32 g_TimeLastReceivedFastRadioPacketFromController;
extern u32 g_TimeLastReceivedSlowRadioPacketFromController;

extern u32 g_LastTimeLostFastLinkFromController;
extern u32 g_LastTimeLostSlowLinkFromController;

extern u32 g_TimeLastDebugFPSComputeTime;
extern u32 g_TimeLastLiveLogCheck;
extern u32 g_TimeLastSetRadioLinkFlagsStartOperation;
extern u32 g_TimeRadioReinitialized;
extern u32 g_TimeLastPacketsOutPerSecCalculation;
extern u32 g_TimeLastVideoCaptureProgramStartCheck;
extern u32 g_TimeLastVideoCaptureProgramRunningCheck;

extern u32 g_TimeLastOverwriteBitrateDownOnTxOverload;
extern u32 g_TimeLastOverwriteBitrateUpOnTxOverload;
extern u32 g_TimeLastVideoProfileChanged;

extern u32 g_TimeLastSetRadioFlagsCommandReceived;

extern u32 g_TimeLastHistoryTxComputation;
extern u32 g_TimeLastTxPacket;
extern u32 g_TimeLastVideoPacketIn;
extern u32 g_TimeLastNotificationRelayParamsChanged;

extern u32 g_uTimeLastCommandSoftwareUpload;

extern u32 g_uTimeLastVideoTxOverload;

extern u32 g_uTimeToSaveVeyeCameraParams;

extern u32 g_uTimeRequestedReboot;
