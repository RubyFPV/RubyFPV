#pragma once
#include <pthread.h>
#ifdef __cplusplus
extern "C" {
#endif 

int hw_execute_process(const char *szCommand, int iTimeoutMs, char* szOutput, int iMaxOutputLength);
int hw_execute_process_wait(const char *szCommand);
int hw_process_exists(const char* szProcName);
char* hw_process_get_pids_inline(const char* szProcName);
void hw_process_get_pids(const char* szProcName, char* szOutput);

void hw_stop_process(const char* szProcName);
int hw_kill_process(const char* szProcName, int iSignal);

void hw_set_priority_current_proc(int nice);
void hw_set_proc_priority(const char* szProcName, int nice, int ionice, int waitForProcess);
void hw_get_proc_priority(const char* szProcName, char* szOutput);

void hw_set_proc_affinity(const char* szProcName, int iExceptThreadId, int iCoreStart, int iCoreEnd);

int hw_execute_bash_command_nonblock(const char* command, char* outBuffer);
int hw_execute_bash_command(const char* command, char* outBuffer);
int hw_execute_bash_command_timeout(const char* command, char* outBuffer, u32 uTimeoutMs);
int hw_execute_bash_command_silent(const char* command, char* outBuffer);
int hw_execute_bash_command_raw(const char* command, char* outBuffer);
int hw_execute_bash_command_raw_timeout(const char* command, char* outBuffer, u32 uTimeoutMs);
int hw_execute_bash_command_raw_silent(const char* command, char* outBuffer);

void hw_execute_ruby_process(const char* szPrefixes, const char* szProcess, const char* szParams, char* szOutput);
void hw_execute_ruby_process_wait(const char* szPrefixes, const char* szProcess, const char* szParams, char* szOutput, int iWait);

void hw_init_worker_thread_attrs(pthread_attr_t* pAttr, const char* szSource);
int hw_get_current_thread_priority(const char* szLogPrefix);
int hw_increase_current_thread_priority(const char* szLogPrefix, int iNewPriority);

#ifdef __cplusplus
}  
#endif