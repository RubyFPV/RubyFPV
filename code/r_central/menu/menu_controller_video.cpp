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
#include "menu_objects.h"
#include "menu_controller_video.h"
#include "menu_text.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_confirmation.h"
#include "menu_calibrate_hdmi.h"

#include "../../base/hdmi.h"
#include "../../base/hardware_files.h"
#include "../../base/hardware_audio.h"

#include "../process_router_messages.h"
#include "../pairing.h"
#if defined (HW_PLATFORM_RADXA)
#include "../../renderer/drm_core.h"
#include "../../renderer/render_engine_cairo.h"
#endif
#include <time.h>
#include <sys/resource.h>
#include <pthread.h>

const char* s_szHDMIInfo = "Note: Changing the HDMI options requires a restart of the controller for the changes to take effect.";
pthread_t s_pThreadAudioTest;
bool s_bStopAudioTest = false;

void* _thread_audio_test_async(void *argument)
{
   log_line("[BGThreadAudio] Started audio thread test.");
   hw_log_current_thread_attributes("audio test");
   srand(get_current_timestamp_ms());

   if ( pairing_isStarted() )
   {
      log_line("[BGThreadAudio] Waiting for vehicle audio to stop...");
      char szPIDs[256];
      hw_process_get_pids("aplay", szPIDs);
      removeTrailingNewLines(szPIDs);
      u32 uTimeStart = get_current_timestamp_ms();
      while ( (strlen(szPIDs) > 2) && (! s_bStopAudioTest) )
      {
         if ( get_current_timestamp_ms() > uTimeStart + 3000 )
            break;
         hardware_sleep_ms(50);
         hw_process_get_pids("aplay", szPIDs);
         removeTrailingNewLines(szPIDs);
      }
      log_line("[BGThreadAudio] Stopped vehicle audio.");
   }

   while ( ! s_bStopAudioTest )
   {
      int iSample = 1 + (rand()%3);
      char szFile[MAX_FILE_PATH_SIZE];
      sprintf(szFile, "res/sample%d.wav", iSample);
      log_line("[BGThreadAudio] Playing audio file: (%s) ...", szFile);
      hardware_audio_play_file(szFile);
   }
   log_line("[BGThreadAudio] Ended audio thread test.");
   return NULL;
}

MenuControllerVideo::MenuControllerVideo(void)
:Menu(MENU_ID_CONTROLLER_VIDEO, L("Audio & Video Output Settings"), NULL)
{
   m_Width = 0.36;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.25;
   float fSliderWidth = 0.12 * m_sfScaleFactor;
   
   char szBuff[64];

   snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), L("Current real display resolution: %dx%d"),
       g_pRenderEngine->getScreenWidth(), g_pRenderEngine->getScreenHeight() );
   addTopLine(szBuff);

   m_hdmigroupOrg = hdmi_get_current_resolution_group();
   m_hdmimodeOrg = hdmi_get_current_resolution_mode();
   log_line("Current HDMI resolution: group: %d, mode: %d", m_hdmigroupOrg, m_hdmimodeOrg);
   addMenuItem(new MenuItemSection(L("HDMI Output")));

   m_pItemsSelect[0] = new MenuItemSelect(L("HDMI output resolution"), L("Sets the HDMI resolution on the controller display."));
   for( int i=0; i<hdmi_get_resolutions_count(); i++ )
   {
      sprintf(szBuff, "%d x %d", hdmi_get_resolution_width(i), hdmi_get_resolution_height(i));
      m_pItemsSelect[0]->addSelection(szBuff);
   }
   m_pItemsSelect[0]->setIsEditable();
   m_IndexHDMIRes = addMenuItem(m_pItemsSelect[0]);
   
   m_pItemsSelect[3] = new MenuItemSelect(L("HDMI refresh rate"), L("Sets the HDMI refresh rate for the display."));
   m_pItemsSelect[3]->setIsEditable();
   m_IndexHDMIRefreshRate = addMenuItem(m_pItemsSelect[3]);

   m_IndexHDMIBoost = -1;
   m_IndexHDMIOverscan = -1;
   m_IndexHDMIVSync = -1;

   #if defined (HW_PLATFORM_RASPBERRY)
   m_pItemsSlider[0] = new MenuItemSlider(L("HDMI Output Boost"), L("Sets the boost voltage level for the HDMI output."), 0,11,6, 0.15);
   m_IndexHDMIBoost = addMenuItem(m_pItemsSlider[0]);

   addMenuItem(new MenuItemText(s_szHDMIInfo, true));

   m_pItemsSelect[2] = new MenuItemSelect(L("HDMI Overscan"), L("Default overscan (margins) on HDMI output. Requires a reboot after changing this value."));
   m_pItemsSelect[2]->addSelection(L("Enabled"));
   m_pItemsSelect[2]->addSelection(L("Disabled"));
   m_pItemsSelect[2]->setIsEditable();
   m_IndexHDMIOverscan = addMenuItem(m_pItemsSelect[2]);
   #endif

   #if defined (HW_PLATFORM_RADXA)
   m_pItemsSelect[6] = new MenuItemSelect(L("HDMI Vertical Sync"), L("Enable vertical sync renderging synchronization on the HDMI display."));
   m_pItemsSelect[6]->addSelection(L("Disabled"));
   m_pItemsSelect[6]->addSelection(L("Enabled"));
   m_pItemsSelect[6]->setIsEditable();
   m_IndexHDMIVSync = addMenuItem(m_pItemsSelect[6]);
   #endif

   m_IndexCalibrateHDMI = addMenuItem(new MenuItem(L("Calibrate HDMI output"), L("Calibrate the colors, brightness and contrast on the controller display.")));
   m_pMenuItems[m_IndexCalibrateHDMI]->showArrow();

   m_IndexStreamerMode = -1;
   m_IndexMPPBuffers = -1;
   m_IndexWaitFullFrame = -1;

   if ( g_pControllerSettings->iDeveloperMode )
   {
      addMenuItem(new MenuItemSection(L("Local Output Dev Options")));

      m_pItemsSelect[7] = new MenuItemSelect("Local video output mode", "Change the way data is sent to the local video streamer.");
      m_pItemsSelect[7]->addSelection("Shared Mem");
      m_pItemsSelect[7]->addSelection("Pipes");
      m_pItemsSelect[7]->addSelection("UDP");
      m_pItemsSelect[7]->setIsEditable();
      m_pItemsSelect[7]->setSelectedIndex(g_pControllerSettings->iStreamerOutputMode);
      m_IndexStreamerMode = addMenuItem(m_pItemsSelect[7]);
      m_pMenuItems[m_IndexStreamerMode]->setTextColor(get_Color_Dev());

      if ( hardware_board_is_radxa(hardware_getBoardType()) )
      {
         m_pItemsSlider[5] = new MenuItemSlider("Video Buffers Size", "Sets a relative size for the video buffers used by live video stream player.", 5,100,30, fSliderWidth);
         m_pItemsSlider[5]->setCurrentValue(g_pControllerSettings->iVideoMPPBuffersSize);
         m_IndexMPPBuffers = addMenuItem(m_pItemsSlider[5]);
         m_pMenuItems[m_IndexMPPBuffers]->setTextColor(get_Color_Dev());
      }

      m_pItemsSelect[8] = new MenuItemSelect("Wait full video frames for output", "Waits to receive a full video frame before outputing it.");
      m_pItemsSelect[8]->addSelection("No");
      m_pItemsSelect[8]->addSelection("Yes");
      m_pItemsSelect[8]->setIsEditable();
      m_pItemsSelect[8]->setSelectedIndex(g_pControllerSettings->iWaitFullFrameForOutput);
      m_IndexWaitFullFrame = addMenuItem(m_pItemsSelect[8]);
      m_pMenuItems[m_IndexWaitFullFrame]->setTextColor(get_Color_Dev());
   }

   addMenuItem(new MenuItemSection(L("Other Video Outputs")));

   m_pItemsSelect[1] = new MenuItemSelect(L("Video Forward To USB Device"), L("Enables or disables forwarding of the video stream to an external device using a USB connection."));
   m_pItemsSelect[1]->addSelection(L("Disabled"));
   m_pItemsSelect[1]->addSelection(L("Raw (H264)"));
   m_pItemsSelect[1]->setIsEditable();
   m_IndexVideoUSBForward = addMenuItem(m_pItemsSelect[1]);

   m_pItemsRange[0] = new MenuItemRange("    USB Port Number", "Sets the USB port number to forward video to.", 1025, 32000, 5001, 1 );
   m_pItemsRange[0]->setSufix("");
   m_IndexVideoUSBPort = addMenuItem(m_pItemsRange[0]);

   m_pItemsRange[1] = new MenuItemRange("    USB Packet Size", "Sets the data packet size to be send to USB. Some VR apps are sensitive to this value.", 10, 2048, 1024, 1 );
   m_pItemsRange[1]->setSufix("");
   m_IndexVideoUSBPacket = addMenuItem(m_pItemsRange[1]);


   m_pItemsSelect[10] = new MenuItemSelect(L("Video Forward To Network"), L("Enables or disables forwarding of the video stream to the local network using the ETH connection."));
   m_pItemsSelect[10]->addSelection(L("Disabled"));
   m_pItemsSelect[10]->addSelection("Raw (H264)");
   m_pItemsSelect[10]->addSelection("RTP Stream");
   m_pItemsSelect[10]->setIsEditable();
   m_IndexVideoETHForward = addMenuItem(m_pItemsSelect[10]);

   m_pItemsRange[10] = new MenuItemRange("    Network Port Number", "Sets the ETH port number to forward video to.", 1025, 32000, 5001, 1 );
   m_pItemsRange[10]->setSufix("");
   m_IndexVideoETHPort = addMenuItem(m_pItemsRange[10]);

   m_pItemsRange[11] = new MenuItemRange("    Network Packet Size", "Sets the data packet size to be sent to ETH.", 10, 2048, 1024, 1 );
   m_pItemsRange[11]->setSufix("");
   m_IndexVideoETHPacket = addMenuItem(m_pItemsRange[11]);

   addMenuItem(new MenuItemSection(L("Audio Output")));

   m_IndexAudioVolume = -1;

   if ( hardware_has_audio_volume() )
   {
      m_pItemsSlider[1] = new MenuItemSlider(L("Audio Output Volume"), L("Sets the audio output volume"), 10,100,50, 0.1);
      m_IndexAudioVolume = addMenuItem(m_pItemsSlider[1]);
   }
   m_IndexAudioTest = addMenuItem(new MenuItem(L("Audio Test"), L("Test local audio output on your HDMI display/output speakers.")));
   m_pMenuItems[m_IndexAudioTest]->showArrow();
}

MenuControllerVideo::~MenuControllerVideo(void)
{
   if ( m_hdmigroupOrg != hdmi_get_current_resolution_group() ||
        m_hdmimodeOrg != hdmi_get_current_resolution_mode() )
   {
      #if defined (HW_PLATFORM_RASPBERRY)
      char szBuff[128];
      sprintf(szBuff, "touch %s%s", FOLDER_CONFIG, FILE_TEMP_HDMI_CHANGED);
      hw_execute_bash_command_silent(szBuff, NULL);

      strcpy(szBuff, FOLDER_CONFIG);
      strcat(szBuff, FILE_TEMP_HDMI_CHANGED);
      FILE* fd = fopen(szBuff, "w");
      if ( NULL != fd )
      {
         fprintf(fd, "%d %d\n", hdmi_get_current_resolution_group(), hdmi_get_current_resolution_mode() );
         fprintf(fd, "%d %d %d\n", hdmi_get_current_resolution_width(), hdmi_get_current_resolution_height(), hdmi_get_current_resolution_refresh() );
         fprintf(fd, "%d %d\n", m_hdmigroupOrg, m_hdmimodeOrg );
         fclose(fd);
      }

      #endif
   }
}

void MenuControllerVideo::valuesToUI()
{
   ControllerSettings* pCS = get_ControllerSettings();
   char szBuff[64];

   #if defined (HW_PLATFORM_RASPBERRY)
   if ( m_IndexHDMIBoost != -1 )
      m_pItemsSlider[0]->setCurrentValue(pCS->iHDMIBoost);
   if ( m_IndexHDMIOverscan != -1 )
   {
      pCS->iDisableHDMIOverscan = config_file_get_value("disable_overscan");
      m_pItemsSelect[2]->setSelectedIndex(pCS->iDisableHDMIOverscan);
   }
   #endif

   if ( -1 != m_IndexHDMIVSync )
      m_pItemsSelect[6]->setSelectedIndex(pCS->iHDMIVSync);

   m_pItemsSelect[0]->setSelection(hdmi_get_current_resolution_index());

   m_pItemsSelect[3]->removeAllSelections();
   for( int i=0; i<hdmi_get_current_resolution_refresh_count(); i++ )
   {
      sprintf(szBuff, "%d Hz", hdmi_get_resolution_refresh_rate(hdmi_get_current_resolution_index(), i));
      m_pItemsSelect[3]->addSelection(szBuff);
   }
   m_pItemsSelect[3]->setSelection(hdmi_get_current_resolution_refresh_index());

   m_pItemsSelect[1]->setSelectedIndex(pCS->iVideoForwardUSBType);
   m_pItemsRange[0]->setCurrentValue(pCS->iVideoForwardUSBPort);
   m_pItemsRange[1]->setCurrentValue(pCS->iVideoForwardUSBPacketSize);
   if ( 0 == pCS->iVideoForwardUSBType )
   {
      m_pItemsRange[0]->setEnabled(false);
      m_pItemsRange[1]->setEnabled(false);
   }
   else
   {
      m_pItemsRange[0]->setEnabled(true);
      m_pItemsRange[1]->setEnabled(true);
   }

   m_pItemsSelect[10]->setSelectedIndex(pCS->nVideoForwardETHType);
   m_pItemsRange[10]->setCurrentValue(pCS->nVideoForwardETHPort);
   m_pItemsRange[11]->setCurrentValue(pCS->nVideoForwardETHPacketSize);
   if ( 0 == pCS->nVideoForwardETHType )
   {
      m_pItemsRange[10]->setEnabled(false);
      m_pItemsRange[11]->setEnabled(false);
   }
   else
   {
      m_pItemsRange[10]->setEnabled(true);
      m_pItemsRange[11]->setEnabled(true);
   }

   if ( -1 != m_IndexAudioVolume )
      m_pItemsSlider[1]->setCurrentValue(pCS->iAudioOutputVolume);

   if ( g_pControllerSettings->iDeveloperMode )
   {
      if ( -1 != m_IndexStreamerMode )
         m_pItemsSelect[7]->setSelectedIndex(g_pControllerSettings->iStreamerOutputMode);
   
      if ( -1 != m_IndexMPPBuffers )
         m_pItemsSlider[5]->setCurrentValue(g_pControllerSettings->iVideoMPPBuffersSize);

      if ( -1 != m_IndexWaitFullFrame )
         m_pItemsSelect[8]->setSelectedIndex(g_pControllerSettings->iWaitFullFrameForOutput);
   }
}

void MenuControllerVideo::Render()
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

void MenuControllerVideo::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   #if defined (HW_PLATFORM_RADXA)
   if ( 2 == iChildMenuId/1000 )
   {
      ruby_mark_reinit_hdmi_display();
      return;
   }
   #endif

   if ( 5 == iChildMenuId/1000 )
   {
      s_bStopAudioTest = true;
      hw_stop_process("aplay");
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_PAUSE_RESUME_AUDIO, 0);
   }
   valuesToUI();
}


int MenuControllerVideo::onBack()
{
   #if defined (HW_PLATFORM_RADXA)
   if ( m_hdmigroupOrg != hdmi_get_current_resolution_group() ||
        m_hdmimodeOrg != hdmi_get_current_resolution_mode() )
   {
      MenuConfirmation* pMC = new MenuConfirmation(L("Changing HDMI resolution"), L("Your display will flicker or go black for few seconds while the HDMI mode is changed."), 2, true);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      return 1;
   }
   #endif
   return Menu::onBack();
}

void MenuControllerVideo::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   ControllerSettings* pCS = get_ControllerSettings();
   if ( NULL == pCS )
   {
      log_softerror_and_alarm("Failed to get pointer to controller settings structure");
      return;
   }

   Preferences* pp = get_Preferences();
   if ( NULL == pp )
   {
      log_softerror_and_alarm("Failed to get pointer to preferences structure");
      return;
   }

   if ( m_IndexHDMIBoost == m_SelectedIndex )
   {
      char szBuff[64];
      int hdmi_boost = m_pItemsSlider[0]->getCurrentValue();
      if ( hdmi_boost == pCS->iHDMIBoost )
         return;
      pCS->iHDMIBoost = hdmi_boost;

      hardware_mount_boot();
      hardware_sleep_ms(50);
      hw_execute_bash_command("cp /boot/config.txt config.txt", NULL);
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "sed -i 's/config_hdmi_boost=[0-9]*/config_hdmi_boost=%d/g' config.txt", hdmi_boost);
      hw_execute_bash_command(szBuff, NULL);
      hw_execute_bash_command("cp config.txt /boot/config.txt", NULL);
      
      save_ControllerSettings();
      return;
   }

   if ( m_IndexHDMIRes == m_SelectedIndex )
   {
      int index = m_pItemsSelect[0]->getSelectedIndex();
      hdmi_set_current_resolution(hdmi_get_resolution_width(index), hdmi_get_resolution_height(index), hdmi_get_resolution_refresh_rate(index,0));
      valuesToUI();
      return;
   }

   if ( m_IndexHDMIRefreshRate == m_SelectedIndex )
   {
      int index = m_pItemsSelect[0]->getSelectedIndex();
      int indexR = m_pItemsSelect[3]->getSelectedIndex();
      hdmi_set_current_resolution(hdmi_get_resolution_width(index), hdmi_get_resolution_height(index), hdmi_get_resolution_refresh_rate(index,indexR));
      return;
   }

   if ( m_IndexHDMIOverscan == m_SelectedIndex )
   {
      pCS->iDisableHDMIOverscan = m_pItemsSelect[2]->getSelectedIndex();
      save_ControllerSettings();
      hardware_mount_boot();
      hardware_sleep_ms(50);
      hw_execute_bash_command("cp /boot/config.txt config.txt", NULL);

      config_file_force_value("config.txt", "disable_overscan", pCS->iDisableHDMIOverscan);
      hw_execute_bash_command("cp config.txt /boot/config.txt", NULL);

      invalidate();
      valuesToUI();
      return;
   }

   if ( (-1 != m_IndexHDMIVSync) && (m_IndexHDMIVSync == m_SelectedIndex) )
   {
      pCS->iHDMIVSync = m_pItemsSelect[6]->getSelectedIndex();
      save_ControllerSettings();
      #if defined (HW_PLATFORM_RADXA)
      ruby_drm_enable_vsync(pCS->iHDMIVSync?true:false);
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      #endif
      invalidate();
      valuesToUI();
      return;
   }

   if ( m_IndexCalibrateHDMI == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuCalibrateHDMI());      
      return;
   }

   if ( m_IndexVideoUSBForward == m_SelectedIndex )
   {
      pCS->iVideoForwardUSBType = m_pItemsSelect[1]->getSelectedIndex();
      save_ControllerSettings();
      invalidate();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      return;
   }
   if ( m_IndexVideoUSBPort == m_SelectedIndex )
   {
      pCS->iVideoForwardUSBPort = m_pItemsRange[0]->getCurrentValue();
      save_ControllerSettings();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      invalidate();
      valuesToUI();
      return;
   }
   if ( m_IndexVideoUSBPacket == m_SelectedIndex )
   {
      pCS->iVideoForwardUSBPacketSize = m_pItemsRange[1]->getCurrentValue();
      save_ControllerSettings();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      invalidate();
      valuesToUI();
      return;
   }


   if ( m_IndexVideoETHForward == m_SelectedIndex )
   {
      pCS->nVideoForwardETHType = m_pItemsSelect[10]->getSelectedIndex();
      save_ControllerSettings();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      invalidate();
      valuesToUI();
      return;
   }

   if ( m_IndexVideoETHPort == m_SelectedIndex )
   {
      pCS->nVideoForwardETHPort = m_pItemsRange[10]->getCurrentValue();
      save_ControllerSettings();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      invalidate();
      valuesToUI();
      return;
   }

   if ( m_IndexVideoETHPacket == m_SelectedIndex )
   {
      pCS->nVideoForwardETHPacketSize = m_pItemsRange[11]->getCurrentValue();
      save_ControllerSettings();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      invalidate();
      valuesToUI();
      return;
   }

   if ( (-1 != m_IndexAudioVolume) && (m_IndexAudioVolume == m_SelectedIndex) )
   {
      pCS->iAudioOutputVolume = m_pItemsSlider[1]->getCurrentValue();
      save_ControllerSettings();
      hardware_set_audio_output_volume(pCS->iAudioOutputVolume);
      valuesToUI();
      return;
   }

   if ( m_IndexAudioTest == m_SelectedIndex )
   {
      if ( ! hardware_has_audio_playback() )
      {
         addMessage(L("Your device does not have audio output capabilities. Can't play audio."));
         return;
      }
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_PAUSE_RESUME_AUDIO, 1);
      s_bStopAudioTest = false;
      pthread_attr_t attr;
      hw_init_worker_thread_attrs(&attr, CORE_AFFINITY_OTHERS, -1, SCHED_OTHER, 0, "test audio async from central");
      if ( 0 != pthread_create(&s_pThreadAudioTest, &attr, &_thread_audio_test_async, NULL) )
      {
         pthread_attr_destroy(&attr);
         MenuConfirmation* pMC = new MenuConfirmation(L("Audio Test"),L("Failed to test audio."), 6, true);
         add_menu_to_stack(pMC);
         send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_PAUSE_RESUME_AUDIO, 0);
         return;
      }
      pthread_attr_destroy(&attr);
      MenuConfirmation* pMC = new MenuConfirmation(L("Audio Test"),L("This tests your HDMI display/output audio speakers. Now you should hear an audio sample."), 5, true);
      pMC->setOkActionText(L("Stop"));
      pMC->addTopLine(L("If you don't hear anything, increase audio volume or check your speakers."));
      add_menu_to_stack(pMC);
      //send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_PAUSE_RESUME_AUDIO, 0);
      return;
   }

   if ( (-1 != m_IndexMPPBuffers) && (m_IndexMPPBuffers == m_SelectedIndex) )
   {
      g_pControllerSettings->iVideoMPPBuffersSize = m_pItemsSlider[5]->getCurrentValue();
      save_ControllerSettings();

      // Force a restart of video streamer

      if ( pairing_isStarted() )
      if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->hasCamera()) )
         send_model_changed_message_to_router(MODEL_CHANGED_VIDEO_CODEC, 0);
      return;
   }

   if ( m_IndexStreamerMode == m_SelectedIndex )
   {
      g_pControllerSettings->iStreamerOutputMode = m_pItemsSelect[7]->getSelectedIndex();
      log_line("Streamer output mode was changed to: %d", g_pControllerSettings->iStreamerOutputMode);
      save_ControllerSettings();
      pairing_stop();
      ruby_signal_alive();
      pairing_start_normal();
      return;
   }
  
   if ( m_IndexWaitFullFrame == m_SelectedIndex )
   {
      g_pControllerSettings->iWaitFullFrameForOutput = m_pItemsSelect[8]->getSelectedIndex();
            save_ControllerSettings();
      save_Preferences();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);

   }
}
