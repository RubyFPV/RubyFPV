#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_radio.h"
#include "menu_item_checkbox.h"
#include "menu_item_text.h"
#include "../../base/video_capture_res.h"

class MenuVehicleVideo: public Menu
{
   public:
      MenuVehicleVideo();
      virtual ~MenuVehicleVideo();
      void showCompact();
      virtual void onShow();
      virtual void Render();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);  
      virtual void onSelectItem();
      virtual void valuesToUI();
            
   private:
      type_video_capture_resolution_info* m_pVideoResolutions;
      int m_iVideoResolutionsCount;
      int m_IndexRes, m_IndexFPS, m_IndexFPSSelector;
      int m_IndexBidirectional;
      int m_IndexVideoBitrate;
      int m_IndexVideoProfile;
      int m_IndexSaveVideoProfile;
      int m_IndexCompareProfiles;
      int m_IndexVideoCodec;
      int m_IndexExpert;
      int m_IndexVideoLinkMode;
      int m_IndexShowFull;
      MenuItem* m_pMenuItemVideoWarning;
      MenuItemSlider* m_pItemsSlider[20];
      MenuItemSelect* m_pItemsSelect[20];
      MenuItemRadio* m_pItemsRadio[5];

      void addItems();
      void checkAddWarningInMenu();
      int getMaxFPSForCurrentVideoRes(bool* pFound);
      bool isStandardFPS(int iFPS);
      void showFPSWarning(int w, int h, int fps);
      void sendVideoSettings();

      bool m_bShowCompact;
      bool m_bShowCustomFPS;
      bool m_bShowProfileSelectorInline;
};