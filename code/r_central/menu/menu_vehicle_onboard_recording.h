#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"

// Phase 2: Vehicle > Onboard Recording menu.
// Lets the user pick between GS recording and onboard SD recording
// (waybeam_venc dual-VENC mode), and select a quality preset for the
// onboard bitrate. Grayed out when the current vehicle doesn't support
// waybeam.
class MenuVehicleOnboardRecording: public Menu
{
   public:
      MenuVehicleOnboardRecording();
      virtual void onShow();
      virtual void Render();
      virtual void onSelectItem();
      virtual void valuesToUI();

   private:
      void _pushSettingsToVehicle();

      MenuItemSelect* m_pItemsSelect[4];
      int m_IndexTarget;
      int m_IndexQuality;
      bool m_bBackendSupported; // cached at menu open
};
