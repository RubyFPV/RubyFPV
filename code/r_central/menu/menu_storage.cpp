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
#include "menu_storage.h"
#include "menu_confirmation.h"
#include "menu_controller_recording.h"

#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#include "../../base/flags_video.h"
#include "../../base/hardware_files.h"
#include "../media.h"
#include "../shared_vars.h"
#include "../launchers_controller.h"
#include "../video_playback.h"

const char* s_szWarningFreeDiskSpace = "You are running low on free storage space. Move your media files to a USB memory stick.";

MenuStorage::MenuStorage(void)
:Menu(MENU_ID_STORAGE, L("Media & Storage"), NULL)
{
   m_Width = 0.68;
   m_Height = 0.66;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.12;
   m_VideoInfoFilesCount = 0;
   m_PicturesFilesCount = 0;
   m_UIFilesPage = 0;
   m_UIFilesPerPage = 8;
   m_UIFilesColumns = 2;
   m_StaticMenuItemsCount = 0;
   m_StaticMenuItemsCountBeforeUIFiles = 0;
   m_ViewScreenShotIndex = -1;
   m_ScreenshotImageId = MAX_U32;
   m_pPopupProgress = NULL;
   m_uMustRefreshTime = 0;
   m_IndexRecordingOptions = -1;
}

MenuStorage::~MenuStorage()
{
   if ( MAX_U32 != m_ScreenshotImageId )
      g_pRenderEngine->freeImage(m_ScreenshotImageId);
   m_ScreenshotImageId = MAX_U32;

   for( int i=0; i<m_VideoInfoFilesCount; i++ )
      free(m_szVideoInfoFiles[i]);
   m_VideoInfoFilesCount = 0;

   for( int i=0; i<m_PicturesFilesCount; i++ )
      free(m_szPicturesFiles[i]);
   m_PicturesFilesCount = 0;
}

void MenuStorage::valuesToUI()
{
}

void MenuStorage::onShow()
{
   char szBuff[1024];
   char szComm[256];
   removeAllTopLines();
   removeAllItems();

   sprintf(szComm, "chmod 777 %s 2>/dev/null 1>/dev/null", FOLDER_MEDIA);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "chmod 777 %s* 2>/dev/null 1>/dev/null", FOLDER_MEDIA);
   hw_execute_bash_command(szComm, NULL);
   
   media_scan_files();

   #if defined(HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA)
   sprintf(szComm, "df -m %s | tail -n 1", FOLDER_MEDIA);
   #endif

   if ( 1 == hw_execute_bash_command_raw(szComm, szBuff) )
   {
      char szTemp[1024];
      long lb, lu, lf;
      sscanf(szBuff, "%s %ld %ld %ld", szTemp, &lb, &lu, &lf);
      m_MemUsed = lu;
      m_MemFree = lf;
   }

   ruby_signal_alive();
   buildFilesListPictures();
   buildFilesListVideo();

   m_UIFilesPage = 0;
   m_IndexCopy = -1;
   m_IndexMove = -1;
   m_IndexDelete = -1;
   m_MainItemsCount = 5;

   m_IndexCopy = addMenuItem(new MenuItem(L("Copy media files to USB memory stick"), L("Copy your screenshots and videos to an external USB memory stick.")));
   m_IndexMove = addMenuItem(new MenuItem(L("Move media files to USB memory stick"), L("Move your screenshots and videos to an external USB memory stick.")));
   m_IndexDelete = addMenuItem(new MenuItem(L("Delete all files"), L("Deletes all videos, pictures from the SD Card.")));
   
   sprintf(szBuff, L("View Screenshots (%d)"), media_get_screenshots_count());
   m_IndexViewPictures = addMenuItem(new MenuItem(szBuff, L("View the screenshots")));
   if ( 0 == media_get_screenshots_count() )
      m_pMenuItems[m_IndexViewPictures]->setEnabled(false);
   else
      m_pMenuItems[m_IndexViewPictures]->showArrow();

   m_IndexRecordingOptions =  addMenuItem(new MenuItem(L("Recording Options"), L("Change recording options.")));
   m_pMenuItems[m_IndexRecordingOptions]->showArrow();

   addMenuItem(new MenuItem(L("Prev Page"),""));
   addMenuItem(new MenuItem(L("Next Page"),""));

   m_StaticMenuItemsCountBeforeUIFiles = m_MainItemsCount;
   m_StaticMenuItemsCount = m_StaticMenuItemsCountBeforeUIFiles+2; // Added PageUp/Down

   if ( m_VideoInfoFilesCount <= m_UIFilesPerPage )
   {
      m_pMenuItems[m_StaticMenuItemsCount-2]->setEnabled(false);
      m_pMenuItems[m_StaticMenuItemsCount-1]->setEnabled(false);
   }

   /*
   m_ExtraItemsHeight = 0.5*height_text;
   m_ExtraItemsHeight += 2.4 * height_text *(1.0+MENU_ITEM_SPACING);

   if ( m_MemFree < 1000 )
      m_ExtraItemsHeight += height_text *(1.0+2.0*MENU_TEXTLINE_SPACING);

   m_ExtraItemsHeight += height_text *(1.2+MENU_ITEM_SPACING) * (m_UIFilesPerPage / m_UIFilesColumns);
   m_ExtraItemsHeight += height_text * 1.5;
   */
   Menu::onShow();

   if ( g_bIsVideoRecording )
   {
      MenuConfirmation* pMC = new MenuConfirmation("Recording in progress", "A recording is in progress. Do you want to stop it?", 1);
      add_menu_to_stack(pMC);
   }
}


void MenuStorage::Render()
{
   if ( -1 != m_ViewScreenShotIndex )
   {
      g_pRenderEngine->drawImage(0, 0, 1,1, m_ScreenshotImageId);
      double cColor[] = {0,0,0,0.7};
      g_pRenderEngine->setColors(cColor, 0.9);
      g_pRenderEngine->drawRoundRect(0.03, 0.03, 0.32, 0.1, 0.02);
      g_pRenderEngine->setColors(get_Color_MenuText());

      char szBuff[128];
      sprintf(szBuff, "Screenshot %d of %d", m_ViewScreenShotIndex+1, media_get_screenshots_count() );
      g_pRenderEngine->drawText(0.05, 0.046, g_idFontMenuLarge, szBuff );
      sprintf(szBuff, "Press [+/-] to navigate and [Back] to close");
      g_pRenderEngine->drawText(0.05, 0.084, g_idFontMenu, szBuff );
      return;
   }

   RenderPrepare();
   float y0 = RenderFrameAndTitle();
   float y = y0;
   float x = m_xPos+m_sfMenuPaddingX;
   float w = 0;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   char szBuff[1024];
  
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(0);

   if ( m_MemFree >= 1024 )
      sprintf(szBuff, "%d Gb ", (int)m_MemFree/1024);
   else
      sprintf(szBuff, "%d Mb ", (int)m_MemFree);
   g_pRenderEngine->drawText(x+w, y, g_idFontMenu, szBuff );
   w += g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
   g_pRenderEngine->drawText(x+w, y, g_idFontMenu, "free out of " );
   w += g_pRenderEngine->textWidth(g_idFontMenu, "free out of " );
   if ( m_MemFree+m_MemUsed >= 1024 )
      sprintf(szBuff, "%d Gb ", (int)(m_MemFree+m_MemUsed)/1024);
   else
      sprintf(szBuff, "%d Mb ", (int)(m_MemFree+m_MemUsed));
   g_pRenderEngine->drawText(x+w, y, g_idFontMenu, szBuff);
   w += g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
   g_pRenderEngine->drawText(x+w, y, g_idFontMenu, "total space." );
   w += g_pRenderEngine->textWidth(g_idFontMenu, "total space." );

   y += height_text *(1.0+MENU_ITEM_SPACING);

   if ( m_MemFree < 1000 )
   {
      y += height_text * MENU_TEXTLINE_SPACING;
      y += g_pRenderEngine->drawMessageLines(m_xPos+m_sfMenuPaddingX, y, s_szWarningFreeDiskSpace, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
      y += height_text * MENU_TEXTLINE_SPACING;
   }
   w = 0;
   sprintf(szBuff, "%d ", media_get_videos_count());
   g_pRenderEngine->drawText(x+w, y, g_idFontMenu, szBuff );
   w += g_pRenderEngine->textWidth(g_idFontMenu, szBuff );
   g_pRenderEngine->drawText(x+w, y, g_idFontMenu, "videos and " );
   w += g_pRenderEngine->textWidth(g_idFontMenu, "videos and " );

   sprintf(szBuff, "%d ", media_get_screenshots_count());
   g_pRenderEngine->drawText(x+w, y, g_idFontMenu, szBuff );
   w += g_pRenderEngine->textWidth(g_idFontMenu, szBuff );
   g_pRenderEngine->drawText(x+w, y, g_idFontMenu, "screenshots stored on persistent media storage." );
   w += g_pRenderEngine->textWidth(g_idFontMenu, "screenshots stored on persistent media storage." );

   y += height_text *(1.0+3.0*MENU_ITEM_SPACING);

   for( int i=0; i<m_StaticMenuItemsCountBeforeUIFiles; i++ )
      y += RenderItem(i,y)*1.1;

   y += height_text*0.6;

   float xpi = x;
   float ypi = y;
   float heightItem = height_text * (1.0+4.0*MENU_ITEM_SPACING);
   float widthItem = m_Width/m_UIFilesColumns - 2*m_sfMenuPaddingX;

   int index = m_UIFilesPerPage * m_UIFilesPage;
   for( int i=0; i<m_UIFilesPerPage; i++ )
   {
      if ( index >= m_VideoInfoFilesCount )
         break;
      if ( i == m_UIFilesPerPage/2 )
      {
         xpi = x + m_Width/m_UIFilesColumns - m_sfMenuPaddingX;
         ypi = y;
      }

      if ( m_SelectedIndex == i+m_StaticMenuItemsCountBeforeUIFiles )
      {
         g_pRenderEngine->setColors(get_Color_MenuItemSelectedBg());
         g_pRenderEngine->drawRoundRect(xpi-m_sfSelectionPaddingX, ypi-m_sfSelectionPaddingY, widthItem + 2.0*m_sfSelectionPaddingX, height_text+2.0*m_sfSelectionPaddingY, MENU_ROUND_MARGIN*m_sfMenuPaddingY);
         g_pRenderEngine->drawRoundRect(xpi+widthItem + 2.0 * m_sfSelectionPaddingX- 0.045*m_sfScaleFactor, ypi+height_text - 2.0*m_sfSelectionPaddingY, 0.045*m_sfScaleFactor, 1.2*height_text, 0.002*m_sfMenuPaddingY);
         g_pRenderEngine->setColors(get_Color_MenuItemSelectedText());
         g_pRenderEngine->drawTextLeft( xpi+widthItem + 2.0 * m_sfSelectionPaddingX -0.014*m_sfScaleFactor, ypi + height_text*0.8, g_idFontMenu, "Play" );
      }
      else
         g_pRenderEngine->setColors(get_Color_MenuText());

      w = 0.0;
      strcpy(szBuff, m_szVideoInfoFiles[index]);
      szBuff[strlen(szBuff)-5] = 0;
      g_pRenderEngine->drawText(xpi+w, ypi, g_idFontMenu, szBuff );
      w = g_pRenderEngine->textWidth(g_idFontMenu, szBuff );
      w += 0.5*height_text / g_pRenderEngine->getAspectRatio();
      sprintf(szBuff, "%02d:%02d",m_VideoFilesDuration[index]/60, m_VideoFilesDuration[index]%60);
      g_pRenderEngine->drawText(xpi+w, ypi, g_idFontMenu, szBuff );
      w += g_pRenderEngine->textWidth(g_idFontMenu, szBuff );
      w += 0.5*height_text / g_pRenderEngine->getAspectRatio();
      char szRes[64];
      strcpy(szRes, "1080p");
      if ( m_VideoFilesHeight[index] < 800 )
         strcpy(szRes, "720p");
      else if ( m_VideoFilesHeight[index] < 1100 )
         strcpy(szRes, "1080p");
      else if ( m_VideoFilesHeight[index] < 1400 )
         strcpy(szRes, "2k");
      else if ( m_VideoFilesHeight[index] < 1900 )
         strcpy(szRes, "3k");
      else if ( m_VideoFilesHeight[index] < 2400 )
         strcpy(szRes, "4k");

      sprintf(szBuff, "%s %d fps", szRes, m_VideoFilesFPS[index]);
      g_pRenderEngine->drawText(xpi+w, ypi, g_idFontMenu, szBuff );
      w += g_pRenderEngine->textWidth(g_idFontMenu, szBuff );
      ypi += heightItem;
      index++;
   }

   y += height_text *(1.2+MENU_ITEM_SPACING) * (m_UIFilesPerPage / m_UIFilesColumns);
   y += height_text * 1.5;

   g_pRenderEngine->setColors(get_Color_MenuText());

   float maxWidth = getUsableWidth();
   
   int maxMenuIndex = m_StaticMenuItemsCount-1;
   int indexStartThisPage = m_UIFilesPerPage * m_UIFilesPage;
   if ( m_VideoInfoFilesCount > indexStartThisPage + m_UIFilesPerPage )
      maxMenuIndex += m_UIFilesPerPage;
   else
      maxMenuIndex += m_VideoInfoFilesCount - indexStartThisPage;

   //y += height_text*0.4;

   sprintf(szBuff, "Page %d of %d", m_UIFilesPage+1,1+(m_VideoInfoFilesCount/m_UIFilesPerPage));
   g_pRenderEngine->drawTextLeft(x+maxWidth-2*m_sfMenuPaddingX, y, g_idFontMenu, szBuff); 

   m_pMenuItems[m_StaticMenuItemsCount-2]->Render(m_RenderXPos + m_sfMenuPaddingX, y, m_SelectedIndex == (maxMenuIndex-1), m_fSelectionWidth*0.34);
   m_pMenuItems[m_StaticMenuItemsCount-1]->Render(m_RenderXPos + 2.0*m_sfMenuPaddingX + m_fSelectionWidth*0.34, y, m_SelectedIndex == (maxMenuIndex), m_fSelectionWidth*0.34);

   g_pRenderEngine->setColors(get_Color_MenuText());
}


void MenuStorage::onMoveUp(bool bIgnoreReversion)
{
   if ( m_ViewScreenShotIndex != -1 )
   {
      m_ViewScreenShotIndex++;
      if ( m_ViewScreenShotIndex >= media_get_screenshots_count() )
         m_ViewScreenShotIndex = 0;

      char szFile[MAX_FILE_PATH_SIZE];
      snprintf(szFile, sizeof(szFile)/sizeof(szFile[0]), "%s%s", FOLDER_MEDIA, m_szPicturesFiles[m_ViewScreenShotIndex]); 
      g_pRenderEngine->freeImage(m_ScreenshotImageId);
      m_ScreenshotImageId = g_pRenderEngine->loadImage(szFile);

      return;
   }

   int maxMenuIndex = m_StaticMenuItemsCount-1;
   int indexStartThisPage = m_UIFilesPerPage * m_UIFilesPage;
   if ( m_VideoInfoFilesCount > indexStartThisPage + m_UIFilesPerPage )
      maxMenuIndex += m_UIFilesPerPage;
   else
      maxMenuIndex += m_VideoInfoFilesCount - indexStartThisPage;

   if ( m_SelectedIndex > 0 )
      m_SelectedIndex--;
   else
      m_SelectedIndex = maxMenuIndex;
   
   onFocusedItemChanged();
}

void MenuStorage::onMoveDown(bool bIgnoreReversion)
{ 
   if ( m_ViewScreenShotIndex != -1 )
   {
      m_ViewScreenShotIndex--;
      if ( m_ViewScreenShotIndex <= 0 )
         m_ViewScreenShotIndex = media_get_screenshots_count()-1;

      char szFile[MAX_FILE_PATH_SIZE];
      snprintf(szFile, sizeof(szFile)/sizeof(szFile[0]), "%s%s", FOLDER_MEDIA, m_szPicturesFiles[m_ViewScreenShotIndex]); 
      g_pRenderEngine->freeImage(m_ScreenshotImageId);
      m_ScreenshotImageId = g_pRenderEngine->loadImage(szFile);

      return;
   }

   int maxMenuIndex = m_StaticMenuItemsCount-1;
   int indexStartThisPage = m_UIFilesPerPage * m_UIFilesPage;
   if ( m_VideoInfoFilesCount > indexStartThisPage + m_UIFilesPerPage )
      maxMenuIndex += m_UIFilesPerPage;
   else
      maxMenuIndex += m_VideoInfoFilesCount - indexStartThisPage;

   if ( m_SelectedIndex < maxMenuIndex )
      m_SelectedIndex++;
   else
      m_SelectedIndex = 0;

   onFocusedItemChanged();
}

void MenuStorage::onMoveLeft(bool bIgnoreReversion)
{
}

void MenuStorage::onMoveRight(bool bIgnoreReversion)
{
}


void MenuStorage::onFocusedItemChanged()
{
   if ( (0 < m_ItemsCount) && (m_SelectedIndex >= 0) && (m_SelectedIndex < m_ItemsCount) && (NULL != m_pMenuItems[m_SelectedIndex]) )
      setTooltip( m_pMenuItems[m_SelectedIndex]->getTooltip() );
}


void MenuStorage::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);
   if ( 1 != returnValue )
      return;

   if ( 1 == iChildMenuId/1000 )
   {
      if ( g_bIsVideoRecording )
      {
         ruby_stop_recording();
         m_uMustRefreshTime = g_TimeNow + 5000;
      }
      return;
   }

   if ( 5 == iChildMenuId/1000 )
   {
      log_line("Confirmed deletion of all media files.");
      char szComm[256];
      if ( 0 < strlen(FOLDER_MEDIA) )
      {
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s* /dev/null 2>&1", FOLDER_MEDIA);
         hw_execute_bash_command(szComm, NULL);   
      }
      onShow();
      return;
   }
}

int MenuStorage::onBack()
{
   if ( g_bIsVideoPlaying )
   {
      video_playback_stop();
      hardware_sleep_ms(50);
      return 1;
   }
   if ( m_ViewScreenShotIndex != -1 )
   {
      if ( MAX_U32 != m_ScreenshotImageId )
         g_pRenderEngine->freeImage(m_ScreenshotImageId);
      m_ScreenshotImageId = MAX_U32;

      m_ViewScreenShotIndex = -1;
      hardware_sleep_ms(50);
      return 1;
   }
   return Menu::onBack();
}

void MenuStorage::buildFilesListPictures()
{
   DIR *d;
   struct dirent *dir;

   for( int i=0; i<m_PicturesFilesCount; i++ )
      free(m_szPicturesFiles[i]);
   m_PicturesFilesCount = 0;

   d = opendir(FOLDER_MEDIA);
   if (d)
   {
      while ((dir = readdir(d)) != NULL)
      {
         if ( strlen(dir->d_name) < 4 )
            continue;
         ruby_signal_alive();

         bool match = false;
         if ( strncmp(dir->d_name, FILE_FORMAT_SCREENSHOT, 6) == 0 )
            match = true;
         if ( ! match )
            continue;
         if ( m_PicturesFilesCount >= MAX_STORAGE_MENU_FILES )
            continue;
         m_szPicturesFiles[m_PicturesFilesCount] = (char*)malloc(strlen(dir->d_name)+1);
         if ( NULL == m_szPicturesFiles[m_PicturesFilesCount] )
            break;
         strcpy(m_szPicturesFiles[m_PicturesFilesCount], dir->d_name);
         m_PicturesFilesCount++;
      }
      closedir(d);
   }
   else
      log_softerror_and_alarm("Failed to open media dir to search for pictures.");
   log_line("Menu Storage: Found %d pictures on storage", m_PicturesFilesCount);
}

void MenuStorage::buildFilesListVideo()
{
   DIR *d;
   FILE* fd;
   struct dirent *dir;
   char szBuff[1024];

   for( int i=0; i<m_VideoInfoFilesCount; i++ )
      free(m_szVideoInfoFiles[i]);
   m_VideoInfoFilesCount = 0;

   d = opendir(FOLDER_MEDIA);
   if (d)
   {
      while ((dir = readdir(d)) != NULL)
      {
         if ( strlen(dir->d_name) < 4 )
            continue;

         ruby_signal_alive();

         bool match = false;
         if ( dir->d_name[strlen(dir->d_name)-4] == 'i' )
         if ( dir->d_name[strlen(dir->d_name)-3] == 'n' )
         if ( dir->d_name[strlen(dir->d_name)-2] == 'f' )
         if ( dir->d_name[strlen(dir->d_name)-1] == 'o' )
         if ( strncmp(dir->d_name, FILE_FORMAT_VIDEO_INFO, 5) == 0 )
            match = true;
         if ( ! match )
            continue;

         if ( m_VideoInfoFilesCount >= MAX_STORAGE_MENU_FILES )
            continue;
         m_szVideoInfoFiles[m_VideoInfoFilesCount] = (char*)malloc(strlen(dir->d_name)+1);
         if ( NULL == m_szVideoInfoFiles[m_VideoInfoFilesCount] )
         {
            log_softerror_and_alarm("Failed to allocate memory for video file info.");
            break;
         }
         strcpy(m_szVideoInfoFiles[m_VideoInfoFilesCount], dir->d_name);
         sprintf(szBuff, "%s%s", FOLDER_MEDIA, m_szVideoInfoFiles[m_VideoInfoFilesCount]);
         fd = fopen(szBuff, "r");
         if ( NULL == fd )
         {
            log_softerror_and_alarm("Failed to open video info file: [%s]", szBuff);
            continue;
         }
         if ( 3 != fscanf(fd, "%s %d %d", szBuff, &(m_VideoFilesFPS[m_VideoInfoFilesCount]), &(m_VideoFilesDuration[m_VideoInfoFilesCount])) )
         {
            log_softerror_and_alarm("Failed to read video info file: [%s], invalid format.", szBuff);
            fclose(fd);
            continue;
         }
         if ( 2 != fscanf(fd, "%d %d", &(m_VideoFilesWidth[m_VideoInfoFilesCount]), &(m_VideoFilesHeight[m_VideoInfoFilesCount])) )
         {
            log_softerror_and_alarm("Failed to read video info file: [%s], invalid format.", szBuff);
            fclose(fd);
            continue;
         }
         if ( 1 != fscanf(fd, "%d", &(m_VideoFilesType[m_VideoInfoFilesCount])) )
         {
            log_softerror_and_alarm("Failed to read video info file video type: [%s], invalid format.", szBuff);
            m_VideoFilesType[m_VideoInfoFilesCount] = VIDEO_TYPE_H264;
         }
         fclose(fd);

         m_VideoInfoFilesCount++;
      }
      closedir(d);
   }
   else
      log_softerror_and_alarm("Failed to open media dir to search for videos.");
   log_line("Menu Storage: Found %d videos on storage", m_VideoInfoFilesCount);
}

void MenuStorage::movePictures(bool bDelete)
{
   char szCommand[1024];
   char szFile[MAX_FILE_PATH_SIZE];
   char szSrcFile[MAX_FILE_PATH_SIZE];

   if ( bDelete )
      log_line("Moving pictures media files...");
   else
      log_line("Copying pictures media files...");

   if ( bDelete )
      m_pPopupProgress->setTitle("Moving screenshots. Please wait...");
   else
      m_pPopupProgress->setTitle("Copying screenshots. Please wait...");

   render_all(get_current_timestamp_ms());

   buildFilesListPictures();

   strcpy(szFile, FOLDER_USB_MOUNT);
   strcat(szFile, "Ruby");
   snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "mkdir -p %s", szFile );
   hw_execute_bash_command(szCommand, NULL);

   for( int i=0; i<m_PicturesFilesCount; i++ )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_TimeNowMicros = get_current_timestamp_micros();
      ruby_signal_alive();
      ruby_processing_loop(true);
      render_all(get_current_timestamp_ms());

      strcpy(szSrcFile, FOLDER_MEDIA);
      strcat(szSrcFile, m_szPicturesFiles[i]);
      if ( -1 == access(szSrcFile, R_OK) )
         continue;

      snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "rm -rf %s/Ruby/%s", FOLDER_USB_MOUNT, m_szPicturesFiles[i]);
      hw_execute_bash_command(szCommand, NULL);

      if ( bDelete )
         snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "mv %s %s/Ruby/%s", szSrcFile, FOLDER_USB_MOUNT, m_szPicturesFiles[i]);
      else
         snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "cp %s %s/Ruby/%s", szSrcFile, FOLDER_USB_MOUNT, m_szPicturesFiles[i]);
      hw_execute_bash_command(szCommand, NULL);
   }
}

bool MenuStorage::moveVideos(bool bDelete)
{
   char szFile[MAX_FILE_PATH_SIZE];
   char szSrcFile[MAX_FILE_PATH_SIZE];
   char szOutFile[MAX_FILE_PATH_SIZE];
   char szCommand[1024];
   char szInfo[256];
   FILE* fd = NULL;
   int fps = 0, duration = 0;

   if ( bDelete )
      log_line("Moving video media files...");
   else
      log_line("Copying video media files...");

   if ( bDelete )
      sprintf(szInfo, "Moving videos to USB memory stick [%s]. Please wait...", hardware_get_mounted_usb_name());
   else
      sprintf(szInfo, "Copying videos to USB memory stick [%s]. Please wait...", hardware_get_mounted_usb_name());

   m_pPopupProgress->setTitle(szInfo);
   render_all(get_current_timestamp_ms());

   buildFilesListVideo();
   bool bHadErrors = false;

   strcpy(szFile, FOLDER_USB_MOUNT);
   strcat(szFile, "Ruby");
   snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "mkdir -p %s", szFile );
   hw_execute_bash_command(szCommand, NULL);

   for( int i=0; i<m_VideoInfoFilesCount; i++ )
   {
      sprintf(szInfo, "Processing video %d of %d. Please wait...", i+1, m_VideoInfoFilesCount);
      m_pPopupProgress->setTitle(szInfo);

      g_TimeNow = get_current_timestamp_ms();
      g_TimeNowMicros = get_current_timestamp_micros();
      ruby_signal_alive();
      ruby_processing_loop(true);
      render_all(get_current_timestamp_ms());

      snprintf(szFile, sizeof(szFile)/sizeof(szFile[0]), "%s%s", FOLDER_MEDIA, m_szVideoInfoFiles[i]);
      fd = fopen(szFile, "r");
      if ( NULL == fd )
         break;
      if ( 3 != fscanf(fd, "%s %d %d", szSrcFile, &fps, &duration ) )
      {
         fclose(fd);
         break;
      }
      fclose(fd);

      int iFreeSpaceKb = hardware_get_free_space_kb();
      snprintf(szFile, sizeof(szFile)/sizeof(szFile[0]), "%s%s", FOLDER_MEDIA, szSrcFile);
      long lSizeVideo = 0;
      fd = fopen(szFile, "rb");
      if ( NULL != fd )
      {
         fseek(fd, 0, SEEK_END);
         lSizeVideo = ftell(fd);
         fseek(fd, 0, SEEK_SET);
         fclose(fd);
      }
      log_line("Video file (%s) size: %d kb, free space: %d kb", szFile, lSizeVideo/1000, iFreeSpaceKb);
      if ( iFreeSpaceKb < ((lSizeVideo/1000)*5)/3 )
      {
         ruby_signal_alive();
         snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "Not enough free space to process video file %s; %d Mb free.", szSrcFile, (int)iFreeSpaceKb/1000);
         m_pPopupProgress->addLine(szCommand);
         bHadErrors = true;
         continue;
      }

      strcpy(szOutFile, szSrcFile);
      hardware_file_replace_extension(szOutFile, "mp4");
      snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "rm -rf %sRuby/%s", FOLDER_USB_MOUNT, szOutFile);
      hw_execute_bash_command(szCommand, NULL);
      snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "rm -rf %s%s", FOLDER_RUBY_TEMP, szOutFile);
      hw_execute_bash_command(szCommand, NULL);

      char szFileOSDIn[MAX_FILE_PATH_SIZE];
      char szFileOSDOut[MAX_FILE_PATH_SIZE];

      strcpy(szFileOSDIn, m_szVideoInfoFiles[i]);
      strcpy(szFileOSDOut, m_szVideoInfoFiles[i]);
      hardware_file_replace_extension(szFileOSDIn, "osd");
      hardware_file_replace_extension(szFileOSDOut, "osd");
      if ( bDelete )
         snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "mv -f %s%s %sRuby/%s 2>/dev/null", FOLDER_MEDIA, szFileOSDIn, FOLDER_USB_MOUNT, szFileOSDOut);
      else
         snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "cp -rf %s%s %sRuby/%s 2>/dev/null", FOLDER_MEDIA, szFileOSDIn, FOLDER_USB_MOUNT, szFileOSDOut);
      hw_execute_bash_command(szCommand, NULL);

      hardware_file_replace_extension(szFileOSDIn, "srt");
      hardware_file_replace_extension(szFileOSDOut, "srt");
      if ( bDelete )
         snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "mv -f %s%s %sRuby/%s 2>/dev/null", FOLDER_MEDIA, szFileOSDIn, FOLDER_USB_MOUNT, szFileOSDOut);
      else
         snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "cp -rf %s%s %sRuby/%s 2>/dev/null", FOLDER_MEDIA, szFileOSDIn, FOLDER_USB_MOUNT, szFileOSDOut);
      hw_execute_bash_command(szCommand, NULL);

      snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "./ruby_video_proc %s%s %s%s &", FOLDER_MEDIA, m_szVideoInfoFiles[i], FOLDER_RUBY_TEMP, szOutFile);
      hw_execute_bash_command(szCommand, NULL);
      
      g_TimeNow = get_current_timestamp_ms();
      u32 uTimeStart = g_TimeNow;

      log_line("Waiting for video processing...");

      // Wait a little for processing process to start
      while ( get_current_timestamp_ms() < uTimeStart + 2000 )
      {
         hardware_sleep_ms(200);
         g_TimeNow = get_current_timestamp_ms();
         ruby_signal_alive();
         ruby_processing_loop(true);
         render_all(get_current_timestamp_ms());
      }

      // Wait for processing process to finish
      while ( ! g_bQuit )
      {
         hardware_sleep_ms(300);
         g_TimeNow = get_current_timestamp_ms();
         ruby_signal_alive();
         ruby_processing_loop(true);
         render_all(get_current_timestamp_ms());
         if ( hw_process_exists("ruby_video_proc") )
            log_line("Waiting for video processing to finish...");
         else
            break;
      }

      log_line("Finished processing video %s", m_szVideoInfoFiles[i]);
      hardware_sleep_ms(100);
      
      snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "mv -f %s%s %sRuby/%s", FOLDER_RUBY_TEMP, szOutFile, FOLDER_USB_MOUNT, szOutFile);
      hw_execute_bash_command(szCommand, NULL);
      
      snprintf(szFile, sizeof(szFile)/sizeof(szFile[0]), "%sRuby/%s", FOLDER_USB_MOUNT, szOutFile);
      if ( access(szFile, R_OK) == -1 )
         log_softerror_and_alarm("Failed to access the output video file [%s]. Not written to USB stick", szFile);
      else
      {
         long lSizeVideo = 0;
         fd = fopen(szFile, "rb");
         if ( NULL != fd )
         {
            fseek(fd, 0, SEEK_END);
            lSizeVideo = ftell(fd);
            fseek(fd, 0, SEEK_SET);
            fclose(fd);
         }
         log_line("Final USB stick output video file [%s] size: %d bytes", szFile, (int)lSizeVideo);
      }

      if ( bDelete )
         sprintf(szInfo, "Moving video %d of %d to USB stick [%s]. Please wait...", i+1, m_VideoInfoFilesCount, hardware_get_mounted_usb_name());
      else
         sprintf(szInfo, "Copying video %d of %d to USB stick [%s]. Please wait...", i+1, m_VideoInfoFilesCount, hardware_get_mounted_usb_name());

      m_pPopupProgress->setTitle(szInfo);

      g_TimeNow = get_current_timestamp_ms();
      g_TimeNowMicros = get_current_timestamp_micros();
      ruby_signal_alive();
      ruby_processing_loop(true);
      render_all(get_current_timestamp_ms());

      hardware_sleep_ms(200);

      if ( bDelete )
      {
         char szComm2[256];
         snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "rm -rf %s%s", FOLDER_MEDIA, m_szVideoInfoFiles[i]);
         strcpy(szComm2, szCommand);
         strcat(szComm2, " 2>/dev/null");
         hw_execute_bash_command(szComm2, NULL);
         hardware_file_replace_extension(szCommand, "h26*");
         strcpy(szComm2, szCommand);
         strcat(szComm2, " 2>/dev/null");
         hw_execute_bash_command(szComm2, NULL);
         hardware_file_replace_extension(szCommand, "osd");
         strcpy(szComm2, szCommand);
         strcat(szComm2, " 2>/dev/null");
         hw_execute_bash_command(szComm2, NULL);
         hardware_file_replace_extension(szCommand, "srt");
         strcpy(szComm2, szCommand);
         strcat(szComm2, " 2>/dev/null");
         hw_execute_bash_command(szComm2, NULL);
      }
   }

   ruby_signal_alive();
   sync();
   ruby_signal_alive();
   hardware_sleep_ms(100);
   return bHadErrors;
}

bool MenuStorage::flowCopyMoveFiles(bool bDeleteToo)
{
   char szBuff[256];
   char szCommand[256];

   if ( NULL != m_pPopupProgress )
      popups_remove(m_pPopupProgress);
   m_pPopupProgress = new Popup("Mounting USB memory stick...",0.28, 0.32, 0.37, 300);
   m_pPopupProgress->setCentered();
   popups_add_topmost(m_pPopupProgress);

   ruby_signal_alive();
   ruby_processing_loop(true);
   render_all(g_TimeNow);
   ruby_signal_alive();

   sprintf(szCommand, "mkdir -p %s", FOLDER_USB_MOUNT);
   hw_execute_bash_command(szCommand, NULL);
   sprintf(szCommand, "chmod -R 777 %s", FOLDER_USB_MOUNT);
   hw_execute_bash_command(szCommand, NULL);

   ruby_pause_watchdog("copy media files");

   int iMountRes = hardware_try_mount_usb();
   if ( 1 != iMountRes )
   {
      ruby_signal_alive();
      ruby_processing_loop(true);
      render_all(g_TimeNow);
      ruby_signal_alive();
      if ( 0 == iMountRes )
         m_pPopupProgress->setTitle("No USB memory stick available.");
      else
      {
         if ( -1 == iMountRes )
            iMountRes = hardware_try_mount_usb();
         if ( 1 != iMountRes )
            m_pPopupProgress->setTitle("USB memory stick available. Try again.");
      }

      ruby_signal_alive();
      ruby_processing_loop(true);
      render_all(g_TimeNow);
      ruby_signal_alive();

      if ( 1 != iMountRes )
      {
         ruby_resume_watchdog("copy media failed");
         ruby_signal_alive();
         m_pPopupProgress->setTimeout(5);
         return false;
      }
   }
   ruby_signal_alive();
   ruby_processing_loop(true);
   render_all(g_TimeNow);
   ruby_signal_alive();

   movePictures(bDeleteToo);
   bool bHadErrors = moveVideos(bDeleteToo);

   if ( bDeleteToo )
      onShow();

   char szUSBDeviceName[128];
   strncpy(szUSBDeviceName, hardware_get_mounted_usb_name(), 127);

   hardware_unmount_usb();
   ruby_signal_alive();
   sync();
   ruby_signal_alive();
   ruby_resume_watchdog("copy media finished");
   invalidate();
   sprintf(szBuff, "Done. It's safe now to remove the USB memory stick [%s].", szUSBDeviceName);
   m_pPopupProgress->setTitle(szBuff);
   if ( bHadErrors )
   {
      m_pPopupProgress->addLine(szBuff);
      m_pPopupProgress->setTimeout(10);
   }
   else
     m_pPopupProgress->setTimeout(5);
   return true;
}

bool MenuStorage::periodicLoop()
{
   if ( m_uMustRefreshTime != 0 )
   if ( (g_TimeNow > m_uMustRefreshTime) || (! g_bIsVideoProcessing) )
   {
      m_uMustRefreshTime = 0;
      menu_refresh_all_menus();
   }
   return false;
}

void MenuStorage::onSelectItem()
{
   if ( m_ViewScreenShotIndex != -1 )
      return;

   if ( g_bIsVideoPlaying )
   {
      char szComm[256];
      if ( access(CONFIG_FILE_FULLPATH_PAUSE_VIDEO_PLAYER, R_OK) != -1 )
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s", CONFIG_FILE_FULLPATH_PAUSE_VIDEO_PLAYER);
      else
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "touch %s", CONFIG_FILE_FULLPATH_PAUSE_VIDEO_PLAYER);
      hw_execute_bash_command(szComm, NULL);
      return;
   }

   Menu::onSelectItem();

   int maxMenuIndex = m_StaticMenuItemsCount-1;
   int indexStartThisPage = m_UIFilesPerPage * m_UIFilesPage;
   if ( m_VideoInfoFilesCount > indexStartThisPage + m_UIFilesPerPage )
      maxMenuIndex += m_UIFilesPerPage;
   else
      maxMenuIndex += m_VideoInfoFilesCount - indexStartThisPage;

   log_line("MenuStorage: selected item index %d, top items: %d, static items: %d, max selectable index: %d, video items: %d, page: %d, start index this page: %d",
       m_SelectedIndex, m_StaticMenuItemsCountBeforeUIFiles, m_StaticMenuItemsCount, maxMenuIndex, m_VideoInfoFilesCount, m_UIFilesPage, indexStartThisPage );

   if ( -1 == m_SelectedIndex )
      return;

   if ( m_IndexCopy == m_SelectedIndex )
   {
      flowCopyMoveFiles(false);
      return;
   }
   if ( m_IndexMove == m_SelectedIndex )
   {
      flowCopyMoveFiles(true);   
      return;
   }
   if ( m_IndexDelete == m_SelectedIndex )
   {
      MenuConfirmation* pMC = new MenuConfirmation("Confirmation","Are you sure you want to delete all files?",5);
      add_menu_to_stack(pMC);
      return;
   }

   if ( m_IndexViewPictures == m_SelectedIndex )
   {
      if ( ! m_pMenuItems[m_IndexViewPictures]->isEnabled() )
      {
         addMessage("View screenshots is not supported on Radxa hardware.");
         return;
      }
      m_ViewScreenShotIndex = media_get_screenshots_count()-1;

      char szFile[MAX_FILE_PATH_SIZE];
      snprintf(szFile, sizeof(szFile)/sizeof(szFile[0]), "%s%s", FOLDER_MEDIA, m_szPicturesFiles[m_ViewScreenShotIndex]); 
      log_line("Menu: Loading screenshot: %s", szFile );
      m_ScreenshotImageId = g_pRenderEngine->loadImage(szFile);
      if ( (0 != m_ScreenshotImageId) && (MAX_U32 != m_ScreenshotImageId) )
         log_line("Men: Image loaded ok");
      return;
   }

   if ( m_IndexRecordingOptions == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuControllerRecording());
      return;
   }

   if ( m_SelectedIndex == (maxMenuIndex-1))
   {
      if ( ! m_pMenuItems[m_StaticMenuItemsCount-2]->isEnabled() )
         return;
      if ( m_UIFilesPage <= 0 )
         return;

      m_UIFilesPage--;

      int maxMenuIndex = m_StaticMenuItemsCount-1;
      int indexStartThisPage = m_UIFilesPerPage * m_UIFilesPage;
      if ( m_VideoInfoFilesCount > indexStartThisPage + m_UIFilesPerPage )
         maxMenuIndex += m_UIFilesPerPage;
      else
         maxMenuIndex += m_VideoInfoFilesCount - indexStartThisPage;
      m_SelectedIndex = maxMenuIndex-1;

      return;
   }

   if ( m_SelectedIndex == maxMenuIndex )
   {
      if ( ! m_pMenuItems[m_StaticMenuItemsCount-1]->isEnabled() )
         return;
      int indexStartThisPage = m_UIFilesPerPage * m_UIFilesPage;

      if ( m_VideoInfoFilesCount <= indexStartThisPage + m_UIFilesPerPage )
         return;

      m_UIFilesPage++;

      int maxMenuIndex = m_StaticMenuItemsCount-1;
      indexStartThisPage = m_UIFilesPerPage * m_UIFilesPage;
      if ( m_VideoInfoFilesCount > indexStartThisPage + m_UIFilesPerPage )
         maxMenuIndex += m_UIFilesPerPage;
      else
         maxMenuIndex += m_VideoInfoFilesCount - indexStartThisPage;
      m_SelectedIndex = maxMenuIndex;
      return;
   }

   playVideoFile(m_SelectedIndex);
}

void MenuStorage::playVideoFile(int iMenuItemIndex)
{
   char szFile[MAX_FILE_PATH_SIZE];
   int index = m_UIFilesPerPage * m_UIFilesPage + iMenuItemIndex - m_StaticMenuItemsCountBeforeUIFiles;
   if ( index < 0 || index >= m_VideoInfoFilesCount )
      return;

   snprintf(szFile, sizeof(szFile)/sizeof(szFile[0]), "%s%s", FOLDER_MEDIA, m_szVideoInfoFiles[index]);
   video_playback_play_file(szFile);

   /*
   if ( pairing_isStarted() )
   {
      for( int i=0; i<10; i++ )
      {
         if ( pairing_isRouterReady() )
            break;
         hardware_sleep_ms(500);
      }
      pairing_stop();
      m_bWasPairingStarted = true;
      ruby_signal_alive();
   }
   */

   log_line("Started video playback of info file: (%s%s)", FOLDER_MEDIA, szFile);
}
