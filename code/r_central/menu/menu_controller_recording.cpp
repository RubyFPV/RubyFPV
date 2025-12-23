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
#include "menu_controller_recording.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "../popup_log.h"
#include "../osd/osd_common.h"
#include "../ruby_central.h"

MenuControllerRecording::MenuControllerRecording(void)
:Menu(MENU_ID_CONTROLLER_RECORDING, L("Video Recording Settings"), NULL)
{
   m_Width = 0.34;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.12;
   float dxMargin = 0.03;

   m_pItemsSelect[1] = new MenuItemSelect(L("Recording buffers"), L("When recording, record directly to the persistent storage or to memory first. Record to memory first when having slow performance on the persistent storage. Either way, when video recording ends, the video is saved to persistent storage."));  
   m_pItemsSelect[1]->addSelection(L("Storage"));
   m_pItemsSelect[1]->addSelection(L("Memory"));
   m_pItemsSelect[1]->setIsEditable();
   m_IndexVideoDestination = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSelect[2] = new MenuItemSelect(L("Record Indicator Style"), L("Select which style of record indicator to show on the OSD"));  
   m_pItemsSelect[2]->addSelection(L("Normal"));
   m_pItemsSelect[2]->addSelection(L("Large"));
   m_pItemsSelect[2]->setIsEditable();
   m_IndexRecordIndicator = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSelect[3] = new MenuItemSelect(L("Recording LED"), L("Select what the recording LED should do when recording is on."));  
   m_pItemsSelect[3]->addSelection(L("Disabled"));
   m_pItemsSelect[3]->addSelection(L("On/Off"));
   m_pItemsSelect[3]->addSelection(L("Blinking"));
   m_pItemsSelect[3]->setIsEditable();
   m_IndexRecordLED = addMenuItem(m_pItemsSelect[3]);

   addMenuItem(new MenuItemSection(L("Recording Triggers")));

   m_pItemsSelect[4] = new MenuItemSelect(L("Record Button"), L("Select which button to use to trigger start/stop of video recording."));  
   m_pItemsSelect[4]->addSelection(L("None"));
   m_pItemsSelect[4]->addSelection(L("QA Button 1"));
   m_pItemsSelect[4]->addSelection(L("QA Button 2"));
   m_pItemsSelect[4]->addSelection(L("QA Button 3"));
   m_pItemsSelect[4]->setIsEditable();
   m_IndexRecordButton = addMenuItem(m_pItemsSelect[4]);

   m_pItemsSelect[5] = new MenuItemSelect(L("Start recording on Arm"), L("Starts video recording (if it's not started already) when the vehicle arms. It requires an active connection of the vehicle to the flight controller."));
   m_pItemsSelect[5]->addSelection(L("No"));
   m_pItemsSelect[5]->addSelection(L("Yes"));
   m_pItemsSelect[5]->setIsEditable();
   m_IndexRecordArm = addMenuItem(m_pItemsSelect[5]);

   m_pItemsSelect[6] = new MenuItemSelect(L("Stop recording on Disarm"), L("Stops video recording (if one is started) when the vehicle disarms. It requires an active connection of the vehicle to the flight controller."));  
   m_pItemsSelect[6]->addSelection(L("No"));
   m_pItemsSelect[6]->addSelection(L("Yes"));
   m_pItemsSelect[6]->setIsEditable();
   m_IndexRecordDisarm = addMenuItem(m_pItemsSelect[6]);

   m_pItemsSelect[7] = new MenuItemSelect(L("Stop recording on link lost"), L("Stops video recording (if one is started) when the radio link is lost, after a timeout."));
   m_pItemsSelect[7]->addSelection(L("No"));
   m_pItemsSelect[7]->addSelection(L("Yes"));
   m_pItemsSelect[7]->setIsEditable();
   m_iIndexStopOnLinkLost = addMenuItem(m_pItemsSelect[7]);

   float fSliderWidth = 0.12;

   m_pItemsSlider[0] = new MenuItemSlider(L("Stop Delay (seconds)"), L("Sets a delay to automatically stop recording if link is lost for too long."), 1,100,20, fSliderWidth);
   m_iIndexStopOnLinkLostTime = addMenuItem(m_pItemsSlider[0]);
   m_pMenuItems[m_iIndexStopOnLinkLostTime]->setMargin(dxMargin);

   addMenuItem(new MenuItemSection(L("Recording Data")));

   m_pItemsSelect[10] = new MenuItemSelect(L("Record also .OSD data file"), L("Enables or disables recording of a separate file with OSD info."));
   m_pItemsSelect[10]->addSelection(L("Disabled"));
   m_pItemsSelect[10]->addSelection(L("Enabled"));
   m_pItemsSelect[10]->setIsEditable();
   m_iIndexRecordOSD = addMenuItem(m_pItemsSelect[10]);

   m_pItemsSelect[11] = new MenuItemSelect(L("Record also .STR data file"), L("Enables or disables recording of a separate file with subtitles info."));
   m_pItemsSelect[11]->addSelection(L("Disabled"));
   m_pItemsSelect[11]->addSelection(L("Enabled"));
   m_pItemsSelect[11]->setIsEditable();
   m_iIndexRecordSTR = addMenuItem(m_pItemsSelect[11]);

   m_pItemsSelect[12] = new MenuItemSelect(L("STR Refresh Time"), L("Sets how fast the STR info refreshes."));
   m_pItemsSelect[12]->addSelection(L("50ms"));
   m_pItemsSelect[12]->addSelection(L("100ms"));
   m_pItemsSelect[12]->addSelection(L("200ms"));
   m_pItemsSelect[12]->addSelection(L("500ms"));
   m_pItemsSelect[12]->setIsEditable();
   m_iIndexSTRFramerate = addMenuItem(m_pItemsSelect[12]);
   m_pMenuItems[m_iIndexSTRFramerate]->setMargin(dxMargin);

   m_pItemsSelect[13] = new MenuItemSelect(L("Record Flight Time"), L("Include flight time into the STR recording."));
   m_pItemsSelect[13]->addSelection(L("No"));
   m_pItemsSelect[13]->addSelection(L("Yes"));
   m_pItemsSelect[13]->setIsEditable();
   m_iIndexSTRTime = addMenuItem(m_pItemsSelect[13]);
   m_pMenuItems[m_iIndexSTRTime]->setMargin(0.03);

   m_pItemsSelect[14] = new MenuItemSelect(L("Record Home Distance"), L("Include home distance into the STR recording."));
   m_pItemsSelect[14]->addSelection(L("No"));
   m_pItemsSelect[14]->addSelection(L("Yes"));
   m_pItemsSelect[14]->setIsEditable();
   m_iIndexSTRHome = addMenuItem(m_pItemsSelect[14]);
   m_pMenuItems[m_iIndexSTRHome]->setMargin(dxMargin);

   m_pItemsSelect[16] = new MenuItemSelect(L("Record Altitude"), L("Include altitude into the STR recording."));
   m_pItemsSelect[16]->addSelection(L("No"));
   m_pItemsSelect[16]->addSelection(L("Yes"));
   m_pItemsSelect[16]->setIsEditable();
   m_iIndexSTRAlt = addMenuItem(m_pItemsSelect[16]);
   m_pMenuItems[m_iIndexSTRAlt]->setMargin(dxMargin);

   m_pItemsSelect[15] = new MenuItemSelect(L("Record GPS Position"), L("Include GPS position into the STR recording."));
   m_pItemsSelect[15]->addSelection(L("No"));
   m_pItemsSelect[15]->addSelection(L("Yes"));
   m_pItemsSelect[15]->setIsEditable();
   m_iIndexSTRGPS = addMenuItem(m_pItemsSelect[15]);
   m_pMenuItems[m_iIndexSTRGPS]->setMargin(dxMargin);

   m_pItemsSelect[18] = new MenuItemSelect(L("Record Battery Voltage"), L("Include vehicle baterry voltage into the STR recording."));
   m_pItemsSelect[18]->addSelection(L("No"));
   m_pItemsSelect[18]->addSelection(L("Yes"));
   m_pItemsSelect[18]->setIsEditable();
   m_iIndexSTRVoltage = addMenuItem(m_pItemsSelect[18]);
   m_pMenuItems[m_iIndexSTRVoltage]->setMargin(dxMargin);

   m_pItemsSelect[17] = new MenuItemSelect(L("Record Radio RSSI/SNR"), L("Include radio RSSI and SNR signal values into the STR recording."));
   m_pItemsSelect[17]->addSelection(L("No"));
   m_pItemsSelect[17]->addSelection(L("Yes"));
   m_pItemsSelect[17]->setIsEditable();
   m_iIndexSTRRSSI = addMenuItem(m_pItemsSelect[17]);
   m_pMenuItems[m_iIndexSTRRSSI]->setMargin(dxMargin);

   m_pItemsSelect[19] = new MenuItemSelect(L("Record Video Bitrate"), L("Include video bitrate into the STR recording."));
   m_pItemsSelect[19]->addSelection(L("No"));
   m_pItemsSelect[19]->addSelection(L("Yes"));
   m_pItemsSelect[19]->setIsEditable();
   m_iIndexSTRBitrate = addMenuItem(m_pItemsSelect[19]);
   m_pMenuItems[m_iIndexSTRBitrate]->setMargin(dxMargin);

   addMenuItem(new MenuItemSection(L("Screenshots")));

   m_pItemsSelect[0] = new MenuItemSelect(L("Add OSD To Screenshots"), L("When taking a screenshot, OSD info can be included in the picture or not."));  
   m_pItemsSelect[0]->addSelection(L("No"));
   m_pItemsSelect[0]->addSelection(L("Yes"));
   m_pItemsSelect[0]->setIsEditable();
   m_iIndexAddOSDToScreenshots = addMenuItem(m_pItemsSelect[0]);
}

void MenuControllerRecording::valuesToUI()
{
   Preferences* p = get_Preferences();
   if ( NULL == p )
   {
      log_softerror_and_alarm("Failed to get pointer to preferences structure");
      return;
   }

   m_pItemsSelect[0]->setSelection(p->iAddOSDOnScreenshots);
   //m_pItemsSelect[0]->setEnabled((p->iActionQuickButton1==quickActionTakePicture) || (p->iActionQuickButton2==quickActionTakePicture));

   m_pItemsSelect[1]->setSelection(p->iVideoDestination);
  
   m_pItemsSelect[2]->setSelection(p->iShowBigRecordButton);
   m_pItemsSelect[3]->setSelection(p->iRecordingLedAction);

   m_pItemsSelect[4]->setSelection(0);
   if ( p->iActionQuickButton1 == quickActionVideoRecord )
      m_pItemsSelect[4]->setSelection(1);
   if ( p->iActionQuickButton2 == quickActionVideoRecord )
      m_pItemsSelect[4]->setSelection(2);
   if ( p->iActionQuickButton3 == quickActionVideoRecord )
      m_pItemsSelect[4]->setSelection(3);

   m_pItemsSelect[5]->setSelection(p->iStartVideoRecOnArm);
   m_pItemsSelect[6]->setSelection(p->iStopVideoRecOnDisarm);

   if ( p->iStopRecordingAfterLinkLostSeconds <= 0 )
   {
      m_pItemsSelect[7]->setSelectedIndex(0);
      m_pItemsSlider[0]->setEnabled(false);
   }
   else
   {
      m_pItemsSelect[7]->setSelectedIndex(1);
      m_pItemsSlider[0]->setEnabled(true);
      m_pItemsSlider[0]->setCurrentValue(p->iStopRecordingAfterLinkLostSeconds);
   }


   m_pItemsSelect[10]->setSelectedIndex(g_pControllerSettings->iRecordOSD);
   m_pItemsSelect[11]->setSelectedIndex(g_pControllerSettings->iRecordSTR);
   for( int i=12; i<20; i++ )
      m_pItemsSelect[i]->setEnabled(g_pControllerSettings->iRecordSTR);

   m_pItemsSelect[12]->setSelectedIndex(g_pControllerSettings->iRecordSTRFramerate);
   m_pItemsSelect[13]->setSelectedIndex(g_pControllerSettings->iRecordSTRTime);
   m_pItemsSelect[14]->setSelectedIndex(g_pControllerSettings->iRecordSTRHome);
   m_pItemsSelect[15]->setSelectedIndex(g_pControllerSettings->iRecordSTRGPS);
   m_pItemsSelect[16]->setSelectedIndex(g_pControllerSettings->iRecordSTRAlt);
   m_pItemsSelect[17]->setSelectedIndex(g_pControllerSettings->iRecordSTRRSSI);
   m_pItemsSelect[18]->setSelectedIndex(g_pControllerSettings->iRecordSTRVoltage);
   m_pItemsSelect[19]->setSelectedIndex(g_pControllerSettings->iRecordSTRBitrate);
}

void MenuControllerRecording::onShow()
{
   removeAllTopLines();
   Menu::onShow();
}

void MenuControllerRecording::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuControllerRecording::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   Preferences* p = get_Preferences();
   if ( NULL == p )
   {
      log_softerror_and_alarm("Failed to get pointer to preferences structure");
      return;
   }

   if ( m_iIndexAddOSDToScreenshots == m_SelectedIndex )
      p->iAddOSDOnScreenshots = m_pItemsSelect[0]->getSelectedIndex();

   if ( 1 == m_SelectedIndex )
   {
      p->iVideoDestination = m_pItemsSelect[1]->getSelectedIndex();
      save_Preferences();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_PREFERENCES_UPDATED, 0);
      return;
   }

   if ( m_IndexRecordIndicator == m_SelectedIndex )
      p->iShowBigRecordButton = m_pItemsSelect[2]->getSelectedIndex();

   if ( m_IndexRecordLED == m_SelectedIndex )
      p->iRecordingLedAction = m_pItemsSelect[3]->getSelectedIndex();

   if ( m_IndexRecordButton == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[4]->getSelectedIndex() )
      {
         if ( p->iActionQuickButton1 == quickActionVideoRecord )
            p->iActionQuickButton1 = quickActionTakePicture;
         if ( p->iActionQuickButton2 == quickActionVideoRecord )
            p->iActionQuickButton2 = quickActionTakePicture;
         if ( p->iActionQuickButton3 == quickActionVideoRecord )
            p->iActionQuickButton3 = quickActionTakePicture;
      }
      if ( 1 == m_pItemsSelect[4]->getSelectedIndex() )
      {
         p->iActionQuickButton1 = quickActionVideoRecord;
         if ( p->iActionQuickButton2 == quickActionVideoRecord )
            p->iActionQuickButton2 = quickActionTakePicture;
         if ( p->iActionQuickButton3 == quickActionVideoRecord )
            p->iActionQuickButton3 = quickActionTakePicture;
      }
      if ( 2 == m_pItemsSelect[4]->getSelectedIndex() )
      {
         p->iActionQuickButton2 = quickActionVideoRecord;
         if ( p->iActionQuickButton1 == quickActionVideoRecord )
            p->iActionQuickButton1 = quickActionTakePicture;
         if ( p->iActionQuickButton3 == quickActionVideoRecord )
            p->iActionQuickButton3 = quickActionTakePicture;
      }
      if ( 3 == m_pItemsSelect[4]->getSelectedIndex() )
      {
         p->iActionQuickButton3 = quickActionVideoRecord;
         if ( p->iActionQuickButton1 == quickActionVideoRecord )
            p->iActionQuickButton1 = quickActionTakePicture;
         if ( p->iActionQuickButton2 == quickActionVideoRecord )
            p->iActionQuickButton2 = quickActionTakePicture;
      }
   }

   if ( m_IndexRecordArm == m_SelectedIndex )
   {
      p->iStartVideoRecOnArm =  m_pItemsSelect[5]->getSelectedIndex();
      save_Preferences();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_PREFERENCES_UPDATED, 0);
      return;
   }

   if ( m_IndexRecordDisarm == m_SelectedIndex )
   {
      p->iStopVideoRecOnDisarm =  m_pItemsSelect[6]->getSelectedIndex();
      save_Preferences();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_PREFERENCES_UPDATED, 0);
      return;
   }

   if ( m_iIndexStopOnLinkLost == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[7]->getSelectedIndex() )
         p->iStopRecordingAfterLinkLostSeconds = 0;
      else
         p->iStopRecordingAfterLinkLostSeconds = 10;
      save_Preferences();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_PREFERENCES_UPDATED, 0);
      return;
   }
   if ( m_iIndexStopOnLinkLostTime == m_SelectedIndex )
   {
      p->iStopRecordingAfterLinkLostSeconds = m_pItemsSlider[0]->getCurrentValue();
      save_Preferences();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_PREFERENCES_UPDATED, 0);
      return;
   }

   if ( m_iIndexRecordOSD == m_SelectedIndex )
   {
      g_pControllerSettings->iRecordOSD = m_pItemsSelect[10]->getSelectedIndex();
      save_ControllerSettings();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);

      if ( (NULL == g_pCurrentModel) || (! g_bFirstModelPairingDone) || (g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
      {
         if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MSP) )
            addMessage(L("Current vehicle does not have MSPOSD telemetry enabled. OSD recording file works only when vehicle telemetry type is MSPOSD."));
         else
            addMessage(L("OSD recording file works only when vehicle telemetry type is MSPOSD."));
      }
   }

   if ( m_iIndexRecordSTR == m_SelectedIndex )
   {
      g_pControllerSettings->iRecordSTR = m_pItemsSelect[11]->getSelectedIndex();
      save_ControllerSettings();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      if ( ruby_is_recording() )
         addMessage2(0, L("A recording is already in progress."), L("The new recoding options will take effect on the next recording."));
   }

   if ( m_iIndexSTRFramerate == m_SelectedIndex )
   {
      g_pControllerSettings->iRecordSTRFramerate = m_pItemsSelect[12]->getSelectedIndex();
      save_ControllerSettings();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      if ( ruby_is_recording() )
         addMessage2(0, L("A recording is already in progress."), L("The new recoding options will take effect on the next recording."));
   }
   if ( (m_iIndexSTRTime == m_SelectedIndex) ||
        (m_iIndexSTRHome == m_SelectedIndex) ||
        (m_iIndexSTRGPS == m_SelectedIndex) ||
        (m_iIndexSTRAlt == m_SelectedIndex) ||
        (m_iIndexSTRRSSI == m_SelectedIndex) ||
        (m_iIndexSTRVoltage == m_SelectedIndex) ||
        (m_iIndexSTRBitrate == m_SelectedIndex) )
   {
      g_pControllerSettings->iRecordSTRTime = m_pItemsSelect[13]->getSelectedIndex();
      g_pControllerSettings->iRecordSTRHome = m_pItemsSelect[14]->getSelectedIndex();
      g_pControllerSettings->iRecordSTRGPS = m_pItemsSelect[15]->getSelectedIndex();
      g_pControllerSettings->iRecordSTRAlt = m_pItemsSelect[16]->getSelectedIndex();
      g_pControllerSettings->iRecordSTRRSSI = m_pItemsSelect[17]->getSelectedIndex();
      g_pControllerSettings->iRecordSTRVoltage = m_pItemsSelect[18]->getSelectedIndex();
      g_pControllerSettings->iRecordSTRBitrate = m_pItemsSelect[19]->getSelectedIndex();
      save_ControllerSettings();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);

      int iNewValue = 0;
      if ( m_iIndexSTRHome == m_SelectedIndex )
         iNewValue = m_pItemsSelect[14]->getSelectedIndex();
      if ( m_iIndexSTRAlt == m_SelectedIndex )
         iNewValue = m_pItemsSelect[16]->getSelectedIndex();
      if ( m_iIndexSTRGPS == m_SelectedIndex )
         iNewValue = m_pItemsSelect[15]->getSelectedIndex();
      if ( m_iIndexSTRVoltage == m_SelectedIndex )
         iNewValue = m_pItemsSelect[18]->getSelectedIndex();

      if ( (m_iIndexSTRHome == m_SelectedIndex) ||
        (m_iIndexSTRGPS == m_SelectedIndex) ||
        (m_iIndexSTRAlt == m_SelectedIndex) ||
        (m_iIndexSTRVoltage == m_SelectedIndex) )
      if ( iNewValue == 1 )
      if ( (NULL == g_pCurrentModel) || ( ! g_bFirstModelPairingDone) || ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_MAVLINK) )
         addMessage("This parameter is recorded only when vehicle's telemtry type is MAVLink.");

      if ( ruby_is_recording() )
         addMessage2(0, L("A recording is already in progress."), L("The new recoding options will take effect on the next recording."));
   }
   save_Preferences();
}
