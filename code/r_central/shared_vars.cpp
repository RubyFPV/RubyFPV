/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "shared_vars.h"

vehicle_and_telemetry_info_t g_VehicleTelemetryInfo;

u32 g_uControllerId = 0;
int g_iBootCount = 0;
bool g_bDebugState = false;
bool g_bQuit = false;

RenderEngine* g_pRenderEngine = NULL;
RenderEngineUI* g_pRenderEngineOSDPlugins = NULL;

ControllerSettings* g_pControllerSettings = NULL;

u32 g_uVideoRecordingStartTime = 0;
bool g_bIsVideoRecording = false;
bool g_bIsVideoProcessing = false;
bool g_bIsVideoPlaying = false;
u32 g_uVideoPlayingTimeMs = 0;
u32 g_uVideoPlayingLengthSec = 0;
bool g_bIsTestingAdaptiveVideo = false;

int g_iControllerCPUSpeedMhz = 0;
int g_iControllerCPULoad = 0;
int g_iControllerCPUTemp = 0;
u16 g_uControllerCPUFlags = 0;

bool g_bToglleOSDOff = false;
bool g_bToglleStatsOff = false;
bool g_bToglleAllOSDOff = false;
bool g_bFreezeOSD = false;

bool g_bMenuPopupUpdateVehicleShown = false;
type_radio_links_parameters g_LastGoodRadioLinksParams;

bool g_bSwitchingRadioLink = false;

PopupCameraParams* g_pPopupCameraParams = NULL;
Popup* g_pPopupLooking = NULL;
Popup* g_pPopupWrongModel = NULL;
Popup* g_pPopupLinkLost = NULL;

bool g_bHasVideoDataOverloadAlarm = false;
bool g_bHasVideoTxOverloadAlarm = false;
Popup* g_pPopupVideoOverloadAlarm = NULL;

int g_iMustSendCurrentActiveOSDLayoutCounter = 0;

CorePluginSettings g_listVehicleCorePlugins[MAX_CORE_PLUGINS_COUNT];
int g_iVehicleCorePluginsCount = 0;

bool g_bMustNegociateRadioLinksFlag = false;
bool g_bAskedForNegociateRadioLink = false;