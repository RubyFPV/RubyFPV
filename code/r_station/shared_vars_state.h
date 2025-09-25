#pragma once
#include "../base/config.h"

#define MAX_RUNTIME_INFO_COMMANDS_RT_TIMES 5

typedef struct
{
   u32 uVehicleId;

   // Pairing info
   bool bIsPairingDone;
   u32 uPairingRequestTime;
   u32 uPairingRequestInterval;
   u32 uPairingRequestId;

   // Radio link roundtrip/ping times

   u32 uTimeLastPingInitiatedToVehicleOnLocalRadioLinks[MAX_RADIO_INTERFACES];
   u32 uTimeLastPingSentToVehicleOnLocalRadioLinks[MAX_RADIO_INTERFACES];
   u8  uLastPingIdSentToVehicleOnLocalRadioLinks[MAX_RADIO_INTERFACES];
   u32 uTimeLastPingReplyReceivedFromVehicleOnLocalRadioLinks[MAX_RADIO_INTERFACES];
   u8  uLastPingIdReceivedFromVehicleOnLocalRadioLinks[MAX_RADIO_INTERFACES];
   u32 uPingRoundtripTimeOnLocalRadioLinks[MAX_RADIO_INTERFACES];
   u32 uMinimumPingTimeMilisec;

   bool bIsVehicleFastUplinkFromControllerLost;
   bool bIsVehicleSlowUplinkFromControllerLost;
   u32 uLastTimeReceivedAckFromVehicle;
   u32 uLastTimeRecvDataFromVehicle;
   int iVehicleClockDeltaMilisec;

   // Commands roundtrip info

   u32 uTimeLastCommandIdSent;
   u32 uLastCommandIdSent;
   u32 uLastCommandIdRetrySent;
   u32 uLastCommandsRoundtripTimesMs[MAX_RUNTIME_INFO_COMMANDS_RT_TIMES];

   u32 uAverageCommandRoundtripMiliseconds;
   u32 uMaxCommandRoundtripMiliseconds;
   u32 uMinCommandRoundtripMiliseconds;

   // Transmission frame info
   u32 uTimeLastDequeuedFrameData;
   u32 uTimeLastFrameStart;
   u16 uCurrentFrameId;
   int iLastVideoDatarateBPS;
   bool bIsReceivingFrameData;
   bool bIsFrameEndDetected;
   bool bIsFrameEnded;
   u32 uTimeStartWindowTxData;
   u32 uTimeEndWindowTxData;
   u32 uTimeLastTxData;

   // Adaptive video info
   bool bIsDoingRetransmissions;
   bool bIsAdaptiveVideoActive;
   u32 uAdaptiveVideoActivationTime;
   bool bDidFirstTimeAdaptiveHandshake;
   u32 uAdaptiveVideoLastCheckTime;
   u32 uAdaptiveVideoRequestId;
   u32 uAdaptiveVideoAckId;
   u32 uLastTimeSentAdaptiveVideoRequest;
   u32 uLastTimeRecvAdaptiveVideoAck;

   u32 uCurrentAdaptiveVideoTargetVideoBitrateBPS;
   u16 uCurrentAdaptiveVideoECScheme; // high: data, low: EC
   int iCurrentDataratesForLinks[MAX_RADIO_INTERFACES];
   u8  uCurrentDRBoost;
   int iCurrentAdaptiveVideoKeyFrameMsTarget;
   u32 uPendingVideoBitrateToSet;
   u16 uPendingECSchemeToSet;
   u8  uPendingDRBoostToSet;
   int iPendingKeyFrameMsToSet;
   int iAdaptiveLevelNow; // from 0 to max lowest adaptive level
} ALIGN_STRUCT_SPEC_INFO type_global_state_vehicle_runtime_info;


typedef struct
{
   type_global_state_vehicle_runtime_info vehiclesRuntimeInfo[MAX_CONCURENT_VEHICLES];
} ALIGN_STRUCT_SPEC_INFO type_global_state_station;


extern type_global_state_station g_State;

void resetVehicleRuntimeInfo(int iIndex);
void resetPairingStateForVehicleRuntimeInfo(int iIndex);
void removeVehicleRuntimeInfo(int iIndex);
bool isPairingDoneWithVehicle(u32 uVehicleId);
int  getVehicleRuntimeIndex(u32 uVehicleId);
type_global_state_vehicle_runtime_info* getVehicleRuntimeInfo(u32 uVehicleId);
void logCurrentVehiclesRuntimeInfo();

void addCommandRTTimeToRuntimeInfo(type_global_state_vehicle_runtime_info* pRuntimeInfo, u32 uRoundtripTimeMs);
void adjustLinkClockDeltasForVehicleRuntimeIndex(int iRuntimeInfoIndex, u32 uRoundtripTimeMs, u32 uLocalTimeVehicleMs);
