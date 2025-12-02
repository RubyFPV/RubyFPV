#pragma once
#include "../base/base.h"
#include "../base/models.h"

void packet_utils_init();
void packet_utils_uninit();

int compute_packet_uplink_datarate(int iVehicleRadioLink, int iRadioInterface, type_radio_links_parameters* pRadioLinksParams, u8* pPacketData);

int send_packet_to_radio_interfaces(u8* pPacketData, int nPacketLength, int iSendToSingleRadioLink, int iRepeatCount, int iTraceSrouce);

int get_controller_radio_interface_index_for_radio_link(int iLocalRadioLinkId);
