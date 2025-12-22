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
#include "menu_vehicle_video_encodings.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "../../base/utils.h"
#include "../../utils/utils_controller.h"

const char* s_szWarningBitrate = "Warning: The current radio datarate is to small for the current video encoding settings.\n You will experience delays in the video stream.\n Increase the radio datarate, or decrease the video bitrate, decrease the encoding params.";

MenuVehicleVideoEncodings::MenuVehicleVideoEncodings(void)
:Menu(MENU_ID_VEHICLE_EXPERT_ENCODINGS, L("Advanced Video Settings (Expert)"), NULL)
{
   m_Width = 0.37;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.18;
   //m_ShowBitrateWarning = false;
   setSubTitle(L("Change advanced vehicle encoding settings"));

   m_bShowVideo = true;
   m_bShowRetransmissions = true;
   m_bShowEC = true;
   m_bShowH264 = true;
}


void MenuVehicleVideoEncodings::onShow()
{
   if ( (!m_bShowVideo) && m_bShowRetransmissions )
   {
      setTitle(L("Retransmissions Advanced Settings"));
      setSubTitle(L("Change the way retransmissions works."));
   }
   int iTmp = getSelectedMenuItemIndex();
   addItems();
   Menu::onShow();

   if ( iTmp >= 0 )
      m_SelectedIndex = iTmp;
   onFocusedItemChanged();
}

void MenuVehicleVideoEncodings::addItems()
{
   int iTmp = getSelectedMenuItemIndex();

   removeAllItems();
   removeAllTopLines();

   m_IndexMenuEC = -1;
   m_IndexMenuRetransmissions = -1;
   m_IndexMenuH264 = -1;

   float fSliderWidth = 0.12;
   char szBuff[256];

   log_line("MenuVehicleVideoEncodings: Current vehicle radio config:");
   g_pCurrentModel->logVehicleRadioInfo();
   u32 uMaxVideoBitrate = g_pCurrentModel->getMaxVideoBitrateSupportedForCurrentRadioLinks();
   log_line("MenuVehicleVideoEncodings: Max video bitrate usable on current radio links is: %.2f Mbps", uMaxVideoBitrate/1000.0/1000.0);

   strcpy(szBuff, L("Settings for unknown video profile:"));
   if ( g_pCurrentModel->video_params.iCurrentVideoProfile == VIDEO_PROFILE_HIGH_PERF ) 
      strcpy(szBuff, L("Settings for High Performance video profile:"));
   else if ( g_pCurrentModel->video_params.iCurrentVideoProfile == VIDEO_PROFILE_HIGH_QUALITY ) 
      strcpy(szBuff, L("Settings for High Quality video profile:"));
   else if ( g_pCurrentModel->video_params.iCurrentVideoProfile == VIDEO_PROFILE_LONG_RANGE ) 
      strcpy(szBuff, L("Settings for Long Range video profile:"));
   else if ( g_pCurrentModel->video_params.iCurrentVideoProfile == VIDEO_PROFILE_USER ) 
      strcpy(szBuff, L("Settings for User video profile:"));
   else
      sprintf(szBuff, "Settings for %s video profile:", str_get_video_profile_name(g_pCurrentModel->video_params.iCurrentVideoProfile));
   addTopLine(szBuff);

   m_IndexVideoBitrate = -1;
   m_IndexFocusMode = -1;
   m_IndexNoise = -1;

   if ( m_bShowVideo )
   {
      uMaxVideoBitrate = uMaxVideoBitrate - (uMaxVideoBitrate % 250000);
      m_pItemsSlider[2] = new MenuItemSlider(L("Video Bitrate (Mbps)"), L("Sets the video bitrate of the video stream generated by the camera."), 2, 4*uMaxVideoBitrate/1000/1000,0, fSliderWidth);
      m_pItemsSlider[2]->setTooltip(L("Sets a target desired bitrate for the video stream."));
      m_pItemsSlider[2]->enableHalfSteps();
      m_pItemsSlider[2]->setSufix("Mbps");
      m_IndexVideoBitrate = addMenuItem(m_pItemsSlider[2]);

      if ( ! g_pCurrentModel->isVideoLinkFixedOneWay() )
      if ( (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK )
         addMenuItem(new MenuItemText("Note: Actual video bitrate will fluctuate up or down as adaptive video kicks in.", true));
   
      m_pMenuItemVideoWarning = new MenuItemText("", false);
      m_pMenuItemVideoWarning->setHidden(true);
      addMenuItem(m_pMenuItemVideoWarning);

      m_pItemsSelect[21] = new MenuItemSelect(L("Focus Mode Type"), L("Sets the way focus mode is working and how it's highlighted in the video.")); 
      m_pItemsSelect[21]->addSelection(L("Auto"));
      m_pItemsSelect[21]->addSelection(L("Black & White"));
      m_pItemsSelect[21]->addSelection(L("Bars"));
      m_pItemsSelect[21]->addSelection(L("Both"));
      m_pItemsSelect[21]->setIsEditable();
      m_IndexFocusMode = addMenuItem(m_pItemsSelect[21]);

      if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
      {
         m_pItemsSelect[20] = new MenuItemSelect(L("Noise Level Reduction"), L("Sets the video noise level reduction strength. Lower values means better performance but more noise in the live video (less reduction of noise)."));  
         m_pItemsSelect[20]->addSelection(L("Disabled"));
         m_pItemsSelect[20]->addSelection(L("0"));
         m_pItemsSelect[20]->addSelection(L("1"));
         m_pItemsSelect[20]->addSelection(L("2"));
         m_pItemsSelect[20]->setIsEditable();
         m_IndexNoise = addMenuItem(m_pItemsSelect[20]);
      }
   }

   if ( m_bShowVideo && (!m_bShowRetransmissions))
   {
      m_IndexMenuRetransmissions = addMenuItem(new MenuItem(L("Retransmissions Settings"), L("Change the way retransmissions works.")));
      m_pMenuItems[m_IndexMenuRetransmissions]->showArrow();
   }
   else
   {
      addMenuItem(new MenuItemSection(L("Retransmissions")));

      m_pItemsSelect[25] = new MenuItemSelect(L("Retransmissions Algorithm"), L("Change the way retransmissions are requested."));  
      m_pItemsSelect[25]->addSelection(L("Regular"));
      m_pItemsSelect[25]->addSelection(L("Aggressive"));
      m_pItemsSelect[25]->setIsEditable();
      m_pItemsSelect[25]->setExtraHeight(1.0* g_pRenderEngine->textHeight(g_idFontMenu) * MENU_ITEM_SPACING);
      m_IndexRetransmissionsFast = addMenuItem(m_pItemsSelect[25]);

      m_pItemsSlider[8] = new MenuItemSlider(L("Retransmissions Sensitivity (ms)"), L("How sensitive should the retransmissions detection be. Lower values means more sensitive and more proactive retransmissions done."), 0,15,5, fSliderWidth);
      m_IndexRetransmissionsGuardInterval = addMenuItem(m_pItemsSlider[8]);
   }

   addMenuItem(new MenuItemSection(L("Data & Error Correction Settings")));

   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* p = get_Preferences();
   int iMaxSize = p->iDebugMaxPacketSize;

   int iMaxPackets = MAX_TOTAL_PACKETS_IN_BLOCK;
   if ( NULL != g_pCurrentModel )
      iMaxPackets = g_pCurrentModel->hwCapabilities.iMaxTxVideoBlockPackets;

   m_IndexPacketSize = -1;
   if ( (NULL != pCS) && (0 != pCS->iDeveloperMode) )
   if ( (NULL != g_pCurrentModel) && (! g_pCurrentModel->is_spectator) )
   {
      m_pItemsSlider[0] = new MenuItemSlider(L("Packet size"), L("How big is each indivisible video packet over the air link. No major impact in link quality. Smaller packets and more EC data increases the chance of error correction but also increases the video latency."), 100,iMaxSize,iMaxSize/2, fSliderWidth);
      m_pItemsSlider[0]->setStep(10);
      m_IndexPacketSize = addMenuItem(m_pItemsSlider[0]);
      m_pMenuItems[m_IndexPacketSize]->setTextColor(get_Color_Dev());
   }

   m_pItemsSlider[1] = new MenuItemSlider(L("Video block size"), L("How many indivisible packets are in a video block. This has an impact on link recovery and error correction. Bigger values might increase the delay in video stream when link is degraded, but also increase the chance of error correction."), 2, iMaxPackets/2, iMaxPackets/4, fSliderWidth);
   m_IndexBlockPackets = addMenuItem(m_pItemsSlider[1]);

   m_pItemsSelect[18] = new MenuItemSelect(L("Error Correction"), L("How much error correction data to add to the video data."));
   m_pItemsSelect[18]->addSelection(L("None"));
   for( int i=10; i<=MAX_VIDEO_EC_PERCENTAGE; i+=10 )
   {
      char szEC[16];
      sprintf(szEC, "%d%%", i);
      m_pItemsSelect[18]->addSelection(szEC);
   }
   m_pItemsSelect[18]->setIsEditable();
   m_IndexBlockECRate = addMenuItem(m_pItemsSelect[18]);

   m_pItemsSelect[9] = new MenuItemSelect(L("Lower radio rates for EC"), L("Lower the radio link modulations when sending EC packets."));
   m_pItemsSelect[9]->addSelection(L("Off"));
   m_pItemsSelect[9]->addSelection(L("On"));
   m_pItemsSelect[9]->setIsEditable();
   m_IndexLowerDRForEC = addMenuItem(m_pItemsSelect[9]);

   m_IndexECSchemeSpread = -1;
   /*
   m_pItemsSelect[19] = new MenuItemSelect(L("EC Spreading Factor"), L("Spreads the EC packets accross multiple video blocks."));
   m_pItemsSelect[19]->addSelection("0");
   m_pItemsSelect[19]->addSelection("1");
   m_pItemsSelect[19]->addSelection("2");
   m_pItemsSelect[19]->addSelection("3");
   m_pItemsSelect[19]->setIsEditable();
   m_IndexECSchemeSpread = addMenuItem(m_pItemsSelect[19]);
   */

   m_pItemsSelect[16] = new MenuItemSelect(L("Boost data rates"), L("Boosts the radio data rates used for this video profile."));  
   m_pItemsSelect[16]->addSelection(L("Off"));
   m_pItemsSelect[16]->addSelection(L("+1"));
   m_pItemsSelect[16]->addSelection(L("+2"));
   m_pItemsSelect[16]->addSelection(L("+3"));
   m_pItemsSelect[16]->setIsEditable();
   m_IndexHigherRates = addMenuItem(m_pItemsSelect[16]);

   m_pItemsSelect[24] = new MenuItemSelect(L("Max data load on radio link"), L("How much can the radio link be loaded with data."));
   m_pItemsSelect[24]->addSelection(L("Auto (Radio Link configured)"));
   for( int i=10; i<=90; i+= 10 )
   {
      char szTmp[32];
      sprintf(szTmp, "%d%%", i);
      m_pItemsSelect[24]->addSelection(szTmp);
   }
   m_pItemsSelect[24]->setIsEditable();
   m_IndexMaxLinkLoadPercentage = addMenuItem(m_pItemsSelect[24]);

   addMenuItem(new MenuItemSection(L("H264/H265 Encoder Settings")));

   m_IndexH264Profile = -1;
   m_IndexH264Level = -1;
   m_IndexH264Refresh = -1;

   if ( ! g_pCurrentModel->isRunningOnOpenIPCHardware() )
   {
      m_pItemsSelect[4] = new MenuItemSelect("H264/H265 Profile", "The higher the H264/H265 profile, the higher the CPU usage on encode and decode and higher the end to end video latencey. Higher profiles can have lower video quality as more compression algorithms are used.");
      m_pItemsSelect[4]->addSelection("Baseline");
      m_pItemsSelect[4]->addSelection("Main");
      m_pItemsSelect[4]->addSelection("High");
      m_pItemsSelect[4]->addSelection("Extended");
      m_pItemsSelect[4]->setIsEditable();
      m_IndexH264Profile = addMenuItem(m_pItemsSelect[4]);

      m_pItemsSelect[5] = new MenuItemSelect("H264 Level", "");  
      m_pItemsSelect[5]->addSelection("4");
      m_pItemsSelect[5]->addSelection("4.1");
      m_pItemsSelect[5]->addSelection("4.2");
      m_pItemsSelect[5]->setIsEditable();
      m_IndexH264Level = addMenuItem(m_pItemsSelect[5]);

      m_pItemsSelect[6] = new MenuItemSelect("H264/H265 Inter Refresh", "");  
      m_pItemsSelect[6]->addSelection("Cyclic");
      m_pItemsSelect[6]->addSelection("Adaptive");
      m_pItemsSelect[6]->addSelection("Both");
      m_pItemsSelect[6]->addSelection("Cyclic Rows");
      m_pItemsSelect[6]->setIsEditable();
      m_IndexH264Refresh = addMenuItem(m_pItemsSelect[6]);
   }

   m_IndexAutoKeyframe = -1;
   m_IndexKeyframeManual = -1;
   m_pMenuItemVideoKeyframeWarning = NULL;

   m_pItemsSelect[22] = new MenuItemSelect(L("Auto Keyframing"), L("Automatic keyframe adjustment based on radio link conditions."));
   m_pItemsSelect[22]->addSelection(L("No"));
   m_pItemsSelect[22]->addSelection(L("Yes"));
   m_pItemsSelect[22]->setIsEditable();
   m_IndexAutoKeyframe = addMenuItem(m_pItemsSelect[22]);

   m_pItemsSlider[6] = new MenuItemSlider(L("Max Auto Keyframe (ms)"), 50,20000,0, fSliderWidth);
   m_pItemsSlider[6]->setTooltip(L("Sets the max allowed keyframe interval (in miliseconds) when auto adjusting is enabled for video keyframe interval parameter."));
   m_pItemsSlider[6]->setStep(10);
   m_IndexMaxKeyFrame = addMenuItem(m_pItemsSlider[6]);

   m_pItemsSlider[5] = new MenuItemSlider(L("Keyframe interval (ms)"), L("A keyframe is added every [n] miliseconds. Bigger keyframe values usually results in better quality video and lower bandwidth requirements but longer breakups if any."), 50,20000,200, fSliderWidth);
   m_pItemsSlider[5]->setStep(10);
   m_pItemsSlider[5]->setSufix("ms");
   m_IndexKeyframeManual = addMenuItem(m_pItemsSlider[5]);
   m_pMenuItemVideoKeyframeWarning = new MenuItemText("", true);
   m_pMenuItemVideoKeyframeWarning->setHidden(true);
   addMenuItem(m_pMenuItemVideoKeyframeWarning);

   m_pItemsSelect[12] = new MenuItemSelect("H264/H265 Slices", "Split video frames into multiple smaller parts. When having heavy radio interference there is a chance that not the entire video is corrupted if video is sliced up. But is uses more processing power.");
   for( int i=1; i<=16; i++ )
   {
      sprintf(szBuff, "%d", i);
      m_pItemsSelect[12]->addSelection(szBuff);
   }
   m_pItemsSelect[12]->setIsEditable();
   m_IndexH264Slices = addMenuItem(m_pItemsSelect[12]);

   m_pItemsSelect[17] = new MenuItemSelect("Remove extra H264/H265 frames", "Removes frames not needed for video decoding.");  
   m_pItemsSelect[17]->addSelection("No");
   m_pItemsSelect[17]->addSelection("Yes");
   m_pItemsSelect[17]->setIsEditable();
   m_IndexRemoveH264PPS = addMenuItem(m_pItemsSelect[17]);

   m_IndexInsertH264PPS = -1;
   m_IndexInsertH264SPSTimings = -1;
   if ( ! g_pCurrentModel->isRunningOnOpenIPCHardware() )
   {
      m_pItemsSelect[7] = new MenuItemSelect("Insert H264/H265 PPS Headers", "");  
      m_pItemsSelect[7]->addSelection("No");
      m_pItemsSelect[7]->addSelection("Yes");
      m_pItemsSelect[7]->setIsEditable();
      m_IndexInsertH264PPS = addMenuItem(m_pItemsSelect[7]);

      m_pItemsSelect[11] = new MenuItemSelect("Fill H264/H265 SPS Timings", "");  
      m_pItemsSelect[11]->addSelection("No");
      m_pItemsSelect[11]->addSelection("Yes");
      m_pItemsSelect[11]->setIsEditable();
      m_IndexInsertH264SPSTimings = addMenuItem(m_pItemsSelect[11]);
   }

   m_IndexIPQuantizationDelta = -1;
   m_IndexCustomQuant = -1;
   m_IndexQuantValue = -1;
   m_IndexLowerQPDeltaOnLink = -1;
   if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
   {
      m_pItemsSlider[18] = new MenuItemSlider("I-P Frames Quantization Delta", -12,12,-2,fSliderWidth);
      m_pItemsSlider[18]->setTooltip("Sets a relative quantization difference between P and I frames. Higher values increase the quality of I frames compared to P frames.");
      m_IndexIPQuantizationDelta = addMenuItem(m_pItemsSlider[18]);

      m_pItemsSelect[23] = new MenuItemSelect(L("Lower Quantization Delta"), L("Lower the encoder quantization I-P delta when radio link quality goes down."));
      m_pItemsSelect[23]->addSelection("Off");
      m_pItemsSelect[23]->addSelection("Medium");
      m_pItemsSelect[23]->addSelection("High");
      m_pItemsSelect[23]->setIsEditable();
      m_IndexLowerQPDeltaOnLink = addMenuItem(m_pItemsSelect[23]);
   }
   else
   {
      m_pItemsSelect[8] = new MenuItemSelect("Auto H264/H265 quantization", "Use default quantization for the H264/H265 video encoding, or set a custom value.");  
      m_pItemsSelect[8]->addSelection("No");
      m_pItemsSelect[8]->addSelection("Yes");
      m_pItemsSelect[8]->setIsEditable();
      m_IndexCustomQuant = addMenuItem(m_pItemsSelect[8]);

      m_pItemsSlider[4] = new MenuItemSlider("Manual video quantization", 0,40,20,fSliderWidth);
      m_pItemsSlider[4]->setTooltip("Sets a fixed H264 quantization parameter for the video stream. Higher values reduces quality but decreases bitrate requirements and latency. The default is about 16");
      m_IndexQuantValue = addMenuItem(m_pItemsSlider[4]);
   }

   m_pItemsSelect[14] = new MenuItemSelect("Enable adaptive H264/H265 quantization", "Enable algorithm that auto adjusts the H264/H265 quantization to match the desired video bitrate in realtime.");  
   m_pItemsSelect[14]->addSelection("No");
   m_pItemsSelect[14]->addSelection("Yes");
   m_pItemsSelect[14]->setIsEditable();
   m_IndexEnableAdaptiveQuantization = addMenuItem(m_pItemsSelect[14]);

   m_pItemsSelect[15] = new MenuItemSelect("Adaptive H264/H265 quantization strength", "How strongh should the algorithm be. The algorithm that auto adjusts the H264/H265 quantization to match the desired video bitrate in realtime.");  
   m_pItemsSelect[15]->addSelection("Low");
   m_pItemsSelect[15]->addSelection("High");
   m_pItemsSelect[15]->setIsEditable();
   m_IndexAdaptiveH264QuantizationStrength = addMenuItem(m_pItemsSelect[15]);

   addSeparator();

   m_IndexHDMIOutput = -1;
   if ( ! g_pCurrentModel->isRunningOnOpenIPCHardware() )
   {
      m_pItemsSelect[1] = new MenuItemSelect(L("Enable Vehicle Local HDMI Output"), L("Enables or disables video output the the HDMI port on the vehicle."));  
      m_pItemsSelect[1]->addSelection(L("Off"));
      m_pItemsSelect[1]->addSelection(L("On"));
      m_pItemsSelect[1]->setIsEditable();
      m_IndexHDMIOutput = addMenuItem(m_pItemsSelect[1]);
   }

   valuesToUI();

   if ( iTmp >= 0 )
   {
      m_SelectedIndex = iTmp;
      onFocusedItemChanged();
   }
}


void MenuVehicleVideoEncodings::valuesToUI()
{
   checkAddWarningInMenu();

   int iVideoProfile = g_pCurrentModel->video_params.iCurrentVideoProfile;
   u32 uVideoProfileFlags = g_pCurrentModel->video_link_profiles[iVideoProfile].uProfileFlags;
   u32 uVideoProfileEncodingFlags = g_pCurrentModel->video_link_profiles[iVideoProfile].uProfileEncodingFlags;
   int adaptiveKeyframe = (uVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME)?1:0;
   int iFixedVideoLink = 0;
   if ( g_pCurrentModel->isVideoLinkFixedOneWay() )
      iFixedVideoLink = 1;

   if ( -1 != m_IndexVideoBitrate )
   {
      m_pItemsSlider[2]->setCurrentValue((4*g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS)/1000/1000);
      m_pItemsSlider[2]->setEnabled( true );
      log_line("MenuVehicleVideoEncodings: Set UI slider video bitrate to %u bps (%d) for current video profile %s", g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uTargetVideoBitrateBPS, m_pItemsSlider[2]->getCurrentValue(), str_get_video_profile_name(g_pCurrentModel->video_params.iCurrentVideoProfile));
   }

   if (adaptiveKeyframe && (! g_pCurrentModel->isVideoLinkFixedOneWay()) )
   {
      m_pItemsSelect[22]->setSelectedIndex(1);
      m_pItemsSlider[6]->setEnabled(true);
      m_pItemsSlider[5]->setEnabled(false);
   }
   else
   {
      m_pItemsSelect[22]->setSelectedIndex(0);
      m_pItemsSlider[6]->setEnabled(false);
      m_pItemsSlider[5]->setEnabled(true);
   }
   m_pItemsSlider[6]->setCurrentValue(g_pCurrentModel->video_params.uMaxAutoKeyframeIntervalMs);


   if ( g_pCurrentModel->video_link_profiles[iVideoProfile].iDefaultLinkLoad <= 0 )
      m_pItemsSelect[24]->setSelectedIndex(0);
   else
      m_pItemsSelect[24]->setSelectedIndex(g_pCurrentModel->video_link_profiles[iVideoProfile].iDefaultLinkLoad/10);

   if ( iFixedVideoLink )
   {
      if ( -1 != m_IndexRetransmissionsFast )
         m_pItemsSelect[25]->setEnabled(false);
      if ( -1 != m_IndexRetransmissionsGuardInterval )
         m_pItemsSlider[8]->setEnabled(false);
   }
   else if ( uVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS )
   {
      if ( -1 != m_IndexRetransmissionsFast )
         m_pItemsSelect[25]->setEnabled(true);
      if ( -1 != m_IndexRetransmissionsGuardInterval )
         m_pItemsSlider[8]->setEnabled(true);
   }
   else
   {
      if ( -1 != m_IndexRetransmissionsFast )
         m_pItemsSelect[25]->setEnabled(false);
      if ( -1 != m_IndexRetransmissionsGuardInterval )
         m_pItemsSlider[8]->setEnabled(false);
   }
   
   if ( -1 != m_IndexRetransmissionsFast )
      m_pItemsSelect[25]->setSelectedIndex((uVideoProfileFlags & VIDEO_PROFILE_FLAG_RETRANSMISSIONS_AGGRESIVE)?1:0);
   if ( -1 != m_IndexRetransmissionsGuardInterval )
      m_pItemsSlider[8]->setCurrentValue((uVideoProfileFlags & VIDEO_PROFILE_FLAG_MASK_RETRANSMISSIONS_GUARD_MASK) >> 8);

   m_pItemsSelect[9]->setSelectedIndex( (uVideoProfileFlags & VIDEO_PROFILE_FLAG_USE_LOWER_DR_FOR_EC_PACKETS)?1:0);
   m_pItemsSelect[16]->setSelectedIndex(0);
   if ( uVideoProfileFlags & VIDEO_PROFILE_FLAG_USE_HIGHER_DATARATE )
      m_pItemsSelect[16]->setSelectedIndex( (uVideoProfileFlags & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK) >> VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT);

   if ( NULL != m_pMenuItemVideoKeyframeWarning )
   {
      m_pMenuItemVideoKeyframeWarning->setHidden(true);
      if ( ! g_pCurrentModel->isVideoLinkFixedOneWay() )
      if ( g_pCurrentModel->video_link_profiles[iVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME )
      {
         m_pMenuItemVideoKeyframeWarning->setTitle(L("Adaptive keyframe is turned on. Keyframe interval will be dynamically adjusted by Ruby. If you want to use a fixed keyframe, turn off Adaptive Keyframe from Bidirectional settings menu or set the video link as One Way."));
         m_pMenuItemVideoKeyframeWarning->setHidden(false);
      }
   }
   if ( -1 != m_IndexKeyframeManual )
   {
      int iKeyframeMS = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iKeyframeMS;
      if ( iKeyframeMS < 0 )
         iKeyframeMS = -iKeyframeMS;
      m_pItemsSlider[5]->setCurrentValue(iKeyframeMS);
   }

   if ( m_IndexPacketSize != -1 )
      m_pItemsSlider[0]->setCurrentValue(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].video_data_length);

   m_pItemsSlider[1]->setCurrentValue(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iBlockDataPackets);
   m_pItemsSlider[1]->setEnabled(true);

   int iECPercent = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iECPercentage;
   iECPercent = iECPercent/10;
   if ( iECPercent > MAX_VIDEO_EC_PERCENTAGE/10 )
      iECPercent = MAX_VIDEO_EC_PERCENTAGE/10;
   m_pItemsSelect[18]->setSelectedIndex(iECPercent);

   u32 uECSpreadHigh = (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_HIGHBIT)?1:0;
   u32 uECSpreadLow = (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_LOWBIT)?1:0;
   u32 uECSpread = uECSpreadLow + (uECSpreadHigh*2);

   if ( -1 != m_IndexECSchemeSpread )
      m_pItemsSelect[19]->setSelectedIndex((int) uECSpread);

   if ( -1 != m_IndexNoise )
   {
      int iNoise = uVideoProfileFlags & VIDEO_PROFILE_FLAGS_MASK_NOISE;
      if ( iNoise == 3 )
         m_pItemsSelect[20]->setSelectedIndex(0);
      else
         m_pItemsSelect[20]->setSelectedIndex(iNoise+1);
   }

   if ( -1 != m_IndexFocusMode )
   {
      if ( (g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_ENABLE_FOCUS_MODE_BW) && (g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_ENABLE_FOCUS_MODE_BARS) )
         m_pItemsSelect[21]->setSelectedIndex(3);
      else if ( g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_ENABLE_FOCUS_MODE_BARS )
         m_pItemsSelect[21]->setSelectedIndex(2);
      else if ( g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_ENABLE_FOCUS_MODE_BW )
         m_pItemsSelect[21]->setSelectedIndex(1);
      else
         m_pItemsSelect[21]->setSelectedIndex(0);
   }

   if ( -1 != m_IndexH264Profile )
      m_pItemsSelect[4]->setSelectedIndex((g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_ENABLE_LOCAL_HDMI_OUTPUT)?1:0);

   if ( -1 != m_IndexH264Profile )
      m_pItemsSelect[4]->setSelection(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].h264profile);
   if ( -1 != m_IndexH264Level )
      m_pItemsSelect[5]->setSelection(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].h264level);
   if ( -1 != m_IndexH264Refresh )
      m_pItemsSelect[6]->setSelection(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].h264refresh);
   if ( -1 != m_IndexInsertH264PPS )
      m_pItemsSelect[7]->setSelection(g_pCurrentModel->video_params.iInsertPPSVideoFrames);
   
   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH_HIGH )
      m_pItemsSelect[15]->setSelectedIndex(1);
   else
      m_pItemsSelect[15]->setSelectedIndex(0);

   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION )
   {
      m_pItemsSelect[14]->setSelectedIndex(1);
      m_pItemsSelect[15]->setEnabled(true);
   }
   else
   {
      m_pItemsSelect[14]->setSelectedIndex(0);
      m_pItemsSelect[15]->setEnabled(false);    
   }

   if ( -1 != m_IndexIPQuantizationDelta )
      m_pItemsSlider[18]->setCurrentValue(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iIPQuantizationDelta);

   if ( -1 != m_IndexLowerQPDeltaOnLink )
   {
       m_pItemsSelect[23]->setSelectedIndex(0);
       if ( uVideoProfileFlags & VIDEO_PROFILE_FLAG_LOWER_QP_DELTA_ON_LOW_LINK )
       {
          m_pItemsSelect[23]->setSelectedIndex(1);
          if ( uVideoProfileFlags & VIDEO_PROFILE_FLAG_LOWER_QP_DELTA_ON_LOW_LINK_HIGH )
             m_pItemsSelect[23]->setSelectedIndex(2);
       }
   }

   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].h264quantization > 0 )
   {
      if ( -1 != m_IndexCustomQuant )
         m_pItemsSelect[8]->setSelection(0);
      if ( -1 != m_IndexQuantValue )
      {
         m_pItemsSlider[4]->setCurrentValue(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].h264quantization);
         m_pItemsSlider[4]->setEnabled(true);
      }
      m_pItemsSelect[14]->setEnabled(false);
      m_pItemsSelect[15]->setEnabled(false);
   }
   else
   {
      if ( -1 != m_IndexCustomQuant )
         m_pItemsSelect[8]->setSelection(1);
      if ( -1 != m_IndexQuantValue )
      {
         m_pItemsSlider[4]->setCurrentValue(-g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].h264quantization);
         m_pItemsSlider[4]->setEnabled(false);
      }
      m_pItemsSelect[14]->setEnabled(true);
      //m_pItemsSelect[15]->setEnabled(true);
   }

   if ( -1 != m_IndexInsertH264SPSTimings )
      m_pItemsSelect[11]->setSelectedIndex(g_pCurrentModel->video_params.iInsertSPTVideoFramesTimings);
   if ( -1 != m_IndexRemoveH264PPS )
      m_pItemsSelect[17]->setSelectedIndex(g_pCurrentModel->video_params.iRemovePPSVideoFrames);
   
   m_pItemsSelect[12]->setSelectedIndex(g_pCurrentModel->video_params.iH264Slices-1);
   //if ( hardware_board_is_openipc(g_pCurrentModel->hwCapabilities.uBoardType) )
   //{
   //   m_pItemsSelect[12]->setSelectedIndex(0);
   //   m_pItemsSelect[12]->setEnabled(false);
   //}
}

void MenuVehicleVideoEncodings::checkAddWarningInMenu()
{
   if ( NULL == m_pMenuItemVideoWarning )
      return;
   m_pMenuItemVideoWarning->setHidden(true);

   if ( ! m_bShowVideo )
      return;
   if ( g_pCurrentModel->isAllVideoLinksFixedRate() )
   {
      log_line("MenuVehicleVideoEncodings: All radio links (%d links) have fixed rate.", g_pCurrentModel->radioLinksParams.links_count);
      for( int iLink=0; iLink<g_pCurrentModel->radioLinksParams.links_count; iLink++ )
      {
         log_line("MenuVehicleVideoEncodings: Is radio link %d adaptive usable? %s, set to datarate video: %d", iLink+1, g_pCurrentModel->isRadioLinkAdaptiveUsable(iLink)?"yes":"no", g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[iLink]);
      }
      g_pCurrentModel->logVehicleRadioInfo();

      m_pMenuItemVideoWarning->setTitle(L("Warning: You did set your radio links to fixed radio modulations. Your maximum settable video bitrate will be limited by that. Switch the radio links to Auto modulations if you want to set higher video bitrates."));
      m_pMenuItemVideoWarning->setHidden(false);
      m_pMenuItemVideoWarning->highlightFirstWord(true);
      if ( -1 != m_IndexVideoBitrate )
         m_pItemsSlider[2]->setExtraHeight(0.5*getMenuFontHeight());
   }
   else if ( -1 != m_IndexVideoBitrate )
      m_pItemsSlider[2]->setExtraHeight(0);
}


void MenuVehicleVideoEncodings::Render()
{
   RenderPrepare();
   float y0 = RenderFrameAndTitle();
   float y = y0;

   for( int i=0; i<m_ItemsCount; i++ )
   {
      y += RenderItem(i,y);
   }
   RenderEnd(y0);
}

bool MenuVehicleVideoEncodings::sendVideoParams()
{
   video_parameters_t paramsNew;
   type_video_link_profile profileNew;
   memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
   memcpy(&profileNew, &(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile]), sizeof(type_video_link_profile));
   
   log_line("MenuVehicleVideoEncodings: Initial video profile encoding flags: %s", str_format_video_profile_flags(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileFlags));
   log_line("MenuVideoEncodings: Initial profile and video differences:");
   g_pCurrentModel->logVideoSettingsDifferences(&paramsNew, &profileNew);

   if ( -1 != m_IndexInsertH264PPS )
      paramsNew.iInsertPPSVideoFrames = m_pItemsSelect[7]->getSelectedIndex();
   if ( -1 != m_IndexInsertH264SPSTimings )
      paramsNew.iInsertSPTVideoFramesTimings = m_pItemsSelect[11]->getSelectedIndex();
   if ( -1 != m_IndexRemoveH264PPS )
      paramsNew.iRemovePPSVideoFrames = m_pItemsSelect[17]->getSelectedIndex();
  
   if ( (-1 != m_IndexCustomQuant) && (-1 != m_IndexQuantValue) )
   {
      if ( 0 == m_pItemsSelect[8]->getSelectedIndex() )
      {
         profileNew.h264quantization = m_pItemsSlider[4]->getCurrentValue();
         if ( profileNew.h264quantization < 5 )
            profileNew.h264quantization = 5;
      }
      else
      {
         if ( profileNew.h264quantization > 0 )
            profileNew.h264quantization = - profileNew.h264quantization;
      }
   }

   if ( -1 != m_IndexIPQuantizationDelta )
      profileNew.iIPQuantizationDelta = m_pItemsSlider[18]->getCurrentValue();

   if ( -1 != m_IndexLowerQPDeltaOnLink )
   {
      profileNew.uProfileFlags &= ~( VIDEO_PROFILE_FLAG_LOWER_QP_DELTA_ON_LOW_LINK | VIDEO_PROFILE_FLAG_LOWER_QP_DELTA_ON_LOW_LINK_HIGH );
      if ( 1 == m_pItemsSelect[23]->getSelectedIndex() )
         profileNew.uProfileFlags |= VIDEO_PROFILE_FLAG_LOWER_QP_DELTA_ON_LOW_LINK;
      if ( 2 == m_pItemsSelect[23]->getSelectedIndex() )
         profileNew.uProfileFlags |= VIDEO_PROFILE_FLAG_LOWER_QP_DELTA_ON_LOW_LINK | VIDEO_PROFILE_FLAG_LOWER_QP_DELTA_ON_LOW_LINK_HIGH;
   }

   if ( 0 == m_pItemsSelect[24]->getSelectedIndex() )
      profileNew.iDefaultLinkLoad = 0;
   else
      profileNew.iDefaultLinkLoad = m_pItemsSelect[24]->getSelectedIndex() * 10;

   if ( -1 != m_IndexNoise )
   {
      int iNoise = m_pItemsSelect[20]->getSelectedIndex();
      profileNew.uProfileFlags &= ~VIDEO_PROFILE_FLAGS_MASK_NOISE;
      if ( iNoise == 0 )
         profileNew.uProfileFlags |= ((u32)3) & VIDEO_PROFILE_FLAGS_MASK_NOISE;
      else
         profileNew.uProfileFlags |= ((u32)(iNoise-1)) & VIDEO_PROFILE_FLAGS_MASK_NOISE;
   }


   if ( -1 != m_IndexRetransmissionsFast )
   {
      profileNew.uProfileFlags &= ~VIDEO_PROFILE_FLAG_RETRANSMISSIONS_AGGRESIVE;
      if ( 1 == m_pItemsSelect[25]->getSelectedIndex() )
         profileNew.uProfileFlags |= VIDEO_PROFILE_FLAG_RETRANSMISSIONS_AGGRESIVE;
   }

   if ( -1 != m_IndexRetransmissionsGuardInterval )
   {
      profileNew.uProfileFlags &= ~VIDEO_PROFILE_FLAG_MASK_RETRANSMISSIONS_GUARD_MASK;
      profileNew.uProfileFlags |= (((u32)(m_pItemsSlider[8]->getCurrentValue())) << 8);
   }

   if ( 1 == m_pItemsSelect[9]->getSelectedIndex() )
      profileNew.uProfileFlags |= VIDEO_PROFILE_FLAG_USE_LOWER_DR_FOR_EC_PACKETS;
   else
      profileNew.uProfileFlags &= ~VIDEO_PROFILE_FLAG_USE_LOWER_DR_FOR_EC_PACKETS;

   if ( -1 != m_IndexFocusMode )
   {
      paramsNew.uVideoExtraFlags &= ~(VIDEO_FLAG_ENABLE_FOCUS_MODE_BW | VIDEO_FLAG_ENABLE_FOCUS_MODE_BARS);
      if ( m_pItemsSelect[21]->getSelectedIndex() == 1 )
         paramsNew.uVideoExtraFlags |= VIDEO_FLAG_ENABLE_FOCUS_MODE_BW;
      if ( m_pItemsSelect[21]->getSelectedIndex() == 2 )
         paramsNew.uVideoExtraFlags |= VIDEO_FLAG_ENABLE_FOCUS_MODE_BARS;
      if ( m_pItemsSelect[21]->getSelectedIndex() == 3 )
         paramsNew.uVideoExtraFlags |= VIDEO_FLAG_ENABLE_FOCUS_MODE_BW | VIDEO_FLAG_ENABLE_FOCUS_MODE_BARS;
   }

   if ( -1 != m_IndexVideoBitrate )
   {
      profileNew.uTargetVideoBitrateBPS = m_pItemsSlider[2]->getCurrentValue()*1000*1000/4;
      log_line("MenuVehicleVideoEncodings: Set update profile video bitrate to %u bps (%d) for new video profile %s", profileNew.uTargetVideoBitrateBPS, m_pItemsSlider[2]->getCurrentValue(), str_get_video_profile_name(paramsNew.iCurrentVideoProfile));
   }
   paramsNew.iH264Slices = 1 + m_pItemsSelect[12]->getSelectedIndex();
   paramsNew.uMaxAutoKeyframeIntervalMs = m_pItemsSlider[6]->getCurrentValue();

   if ( -1 != m_IndexKeyframeManual )
      profileNew.iKeyframeMS = m_pItemsSlider[5]->getCurrentValue();

   if ( 0 == m_pItemsSelect[22]->getSelectedIndex() )
   {
      profileNew.uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME;
      if ( profileNew.iKeyframeMS < 0 )
         profileNew.iKeyframeMS = - profileNew.iKeyframeMS;
   }
   else
   {
      profileNew.uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME;
      profileNew.iKeyframeMS = DEFAULT_VIDEO_KEYFRAME_AUTO;
      if ( paramsNew.iCurrentVideoProfile == VIDEO_PROFILE_HIGH_PERF )
         profileNew.iKeyframeMS = DEFAULT_VIDEO_KEYFRAME_AUTO_HP;
      if ( paramsNew.iCurrentVideoProfile == VIDEO_PROFILE_LONG_RANGE )
         profileNew.iKeyframeMS = DEFAULT_VIDEO_KEYFRAME_AUTO_LR;

      if ( profileNew.iKeyframeMS > 0 )
         profileNew.iKeyframeMS = - profileNew.iKeyframeMS;
   }

   if ( -1 != m_IndexPacketSize )
      profileNew.video_data_length = m_pItemsSlider[0]->getCurrentValue();
   
   profileNew.iBlockDataPackets = m_pItemsSlider[1]->getCurrentValue();
   profileNew.iECPercentage = m_pItemsSelect[18]->getSelectedIndex() * 10;
   g_pCurrentModel->convertECPercentageToData(&profileNew);

   int iCurrentDRBoost = (profileNew.uProfileFlags & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK) >> VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT;
   profileNew.uProfileFlags &= ~ VIDEO_PROFILE_FLAG_USE_HIGHER_DATARATE;
   profileNew.uProfileFlags &= ~ VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK;
   if ( 0 != m_pItemsSelect[16]->getSelectedIndex() )
   {
      u32 uValue = m_pItemsSelect[16]->getSelectedIndex();
      uValue = (uValue << VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK_SHIFT) & VIDEO_PROFILE_FLAGS_HIGHER_DATARATE_MASK;
      profileNew.uProfileFlags |= VIDEO_PROFILE_FLAG_USE_HIGHER_DATARATE;
      profileNew.uProfileFlags |= uValue;

      if ( iCurrentDRBoost != (int)uValue )
         log_line("MenuVehicleVideoEncodings: Video DR boots update from %d to %d", iCurrentDRBoost, (int)uValue);
   }

   if ( -1 != m_IndexECSchemeSpread )
   {
      u32 uECSpread = (u32) m_pItemsSelect[19]->getSelectedIndex();

      profileNew.uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_HIGHBIT | VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_LOWBIT);
      if ( uECSpread & 0x01 )
         profileNew.uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_LOWBIT;
      if ( uECSpread & 0x02 )
         profileNew.uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_HIGHBIT;
   }

   profileNew.uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION | VIDEO_PROFILE_ENCODING_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH_HIGH);
   if ( 1 == m_pItemsSelect[14]->getSelectedIndex() )
      profileNew.uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION;
   if ( 1 == m_pItemsSelect[15]->getSelectedIndex() )
      profileNew.uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH_HIGH;

   if ( -1 != m_IndexH264Profile )
      profileNew.h264profile = m_pItemsSelect[4]->getSelectedIndex();
   if ( -1 != m_IndexH264Level )
      profileNew.h264level = m_pItemsSelect[5]->getSelectedIndex();
   if ( -1 != m_IndexH264Refresh )
      profileNew.h264refresh = m_pItemsSelect[6]->getSelectedIndex();

   type_video_link_profile profiles[MAX_VIDEO_LINK_PROFILES];
   memcpy((u8*)&profiles[0], (u8*)&g_pCurrentModel->video_link_profiles[0], MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile));
   char szCurrentProfile[64];
   strcpy(szCurrentProfile, str_get_video_profile_name(g_pCurrentModel->video_params.iCurrentVideoProfile));

   if ( g_pCurrentModel->radioRuntimeCapabilities.uFlagsRuntimeCapab & MODEL_RUNTIME_RADIO_CAPAB_FLAG_COMPUTED ) 
      log_line("MenuVehicleVideoEncodings: Currently computed max supported datarates: MCS: %d, legacy: %d", g_pCurrentModel->radioRuntimeCapabilities.iMaxSupportedMCSDataRate, g_pCurrentModel->radioRuntimeCapabilities.iMaxSupportedLegacyDataRate);
   else
      log_line("MenuVehicleVideoEncodings: Currently maxc supported datarates are not computed.");
   log_line("MenuVehicleVideoEncodings: Current DR boost: %d, new DR boost: %d", iCurrentDRBoost, m_pItemsSelect[16]->getSelectedIndex());

   if ( iCurrentDRBoost != m_pItemsSelect[16]->getSelectedIndex() )
   {
      u32 uMaxVideoBitrate = g_pCurrentModel->getMaxVideoBitrateSupportedForRadioLinks(&g_pCurrentModel->radioLinksParams, &paramsNew, &(profiles[0]));
      if ( profileNew.uTargetVideoBitrateBPS > uMaxVideoBitrate )
      {
         log_line("MenuVehicleVideoEncodings: Will decrease video bitrate (%u kbps) for video profile %s to max allowed on current links: %u kbps",
            profileNew.uTargetVideoBitrateBPS/1000,
            str_get_video_profile_name(paramsNew.iCurrentVideoProfile),
            uMaxVideoBitrate/1000);
         profileNew.uTargetVideoBitrateBPS = uMaxVideoBitrate;
      }
   }

   log_line("MenuVehicleVideoEncodings: New video profile encoding flags: %s", str_format_video_profile_flags(profileNew.uProfileFlags));
   log_line("MenuVideoEncodings: Profile and video differences before searchig for a match:");
   g_pCurrentModel->logVideoSettingsDifferences(&paramsNew, &profileNew);

   int iMatchProfile = g_pCurrentModel->isVideoSettingsMatchingBuiltinVideoProfile(&paramsNew, &profileNew);
   if ( (iMatchProfile >= 0) && (iMatchProfile < MAX_VIDEO_LINK_PROFILES) )
   {
      log_line("MenuVehicleVideoEncodings: Will switch to matched to video profile %s, current video profile was: %s", str_get_video_profile_name(iMatchProfile), szCurrentProfile);
      paramsNew.iCurrentVideoProfile = iMatchProfile;
   }
   else
   {
      log_line("MenuVehicleVideoEncodings: Will switch to custom profile, current video profile was: %s", szCurrentProfile);
      paramsNew.iCurrentVideoProfile = VIDEO_PROFILE_CUST;
   }

   memcpy((u8*)&profiles[paramsNew.iCurrentVideoProfile ], &profileNew, sizeof(type_video_link_profile));
   g_pCurrentModel->logVideoSettingsDifferences(&paramsNew, &profileNew);

   if ( 0 == memcmp(&paramsNew, (u8*)&(g_pCurrentModel->video_params), sizeof(video_parameters_t)) )
   if ( 0 == memcmp(profiles, (u8*)&(g_pCurrentModel->video_link_profiles[0]), MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile)) )
   {
      log_line("MenuVideoEncodings: No change in video parameters.");
      return false;
   }
   log_line("MenuVideoEncodings: Sending video encoding flags: %s", str_format_video_encoding_flags(profileNew.uProfileEncodingFlags));

   log_line("MenuVideoEncodings: Sending new video flags to vehicle for video profile %s: %s, %s, %s",
      str_get_video_profile_name(g_pCurrentModel->video_params.iCurrentVideoProfile),
      (profileNew.uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS)?"Retransmissions=On":"Retransmissions=Off",
      (profileNew.uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK)?"AdaptiveVideo=On":"AdaptiveVideo=Off",
      (profileNew.uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO)?"AdaptiveUseControllerInfo=On":"AdaptiveUseControllerInfo=Off"
      );

   log_line("MenuVideoEncodings: Adaptive video quantization: %s, %s",
    (profileNew.uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION)?"On":"ROff",
    (profileNew.uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH_HIGH)?"Strength: High":"Strength: Low");


   bool bIsInlineFastChange = false;
   if ( profileNew.iKeyframeMS != g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iKeyframeMS )
      bIsInlineFastChange = true;
   if ( (profileNew.uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME) != (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME) )
      bIsInlineFastChange = true;

   bool bResetState = false;
   if ( (paramsNew.iCurrentVideoProfile != g_pCurrentModel->video_params.iCurrentVideoProfile) ||
        ((profileNew.uProfileEncodingFlags & (~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME)) != (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & (~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME))) )
   {
      log_line("MenuVehicleVideoEncodings: Must reset adaptive state.");
      bIsInlineFastChange = false;
      bResetState = true;
   }

   bResetState = false;
   bIsInlineFastChange = true;

   if ( bIsInlineFastChange )
      log_line("MenuVehicleVideoEncodings: Doing fast inline change with no adaptive pause.");
   else
      send_pause_adaptive_to_router(4000);
   if ( bResetState )
      send_reset_adaptive_state_to_router(g_pCurrentModel->uVehicleId);

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMETERS, 0, (u8*)&paramsNew, sizeof(video_parameters_t), (u8*)(&profiles[0]), MAX_VIDEO_LINK_PROFILES * sizeof(type_video_link_profile)) )
   {
      valuesToUI();
      return false;
   }
   return true;
}

void MenuVehicleVideoEncodings::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( hardware_board_is_openipc(g_pCurrentModel->hwCapabilities.uBoardType) )
   if ( ((m_IndexHDMIOutput != -1) && (m_IndexHDMIOutput == m_SelectedIndex)) ||
        (m_IndexCustomQuant == m_SelectedIndex) ||
        (m_IndexQuantValue == m_SelectedIndex) ||
        (m_IndexEnableAdaptiveQuantization == m_SelectedIndex) ||
        (m_IndexAdaptiveH264QuantizationStrength == m_SelectedIndex) )
   {
      addUnsupportedMessageOpenIPC(NULL);
      valuesToUI();
      return;    
   }

   if ( ((m_IndexPacketSize != -1) && (m_IndexPacketSize == m_SelectedIndex)) || 
        (m_IndexBlockPackets == m_SelectedIndex) ||
        (m_IndexBlockECRate == m_SelectedIndex) || 
        (m_IndexHigherRates == m_SelectedIndex) ||
        (m_IndexLowerDRForEC == m_SelectedIndex) ||
        ((-1 != m_IndexECSchemeSpread) && (m_IndexECSchemeSpread == m_SelectedIndex)) )
      sendVideoParams();

   if ( (-1 != m_IndexHDMIOutput) && (m_IndexHDMIOutput == m_SelectedIndex) )
   {
      if ( hardware_board_is_openipc(g_pCurrentModel->hwCapabilities.uBoardType) )
      {
         addUnsupportedMessageOpenIPC(NULL);
         return;
      }
      video_parameters_t paramsOld;
      memcpy(&paramsOld, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      int index = m_pItemsSelect[1]->getSelectedIndex();
      if ( index == 0 )
         g_pCurrentModel->video_params.uVideoExtraFlags &= ~(VIDEO_FLAG_ENABLE_LOCAL_HDMI_OUTPUT);
      else
         g_pCurrentModel->video_params.uVideoExtraFlags |= VIDEO_FLAG_ENABLE_LOCAL_HDMI_OUTPUT;

      video_parameters_t paramsNew;
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      memcpy(&g_pCurrentModel->video_params, &paramsOld, sizeof(video_parameters_t));

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMETERS, 0, (u8*)&paramsNew, sizeof(video_parameters_t), (u8*)&(g_pCurrentModel->video_link_profiles[0]), MAX_VIDEO_LINK_PROFILES * sizeof(type_video_link_profile)) )
         valuesToUI();
      return;
   }

   if ( ((-1 != m_IndexNoise) && (m_IndexNoise == m_SelectedIndex)) ||
        ((-1 != m_IndexFocusMode) && (m_IndexFocusMode == m_SelectedIndex)) )
      sendVideoParams();

   if ( (-1 != m_IndexVideoBitrate) && (m_IndexVideoBitrate == m_SelectedIndex) )
   {
      sendVideoParams();
      return;
   }

   if ( (-1 != m_IndexH264Profile) && (m_IndexH264Profile == m_SelectedIndex) )
      sendVideoParams();
   if ( (-1 != m_IndexH264Level) && (m_IndexH264Level == m_SelectedIndex) )
      sendVideoParams();
   if ( (-1 != m_IndexH264Refresh) && (m_IndexH264Refresh == m_SelectedIndex) )
      sendVideoParams();
   if ( m_IndexMaxLinkLoadPercentage == m_SelectedIndex )
      sendVideoParams();

   if ( (-1 != m_IndexAutoKeyframe) && (m_IndexAutoKeyframe == m_SelectedIndex) )
   {
      sendVideoParams();
      return;
   }
   if ( (-1 != m_IndexKeyframeManual) && (m_IndexKeyframeManual == m_SelectedIndex) )
   {
      sendVideoParams();
      return;
   }

   if ( ((-1 != m_IndexRetransmissionsGuardInterval) && (m_IndexRetransmissionsGuardInterval == m_SelectedIndex)) || 
        ((-1 != m_IndexRetransmissionsFast) && (m_IndexRetransmissionsFast == m_SelectedIndex)) )
   {
      if ( ! is_sw_version_atleast(g_pCurrentModel, 11, 6) )
      {
         addMessage(L("Video functionality has changed. You need to update your vehicle software."));
         return;
      }
      sendVideoParams();
      return;
   }

   if ( (-1 != m_IndexRemoveH264PPS) && (m_IndexRemoveH264PPS == m_SelectedIndex) )
   {
      sendVideoParams();
      return;
   }
   if ( (-1 != m_IndexInsertH264PPS) && (m_IndexInsertH264PPS == m_SelectedIndex) )
   {
      sendVideoParams();
      return;
   }
   if ( (-1 != m_IndexInsertH264SPSTimings) && (m_IndexInsertH264SPSTimings == m_SelectedIndex) )
   {
      sendVideoParams();
      return;
   }

   if ( m_IndexH264Slices == m_SelectedIndex )
   {
      if ( sendVideoParams() )
      {
         #if defined HW_PLATFORM_RASPBERRY
         if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
            addMessage(L("Slice units might not work properly when a Raspberry Pi controller is paired with a vehicle running on OpenIPC hardware."));
         #endif
      }
      return;
   }

   if ( (-1 != m_IndexCustomQuant) && (-1 != m_IndexQuantValue) )
   if ( (m_IndexCustomQuant == m_SelectedIndex) || (m_IndexQuantValue == m_SelectedIndex) )
   if ( menu_check_current_model_ok_for_edit() )
   {
      sendVideoParams();
      return;    
   }

   if ( (-1 != m_IndexIPQuantizationDelta) && (m_IndexIPQuantizationDelta == m_SelectedIndex) )
   {
      sendVideoParams();
      return;
   }

   if ( (-1 != m_IndexLowerQPDeltaOnLink) && (m_IndexLowerQPDeltaOnLink == m_SelectedIndex) )
   {
      sendVideoParams();
      return;
   }

   if ( (m_IndexEnableAdaptiveQuantization == m_SelectedIndex) ||
        (m_IndexAdaptiveH264QuantizationStrength == m_SelectedIndex) )
   {
      sendVideoParams();
      return;
   }

   if ( m_IndexMaxKeyFrame == m_SelectedIndex )
   {
      sendVideoParams();
      return;
   }

   if ( (-1 != m_IndexMenuRetransmissions) && (m_IndexMenuRetransmissions == m_SelectedIndex) )
   {
      MenuVehicleVideoEncodings* pMenuEnc = new MenuVehicleVideoEncodings();
      pMenuEnc->m_bShowVideo = false;
      pMenuEnc->m_bShowRetransmissions = true;
      pMenuEnc->m_bShowEC = false;
      pMenuEnc->m_bShowH264 = false;
      add_menu_to_stack(pMenuEnc);
   }
}
