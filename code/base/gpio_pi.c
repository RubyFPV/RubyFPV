#include "base.h"
#include "gpio.h"
#include "hardware_procs.h"
#include "config_hw.h"
#include "config_file_names.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int s_iGPIOButtonsDirectionDetected = 0;
static int s_iGPIOButtonsPullDirection = 1;

void _gpio_load_custom_mapping();


int GPIOExport(int pin)
{
   if ( pin <= 0 )
      return 0;
   char buffer[6];
   ssize_t bytes_written;
   int fd;

   fd = open("/sys/class/gpio/export", O_WRONLY);
   if (-1 == fd)
   {
      //fprintf(stderr, "Failed to open export for writing!\n");
      return(-1);
   }

   bytes_written = snprintf(buffer, 6, "%d", pin);
   write(fd, buffer, bytes_written);
   close(fd);
   log_line("[GPIO] Exported pin %d", pin);
   return 0;
}

int GPIOUnexport(int pin)
{
   if ( pin <= 0 )
      return 0;
   char buffer[6];
   ssize_t bytes_written;
   int fd;

   fd = open("/sys/class/gpio/unexport", O_WRONLY);
   if (-1 == fd) {
    //fprintf(stderr, "Failed to open unexport for writing!\n");
    return(-1);
   }

   bytes_written = snprintf(buffer, 6, "%d", pin);
   write(fd, buffer, bytes_written);
   close(fd);
   return 0;
}

int GPIODirection(int pin, int dir)
{
   if ( pin <= 0 )
      return 0;
   static const char s_directions_str[]  = "in\0out";
   char path[64];
   int fd;

   snprintf(path, 64, "/sys/class/gpio/gpio%d/direction", pin);
   fd = open(path, O_WRONLY);
   if (-1 == fd) {
    //fprintf(stderr, "Failed to open gpio direction for writing!\n");
    return(-1);
   }

   if (-1 == write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)) {
    //fprintf(stderr, "Failed to set direction!\n");
    return(-1);
   }

   close(fd);
   return 0;
}

int GPIORead(int pin)
{
   if ( pin <= 0 )
      return -1;

   char path[64];
   char value_str[5];
   int fd;

   snprintf(path, 64, "/sys/class/gpio/gpio%d/value", pin);
   fd = open(path, O_RDONLY);
   if (-1 == fd) {
    //fprintf(stderr, "Failed to open gpio value for reading!\n");
    return(-1);
   }

   int ir = read(fd, value_str, 3);
          if ( ir == -1 ) {
    //fprintf(stderr, "Failed to read value!\n");
    return(-1);
   }
          value_str[ir] = 0;
   close(fd);
   return atoi(value_str);
}

int GPIOWrite(int pin, int value)
{
   if ( pin <= 0 )
      return 0;

   static const char s_values_str[] = "01";

   char path[64];
   int fd;

   snprintf(path, 64, "/sys/class/gpio/gpio%d/value", pin);
   fd = open(path, O_WRONLY);
   if (-1 == fd) {
    //fprintf(stderr, "Failed to open gpio value for writing!\n");
    return(-1);
   }

   if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) {
    //fprintf(stderr, "Failed to write value!\n");
    return(-1);
   }

   close(fd);
   return 0;
}

int GPIOGetButtonsPullDirection()
{
   return s_iGPIOButtonsPullDirection;
}

int _GPIOTryPullUpDown(int iPin, int iPullDirection)
{
   if ( iPin <= 0 )
      return 0;

   int failed = 0;
   if (-1 == GPIODirection(iPin, OUT))
   {
      log_line("Failed set GPIO pin %d to output", iPin);
      failed = 1;
   }

   if ( iPullDirection == 0 )
   {
      if ( 0 != GPIOWrite(iPin, LOW) )
         log_line("Failed to pull down pin %d", iPin);
   }
   else
   {
      if ( 0 != GPIOWrite(iPin, HIGH) )
         log_line("Failed to pull up pin %d", iPin);
   }

   if (-1 == GPIODirection(iPin, IN))
   {
      log_line("Failed set GPIO pin %d to input.", iPin);
      failed = 1;
   }
   
   char szComm[64];
   sprintf(szComm, "raspi-gpio set %d ip %s 2>&1", iPin, (iPullDirection==0)?"pd":"pu");
   hw_execute_bash_command_silent(szComm, NULL);

   sprintf(szComm, "gpio -g mode %d in", iPin);
   hw_execute_bash_command_silent(szComm, NULL);
   sprintf(szComm, "gpio -g mode %d %s", iPin, (iPullDirection==0)?"down":"up");
   hw_execute_bash_command_silent(szComm, NULL);

   return failed;
}


void _GPIO_PullAllDown()
{
   char szBuff[64];
   _GPIOTryPullUpDown(GPIOGetPinMenu(), 0);
   _GPIOTryPullUpDown(GPIOGetPinBack(), 0);
   _GPIOTryPullUpDown(GPIOGetPinPlus(), 0);
   _GPIOTryPullUpDown(GPIOGetPinMinus(), 0);
   _GPIOTryPullUpDown(GPIOGetPinQA1(), 0);
   _GPIOTryPullUpDown(GPIOGetPinQA2(), 0);
   _GPIOTryPullUpDown(GPIOGetPinQA22(), 0);
   _GPIOTryPullUpDown(GPIOGetPinQA3(), 0);
   _GPIOTryPullUpDown(GPIOGetPinQAPlus(), 0);
   _GPIOTryPullUpDown(GPIOGetPinQAMinus(), 0);

   if ( GPIOGetPinMenu() > 0 )
   {
   sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinMenu());
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d down", GPIOGetPinMenu());
   hw_execute_bash_command_silent(szBuff, NULL);
   }
   if ( GPIOGetPinBack() > 0 )
   {
   sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinBack());
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d down", GPIOGetPinBack());
   hw_execute_bash_command_silent(szBuff, NULL);
   }
   if ( GPIOGetPinPlus() > 0 )
   {
   sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinPlus());
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d down", GPIOGetPinPlus());
   hw_execute_bash_command_silent(szBuff, NULL);
   }
   if ( GPIOGetPinMinus() > 0 )
   {
   sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinMinus());
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d down", GPIOGetPinMinus());
   hw_execute_bash_command_silent(szBuff, NULL);
   }
   if ( GPIOGetPinQA1() > 0 )
   {
   sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinQA1());
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d down", GPIOGetPinQA1());
   hw_execute_bash_command_silent(szBuff, NULL);
   }
   if ( GPIOGetPinQA2() > 0 )
   {
   sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinQA2());
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d down", GPIOGetPinQA2());
   hw_execute_bash_command_silent(szBuff, NULL);
   }
   if ( GPIOGetPinQA22() > 0 )
   {
   sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinQA22());
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d down", GPIOGetPinQA22());
   hw_execute_bash_command_silent(szBuff, NULL);
   }
   if ( GPIOGetPinQA3() > 0 )
   {
   sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinQA3());
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d down", GPIOGetPinQA3());
   hw_execute_bash_command_silent(szBuff, NULL);
   }
   if ( GPIOGetPinQAPlus() > 0 )
   {
   sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinQAPlus());
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d down", GPIOGetPinQAPlus());
   hw_execute_bash_command_silent(szBuff, NULL);
   }
   if ( GPIOGetPinQAMinus() > 0 )
   {
   sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinQAMinus());
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d down", GPIOGetPinQAMinus());
   hw_execute_bash_command_silent(szBuff, NULL);
   }
   log_line("[GPIO] Pulled all buttons down");
}

void _GPIO_PullAllUp()
{
   char szBuff[64];
   _GPIOTryPullUpDown(GPIOGetPinMenu(), 1);
   _GPIOTryPullUpDown(GPIOGetPinBack(), 1);
   _GPIOTryPullUpDown(GPIOGetPinPlus(), 1);
   _GPIOTryPullUpDown(GPIOGetPinMinus(), 1);
   _GPIOTryPullUpDown(GPIOGetPinQA1(), 1);
   _GPIOTryPullUpDown(GPIOGetPinQA2(), 1);
   _GPIOTryPullUpDown(GPIOGetPinQA22(), 1);
   _GPIOTryPullUpDown(GPIOGetPinQA3(), 1);
   _GPIOTryPullUpDown(GPIOGetPinQAPlus(), 1);
   _GPIOTryPullUpDown(GPIOGetPinQAMinus(), 1);

   if ( GPIOGetPinMenu() > 0 )
   {
   sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinMenu());
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d up", GPIOGetPinMenu());
   hw_execute_bash_command_silent(szBuff, NULL);
   }
   if ( GPIOGetPinBack() > 0 )
   {
   sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinBack());
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d up", GPIOGetPinBack());
   hw_execute_bash_command_silent(szBuff, NULL);
   }
   if ( GPIOGetPinPlus() > 0 )
   {
   sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinPlus());
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d up", GPIOGetPinPlus());
   hw_execute_bash_command_silent(szBuff, NULL);
   }
   if ( GPIOGetPinMinus() > 0 )
   {
   sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinMinus());
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d up", GPIOGetPinMinus());
   hw_execute_bash_command_silent(szBuff, NULL);
   }
   if ( GPIOGetPinQA1() > 0 )
   {
   sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinQA1());
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d up", GPIOGetPinQA1());
   hw_execute_bash_command_silent(szBuff, NULL);
   }
   if ( GPIOGetPinQA2() > 0 )
   {
   sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinQA2());
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d up", GPIOGetPinQA2());
   hw_execute_bash_command_silent(szBuff, NULL);
   }
   if ( GPIOGetPinQA22() > 0 )
   {
   sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinQA22());
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d up", GPIOGetPinQA22());
   hw_execute_bash_command_silent(szBuff, NULL);
   }
   if ( GPIOGetPinQA3() > 0 )
   {
   sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinQA3());
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d up", GPIOGetPinQA3());
   hw_execute_bash_command_silent(szBuff, NULL);
   }
   if ( GPIOGetPinQAPlus() > 0 )
   {
   sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinQAPlus());
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d up", GPIOGetPinQAPlus());
   hw_execute_bash_command_silent(szBuff, NULL);
   }
   if ( GPIOGetPinQAMinus() > 0 )
   {
   sprintf(szBuff, "gpio -g mode %d in", GPIOGetPinQAMinus());
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d up", GPIOGetPinMinus());
   hw_execute_bash_command_silent(szBuff, NULL);
   }
   log_line("[GPIO] Pulled all buttons up");
}

// Returns 0 on success, 1 on failure

int GPIOInitButtons()
{
   _gpio_load_custom_mapping();
   log_line("[GPIO] Export and initialize buttons...");

   s_iGPIOButtonsDirectionDetected = 1;
   s_iGPIOButtonsPullDirection = 0;

   int failed = 0;
   if (-1 == GPIOExport(GPIOGetPinMenu()) || -1 == GPIOExport(GPIOGetPinBack()))
   {
      log_line("Failed to get GPIO access to pin Menu/Back.");
      failed = 1;
   }
   if (-1 == GPIOExport(GPIOGetPinPlus()) || -1 == GPIOExport(GPIOGetPinMinus()))
   {
      log_line("Failed to get GPIO access to pin Plus/Minus.");
      failed = 1;
   }
   if (-1 == GPIOExport(GPIOGetPinQA1()))
   {
      log_line("Failed to get GPIO access to pin QA1.");
      failed = 1;
   }
   if (-1 == GPIOExport(GPIOGetPinQA2()))
   {
      log_line("Failed to get GPIO access to pin QA2.");
      failed = 1;
   }
   if (-1 == GPIOExport(GPIOGetPinQA22()))
   {
      log_line("Failed to get GPIO access to pin QA2_2.");
      failed = 1;
   }
   if (-1 == GPIOExport(GPIOGetPinQA3()))
   {
      log_line("Failed to get GPIO access to pin QA3.");
      failed = 1;
   }

   if (-1 == GPIOExport(GPIOGetPinQAPlus()))
   {
      log_line("Failed to get GPIO access to pin QAPLUS.");
      failed = 1;
   }

   if (-1 == GPIOExport(GPIOGetPinQAMinus()))
   {
      log_line("Failed to get GPIO access to pin QAMINUS.");
      failed = 1;
   }

   if (-1 == GPIODirection(GPIOGetPinMenu(), IN) || -1 == GPIODirection(GPIOGetPinBack(), IN))
   {
      log_line("Failed set GPIO configuration for pin Menu/Back.");
      failed = 1;
   }
   if (-1 == GPIODirection(GPIOGetPinPlus(), IN) || -1 == GPIODirection(GPIOGetPinMinus(), IN))
   {
      log_line("Failed set GPIO configuration for pin Plus/Minus.");
      failed = 1;
   }
   if (-1 == GPIODirection(GPIOGetPinQA1(), IN))
   {
      log_line("Failed set GPIO configuration for pin QA1.");
      failed = 1;
   }
   if (-1 == GPIODirection(GPIOGetPinQA2(), IN))
   {
      log_line("Failed set GPIO configuration for pin QA2.");
      failed = 1;
   }
   if (-1 == GPIODirection(GPIOGetPinQA22(), IN))
   {
      log_line("Failed set GPIO configuration for pin QA2_2.");
      failed = 1;
   }
   if (-1 == GPIODirection(GPIOGetPinQA3(), IN))
   {
      log_line("Failed set GPIO configuration for pin QA3.");
      failed = 1;
   }

   if (-1 == GPIODirection(GPIOGetPinQAPlus(), IN))
   {
      log_line("Failed set GPIO configuration for pin QAACTIONPLUS.");
      failed = 1;
   }

   if (-1 == GPIODirection(GPIOGetPinQAMinus(), IN))
   {
      log_line("Failed set GPIO configuration for pin QAACTIONMINUS.");
      failed = 1;
   }

   char szComm[64];
   
   if ( GPIOGetPinMenu() > 0 )
   {
   sprintf(szComm, "raspi-gpio set %d ip 2>&1", GPIOGetPinMenu());
   hw_execute_bash_command(szComm, NULL);
   }
   if ( GPIOGetPinBack() > 0 )
   {
   sprintf(szComm, "raspi-gpio set %d ip 2>&1", GPIOGetPinBack());
   hw_execute_bash_command(szComm, NULL);
   }
   if ( GPIOGetPinPlus() > 0 )
   {
   sprintf(szComm, "raspi-gpio set %d ip 2>&1", GPIOGetPinPlus());
   hw_execute_bash_command(szComm, NULL);
   }
   if ( GPIOGetPinMinus() > 0 )
   {
   sprintf(szComm, "raspi-gpio set %d ip 2>&1", GPIOGetPinMinus());
   hw_execute_bash_command(szComm, NULL);
   }
   if ( GPIOGetPinQA1() > 0 )
   {
   sprintf(szComm, "raspi-gpio set %d ip 2>&1", GPIOGetPinQA1());
   hw_execute_bash_command(szComm, NULL);
   }
   if ( GPIOGetPinQA2() > 0 )
   {
   sprintf(szComm, "raspi-gpio set %d ip 2>&1", GPIOGetPinQA2());
   hw_execute_bash_command(szComm, NULL);
   }
   if ( GPIOGetPinQA22() > 0 )
   {
   sprintf(szComm, "raspi-gpio set %d ip 2>&1", GPIOGetPinQA22());
   hw_execute_bash_command(szComm, NULL);
   }
   if ( GPIOGetPinQA3() > 0 )
   {
   sprintf(szComm, "raspi-gpio set %d ip 2>&1", GPIOGetPinQA3());
   hw_execute_bash_command(szComm, NULL);
   }

   _GPIO_PullAllDown();

   return failed;
}

