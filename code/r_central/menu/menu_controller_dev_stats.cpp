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
#include "menu_controller_dev_stats.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"
#include "../../radio/radiolink.h"
#include "../../base/utils.h"
#include "../pairing.h"
#include "../link_watch.h"



MenuControllerDevStatsConfig::MenuControllerDevStatsConfig(void)
:Menu(MENU_ID_CONTROLLER_DEV_STATS, "Developer Runtime Stats Config", NULL)
{
   m_Width = 0.44;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.16;
}

void MenuControllerDevStatsConfig::valuesToUI()
{
   addItems();
}

void MenuControllerDevStatsConfig::addItems()
{
   int iTmp = getSelectedMenuItemIndex();
   Preferences* pP = get_Preferences();
   
   removeAllItems();

   if ( NULL == pP )
      return;

   m_pItemsSelect[0] = new MenuItemSelect("Show/Hide Using QA Button", "");
   m_pItemsSelect[0]->addSelection("None");
   m_pItemsSelect[0]->addSelection("QA1");
   m_pItemsSelect[0]->addSelection("QA2");
   m_pItemsSelect[0]->addSelection("QA3");
   m_pItemsSelect[0]->setSelectedIndex(pP->iDebugStatsQAButton);
   m_pItemsSelect[0]->setIsEditable();
   m_IndexQAButton = addMenuItem(m_pItemsSelect[0]);

   //-----------------------------------------------------------
   addMenuItem(new MenuItemSection("Radio"));

   m_pItemsSelect[1] = new MenuItemSelect("Show RX/TX Packets", "");
   m_pItemsSelect[1]->addSelection("No");
   m_pItemsSelect[1]->addSelection("Yes");
   m_pItemsSelect[1]->setUseMultiViewLayout();
   m_pItemsSelect[1]->setSelectedIndex( (pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_TX_PACKETS)?1:0);
   m_IndexShowRXTXPackets = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSelect[4] = new MenuItemSelect("Show RX Missing Packets", "");
   m_pItemsSelect[4]->addSelection("No");
   m_pItemsSelect[4]->addSelection("Yes");
   m_pItemsSelect[4]->setUseMultiViewLayout();
   m_pItemsSelect[4]->setSelectedIndex( (pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_MISSING_PACKETS)?1:0);
   m_IndexShowRxMissingPackets = addMenuItem(m_pItemsSelect[4]);

   m_pItemsSelect[5] = new MenuItemSelect("Show RX Missing Packets Max Gap", "");
   m_pItemsSelect[5]->addSelection("No");
   m_pItemsSelect[5]->addSelection("Yes");
   m_pItemsSelect[5]->setUseMultiViewLayout();
   m_pItemsSelect[5]->setSelectedIndex( (pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_MISSING_PACKETS_MAX_GAP)?1:0);
   m_IndexShowRxMissingPacketsMaxGap = addMenuItem(m_pItemsSelect[5]);

   m_pItemsSelect[13] = new MenuItemSelect("Show RX Air Gaps", "Show distance (in miliseconds) between received radio packets.");
   m_pItemsSelect[13]->addSelection("No");
   m_pItemsSelect[13]->addSelection("Yes");
   m_pItemsSelect[13]->setUseMultiViewLayout();
   m_pItemsSelect[13]->setSelectedIndex( (pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_AIR_GAPS)?1:0);
   m_IndexShowRXAirGaps = addMenuItem(m_pItemsSelect[13]);

   m_pItemsSelect[3] = new MenuItemSelect("Show RX DBM", "");
   m_pItemsSelect[3]->addSelection("No");
   m_pItemsSelect[3]->addSelection("Yes");
   m_pItemsSelect[3]->setUseMultiViewLayout();
   m_pItemsSelect[3]->setSelectedIndex( (pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_DBM)?1:0);
   m_IndexShowRxDBM = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSelect[15] = new MenuItemSelect("Show RX SNR", "");
   m_pItemsSelect[15]->addSelection("No");
   m_pItemsSelect[15]->addSelection("Yes");
   m_pItemsSelect[15]->setUseMultiViewLayout();
   m_pItemsSelect[15]->setSelectedIndex( (pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_SNR)?1:0);
   m_IndexShowRxSNR = addMenuItem(m_pItemsSelect[15]);

   m_pItemsSelect[6] = new MenuItemSelect("Show RX Consumed Packets", "");
   m_pItemsSelect[6]->addSelection("No");
   m_pItemsSelect[6]->addSelection("Yes");
   m_pItemsSelect[6]->setUseMultiViewLayout();
   m_pItemsSelect[6]->setSelectedIndex( (pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_CONSUMED_PACKETS)?1:0);
   m_IndexShowRxConsumedPackets = addMenuItem(m_pItemsSelect[6]);

   m_pItemsSelect[14] = new MenuItemSelect("Show TX High/Reg Priority Packets", "");
   m_pItemsSelect[14]->addSelection("No");
   m_pItemsSelect[14]->addSelection("Yes");
   m_pItemsSelect[14]->setUseMultiViewLayout();
   m_pItemsSelect[14]->setSelectedIndex( (pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_TX_HIGH_REG_PACKETS)?1:0);
   m_IndexShowTxHighRegPackets = addMenuItem(m_pItemsSelect[14]);
  
   //-----------------------------------------------------------
   addMenuItem(new MenuItemSection("Video"));

   m_pItemsSelect[16] = new MenuItemSelect("Show Video Frames Sizes", "");
   m_pItemsSelect[16]->addSelection("No");
   m_pItemsSelect[16]->addSelection("Yes");
   m_pItemsSelect[16]->setUseMultiViewLayout();
   m_pItemsSelect[16]->setSelectedIndex( (pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_SIZES)?1:0);
   m_IndexShowVideoFramesSizes = addMenuItem(m_pItemsSelect[16]);

   m_pItemsSelect[19] = new MenuItemSelect("Show Video Frames Jitter/Times (@Capture)", "");
   m_pItemsSelect[19]->addSelection("No");
   m_pItemsSelect[19]->addSelection("Yes");
   m_pItemsSelect[19]->setUseMultiViewLayout();
   m_pItemsSelect[19]->setSelectedIndex( (pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_CAPTURE_JITTER)?1:0);
   m_IndexShowVideoFramesCaptureJitter = addMenuItem(m_pItemsSelect[19]);

   m_pItemsSelect[2] = new MenuItemSelect("Show Output Video Frames", "");
   m_pItemsSelect[2]->addSelection("No");
   m_pItemsSelect[2]->addSelection("Yes");
   m_pItemsSelect[2]->setUseMultiViewLayout();
   m_pItemsSelect[2]->setSelectedIndex( (pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_OUTPUT_VIDEO_FRAMES)?1:0);
   m_IndexShowOutputVideoFrames = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSelect[8] = new MenuItemSelect("Show RX Max EC Used", "");
   m_pItemsSelect[8]->addSelection("No");
   m_pItemsSelect[8]->addSelection("Yes");
   m_pItemsSelect[8]->setUseMultiViewLayout();
   m_pItemsSelect[8]->setSelectedIndex( (pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_VIDEO_MAX_EC_USED)?1:0);
   m_IndexShowRxMaxECUsed = addMenuItem(m_pItemsSelect[8]);

   m_pItemsSelect[9] = new MenuItemSelect("Show Unrecoverable/skipped Video Blocks", "");
   m_pItemsSelect[9]->addSelection("No");
   m_pItemsSelect[9]->addSelection("Yes");
   m_pItemsSelect[9]->setUseMultiViewLayout();
   m_pItemsSelect[9]->setSelectedIndex( (pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_VIDEO_UNRECOVERABLE_BLOCKS)?1:0);
   m_IndexShowUnrecoverableVideoBlocks = addMenuItem(m_pItemsSelect[9]);

   m_pItemsSelect[10] = new MenuItemSelect("Show Video Profile Changes", "");
   m_pItemsSelect[10]->addSelection("No");
   m_pItemsSelect[10]->addSelection("Yes");
   m_pItemsSelect[10]->setUseMultiViewLayout();
   m_pItemsSelect[10]->setSelectedIndex( (pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_PROFILE_CHANGES)?1:0);
   m_IndexShowVideoProfileChanges = addMenuItem(m_pItemsSelect[10]);

   m_pItemsSelect[11] = new MenuItemSelect("Show Video Retransmissions", "");
   m_pItemsSelect[11]->addSelection("No");
   m_pItemsSelect[11]->addSelection("Yes");
   m_pItemsSelect[11]->setUseMultiViewLayout();
   m_pItemsSelect[11]->setSelectedIndex( (pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_RETRANSMISSIONS)?1:0);
   m_IndexShowVideoRetransmissions = addMenuItem(m_pItemsSelect[11]);

   //-----------------------------------------------------------
   addMenuItem(new MenuItemSection("Processing Times"));

   m_pItemsSelect[7] = new MenuItemSelect("Show Min/Max Ack Time", "");
   m_pItemsSelect[7]->addSelection("No");
   m_pItemsSelect[7]->addSelection("Yes");
   m_pItemsSelect[7]->setUseMultiViewLayout();
   m_pItemsSelect[7]->setSelectedIndex( (pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_MIN_MAX_ACK_TIME)?1:0);
   m_IndexShowMinMaxAckTime = addMenuItem(m_pItemsSelect[7]);

   m_pItemsSelect[12] = new MenuItemSelect("Show Ack Roundtrip-times History", "");
   m_pItemsSelect[12]->addSelection("No");
   m_pItemsSelect[12]->addSelection("Yes");
   m_pItemsSelect[12]->setUseMultiViewLayout();
   m_pItemsSelect[12]->setSelectedIndex( (pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_ACK_TIME_HISTORY)?1:0);
   m_IndexShowAckTimeHist = addMenuItem(m_pItemsSelect[12]);

   m_pItemsSelect[17] = new MenuItemSelect("Show Video Frames Processing Times (@Air Side)", "");
   m_pItemsSelect[17]->addSelection("No");
   m_pItemsSelect[17]->addSelection("Yes");
   m_pItemsSelect[17]->setUseMultiViewLayout();
   m_pItemsSelect[17]->setSelectedIndex( (pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_PROC_TIMES)?1:0);
   m_IndexShowVideoFramesProcTimes = addMenuItem(m_pItemsSelect[17]);

   m_pItemsSelect[20] = new MenuItemSelect("Show Video Frames Send Duration (@Air Side)", "");
   m_pItemsSelect[20]->addSelection("No");
   m_pItemsSelect[20]->addSelection("Yes");
   m_pItemsSelect[20]->setUseMultiViewLayout();
   m_pItemsSelect[20]->setSelectedIndex( (pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_SEND_DURATION)?1:0);
   m_IndexShowVideoFramesSendTimes = addMenuItem(m_pItemsSelect[20]);

   m_pItemsSelect[18] = new MenuItemSelect("Show Video Frames Receiving Duration (@GS Side)", "");
   m_pItemsSelect[18]->addSelection("No");
   m_pItemsSelect[18]->addSelection("Yes");
   m_pItemsSelect[18]->setUseMultiViewLayout();
   m_pItemsSelect[18]->setSelectedIndex( (pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_RECV_DURATION)?1:0);
   m_IndexShowVideoFramesRecvTimes = addMenuItem(m_pItemsSelect[18]);

   for( int i=0; i<m_ItemsCount; i++ )
      m_pMenuItems[i]->setTextColor(get_Color_Dev());

   m_SelectedIndex = iTmp;
   if ( m_SelectedIndex >= m_ItemsCount )
      m_SelectedIndex = m_ItemsCount-1;
}

void MenuControllerDevStatsConfig::onShow()
{
   addItems();
   Menu::onShow();
}

void MenuControllerDevStatsConfig::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yTop);
}


void MenuControllerDevStatsConfig::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);
}

void MenuControllerDevStatsConfig::onSelectItem()
{
   Preferences* pP = get_Preferences();
  
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( NULL == pP)
      return;

   if ( m_IndexQAButton == m_SelectedIndex )
   {
      pP->iDebugStatsQAButton = m_pItemsSelect[0]->getSelectedIndex();
   }

   if ( m_IndexShowRXTXPackets == m_SelectedIndex )
   {
      if ( m_pItemsSelect[1]->getSelectedIndex() != 0 )
         pP->uDebugStatsFlags |= CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_TX_PACKETS;
      else
         pP->uDebugStatsFlags &= ~CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_TX_PACKETS;
   }

   if ( m_IndexShowRXAirGaps == m_SelectedIndex )
   {
      if ( m_pItemsSelect[13]->getSelectedIndex() != 0 )
         pP->uDebugStatsFlags |= CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_AIR_GAPS;
      else
         pP->uDebugStatsFlags &= ~CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_AIR_GAPS;
   }

   if ( m_IndexShowOutputVideoFrames == m_SelectedIndex )
   {
      if ( m_pItemsSelect[2]->getSelectedIndex() != 0 )
         pP->uDebugStatsFlags |= CTRL_RT_DEBUG_INFO_FLAG_SHOW_OUTPUT_VIDEO_FRAMES;
      else
         pP->uDebugStatsFlags &= ~CTRL_RT_DEBUG_INFO_FLAG_SHOW_OUTPUT_VIDEO_FRAMES;
   }

   if ( m_IndexShowRxDBM == m_SelectedIndex )
   {
      if ( m_pItemsSelect[3]->getSelectedIndex() != 0 )
         pP->uDebugStatsFlags |= CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_DBM;
      else
         pP->uDebugStatsFlags &= ~CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_DBM;
   }

   if ( m_IndexShowRxSNR == m_SelectedIndex )
   {
      if ( m_pItemsSelect[15]->getSelectedIndex() != 0 )
         pP->uDebugStatsFlags |= CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_SNR;
      else
         pP->uDebugStatsFlags &= ~CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_SNR;
   }


   if ( m_IndexShowRxMissingPackets == m_SelectedIndex )
   {
      if ( m_pItemsSelect[4]->getSelectedIndex() != 0 )
         pP->uDebugStatsFlags |= CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_MISSING_PACKETS;
      else
         pP->uDebugStatsFlags &= ~CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_MISSING_PACKETS;
   }

   if ( m_IndexShowRxMissingPacketsMaxGap == m_SelectedIndex )
   {
      if ( m_pItemsSelect[5]->getSelectedIndex() != 0 )
         pP->uDebugStatsFlags |= CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_MISSING_PACKETS_MAX_GAP;
      else
         pP->uDebugStatsFlags &= ~CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_MISSING_PACKETS_MAX_GAP;
   }

   if ( m_IndexShowRxConsumedPackets == m_SelectedIndex )
   {
      if ( m_pItemsSelect[6]->getSelectedIndex() != 0 )
         pP->uDebugStatsFlags |= CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_CONSUMED_PACKETS;
      else
         pP->uDebugStatsFlags &= ~CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_CONSUMED_PACKETS;
   }

   if ( m_IndexShowTxHighRegPackets == m_SelectedIndex )
   {
      if ( m_pItemsSelect[14]->getSelectedIndex() != 0 )
         pP->uDebugStatsFlags |= CTRL_RT_DEBUG_INFO_FLAG_SHOW_TX_HIGH_REG_PACKETS;
      else
         pP->uDebugStatsFlags &= ~CTRL_RT_DEBUG_INFO_FLAG_SHOW_TX_HIGH_REG_PACKETS;
   }


   if ( m_IndexShowMinMaxAckTime == m_SelectedIndex )
   {
      if ( m_pItemsSelect[7]->getSelectedIndex() != 0 )
         pP->uDebugStatsFlags |= CTRL_RT_DEBUG_INFO_FLAG_SHOW_MIN_MAX_ACK_TIME;
      else
         pP->uDebugStatsFlags &= ~CTRL_RT_DEBUG_INFO_FLAG_SHOW_MIN_MAX_ACK_TIME;
   }

   if ( m_IndexShowAckTimeHist == m_SelectedIndex )
   {
      if ( m_pItemsSelect[12]->getSelectedIndex() != 0 )
         pP->uDebugStatsFlags |= CTRL_RT_DEBUG_INFO_FLAG_SHOW_ACK_TIME_HISTORY;
      else
         pP->uDebugStatsFlags &= ~CTRL_RT_DEBUG_INFO_FLAG_SHOW_ACK_TIME_HISTORY;
   }

   if ( m_IndexShowRxMaxECUsed == m_SelectedIndex )
   {
      if ( m_pItemsSelect[8]->getSelectedIndex() != 0 )
         pP->uDebugStatsFlags |= CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_VIDEO_MAX_EC_USED;
      else
         pP->uDebugStatsFlags &= ~CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_VIDEO_MAX_EC_USED;
   }

   if ( m_IndexShowUnrecoverableVideoBlocks == m_SelectedIndex )
   {
      if ( m_pItemsSelect[9]->getSelectedIndex() != 0 )
         pP->uDebugStatsFlags |= CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_VIDEO_UNRECOVERABLE_BLOCKS;
      else
         pP->uDebugStatsFlags &= ~CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_VIDEO_UNRECOVERABLE_BLOCKS;
   }

   if ( m_IndexShowVideoFramesSizes == m_SelectedIndex )
   {
      if ( m_pItemsSelect[16]->getSelectedIndex() != 0 )
         pP->uDebugStatsFlags |= CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_SIZES;
      else
         pP->uDebugStatsFlags &= ~CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_SIZES;
   }

   if ( m_IndexShowVideoFramesCaptureJitter == m_SelectedIndex )
   {
      if ( m_pItemsSelect[19]->getSelectedIndex() != 0 )
         pP->uDebugStatsFlags |= CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_CAPTURE_JITTER;
      else
         pP->uDebugStatsFlags &= ~CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_CAPTURE_JITTER;
   }

   if ( m_IndexShowVideoFramesProcTimes == m_SelectedIndex )
   {
      if ( m_pItemsSelect[17]->getSelectedIndex() != 0 )
         pP->uDebugStatsFlags |= CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_PROC_TIMES;
      else
         pP->uDebugStatsFlags &= ~CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_PROC_TIMES;
   }

   if ( m_IndexShowVideoFramesSendTimes == m_SelectedIndex )
   {
      if ( m_pItemsSelect[20]->getSelectedIndex() != 0 )
         pP->uDebugStatsFlags |= CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_SEND_DURATION;
      else
         pP->uDebugStatsFlags &= ~CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_SEND_DURATION;
   }

   if ( m_IndexShowVideoFramesRecvTimes == m_SelectedIndex )
   {
      if ( m_pItemsSelect[18]->getSelectedIndex() != 0 )
         pP->uDebugStatsFlags |= CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_RECV_DURATION;
      else
         pP->uDebugStatsFlags &= ~CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_FRAMES_RECV_DURATION;
   }

   if ( m_IndexShowVideoProfileChanges == m_SelectedIndex )
   {
      if ( m_pItemsSelect[10]->getSelectedIndex() != 0 )
         pP->uDebugStatsFlags |= CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_PROFILE_CHANGES;
      else
         pP->uDebugStatsFlags &= ~CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_PROFILE_CHANGES;
   }

   if ( m_IndexShowVideoRetransmissions == m_SelectedIndex )
   {
      if ( m_pItemsSelect[11]->getSelectedIndex() != 0 )
         pP->uDebugStatsFlags |= CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_RETRANSMISSIONS;
      else
         pP->uDebugStatsFlags &= ~CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_RETRANSMISSIONS;
   }

   save_Preferences();
}

