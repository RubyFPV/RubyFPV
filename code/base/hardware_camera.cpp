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

#include "base.h"
#include "config.h"
#include "hardware.h"
#include "hardware_camera.h"
#include "hardware_procs.h"
#include "hardware_i2c.h"
#include "../common/string_utils.h"

#include <ctype.h>
#include <pthread.h>

bool s_bDetectedCameraType = false;

int s_bHardwareHasCamera = 0;
u32 s_uHardwareCameraType = 0;
int s_iHardwareCameraI2CBus = -1;
int s_iHardwareCameraDevId = -1;
int s_iHardwareCameraHWVer = -1;

// parses a string of format: "device id is 0xAB" or "device id is 0x A"
int _hardware_get_camera_device_id_from_string(char* szDeviceId)
{
   if ( (NULL == szDeviceId) || (0 == szDeviceId[0]) )
      return -1;
   log_line("[Hardware] Parsing string to find camera device id: [%s]", szDeviceId);

   char* szDevId = strstr(szDeviceId, "device id is");
   if ( NULL == szDevId )
      return -1;
   if ( strlen(szDevId) < strlen("device id is")+3 )
      return -1;
         
   int index = strlen(szDevId)-1;
   while ( (index >0) && (szDevId[index] == 10 || szDevId[index] == 13) )
      index--;
   szDevId[index+1] = 0;
   while ( (index>0) && ( isdigit(szDevId[index]) || (toupper(szDevId[index]) >= 'A' && toupper(szDevId[index]) <= 'F') ) )
      index--;
   index++;

   if ( szDevId[index] == 0 )
      return -1;

   szDevId += index;
   int iDevId = (int)strtol(szDevId, NULL, 16);
   log_line("[Hardware] Found camera device id: [%s], as int: %d", szDevId, iDevId );
   return iDevId;
}

// parses a string of format: "hardware version is 0xAB" or "hardware version is 0x A"
int _hardware_get_camera_hardware_version_from_string(char* szHardwareId)
{
   if ( (NULL == szHardwareId) || (0 == szHardwareId[0]) )
      return -1;
   log_line("[Hardware] Parsing string to find camera hardware version: [%s]", szHardwareId);

   char* szHWId = strstr(szHardwareId, "hardware version is");
   if ( NULL == szHWId )
      return -1;
   if ( strlen(szHWId) < strlen("hardware version is")+3 )
      return -1;
         
   int index = strlen(szHWId)-1;
   while ( (index >0) && (szHWId[index] == 10 || szHWId[index] == 13) )
      index--;
   szHWId[index+1] = 0;
   while ( (index>0) && ( isdigit(szHWId[index]) || (toupper(szHWId[index]) >= 'A' && toupper(szHWId[index]) <= 'F') ) )
      index--;
   index++;

   if ( szHWId[index] == 0 )
      return -1;

   szHWId += index;
   int iHWId = (int)strtol(szHWId, NULL, 16);
   log_line("[Hardware] Found camera hardware version: [%s], as int: %d", szHWId, iHWId );
   return iHWId;
}

static bool s_bThreadToDetectCameraIsRunning = false;
static char s_szThreadToDetectCameraBuffer[128];
void* _thread_hardware_camera_detect_on_raspbery(void *argument)
{
   s_bThreadToDetectCameraIsRunning = true;
   log_line("[Hardware] Started thread to detect camera...");
   hw_log_current_thread_attributes("detect camera");
   hw_execute_bash_command_raw("vcgencmd get_camera", s_szThreadToDetectCameraBuffer);
   hardware_sleep_ms(50);
   log_line("[Hardware] Finished thread to detect camera. Output: (%s)", s_szThreadToDetectCameraBuffer);
   s_bThreadToDetectCameraIsRunning = false;
   return NULL;
}

u32 _hardware_detect_camera_type()
{
   log_line("[Hardware] Detecting camera type...");

   char szBuff[256];
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_CONFIG_CAMERA_TYPE);
   FILE* fd = fopen(szFile, "r");
   if ( NULL != fd )
   {
      if ( 5 == fscanf(fd, "%d %u %d %d %d", &s_bHardwareHasCamera, &s_uHardwareCameraType, &s_iHardwareCameraI2CBus, &s_iHardwareCameraDevId, &s_iHardwareCameraHWVer) )
      {
         s_bDetectedCameraType = true;
         str_get_hardware_camera_type_string_to_string(s_uHardwareCameraType, szBuff);
         log_line("[Hardware] Loaded camera type: %u (%s), has camera: %s, camera i2c bus: %d", 
            s_uHardwareCameraType, szBuff, s_bHardwareHasCamera?"yes":"no", s_iHardwareCameraI2CBus);
         fclose(fd);
         return s_uHardwareCameraType;
      }
      log_softerror_and_alarm("[Hardware] Failed to parse camera type config file [%s]", szFile);
      fclose(fd);
   }
   
   s_bHardwareHasCamera = 0;
   s_uHardwareCameraType = CAMERA_TYPE_NONE;
   s_iHardwareCameraI2CBus = -1;

   #if defined (HW_PLATFORM_RASPBERRY)
   char szOutput[512];
   char szComm[256];

   int retryCount = 3;
    
   if ( access(FILE_FORCE_VEHICLE_NO_CAMERA, R_OK ) != -1 )
   {
      log_line("File %s present to force no camera.", FILE_FORCE_VEHICLE_NO_CAMERA);
      retryCount = 0;
   }

   while ( retryCount > 0 )
   {
      if ( hardware_i2c_has_device_id(I2C_DEVICE_ADDRESS_CAMERA_HDMI) )
      {
         s_iHardwareCameraI2CBus = hardware_i2c_get_device_bus_number(I2C_DEVICE_ADDRESS_CAMERA_HDMI);
         log_line("Hardware: HDMI Camera detected on i2c bus %d.", s_iHardwareCameraI2CBus);
         s_bHardwareHasCamera = 1;
         s_uHardwareCameraType = CAMERA_TYPE_HDMI;
         break;
      }
      
      if ( hardware_i2c_has_device_id(I2C_DEVICE_ADDRESS_CAMERA_VEYE) )
      {
         s_iHardwareCameraI2CBus =  hardware_i2c_get_device_bus_number(I2C_DEVICE_ADDRESS_CAMERA_VEYE);
         log_line("Hardware: Veye Camera detected on i2c bus %d.", s_iHardwareCameraI2CBus);
         s_bHardwareHasCamera = 1;
         s_uHardwareCameraType = CAMERA_TYPE_VEYE307;

         sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -r -f devid -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, s_iHardwareCameraI2CBus);
         hw_execute_bash_command_raw(szComm, szOutput);
         int iDevId = _hardware_get_camera_device_id_from_string(szOutput);
         if ( (iDevId <= 0) || (iDevId >= 255) )
         {
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -r -f devid -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, s_iHardwareCameraI2CBus);
            hw_execute_bash_command_raw(szComm, szOutput);
            iDevId = _hardware_get_camera_device_id_from_string(szOutput);
         }
  
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -r -f hdver -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, s_iHardwareCameraI2CBus);
         hw_execute_bash_command_raw(szComm, szOutput);
         int iHWId = _hardware_get_camera_hardware_version_from_string(szOutput);
         if ( (iHWId <= 0) || (iHWId >= 255) )
         {
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -r -f hdver -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, s_iHardwareCameraI2CBus);
            hw_execute_bash_command_raw(szComm, szOutput);
            iHWId = _hardware_get_camera_hardware_version_from_string(szOutput);
         }
  
         if ( (iDevId > 0) && (iDevId < 255) )
         {
            s_iHardwareCameraDevId = iDevId;
            s_iHardwareCameraHWVer = iHWId;
            log_line("Found veye camera device id: %d", iDevId);
            if ( 6 == iDevId )
            {
               s_uHardwareCameraType = CAMERA_TYPE_VEYE327;
               log_line("Veye camera type: 327, hardware version: %d", iHWId);
            }
            else if ( (iDevId == 33) || (iDevId == 34) || (iDevId == 51) || (iDevId == 52) )
            {
               s_uHardwareCameraType = CAMERA_TYPE_VEYE307;
               log_line("Veye camera type: 307, hardware version: %d", iHWId);
            }
            else
            {
               s_uHardwareCameraType = CAMERA_TYPE_VEYE327;
               log_line("Veye camera type: N/A (dev id: %d). Default to 327, hardware version: %d", iDevId, iHWId);
            }
         }
         //if ( s_uHardwareCameraType == CAMERA_TYPE_VEYE307 )
         //   sprintf(szComm, "current_dir=$PWD; cd %s/; ./camera_i2c_config; cd $current_dir", VEYE_COMMANDS_FOLDER307);
         //else
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./camera_i2c_config; cd $current_dir", VEYE_COMMANDS_FOLDER);
         hw_execute_bash_command(szComm, NULL);

         break;
      }

      if ( 0 == s_bHardwareHasCamera )
      {
         szBuff[0] = 0;
         s_szThreadToDetectCameraBuffer[0] = 0;
         s_bThreadToDetectCameraIsRunning = true;
         u32 uTimeStart = get_current_timestamp_ms();
         pthread_t pth;
         pthread_attr_t attr;
         hw_init_worker_thread_attrs(&attr, CORE_AFFINITY_OTHERS, -1, SCHED_OTHER, 0, "detect camera");
         if ( 0 != pthread_create(&pth, &attr, &_thread_hardware_camera_detect_on_raspbery, szBuff) )
         {
            log_softerror_and_alarm("[Hardware] Failed to create thread to detect camera. Do it manualy.");
            pthread_attr_destroy(&attr);
            s_bThreadToDetectCameraIsRunning = false;
            hw_execute_bash_command_raw("vcgencmd get_camera", szBuff);
            removeTrailingNewLines(szBuff);
         }
         else
         {
            pthread_attr_destroy(&attr);
            while ( s_bThreadToDetectCameraIsRunning )
            {
               hardware_sleep_ms(20);
               if ( get_current_timestamp_ms() > uTimeStart + 5000 )
               {
                  log_error_and_alarm("[Hardware] Thread to detect camera is stuck. Default to no camera.");
                  pthread_cancel(pth);
                  s_szThreadToDetectCameraBuffer[0] = 0;
                  //hardware_reboot();
               }
            }
            strncpy(szBuff, s_szThreadToDetectCameraBuffer, sizeof(szBuff)/sizeof(szBuff[0]) - 1);
            szBuff[sizeof(szBuff)/sizeof(szBuff[0])-1] = 0;
            removeTrailingNewLines(szBuff);
         }
         
         log_line("Camera detection response string: %s", szBuff);
         if ( NULL != strstr(szBuff, "detected=1") || 
              NULL != strstr(szBuff, "detected=2") )
         {
            log_line("Hardware: CSI Camera detected.");
            s_bHardwareHasCamera = 1;
            s_iHardwareCameraI2CBus = -1;
            s_uHardwareCameraType = CAMERA_TYPE_CSI;
            break;
         }
      }
      hardware_sleep_ms(400);
      retryCount--;
   }
   #endif

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)

   char szOutput[4096];
   memset(szOutput, 0, sizeof(szOutput)/sizeof(szOutput[0]));
   s_bHardwareHasCamera = 1;
   s_iHardwareCameraI2CBus = -1;

   s_uHardwareCameraType = CAMERA_TYPE_OPENIPC_IMX415;

   char* pSensorModel = hardware_camera_get_oipc_sensor_raw_name(szOutput);

   if ( NULL == pSensorModel )
   {
      log_softerror_and_alarm("[Hardware] Failed to get OpenIPC camera sensor type.");
   }
   else
   {
      log_line("Detected raw camera sensor type: [%s]", pSensorModel);
      if ( NULL != strstr(pSensorModel, "imx307") )
         s_uHardwareCameraType = CAMERA_TYPE_OPENIPC_IMX307;
      if ( NULL != strstr(pSensorModel, "imx335") )
         s_uHardwareCameraType = CAMERA_TYPE_OPENIPC_IMX335;
      if ( NULL != strstr(pSensorModel, "imx415") )
         s_uHardwareCameraType = CAMERA_TYPE_OPENIPC_IMX415;
   }
   #endif

   if ( s_bHardwareHasCamera == 0 )
      log_line("[Hardware] No camera detected.");
   else
   {
      str_get_hardware_camera_type_string_to_string(s_uHardwareCameraType, szBuff);
      log_line("[Hardware] Detected camera type %d: %s", (int)s_uHardwareCameraType, szBuff);
   }
   s_bDetectedCameraType = true;

   fd = fopen(szFile, "w");
   if ( NULL != fd )
   {
      fprintf(fd, "%d %u %d %d %d\n", s_bHardwareHasCamera, s_uHardwareCameraType, s_iHardwareCameraI2CBus, s_iHardwareCameraDevId, s_iHardwareCameraHWVer);
      fclose(fd);
   }

   return s_uHardwareCameraType;
}

int hardware_hasCamera()
{
   if ( ! s_bDetectedCameraType )
      _hardware_detect_camera_type();
   return s_bHardwareHasCamera;
}

u32 hardware_getCameraType()
{
   if ( ! s_bDetectedCameraType )
      _hardware_detect_camera_type();
   return s_uHardwareCameraType;      
}

int hardware_getCameraI2CBus()
{
   if ( ! s_bDetectedCameraType )
      _hardware_detect_camera_type();
   return s_iHardwareCameraI2CBus;
}

int hardware_getVeyeCameraDevId()
{
   if ( ! s_bDetectedCameraType )
      _hardware_detect_camera_type();
   return s_iHardwareCameraDevId;
}

int hardware_getVeyeCameraHWVer()
{
   if ( ! s_bDetectedCameraType )
      _hardware_detect_camera_type();
   return s_iHardwareCameraHWVer;
}

int hardware_isCameraVeye()
{
   if ( ! s_bDetectedCameraType )
      _hardware_detect_camera_type();

   if ( s_uHardwareCameraType == CAMERA_TYPE_VEYE290 ||
        s_uHardwareCameraType == CAMERA_TYPE_VEYE307 ||
        s_uHardwareCameraType == CAMERA_TYPE_VEYE327 )
      return 1;
   return 0;
}

int hardware_isCameraVeye307()
{
   if ( ! s_bDetectedCameraType )
      _hardware_detect_camera_type();

   if ( s_uHardwareCameraType == CAMERA_TYPE_VEYE307 )
      return 1;
   return 0;
}


int hardware_isCameraHDMI()
{
   if ( ! s_bDetectedCameraType )
      _hardware_detect_camera_type();

   if ( s_uHardwareCameraType == CAMERA_TYPE_HDMI )
      return 1;
   return 0;
}

char* hardware_camera_get_oipc_sensor_raw_name(char* pszOutput)
{
   if ( NULL == pszOutput )
      return NULL;
   pszOutput[0] = 0;
   hw_execute_bash_command("ipcinfo -s 2>/dev/null", pszOutput);
   removeTrailingNewLines(pszOutput);
   removeLeadingWhiteSpace(pszOutput);
   if ( strlen(pszOutput) > 2 )
   {
      for( int i=0; i<(int)strlen(pszOutput); i++ )
         pszOutput[i] = tolower(pszOutput[i]);
      log_line("[Hardware] Get raw camera sensor type from ipcinfo: [%s]", pszOutput);
      return pszOutput;
   }

   pszOutput[0] = 0;
   hw_execute_bash_command("fw_printenv sensor 2>/dev/null", pszOutput);
   removeTrailingNewLines(pszOutput);
   char* pSensor = NULL;
   if ( NULL != strstr(pszOutput, "=") )
      pSensor = strstr(pszOutput, "=") + 1;
   if ( (NULL != pSensor) && (strlen(pSensor) > 2) )
   {
      for( int i=0; i<(int)strlen(pSensor); i++ )
         pSensor[i] = tolower(pSensor[i]);
      log_line("[Hardware] Get raw camera sensor type from fw_env: [%s]", pszOutput);
      return pSensor;
   }

   log_softerror_and_alarm("[Hardware] Could not find camera sensor type from icpinfo or fw_env.");
   return NULL;

   /*
   pszOutput[0] = 0;
   hw_execute_bash_command("ipctool", pszOutput);
   for( int i=0; i<strlen(pszOutput); i++ )
       pszOutput[i] = tolower(pszOutput[i]);

   char* pSensors = strstr(pszOutput, "sensors:");
   if ( NULL == pSensors )
      pSensors = strstr(pszOutput, "sensor:");
   char* pModel = NULL;
   if ( NULL != pSensors )
      pModel = strstr(pSensors, "model:");
   if ( NULL != pModel )
   {
      char* pszTemp = pModel;
      while ( 0 != (*pszTemp) )
      {
         if ( (*pszTemp) < 24 )
         {
            *pszTemp = 0;
            break;
         }
         pszTemp++;
      }
      return pModel+7;
   }
   return NULL;
   */
}

void hardware_camera_check_set_oipc_sensor()
{
   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   char szSensor[128];
   char szComm[128];

   hw_execute_bash_command("fw_printenv sensor 2>/dev/null", szSensor);
   char* pSensor = szSensor;
   if ( NULL != strstr(szSensor, "=") )
      pSensor = strstr(szSensor, "=") + 1;
   if ( strlen(pSensor) > 2 )
      return;

   char szOutput[4096];
   memset(szOutput, 0, sizeof(szOutput)/sizeof(szOutput[0]));
   pSensor = hardware_camera_get_oipc_sensor_raw_name(szOutput);
   if ( NULL != pSensor )
   {
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "fw_setenv sensor %s", pSensor);
      hw_execute_bash_command(szComm, NULL);
   }
   #endif
}

void hardware_camera_set_default_oipc_calibration(int iCameraType)
{
   #if defined (HW_PLATFORM_OPENIPC_CAMERA)

   hardware_camera_check_set_oipc_sensor();

   char szMajComm[256];
   szMajComm[0] = 0;
   log_line("[HardwareCamera] Set default calibration for camera type: %d (%s)", iCameraType, str_get_hardware_camera_type_string(iCameraType));

   if ( iCameraType == CAMERA_TYPE_OPENIPC_IMX335 )
   {
      if ( access("/etc/sensors/imx335_fpv.bin", R_OK) != -1 )
         sprintf(szMajComm, "cli -s .isp.sensorConfig /etc/sensors/imx335_fpv.bin");
      else
         sprintf(szMajComm, "cli -s .isp.sensorConfig /etc/sensors/imx335.bin");
   }
   else
   {
      if ( access("/etc/sensors/imx415_fpv.bin", R_OK) != -1 )
         sprintf(szMajComm, "cli -s .isp.sensorConfig /etc/sensors/imx415_fpv.bin");
      else
         sprintf(szMajComm, "cli -s .isp.sensorConfig /etc/sensors/imx415.bin");
   }

   if ( 0 != szMajComm[0] )
      hw_execute_bash_command_raw(szMajComm, NULL);
   #endif
}