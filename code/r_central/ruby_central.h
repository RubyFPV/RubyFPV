#pragma once
#include "../base/base.h"
#include "../base/hardware_procs.h"
#include "popup.h"

#define START_SEQ_DELAY 50

#define START_SEQ_NONE 0
#define START_SEQ_PRE_LOAD_CONFIG 1
#define START_SEQ_LOAD_CONFIG 2
#define START_SEQ_PRE_SEARCH_DEVICES 3
#define START_SEQ_SEARCH_DEVICES 4
#define START_SEQ_PRE_SEARCH_INTERFACES 5
#define START_SEQ_SEARCH_INTERFACES 6
#define START_SEQ_PRE_NICS 10
#define START_SEQ_NICS 11
#define START_SEQ_PRE_SYNC_DATA 15
#define START_SEQ_SYNC_DATA 16
#define START_SEQ_SYNC_DATA_FAILED 17

#define START_SEQ_PRE_LOAD_DATA 30
#define START_SEQ_LOAD_DATA 31
#define START_SEQ_LOAD_PLUGINS 35
#define START_SEQ_START_PROCESSES 40
#define START_SEQ_SCAN_MEDIA_PRE 50
#define START_SEQ_SCAN_MEDIA 51
#define START_SEQ_PROCESS_VIDEO 60

#define START_SEQ_COMPLETED 200
#define START_SEQ_FAILED 201

Popup* ruby_get_startup_popup();

void ruby_processing_loop(bool bNoKeys);

void render_background_and_paddings(bool bForceBackground);
void render_all(u32 timeNow, bool bForceBackground = false, bool bDoInputLoop = false);
void render_all_with_menus(u32 timeNow, bool bRenderMenus, bool bForceBackground = false, bool bDoInputLoop = false);
int ruby_start_recording();
int ruby_stop_recording();
bool ruby_is_recording();

void ruby_load_models();

int ruby_get_start_sequence_step();

void ruby_signal_alive();

void ruby_pause_watchdog(const char* szReason);
void ruby_resume_watchdog(const char* szReason);
void ruby_resume_watchdog_force(const char* szReason);

void synchronize_shared_mems();

void ruby_set_active_model_id(u32 uVehicleId);

void ruby_mark_reinit_hdmi_display();
void ruby_reinit_hdmi_display();
void ruby_shutdown_ui();

bool ruby_central_has_sdcard_update(bool bDoUpdateToo);
void ruby_central_show_mira(bool bShow);
bool ruby_central_is_showing_mira();
