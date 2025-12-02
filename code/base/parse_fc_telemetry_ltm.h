#pragma once

#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../radio/radiopackets2.h"

bool parse_telemetry_from_fc_ltm( u8* buffer, int length, t_packet_header_fc_telemetry* pphfct, t_packet_header_ruby_telemetry_extended_v6* pPHRTE, u8 vehicleType );
