/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga petrusoroaga@yahoo.com
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

#include <fcntl.h>        // serialport
#include <termios.h>      // serialport
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/joystick.h>
#include <errno.h>
#include <stdlib.h>

#include "base.h"
#include "hardware.h"
#include "gpio.h"
#include "config.h"
#include "hardware_procs.h"
#include "hardware_camera.h"
#include "hardware_files.h"
#include "hardware_i2c.h"
#include "../common/string_utils.h"
#if defined(HW_PLATFORM_RADXA)
#include <SDL2/SDL.h>
#endif


#ifdef HW_CAPABILITY_GPIO
static int s_iButtonsWhereInited = 0;
#endif

static int s_ihwSwapButtons = 0;
int sLastReadMenu = 0;
int sLastReadBack = 0;
int sLastReadPlus = 0;
int sLastReadMinus = 0;
int sLastReadQA1 = 0;
int sLastReadQA2 = 0;
int sLastReadQA22 = 0;
int sLastReadQA3 = 0;
int sLastReadQAPlus = 0;
int sLastReadQAMinus = 0;

int sKeyMenuPressed = 0;
int sKeyBackPressed = 0;
int sKeyPlusPressed = 0;
int sKeyMinusPressed = 0;
int sKeyQA1Pressed = 0;
int sKeyQA2Pressed = 0;
int sKeyQA22Pressed = 0;
int sKeyQA3Pressed = 0;
int sKeyQAPlusPressed = 0;
int sKeyQAMinusPressed = 0;

u32 keyMenuDownStartTime = 0;
u32 keyMenuInitialDownStartTime = 0;
u32 keyBackDownStartTime = 0;
u32 keyPlusDownStartTime = 0;
u32 keyMinusDownStartTime = 0;
u32 keyPlusInitialDownStartTime = 0;
u32 keyMinusInitialDownStartTime = 0;
u32 keyQA1DownStartTime = 0;
u32 keyQA2DownStartTime = 0;
u32 keyQA22DownStartTime = 0;
u32 keyQA3DownStartTime = 0;
u32 keyQAPlusDownStartTime = 0;
u32 keyQAMinusDownStartTime = 0;

int sInitialReadQA1 = 0;
int sInitialReadQA2 = 0;
int sInitialReadQA22 = 0;
int sInitialReadQA3 = 0;
int sInitialReadQAPlus = 0;
int sInitialReadQAMinus = 0;

static u32 s_long_key_press_delta = 700;
#ifdef HW_CAPABILITY_GPIO
static u32 s_long_press_repeat_time = 120;
#endif
int s_bBlockCurrentPressedKeys = 0;

bool s_bHardwareWasSetup = false;
u32 s_uTimeInitHardware = 0;
bool s_bDoHardwareInitializationLedSequence = false;

bool s_bIsLedRedOn = false;
bool s_bIsLedRedBlinking = false;
bool s_bIsLedRedBlinkingFast = false;
u32 s_uTimeLedRedBlinkUntil = 0;
u32 s_uTimeLedRedLastBlink = 0;

bool s_bIsLedGreenOn = false;
bool s_bIsLedGreenBlinking = false;
bool s_bIsLedGreenBlinkingFast = false;
u32 s_uTimeLedGreenBlinkUntil = 0;
u32 s_uTimeLedGreenLastBlink = 0;

bool s_bHardwareDetectedBoardType = false;
u32  s_uHardwareBoardType = 0;
bool s_bHarwareHasDetectedSystemType = false;
int  s_iHardwareSystemIsVehicle = 0;

int s_iHardwareJoystickCount = 0;
hw_joystick_info_t s_HardwareJoystickInfo[MAX_JOYSTICK_INTERFACES];

void _hardware_detectSystemType()
{
   log_line("[Hardware] Detecting system type...");
   
   char szBuff[256];
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_CONFIG_SYSTEM_TYPE);
   FILE* fd = fopen(szFile, "r");
   if ( NULL != fd )
   {
      if ( 2 == fscanf(fd, "%d %u", &s_iHardwareSystemIsVehicle, &s_uHardwareBoardType) )
      {
         s_bHarwareHasDetectedSystemType = true;
         log_line("[Hardware] Loaded system Type: %s, board type: %s", s_iHardwareSystemIsVehicle?"[vehicle]":"[controller]", str_get_hardware_board_name(s_uHardwareBoardType));
         fclose(fd);
         return;
      }
      log_softerror_and_alarm("[Hardware] Failed to parse system type config file [%s]", szFile); 
      fclose(fd);
   }
   log_line("[Hardware] Do full detection of board and system type...");
   hw_execute_bash_command_raw("cat /proc/device-tree/model", szBuff);
   log_line("[Hardware] Board description string: %s", szBuff);

   s_uHardwareBoardType = hardware_getBoardType();
   strncpy(szBuff, str_get_hardware_board_name(s_uHardwareBoardType), 255);
   if ( szBuff[0] == 0 )
      strcpy(szBuff, "N/A");
   log_line("[Hardware] Detected board type: %s", szBuff);

   int iDoAditionalChecks = 1;
   s_iHardwareSystemIsVehicle = 0;

   #if defined (HW_CAPABILITY_GPIO)
   #if defined (HW_PLATFORM_RASPBERRY)
   int val = GPIORead(GPIOGetPinDetectVehicle());
   if ( val == 1 )
   {
      log_line("[Hardware] Detected GPIO signal to start as vehicle or relay.");
      s_iHardwareSystemIsVehicle = 1;
      iDoAditionalChecks = 0;
   }
   
   val = GPIORead(GPIOGetPinDetectController());
   if ( val == 1 )
   {
      log_line("[Hardware] Detected GPIO signal to start as controller.");
      s_iHardwareSystemIsVehicle = 0;
      iDoAditionalChecks = 0;
   }
   #endif
   #endif
   
   #if defined (HW_PLATFORM_RASPBERRY) || defined (HW_PLATFORM_RADXA)
   if( access( FILE_FORCE_VEHICLE, R_OK ) != -1 )
   {
      log_line("[Hardware] Detected file %s to force start as vehicle or relay.", FILE_FORCE_VEHICLE);
      s_iHardwareSystemIsVehicle = 1;
      iDoAditionalChecks = 0;
   }   
   strcpy(szFile, FOLDER_WINDOWS_PARTITION);
   strcat(szFile, "forcevehicle.txt");
   if( access( szFile, R_OK ) != -1 )
   {
      log_line("[Hardware] Detected file %s to force start as vehicle or relay.", szFile);
      s_iHardwareSystemIsVehicle = 1;
      iDoAditionalChecks = 0;
   }   
   if( access( FILE_FORCE_CONTROLLER, R_OK ) != -1 )
   {
      log_line("[Hardware] Detected file %s to force start as controller.", FILE_FORCE_CONTROLLER);
      s_iHardwareSystemIsVehicle = 0;
      iDoAditionalChecks = 0;
   }   
   #endif

   #if defined (HW_CAPABILITY_I2C)
   hw_execute_bash_command("modprobe i2c-dev", NULL);

   #if defined (HW_PLATFORM_RASPBERRY)
   // Initialize I2C bus 0 for different boards types
   log_line("[Hardware] Initialize I2C busses...");
   char szComm[256];
   char szOutput[4096];
   sprintf(szComm, "current_dir=$PWD; cd %s/; ./camera_i2c_config 2>/dev/null; cd $current_dir", VEYE_COMMANDS_FOLDER);
   hw_execute_bash_command(szComm, szOutput);
   strcat(szOutput, "\n*END*\n");
   log_line("[Hardware] I2C config output:");
   log_line(szOutput);
   if ( ((s_uHardwareBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_PI3APLUS) ||
        ((s_uHardwareBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_PI3B) ||
        ((s_uHardwareBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_PI3BPLUS) ||
        ((s_uHardwareBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_PI4B) )
   {
      log_line("[Hardware] Initializing I2C busses for Pi 3/4...");
      hw_execute_bash_command("raspi-gpio set 0 ip", NULL);
      hw_execute_bash_command("raspi-gpio set 1 ip", NULL);
      hw_execute_bash_command("raspi-gpio set 44 ip", NULL);
      hw_execute_bash_command("raspi-gpio set 44 a1", NULL);
      hw_execute_bash_command("raspi-gpio set 45 ip", NULL);
      hw_execute_bash_command("raspi-gpio set 45 a1", NULL);
      hardware_sleep_ms(200);
      hw_execute_bash_command("i2cdetect -y 0 0x0F 0x0F", NULL);
      hardware_sleep_ms(200);
      log_line("[Hardware] Done initializing I2C busses for Pi 3/4.");
   }
   #endif
   #endif

   if ( iDoAditionalChecks )
   if( access( FILE_FORCE_VEHICLE_NO_CAMERA, R_OK ) != -1 )
   {
      log_line("[Hardware] Detected file %s to force start as vehicle or relay with no camera.", FILE_FORCE_VEHICLE_NO_CAMERA);
      s_iHardwareSystemIsVehicle = 1;
   }   

   #if defined (HW_PLATFORM_RASPBERRY) || defined (HW_PLATFORM_OPENIPC_CAMERA)
   if ( iDoAditionalChecks )
   if ( hardware_hasCamera() && (hardware_getCameraType() != CAMERA_TYPE_NONE) )
   {
      log_line("[Hardware] Has Camera.");
      s_iHardwareSystemIsVehicle = 1;
   }
   #endif
   
   if ( s_iHardwareSystemIsVehicle )
   {
      if ( 0 == hardware_hasCamera() )
         log_line("[Hardware] Detected system as vehicle/relay without camera.");
      else
         log_line("[Hardware] Detected system as vehicle/relay with camera.");
   }
   else
      log_line("[Hardware] Detected system as controller.");

   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_CONFIG_SYSTEM_TYPE);
   fd = fopen(szFile, "w");
   if ( NULL != fd )
   {
      fprintf(fd, "%d %u\n", s_iHardwareSystemIsVehicle, s_uHardwareBoardType);
      fclose(fd);
   }

   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_CONFIG_BOARD_TYPE);
   fd = fopen(szFile, "w");
   if ( NULL == fd )
      log_softerror_and_alarm("[Hardware] Failed to save board configuration to file: %s", szFile);
   else
   {
      fprintf(fd, "%u %s\n", s_uHardwareBoardType, szBuff);
      fclose(fd);
   }


   #if defined (HW_PLATFORM_RASPBERRY)
   fd = fopen("/boot/ruby_board.txt", "w");
   if ( NULL != fd )
   {
      fprintf(fd, "%u\n", s_uHardwareBoardType);
      fclose(fd);
   }
   hw_execute_bash_command("cat /proc/device-tree/model > /boot/ruby_board_desc.txt", NULL);
   #endif

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   hw_execute_bash_command("cat /proc/device-tree/model > /root/ruby/config/ruby_board_desc.txt", NULL);
   #endif

   log_line("[Hardware] Detected system Type: %s", s_iHardwareSystemIsVehicle?"[vehicle]":"[controller]");

   s_bHarwareHasDetectedSystemType = true;
}

int init_hardware()
{
   log_force_full_log();
   
   log_line("[Hardware] Start Initialization...");
   s_uTimeInitHardware = get_current_timestamp_ms();

   hardware_files_init();

   int failed = 0;

   #ifdef HW_CAPABILITY_GPIO
   int failedButtons = GPIOInitButtons();
   if ( failedButtons )
   {
      #if defined (HW_PLATFORM_RASPBERRY)
      log_softerror_and_alarm("[Hardware] Failed to set GPIOs for buttons (for Raspberry)");
      #else
      log_softerror_and_alarm("[Hardware] Failed to set GPIOs for buttons (for Radxa)");
      #endif
      failed = 1;
   }
   else
   {
      #if defined (HW_PLATFORM_RASPBERRY)
      log_line("[Hardware] Initialized GPIO buttons (for Raspberry)");
      #else
      log_line("[Hardware] Initialized GPIO buttons (for Radxa)");
      #endif
      s_iButtonsWhereInited = 1;
   }

   log_line("[Hardware] Pins for buzzer: %d, vehicle detect: %d, controller detect: %d",
      GPIOGetPinBuzzer(), GPIOGetPinDetectVehicle(), GPIOGetPinDetectController());

   if ( (GPIOGetPinBuzzer() > 0) && (-1 == GPIOExport(GPIOGetPinBuzzer())) )
   {
      log_line("[Hardware] Failed to get GPIO access to pin for buzzer.");
      failed = 1;
   }

   if ( (GPIOGetPinBuzzer() > 0) && (-1 == GPIODirection(GPIOGetPinBuzzer(), OUT)) )
   {
      log_line("[Hardware] Failed set GPIO configuration for pin buzzer.");
      failed = 1;
   }

   if ( (GPIOGetPinDetectVehicle() > 0) && (-1 == GPIOExport(GPIOGetPinDetectVehicle())) )
   {
      log_line("[Hardware] Failed to get GPIO access to pin VehicleType.");
      failed = 1;
   }

   if ( (GPIOGetPinDetectVehicle() > 0) && (-1 == GPIODirection(GPIOGetPinDetectVehicle(), IN)) )
   {
      log_line("[Hardware] Failed set GPIO configuration for pin VehicleType.");
      failed = 1;
   }

   if ( (GPIOGetPinDetectController() > 0) && (-1 == GPIOExport(GPIOGetPinDetectController())) )
   {
      log_line("[Hardware] Failed to get GPIO access to pin ControllerType.");
      failed = 1;
   }

   if ( (GPIOGetPinDetectController() > 0) && (-1 == GPIODirection(GPIOGetPinDetectController(), IN)) )
   {
      log_line("[Hardware] Failed set GPIO configuration for pin ControllerType.");
      failed = 1;
   }

   if ( GPIOGetPinLedRed() > 0 )
   {
      if ( -1 == GPIOExport(GPIOGetPinLedRed()) )
      {
         log_line("[Hardware] Failed set GPIO configuration for pin for LED red (GPIO: %d)", GPIOGetPinLedRed());
         failed = 1;
      }
      
      if ( -1 == GPIODirection(GPIOGetPinLedRed(), OUT) )
      {
         log_line("[Hardware] Failed set GPIO configuration for pin for LED red (GPIO: %d)", GPIOGetPinLedRed());
         failed = 1;
      }
      else
         log_line("[Hardware] Did set GPIO configuration for pin for LED red (GPIO: %d)", GPIOGetPinLedRed());
   }
   else
      log_line("[Hardware] No GPIO for led red.");

   if ( GPIOGetPinLedGreen() > 0 )
   {
      if ( -1 == GPIOExport(GPIOGetPinLedGreen()) )
      {
         log_line("[Hardware] Failed set GPIO configuration for pin for LED green (GPIO: %d)", GPIOGetPinLedGreen());
         failed = 1;
      }
      
      if ( -1 == GPIODirection(GPIOGetPinLedGreen(), OUT) )
      {
         log_line("[Hardware] Failed set GPIO configuration for pin for LED green (GPIO: %d)", GPIOGetPinLedGreen());
         failed = 1;
      }
      else
         log_line("[Hardware] Did set GPIO configuration for pin for LED green (GPIO: %d)", GPIOGetPinLedGreen());
   }
   else
      log_line("[Hardware] No GPIO for led green.");

  
   #ifdef HW_PLATFORM_RASPBERRY
   char szBuff[64];
   if ( GPIOGetPinDetectVehicle() > 0 )
   {
      sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinDetectVehicle());
      hw_execute_bash_command_silent(szBuff, NULL);
      sprintf(szBuff, "gpio -g mode %d down", GPIOGetPinDetectVehicle());
      hw_execute_bash_command_silent(szBuff, NULL);
   }
   if ( GPIOGetPinDetectController() > 0 )
   {
      sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinDetectController());
      hw_execute_bash_command_silent(szBuff, NULL);
      sprintf(szBuff, "gpio -g mode %d down", GPIOGetPinDetectController());
      hw_execute_bash_command_silent(szBuff, NULL);
   }
   #endif

   if ( GPIOGetPinBuzzer() > 0 )
      GPIOWrite(GPIOGetPinBuzzer(), LOW);

   log_line("[Hardware] GPIO setup successfully.");

   if ( s_iButtonsWhereInited )
   {
      sInitialReadQA1 = GPIORead(GPIOGetPinQA1());
      sInitialReadQA2 = GPIORead(GPIOGetPinQA2());
      sInitialReadQA22 = GPIORead(GPIOGetPinQA22());
      sInitialReadQA3 = GPIORead(GPIOGetPinQA3());
      sInitialReadQAPlus = GPIORead(GPIOGetPinQAPlus());
      sInitialReadQAMinus = GPIORead(GPIOGetPinQAMinus());
      log_line("[Hardware] Initial read of Quick Actions buttons: %d %d-%d %d, [%d, %d]", sInitialReadQA1, sInitialReadQA2, sInitialReadQA22, sInitialReadQA3, sInitialReadQAPlus, sInitialReadQAMinus);
   }
   else
      log_line("[Hardware] GPIO menu buttons where not setup yet.");
   #endif

   s_iHardwareJoystickCount = 0;
   s_bHardwareWasSetup = true;
   s_bDoHardwareInitializationLedSequence = true;

   log_line("[Hardware] Initialization complete. %s.", failed?"Failed":"No errors");
   log_regular_mode();
   
   if ( failed )
      return 0;
   return 1;
}

int init_hardware_only_detection_pins()
{
   s_uTimeInitHardware = get_current_timestamp_ms();

   #ifdef HW_CAPABILITY_GPIO
   if (-1 == GPIOExport(GPIOGetPinDetectVehicle()))
   {
      log_line("[Hardware] Failed to get GPIO access to Ruby type PIN.");
      return(0);
   }

   if (-1 == GPIODirection(GPIOGetPinDetectVehicle(), IN))
   {
      log_line("[Hardware] Failed set GPIO configuration for Ruby type PIN.");
      return(0);
   }

   if (-1 == GPIOExport(GPIOGetPinDetectController()) )
   {
      log_line("[Hardware] Failed to get GPIO access to Ruby type C PIN.");
      return(0);
   }

   if (-1 == GPIODirection(GPIOGetPinDetectController(), IN))
   {
      log_line("[Hardware] Failed set GPIO configuration for Ruby type C PIN.");
      return(0);
   }

   #ifdef HW_PLATFORM_RASPBERRY
   char szBuff[64];
   if ( GPIOGetPinDetectVehicle() > 0 )
   {
      sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinDetectVehicle());
      hw_execute_bash_command_silent(szBuff, NULL);
      sprintf(szBuff, "gpio -g mode %d down", GPIOGetPinDetectVehicle());
      hw_execute_bash_command_silent(szBuff, NULL);
   }
   if ( GPIOGetPinDetectController() > 0 )
   {
      sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinDetectController());
      hw_execute_bash_command_silent(szBuff, NULL);
      sprintf(szBuff, "gpio -g mode %d down", GPIOGetPinDetectController());
      hw_execute_bash_command_silent(szBuff, NULL);
   }
   #endif
   
   log_line("[Hardware] GPIO setup successfully.");
   #endif
   s_iHardwareJoystickCount = 0;
   s_bHardwareWasSetup = true;
   return 1;
}

void hardware_reboot()
{
   log_line("[Hardware] Entered reboot sequence...");
   hardware_sleep_ms(200);
   hw_execute_bash_command("sync", NULL);
   hardware_sleep_ms(500);
   hw_execute_bash_command_nonblock("reboot -f", NULL);
   log_softerror_and_alarm("[Hardware] Done executing reboot command");
   int iCounter = 0;
   // reboot -f --no-wall
   // reboot --reboot --no-wall
   while (1)
   {
      hardware_sleep_ms(500);
      iCounter++;
      if ( iCounter > 4 )
      {
         log_softerror_and_alarm("[Hardware] Regular reboot failed. Force reboot...");
         #if defined (HW_PLATFORM_OPENIPC_CAMERA)
         hw_execute_bash_command_nonblock("reboot -f", NULL);
         #else
         hw_execute_bash_command_nonblock("reboot -f --no-wall", NULL);
         #endif

         #if defined (HW_PLATFORM_OPENIPC_CAMERA)
         hw_execute_bash_command("reboot -f", NULL);
         #else
         hw_execute_bash_command("reboot -f --no-wall", NULL);
         #endif

         iCounter = 0;
      }
   }
}


void hardware_release()
{
   #ifdef HW_CAPABILITY_GPIO
   GPIOUnexport(GPIOGetPinMenu());
   GPIOUnexport(GPIOGetPinBack());
   GPIOUnexport(GPIOGetPinMinus());
   GPIOUnexport(GPIOGetPinPlus());
   GPIOUnexport(GPIOGetPinQA1());
   GPIOUnexport(GPIOGetPinQA2());
   GPIOUnexport(GPIOGetPinQA22());
   GPIOUnexport(GPIOGetPinQA3());
   GPIOUnexport(GPIOGetPinQAPlus());
   GPIOUnexport(GPIOGetPinQAMinus());
   GPIOUnexport(GPIOGetPinBuzzer());
   GPIOUnexport(GPIOGetPinLedRed());
   GPIOUnexport(GPIOGetPinLedGreen());
   GPIOUnexport(GPIOGetPinDetectVehicle());
   GPIOUnexport(GPIOGetPinDetectController());
   #endif
   log_line("[Hardware] Released!");
}

void hardware_swap_buttons(int swap)
{
   s_ihwSwapButtons = swap;
}

void _hardware_detect_board_openipc(char* szBoardId)
{
   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   s_uHardwareBoardType = BOARD_TYPE_OPENIPC_GOKE200;
   hw_execute_bash_command_raw("ipcinfo -c 2>/dev/null", szBoardId);
   log_line("[Hardware] Detected board type: (%s)", szBoardId);
   if ( NULL != strstr(szBoardId, "gk72") )
   {
      s_uHardwareBoardType = BOARD_TYPE_OPENIPC_GOKE200;
      if ( NULL != strstr(szBoardId, "v210") )
         s_uHardwareBoardType = BOARD_TYPE_OPENIPC_GOKE210;
      if ( NULL != strstr(szBoardId, "v300") )
         s_uHardwareBoardType = BOARD_TYPE_OPENIPC_GOKE300;
   }
   if ( NULL != strstr(szBoardId, "ssc338") )
      s_uHardwareBoardType = BOARD_TYPE_OPENIPC_SIGMASTAR_338Q;
   if ( NULL != strstr(szBoardId, "ssc33x") )
      s_uHardwareBoardType = BOARD_TYPE_OPENIPC_SIGMASTAR_338Q;
   #endif
}

void _hardware_detect_board_radxa(char* szBoardId)
{
   #if defined (HW_PLATFORM_RADXA)
   s_uHardwareBoardType = BOARD_TYPE_RADXA_ZERO3;
   char szOutput[4096];

   if ( NULL != szBoardId )
   {
      hw_execute_bash_command_raw("uname -r", szOutput);
      removeTrailingNewLines(szOutput);
      strncpy(szBoardId, szOutput, 31);
      szBoardId[31] = 0;
   }

   hw_execute_bash_command_raw("uname -a", szOutput);
   removeTrailingNewLines(szOutput);
   if ( NULL != strstr(szOutput, "radxa3c") )
      s_uHardwareBoardType = BOARD_TYPE_RADXA_3C;

   hw_execute_bash_command_raw("lscpu | grep Model", szOutput);
   if ( NULL != strstr(szOutput, "Cortex-A55") )
   {
      bool bVRx = false;
      if ( access("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq", R_OK) == -1 )
         bVRx = true;
      if ( access("/home/radxa/ruby", R_OK) != -1 )
      if ( access("/home/8812eu_radxa.ko", R_OK) != -1 )
      if ( (access("/home/radxa/ruby/drivers", R_OK) == -1) || bVRx )
      {
         hw_execute_bash_command_raw("lsusb | grep 0bda:a81a", szOutput);
         char* pId = strstr(szOutput, "0bda:a81a");
         if ( NULL != pId )
         {
            pId += 8;
            pId = strstr(pId, "0bda:a81a");
            if ( NULL != pId )
            {
               log_line("[Hardware] Detected RunCam VRx board.");
               s_uHardwareBoardType = BOARD_TYPE_RADXA_RUNCAM_VRX;
            }
         }
      }
   }
   #endif
}

u32 hardware_detectBoardType()
{
   if ( s_bHardwareDetectedBoardType )
   {
      log_line("[Hardware] Detected board %d (%s)", s_uHardwareBoardType, str_get_hardware_board_name(s_uHardwareBoardType));
      return s_uHardwareBoardType;
   }
   s_bHardwareDetectedBoardType = true;

   char szBoardId[64];
   szBoardId[0] = 0;
   #if defined (HW_PLATFORM_RASPBERRY)
   hw_execute_bash_command("cat /proc/cpuinfo | grep 'Revision' | awk '{print $3}'", szBoardId);
   removeTrailingNewLines(szBoardId);
   log_line("[Hardware] Detected board Id: (%s)", szBoardId);

   if ( strcmp(szBoardId, "a03111") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBoardId, "a03112") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBoardId, "a03113") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBoardId, "a03114") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBoardId, "a03115") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBoardId, "a03116") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBoardId, "b03111") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBoardId, "b03112") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBoardId, "b03114") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBoardId, "b03115") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBoardId, "b03116") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBoardId, "c03111") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBoardId, "c03112") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBoardId, "c03114") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBoardId, "c03115") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBoardId, "c03116") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBoardId, "d03114") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBoardId, "d03115") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBoardId, "d03116") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI4B;}

   if ( strcmp(szBoardId, "9020e0") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI3APLUS;}
   if ( strcmp(szBoardId, "29020e0") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI3APLUS;}

   if ( strcmp(szBoardId, "a02082") == 0 )  { s_uHardwareBoardType = BOARD_TYPE_PI3B;}
   if ( strcmp(szBoardId, "a22082") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI3B;}
   if ( strcmp(szBoardId, "a32082") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI3B;}
   if ( strcmp(szBoardId, "a52082") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI3B;}
   if ( strcmp(szBoardId, "a020d3") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI3BPLUS;}
   if ( strcmp(szBoardId, "2a02082") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI3B;}
   if ( strcmp(szBoardId, "2a22082") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI3B;}
   if ( strcmp(szBoardId, "2a32082") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI3B;}
   if ( strcmp(szBoardId, "2a52082") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI3B;}
   if ( strcmp(szBoardId, "2a020d3") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI3BPLUS;}

   if ( strcmp(szBoardId, "900093") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PIZERO;}
   if ( strcmp(szBoardId, "9000c1") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PIZEROW;}
   if ( strcmp(szBoardId, "900092") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PIZERO;}
   if ( strcmp(szBoardId, "1900093") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PIZERO;}
   if ( strcmp(szBoardId, "19000c1") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PIZEROW;}
   if ( strcmp(szBoardId, "2900092") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PIZERO;}
   if ( strcmp(szBoardId, "2900093") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PIZERO;}
   if ( strcmp(szBoardId, "29000c1") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PIZEROW;}

   if ( strcmp(szBoardId, "902120") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PIZERO2;}
   if ( strcmp(szBoardId, "2902120") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PIZERO2;}

   if ( strcmp(szBoardId, "920092") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PIZERO;}
   if ( strcmp(szBoardId, "920093") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PIZERO;}
   if ( strcmp(szBoardId, "2920092") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PIZERO;}
   if ( strcmp(szBoardId, "2920093") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PIZERO;}

   if ( strcmp(szBoardId, "a01040") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI2B;}
   if ( strcmp(szBoardId, "a21041") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI2BV11;}
   if ( strcmp(szBoardId, "a01041") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI2BV11;}
   if ( strcmp(szBoardId, "a22042") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI2BV12;}

   if ( strcmp(szBoardId, "2a01040") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI2B;}
   if ( strcmp(szBoardId, "2a21041") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI2BV11;}
   if ( strcmp(szBoardId, "2a01041") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI2BV11;}
   if ( strcmp(szBoardId, "2a22042") == 0 ) { s_uHardwareBoardType = BOARD_TYPE_PI2BV12;}
   #endif

   #if defined (HW_PLATFORM_RADXA)
   _hardware_detect_board_radxa(szBoardId);
   #endif

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   _hardware_detect_board_openipc(szBoardId);
   #endif

   char szBoardName[128];
   strncpy(szBoardName, str_get_hardware_board_name(s_uHardwareBoardType), 127);
   if ( szBoardName[0] == 0 )
      strcpy(szBoardName, "N/A");
   log_line("[Hardware] Detected board first time, board type for id (%s) (int: %d): %s", szBoardId, s_uHardwareBoardType, szBoardName);
   return s_uHardwareBoardType;
}

u32 hardware_getBoardType()
{
   if ( ! s_bHardwareDetectedBoardType )
      hardware_detectBoardType();
   static bool s_bLogBoardTypeOnce = false;
   if ( ! s_bLogBoardTypeOnce )
      log_line("[Hardware] Board type is %d (%s)", s_uHardwareBoardType & BOARD_TYPE_MASK, str_get_hardware_board_name(s_uHardwareBoardType));
   s_bLogBoardTypeOnce = true;
   return s_uHardwareBoardType;
}


static u32 s_uBoardSubTypesPopulatedIndexes[256];
static int s_iBoardSubTypesCount = 0;

void _hardware_compute_board_subtypes_populated_indexes()
{
   s_iBoardSubTypesCount = 0;
   s_uBoardSubTypesPopulatedIndexes[s_iBoardSubTypesCount++] = BOARD_SUBTYPE_OPENIPC_GENERIC;
   s_uBoardSubTypesPopulatedIndexes[s_iBoardSubTypesCount++] = BOARD_SUBTYPE_OPENIPC_GENERIC_30KQ;
   s_uBoardSubTypesPopulatedIndexes[s_iBoardSubTypesCount++] = BOARD_SUBTYPE_OPENIPC_AIO_ULTRASIGHT;
   s_uBoardSubTypesPopulatedIndexes[s_iBoardSubTypesCount++] = BOARD_SUBTYPE_OPENIPC_AIO_MARIO;
   s_uBoardSubTypesPopulatedIndexes[s_iBoardSubTypesCount++] = BOARD_SUBTYPE_OPENIPC_AIO_RUNCAM_V1;
   s_uBoardSubTypesPopulatedIndexes[s_iBoardSubTypesCount++] = BOARD_SUBTYPE_OPENIPC_AIO_RUNCAM_V2;
   s_uBoardSubTypesPopulatedIndexes[s_iBoardSubTypesCount++] = BOARD_SUBTYPE_OPENIPC_AIO_EMAX_MINI;
   s_uBoardSubTypesPopulatedIndexes[s_iBoardSubTypesCount++] = BOARD_SUBTYPE_OPENIPC_AIO_EMAX;
   s_uBoardSubTypesPopulatedIndexes[s_iBoardSubTypesCount++] = BOARD_SUBTYPE_OPENIPC_AIO_THINKER;
   s_uBoardSubTypesPopulatedIndexes[s_iBoardSubTypesCount++] = BOARD_SUBTYPE_OPENIPC_AIO_THINKER_E;
}

int hardware_get_board_subtypes_count()
{
   if ( 0 == s_iBoardSubTypesCount )
      _hardware_compute_board_subtypes_populated_indexes();
   return s_iBoardSubTypesCount;
}

u32 hardware_get_board_subtype_at_index(int iIndex)
{
   if ( 0 == s_iBoardSubTypesCount )
      _hardware_compute_board_subtypes_populated_indexes();

   if ( (iIndex < 0) || (iIndex >= s_iBoardSubTypesCount) )
      return BOARD_SUBTYPE_OPENIPC_UNKNOWN;
   return s_uBoardSubTypesPopulatedIndexes[iIndex];
}

int hardware_get_board_subtype_index(u32 uBoardSubType)
{
   if ( 0 == s_iBoardSubTypesCount )
      _hardware_compute_board_subtypes_populated_indexes();
   for( int i=0; i<s_iBoardSubTypesCount; i++ )
   {
      if ( s_uBoardSubTypesPopulatedIndexes[i] == uBoardSubType )
         return i;
   }
   return -1;
}

void hardware_detectBoardAndSystemType()
{
   if ( ! s_bHarwareHasDetectedSystemType )
      _hardware_detectSystemType();
   
   if ( s_iHardwareSystemIsVehicle )
   {
      if ( 0 == hardware_hasCamera() )
         log_line("[Hardware] Detected system as vehicle/relay without camera.");
      else
         log_line("[Hardware] Detected system as vehicle/relay with camera.");
   }
   else
      log_line("[Hardware] Detected system as controller.");
}

int hardware_board_is_raspberry(u32 uBoardType)
{
   if ( (uBoardType & BOARD_TYPE_MASK) > 0 )
   if ( (uBoardType & BOARD_TYPE_MASK) <= BOARD_TYPE_PI4B )
      return 1;
   return 0;
}

int hardware_board_is_radxa(u32 uBoardType)
{
   if ( ((uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_RADXA_ZERO3) ||
        ((uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_RADXA_3C) ||
        ((uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_RADXA_RUNCAM_VRX) )
      return 1;
   return 0; 
}

int hardware_board_is_openipc(u32 uBoardType)
{
   if ( hardware_board_is_goke(uBoardType) )
      return 1;
   if ( (uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_SIGMASTAR_338Q )
      return 1;
   return 0;
}

int hardware_board_is_goke(u32 uBoardType)
{
   if ( (uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE200 ||
        (uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE210 ||
        (uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE300 )
      return 1; 
   return 0;
}

int hardware_board_is_sigmastar(u32 uBoardType)
{
   if ( (uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_SIGMASTAR_338Q )
      return 1;
   return 0;
}

bool _hardware_enum_joystick_query_device(const char* szDevicePath, int iDeviceIndex, hw_joystick_info_t* pJoystickInfo)
{
   if ( (NULL == szDevicePath) || (0 == szDevicePath[0]) || (NULL == pJoystickInfo) )
      return false;
   if( access( szDevicePath, R_OK ) == -1 )
      return false;

   int fd = open(szDevicePath, O_NONBLOCK);
   if ( fd < 0 )
   {
      log_softerror_and_alarm("[Hardware] Failed to open joystick device file: [%s], error: %d, %s", szDevicePath, errno, strerror(errno));
      return false;
   }

   log_line("[Hardware] Enumerating capabilities of joystick device file: [%s]...", szDevicePath);

   char name[256];
   if (ioctl(fd, JSIOCGNAME(sizeof(name)), name) < 0)
      strncpy(name, "Unknown", sizeof(name));

   u32 uid = 0;
   if (ioctl(fd, JSIOCGVERSION, &uid) == -1)
      log_softerror_and_alarm("[Hardware] Error to query for joystick UID");

   u8 count_axes = 0;
   if (ioctl(fd, JSIOCGAXES, &count_axes) == -1)
      log_softerror_and_alarm("[Hardware] Error to query for joystick axes");

   u8 count_buttons = 0;
   if (ioctl(fd, JSIOCGBUTTONS, &count_buttons) == -1)
      log_softerror_and_alarm("[Hardware] Error to query for joystick buttons");

   close(fd);

   if ( count_axes > MAX_JOYSTICK_AXES )
      count_axes = MAX_JOYSTICK_AXES;
   if ( count_buttons > MAX_JOYSTICK_BUTTONS )
      count_buttons = MAX_JOYSTICK_BUTTONS;

   for( int k=0; k<(int)strlen(name); k++ )
      uid += name[k]*k;
   pJoystickInfo->deviceIndex = iDeviceIndex;
   strcpy(pJoystickInfo->szName, name);
   pJoystickInfo->uId = uid;
   pJoystickInfo->countAxes = count_axes;
   pJoystickInfo->countButtons = count_buttons;
   for( int k=0; k<MAX_JOYSTICK_AXES; k++ )
      pJoystickInfo->axesValues[k] = 0;
   for( int k=0; k<MAX_JOYSTICK_BUTTONS; k++ )
      pJoystickInfo->buttonsValues[k] = 0;
   pJoystickInfo->fd = -1;
   pJoystickInfo->pObject = NULL;

   return true;
}

void hardware_enum_joystick_interfaces()
{
   log_line("----------------------------------------------------------------");
   log_line("|  [Hardware] Enumerating hardware HID interfaces...            |");

   s_iHardwareJoystickCount = 0;
   char szDevName[256];
   for( int i=0; i<MAX_JOYSTICK_INTERFACES; i++ )
   {
      s_HardwareJoystickInfo[i].deviceIndex = -1;
      s_HardwareJoystickInfo[i].szName[0] = 0;
      s_HardwareJoystickInfo[i].uId = 0;
      s_HardwareJoystickInfo[i].countAxes = 0;
      s_HardwareJoystickInfo[i].countButtons = 0;
   }

   #if defined (HW_PLATFORM_RASPBERRY)
   for( int i=0; i<MAX_JOYSTICK_INTERFACES; i++ )
   {
      sprintf(szDevName, "/dev/input/js%d", i);
      if ( ! _hardware_enum_joystick_query_device(szDevName, i, &(s_HardwareJoystickInfo[s_iHardwareJoystickCount])) )
         continue;
   
      log_line("|  Found joystick interface [%s]: Name: %s, UID: %u, has %d axes and %d buttons.", szDevName, s_HardwareJoystickInfo[s_iHardwareJoystickCount].szName, s_HardwareJoystickInfo[s_iHardwareJoystickCount].uId, s_HardwareJoystickInfo[s_iHardwareJoystickCount].countAxes, s_HardwareJoystickInfo[s_iHardwareJoystickCount].countButtons);
      s_iHardwareJoystickCount++;
   }
   #endif
   #if defined (HW_PLATFORM_RADXA)
   SDL_JoystickEventState(SDL_ENABLE);
   if ( SDL_Init(SDL_INIT_JOYSTICK) < 0 )
      log_softerror_and_alarm("[Hardware] Failed to init SDL for joystick.");
   else
   {
      int iCountJoysticks = SDL_NumJoysticks();
      if ( iCountJoysticks < 0 )
         log_softerror_and_alarm("[Hardware] Failed to query SDL for joysticks count.");
      else if ( iCountJoysticks > 0 )
      {
         SDL_JoystickEventState(SDL_ENABLE);

         SDL_Joystick *pSDLJoystick = SDL_JoystickOpen(0);
         if ( NULL == pSDLJoystick )
            log_softerror_and_alarm("[Hardware] Failed to get joystick object from SDL.");
         else
         {
            strcpy(szDevName, "/dev/input/js-sdl");
            const char *szName = SDL_JoystickName(pSDLJoystick);
            if ( (NULL == szName) || (0 == szName[0]) )
               strcpy(s_HardwareJoystickInfo[s_iHardwareJoystickCount].szName, "Generic Joystick");
            else
               strncpy(s_HardwareJoystickInfo[s_iHardwareJoystickCount].szName, szName, MAX_JOYSTICK_INTERFACE_NAME-1);
            s_HardwareJoystickInfo[s_iHardwareJoystickCount].szName[MAX_JOYSTICK_INTERFACE_NAME-1] = 0;

            s_HardwareJoystickInfo[s_iHardwareJoystickCount].countAxes = SDL_JoystickNumAxes(pSDLJoystick);
            s_HardwareJoystickInfo[s_iHardwareJoystickCount].countButtons = SDL_JoystickNumButtons(pSDLJoystick);
            if ( s_HardwareJoystickInfo[s_iHardwareJoystickCount].countAxes > MAX_JOYSTICK_AXES )
               s_HardwareJoystickInfo[s_iHardwareJoystickCount].countAxes = MAX_JOYSTICK_AXES;
            if ( s_HardwareJoystickInfo[s_iHardwareJoystickCount].countButtons > MAX_JOYSTICK_BUTTONS )
               s_HardwareJoystickInfo[s_iHardwareJoystickCount].countButtons = MAX_JOYSTICK_BUTTONS;

            u32 uId = 0x00F2F3F4;
            for( int k=0; k<(int)strlen(s_HardwareJoystickInfo[s_iHardwareJoystickCount].szName); k++ )
                uId += s_HardwareJoystickInfo[s_iHardwareJoystickCount].szName[k]*k;

            s_HardwareJoystickInfo[s_iHardwareJoystickCount].deviceIndex = 0;
            s_HardwareJoystickInfo[s_iHardwareJoystickCount].uId = uId;

            for( int k=0; k<MAX_JOYSTICK_AXES; k++ )
               s_HardwareJoystickInfo[s_iHardwareJoystickCount].axesValues[k] = 0;
            for( int k=0; k<MAX_JOYSTICK_BUTTONS; k++ )
               s_HardwareJoystickInfo[s_iHardwareJoystickCount].buttonsValues[k] = 0;
            s_HardwareJoystickInfo[s_iHardwareJoystickCount].fd = -1;
            s_HardwareJoystickInfo[s_iHardwareJoystickCount].pObject = NULL;
            SDL_JoystickClose(pSDLJoystick);

            log_line("|  Found joystick interface [%s]: Name: %s, UID: %u, has %d axes and %d buttons.", szDevName, s_HardwareJoystickInfo[s_iHardwareJoystickCount].szName, s_HardwareJoystickInfo[s_iHardwareJoystickCount].uId, s_HardwareJoystickInfo[s_iHardwareJoystickCount].countAxes, s_HardwareJoystickInfo[s_iHardwareJoystickCount].countButtons);
            log_line("[Hardware] Successfully added a SDL joystick.");
            s_iHardwareJoystickCount++;
         }
      }
   }
   #endif
   log_line("|  [Hardware] Enumerating hardware HID interfaces result: found %d input interfaces. |", s_iHardwareJoystickCount);
   log_line("-------------------------------------------------------------------------------------");
}

int hardware_get_joystick_interfaces_count()
{
   return s_iHardwareJoystickCount;
}

hw_joystick_info_t* hardware_get_joystick_info(int index)
{
   if ( (index < 0) || (index >= s_iHardwareJoystickCount) )
      return NULL;
   return &s_HardwareJoystickInfo[index];
}

int hardware_open_joystick(int joystickIndex)
{
   if ((joystickIndex < 0) || (joystickIndex >= s_iHardwareJoystickCount) )
      return 0;
   if ( s_HardwareJoystickInfo[joystickIndex].deviceIndex < 0 )
      return 0;

   #ifdef HW_PLATFORM_RASPBERRY

   if ( s_HardwareJoystickInfo[joystickIndex].fd > 0 )
   {
      log_softerror_and_alarm("[Hardware] Opening HID interface /dev/input/js%d (it's already opened)", s_HardwareJoystickInfo[joystickIndex].deviceIndex);
      return 1;
   }
   char szDevName[256];
   sprintf(szDevName, "/dev/input/js%d", s_HardwareJoystickInfo[joystickIndex].deviceIndex);
   if( access( szDevName, R_OK ) == -1 )
   {
      log_softerror_and_alarm("[Hardware] Failed to access HID interface /dev/input/js%d !", s_HardwareJoystickInfo[joystickIndex].deviceIndex);
      return 0;
   }
   s_HardwareJoystickInfo[joystickIndex].fd = open(szDevName, O_RDONLY | O_NONBLOCK);
   if ( s_HardwareJoystickInfo[joystickIndex].fd <= 0 )
   {
      log_softerror_and_alarm("[Hardware] Failed to open HID interface /dev/input/js%d !", s_HardwareJoystickInfo[joystickIndex].deviceIndex);
      s_HardwareJoystickInfo[joystickIndex].fd = -1;
      return 0;
   }
   log_line("[Hardware] Opened HID interface /dev/input/js%d;", s_HardwareJoystickInfo[joystickIndex].deviceIndex);
   return 1;
   #endif

   #if defined (HW_PLATFORM_RADXA)
   if ( NULL != s_HardwareJoystickInfo[joystickIndex].pObject )
   {
      log_softerror_and_alarm("[Hardware] Opening HID interface SDL joystick %d (it's already opened)", s_HardwareJoystickInfo[joystickIndex].deviceIndex);
      return 1;
   }
   SDL_JoystickEventState(SDL_ENABLE);
   SDL_Joystick *pSDLJoystick = SDL_JoystickOpen(0);
   if ( NULL == pSDLJoystick )
   {
      log_softerror_and_alarm("[Hardware] Failed to open HID interface SDL joystick %d !", s_HardwareJoystickInfo[joystickIndex].deviceIndex);
      s_HardwareJoystickInfo[joystickIndex].fd = -1;
      s_HardwareJoystickInfo[joystickIndex].pObject = NULL;
      return 0;
   }
   s_HardwareJoystickInfo[joystickIndex].pObject = pSDLJoystick;
   log_line("[Hardware] Opened HID interface SDL joystick %d", s_HardwareJoystickInfo[joystickIndex].deviceIndex);
   return 1;
   #endif
   return 0;
}

void hardware_close_joystick(int joystickIndex)
{
   if ((joystickIndex < 0) || (joystickIndex >= s_iHardwareJoystickCount) )
      return;
   #ifdef HW_PLATFORM_RASPBERRY
   if ( -1 == s_HardwareJoystickInfo[joystickIndex].fd )
      log_softerror_and_alarm("[Hardware] Closing HID interface /dev/input/js%d (it's already closed)", s_HardwareJoystickInfo[joystickIndex].deviceIndex);
   else
      close(s_HardwareJoystickInfo[joystickIndex].fd);
   #endif

   #if defined (HW_PLATFORM_RADXA)
   if ( NULL == s_HardwareJoystickInfo[joystickIndex].pObject )
      log_softerror_and_alarm("[Hardware] Closing HID interface SDL joystick %d (it's already closed)", s_HardwareJoystickInfo[joystickIndex].deviceIndex);
   else
      SDL_JoystickClose((SDL_Joystick*)s_HardwareJoystickInfo[joystickIndex].pObject);
   #endif
   s_HardwareJoystickInfo[joystickIndex].pObject = NULL;
   s_HardwareJoystickInfo[joystickIndex].fd = -1;
   log_line("[Hardware] Closed HID interface /dev/input/js%d;", s_HardwareJoystickInfo[joystickIndex].deviceIndex);
}

// Return 1 if joystick file is opened
int hardware_is_joystick_opened(int joystickIndex)
{
   if ( (joystickIndex < 0) || (joystickIndex >= s_iHardwareJoystickCount) )
      return 0;
   if ( (-1 == s_HardwareJoystickInfo[joystickIndex].fd) && (NULL == s_HardwareJoystickInfo[joystickIndex].pObject) )
      return 0;
   return 1;
}

// Returns the count of new events
// Return -1 on error

int hardware_read_joystick(int joystickIndex, int miliSec)
{
   if ((joystickIndex < 0) || (joystickIndex >= s_iHardwareJoystickCount) )
      return -1;
   if ( s_HardwareJoystickInfo[joystickIndex].deviceIndex < 0 )
      return -1;
   if ( (-1 == s_HardwareJoystickInfo[joystickIndex].fd) && (NULL == s_HardwareJoystickInfo[joystickIndex].pObject) )
      return -1;

   memcpy( &s_HardwareJoystickInfo[joystickIndex].buttonsValuesPrev, &s_HardwareJoystickInfo[joystickIndex].buttonsValues, MAX_JOYSTICK_BUTTONS*sizeof(int));
   memcpy( &s_HardwareJoystickInfo[joystickIndex].axesValuesPrev, &s_HardwareJoystickInfo[joystickIndex].axesValues, MAX_JOYSTICK_AXES*sizeof(int));

   #ifdef HW_PLATFORM_RASPBERRY
   if ( -1 == s_HardwareJoystickInfo[joystickIndex].fd )
      return -1;

   int countEvents = 0;
   u32 timeStart = get_current_timestamp_micros();
   u32 timeEnd = timeStart + miliSec*1000;
   if ( timeEnd < timeStart )
      timeEnd = timeStart;

   while ( get_current_timestamp_micros() < timeEnd )
   { 
      hardware_sleep_micros(200);
      struct js_event joystickEvent[8];
      int iRead = read(s_HardwareJoystickInfo[joystickIndex].fd, &joystickEvent[0], sizeof(joystickEvent));
      if ( iRead == 0 )
         continue;
      if ( iRead < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) )
         continue;
      if ( iRead < 0 )
      {
         log_softerror_and_alarm("[Hardware] Error on reading joystick data, joystick index: %d, error: %d", joystickIndex, errno);
         hardware_close_joystick(joystickIndex);
         return -1;
      }
      int count = iRead / sizeof(joystickEvent[0]);
      for( int i=0; i<count; i++ )
      {
         if ( (joystickEvent[i].type & ~JS_EVENT_INIT) == JS_EVENT_BUTTON )
         if ( joystickEvent[i].number >= 0 && joystickEvent[i].number < MAX_JOYSTICK_BUTTONS )
         {
            s_HardwareJoystickInfo[joystickIndex].buttonsValues[joystickEvent[i].number] = joystickEvent[i].value;
            countEvents++;
         }
         if ( (joystickEvent[i].type & ~JS_EVENT_INIT) == JS_EVENT_AXIS )
         if ( joystickEvent[i].number >= 0 && joystickEvent[i].number < MAX_JOYSTICK_AXES )
         {
            s_HardwareJoystickInfo[joystickIndex].axesValues[joystickEvent[i].number] = joystickEvent[i].value;
            countEvents++;
         }
      }
   }
   return countEvents;
   #endif

   #if defined (HW_PLATFORM_RADXA)
   if ( NULL == s_HardwareJoystickInfo[joystickIndex].pObject )
      return -1;

   int iCount = s_HardwareJoystickInfo[joystickIndex].countButtons * 2 + s_HardwareJoystickInfo[joystickIndex].countAxes * 2;
   SDL_Event event;
   while ((iCount > 0) && (SDL_PollEvent(&event) != 0))
   {
      iCount--;
   }

   int iCountEvents = 0;
   SDL_Joystick *pSDLJoystick = (SDL_Joystick*) s_HardwareJoystickInfo[joystickIndex].pObject;
   for( int i=0; i<s_HardwareJoystickInfo[joystickIndex].countButtons; i++ )
   {
      s_HardwareJoystickInfo[joystickIndex].buttonsValues[i] = SDL_JoystickGetButton(pSDLJoystick, i);
      if ( s_HardwareJoystickInfo[joystickIndex].buttonsValues[i] != s_HardwareJoystickInfo[joystickIndex].buttonsValuesPrev[i] )
         iCountEvents++;
   }
   for( int i=0; i<s_HardwareJoystickInfo[joystickIndex].countAxes; i++ )
   {
      s_HardwareJoystickInfo[joystickIndex].axesValues[i] = SDL_JoystickGetAxis(pSDLJoystick, i);
      if ( s_HardwareJoystickInfo[joystickIndex].axesValues[i] != s_HardwareJoystickInfo[joystickIndex].axesValuesPrev[i] )
         iCountEvents++;
   }
   return iCountEvents;
   #endif

   return -1;
}

u16 hardware_get_flags()
{
   u16 retValue = 0xFFFF;

   #ifdef HW_PLATFORM_RASPBERRY
   char szOut[1024];
   hw_execute_bash_command_silent("vcgencmd get_throttled", szOut);
   int len = strlen(szOut)-1;
   while (len > 0 )
   {
      if ( szOut[len] == '=')
         break;
      len--;
   }
   if ( szOut[len] == '=' )
   {
      unsigned long ul = 0;
      if ( 1 == sscanf(szOut+len+1, "0x%lX", &ul) )
      {
         u8 u = (ul & 0xF) | ((ul>>16) << 4);
         retValue = u;
      }
   }
   #else
   retValue = 0;
   #endif
   return retValue;
}

void gpio_read_buttons_loop()
{
   if ( ! s_bHardwareWasSetup )
      return;

   #ifdef HW_CAPABILITY_GPIO

   int iForceMenuPressed = 0;
   int iForceBackPressed = 0;
   int iForceQA1Pressed = 0;

   // Check inputs
   int rMenu = GPIORead(GPIOGetPinMenu());
   int rBack = GPIORead(GPIOGetPinBack());
   int rPlus = GPIORead(GPIOGetPinPlus());
   int rMinus = GPIORead(GPIOGetPinMinus());
   int rQA1 = GPIORead(GPIOGetPinQA1());
   int rQA2 = GPIORead(GPIOGetPinQA2());
   int rQA3 = GPIORead(GPIOGetPinQA3());
   #ifdef HW_PLATFORM_RASPBERRY
   int rQA22 = GPIORead(GPIOGetPinQA22());
   //int rQAPlus = 0;//GPIORead(GPIOGetPinQAPlus());
   //int rQAMinus = 0;//GPIORead(GPIOGetPinQAMinus());
   #endif

   u32 time_now = get_current_timestamp_ms();

   if ( GPIOGetButtonsPullDirection() == rMenu )
   {
      sKeyMenuPressed = 0;
      keyMenuDownStartTime = 0;
      keyMenuInitialDownStartTime = 0;
   }
   else if ( rMenu != sLastReadMenu )
   {
      sKeyMenuPressed = 1;
      keyMenuDownStartTime = time_now;
      keyMenuInitialDownStartTime = time_now;
   }
   else
      sKeyMenuPressed = 0;

   if ( ( GPIOGetButtonsPullDirection() != rMenu) && (0 != keyMenuDownStartTime) && (time_now > (keyMenuDownStartTime+s_long_key_press_delta+s_long_press_repeat_time)) )
   {
      sKeyMenuPressed = 1;
      keyMenuDownStartTime += s_long_press_repeat_time;
   }
   
   if ( GPIOGetButtonsPullDirection() == rBack )
   {
      sKeyBackPressed = 0;
      keyBackDownStartTime = 0;
   }
   else if ( rBack != sLastReadBack )
   {
      sKeyBackPressed = 1;
      keyBackDownStartTime = time_now;
   }
   else
      sKeyBackPressed = 0;

   if ( (GPIOGetButtonsPullDirection() != rBack) && 0 != keyBackDownStartTime && time_now > (keyBackDownStartTime+s_long_key_press_delta + s_long_press_repeat_time) )
   {
      sKeyBackPressed = 1;
      keyBackDownStartTime += s_long_press_repeat_time;
   }
    
   if ( GPIOGetButtonsPullDirection() == rPlus )
   {
      sKeyPlusPressed = 0;
      keyPlusDownStartTime = 0;
      keyPlusInitialDownStartTime = 0;
   }
   else if ( rPlus != sLastReadPlus )
   {
      sKeyPlusPressed = 1;
      keyPlusDownStartTime = time_now;
      keyPlusInitialDownStartTime = time_now;
   }
   else
   {
      sKeyPlusPressed = 0;
   }

   if ( (GPIOGetButtonsPullDirection() != rPlus) && (0 != keyPlusDownStartTime) && (time_now > (keyPlusDownStartTime+s_long_key_press_delta+s_long_press_repeat_time)) )
   {
      sKeyPlusPressed = 1;
      keyPlusDownStartTime += s_long_press_repeat_time;
   }
   
   if ( GPIOGetButtonsPullDirection() == rMinus )
   {
      sKeyMinusPressed = 0;
      keyMinusDownStartTime = 0;
      keyMinusInitialDownStartTime = 0;
   }
   else if ( rMinus != sLastReadMinus )
   {
      sKeyMinusPressed = 1;
      keyMinusDownStartTime = time_now;
      keyMinusInitialDownStartTime = time_now;
   }
   else
      sKeyMinusPressed = 0;

   if ( (GPIOGetButtonsPullDirection() != rMinus) && 0 != keyMinusDownStartTime && time_now > (keyMinusDownStartTime+s_long_key_press_delta+s_long_press_repeat_time) )
   {
      sKeyMinusPressed = 1;
      keyMinusDownStartTime += s_long_press_repeat_time;
   }

   if ( GPIOGetButtonsPullDirection() == rQA1 )
   {
      sKeyQA1Pressed = 0;
      keyQA1DownStartTime = 0;
   }
   else if ( rQA1 !=sLastReadQA1 )
   {
      sKeyQA1Pressed = 1;
      keyQA1DownStartTime = time_now;
   }
   else
      sKeyQA1Pressed = 0;

   if ( (GPIOGetButtonsPullDirection() != rQA1) && 0 != keyQA1DownStartTime && time_now > (keyQA1DownStartTime+s_long_key_press_delta+s_long_press_repeat_time) )
   {
      sKeyQA1Pressed = 1;
      keyQA1DownStartTime += s_long_press_repeat_time;
   }

   if ( GPIOGetButtonsPullDirection() == rQA2 )
   {
      sKeyQA2Pressed = 0;
      keyQA2DownStartTime = 0;
   }
   else if ( rQA2 != sLastReadQA2 )
   {
      sKeyQA2Pressed = 1;
      keyQA2DownStartTime = time_now;
   }
   else
      sKeyQA2Pressed = 0;

   if ( (GPIOGetButtonsPullDirection() != rQA2) && 0 != keyQA2DownStartTime && time_now > (keyQA2DownStartTime+s_long_key_press_delta+s_long_press_repeat_time) )
   {
      sKeyQA2Pressed = 1;
      keyQA2DownStartTime += s_long_press_repeat_time;
   }

   if ( GPIOGetButtonsPullDirection() == rQA3 )
   {
      sKeyQA3Pressed = 0;
      keyQA3DownStartTime = 0;
   }
   else if ( rQA3 != sLastReadQA3 )
   {
      sKeyQA3Pressed = 1;
      keyQA3DownStartTime = time_now;
   }
   else
      sKeyQA3Pressed = 0;

   if ( (GPIOGetButtonsPullDirection() != rQA3) && 0 != keyQA3DownStartTime && time_now > (keyQA3DownStartTime+s_long_key_press_delta+s_long_press_repeat_time) )
   {
      sKeyQA3Pressed = 1;
      keyQA3DownStartTime += s_long_press_repeat_time;
   }

   #ifdef HW_PLATFORM_RASPBERRY
   if ( GPIOGetButtonsPullDirection() == rQA22 )
   {
      sKeyQA22Pressed = 0;
      keyQA22DownStartTime = 0;
   }
   else if ( rQA22 != sLastReadQA22 )
   {
      sKeyQA22Pressed = 1;
      keyQA22DownStartTime = time_now;
   }
   else
      sKeyQA22Pressed = 0;

   if ( (GPIOGetButtonsPullDirection() != rQA22) && 0 != keyQA22DownStartTime && time_now > (keyQA22DownStartTime+s_long_key_press_delta+s_long_press_repeat_time) )
   {
      sKeyQA22Pressed = 1;
      keyQA22DownStartTime += s_long_press_repeat_time;
   }
   #endif
   /*
   if ( GPIOGetButtonsPullDirection() == rQAPlus )
   {
      sKeyQAPlusPressed = 0;
      keyQAPlusDownStartTime = 0;
   }
   else if ( rQAPlus != sLastReadQAPlus )
   {
      sKeyQAPlusPressed = 1;
      keyQAPlusDownStartTime = time_now;
   }
   else
      sKeyQAPlusPressed = 0;

   if ( (GPIOGetButtonsPullDirection() != rQAPlus) && 0 != keyQAPlusDownStartTime && time_now > (keyQAPlusDownStartTime+s_long_key_press_delta+s_long_press_repeat_time) )
   {
      sKeyQAPlusPressed = 1;
      keyQAPlusDownStartTime += s_long_press_repeat_time;
   }

   if ( GPIOGetButtonsPullDirection() == rQAMinus )
   {
      sKeyQAMinusPressed = 0;
      keyQAMinusDownStartTime = 0;
   }
   else if ( rQAMinus != sLastReadQAMinus )
   {
      sKeyQAMinusPressed = 1;
      keyQAMinusDownStartTime = time_now;
   }
   else
      sKeyQAMinusPressed = 0;

   if ( (GPIOGetButtonsPullDirection() != rQAMinus) && 0 != keyQAMinusDownStartTime && time_now > (keyQAMinusDownStartTime+s_long_key_press_delta+s_long_press_repeat_time) )
   {
      sKeyQAMinusPressed = 1;
      keyQAMinusDownStartTime += s_long_press_repeat_time;
   }
   */

   //printf("\n%d %d    %d %d %d %d\n", sKeyQA1Pressed,sKeyQA2Pressed, sKeyMenuPressed, sKeyBackPressed, sKeyPlusPressed, sKeyMinusPressed );

   if ( s_bBlockCurrentPressedKeys )
   {
      sKeyMenuPressed = 0;
      sKeyBackPressed = 0;
      sKeyPlusPressed = 0;
      sKeyMinusPressed = 0;
      sKeyQA1Pressed = 0;
      sKeyQA2Pressed = 0;
      sKeyQA22Pressed = 0;
      sKeyQA3Pressed = 0;
      sKeyQAMinusPressed = 0;
      sKeyQAMinusPressed = 0;

      if ( (GPIOGetButtonsPullDirection() == rMenu) && (GPIOGetButtonsPullDirection() != sLastReadMenu) )
         s_bBlockCurrentPressedKeys = 0;
      if ( (GPIOGetButtonsPullDirection() == rBack) && (GPIOGetButtonsPullDirection() != sLastReadBack) )
         s_bBlockCurrentPressedKeys = 0;
      if ( (GPIOGetButtonsPullDirection() == rPlus) && (GPIOGetButtonsPullDirection() != sLastReadPlus) )
         s_bBlockCurrentPressedKeys = 0;
      if ( (GPIOGetButtonsPullDirection() == rMinus) && (GPIOGetButtonsPullDirection() != sLastReadMinus) )
         s_bBlockCurrentPressedKeys = 0;
      if ( (GPIOGetButtonsPullDirection() == rQA1) && (GPIOGetButtonsPullDirection() != sLastReadQA1) )
         s_bBlockCurrentPressedKeys = 0;
      if ( (GPIOGetButtonsPullDirection() == rQA2) && (GPIOGetButtonsPullDirection() != sLastReadQA2) )
         s_bBlockCurrentPressedKeys = 0;
      if ( (GPIOGetButtonsPullDirection() == rQA3) && (GPIOGetButtonsPullDirection() != sLastReadQA3) )
         s_bBlockCurrentPressedKeys = 0;
      #ifdef HW_PLATFORM_RASPBERRY
      if ( (GPIOGetButtonsPullDirection() == rQA22) && (GPIOGetButtonsPullDirection() != sLastReadQA22) )
         s_bBlockCurrentPressedKeys = 0;
      #endif
      //if ( (GPIOGetButtonsPullDirection() == rQAMinus) && (GPIOGetButtonsPullDirection() != sLastReadQAMinus) )
      //   s_bBlockCurrentPressedKeys = 0;
      //if ( (GPIOGetButtonsPullDirection() == rQAPlus) && (GPIOGetButtonsPullDirection() != sLastReadQAPlus) )
      //   s_bBlockCurrentPressedKeys = 0;
   }
   sLastReadMenu = rMenu;
   sLastReadBack = rBack;
   sLastReadPlus = rPlus;
   sLastReadMinus = rMinus;
   sLastReadQA1 = rQA1;
   sLastReadQA2 = rQA2;
   sLastReadQA3 = rQA3;
   #ifdef HW_PLATFORM_RASPBERRY
   sLastReadQA22 = rQA22;
   #endif
   //sLastReadQAPlus = rQAPlus;
   //sLastReadQAMinus = rQAMinus;

   if ( iForceMenuPressed )
   {
      sKeyMenuPressed = 1;
      keyMenuDownStartTime = time_now;
      log_line("[Hardware] Set menu button as pressed after initial detection.");
   }
   if ( iForceBackPressed )
   {
      sKeyBackPressed = 1;
      keyBackDownStartTime = time_now;
      log_line("[Hardware] Set back button as pressed after initial detection.");
   }
   if ( iForceQA1Pressed )
   {
      sKeyQA1Pressed = 1;
      keyQA1DownStartTime = time_now;
      log_line("[Hardware] Set QA1 button as pressed after initial detection.");
   }
   #endif
}

void _hardware_leds_loop()
{
   if ( 0 == s_uTimeInitHardware )
      return;

   u32 uTimeNow = get_current_timestamp_ms();

   if ( s_bDoHardwareInitializationLedSequence )
   {
      if ( uTimeNow < (s_uTimeInitHardware + 5000 ) )
      {
         int delta = (uTimeNow - s_uTimeInitHardware)/200;
         int step = delta % 4;
         switch (step)
         {
             case 0: hardware_led_red_set_on(); hardware_led_green_set_on(); break;
             case 1: hardware_led_red_set_off(); hardware_led_green_set_off(); break;
             case 2: hardware_led_red_set_on(); hardware_led_green_set_on(); break;
             case 3: hardware_led_red_set_off(); hardware_led_green_set_off(); break;
         }
         log_line("[Hardware] Blinking boot leds, step %d", step);
         return;
      }
      log_line("[Hardware] Done doing initialization LEDs sequence.");
      s_bDoHardwareInitializationLedSequence = false;
      GPIOWrite(GPIOGetPinBuzzer(), LOW);
      hardware_led_red_set_off();
      hardware_led_green_set_off();
      if ( hardware_is_running_on_runcam_vrx() )
      {
         hardware_led_red_set_on();
         hardware_led_green_set_off();
      }
   }

   if ( s_bIsLedRedBlinking || s_bIsLedRedBlinkingFast )
   {
      if ( uTimeNow >= s_uTimeLedRedBlinkUntil )
      {
         if ( hardware_is_running_on_runcam_vrx() )
            hardware_led_red_set_on();
         else
            hardware_led_red_set_off();
      }
      else if ( (s_bIsLedRedBlinking && (uTimeNow >= s_uTimeLedRedLastBlink + 700)) ||
                (s_bIsLedRedBlinkingFast && (uTimeNow >= s_uTimeLedRedLastBlink + 200)) )
      {
          if ( s_bIsLedRedOn )
          {
             if ( hardware_is_running_on_runcam_vrx() )
                GPIOWrite(GPIOGetPinLedRed(), HIGH);
             else
                GPIOWrite(GPIOGetPinLedRed(), LOW);
          }
          else
          {
             if ( hardware_is_running_on_runcam_vrx() )
                GPIOWrite(GPIOGetPinLedRed(), LOW);
             else
                GPIOWrite(GPIOGetPinLedRed(), HIGH);
          }
          s_bIsLedRedOn = ! s_bIsLedRedOn;
          s_uTimeLedRedLastBlink = uTimeNow;
      }
   }

   if ( s_bIsLedGreenBlinking || s_bIsLedGreenBlinkingFast )
   {
      if ( uTimeNow >= s_uTimeLedGreenBlinkUntil )
      {
         if ( hardware_is_running_on_runcam_vrx() )
            hardware_led_green_set_on();
         else
            hardware_led_green_set_off();
      }
      else if ( (s_bIsLedGreenBlinking && (uTimeNow >= s_uTimeLedGreenLastBlink + 700)) ||
                (s_bIsLedGreenBlinkingFast && (uTimeNow >= s_uTimeLedGreenLastBlink + 200)) )
      {
          if ( s_bIsLedGreenOn )
          {
             if ( hardware_is_running_on_runcam_vrx() )
                GPIOWrite(GPIOGetPinLedGreen(), HIGH);
             else
                GPIOWrite(GPIOGetPinLedGreen(), LOW);
          }
          else
          {
             if ( hardware_is_running_on_runcam_vrx() )
                GPIOWrite(GPIOGetPinLedGreen(), LOW);
             else
                GPIOWrite(GPIOGetPinLedGreen(), HIGH);
          }
          s_bIsLedGreenOn = ! s_bIsLedGreenOn;
          s_uTimeLedGreenLastBlink = uTimeNow;
      }
   }
}

void hardware_loop()
{
   if ( ! s_bHardwareWasSetup )
      return;

   #ifdef HW_CAPABILITY_GPIO
   gpio_read_buttons_loop();
   _hardware_leds_loop();
   #endif
}

int isKeyMenuPressed() { return sKeyMenuPressed > 0; }
int isKeyBackPressed() { return sKeyBackPressed > 0; }

int isKeyPlusPressed()
{
   if ( s_ihwSwapButtons == 0 )
      return sKeyPlusPressed > 0;
   else
      return sKeyMinusPressed > 0;
}

int isKeyMinusPressed()
{
   if ( s_ihwSwapButtons == 0 )
      return sKeyMinusPressed > 0;
   else
      return sKeyPlusPressed > 0;
}

int isKeyMenuLongPressed()
{
   u32 time_now = get_current_timestamp_ms();
   if ( (sKeyMenuPressed > 0) && (0 != keyMenuDownStartTime) && (time_now > (keyMenuDownStartTime+s_long_key_press_delta)) )
      return 1;
   return 0;
}

int isKeyMenuLongLongPressed()
{
   u32 time_now = get_current_timestamp_ms();
   if ( (0 != keyMenuInitialDownStartTime) && (time_now > (keyMenuInitialDownStartTime+4*s_long_key_press_delta)) )
      return 1;
   return 0;
}

int isKeyBackLongPressed()
{
   u32 time_now = get_current_timestamp_ms();
   if ( (sKeyBackPressed > 0) && (0 != keyBackDownStartTime) && (time_now > (keyBackDownStartTime+s_long_key_press_delta)) )
      return 1;
   return 0;
}

int isKeyPlusLongPressed()
{
   u32 time_now = get_current_timestamp_ms();
   if ( s_ihwSwapButtons == 0 )
   {
      if ( (sKeyPlusPressed > 0) && (0 != keyPlusDownStartTime) && (time_now > (keyPlusDownStartTime+s_long_key_press_delta)) )
         return 1;
   }
   else
   {
      if ( (sKeyMinusPressed > 0) && (0 != keyMinusDownStartTime) && (time_now > (keyMinusDownStartTime+s_long_key_press_delta)) )
         return 1;
   }

   return 0;
}

int isKeyMinusLongPressed()
{
   u32 time_now = get_current_timestamp_ms();
   if ( s_ihwSwapButtons == 0 )
   {
      if ( (sKeyMinusPressed > 0) && (0 != keyMinusDownStartTime) && (time_now > (keyMinusDownStartTime+s_long_key_press_delta)) )
         return 1;
   }
   else
   {
      if ( (sKeyPlusPressed > 0) && (0 != keyPlusDownStartTime) && (time_now > (keyPlusDownStartTime+s_long_key_press_delta)) )
         return 1;
   }

   return 0;
}

int isKeyPlusLongLongPressed()
{
   u32 time_now = get_current_timestamp_ms();
   if ( s_ihwSwapButtons == 0 )
   {
      if ( (0 != keyPlusInitialDownStartTime) && (time_now > (keyPlusInitialDownStartTime+4*s_long_key_press_delta)) )
         return 1;
   }
   else
   {
      if ( (0 != keyMinusInitialDownStartTime) && (time_now > (keyMinusInitialDownStartTime+4*s_long_key_press_delta)) )
         return 1;
   }

   return 0;
}

int isKeyMinusLongLongPressed()
{
   u32 time_now = get_current_timestamp_ms();
   if ( s_ihwSwapButtons == 0 )
   {
      if ( (0 != keyMinusInitialDownStartTime) && (time_now > (keyMinusInitialDownStartTime+4*s_long_key_press_delta)) )
         return 1;
   }
   else
   {
      if ( (0 != keyPlusInitialDownStartTime) && (time_now > (keyPlusInitialDownStartTime+4*s_long_key_press_delta)) )
         return 1;
   }

   return 0;
}

int isKeyQA1Pressed()
{
   if ( sInitialReadQA1 > 0 )
      return 0;
   return (sKeyQA1Pressed > 0);
}

int isKeyQA2Pressed()
{
   int pressed = 0;
   if ( 0 == sInitialReadQA2 )
      pressed = pressed || (sKeyQA2Pressed > 0);
   if ( 0 == sInitialReadQA22 )
      pressed = pressed || (sKeyQA22Pressed > 0);
   return pressed;
}

int isKeyQA3Pressed()
{
   if ( sInitialReadQA3 > 0 )
      return 0;
   return (sKeyQA3Pressed > 0);
}

void hardware_override_keys(int iKeyMenu, int iKeyBack, int iKeyPlus, int iKeyMinus, int iKeyMenuLong, int iKeyBackLong, int iKeyPlusLong, int iKeyMinusLong)
{
   if ( iKeyMenu )
      sKeyMenuPressed = 1;
   if ( iKeyBack )
      sKeyBackPressed = 1;
   if ( iKeyPlus )
      sKeyPlusPressed = 1;
   if ( iKeyMinus )
      sKeyMinusPressed = 1;

   if ( iKeyMenuLong )
   {
      sKeyMenuPressed = 1;
      u32 time_now = get_current_timestamp_ms();
      keyMenuDownStartTime = time_now - 2000;
   }
   if ( iKeyBackLong )
   {
      sKeyBackPressed = 1;
      u32 time_now = get_current_timestamp_ms();
      keyBackDownStartTime = time_now - 2000;
   }
   if ( iKeyPlusLong )
   {
      sKeyPlusPressed = 1;
      u32 time_now = get_current_timestamp_ms();
      keyPlusDownStartTime =  time_now - 2000;
   }
   if ( iKeyMinusLong )
   {
      sKeyMinusPressed = 1;
      u32 time_now = get_current_timestamp_ms();
      keyMinusDownStartTime = time_now - 2000;
   }
}

void hardware_blockCurrentPressedKeys()
{
   s_bBlockCurrentPressedKeys = 1;

   sKeyMenuPressed = 0;
   sKeyBackPressed = 0;
   sKeyPlusPressed = 0;
   sKeyMinusPressed = 0;
   sKeyQA1Pressed = 0;
   sKeyQA2Pressed = 0;
   sKeyQA22Pressed = 0;
   sKeyQA3Pressed = 0;
}

int hardware_is_station()
{
   if ( ! s_bHarwareHasDetectedSystemType )
      _hardware_detectSystemType();
   return (s_iHardwareSystemIsVehicle==1)?0:1;
}

int hardware_is_vehicle()
{
   if ( ! s_bHarwareHasDetectedSystemType )
      _hardware_detectSystemType();
   return s_iHardwareSystemIsVehicle;
}

int hardware_is_running_on_openipc()
{
   if ( ! s_bHarwareHasDetectedSystemType )
      _hardware_detectSystemType();

   return hardware_board_is_openipc(hardware_getBoardType());
}

int hardware_is_running_on_runcam_vrx()
{
   if ( ! s_bHarwareHasDetectedSystemType )
      _hardware_detectSystemType();

   return (hardware_getBoardType() == BOARD_TYPE_RADXA_RUNCAM_VRX)?1:0;
}

void hardware_led_red_set_on()
{
   #ifdef HW_CAPABILITY_GPIO
   if ( hardware_is_running_on_runcam_vrx() )
      GPIOWrite(GPIOGetPinLedRed(), LOW);
   else
      GPIOWrite(GPIOGetPinLedRed(), HIGH);

   s_bIsLedRedOn = true;
   s_bIsLedRedBlinking = false;
   s_bIsLedRedBlinkingFast = false;
   s_uTimeLedRedBlinkUntil = 0;
   s_uTimeLedRedLastBlink = 0;
   #endif
}

void hardware_led_red_set_off()
{
   #ifdef HW_CAPABILITY_GPIO
   if ( hardware_is_running_on_runcam_vrx() )
      GPIOWrite(GPIOGetPinLedRed(), HIGH);
   else
      GPIOWrite(GPIOGetPinLedRed(), LOW);

   s_bIsLedRedOn = false;
   s_bIsLedRedBlinking = false;
   s_bIsLedRedBlinkingFast = false;
   s_uTimeLedRedBlinkUntil = 0;
   s_uTimeLedRedLastBlink = 0;
   #endif
}

void hardware_led_red_set_blinking(u32 uPeriodToBlink)
{
   if ( s_bIsLedRedBlinking || s_bIsLedRedBlinkingFast )
   {
      s_bIsLedRedBlinking = true;
      s_bIsLedRedBlinkingFast = false;
      if ( uPeriodToBlink == MAX_U32 )
         s_uTimeLedRedBlinkUntil = MAX_U32;
      else
         s_uTimeLedRedBlinkUntil = s_uTimeLedRedLastBlink + uPeriodToBlink;
      return;
   }

   hardware_led_red_set_on();

   s_bIsLedRedBlinking = true;
   s_bIsLedRedBlinkingFast = false;
   s_uTimeLedRedLastBlink = get_current_timestamp_ms();
   if ( uPeriodToBlink == MAX_U32 )
      s_uTimeLedRedBlinkUntil = MAX_U32;
   else
      s_uTimeLedRedBlinkUntil = s_uTimeLedRedLastBlink + uPeriodToBlink;
}

void hardware_led_red_set_blinking_fast(u32 uPeriodToBlink)
{
   if ( s_bIsLedRedBlinking || s_bIsLedRedBlinkingFast )
   {
      s_bIsLedRedBlinking = false;
      s_bIsLedRedBlinkingFast = true;
      if ( uPeriodToBlink == MAX_U32 )
         s_uTimeLedRedBlinkUntil = MAX_U32;
      else
         s_uTimeLedRedBlinkUntil = s_uTimeLedRedLastBlink + uPeriodToBlink;
      return;
   }

   hardware_led_red_set_on();

   s_bIsLedRedBlinking = false;
   s_bIsLedRedBlinkingFast = true;
   s_uTimeLedRedLastBlink = get_current_timestamp_ms();
   if ( uPeriodToBlink == MAX_U32 )
      s_uTimeLedRedBlinkUntil = MAX_U32;
   else
      s_uTimeLedRedBlinkUntil = s_uTimeLedRedLastBlink + uPeriodToBlink;
}

void hardware_led_green_set_on()
{
   #ifdef HW_CAPABILITY_GPIO
   if ( hardware_is_running_on_runcam_vrx() )
      GPIOWrite(GPIOGetPinLedGreen(), LOW);
   else
      GPIOWrite(GPIOGetPinLedGreen(), HIGH);
   #endif
   s_bIsLedGreenOn = true;
   s_bIsLedGreenBlinking = false;
   s_bIsLedGreenBlinkingFast = false;
   s_uTimeLedGreenBlinkUntil = 0;
   s_uTimeLedGreenLastBlink = 0;
}

void hardware_led_green_set_off()
{
   #ifdef HW_CAPABILITY_GPIO
   if ( hardware_is_running_on_runcam_vrx() )
      GPIOWrite(GPIOGetPinLedGreen(), HIGH);
   else
      GPIOWrite(GPIOGetPinLedGreen(), LOW);
   #endif
   s_bIsLedGreenOn = false;
   s_bIsLedGreenBlinking = false;
   s_bIsLedGreenBlinkingFast = false;
   s_uTimeLedGreenBlinkUntil = 0;
   s_uTimeLedGreenLastBlink = 0;
}

void hardware_led_green_set_blinking(u32 uPeriodToBlink)
{
   if ( s_bIsLedGreenBlinking || s_bIsLedGreenBlinkingFast )
   {
      s_bIsLedGreenBlinking = true;
      s_bIsLedGreenBlinkingFast = false;
      if ( uPeriodToBlink == MAX_U32 )
         s_uTimeLedGreenBlinkUntil = MAX_U32;
      else
         s_uTimeLedGreenBlinkUntil = s_uTimeLedGreenLastBlink + uPeriodToBlink;
      return;
   }

   hardware_led_green_set_on();

   s_bIsLedGreenBlinking = true;
   s_bIsLedGreenBlinkingFast = false;
   s_uTimeLedGreenLastBlink = get_current_timestamp_ms();
   if ( uPeriodToBlink == MAX_U32 )
      s_uTimeLedGreenBlinkUntil = MAX_U32;
   else
      s_uTimeLedGreenBlinkUntil = s_uTimeLedGreenLastBlink + uPeriodToBlink;
}

void hardware_led_green_set_blinking_fast(u32 uPeriodToBlink)
{
   if ( s_bIsLedGreenBlinking || s_bIsLedGreenBlinkingFast )
   {
      s_bIsLedGreenBlinking = false;
      s_bIsLedGreenBlinkingFast = true;
      if ( uPeriodToBlink == MAX_U32 )
         s_uTimeLedGreenBlinkUntil = MAX_U32;
      else
         s_uTimeLedGreenBlinkUntil = s_uTimeLedGreenLastBlink + uPeriodToBlink;
      return;
   }

   hardware_led_green_set_on();

   s_bIsLedGreenBlinking = false;
   s_bIsLedGreenBlinkingFast = true;
   s_uTimeLedGreenLastBlink = get_current_timestamp_ms();
   if ( uPeriodToBlink == MAX_U32 )
      s_uTimeLedGreenBlinkUntil = MAX_U32;
   else
      s_uTimeLedGreenBlinkUntil = s_uTimeLedGreenLastBlink + uPeriodToBlink;
}

static char s_szHardwareETHName[64];

char* hardware_has_eth()
{
   char szETHName[128];
   s_szHardwareETHName[0] = 0;

   hw_execute_bash_command_raw("ls /sys/class/net/ | grep eth0", szETHName);
   if ( strlen(szETHName) < 4 )
      hw_execute_bash_command_raw("ls /sys/class/net/ | grep eth1", szETHName);
   if ( strlen(szETHName) < 4 )
      hw_execute_bash_command_raw("ls /sys/class/net/ | grep etx", szETHName);
   if ( strlen(szETHName) < 4 )
      hw_execute_bash_command_raw("ls /sys/class/net/ | grep enx", szETHName);

   if ( strlen(szETHName) < 4 )
   {
      log_line("ETH not found.");
      return NULL;
   }

   for( int i=strlen(szETHName)-1; i>0; i-- )
   {
      if ( (szETHName[i] == 10) || (szETHName[i] == 13) || (szETHName[i] == ' ') )
         szETHName[i] = 0;
      else
         break;
   }
   strncpy(s_szHardwareETHName, szETHName, sizeof(s_szHardwareETHName)/sizeof(s_szHardwareETHName[0]));

   char szComm[256];
   char szOutput[1024];
   szOutput[0] = 0;
   sprintf(szComm, "ip link set dev %s up", szETHName);
   hw_execute_bash_command(szComm, szOutput);
   if ( 5 < strlen(szOutput) )
      log_line("[Hardware] ETH up command failed: (%s)", szOutput);
   
   return s_szHardwareETHName;
}

void hardware_set_default_sigmastar_cpu_freq()
{
   hw_execute_bash_command_raw("echo 'performance' | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor", NULL);
   char szBuff[256];
   snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "echo %d | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq", DEFAULT_FREQ_OPENIPC_SIGMASTAR*1000);
   hw_execute_bash_command_raw(szBuff, NULL);
   hw_execute_bash_command_raw("echo 800000 | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq", NULL);
}

void hardware_set_default_radxa_cpu_freq()
{
   if ( hardware_is_running_on_runcam_vrx() )
      return;
   hw_execute_bash_command_raw("echo 'performance' | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor", NULL);
   char szBuff[256];
   snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "echo %d | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq", DEFAULT_FREQ_RADXA*1000);
   hw_execute_bash_command_raw(szBuff, NULL);
   hw_execute_bash_command_raw("echo 1400000 | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq", NULL); 
}

int hardware_get_cpu_speed()
{
   #if defined(HW_PLATFORM_RASPBERRY)
   char szOutput[64];
   szOutput[0] = 0;
   hw_execute_bash_command_raw_silent("vcgencmd measure_clock arm", szOutput);

   if ( 0 == szOutput[0] )
      return 0;

   char* p = &szOutput[0];
   int pos = 0;
   while ( (*p) && (*p != '=') && pos < 63 )
   {
      p++;
      pos++;
   }

   if ( 0 == (*p) || pos >= 63 )
      return 0;

   p++;
   int len = strlen(p);
   if ( len > 6 )
   {
     if ( p[len-1] == 10 || p[len-1] == 13 )
     {
        p[len-1] = 0;
        len--;
     }
     if ( p[len-1] == 10 || p[len-1] == 13 )
     {
        p[len-1] = 0;
        len--;
     }
     if ( len > 6 )
        p[len-6] = 0;
   }
   return atoi(p);
   #endif

   #if defined(HW_PLATFORM_RADXA)
   if ( hardware_is_running_on_runcam_vrx() )
      return 1000;
   char szOutput[1024];
   hw_execute_bash_command_raw_silent("cat /sys/devices/system/cpu/cpufreq/policy0/cpuinfo_cur_freq 2>/dev/null", szOutput);
   return atoi(szOutput)/1000;
   #endif

   #if defined(HW_PLATFORM_OPENIPC_CAMERA)
   char szOutput[1024];
   hw_execute_bash_command_raw_silent("cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq 2>/dev/null", szOutput);
   int iFreqMhz = atoi(szOutput)/1000;
   if ( access("/sys/devices/system/cpu/cpu1/cpufreq/cpuinfo_cur_freq", R_OK) != -1 )
   {
      hw_execute_bash_command_raw_silent("cat /sys/devices/system/cpu/cpu1/cpufreq/cpuinfo_cur_freq 2>/dev/null", szOutput);
      int iFreqMhz1 = atoi(szOutput)/1000;
      if ( iFreqMhz1 > 10 )
      if ( iFreqMhz1 < iFreqMhz )
         iFreqMhz = iFreqMhz1;
   }
   return iFreqMhz;
   #endif

   return 1000;
}

int hardware_get_gpu_speed()
{
   #if defined (HW_PLATFORM_RASPBERRY)
   char szOutput[64];
   szOutput[0] = 0;
   hw_execute_bash_command_raw_silent("vcgencmd measure_clock core", szOutput);

   if ( 0 == szOutput[0] )
      return 0;

   char* p = &szOutput[0];
   int pos = 0;
   while ( (*p) && (*p != '=') && pos < 63 )
   {
      p++;
      pos++;
   }

   if ( 0 == (*p) || pos >= 63 )
      return 0;

   p++;
   int len = strlen(p);
   if ( len > 6 )
   {
     if ( p[len-1] == 10 || p[len-1] == 13 )
     {
        p[len-1] = 0;
        len--;
     }
     if ( p[len-1] == 10 || p[len-1] == 13 )
     {
        p[len-1] = 0;
        len--;
     }
     if ( len > 6 )
        p[len-6] = 0;
   }
   return atoi(p);
   #elif defined(HW_PLATFORM_RADXA)
   char szOutput[1024];
   hw_execute_bash_command_raw_silent("cat /sys/devices/system/cpu/cpufreq/policy0/cpuinfo_cur_freq 2>/dev/null", szOutput);
   return atoi(szOutput)/1000;
   #else
   return 1000;
   #endif
}

int hardware_get_cpu_temp()
{
   int iTemp1 = 0, iTemp2 = 0;

   #if defined(HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA)
   FILE* fd = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
   if ( NULL != fd )
   {
      fscanf(fd, "%d", &iTemp1);
      fclose(fd);
      fd = NULL;
   }
   else
      iTemp1 = 0;
   if ( access("/sys/class/thermal/thermal_zone1/temp", R_OK) != -1 )
   {
      fd = fopen("/sys/class/thermal/thermal_zone1/temp", "r");
      if ( NULL != fd )
      {
         fscanf(fd, "%d", &iTemp2);
         fclose(fd);
         fd = NULL;
      }
      else
         iTemp2 = 0;
   }
   if ( iTemp2 > iTemp1 )
      iTemp1 = iTemp2;
   #endif

   #ifdef HW_PLATFORM_OPENIPC_CAMERA
   char szBuff[1024];
   szBuff[0] = 0;
   hw_execute_bash_command("ipcinfo -t", szBuff);
   for( int i=0; i<(int)strlen(szBuff); i++ )
   {
      if ( szBuff[i] == '.' || szBuff[i] == 10 )
      {
         szBuff[i] = 0;
         break;
      }
   }
   iTemp1 = 1000 * atoi(szBuff);
   #endif

   return iTemp1/1000;    
}

void hardware_set_oipc_freq_boost(int iFreqCPUMhz, int iGPUBoost)
{
   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   if ( hardware_board_is_sigmastar(hardware_getBoardType()) )
   {
      hardware_set_oipc_cpu_freq(iFreqCPUMhz);
      hardware_set_oipc_gpu_boost(iGPUBoost);
   }
   #endif
}

void hardware_set_oipc_cpu_freq(int iFreqCPUMhz)
{
   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   hw_execute_bash_command_raw("echo 'performance' | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor", NULL);
   char szComm[256];
   sprintf(szComm, "echo %d | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq", iFreqCPUMhz*1000);
   hw_execute_bash_command_raw(szComm, NULL);
   hw_execute_bash_command_raw("echo 700000 | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq", NULL);
   #endif
}

void hardware_set_oipc_gpu_boost(int iGPUBoost)
{
   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   if ( hardware_board_is_sigmastar(hardware_getBoardType()) )
   {
      if ( 0 == iGPUBoost )
      {
         hw_execute_bash_command_raw("echo 384000000 > /sys/venc/ven_clock", NULL);
         hw_execute_bash_command_raw("echo 320000000 > /sys/venc/ven_clock_2nd", NULL);
      }
      else if ( 1 == iGPUBoost )
      {
         hw_execute_bash_command_raw("echo 432000000 > /sys/venc/ven_clock", NULL);
         hw_execute_bash_command_raw("echo 336000000 > /sys/venc/ven_clock_2nd", NULL);
      }
      else if ( 2 == iGPUBoost )
      {
         hw_execute_bash_command_raw("echo 480000000 > /sys/venc/ven_clock", NULL);
         hw_execute_bash_command_raw("echo 348000000 > /sys/venc/ven_clock_2nd", NULL);
      }
      else
      {
         int iCore1 = (iGPUBoost/10)%10;
         int iCore2 = (iGPUBoost/100)%10;
         int iFreq1 = 0;
         int iFreq2 = 0;
         switch(iCore1)
         {
            case 0: iFreq1 = 384; break;
            case 1: iFreq1 = 432; break;
            case 2: iFreq1 = 480; break;
            default: break;
         }
         switch(iCore2)
         {
            case 0: iFreq2 = 320; break;
            case 1: iFreq2 = 336; break;
            case 2: iFreq2 = 348; break;
            case 3: iFreq2 = 384; break;
            default: break;
         }
         if ( iFreq1 > 0 )
         {
            char szComm[256];
            sprintf(szComm, "echo %d > /sys/venc/ven_clock", iFreq1*1000*1000);
            hw_execute_bash_command_raw(szComm, NULL);
         }
         if ( iFreq2 > 0 )
         {
            char szComm[256];
            sprintf(szComm, "echo %d > /sys/venc/ven_clock_2nd", iFreq2*1000*1000);
            hw_execute_bash_command_raw(szComm, NULL);
         }
      }
   }
   #endif
}

void _hardware_set_int_affinity_core(const char* szIntName, int iCoreIndex)
{
   if ( (NULL == szIntName) || (0 == szIntName[0]) || (iCoreIndex < 0) )
      return;

   char szComm[256];
   char szOutput[256];
   for( int i=0; i<6; i++ )
   {
      szOutput[0] = 0;
      sprintf(szComm, "cat /proc/interrupts | grep -m 6 %s | sed -n %dp", szIntName, i+1);
      hw_execute_bash_command(szComm, szOutput);
      int iLen = (int)strlen(szOutput);
      if ( 0 == iLen )
         break;
      bool bHasDigits = false;
      for( int k=0; k<iLen; k++ )
      {
         if ( ! isdigit(szOutput[k]) )
            szOutput[k] = ' ';
         else
            bHasDigits = true;
      }
      if ( ! bHasDigits )
         break;
      int iIntNumber = -1;
      if ( 1 != sscanf(szOutput, "%d", &iIntNumber) )
         iIntNumber = -1;

      if ( -1 == iIntNumber )
      {
         log_softerror_and_alarm("[Hardware] Can't find interrupt number for interrupt: (%s)", szIntName);
         continue;
      }

      sprintf(szComm, "echo %d > /proc/irq/%d/smp_affinity", iCoreIndex+1, iIntNumber);
      hw_execute_bash_command(szComm, NULL);
   }
}

void hardware_balance_interupts()
{
   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   char szOutput[128];
   hw_execute_bash_command_raw("nproc --all", szOutput);
   removeTrailingNewLines(szOutput);
   log_line("[Hardware] CPU cores: (%s)", szOutput);
   strcat(szOutput, "\n");
   int iCPUCoresCount = 1;
   if ( (1 != sscanf(szOutput, "%d", &iCPUCoresCount)) || (1 == iCPUCoresCount) )
   {
      iCPUCoresCount = 1;
      szOutput[0] = 0;
      hw_execute_bash_command_raw("cat /proc/cpuinfo | grep processor | grep 0", szOutput);
      removeTrailingNewLines(szOutput);
      log_line("[Hardware] procinfo: (%s)", szOutput);
      if ( strlen(szOutput) > 8 )
      {
         szOutput[0] = 0;
         hw_execute_bash_command_raw("cat /proc/cpuinfo | grep processor | grep 1", szOutput);
         removeTrailingNewLines(szOutput);
         log_line("[Hardware] procinfo: (%s)", szOutput);
         if ( strlen(szOutput) > 8 )
         if ( NULL != strstr(szOutput, "1") )
            iCPUCoresCount = 2;
      }
   }

   if ( iCPUCoresCount > 1 )
   {
      log_line("[Hardware] Balancing interrupts (%d cores)...", iCPUCoresCount);
      _hardware_set_int_affinity_core(":usb", 1);
      _hardware_set_int_affinity_core("ms_serial", 1);
   }
   else
      log_line("[Hardware] Can't balance interrupts: single core CPU.");
   #endif
}