#pragma once

#include "../base/hardware.h"
#include "../base/ctrl_preferences.h"
#define CONTROLLER_SETTINGS_STAMP_ID "v10.3"

#ifdef __cplusplus
extern "C" {
#endif 

typedef struct
{
   int iUseBrokenVideoCRC;
   int iFixedTxPower;
   int iHDMIBoost;

   int iOverVoltage; // 0 - disabled
   int iFreqARM; // 0 - disabled
   int iFreqGPU; // 0 - disabled

   int iCoresAdjustment;
   int iPrioritiesAdjustment;

   int iThreadPriorityRouter; // 0,1 - disabled, 2...100: rt, 101-139: nice, lower number - higher priority
   int iThreadPriorityRadioRx;
   int iThreadPriorityRadioTx;
   int iThreadPriorityCentral;
   int iThreadPriorityVideo;
   int iThreadPriorityVideoRecording;
   int iThreadPriorityRC;
   int iThreadPriorityOthers;

   int ioNiceRouter; // 0 or negative - disabled
   int ioNiceRxVideo; // 0 or negative - disabled

   int iVideoForwardUSBType; // 0 - none, 1 - raw (h264)
   int iVideoForwardUSBPort;
   int iVideoForwardUSBPacketSize;
   int nVideoForwardETHType; // 0 - none, 1 - raw (h264), 2 - rtp (gstreamer)
   int nVideoForwardETHPort;
   int nVideoForwardETHPacketSize;
   int iTelemetryForwardUSBType; // 0 - none, 1 - enabled
   int iTelemetryForwardUSBPort;
   int iTelemetryForwardUSBPacketSize;
   int iDisableHDMIOverscan;
   int iDeveloperMode;
   int iRenderFPS;
   int iShowVoltage;
   int nRetryRetransmissionAfterTimeoutMS;
   int nRequestRetransmissionsOnVideoSilenceMs;
   int nUseFixedIP;
   u32 uFixedIP;
   int nAutomaticTxCard;
   int nRotaryEncoderFunction;
   int nRotaryEncoderSpeed;
   int nRotaryEncoderFunction2;
   int nRotaryEncoderSpeed2;
   int nPingClockSyncFrequency;
   int nGraphVideoRefreshInterval;
   int iDisableRetransmissionsAfterControllerLinkLostMiliseconds; // 0 to disable functionality
   int iVideoDecodeStatsSnapshotClosesOnTimeout;
   int iFreezeOSD;
   int iDummyCS1;
   int iShowControllerAdaptiveInfoStats;
   int iShowVideoStreamInfoCompactType;

   int iSearchSiKAirRate;
   int iSearchSiKECC;
   int iSearchSiKLBT;
   int iSearchSiKMCSTR;

   int iAudioOutputDevice;
   int iAudioOutputVolume;

   int iDevRxLoopTimeout;
   u32 uShowBigRxHistoryInterface;

   int iSiKPacketSize;

   int iRadioTxUsesPPCAP;
   int iRadioBypassSocketBuffers;
   int iStreamerOutputMode; // 0 - sm, 1 - pipe, 2 - udp
   int iVideoMPPBuffersSize;
   int iHDMIVSync;
   int iEasterEgg1;
   int iDbgPingGraphs;
   int iEnableDebugStats;
   int iWaitFullFrameForOutput;

   int iRecordOSD;
   int iRecordSTR;
   int iRecordSTRFramerate;
   int iRecordSTRTime;
   int iRecordSTRHome;
   int iRecordSTRGPS;
   int iRecordSTRAlt;
   int iRecordSTRRSSI;
   int iRecordSTRVoltage;
   int iRecordSTRBitrate;
} ControllerSettings;

int save_ControllerSettings();
int load_ControllerSettings();
void reset_ControllerSettings();
void reset_ControllerPriorities();
ControllerSettings* get_ControllerSettings();

u32 compute_ping_interval_ms(u32 uModelFlags, u32 uRxTxSyncType, u32 uCurrentVideoProfileFlags);

#ifdef __cplusplus
}  
#endif 