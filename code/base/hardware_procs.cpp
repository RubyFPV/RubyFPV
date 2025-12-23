#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <ctype.h>
#include <pthread.h>
#include <sched.h>
#include <errno.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <poll.h>

#include "base.h"
#include "config.h"
#include "hardware_procs.h"
#include "hardware.h"

extern "C" {
#include "../common/string_utils.h"
}


static bool s_bHwDbgSortOnCores = false;
static bool s_bHwDbgFilterAirOnly = false;
static bool s_bHwDbgFilterGroundOnly = false;
static bool s_bHwDbgShowAir = true;
static bool s_bHwDbgShowGround = true;


char* _get_short_proc_name(const char* szProcessName, const char* szParams)
{
   static char s_szShortProcName[32];
   strncpy(s_szShortProcName, szProcessName, 31);
   s_szShortProcName[31] = 0;

   int iProgLen = 13;
   int iParamLen = 7;

   int iLen = strlen(s_szShortProcName);
   for( int i=iLen; i<iProgLen+iParamLen; i++ )
       s_szShortProcName[i] = ' ';
   s_szShortProcName[iProgLen+iParamLen] = 0;

   if ( ( NULL != szParams) && (0 != szParams[0]) && (0 != szParams[1]) )
   {
      for( int i=1; i<iParamLen; i++ )
      {
         if ( szParams[i-1] == 0 )
            break;
         s_szShortProcName[iProgLen+i] = szParams[i-1];
      }
      s_szShortProcName[iProgLen+iParamLen] = 0;
   }
   return s_szShortProcName;
}

void _print_proc_line(const char* szProcessName, const char* szParams, int iPID, int iTID, char chState, int iCPU, int iSchClass, int iNice, int iRTPrio, int iRealPrio, const char* szAffinity, int iTaskCount, int iCoreFilter)
{
   if ( 0 == iPID )
   {
      printf("%s  [not_found]\n", _get_short_proc_name(szProcessName, ""));
      return;
   }
   if ( 0 == iTID )
   {
      printf("%s  %d  [no_tasks]\n", _get_short_proc_name(szProcessName, ""), iPID);
      return;
   }
   char* pName = _get_short_proc_name(szProcessName, szParams);
   if ( (iPID != iTID) && (-1 == iCoreFilter) )
   {
      pName = _get_short_proc_name("`--", szParams);
      if ( iTaskCount > 0 )
         pName[3] = '1' + iTaskCount;
   }

   char szClass[32];
   sprintf(szClass, "[%d]", iSchClass);
   if ( iSchClass == SCHED_RR )
      strcpy(szClass, "-RR-");
   if ( iSchClass == SCHED_FIFO )
      strcpy(szClass, "FIFO");
   if ( iSchClass == SCHED_OTHER )
      strcpy(szClass, "OTHR");

   char szPostfix[32];
   szPostfix[0] = 0;
   if ( iRealPrio < 0 )
      strcpy(szPostfix, "<- RT ");
   else
      strcpy(szPostfix, "     ");
   if ( (iPID == iTID) || (-1 != iCoreFilter) )
   {
      strcat(szPostfix, "  ");
      strcat(szPostfix, pName);
   }
   if ( (iPID == iTID) && (-1 == iCoreFilter) )
      printf("%s %c   %d\t %d\t %d %s\t  %s    %d\t %d\t %d %s\n",
         pName, chState,
         iPID, iTID, iCPU, szAffinity,
         szClass, iNice, iRTPrio, iRealPrio+100, szPostfix);
   else
      printf("%s %c   `-\t %d\t %d %s\t  %s    %d\t %d\t %d %s\n",
         pName, chState,
         iTID, iCPU, szAffinity,
         szClass, iNice, iRTPrio, iRealPrio+100, szPostfix);
}

void _log_task(char* szProgramName, int iPID, int iTID, int iTaskCount, int iCoreFilter)
{
   if ( (iPID <= 0) || (iTID <= 0) )
      return;

   char szFile[MAX_FILE_PATH_SIZE];
   sprintf(szFile, "/proc/%d/stat", iTID);
   
   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
   {
      if ( -1 == iCoreFilter )
         printf("[Can't read TID stat file: %s]\n", szFile);
      return;
   }
   
   char szTmp[64];
   int iTmp = 0;
   long int ilTmp = 0;
   unsigned int uTmp = 0;
   long unsigned int ulTmp = 0;
   unsigned long long ullTmp = 0;

   char chState = 0;
   long int iRealPriority = 0;
   long int iNice = 0;
   long int iThreads = 0;
   int iProcessor = 0;
   unsigned int uRTPriority = 0;
   unsigned int uSchPolicy = 0;

   //1-10
   fscanf(fd, "%d %s %c %d %d", &iTmp, szTmp, &chState, &iTmp, &iTmp);
   fscanf(fd, "%d %d %d %u %lu", &iTmp, &iTmp, &iTmp, &uTmp, &ulTmp);
   //11-20
   fscanf(fd, "%lu %lu %lu %lu %lu", &ulTmp, &ulTmp, &ulTmp, &ulTmp, &ulTmp);
   fscanf(fd, "%lu %lu %ld %ld %ld", &ulTmp, &ulTmp, &iRealPriority, &iNice, &iThreads);
   //21-30
   fscanf(fd, "%ld %llu %lu %ld %lu", &ilTmp, &ullTmp, &ulTmp, &ilTmp, &ulTmp);
   fscanf(fd, "%lu %lu %lu %lu %lu", &ulTmp, &ulTmp, &ulTmp, &ulTmp, &ulTmp);
   //31-40
   fscanf(fd, "%lu %lu %lu %lu %lu", &ulTmp, &ulTmp, &ulTmp, &ulTmp, &ulTmp);
   fscanf(fd, "%lu %lu %lu %d %u", &ulTmp, &ulTmp, &ulTmp, &iProcessor, &uRTPriority);

   //41
   fscanf(fd, "%u", &uSchPolicy);
   fclose(fd);


   if ( -1 != iCoreFilter )
   if ( iCoreFilter != iProcessor )
      return;

   char szCmdParams[1024];
   szCmdParams[0] = 0;

   sprintf(szFile, "/proc/%d/cmdline", iTID);
   fd = fopen(szFile, "r");
   if ( NULL == fd )
   {
      if ( -1 == iCoreFilter )
         printf("[Can't read TID cmdline file: %s]\n", szFile);
   }
   else
   {
      char szBuff[1024];
      memset(szBuff, 0, 1024);
      int iRead = fread(szBuff, 1,1023, fd);
      fclose(fd);
      szBuff[iRead] = 0;
      char* pParams = NULL;
      if ( iRead > 1 )
      for( int i=iRead-1; i>0; i-- )
      {
         if ( szBuff[i] == '-' )
         {
            pParams = &(szBuff[i]);
            break;
         }
      }
      if ( NULL != pParams )
         strcpy(szCmdParams, pParams);
   }

   cpu_set_t cpuSet;
   CPU_ZERO(&cpuSet);

   char szAffinity[32];
   strcpy(szAffinity,"    ");

   if ( 0 != sched_getaffinity(iTID, sizeof(cpuSet), &cpuSet) )
      printf("[failed to get CPU affinity for TID: %d\n", iTID);
   else
   {
      for( int i=0; i<hw_procs_get_cpu_count(); i++ )
      {
         if ( CPU_ISSET(i, &cpuSet) )
            szAffinity[i] = 'x';
         else
            szAffinity[i] = '.';
      }
   }

   _print_proc_line(szProgramName, szCmdParams, iPID, iTID, chState, iProcessor, uSchPolicy, iNice, uRTPriority, iRealPriority, szAffinity, iTaskCount, iCoreFilter);
}

void _log_process(int iPID, int iCoreFilter)
{
   if ( iPID <= 0 )
      return;

   int iTmpPID = 0;
   char szProgramName[128];
   szProgramName[0] = 0;

   char szComm[MAX_FILE_PATH_SIZE];
   char szOutput[1024];
   char szFile[MAX_FILE_PATH_SIZE];
   sprintf(szFile, "/proc/%d/stat", iPID);
   
   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
   {
      if ( iCoreFilter == -1 )
         printf("[Can't read PID stat file: %s]\n", szFile);
      return;
   }
   
   fscanf(fd, "%d %s", &iTmpPID, szProgramName);
   fclose(fd);

   szProgramName[strlen(szProgramName)] = 0;
   if ( szProgramName[0] == '(' )
   {
      int iLen = strlen(szProgramName);
      for( int i=0; i<iLen; i++ )
         szProgramName[i] = szProgramName[i+1];
   }
   if ( szProgramName[strlen(szProgramName)-1] == ')' )
       szProgramName[strlen(szProgramName)-1] = 0;

   sprintf(szComm, "ls /proc/%d/task", iPID);
   hw_execute_bash_command(szComm, szOutput);
   if ( 0 == strlen(szOutput) )
   {
      if ( -1 == iCoreFilter )
         _print_proc_line(szProgramName, "", iPID, ' ', 0, 0, 0, 0, 0, 0, "", 0, -1);
      return;
   }

   int iTasks[32];
   int iCountTasks = 0;
   int iSelfTaskIndex = -1;

   szOutput[strlen(szOutput)] = 0;
   char* pTmp = szOutput;
   char* pToken = szOutput;
   while (*pTmp != 0)
   {
      if ( isdigit(*pTmp) )
      {
         pTmp++;
         continue;
      }
      *pTmp = 0;
      if ( (*pToken != 0) && isdigit(*pToken) )
      {
         int iTID = atoi(pToken);
         if ( iTID > 0 )
         {
            iTasks[iCountTasks] = atoi(pToken);
            if ( iTasks[iCountTasks] == iPID )
               iSelfTaskIndex = iCountTasks;
            iCountTasks++;
         }
      }
      pTmp++;
      pToken = pTmp;
   }

   if ( -1 != iSelfTaskIndex )
      _log_task(szProgramName, iPID, iTasks[iSelfTaskIndex], 0, iCoreFilter);
   int iTmp = 0;
   for( int i=0; i<iCountTasks; i++ )
   {
      if ( i != iSelfTaskIndex )
         _log_task(szProgramName, iPID, iTasks[i], iTmp, iCoreFilter);
      iTmp++;
   }
}

void _enum_process(const char* szProcessName, int iCoreFilter)
{
   char szComm[MAX_FILE_PATH_SIZE];
   char szOutput[1024];

   sprintf(szComm, "pidof %s", szProcessName);
   hw_execute_bash_command(szComm, szOutput);

   if ( strlen(szOutput) < 1 )
   {
      if ( iCoreFilter == -1 )
         _print_proc_line(szProcessName, "",  0, 0, ' ', 0, 0, 0, 0, 0, "", 0, -1);
      return;
   }
   
   szOutput[strlen(szOutput)] = 0;
   char* pToken = szOutput;
   char* pTmp = szOutput;

   int iCountInstances = 0;
   while (*pTmp != 0)
   {
      if ( isdigit(*pTmp) )
      {
         pTmp++;
         continue;
      }
      *pTmp = 0;
      if ( (*pToken != 0) && isdigit(*pToken) )
      {
         int iPID = atoi(pToken);
         if ( iPID > 0 )
         {
            _log_process(iPID, iCoreFilter);
            iCountInstances++;
         }
      }
      pTmp++;
      pToken = pTmp;
   }

   if ( 0 == strlen(szOutput) || (0 == iCountInstances) )
   {
      if ( iCoreFilter == -1 )
         _print_proc_line(szProcessName, "", 0, 0, ' ', 0, 0, 0, 0, 0, "", 0, -1);
      return;
   }
}

void hw_log_processes(int argc, char *argv[])
{
   if ( NULL != strstr(argv[argc-1], "-h") )
   {
      printf("Arguments:\n");
      printf("  -v  version\n");
      printf("  -h  help\n");
      printf("  -a  vehicle (air side) only\n");
      printf("  -g  controller (ground side) only\n");
      printf("  -t  sort on priorities\n");
      printf("  -c  sort on CPU cores\n");
      printf("  -pagefaults show processes memory page faults\n");
      return;
   }

   s_bHwDbgSortOnCores = false;
   s_bHwDbgFilterAirOnly = false;
   s_bHwDbgFilterGroundOnly = false;
   s_bHwDbgShowAir = true;
   s_bHwDbgShowGround = true;

   for( int i=1; i<argc; i++ )
   {
      if ( NULL == argv[i] )
         continue;
      if ( 0 == strcmp(argv[i], "-c") )
         s_bHwDbgSortOnCores = true;
      if ( 0 == strcmp(argv[i], "-a") )
      {
          s_bHwDbgFilterAirOnly = true;
          s_bHwDbgShowGround = false;
      }
      if ( 0 == strcmp(argv[i], "-g") )
      {
          s_bHwDbgFilterGroundOnly = true;
          s_bHwDbgShowAir = false;
      }
   }

   int iCoreFilterStart = -1;
   int iCoreFilterEnd = -1;
   if ( s_bHwDbgSortOnCores )
   {
      iCoreFilterStart = 0;
      iCoreFilterEnd = hw_procs_get_cpu_count()-1;
   }
   printf("Ruby processes and threads (%d CPU cores):\n", hw_procs_get_cpu_count());
   printf("(state -> R=running S=sleep D=wait Z=zombie T=stopped W=paging X=dead P=parked I=idle)\n");
   printf("Name            (state)  PID\t TID\t  CPU C S_CLASS  NICE  RT_PRIO RAW_PRIO\n");
   printf("----------------------------------------------------------------------------------\n");
   
   for( int iCore=iCoreFilterStart; iCore <= iCoreFilterEnd; iCore++ )
   {
      _enum_process("ruby_logger", iCore);

      if ( s_bHwDbgShowGround )
      {
         if ( iCore != -1 )
            printf("\n");
         _enum_process("ruby_controller", iCore);
         _enum_process("ruby_central", iCore);
         _enum_process("ruby_rt_station", iCore);
         _enum_process("ruby_rx_telemetry", iCore);
         _enum_process("ruby_tx_rc", iCore);
         _enum_process("ruby_i2c", iCore);
         #if defined (HW_PLATFORM_RASPBERRY)
         _enum_process("ruby_player_s", iCore);
         _enum_process("ruby_player_f", iCore);
         _enum_process("ruby_player_p", iCore);
         #endif
         #if defined (HW_PLATFORM_RADXA)
         _enum_process("ruby_player_radxa", iCore);
         #endif
      }
      if ( s_bHwDbgShowAir )
      {
         if ( iCore != -1 )
            printf("\n");
         #if defined (HW_PLATFORM_RASPBERRY)
         _enum_process("ruby_alive", iCore);
         #endif
         _enum_process("ruby_start", iCore);
         _enum_process("ruby_rt_vehicle", iCore);
         _enum_process("ruby_tx_telemetry", iCore);
         #if defined (HW_PLATFORM_RASPBERRY)
         _enum_process("ruby_capture_raspi", iCore);
         _enum_process("ruby_capture_veye", iCore);
         #endif
         #if defined (HW_PLATFORM_OPENIPC_CAMERA)
         _enum_process("majestic", iCore);
         #endif
      }
   }
}


int hw_process_exists(const char* szProcName)
{
   if ( (NULL == szProcName) || (0 == szProcName[0]) )
      return 0;
   char szPids[256];
   hw_process_get_pids(szProcName, szPids);
   removeTrailingNewLines(szPids);
   char* p = removeLeadingWhiteSpace(szPids);

   if ( strlen(p) < 3 )
   {
      log_line("Process (%s) is not running.", szProcName);
      return 0;
   }
   if ( ! isdigit(*p) )
   {
      log_line("Process (%s) is not running.", szProcName);
      return 0;
   }

   int iPID = 0;
   if ( 1 != sscanf(p, "%d", &iPID) )
   {
      log_line("Process (%s) is not running.", szProcName);
      return 0;
   }
   if ( iPID < 100 )
   {
      log_line("Process (%s) is not running.", szProcName);
      return 0;
   }
   log_line("Process (%s) is running, PID: %d", szProcName, iPID);
   return iPID;
}

char* hw_process_get_pids_inline(const char* szProcName)
{
   static char s_szHWProcessPIDs[256];
   s_szHWProcessPIDs[0] = 0;
   hw_process_get_pids(szProcName, s_szHWProcessPIDs);
   return s_szHWProcessPIDs;
}

void hw_process_get_pids(const char* szProcName, char* szOutput)
{
   if ( NULL == szOutput )
      return;

   szOutput[0] = 0;

   if ( (NULL == szProcName) || (0 == szProcName[0]) )
      return;

   char szComm[128];

   log_line("Check existence of process (%s)...", szProcName);
   sprintf(szComm, "pidof %s", szProcName);
   hw_execute_bash_command_raw_silent(szComm, szOutput);
   removeTrailingNewLines(szOutput);
   log_line("Result of [pidof %s]: (%s)", szProcName, szOutput);
   char* p = removeLeadingWhiteSpace(szOutput);
   if ( strlen(p) < 3 )
   {
      log_line("No result on pidof. Check using pgrep...");
      sprintf(szComm, "pgrep %s", szProcName);
      hw_execute_bash_command_raw_silent(szComm, szOutput);
      removeTrailingNewLines(szOutput);
      log_line("Result of [pgrep %s]: (%s)", szProcName, szOutput);
      p = removeLeadingWhiteSpace(szOutput);
      if ( strlen(p) < 3 )
      {
         log_line("No results on pgrep. Check using ps...");
         sprintf(szComm, "ps -ae | grep %s | grep -v \"grep\"", szProcName);
         hw_execute_bash_command_raw_silent(szComm, szOutput);
         removeTrailingNewLines(szOutput);
         log_line("Result of [ps search %s]: (%s)", szProcName, szOutput);
         p = removeLeadingWhiteSpace(szOutput);
      }
   }
}

int hw_process_get_current_core(int iPID)
{
   if ( iPID < 10 )
   {
      log_softerror_and_alarm("[HWP] Tried to get current CPU core for invalid process PID: %d", iPID);
      return -1;
   }

   char szFile[MAX_FILE_PATH_SIZE];
   sprintf(szFile, "/proc/%d/stat", iPID);
   
   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("[HwProc] Can't read process stat file [%s] for PID %d, can't read current CPU core.", szFile, iPID);
      return -1;
   }
   
   char szTmp[64];
   int iTmp = 0;
   long int ilTmp = 0;
   unsigned int uTmp = 0;
   long unsigned int ulTmp = 0;
   unsigned long long ullTmp = 0;

   char chState = 0;
   long int iRealPriority = 0;
   long int iNice = 0;
   long int iThreads = 0;
   int iProcessor = 0;
   unsigned int uRTPriority = 0;
   unsigned int uSchPolicy = 0;

   //1-10
   fscanf(fd, "%d %s %c %d %d", &iTmp, szTmp, &chState, &iTmp, &iTmp);
   fscanf(fd, "%d %d %d %u %lu", &iTmp, &iTmp, &iTmp, &uTmp, &ulTmp);
   //11-20
   fscanf(fd, "%lu %lu %lu %lu %lu", &ulTmp, &ulTmp, &ulTmp, &ulTmp, &ulTmp);
   fscanf(fd, "%lu %lu %ld %ld %ld", &ulTmp, &ulTmp, &iRealPriority, &iNice, &iThreads);
   //21-30
   fscanf(fd, "%ld %llu %lu %ld %lu", &ilTmp, &ullTmp, &ulTmp, &ilTmp, &ulTmp);
   fscanf(fd, "%lu %lu %lu %lu %lu", &ulTmp, &ulTmp, &ulTmp, &ulTmp, &ulTmp);
   //31-40
   fscanf(fd, "%lu %lu %lu %lu %lu", &ulTmp, &ulTmp, &ulTmp, &ulTmp, &ulTmp);
   fscanf(fd, "%lu %lu %lu %d %u", &ulTmp, &ulTmp, &ulTmp, &iProcessor, &uRTPriority);

   //41
   fscanf(fd, "%u", &uSchPolicy);
   fclose(fd);

   log_line("[HWP] Process PID %d current CPU core is: %d", iPID, iProcessor);
   return iProcessor;
}

void hw_stop_process(const char* szProcName)
{
   char szComm[1024];
   char szPIDs[512];

   if ( NULL == szProcName || 0 == szProcName[0] )
      return;

   log_line("Stopping process [%s]...", szProcName);
   
   hw_process_get_pids(szProcName, szPIDs);
   removeTrailingNewLines(szPIDs);
   replaceNewLinesToSpaces(szPIDs);
   if ( strlen(szPIDs) > 2 )
   {
      log_line("Found PID(s) for process to stop %s: %s", szProcName, szPIDs);
      sprintf(szComm, "kill %s 2>/dev/null", szPIDs);
      hw_execute_bash_command(szComm, NULL);
      int retryCount = 30;
      u32 uTimeStart = get_current_timestamp_ms();
      while ( retryCount > 0 )
      {
         hardware_sleep_ms(10);
         szPIDs[0] = 0;
         hw_process_get_pids(szProcName, szPIDs);
         removeTrailingNewLines(szPIDs);
         replaceNewLinesToSpaces(szPIDs);
         if ( strlen(szPIDs) < 2 )
         {
            log_line("Did stopped process %s", szProcName);
            return;
         }
         retryCount--;
         if ( get_current_timestamp_ms() > uTimeStart + 1500 )
            retryCount = 0;
      }
      sprintf(szComm, "kill -9 %s 2>/dev/null", szPIDs);
      hw_execute_bash_command(szComm, NULL);
      hardware_sleep_ms(20);
   }
   else
      log_line("Process [%s] does not exists.", szProcName);
}


int hw_kill_process(const char* szProcName, int iSignal)
{
   char szCommStop[512];
   char szPIDs[256];

   if ( (NULL == szProcName) || (0 == szProcName[0]) )
      return -1;

   hw_process_get_pids(szProcName, szPIDs);
   removeTrailingNewLines(szPIDs);
   replaceNewLinesToSpaces(szPIDs);
   if ( strlen(szPIDs) < 3 )
   {
      log_line("Process %s does not exist. Nothing to kill.", szProcName);
      return 0;
   }
   sprintf(szCommStop, "kill %d %s 2>/dev/null", iSignal, szPIDs);
   hw_execute_bash_command_raw(szCommStop, NULL);
   hardware_sleep_ms(20);

   hw_process_get_pids(szProcName, szPIDs);
   removeTrailingNewLines(szPIDs);
   replaceNewLinesToSpaces(szPIDs);
   if ( strlen(szPIDs) < 3 )
      return 1;

   log_line("Process still exists, %s PIDs are: %s", szProcName, szPIDs);

   int retryCount = 5;
   while ( retryCount > 0 )
   {
      hardware_sleep_ms(10);
      hw_execute_bash_command_raw(szCommStop, NULL);
      szPIDs[0] = 0;
      hw_process_get_pids(szProcName, szPIDs);
      removeTrailingNewLines(szPIDs);
      replaceNewLinesToSpaces(szPIDs);
      if ( strlen(szPIDs) < 3 )
         return 1;
      log_line("Process still exists (%d), %s PIDs are: %s", retryCount, szProcName, szPIDs);
      retryCount--;
   }
   return 0;
}

// timeout: positive -> max ms to wait; 0 -> wait until process finishes; negative -> do not wait for the process
int hw_execute_process(const char *szCommand, int iTimeoutMs, char* szOutput, int iMaxOutputLength)
{
   static int s_iCountExecutions = 0;
   s_iCountExecutions++;

   if ( NULL != szOutput )
       *szOutput = 0;
   if ( (NULL != szOutput) && (iMaxOutputLength > 0) )
       memset(szOutput, 0, iMaxOutputLength);

   if ( NULL == szCommand || (0 == szCommand[0]) )
   {
      log_softerror_and_alarm("[HWP-%d] Tried to execute invalid process, no name.", s_iCountExecutions);
      return -1;
   }

   if ( NULL != strstr(szCommand, "*") )
   {
       log_softerror_and_alarm("[HWP-%d] Tried to execute command with wildcards: [%s].", s_iCountExecutions, szCommand);
       return -1;
   }

   if ( iTimeoutMs < 0 )
      log_line("[HWP-%d] Executing process (without waiting for it): [%s]...", s_iCountExecutions, szCommand);
   else if ( 0 == iTimeoutMs )
      log_line("[HWP-%d] Executing process (wait infinite): [%s]...", s_iCountExecutions, szCommand);
   else
      log_line("[HWP-%d] Executing process (max wait %d ms): [%s]...", s_iCountExecutions, iTimeoutMs, szCommand);

   pid_t pid = getpid();
   pid_t ppid = getppid();
   log_line("[HWP-%d] Current PID: %d, parent PID: %d", s_iCountExecutions, (int)pid, (int)ppid);

   char* pTmp = (char*)strstr(szCommand, "2>");
   if ( NULL != pTmp )
      *pTmp = 0;
   pTmp = (char*)strstr(szCommand, "1>");
   if ( NULL != pTmp )
      *pTmp = 0;

   char* argv[32];
   memset(argv, 0, sizeof(argv));

   char szTmpBuff[4096];
   char szErrorBuffer[4096];
   int iCountReadError = 0;

   int iCountArg = 0;
   char szArgs[1024];
   strncpy(szArgs, szCommand, 1023);
   szArgs[1023] = 0;
   pTmp = szArgs;
   char* pToken = pTmp;
   while ( 1 )
   {
      if ( (*pTmp != ' ') && (*pTmp != 0) )
      {
         pTmp++;
         continue;
      }
      if ( ((pToken[0] == '1') || (pToken[0] == '2')) && (pToken[1] == '>') )
      {
         argv[iCountArg] = NULL;
         break;
      }

      argv[iCountArg] = pToken;
      iCountArg++;

      if ( (*pTmp == 0) || (iCountArg > 30) )
         break;
      *pTmp = 0;
      pTmp++;
      while ( *pTmp == ' ' )
          pTmp++;
      pToken = pTmp;
   }

   if ( (iCountArg < 1) || (strlen(argv[0]) < 2) )
   {
      log_softerror_and_alarm("[HWP-%d] Tried to execute invalid process, invalid name.", s_iCountExecutions);
      return -1;
   }

   u32 uTimeStart = get_current_timestamp_ms();

   int stdout_fds[2];
   int stderr_fds[2];
   pipe(stdout_fds);
   pipe(stderr_fds);
 
   const pid_t pidChild = fork();
   if (0 == pidChild)
   {
      close(stdout_fds[0]);
      close(stderr_fds[0]);

      char szParams[256];
      szParams[0] = 0;
      int iCountParams = 0;
      for( int i=1; i<32; i++ )
      {
         if ( argv[i] == NULL )
             break;
         if ( 0 != szParams[0] )
             strcat(szParams, ", ");
         strcat(szParams, "[");
         strcat(szParams, argv[i]);
         strcat(szParams, "]");
         iCountParams++;
      }
      log_line("[HWPC-%d] Executing in child process: [%s], %d params: %s", s_iCountExecutions, argv[0], iCountParams, szParams);
      pid_t pid = getpid();
      pid_t ppid = getppid();
      log_line("[HWPC-%d] Forked child current PID: %d, parent PID: %d", s_iCountExecutions, (int)pid, (int)ppid);

      dup2(stdout_fds[1], 1);
      dup2(stderr_fds[1], 2);
      execvp(argv[0], argv);
      // This will never be called.
      log_line("[HWPC-%d] Done executing in child process.", s_iCountExecutions);
      exit(0);
   }

   if ( iTimeoutMs < 0 )
   {
      u32 uTime = get_current_timestamp_ms() - uTimeStart;
      log_line("[HWP-%d] Finished launching child pid %d, with no wait, in %u ms", s_iCountExecutions, pidChild, uTime);
      return 0;
   }

   log_line("[HWP-%d] Waiting for child process (PID: %d)...", s_iCountExecutions, pidChild);
   if ( 0 == iTimeoutMs )
       iTimeoutMs = 10000000;

   int status = 0;
   int iResWait = 0;

   int iCountRead = 0;
   int iPartialTotalRead = 0;
   fd_set readSet;
   fd_set exceSet;
   struct timeval timeRead;

   while ( 0 == iResWait )
   {
      status = 0;
      iResWait = waitpid(pidChild , &status , WNOHANG);
      if ( iResWait < 0 )
         log_line("[HWP-%d] Failed to wait on child process. Errno: %d, error: %s", s_iCountExecutions, errno, strerror(errno));

      if ( (iResWait > 0) || (WIFSIGNALED(status)) || (WCOREDUMP(status)) )
      {
         if ( WIFSIGNALED(status) )
            log_line("[HWP-%d] Child process terminated by signal.", s_iCountExecutions);
         if ( WCOREDUMP(status) )
            log_line("[HWP-%d] Child process terminated with a core dump.", s_iCountExecutions);
         if ( WIFSTOPPED(status) )
            log_line("[HWP-%d] Child process was stopped by signal.", s_iCountExecutions);
         break;
      }
      hardware_sleep_ms(1);
      iTimeoutMs--;
      if ( 0 == (iTimeoutMs % 1000) )
          log_line("[HWP-%d] Still waiting for child process...", s_iCountExecutions);
      if ( iTimeoutMs == 0 )
      {
         log_line("[HWP-%d] Timed out waiting for child process.", s_iCountExecutions);
         break;
      }
      // Try to read stdoutput and stderror

      FD_ZERO(&readSet);
      FD_SET(stdout_fds[0], &readSet);
      FD_ZERO(&exceSet);
      FD_SET(stdout_fds[0], &exceSet);
      timeRead.tv_sec = 0;
      timeRead.tv_usec = 100; // .1 milisec timeout

      int iSelResult = select(stdout_fds[0]+1, &readSet, NULL, &exceSet, &timeRead);
      if ( iSelResult > 0 )
      {
         if ( FD_ISSET(stdout_fds[0], &readSet) )
         {
            int iRead = read(stdout_fds[0], szTmpBuff, 4096);
            if ( iRead > 0 )
               iPartialTotalRead += iRead;
            log_line("[HWP-%d] Read partial stdout from child process, %d bytes (total %d bytes)", s_iCountExecutions, iRead, iPartialTotalRead);
            if ( (iRead > 0) && (NULL != szOutput) && (iMaxOutputLength > 0) && (iCountRead + iRead < iMaxOutputLength) )
            {
                memcpy(&szOutput[iCountRead], szTmpBuff, iRead);
                iCountRead += iRead;
                szOutput[iCountRead] = 0;
            }
         }
         if ( FD_ISSET(stdout_fds[0], &exceSet) )
         {
            log_line("[HWP-%d] Reading child process stdout (PID: %d) returned exception.", s_iCountExecutions, pidChild);
         }
      }

      FD_ZERO(&readSet);
      FD_SET(stderr_fds[0], &readSet);
      FD_ZERO(&exceSet);
      FD_SET(stderr_fds[0], &exceSet);
      timeRead.tv_sec = 0;
      timeRead.tv_usec = 100; // .1 milisec timeout

      iSelResult = select(stderr_fds[0]+1, &readSet, NULL, &exceSet, &timeRead);
      if ( iSelResult > 0 )
      {
         if ( FD_ISSET(stderr_fds[0], &readSet) )
         {
            int iRead = read(stderr_fds[0], szTmpBuff, 4096);
            if ( iRead > 0 )
               iPartialTotalRead += iRead;
            log_line("[HWP-%d] Read partial stderr from child process, %d bytes (total %d bytes)", s_iCountExecutions, iRead, iPartialTotalRead);
            if ( (iRead > 0) && (NULL != szOutput) && (iMaxOutputLength > 0) && (iCountRead + iRead < iMaxOutputLength) )
            {
                memcpy(&szOutput[iCountRead], szTmpBuff, iRead);
                iCountRead += iRead;
                szOutput[iCountRead] = 0;
            }

            if ( iRead > 0 )
            {
                int iMaxRead = iRead;
                if ( iMaxRead + iCountReadError >= 4095 )
                    iMaxRead = 4095 - iCountReadError;
                if ( iMaxRead > 0 )
                {
                   memcpy(&szErrorBuffer[iCountReadError], szTmpBuff, iMaxRead);
                   iCountReadError += iMaxRead;
                   szErrorBuffer[iCountReadError] = 0;
                }
            }
         }
         if ( FD_ISSET(stderr_fds[0], &exceSet) )
         {
            log_line("[HWP-%d] Reading child process stderr (PID: %d) returned exception.", s_iCountExecutions, pidChild);
         }
      }
   }

   if ( (NULL == szOutput) || (iMaxOutputLength <= 0) )
   {
      close(stdout_fds[0]);
      close(stdout_fds[1]);
      close(stderr_fds[0]);
      close(stderr_fds[1]);

      u32 uTime = get_current_timestamp_ms() - uTimeStart;
      if ( 0 == iTimeoutMs )
         log_line("[HWP-%d] Timed out waiting for child pid %d, res wait: %d, in %u ms, no output needed.", s_iCountExecutions, pidChild, iResWait, uTime);
      else
         log_line("[HWP-%d] Finished executing and waiting for child pid %d, res wait: %d, in %u ms, no output needed.", s_iCountExecutions, pidChild, iResWait, uTime);

      if ( iCountReadError > 0 )
         log_line("[HWP-%d] Error output from child process: [%s]", s_iCountExecutions, szErrorBuffer);
      return 0;
   }

   u32 uTime = get_current_timestamp_ms() - uTimeStart;
   if ( 0 == iTimeoutMs )
      log_line("[HWP-%d] Timedout waiting for child pid %d, res wait: %d, in %u ms", s_iCountExecutions, pidChild, iResWait, uTime);
   else
      log_line("[HWP-%d] Finished waiting for child pid %d, res wait: %d, in %u ms", s_iCountExecutions, pidChild, iResWait, uTime);
   log_line("[HWP-%d] Reading child process stdout (PID: %d, read so far: %d bytes), will read max %d bytes...", s_iCountExecutions, pidChild, iCountRead, iMaxOutputLength);
   do
   {
      FD_ZERO(&readSet);
      FD_SET(stdout_fds[0], &readSet);
      FD_ZERO(&exceSet);
      FD_SET(stdout_fds[0], &exceSet);
      timeRead.tv_sec = 0;
      timeRead.tv_usec = 100; // .1 milisec timeout

      int iSelResult = select(stdout_fds[0]+1, &readSet, NULL, &exceSet, &timeRead);
      if ( iSelResult <= 0 )
      {
         log_line("[HWP-%d] Reading child process stdout (PID: %d) has no more data (res: %d, %d bytes now).", s_iCountExecutions, pidChild, iSelResult, iCountRead);
         break;
      }
      if ( FD_ISSET(stdout_fds[0], &readSet) )
      {
         int iRead = read(stdout_fds[0], &(szOutput[iCountRead]), iMaxOutputLength-iCountRead-1);
         if ( iRead == 0 )
         {
            log_line("[HWP-%d] Reading child process stdout (PID: %d) reached EOF.", s_iCountExecutions, pidChild);
            break;
         }
         if ( iRead > 0 )
            iCountRead += iRead;
         if ( iCountRead >= iMaxOutputLength-1 )
         {
            iCountRead = iMaxOutputLength-1;
            break;
         }
      }
      if ( FD_ISSET(stdout_fds[0], &exceSet) )
      {
         log_line("[HWP-%d] Reading child process stdout (PID: %d) returned exception.", s_iCountExecutions, pidChild);
         break;
      }
   } while ( 1 );
   szOutput[iCountRead] = 0;

   log_line("[HWP-%d] Finished reading child process stdout. %d bytes", s_iCountExecutions, iCountRead);

   do
   {
      if ( iCountRead >= iMaxOutputLength-1 )
      {
         iCountRead = iMaxOutputLength-1;
         break;
      }

      FD_ZERO(&readSet);
      FD_SET(stderr_fds[0], &readSet);
      FD_ZERO(&exceSet);
      FD_SET(stderr_fds[0], &exceSet);
      timeRead.tv_sec = 0;
      timeRead.tv_usec = 100; // .1 milisec timeout

      int iSelResult = select(stderr_fds[0]+1, &readSet, NULL, &exceSet, &timeRead);
      if ( iSelResult <= 0 )
      {
         log_line("[HWP-%d] Reading child process stderr (PID: %d) has no more data (res: %d, %d bytes now).", s_iCountExecutions, pidChild, iSelResult, iCountRead);
         break;
      }
      
      if ( FD_ISSET(stderr_fds[0], &readSet) )
      {
         int iRead = read(stderr_fds[0], &(szOutput[iCountRead]), iMaxOutputLength-iCountRead-1);
         if ( iRead == 0 )
         {
            log_line("[HWP-%d] Reading child process stderror (PID: %d) reached EOF.", s_iCountExecutions, pidChild);
            break;
         }
         if ( iRead > 0 )
            iCountRead += iRead;

         if ( iRead > 0 )
         {
             int iMaxRead = iRead;
             if ( iMaxRead + iCountReadError >= 4095 )
                 iMaxRead = 4095 - iCountReadError;
             if ( iMaxRead > 0 )
             {
                memcpy(&szErrorBuffer[iCountReadError], szTmpBuff, iMaxRead);
                iCountReadError += iMaxRead;
                szErrorBuffer[iCountReadError] = 0;
             }
         }

         if ( iCountRead >= iMaxOutputLength-1 )
         {
            iCountRead = iMaxOutputLength-1;
            break;
         }
      }
      if ( FD_ISSET(stderr_fds[0], &exceSet) )
      {
         log_line("[HWP-%d] Reading child process stderror (PID: %d) returned exception.", s_iCountExecutions, pidChild);
         break;
      }
   } while ( 1 );
   szOutput[iCountRead] = 0;

   log_line("[HWP-%d] Finished reading child process stderror. %d bytes", s_iCountExecutions, iCountRead);
   if ( iCountReadError > 0 )
      log_line("[HWP-%d] Error output from child process: [%s]", s_iCountExecutions, szErrorBuffer);

   close(stdout_fds[0]);
   close(stdout_fds[1]);
   close(stderr_fds[0]);
   close(stderr_fds[1]);

   uTime = get_current_timestamp_ms() - uTimeStart;
   log_line("[HWP-%d] Finished executing and waiting for child pid %d, res wait: %d, read %d bytes in %u ms", s_iCountExecutions, pidChild, iResWait, iCountRead, uTime);
   return 0;
}

int hw_execute_process_wait(const char *szCommand)
{
    return hw_execute_process(szCommand, 0, NULL, 0);
}

int hw_launch_process_exec(const char *szFile, const char* szParam1, const char* szParam2, const char* szParam3, const char* szParam4)
{
   // execv messes up the timer

   char szBuff[1024];
   if ( (NULL == szParam1) && (NULL == szParam2) && (NULL == szParam3) && (NULL == szParam4) )
      sprintf(szBuff, "%s &", szFile);
   else if ( (NULL != szParam1) && (NULL == szParam2) && (NULL == szParam3) && (NULL == szParam4) )
      sprintf(szBuff, "%s %s &", szFile, szParam1);
   else
      sprintf(szBuff, "%s %s %s %s %s &", szFile, ((NULL != szParam1)?szParam1:""), ((NULL != szParam2)?szParam2:""), ((NULL != szParam3)?szParam3:""), ((NULL != szParam4)?szParam4:"") );
   
   hw_execute_bash_command(szBuff, NULL);
   return 0;

   log_line("--------------------------------------------------------");
   log_line("|   Launching process: %s %s %s %s %s", szFile, ((NULL != szParam1)?szParam1:""), ((NULL != szParam2)?szParam2:""), ((NULL != szParam3)?szParam3:""), ((NULL != szParam4)?szParam4:""));
   log_line("--------------------------------------------------------");

   pid_t   my_pid;
#ifdef WAIT_FOR_COMPLETION
   int status;
   int timeout /* unused ifdef WAIT_FOR_COMPLETION */;
#endif
   char    *argv[6];
   argv[5] = NULL;
   argv[4] = NULL;
   argv[3] = NULL;
   argv[2] = NULL;
   argv[1] = NULL;
   if ( (NULL != szParam4) && (0 < strlen(szParam4)) )
   {
      argv[4] = (char*)malloc(strlen(szParam4)+1);
      strcpy(argv[4], szParam4);
   }
   if ( (NULL != szParam3) && (0 < strlen(szParam3)) )
   {
      argv[3] = (char*)malloc(strlen(szParam3)+1);
      strcpy(argv[3], szParam3);
   }
   if ( (NULL != szParam2) && (0 < strlen(szParam2)) )
   {
      argv[2] = (char*)malloc(strlen(szParam2)+1);
      strcpy(argv[2], szParam2);
   }
   if ( (NULL != szParam1) && (0 < strlen(szParam1)) )
   {
      argv[1] = (char*)malloc(strlen(szParam1)+1);
      strcpy(argv[1], szParam1);
   }

   argv[0] = (char*)malloc(strlen(szFile)+1);
   strcpy(argv[0], szFile);
   if (0 == (my_pid = fork()))
   {
      if (-1 == execve(argv[0], (char **)argv , NULL))
      {
         perror("child process execve failed [%m]");
         return -1;
      }
   }

#ifdef WAIT_FOR_COMPLETION
    timeout = 1000;

    while (0 == waitpid(my_pid , &status , WNOHANG)) {
            if ( --timeout < 0 ) {
                    perror("timeout");
                    return -1;
            }
            sleep(1);
    }

    printf("%s WEXITSTATUS %d WIFEXITED %d [status %d]\n",
            argv[0], WEXITSTATUS(status), WIFEXITED(status), status);

    if (1 != WIFEXITED(status) || 0 != WEXITSTATUS(status)) {
            perror("%s failed, halt system");
            return -1;
    }

#endif
    return 0;
}

int _hw_execute_bash_command(const char* command, char* outBuffer, int iSilent, u32 uTimeoutMs)
{
   if ( NULL != outBuffer )
      *outBuffer = 0;
   FILE* fp = popen( command, "r" );
   if ( NULL == fp )
   {
      log_error_and_alarm("Failed to execute command: %s", command);
      return 0;
   }
   if ( uTimeoutMs == 0 )
      uTimeoutMs = 5;
   char szBuff[2048];
   u32 uTimeStart = get_current_timestamp_ms();
   int iCountRead = 0;
   int iRead = 0;
   char* pOut = outBuffer;

   int iResult = 1;
   while ( 1 )
   {
      if ( ferror(fp) || feof(fp) )
         break;

      iRead = fread(szBuff, 1, 1022, fp);
      if ( iRead > 0 )
      {
         iCountRead += iRead;
         if ( (NULL != pOut) && (iCountRead < 4094) )
         {
            szBuff[iRead] = 0;
            memcpy(pOut, szBuff, iRead+1);
            pOut += iRead;
         }
      }
      if ( get_current_timestamp_ms() >= uTimeStart + uTimeoutMs )
      {
         log_line("Abandoning reading start process output.");
         iResult = -1;
         break;
      }
   }

   if ( 0 == iSilent )
   {
      if ( (iCountRead > 0) && (iCountRead < 100) )
      {
         char szTmp[100];
         if ( NULL != outBuffer )
            strncpy(szTmp, outBuffer, 99);
         else
            strncpy(szTmp, szBuff, 99);
         szTmp[99] = 0;
         removeTrailingNewLines(szTmp);
         removeLeadingWhiteSpace(szTmp);
         log_line("Read process output: %d bytes in %u ms. Content: [%s]", iCountRead, get_current_timestamp_ms() - uTimeStart, szTmp);
      }
      else
         log_line("Read process output: %d bytes in %u ms", iCountRead, get_current_timestamp_ms() - uTimeStart);
   }
   if ( -1 == pclose(fp) )
   {
      log_softerror_and_alarm("Failed to close command: %s", command);
      return 0;
   }
   return iResult;
}

int hw_execute_bash_command_nonblock(const char* command, char* outBuffer)
{
   if ( NULL != outBuffer )
      *outBuffer = 0;

   if ( (NULL == command) || (0 == command[0]) )
      return 0;

   char szCommand[1024];
   int iLen = strlen(command);

   if ( NULL == strstr(command, "2>/dev/null") )
   {
      if ( command[iLen-1] != '&' )
         snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "%s 2>/dev/null &", command);
      else
      {
         char szTmp[1024];
         strncpy(szTmp, command, sizeof(szTmp)/sizeof(szTmp[0]));
         szTmp[iLen-1] = 0;
         if ( command[iLen-2] == ' ' )
            szTmp[iLen-2] = 0;
         snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "%s 2>/dev/null &", szTmp);
      }
   }
   else
      strncpy(szCommand, command, sizeof(szCommand)/sizeof(szCommand[0]));


   log_line("Executing command nonblock: %s", szCommand);
   FILE* fp = popen( szCommand, "r" );
   if ( NULL == fp )
   {
      log_error_and_alarm("Failed to execute command: [%s]", szCommand);
      return 0;
   }

   if ( NULL != outBuffer )
   {
      char szBuff[256];
      if ( fgets(szBuff, 254, fp) != NULL)
      {
         szBuff[254] = 0;
         strcpy(outBuffer, szBuff);
      }
      else
         log_line("Empty response from executing command.");
   }
   if ( -1 == pclose(fp) )
   {
      log_softerror_and_alarm("Failed to execute command: [%s]", szCommand);
      return 0;
   }

   log_line("Executed command nonblock: [%s]", szCommand);
   return 1;
}

int hw_execute_bash_command(const char* command, char* outBuffer)
{
   log_line("Executing command: %s", command);
   return _hw_execute_bash_command(command, outBuffer, 0, 3000);
}

int hw_execute_bash_command_timeout(const char* command, char* outBuffer, u32 uTimeoutMs)
{
   log_line("Executing command (timeout: %u ms): %s", uTimeoutMs, command);
   return _hw_execute_bash_command(command, outBuffer, 0, uTimeoutMs);
}

int hw_execute_bash_command_silent(const char* command, char* outBuffer)
{
   if ( (NULL == command) || (0 == command[0]) )
      return 0;
   char szCommand[1024];
   int iLen = strlen(command);

   if ( NULL == strstr(command, "2>/dev/null") )
   {
      if ( command[iLen-1] != '&' )
         snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "%s 2>/dev/null &", command);
      else
      {
         char szTmp[1024];
         strncpy(szTmp, command, sizeof(szTmp)/sizeof(szTmp[0]));
         szTmp[iLen-1] = 0;
         if ( command[iLen-2] == ' ' )
            szTmp[iLen-2] = 0;
         snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "%s 2>/dev/null &", szTmp);
      }
   }
   else
      strncpy(szCommand, command, sizeof(szCommand)/sizeof(szCommand[0]));

   return _hw_execute_bash_command(szCommand, outBuffer, 1, 3000);
}

int hw_execute_bash_command_raw(const char* command, char* outBuffer)
{
   log_line("Executing command raw (on PID %d): %s", getpid(), command);
   return _hw_execute_bash_command(command, outBuffer, 0, 3000);
}

int hw_execute_bash_command_raw_timeout(const char* command, char* outBuffer, u32 uTimeoutMs)
{
   log_line("Executing command raw timeout %u ms: %s", uTimeoutMs, command);
   return _hw_execute_bash_command(command, outBuffer, 0, uTimeoutMs); 
}

int hw_execute_bash_command_raw_silent(const char* command, char* outBuffer)
{
   return _hw_execute_bash_command(command, outBuffer, 1, 3000);
}

void hw_execute_ruby_process(const char* szPrefixes, const char* szProcess, const char* szParams, char* szOutput)
{
   hw_execute_ruby_process_wait(szPrefixes, szProcess, szParams, szOutput, 0);
}

void hw_execute_ruby_process_wait(const char* szPrefixes, const char* szProcess, const char* szParams, char* szOutput, int iWait)
{
   if ( (NULL == szProcess) || (0 == szProcess[0]) )
      return;
   if ( (NULL != szPrefixes) && (0 != szPrefixes[0]) )
      log_line("Executing Ruby process: [%s], prefixes: [%s], params: [%s], wait: %s", szProcess, szPrefixes, ((NULL != szParams)?szParams:"None"), (iWait?"yes":"no"));
   else
      log_line("Executing Ruby process: [%s], no prefixes, params: [%s], wait: %s", szProcess, ((NULL != szParams)?szParams:"None"), (iWait?"yes":"no"));

   if ( NULL != szOutput )
      szOutput[0] = 0;
   
   char szFullPath[MAX_FILE_PATH_SIZE];
   char szFullPathDebug[MAX_FILE_PATH_SIZE];
   szFullPathDebug[0] = 0;
   strcpy(szFullPath, szProcess);
   if ( access(szFullPath, R_OK) == -1 )
      sprintf(szFullPath, "%s%s", FOLDER_BINARIES, szProcess);

   if ( access("/tmp/debug", R_OK) != -1 )
   {
      sprintf(szFullPathDebug, "/tmp/%s", szProcess);
      if ( access(szFullPathDebug, R_OK) != -1 )
         strcpy(szFullPath, szFullPathDebug);
   }
   if ( access(szFullPath, R_OK) == -1 )
   {
      log_error_and_alarm("Can't execute Ruby process. Not found here: [%s] or here: [%s]", szProcess, szFullPath );
      return;
   }

   char szCommand[512];
   szCommand[0] = 0;
   if ( (NULL != szPrefixes) && (0 != szPrefixes[0]) )
   {
      strcpy(szCommand, szPrefixes);
      strcat(szCommand, " ");
   }
   if ( szFullPath[0] != '/' )
     strcat(szCommand, "./");
   strcat(szCommand, szFullPath);

   if ( (NULL != szParams) && (0 != szParams[0]) )
   {
      strcat(szCommand, " ");
      strcat(szCommand, szParams);
   }

   if ( ! iWait )
      strcat(szCommand, " &");

   FILE* fp = popen( szCommand, "r" );
   if ( NULL == fp )
   {
      log_error_and_alarm("Failed to execute Ruby process: [%s]", szCommand);
      return;
   }

   if ( ! iWait )
   {
      hardware_sleep_ms(10);
      if ( ferror(fp) || feof(fp) )
         log_line("Failed to check for file end.");
   }
   else
   {
      char szOutputBuffer[4096];
      u32 uTimeStart = get_current_timestamp_ms();
      u32 uTimeoutMs = 2000;

      if ( NULL != szOutput )
         szOutput[0] = 0;
      int iCountRead = 0;
      while ( 1 )
      {
         if ( ferror(fp) || feof(fp) )
            break;
         int iRead = fread(szOutputBuffer, 1, 4092, fp);
         if ( iRead < 0 )
            log_line("Failed to read process output, error: %d, %d, %s", iRead, errno, strerror(errno));
         if ( iRead > 0 )
         {
            iCountRead += iRead;
            if ( NULL != szOutput )
            {
               szOutputBuffer[iRead] = 0;
               memcpy(szOutput, szOutputBuffer, 127);
               szOutput[127] = 0;
            }
         }
         if ( get_current_timestamp_ms() >= uTimeStart + uTimeoutMs )
         {
            log_line("Abandoning reading start process output.");
            break;
         }
      }
      log_line("Read process output for process (%s): %d bytes in %u ms", szProcess, iCountRead, get_current_timestamp_ms() - uTimeStart);
   }
   
   if ( -1 == pclose(fp) )
      log_softerror_and_alarm("Failed to launch and confirm Ruby process: [%s]", szCommand);
   else
      log_line("Launched Ruby process: [%s]", szCommand);
}

static int s_iCPUCoresCount = -1;
int hw_procs_get_cpu_count()
{
   if ( s_iCPUCoresCount > 0 )
      return s_iCPUCoresCount;

   s_iCPUCoresCount = 1;
   char szOutput[128];
   hw_execute_bash_command_raw("nproc --all", szOutput);
   if ( 1 != sscanf(szOutput, "%d", &s_iCPUCoresCount) )
   {
      log_softerror_and_alarm("[HWP] Failed to get CPU cores count.");
      s_iCPUCoresCount = 1;
   }
   if ( (s_iCPUCoresCount < 1) || (s_iCPUCoresCount > 32) )
   {
      log_softerror_and_alarm("[HWP] Invalid value for get CPU cores count.");
      s_iCPUCoresCount = 1;
   }
   return s_iCPUCoresCount;
}

int hw_get_current_thread_id()
{
   #if defined(HW_PLATFORM_RADXA)
   return gettid();
   #endif
   //return pthread_getthreadid_np();
   return (int)pthread_self();
   //return  gettid();
}

int hw_init_worker_thread_attrs(pthread_attr_t* pAttr, int iDesiredCore, int iStackSizeBytes, int iSchedulingClass, int iRawPriority, const char* szSource)
{
   if ( (NULL == pAttr) || (NULL == szSource) )
      return -1;

   size_t iStackSize = 0;
   pid_t pid = getpid();

   pthread_attr_init(pAttr);
   if ( 0 != pthread_attr_getstacksize(pAttr, &iStackSize) )
      log_softerror_and_alarm("[HWP] Failed to get thread default stack size (for: %s). Error: %d (%s)", szSource, errno, strerror(errno));
   else
      log_line("[HWP] Thread current initial stack size (for: %s): %d bytes (min allowed: %d bytes)", szSource, iStackSize, PTHREAD_STACK_MIN);

   size_t iNewStackSize = 64000;
   if ( iStackSizeBytes > 0 )
      iNewStackSize = iStackSizeBytes;
   else
   {
      if ( NULL != szSource )
      if ( (NULL != strstr(szSource, "radio_rx")) || (NULL != strstr(szSource, "HwCamMajesticThread")) )
         iNewStackSize = 128 * 1024;
      if ( (NULL != strstr(szSource, "video_recording")) )
         iNewStackSize = 256 * 1024;
   }
   if ( iNewStackSize < PTHREAD_STACK_MIN )
      iNewStackSize = PTHREAD_STACK_MIN;

   for( int i=0; i<3; i++ )
   {
      if ( 0 != pthread_attr_setstacksize(pAttr, iNewStackSize) )
      {
         #if defined (HW_PLATFORM_RADXA)
         log_line("[HWP] ERROR: Failed to set thread new stack size to %d bytes (for: %s). Error: %d (%s)", iNewStackSize, szSource, errno, strerror(errno));
         #else
         log_softerror_and_alarm("[HWP] Failed to set thread new stack size (for: %s). Error: %d (%s)", szSource, errno, strerror(errno));
         #endif
         log_softerror_and_alarm("[HWP] Using initial original stack size of %d bytes (for: %s)", iStackSize, szSource);
         iNewStackSize *= 2;
      }
      else
         break;
   }
   iStackSize = 0;
   if ( 0 != pthread_attr_getstacksize(pAttr, &iStackSize) )
      log_softerror_and_alarm("[HWP] Failed to get thread new stack size (for: %s). Error: %d (%s)", szSource, errno, strerror(errno));
   else
      log_line("[HWP] Thread new stack size (for: %s): %d bytes", szSource, iStackSize);

   pthread_attr_setdetachstate(pAttr, PTHREAD_CREATE_DETACHED);
   pthread_attr_setinheritsched(pAttr, PTHREAD_INHERIT_SCHED);
   int iRet = 0;
   
   pthread_attr_setinheritsched(pAttr, PTHREAD_EXPLICIT_SCHED);

   // If raw prio < 2, set to SCHED_OTHER, prio 0
   if ( iRawPriority < 2 )
   {
       log_line("[HWP] Thread attr: disabled raw priority, switch to OTHER schedulling class.");
       iSchedulingClass = SCHED_OTHER;
   }
   iRet = pthread_attr_setschedpolicy(pAttr, iSchedulingClass);
   if ( 0 != iRet )
      log_softerror_and_alarm("[HWP] Failed to set scheduling class attr (for: %s, schedule class: %s). Error: %d (%s)", szSource, str_format_schedule_policy(iSchedulingClass), errno, strerror(errno));
   struct sched_param params;
   params.sched_priority = 0;
   if ( (iRawPriority > 1) && (iRawPriority <= 100) )
      params.sched_priority = 100 - iRawPriority;
   iRet = pthread_attr_setschedparam(pAttr, &params);
   if ( 0 != iRet )
      log_softerror_and_alarm("[HWP] Failed to set scheduling priority attr (for: %s, schedule class: %s, raw priority: %d, rt priority: %d). Error: %d (%s)", szSource, str_format_schedule_policy(iSchedulingClass), iRawPriority, params.sched_priority, errno, strerror(errno));
   else
      log_line("[HWP] Thread attr: updated scheduling priority to: %s, raw priority: %d, RT priority: %d", str_format_schedule_policy(iSchedulingClass), iRawPriority, params.sched_priority);

   if ( (iDesiredCore >= 0) && (hw_procs_get_cpu_count() > 1) )
   {
      if ( iDesiredCore >= hw_procs_get_cpu_count() )
         iDesiredCore = hw_procs_get_cpu_count()-1;
      cpu_set_t cpuSet;
      CPU_ZERO(&cpuSet);
      CPU_SET(iDesiredCore, &cpuSet);
      iRet = pthread_attr_setaffinity_np(pAttr, sizeof(cpuSet), &cpuSet);
      if ( 0 != iRet )
         log_softerror_and_alarm("[HWP] Failed to init thread cpu affinity on PID %d (for: %s) to core %d (out of %d cores). Error: %d (%s)", pid, szSource, iDesiredCore, hw_procs_get_cpu_count(), errno, strerror(errno));
      else
         log_line("[HWP] Init thread cpu affinity successfully on PID %d (for: %s), cpu affinity to core %d (out of %d cores)", pid, szSource, iDesiredCore, hw_procs_get_cpu_count());
   }
   else
      log_line("[HWP] Init thread: no CPU affinity was requested.");

   if ( iRet == 0 )
      log_line("[HWP] Init thread attributes successfully, on PID %d (for: %s), class: %s, raw prio: %d, rt prio: %d", pid, szSource, str_format_schedule_policy(iSchedulingClass), iRawPriority, params.sched_priority);
   return iRet;
}

void hw_log_current_thread_attributes(const char* szPrefix)
{
   if ( log_is_errors_only() )
      return;

   pid_t pid = getpid();
   //pid_t ppid = getppid();
   int tid = hw_get_current_thread_id();
   pthread_t thread = pthread_self();

   cpu_set_t cpuSet;
   CPU_ZERO(&cpuSet);

   size_t iStackSize = 0, iGuardSize = 0;
   void *pStackAddr = NULL;
   pthread_attr_t attr;
   struct sched_param sch_params;
   int iDetachState = 0;
   int iSchedulePolicy = 0;

   char szTmp[64];
   char szOutput[256];
   szOutput[0] = 0;

   if ( 0 != pthread_getattr_np(thread, &attr) )
      log_softerror_and_alarm("[HWP] Failed to get attributes for current thread %d, error: %d (%s)", thread, errno, strerror(errno));
   else
   {
      if ( 0 != pthread_attr_getdetachstate(&attr, &iDetachState) )
         log_softerror_and_alarm("[HWP] Failed to get detach state attr for current thread %d, error: %d (%s)", thread, errno, strerror(errno));
      else
         strcat(szOutput, (iDetachState == PTHREAD_CREATE_DETACHED)?"DETACHED, ":"JOINABLE, ");

      if ( 0 != pthread_attr_getschedpolicy(&attr, &iSchedulePolicy) )
         log_softerror_and_alarm("[HWP] Failed to get schedule policy attr for current thread %d, error: %d (%s)", thread, errno, strerror(errno));
      else
      {
         strcat(szOutput, str_format_schedule_policy(iSchedulePolicy));
         strcat(szOutput, ", ");
      }
      if ( 0 != pthread_attr_getschedparam(&attr, &sch_params) )
         log_softerror_and_alarm("[HWP] Failed to get schedule params attr for current thread %d, error: %d (%s)", thread, errno, strerror(errno));
      else
      {
         sprintf(szTmp, "sched priority: %d, ", sch_params.sched_priority);
         strcat(szOutput, szTmp);
      }
      if ( 0 != pthread_attr_getguardsize(&attr, &iGuardSize) )
         log_softerror_and_alarm("[HWP] Failed to get guard size attr for current thread %d, error: %d (%s)", thread, errno, strerror(errno));
      else
      {
         sprintf(szTmp, "guard: %d bytes, ", (int)iGuardSize);
         strcat(szOutput, szTmp);
      }
      if ( 0 != pthread_attr_getstack(&attr, &pStackAddr, &iStackSize) )
         log_softerror_and_alarm("[HWP] Failed to get stack size attr for current thread %d, error: %d (%s)", thread, errno, strerror(errno));
      else
      {
         sprintf(szTmp, "stack: %d bytes, ", (int)iStackSize);
         strcat(szOutput, szTmp);
      }
      pthread_attr_destroy(&attr);
   }

   if ( 0 != pthread_getaffinity_np(thread, sizeof(cpuSet), &cpuSet) )
      log_softerror_and_alarm("[HWP] Log thread: Failed to get current cpu affinity for current thread %d, error: %d (%s)", thread, errno, strerror(errno));
   else
   {
      strcat(szOutput, "CPU aff: ");
      for( size_t j = 0; j < CPU_SETSIZE; j++ )
      {
         if ( CPU_ISSET(j, &cpuSet) )
         {
            sprintf(szTmp, "%d,", (int)j);
            strcat(szOutput, szTmp);
         }
      }
      int k = strlen(szOutput);
      if ( (k > 1) && (szOutput[k-1] == ',') )
         szOutput[k-1] = 0;
      else if ( ( k > 2 ) && ((szOutput[k-1] == ' ') || (szOutput[k-1] == ',') || (szOutput[k-2] == ' ') || (szOutput[k-2] == ',')) )
         szOutput[k-2] = 0;
      sprintf(szTmp, " of %d cores", hw_procs_get_cpu_count());
      strcat(szOutput, szTmp);
   }
   int k = strlen(szOutput);
   if ( (k > 1) && (szOutput[k-1] == ',') )
      szOutput[k-1] = 0;
   else if ( ( k > 2 ) && ((szOutput[k-1] == ' ') || (szOutput[k-1] == ',') || (szOutput[k-2] == ' ') || (szOutput[k-2] == ',')) )
      szOutput[k-2] = 0;

   if ( (NULL != szPrefix) && (0 != szPrefix[0]) )
      log_line("[HWP] Current thread (%s) (%d of PID %d) props: %s", szPrefix, tid, pid, szOutput );
   else
      log_line("[HWP] Current thread (%d of PID %d) props: %s", tid, pid, szOutput );
}

void hw_set_process_affinity(const char* szProcName, int iExceptThreadId, int iCoreStart, int iCoreEnd)
{
   if ( NULL == szProcName || 0 == szProcName[0] )
   {
      log_softerror_and_alarm("[HWP] Tried to adjus affinity for NULL process");
      return;
   }

   if ( hw_procs_get_cpu_count() < 2 )
   {
      log_line("[HWP] No core affinity to adjust for [%s] as we are in a single core CPU.", szProcName);
      return;
   }

   if ( (iCoreStart < 0) && (iCoreEnd < 0) )
   {
      log_line("[HWP] No core affinity to adjust for [%s], core affinity is disabled for it.", szProcName);
      return;
   }

   char szPIDs[256];

   hw_process_get_pids(szProcName, szPIDs);
   removeTrailingNewLines(szPIDs);
   replaceNewLinesToSpaces(szPIDs);
   if ( strlen(szPIDs) < 3 )
   {
      log_softerror_and_alarm("[HWP] Failed to set process affinity for process [%s], no such process.", szProcName);
      return;
   }
   log_line("[HWP] Adjusting affinity for process [%s] (PIDs: %s) except thread id: %d ...", szProcName, szPIDs, iExceptThreadId);

   char* p = removeLeadingWhiteSpace(szPIDs);
   int iPID = atoi(p);

   if ( iPID < 100 )
   {
      log_softerror_and_alarm("[HWP] Failed to set process affinity for process [%s], invalid pid: %d.", szProcName, iPID);
      return;
   }

   /*
   char szComm[128];
   char szOutput[256];

   sprintf(szComm, "ls /proc/%d/task 2>/dev/null", iPID);
   hw_execute_bash_command_raw_silent(szComm, szOutput);
   
   if ( strlen(szOutput) < 3 )
   {
      log_softerror_and_alarm("[HWP] Failed to set process affinity for process [%s], invalid tasks: [%s].", szProcName, szOutput);
      return;
   }

   replaceNewLinesToSpaces(szOutput);
   char* pTmp = removeLeadingWhiteSpace(szOutput);

   log_line("[HWP] Child processes to adjust affinity for, for process [%s] %d: [%s]", szProcName, iPID, pTmp);
   do
   {
       int iTask = 0;
       if ( 1 != sscanf(pTmp, "%d", &iTask) )
       {
          log_softerror_and_alarm("[HWP] Failed to set process affinity for process [%s], invalid parsing tasks from [%s].", szProcName, pTmp);
          return;
       }
       if ( iTask < 100 )
       {
          log_softerror_and_alarm("[HWP] Failed to set process affinity for process [%s], read invalid parsing tasks (%d) from [%s].", szProcName, iTask, pTmp);
          return;
       }

       if ( iTask == iExceptThreadId )
          log_line("[HWP] Skip exception thread id: %d", iExceptThreadId);
       else
       {
          // Set affinity

          if ( iCoreStart >= hw_procs_get_cpu_count() )
             iCoreStart = hw_procs_get_cpu_count()-1;
          if ( iCoreEnd >= hw_procs_get_cpu_count() )
             iCoreEnd = hw_procs_get_cpu_count()-1;

          if ( iCoreStart == iCoreEnd )
          {
             sprintf(szComm, "taskset -cp %d %d 2>/dev/null", iCoreStart, iTask);
             hw_execute_bash_command(szComm, NULL);
          }
          else
          {
             sprintf(szComm, "taskset -cp %d-%d %d", iCoreStart, iCoreEnd, iTask);
             hw_execute_bash_command(szComm, NULL);        
          }
       }
       // Go to next task in string
       while ( (*pTmp) && (*pTmp != ' ') )
          pTmp++;

       while ( (*pTmp) == ' ' )
          pTmp++;
   }
   while(*pTmp);
   */
   cpu_set_t cpuSet;
   CPU_ZERO(&cpuSet);
   for( int i=iCoreStart; i<=iCoreEnd; i++ )
      CPU_SET(i, &cpuSet);

   if ( 0 != sched_setaffinity(iPID, sizeof(cpuSet), &cpuSet) )
      log_softerror_and_alarm("[HWP] Failed to set affinity using sched api, error: %d (%s)", errno, strerror(errno));
   else
      log_line("[HWP] Did set affinity using sched api.");

   log_line("[HWP] Done adjusting affinity for process [%s].", szProcName);
}

void hw_set_current_thread_affinity(const char* szLogPrefix, int iCoreStart, int iCoreEnd)
{
   if ( hw_procs_get_cpu_count() < 2 )
   {
      log_line("[HWP] No core affinity to adjust for current thread on [%s] as we are in a single core CPU.", szLogPrefix);
      return;
   }

   if ( (iCoreStart < 0) && (iCoreEnd < 0) )
   {
      log_line("[HWP] No core affinity to adjust for current thread on [%s], core affinity is disabled for it.", szLogPrefix);
      return;
   }

   if ( iCoreStart >= hw_procs_get_cpu_count() )
      iCoreStart = hw_procs_get_cpu_count()-1;
   if ( iCoreEnd >= hw_procs_get_cpu_count() )
      iCoreEnd = hw_procs_get_cpu_count()-1;

   pthread_t this_thread = pthread_self();
   cpu_set_t cpuSetGet;
   cpu_set_t cpuSetSet;
   CPU_ZERO(&cpuSetGet);
   CPU_ZERO(&cpuSetSet);

   for( int i=iCoreStart; i<=iCoreEnd; i++ )
      CPU_SET(i, &cpuSetSet);

   bool bDifferent = false;

   if ( 0 != pthread_getaffinity_np(this_thread, sizeof(cpuSetGet), &cpuSetGet) )
   {
      log_softerror_and_alarm("[HWP] Update affinities: Failed to get current cpu affinity for current thread %d, error: %d (%s)", this_thread, errno, strerror(errno));
      bDifferent = true;
   }
   else
   {
      for( size_t j = 0; j < CPU_SETSIZE; j++ )
      {
         if ( (CPU_ISSET(j, &cpuSetSet)) != (CPU_ISSET(j, &cpuSetGet)) )
         {
            bDifferent = true;
            break;
         }
      }      
   }

   if ( ! bDifferent )
   {
      log_line("[HWP] CPU affinity for current thread (%s) is unchanged, (on cores: %d-%d)", szLogPrefix, iCoreStart, iCoreEnd);
      return;
   }
   
   if ( 0 != pthread_setaffinity_np(this_thread, sizeof(cpuSetSet), &cpuSetSet) )
      log_line("[HWP] Failed to set cpu affinity for current thread (%s) to cores %d-%d, error: %d, (%s)", szLogPrefix, iCoreStart, iCoreEnd, errno, strerror(errno));
   else
      log_line("[HWP] Did set cpu affinity for current thread (%s) to cores: %d-%d", szLogPrefix, iCoreStart, iCoreEnd);
}

void hw_set_priority_current_proc(int iRawPriority)
{
   log_line("[HWP] Set current proc raw priority to: %d", iRawPriority);
   // 0,1 disabled
   // 2...100 RT priority
   // 101..140 nice priority
   
   if ( (iRawPriority < 2) || (iRawPriority > 139) )
      return;

   if ( (iRawPriority > 1) && (iRawPriority <= 100) )
      hw_set_current_thread_rt_priority("set_proc_prio", iRawPriority);
   else
      hw_set_current_proc_nice_priority("set_proc_prio", iRawPriority);
}


void hw_get_process_priority(const char* szProcName, char* szOutput)
{
   char szPIDs[128];
   char szComm[256];
   char szCommOut[1024];
   if ( NULL == szOutput )
      return;

   szOutput[0] = 0;
   if ( NULL == szProcName || 0 == szProcName[0] )
      return;

   if ( szProcName[0] == 'r' && szProcName[1] == 'u' )
      strcpy(szOutput, szProcName+5);
   else
      strcpy(szOutput, szProcName);
   strcat(szOutput, ": ");

   hw_process_get_pids(szProcName, szPIDs);
   removeTrailingNewLines(szPIDs);
   replaceNewLinesToSpaces(szPIDs);

   if ( strlen(szPIDs) <= 2 )
   {
      strcat(szOutput, "Not Running");
      return;
   }
   strcat(szOutput, "Running, ");

   char* p = removeLeadingWhiteSpace(szPIDs);
   char* t = p;
   while ( (*t) != 0 )
   {
      if ( (*t) == ' ' )
      {
         *t = 0;
         break;
      }
      t++;
   }
   sprintf(szComm, "cat /proc/%s/stat | awk '{print \"real-priority: \" $18+100 \", nice: \" $19}'", p);
   hw_execute_bash_command_raw(szComm, szCommOut);
   if ( 0 < strlen(szCommOut) )
      szCommOut[strlen(szCommOut)-1] = 0;
   strcat(szOutput, "pri.");
   strcat(szOutput, szCommOut+8);

   #ifdef HW_CAPABILITY_IONICE
   strcat(szOutput, ", io priority: ");

   sprintf(szComm, "ionice -p %s", p);
   hw_execute_bash_command_raw(szComm, szCommOut);
   if ( 0 < strlen(szCommOut) )
      szCommOut[strlen(szCommOut)-1] = 0;
   strcat(szOutput, szCommOut);
   #endif
   strcat(szOutput, ";");
}

// Returns current priority
int hw_get_current_thread_rt_priority(const char* szLogPrefix)
{
   char szTmp[2];
   szTmp[0] = 0;
   char* szPrefix = szTmp;
   if ( (NULL != szLogPrefix) && (0 != szLogPrefix[0]) )
     szPrefix = (char*)szLogPrefix;

   pthread_t this_thread = pthread_self();
   struct sched_param params;
   int policy = 0;
   int iRetValue = -1;
   int ret = 0;
   ret = pthread_getschedparam(this_thread, &policy, &params);
   if ( ret != 0 )
     log_softerror_and_alarm("[HWP] (%s) Failed to get schedule param", szPrefix);
   else
   {
      iRetValue = params.sched_priority;
      log_line("[HWP] (%s) Current thread policy/priority: %s/%d", szPrefix, str_format_schedule_policy(policy), params.sched_priority);
   }
   
   return iRetValue;
}

void hw_set_current_thread_raw_priority(const char* szLogPrefix, int iNewRawPriority)
{
   log_line("[HWP] Set current thread (%s) raw priority to: %d", szLogPrefix, iNewRawPriority);
   // 0,1 disabled
   // 2...100 RT priority
   // 101..140 nice priority
   
   // If 0, set to SCHED_OTHER, prio 0
   if ( iNewRawPriority < 1 )
   {
      char szTmp[2];
      szTmp[0] = 0;
      char* szPrefix = szTmp;
      if ( (NULL != szLogPrefix) && (0 != szLogPrefix[0]) )
        szPrefix = (char*)szLogPrefix;

      pthread_t this_thread = pthread_self();
      struct sched_param params;
      int policy = 0;
      int ret = 0;
      ret = pthread_getschedparam(this_thread, &policy, &params);
      if ( ret != 0 )
        log_softerror_and_alarm("[HWP] (%s) Failed to get schedule param", szPrefix);
      else
         log_line("[HWP] (%s) Current thread policy/priority: %s/%d", szPrefix, str_format_schedule_policy(policy), params.sched_priority);

      params.sched_priority = 0;
      ret = pthread_setschedparam(this_thread, SCHED_OTHER, &params);
      if ( ret != 0 )
         log_softerror_and_alarm("[HWP] (%s) Failed to set thread schedule class, error: %d, %s", szPrefix, errno, strerror(errno));

      ret = pthread_getschedparam(this_thread, &policy, &params);
      if ( ret != 0 )
        log_softerror_and_alarm("[HWP] (%s) Failed to get schedule param", szPrefix);
      log_line("[HWP] (%s) Current new thread policy/priority: %s/%d", szPrefix, str_format_schedule_policy(policy), params.sched_priority);
   }
   else if ( (iNewRawPriority > 1) && (iNewRawPriority <= 100) )
      hw_set_current_thread_rt_priority(szLogPrefix, iNewRawPriority);
   else if ( iNewRawPriority < 140 )
      hw_set_current_thread_nice_priority(szLogPrefix, iNewRawPriority);
}

void hw_set_current_thread_rt_priority(const char* szLogPrefix, int iNewRawPriority)
{
   char szTmp[2];
   szTmp[0] = 0;
   char* szPrefix = szTmp;
   if ( (NULL != szLogPrefix) && (0 != szLogPrefix[0]) )
     szPrefix = (char*)szLogPrefix;

   pthread_t this_thread = pthread_self();
   struct sched_param params;
   int policy = 0;
   int ret = 0;
   ret = pthread_getschedparam(this_thread, &policy, &params);
   if ( ret != 0 )
     log_softerror_and_alarm("[HWP] (%s) Failed to get schedule param", szPrefix);
   else
      log_line("[HWP] (%s) Current thread policy/priority: %s/%d", szPrefix, str_format_schedule_policy(policy), params.sched_priority);

   params.sched_priority = 0;
   if ( (iNewRawPriority > 1) && (iNewRawPriority <= 100) )
      params.sched_priority = 100 - iNewRawPriority;
   ret = pthread_setschedparam(this_thread, SCHED_FIFO, &params);
   if ( ret != 0 )
      log_softerror_and_alarm("[HWP] (%s) Failed to set thread schedule class, error: %d, %s", szPrefix, errno, strerror(errno));

   ret = pthread_getschedparam(this_thread, &policy, &params);
   if ( ret != 0 )
     log_softerror_and_alarm("[HWP] (%s) Failed to get schedule param", szPrefix);
   log_line("[HWP] (%s) Current new thread policy/priority: %s/%d", szPrefix, str_format_schedule_policy(policy), params.sched_priority);
}

void hw_set_current_thread_nice_priority(const char* szLogPrefix, int iNewRawPriority)
{
   nice(iNewRawPriority-120);
   log_line("[HWP] Did set nice value for current thred (%s) to: %d", szLogPrefix, iNewRawPriority-120);
}

void hw_set_current_proc_nice_priority(const char* szLogPrefix, int iNewRawPriority)
{
   // First set scheduiling class

   char szTmp[2];
   szTmp[0] = 0;
   char* szPrefix = szTmp;
   if ( (NULL != szLogPrefix) && (0 != szLogPrefix[0]) )
     szPrefix = (char*)szLogPrefix;

   pthread_t this_thread = pthread_self();
   struct sched_param params;
   int policy = 0;
   int ret = 0;
   ret = pthread_getschedparam(this_thread, &policy, &params);
   if ( ret != 0 )
     log_softerror_and_alarm("[HWP] (%s) Failed to get schedule param", szPrefix);
   else
      log_line("[HWP] (%s) Current thread policy/priority: %s/%d", szPrefix, str_format_schedule_policy(policy), params.sched_priority);

   params.sched_priority = 0;
   ret = pthread_setschedparam(this_thread, SCHED_OTHER, &params);
   if ( ret != 0 )
      log_softerror_and_alarm("[HWP] (%s) Failed to set thread schedule class, error: %d, %s", szPrefix, errno, strerror(errno));

   ret = pthread_getschedparam(this_thread, &policy, &params);
   if ( ret != 0 )
     log_softerror_and_alarm("[HWP] (%s) Failed to get schedule param", szPrefix);
   log_line("[HWP] (%s) Current new thread policy/priority: %s/%d", szPrefix, str_format_schedule_policy(policy), params.sched_priority);


   if ( (iNewRawPriority > 100) && (iNewRawPriority < 140) )
   {
      char szComm[256];
      int iPID = getpid();
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "renice -n %d -p %d", iNewRawPriority-120, iPID);
      hw_execute_bash_command(szComm, NULL);
    
      setpriority(PRIO_PROCESS, 0, 120-iNewRawPriority);
   }
}