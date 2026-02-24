#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuControllerUpdate: public Menu
{
   public:
      MenuControllerUpdate();
      virtual void onShow();
      virtual void Render();
      virtual bool periodicLoop();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();

   protected:
      bool m_bDoDefaultHandling;
      void updateControllerSoftware(const char* szUpdateFile);

   private:
      void addItems();
      MenuItemSelect* m_pItemsSelect[10];

      int m_IndexUpdateUSB;
      int m_IndexUpdateNet;
      int m_iMustStartUpdate;
      bool m_bWaitingForUserFinishUpdateConfirmation;
};