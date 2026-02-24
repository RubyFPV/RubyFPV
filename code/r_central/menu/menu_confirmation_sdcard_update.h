#pragma once
#include "menu_objects.h"
#include <pthread.h>


class MenuConfirmationSDCardUpdate: public Menu
{
   public:
      MenuConfirmationSDCardUpdate();
      virtual ~MenuConfirmationSDCardUpdate();
      virtual void onShow();
      virtual bool periodicLoop();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual int onBack();
      virtual void onSelectItem();

   protected:
      bool m_bDoingUpdate;
      bool m_bUpdateFinished;
      pthread_t m_pThreadUpdate;
};