#pragma once
#include "menu_objects.h"


class MenuQuickMenu: public Menu
{
   public:
      typedef enum {
            None = 0,
            CycleOSDScreen = 1,
            CycleOSDSize = 2,
            TakePicture = 4,
            VideoRecording = 8,
            ToggleOSDOff = 16,
            ToggleStatsOff = 32,
            ToggleAllOff = 64,
            RelaySwitch = 128,
            SwitchCameraProfile = 256,
            RCOutputOnOff = 512,
            RotaryEncoderFunction = 1024,
            FreezeOSD= 2048,
            CycleFavoriteVehicles = 4096,
            PITMode = 8192
      } t_quick_menu_actions;

   public:
      MenuQuickMenu();
      virtual ~MenuQuickMenu();
      virtual void Render();
      virtual void onShow();
      virtual void onSelectItem();


   private:
      void addItems();

      int m_iIndexSimpleSetup;
      int m_iIndexVehicle, m_iIndexMyVehicles;
      int m_iIndexSpectator, m_iIndexSearch;
      int m_iIndexController, m_iIndexMedia;
      int m_iIndexSystem;
};
