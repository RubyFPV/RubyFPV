/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga petrusoroaga@yahoo.com
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
   m_Width = 0.36;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.1;

   float dxMargin = 0.016 * Menu::getScaleFactor();
   float fSliderWidth = 0.12;

   m_pItemsSelect[4] = new MenuItemSelect(L("Retransmissions"), L("Enable retransmissions of video data."));  
   m_pItemsSelect[4]->addSelection(L("Off"));
   m_pItemsSelect[4]->addSelection(L("On"));
   m_pItemsSelect[4]->setIsEditable();
   m_IndexRetransmissions = addMenuItem(m_pItemsSelect[4]);

   m_pItemsSelect[6] = new MenuItemSelect(L("Retransmissions Algorithm"), L("Change the way retransmissions are requested."));  
   m_pItemsSelect[6]->addSelection(L("Regular"));
   m_pItemsSelect[6]->addSelection(L("Aggressive"));
   m_pItemsSelect[6]->setIsEditable();
   m_pItemsSelect[6]->setMargin(dxMargin);
   m_pItemsSelect[6]->setExtraHeight(1.0* g_pRenderEngine->textHeight(g_idFontMenu) * MENU_ITEM_SPACING);
   m_IndexRetransmissionsFast = addMenuItem(m_pItemsSelect[6]);

   m_pItemsSelect[2] = new MenuItemSelect(L("Auto Video Quality"), L("Reduce the video quality when radio link quality goes down."));  
   m_pItemsSelect[2]->addSelection("Off");
   m_pItemsSelect[2]->addSelection("On");
   m_pItemsSelect[2]->setIsEditable();
   m_IndexAdaptiveVideo = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSelect[5] = new MenuItemSelect(L("Adaptive Video Level"), L("Automatically adjust video and radio transmission parameters when the radio link quality and the video link quality goes lower in order to improve range, video link and quality. Full: go to lowest video quality and best transmission if needed. Medium: Moderate lower quality."));
   m_pItemsSelect[5]->addSelection(L("Medium"));
   m_pItemsSelect[5]->addSelection(L("Full"));
   m_pItemsSelect[5]->setIsEditable();
   m_pItemsSelect[5]->setMargin(dxMargin);
   m_IndexAdaptiveVideoLevel = addMenuItem(m_pItemsSelect[5]);

   m_pItemsSelect[0] = new MenuItemSelect(L("Algorithm"), L("Change the way adaptive video works."));
   m_pItemsSelect[0]->addSelection(L("Default"));
   m_pItemsSelect[0]->addSelection(L("New"));
   m_pItemsSelect[0]->setIsEditable();
   m_pItemsSelect[0]->setMargin(dxMargin);
   m_IndexAdaptiveAlgorithm = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSlider[0] = new MenuItemSlider(L("Auto Adjustment Strength"), L("How aggressive should the auto video link adjustments be (adaptive video and auto keyframe). 1 is the slowest adjustment strength, 10 is the fastest and most aggressive adjustment strength."), 1,10,5, fSliderWidth);
   m_pItemsSlider[0]->setStep(1);
   m_pItemsSlider[0]->setMargin(dxMargin);
   m_IndexVideoAdjustStrength = addMenuItem(m_pItemsSlider[0]);

   m_IndexAdaptiveUseControllerToo = -1;
   /*
   m_pItemsSelect[9] = new MenuItemSelect(L("Use Controller Feedback"), L("When vehicle adjusts video link params, use the feedback from the controller too in deciding the best video link params to be used."));  
   m_pItemsSelect[9]->addSelection(L("No"));
   m_pItemsSelect[9]->addSelection(L("Yes"));
   m_pItemsSelect[9]->setIsEditable();
   m_pItemsSelect[9]->setMargin(dxMargin);
   m_IndexAdaptiveUseControllerToo = addMenuItem(m_pItemsSelect[9]);
   */

   m_IndexVideoLinkLost = -1;
   /*
   m_pItemsSelect[10] = new MenuItemSelect(L("Lower Quality On Link Lost"), L("When vehicle looses connection from the controller, go to a lower video quality and lower radio datarate."));  
   m_pItemsSelect[10]->addSelection(L("No"));
   m_pItemsSelect[10]->addSelection(L("Yes"));
   m_pItemsSelect[10]->setIsEditable();
   m_pItemsSelect[10]->setMargin(dxMargin);
   m_IndexVideoLinkLost = addMenuItem(m_pItemsSelect[10]);
   */
}

MenuVehicleVideoBidirectional::~MenuVehicleVideoBidirectional()
{
}

void MenuVehicleVideoBidirectional::valuesToUI()
{ 
   int adaptiveVideo = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK)?1:0;
   //int adaptiveKeyframe = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME)?1:0;
   int useControllerInfo = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO)?1:0;
   int controllerLinkLost = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST)?1:0;
   
   if ( hardware_board_is_goke(g_pCurrentModel->hwCapabilities.uBoardType) )
      adaptiveVideo = 0;

   m_pItemsSelect[0]->setSelection( (g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_NEW_ADAPTIVE_ALGORITHM)?1:0);
   m_pItemsSelect[0]->setEnabled(false);

   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS )
   {
      m_pItemsSelect[4]->setSelectedIndex(1);
      m_pItemsSelect[6]->setEnabled(true);
   }
   else
   {
      m_pItemsSelect[4]->setSelectedIndex(0);
      m_pItemsSelect[6]->setEnabled(false);
   }
   m_pItemsSelect[6]->setSelectedIndex((g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_RETRANSMISSIONS_FAST)?1:0);

   if ( adaptiveVideo )
   {
      m_pItemsSelect[2]->setSelectedIndex(1);
      m_pItemsSelect[5]->setEnabled(true);
      m_pItemsSlider[0]->setEnabled(true);
      if ( -1 != m_IndexAdaptiveUseControllerToo )
         m_pItemsSelect[9]->setEnabled(true);
      if ( -1 != m_IndexVideoLinkLost )
         m_pItemsSelect[10]->setEnabled(true);

      if ( (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO )
         m_pItemsSelect[5]->setSelectedIndex(0);
      else
         m_pItemsSelect[5]->setSelectedIndex(1);

      m_pItemsSlider[0]->setCurrentValue(g_pCurrentModel->video_params.videoAdjustmentStrength);
      if ( -1 != m_IndexAdaptiveUseControllerToo )
         m_pItemsSelect[9]->setSelectedIndex(useControllerInfo);
      if ( -1 != m_IndexVideoLinkLost )
         m_pItemsSelect[10]->setSelectedIndex(controllerLinkLost);
   }
   else
   {
      m_pItemsSelect[2]->setSelectedIndex(0);
      m_pItemsSelect[5]->setEnabled(false);
      m_pItemsSlider[0]->setEnabled(false);
      if ( -1 != m_IndexAdaptiveUseControllerToo )
         m_pItemsSelect[9]->setEnabled(false);
      if ( -1 != m_IndexVideoLinkLost )
         m_pItemsSelect[10]->setEnabled(false);
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

void MenuVehicleVideoBidirectional::sendVideoLinkProfiles()
{
   if ( get_sw_version_build(g_pCurrentModel) < 283 )
   {
      addMessage(L("Video functionality has changed. You need to update your vehicle sowftware."));
      return;
   }
   type_video_link_profile profiles[MAX_VIDEO_LINK_PROFILES];
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      memcpy(&profiles[i], &(g_pCurrentModel->video_link_profiles[i]), sizeof(type_video_link_profile));

   type_video_link_profile* pProfile = &(profiles[g_pCurrentModel->video_params.user_selected_video_link_profile]);
   int keyframe_ms = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].keyframe_ms;
   if ( keyframe_ms < 0 )
      keyframe_ms = -keyframe_ms;
   pProfile->keyframe_ms = keyframe_ms;

   if ( 0 == m_pItemsSelect[2]->getSelectedIndex() )  
      pProfile->uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK;
   else
      pProfile->uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK;

   if ( 0 == m_pItemsSelect[4]->getSelectedIndex() )
      pProfile->uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS;
   else
      pProfile->uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS;

   if ( -1 != m_IndexAdaptiveUseControllerToo )
   {
      if ( 0 == m_pItemsSelect[9]->getSelectedIndex() )
         pProfile->uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO;
      else
         pProfile->uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO;
   }

   if ( -1 != m_IndexVideoLinkLost )
   {
      if ( 0 == m_pItemsSelect[10]->getSelectedIndex() )
         pProfile->uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST;
      else
         pProfile->uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST;
   }
   if ( 1 == m_pItemsSelect[2]->getSelectedIndex() )  
   {
      if ( 0 == m_pItemsSelect[5]->getSelectedIndex() )
      {
         pProfile->uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK;
         pProfile->uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO;
      }
      else
      {
         pProfile->uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK;
         pProfile->uProfileEncodingFlags &= (~VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO);
      }
   }

   if ( pProfile->uProfileEncodingFlags == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags )
   if ( pProfile->keyframe_ms == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].keyframe_ms )
      return;

   // Propagate changes to lower video profiles

   propagate_video_profile_changes_to_lower_profiles(g_pCurrentModel, &g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile], pProfile, &(profiles[0]));

   log_line("Sending video encoding flags: %s", str_format_video_encoding_flags(pProfile->uProfileEncodingFlags));
      
   u8 buffer[1024];
   memcpy( buffer, &(profiles[0]), MAX_VIDEO_LINK_PROFILES * sizeof(type_video_link_profile) );

   log_line("Sending new video link profiles to vehicle.");
   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_UPDATE_VIDEO_LINK_PROFILES, 0, buffer, MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile)) )
      valuesToUI();
   else
   {
      if ( modelvideoLinkProfileIsOnlyVideoKeyframeChanged(&(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile]), pProfile) )
         send_control_message_to_router(PACEKT_TYPE_LOCAL_CONTROLLER_ADAPTIVE_VIDEO_PAUSE, 500);
      else
         send_control_message_to_router(PACEKT_TYPE_LOCAL_CONTROLLER_ADAPTIVE_VIDEO_PAUSE, 7000);
   }
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

   g_TimeLastVideoCameraChangeCommand = g_TimeNow;

   if ( m_IndexAdaptiveAlgorithm == m_SelectedIndex )
   {
      if ( get_sw_version_build(g_pCurrentModel) < 283 )
      {
         addMessage(L("Video functionality has changed. You need to update your vehicle sowftware."));
         return;
      }
      video_parameters_t paramsNew;
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      paramsNew.uVideoExtraFlags &= ~VIDEO_FLAG_NEW_ADAPTIVE_ALGORITHM;
      if ( 1 == m_pItemsSelect[0]->getSelectedIndex() )
         paramsNew.uVideoExtraFlags |= VIDEO_FLAG_NEW_ADAPTIVE_ALGORITHM;

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMS, 0, (u8*)&paramsNew, sizeof(video_parameters_t)) )
         valuesToUI();    
   }
   
   if ( m_IndexAdaptiveVideo == m_SelectedIndex )
   if ( hardware_board_is_goke(g_pCurrentModel->hwCapabilities.uBoardType) )
   {
      addUnsupportedMessageOpenIPCGoke(NULL);
      valuesToUI();
      return;
   }

   if ( m_IndexRetransmissions == m_SelectedIndex || 
        ((-1 != m_IndexVideoLinkLost) && (m_IndexVideoLinkLost == m_SelectedIndex)) )
   {
      sendVideoLinkProfiles();
      return;
   }

   if ( m_IndexRetransmissionsFast == m_SelectedIndex )
   {
      if ( get_sw_version_build(g_pCurrentModel) < 283 )
      {
         addMessage(L("Video functionality has changed. You need to update your vehicle sowftware."));
         return;
      }
      video_parameters_t paramsNew;
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      paramsNew.uVideoExtraFlags &= ~VIDEO_FLAG_RETRANSMISSIONS_FAST;
      if ( 1 == m_pItemsSelect[6]->getSelectedIndex() )
         paramsNew.uVideoExtraFlags |= VIDEO_FLAG_RETRANSMISSIONS_FAST;

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMS, 0, (u8*)&paramsNew, sizeof(video_parameters_t)) )
         valuesToUI();
      return;
   }


   if ( m_IndexAdaptiveVideo == m_SelectedIndex || m_IndexAdaptiveVideoLevel == m_SelectedIndex ||
      ((-1 != m_IndexAdaptiveUseControllerToo) && (m_IndexAdaptiveUseControllerToo == m_SelectedIndex)) )
   {
      sendVideoLinkProfiles();
      return;
   }

   if ( m_IndexVideoAdjustStrength == m_SelectedIndex )
   {
      if ( get_sw_version_build(g_pCurrentModel) < 283 )
      {
         addMessage(L("Video functionality has changed. You need to update your vehicle sowftware."));
         return;
      }
      video_parameters_t paramsNew;
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      paramsNew.videoAdjustmentStrength = m_pItemsSlider[0]->getCurrentValue();

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMS, 0, (u8*)&paramsNew, sizeof(video_parameters_t)) )
         valuesToUI();
      return;
   }
}
