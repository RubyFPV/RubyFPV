#include "timers.h"

// Globals
u32 g_TimeLastPeriodicCheck = 0;

// Vehicle

u32 g_TimeLastCheckRadioSilenceFailsafe = 0;

// Telemetry

u32 g_TimeLastCheckEverySecond = 0;
u32 g_TimeLastRCSentToFC = 0;
u32 g_TimeLastMAVLinkHeartbeatSent = 0;

// RC TX

u32 g_TimeLastFrameReceived = 0;
u32 g_TimeLastQualityMeasurement = 0;

// Commands


// Router

u32 g_TimeFirstReceivedRadioPacketFromController = 0;
u32 g_TimeLastReceivedFastRadioPacketFromController = 0;
u32 g_TimeLastReceivedSlowRadioPacketFromController = 0;

u32 g_LastTimeLostFastLinkFromController = 0;
u32 g_LastTimeLostSlowLinkFromController = 0;

u32 g_TimeLastDebugFPSComputeTime = 0;
u32 g_TimeLastLiveLogCheck = 0;
u32 g_TimeLastSetRadioLinkFlagsStartOperation = 0;
u32 g_TimeRadioReinitialized = 0;
u32 g_TimeLastPacketsOutPerSecCalculation = 0;

u32 g_TimeLastVideoCaptureProgramStartCheck = 0;
u32 g_TimeLastVideoCaptureProgramRunningCheck = 0;

u32 g_TimeLastOverwriteBitrateDownOnTxOverload = 0;
u32 g_TimeLastOverwriteBitrateUpOnTxOverload = 0;
u32 g_TimeLastVideoProfileChanged = 0;

u32 g_TimeLastSetRadioFlagsCommandReceived = 0;
u32 g_TimeLastHistoryTxComputation = 0;
u32 g_TimeLastTxPacket = 0;
u32 g_TimeLastVideoPacketIn = 0;

u32 g_uTimeLastCommandSoftwareUpload = 0;

u32 g_uTimeLastVideoTxOverload = 0;

u32 g_uTimeToSaveVeyeCameraParams = 0;

u32 g_uTimeRequestedReboot = 0;
