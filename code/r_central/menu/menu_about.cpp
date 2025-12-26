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
#include "menu_controller.h"
#include "menu_text.h"
#include "menu_item_section.h"
#include "menu_about.h"
#include "../osd/osd_common.h"
#include "../../base/hardware.h"

extern u32 g_idIconOpenIPC;

MenuAbout::MenuAbout(void)
:Menu(MENU_ID_ABOUT, L("About"), NULL)
{
   m_Width = 0.46;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.18;

   char szBuff[1024];
   char szOutput[1024];
   char szComm[256];
   char szTemp[128];
   char szFile[MAX_FILE_PATH_SIZE];

   szBuff[0] = 0;
   strcpy(szBuff, "Ruby base version: N/A");

   FILE* fd = try_open_base_version_file(NULL);

   if ( NULL != fd )
   {
      szOutput[0] = 0;
      if ( 1 == fscanf(fd, "%s", szOutput) )
      {
         strcpy(szBuff, L("Ruby base version: "));
         strcat(szBuff, szOutput);
      }
      fclose(fd);
   }

   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_INFO_LAST_UPDATE);
   fd = fopen(szFile, "r");
   if ( NULL == fd )
   {
      strcpy(szFile, FOLDER_BINARIES);
      strcat(szFile, FILE_INFO_LAST_UPDATE);
      fd = fopen(szFile, "r"); 
   }
   if ( NULL != fd )
   {
      szOutput[0] = 0;
      if ( 1 == fscanf(fd, "%s", szOutput) )
      {
         strcat(szBuff, ", last update: ");
         strcat(szBuff, szOutput);
      }
      fclose(fd);
   }
   
   addTopLine(szBuff);

   addTopLine(" ");

   log_line("Menu About: create HW info.");

   hw_execute_bash_command_raw("cat /proc/device-tree/model", szOutput);

   snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Board: %s, ", szOutput);
   
   int temp = hardware_get_cpu_temp();
   int speed = hardware_get_cpu_speed();
   sprintf(szTemp, "CPU: %d Mhz, Temp: %d C", speed, temp); 
   strcat(szBuff, szTemp);
   addTopLine(szBuff);

   addTopLine(" ");
   addTopLine(" ");


   #ifdef HW_PLATFORM_RASPBERRY
   sprintf(szComm, "df -m %s | grep root", FOLDER_BINARIES);
   #endif
   #ifdef HW_PLATFORM_RADXA
   sprintf(szComm, "df -m %s | grep mmc", FOLDER_BINARIES);
   #endif
   if ( 1 == hw_execute_bash_command_raw(szComm, szBuff) )
   {
      char szTemp[1024];
      long lb, lu, lf;
      sscanf(szBuff, "%s %ld %ld %ld", szTemp, &lb, &lu, &lf);
      sprintf(szBuff, "System storage: %ld Mb free out of %ld Mb total.", lf, lu+lf);
      addTopLine(szBuff);
   }
  
   addTopLine(" ");
   addTopLine("---");
   addTopLine(" ");
   addTopLine(L("Ruby system developed by: Petru Soroaga"));
   addTopLine(L("Main contributors: Jessica Severin, Liuli Xia, Ning"));
   addTopLine("");
   
   if ( (NULL != g_pCurrentModel) && g_pCurrentModel->isRunningOnOpenIPCHardware() )
   {
      addTopLine("IP cameras base linux distro support provided by:");
      addTopLine("OpenIPC: https://openipc.org");
      addTopLine("https://github.com/OpenIPC");
      addTopLine("");
   }

   addTopLine(L("For info on the licence terms, check the license.txt file."));
   addTopLine("For more info, questions and suggestions find us on www.rubyfpv.com");
   addTopLine("---");
   addTopLine(" ");
   m_IndexOK = addMenuItem(new MenuItem(L("Ok"), L("Close the menu.")));
}

void MenuAbout::valuesToUI()
{
}

void MenuAbout::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   float yEnd = y;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float iconHeight = 2.0*height_text;
   float iconWidth = iconHeight/g_pRenderEngine->getAspectRatio();
   g_pRenderEngine->drawIcon(m_RenderXPos + m_RenderWidth - m_sfMenuPaddingX - iconWidth - 0.02, m_RenderYPos - iconHeight + 9.4*height_text, iconWidth, iconHeight, g_idIconRuby);
   if ( (NULL != g_pCurrentModel) && g_pCurrentModel->isRunningOnOpenIPCHardware() )
      g_pRenderEngine->drawIcon(m_RenderXPos + m_RenderWidth - m_sfMenuPaddingX - iconWidth - 0.02, yEnd - iconHeight - 7*g_pRenderEngine->textHeight(g_idFontMenu), iconWidth, iconHeight, g_idIconOpenIPC);

   RenderEnd(yTop);
}

bool MenuAbout::periodicLoop()
{
   return false;
}

void MenuAbout::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);
}

void MenuAbout::onSelectItem()
{
   Menu::onSelectItem();

   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   menu_stack_pop(0);
}
