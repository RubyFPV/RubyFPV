#pragma once

#include "config.h"

#define MAX_PREFERENCES_CHECKBOXES 100

#ifdef __cplusplus
extern "C" {
#endif 

#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_TX_PACKETS          ((u32)(((u32)0x01)<<2))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_OUTPUT_VIDEO_FRAMES    ((u32)(((u32)0x01)<<3))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_DBM                 ((u32)(((u32)0x01)<<4))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_MISSING_PACKETS     ((u32)(((u32)0x01)<<5))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_MISSING_PACKETS_MAX_GAP ((u32)(((u32)0x01)<<6))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_CONSUMED_PACKETS    ((u32)(((u32)0x01)<<7))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_MIN_MAX_ACK_TIME       ((u32)(((u32)0x01)<<8))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_VIDEO_MAX_EC_USED   ((u32)(((u32)0x01)<<9))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_VIDEO_UNRECOVERABLE_BLOCKS ((u32)(((u32)0x01)<<10))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_PROFILE_CHANGES  ((u32)(((u32)0x01)<<11))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_RETRANSMISSIONS  ((u32)(((u32)0x01)<<12))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_ACK_TIME_HISTORY       ((u32)(((u32)0x01)<<13))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_AIR_GAPS            ((u32)(((u32)0x01)<<14))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_TX_HIGH_REG_PACKETS    ((u32)(((u32)0x01)<<15))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_SNR                 ((u32)(((u32)0x01)<<16))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_SIZES     ((u32)(((u32)0x01)<<17))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_PROC_TIMES ((u32)(((u32)0x01)<<18))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_RECV_DURATION ((u32)(((u32)0x01)<<19))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_CAPTURE_JITTER ((u32)(((u32)0x01)<<20))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_SEND_DURATION ((u32)(((u32)0x01)<<21))


#define ID_DONOT_SHOW_AGAIN_MIXED_PI_OPENIPC_HARDWARE 1

typedef enum
{
   osdLayout1 = 0,
   osdLayout2,
   osdLayout3,
   osdLayoutLean,
   osdLayoutLeanExtended,
   osdLayoutLast
} osdLayout;

typedef enum
{
   quickActionNone = 0,
   quickActionCycleOSD,
   quickActionOSDSize,
   quickActionTakePicture,
   quickActionVideoRecord,
   quickActionToggleOSD,
   quickActionToggleStats,
   quickActionToggleAllOff,
   quickActionRelaySwitch,
   quickActionCameraProfileSwitch,
   quickActionRCEnable,
   quickActionRotaryFunction,
   quickActionOSDFreeze,
   quickActionSwitchFavorite,
   quickActionPITMode,
   quickActionLast
} quickAction;


typedef enum
{
   prefVideoDestination_Disk = 0,
   prefVideoDestination_Mem,
   prefVideoDestination_USB
} prefVideoDestination;


typedef enum
{
   prefUnitsMetric = 0,
   prefUnitsImperial = 1,
   prefUnitsMeters = 2,
   prefUnitsFeets = 3
} prefUnits;

typedef struct
{
   int iMenusStacked;
   int iInvertColorsOSD;
   int iInvertColorsMenu;
   int iOSDScreenSize;
   int iOSDFlipVertical;
   int iScaleOSD; // -1 ..0.. +3
   int iScaleAHI; // -3...0 .. 3
   int iScaleMenus; // -3 ..0.. +3
   int iActionQuickButton1;
   int iActionQuickButton2;
   int iActionQuickButton3;
   int iAddOSDOnScreenshots;
   int iStatsToggledOff;
   int iShowLogWindow; // 0 - No, 1 - Only on new content, 2 - Always
   int iMenuDismissesAlarm; // 0
   int iVideoDestination; // 0 - disk, 1 - memory
   int iStartVideoRecOnArm;
   int iStopVideoRecOnDisarm;
   int iShowControllerCPUInfo;
   int iShowBigRecordButton;
   int iSwapUpDownButtons;
   int iSwapUpDownButtonsValues;
   int iAHIToSides;
   int iAHIShowAirSpeed;
   int iAHIStrokeSize; // -2..0..2
   int iUnits;
   int iColorOSD[4]; // 0...255
   int iColorOSDOutline[4]; // 0...255
   int iColorAHI[4]; // 0...255
   int iOSDOutlineThickness; //-3..0..3 (-3 is none)
   int iOSDFont;
   int iRecordingLedAction; // 0 - none, 1 - turn on/off, 2 - blink

   int iDebugMaxPacketSize;
   int iDebugSBWS; // Scramble Blocks Window Size. In HQ mode
   int iDebugRestartOnRadioSilence;
   int iDebugShowDevVideoStats;
   int iDebugShowDevRadioStats;
   int iDebugShowFullRXStats;
   int iDebugShowVideoSnapshotOnDiscard;
   int iDebugWiFiChangeDelay; // 1...100 milisec
   int iPersistentMessages;
   int nLogLevel; // 0 - all, 1 - errors

   int iAutoExportSettings;
   int iAutoExportSettingsWasModified;
   int iShowProcessesMonitor;
   int iShowCPULoad;
   u32 uEnabledAlarms;
   u32 uEnabledQuickMenu;
   int iShowOnlyPresentTxPowerCards;
   int iShowTxBoosters;
   int iMenuStyle; // 0: clasic, 1: sticky left
   int iStopRecordingAfterLinkLostSeconds;
   int iDebugStatsQAButton;
   u32 uDebugStatsFlags;
   int iLanguage;
   int iDoNotShowAgainIds[MAX_PREFERENCES_CHECKBOXES];
   int iDoNotShowAgainValues[MAX_PREFERENCES_CHECKBOXES];
   int iUnitsHeight;
   int iOSDFontBold;
   int iMenuFontBold;
   int iMSPOSDSize; // 0...150%
   int iMSPOSDDeltaX; // delta chars
   int iMSPOSDDeltaY; // delta chars
   int iShowCompactMenus;
} Preferences;

int save_Preferences();
int load_Preferences();
void reset_Preferences();
Preferences* get_Preferences();
int getPreferencesDoNotShowAgain(int iUniqueId);
void setPreferencesDoNotShowAgain(int iUniqueId, int iDoNotShowAgain);
void removePreferencesDoNotShowAgain(int iUniqueId);
#ifdef __cplusplus
}  
#endif
