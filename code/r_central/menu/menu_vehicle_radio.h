#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_edit.h"
#include "menu_item_slider.h"
#include "../popup_radio_int.h"

class MenuVehicleRadioConfig: public Menu
{
   public:
      MenuVehicleRadioConfig();
      virtual ~MenuVehicleRadioConfig();
      virtual void onAddToStack();
      virtual void onShow();
      virtual int onBack();
      virtual void onFocusedItemChanged();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void Render();
      virtual void valuesToUI();
      virtual void onSelectItem();
            
   private:
      MenuItemSelect* m_pItemsSelect[50];
      MenuItemSlider* m_pItemsSlider[10];
      
      int m_IndexPrioritizeUplink;
      int m_IndexDisableUplink;
      int m_IndexEncryption;
      int m_IndexTxPowers[MAX_RADIO_INTERFACES];
      int m_IndexShowTxPower;
      int m_IndexPitMode;
      int m_IndexRadioConfig;
      int m_IndexOptimizeLinks;
      int m_IndexDevRuntimeInfo;
      int m_IndexFreq[MAX_RADIO_INTERFACES];
      int m_IndexConfigureLinks[MAX_RADIO_INTERFACES];
      u32 m_SupportedChannels[MAX_RADIO_INTERFACES][100];
      int m_SupportedChannelsCount[MAX_RADIO_INTERFACES];

      bool m_bControllerHasKey;
      int m_iMaxPowerMw;

      PopupRadioInterface* m_pPopupRadioInterface;
      int m_iLastLinkAddedPopupFor;
      bool m_bDisablePopupCreation;
      void populate();
      void populateFrequencies();
      void populateTxPowers();
      void sendNewRadioLinkFrequency(int iVehicleLinkIndex, u32 uNewFreqKhz);
      void computeSendPowerToVehicle(int iVehicleLinkIndex);

};