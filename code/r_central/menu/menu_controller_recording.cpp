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

MenuControllerRecording::MenuControllerRecording(void)
:Menu(MENU_ID_CONTROLLER_RECORDING, L("Video Recording Settings"), NULL)
{
   m_Width = 0.34;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.12;

   int c = 0;

   m_pItemsSelect[c] = new MenuItemSelect(L("Add OSD To Screenshots"), L("When taking a screenshot, OSD info can be included in the picture or not."));  
   m_pItemsSelect[c]->addSelection(L("No"));
   m_pItemsSelect[c]->addSelection(L("Yes"));
   m_pItemsSelect[c]->setIsEditable();
   addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect(L("Recording buffers"), L("When recording, record directly to the persistent storage or to memory first. Record to memory first when having slow performance on the persistent storage. Either way, when video recording ends, the video is saved to persistent storage."));  
   m_pItemsSelect[c]->addSelection(L("Storage"));
   m_pItemsSelect[c]->addSelection(L("Memory"));
   m_pItemsSelect[c]->setIsEditable();
   m_IndexVideoDestination = addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect(L("Record Indicator Style"), L("Select which style of record indicator to show on the OSD"));  
   m_pItemsSelect[c]->addSelection(L("Normal"));
   m_pItemsSelect[c]->addSelection(L("Large"));
   m_pItemsSelect[c]->setIsEditable();
   m_IndexRecordIndicator = addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect(L("Recording LED"), L("Select what the recording LED should do when recording is on."));  
   m_pItemsSelect[c]->addSelection(L("Disabled"));
   m_pItemsSelect[c]->addSelection(L("On/Off"));
   m_pItemsSelect[c]->addSelection(L("Blinking"));
   m_pItemsSelect[c]->setIsEditable();
   m_IndexRecordLED = addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect(L("Record Button"), L("Select which button to use to trigger start/stop of video recording."));  
   m_pItemsSelect[c]->addSelection(L("None"));
   m_pItemsSelect[c]->addSelection(L("QA Button 1"));
   m_pItemsSelect[c]->addSelection(L("QA Button 2"));
   m_pItemsSelect[c]->addSelection(L("QA Button 3"));
   m_pItemsSelect[c]->setIsEditable();
   m_IndexRecordButton = addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect(L("Start recording on Arm"), L("Starts video recording (if it's not started already) when the vehicle arms. It requires an active connection of the vehicle to the flight controller."));
   m_pItemsSelect[c]->addSelection(L("No"));
   m_pItemsSelect[c]->addSelection(L("Yes"));
   m_pItemsSelect[c]->setIsEditable();
   m_IndexRecordArm = addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect(L("Stop recording on Disarm"), L("Stops video recording (if one is started) when the vehicle disarms. It requires an active connection of the vehicle to the flight controller."));  
   m_pItemsSelect[c]->addSelection(L("No"));
   m_pItemsSelect[c]->addSelection(L("Yes"));
   m_pItemsSelect[c]->setIsEditable();
   m_IndexRecordDisarm = addMenuItem(m_pItemsSelect[c]);
   c++;

   m_pItemsSelect[c] = new MenuItemSelect(L("Stop recording on link lost"), L("Stops video recording (if one is started) when the radio link is lost, after a timeout."));
   m_pItemsSelect[c]->addSelection(L("No"));
   m_pItemsSelect[c]->addSelection(L("Yes"));
   m_pItemsSelect[c]->setIsEditable();
   m_iIndexStopOnLinkLost = addMenuItem(m_pItemsSelect[c]);

   float fSliderWidth = 0.12;

   m_pItemsSlider[0] = new MenuItemSlider(L("Delay (seconds)"), L("Sets a delay to automatically stop recording if link is lost for too long."), 1,100,20, fSliderWidth);

   m_iIndexStopOnLinkLostTime = addMenuItem(m_pItemsSlider[0]);
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

   if ( 0 == m_SelectedIndex )
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
   save_Preferences();
}
