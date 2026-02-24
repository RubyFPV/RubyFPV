#pragma once
#include <pthread.h>
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_controller_update.h"

class MenuControllerUpdateNet: public MenuControllerUpdate
{
   public:
      MenuControllerUpdateNet();
      virtual void onShow();
      virtual void valuesToUI();
      virtual void Render();
      virtual bool periodicLoop();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();

      void onFinishCheckAsync(int iResult);
      void onUpdateCounter();

      int m_iOnlineVersionMajor;
      int m_iOnlineVersionMinor;
      bool m_bNeedsUserConsent;
      int m_iUserConsentResult;

   private:
      void addItems();
      void onFinishChecksUpdate();
      MenuItemSelect* m_pItemsSelect[10];
      MenuItemSlider* m_pItemsSlider[5];

      int m_iIndexMenuCheck;
      int m_iIndexMenuBack;

      static u32 m_uImgIdInfo;
      static u32 m_uImgIdInfoEth;
      bool m_bCheckInProgress;
      bool m_bThreadRunning;
      int m_iUpdateStepCounter;
      int m_iUpdateThreadResult;
      pthread_t m_pThreadCheckUpdate;
};