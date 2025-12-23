#pragma once
#include "../public/i2c_protocols.h"
#include "config.h"

#define MAX_I2C_BUS_COUNT 10
#define MAX_I2C_BUS_NAME 32

#define MAX_I2C_DEVICES 20
#define MAX_I2C_DEVICE_SETTINGS 24

#define MAX_I2C_DEVICE_NAME 32
#define I2C_DEVICE_ADDRESS_CAMERA_HDMI 0x0F
#define I2C_DEVICE_ADDRESS_CAMERA_CSI  0x36
#define I2C_DEVICE_ADDRESS_CAMERA_VEYE 0x3B
#define I2C_DEVICE_ADDRESS_INA219_1  0x40
#define I2C_DEVICE_ADDRESS_INA219_2  0x41
#define I2C_DEVICE_ADDRESS_PICO_RC_IN 0x18
#define I2C_DEVICE_ADDRESS_PICO_EXTENDER 0x19
#define I2C_DEVICE_ADDRESS_SSD1306_1 0x3C
#define I2C_DEVICE_ADDRESS_SSD1306_2 0x3D

#define I2C_DEVICE_TYPE_UNKNOWN 0
#define I2C_DEVICE_TYPE_CAMERA_CSI  1
#define I2C_DEVICE_TYPE_CAMERA_VEYE 2
#define I2C_DEVICE_TYPE_CAMERA_HDMI 3
#define I2C_DEVICE_TYPE_INA219 5
#define I2C_DEVICE_TYPE_PICO_RC_IN 6
#define I2C_DEVICE_TYPE_PICO_EXTENDER 7
#define I2C_DEVICE_TYPE_RUBY_ADDON 10
#define I2C_DEVICE_TYPE_OLED_SCREEN 11

#define I2C_DEVICE_NAME_UNKNOWN "Unknown"
#define I2C_DEVICE_NAME_CAMERA_CSI "Camera CSI"
#define I2C_DEVICE_NAME_CAMERA_HDMI "Camera HDMI"
#define I2C_DEVICE_NAME_CAMERA_VEYE "Camera Veye"
#define I2C_DEVICE_NAME_INA219 "Current Sensor INA219"
#define I2C_DEVICE_NAME_RC_IN "RC Input"
#define I2C_DEVICE_NAME_PICO_EXTENDER "Pico Extender"
#define I2C_DEVICE_NAME_RUBY_ADDON "AddOn"
#define I2C_DEVICE_NAME_OLED_SCREEN "OLED Screen"

// RC In device // To deprecate, use i2c_protocols definitions
#define I2C_DEVICE_COMMAND_ID_RC_IN_SET_STREAM_TYPE_SBUS 1
#define I2C_DEVICE_COMMAND_ID_RC_IN_SET_STREAM_TYPE_IBUS 2
#define I2C_DEVICE_COMMAND_ID_RC_IN_SET_UN_INVERTED 3
#define I2C_DEVICE_COMMAND_ID_RC_IN_SET_INVERTED 4
#define I2C_DEVICE_COMMAND_ID_RC_IN_GET_FRAME_NUMBER 9
#define I2C_DEVICE_COMMAND_ID_RC_IN_GET_CHANNEL 10 // + ch index
#define I2C_DEVICE_PARAM_MAX_CHANNELS 24

// Pico Extender // To deprecate, use i2c_protocols definitions
#define I2C_DEVICE_COMMAND_ID_PICO_EXTENDER_GET_ROTARY_ENCODER_ACTIONS 50
#define I2C_DEVICE_COMMAND_ID_PICO_EXTENDER_GET_ROTARY_ENCODER_ACTIONS_SLOW 51

// To deprecate, use i2c_protocols definitions
#define I2C_DEVICE_COMMAND_ID_GET_VERSION 100

#ifdef __cplusplus
extern "C" {
#endif 

typedef struct
{
   int nBusNumber;
   char szName[MAX_I2C_BUS_NAME];
   u8 devices[128]; // 0 - none, 1 - detected
   u8 picoExtenderVersion;
} ALIGN_STRUCT_SPEC_INFO hw_i2c_bus_info_t;


typedef struct
{
   char szDeviceName[MAX_I2C_DEVICE_NAME];
   int  nI2CAddress;
   int  nVersion;
   int  nDeviceType;
   int  bConfigurable;
   int  bEnabled;
   u32  uCapabilitiesFlags;
   u32  uParams[MAX_I2C_DEVICE_SETTINGS];
} ALIGN_STRUCT_SPEC_INFO t_i2c_device_settings;


void hardware_i2c_reset_enumerated_flag();
void hardware_i2c_log_devices();
void hardware_i2c_enumerate_busses(int iConsoleLog);
int hardware_i2c_get_busses_count();
int hardware_i2c_get_found_count_known_devices();
int hardware_i2c_get_found_count_configurable_devices();
hw_i2c_bus_info_t* hardware_i2c_get_bus_info(int busIndex);
int hardware_i2c_has_device_id(u8 deviceAddress);
int hardware_i2c_get_device_bus_number(u8 deviceAddress);
int hardware_is_known_i2c_device(u8 deviceAddress);
void hardware_i2c_get_device_name(u8 deviceAddress, char* szOutput);

int hardware_i2c_HasPicoExtender(); // to deprecate
int hardware_i2c_get_pico_extender_version(); // to deprecate


int hardware_i2c_save_device_settings();
int hardware_i2c_load_device_settings();

t_i2c_device_settings* hardware_i2c_get_device_settings(u8 i2cAddress);
t_i2c_device_settings* hardware_i2c_add_device_settings(u8 i2cAddress);
void hardware_i2c_update_device_info(u8 i2cAddress);
void hardware_i2c_check_and_update_device_settings();

int hardware_i2c_has_current_sensor();
int hardware_i2c_has_external_extenders();
int hardware_i2c_has_external_extenders_rotary_encoders();
int hardware_i2c_has_external_extenders_buttons();
// Returns i2c address of the device
int hardware_i2c_has_external_extenders_rcin();
int hardware_i2c_has_oled_screen();
#ifdef __cplusplus
}  
#endif
