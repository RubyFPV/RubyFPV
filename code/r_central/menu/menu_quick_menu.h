#pragma once
#include "menu_objects.h"
#include <map>

class MenuQuickMenu: public Menu
{
   public:
      typedef enum {
            None = 0,
            CycleOSDScreen = 1,
            CycleOSDSize = 2,
            TakePicture = 4,
            VideoRecording = 8,
            ToggleOSD = 16,
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
      static int iPrevSelectedItem;

   public:
      MenuQuickMenu();
      virtual ~MenuQuickMenu();
      virtual void Render();
      virtual void onShow();
      virtual void onSelectItem();


   private:
      std::map<int,t_quick_menu_actions>    m_pItemAction;

      void  addItems();
      void  executeQuickActionToggleRCEnabled();
      void  executeQuickActionCameraProfileSwitch();
      void  executeQuickActionOSDSize();

};
