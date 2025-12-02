#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiopackets_rc.h"
#include "../base/shared_mem.h"
#include "../base/shared_mem_controller_only.h"
#include "../base/shared_mem_i2c.h"
#include "../base/models.h"
#include "../base/models_list.h"
#include "../base/commands.h"
#include "../base/core_plugins_settings.h"
#include "../base/ctrl_settings.h"
#include "../renderer/render_engine.h"
#include "../base/plugins_settings.h"
#include "../public/telemetry_info.h"
#include "../public/settings_info.h"
#include "../public/render_engine_ui.h"

#include "popup_commands.h"
#include "popup_camera_params.h"
#include "menu_objects.h"
#include "osd_plugins.h"
#include "shared_vars_ipc.h"
#include "shared_vars_state.h"
#include "shared_vars_osd.h"


extern vehicle_and_telemetry_info_t g_VehicleTelemetryInfo;

extern u32 g_uControllerId;
extern int g_iBootCount;
extern bool g_bDebugState;
extern bool g_bQuit;

extern RenderEngine* g_pRenderEngine;
extern RenderEngineUI* g_pRenderEngineOSDPlugins;

extern ControllerSettings* g_pControllerSettings;

extern u32 g_uVideoRecordingStartTime;
extern bool g_bIsVideoRecording;
extern bool g_bIsVideoProcessing;
extern bool g_bIsVideoPlaying;
extern u32 g_uVideoPlayingTimeMs;
extern u32 g_uVideoPlayingLengthSec;
extern bool g_bIsTestingAdaptiveVideo;

extern int g_iControllerCPUSpeedMhz;
extern int g_iControllerCPULoad;
extern int g_iControllerCPUTemp;
extern u16 g_uControllerCPUFlags;

extern PopupCameraParams* g_pPopupCameraParams;
extern Popup* g_pPopupLooking;
extern Popup* g_pPopupWrongModel;
extern Popup* g_pPopupLinkLost;

extern bool g_bToglleOSDOff;
extern bool g_bToglleStatsOff;
extern bool g_bToglleAllOSDOff;
extern bool g_bFreezeOSD;

extern bool g_bMenuPopupUpdateVehicleShown;
extern type_radio_links_parameters g_LastGoodRadioLinksParams;
extern bool g_bSwitchingRadioLink;

extern bool g_bHasVideoDataOverloadAlarm;
extern bool g_bHasVideoTxOverloadAlarm;
extern Popup* g_pPopupVideoOverloadAlarm;

extern int g_iMustSendCurrentActiveOSDLayoutCounter;

extern CorePluginSettings g_listVehicleCorePlugins[MAX_CORE_PLUGINS_COUNT];
extern int g_iVehicleCorePluginsCount;

extern bool g_bMustNegociateRadioLinksFlag;
extern bool g_bAskedForNegociateRadioLink;
