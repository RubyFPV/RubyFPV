#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_vehicle_radio_link_sik.h"
#include "menu_vehicle_radio_link_elrs.h"

class MenuVehicleRadioLink: public Menu
{
   public:
      MenuVehicleRadioLink(int iRadioLink);
      virtual ~MenuVehicleRadioLink();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onShow();
      virtual int onBack();
      virtual void onVehicleCommandFinished(u32 uCommandId, u32 uCommandType, bool bSucceeded);
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();
      void onChangeRadioConfigFinished(bool bSucceeded);

   private:
      void addMenuItems();
      void addMenuItemFrequencies();
      void addMenuItemsCapabilities();
      void addMenuItemsDataRates();
      void addMenuItemsMCS();

      int convertMCSRateToLegacyRate(int iMCSRate, bool bIsHT40);
      int convertLegacyRateToMCSRate(int iLegacyRate, bool bIsHT40);
      void sendRadioLinkCapabilities(int iRadioLink);
      void sendRadioLinkConfig(int linkIndex);
      void sendRadioLinkConfigParams(type_radio_links_parameters* pRadioLinkParams, bool bCheckVideo);
      void sendNewRadioLinkFrequency(int iVehicleLinkIndex, u32 uNewFreqKhz);

      int m_iVehicleRadioLink;
      int m_iVehicleRadioInterface;

      MenuItemSlider* m_pItemsSlider[20];
      MenuItemSelect* m_pItemsSelect[20];

      u32 m_uControllerSupportedBands;
      u32 m_SupportedChannels[MAX_MENU_CHANNELS];
      int m_SupportedChannelsCount;
      int m_IndexFrequency;
      int m_IndexUsage;
      int m_IndexCapabilities;
      int m_IndexMaxLoad;
      int m_IndexDataRatesType;
      int m_IndexDataRateVideo;
      int m_IndexDataRateDataDownlink;
      int m_IndexDataRateDataUplink;
      int m_IndexHT;
      int m_IndexLDPC;
      int m_IndexSGI;
      int m_IndexSTBC;
      int m_IndexReset;

      bool m_bWaitingConfirmationFromUser;
      bool m_bWaitingVideoChangeConfirmationFromVehicle;
      type_radio_links_parameters m_RadioLinksParamsToApply;
};