#pragma once
#include "menu_objects.h"


class MenuInfoProcs: public Menu
{
   public:
      MenuInfoProcs(const char* szinfo);
      virtual ~MenuInfoProcs();
      virtual void onShow();
      virtual void Render();
      virtual void onSelectItem();

      void appendInfo(const char* szInfo);
   protected:
      char m_szInfo[4096];

     float _parseAddDbgProcsInfoLine(float yPos, const char* szDbgInfoLine);
};