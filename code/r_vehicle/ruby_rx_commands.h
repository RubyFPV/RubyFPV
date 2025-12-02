#pragma once
#include "../base/base.h"

void signalReboot(bool bSaveModel);
void sendControlMessage(u8 packet_type, u32 extraParam);
void sendCommandReply(u8 responseFlags, int iResponseExtraParam, int delayMiliSec);

int r_start_commands_rx(int argc, char* argv[]);