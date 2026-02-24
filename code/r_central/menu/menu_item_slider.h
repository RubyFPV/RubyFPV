#pragma once
#include "menu_items.h"

class MenuItemSlider: public MenuItem
{
   public:
     MenuItemSlider(const char* title, int min, int max, int mid, float sliderWidth);
     MenuItemSlider(const char* title, const char* tooltip, int min, int max, int mid, float sliderWidth);
     virtual ~MenuItemSlider();

     void enableHalfSteps();
     void setStep(int step);
     void setCurrentValue(int val);
     void setMaxValue(int val);
     int getCurrentValue();
     void setSufix(const char* szSufix);

     virtual void beginEdit();
     virtual void endEdit(bool bCanceled);

     virtual void Render(float xPos, float yPos, bool bSelected, float fWidthSelection);

     virtual void onKeyUp(bool bIgnoreReversion);
     virtual void onKeyDown(bool bIgnoreReversion);
     virtual void onKeyLeft(bool bIgnoreReversion);
     virtual void onKeyRight(bool bIgnoreReversion);

   protected:
      float m_SliderWidth;
      int m_ValueMin;
      int m_ValueMax;
      int m_ValueMid;
      int m_ValueCurrent;
      int m_ValuePrev;
      int m_Step;
      bool m_HalfStepsEnabled;
      char m_szSufix[32];
};
