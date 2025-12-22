#pragma once

#include "../base/base.h"

void start_pipes_to_router();
void stop_pipes_to_router();

void send_pause_adaptive_to_router(u32 uTimeToPauseMs);
void send_reset_adaptive_state_to_router(u32 uVehicleId);

int send_model_changed_message_to_router(u32 uChangeType, u8 uExtraParam);
int send_control_message_to_router(u8 packet_type, u32 extraParam);
int send_control_message_to_router_and_data(u8 packet_type, u8* pData, int nDataLength);
int send_packet_to_router(u8* pPacket, int nLength);

int try_read_messages_from_router(u32 uMaxMiliseconds);
int _process_received_message_from_router(u8* pPacketBuffer);
