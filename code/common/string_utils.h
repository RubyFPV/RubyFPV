#pragma once

#ifdef __cplusplus
extern "C" {
#endif 


void str_sanitize_modelname(char* szName);
void str_sanitize_filename(char* szFileName);
char* str_capitalize_first_letter(char* szText);

char* str_format_time(u32 miliseconds);
char* str_format_binary_number(u32 uNumber);
char* str_format_schedule_policy(int iSchedulePolicy);

void str_getDataRateDescription(int dataRateBPS, int iHT40, char* szOutput);
void str_getDataRateDescriptionNoSufix(int dataRateBPS, char* szOutput);
char* str_getDataRateDescriptionAlternative(int dataRateBPS);
char* str_format_datarate_inline(int dataRateBPS);

char* str_format_bitrate_inline(int iBitrateBPS);
void str_format_bitrate(int bitrate_bps, char* szBuffer);
void str_format_bitrate_no_sufix(int bitrate_bps, char* szBuffer);
const char* str_getBandName(u32 band);
void str_get_supported_bands_string(u32 bands, char* szOut);

char* str_format_frequency(u32 uFrequencyKhz);
char* str_format_frequency_no_sufix(u32 uFrequencyKhz);
char* str_get_packet_type(int iPacketType);
char* str_get_packet_history_symbol(int iPacketType, int iRepeatCount);
char* str_get_packet_test_link_command(int iTestCommandId);

char* str_get_pipe_flags(int iFlags);
char* str_format_processes_flags(u32 uProcessesFlags);

const char* str_get_hardware_board_name(u32 board_type);
const char* str_get_hardware_board_name_short(u32 board_type);
const char* str_get_hardware_wifi_name(u32 wifi_type);
const char* str_get_hardware_camera_type_string(u32 uCamType);
void str_get_hardware_camera_type_string_to_string(u32 uCamType, char* szOutput);

const char* str_get_radio_type_description(int iRadioType);
const char* str_get_radio_driver_description(int iDriverType);
const char* str_get_radio_card_model_string(int cardModel);
const char* str_get_radio_card_model_string_short(int cardModel);
void str_get_radio_capabilities_description(u32 flags, char* szOutput);

void str_get_radio_frame_flags_description(u32 radioFlags, char* szOutput);
char* str_get_radio_frame_flags_description2(u32 radioFlags);

char* str_format_adaptive_video_flags(u8 uFlags);
char* str_format_video_profile_flags(u32 uVideoProfileFlags);
char* str_format_video_encoding_flags(u32 uVideoProfileEncodingFlags);
char* str_get_video_profile_name(u32 videoProfileId);
char* str_get_decode_h264_profile_name(u8 uH264Profile, u8 uH264ProfileConstrains, u8 uH264Level);

char* str_get_radio_stream_name(int iStreamId);

char* str_get_osd_screen_name(int iOSDId);

char* str_get_serial_port_usage(int iSerialPortUsage);

char* str_get_model_flags(u32 uModelFlags);
char* str_get_developer_flags(u32 uDeveloperFlags);

char* str_get_command_response_flags_string(u32 uResponseFlags);

char* str_get_component_id(int iComponentId);
char* str_get_model_change_type(int iModelChangeType);

char* str_format_relay_flags(u32 uRelayFlags);
char* str_format_relay_mode(u32 uRelayMode);

char* str_format_firmware_type(u32 uFirmwareType);

#ifdef __cplusplus
}  
#endif 