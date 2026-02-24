#pragma once
#include "menu_objects.h"

#define M_MAX_TEXT_LINES 1000

class MenuText: public Menu
{
   public:
      MenuText();
      virtual ~MenuText();
      void addTextLine(const char* szText);
      virtual void onShow();
      virtual void Render();
      virtual void onSelectItem();

   protected:
      virtual void computeRenderSizes();

      char* m_szLines[M_MAX_TEXT_LINES];
      int m_LinesCount;
      int m_TopLineIndex;
      float m_fScaleFactor;
};