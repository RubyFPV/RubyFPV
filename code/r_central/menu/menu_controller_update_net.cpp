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

#include <ctype.h>
#include <pthread.h>
#include "menu.h"
#include "menu_controller_update_net.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"
#include "../popup_log.h"
#include "../osd/osd_common.h"

u32 MenuControllerUpdateNet::m_uImgIdInfo = 0;
u32 MenuControllerUpdateNet::m_uImgIdInfoEth = 0;

MenuControllerUpdateNet::MenuControllerUpdateNet(void)
:MenuControllerUpdate()
{
   m_MenuId = MENU_ID_CONTROLLER_UPDATE_NET;
   m_Width = 0.48;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.16;
   m_bCheckInProgress = false;
   m_bThreadRunning = false;
   m_iUpdateStepCounter = 0;
   m_iUpdateThreadResult = 0;
   m_bNeedsUserConsent = false;
   m_bDoDefaultHandling = false;
   m_iUserConsentResult = 0;
   setTitle(L("Controller Update from Internet"));
}

void MenuControllerUpdateNet::onShow()
{
   addItems();
   Menu::onShow();
}

void MenuControllerUpdateNet::addItems()
{
   if ( 0 == m_uImgIdInfo )
      m_uImgIdInfo = g_pRenderEngine->loadIcon("res/info_ethupdate.png");
   if ( 0 == m_uImgIdInfoEth )
      m_uImgIdInfoEth = g_pRenderEngine->loadIcon("res/info_ethusb.png");
   removeAllTopLines();
   addExtraHeightAtStart(0.14);
   addTopLine(L("Connect your Ruby controller to your home internet router using an ETH cable and then press the [Check for Updates] button to check online if there are any Ruby updates available for your system."));
   addTopLine(L("If your Ruby controller does not have an ETH port, you can use a USB to ETH adapter to connect it."));
   addTopLine(L("Vehicles:"));
   addTopLine(L("If your controller gets updated then you will have the option to update over the radio link (OTA) your vehicles and air units as soon as you connect to them."));
   m_iIndexMenuCheck = addMenuItem(new MenuItem(L("Check for Updates"), L("Checks for Ruby updates available online.")));
   m_iIndexMenuBack = addMenuItem(new MenuItem(L("Back"), L("Closes this menu.")));

   m_pMenuItems[1]->setExtraHeight(getMenuFontHeight());
}

void MenuControllerUpdateNet::valuesToUI()
{
}

void MenuControllerUpdateNet::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);

   if ( (0 != m_uImgIdInfo) && (0 != m_uImgIdInfoEth) )
   {
      float xPos = m_RenderXPos + m_sfMenuPaddingX;
      float yPos = m_RenderYPos + m_sfMenuPaddingY + height_text;
      
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

      float wImg = m_RenderWidth*0.74;
      float hImg = wImg*g_pRenderEngine->getAspectRatio()*0.23;

      g_pRenderEngine->drawIcon(xPos+wImg*0.05, yPos+hImg*0.05, wImg*0.9, hImg*0.9, m_uImgIdInfo);

      float wImg2 = m_RenderWidth*0.2;
      float hImg2 = wImg2*g_pRenderEngine->getAspectRatio()*0.8;

      g_pRenderEngine->drawIcon(xPos+wImg, yPos+hImg*0.1, wImg2*0.9, hImg2*0.9, m_uImgIdInfoEth);
   }

   y += getMenuFontHeight()*0.8;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);

   if ( m_bCheckInProgress )
   {
      float yPos = m_RenderYPos + m_RenderTitleHeight + 2.0*height_text;
      float xPos = m_RenderXPos + m_sfMenuPaddingX;
      float fWidth = m_RenderWidth - 2*m_sfMenuPaddingX ;
      float fHeight = (m_RenderYPos + m_RenderHeight - m_sfMenuPaddingY) - yPos - 2.0 * height_text;

      float fAlpha = g_pRenderEngine->setGlobalAlfa(0.9);
      bool bAlpha = g_pRenderEngine->isAlphaBlendingEnabled();
      g_pRenderEngine->enableAlphaBlending();
      g_pRenderEngine->setColors(get_Color_MenuBg());
      //g_pRenderEngine->setStroke(get_Color_MenuText());
      g_pRenderEngine->setStroke(0,0,0,0);
      g_pRenderEngine->drawRoundRect(xPos - g_pRenderEngine->getPixelWidth(), yPos - g_pRenderEngine->getPixelHeight(), fWidth + 2.0 * g_pRenderEngine->getPixelWidth(), fHeight + 2.0*g_pRenderEngine->getPixelHeight(), 0.01*Menu::getMenuPaddingY());
      g_pRenderEngine->setAlphaBlendingEnabled(bAlpha);
      g_pRenderEngine->setGlobalAlfa(fAlpha);

      yPos += fHeight*0.3;
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStroke(get_Color_MenuText());
      g_pRenderEngine->setFill(0,0,0,0);
      g_pRenderEngine->drawRoundRect(xPos + fWidth*0.2, yPos, fWidth-0.4*fWidth, fHeight*0.5, 0.01*Menu::getMenuPaddingY());
      g_pRenderEngine->setColors(get_Color_MenuText());

      yPos += m_sfMenuPaddingY + 2.0*height_text;
      char szText[128];
      strcpy(szText, L("Checking for updates"));
      float fTextWidth = g_pRenderEngine->textWidth(g_idFontMenu, szText);
      g_pRenderEngine->drawText(xPos + (fWidth-fTextWidth)*0.5, yPos, g_idFontMenu, szText);

      yPos += height_text;
      strcpy(szText, L("Please wait"));
      fTextWidth = g_pRenderEngine->textWidth(g_idFontMenu, szText);
      for( int i=0; i<(m_iLoopCounter%4); i++ )
         strcat(szText, ".");
      g_pRenderEngine->drawText(xPos + (fWidth-fTextWidth)*0.5, yPos,  g_idFontMenu, szText);

      /*
      yPos += height_text*1.5;

      int iTotalSteps = 100;
      int iCurrentStep = m_iUpdateStepCounter;

      g_pRenderEngine->setStroke(get_Color_MenuText());
      g_pRenderEngine->setFill(0,0,0,0);
      g_pRenderEngine->drawRoundRect(xPos + fWidth*0.25, yPos, fWidth-0.5*fWidth, height_text, 0.01*Menu::getMenuPaddingY());
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setFill(get_Color_IconSucces());
      g_pRenderEngine->drawRoundRect(xPos + fWidth*0.25, yPos, (fWidth-0.5*fWidth) * (float)iCurrentStep / (float)iTotalSteps, height_text, 0.01*Menu::getMenuPaddingY());
      g_pRenderEngine->setColors(get_Color_MenuText());    
      */
   }
}

bool MenuControllerUpdateNet::periodicLoop()
{
   if ( m_bCheckInProgress )
   {
      if ( ! m_bThreadRunning )
         onFinishChecksUpdate();

      if ( m_bNeedsUserConsent )
      {
         m_bNeedsUserConsent = false;
         char szBuff[256];
         sprintf(szBuff, "Your controller has software version %d.%d and software version %d.%d is available to download. Do you want to upgrade?", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR, m_iOnlineVersionMajor, m_iOnlineVersionMinor);
         MenuConfirmation* pMC = new MenuConfirmation( L("Upgrade Confirmation"), szBuff, 2);
         pMC->setDefaultActionPositive();
         add_menu_to_stack(pMC);
      }
   }
   return false;
}


void* _thread_check_update(void *argument)
{
   MenuControllerUpdateNet* pMenu = (MenuControllerUpdateNet*)argument;
   hw_log_current_thread_attributes("check net update");

   hardware_sleep_ms(200);
   pMenu->onUpdateCounter();

   hw_execute_bash_command("rm -rf ver-git.txt", NULL);
   system("/usr/bin/wget --no-check-certificate -q https://raw.githubusercontent.com/RubyFPV/RubyFPV/refs/heads/main/version_ruby_base.txt -O ver-git.txt");

   hardware_sleep_ms(500);

   char szFilename[MAX_FILE_PATH_SIZE];
   strcpy(szFilename, FOLDER_BINARIES);
   strcat(szFilename, "ver-git.txt");

   if ( access(szFilename, R_OK) == -1 )
   {
      pMenu->onFinishCheckAsync(-1);
      return NULL;
   }

   FILE* fd = fopen(szFilename, "rb");
   if ( NULL == fd )
   {
      pMenu->onFinishCheckAsync(-2);
      return NULL;
   }

   char szVer[10];
   memset(szVer, 0, 10);
   for( int i=0; i<7; i++ )
   {
      if ( 1 != fscanf(fd, "%c", &szVer[i]) )
         break;
      if ( ! isdigit(szVer[i]) )
         szVer[i] = ' ';
   }

   fclose(fd);
   hw_execute_bash_command("rm -rf ver-git.txt", NULL);

   szVer[9] = 0;
   int iMajor = 0;
   int iMinor = 0;

   if ( 2 != sscanf(szVer, "%d %d", &iMajor, &iMinor) )
   {
      pMenu->onFinishCheckAsync(-3);
      return NULL;    
   }

   pMenu->onUpdateCounter();
   
   if ( (iMajor == SYSTEM_SW_VERSION_MAJOR) && (iMinor == SYSTEM_SW_VERSION_MINOR) )
   {
      pMenu->onFinishCheckAsync(2);
      return NULL;
   }

   pMenu->m_iOnlineVersionMajor = iMajor;
   pMenu->m_iOnlineVersionMinor = iMinor;
   pMenu->m_iUserConsentResult = 0;
   pMenu->m_bNeedsUserConsent = true;

   while ( pMenu->m_iUserConsentResult == 0 )
   {
      hardware_sleep_ms(100);
   }

   if ( 1 != pMenu->m_iUserConsentResult )
   {
      pMenu->onFinishCheckAsync(0);
      return NULL;
   }

   pMenu->onFinishCheckAsync(1);
   return NULL;
}

void MenuControllerUpdateNet::onUpdateCounter()
{
   m_iUpdateStepCounter++;
}

void MenuControllerUpdateNet::onFinishCheckAsync(int iResult)
{
   m_iUpdateThreadResult = iResult;
   m_bThreadRunning = false;
}

void MenuControllerUpdateNet::onFinishChecksUpdate()
{
   m_bCheckInProgress = false;
   m_bThreadRunning = false;
   m_pMenuItems[0]->setEnabled(true);
   m_pMenuItems[1]->setEnabled(true);
   m_SelectedIndex = 0;
   onFocusedItemChanged();

   if ( m_iUpdateThreadResult == 1 )
   {
      char szURL[512];
      snprintf(szURL, sizeof(szURL)/sizeof(szURL[0]), "https://github.com/RubyFPV/RubyFPV/releases/download/%d.%d/ruby_update_%d.%d.zip",
         m_iOnlineVersionMajor, m_iOnlineVersionMinor, m_iOnlineVersionMajor, m_iOnlineVersionMinor);
      log_line("Doing update from URL: (%s)", szURL);
      updateControllerSoftware(szURL);
      return;
   }

   if ( m_iUpdateThreadResult == 2 )
      addMessage(L("You already have the latest Ruby version."));
   if ( m_iUpdateThreadResult == -1 )
      addMessage(L("Failed to check for new versions online. Please check your internet connection."));
   if ( m_iUpdateThreadResult == -2  )
      addMessage(L("Failed to get the new version info. Please check your internet connection."));
   if ( m_iUpdateThreadResult == -3  )
      addMessage(L("Failed to get proper update info from server."));
}

void MenuControllerUpdateNet::onReturnFromChild(int iChildMenuId, int returnValue)
{
   MenuControllerUpdate::onReturnFromChild(iChildMenuId, returnValue);

   if ( 2 == iChildMenuId/1000 )
   {
      if ( 1 == returnValue )
         m_iUserConsentResult = 1;
      else
         m_iUserConsentResult = -1;
      return;
   }
}

void MenuControllerUpdateNet::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( m_iIndexMenuBack == m_SelectedIndex )
   {
      menu_stack_pop(0);
      return;
   }

   if ( m_iIndexMenuCheck == m_SelectedIndex )
   {
      m_pMenuItems[0]->setEnabled(false);
      m_pMenuItems[1]->setEnabled(false);
      m_bCheckInProgress = true;
      m_bThreadRunning = true;
      m_iUpdateStepCounter = 0;
      m_iUpdateThreadResult = 0;
      m_bNeedsUserConsent = false;
      m_iUserConsentResult = 0;
      pthread_attr_t attr;
      hw_init_worker_thread_attrs(&attr, CORE_AFFINITY_OTHERS, -1, SCHED_OTHER, 0, "check update from net");
      if ( 0 != pthread_create(&m_pThreadCheckUpdate, &attr, &_thread_check_update, (void*)this) )
      {
         pthread_attr_destroy(&attr);
         m_bThreadRunning = false;
         m_bCheckInProgress = false;
         onFinishChecksUpdate();
         addMessage("Failed to check for updates.");
         return;
      }
      pthread_attr_destroy(&attr);
   }
}
