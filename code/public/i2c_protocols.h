#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

#ifdef __cplusplus
}
#endif

// Protocol is:
// 1. * Master sends a command to slave (2...N bytes, depending on command type). First byte of all commands is the command start flag;
// 2. * Slave responds with the right answer based on the command (1...N bytes, depending on command response type);
// 3. * Repeat the steps above;
// That means: slave device (Ruby extension device) just waits on I2C for commands from master (Ruby main controller) and responds to those commands as needed.
// Note!!! All commands and responses have 1 extra byte at the end as the CRC
// Use the example function at the end of file to compute the CRC
// Note!!! Always check the CRC value when you get something. Ignore the commands with bad CRC. There can always be noise or bad I2C devices on the I2C bus.


#define I2C_DEVICE_DEFAULT_ADDRESS 0x60
#define I2C_DEVICE_MIN_ADDRESS_RANGE 0x60
#define I2C_DEVICE_MAX_ADDRESS_RANGE 0x6F

// Note: the address [I2C_DEVICE_MAX_ADDRESS_RANGE-2] is used by the Ruby Pico Extender. If you use one, your plugin should use a different address.

#define I2C_PROTOCOL_STRING_LENGTH 24

// Capabilities flags supported by the slave Ruby device; these capabilities will be queried by the Ruby controller and will be reported back to Ruby controller using the I2C interface;

#define I2C_CAPABILITY_FLAG_SPI       ((u16)(((u16)0x01)<<1))    // Set if the slave device does support SPI communication with the Ruby controller (if not, only I2C is used)
#define I2C_CAPABILITY_FLAG_BUTTONS   ((u16)(((u16)0x01)<<2))    // Set if the slave device has buttons (for UI navigation in Ruby);
#define I2C_CAPABILITY_FLAG_ROTARY    ((u16)(((u16)0x01)<<3))    // Set if the slave device has rotary encoder (for UI navigation/Camera control);
#define I2C_CAPABILITY_FLAG_ROTARY2    ((u16)(((u16)0x01)<<4))    // Set if the slave device has the secondary rotary encoder (for UI navigation/Camera control);
#define I2C_CAPABILITY_FLAG_LEDS      ((u16)(((u16)0x01)<<5))    // Set if the slave device has LEDs to be controlled by the Ruby controller;
#define I2C_CAPABILITY_FLAG_RC_INPUT  ((u16)(((u16)0x01)<<6))    // Set if the slave device has RC input hardware;
#define I2C_CAPABILITY_FLAG_RC_OUTPUT ((u16)(((u16)0x01)<<7))       // Set if the slave device should output RC frames to the FC;
#define I2C_CAPABILITY_FLAG_FLIGHT_CONTROL ((u16)(((u16)0x01)<<8))  // Set if the slave device wants to send flight commands to the vehicle;
#define I2C_CAPABILITY_FLAG_CAMERA_CONTROL ((u16)(((u16)0x01)<<9))  // Set if the slave device wants to send camera commands (brightness, contrast, etc);
#define I2C_CAPABILITY_FLAG_SOUNDS    ((u16)(((u16)0x01)<<10))  // Set if the slave device can play sounds (alarms);

#define I2C_COMMAND_START_FLAG 0xFF

#define I2C_COMMAND_ID_GET_FLAGS  0x01
// Get flags: master asks the slave for the capabilities it supports;
// Master sends: 1 byte: command id (I2C_COMMAND_ID_GET_FLAGS)
// Slave responds: 2 bytes: capabilities (flags as OR-ed bits) supported by the I2C device;


#define I2C_COMMAND_ID_GET_VERSION  0x02
// Get flags: master asks the slave for the version;
// Master sends: 1 byte: command id (I2C_COMMAND_ID_GET_VERSION);
// Slave responds: 1 byte: version of the software on the I2C slave device;


#define I2C_COMMAND_ID_GET_NAME  0x03
// Get name: master asks slave for the device name, to be used in the user interface;
// Master sends: 1 byte: command id;
// Slave responds: I2C_PROTOCOL_STRING_LENGTH bytes: null terminated string, padded with 0 up to I2C_PROTOCOL_STRING_LENGTH bytes;


#define I2C_COMMAND_ID_SET_ADDRESS  0x04
// Set address: master asks slave to set it's address to a custom one, to avoid conflicts
// Master sends: 2 bytes: command id and the new I2C address to be used by slave device;
// Slave responds: 1 byte: 0 - ok, 1 - error


#define I2C_COMMAND_ID_GET_BUTTONS_EVENTS  0x10
// Get buttons: master asks slave if any buttons where pressed;
// Master sends: 1 byte: command id;
// Slave responds: 4 bytes:
//   first 2 bytes: each bit represents 1 if a button was pressed, for a possible of 16 buttons on the device;
//   last 2 bytes: each bit represents 1 if a button was long pressed, for a possible of 16 buttons on the device;
// bit 0 - Menu/Ok button
// bit 1 - Cancel button
// bit 2 - Plus button
// bit 3 - Minus button
// bit 4 - QA1 button
// bit 5 - QA2 button
// bit 6 - QA3 button
// bit 7 - Action Plus button
// bit 8 - Action Minus button
// bit 9... - future use



#define I2C_COMMAND_ID_GET_ROTARY_EVENTS  0x11
#define I2C_COMMAND_ID_GET_ROTARY_EVENTS2  0x12
// Get rotary encoder events (for main and secondary rotary encoders, if present);
// Master asks slave if any rotary encoder events (first or secondary rotary encoder) took place.
// Master sends: 1 byte: command id;
// Slave responds: 1 byte: each bit represents:
//     bit 0: rotary encoder was pressed;
//     bit 1: rotary encoder was long pressed;
//     bit 2: rotary encoder was rotated CCW;
//     bit 3: rotary encoder was rotated CW;
//     bit 4: rotary encoder was rotated fast CCW;
//     bit 5: rotary encoder was rotated fast CW;


#define I2C_COMMAND_ID_SET_RC_INPUT_FLAGS  0x20
#define I2C_COMMAND_RC_FLAG_SBUS 1
#define I2C_COMMAND_RC_FLAG_IBUS (1<<1)
#define I2C_COMMAND_RC_FLAG_PPM (1<<2)
#define I2C_COMMAND_RC_FLAG_INVERT_UART (1<<4)
// Set RC Input flags: master tells the slave what RC protocol to read and if the RC input UART should be inverted or not (SBUS should be inverted).
// Master sends: 2 bytes: command id and the RC input flags:
//  bit 0-4: RC protocol: 1 - parse SBUS RC input; 2 - parse IBUS RC input; 4 - parse PPM RC input;
//  bit 4: 0 - non inverted UART, 1 - invert UART;
// Slave responds: 1 byte: 0 - ok, 1 - error

#define I2C_COMMAND_ID_RC_GET_CHANNELS  0x21
// Gets the current RC channels values from the slave device;
// Master sends: 1 byte: command id;
// Slave responds: 26 bytes: 1 byte flags, 1 byte frame number, 24 bytes for RC channels values (16 channels, 12 bits per channel, for 0...4095 posible values).
// byte 0: flags: bit 0 is set if RC input failsafe event occured on the slave device.
// byte 1: frame number: monotonically increasing on each received RC frame
// byte 1-16 - LSBits (8 bits) of each channel, from ch 1 to ch 16;
// byte 17-24 - MSBits (4 bits) of each channel, from ch 1 to ch 16;
// Unused channels should be set to zero.


#define I2C_COMMAND_ID_SET_RC_OUTPUT_FLAGS  0x30
// Set RC Output flags: master tells the slave what RC protocol to generate and if the RC output UART should be inverted or not (SBUS should be inverted).
// Master sends: 2 bytes: command id and the RC output flags (same bits as RC input flags):
//  bit 0-4: RC protocol: 1 - SBUS RC output; 2 - IBUS RC output; 4 - PPM RC output;
//  bit 4: 0 - non inverted UART, 1 - invert UART;
// Slave responds: 1 byte: 0 - ok, 1 - error


#define I2C_COMMAND_ID_RC_SET_CHANNELS  0x31
// Sets the current RC channels values to the slave device;
// Master sends: 26 bytes: 1 byte command id, 1 byte flags, 24 bytes: RC channels values (16 channels, 12 bits per channel, for 0...4095 posible values).
// byte 0: command id (this one);
// byte 1: flags: bit 0: set if failsafe should be signaled by the slave device; not set otherways;
// byte 2-17 - LSBits (8 bits) of each channel, from ch 1 to ch 16;
// byte 18-25 - MSBits (4 bits) of each channel, from ch 1 to ch 16;
// Unused channels should be set to zero.


#define I2C_COMMAND_ID_FLIGHT_CTRL_QUERY_ARM  0x40
// Ask the slave device if the vehicle should receive the arm or disarm command
// Master sends: 1 byte: command id;
// Slave responds: 1 byte:
// bit 0: 0 - no change; 1 - has new arm state
// bit 1: 0 - disarm; 1 - arm

#define I2C_COMMAND_ID_FLIGHT_CTRL_QUERY_MODE  0x41
// Ask the slave device if the vehicle should receive a new flight mode
// Master sends: 1 byte: command id;
// Slave responds: 1 byte:
// bit 0: 0 - no change; 1 - has new flight mode;
// bit 1..7: new flight mode as defined by ArduPilot;


#define I2C_COMMAND_ID_CAMERA_CTRL_QUERY  0x50
// Ask the slave device if they have any pending camera params changes (or wants to change something);
// Master sends: 1 byte: command id;
// Slave responds: 1 byte:
// bit 0: 0 - no change; 1 - some changes are pending

#define I2C_COMMAND_ID_CAMERA_CTRL_GET_PARAMS  0x51
// Ask the slave device for the new camera params;
// Master sends: 1 byte: command id;
// Slave responds: 4 bytes:
// byte 0: brightness (0..100)
// byte 1: contrast (0..100)
// byte 2: saturation (0..100)
// byte 3: sharpness (0..100)

#define I2C_COMMAND_ID_VIDEO_GET_QUALITY 0x55
// Ask the slave device if they want to change the video quality
// Master sends: 1 byte: command id;
// Slave responds: 1 byte:
// bit 0: 0 - no (video quality is auto, decided by Ruby), 0-100 sets a custom video quality (0=lowest quality)


#define I2C_COMMAND_ID_PLAY_SOUND_ALARM  0x60
// Ask the slave to play a particular sound
// Master sends: 2 bytes: command id; sound id:
// 1 - battery alarm;
// 2 - arm alarm;
// 3 - disarm alarm;
// Slave responds: 1 byte:
// bit 0: 0 - failed; 1 - succeeded;


// CRC table used for CRC calculations
/*
const u8 s_crc_i2c_table[256] = {
0x00,0x31,0x62,0x53,0xC4,0xF5,0xA6,0x97,0xB9,0x88,0xDB,0xEA,0x7D,0x4C,0x1F,0x2E,
0x43,0x72,0x21,0x10,0x87,0xB6,0xE5,0xD4,0xFA,0xCB,0x98,0xA9,0x3E,0x0F,0x5C,0x6D,
0x86,0xB7,0xE4,0xD5,0x42,0x73,0x20,0x11,0x3F,0x0E,0x5D,0x6C,0xFB,0xCA,0x99,0xA8,
0xC5,0xF4,0xA7,0x96,0x01,0x30,0x63,0x52,0x7C,0x4D,0x1E,0x2F,0xB8,0x89,0xDA,0xEB,
0x3D,0x0C,0x5F,0x6E,0xF9,0xC8,0x9B,0xAA,0x84,0xB5,0xE6,0xD7,0x40,0x71,0x22,0x13,
0x7E,0x4F,0x1C,0x2D,0xBA,0x8B,0xD8,0xE9,0xC7,0xF6,0xA5,0x94,0x03,0x32,0x61,0x50,
0xBB,0x8A,0xD9,0xE8,0x7F,0x4E,0x1D,0x2C,0x02,0x33,0x60,0x51,0xC6,0xF7,0xA4,0x95,
0xF8,0xC9,0x9A,0xAB,0x3C,0x0D,0x5E,0x6F,0x41,0x70,0x23,0x12,0x85,0xB4,0xE7,0xD6,
0x7A,0x4B,0x18,0x29,0xBE,0x8F,0xDC,0xED,0xC3,0xF2,0xA1,0x90,0x07,0x36,0x65,0x54,
0x39,0x08,0x5B,0x6A,0xFD,0xCC,0x9F,0xAE,0x80,0xB1,0xE2,0xD3,0x44,0x75,0x26,0x17,
0xFC,0xCD,0x9E,0xAF,0x38,0x09,0x5A,0x6B,0x45,0x74,0x27,0x16,0x81,0xB0,0xE3,0xD2,
0xBF,0x8E,0xDD,0xEC,0x7B,0x4A,0x19,0x28,0x06,0x37,0x64,0x55,0xC2,0xF3,0xA0,0x91,
0x47,0x76,0x25,0x14,0x83,0xB2,0xE1,0xD0,0xFE,0xCF,0x9C,0xAD,0x3A,0x0B,0x58,0x69,
0x04,0x35,0x66,0x57,0xC0,0xF1,0xA2,0x93,0xBD,0x8C,0xDF,0xEE,0x79,0x48,0x1B,0x2A,
0xC1,0xF0,0xA3,0x92,0x05,0x34,0x67,0x56,0x78,0x49,0x1A,0x2B,0xBC,0x8D,0xDE,0xEF,
0x82,0xB3,0xE0,0xD1,0x46,0x77,0x24,0x15,0x3B,0x0A,0x59,0x68,0xFF,0xCE,0x9D,0xAC };

// CRC-8 function:

u8 calculate_i2c_crc(u8* pData, int iLength)
{
   u8 uCrc = 0xFF;
   if ( NULL == pData || iLength <= 0 )
      return uCrc;
   for ( int i = 0; i < iLength; i++)
      uCrc = s_crc_i2c_table[pData[i] ^ uCrc];
   return uCrc;
}

*/