#include "base.h"
#include "gpio.h"
#include "hardware_procs.h"
#include "config_hw.h"
#include "config_file_names.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int g_iGPIOPinLedRed = GPIO_PIN_LED_RED;
int g_iGPIOPinLedGreen = GPIO_PIN_LED_GREEN;
int g_iGPIOPinBuzzer = GPIO_PIN_BUZZER;
int g_iGPIOPinMenu = GPIO_PIN_MENU;
int g_iGPIOPinBack = GPIO_PIN_BACK;
int g_iGPIOPinPlus = GPIO_PIN_PLUS;
int g_iGPIOPinMinus = GPIO_PIN_MINUS;
int g_iGPIOPinQA1 = GPIO_PIN_QACTION1;
int g_iGPIOPinQA2 = GPIO_PIN_QACTION2;
int g_iGPIOPinQA22 = GPIO_PIN_QACTION2_2;
int g_iGPIOPinQA3 = GPIO_PIN_QACTION3;
int g_iGPIOPinQAPlus = GPIO_PIN_QACTIONPLUS;
int g_iGPIOPinQAMinus = GPIO_PIN_QACTIONMINUS;
int g_iGPIOPinDetectVehicle = GPIO_PIN_DETECT_TYPE_VEHICLE;
int g_iGPIOPinDetectController = GPIO_PIN_DETECT_TYPE_CONTROLLER;

int GPIOGetPinLedRed() { return g_iGPIOPinLedRed; }
int GPIOGetPinLedGreen() { return g_iGPIOPinLedGreen; }
int GPIOGetPinBuzzer() { return g_iGPIOPinBuzzer; }
int GPIOGetPinMenu()   { return g_iGPIOPinMenu; }
int GPIOGetPinBack()   { return g_iGPIOPinBack; }
int GPIOGetPinPlus()   { return g_iGPIOPinPlus; }
int GPIOGetPinMinus()  { return g_iGPIOPinMinus; }
int GPIOGetPinQA1()    { return g_iGPIOPinQA1; }
int GPIOGetPinQA2()    { return g_iGPIOPinQA2; }
int GPIOGetPinQA22()    { return g_iGPIOPinQA22; }
int GPIOGetPinQA3()    { return g_iGPIOPinQA3; }
int GPIOGetPinQAPlus() { return g_iGPIOPinQAPlus; }
int GPIOGetPinQAMinus() { return g_iGPIOPinQAMinus; }
int GPIOGetPinDetectVehicle() { return g_iGPIOPinDetectVehicle; }
int GPIOGetPinDetectController() { return g_iGPIOPinDetectController; }

int _gpio_reverse_find_pin_mapping(int iGPIOPin)
{
   if ( (iGPIOPin <= 0) || (iGPIOPin >= 50) )
      return -1;

   #if defined(HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA)

   char szComm[128];
   char szOutput[256];

   sprintf(szComm, "gpiofind PIN_%d", iGPIOPin);
   szOutput[0] = 0;
   hw_execute_bash_command_raw(szComm, szOutput);
   int iLen = strlen(szOutput);
   if ( iLen < 4 )
      return -1;

   for( int i=0; i<iLen; i++ )
   {
      if ( ! isdigit(szOutput[i]) )
         szOutput[i] = ' ';
   }

   int iBank = 0;
   int iOffset = 0;

   if ( 2 != sscanf(szOutput, "%d %d", &iBank, &iOffset) )
      return -1;

   int iNewPin = iBank*32 + iOffset;

   log_line("[GPIO] Mapped PIN_%d to Radxa pin %d", iGPIOPin, iNewPin);
   return iNewPin;
   #endif
   return -1;
}

void _gpio_load_custom_mapping()
{
   #if defined(HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA)
   log_line("[GPIO] Loading custom GPIO mappings from file (%s%s)...", FOLDER_WINDOWS_PARTITION, FILE_CONFIG_GPIO);
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_WINDOWS_PARTITION);
   strcat(szFile, FILE_CONFIG_GPIO);
   if ( access( szFile, R_OK) == -1 )
   {
      log_line("[GPIO] No file with custom GPIO mappings.");
      return;
   }
   FILE* fd = fopen(szFile, "rb");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("[GPIO] Can't access custom GPIO mappings file.");
      return;
   }

   int iTmp1 = 0;
   int iTmp2 = 0;
   int iTmp3 = 0;
   int iTmp4 = 0;

   if ( 4 != fscanf(fd, "%d %d %d %d", &iTmp1, &iTmp2, &iTmp3, &iTmp4) )
   {
      log_softerror_and_alarm("[GPIO] Invalid mapping file (%s). Ignoring it.", szFile);
      fclose(fd);
      return;
   }
   if ( (iTmp1 <= 0) || (iTmp2 <= 0) || (iTmp3 <= 0) || (iTmp4 <= 0) )
   {
      log_softerror_and_alarm("[GPIO] Invalid (2) mapping file (%s). Ignoring it.", szFile);
      fclose(fd);
      return;
   }

   g_iGPIOPinMenu = iTmp1;
   g_iGPIOPinBack = iTmp2;
   g_iGPIOPinPlus = iTmp3;
   g_iGPIOPinMinus = iTmp4;

   #if defined (HW_PLATFORM_RADXA)
   g_iGPIOPinMenu = _gpio_reverse_find_pin_mapping(g_iGPIOPinMenu);
   g_iGPIOPinBack = _gpio_reverse_find_pin_mapping(g_iGPIOPinBack);
   g_iGPIOPinPlus = _gpio_reverse_find_pin_mapping(g_iGPIOPinPlus);
   g_iGPIOPinMinus = _gpio_reverse_find_pin_mapping(g_iGPIOPinMinus);
   #endif

   log_line("[GPIO] Assigned GPIO for menu/back/plus/minus: %d %d %d %d", g_iGPIOPinMenu, g_iGPIOPinBack, g_iGPIOPinPlus, g_iGPIOPinMinus);

   iTmp1 = 0;
   iTmp2 = 0;
   iTmp3 = 0;

   if ( 1 != fscanf(fd, "%d", &iTmp1) )
      iTmp1 = 0;
   if ( 1 != fscanf(fd, "%d", &iTmp2) )
      iTmp2 = 0;
   if ( 1 != fscanf(fd, "%d", &iTmp3) )
      iTmp3 = 0;


   log_line("[GPIO] GPIO custom pins for QA1/2/3: %d %d %d", iTmp1, iTmp2, iTmp3);
   #if defined (HW_PLATFORM_RADXA)
   if ( iTmp1 > 0 )
      g_iGPIOPinQA1 = _gpio_reverse_find_pin_mapping(iTmp1);
   if ( iTmp2 > 0 )
      g_iGPIOPinQA2 = _gpio_reverse_find_pin_mapping(iTmp2);
   if ( iTmp3 > 0 )
      g_iGPIOPinQA3 = _gpio_reverse_find_pin_mapping(iTmp3);
   #endif

   log_line("[GPIO] Assigned GPIO for QA1/QA2/QA3: %d %d %d", g_iGPIOPinQA1, g_iGPIOPinQA2, g_iGPIOPinQA3);
   fclose(fd);
   #endif
}

#ifdef HW_CAPABILITY_GPIO

#ifdef HW_PLATFORM_RASPBERRY
#include "gpio_pi.c"
#endif

#ifdef HW_PLATFORM_RADXA
#include "gpio_radxa.c"
#endif

#else

int GPIOExport(int pin)
{
   return 0;
}

int GPIOUnexport(int pin)
{
  	return 0;
}

int GPIODirection(int pin, int dir)
{
  	return 0;
}

int GPIORead(int pin)
{
   return 0;
}

int GPIOWrite(int pin, int value)
{
  	return 0;
}

int GPIOGetButtonsPullDirection()
{
   return 1;
}

int _GPIOTryPullUpDown(int iPin, int iPullDirection)
{
   return 0;
}


void _GPIO_PullAllDown()
{
}

void _GPIO_PullAllUp()
{
}

// Returns 0 on success, 1 on failure

int GPIOInitButtons()
{
   return 0;
}
#endif