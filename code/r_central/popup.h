#pragma once
#include "../base/base.h"

#define MAX_POPUPS 20
#define MAX_POPUP_LINES 30

#define POPUP_MARGINS 1.0 // as percentage of current font
#define POPUP_ROUND_MARGIN 0.03
#define POPUP_FONT_SIZE 0.021
#define POPUP_TITLE_SCALE 1.2  // percentage of regular line height
extern float POPUP_LINE_SPACING;

class Popup
{
   public:
   Popup(const char* title, float x, float y, float timeoutSec);
   Popup(const char* title, float x, float y, float maxWidth, float timeoutSec);
   Popup(bool bCentered, const char* title, float timeoutSec);
   virtual ~Popup();
   virtual void onShow();
   virtual void Render();

   void setFont(u32 idFont);
   u32 getFontId();
   const char* getTitle();
   void setNoBackground();
   void setBackgroundAlpha(float fAlpha);
   void invalidate();
   int isExpired();
   void resetTimer();
   void setTimeout(float timoutSeconds);
   float getTimeout();
   void setTitle(const char* szTitle);
   virtual void addLine(const char* szLine);
   void setLine(int iLine, const char* szLine);
   char* getLine(int iLine);

   u32 getCreationTime();
   bool hasDisabledAutoRemove();
   void disableAutoRemove();
   int getLinesCount();
   void removeLastLine();
   void removeAllLines();
   void useSmallLines(bool bSmallLines);
   void refresh();
   bool isRenderBelowMenu();
   void setMaxWidth(float width);
   void setFixedWidth(float width);
   void setRenderBelowMenu(bool bBelowMenu);
   void showTimeoutProgress();
   void setCentered();
   void setCenteredTexts();
   void setBottomAlign(bool b);
   void setBottomMargin(float dymargin);
   float getBottomMargin();
   void setCustomPadding(float fPadding);
   void setIconId(u32 idIcon, const double* pColor);
   void setIconId2(u32 idIcon2, const double* pColor2);
   float getRenderHeight();
   void setXPos(float xPos);
   void setYPos(float yPos);

   float m_xPos;
   float m_yPos;

   protected:
      bool m_bDisableAutoRemove;
      bool m_bInvalidated;
      bool m_bBelowMenu;
      bool m_bTopmost;
      bool m_bShowTimeoutBar;
      bool m_bCentered;
      bool m_bCenterTexts;
      bool m_bBottomAlign;
      bool m_bNoBackground;
      bool m_bSmallLines;
      u32 m_uIdFont;
      u32 m_idIcon;
      u32 m_idIcon2;
      double m_ColorIcon[4];
      double m_ColorIcon2[4];
      float m_fIconWidth;
      float m_fIconWidth2;
      float m_fIconHeight;
      float m_fIconHeight2;
      float m_fBackgroundAlpha;
      float m_fBottomMargin;
      float m_fPadding; // as percent 0 to 1 of font height

      float m_fMaxWidth;
      float m_fFixedWidth;
      float m_fPaddingX;
      float m_fPaddingY;
      float m_RenderXPos;
      float m_RenderYPos;
      float m_RenderWidth;
      float m_RenderHeight;
      char m_szTitle[256];
      char* m_szLines[MAX_POPUP_LINES];
      int m_LinesCount;
      float m_fTimeoutSeconds;
      u32 m_uTimeoutEndTime;
      u32 m_StartTime;
      u32 m_uCreatedTime;

      virtual float computeIconsSizes();
      virtual void computeSize();
};

void popups_add(Popup* p);
void popups_add_bottom(Popup* p);
void popups_add_topmost(Popup* p);
bool popups_has_popup(Popup* p);

void popups_remove(Popup* p);
void popups_remove_all(Popup* pExceptionPopup = NULL);
void popups_render();
void popups_render_topmost();
void popups_render_bottom();
void popups_invalidate_all();

int popups_get_count();
int popups_get_topmost_count();
Popup* popups_get_at(int index);
Popup* popups_get_topmost_at(int index);
