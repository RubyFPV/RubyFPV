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
#include "menu_vehicle_video_bidir.h"
#include "menu_vehicle_video_profile.h"
#include "menu_vehicle_video_encodings.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_section.h"

#include "../osd/osd_common.h"
#include "../process_router_messages.h"

MenuVehicleVideoBidirectional::MenuVehicleVideoBidirectional(void)
:Menu(MENU_ID_VEHICLE_VIDEO_BIDIRECTIONAL, L("Bidirectional Video Settings"), NULL)
{
   m_Width = 0.38;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.1;

   float dxMargin = 0.016 * Menu::getScaleFactor();
   float fSliderWidth = 0.12;

   m_IndexLevelRSSI = -1;
   m_IndexLevelSNR = -1;
   m_IndexLevelRetr = -1;
   m_IndexLevelRXLost = -1;
   m_IndexLevelECUsed = -1;
   m_IndexLevelECMax = -1;
   m_IndexLevelTimeToSwitchUp = -1;

   m_pMenuItemAdaptiveTimers = NULL;
   m_pMenuItemTimeToSwitchUp = NULL;
   m_pMenuItemRSSI = NULL;
   m_pMenuItemSNR = NULL;
   m_pMenuItemRetr = NULL;
   m_pMenuItemRxLost = NULL;
   m_pMenuItemECUsed = NULL;
   m_pMenuItemECMax = NULL;

   m_iTempAdaptiveStrength = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iAdaptiveAdjustmentStrength;
   m_uTempAdaptiveWeights = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uAdaptiveWeights;
   compute_adaptive_metrics(&m_AdaptiveMetrics, m_iTempAdaptiveStrength, m_uTempAdaptiveWeights);
   log_adaptive_metrics(g_pCurrentModel, &m_AdaptiveMetrics, m_iTempAdaptiveStrength, m_uTempAdaptiveWeights);

   m_pItemsRadio[0] = new MenuItemRadio("", L("Set the way video link behaves: fixed broadcast video, or auto adaptive video link and stream."));
   m_pItemsRadio[0]->addSelection(L("Fixed One Way Video Link"), L("The radio video link will be one way broadcast only, no retransmissions will take place, video quality and video parameters will not be adjusted in real time."));
   m_pItemsRadio[0]->addSelection(L("Bidirectional/Adaptive Video Link"), L("Video and radio link parameters will be adjusted automatically in real time based on radio conditions."));
   m_pItemsRadio[0]->setEnabled(true);
   m_pItemsRadio[0]->useSmallLegend(true);
   m_IndexOneWay = addMenuItem(m_pItemsRadio[0]);

   m_pItemsSelect[4] = new MenuItemSelect(L("Retransmissions"), L("Enable retransmissions of video data."));  
   m_pItemsSelect[4]->addSelection(L("Off"));
   m_pItemsSelect[4]->addSelection(L("On"));
   m_pItemsSelect[4]->setIsEditable();
   m_IndexRetransmissions = addMenuItem(m_pItemsSelect[4]);

   m_IndexRetransmissionsFast = -1;
   m_IndexRetransmissionsGuardInterval = -1;
   /*
   m_pItemsSelect[6] = new MenuItemSelect(L("Retransmissions Algorithm"), L("Change the way retransmissions are requested."));  
   m_pItemsSelect[6]->addSelection(L("Regular"));
   m_pItemsSelect[6]->addSelection(L("Aggressive"));
   m_pItemsSelect[6]->setIsEditable();
   m_pItemsSelect[6]->setMargin(dxMargin);
   m_pItemsSelect[6]->setExtraHeight(1.0* g_pRenderEngine->textHeight(g_idFontMenu) * MENU_ITEM_SPACING);
   m_IndexRetransmissionsFast = addMenuItem(m_pItemsSelect[6]);

   m_pItemsSlider[8] = new MenuItemSlider(L("Retransmissions Sensitivity (ms)"), L("How sensitive should the retransmissions detection be. Lower values means more sensitive and more proactive retransmissions done."), 0,15,5, fSliderWidth);
   m_pItemsSlider[8]->setMargin(dxMargin);
   m_IndexRetransmissionsGuardInterval = addMenuItem(m_pItemsSlider[8]);
   */
   m_IndexAdaptiveVideo = -1;
   m_IndexAdaptiveVideoLevel = -1;
   m_IndexAdaptiveAlgorithm = -1;
   m_IndexAdaptiveAdjustmentStrength = -1;
   m_IndexAdaptiveUseControllerToo = -1;
   m_IndexVideoLinkLost = -1;

   m_pItemsSelect[2] = new MenuItemSelect(L("Adaptive Video Quality"), L("Reduce the video quality when radio link quality goes down."));  
   m_pItemsSelect[2]->addSelection("Off");
   m_pItemsSelect[2]->addSelection("On");
   m_pItemsSelect[2]->setIsEditable();
   m_IndexAdaptiveVideo = addMenuItem(m_pItemsSelect[2]);

   if ( g_pCurrentModel->isAllVideoLinksFixedRate() )
   {
      MenuItemText* pItem = new MenuItemText("", false);
      log_line("MenuVehicleVideoBidirectional: All radio links (%d links) have fixed rate.", g_pCurrentModel->radioLinksParams.links_count);
      for( int iLink=0; iLink<g_pCurrentModel->radioLinksParams.links_count; iLink++ )
      {
         log_line("MenuVehicleVideoBidirectional: Is radio link %d adaptive usable? %s, set to datarate video: %d", iLink+1, g_pCurrentModel->isRadioLinkAdaptiveUsable(iLink)?"yes":"no", g_pCurrentModel->radioLinksParams.downlink_datarate_video_bps[iLink]);
      }
      g_pCurrentModel->logVehicleRadioInfo();

      pItem->setTitle(L("Warning: You did set your radio links to fixed radio modulations. Adaptive video is turned off. Switch the radio links to Auto modulations if you want to enable adaptive video."));
      pItem->setHidden(false);
      pItem->highlightFirstWord(true);
      addMenuItem(pItem);
      m_pItemsSelect[2]->setEnabled(false);
   }
   else
   {
      m_pItemsSelect[5] = new MenuItemSelect(L("Adaptive Level"), L("Automatically adjust video and radio transmission parameters when the radio link quality goes lower. Full: go all the way to lowest video quality and best transmission if needed. Medium: Moderate lower quality."));
      m_pItemsSelect[5]->addSelection(L("Medium"));
      m_pItemsSelect[5]->addSelection(L("Full"));
      m_pItemsSelect[5]->setIsEditable();
      m_pItemsSelect[5]->setMargin(dxMargin);
      m_IndexAdaptiveVideoLevel = addMenuItem(m_pItemsSelect[5]);

      /*
      m_pItemsSelect[0] = new MenuItemSelect(L("Algorithm"), L("Change the way adaptive video works."));
      m_pItemsSelect[0]->addSelection(L("Default"));
      m_pItemsSelect[0]->addSelection(L("New"));
      m_pItemsSelect[0]->setIsEditable();
      m_pItemsSelect[0]->setMargin(dxMargin);
      m_IndexAdaptiveAlgorithm = addMenuItem(m_pItemsSelect[0]);
      */

      m_pItemsSlider[0] = new MenuItemSlider(L("Auto Adjustment Strength"), L("How aggressive should the auto video link adjustments be (adaptive video and auto keyframe). 1 is the slowest adjustment strength, 10 is the fastest and most aggressive adjustment strength."), 1,10,5, fSliderWidth);
      m_pItemsSlider[0]->setMargin(dxMargin);
      m_IndexAdaptiveAdjustmentStrength = addMenuItem(m_pItemsSlider[0]);
      
      log_line("MenuVehicleVideoBidirectional: Adaptive strength menu item index: %d", m_IndexAdaptiveAdjustmentStrength);
      ControllerSettings* pCS = get_ControllerSettings();
      if ( pCS->iDeveloperMode )
      {
         m_pMenuItemAdaptiveTimers = new MenuItemText("Min time to switch lower/higher:", true);
         m_pMenuItemAdaptiveTimers->setTextColor(get_Color_Dev());
         addMenuItem(m_pMenuItemAdaptiveTimers);

         m_pMenuItemTimeToSwitchUp = new MenuItemText("Good interval to switch up:", true);
         m_pMenuItemTimeToSwitchUp->setTextColor(get_Color_Dev());
         addMenuItem(m_pMenuItemTimeToSwitchUp);
         m_pMenuItemRSSI = new MenuItemText("RSSI/SNR:", true);
         m_pMenuItemRSSI->setTextColor(get_Color_Dev());
         addMenuItem(m_pMenuItemRSSI);
         m_pMenuItemRetr = new MenuItemText("Retr/RxLost:", true);
         m_pMenuItemRetr->setTextColor(get_Color_Dev());
         addMenuItem(m_pMenuItemRetr);
         m_pMenuItemECUsed = new MenuItemText("EC-Used/EC-Max:", true);
         m_pMenuItemECUsed->setTextColor(get_Color_Dev());
         addMenuItem(m_pMenuItemECUsed);

         m_pItemsSlider[1] = new MenuItemSlider(L("RSSI Strength"), L("Weight of RSSI signal. 0 for disabled."), 0,15,5, fSliderWidth);
         m_pItemsSlider[1]->setMargin(dxMargin);
         m_pItemsSlider[1]->setTextColor(get_Color_Dev());
         m_IndexLevelRSSI = addMenuItem(m_pItemsSlider[1]);

         m_pItemsSlider[2] = new MenuItemSlider(L("SNR Strength"), L("Weight of SNR of signal. 0 for disabled."), 0,15,5, fSliderWidth);
         m_pItemsSlider[2]->setMargin(dxMargin);
         m_pItemsSlider[2]->setTextColor(get_Color_Dev());
         m_IndexLevelSNR = addMenuItem(m_pItemsSlider[2]);

         m_pItemsSlider[3] = new MenuItemSlider(L("Retransmissions Strength"), L("Weight of retransmissions activity. 0 for disabled."), 0,15,5, fSliderWidth);
         m_pItemsSlider[3]->setMargin(dxMargin);
         m_pItemsSlider[3]->setTextColor(get_Color_Dev());
         m_IndexLevelRetr = addMenuItem(m_pItemsSlider[3]);

         m_pItemsSlider[4] = new MenuItemSlider(L("RX Packets Strength"), L("Weight of lost RX packets. 0 for disabled."), 0,15,5, fSliderWidth);
         m_pItemsSlider[4]->setMargin(dxMargin);
         m_pItemsSlider[4]->setTextColor(get_Color_Dev());
         m_IndexLevelRXLost = addMenuItem(m_pItemsSlider[4]);

         m_pItemsSlider[5] = new MenuItemSlider(L("EC-Used Strength"), L("Weight of all error correction activity. 0 for disabled."), 0,15,5, fSliderWidth);
         m_pItemsSlider[5]->setMargin(dxMargin);
         m_pItemsSlider[5]->setTextColor(get_Color_Dev());
         m_IndexLevelECUsed = addMenuItem(m_pItemsSlider[5]);

         m_pItemsSlider[6] = new MenuItemSlider(L("EC-Max Strength"), L("Weight of max error correction activity. 0 for disabled."), 0,15,5, fSliderWidth);
         m_pItemsSlider[6]->setMargin(dxMargin);
         m_pItemsSlider[6]->setTextColor(get_Color_Dev());
         m_IndexLevelECMax = addMenuItem(m_pItemsSlider[6]);

         m_pItemsSlider[7] = new MenuItemSlider(L("Switch Up Delay"), L("Weight of good time to switch up. 0 for disabled."), 0,15,5, fSliderWidth);
         m_pItemsSlider[7]->setMargin(dxMargin);
         m_pItemsSlider[7]->setTextColor(get_Color_Dev());
         m_IndexLevelTimeToSwitchUp = addMenuItem(m_pItemsSlider[7]);
      }

      /*
      m_pItemsSelect[9] = new MenuItemSelect(L("Use Controller Feedback"), L("When vehicle adjusts video link params, use the feedback from the controller too in deciding the best video link params to be used."));  
      m_pItemsSelect[9]->addSelection(L("No"));
      m_pItemsSelect[9]->addSelection(L("Yes"));
      m_pItemsSelect[9]->setIsEditable();
      m_pItemsSelect[9]->setMargin(dxMargin);
      m_IndexAdaptiveUseControllerToo = addMenuItem(m_pItemsSelect[9]);
      */

      /*
      m_pItemsSelect[10] = new MenuItemSelect(L("Lower Quality On Link Lost"), L("When vehicle looses connection from the controller, go to a lower video quality and lower radio datarate."));  
      m_pItemsSelect[10]->addSelection(L("No"));
      m_pItemsSelect[10]->addSelection(L("Yes"));
      m_pItemsSelect[10]->setIsEditable();
      m_pItemsSelect[10]->setMargin(dxMargin);
      m_IndexVideoLinkLost = addMenuItem(m_pItemsSelect[10]);
      */
   }
}

MenuVehicleVideoBidirectional::~MenuVehicleVideoBidirectional()
{
}

void MenuVehicleVideoBidirectional::onShow()
{
   int iTmp = getSelectedMenuItemIndex();
   valuesToUI();
   Menu::onShow();

   if ( (iTmp >= 0) && (iTmp <m_ItemsCount) )
   {
      m_SelectedIndex = iTmp;
      onFocusedItemChanged();
   }
}

void MenuVehicleVideoBidirectional::valuesToUI()
{
   m_iTempAdaptiveStrength = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iAdaptiveAdjustmentStrength;
   m_uTempAdaptiveWeights = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uAdaptiveWeights;
   compute_adaptive_metrics(&m_AdaptiveMetrics, m_iTempAdaptiveStrength, m_uTempAdaptiveWeights);
   _updateDevValues();

   int adaptiveVideo = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK)?1:0;
   int useControllerInfo = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO)?1:0;
   int controllerLinkLost = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST)?1:0;
   u32 uProfileFlags = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileFlags;
   if ( hardware_board_is_goke(g_pCurrentModel->hwCapabilities.uBoardType) )
      adaptiveVideo = 0;

   int iFixedVideoLink = 0;
   if ( g_pCurrentModel->isVideoLinkFixedOneWay() )
      iFixedVideoLink = 1;
   m_pItemsRadio[0]->setSelectedIndex(1-iFixedVideoLink);
   m_pItemsRadio[0]->setFocusedIndex(1-iFixedVideoLink);


   if ( -1 != m_IndexAdaptiveAlgorithm )
   {
      if ( iFixedVideoLink )
      {
         m_pItemsSelect[0]->setSelection(0);
         m_pItemsSelect[0]->setEnabled(false);
      }
      else
      {
         m_pItemsSelect[0]->setSelection( (g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_NEW_ADAPTIVE_ALGORITHM)?1:0);
         m_pItemsSelect[0]->setEnabled(false);
      }
   }

   if ( iFixedVideoLink )
   {
      m_pItemsSelect[4]->setSelectedIndex(0);
      m_pItemsSelect[4]->setEnabled(false);
      if ( -1 != m_IndexRetransmissionsFast )
         m_pItemsSelect[6]->setEnabled(false);
      if ( -1 != m_IndexRetransmissionsGuardInterval )
         m_pItemsSlider[8]->setEnabled(false);
   }
   else if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS )
   {
      m_pItemsSelect[4]->setSelectedIndex(1);
      if ( -1 != m_IndexRetransmissionsFast )
         m_pItemsSelect[6]->setEnabled(true);
      if ( -1 != m_IndexRetransmissionsGuardInterval )
         m_pItemsSlider[8]->setEnabled(true);
   }
   else
   {
      m_pItemsSelect[4]->setSelectedIndex(0);
      if ( -1 != m_IndexRetransmissionsFast )
         m_pItemsSelect[6]->setEnabled(false);
      if ( -1 != m_IndexRetransmissionsGuardInterval )
         m_pItemsSlider[8]->setEnabled(false);
   }
   
   if ( -1 != m_IndexRetransmissionsFast )
      m_pItemsSelect[6]->setSelectedIndex((uProfileFlags & VIDEO_PROFILE_FLAG_RETRANSMISSIONS_AGGRESIVE)?1:0);
   if ( -1 != m_IndexRetransmissionsGuardInterval )
      m_pItemsSlider[8]->setCurrentValue((uProfileFlags & VIDEO_PROFILE_FLAG_MASK_RETRANSMISSIONS_GUARD_MASK) >> 8);

   if ( -1 != m_IndexAdaptiveVideo )
   {
      if ( adaptiveVideo && (! iFixedVideoLink) && (! g_pCurrentModel->isAllVideoLinksFixedRate()) )
      {
         m_pItemsSelect[2]->setEnabled(true);
         m_pItemsSelect[2]->setSelectedIndex(1);
      }
      else
      {
         m_pItemsSelect[2]->setSelectedIndex(0);
         if ( iFixedVideoLink || g_pCurrentModel->isAllVideoLinksFixedRate() )
            m_pItemsSelect[2]->setEnabled(false);
         else
            m_pItemsSelect[2]->setEnabled(true);
      }
   }
   if ( adaptiveVideo && (! iFixedVideoLink) )
   {
      if ( -1 != m_IndexAdaptiveVideoLevel )
      {
         m_pItemsSelect[5]->setEnabled(true);
         if ( (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO )
            m_pItemsSelect[5]->setSelectedIndex(0);
         else
            m_pItemsSelect[5]->setSelectedIndex(1);
      }
      if ( -1 != m_IndexAdaptiveAdjustmentStrength )
      {
         m_pItemsSlider[0]->setEnabled(true);
         m_pItemsSlider[0]->setCurrentValue(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].iAdaptiveAdjustmentStrength);
      }
      if ( -1 != m_IndexAdaptiveUseControllerToo )
         m_pItemsSelect[9]->setEnabled(true);
      if ( -1 != m_IndexVideoLinkLost )
         m_pItemsSelect[10]->setEnabled(true);

      if ( -1 != m_IndexAdaptiveUseControllerToo )
         m_pItemsSelect[9]->setSelectedIndex(useControllerInfo);
      if ( -1 != m_IndexVideoLinkLost )
         m_pItemsSelect[10]->setSelectedIndex(controllerLinkLost);

      if ( -1 != m_IndexLevelRSSI )
         m_pItemsSlider[1]->setEnabled(true);
      if ( -1 != m_IndexLevelSNR )
         m_pItemsSlider[2]->setEnabled(true);
      if ( -1 != m_IndexLevelRetr )
         m_pItemsSlider[3]->setEnabled(true);
      if ( -1 != m_IndexLevelRXLost )
         m_pItemsSlider[4]->setEnabled(true);
      if ( -1 != m_IndexLevelECUsed )
         m_pItemsSlider[5]->setEnabled(true);
      if ( -1 != m_IndexLevelECMax )
         m_pItemsSlider[6]->setEnabled(true);
      if ( -1 != m_IndexLevelTimeToSwitchUp )
         m_pItemsSlider[7]->setEnabled(true);
   }
   else
   {
      if ( -1 != m_IndexAdaptiveVideoLevel )
         m_pItemsSelect[5]->setEnabled(false);
      if ( -1 != m_IndexAdaptiveAdjustmentStrength )
         m_pItemsSlider[0]->setEnabled(false);
      if ( -1 != m_IndexAdaptiveUseControllerToo )
         m_pItemsSelect[9]->setEnabled(false);
      if ( -1 != m_IndexVideoLinkLost )
         m_pItemsSelect[10]->setEnabled(false);

      if ( -1 != m_IndexLevelRSSI )
         m_pItemsSlider[1]->setEnabled(false);
      if ( -1 != m_IndexLevelSNR )
         m_pItemsSlider[2]->setEnabled(false);
      if ( -1 != m_IndexLevelRetr )
         m_pItemsSlider[3]->setEnabled(false);
      if ( -1 != m_IndexLevelRXLost )
         m_pItemsSlider[4]->setEnabled(false);
      if ( -1 != m_IndexLevelECUsed )
         m_pItemsSlider[5]->setEnabled(false);
      if ( -1 != m_IndexLevelECMax )
         m_pItemsSlider[6]->setEnabled(false);
      if ( -1 != m_IndexLevelTimeToSwitchUp )
         m_pItemsSlider[7]->setEnabled(false);
   }

   u32 uWeights = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uAdaptiveWeights;
   if ( -1 != m_IndexLevelRSSI )
      m_pItemsSlider[1]->setCurrentValue(uWeights & 0x0F);
   if ( -1 != m_IndexLevelSNR )
      m_pItemsSlider[2]->setCurrentValue((uWeights >> 4) & 0x0F);
   if ( -1 != m_IndexLevelRetr )
      m_pItemsSlider[3]->setCurrentValue((uWeights >> 8) & 0x0F);
   if ( -1 != m_IndexLevelRXLost )
      m_pItemsSlider[4]->setCurrentValue((uWeights >> 12) & 0x0F);
   if ( -1 != m_IndexLevelECUsed )
      m_pItemsSlider[5]->setCurrentValue((uWeights >> 16) & 0x0F);
   if ( -1 != m_IndexLevelECMax )
      m_pItemsSlider[6]->setCurrentValue((uWeights >> 20) & 0x0F);
   if ( -1 != m_IndexLevelTimeToSwitchUp )
      m_pItemsSlider[7]->setCurrentValue((uWeights >> 24) & 0x0F);
}

void MenuVehicleVideoBidirectional::_updateDevValues()
{
   char szText[256];

   if ( NULL != m_pMenuItemAdaptiveTimers )
   {
      sprintf(szText, "Min time to switch lower/higher: %u / %u", m_AdaptiveMetrics.uMinimumTimeToSwitchLower, m_AdaptiveMetrics.uMinimumTimeToSwitchHigher);
      m_pMenuItemAdaptiveTimers->setTitle(szText);
   }

   if ( NULL != m_pMenuItemRSSI )
   {
      strcpy(szText, "RSSI: Disabled / SNR: Disabled");
      if ( (m_AdaptiveMetrics.iMinimRSSIThreshold > -1000) && (m_AdaptiveMetrics.iMinimSNRThreshold > -1000) )
         sprintf(szText, "RSSI: %d dbm margin / SNR: %d dbm margin", m_AdaptiveMetrics.iMinimRSSIThreshold, m_AdaptiveMetrics.iMinimSNRThreshold);
      else if ( m_AdaptiveMetrics.iMinimRSSIThreshold > -1000 )
         sprintf(szText, "RSSI: %d dbm margin / SNR: Disabled", m_AdaptiveMetrics.iMinimRSSIThreshold);
      else
         sprintf(szText, "RSSI: Disabled / SNR: %d dbm margin", m_AdaptiveMetrics.iMinimSNRThreshold);
      m_pMenuItemRSSI->setTitle(szText);
   }

   if ( NULL != m_pMenuItemTimeToSwitchUp )
   {
      sprintf(szText, "Good interval to switch up: %u ms", m_AdaptiveMetrics.uMinimumGoodTimeToSwitchHigher);
      m_pMenuItemTimeToSwitchUp->setTitle(szText);    
   }

   if ( NULL != m_pMenuItemRetr )
   {
      strcpy(szText, "Retr/Rx-Lost: Disabled / Disabled");
      if ( (m_AdaptiveMetrics.uTimeToLookBackForRetr != MAX_U32) && (m_AdaptiveMetrics.uTimeToLookBackForRxLost != MAX_U32) )
         sprintf(szText, "Retr/Rx-Lost: max %d on %u ms / max %d%% on %u ms", m_AdaptiveMetrics.iMaxRetr, m_AdaptiveMetrics.uTimeToLookBackForRetr, m_AdaptiveMetrics.iMaxRxLostPercent, m_AdaptiveMetrics.uTimeToLookBackForRxLost);
      else if ( m_AdaptiveMetrics.uTimeToLookBackForRetr != MAX_U32 )
         sprintf(szText, "Retr/Rx-Lost: max %d on %u ms / Disabled", m_AdaptiveMetrics.iMaxRetr, m_AdaptiveMetrics.uTimeToLookBackForRetr);
      else
         sprintf(szText, "Retr/Rx-Lost: Disabled / max %d%% on %u ms", m_AdaptiveMetrics.iMaxRxLostPercent, m_AdaptiveMetrics.uTimeToLookBackForRxLost);
      m_pMenuItemRetr->setTitle(szText);
   }

   if ( NULL != m_pMenuItemECUsed )
   {
      strcpy(szText, "EC-Used/EC-Max: Disabled / Disabled");
      if ( (m_AdaptiveMetrics.uTimeToLookBackForECUsed != MAX_U32) && (m_AdaptiveMetrics.uTimeToLookBackForECMax != MAX_U32) )
         sprintf(szText, "EC-Used/EC-Max: max %d%% on %u ms / max %d%% on %u ms", m_AdaptiveMetrics.iPercentageECUsed, m_AdaptiveMetrics.uTimeToLookBackForECUsed, m_AdaptiveMetrics.iPercentageECMax, m_AdaptiveMetrics.uTimeToLookBackForECMax);
      else if ( MAX_U32 != m_AdaptiveMetrics.uTimeToLookBackForECUsed )
         sprintf(szText, "EC-Used/EC-Max: max %d%% on %u ms / Disabled", m_AdaptiveMetrics.iPercentageECUsed, m_AdaptiveMetrics.uTimeToLookBackForECUsed);
      else
         sprintf(szText, "EC-Used/EC-Max: Disabled / max %d%% on %u ms", m_AdaptiveMetrics.iPercentageECMax, m_AdaptiveMetrics.uTimeToLookBackForECMax);

      m_pMenuItemECUsed->setTitle(szText);
   }
}

void MenuVehicleVideoBidirectional::Render()
{
   RenderPrepare();
   
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehicleVideoBidirectional::sendVideoSettings(bool bIsInlineFastChange)
{
   video_parameters_t paramsNew;
   type_video_link_profile profileNew;
   memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
   memcpy(&profileNew, &(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile]), sizeof(type_video_link_profile));

   profileNew.uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ONE_WAY_FIXED_VIDEO;
   if ( 0 == m_pItemsRadio[0]->getSelectedIndex() )
      profileNew.uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ONE_WAY_FIXED_VIDEO;

   if ( 0 == m_pItemsSelect[4]->getSelectedIndex() )
      profileNew.uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS;
   else
      profileNew.uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS;

   if ( -1 != m_IndexRetransmissionsFast )
   {
      profileNew.uProfileFlags &= ~VIDEO_PROFILE_FLAG_RETRANSMISSIONS_AGGRESIVE;
      if ( 1 == m_pItemsSelect[6]->getSelectedIndex() )
         profileNew.uProfileFlags |= VIDEO_PROFILE_FLAG_RETRANSMISSIONS_AGGRESIVE;
   }

   if ( -1 != m_IndexRetransmissionsGuardInterval )
   {
      profileNew.uProfileFlags &= ~VIDEO_PROFILE_FLAG_MASK_RETRANSMISSIONS_GUARD_MASK;
      profileNew.uProfileFlags |= (((u32)(m_pItemsSlider[8]->getCurrentValue())) << 8);
   }

   if ( -1 != m_IndexAdaptiveUseControllerToo )
   {
      if ( 0 == m_pItemsSelect[9]->getSelectedIndex() )
         profileNew.uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO;
      else
         profileNew.uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO;
   }

   if ( profileNew.uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK )
   if ( -1 != m_IndexLevelRSSI )
   {
      profileNew.uAdaptiveWeights = (m_pItemsSlider[1]->getCurrentValue() & 0x0F) |
         (((m_pItemsSlider[2]->getCurrentValue()) & 0x0F) << 4) |
         (((m_pItemsSlider[3]->getCurrentValue()) & 0x0F) << 8) |
         (((m_pItemsSlider[4]->getCurrentValue()) & 0x0F) << 12) |
         (((m_pItemsSlider[5]->getCurrentValue()) & 0x0F) << 16) |
         (((m_pItemsSlider[6]->getCurrentValue()) & 0x0F) << 20) |
         (((m_pItemsSlider[7]->getCurrentValue()) & 0x0F) << 24);
   }

   if ( -1 != m_IndexVideoLinkLost )
   {
      if ( 0 == m_pItemsSelect[10]->getSelectedIndex() )
         profileNew.uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST;
      else
         profileNew.uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST;
   }

   if ( -1 != m_IndexAdaptiveVideo )
   {
      if ( 0 == m_pItemsSelect[2]->getSelectedIndex() )
      {
         profileNew.uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK;
         profileNew.uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO;
      }
      else
      {
         profileNew.uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK;
         if ( -1 != m_IndexAdaptiveAdjustmentStrength )
            profileNew.iAdaptiveAdjustmentStrength = m_pItemsSlider[0]->getCurrentValue();
    
         if ( -1 != m_IndexAdaptiveVideoLevel )
         {
            if ( 0 == m_pItemsSelect[5]->getSelectedIndex() )
               profileNew.uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO;
            else
               profileNew.uProfileEncodingFlags &= (~VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO);
         }
      }
   }

   if ( -1 != m_IndexAdaptiveAlgorithm )
   {
      paramsNew.uVideoExtraFlags &= ~VIDEO_FLAG_NEW_ADAPTIVE_ALGORITHM;
      if ( 1 == m_pItemsSelect[0]->getSelectedIndex() )
         paramsNew.uVideoExtraFlags |= VIDEO_FLAG_NEW_ADAPTIVE_ALGORITHM;
   }

   type_video_link_profile profiles[MAX_VIDEO_LINK_PROFILES];
   memcpy((u8*)&profiles[0], (u8*)&g_pCurrentModel->video_link_profiles[0], MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile));
   char szCurrentProfile[64];
   strcpy(szCurrentProfile, str_get_video_profile_name(g_pCurrentModel->video_params.iCurrentVideoProfile));
   int iMatchProfile = g_pCurrentModel->isVideoSettingsMatchingBuiltinVideoProfile(&paramsNew, &profileNew);
   if ( (iMatchProfile >= 0) && (iMatchProfile < MAX_VIDEO_LINK_PROFILES) )
   {
      log_line("MenuVehicleVideoBidirectional: Will switch to matched to video profile %s, current video profile was: %s", str_get_video_profile_name(iMatchProfile), szCurrentProfile);
      paramsNew.iCurrentVideoProfile = iMatchProfile;
   }
   else
   {
      log_line("MenuVehicleVideoBidirectional: Will switch to custom profile, current video profile was: %s", szCurrentProfile);
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

   log_line("MenuVehicleVideoBidirectional: Is fast change: %s, selected menu item index: %d", bIsInlineFastChange?"yes":"no", m_SelectedIndex);
   log_line("MenuVehicleVideoBidirectional: Sending video encoding flags: %s", str_format_video_encoding_flags(profileNew.uProfileEncodingFlags));

   bool bResetState = false;
   if ( (paramsNew.iCurrentVideoProfile != g_pCurrentModel->video_params.iCurrentVideoProfile) ||
        (profileNew.uProfileEncodingFlags != g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.iCurrentVideoProfile].uProfileEncodingFlags) )
   {
      log_line("MenuVehicleVideoBidirectional: Must reset adaptive state.");
      bIsInlineFastChange = false;
      bResetState = true;
   }
   if ( bIsInlineFastChange )
      log_line("MenuVehicleVideoBidirectional: Doing fast inline change with no adaptive pause.");
   else
      send_pause_adaptive_to_router(4000);
   if ( bResetState )
      send_reset_adaptive_state_to_router(g_pCurrentModel->uVehicleId);

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMETERS, 0, (u8*)&paramsNew, sizeof(video_parameters_t), (u8*)&profiles[0], MAX_VIDEO_LINK_PROFILES * sizeof(type_video_link_profile)) )
      valuesToUI();
}

void MenuVehicleVideoBidirectional::onItemValueChanged(int itemIndex)
{
   Menu::onItemValueChanged(itemIndex);

   if ( ! m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   bool bUpdate = false;
   if ( (-1 != m_IndexAdaptiveAdjustmentStrength) && (itemIndex == m_IndexAdaptiveAdjustmentStrength) )
      bUpdate = true;

   if ( m_IndexLevelRSSI != -1 )
   if ( (itemIndex == m_IndexLevelRSSI) ||
        (itemIndex == m_IndexLevelSNR) ||
        (itemIndex == m_IndexLevelRetr) ||
        (itemIndex == m_IndexLevelRXLost) ||
        (itemIndex == m_IndexLevelECUsed) ||
        (itemIndex == m_IndexLevelECMax) ||
        (itemIndex == m_IndexLevelTimeToSwitchUp) )
      bUpdate = true;

   if ( ! bUpdate )
      return;

   if ( (-1 != m_IndexAdaptiveAdjustmentStrength) && (itemIndex == m_IndexAdaptiveAdjustmentStrength) )
      m_iTempAdaptiveStrength = m_pItemsSlider[0]->getCurrentValue();
   else if ( -1 != m_IndexLevelRSSI )
      m_uTempAdaptiveWeights = (m_pItemsSlider[1]->getCurrentValue() & 0x0F) |
         (((m_pItemsSlider[2]->getCurrentValue()) & 0x0F) << 4) |
         (((m_pItemsSlider[3]->getCurrentValue()) & 0x0F) << 8) |
         (((m_pItemsSlider[4]->getCurrentValue()) & 0x0F) << 12) |
         (((m_pItemsSlider[5]->getCurrentValue()) & 0x0F) << 16) |
         (((m_pItemsSlider[6]->getCurrentValue()) & 0x0F) << 20) |
         (((m_pItemsSlider[7]->getCurrentValue()) & 0x0F) << 24);

   compute_adaptive_metrics(&m_AdaptiveMetrics, m_iTempAdaptiveStrength, m_uTempAdaptiveWeights);
   _updateDevValues();
}

void MenuVehicleVideoBidirectional::onItemEndEdit(int itemIndex)
{
   Menu::onItemEndEdit(itemIndex);
}


void MenuVehicleVideoBidirectional::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);
}

void MenuVehicleVideoBidirectional::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   log_line("MenuVehicleVideoBidirectional: selected menu item index: %d", m_SelectedIndex);
   g_TimeLastVideoCameraChangeCommand = g_TimeNow;

   if ( m_IndexOneWay == m_SelectedIndex )
   {
      int iIndex = m_pItemsRadio[0]->getFocusedIndex();
      log_line("MenuVehicleVideoBidirectional: Selected video link mode option %d", iIndex);

      if ( iIndex == 1 )
      if ( g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_DOWNLINK_ONLY )
      {
         addMessage2(0, L("Incompatible settings"), L("You have disabled all the radio uplinks. Adaptive video mode is not possible without any radio uplink. Enable radio uplinks from Menu->Vehicle->Radio."));
         valuesToUI();
         return;
      } 
      m_pItemsRadio[0]->setSelectedIndex(iIndex);
      sendVideoSettings(false);
   }

   if ( (-1 != m_IndexAdaptiveAlgorithm) && (m_IndexAdaptiveAlgorithm == m_SelectedIndex) )
   {
      sendVideoSettings(false);
      return;
   }
   
   if ( (-1 != m_IndexAdaptiveVideo) && (m_IndexAdaptiveVideo == m_SelectedIndex) )
   if ( hardware_board_is_goke(g_pCurrentModel->hwCapabilities.uBoardType) )
   {
      addUnsupportedMessageOpenIPCGoke(NULL);
      valuesToUI();
      return;
   }

   if ( (m_IndexRetransmissions == m_SelectedIndex) ||
        ((-1 != m_IndexRetransmissionsGuardInterval) && (m_IndexRetransmissionsGuardInterval == m_SelectedIndex)) || 
        ((-1 != m_IndexVideoLinkLost) && (m_IndexVideoLinkLost == m_SelectedIndex)) )
   {
      sendVideoSettings(false);
      return;
   }

   if ( (-1 != m_IndexRetransmissionsFast) && (m_IndexRetransmissionsFast == m_SelectedIndex) )
   {
      if ( ! is_sw_version_atleast(g_pCurrentModel, 11, 6) )
      {
         addMessage(L("Video functionality has changed. You need to update your vehicle software."));
         return;
      }
      sendVideoSettings(true);
      return;
   }


   if ( ((-1 != m_IndexAdaptiveVideo) && (m_IndexAdaptiveVideo == m_SelectedIndex)) ||
      ((-1 != m_IndexAdaptiveUseControllerToo) && (m_IndexAdaptiveUseControllerToo == m_SelectedIndex)) )
   {
      sendVideoSettings(false);
      return;
   }

   if ( (-1 != m_IndexAdaptiveVideoLevel) && (m_IndexAdaptiveVideoLevel == m_SelectedIndex) )
   {
      sendVideoSettings(false);
      return;
   }

   if ( (-1 != m_IndexAdaptiveAdjustmentStrength) && (m_IndexAdaptiveAdjustmentStrength == m_SelectedIndex) )
   {
      sendVideoSettings(true);
      return;
   }

   if ( ((-1 != m_IndexLevelRSSI) && (m_SelectedIndex == m_IndexLevelRSSI)) ||
        ((-1 != m_IndexLevelSNR) && (m_SelectedIndex == m_IndexLevelSNR)) ||
        ((-1 != m_IndexLevelRetr) && (m_SelectedIndex == m_IndexLevelRetr)) ||
        ((-1 != m_IndexLevelRXLost) && (m_SelectedIndex == m_IndexLevelRXLost)) ||
        ((-1 != m_IndexLevelECUsed) && (m_SelectedIndex == m_IndexLevelECUsed)) ||
        ((-1 != m_IndexLevelECMax) && (m_SelectedIndex == m_IndexLevelECMax)) ||
        ((-1 != m_IndexLevelTimeToSwitchUp) && (m_SelectedIndex == m_IndexLevelTimeToSwitchUp)) )
   {
      sendVideoSettings(true);
      return;
   }
}
