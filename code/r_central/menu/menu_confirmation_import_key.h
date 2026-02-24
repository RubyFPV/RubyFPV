#pragma once
#include "menu_objects.h"


class MenuConfirmationImportKey: public Menu
{
   public:
      MenuConfirmationImportKey(const char* szTitle, const char* szText, int id);
      virtual ~MenuConfirmationImportKey();
      virtual void onShow();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();

};