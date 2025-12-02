#pragma once

#include "../base/base.h"

void init_radio_rx_structures();

void process_received_single_radio_packet(int iInterfaceIndex, u8* pData, int iDataLength);
