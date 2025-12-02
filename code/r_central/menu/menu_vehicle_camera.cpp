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
#include "menu.h"
#include "menu_vehicle_camera.h"
#include "menu_vehicle_camera_gains.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_calibrate_hdmi.h"
#include "menu_confirmation.h"
#include "../../base/hardware_files.h"
#include "../osd/osd_common.h"
#include <sys/types.h>
#include <dirent.h>


typedef struct 
{
   const char* szFriendlyName;
   const char* szBinFile;
   bool bIsFor415Sensor;
   int iMenuSelectionIndex;
} type_camera_bin_mapping;

static type_camera_bin_mapping s_CameraBinFilesMapping[] = 
{
  {"RunCam", "imx415_runcam_20240717", true, -1},
  {"FPV-2", "imx415_ssc338q_20240613", true, -1},
  {"FPV-2", "imx335_ssc338q_20240628", false, -1},
  {"Milos", "imx415_milos12", true, -1},
  {"Milos", "imx335_milos6", false, -1},
  {"AWB-1", "imx335_fpv_awb1", false, -1},
  {"Spike5", "imx335_spike5", false, -1}
};

static char s_szUserCalibrationFiles[50][MAX_CAMERA_BIN_PROFILE_NAME+1];
static int s_iUserCalibrationFilesIndexes[50];

MenuVehicleCamera::MenuVehicleCamera(void)
:Menu(MENU_ID_VEHICLE_CAMERA, L("Camera Settings"), NULL)
{
   m_Width = 0.32;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.13;

   m_bShowCompact = false;
   m_bDidAnyLiveUpdates = false;
   m_IndexShowFull = -1;
   m_iLastCameraType = -1;

   resetIndexes();
}

MenuVehicleCamera::~MenuVehicleCamera()
{
   if ( m_bDidAnyLiveUpdates )
      saveControllerModel(g_pCurrentModel);
}

void MenuVehicleCamera::showCompact()
{
   m_bShowCompact = true;
}

void MenuVehicleCamera::resetIndexes()
{
   m_IndexCamera = m_IndexForceMode = -1;
   m_IndexProfile = -1;
   m_IndexBrightness = m_IndexContrast = m_IndexSaturation = m_IndexSharpness = -1;
   m_IndexHue = -1;
   m_IndexEV = m_IndexEVValue = -1;
   m_IndexAGC = -1;
   m_IndexExposureMode = m_IndexExposureValue = m_IndexWhiteBalance = -1;
   m_IndexAnalogGains = -1;
   m_IndexMetering = m_IndexDRC = -1;
   m_IndexISO = m_IndexISOValue = -1;
   m_IndexShutterMode = m_IndexShutterValue = -1;
   m_IndexWDR = -1;
   m_IndexDayNight = -1;
   m_IndexVideoStab = m_IndexFlip = m_IndexReset = -1;
   m_IndexIRCut = -1;
   m_IndexOpenIPCDayNight = -1;
   m_IndexOpenIPC3A = -1;
   m_IndexOpenIPCBinProfile = -1;

   m_iCountStandardCalibrationFiles = 0;
   m_iCountUserCalibrationFiles = 0;
   m_iSelectionIndexUnknownCalibrationFile = -1;
   m_iSelectionIndexImportCalibrationFiles = -1;

   for( int i=0; i<50; i++ )
   {
      s_szUserCalibrationFiles[i][0] = 0;
      s_iUserCalibrationFilesIndexes[i] = -1;
   }
   for( int i=0; i<(int)sizeof(s_CameraBinFilesMapping)/(int)sizeof(s_CameraBinFilesMapping[0]); i++ )
   {
      s_CameraBinFilesMapping[i].iMenuSelectionIndex = -1;
   }
}

void MenuVehicleCamera::onShow()
{
   log_line("MenuVehicleCamera: onShow start...");
   int iTmp = getSelectedMenuItemIndex();
   
   addItems();

   Menu::onShow();

   if ( iTmp >= 0 )
      m_SelectedIndex = iTmp;
   onFocusedItemChanged();
   log_line("MenuVehicleCamera: onShow completed.");
}

void MenuVehicleCamera::addItems()
{
   log_line("MenuVehicleCamera: addItems");
   int iTmp = getSelectedMenuItemIndex();

   removeAllItems();
   removeAllTopLines();

   m_IndexShowFull = -1;
   resetIndexes();

   for( int i=0; i<25; i++ )
   {
      m_pItemsSlider[i] = NULL;
      m_pItemsSelect[i] = NULL;
      m_pItemsRange[i] = NULL;
   }

   for( int i=0; i<25; i++ )
   {
      m_pItemsSlider[i] = NULL;
      m_pItemsSelect[i] = NULL;
      m_pItemsRange[i] = NULL;
   }

   char szCam[256];
   char szCam2[256];
   str_get_hardware_camera_type_string_to_string(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraType, szCam2);
   snprintf(szCam, 255, "Camera: %s", szCam2);

   char* szCamName = g_pCurrentModel->getCameraName(g_pCurrentModel->iCurrentCamera);
   log_line("Current camera (%d) name: [%s]", g_pCurrentModel->iCurrentCamera, szCamName);
   
   if ( (NULL != szCamName) && (0 != szCamName[0]) )
   {
      strcat(szCam, " (");
      strcat(szCam, szCamName);
      strcat(szCam, ")");
   }
   addTopLine(szCam);

   if ( g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraType == CAMERA_TYPE_HDMI )
      addTopLine("Note: Some HDMI-CSI adapters do not support changing camera/video params on the fly!");


   float fSliderWidth = 0.12 * Menu::getScaleFactor();
   float fMargin = 0.01 * Menu::getScaleFactor();

   log_line("MenuCamera: Total cameras detected: %d", g_pCurrentModel->iCameraCount);
   log_line("MenuCamera: Active camera: %d", g_pCurrentModel->iCurrentCamera+1);

   for( int i=0; i<g_pCurrentModel->iCameraCount; i++ )
   {
      char szBuff1[128];
      char szBuff2[128];
      str_get_hardware_camera_type_string_to_string(g_pCurrentModel->camera_params[i].iCameraType, szBuff1);
      str_get_hardware_camera_type_string_to_string(g_pCurrentModel->camera_params[i].iForcedCameraType, szBuff2);
      log_line("MenuCamera: Camera %d hardware type: %s, overwrite type: %s, current profile: %d", i+1, szBuff1, szBuff2, g_pCurrentModel->camera_params[i].iCurrentProfile);
   }

   m_iLastCameraType = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType;

   szCamName = g_pCurrentModel->getCameraName(g_pCurrentModel->iCurrentCamera);

   m_IndexCamera = -1;
   if ( ! m_bShowCompact )
   {
      m_pItemsSelect[11] = new MenuItemSelect(L("Active Camera"), L("Selects which camera should be active, if multiple cameras are present on this vehicle."));
      for( int i=0; i<g_pCurrentModel->iCameraCount; i++ )
      {
         szCam[0] = 0;
         szCam2[0] = 0;
         str_get_hardware_camera_type_string_to_string(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraType, szCam2);
         sprintf(szCam, "%s", szCam2);

         szCamName = g_pCurrentModel->getCameraName(g_pCurrentModel->iCurrentCamera);
         if ( (NULL != szCamName) && (0 != szCamName[0]) )
         if ( NULL == strstr(szCam, szCamName) )
         {
            strcat(szCam, " (");
            strcat(szCam, szCamName);
            strcat(szCam, ")");
         }
         m_pItemsSelect[11]->addSelection(szCam);
      }
      m_pItemsSelect[11]->setIsEditable();
      m_pItemsSelect[11]->setExtraHeight(0.8*m_sfMenuPaddingY);
      m_IndexCamera = addMenuItem(m_pItemsSelect[11]);
   }

   m_IndexForceMode = -1;
   if ( ! m_bShowCompact )
   {
      m_pItemsSelect[12] = new MenuItemSelect(L("Camera Type"), L("Autodetect the active camera type or force a particular camera type for the active camera."));
      m_pItemsSelect[12]->addSelection(L("Autodetect"));
      m_pItemsSelect[12]->addSelection("CSI Camera", !g_pCurrentModel->isRunningOnOpenIPCHardware());
      m_pItemsSelect[12]->addSelection("HDMI Camera", !g_pCurrentModel->isRunningOnOpenIPCHardware());
      m_pItemsSelect[12]->addSelection("Veye 290", !g_pCurrentModel->isRunningOnOpenIPCHardware());
      m_pItemsSelect[12]->addSelection("Veye 307", !g_pCurrentModel->isRunningOnOpenIPCHardware());
      m_pItemsSelect[12]->addSelection("Veye 327", !g_pCurrentModel->isRunningOnOpenIPCHardware());
      m_pItemsSelect[12]->addSelection("OpenIPC IMX307", g_pCurrentModel->isRunningOnOpenIPCHardware());
      m_pItemsSelect[12]->addSelection("OpenIPC IMX335", g_pCurrentModel->isRunningOnOpenIPCHardware());
      m_pItemsSelect[12]->addSelection("OpenIPC IMX415", g_pCurrentModel->isRunningOnOpenIPCHardware());
      //m_pItemsSelect[12]->addSelection("USB Camera", false);
      m_pItemsSelect[12]->addSelection("IP Camera", false);
      m_pItemsSelect[12]->setIsEditable();
      m_IndexForceMode = addMenuItem(m_pItemsSelect[12]);
   }

   m_IndexProfile = -1;
   if ( ! m_bShowCompact )
   {
      m_pItemsSelect[0] = new MenuItemSelect(L("Profile")); 
      for( int i=0; i<MODEL_CAMERA_PROFILES; i++ )
      {
         char szBuff[32];
         sprintf(szBuff, "Profile %s", model_getCameraProfileName(i));
         if ( i == 2 )
            strcpy(szBuff, "Profile C");
         m_pItemsSelect[0]->addSelection(szBuff);
      }
      m_pItemsSelect[0]->setIsEditable();
      m_IndexProfile = addMenuItem(m_pItemsSelect[0]);
   }
   m_pItemsSlider[0] = new MenuItemSlider(L("Brightness"), 0,100,50, fSliderWidth);
   if ( ! m_bShowCompact )
      m_pItemsSlider[0]->setMargin(fMargin);
   m_IndexBrightness = addMenuItem(m_pItemsSlider[0]);
 
   m_pItemsSlider[1] = new MenuItemSlider(L("Contrast"), 0,100,0, fSliderWidth);
   if ( ! m_bShowCompact )
      m_pItemsSlider[1]->setMargin(fMargin);
   m_IndexContrast = addMenuItem(m_pItemsSlider[1]);

   m_pItemsSlider[2] = new MenuItemSlider(L("Saturation"), -100,100,0, fSliderWidth);
   if ( ! m_bShowCompact )
      m_pItemsSlider[2]->setMargin(fMargin);
   m_IndexSaturation = addMenuItem(m_pItemsSlider[2]);

   if ( g_pCurrentModel->isActiveCameraVeye307() ||
        g_pCurrentModel->isActiveCameraOpenIPC() )
   {
      m_pItemsSlider[8] = new MenuItemSlider(L("Hue"), 0,100,0, fSliderWidth);
      if ( ! m_bShowCompact )
         m_pItemsSlider[8]->setMargin(fMargin);
      m_IndexHue = addMenuItem(m_pItemsSlider[8]);
   }

   if ( g_pCurrentModel->isActiveCameraVeye327290() || g_pCurrentModel->isActiveCameraCSICompatible() )
   {
      if ( g_pCurrentModel->isActiveCameraVeye327290() )
         m_pItemsSlider[3] = new MenuItemSlider(L("Sharpness"), 0,10,0, fSliderWidth);
      else
         m_pItemsSlider[3] = new MenuItemSlider(L("Sharpness"), -100,100,0, fSliderWidth);
      if ( ! m_bShowCompact )
         m_pItemsSlider[3]->setMargin(fMargin);
      m_IndexSharpness = addMenuItem(m_pItemsSlider[3]);
   }

   m_IndexDayNight = -1;
   if ( (!m_bShowCompact) && (g_pCurrentModel->isActiveCameraVeye()) )
   {
      m_pItemsSelect[15] = new MenuItemSelect(L("Day/Night Mode"), L("Sets the mode to daylight (color and IR cut) or night (black and white, no IR cut)."));
      m_pItemsSelect[15]->addSelection(L("Daylight"));
      m_pItemsSelect[15]->addSelection(L("Night B&W"));
      m_pItemsSelect[15]->setMargin(fMargin);
      m_IndexDayNight = addMenuItem(m_pItemsSelect[15]);
   }

   if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
   {
      m_pItemsSelect[3] = new MenuItemSelect(L("White Balance"));
      m_pItemsSelect[3]->addSelection(L("Off"));
      m_pItemsSelect[3]->addSelection(L("Auto"));
      if ( ! g_pCurrentModel->isActiveCameraVeye() )
      {
         m_pItemsSelect[3]->addSelection("Sun");
         m_pItemsSelect[3]->addSelection("Cloud");
         m_pItemsSelect[3]->addSelection("Shade");
         m_pItemsSelect[3]->addSelection("Horizont");
         m_pItemsSelect[3]->addSelection("Grey World");
      }
      m_pItemsSelect[3]->setIsEditable();
      m_pItemsSelect[3]->setMargin(fMargin);
      m_IndexWhiteBalance = addMenuItem(m_pItemsSelect[3]);
   }

   if ( g_pCurrentModel->isActiveCameraVeye327290() )
   {
      m_pItemsSelect[14] = new MenuItemSelect(L("WDR mode"));
      m_pItemsSelect[14]->addSelection(L("Off"));
      m_pItemsSelect[14]->addSelection("On, Low");
      m_pItemsSelect[14]->addSelection("On, High");
      m_pItemsSelect[14]->addSelection("On, DOL");
      m_pItemsSelect[14]->setIsEditable();
      m_pItemsSelect[14]->setMargin(fMargin);
      m_IndexWDR = addMenuItem(m_pItemsSelect[14]);
   }

   if ( g_pCurrentModel->isActiveCameraVeye327290() )
   {
      m_pItemsSlider[7] = new MenuItemSlider("AGC", L("AGC stands for auto gain control. It is a part of auto exposure, in different light intensity, will try to adjust the gain of sensor to achieve the same image brightness."), 0,15,5, fSliderWidth);
      m_pItemsSlider[7]->setMargin(fMargin);
      m_IndexAGC = addMenuItem(m_pItemsSlider[7]);
   }

   m_IndexAnalogGains = -1;
   if ( (! m_bShowCompact) && g_pCurrentModel->isActiveCameraCSI() )
   {
      m_IndexAnalogGains = addMenuItem( new MenuItem(L("Analog Gains"), L("Sets the analog gains when the AWB is turned off.")));
      m_pMenuItems[m_IndexAnalogGains]->showArrow();
      m_pMenuItems[m_IndexAnalogGains]->setMargin(fMargin);
   }

   m_IndexShutterMode = -1;
   m_IndexShutterValue = -1;
   m_IndexOpenIPC3A = -1;
   if ( ! m_bShowCompact )
   {
      if ( g_pCurrentModel->isActiveCameraCSI() || g_pCurrentModel->isActiveCameraVeye() )
      {
         m_pItemsSelect[7] = new MenuItemSelect(L("Shutter Speed Mode"), L("Sets shutter speed to auto or manual."));
         m_pItemsSelect[7]->addSelection(L("Auto"));
         m_pItemsSelect[7]->addSelection(L("Manual"));
         m_pItemsSelect[7]->setIsEditable();
         m_pItemsSelect[7]->setMargin(fMargin);
         m_IndexShutterMode = addMenuItem(m_pItemsSelect[7]);
      
         m_pItemsSlider[6] = new MenuItemSlider("Shutter Speed", "Sets the shutter speed to 1/x of a second", 30,5000,1000, fSliderWidth);
         m_pItemsSlider[6]->setStep(30);
         m_pItemsSlider[6]->setMargin(fMargin);
         m_IndexShutterValue = addMenuItem(m_pItemsSlider[6]);
      }
      else if ( hardware_board_is_sigmastar(g_pCurrentModel->hwCapabilities.uBoardType) )
      {
         m_pItemsSelect[22] = new MenuItemSelect("3D Noise Algo", "Sets NR3D algorithms used by the camera ISP processor to reduce noise in the image.");
         m_pItemsSelect[22]->addSelection(L("FPV (Default)"));
         m_pItemsSelect[22]->addSelection("Sigmastar");
         m_pItemsSelect[22]->setIsEditable();
         m_pItemsSelect[22]->setMargin(fMargin);
         m_IndexOpenIPC3A = addMenuItem(m_pItemsSelect[22]);

         m_pItemsSelect[7] = new MenuItemSelect(L("Shutter Speed Mode"), L("Sets the shutter speed to be auto controllerd by camera or manual set by user."));  
         m_pItemsSelect[7]->addSelection(L("Auto"));
         m_pItemsSelect[7]->addSelection(L("Manual"));
         m_pItemsSelect[7]->setIsEditable();
         m_pItemsSelect[7]->setMargin(fMargin);
         m_IndexShutterMode = addMenuItem(m_pItemsSelect[7]);

         m_pItemsSlider[6] = new MenuItemSlider("Shutter Speed", "Sets the camera shutter speed, in miliseconds.", 1,100,10, fSliderWidth);
         m_pItemsSlider[6]->setMargin(fMargin);
         m_IndexShutterValue = addMenuItem(m_pItemsSlider[6]);
      }
   }

   if ( g_pCurrentModel->isActiveCameraCSI() )
   {
      m_pItemsSelect[2] = new MenuItemSelect(L("Exposure"));  
      m_pItemsSelect[2]->addSelection(L("Auto"));
      m_pItemsSelect[2]->addSelection("Night");
      m_pItemsSelect[2]->addSelection("Back Light");
      m_pItemsSelect[2]->addSelection("Sports");
      m_pItemsSelect[2]->addSelection("Very Long");
      m_pItemsSelect[2]->addSelection("Fixed FPS");
      m_pItemsSelect[2]->addSelection("Antishake");
      m_pItemsSelect[2]->addSelection(L("Off"));
      m_pItemsSelect[2]->setIsEditable();
      m_pItemsSelect[2]->setMargin(fMargin);
      m_IndexExposureMode = addMenuItem(m_pItemsSelect[2]);
   }

   m_IndexMetering = -1;
   if ( (! m_bShowCompact) && g_pCurrentModel->isActiveCameraCSI() )
   {
      m_pItemsSelect[4] = new MenuItemSelect(L("Metering"));
      m_pItemsSelect[4]->addSelection("Average");
      m_pItemsSelect[4]->addSelection("Spot");
      m_pItemsSelect[4]->addSelection("Backlit");
      m_pItemsSelect[4]->addSelection("Matrix");
      m_pItemsSelect[4]->setIsEditable();
      m_pItemsSelect[4]->setMargin(fMargin);
      m_IndexMetering = addMenuItem(m_pItemsSelect[4]);
   }

   m_IndexDRC = -1;
   if ( (! m_bShowCompact) && g_pCurrentModel->isActiveCameraCSI() )
   {
      m_pItemsSelect[5] = new MenuItemSelect("DRC", "Dynamic Range Compensation");
      m_pItemsSelect[5]->addSelection("Off");
      m_pItemsSelect[5]->addSelection("Low");
      m_pItemsSelect[5]->addSelection("Medium");
      m_pItemsSelect[5]->addSelection("High");
      m_pItemsSelect[5]->setIsEditable();
      m_pItemsSelect[5]->setMargin(fMargin);
      m_IndexDRC = addMenuItem(m_pItemsSelect[5]);
   }

   m_IndexISO = -1;
   m_IndexISOValue = -1;
   if ( (! m_bShowCompact) && g_pCurrentModel->isActiveCameraCSI() )
   {
      m_pItemsSelect[6] = new MenuItemSelect("ISO", L("Sets manual/auto ISO."));
      m_pItemsSelect[6]->addSelection(L("Auto"));
      m_pItemsSelect[6]->addSelection(L("Manual"));
      m_pItemsSelect[6]->setIsEditable();
      m_pItemsSelect[6]->setMargin(fMargin);
      m_IndexISO = addMenuItem(m_pItemsSelect[6]);

      m_pItemsSlider[5] = new MenuItemSlider("", "ISO Value", 100,800,400, fSliderWidth);
      m_pItemsSlider[5]->setStep(20);
      m_pItemsSlider[5]->setMargin(fMargin);
      m_IndexISOValue = addMenuItem(m_pItemsSlider[5]);
   }

   m_IndexEV = -1;
   m_IndexEVValue = -1;
   if ( (! m_bShowCompact) && g_pCurrentModel->isActiveCameraCSI() )
   {
      m_pItemsSelect[1] = new MenuItemSelect("EV Compensation", "Sets exposure compensation to manual or auto. Manual values work only when AWB is enabled.");
      m_pItemsSelect[1]->addSelection("Auto");
      m_pItemsSelect[1]->addSelection("Manual");
      m_pItemsSelect[1]->setIsEditable();
      m_pItemsSelect[1]->setMargin(fMargin);
      m_IndexEV = addMenuItem(m_pItemsSelect[1]);

      m_pItemsSlider[4] = new MenuItemSlider("", "EV Compensation value", -10,10,0, fSliderWidth);
      m_pItemsSlider[4]->setMargin(fMargin);
      m_IndexEVValue = addMenuItem(m_pItemsSlider[4]);
   }

   m_IndexVideoStab = -1;
   if ( (! m_bShowCompact) && g_pCurrentModel->isActiveCameraCSI() )
   {
      m_pItemsSelect[8] = new MenuItemSelect(L("Video Stabilisation"), L("Enables video stabilisation, if supported by the camera."));
      m_pItemsSelect[8]->addSelection(L("Off"));
      m_pItemsSelect[8]->addSelection(L("On"));
      m_pItemsSelect[8]->setIsEditable();
      m_pItemsSelect[8]->setMargin(fMargin);
      m_IndexVideoStab = addMenuItem(m_pItemsSelect[8]);
   }

   m_pItemsSelect[9] = new MenuItemSelect(L("Flip camera"), L("Flips the camera video output upside down."));  
   m_pItemsSelect[9]->addSelection(L("No"));
   m_pItemsSelect[9]->addSelection(L("Yes"));
   m_pItemsSelect[9]->setIsEditable();
   m_pItemsSelect[9]->setMargin(fMargin);
   m_IndexFlip = addMenuItem(m_pItemsSelect[9]);


   m_IndexIRCut = -1;
   if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
   {
      m_pItemsSelect[20] = new MenuItemSelect(L("IR Cut"), L("Turn IR cut filter on/off."));
      m_pItemsSelect[20]->addSelection(L("On"));
      m_pItemsSelect[20]->addSelection(L("Off"));
      m_pItemsSelect[20]->setIsEditable();
      m_pItemsSelect[20]->setMargin(fMargin);
      m_IndexIRCut = addMenuItem(m_pItemsSelect[20]);
   }

   m_IndexOpenIPCDayNight = -1;
   if ( ! m_bShowCompact )
   if (g_pCurrentModel->isRunningOnOpenIPCHardware())
   if (g_pCurrentModel->isActiveCameraOpenIPC())
   {
      m_pItemsSelect[21] = new MenuItemSelect(L("Day/Night Mode"), L("Sets the mode to daylight (color) or night (black and white)."));
      m_pItemsSelect[21]->addSelection(L("Daylight"));
      m_pItemsSelect[21]->addSelection(L("Night B&W"));
      m_pItemsSelect[21]->setIsEditable();
      m_pItemsSelect[21]->setMargin(fMargin);
      m_IndexOpenIPCDayNight = addMenuItem(m_pItemsSelect[21]);
   }

   m_IndexOpenIPCBinProfile = -1;
   if ( ! m_bShowCompact )
   if (g_pCurrentModel->isRunningOnOpenIPCHardware())
   if (g_pCurrentModel->isActiveCameraOpenIPC())
   {
      populateUserCalibrationFiles();
   }

   m_IndexReset = -1;
   if ( ! m_bShowCompact )
   {
      m_IndexReset = addMenuItem(new MenuItem(L("Reset Profile"), L("Resets the current vehicle's camera paramters (brightness, contrast, etc) to the default values for the current profile.")));
      //m_pMenuItems[m_IndexReset]->setMargin(fMargin);
   }

   m_IndexCalibrateHDMI = -1;
   if ( ! m_bShowCompact )
   {
      m_IndexCalibrateHDMI = addMenuItem(new MenuItem(L("Calibrate HDMI output"), L("Calibrate the colors, brightness and contrast on the controller display.")));
      m_pMenuItems[m_IndexCalibrateHDMI]->showArrow();
   }

   if ( m_bShowCompact )
      m_IndexShowFull = addMenuItem(new MenuItem(L("Show all camera settings"), L("")));

   log_line("MenuCamera: Added items.");

   updateUIValues();
   log_line("MenuCamera: Updated items.");

   if ( iTmp >= 0 )
   {
      m_SelectedIndex = iTmp;
      onFocusedItemChanged();
   }
}

void MenuVehicleCamera::updateUIValues()
{
   int iCameraProfileIndex = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;
   log_line("MenuVehicleCamera: update UI values (current camera profile: %d)...", iCameraProfileIndex);

   if ( -1 != m_IndexProfile )
      m_pItemsSelect[0]->setSelection(iCameraProfileIndex);

   if ( (m_IndexCamera != -1) && (NULL != m_pItemsSelect[11]) )
      m_pItemsSelect[11]->setSelection(g_pCurrentModel->iCurrentCamera);

   if ( -1 != m_IndexOpenIPCBinProfile )
      updateCalibrationFileSelection();

   if ( -1 != m_IndexForceMode )
   {
      if ( 0 == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
         m_pItemsSelect[12]->setSelection(0);
      else if ( CAMERA_TYPE_CSI == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
         m_pItemsSelect[12]->setSelection(1);
      else if ( CAMERA_TYPE_HDMI == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
         m_pItemsSelect[12]->setSelection(2);
      else if ( CAMERA_TYPE_VEYE290 == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
         m_pItemsSelect[12]->setSelection(3);
      else if ( CAMERA_TYPE_VEYE307 == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
         m_pItemsSelect[12]->setSelection(4);
      else if ( CAMERA_TYPE_VEYE327 == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
         m_pItemsSelect[12]->setSelection(5);
      else if ( CAMERA_TYPE_OPENIPC_IMX307 == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
         m_pItemsSelect[12]->setSelection(6);
      else if ( CAMERA_TYPE_OPENIPC_IMX335 == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
         m_pItemsSelect[12]->setSelection(7);
      else if ( CAMERA_TYPE_OPENIPC_IMX415 == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
         m_pItemsSelect[12]->setSelection(8);
      else if ( CAMERA_TYPE_IP == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
         m_pItemsSelect[12]->setSelection(10);
      else
         m_pItemsSelect[12]->setSelection(0);
   }

   m_pItemsSlider[0]->setCurrentValue(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].brightness);
   m_pItemsSlider[1]->setCurrentValue(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].contrast);
   m_pItemsSlider[2]->setCurrentValue(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].saturation-100);

   if ( g_pCurrentModel->isActiveCameraVeye327290() || ( ! g_pCurrentModel->isActiveCameraVeye()) )
   if ( (NULL != m_pItemsSlider[3]) && (-1 != m_IndexSharpness) )
   {
      if ( g_pCurrentModel->isActiveCameraVeye327290() )
         m_pItemsSlider[3]->setCurrentValue(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].sharpness-100);
      else
         m_pItemsSlider[3]->setCurrentValue(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].sharpness-100);
   }

   if ( g_pCurrentModel->isActiveCameraHDMI() )
   {
      m_pItemsSlider[0]->setEnabled(false);
      m_pItemsSlider[1]->setEnabled(false);
      m_pItemsSlider[2]->setEnabled(false);
      if ( NULL != m_pItemsSlider[3] )
         m_pItemsSlider[3]->setEnabled(false);
   }
   else
   {
      m_pItemsSlider[0]->setEnabled(true);
      m_pItemsSlider[1]->setEnabled(true);
      m_pItemsSlider[2]->setEnabled(true);
      if ( NULL != m_pItemsSlider[3] )
         m_pItemsSlider[3]->setEnabled(true);
   }

   if ( (-1 != m_IndexHue) && (NULL != m_pItemsSlider[8]) )
   {
      m_pItemsSlider[8]->setEnabled(true);
      m_pItemsSlider[8]->setCurrentValue((int)(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].hue));
   }

   if ( (-1 != m_IndexDayNight) && (NULL != m_pItemsSelect[15]) )
   {
      if ( g_pCurrentModel->isActiveCameraVeye() )
      {
         m_pItemsSelect[15]->setEnabled(true);
         m_pItemsSelect[15]->setSelectedIndex(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].dayNightMode );
      }
      else
         m_pItemsSelect[15]->setEnabled(false);
   }

   if ( hardware_board_is_sigmastar(g_pCurrentModel->hwCapabilities.uBoardType) )
   if ( (-1 != m_IndexOpenIPC3A) && (NULL != m_pItemsSelect[22]) )
   {
      if ( g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].uFlags & CAMERA_FLAG_OPENIPC_3A_FPV )
         m_pItemsSelect[22]->setSelectedIndex(0);
      else
         m_pItemsSelect[22]->setSelectedIndex(1);
   }
   if ( (-1 != m_IndexWhiteBalance) && (NULL != m_pItemsSelect[3]) )
      m_pItemsSelect[3]->setSelection(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].whitebalance);

   if ( (-1 != m_IndexWDR) && (NULL != m_pItemsSelect[14]) )
      m_pItemsSelect[14]->setSelection((int)g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].wdr);

   if ( (m_IndexAGC != -1) && (NULL != m_pItemsSlider[7]) )
      m_pItemsSlider[7]->setCurrentValue((int)g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].drc);

   bool enableGains = false;
   if ( g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].whitebalance == 0 )
      enableGains = true;

   if ( -1 != m_IndexAnalogGains )
      m_pMenuItems[m_IndexAnalogGains]->setEnabled(enableGains);

   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
   {
      if ( hardware_board_is_sigmastar(g_pCurrentModel->hwCapabilities.uBoardType) )
      if ( -1 != m_IndexShutterMode )
      {
         if ( g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].iShutterSpeed <= 0 )
         {
            m_pItemsSelect[7]->setSelectedIndex(0);
            if ( (-1 != m_IndexShutterValue) && (m_pItemsSlider[6] != NULL) )
            {
               m_pItemsSlider[6]->setEnabled(false);
               m_pItemsSlider[6]->setCurrentValue(-g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].iShutterSpeed);
            }
         } 
         else
         {
            m_pItemsSelect[7]->setSelectedIndex(1);
            if ( (m_IndexShutterValue != -1) && (m_pItemsSlider[6] != NULL) )
            {
               m_pItemsSlider[6]->setEnabled(true);
               m_pItemsSlider[6]->setCurrentValue(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].iShutterSpeed);
            }
         }
      }
   }
   else
   {
      if ( (-1 != m_IndexShutterMode) && (NULL != m_pItemsSelect[7]) )
         m_pItemsSelect[7]->setSelection((g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].iShutterSpeed > 0)?1:0);

      if ( (-1 != m_IndexShutterValue) && (NULL != m_pItemsSlider[6]) )
      {
         if ( g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].iShutterSpeed > 0 )
            m_pItemsSlider[6]->setCurrentValue((int)g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].iShutterSpeed);
         else
            m_pItemsSlider[6]->setCurrentValue(-(int)g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].iShutterSpeed);
         m_pItemsSlider[6]->setEnabled((g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].iShutterSpeed > 0)?true:false);
      }
   }

   if ( g_pCurrentModel->isActiveCameraCSI() )
   if ( (m_IndexExposureMode != -1) && (m_pItemsSelect[2] != NULL) )
      m_pItemsSelect[2]->setSelection(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].exposure);
   
   if ( (m_IndexMetering != -1) && (m_pItemsSelect[4] != NULL) )
      m_pItemsSelect[4]->setSelection(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].metering);
   
   if ( (m_IndexDRC != -1) && (m_pItemsSelect[5] != NULL) )
      m_pItemsSelect[5]->setSelection(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].drc);

   if ( (-1 != m_IndexISO) && (NULL != m_pItemsSelect[6]) )
      m_pItemsSelect[6]->setSelection(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].iso != 0);
   
   if ( (-1 != m_IndexISOValue) && (NULL != m_pItemsSlider[5]) )
   {
      m_pItemsSlider[5]->setCurrentValue((int)g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].iso);
      m_pItemsSlider[5]->setEnabled(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].iso != 0);
   }

   if ( (-1 != m_IndexEV) && (NULL != m_pItemsSelect[1]) )
      m_pItemsSelect[1]->setSelection(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].ev != 0);
 
   if ( (-1 != m_IndexEVValue) && (NULL != m_pItemsSlider[4]) )
   {
      m_pItemsSlider[4]->setCurrentValue(((int)g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].ev)-11);
      m_pItemsSlider[4]->setEnabled(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].ev != 0);
   }

   if ( (-1 != m_IndexVideoStab) && (NULL != m_pItemsSelect[8]) )
      m_pItemsSelect[8]->setSelection((g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].vstab!=0)?1:0);

   if ( (-1 != m_IndexFlip) && (NULL != m_pItemsSelect[9]) )
      m_pItemsSelect[9]->setSelection((g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].flip_image!=0)?1:0);

   if ( (-1 != m_IndexIRCut) && (NULL != m_pItemsSelect[20]) )
   {
      if ( g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].uFlags & CAMERA_FLAG_IR_FILTER_OFF )
         m_pItemsSelect[20]->setSelectedIndex(1);
      else
         m_pItemsSelect[20]->setSelectedIndex(0);
   }

   if ( (-1 != m_IndexOpenIPCDayNight) && (NULL != m_pItemsSelect[21]) )
   {
      if ( g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfileIndex].uFlags & CAMERA_FLAG_OPENIPC_DAYLIGHT_OFF )
         m_pItemsSelect[21]->setSelectedIndex(1);
      else
         m_pItemsSelect[21]->setSelectedIndex(0);
   }
}

void MenuVehicleCamera::valuesToUI()
{
   int iTmp = getSelectedMenuItemIndex();
   
   addItems();

   if ( (iTmp >= 0) && (iTmp <m_ItemsCount) )
   {
      m_SelectedIndex = iTmp;
      onFocusedItemChanged();
   }
}

void MenuVehicleCamera::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
   {
      y += RenderItem(i,y);
   }
   RenderEnd(yTop);
}

bool MenuVehicleCamera::canSendLiveUpdates(int iItemIndex)
{
   if ( NULL == g_pCurrentModel || (! g_pCurrentModel->hasCamera()) )
      return false;

   if ( iItemIndex != m_IndexBrightness )
   if ( iItemIndex != m_IndexContrast )
   if ( iItemIndex != m_IndexSaturation )
   if ( iItemIndex != m_IndexSharpness )
   if ( iItemIndex != m_IndexHue )
   if ( (iItemIndex != m_IndexAGC) || (! g_pCurrentModel->isActiveCameraVeye327290()) )
      return false;

   if ( g_pCurrentModel->isActiveCameraCSI() )
      return true;

   if ( g_pCurrentModel->isActiveCameraVeye327290() )
      return true;

   if ( g_pCurrentModel->isActiveCameraVeye307() )
      return true;

   if ( hardware_board_is_openipc(g_pCurrentModel->hwCapabilities.uBoardType) )
   if ( ! hardware_board_is_goke(g_pCurrentModel->hwCapabilities.uBoardType) )
      return true;
     
   return false;
}

void MenuVehicleCamera::sendCameraParams(int itemIndex, bool bQuick)
{
   type_camera_parameters cparams;
   memcpy(&cparams, &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), sizeof(type_camera_parameters));

   int iProfile = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;
   bool bSendToVehicle = false;

   if ( m_IndexBrightness == itemIndex || itemIndex == -1 )
   if ( cparams.profiles[iProfile].brightness != m_pItemsSlider[0]->getCurrentValue() )
   {
      cparams.profiles[iProfile].brightness = m_pItemsSlider[0]->getCurrentValue();
      bSendToVehicle = true;
   }

   if ( m_IndexContrast == itemIndex || itemIndex == -1 )
   if ( cparams.profiles[iProfile].contrast != m_pItemsSlider[1]->getCurrentValue() )
   {
      cparams.profiles[iProfile].contrast = m_pItemsSlider[1]->getCurrentValue();
      bSendToVehicle = true;
   }

   if ( (m_IndexHue == itemIndex) && (m_IndexHue != -1) )
   if ( NULL != m_pItemsSlider[8] )
   if ( (int)(cparams.profiles[iProfile].hue) != m_pItemsSlider[8]->getCurrentValue() )
   {
      cparams.profiles[iProfile].hue = m_pItemsSlider[8]->getCurrentValue();
      bSendToVehicle = true;
   }

   if ( m_IndexSaturation == itemIndex || itemIndex == -1 )
   if ( cparams.profiles[iProfile].saturation != (m_pItemsSlider[2]->getCurrentValue()+100) )
   {
      cparams.profiles[iProfile].saturation = m_pItemsSlider[2]->getCurrentValue()+100;
      bSendToVehicle = true;
   }

   if ( -1 != m_IndexIRCut )
   if ( (m_IndexIRCut == itemIndex) || (itemIndex == -1) )
   {    
      if ( 0 == m_pItemsSelect[20]->getSelectedIndex() )
         cparams.profiles[iProfile].uFlags &= ~CAMERA_FLAG_IR_FILTER_OFF;
      else
         cparams.profiles[iProfile].uFlags |= CAMERA_FLAG_IR_FILTER_OFF;
      bSendToVehicle = true;
   }

   if ( (-1 != m_IndexOpenIPC3A) && (NULL != m_pItemsSelect[22]) )
   {
      if ( 1 == m_pItemsSelect[22]->getSelectedIndex() )
         cparams.profiles[iProfile].uFlags &= ~CAMERA_FLAG_OPENIPC_3A_FPV;
      else
         cparams.profiles[iProfile].uFlags |= CAMERA_FLAG_OPENIPC_3A_FPV; 
   }
   if (-1 != m_IndexOpenIPCDayNight)
   if ((m_IndexOpenIPCDayNight == itemIndex) || (itemIndex == -1))
   {
      if (0 == m_pItemsSelect[21]->getSelectedIndex())
         cparams.profiles[iProfile].uFlags &= ~CAMERA_FLAG_OPENIPC_DAYLIGHT_OFF;
      else
         cparams.profiles[iProfile].uFlags |= CAMERA_FLAG_OPENIPC_DAYLIGHT_OFF;
      bSendToVehicle = true;
   }

   if ( (m_IndexSharpness != -1 && m_IndexSharpness == itemIndex ) || itemIndex == -1 )
   if ( m_pItemsSlider[3] != NULL )
   {
      if ( g_pCurrentModel->isActiveCameraVeye327290() )
      {
         if ( cparams.profiles[iProfile].sharpness != (m_pItemsSlider[3]->getCurrentValue()+100) )
         {
            cparams.profiles[iProfile].sharpness = m_pItemsSlider[3]->getCurrentValue()+100;
            bSendToVehicle = true;
         }
      }
      else
      {
         if ( cparams.profiles[iProfile].sharpness != (m_pItemsSlider[3]->getCurrentValue()+100) )
         {
            cparams.profiles[iProfile].sharpness = m_pItemsSlider[3]->getCurrentValue()+100;
            bSendToVehicle = true;
         }
      }
   }

   if ( m_IndexDayNight != -1 )
   if ( (m_IndexDayNight == itemIndex) || (itemIndex == -1) )
   if ( cparams.profiles[iProfile].dayNightMode != (m_pItemsSelect[15]->getSelectedIndex()) )
   {
      cparams.profiles[iProfile].dayNightMode = m_pItemsSelect[15]->getSelectedIndex();
      bSendToVehicle = true;
   }

   if ( (m_IndexEV != -1) && (m_IndexEVValue != -1) && (m_pItemsSelect[1] != NULL) && (m_pItemsSlider[4] != NULL) )
   {
      cparams.profiles[iProfile].ev = (m_pItemsSelect[1]->getSelectedIndex() == 0)?0:11;
      if ( m_pItemsSelect[1]->getSelectedIndex() != 0 )
         cparams.profiles[iProfile].ev = m_pItemsSlider[4]->getCurrentValue()+11;    
   }

   if ( g_pCurrentModel->isActiveCameraCSI() )
   if ( (m_IndexExposureMode != -1) && (m_pItemsSelect[2] != NULL) )
      cparams.profiles[iProfile].exposure = m_pItemsSelect[2]->getSelectedIndex();

   if ( m_IndexWhiteBalance != -1 )
      cparams.profiles[iProfile].whitebalance = m_pItemsSelect[3]->getSelectedIndex();

   if ( (m_IndexAGC != -1) && (m_pItemsSlider[7] != NULL) )
   {
      cparams.profiles[iProfile].drc = m_pItemsSlider[7]->getCurrentValue();
      bSendToVehicle = true;
   }
   else if ( (m_IndexDRC != -1) && (m_pItemsSelect[5] != NULL) )
      cparams.profiles[iProfile].drc = m_pItemsSelect[5]->getSelectedIndex();

   if ( (m_IndexMetering != -1) && (m_pItemsSelect[4] != NULL) )
      cparams.profiles[iProfile].metering = m_pItemsSelect[4]->getSelectedIndex();

   if ( (m_IndexISO != -1) && (m_IndexISOValue != -1) && (m_pItemsSelect[6] != NULL) && (m_pItemsSlider[5] != NULL) )
   {
      cparams.profiles[iProfile].iso = (m_pItemsSelect[6]->getSelectedIndex() == 0)?0:400;
      if ( m_pItemsSelect[6]->getSelectedIndex() != 0 )
         cparams.profiles[iProfile].iso = m_pItemsSlider[5]->getCurrentValue();
   }

   if ( (m_IndexShutterMode != -1) && (m_IndexShutterValue != -1) )
   {
      cparams.profiles[iProfile].iShutterSpeed = 0;
      if ( m_pItemsSlider[6] != NULL )
      {
         if ( 0 == m_pItemsSelect[7]->getSelectedIndex() )
            cparams.profiles[iProfile].iShutterSpeed = -m_pItemsSlider[6]->getCurrentValue();
         else
            cparams.profiles[iProfile].iShutterSpeed = m_pItemsSlider[6]->getCurrentValue();
      }

      if ( g_pCurrentModel->isActiveCameraOpenIPC() )
      if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
      if ( g_pCurrentModel->validate_fps_and_exposure_settings(&cparams.profiles[iProfile], false))
         addMessage("You camera exposure time was adjusted to accomodate the currently set video FPS.");
   }

   if ( (m_IndexVideoStab != -1) && (m_pItemsSelect[8] != NULL) )
      cparams.profiles[iProfile].vstab = m_pItemsSelect[8]->getSelectedIndex();

   if ( (m_IndexWDR != -1) && (m_pItemsSelect[14] != NULL) )
      cparams.profiles[iProfile].wdr = m_pItemsSelect[14]->getSelectedIndex();
  
   if ( NULL != m_pItemsSelect[9] )
      cparams.profiles[iProfile].flip_image = m_pItemsSelect[9]->getSelectedIndex();

   if ( bQuick )
   {
      if ( bSendToVehicle )
      {
         m_bDidAnyLiveUpdates = true;
         memcpy(&(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), &cparams, sizeof(type_camera_parameters));
         handle_commands_send_single_oneway_command(2, COMMAND_ID_SET_CAMERA_PARAMETERS, g_pCurrentModel->iCurrentCamera, (u8*)(&cparams), sizeof(type_camera_parameters));
         //saveControllerModel(g_pCurrentModel);  
      }
   }
   else
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_CAMERA_PARAMETERS, g_pCurrentModel->iCurrentCamera, (u8*)(&cparams), sizeof(type_camera_parameters)) )
         valuesToUI();
   }
}

void MenuVehicleCamera::onItemValueChanged(int itemIndex)
{
   Menu::onItemValueChanged(itemIndex);

   if ( ! m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( (itemIndex == m_IndexCamera) || (itemIndex == m_IndexForceMode) || (itemIndex == m_IndexFlip) )
      return;

   if ( ! canSendLiveUpdates(itemIndex) )
      return;

   sendCameraParams(itemIndex, true);
}

bool MenuVehicleCamera::binCalibrationFileIsStandardFile(const char* szBinCalibrationFile)
{
   if ( (NULL == szBinCalibrationFile) || (0 == szBinCalibrationFile[0]) )
      return false;

   for( int i=0; i<(int)sizeof(s_CameraBinFilesMapping)/(int)sizeof(s_CameraBinFilesMapping[0]); i++ )
   {
      if ( 0 == strcmp(szBinCalibrationFile, s_CameraBinFilesMapping[i].szBinFile) )
         return true;
   }
   return false;
}

bool MenuVehicleCamera::binCalibrationFileIsUserFile(const char* szBinCalibrationFile)
{
   if ( (NULL == szBinCalibrationFile) || (0 == szBinCalibrationFile[0]) )
      return false;

   for( int i=0; i<m_iCountUserCalibrationFiles; i++ )
   {
      if ( 0 == strcmp(szBinCalibrationFile, &(s_szUserCalibrationFiles[i][0])) )
         return true;
   }
   return false;   
}

void MenuVehicleCamera::populateUserCalibrationFiles()
{
   m_iCountStandardCalibrationFiles = 0;
   m_iCountUserCalibrationFiles = 0;
   m_iSelectionIndexUnknownCalibrationFile = -1;
   m_iSelectionIndexImportCalibrationFiles = -1;

   g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName[MAX_CAMERA_BIN_PROFILE_NAME-1] = 0;
   log_line("MenuVehicleCamera: Populate calibration files...");
   log_line("MenuVehicleCamera: Current calibration file: [%s], type: %d",
      g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName,
      g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraBinProfile);

   m_pItemsSelect[23] = new MenuItemSelect(L("Sensor Calibration File"), L("Sets the camera ISP sensor calibration file to use. Affects WDR, light sensitivity, how camera handles changing light conditions."));
   m_pItemsSelect[23]->addSelection(L("Default"));

   for( int i=0; i<(int)sizeof(s_CameraBinFilesMapping)/(int)sizeof(s_CameraBinFilesMapping[0]); i++ )
   {
      s_CameraBinFilesMapping[i].iMenuSelectionIndex = -1;

      if ( g_pCurrentModel->isActiveCameraSensorOpenIPCIMX415() && s_CameraBinFilesMapping[i].bIsFor415Sensor )
      {
         s_CameraBinFilesMapping[i].iMenuSelectionIndex = m_pItemsSelect[23]->addSelection(s_CameraBinFilesMapping[i].szFriendlyName);
         log_line("MenuVehicleCamera: Added standard calibration (%s / %s) to selection index %d",
            s_CameraBinFilesMapping[i].szFriendlyName, s_CameraBinFilesMapping[i].szBinFile, s_CameraBinFilesMapping[i].iMenuSelectionIndex);
      }
      if ( (!g_pCurrentModel->isActiveCameraSensorOpenIPCIMX415()) && (!s_CameraBinFilesMapping[i].bIsFor415Sensor) )
      {
         s_CameraBinFilesMapping[i].iMenuSelectionIndex = m_pItemsSelect[23]->addSelection(s_CameraBinFilesMapping[i].szFriendlyName);
         log_line("MenuVehicleCamera: Added standard calibration (%s / %s) to selection index %d",
            s_CameraBinFilesMapping[i].szFriendlyName, s_CameraBinFilesMapping[i].szBinFile, s_CameraBinFilesMapping[i].iMenuSelectionIndex);
      }
   }

   m_iCountStandardCalibrationFiles = m_pItemsSelect[23]->getSelectionsCount() - 1;
   log_line("MenuVehicleCamera: Added %d standard calibration files.", m_iCountStandardCalibrationFiles);

   DIR *d;
   struct dirent *dir;
   char szFile[MAX_FILE_PATH_SIZE];
   d = opendir(FOLDER_CALIBRATION_FILES);

   if ( d != NULL )
   {
      while ((dir = readdir(d)) != NULL)
      {
         if ( m_iCountUserCalibrationFiles > 40 )
            break;
         if ( strlen(dir->d_name) < 4 )
            continue;
         if ( NULL == strstr(dir->d_name, ".bin") )
            continue;
         if ( NULL != strstr(dir->d_name, " ") )
            continue;

         snprintf(szFile, sizeof(szFile)/sizeof(szFile[0]), "%s/%s", FOLDER_CALIBRATION_FILES, dir->d_name);
         long lSize = get_filesize(szFile);
         if ( (lSize < 1000) || (lSize > 300000) )
            continue;

         strncpy(szFile, dir->d_name, sizeof(szFile)/sizeof(szFile[0]));
         char* pExt = strstr(szFile, ".bin");
         if ( NULL != pExt )
            *pExt = 0;
         strncpy(&(s_szUserCalibrationFiles[m_iCountUserCalibrationFiles][0]), szFile, MAX_CAMERA_BIN_PROFILE_NAME);
         s_szUserCalibrationFiles[m_iCountUserCalibrationFiles][MAX_CAMERA_BIN_PROFILE_NAME-1] = 0;

         s_iUserCalibrationFilesIndexes[m_iCountUserCalibrationFiles] = m_pItemsSelect[23]->addSelection(szFile);

         log_line("MenuVehicleCamera: Added user calibration (%s) to selection index %d",
            szFile, s_iUserCalibrationFilesIndexes[m_iCountUserCalibrationFiles]);
         m_iCountUserCalibrationFiles++;
      }
      closedir(d);
   }

   log_line("MenuVehicleCamera: Added %d user calibration files.", m_iCountUserCalibrationFiles);
   
   if ( 0 != g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName[0] )
   if ( ! binCalibrationFileIsStandardFile(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName) )
   if ( ! binCalibrationFileIsUserFile(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName) )
   {
      m_iSelectionIndexUnknownCalibrationFile = m_pItemsSelect[23]->addSelection(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName);
      log_line("MenuVehicleCamera: Added unknown calibration file as selection index %d", m_iSelectionIndexUnknownCalibrationFile);
   }

   m_iSelectionIndexImportCalibrationFiles = m_pItemsSelect[23]->addSelection(L("Import additional files"));
   if ( m_iCountUserCalibrationFiles > 0 )
      m_pItemsSelect[23]->addSelection(L("Delete all custom files"));
   
   m_pItemsSelect[23]->setIsEditable();
   m_IndexOpenIPCBinProfile = addMenuItem(m_pItemsSelect[23]);
}


void MenuVehicleCamera::updateCalibrationFileSelection()
{
   if ( -1 == m_IndexOpenIPCBinProfile )
      return;

   int iCameraBinProfile = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraBinProfile;
   if ( (0 == iCameraBinProfile) || ( 0 == g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName[0]) )
   {
      m_pItemsSelect[23]->setSelectedIndex(0);
      return;
   }

   for( int i=0; i<m_iCountUserCalibrationFiles; i++ )
   {
      if ( -1 != s_iUserCalibrationFilesIndexes[i] )
      if ( 0 == strcmp(&(s_szUserCalibrationFiles[i][0]), g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName) )
      {
         m_pItemsSelect[23]->setSelectedIndex(s_iUserCalibrationFilesIndexes[i]);
         return;
      }
   }

   for( int i=0; i<(int)sizeof(s_CameraBinFilesMapping)/(int)sizeof(s_CameraBinFilesMapping[0]); i++ )
   {
      if ( -1 != s_CameraBinFilesMapping[i].iMenuSelectionIndex )
      if ( 0 == strcmp(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName, s_CameraBinFilesMapping[i].szBinFile) )
      {
         m_pItemsSelect[23]->setSelectedIndex(s_CameraBinFilesMapping[i].iMenuSelectionIndex);
         return;
      }
   }

   if ( -1 != m_iSelectionIndexUnknownCalibrationFile )
   {
      m_pItemsSelect[23]->setSelectedIndex(m_iSelectionIndexUnknownCalibrationFile);
      return;
   }
   m_pItemsSelect[23]->setSelectedIndex(0);
}

void MenuVehicleCamera::importCalibrationFiles()
{
   char szComm[256];
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "mkdir -p %s 2>/dev/null 1>/dev/null", FOLDER_CALIBRATION_FILES);
   hw_execute_bash_command(szComm, NULL);

   if ( 1 != hardware_try_mount_usb() )
   {
      addMessage(L("No USB memory stick detected. Please insert a USB stick."));
      return;
   }
   ruby_pause_watchdog("import calibration files from USB stick");

   DIR *d;
   struct dirent *dir;
   char szFile[MAX_FILE_PATH_SIZE];
   int iCountFound = 0;
   d = opendir(FOLDER_USB_MOUNT);

   if ( ! d )
   {
      hardware_unmount_usb();
      ruby_resume_watchdog("failed import calibration files");
      addMessage(L("Failed to quiery the USB memory stick."));
      return;
   }

   while ((dir = readdir(d)) != NULL)
   {
      if ( strlen(dir->d_name) < 4 )
         continue;
      if ( NULL == strstr(dir->d_name, ".bin") )
         continue;
      if ( NULL != strstr(dir->d_name, " ") )
         continue;

      snprintf(szFile, sizeof(szFile)/sizeof(szFile[0]), "%s/%s", FOLDER_USB_MOUNT, dir->d_name);
      long lSize = get_filesize(szFile);
      if ( (lSize < 1000) || (lSize > 300000) )
         continue;

      iCountFound++;
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s %s%s",
         szFile, FOLDER_CALIBRATION_FILES, dir->d_name);
      hw_execute_bash_command(szComm, NULL);
   }   
   closedir(d);

   hardware_unmount_usb();
   ruby_resume_watchdog("finished import calibration files");
   onShow();
   char szText[256];
   if ( 0 == iCountFound )
      strcpy(szText, L("No valid camera calibration files (.bin) found on the USB memory stick."));
   else
      sprintf(szText, "Imported %d camera calibration files", iCountFound);
   addMessage(szText);
   valuesToUI();

}

void MenuVehicleCamera::uploadCalibrationFile(int iType, const char* szCalibrationFile)
{
   if ( (NULL == szCalibrationFile) || (0 == szCalibrationFile[0]) )
   {
      addMessage("Tried to upload invalid calibration file.");
      valuesToUI();
      return;
   }

   char szCalibFileShort[MAX_FILE_PATH_SIZE];
   char szCalibFileFullPath[MAX_FILE_PATH_SIZE];

   strcpy(szCalibFileFullPath, szCalibrationFile);
   if ( NULL != strstr(szCalibFileFullPath, "res/") )
      log_line("MenuVehicleCamera: Uploading standard calib file: [%s]", szCalibFileFullPath);
   if ( NULL == strstr(szCalibFileFullPath, ".bin") )
      strcat(szCalibFileFullPath, ".bin");

   char *pTmp = (char*) &(szCalibrationFile[strlen(szCalibrationFile)-1]);
   while ( (*pTmp != '\\') && (*pTmp != '/') && (pTmp != szCalibrationFile) )
      pTmp--;
   if ( (*pTmp == '\\') || (*pTmp == '/') )
      pTmp++;
   strcpy(szCalibFileShort, pTmp);
   pTmp = strstr(szCalibFileShort, ".bin");
   if ( NULL != pTmp )
      *pTmp = 0;

   log_line("MenuVehicleCamera: Source calibration file path to upload: [%s]", szCalibFileFullPath);
   log_line("MenuVehicleCamera: Source calibration short file name: [%s]", szCalibFileShort);

   t_packet_header_command_upload_calib_file calib_segment_data;
   memset(&calib_segment_data, 0, sizeof(t_packet_header_command_upload_calib_file));

   static int s_iCalibrationFileUploadIndex = 0;
   s_iCalibrationFileUploadIndex++;

   calib_segment_data.calibration_file_type = iType;
   calib_segment_data.iUploadFileIndex = s_iCalibrationFileUploadIndex;
   strncpy(calib_segment_data.szFileName, szCalibFileShort, 63);

   log_line("MenuVehicleCamera: Sending calibration file [%s] to vehicle...", szCalibFileFullPath);
   int iFileSize = (int)get_filesize(szCalibFileFullPath);

   if ( (iFileSize < 1000) || (iFileSize > 200000) )
   {
      log_softerror_and_alarm("MenuVehicleCamera: Tried to send invalid calibration file of %d bytes", iFileSize);
      addMessage("Tried to upload invalid calibration file (too big).");
      valuesToUI();
      return;
   }

   FILE* fp = fopen(szCalibFileFullPath, "rb");
   if ( NULL == fp )
   {
      log_softerror_and_alarm("MenuVehicleCamera: Failed to open calibration file.");
      addMessage("Failed to upload invalid calibration file.");
      valuesToUI();
      return;
   }

   Popup* p = new Popup(L("Uploading camera calibration file. Please wait..."), 0.3, 0.3, 0.7, 60 );
   p->setCentered();
   p->setIconId(g_idIconUplink, get_Color_IconNormal());
   p->addLine("0%");
   popups_add_topmost(p);

   ruby_pause_watchdog("upload calibration files");
   g_bUpdateInProgress = true;
   send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_UPDATE_STARTED,0);

   int iBlockSize = 1100;
   int iCountSegments = iFileSize / iBlockSize;
   if ( iFileSize > iCountSegments * iBlockSize )
      iCountSegments++;

   calib_segment_data.total_file_size = iFileSize;

   send_pause_adaptive_to_router(30000);

   u32 uTimeLastRender = g_TimeNow;
   u8 uSegmentData[4096];
   int iSegmentOffset = 0;

   for( int iSegment=0; iSegment<iCountSegments; iSegment++ )
   {
      log_line("MenuVehicleCamera: Sending segment %d of %d", iSegment, iCountSegments);
      int iRead = fread(&uSegmentData[sizeof(t_packet_header_command_upload_calib_file)], 1, iBlockSize, fp);
      if ( iRead <= 0 )
      {
         log_softerror_and_alarm("MenuVehicleCamera: Failed to read calibration file segment %d (of %d), error: %d", iSegment, iCountSegments, iRead);
         addMessage("An internal error occured uploading calibration file.");
         send_pause_adaptive_to_router(0);
         valuesToUI();
         break;
      }

      g_TimeNow = get_current_timestamp_ms();
      g_TimeNowMicros = get_current_timestamp_micros();
      ruby_signal_alive();

      calib_segment_data.segment_index = iSegment;
      calib_segment_data.segment_offset = iSegmentOffset;
      calib_segment_data.segment_length = iRead;
      calib_segment_data.is_last_segment = 0;
      if ( iSegment == (iCountSegments-1) )
         calib_segment_data.is_last_segment = 1;
      iSegmentOffset += iRead;

      memcpy(uSegmentData, &calib_segment_data, sizeof(t_packet_header_command_upload_calib_file));
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_UPLOAD_CALIBRATION_FILE, 0, uSegmentData, sizeof(t_packet_header_command_upload_calib_file)+iRead) )
      {
         log_softerror_and_alarm("MenuVehicleCamera: Failed to send command to vehicle.");
         addMessage("Failed to send command to vehicle.");
         send_pause_adaptive_to_router(0);
         valuesToUI();
         break;
      }

      u32 uCommandId = handle_commands_get_current_command_counter();
      u32 uTimeStartWait = g_TimeNow;

      while ( g_TimeNow < uTimeStartWait + 4000 )
      {
         g_TimeNow = get_current_timestamp_ms();
         g_TimeNowMicros = get_current_timestamp_micros();
         ruby_signal_alive();
         ruby_processing_loop(true);
         if ( g_TimeNow > (uTimeLastRender+100) )
         {
            p->removeAllLines();
            char szLine[128];
            sprintf(szLine, "%d%%", (iSegment*100)/iCountSegments);
            p->addLine(szLine);
            uTimeLastRender = g_TimeNow;
            render_all(g_TimeNow);
         }
         if ( handle_commands_get_last_command_id_response_received() == uCommandId )
            break;
      }
      if ( (! handle_commands_last_command_succeeded()) || (handle_commands_get_last_command_id_response_received() != uCommandId) )
      {
         log_softerror_and_alarm("MenuVehicleCamera: Failed to get response from vehicle.");
         addMessage("Failed to get response from vehicle.");
         send_pause_adaptive_to_router(0);
         valuesToUI();
         break;
      }
   }

   fclose(fp);
   popups_remove(p);
   send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED,0);
   ruby_resume_watchdog("finished upload calibration files");
   g_bUpdateInProgress = false;
   addMessage(L("Camera calibration file was uploaded and applied."));
   valuesToUI();
}

void MenuVehicleCamera::onCalibrationFileSelectionChanged()
{
   int iIndexSelection = m_pItemsSelect[23]->getSelectedIndex();
   int iSelectionsCount = m_pItemsSelect[23]->getSelectionsCount();

   log_line("MenuVehicleCamera: Selected calibration index %d of %d", iIndexSelection, iSelectionsCount);
   log_line("MenuVehicleCamera: Has %d standard calibrations, %d user calibrations, has unknown calibration index: %s",
      m_iCountStandardCalibrationFiles, m_iCountUserCalibrationFiles, (m_iSelectionIndexUnknownCalibrationFile == -1)? "no":"yes");

   if ( 0 == iIndexSelection )
   {
      send_pause_adaptive_to_router(7000);
      t_packet_header_command_upload_calib_file header;
      memset(&header, 0, sizeof(t_packet_header_command_upload_calib_file));
      header.calibration_file_type = 0;
      header.segment_index = MAX_U32;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_UPLOAD_CALIBRATION_FILE, 0, (u8*)(&header), sizeof(t_packet_header_command_upload_calib_file)) )
      {
         send_pause_adaptive_to_router(0);
         valuesToUI();
      }
      return;
   }

   if ( (-1 != m_iSelectionIndexImportCalibrationFiles) && (iIndexSelection == m_iSelectionIndexImportCalibrationFiles) )
   {
      MenuConfirmation* pMC = new MenuConfirmation(L("Import Files"), L("Import custom camera calibration files from a USB memory stick."), 2, true);
      pMC->addTopLine(L("Insert a USB memory stick into the controller and press [Ok] to import any camera custom calibrations files present on the memory stick."));
      add_menu_to_stack(pMC);
      return;
   }

   if ( m_iCountUserCalibrationFiles > 0 )
   if ( iIndexSelection == iSelectionsCount - 1 )
   {
      MenuConfirmation* pMC = new MenuConfirmation(L("Warning! Delete Confirmation"), L("Are you sure you want to delete all custom calibration files you imported on the controller?"), 1);
      pMC->addTopLine(L("All custom camera calibration files you imported to this controller will be deleted."));
      add_menu_to_stack(pMC);
      return;
   }


   if ( iIndexSelection <= m_iCountStandardCalibrationFiles )
   {
      for( int i=0; i<(int)sizeof(s_CameraBinFilesMapping)/(int)sizeof(s_CameraBinFilesMapping[0]); i++ )
      {
         if ( s_CameraBinFilesMapping[i].iMenuSelectionIndex == iIndexSelection )
         {
            log_line("MenuVehicleCamera: Selected standard calibration: %s / %s",
               s_CameraBinFilesMapping[i].szFriendlyName, s_CameraBinFilesMapping[i].szBinFile);
            char szFile[MAX_FILE_PATH_SIZE];
            strcpy(szFile, FOLDER_RESOURCES);
            strcat(szFile, s_CameraBinFilesMapping[i].szBinFile);
            uploadCalibrationFile(1, szFile);
            return;
         }
      }
      return;
   }

   if ( iIndexSelection > m_iCountStandardCalibrationFiles )
   if ( iIndexSelection <= m_iCountStandardCalibrationFiles + m_iCountUserCalibrationFiles )
   {
      for( int i=0; i<m_iCountUserCalibrationFiles; i++ )
      {
         if ( s_iUserCalibrationFilesIndexes[i] == iIndexSelection )
         {
            log_line("MenuVehicleCamera: Selected user calibration: %s", s_szUserCalibrationFiles[i]);
            char szFile[MAX_FILE_PATH_SIZE];
            strcpy(szFile, FOLDER_CALIBRATION_FILES);
            strcat(szFile, s_szUserCalibrationFiles[i]);
            uploadCalibrationFile(1, szFile);
            return;
         }
      }
      return;    
   }
}

void MenuVehicleCamera::onItemEndEdit(int itemIndex)
{
   Menu::onItemEndEdit(itemIndex);
   if ( ! canSendLiveUpdates(itemIndex) )
      return;

   sendCameraParams(itemIndex, true);
}


void MenuVehicleCamera::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   log_line("MenuVehicleCamera: Returned from child id: %d, return value: %d", iChildMenuId, returnValue);
   
   // Delete all user calibration files
   if ( (1 == iChildMenuId/1000) && (1 == returnValue) )
   {
      // Preserve existing custom one
      char szComm[256];

      if ( g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName[0] != 0 )
      {
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s%s.bin %s%s.bin",
             FOLDER_CALIBRATION_FILES, g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName,
             FOLDER_RUBY_TEMP, g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName);
         hw_execute_bash_command(szComm, NULL);
      }

      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s* 2>/dev/null 1>/dev/null", FOLDER_CALIBRATION_FILES);
      hw_execute_bash_command(szComm, NULL);

      if ( g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName[0] != 0 )
      {
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s%s.bin %s%s.bin",
             FOLDER_RUBY_TEMP, g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName,
             FOLDER_CALIBRATION_FILES, g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName);
         hw_execute_bash_command(szComm, NULL);
      }
      addMessage(L("All custom camera calibration files have been deleted."));
      valuesToUI();
      return;
   }

   // Import calibration files
   if ( (2 == iChildMenuId/1000) && (1 == returnValue) )
   {
      importCalibrationFiles();
      valuesToUI();
      return;
   }
}

void MenuVehicleCamera::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   g_TimeLastVideoCameraChangeCommand = g_TimeNow;

   if ( (-1 != m_IndexCamera) && (m_IndexCamera == m_SelectedIndex) )
   {
      int tmp = m_pItemsSelect[11]->getSelectedIndex();
      if ( tmp != g_pCurrentModel->iCurrentCamera )
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_CURRENT_CAMERA, tmp, NULL, 0) )
         valuesToUI();
      return;
   }

   if ( (-1 != m_IndexShowFull) && (m_IndexShowFull == m_SelectedIndex) )
   {
      m_bShowCompact = false;
      m_SelectedIndex = 0;
      onFocusedItemChanged();
      addItems();
      return;
   }

   if ( (-1 != m_IndexForceMode) && (m_IndexForceMode == m_SelectedIndex) )
   {
      int tmp = m_pItemsSelect[12]->getSelectedIndex();
      int iCamType = 0;
      switch(tmp)
      {
         case 0: iCamType = 0; break;
         case 1: iCamType = CAMERA_TYPE_CSI; break;
         case 2: iCamType = CAMERA_TYPE_HDMI; break;
         case 3: iCamType = CAMERA_TYPE_VEYE290; break;
         case 4: iCamType = CAMERA_TYPE_VEYE307; break;
         case 5: iCamType = CAMERA_TYPE_VEYE327; break;
         case 6: iCamType = CAMERA_TYPE_OPENIPC_IMX307; break;
         case 7: iCamType = CAMERA_TYPE_OPENIPC_IMX335; break;
         case 8: iCamType = CAMERA_TYPE_OPENIPC_IMX415; break;
      }
      if ( iCamType != g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType )
      {
         send_pause_adaptive_to_router(6000);
         if ( ! handle_commands_send_to_vehicle(COMMAND_ID_FORCE_CAMERA_TYPE, iCamType, NULL, 0) )
            valuesToUI();
         if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
            addMessage(L("Vehicle will reboot to update the firmware state."));
      }
      return;
   }

   if ( (-1 != m_IndexProfile) && (m_IndexProfile == m_SelectedIndex) )
   {
      int tmp = m_pItemsSelect[0]->getSelectedIndex();
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_CAMERA_PROFILE, tmp, NULL, 0) )
         valuesToUI();
   }

   if ( m_IndexBrightness == m_SelectedIndex )
   {
      if ( canSendLiveUpdates(m_SelectedIndex) )
         return;
      sendCameraParams(-1, false);
   }

   if ( m_IndexContrast == m_SelectedIndex )
   {
      if ( canSendLiveUpdates(m_SelectedIndex) )
         return;
      sendCameraParams(-1, false);
   }

   if ( m_IndexSaturation == m_SelectedIndex )
   {
      if ( canSendLiveUpdates(m_SelectedIndex) )
         return;
      sendCameraParams(-1, false);
   }

   if ( (-1 != m_IndexHue) && (m_IndexHue == m_SelectedIndex) )
   {
      if ( canSendLiveUpdates(m_SelectedIndex) )
         return;
      sendCameraParams(-1, false);
   }

   if ( m_IndexSharpness == m_SelectedIndex )
   {
      if ( canSendLiveUpdates(m_SelectedIndex) )
         return;
      sendCameraParams(-1, false);
   }

   if ( (-1 != m_IndexDayNight) && (m_IndexDayNight == m_SelectedIndex) )
      sendCameraParams(-1, false);

   if ( (-1 != m_IndexEV) && (m_IndexEV == m_SelectedIndex) )
      sendCameraParams(-1, false);

   if ( (-1 != m_IndexEVValue) && (m_IndexEVValue == m_SelectedIndex) )
      sendCameraParams(-1, false);

   if ( -1 != m_IndexExposureMode )
   if ( m_IndexExposureMode == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( -1 != m_IndexExposureValue )
   if ( m_IndexExposureValue == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexWhiteBalance == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( g_pCurrentModel->isActiveCameraVeye327290() )
   if ( m_IndexAGC == m_SelectedIndex )
   {
      if ( canSendLiveUpdates(m_SelectedIndex) )
         return;
      sendCameraParams(-1, false);
   }
   if ( (-1 != m_IndexAnalogGains) && (m_IndexAnalogGains == m_SelectedIndex) )
   {
      add_menu_to_stack(new MenuVehicleCameraGains());
      return;
   }

   if ( (-1 != m_IndexMetering) && (m_IndexMetering == m_SelectedIndex) )
      sendCameraParams(-1, false);

   if ( (-1 != m_IndexDRC) && (m_IndexDRC == m_SelectedIndex) )
      sendCameraParams(-1, false);

   if ( (-1 != m_IndexISO) && (m_IndexISO == m_SelectedIndex) )
      sendCameraParams(-1, false);

   if ( (-1 != m_IndexISOValue) && (m_IndexISOValue == m_SelectedIndex) )
      sendCameraParams(-1, false);

   if ( (-1 != m_IndexShutterMode) && (m_IndexShutterMode == m_SelectedIndex) )
      sendCameraParams(-1, false);

   if ( (-1 != m_IndexShutterValue) && (m_IndexShutterValue == m_SelectedIndex) )
      sendCameraParams(-1, false);

   if ( (-1 != m_IndexVideoStab) && (m_IndexVideoStab == m_SelectedIndex) )
      sendCameraParams(-1, false);

   if ( m_IndexFlip == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexWDR != -1 && m_IndexWDR == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexIRCut != -1 )
   if ( m_IndexIRCut == m_SelectedIndex )
      sendCameraParams(-1, false);

   if ( m_IndexOpenIPC3A != -1 )
   if ( m_IndexOpenIPC3A == m_SelectedIndex )
      sendCameraParams(-1, false);

   if (m_IndexOpenIPCDayNight != -1)
   if (m_IndexOpenIPCDayNight == m_SelectedIndex)
      sendCameraParams(-1, false);

   if ( (-1 != m_IndexCalibrateHDMI) && (m_IndexCalibrateHDMI == m_SelectedIndex) )
   {
      add_menu_to_stack(new MenuCalibrateHDMI());      
      return;
   }

   if ( (-1 != m_IndexReset) && (m_IndexReset == m_SelectedIndex) )
   {
      g_pCurrentModel->resetCameraToDefaults(g_pCurrentModel->iCurrentCamera);
      handle_commands_send_to_vehicle(COMMAND_ID_SET_CAMERA_PARAMETERS, g_pCurrentModel->iCurrentCamera, (u8*)(&(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera])), sizeof(type_camera_parameters));
   }

   if ( (-1 != m_IndexOpenIPCBinProfile) && (m_IndexOpenIPCBinProfile == m_SelectedIndex) )
   {
      onCalibrationFileSelectionChanged();
   }
}