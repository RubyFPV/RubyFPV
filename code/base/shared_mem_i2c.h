#pragma once

#include "../base/base.h"
#include "../base/config.h"

#define SHARED_MEM_NAME_I2C_CURRENT "/SYSTEM_SHARED_MEM_RUBY_I2C_CURRENT"
#define SHARED_MEM_NAME_I2C_CONTROLLER_RC_IN "/SYSTEM_SHARED_MEM_RUBY_I2C_CONTROLLER_RC_IN"
#define SHARED_MEM_NAME_I2C_ROTARY_ENCODER_BUTTONS_EVENTS "/SYSTEM_SHARED_MEM_RUBY_I2C_ROTARY_ENCODER_BUTTONS_EVENTS"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
   u32 lastSetTime;
   u32 uParam;
   u32 voltage;  // MAX_U32 for not measured
   u32 current;  // MAX_U32 for not measured
} ALIGN_STRUCT_SPEC_INFO t_shared_mem_i2c_current;


#define RC_IN_FLAG_HAS_INPUT 0x01

typedef struct
{
   u8 version;
   u8 uFlags; // bit 0 - has input
   u32 uTimeStamp;
   u8 uFrameIndex;
   u8 uChannelsCount;
   u16 uChannels[MAX_RC_CHANNELS];
} ALIGN_STRUCT_SPEC_INFO t_shared_mem_i2c_controller_rc_in;

typedef struct
{
   u32 uTimeStamp; // when the event happened
   u16 uEventIndex; // increasing index
   u32 uButtonsEvents; // 4 bytes, same as in i2c_protocols;
   u8  uRotaryEncoderEvents; //1 byte, same as in i2c_protocols;
   u8  uRotaryEncoder2Events;
   u8  uHasRotaryEncoder;
   u8  uHasSecondaryRotaryEncoder;
   u32 uCRC; // should be last in the structure as is not part of the CRC computation;
} ALIGN_STRUCT_SPEC_INFO t_shared_mem_i2c_rotary_encoder_buttons_events;


t_shared_mem_i2c_current* shared_mem_i2c_current_open_for_write();
t_shared_mem_i2c_current* shared_mem_i2c_current_open_for_read();
void shared_mem_i2c_current_close(t_shared_mem_i2c_current* pAddress);

t_shared_mem_i2c_controller_rc_in* shared_mem_i2c_controller_rc_in_open_for_read();
t_shared_mem_i2c_controller_rc_in* shared_mem_i2c_controller_rc_in_open_for_write();
void shared_mem_i2c_controller_rc_in_close(t_shared_mem_i2c_controller_rc_in* pAddress);

t_shared_mem_i2c_rotary_encoder_buttons_events* shared_mem_i2c_rotary_encoder_buttons_events_open_for_read();
t_shared_mem_i2c_rotary_encoder_buttons_events* shared_mem_i2c_rotary_encoder_buttons_events_open_for_write();
void shared_mem_i2c_rotary_encoder_buttons_events_close(t_shared_mem_i2c_rotary_encoder_buttons_events* pAddress);

#ifdef __cplusplus
}
#endif
