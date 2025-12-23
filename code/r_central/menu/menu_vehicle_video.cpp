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

#include "../../base/video_capture_res.h"
#include "../../base/utils.h"
#include "../../utils/utils_controller.h"
#include "menu.h"
#include "menu_vehicle_video.h"
#include "menu_vehicle_video_bidir.h"
#include "menu_vehicle_video_profile.h"
#include "menu_vehicle_video_encodings.h"
#include "menu_vehicle_video_compare.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_section.h"

#include "../osd/osd_common.h"
#include "../process_router_messages.h"

MenuVehicleVideo::MenuVehicleVideo(void)
:Menu(MENU_ID_VEHICLE_VIDEO, L("Video Settings"), NULL)
{
   m_Width = 0.42;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.1;
   m_pItemsSelect[0] = NULL;
   m_bShowCompact = false;
   m_bShowCustomFPS = false;
   m_bShowProfileSelectorInline = false;
   m_IndexShowFull = -1;
}

MenuVehicleVideo::~MenuVehicleVideo()
{
}

void MenuVehicleVideo::showCompact()
{
   m_bShowCompact = true;
}

void MenuVehicleVideo::onShow()
{
   int iTmp = getSelectedMenuItemIndex();

   addItems();

   Menu::onShow();

   if ( iTmp >= 0 )
      m_SelectedIndex = iTmp;
   onFocusedItemChanged();
}

void MenuVehicleVideo::addItems()
{
   int iTmp = getSelectedMenuItemIndex();

   float fSliderWidth = 0.12 * Menu::getScaleFactor();
   
   removeAllItems();
   m_pItemsSelect[0] = NULL;

   m_IndexShowFull = -1;
   m_IndexBidirectional = -1;
   m_IndexFPSSelector = -1;
   m_IndexFPS = -1;

   char szBuff[32];

   char szCam[256];
   char szCam2[128];
   str_get_hardware_camera_type_string_to_string(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraType, szCam2);
   strcpy(szCam, L("Video Settings, Active Camera:"));
   strcat(szCam, ": ");
   strcat(szCam, szCam2);
   //addTopLine(szCam);
   setTitle(szCam);
  
   m_pVideoResolutions = getOptionsVideoResolutions(g_pCurrentModel->getActiveCameraType());
   m_iVideoResolutionsCount = getOptionsVideoResolutionsCount(g_pCurrentModel->getActiveCameraType());

   m_pItemsSelect[0] = new MenuItemSelect(L("Resolution"), L("Sets the resolution of the video stream."));
   
   for( int i=0; i<m_iVideoResolutionsCount; i++ )
   {
      sprintf(szBuff, "%s (%d x %d)", m_pVideoResolutions[i].szName, m_pVideoResolutions[i].iWidth, m_pVideoResolutions[i].iHeight);

      if ( (i==0) && ( g_pCurrentModel->isActiveCameraVeye() ) && ( ! g_pCurrentModel->isActiveCameraVeye307() ) )
         m_pItemsSelect[0]->addSelection(szBuff, false);
      else
         m_pItemsSelect[0]->addSelection(szBuff);
   }


   m_pItemsSelect[0]->setIsEditable();
   m_IndexRes = addMenuItem(m_pItemsSelect[0]);      

   bool bFound = false;
   int iMaxFPS = getMaxFPSForCurrentVideoRes(&bFound);

   m_pItemsSelect[3] = new MenuItemSelect(L("FPS"), L("Sets the FPS of the video stream."));
   m_pItemsSelect[3]->addSelection("24");
   m_pItemsSelect[3]->addSelection("30");
   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
   if ( iMaxFPS >= 59 )
      m_pItemsSelect[3]->addSelection("59");
   if ( iMaxFPS >= 60 )
      m_pItemsSelect[3]->addSelection("60");
   if ( iMaxFPS >= 90 )
      m_pItemsSelect[3]->addSelection("90");
   if ( iMaxFPS >= 120 )
      m_pItemsSelect[3]->addSelection("120");
   m_pItemsSelect[3]->addSelection(L("Custom"));
   m_pItemsSelect[3]->setIsEditable();
   m_IndexFPSSelector = addMenuItem(m_pItemsSelect[3]);

   m_IndexFPS = -1;
   if ( m_bShowCustomFPS || (! isStandardFPS(g_pCurrentModel->video_params.iVideoFPS)) )
   {
      m_pItemsSlider[0] = new MenuItemSlider(L("Custom FPS"), L("Sets the FPS of the video stream to any custom supported value."), 2,iMaxFPS, 30, fSliderWidth);
      m_IndexFPS = addMenuItem(m_pItemsSlider[0]);
   }

   m_IndexVideoBitrate = -1;
   m_pMenuItemVideoWarning = NULL;
   /*
   u32 uMaxVideoBitrate = g_pCurrentModel->getMaxVideoBitrateSupportedForCurrentRadioLinks();
   m_pItemsSlider[2] = new MenuItemSlider(L("Video Bitrate (Mbps)"), L("Sets the video bitrate of the video stream generated by the camera."), 2, 4*uMaxVideoBitrate/1000/1000,0, fSliderWidth);
   m_pItemsSlider[2]->setTooltip(L("Sets a target desired bitrate for the video stream."));
   m_pItemsSlider[2]->enableHalfSteps();
   m_pItemsSlider[2]->setSufix("Mbps");
   m_IndexVideoBitrate = addMenuItem(m_pItemsSlider[2]);
   
   m_pMenuItemVideoWarning = new MenuItemText("", true);
   m_pMenuItemVideoWarning->setHidden(true);
   addMenuItem(m_pMenuItemVideoWarning);
   */

   m_pItemsSelect[10] = new MenuItemSelect(L("Video Codec"), L("Change the codec used to encode the source video stream."));
   m_pItemsSelect[10]->addSelection("H264");
   m_pItemsSelect[10]->addSelection("H265");
   m_pItemsSelect[10]->setIsEditable();
   m_IndexVideoCodec = addMenuItem(m_pItemsSelect[10]);

   if ( m_bShowProfileSelectorInline )
   {
      addSeparator();
      int iIndex = addMenuItem(new MenuItemText(L("Video Profile")));
      m_pMenuItems[iIndex]->setExtraHeight(getMenuFontHeight()*0.5);
   }

   m_IndexVideoProfile = -1;
   //if ( ! m_bShowCompact )
   {
      if ( m_bShowProfileSelectorInline )
      {
         char szLegend[256];
         m_pItemsRadio[0] = new MenuItemRadio("", "");
         strcpy(szLegend, "Use this option to automatically adjust video parameters for better video quality;");
         m_pItemsRadio[0]->addSelection(L("High Quality"), szLegend);
         strcpy(szLegend, "Use this option to automatically adjust video parameters for video low latency;");
         m_pItemsRadio[0]->addSelection(L("High Performance"), szLegend);
         strcpy(szLegend, "Use this option to automatically adjust video parameters for better range in detriment of video quality and latency;");
         m_pItemsRadio[0]->addSelection(L("Long Range"), szLegend);
         strcpy(szLegend, "Use this option if you wish to manually modify the advanced video parameters;");
         m_pItemsRadio[0]->addSelection(L("User Defined"), szLegend);
         strcpy(szLegend, "This option is automatically selected if you change video settings to custom ones;");
         m_pItemsRadio[0]->addSelection(L("Custom"), szLegend);
         m_pItemsRadio[0]->setEnabled(true);
         m_pItemsRadio[0]->useSmallLegend(true);
         //m_pItemsRadio[0]->setExtraHeight(getMenuFontHeight()*0.5);
         m_IndexVideoProfile = addMenuItem(m_pItemsRadio[0]);
      }
      else
      {
         m_pItemsSelect[2] = new MenuItemSelect(L("Video Profile"), L("Change all video params to get a particular desired video quality."));  
         m_pItemsSelect[2]->addSelection(L("High Quality"));
         m_pItemsSelect[2]->addSelection(L("High Performance"));
         m_pItemsSelect[2]->addSelection(L("Long Range"));
         m_pItemsSelect[2]->addSelection(L("User Defined"));
         m_pItemsSelect[2]->addSelection(L("Custom"));
         m_pItemsSelect[2]->disableClick();
         m_IndexVideoProfile = addMenuItem(m_pItemsSelect[2]);
      }
   }

   m_IndexSaveVideoProfile = addMenuItem(new MenuItem(L("Save Current Settings As User Profile"), L("Saves current video settings as the User profile.")));
   //m_IndexCompareProfiles = addMenuItem(new MenuItem(L("Compare Video Profiles"), L("See the difference in settings between all video profiles.")));
   //m_pMenuItems[m_IndexCompareProfiles]->showArrow();
   m_IndexCompareProfiles = -1;

   addSeparator();

   m_pItemsSelect[5] = new MenuItemSelect(L("Video Link Mode"), L("Sets the mode of the video link: one way broadcast or bidirectional adaptive video link."));
   m_pItemsSelect[5]->addSelection(L("Fixed One Way Video"));
   m_pItemsSelect[5]->addSelection(L("Bidirectional Video"));
   m_pItemsSelect[5]->setIsEditable();
   m_IndexVideoLinkMode = addMenuItem(m_pItemsSelect[5]);

   if ( ! g_pCurrentModel->isVideoLinkFixedOneWay() )
   {
      m_IndexBidirectional = addMenuItem(new MenuItem(L("Bidirectional Link Settings"), L("Change video bidirectional link settings, like adaptive settings, retransmissions, etc.")));
      m_pMenuItems[m_IndexBidirectional]->showArrow();
      m_pMenuItems[m_IndexBidirectional]->setMargin(Menu::getMenuPaddingX());
   }

   m_IndexExpert = -1;
   if ( ! m_bShowCompact )
   {
      m_IndexExpert = addMenuItem(new MenuItem(L("Advanced Video Settings"), L("Change advanced video parameters for current profile.")));
      m_pMenuItems[m_IndexExpert]->showArrow();
   }   

   if ( m_bShowCompact )
      m_IndexShowFull = addMenuItem(new MenuItem(L("Show all video settings"), L("")));

   valuesToUI();

   if ( iTmp >= 0 )
   {
      m_SelectedIndex = iTmp;
      onFocusedItemChanged();
   }
}

void MenuVehicleVideo::valuesToUI()
{
   char szBuff[128];
   
   checkAddWarningInMenu();

   //u32 uVideoProfileEncodingFlags = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags;

   m_pItemsSelect[10]->setEnabled(true);
   if ( g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265 )
      m_pItemsSelect[10]->setSelectedIndex(1);
   else
      m_pItemsSelect[10]->setSelectedIndex(0);

   bool bFound = false;
   int iMaxFPS = getMaxFPSForCurrentVideoRes(&bFound);

   if ( ! bFound )
   {
      sprintf(szBuff, "Info: You are using a custom resolution (%d x %d) on this %s.", g_pCurrentModel->video_params.iVideoWidth, g_pCurrentModel->video_params.iVideoHeight, g_pCurrentModel->getVehicleTypeString());
      addTopLine(szBuff);
   }
   
   if ( -1 != m_IndexVideoBitrate )
   {
      m_pItemsSlider[2]->setCurrentValue(4*g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS/1000/1000);
      m_pItemsSlider[2]->setEnabled( true );
   }

   if ( -1 != m_IndexFPSSelector )
   {
       int iDelta = 1;
       if ( g_pCurrentModel->isActiveCameraOpenIPC() )
           iDelta = 0;

       if ( m_bShowCustomFPS || (! isStandardFPS(g_pCurrentModel->video_params.iVideoFPS)) )
          m_pItemsSelect[3]->setSelectedIndex(m_pItemsSelect[3]->getSelectionsCount()-1);
       else if ( g_pCurrentModel->video_params.iVideoFPS == 24 )
          m_pItemsSelect[3]->setSelectedIndex(0);
       else if ( g_pCurrentModel->video_params.iVideoFPS == 30 )
          m_pItemsSelect[3]->setSelectedIndex(1);
       else if ( (g_pCurrentModel->video_params.iVideoFPS == 59) && g_pCurrentModel->isActiveCameraOpenIPC() )
          m_pItemsSelect[3]->setSelectedIndex(2);
       else if ( g_pCurrentModel->video_params.iVideoFPS == 60 )
          m_pItemsSelect[3]->setSelectedIndex(3-iDelta);
       else if ( g_pCurrentModel->video_params.iVideoFPS == 90 )
          m_pItemsSelect[3]->setSelectedIndex(4-iDelta);
       else if ( g_pCurrentModel->video_params.iVideoFPS == 120 )
          m_pItemsSelect[3]->setSelectedIndex(5-iDelta);
   }

   if ( -1 != m_IndexFPS )
   {
      m_pItemsSlider[0]->setMaxValue(iMaxFPS);
      m_pItemsSlider[0]->setCurrentValue(g_pCurrentModel->video_params.iVideoFPS);
   }

   if ( -1 != m_IndexVideoProfile )
   {
      if ( m_bShowProfileSelectorInline )
      {
         m_pItemsRadio[0]->setSelectedIndex(g_pCurrentModel->video_params.iCurrentVideoProfile);
         m_pItemsRadio[0]->setFocusedIndex(g_pCurrentModel->video_params.iCurrentVideoProfile);
      }
      else
         m_pItemsSelect[2]->setSelectedIndex(g_pCurrentModel->video_params.iCurrentVideoProfile);
   }
   if ( g_pCurrentModel->video_params.iCurrentVideoProfile == VIDEO_PROFILE_CUST )
      m_pMenuItems[m_IndexSaveVideoProfile]->setEnabled(true);
   else
      m_pMenuItems[m_IndexSaveVideoProfile]->setEnabled(false);
   if ( g_pCurrentModel->isVideoLinkFixedOneWay() )
      m_pItemsSelect[5]->setSelectedIndex(0);
   else
      m_pItemsSelect[5]->setSelectedIndex(1);   
}

void MenuVehicleVideo::Render()
{
   RenderPrepare();
   
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehicleVideo::checkAddWarningInMenu()
{
   /*
   if ( NULL == m_pMenuItemVideoWarning )
      return;
   m_pMenuItemVideoWarning->setHidden(true);

   u32 uMaxVideoBitrate = g_pCurrentModel->getMaxVideoBitrateSupportedForCurrentRadioLinks();
   log_line("MenuVideo: Max usable video bitrate for current vehicle: %u kbps", uMaxVideoBitrate/1000);
   if ( uMaxVideoBitrate >= 50000000 )
      return;

   m_pMenuItemVideoWarning->setTitle("You did set fixed radio data rates for your radio links. Your max usable video bitrate will be limited by that.");
   m_pMenuItemVideoWarning->setHidden(false);
   */
}

int MenuVehicleVideo::getMaxFPSForCurrentVideoRes(bool* pFound)
{
   int iMaxFPS = 90;
   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
      iMaxFPS = 120;
    
   bool bFound = false;
   for(int i=0; i<m_iVideoResolutionsCount; i++ )
   {
      if ( m_pVideoResolutions[i].iWidth == g_pCurrentModel->video_params.iVideoWidth )
      if ( m_pVideoResolutions[i].iHeight == g_pCurrentModel->video_params.iVideoHeight )
      {
         if ( NULL != m_pItemsSelect[0] )
            m_pItemsSelect[0]->setSelection(i);
         iMaxFPS = getOptionsVideoResolutionMaxFPS(g_pCurrentModel->getActiveCameraType(), m_pVideoResolutions[i].iWidth, m_pVideoResolutions[i].iHeight);
         bFound = true;
         break;
      }
   }

   if ( NULL != pFound )
      *pFound = bFound;
   return iMaxFPS;
}

bool MenuVehicleVideo::isStandardFPS(int iFPS)
{
   if ( (iFPS == 24) || (iFPS == 30) || (iFPS == 60) || (iFPS == 90) || (iFPS == 120) )
      return true;
   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
   if ( iFPS == 59 )
      return true;
   return false;
}

void MenuVehicleVideo::showFPSWarning(int w, int h, int fps)
{
   char szBuff[128];
   sprintf(szBuff, L("Max FPS for this video mode (%d x %d) for this camera is %d FPS"), w,h,fps);
   Popup* p = new Popup(true, szBuff, 5 );
   sprintf(szBuff, L("FPS was adjusted to %d FPS to match current video resolution."), fps);
   p->addLine(szBuff);
   p->setIconId(g_idIconWarning, get_Color_IconWarning());
   popups_add_topmost(p);
}

void MenuVehicleVideo::sendVideoSettings()
{
   if ( ! is_sw_version_atleast(g_pCurrentModel, 11, 6) )
   {
      addMessage(L("Video functionality has changed. You need to update your vehicle software."));
      return;
   }
   video_parameters_t paramsNew;
   type_video_link_profile profileNew;
   memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
   memcpy(&profileNew, &(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile]), sizeof(type_video_link_profile));

   if ( m_bShowProfileSelectorInline )
      paramsNew.iCurrentVideoProfile = m_pItemsRadio[0]->getSelectedIndex();

   int videoResolutionIndex = m_pItemsSelect[0]->getSelectedIndex();
 
   paramsNew.iVideoWidth = m_pVideoResolutions[videoResolutionIndex].iWidth;
   paramsNew.iVideoHeight = m_pVideoResolutions[videoResolutionIndex].iHeight;

   bool bHasCustomFPVSelection = false;
   if ( -1 != m_IndexFPSSelector )
   {
       int iIndex = m_pItemsSelect[3]->getSelectedIndex();
       if ( iIndex < m_pItemsSelect[3]->getSelectionsCount() - 1 )
       {
          int iDelta = 1;
          if ( g_pCurrentModel->isActiveCameraOpenIPC() )
              iDelta = 0;
          if ( iIndex == 0 )
             paramsNew.iVideoFPS = 24;
          else if ( iIndex == 1 )
             paramsNew.iVideoFPS = 30;
          else if ( (iIndex == 2) && g_pCurrentModel->isActiveCameraOpenIPC() )
             paramsNew.iVideoFPS = 59;
          else if ( iIndex == 3-iDelta )
             paramsNew.iVideoFPS = 60;
          else if ( iIndex == 4-iDelta )
             paramsNew.iVideoFPS = 90;
          else if ( iIndex == 5-iDelta )
             paramsNew.iVideoFPS = 120;
       }
       else
          bHasCustomFPVSelection = true;
   }
   if ( (-1 != m_IndexFPS) && bHasCustomFPVSelection )
      paramsNew.iVideoFPS = m_pItemsSlider[0]->getCurrentValue();

   if ( paramsNew.iCurrentVideoProfile != g_pCurrentModel->video_params.iCurrentVideoProfile )
   if ( g_pCurrentModel->video_link_profiles[paramsNew.iCurrentVideoProfile].iDefaultFPS > 0 )
      paramsNew.iVideoFPS = g_pCurrentModel->video_link_profiles[paramsNew.iCurrentVideoProfile].iDefaultFPS;

   if ( paramsNew.iVideoFPS > m_pVideoResolutions[videoResolutionIndex].iMaxFPS )
   {
      paramsNew.iVideoFPS = m_pVideoResolutions[videoResolutionIndex].iMaxFPS;
      showFPSWarning(paramsNew.iVideoWidth, paramsNew.iVideoHeight, paramsNew.iVideoFPS);

      if ( paramsNew.iVideoFPS < 30 )
         paramsNew.iVideoFPS = 24;
      else if ( paramsNew.iVideoFPS < 59 )
         paramsNew.iVideoFPS = 30;
      else if ( paramsNew.iVideoFPS < 60 )
         paramsNew.iVideoFPS = 59;
      else if ( paramsNew.iVideoFPS < 90 )
         paramsNew.iVideoFPS = 60;
      else if ( paramsNew.iVideoFPS < 120 )
         paramsNew.iVideoFPS = 90;
      else
         paramsNew.iVideoFPS = 120;
   }

   if ( (paramsNew.iVideoWidth != g_pCurrentModel->video_params.iVideoWidth) ||
        (paramsNew.iVideoHeight != g_pCurrentModel->video_params.iVideoHeight) )
   {
      if ( paramsNew.iVideoWidth > 1500 )
      if ( paramsNew.iVideoFPS > 60 )
         paramsNew.iVideoFPS = 60;
      if ( paramsNew.iVideoWidth > 1980 )
      if ( paramsNew.iVideoFPS > 30 )
         paramsNew.iVideoFPS = 30;
      if ( paramsNew.iVideoWidth >= 2048 )
      if ( paramsNew.iVideoFPS > 20 )
         paramsNew.iVideoFPS = 20;
   }

   #if defined (HW_PLATFORM_RASPBERRY)
   if ( (paramsNew.iVideoWidth > 1980) || (paramsNew.iVideoHeight > 1080) )
   {
      addMessage(L("Can't use 2k,4k resolutions on Raspberry Pi controllers."));
      valuesToUI();
      return;
   }
   #endif

   if ( 0 == m_pItemsSelect[10]->getSelectedIndex() )
      paramsNew.uVideoExtraFlags &= ~VIDEO_FLAG_GENERATE_H265;
   else
   {
      #if defined (HW_PLATFORM_RASPBERRY)
      addMessage(L("Your controller Raspberry Pi hardware supports only H264 video decoder. Can't use H265 codec."));
      valuesToUI();
      return;
      #endif
      if ( ! g_pCurrentModel->isRunningOnOpenIPCHardware() )
      {
         char szTextW[256];
         sprintf(szTextW, "Your %s's Raspberry Pi hardware supports only H264 video encoder/decoder.", g_pCurrentModel->getVehicleTypeString());
         addMessage(szTextW);
         valuesToUI();
         return;
      }         
      paramsNew.uVideoExtraFlags |= VIDEO_FLAG_GENERATE_H265;
   }

   if ( (paramsNew.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265) != (g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265) )
      log_line("MenuVehicleVideo:: Changed video codec type from %s to %s",
         (g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265)?"H265":"H264",
         (paramsNew.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265)?"H265":"H264");
   else
      log_line("MenuVehicleVideo: Video codes is unchanged");

   //profileNew.uTargetVideoBitrateBPS = m_pItemsSlider[2]->getCurrentValue()*1000*1000/4;

   type_video_link_profile profiles[MAX_VIDEO_LINK_PROFILES];
   memcpy((u8*)&profiles[0], (u8*)&g_pCurrentModel->video_link_profiles[0], MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile));
   char szCurrentProfile[64];
   strcpy(szCurrentProfile, str_get_video_profile_name(g_pCurrentModel->video_params.iCurrentVideoProfile));

   if ( paramsNew.iCurrentVideoProfile != g_pCurrentModel->video_params.iCurrentVideoProfile )
   {
      log_line("MenuVehicleVideo: Changed only selected video profile. Do not update video profile settings.");
   }
   else
   {
      int iMatchProfile = g_pCurrentModel->isVideoSettingsMatchingBuiltinVideoProfile(&paramsNew, &profileNew);
      if ( (iMatchProfile >= 0) && (iMatchProfile < MAX_VIDEO_LINK_PROFILES) )
      {
         log_line("MenuVehicleVideo: Will switch to matched to video profile %s, current video profile was: %s", str_get_video_profile_name(iMatchProfile), szCurrentProfile);
         paramsNew.iCurrentVideoProfile = iMatchProfile;
      }
      else
      {
         log_line("MenuVehicleVideo: Will switch to custom profile, current video profile was: %s", szCurrentProfile);
         paramsNew.iCurrentVideoProfile = VIDEO_PROFILE_CUST;
      }
      memcpy((u8*)&profiles[paramsNew.iCurrentVideoProfile], &profileNew, sizeof(type_video_link_profile));
      g_pCurrentModel->logVideoSettingsDifferences(&paramsNew, &profileNew);
   }

   if ( 0 == memcmp(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t)) )
   if ( 0 == memcmp(profiles, g_pCurrentModel->video_link_profiles, MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile)) )
   {
      log_line("MenuVideo: No change in video parameters.");
      return;
   }
   log_line("Sending video encoding flags: %s", str_format_video_encoding_flags(profileNew.uProfileEncodingFlags));

   if ( paramsNew.iVideoFPS != g_pCurrentModel->video_params.iVideoFPS )
      send_pause_adaptive_to_router(4000);
   else
      send_pause_adaptive_to_router(5000);

   if ( paramsNew.iCurrentVideoProfile != g_pCurrentModel->video_params.iCurrentVideoProfile )
      send_reset_adaptive_state_to_router(g_pCurrentModel->uVehicleId);

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMETERS, 0, (u8*)&paramsNew, sizeof(video_parameters_t), (u8*)&profiles[0], MAX_VIDEO_LINK_PROFILES * sizeof(type_video_link_profile)) )
      valuesToUI();
}


void MenuVehicleVideo::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   if ( (10 == iChildMenuId/1000) || (MENU_ID_VEHICLE_VIDEO_PROFILE == iChildMenuId))
   {
      valuesToUI();
      return;
   }
}

void MenuVehicleVideo::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( ! is_sw_version_atleast(g_pCurrentModel, 11, 6) )
   {
      addMessage(L("Video functionality has changed. You need to update your vehicle software."));
      return;
   }


   g_TimeLastVideoCameraChangeCommand = g_TimeNow;

   /*
   if ( m_IndexAutoKeyframe == m_SelectedIndex )
   if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
   {
      addUnsupportedMessageOpenIPC(NULL);
      valuesToUI();
      return;
   }
   */
   
   if ( (-1 != m_IndexShowFull) && (m_IndexShowFull == m_SelectedIndex) )
   {
      m_bShowCompact = false;
      m_SelectedIndex = 0;
      onFocusedItemChanged();
      addItems();
      return;
   }

   if ( (-1 != m_IndexCompareProfiles) && (m_IndexCompareProfiles == m_SelectedIndex) )
   {
      add_menu_to_stack(new MenuVehicleVideoCompare());
      return;
   }

   if ( m_IndexSaveVideoProfile == m_SelectedIndex )
   {
      video_parameters_t paramsNew;
      type_video_link_profile profiles[MAX_VIDEO_LINK_PROFILES];
      memcpy((u8*)&profiles[0], (u8*)&g_pCurrentModel->video_link_profiles[0], MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile));
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      
      memcpy((u8*)&profiles[VIDEO_PROFILE_USER], (u8*)&profiles[paramsNew.iCurrentVideoProfile], sizeof(type_video_link_profile));
      paramsNew.iCurrentVideoProfile = VIDEO_PROFILE_USER;

      send_pause_adaptive_to_router(4000);
      send_reset_adaptive_state_to_router(g_pCurrentModel->uVehicleId);
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMETERS, 0, (u8*)&paramsNew, sizeof(video_parameters_t), (u8*)&(g_pCurrentModel->video_link_profiles[0]), MAX_VIDEO_LINK_PROFILES * sizeof(type_video_link_profile)) )
         valuesToUI();
      return;
   }

   if ( (m_IndexRes == m_SelectedIndex) || ((m_IndexFPS != -1) && (m_IndexFPS == m_SelectedIndex)) || ((m_IndexFPSSelector != -1) && (m_IndexFPSSelector == m_SelectedIndex)) )
   {
      sendVideoSettings();
      if ( (m_IndexFPSSelector != -1) && (m_IndexFPSSelector == m_SelectedIndex) )
      {
         if ( m_pItemsSelect[3]->getSelectedIndex() == m_pItemsSelect[3]->getSelectionsCount() - 1 )
            m_bShowCustomFPS = true;
         else
            m_bShowCustomFPS = false;
         addItems();
      }
      return;
   }

   if ( (-1 != m_IndexVideoBitrate) && (m_IndexVideoBitrate == m_SelectedIndex) )
   {
      sendVideoSettings();
      return;
   }

   if ( (-1 != m_IndexVideoProfile) && (m_IndexVideoProfile == m_SelectedIndex) )
   {
      if ( m_bShowProfileSelectorInline )
      {
         if ( m_pItemsRadio[0]->getSelectedIndex() == g_pCurrentModel->video_params.iCurrentVideoProfile )
            return;
         sendVideoSettings();
      }
      else
         add_menu_to_stack(new MenuVehicleVideoProfileSelector());
      return;
   }

   if ( m_IndexVideoLinkMode == m_SelectedIndex )
   {
      video_parameters_t paramsNew;
      type_video_link_profile profileNew;
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      memcpy(&profileNew, &(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile]), sizeof(type_video_link_profile));

      profileNew.uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ONE_WAY_FIXED_VIDEO;
      if ( 0 == m_pItemsSelect[5]->getSelectedIndex() )
         profileNew.uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ONE_WAY_FIXED_VIDEO;

      type_video_link_profile profiles[MAX_VIDEO_LINK_PROFILES];
      memcpy((u8*)&profiles[0], (u8*)&g_pCurrentModel->video_link_profiles[0], MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile));

      int iMatchProfile = g_pCurrentModel->isVideoSettingsMatchingBuiltinVideoProfile(&paramsNew, &profileNew);
      if ( (iMatchProfile >= 0) && (iMatchProfile < MAX_VIDEO_LINK_PROFILES) )
      {
         log_line("MenuVideo: Matched to video profile %s", str_get_video_profile_name(iMatchProfile));
         paramsNew.iCurrentVideoProfile = iMatchProfile;
      }
      else
      {
         log_line("MenuVideo: Switched to custom profile");
         paramsNew.iCurrentVideoProfile = VIDEO_PROFILE_CUST;
      }
      memcpy((u8*)&profiles[paramsNew.iCurrentVideoProfile ], &profileNew, sizeof(type_video_link_profile));
      g_pCurrentModel->logVideoSettingsDifferences(&paramsNew, &profileNew);

      if ( 0 == memcmp(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t)) )
      if ( 0 == memcmp(profiles, g_pCurrentModel->video_link_profiles, MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile)) )
      {
         log_line("MenuVehicleVideoBidirectional: No change in video parameters.");
         return;
      }

      log_line("Sending video encoding flags: %s", str_format_video_encoding_flags(profileNew.uProfileEncodingFlags));
      send_pause_adaptive_to_router(4000);
      send_reset_adaptive_state_to_router(g_pCurrentModel->uVehicleId);

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMETERS, 0, (u8*)&paramsNew, sizeof(video_parameters_t), (u8*)&profiles[0], MAX_VIDEO_LINK_PROFILES * sizeof(type_video_link_profile)) )
         valuesToUI();
      return;
   }

   if ( (-1 != m_IndexBidirectional) && (m_IndexBidirectional == m_SelectedIndex) )
   {
      add_menu_to_stack(new MenuVehicleVideoBidirectional());
      return;
   }


   if ( (-1 != m_IndexExpert) && (m_IndexExpert == m_SelectedIndex) )
   {
      MenuVehicleVideoEncodings* pMenuEnc = new MenuVehicleVideoEncodings();
      pMenuEnc->m_bShowVideo = true;
      pMenuEnc->m_bShowRetransmissions = true;
      pMenuEnc->m_bShowEC = true;
      pMenuEnc->m_bShowH264 = true;
      add_menu_to_stack(pMenuEnc);
   }

   if ( m_IndexVideoCodec == m_SelectedIndex )
   {
      sendVideoSettings();
      return;
   }
}
