#pragma once
#include "menu_items.h"

#define MAX_MENU_ITEM_RADIO_SELECTIONS 10

class MenuItemRadio: public MenuItem
{
   public:
     MenuItemRadio(const char* title);
     MenuItemRadio(const char* title, const char* tooltip);
     virtual ~MenuItemRadio();

     void removeAllSelections();
     void addSelection(const char* szText);
     void addSelection(const char* szText, const char* szLegend);
     void setSelectedIndex(int index);
     void enableSelectionIndex(int index, bool bEnable);

     void setFocusedIndex(int index);
     int getFocusedIndex();
     int getSelectedIndex();
     int getSelectionsCount();

     void useSmallLegend(bool bSmall);

     virtual bool preProcessKeyUp();
     virtual bool preProcessKeyDown();

     virtual void onClick();
     virtual void onKeyUp(bool bIgnoreReversion);
     virtual void onKeyDown(bool bIgnoreReversion);
     virtual void onKeyLeft(bool bIgnoreReversion);
     virtual void onKeyRight(bool bIgnoreReversion);

     virtual float getItemHeight(float maxWidth);
     virtual float getTitleWidth(float maxWidth);
     virtual float getValueWidth(float maxWidth);

     virtual void Render(float xPos, float yPos, bool bSelected, float fWidthSelection);
     virtual void RenderCondensed(float xPos, float yPos, bool bSelected, float fWidthSelection);

   protected:
      bool setPrevAvailableFocusableItem();
      bool setNextAvailableFocusableItem();

      int m_nSelectedIndex;
      int m_nFocusedIndex;
      int m_nSelectionsCount;
      char* m_szSelections[MAX_MENU_ITEM_RADIO_SELECTIONS];
      char* m_szSelectionsLegend[MAX_MENU_ITEM_RADIO_SELECTIONS];
      bool m_bEnabledSelections[MAX_MENU_ITEM_RADIO_SELECTIONS];

      bool m_bSmallLegend;
      float m_fSelectorWidth;
      float m_fLegendDx;
};
