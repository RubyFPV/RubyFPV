#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuQuickMenuSettings: public Menu
{
   public:
      MenuQuickMenuSettings();
      virtual void onShow(); 
      virtual void Render();
      virtual void valuesToUI();
      virtual void onSelectItem();

   private:
      MenuItemSelect* m_pItemsSelect[15];

      int m_IndexAllQuickMenu;
      int m_IndexCycleOSDScreen;
      int m_IndexCycleOSDSize;
      int m_IndexTakePicture;
      int m_IndexVideoRecording;
      int m_IndexToggleOSDOff;
      int m_IndexToggleStatsOff;
      int m_IndexToggleAllOff;
      int m_IndexRelaySwitch;
      int m_IndexSwitchCameraProfile;
      int m_IndexRCOutputOnOff;
      int m_IndexRotaryEncoderFunction;
      int m_IndexFreezeOSD;
      int m_IndexCycleFavoriteVehicles;
      int m_IndexPITMode;

      bool m_bMenuQuickMenuSettingsIsOnCustomOption;
};
