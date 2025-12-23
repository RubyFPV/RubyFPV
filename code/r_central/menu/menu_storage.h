#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "../popup.h"


#define MAX_STORAGE_MENU_FILES 512

class MenuStorage: public Menu
{
   public:
      MenuStorage();
      virtual ~MenuStorage();
      virtual void onShow(); 
      virtual void Render();
      virtual void valuesToUI();
      virtual void onMoveUp(bool bIgnoreReversion);
      virtual void onMoveDown(bool bIgnoreReversion);
      virtual void onMoveLeft(bool bIgnoreReversion);
      virtual void onMoveRight(bool bIgnoreReversion);
      virtual void onFocusedItemChanged();
      virtual bool periodicLoop();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);  
      virtual int onBack();
      virtual void onSelectItem();

   private:
      void playVideoFile(int iMenuItemIndex);

      MenuItemSelect* m_pItemsSelect[10];
      char* m_szVideoInfoFiles[MAX_STORAGE_MENU_FILES];
      char* m_szPicturesFiles[MAX_STORAGE_MENU_FILES];
      int m_VideoFilesDuration[MAX_STORAGE_MENU_FILES];
      int m_VideoFilesFPS[MAX_STORAGE_MENU_FILES];
      int m_VideoFilesWidth[MAX_STORAGE_MENU_FILES];
      int m_VideoFilesHeight[MAX_STORAGE_MENU_FILES];
      int m_VideoFilesType[MAX_STORAGE_MENU_FILES];
      int m_VideoInfoFilesCount;
      int m_PicturesFilesCount;
      int m_UIFilesPage;
      int m_UIFilesPerPage;
      int m_UIFilesColumns;
      int m_IndexViewPictures;

      long m_MemUsed;
      long m_MemFree;
      bool m_bWasPairingStarted;
      int m_StaticMenuItemsCount;
      int m_StaticMenuItemsCountBeforeUIFiles;

      int m_ViewScreenShotIndex;
      u32 m_ScreenshotImageId;
      Popup* m_pPopupProgress;
      u32 m_uMustRefreshTime;
      int m_IndexCopy;
      int m_IndexMove;
      int m_IndexDelete;
      int m_IndexRecordingOptions;
      int m_MainItemsCount;
      
      void buildFilesListPictures();
      void buildFilesListVideo();
      void movePictures(bool bDelete);
      bool moveVideos(bool bDelete);

      bool flowCopyMoveFiles(bool bDeleteToo);
};