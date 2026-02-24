#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "../../base/config.h"
#include "../../public/settings_info.h"

class MenuVehicleOSDPlugin: public Menu
{
   public:
      MenuVehicleOSDPlugin(int pluginIndex);
      virtual ~MenuVehicleOSDPlugin();
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
      void readPlugin();
      void stopAction();

      void onMinusAction();
      void onPlusAction();

      MenuItemSlider* m_pItemsSlider[5];
      MenuItemSelect* m_pItemsSelect[MAX_PLUGIN_SETTINGS+10];
      MenuItemRange* m_pItemsRange[MAX_PLUGIN_SETTINGS];

      int m_nPluginIndex;
      int m_nPluginSettingsCount;
      bool m_bIsMovingH;
      bool m_bIsMovingV;
      bool m_bIsResizing;
      float m_fXOrg;
      float m_fYOrg;
      float m_fWOrg;
      float m_fHOrg;
      float m_fMenuOrgAlpha;

      int m_IndexSettings[MAX_PLUGIN_SETTINGS];
      int m_IndexMoveX;
      int m_IndexMoveY;
      int m_IndexResize;
};