#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <resolv.h>
#include <string.h>
#include <utime.h>
#include <unistd.h>
#include <getopt.h>
#include <endian.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/ipc.h>
#include <semaphore.h>
#include "config_hw.h"


typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef u32 __le32;

#define MAX_U32 0xFFFFFFFF
#define MAX_VEHICLE_NAME_LENGTH 16
#define MAX_SERVICE_LOG_ENTRY_LENGTH 300
#define LOGGER_MESSAGE_QUEUE_ID 123
#define SM_STREAMER_SIZE 512000
#define SM_STREAMER_NAME "/SSMRVideo"
#define RUBY_HW_CLOCK_ID CLOCK_MONOTONIC

#define SYSTEM_NAME "Ruby"
// dword[3...0]: BB.BB.MM.mm  (BB.BB: build number (highest bytes), MM: major ver, mm: minor ver (lowest byte)) 
#define SYSTEM_SW_VERSION_MAJOR 11
#define SYSTEM_SW_VERSION_MINOR 7
#define SYSTEM_SW_BUILD_NUMBER  11704
//#define SYSTEM_IS_PRERELEASE 1

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define le16_to_cpu(x) (x)
#define le32_to_cpu(x) (x)
#else
#define le16_to_cpu(x) ((((x)&0xff)<<8)|(((x)&0xff00)>>8))
#define le32_to_cpu(x) \
((((x)&0xff)<<24)|(((x)&0xff00)<<8)|(((x)&0xff0000)>>8)|(((x)&0xff000000)>>24))
#endif
#define unlikely(x) (x)

typedef struct
{
    long type;
    char text[MAX_SERVICE_LOG_ENTRY_LENGTH];
} type_log_message_buffer;

typedef struct
{
   u32 uTime;
   u32 uTime2;
   u32 uCounter;
   u32 uCounter2;
   u32 uValueMinimLocal;
   u32 uValueMaximLocal;
   u32 uValueAverageLocal;
   u32 uValueMinim;
   u32 uValueMaxim;
   u32 uValueAverage;
   u32 uValueNow;
} type_u32_couters;

#ifdef __cplusplus
extern "C" {
#endif 

extern u32 g_TimeNow;
extern u32 g_TimeStart;
extern u32 g_TimeNowMicros;
extern u32 g_uLoopCounter;

u32 revert_word(u32 input);

void reset_counters(type_u32_couters* pCounters);

u32 base_compute_crc32(u8 *buf, int length);
u8 base_compute_crc8(u8* pBuffer, int iLength);
int base_check_crc32(u8* pBuffer, int iLength);

void hardware_sleep_sec(u32 uSeconds);
void hardware_sleep_ms(u32 miliSeconds);
void hardware_sleep_micros(u32 microSeconds);

u32 get_current_timestamp_micros();
u32 get_current_timestamp_ms();
u32 get_current_timestamp_ms_tens();
u32 get_boot_timestamp_ms();
int is_first_boot();

char* removeTrailingZero(char* szBuff);
char* removeNewLines(char* szBuff);
char* removeTrailingNewLines(char* szBuff);
char* removeLeadingWhiteSpace(char* szBuff);
char* replaceNewLinesToSpaces(char* szBuff);

void log_init_local_only(const char* component_name);
void log_init(const char* component_name);
void log_arguments(int argc, char *argv[]);
void log_add_file(const char* szFileName);
void log_disable();
void log_disable_stdout();
void log_enable_stdout();
void log_only_errors();
void log_enable_full();
void log_force_full_log();
void log_regular_mode();
int  log_is_errors_only();

void log_format_time(u32 miliseconds, char* szOutTime);
void log_line(const char* format, ...);
void log_line_forced_to_file(const char* format, ...);
void log_buffer(const u8* buffer, int size);
void log_buffer1(const u8* buffer, int size, int delim1);
void log_buffer2(const u8* buffer, int size, int delim1, int delim2);
void log_buffer3(const u8* buffer, int size, int delim1, int delim2, int delim3);
void log_buffer4(const u8* buffer, int size, int delim1, int delim2, int delim3, int delim4);
void log_buffer5(const u8* buffer, int size, int delim1, int delim2, int delim3, int delim4, int delim5);
void log_dword(const char* szText, u32 value);
void log_dword_bits(const char* szText, u32 value);
void log_always(const char* szText);

void log_softerror_and_alarm(const char* format, ...);
void log_error_and_alarm(const char* format, ...);
void log_line_watchdog(const char* format, ...);
void log_line_commands(const char* format, ...);

int check_licences();

long get_filesize(const char* szFileName);
key_t generate_msgqueue_key(int iMsgQueueId);

int is_semaphore_signaled_clear(sem_t* pSemaphore, const char* szSemName);
int is_semaphore_signaled_clear_logok(sem_t* pSemaphore, const char* szSemName, int iLogOk);
#ifdef __cplusplus
}  
#endif 
