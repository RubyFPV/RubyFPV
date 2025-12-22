#pragma once
#include "../base/base.h"
#include "../base/config.h"

bool hardware_file_check_and_fix_access(char* szFullFileName);
long hardware_file_get_file_size(const char* szFullFileName);
void hardware_file_replace_extension(const char* szFile, const char* szNewExtension);
void hardware_files_check_config_folder();

#ifdef __cplusplus
extern "C" {
#endif 

void hardware_files_init();

void hardware_mount_root();
void hardware_mount_boot();
int hardware_get_free_space_kb();
int hardware_get_free_space_kb_async(int iCPUCore);
int hardware_has_finished_get_free_space_kb_async();

int hardware_try_mount_usb();
void hardware_unmount_usb();
char* hardware_get_mounted_usb_name();

#ifdef __cplusplus
}
#endif
