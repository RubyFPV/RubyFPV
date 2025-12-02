#pragma once
#include "../base/base.h"
#include "../base/models.h"
#include "../radio/radiopackets2.h"

int radio_links_open_rxtx_radio_interfaces();
void radio_links_close_rxtx_radio_interfaces();
bool radio_links_apply_settings(Model* pModel, int iRadioLink, type_radio_links_parameters* pRadioLinkParamsOld, type_radio_links_parameters* pRadioLinkParamsNew);

u32 radio_links_get_last_start_time();
bool radio_links_are_marked_for_restart();
bool radio_links_restart(bool bAsync);
