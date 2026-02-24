#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "../../base/config.h"
#include "../osd/osd_widgets.h"

class MenuVehicleOSDWidget: public Menu
{
   public:
      MenuVehicleOSDWidget(int iWidgetIndex);
      virtual ~MenuVehicleOSDWidget();
      virtual void valuesToUI();
      virtual void onShow();
      virtual void Render();
      virtual bool periodicLoop();

      virtual void onMoveUp(bool bIgnoreReversion);
      virtual void onMoveDown(bool bIgnoreReversion);
      virtual void onMoveLeft(bool bIgnoreReversion);
      virtual void onMoveRight(bool bIgnoreReversion);

      virtual int onBack();
      virtual void onSelectItem();

   private:
      void stopAction();
      void onPlusAction();
      void onMinusAction();

      MenuItemSlider* m_pItemsSlider[5];
      MenuItemSelect* m_pItemsSelect[MAX_OSD_WIDGET_PARAMS+10];
      MenuItemRange* m_pItemsRange[MAX_OSD_WIDGET_PARAMS];

      int m_nWidgetIndex;
      int m_nWidgetSettingsCount;
      bool m_bIsMovingH;
      bool m_bIsMovingV;
      bool m_bIsResizing;
      bool m_bIsResizingH;
      bool m_bIsResizingV;
      float m_fXOrg;
      float m_fYOrg;
      float m_fWOrg;
      float m_fHOrg;
      float m_fMenuOrgAlpha;

      int m_IndexSettings[MAX_OSD_WIDGET_PARAMS];
      int m_IndexMoveX;
      int m_IndexMoveY;
      int m_IndexResize;
      int m_IndexResizeH;
      int m_IndexResizeV;
};