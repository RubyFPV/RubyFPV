/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdarg.h>
#include <unistd.h>
#include <sys/file.h>
#include <time.h>
#include "base.h"
#include "config_file_names.h"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

u32 g_TimeNow = 0;
u32 g_TimeStart = 0;
u32 g_TimeNowMicros = 0;
u32 g_uLoopCounter = 0;

static int s_bootCount = -1;
static long long sStartTimeStamp_ms;
static long long sStartTimeStamp_micros;
u32 s_uTimeStartLogForCurrentProcess = 0;

static char sszComponentName[64] = {0};

#ifdef DISABLE_ALL_LOGS
static int s_logDisabled = 1;
#else
static int s_logDisabled = 0;
#endif

static int s_iLogForceFullMode = 0;
static int s_logUseService = 0;
static key_t s_logServiceKey = 0;
static int s_logServiceMessageQueue = -1;
static int s_logServiceAccessErrorCount = 0;

static int s_logDisabledStdout = 1;
static int s_logOnlyErrors = 0;

static int s_logAddTime = 1;
static char s_szAdditionalLogFile[128];

const u32 crc32_table[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3,	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de,	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,	0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5,	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,	0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940,	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,	0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};


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


u32 revert_word(u32 input)
{
   u32 out = (input & 0xFF) << 8;
   out = out | ((input>>8) & 0xFF);
   return out;
} 

void reset_counters(type_u32_couters* pCounters)
{
   if ( NULL == pCounters )
      return;
   pCounters->uCounter = 0;
   pCounters->uCounter2 = 0;
   pCounters->uTime = 0;
   pCounters->uTime2 = 0;
   pCounters->uValueMaxim = 0;
   pCounters->uValueMinim = 0;
   pCounters->uValueAverage = 0;
   pCounters->uValueMaximLocal = 0;
   pCounters->uValueMinimLocal = 0;
   pCounters->uValueAverageLocal = 0;
   pCounters->uValueNow = 0;
}

u32 base_compute_crc32(u8 *buf, int length)
{
   u8* p = buf;
   u32 crc;
   crc = ~0U;

   while (length--)
   {
      crc = crc32_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
   }
   return crc ^ ~0U;
} 

u8 base_compute_crc8(u8* pBuffer, int iLength)
{
   u8 uCrc = 0xFF;
   if ( NULL == pBuffer || iLength <= 0 )
      return uCrc;
   for ( int i = 0; i < iLength; i++)
      uCrc = s_crc_i2c_table[pBuffer[i] ^ uCrc];
   return uCrc;
}

int base_check_crc32(u8* pBuffer, int iLength)
{
   u32 crc = base_compute_crc32(pBuffer + sizeof(u32), iLength-sizeof(u32)); 
   u32* p = (u32*)pBuffer;
   if ( *p != crc )
      return 0;
   return 1;
}

void _init_timestamp_for_process()
{
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_BOOT_COUNT);
   FILE* fd = fopen(szFile, "r");
   if ( NULL != fd )
   {
      if ( 1 != fscanf(fd, "%d", &s_bootCount) )
      {
         s_bootCount = 0;
         log_line_forced_to_file("Failed to read boot count from file [%s] for PID %d", szFile, getpid());
      }
      fclose(fd);
   }
   else
      log_line_forced_to_file("Failed to access boot count file [%s] for PID %d", szFile, getpid());

   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_BOOT_TIMESTAMP);
   fd = fopen(szFile, "r");
   if ( NULL == fd )
   {
      log_line_forced_to_file("Failed to access boot timestamp file [%s] for PID %d", szFile, getpid());
      struct timespec t;
      clock_gettime(RUBY_HW_CLOCK_ID, &t);
      sStartTimeStamp_micros = t.tv_sec*1000LL*1000LL + t.tv_nsec/1000LL;
      sStartTimeStamp_ms = t.tv_sec*1000LL + t.tv_nsec/1000LL/1000LL;
      //struct timeval t;
      //gettimeofday(&t, NULL);
      //sStartTimeStamp_micros = t.tv_sec*1000LL*1000LL + t.tv_usec;
      //sStartTimeStamp_ms = t.tv_sec*1000LL + t.tv_usec/1000LL;
      int count = 0;
      while ( count<10 )
      {
         fd = fopen(szFile, "w");
         if ( NULL != fd )
         {
            fprintf(fd, "%lld\n", sStartTimeStamp_ms);
            fclose(fd);
            return;
         }
         else
            log_line_forced_to_file("Failed to write boot timestamp to file [%s] for PID %d", szFile, getpid());
         #ifdef HW_PLATFORM_RASPBERRY
         system("sudo mount -o remount,rw /");
         struct timespec to_sleep = { 0, (long int)(50*1000*1000) };
         clock_nanosleep(RUBY_HW_CLOCK_ID, 0, &to_sleep, NULL);
         #endif
         count++;
      }
      return;
   }
   else
   {
      if ( 1 != fscanf(fd, "%lld\n", &sStartTimeStamp_ms) )
         log_line_forced_to_file("Failed to read boot timestamp from file [%s] for PID %d", szFile, getpid());
      fclose(fd);
      sStartTimeStamp_micros = sStartTimeStamp_ms * 1000;
   }
}

void hardware_sleep_sec(u32 uSeconds)
{
   sleep(uSeconds);
}

void hardware_sleep_ms(u32 miliSeconds)
{
   struct timespec to_sleep = { 0, (long int)(miliSeconds*1000*1000) };
   clock_nanosleep(RUBY_HW_CLOCK_ID, 0, &to_sleep, NULL);
}

void hardware_sleep_micros(u32 microSeconds)
{
   struct timespec to_sleep = { 0, (long int)(microSeconds*1000) };
   clock_nanosleep(RUBY_HW_CLOCK_ID, 0, &to_sleep, NULL);
}

u32 get_current_timestamp_micros()
{
   struct timespec t;
   clock_gettime(RUBY_HW_CLOCK_ID, &t);
   long long lt = t.tv_sec*1000LL*1000LL + t.tv_nsec/1000LL;
   //struct timeval t;
   //gettimeofday(&t, NULL);
   //long long lt = t.tv_sec*1000LL*1000LL + t.tv_usec;
   return (lt - sStartTimeStamp_micros);
}

u32 get_current_timestamp_ms()
{
   struct timespec t;
   clock_gettime(RUBY_HW_CLOCK_ID, &t);
   long long lt = t.tv_sec*1000LL + t.tv_nsec/1000LL/1000LL;
   //struct timeval t;
   //gettimeofday(&t, NULL);
   //long long lt = t.tv_sec*1000LL + t.tv_usec/1000LL;
   return (lt - sStartTimeStamp_ms);
}

u32 get_current_timestamp_ms_tens()
{
   struct timespec t;
   clock_gettime(RUBY_HW_CLOCK_ID, &t);
   long long lt = t.tv_sec*10000LL + t.tv_nsec/1000LL/100LL;
   //struct timeval t;
   //gettimeofday(&t, NULL);
   //long long lt = t.tv_sec*1000LL + t.tv_usec/1000LL;
   return (lt - sStartTimeStamp_ms*10);
}

u32 get_boot_timestamp_ms()
{
   return sStartTimeStamp_ms;
}

int is_first_boot()
{
   if ( s_bootCount < 2 )
      return 1;
   return 0;
}

char* removeTrailingZero(char* szBuff)
{
   if ( (NULL == szBuff) || (0 == szBuff[0]) )
      return szBuff;
   int index = strlen(szBuff)-1;
   while ( index >= 0 )
   {
      if ( (szBuff[index] != '0') && (szBuff[index] != '.') )
         break;
      if ( szBuff[index] == '.' )
      {
         szBuff[index] = 0;
         break;
      }
      szBuff[index] = 0;
      index--;
   }
   return szBuff;
}

char* removeNewLines(char* szBuff)
{
   if ( (NULL == szBuff) || (0 == szBuff[0]) )
      return szBuff;

   removeTrailingNewLines(szBuff);

   int iLen = strlen(szBuff);
   for( int i=0; i<iLen; i++ )
   {
      if ( (szBuff[i] >= 10) && (szBuff[i] <= 14) )
         szBuff[i] = '*';
   }
   return szBuff;
}

char* removeTrailingNewLines(char* szBuff)
{
   if ( (NULL == szBuff) || (0 == szBuff[0]) )
      return szBuff;
   int iLen = strlen(szBuff);
   iLen--;
   while ( iLen >= 0 )
   {
      if ( (szBuff[iLen] >= 10) && (szBuff[iLen] <= 14) )
         szBuff[iLen] = 0;
      else
         break;
      iLen--;
   }
   return szBuff;
}

char* removeLeadingWhiteSpace(char* szBuff)
{
   if ( (NULL == szBuff) || (0 == szBuff[0]) )
      return szBuff;
   int iLen = strlen(szBuff);
   int i = 0;
   char* p = szBuff;
   while ( i < iLen )
   {
      if ( *p <= 32 )
         *p = ' ';
      else
         break;
      i++;
      p++;
   }
   return p;
}

char* replaceNewLinesToSpaces(char* szBuff)
{
   if ( (NULL == szBuff) || (0 == szBuff[0]) )
      return szBuff;
      
   removeTrailingNewLines(szBuff);

   int iLen = strlen(szBuff);
   for( int i=0; i<iLen; i++ )
   {
      if ( (szBuff[i] >= 10) && (szBuff[i] <= 14) )
         szBuff[i] = ' ';
   }
   return szBuff;

}

int _log_check_for_service_log_access()
{
   if ( 0 == s_logUseService )
      return 0;
   if ( s_logServiceMessageQueue >= 0 )
      return 1;

   if ( 0 == s_logServiceKey )
   {
      pid_t pid = getpid();
      pid_t ppid = getppid();
      log_line_forced_to_file("Generate a new key for accessing logger message queue, from PID: %d, parent PID: %d...", (int)pid, (int)ppid);
      s_logServiceKey = generate_msgqueue_key(LOGGER_MESSAGE_QUEUE_ID);
   }
   if ( 0 == s_logServiceKey )
   {
      log_line_forced_to_file("Failed to generate logger message queue key. Using regular log instead.");
      s_logUseService = 0;
      return 0;
   }

   s_logServiceMessageQueue = msgget(s_logServiceKey, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

   if ( s_logServiceMessageQueue < 0 )
   {
      if ( get_current_timestamp_ms() > s_uTimeStartLogForCurrentProcess + 5000 )
      {
         pid_t pid = getpid();
         pid_t ppid = getppid();
         s_logServiceAccessErrorCount++;
         if ( s_logServiceAccessErrorCount < 10 )
            log_softerror_and_alarm("Failed to access the logger service message queue (from PID: %d, parent PID: %d)", (int)pid, (int)ppid);
         else if ( s_logServiceAccessErrorCount == 10 )
         {
            log_softerror_and_alarm("Failed to access the logger service message queue. Using regular log instead.");
            log_line_forced_to_file("Failed to access the logger service message queue. Using regular log instead.");
            s_logUseService = 0;
         }
      }
      return 0;
   }
   else
   {
      log_line_forced_to_file("Got access to logger service. Using logger service for log. key id: 0x%X, msgqueue id: %d", s_logServiceKey, s_logServiceMessageQueue);
   }

   return 1;
}

int _log_service_entry(char* szTime, char* szBuff)
{
   if ( ! s_logDisabledStdout )
      printf("%s %s: %s\n", szTime, sszComponentName, szBuff);

   type_log_message_buffer msg;
   msg.type = 1;
   msg.text[0] = 0;

   strcpy(msg.text, "S");
   strcat(msg.text, szTime);
   strcat(msg.text, " ");
   strcat(msg.text, sszComponentName);
   strcat(msg.text, ": ");
   szBuff[MAX_SERVICE_LOG_ENTRY_LENGTH-3-strlen(msg.text)] = 0;
   strcat(msg.text, szBuff);

   if ( msgsnd(s_logServiceMessageQueue, &msg, strlen(msg.text)+1, IPC_NOWAIT) < 0 )
      return 0;
   return 1;
}


int _log_service_entry_error(char* szTime, char* szBuff)
{
   if ( ! s_logDisabledStdout )
      printf("%s %s: ERROR: %s\n", szTime, sszComponentName, szBuff);

   type_log_message_buffer msg;
   msg.type = 3;
   msg.text[0] = 0;

   strcpy(msg.text, "S");
   strcat(msg.text, szTime);
   strcat(msg.text, " ");
   strcat(msg.text, sszComponentName);
   strcat(msg.text, ": ERROR: ");
   szBuff[MAX_SERVICE_LOG_ENTRY_LENGTH-3-strlen(msg.text)] = 0;
   strcat(msg.text, szBuff);

   if ( msgsnd(s_logServiceMessageQueue, &msg, strlen(msg.text)+1, IPC_NOWAIT) < 0 )
      return 0;
   return 1;
}

int _log_service_entry_softerror(char* szTime, char* szBuff)
{
   if ( ! s_logDisabledStdout )
      printf("%s %s: SOFTERROR: %s\n", szTime, sszComponentName, szBuff);

   type_log_message_buffer msg;
   msg.type = 2;
   msg.text[0] = 0;

   strcpy(msg.text, "S");
   strcat(msg.text, szTime);
   strcat(msg.text, " ");
   strcat(msg.text, sszComponentName);
   strcat(msg.text, ": SOFTERROR: ");
   szBuff[MAX_SERVICE_LOG_ENTRY_LENGTH-3-strlen(msg.text)] = 0;
   strcat(msg.text, szBuff);

   if ( msgsnd(s_logServiceMessageQueue, &msg, strlen(msg.text)+1, IPC_NOWAIT) < 0 )
      return 0;
   return 1;
}

void log_init_local_only(const char* component_name)
{
   s_logServiceMessageQueue = -1;
   s_logServiceAccessErrorCount = 0;
   s_logUseService = 0;

   strcpy(sszComponentName, component_name);
   s_szAdditionalLogFile[0] = 0;
   _init_timestamp_for_process();
   s_uTimeStartLogForCurrentProcess = get_current_timestamp_ms();

   char szLogLine[256];
   pid_t pid = getpid();
   pid_t ppid = getppid();
   sprintf(szLogLine, "Starting (PID: %d, parent PID: %d)...", (int)pid, (int)ppid);
   log_line(szLogLine);
}

void log_init(const char* component_name)
{
   s_logServiceMessageQueue = -1;
   s_logServiceAccessErrorCount = 0;
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, LOG_USE_PROCESS);
   if( access(szFile, R_OK) != -1 )
      s_logUseService = 1;
   else
      s_logUseService = 0;

   strcpy(sszComponentName, component_name);
   s_szAdditionalLogFile[0] = 0;
   _init_timestamp_for_process();
   s_uTimeStartLogForCurrentProcess = get_current_timestamp_ms();
   
   _log_check_for_service_log_access();
   
   char szLogLine[256];
   pid_t pid = getpid();
   pid_t ppid = getppid();
   sprintf(szLogLine, "Starting (PID: %d, parent PID: %d)...", (int)pid, (int)ppid);
   log_line_forced_to_file(szLogLine);
   struct timespec ts;
   clock_getres(RUBY_HW_CLOCK_ID, &ts);
   log_line_forced_to_file("Current clock (id %d) resolution: %d sec, %d nanosec", RUBY_HW_CLOCK_ID, ts.tv_sec, ts.tv_nsec);
}

void log_arguments(int argc, char *argv[])
{
   char szHWPlatform[32];
   strcpy(szHWPlatform, "N/A");

   #if defined(HW_PLATFORM_OPENIPC_CAMERA)
   strcpy(szHWPlatform, "OPIPC");
   #elif defined(HW_PLATFORM_LINUX_GENERIC)
   strcpy(szHWPlatform, "Linux");
   #elif defined(HW_PLATFORM_RASPBERRY)
   strcpy(szHWPlatform, "PI");
   #elif defined(HW_PLATFORM_RADXA)
   strcpy(szHWPlatform, "RadxaZero3");
   #endif
   log_line_forced_to_file("Process version: %d.%d (b-%d) HW: %s", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR, SYSTEM_SW_BUILD_NUMBER, szHWPlatform);
   log_line_forced_to_file("Using logger service: %s", (s_logUseService!=0)?"yes":"no");
   if ( argc <= 0 )
   {
      log_line_forced_to_file("Executed with no arguments");
      return;
   }
   log_line_forced_to_file("Executed with %d arguments:", argc);
   for( int i=0; i<argc; i++ )
   {
      if ( NULL == argv[i] )
         log_line_forced_to_file("Arg %d: NULL", i);
      else
         log_line_forced_to_file("Arg %d: [%s]", i, argv[i]);
   }
}

void log_add_file(const char* szFileName)
{
   if ( NULL == szFileName )
      return;

   strncpy(s_szAdditionalLogFile, szFileName, sizeof(s_szAdditionalLogFile)/sizeof(s_szAdditionalLogFile[0]));
   s_szAdditionalLogFile[sizeof(s_szAdditionalLogFile)/sizeof(s_szAdditionalLogFile[0]) - 1] = 0;
   log_line("Starting additional log output to file: %s", s_szAdditionalLogFile);
}

void log_disable()
{
   if ( access("/tmp/debuglog", R_OK) != -1 )
   {
      log_line_forced_to_file("Setting the log to disabled overwritten by /tmp/debuglog flag.");
      s_logDisabled = 0;
      return;
   }
   s_logDisabled = 1;
}

void log_disable_stdout()
{
   s_logDisabledStdout = 1;
}

void log_enable_stdout()
{
   s_logDisabledStdout = 0;
}

void log_only_errors()
{
   if ( access("/tmp/debuglog", R_OK) != -1 )
   {
      log_line_forced_to_file("Setting the log level to errors only overwritten by /tmp/debuglog flag.");
      s_logOnlyErrors = 0;
      return;
   }
   log_line_forced_to_file("Setting the log level to errors only.");
   s_logOnlyErrors = 1;
}

void log_enable_full()
{
   log_line_forced_to_file("Setting the log level to full.");
   s_logOnlyErrors = 0;
}

void log_force_full_log()
{
   s_iLogForceFullMode = 1;
}

void log_regular_mode()
{
   s_iLogForceFullMode = 0;
}

int log_is_errors_only()
{
   return s_logOnlyErrors;
}

void _log_format_time_mstens(char* szOutTime)
{
   //u32 uMilisTens = get_current_timestamp_ms_tens();
   //sprintf(szOutTime, "%d-%d:%02d:%02d.%03d", s_bootCount, (int)(uMilisTens/1000/60/60/10), (int)(uMilisTens/1000/60/10)%60, (int)((uMilisTens/1000/10)%60), (int)((uMilisTens/10)%1000));

   u32 uTime = get_current_timestamp_ms();
   if ( uTime < g_TimeNow )
      uTime = get_current_timestamp_ms();
   g_TimeNow = uTime;
   //sprintf(szOutTime, "%d-%d:%02d:%02d.%03d", s_bootCount, (int)(g_TimeNow/1000/60/60), (int)(g_TimeNow/1000/60)%60, (int)((g_TimeNow/1000)%60), (int)(g_TimeNow%1000));
   log_format_time(g_TimeNow, szOutTime);
}


void log_format_time(u32 miliseconds, char* szOutTime)
{
   if ( NULL == szOutTime )
      return;
   sprintf(szOutTime, "%d-%d:%02d:%02d.%03d %03u", s_bootCount, (int)(miliseconds/1000/60/60), (int)(miliseconds/1000/60)%60, (int)((miliseconds/1000)%60), (int)(miliseconds%1000), g_uLoopCounter % 1000);
}

void log_line(const char* format, ...)
{
   if ( s_logDisabled || (s_logOnlyErrors && (!s_iLogForceFullMode)) )
      return;

   va_list args;
   va_start(args, format);

   char szTime[64];
   szTime[0] = 0;
   if ( s_logAddTime )
      _log_format_time_mstens(szTime);
 
   if ( _log_check_for_service_log_access() )
   {
      char szBuff[MAX_SERVICE_LOG_ENTRY_LENGTH];
      vsnprintf(szBuff,MAX_SERVICE_LOG_ENTRY_LENGTH-1, format, args);
      szBuff[MAX_SERVICE_LOG_ENTRY_LENGTH-1] = 0;
      _log_service_entry(szTime, szBuff);
      va_end(args);
      return;
   }

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_LOGS);
   strcat(szFile, LOG_FILE_SYSTEM);
   FILE* fd = fopen(szFile, "a+");
   //int lock = flock(fileno(fd), LOCK_EX);

   if ( 0 != s_szAdditionalLogFile[0] )
   {
      FILE* fdAux = fopen(s_szAdditionalLogFile, "a+");
      if ( NULL != fdAux )
      {
         fprintf(fdAux, "%s %s: ", szTime, sszComponentName);  
         fclose(fdAux);
      }
   }

   if ( ! s_logDisabledStdout )
      printf("%s %s: ", szTime, sszComponentName);
   if ( NULL != fd )
     fprintf(fd, "%s %s: ", szTime, sszComponentName);

   if ( 0 != s_szAdditionalLogFile[0] )
   {
      FILE* fdAux = fopen(s_szAdditionalLogFile, "a+");
      if ( NULL != fdAux )
      {
         vfprintf(fdAux, format, args);
         fclose(fdAux);
      }
   }

   if ( NULL != fd )
      vfprintf(fd, format, args);
   if ( ! s_logDisabledStdout )
      vprintf(format, args);

   if ( 0 != s_szAdditionalLogFile[0] )
   {
      FILE* fdAux = fopen(s_szAdditionalLogFile, "a+");
      if ( NULL != fdAux )
      {
         fprintf(fdAux, "\n");
         fclose(fdAux);
      }
   }
   va_end(args);

   if ( ! s_logDisabledStdout )
      printf("\n");
   if ( NULL != fd )
     fprintf(fd, "\n");  

   //if ( 0 == lock )
   //   flock(fileno(fd), LOCK_UN);
   if ( NULL != fd )
      fclose(fd);
}


void log_line_forced_to_file(const char* format, ...)
{
   va_list args;
   va_start(args, format);

   char szTime[64];
   szTime[0] = 0;
   if ( s_logAddTime )
      _log_format_time_mstens(szTime);

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_LOGS);
   strcat(szFile, LOG_FILE_SYSTEM);
   FILE* fd = fopen(szFile, "a+");
   //int lock = flock(fileno(fd), LOCK_EX);

   if ( 0 != s_szAdditionalLogFile[0] )
   {
      FILE* fdAux = fopen(s_szAdditionalLogFile, "a+");
      if ( NULL != fdAux )
      {
         fprintf(fdAux, "%s(F) %s: ", szTime, sszComponentName);  
         fclose(fdAux);
      }
   }

   if ( ! s_logDisabledStdout )
      printf("%s(F) %s: ", szTime, sszComponentName);
   if ( NULL != fd )
     fprintf(fd, "%s(F) %s: ", szTime, sszComponentName);

   if ( 0 != s_szAdditionalLogFile[0] )
   {
      FILE* fdAux = fopen(s_szAdditionalLogFile, "a+");
      if ( NULL != fdAux )
      {
         vfprintf(fdAux, format, args);
         fclose(fdAux);
      }
   }

   if ( NULL != fd )
      vfprintf(fd, format, args);
   if ( ! s_logDisabledStdout )
      vprintf(format, args);

   if ( 0 != s_szAdditionalLogFile[0] )
   {
      FILE* fdAux = fopen(s_szAdditionalLogFile, "a+");
      if ( NULL != fdAux )
      {
         fprintf(fdAux, "\n");
         fclose(fdAux);
      }
   }

   if ( ! s_logDisabledStdout )
      printf("\n");
   if ( NULL != fd )
     fprintf(fd, "\n");  

   va_end(args);
   //if ( 0 == lock )
   //   flock(fileno(fd), LOCK_UN);
   if ( NULL != fd )
      fclose(fd);
}

void log_line_watchdog(const char* format, ...)
{
   if ( s_logDisabled || (s_logOnlyErrors && (!s_iLogForceFullMode)) )
      return;

   va_list args;
   va_start(args, format);

   char szTime[64];
   szTime[0] = 0;
   if ( s_logAddTime )
      _log_format_time_mstens(szTime);
 
   if ( _log_check_for_service_log_access() )
   {
      char szBuff[MAX_SERVICE_LOG_ENTRY_LENGTH];
      vsnprintf(szBuff, MAX_SERVICE_LOG_ENTRY_LENGTH-1, format, args);
      szBuff[MAX_SERVICE_LOG_ENTRY_LENGTH-1] = 0;
      _log_service_entry(szTime, szBuff);
      va_end(args);
      return;
   }

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_LOGS);
   strcat(szFile, LOG_FILE_SYSTEM);

   char szFile2[MAX_FILE_PATH_SIZE];
   strcpy(szFile2, FOLDER_LOGS);
   strcat(szFile2, LOG_FILE_WATCHDOG);

   FILE* fd = fopen(szFile, "a+");
   FILE* fd2 = fopen(szFile2, "a+");
   //int lock = flock(fileno(fd), LOCK_EX);

   if ( ! s_logDisabledStdout )
      printf("%s %s: ", szTime, sszComponentName);
   if ( NULL != fd )
     fprintf(fd, "%s %s: ", szTime, sszComponentName);  
   if ( NULL != fd2 )
     fprintf(fd2, "%s %s: ", szTime, sszComponentName);  

   if ( NULL != fd )
      vfprintf(fd, format, args);
   if ( NULL != fd2 )
      vfprintf(fd2, format, args);
   if ( ! s_logDisabledStdout )
      vprintf(format, args);

   if ( ! s_logDisabledStdout )
      printf("\n");
   if ( NULL != fd )
     fprintf(fd, "\n");  
   if ( NULL != fd2 )
     fprintf(fd2, "\n");  

   va_end(args);
   //if ( 0 == lock )
   //   flock(fileno(fd), LOCK_UN);
   if ( NULL != fd )
      fclose(fd);
   if ( NULL != fd2 )
      fclose(fd2);
}


void log_line_commands(const char* format, ...)
{
   if ( s_logDisabled || (s_logOnlyErrors && (!s_iLogForceFullMode)) )
      return;

   va_list args;
   va_start(args, format);

   char szTime[64];
   szTime[0] = 0;
   if ( s_logAddTime )
      _log_format_time_mstens(szTime);
 
   if ( _log_check_for_service_log_access() )
   {
      char szBuff[MAX_SERVICE_LOG_ENTRY_LENGTH];
      vsnprintf(szBuff, MAX_SERVICE_LOG_ENTRY_LENGTH-1, format, args);
      szBuff[MAX_SERVICE_LOG_ENTRY_LENGTH-1] = 0;
      _log_service_entry(szTime, szBuff);
      va_end(args);
      return;
   }

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_LOGS);
   strcat(szFile, LOG_FILE_SYSTEM);

   char szFile2[MAX_FILE_PATH_SIZE];
   strcpy(szFile2, FOLDER_LOGS);
   strcat(szFile2, LOG_FILE_COMMANDS);

   FILE* fd = fopen(szFile, "a+");
   FILE* fd2 = fopen(szFile2, "a+");
   //int lock = flock(fileno(fd), LOCK_EX);

   if ( ! s_logDisabledStdout )
      printf("%s %s: ", szTime, sszComponentName);
   if ( NULL != fd )
     fprintf(fd, "%s %s: ", szTime, sszComponentName);  
   if ( NULL != fd2 )
     fprintf(fd2, "%s %s: ", szTime, sszComponentName);  

   if ( NULL != fd )
      vfprintf(fd, format, args);
   if ( NULL != fd2 )
      vfprintf(fd2, format, args);
   if ( ! s_logDisabledStdout )
      vprintf(format, args);

   if ( ! s_logDisabledStdout )
      printf("\n");
   if ( NULL != fd )
     fprintf(fd, "\n");  
   if ( NULL != fd2 )
     fprintf(fd2, "\n");  

   va_end(args);
   //if ( 0 == lock )
   //   flock(fileno(fd), LOCK_UN);
   if ( NULL != fd )
      fclose(fd);
   if ( NULL != fd2 )
      fclose(fd2);
}

void log_buffer(const u8* buffer, int size)
{
   log_buffer3(buffer, size, -1, -1, -1);
}

void log_buffer1(const u8* buffer, int size, int delim1)
{
   log_buffer3(buffer, size, delim1, -1, -1);
}

void log_buffer2(const u8* buffer, int size, int delim1, int delim2)
{
   log_buffer3(buffer, size, delim1, delim2, -1);
}

void log_buffer3(const u8* buffer, int size, int delim1, int delim2, int delim3)
{
   log_buffer5(buffer, size, delim1, delim2, delim3, -1, -1);
}

void log_buffer4(const u8* buffer, int size, int delim1, int delim2, int delim3, int delim4)
{
   log_buffer5(buffer, size, delim1, delim2, delim3, delim4, -1);
}

void log_buffer5(const u8* buffer, int size, int delim1, int delim2, int delim3, int delim4, int delim5)
{
   if ( s_logDisabled || (s_logOnlyErrors && (!s_iLogForceFullMode)) )
      return;

   char szTime[64];
   szTime[0] = 0;
   if ( s_logAddTime )
      _log_format_time_mstens(szTime);

   if ( _log_check_for_service_log_access() )
   {
      char szBuff[MAX_SERVICE_LOG_ENTRY_LENGTH];
      char szTmp[16];
      snprintf(szBuff, MAX_SERVICE_LOG_ENTRY_LENGTH-1, "Buff-len %d: [", size);
      int len = strlen(szBuff);

      int i=0;
      for( i=0; i<size-1; i++ )
      {
         sprintf(szTmp, "%x", buffer[i]);
         strcat(szBuff, szTmp);
         len += strlen(szTmp);
    
         int bBreak = 0;
         if ( delim1 > 0 )
         if ( (i+1) == delim1 )
            bBreak = 1;
         if ( delim1 > 0 && delim2 > 0 )
         if ( (i+1) == delim1+delim2 )
            bBreak = 1;
         if ( delim1 > 0 && delim2 > 0 && delim3 > 0)
         if ( (i+1) == delim1+delim2+delim3 )
            bBreak = 1;
         if ( delim1 > 0 && delim2 > 0 && delim3 > 0 && delim4 > 0)
         if ( (i+1) == delim1+delim2+delim3+delim4 )
            bBreak = 1;
         if ( delim1 > 0 && delim2 > 0 && delim3 > 0 && delim4 > 0 && delim5 > 0)
         if ( (i+1) == delim1+delim2+delim3+delim4+delim5 )
            bBreak = 1;

         if ( bBreak )
         {
            strcat(szBuff, "] - [");
            len += 5;
            continue;
         }

         if ( (i%8) == 7 )
         {
            strcat(szBuff, " - ");
            len += 3;
         }
         else
         {
            strcat(szBuff, ",");
            len++;
         }
         if ( len > MAX_SERVICE_LOG_ENTRY_LENGTH-16 )
         {
            strcat(szBuff, " ...");
            _log_service_entry(szTime, szBuff);
            snprintf(szBuff, MAX_SERVICE_LOG_ENTRY_LENGTH-1, "Buff (cont): [... ");
            len = strlen(szBuff);
         }
      }
      sprintf(szTmp,"%x]\n", buffer[size-1]);  
      strcat(szBuff, szTmp);
      _log_service_entry(szTime, szBuff);
      return;
   }

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_LOGS);
   strcat(szFile, LOG_FILE_SYSTEM);

   FILE* fd = fopen(szFile, "a+");
   //int lock = flock(fileno(fd), LOCK_EX);

   if ( ! s_logDisabledStdout )
      printf("Len %d: [", size);
   if ( NULL != fd )
     fprintf(fd, "Len %d: [", size);  

   int i=0;
   for( i=0; i<size-1; i++ )
   {
      if ( ! s_logDisabledStdout )
         printf("%x", buffer[i]);
      if ( NULL != fd )
        fprintf(fd, "%x", buffer[i]);

      int bBreak = 0;
      if ( delim1 > 0 )
      if ( (i+1) == delim1 )
         bBreak = 1;
      if ( delim1 > 0 && delim2 > 0 )
      if ( (i+1) == delim1+delim2 )
         bBreak = 1;
      if ( delim1 > 0 && delim2 > 0 && delim3 > 0)
      if ( (i+1) == delim1+delim2+delim3 )
         bBreak = 1;
      if ( delim1 > 0 && delim2 > 0 && delim3 > 0 && delim4 > 0)
      if ( (i+1) == delim1+delim2+delim3+delim4 )
         bBreak = 1;
      if ( delim1 > 0 && delim2 > 0 && delim3 > 0 && delim4 > 0 && delim5 > 0)
      if ( (i+1) == delim1+delim2+delim3+delim4+delim5 )
         bBreak = 1;
      if ( bBreak )
      {
         if ( ! s_logDisabledStdout )
            printf("] - [");
         if ( NULL != fd )
           fprintf(fd, "] - [");
         continue;
      }

      if ( (i%8) == 7 )
      {
         if ( ! s_logDisabledStdout )
            printf(" - ");
         if ( NULL != fd )
           fprintf(fd, " - ");
      }
      else
      {
         if ( ! s_logDisabledStdout )
            printf(",");
         if ( NULL != fd )
           fprintf(fd, ",");
      }
   }
   if ( ! s_logDisabledStdout )
      printf("%x]\n", buffer[size-1]);
   if ( NULL != fd )
     fprintf(fd, "%x]\n", buffer[size-1]);  

   //if ( 0 == lock )
   //   flock(fileno(fd), LOCK_UN);

   if ( NULL != fd )
      fclose(fd);
}

void log_dword(const char* szText, u32 value)
{
   if ( s_logDisabled || (s_logOnlyErrors && (!s_iLogForceFullMode)) )
      return;

   char szTime[64];
   szTime[0] = 0;
   if ( s_logAddTime )
      _log_format_time_mstens(szTime);
 
   if ( _log_check_for_service_log_access() )
   {
      char szBuff[MAX_SERVICE_LOG_ENTRY_LENGTH];
      snprintf(szBuff, MAX_SERVICE_LOG_ENTRY_LENGTH-1, "%s %u", szText, value);
      szBuff[MAX_SERVICE_LOG_ENTRY_LENGTH-1] = 0;
      _log_service_entry(szTime, szBuff);
      return;
   }

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_LOGS);
   strcat(szFile, LOG_FILE_SYSTEM);
   FILE* fd = fopen(szFile, "a+");
   
   if ( ! s_logDisabledStdout )
      printf("%s %s: ", szTime, sszComponentName);
   if ( NULL != fd )
     fprintf(fd, "%s %s: ", szTime, sszComponentName);  

   
   if ( NULL != fd )
   {   fprintf(fd, szText); fprintf(fd, ": "); }
   if ( ! s_logDisabledStdout )
   {   printf(szText); printf(": "); }

   for( int i=31; i>=0; i-- )
   {
      int bit = (value>>i) & 0x01;
      if ( NULL != fd )
         fprintf(fd, "%d", bit);
      if ( ! s_logDisabledStdout )
         printf("%d", bit);
      if ( (i % 4) == 0 )
      {
         if ( NULL != fd )
            fprintf(fd, " ");
         if ( ! s_logDisabledStdout )
            printf(" ");
      }
   }
   if ( ! s_logDisabledStdout )
      printf("\n");
   if ( NULL != fd )
     fprintf(fd, "\n");  

   if ( NULL != fd )
      fclose(fd);
}

void log_dword_bits(const char* szText, u32 value)
{
   if ( s_logDisabled || (s_logOnlyErrors && (!s_iLogForceFullMode)) )
      return;

   char szTime[64];
   szTime[0] = 0;
   if ( s_logAddTime )
      _log_format_time_mstens(szTime);
 
   if ( _log_check_for_service_log_access() )
   {
      char szBuff[MAX_SERVICE_LOG_ENTRY_LENGTH];
      snprintf(szBuff, MAX_SERVICE_LOG_ENTRY_LENGTH-1, "%s %u", szText, value);
      szBuff[MAX_SERVICE_LOG_ENTRY_LENGTH-1] = 0;
      _log_service_entry(szTime, szBuff);
      return;
   }

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_LOGS);
   strcat(szFile, LOG_FILE_SYSTEM);
   FILE* fd = fopen(szFile, "a+");
   
   if ( ! s_logDisabledStdout )
      printf("%s %s: ", szTime, sszComponentName);
   if ( NULL != fd )
     fprintf(fd, "%s %s: ", szTime, sszComponentName);  

   
   if ( NULL != fd )
   {
       fprintf(fd, szText);
       fprintf(fd, ": ");
   }
   if ( ! s_logDisabledStdout )
   {
      printf(szText);
      printf(": ");
   }

   for( int i=31; i>=0; i-- )
   {
      int bit = (value>>i) & 0x01;
      if ( NULL != fd )
         fprintf(fd, "%d", bit);
      if ( ! s_logDisabledStdout )
         printf("%d", bit);
      if ( (i % 4) == 0 )
      {
         if ( NULL != fd )
            fprintf(fd, " ");
         if ( ! s_logDisabledStdout )
            printf(" ");
      }
   }
   if ( ! s_logDisabledStdout )
      printf("\n");
   if ( NULL != fd )
     fprintf(fd, "\n");  

   if ( NULL != fd )
      fclose(fd);
}

void log_always(const char* szText)
{
   char szTime[64];
   szTime[0] = 0;
   if ( s_logAddTime )
      _log_format_time_mstens(szTime);
 
   if ( _log_check_for_service_log_access() )
   {
      char szBuff[MAX_SERVICE_LOG_ENTRY_LENGTH];
      szBuff[0] = 0;
      if ( (NULL != szText) && (0 != szText[0]) )
         strncpy(szBuff, szText, MAX_SERVICE_LOG_ENTRY_LENGTH-1);
      else
         strncpy(szBuff, "NoLog", MAX_SERVICE_LOG_ENTRY_LENGTH-1);
      szBuff[MAX_SERVICE_LOG_ENTRY_LENGTH-1] = 0;
      _log_service_entry(szTime, szBuff);
      return;
   }

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_LOGS);
   strcat(szFile, LOG_FILE_SYSTEM);
   FILE* fd = fopen(szFile, "a+");
   //int lock = flock(fileno(fd), LOCK_EX);

   if ( 0 != s_szAdditionalLogFile[0] )
   {
      FILE* fdAux = fopen(s_szAdditionalLogFile, "a+");
      if ( NULL != fdAux )
      {
         fprintf(fdAux, "%s %s: ", szTime, sszComponentName);  
         fclose(fdAux);
      }
   }

   if ( ! s_logDisabledStdout )
      printf("%s %s: ", szTime, sszComponentName);
   if ( NULL != fd )
     fprintf(fd, "%s %s: ", szTime, sszComponentName);

   if ( 0 != s_szAdditionalLogFile[0] )
   {
      FILE* fdAux = fopen(s_szAdditionalLogFile, "a+");
      if ( NULL != fdAux )
      {
         if ( (NULL != szText) && (0 != szText[0]) )
            fprintf(fdAux, "%s", szText);
         else
            fprintf(fdAux, "NoLog");
         fclose(fdAux);
      }
   }

   if ( NULL != fd )
   {
      if ( (NULL != szText) && (0 != szText[0]) )
         fprintf(fd, "%s", szText);
      else
         fprintf(fd, "NoLog");
   }
   if ( ! s_logDisabledStdout )
   {
      if ( (NULL != szText) && (0 != szText[0]) )
         printf("%s", szText);
      else
         printf("NoLog");
   }
   if ( 0 != s_szAdditionalLogFile[0] )
   {
      FILE* fdAux = fopen(s_szAdditionalLogFile, "a+");
      if ( NULL != fdAux )
      {
         fprintf(fdAux, "\n");
         fclose(fdAux);
      }
   }

   if ( ! s_logDisabledStdout )
      printf("\n");
   if ( NULL != fd )
     fprintf(fd, "\n");  

   //if ( 0 == lock )
   //   flock(fileno(fd), LOCK_UN);
   if ( NULL != fd )
      fclose(fd);
}

void log_error_and_alarm(const char* format, ...)
{
   //hardware_led_red_set_blinking_fast(2000);

   if ( s_logDisabled )
      return;

   va_list args;
   va_start(args, format);

   char szTime[64];
   szTime[0] = 0;
   if ( s_logAddTime )
      _log_format_time_mstens(szTime);

   if ( _log_check_for_service_log_access() )
   {
      char szBuff[MAX_SERVICE_LOG_ENTRY_LENGTH];
      vsnprintf(szBuff, MAX_SERVICE_LOG_ENTRY_LENGTH-1, format, args);
      szBuff[MAX_SERVICE_LOG_ENTRY_LENGTH-1] = 0;
      _log_service_entry_error(szTime, szBuff);
      va_end(args);
      return;
   }

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_LOGS);
   strcat(szFile, LOG_FILE_SYSTEM);

   char szFile2[MAX_FILE_PATH_SIZE];
   strcpy(szFile2, FOLDER_LOGS);
   strcat(szFile2, LOG_FILE_ERRORS);

   FILE* fd = fopen(szFile, "a+");
   FILE* fd2 = fopen(szFile2, "a+");
   //int lock = flock(fileno(fd), LOCK_EX);
   //int lock2 = flock(fileno(fd2), LOCK_EX);
 
   if ( 0 != s_szAdditionalLogFile[0] )
   {
      FILE* fdAux = fopen(s_szAdditionalLogFile, "a+");
      if ( NULL != fdAux )
      {
         fprintf(fdAux, "%s %s: ", szTime, sszComponentName);
         fclose(fdAux);
      }
   }

   if ( ! s_logDisabledStdout )
      printf("%s %s: ERROR: ", szTime, sszComponentName);
   if ( NULL != fd )
     fprintf(fd, "%s %s: ERROR: ", szTime, sszComponentName);  
   if ( NULL != fd2 )
     fprintf(fd2, "%s %s: ERROR: ", szTime, sszComponentName);  

   if ( 0 != s_szAdditionalLogFile[0] )
   {
      FILE* fdAux = fopen(s_szAdditionalLogFile, "a+");
      if ( NULL != fdAux )
      {
         vfprintf(fdAux, format, args);
         fclose(fdAux);
      }
   }

   if ( ! s_logDisabledStdout )
      vprintf(format, args);
   if ( NULL != fd )
      vfprintf(fd, format, args);
   if ( NULL != fd2 )
      vfprintf(fd2, format, args);

   if ( 0 != s_szAdditionalLogFile[0] )
   {
      FILE* fdAux = fopen(s_szAdditionalLogFile, "a+");
      if ( NULL != fdAux )
      {
         fprintf(fdAux, "\n");
         fclose(fdAux);
      }
   }

   if ( ! s_logDisabledStdout )
      printf("\n");
   if ( NULL != fd )
     fprintf(fd, "\n");  
   if ( NULL != fd2 )
     fprintf(fd2, "\n");  

   va_end(args);
   //if ( 0 == lock )
   //   flock(fileno(fd), LOCK_UN);
   //if ( 0 == lock2 )
   //   flock(fileno(fd2), LOCK_UN);

   if ( NULL != fd )
      fclose(fd);
   if ( NULL != fd2 )
      fclose(fd2);
}

void log_softerror_and_alarm(const char* format, ...)
{
   //hardware_led_red_set_blinking(2000);

   if ( s_logDisabled )
      return;

   va_list args;
   va_start(args, format);

   char szTime[64];
   szTime[0] = 0;
   if ( s_logAddTime )
      _log_format_time_mstens(szTime);

   if ( _log_check_for_service_log_access() )
   {
      char szBuff[MAX_SERVICE_LOG_ENTRY_LENGTH];
      vsnprintf(szBuff, MAX_SERVICE_LOG_ENTRY_LENGTH-1, format, args);
      szBuff[MAX_SERVICE_LOG_ENTRY_LENGTH-1] = 0;
      _log_service_entry_softerror(szTime, szBuff);
      va_end(args);
      return;
   }

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_LOGS);
   strcat(szFile, LOG_FILE_SYSTEM);

   char szFile2[MAX_FILE_PATH_SIZE];
   strcpy(szFile2, FOLDER_LOGS);
   strcat(szFile2, LOG_FILE_ERRORS_SOFT);

   FILE* fd = fopen(szFile, "a+");
   FILE* fd2 = fopen(szFile2, "a+");
   //int lock = flock(fileno(fd), LOCK_EX);
   //int lock2 = flock(fileno(fd2), LOCK_EX);
 
   if ( 0 != s_szAdditionalLogFile[0] )
   {
      FILE* fdAux = fopen(s_szAdditionalLogFile, "a+");
      if ( NULL != fdAux )
      {
         fprintf(fdAux, "%s %s: ", szTime, sszComponentName);
         fclose(fdAux);
      }
   }

   if ( ! s_logDisabledStdout )
      printf("%s %s: SOFT_ERROR: ", szTime, sszComponentName);
   if ( NULL != fd )
     fprintf(fd, "%s %s: SOFT_ERROR: ", szTime, sszComponentName);  
   if ( NULL != fd2 )
     fprintf(fd2, "%s %s: SOFT_ERROR: ", szTime, sszComponentName);  

   if ( 0 != s_szAdditionalLogFile[0] )
   {
      FILE* fdAux = fopen(s_szAdditionalLogFile, "a+");
      if ( NULL != fdAux )
      {
         vfprintf(fdAux, format, args);
         fclose(fdAux);
      }
   }

   if ( ! s_logDisabledStdout )
      vprintf(format, args);
   if ( NULL != fd )
      vfprintf(fd, format, args);
   if ( NULL != fd2 )
      vfprintf(fd2, format, args);

   if ( 0 != s_szAdditionalLogFile[0] )
   {
      FILE* fdAux = fopen(s_szAdditionalLogFile, "a+");
      if ( NULL != fdAux )
      {
         fprintf(fdAux, "\n");  
         fclose(fdAux);
      }
   }

   if ( ! s_logDisabledStdout )
      printf("\n");
   if ( NULL != fd )
     fprintf(fd, "\n");  
   if ( NULL != fd2 )
     fprintf(fd2, "\n");  

   va_end(args);
   //if ( 0 == lock )
   //   flock(fileno(fd), LOCK_UN);
   //if ( 0 == lock2 )
   //   flock(fileno(fd2), LOCK_UN);

   if ( NULL != fd )
      fclose(fd);
   if ( NULL != fd2 )
      fclose(fd2);
}


int check_licences()
{
   #ifdef FEATURE_CHECK_LICENCES
   
   char szBuff[1024];
   char szIds[32][64];
   int iCountIds = 0;
   int bOk = 0;

   strcpy(szIds[iCountIds], "000000005f5c90dd");
   iCountIds++;
   strcpy(szIds[iCountIds], "000000002ddb1a99");
   iCountIds++;

   strcpy(szIds[iCountIds], "100000007304c2f0");
   iCountIds++;

   hw_execute_bash_command_silent("cat /sys/firmware/devicetree/base/serial-number", szBuff);

   for( int i=0; i<strlen(szBuff); i++ )
      if ( szBuff[i] == 10 || szBuff[i] == 13 )
         szBuff[i] = 0;

   for( int i=0; i<iCountIds; i++ )
   {
      if ( 0 == strcmp(szIds[i], szBuff) )
      {
         bOk = 1;
         break;
      }
   }
   if ( bOk )
      return 1;
   //printf("L: %s\n\n", szBuff);
   for( int i=0; i<14; i++ )
      hardware_sleep_ms(800);
   hw_execute_bash_command("shutdown now", NULL);
   #endif
   return 1;
}

long get_filesize(const char* szFileName)
{
   long lSize = -1;
   FILE* fd = fopen(szFileName, "rb");
   if ( NULL != fd )
   {
      fseek(fd, 0, SEEK_END);
      lSize = ftell(fd);
      fseek(fd, 0, SEEK_SET);
      fclose(fd);
   }
   return lSize;
}

key_t generate_msgqueue_key(int iMsgQueueId)
{
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_BINARIES);
   strcat(szFile, "ruby_logger");
   if ( access(szFile, R_OK) == -1 )
   {
      if ( access(szFile, R_OK) != -1 )
         strcpy(szFile, "/tmp/ruby_start");
      else if ( access("/tmp/debug", R_OK) != -1 )
         strcpy(szFile, "/tmp/debug");
   }
   key_t key = ftok(szFile, iMsgQueueId);

   log_line_forced_to_file("Generated message queue key 0x%X for msg queue id %d, from file [%s]", key, iMsgQueueId, szFile);
   return key;
}

int is_semaphore_signaled_clear(sem_t* pSemaphore, const char* szSemName)
{
   return is_semaphore_signaled_clear_logok(pSemaphore, szSemName, 1);
}

int is_semaphore_signaled_clear_logok(sem_t* pSemaphore, const char* szSemName, int iLogOk)
{
   if ( NULL == pSemaphore )
   {
      if ( NULL != szSemName )
         log_softerror_and_alarm("Tried to query NULL semaphore, sem name: [%s]", szSemName);
      else
         log_softerror_and_alarm("Tried to query NULL semaphore, sem name: NULL");
      return 0;
   }
   //int iSemValue = 0;
   //if ( 0 == sem_getvalue(pSemaphore, &iSemValue) )
   //if ( iSemValue > 0 )
   if ( 0 == sem_trywait(pSemaphore) )
   {
      int iSemValue = 0;
      if ( 0 != sem_getvalue(pSemaphore, &iSemValue) )
      {
         if ( NULL != szSemName )
            log_softerror_and_alarm("Failed to get sem value after trywait, sem name: [%s]", szSemName);
         else
            log_softerror_and_alarm("Failed to get sem value after trywait, sem name: NULL");
         iSemValue = 0;
      }
      int iCount = iSemValue;
      while ( (iCount > 0) && (0 == sem_trywait(pSemaphore)) )
      {
         iCount--;
      }
      if ( iLogOk )
      {
         if ( NULL != szSemName )
            log_line("Semaphore [%s] is signaled (value %d) and cleared.", szSemName, iSemValue);
         else
            log_line("Semaphore NULL is signaled (value %d) and cleared.", iSemValue);
      }
      return iSemValue+1;
   }
   return 0;
}
