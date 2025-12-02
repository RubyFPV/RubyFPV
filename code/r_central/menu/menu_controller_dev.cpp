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
#include "menu_text.h"
#include "menu_controller_dev.h"
#include "menu_controller_dev_stats.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"
#include "../../radio/radiolink.h"
#include "../../base/utils.h"
#include "../pairing.h"
#include "../link_watch.h"
#include "../process_router_messages.h"



MenuControllerDev::MenuControllerDev(void)
:Menu(MENU_ID_CONTROLLER_DEV, "Developer Settings", NULL)
{
   m_Width = 0.36;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.16;
}

void MenuControllerDev::valuesToUI()
{
   addItems();
}

void MenuControllerDev::addItems()
{
   int iTmp = getSelectedMenuItemIndex();
   Preferences* pP = get_Preferences();
   float fSliderWidth = 0.14 * m_sfScaleFactor;

   removeAllItems();

   m_IndexMPPBuffers = -1;
   if ( (NULL == g_pControllerSettings) || (NULL == pP) )
      return;

   addMenuItem(new MenuItemSection("Radio Links"));

   char szTitle[128];
   if ( g_pControllerSettings->iRadioTxUsesPPCAP )
      strcpy(szTitle, "Controller Radio Tx Type (now PPCAP)");
   else
      strcpy(szTitle, "Controller Radio Tx Type (now socket)");
   m_pItemsSelect[9] = new MenuItemSelect(szTitle, "What method the controller uses for transmitting radio packets.");
   m_pItemsSelect[9]->addSelection("Sockets");
   m_pItemsSelect[9]->addSelection("PPCAP");
   m_pItemsSelect[9]->setIsEditable();
   m_pItemsSelect[9]->setSelectedIndex(0);
   if ( g_pControllerSettings->iRadioTxUsesPPCAP )
      m_pItemsSelect[9]->setSelectedIndex(1);

   m_IndexPCAPRadioTx = addMenuItem(m_pItemsSelect[9]);

   m_pItemsSelect[6] = new MenuItemSelect("Bypass kernel sockets buffers", "Skip kernel's qdisc (traffic control) layer (PACKET_QDISC_BYPASS).");
   m_pItemsSelect[6]->addSelection("No");
   m_pItemsSelect[6]->addSelection("Yes");
   m_pItemsSelect[6]->setIsEditable();
   m_pItemsSelect[6]->setSelectedIndex(g_pControllerSettings->iRadioBypassSocketBuffers);
   m_IndexBypassSocketBuffers = addMenuItem(m_pItemsSelect[6]);

   m_pItemsSlider[2] = new MenuItemSlider("Max Radio Packet Size", "Maximum size in bytes that can be set for a radio packet in the user interface.", 100,1500,1250, fSliderWidth);
   m_pItemsSlider[2]->setStep(10);
   m_pItemsSlider[2]->setCurrentValue(pP->iDebugMaxPacketSize);
   m_IndexMaxPacketSize = addMenuItem(m_pItemsSlider[2]);


   m_pItemsSlider[7] = new MenuItemSlider("Ping/Clock Sync Frequency", "How often is the clock sync done with the vehicle.", 1,50,10, fSliderWidth);
   m_pItemsSlider[7]->setCurrentValue(g_pControllerSettings->nPingClockSyncFrequency);
   m_IndexPingClockSpeed = addMenuItem(m_pItemsSlider[7]);

   m_pItemsSlider[8] = new MenuItemSlider("Radio Reconfiguration Delay (ms)", "When 2.4/5.8Ghz radio interfaces need to be reconfigured, allow a delay for reconfiguration. Important for Atheros chipset cards mostly (in miliseconds).", 1,100,10, fSliderWidth);
   m_pItemsSlider[8]->setCurrentValue(pP->iDebugWiFiChangeDelay);
   m_IndexWiFiChangeDelay = addMenuItem(m_pItemsSlider[8]);

   m_pItemsSlider[9] = new MenuItemSlider("Radio Rx Loop Check Max Time (ms)", "The threshold for generating an alarm when radio Rx loop takes too much time (in miliseconds).", 1,100,10, fSliderWidth);
   m_pItemsSlider[9]->setCurrentValue(g_pControllerSettings->iDevRxLoopTimeout);
   m_IndexRxLoopTimeout = addMenuItem(m_pItemsSlider[9]);

   m_IndexDebugRTStatsGraphs = addMenuItem(new MenuItem("Show real time stats graphs", "Show live monitor of Rx links and video stats"));
   m_pMenuItems[m_IndexDebugRTStatsGraphs]->showArrow();

   m_IndexDebugRTStatsConfig = addMenuItem(new MenuItem("Real time debug stats config", "Configure live monitor of Rx links and video stats"));
   m_pMenuItems[m_IndexDebugRTStatsConfig]->showArrow();
   
   addMenuItem(new MenuItemSection("OSD"));

   m_pItemsSelect[3] = new MenuItemSelect("OSD Render FPS", "How often should the OSD be drawn.");
   m_pItemsSelect[3]->addSelection("10");
   m_pItemsSelect[3]->addSelection("15");
   m_pItemsSelect[3]->addSelection("20");
   m_pItemsSelect[3]->addSelection("25");
   m_pItemsSelect[3]->setIsEditable();
   m_pItemsSelect[3]->setSelection( (g_pControllerSettings->iRenderFPS-10)/5 );
   m_IndexRenderOSDFSP = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSelect[13] = new MenuItemSelect("Show UI/OSD CPU Usage", "Shows the CPU resources used by the UI and OSD interface.");
   m_pItemsSelect[13]->addSelection("No");
   m_pItemsSelect[13]->addSelection("Yes");
   m_pItemsSelect[13]->setIsEditable();
   m_pItemsSelect[13]->setSelectedIndex(pP->iShowCPULoad);
   m_IndexCPULoad = addMenuItem(m_pItemsSelect[13]);

   m_pItemsSelect[14] = new MenuItemSelect("Pause OSD", "Pause/Resume OSD using the [Cancel]/[Back] button.");
   m_pItemsSelect[14]->addSelection("No");
   m_pItemsSelect[14]->addSelection("Yes");
   m_pItemsSelect[14]->setIsEditable();
   m_pItemsSelect[14]->setSelectedIndex(g_pControllerSettings->iFreezeOSD);
   m_IndexFreezeOSD = addMenuItem(m_pItemsSelect[14]);

   addMenuItem(new MenuItemSection("Other Settings"));

   m_pItemsSelect[1] = new MenuItemSelect("Local video output mode", "Change the way data is sent to the local video streamer.");
   m_pItemsSelect[1]->addSelection("Shared Mem");
   m_pItemsSelect[1]->addSelection("Pipes");
   m_pItemsSelect[1]->addSelection("UDP");
   m_pItemsSelect[1]->setIsEditable();
   m_pItemsSelect[1]->setSelectedIndex(g_pControllerSettings->iStreamerOutputMode);
   m_IndexStreamerMode = addMenuItem(m_pItemsSelect[1]);

   m_IndexMPPBuffers = -1;
   if ( hardware_board_is_radxa(hardware_getBoardType()) )
   {
      m_pItemsSlider[0] = new MenuItemSlider("Video Buffers Size", "Sets a relative size for the video buffers used by live video stream player.", 5,100,30, fSliderWidth);
      m_pItemsSlider[0]->setCurrentValue(g_pControllerSettings->iVideoMPPBuffersSize);
      m_IndexMPPBuffers = addMenuItem(m_pItemsSlider[0]);
   }

   m_pItemsSelect[4] = new MenuItemSelect("Wait full video frames for output", "Waits to receive a full video frame before outputing it.");
   m_pItemsSelect[4]->addSelection("No");
   m_pItemsSelect[4]->addSelection("Yes");
   m_pItemsSelect[4]->setIsEditable();
   m_pItemsSelect[4]->setSelectedIndex(g_pControllerSettings->iWaitFullFrameForOutput);
   m_IndexWaitFullFrame = addMenuItem(m_pItemsSelect[4]);

   m_IndexResetDev = addMenuItem(new MenuItem("Reset Developer Settings", "Resets all the developer settings to the factory default values."));
   m_IndexExit = addMenuItem(new MenuItem("Exit to shell", "Closes Ruby and exits to linux shell."));

   for( int i=0; i<m_ItemsCount; i++ )
      m_pMenuItems[i]->setTextColor(get_Color_Dev());

   if ( iTmp >= 0 )
      m_SelectedIndex = iTmp;
   if ( m_SelectedIndex >= m_ItemsCount )
      m_SelectedIndex = m_ItemsCount-1;
}

void MenuControllerDev::onShow()
{
   addItems();
   Menu::onShow();
}

void MenuControllerDev::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yTop);
}


void MenuControllerDev::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   if ( (2 == iChildMenuId/1000) && (1 == returnValue) )
   {
      g_pControllerSettings->iDisableRetransmissionsAfterControllerLinkLostMiliseconds = DEFAULT_CONTROLLER_LINK_MILISECONDS_TIMEOUT_TO_DISABLE_RETRANSMISSIONS;
      g_pControllerSettings->nRetryRetransmissionAfterTimeoutMS = DEFAULT_VIDEO_RETRANS_MINIMUM_RETRY_INTERVAL;
      g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs = DEFAULT_VIDEO_RETRANS_REQUEST_ON_VIDEO_SILENCE_MS;
      g_pControllerSettings->iRadioTxUsesPPCAP = DEFAULT_USE_PPCAP_FOR_TX;
      save_ControllerSettings();
      save_Preferences();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      //Menu* pm = new MenuConfirmation("Developer Settings Reset", "Controller will reboot now.", 3, true);
      //pm->m_yPos = 0.4;
      //add_menu_to_stack(pm);
      //return;
   }

   if ( 3 == iChildMenuId/1000 )
   {
      hardware_reboot();
   }
}

void MenuControllerDev::onSelectItem()
{
   Preferences* pP = get_Preferences();
   bool bUpdatedController = false;

   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( (NULL == pP) || (NULL == g_pControllerSettings) )
      return;

   if ( m_IndexMaxPacketSize == m_SelectedIndex )
   {
      int val = m_pItemsSlider[2]->getCurrentValue();
      pP->iDebugMaxPacketSize = val;
      save_Preferences();
   }

   if ( m_IndexPCAPRadioTx == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[9]->getSelectedIndex() )
         g_pControllerSettings->iRadioTxUsesPPCAP = 0;
      else
         g_pControllerSettings->iRadioTxUsesPPCAP = 1;
      bUpdatedController = true;
   }

   if ( m_IndexBypassSocketBuffers == m_SelectedIndex )
   {
      g_pControllerSettings->iRadioBypassSocketBuffers = m_pItemsSelect[6]->getSelectedIndex();
      bUpdatedController = true;
   }

   if ( m_IndexPingClockSpeed == m_SelectedIndex )
   {
      g_pControllerSettings->nPingClockSyncFrequency = m_pItemsSlider[7]->getCurrentValue();
      bUpdatedController = true;
   }

   if ( m_IndexWiFiChangeDelay == m_SelectedIndex )
   {
      pP->iDebugWiFiChangeDelay = m_pItemsSlider[8]->getCurrentValue();
      bUpdatedController = true;
   }

   if ( m_IndexRxLoopTimeout == m_SelectedIndex )
   {
      g_pControllerSettings->iDevRxLoopTimeout = m_pItemsSlider[9]->getCurrentValue();
      
      if ( NULL != g_pCurrentModel )
      if ( ! g_pCurrentModel->is_spectator )
      if ( pairing_isStarted() && link_is_vehicle_online_now(g_pCurrentModel->uVehicleId) )
      if ( ! handle_commands_send_developer_flags(g_pCurrentModel->uDeveloperFlags) )
         valuesToUI(); 
   }

   if ( m_IndexRenderOSDFSP == m_SelectedIndex )
   {
      g_pControllerSettings->iRenderFPS = 10 + m_pItemsSelect[3]->getSelectedIndex()*5;
      save_ControllerSettings();
      valuesToUI();
      return;
   }

   if ( m_IndexCPULoad == m_SelectedIndex )
   {
      pP->iShowCPULoad = m_pItemsSelect[13]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
      return;
   }

   if ( (-1 != m_IndexMPPBuffers) && (m_IndexMPPBuffers == m_SelectedIndex) )
   {
      g_pControllerSettings->iVideoMPPBuffersSize = m_pItemsSlider[0]->getCurrentValue();
      save_ControllerSettings();

      // Force a restart of video streamer

      if ( pairing_isStarted() )
      if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->hasCamera()) )
         send_model_changed_message_to_router(MODEL_CHANGED_VIDEO_CODEC, 0);
      return;
   }

   if ( m_IndexFreezeOSD == m_SelectedIndex )
   {
      g_pControllerSettings->iFreezeOSD = m_pItemsSelect[14]->getSelectedIndex();
      save_ControllerSettings();
      valuesToUI();
      return;
   }

   if ( m_IndexResetDev == m_SelectedIndex )
   {
      Menu* pm = new MenuConfirmation("Developer Settings Reset", "All developer settings where reset.", 2);
      pm->m_yPos = 0.4;
      add_menu_to_stack(pm);
      return;
   }
   if ( m_IndexStreamerMode == m_SelectedIndex )
   {
      g_pControllerSettings->iStreamerOutputMode = m_pItemsSelect[1]->getSelectedIndex();
      log_line("Streamer output mode was changed to: %d", g_pControllerSettings->iStreamerOutputMode);
      save_ControllerSettings();
      pairing_stop();
      ruby_signal_alive();
      pairing_start_normal();
      return;
   }
  
   if ( m_IndexWaitFullFrame == m_SelectedIndex )
   {
      g_pControllerSettings->iWaitFullFrameForOutput = m_pItemsSelect[4]->getSelectedIndex();
      bUpdatedController = true;
   }

   if ( bUpdatedController )
   {
      save_ControllerSettings();
      save_Preferences();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
   }

   if ( m_IndexDebugRTStatsGraphs == m_SelectedIndex )
   {
      g_pControllerSettings->iEnableDebugStats = 1;
      save_ControllerSettings();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, 0xFF);
      menu_discard_all();
      return;
   }

   if ( m_IndexDebugRTStatsConfig == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuControllerDevStatsConfig());
      return;
   }

   if ( m_IndexExit == m_SelectedIndex )
   {
      pairing_stop();
      g_bQuit = true;
   }
}

