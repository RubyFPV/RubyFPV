/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "menu_search.h"
#include "menu.h"
#include "../../common/models_connect_frequencies.h"

#include "menu_item_text.h"
#include "menu_search_connect.h"
#include "../process_router_messages.h"
#include "../../base/utils.h"
#include "../link_watch.h"

static int s_LastSearchedFrequency = 0;
static u32 s_uVideoReceivedOnFreqKhz = 0;

void MenuSearch::onVideoReceived(u32 uFreqKhz)
{
   s_uVideoReceivedOnFreqKhz = uFreqKhz;
}

MenuSearch::MenuSearch(void)
:Menu(MENU_ID_SEARCH, L("Search for vehicles"), "")
{
   m_Width = 0.30;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.2;
   m_SpectatorOnlyMode = false;
   search_finished_with_no_results = false;
   m_nSkippedCount = 0;

   addExtraHeightAtEnd(3.0*g_pRenderEngine->textHeight(g_idFontMenu));

   addTopLine(L("Make sure your vehicle is powered on when you start searching."));
   addTopLine(L("Note: Long press on [Cancel] key or choose 'Stop Search' to cancel a search in progress."));
   addTopLine("");

   log_line("MenuSearch: On open, this is the current radio interfaces configuration:");
   hardware_load_radio_info();
   controllerRadioInterfacesLogInfo();

   m_bIsSearchingManual = false;
   m_bIsSearchingAuto = false;
   m_bIsSearchingSiK = false;
   m_bIsSearchPaused = false;
   m_bMustSwitchBack = false;
   m_bDidConnectToAVehicle = false;


   m_pModelOriginal = g_pCurrentModel;

   if ( NULL != g_pCurrentModel )
      log_line("MenuSearch open: has a current model: VID %u, name: [%s]", g_pCurrentModel->uVehicleId, g_pCurrentModel->getLongName());
   else
      log_line("MenuSearch open: does not have a current model.");

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      radio_hw_info_t* pRadioInterface = hardware_get_radio_info(i);
      if ( NULL == pRadioInterface )
         continue;
      m_FrequencyOriginal[i] = pRadioInterface->uCurrentFrequencyKhz;
   }

   m_pPopupSearch = NULL;

   m_SupportedBands = 0;
   m_iCountSupportedBands = 0;
   m_iSearchModelTypes = 0;

   m_bHasSiKRadio = false;
   m_uSiKBands = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( (NULL == pRadioHWInfo) || (!pRadioHWInfo->isSupported) )
      {
         log_line("Not using radio interface %d for search as it's not supported.", i+1 );
         continue;
      }
      if ( ! pRadioHWInfo->isConfigurable )
         continue;
      if ( controllerIsCardDisabled(pRadioHWInfo->szMAC) )
      {
         log_line("Not using radio interface %d for search as it's disabled.", i+1 );
         continue;
      }

      if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
      {
         m_bHasSiKRadio = true;
         m_uSiKBands |= pRadioHWInfo->supportedBands;
      }

      if ( controllerIsCardTXOnly(pRadioHWInfo->szMAC) )
      {
         log_line("Not using radio interface %d for search as it's TX only.", i+1 );
         continue;
      }
      m_SupportedBands = m_SupportedBands | pRadioHWInfo->supportedBands;
   }

   if ( m_bHasSiKRadio )
    m_Width = 0.38;
   
   _add_menu_items();
}

void MenuSearch::valuesToUI()
{
   log_line("MenuSearch: updating UI values...");

   if ( m_bHasSiKRadio )
   {
      ControllerSettings* pCS = get_ControllerSettings();
      if ( -1 != m_IndexSiKAirRate )
      {
          m_pItemsSelect[2]->setSelectedIndex(0);
          for( int i=0; i<getSiKAirDataRatesCount(); i++ )
          {
             if ( pCS->iSearchSiKAirRate != DEFAULT_RADIO_DATARATE_SIK_AIR )
             if ( pCS->iSearchSiKAirRate == getSiKAirDataRates()[i] )
             {
                m_pItemsSelect[2]->setSelectedIndex(i+1);
                break;
             }
         }
      }

      if ( -1 != m_IndexSiKECC )
      {
         m_pItemsSelect[3]->setSelectedIndex(0);
         if ( pCS->iSearchSiKECC )
            m_pItemsSelect[3]->setSelectedIndex(1);
      }
      if ( -1 != m_IndexSiKLBT )
      {
         m_pItemsSelect[4]->setSelectedIndex(0);
         if ( pCS->iSearchSiKLBT )
            m_pItemsSelect[4]->setSelectedIndex(1);
      }
      if ( -1 != m_IndexSiKMCSTR )
      {
         m_pItemsSelect[5]->setSelectedIndex(0);
         if ( pCS->iSearchSiKMCSTR )
            m_pItemsSelect[5]->setSelectedIndex(1);
      }

      if ( 0 != m_iSearchModelTypes )
      {
         enableMenuItem(m_IndexSiKInfo, false);
         enableMenuItem(m_IndexSiKAirRate, false);
         enableMenuItem(m_IndexSiKECC, false);
         enableMenuItem(m_IndexSiKLBT, false);
         enableMenuItem(m_IndexSiKMCSTR, false);
      }
      else
      {
         enableMenuItem(m_IndexSiKInfo, true);
         enableMenuItem(m_IndexSiKAirRate, true);
         enableMenuItem(m_IndexSiKECC, true);
         enableMenuItem(m_IndexSiKLBT, true);
         enableMenuItem(m_IndexSiKMCSTR, true);
      }
   }
   log_line("MenuSearch: updated UI values.");
}

int MenuSearch::_populate_search_frequencies()
{
   int selectedIndex = 0;
   char szBuff[32];

   m_NumChannels = 0;

   m_pItemsSelectFreq = new MenuItemSelect(L("Manual Search"), L("Manualy search on a particular frequency"));
   
   u32 currentFreqKhz = DEFAULT_FREQUENCY;

   if ( NULL == g_pCurrentModel )
   {
       currentFreqKhz = s_LastSearchedFrequency;
       if ( 0 == s_LastSearchedFrequency )
       {
          currentFreqKhz = DEFAULT_FREQUENCY;
          for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
          {
            radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
            if ( pRadioHWInfo->isConfigurable )
            if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_58 )
               currentFreqKhz = DEFAULT_FREQUENCY58;
          }
      }
   }
   if ( NULL != g_pCurrentModel )
   {
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         u32 cardFlags = controllerGetCardFlags(pRadioHWInfo->szMAC);
         if ( pRadioHWInfo->isConfigurable )
         if ( ! controllerIsCardDisabled(pRadioHWInfo->szMAC) )
         if ( ! controllerIsCardTXOnly(pRadioHWInfo->szMAC) )
         if ( cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
            currentFreqKhz = pRadioHWInfo->uCurrentFrequencyKhz;
      }
      if ( 0 == currentFreqKhz )
         currentFreqKhz = DEFAULT_FREQUENCY;
   }

   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_433 )
   {
      for( int i=0; i<getChannels433Count(); i++ )
      {
         m_Channels[m_NumChannels] = getChannels433()[i];
         strcpy(szBuff, str_format_frequency(getChannels433()[i]));
         if ( currentFreqKhz == getChannels433()[i] )
            selectedIndex = m_NumChannels;
         m_pItemsSelectFreq->addSelection(szBuff);
         m_NumChannels++;
      }
      m_pItemsSelectFreq->addSeparator();
   }

   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_868 )
   {
      for( int i=0; i<getChannels868Count(); i++ )
      {
         m_Channels[m_NumChannels] = getChannels868()[i];
         strcpy(szBuff, str_format_frequency(getChannels868()[i]));
         if ( currentFreqKhz == getChannels868()[i] )
            selectedIndex = m_NumChannels;
         m_pItemsSelectFreq->addSelection(szBuff);
         m_NumChannels++;
      }
      m_pItemsSelectFreq->addSeparator();
   }

   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_915 )
   {
      for( int i=0; i<getChannels915Count(); i++ )
      {
      m_Channels[m_NumChannels] = getChannels915()[i];
      strcpy(szBuff, str_format_frequency(getChannels915()[i]));
      if ( currentFreqKhz == getChannels915()[i] )
         selectedIndex = m_NumChannels;
      m_pItemsSelectFreq->addSelection(szBuff);
      m_NumChannels++;
      }
      m_pItemsSelectFreq->addSeparator();
   }

   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_23 )
   {
      for( int i=0; i<getChannels23Count(); i++ )
      {
      m_Channels[m_NumChannels] = getChannels23()[i];
      strcpy(szBuff, str_format_frequency(getChannels23()[i]));
      if ( currentFreqKhz == getChannels23()[i] )
         selectedIndex = m_NumChannels;
      m_pItemsSelectFreq->addSelection(szBuff);
      m_NumChannels++;
      }
      m_pItemsSelectFreq->addSeparator();
   }

   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_24 )
   {
      for( int i=0; i<getChannels24Count(); i++ )
      {
      m_Channels[m_NumChannels] = getChannels24()[i];
      strcpy(szBuff,str_format_frequency(getChannels24()[i]));
      if ( currentFreqKhz == getChannels24()[i] )
         selectedIndex = m_NumChannels;
      m_pItemsSelectFreq->addSelection(szBuff);
      m_NumChannels++;
      }
      m_pItemsSelectFreq->addSeparator();
   }
   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_25 )
   {
      for( int i=0; i<getChannels25Count(); i++ )
      {
         m_Channels[m_NumChannels] = getChannels25()[i];
         strcpy(szBuff, str_format_frequency(getChannels25()[i]));
         if ( currentFreqKhz == getChannels25()[i] )
            selectedIndex = m_NumChannels;
         m_pItemsSelectFreq->addSelection(szBuff);
         m_NumChannels++;
      }
      m_pItemsSelectFreq->addSeparator();
   }
   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_58 )
   {
      Preferences* pP = get_Preferences();
      int iCountCh = getChannels58Count();
   
      if ( pP->iScaleMenus > 0 )
         iCountCh /= 2;

      for( int i=0; i<iCountCh; i++ )
      {
         m_Channels[m_NumChannels] = getChannels58()[i];
         strcpy(szBuff, str_format_frequency(getChannels58()[i]));
         if ( currentFreqKhz == getChannels58()[i] )
            selectedIndex = m_NumChannels;
         m_pItemsSelectFreq->addSelection(szBuff);
         m_NumChannels++;
      }
      m_pItemsSelectFreq->addSeparator();

      if ( pP->iScaleMenus > 0 )
      {
         for( int i=iCountCh; i<getChannels58Count(); i++ )
         {
            m_Channels[m_NumChannels] = getChannels58()[i];
            strcpy(szBuff, str_format_frequency(getChannels58()[i]));
            if ( currentFreqKhz == getChannels58()[i] )
               selectedIndex = m_NumChannels;
            m_pItemsSelectFreq->addSelection(szBuff);
            m_NumChannels++;
         }
         m_pItemsSelectFreq->addSeparator();
      }
   }

   if ( 0 == m_NumChannels )
      m_pItemsSelectFreq->addSelection(L("No supported frequencies"));
   m_pItemsSelectFreq->setSelection(selectedIndex);
   m_pItemsSelectFreq->setIsEditable();

   return addMenuItem(m_pItemsSelectFreq);
}


void MenuSearch::_add_menu_items()
{
   int iTmp = m_SelectedIndex;

   removeAllItems();

   for( int i=0; i<10; i++ )
      m_pItemsSelect[i] = 0;

   m_IndexSiKInfo = -1;
   m_IndexSiKAirRate = -1;
   m_IndexSiKECC = -1;
   m_IndexSiKLBT = -1;
   m_IndexSiKMCSTR = -1;

   log_line("MenuSearch: Has SiK radios? %s", m_bHasSiKRadio?"yes":"no");

   m_iCountSupportedBands = 0;
   m_pItemSelectBand = new MenuItemSelect(L("Band:"), L("Change the frequency bands to search on."));
   
   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_433 )
   {
      m_SupportedBandsList[m_iCountSupportedBands] = RADIO_HW_SUPPORTED_BAND_433;
      m_iCountSupportedBands++;
      m_pItemSelectBand->addSelection("433 Mhz");
   }
   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_868 )
   {
      m_SupportedBandsList[m_iCountSupportedBands] = RADIO_HW_SUPPORTED_BAND_868;
      m_iCountSupportedBands++;
      m_pItemSelectBand->addSelection("868 Mhz");
   }
   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_915 )
   {
      m_SupportedBandsList[m_iCountSupportedBands] = RADIO_HW_SUPPORTED_BAND_915;
      m_iCountSupportedBands++;
      m_pItemSelectBand->addSelection("915 Mhz");
   }
   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_23 )
   {
      m_SupportedBandsList[m_iCountSupportedBands] = RADIO_HW_SUPPORTED_BAND_23;
      m_iCountSupportedBands++;
      m_pItemSelectBand->addSelection("2.3 Ghz");
   }
   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_24 )
   {
      m_SupportedBandsList[m_iCountSupportedBands] = RADIO_HW_SUPPORTED_BAND_24;
      m_iCountSupportedBands++;
      m_pItemSelectBand->addSelection("2.4 Ghz");
   }
   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_25 )
   {
      m_SupportedBandsList[m_iCountSupportedBands] = RADIO_HW_SUPPORTED_BAND_25;
      m_iCountSupportedBands++;
      m_pItemSelectBand->addSelection("2.5 Ghz");
   }
   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_58 )
   {
      m_SupportedBandsList[m_iCountSupportedBands] = RADIO_HW_SUPPORTED_BAND_58;
      m_iCountSupportedBands++;
      m_pItemSelectBand->addSelection("5.8 Ghz");
   }

   for( int i=0; i<m_iCountSupportedBands; i++ )
      log_line("MenuSearch() Supported search bands (%d of %d): %s", i+1, m_iCountSupportedBands, str_getBandName(m_SupportedBandsList[i]));
   
   if ( 0 == m_iCountSupportedBands )
      m_pItemSelectBand->addSelection(L("No bands supported."));
   m_pItemSelectBand->setIsEditable();

   m_IndexBand = addMenuItem(m_pItemSelectBand);

   m_IndexModelTypes = -1;
   
   m_IndexStartSearch = addMenuItem(new MenuItem(L("Start Search"), L("Start/Stop searching for vehicles on current band.")));
   m_IndexManualSearch = _populate_search_frequencies();

   if ( m_bHasSiKRadio )
   {
      float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
  
      char szBuff[256];
      char szBands[128];
      str_get_supported_bands_string(m_uSiKBands, szBands);
      sprintf(szBuff, "SiK parameters (for searching on %s)", szBands);
      
      MenuItemText* pItem = new MenuItemText(szBuff);
      pItem->setMargin(0.0);
      m_IndexSiKInfo = addMenuItem(pItem);

      m_pItemsSelect[2] = new MenuItemSelect(L("SiK Radio Data Rate"), L("Sets the physical radio air data rate to use while searching."));
      
      sprintf(szBuff, "Default (%d kbps)", DEFAULT_RADIO_DATARATE_SIK_AIR/1000);
      m_pItemsSelect[2]->addSelection(szBuff);

      for( int i=0; i<getSiKAirDataRatesCount(); i++ )
      {
         sprintf(szBuff, "%d kbps", (getSiKAirDataRates()[i])/1000);
         m_pItemsSelect[2]->addSelection(szBuff);
      }
      
      m_pItemsSelect[2]->setIsEditable();
      m_pItemsSelect[2]->setMargin(height_text);
      m_IndexSiKAirRate = addMenuItem(m_pItemsSelect[2]);


      m_pItemsSelect[3] = new MenuItemSelect("SiK Error Correction", "Enables hardware error correction flag while searching.");
      m_pItemsSelect[3]->addSelection("No (Default)");
      m_pItemsSelect[3]->addSelection("Yes");
      m_pItemsSelect[3]->setIsEditable();
      m_pItemsSelect[3]->setMargin(height_text);
      m_IndexSiKECC = addMenuItem(m_pItemsSelect[3]);

      m_pItemsSelect[4] = new MenuItemSelect("SiK Listen Before Talk", "Enables listen before talk flag while searching.");
      m_pItemsSelect[4]->addSelection("No (Default)");
      m_pItemsSelect[4]->addSelection("Yes");
      m_pItemsSelect[4]->setIsEditable();
      m_pItemsSelect[4]->setMargin(height_text);
      m_IndexSiKLBT = addMenuItem(m_pItemsSelect[4]);

      m_pItemsSelect[5] = new MenuItemSelect("SiK Enable Manchester Encoding", "Enables Manchester type of encoding flag while searching.");
      m_pItemsSelect[5]->addSelection("No (Default)");
      m_pItemsSelect[5]->addSelection("Yes");
      m_pItemsSelect[5]->setIsEditable();
      m_pItemsSelect[5]->setMargin(height_text);
      m_IndexSiKMCSTR = addMenuItem(m_pItemsSelect[5]);
   }

   m_SelectedIndex = iTmp;
   computeRenderSizes();

   log_line("MenuSearch: Added items.");
}


void MenuSearch::setSpectatorOnly()
{
   m_SpectatorOnlyMode = true;

   removeAllTopLines();
   addTopLine(L("Search for vehicles to connect to as spectator only:"));
   addTopLine(L("Note: Long press on [Cancel] or 'Stop Search' to cancel a search in progress."));
   addTopLine("");
}

void MenuSearch::onShow()
{
   render_search_step = -1;
   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_SEARCH_BAND);
   m_SearchBandIndex = load_simple_config_fileI(szFile, 100);

   if ( (100 == m_SearchBandIndex) || (m_SearchBandIndex<0) || (m_SearchBandIndex >=m_iCountSupportedBands) )
   {
      if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_58 )
      {
         for( int i=0; i<m_iCountSupportedBands; i++ )
         {
            if ( m_SupportedBandsList[i] == RADIO_HW_SUPPORTED_BAND_58 )
            {
               m_SearchBandIndex = i;
               break;
            }
         }
      }
      else if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_24 )
      {
         for( int i=0; i<m_iCountSupportedBands; i++ )
         {
            if ( m_SupportedBandsList[i] == RADIO_HW_SUPPORTED_BAND_24 )
            {
               m_SearchBandIndex = i;
               break;
            }
         }
      }
      else
         m_SearchBandIndex = m_iCountSupportedBands-1;
   }
   if ( m_SearchBandIndex >= m_iCountSupportedBands )
      m_SearchBandIndex = m_iCountSupportedBands-1;

   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_SEARCH_BAND);
   save_simple_config_fileI(szFile, m_SearchBandIndex);
   m_pItemSelectBand->setSelection(m_SearchBandIndex);

   m_bIsSearchingAuto = false;
   m_bIsSearchingManual = false;
   m_bIsSearchingSiK = false;
   m_bIsSearchPaused = false;
   g_bSearchFoundVehicle = false;
   Menu::onShow();
}

void MenuSearch::startSearch()
{
   log_line("MenuSearch::startSearch() on band %s", str_getBandName(m_SupportedBandsList[m_SearchBandIndex]));
   ruby_pause_watchdog("start search");

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      char szTmp[128];
      str_get_supported_bands_string(pRadioHWInfo->supportedBands, szTmp);
      log_line("MenuSearch: Radio interface %d (%s) supported bands: %s", i+1, pRadioHWInfo->szName, szTmp);
   }

   if ( NULL != m_pPopupSearch )
   {
      popups_remove(m_pPopupSearch);
      m_pPopupSearch = NULL;
   }

   bool bFoundSupportedRadioInterface = false;

   g_iCurrentActiveVehicleRuntimeInfoIndex = 0;
   
   m_pSearchChannels = NULL;
   m_SearchChannelsCount = 0;
   m_bIsSearchingSiK = false;
   
   if ( m_SupportedBandsList[m_SearchBandIndex] == RADIO_HW_SUPPORTED_BAND_433 )
   {
     m_pSearchChannels = getChannels433();
     m_SearchChannelsCount = getChannels433Count();
     for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
     {
        radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
        if ( pRadioHWInfo->isConfigurable )
        if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_433 )
        {
           bFoundSupportedRadioInterface = true;
           if ( m_bHasSiKRadio )
              m_bIsSearchingSiK = true;
           break;
        }
     }
   }
   if ( m_SupportedBandsList[m_SearchBandIndex] == RADIO_HW_SUPPORTED_BAND_868 )
   {
     m_pSearchChannels = getChannels868();
     m_SearchChannelsCount = getChannels868Count();
     for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
     {
        radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
        if ( pRadioHWInfo->isConfigurable )
        if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_868 )
        {
           bFoundSupportedRadioInterface = true;
           if ( m_bHasSiKRadio )
              m_bIsSearchingSiK = true;
           break;
        }
     }
   }
   if ( m_SupportedBandsList[m_SearchBandIndex] == RADIO_HW_SUPPORTED_BAND_915 )
   {
     m_pSearchChannels = getChannels915();
     m_SearchChannelsCount = getChannels915Count();
     for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
     {
        radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
        if ( pRadioHWInfo->isConfigurable )
        if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_915 )
        {
           bFoundSupportedRadioInterface = true;
           if ( m_bHasSiKRadio )
              m_bIsSearchingSiK = true;
           break;
        }
     }
   }
   if ( m_SupportedBandsList[m_SearchBandIndex] == RADIO_HW_SUPPORTED_BAND_23 )
   {
     m_pSearchChannels = getChannels23();
     m_SearchChannelsCount = getChannels23Count();
     for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
     {
        radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
        if ( pRadioHWInfo->isConfigurable )
        if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_23 )
        {
           bFoundSupportedRadioInterface = true;
           break;
        }
     }
   }
   if ( m_SupportedBandsList[m_SearchBandIndex] == RADIO_HW_SUPPORTED_BAND_24 )
   {
     m_pSearchChannels = getChannels24();
     m_SearchChannelsCount = getChannels24Count();
     for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
     {
        radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
        if ( pRadioHWInfo->isConfigurable )
        if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_24 )
        {
           bFoundSupportedRadioInterface = true;
           break;
        }
     }
   }
   if ( m_SupportedBandsList[m_SearchBandIndex] == RADIO_HW_SUPPORTED_BAND_25 )
   {
     m_pSearchChannels = getChannels25();
     m_SearchChannelsCount = getChannels25Count();
     for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
     {
        radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
        if ( pRadioHWInfo->isConfigurable )
        if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_25 )
        {
           bFoundSupportedRadioInterface = true;
           break;
        }
     }
   }
   if ( m_SupportedBandsList[m_SearchBandIndex] == RADIO_HW_SUPPORTED_BAND_58 )
   {
     m_pSearchChannels = getChannels58();
     m_SearchChannelsCount = getChannels58Count();
     for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
     {
        radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
        if ( pRadioHWInfo->isConfigurable )
        if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_58 )
        {
           bFoundSupportedRadioInterface = true;
           break;
        }
     }
   }

   if ( ! bFoundSupportedRadioInterface )
   {
      Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE, L("Info"), NULL);
      pm->m_xPos = 0.4; pm->m_yPos = 0.4;
      pm->m_Width = 0.36;
      pm->addTopLine(L("None of your radio cards support this frequency band. Please choose another frequency band to search on."));
      add_menu_to_stack(pm);
      return;
   }
   
   g_bSearching = true;
   g_iSearchFrequency = 0;
   g_bSearchFoundVehicle = false;
   g_RouterIsReadyTimestamp = 0;
   g_pCurrentModel = NULL;

   s_uVideoReceivedOnFreqKhz = 0;

   link_watch_remove_popups();
   render_all(get_current_timestamp_ms(), true);

   createSearchPopup();

   render_all(get_current_timestamp_ms(), true);
   pairing_stop();

   if ( ! m_bDidConnectToAVehicle )
      m_bMustSwitchBack = true;

   if ( m_bHasSiKRadio )
   {
      enableMenuItem(m_IndexSiKInfo, false);
      enableMenuItem(m_IndexSiKAirRate, false);
      enableMenuItem(m_IndexSiKECC, false);
      enableMenuItem(m_IndexSiKLBT, false);
      enableMenuItem(m_IndexSiKMCSTR, false);
   }
   enableMenuItem(m_IndexBand, false);
   m_pItemsSelectFreq->setEnabled(false);
   m_pMenuItems[m_IndexStartSearch]->setTitle(L("Stop Search"));

   handle_commands_abandon_command();
   reset_vehicle_runtime_info(&g_SearchVehicleRuntimeInfo);
   
   log_line("MenuSearch: Finished start search function");
   render_all(get_current_timestamp_ms(), true);
   log_line("MenuSearch: Exit start search function");

   m_CurrentSearchFrequencyKhz = 0;
   m_bIsSearchingManual = false;
   m_bIsSearchingAuto = true;
   m_bIsSearchPaused = false;
   m_nSkippedCount = 0;

   
   search_finished_with_no_results = false;
   render_search_step = -1;
}

void MenuSearch::stopSearch()
{
   log_line("MenuSearch: Stopping search...");

   if ( m_pPopupSearch )
   {
      popups_remove( m_pPopupSearch );
      m_pPopupSearch = NULL;
   }

   ruby_pause_watchdog("search stop");

   pairing_stop();

   log_line("MenuSearch: Controller radio interfaces configuration after stopping search:");
   hardware_load_radio_info();

   g_bSearching = false;
   g_bSearchFoundVehicle = false;
   g_iSearchFrequency = 0;
   s_uVideoReceivedOnFreqKhz = 0;
   m_CurrentSearchFrequencyKhz = 0;
   m_bIsSearchingAuto = false;
   m_bIsSearchingManual = false;
   m_bIsSearchingSiK = false;
   m_bIsSearchPaused = false;
   render_search_step = -1;

   reset_vehicle_runtime_info(&g_SearchVehicleRuntimeInfo);

   if ( m_bHasSiKRadio )
   {
      enableMenuItem(m_IndexSiKInfo, true);
      enableMenuItem(m_IndexSiKAirRate, true);
      enableMenuItem(m_IndexSiKECC, true);
      enableMenuItem(m_IndexSiKLBT, true);
      enableMenuItem(m_IndexSiKMCSTR, true);
   }

   enableMenuItem(m_IndexBand, true);
   m_pItemsSelectFreq->setEnabled(true);
   m_pMenuItems[m_IndexStartSearch]->setTitle(L("Start Search"));
   invalidate();
   log_line("MenuSearch: Stopped search. Complete.");
}

void MenuSearch::Render()
{
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   
   RenderPrepare();
   float yTop = Menu::RenderFrameAndTitle();
   float y0 = yTop;
   float y = y0;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   y += height_text*0.4;

   char szBuff[200];
   
   if ( search_finished_with_no_results && (! g_bSearchFoundVehicle) )
   {
       g_pRenderEngine->setColors(get_Color_MenuText());
       strcpy(szBuff, L("No vehicles found!"));
       if ( m_nSkippedCount > 0 )
          strcpy(szBuff, L("Vehicle found and skipped."));
       g_pRenderEngine->drawMessageLines(m_xPos+m_sfMenuPaddingX, y, szBuff, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
   
       y += height_text *(1.0+MENU_ITEM_SPACING);
       
       strcpy(szBuff, L("You could search on other bands and channels."));
       if ( m_nSkippedCount > 0 )
          strcpy(szBuff, " ");
       g_pRenderEngine->drawMessageLines(m_xPos+m_sfMenuPaddingX, y, szBuff, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
       RenderEnd(yTop);
       return;
   }

   if ( (! m_bIsSearchingManual) && (! m_bIsSearchingAuto) && (!g_bSearchFoundVehicle) )
   {
      RenderEnd(yTop);
      return;
   }

   if ( m_bIsSearchingManual || m_bIsSearchingAuto )
      onSearchStep();
       
   g_pRenderEngine->setColors(get_Color_MenuText());

   sprintf(szBuff, L("Searching ..."));
   if ( render_search_step >= 0 )
      for(int k=0; k<(render_search_step%6); k++ )
         strcat(szBuff,".");

   if ( NULL != m_pPopupSearch )
   {
      char szTitle[128];
      strcpy(szTitle, m_pPopupSearch->getTitle());
      int iLen = strlen(szTitle)-1;
      while ( (iLen > 0) && ((szTitle[iLen] == '.') || (szTitle[iLen] == '|')) )
      {
         szTitle[iLen] = 0;
         iLen--;
      }
      int k=0;
      int pos = (render_search_step%8);
      while ( k < pos-1 )
      {
         strcat(szTitle,".");
         k++;
      }
      strcat(szTitle, "|");
      k++;
      while ( k < 8 )
      {
         strcat(szTitle, ".");
         k++;
      }
      m_pPopupSearch->setTitle(szTitle);
   }

   g_pRenderEngine->drawMessageLines(m_xPos+m_sfMenuPaddingX, y, szBuff, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
   y += height_text *(1.0+MENU_ITEM_SPACING);

   sprintf(szBuff, L("Searching on %s"), str_format_frequency(m_CurrentSearchFrequencyKhz));
   g_pRenderEngine->drawMessageLines(m_xPos+m_sfMenuPaddingX, y, szBuff, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
   y += height_text *(1.0+MENU_ITEM_SPACING);

   if ( g_bSearchFoundVehicle )
   {
   }
   RenderEnd(yTop);
}


void MenuSearch::onSearchStep()
{
   if ( (! m_bIsSearchingManual) && (! m_bIsSearchingAuto) && (!g_bSearchFoundVehicle) )
      return;

   if ( m_bIsSearchPaused)
      return;

   if ( -1 == render_search_step )
   {
      reset_vehicle_runtime_info(&g_SearchVehicleRuntimeInfo);
      log_line("MenuSearch::onSearchStep() initialized first search step.");
      m_CurrentSearchFrequencyKhz = m_pSearchChannels[0];
      g_iSearchFrequency = m_pSearchChannels[0];
      s_uVideoReceivedOnFreqKhz = 0;
      render_search_step = 0;
      return;
   }

   //u32 delayMs = DEFAULT_DELAY_WIFI_CHANGE;
   //Preferences* pP = get_Preferences();
   //if ( NULL != pP )
   //   delayMs = (u32) pP->iDebugWiFiChangeDelay;
   //if ( delayMs<1 || delayMs > 200 )
   //   delayMs = DEFAULT_DELAY_WIFI_CHANGE;
   u32 delayMs = 30;
   
   int step = (render_search_step/3);
   int substep = (render_search_step%3);
   log_line("MenuSearch::onSearchStep(), step: %d, substep: %d", step, substep);

   // substep 0: update UI and the search frequency value for the next search frequency
   // substep 1: switch to the new frequency
   // substep 2: search on new frequency

   if ( step >= m_SearchChannelsCount )
   {
      log_line("MenuSearch::onSearchStep() reached end step.");
      search_finished_with_no_results = true;
      //printf("\n set sf: %d\n", search_finished_with_no_results);
      stopSearch();
      reset_vehicle_runtime_info(&g_SearchVehicleRuntimeInfo);
      return;
   }

   // substep 0: update UI and the search frequency value for the next search frequency
   if ( substep == 0 )
   {
      reset_vehicle_runtime_info(&g_SearchVehicleRuntimeInfo);
      m_CurrentSearchFrequencyKhz = m_pSearchChannels[step];
      g_iSearchFrequency = m_pSearchChannels[step];
      s_uVideoReceivedOnFreqKhz = 0;
      log_line("Searching - switch UI to frequency for step %d: %s", step, str_format_frequency(m_CurrentSearchFrequencyKhz));
      if ( NULL != m_pPopupSearch )
      {
         char szTitle[128];
         sprintf(szTitle, L("Searching on %s"), str_format_frequency(m_CurrentSearchFrequencyKhz));     
         strcat(szTitle, " ...");
         m_pPopupSearch->setTitle(szTitle);
      }
      render_search_step++;
      return;
   }

   // substep 1: switch to the new frequency
   if ( substep == 1 )
   {
      g_RouterIsReadyTimestamp = 0;
      s_LastSearchedFrequency = m_CurrentSearchFrequencyKhz;
      reset_vehicle_runtime_info(&g_SearchVehicleRuntimeInfo);
      s_uVideoReceivedOnFreqKhz = 0;
      if ( step == 0 )
      {
         int iBand = getBand(m_CurrentSearchFrequencyKhz);
         log_line("MenuSearch::onSearchStep() start searching processes (for freq %s and band %s)...",
            str_format_frequency(m_CurrentSearchFrequencyKhz), str_getBandName(iBand));
         if ( m_bHasSiKRadio && m_bIsSearchingSiK &&
             ( iBand == RADIO_HW_SUPPORTED_BAND_433 || iBand == RADIO_HW_SUPPORTED_BAND_868 || iBand == RADIO_HW_SUPPORTED_BAND_915 ) )
         {
             int iAirDataRate = DEFAULT_RADIO_DATARATE_SIK_AIR;
             if ( m_pItemsSelect[2]->getSelectedIndex() > 0 )
                iAirDataRate = getSiKAirDataRates()[m_pItemsSelect[2]->getSelectedIndex()-1];
             int iECC = m_pItemsSelect[3]->getSelectedIndex();
             int iLBT = m_pItemsSelect[4]->getSelectedIndex();
             int iMCSTR = m_pItemsSelect[5]->getSelectedIndex();
             pairing_start_search_sik_mode(m_CurrentSearchFrequencyKhz, iAirDataRate, iECC, iLBT, iMCSTR);
         }  
         else
            pairing_start_search_mode(m_CurrentSearchFrequencyKhz, m_iSearchModelTypes);

         hardware_sleep_ms(delayMs);
      }
      else
      {
         log_line("MenuSearch::onSearchStep() send command to router to change frequency to %s", str_format_frequency(m_CurrentSearchFrequencyKhz) );
         send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROLLER_SEARCH_FREQ_CHANGED, m_CurrentSearchFrequencyKhz );
      }
      render_search_step++;
      return;
   }

   // substep 2: search on new frequency
   if ( substep == 2 )
   {
      if ( 0 == g_RouterIsReadyTimestamp )
      {
         log_line("MenuSearch::onSearchStep(): waiting for router to be ready...");
         reset_vehicle_runtime_info(&g_SearchVehicleRuntimeInfo);
         return;
      }
      char szBands[128];
      str_get_supported_bands_string(getBand(m_CurrentSearchFrequencyKhz), szBands);
      log_line("MenuSearch::onSearchStep() Router ready %u ms ago, current search frequency: %s, band: %s, have SiK radios: %d",
        g_TimeNow - g_RouterIsReadyTimestamp, str_format_frequency(m_CurrentSearchFrequencyKhz), szBands, hardware_radio_has_sik_radios());
      

      u32 uWaitTimeMs = 3*1100/DEFAULT_TELEMETRY_SEND_RATE;
      if ( hardware_radio_has_sik_radios() )
      if ( (getBand(m_CurrentSearchFrequencyKhz) == RADIO_HW_SUPPORTED_BAND_433) ||
           (getBand(m_CurrentSearchFrequencyKhz) == RADIO_HW_SUPPORTED_BAND_868) ||
           (getBand(m_CurrentSearchFrequencyKhz) == RADIO_HW_SUPPORTED_BAND_915) )
      {
         uWaitTimeMs = 1500;
         log_line("MenuSearch::onSearchStep() Searching on 433/868/915 Mhz band and we have Sik radios. Increase search time to %u ms", uWaitTimeMs);
      }
      /*
      u32 uWaitTimeMs = 250;
      if ( 0 != s_uVideoReceivedOnFreqKhz )
      if ( s_uVideoReceivedOnFreqKhz == m_CurrentSearchFrequencyKhz )
      {
         uWaitTimeMs = 4*1100/DEFAULT_TELEMETRY_SEND_RATE;
         if ( hardware_radio_has_sik_radios() )
         if ( (getBand(m_CurrentSearchFrequencyKhz) == RADIO_HW_SUPPORTED_BAND_433) ||
              (getBand(m_CurrentSearchFrequencyKhz) == RADIO_HW_SUPPORTED_BAND_868) ||
              (getBand(m_CurrentSearchFrequencyKhz) == RADIO_HW_SUPPORTED_BAND_915) )
         {
            uWaitTimeMs = 1500;
            log_line("MenuSearch::onSearchStep() Searching on 433/868/915 Mhz band and we have Sik radios. Increase search time to %u ms", uWaitTimeMs);
         }
      }
      */
      if ( g_TimeNow > g_RouterIsReadyTimestamp + uWaitTimeMs )
      {
         log_line("MenuSearch::onSearchStep(): Nothing found on %s after %d ms of waiting.", str_format_frequency(m_CurrentSearchFrequencyKhz), 3*1100/DEFAULT_TELEMETRY_SEND_RATE);
         render_search_step++;
         return;
      }

      bool bHasVideoData = false;
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         for( int k=STREAM_ID_VIDEO_1; k<MAX_RADIO_STREAMS; k++ )
         {
            if ( g_SM_RadioStats.radio_streams[i][k].totalRxPackets > 20 )
            {
               bHasVideoData = true;
               break;
            }
         }
      }
      log_line("MenuSearch::onSearchStep() Has received video data? %s", bHasVideoData?"Yes":"No");
      if ( bHasVideoData )
         uWaitTimeMs = 2500;

      bool bVehicleIsOnCurrentFreq = false;
      if ( (g_SearchVehicleRuntimeInfo.bGotRubyTelemetryInfo) && (g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uVehicleId != 0) )
      {
         char szTmpBuff[256];
         szTmpBuff[0] = 0;
         for( int i=0; i<g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.radio_links_count; i++ )
         {
            char szTmp2[64];
            sprintf(szTmp2, "%s ", str_format_frequency(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRadioFrequenciesKhz[i]) );
            strcat(szTmpBuff, szTmp2);
         }
         log_line("MenuSearch::onSearchStep() There is a vehicle found on current frequency. Vehicle id: %u, has %d radio links: %s.", g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uVehicleId, g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.radio_links_count, szTmpBuff);
         for( int i=0; i<g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.radio_links_count; i++ )
         {
            if ( g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRadioFrequenciesKhz[i] == m_CurrentSearchFrequencyKhz )
               bVehicleIsOnCurrentFreq = true;
            if ( g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRadioFrequenciesKhz[i] < 10000 )
            if ( g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRadioFrequenciesKhz[i]*1000 == m_CurrentSearchFrequencyKhz )
               bVehicleIsOnCurrentFreq = true;
         }
      }
      if ( bVehicleIsOnCurrentFreq )
      {
         u8 vMaj = g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.rubyVersion;
         u8 vMin = g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.rubyVersion;
         vMaj = vMaj >> 4;
         vMin = vMin & 0x0F;
   
         char szFreq1[64];
         char szFreq2[64];
         char szFreq3[64];

         strcpy(szFreq1, str_format_frequency(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRadioFrequenciesKhz[0]) );
         strcpy(szFreq2, str_format_frequency(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRadioFrequenciesKhz[1]) );
         strcpy(szFreq3, str_format_frequency(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRadioFrequenciesKhz[2]) );

         log_line("MenuSearch::onSearchStep() Found a vehicle while searching on %s: vehicle ID: %u, version: %d.%d, radio links (%d): %s, %s, %s",
             str_format_frequency(m_CurrentSearchFrequencyKhz), g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uVehicleId, vMaj, vMin,
             g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.radio_links_count,
             szFreq1, szFreq2, szFreq3 );
         m_bIsSearchPaused = true;
         g_bSearchFoundVehicle = true;
         invalidate();
         setTooltip("");
         if ( NULL != m_pPopupSearch )
         {
            popups_remove(m_pPopupSearch);
            m_pPopupSearch = NULL;
         }
         MenuSearchConnect* pMenu = new MenuSearchConnect();
         pMenu->m_iSearchModelTypes = m_iSearchModelTypes;
         pMenu->setCurrentFrequency(m_CurrentSearchFrequencyKhz);
         if ( m_SpectatorOnlyMode )
            pMenu->setSpectatorOnly();
         add_menu_to_stack(pMenu);
         log_line("MenuSearch::onSearchStep() Added connect menu to stack");
      }
      else if ( g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uVehicleId != 0 )
      {
         char szFreq1[64];
         char szFreq2[64];
         char szFreq3[64];
         strcpy(szFreq1, str_format_frequency(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRadioFrequenciesKhz[0]) );
         strcpy(szFreq2, str_format_frequency(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRadioFrequenciesKhz[1]) );
         strcpy(szFreq3, str_format_frequency(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRadioFrequenciesKhz[2]) );
         log_softerror_and_alarm("Found a vehicle that emits on these frequencies %s, %s, %s, but is not on current search frequencty %s!",
            szFreq1, szFreq2, szFreq3, str_format_frequency(m_CurrentSearchFrequencyKhz) );
      }
      else
         log_line("MenuSearch::onSearchStep() No vehicle on current frequency (%s): got telem search info? %s, search telemetry VID: %u",
           str_format_frequency(m_CurrentSearchFrequencyKhz), g_SearchVehicleRuntimeInfo.bGotRubyTelemetryInfo?"yes":"no", g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uVehicleId);
   }
}

void MenuSearch::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   if ( NULL != m_pPopupSearch )
   {
      popups_remove(m_pPopupSearch);
      m_pPopupSearch = NULL;
   }

   if ( 1 != iChildMenuId/1000 )
      return;

   if ( NULL != m_pModelOriginal )
      log_line("MenuSearch: had an initial model when opened search menu: VID %u, name: [%s]", m_pModelOriginal->uVehicleId, m_pModelOriginal->getLongName());
   else
      log_line("MenuSearch: did not had an initial model when opened search menu.");

   if ( NULL != g_pCurrentModel )
      log_line("MenuSearch: has a current model: VID %u, name: [%s]", g_pCurrentModel->uVehicleId, g_pCurrentModel->getLongName());
   else
      log_line("MenuSearch: does not have a current model.");

   log_line("Search: First pairing done? %s", g_bFirstModelPairingDone?"Yes":"No");
   // Skip
   if ( 0 == returnValue )
   {
      m_nSkippedCount++;
      log_line("Pressed Skip search");
      m_bIsSearchPaused = false;
      g_bSearchFoundVehicle = false;
      reset_vehicle_runtime_info(&g_SearchVehicleRuntimeInfo);
      invalidate();
      createSearchPopup();
      return;
   }

   if ( NULL != m_pModelOriginal )
   {
      log_line("MenuSearch: Save current original model, vehicle id: %u, it's controller id: %u. Received vehicle id while searching: %u",
         m_pModelOriginal->uVehicleId, m_pModelOriginal->uControllerId,
         g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uVehicleId );

      saveControllerModel(m_pModelOriginal);
   }
   else
      log_line("MenuSearch: No original model, do not save it.");

   bool bIsNew = false;
   if( ! ruby_is_first_pairing_done() )
   {
      log_line("MenuSearch: First pairing was not completed before. Deleting all models.");
      deleteAllModels();
      bIsNew = true;
   }
   else
      log_line("MenuSearch: First pairing was already completed.");

   if ( ! controllerHasModelWithId(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uVehicleId) )
      bIsNew = true;

   Model* pExistingModel = NULL;
   if ( ! bIsNew )
   {
      pExistingModel = findModelWithId(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uVehicleId, 14);
      if ( NULL == pExistingModel )
         bIsNew = true;
   } 
   if ( bIsNew )
      log_line("MenuSearch: The found vehicle VID %u is new (not on controller).", g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uVehicleId);
   else
      log_line("MenuSearch: The found vehicle VID %u (is spectator: %s) already exists on controller.", g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uVehicleId, pExistingModel->is_spectator?"yes":"no");
   
   // Connect as Spectator

   if ( 1 == returnValue )
   {
      log_line("MenuSearch: Pressed add as spectator. Current total spectator vehicles: %d.", getControllerModelsSpectatorCount());
      
      if ( pExistingModel != NULL)
      {
         if ( (pExistingModel->is_spectator == false) || modelIsInControllerList(pExistingModel->uVehicleId) )
            deleteModel(pExistingModel);
      }
      Model* pModel = addSpectatorModel(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uVehicleId);
      pModel->populateFromVehicleTelemetryData_v6(&(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended));
      pModel->is_spectator = true;

      set_model_main_connect_frequency(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uVehicleId, m_CurrentSearchFrequencyKhz);

      stopSearch();
      ruby_set_is_first_pairing_done();
      g_bFirstModelPairingDone = true;
      setCurrentModel(pModel->uVehicleId);
      g_pCurrentModel = getCurrentModel();
      setControllerCurrentModel(g_pCurrentModel->uVehicleId);
      saveControllerModel(g_pCurrentModel);

      ruby_set_active_model_id(g_pCurrentModel->uVehicleId);

      if ( bIsNew )
         onModelAdded(pModel->uVehicleId);
      onMainVehicleChanged(true);
      log_line("MenuSearch: Added vehicle id: %u as spectator.", pModel->uVehicleId);
      pairing_start_normal();
      m_bDidConnectToAVehicle = true;
      m_bMustSwitchBack = false;
      log_line("MenuSearch: Close all");
      menu_discard_all();
      return;
   }

   // Connect for Control

   if ( 2 == returnValue )
   {
      log_line("MenuSearch: Pressed add as controller. Current total controller vehicles: %d.", getControllerModelsCount());
     
      if ( pExistingModel != NULL)
      {
         log_line("MenuSearch: There is already an existing model for VID %u, model's software ver: %d.%d b-%d", pExistingModel->uVehicleId, get_sw_version_major(pExistingModel), get_sw_version_minor(pExistingModel), get_sw_version_build(pExistingModel));
         if ( pExistingModel->is_spectator || modelIsInSpectatorList(pExistingModel->uVehicleId) )
         {
            log_line("MenuSearch: Delete exiting spectator model for this VID %u", pExistingModel->uVehicleId);
            deleteModel(pExistingModel);
            bIsNew = true;
         }
      }

      Model* pModel = NULL;
      if ( bIsNew )
      {
         log_line("MenuSearch: Adding a new vehicle model as controller");
         log_line("MenuSearch: Creating a vehicle model for VID: %u ...", g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uVehicleId);
         pModel = addNewModel(g_SearchVehicleRuntimeInfo.uVehicleId, (g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.rubyVersion>>4) & 0x0F, (g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.rubyVersion & 0x0F));
      }
      else
      {
         pModel = findModelWithId(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uVehicleId, 15);
         if ( NULL == pModel )
         {
            bIsNew = true;
            log_line("MenuSearch: Creating a vehicle model for VID: %u ...", g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uVehicleId);
            pModel = addNewModel(g_SearchVehicleRuntimeInfo.uVehicleId, (g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.rubyVersion>>4) &0x0F, (g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.rubyVersion & 0x0F));
         }
         else
            log_line("MenuSearch: Found existing controller vehicle model for VID %u.", pModel->uVehicleId);
      }
      pModel->populateFromVehicleTelemetryData_v6(&(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended));
      pModel->is_spectator = false;
      set_model_main_connect_frequency(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uVehicleId, m_CurrentSearchFrequencyKhz);

      stopSearch();
      log_line("MenuSearch: Save model VID %u, mode: %s", pModel->uVehicleId, (pModel->is_spectator)?"spectator":"control");
      ruby_set_is_first_pairing_done();
      g_bFirstModelPairingDone = true;
      g_bDidAnUpdate = false;
      setCurrentModel(pModel->uVehicleId);
      g_pCurrentModel = getCurrentModel();

      setControllerCurrentModel(g_pCurrentModel->uVehicleId);
      saveControllerModel(g_pCurrentModel);

      ruby_set_active_model_id(g_pCurrentModel->uVehicleId);
     
      if ( g_pCurrentModel->getVehicleFirmwareType() == MODEL_FIRMWARE_TYPE_RUBY )
         g_pCurrentModel->b_mustSyncFromVehicle = true;
      log_line("MenuSearch: Changed current main vehicle to vehicle id %u", g_pCurrentModel->uVehicleId);
      if ( bIsNew )
         onModelAdded(pModel->uVehicleId);
      onMainVehicleChanged(true);
      log_line("Added VID %u as a controller model", pModel->uVehicleId);
      pairing_start_normal();
      m_bDidConnectToAVehicle = true;
      m_bMustSwitchBack = false;
      log_line("MenuSearch: Close all");
      menu_discard_all();
      return;
   }
}

int MenuSearch::onBack()
{
   log_line("MenuSearch::onBack()");

   if ( m_pPopupSearch )
   {
      popups_remove( m_pPopupSearch );
      m_pPopupSearch = NULL;
   }

   if ( m_pItemSelectBand->isEditing() )
   {
      log_line("MenuSearch::onBack() end edit");
      m_pItemSelectBand->endEdit(true);
      return 1;
   }

   if ( m_pItemsSelectFreq->isEditing() )
   {
      log_line("MenuSearch::onBack() end edit2");
      search_finished_with_no_results = false;
      m_pItemsSelectFreq->endEdit(true);
      return 1;
   }

   search_finished_with_no_results = false;

   if ( m_bIsSearchingManual || m_bIsSearchingAuto )
   {
      log_line("MenuSearch::onBack() stop search");
      hardware_blockCurrentPressedKeys();
      stopSearch();
      return 1;
   }

   // Switch back to the original vehicle when we opened the search menu, if any

   if ( m_bMustSwitchBack )
   {
      log_line("MenuSearch::onBack() switch back");
      pairing_stop();
      log_line("MenuSearch: Controller radio interfaces configuration on closing the menu and switching back to original model:");
      hardware_load_radio_info();
      g_pCurrentModel = m_pModelOriginal;
      m_bMustSwitchBack = false;

      if ( ! g_bFirstModelPairingDone )
      {
         log_line("MenuSearch:onBack() Switched back to previous active current model with no first pairing done, vehicle id: %u", g_pCurrentModel->uVehicleId);
         pairing_start_normal();
      }
      else if ( (NULL != g_pCurrentModel) && ((0 != getControllerModelsCount()) || (0 != getControllerModelsSpectatorCount()) ) )
      {
         log_line("MenuSearch:onBack() Switched back to previous active current model, vehicle id: %u", g_pCurrentModel->uVehicleId);
         pairing_start_normal();
      }
      else
         log_line("MenuSearch:onBack() no previous current model to switch back to.");
   }

   ruby_resume_watchdog_force("search close menu");
   return Menu::onBack();
}

void MenuSearch::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   int iCountValidRadioInterfaces = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;

      if ( controllerIsCardDisabled(pRadioHWInfo->szMAC) )
         continue;
      iCountValidRadioInterfaces++;
   }
   log_line("Valid interfaces for search: %d", iCountValidRadioInterfaces);
   
   if ( m_IndexBand == m_SelectedIndex )
   {
      m_SearchBandIndex = m_pItemSelectBand->getSelectedIndex();
      char szFile[128];
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_CURRENT_SEARCH_BAND);
      save_simple_config_fileI(szFile, m_SearchBandIndex);
      return;
   }

   // Start/Stop Search
   if ( m_IndexStartSearch == m_SelectedIndex )
   {
      log_line("Pressed Start/Stop search button");
      if ( 0 == iCountValidRadioInterfaces )
      {
         addMessage2(0, L("No active radio interfaces."), L("All your controller's radio interfaces are disabled. Enable at least a radio interface (from the Radio menu) to be able to search for vehicles."));
         return;
      }
      if ( ! m_bIsSearchingAuto )
      {
         ruby_pause_watchdog("search manual freq");
         startSearch();
      }
      else
      {
         hardware_blockCurrentPressedKeys();
         stopSearch();
         ruby_resume_watchdog("stop search manual freq");
      }
      return;
   }

   // Manual search on a frequency
   if ( (m_IndexManualSearch == m_SelectedIndex) && (! m_pItemsSelectFreq->isEditing()) )
   {
      log_line("Pressed manual search button");

      if ( (0 == iCountValidRadioInterfaces) || (0 == m_NumChannels) )
      {
         addMessage2(0, L("No active radio interfaces."), L("All your controller's radio interfaces are disabled. Enable at least a radio interface (from the Radio menu) to be able to search for vehicles."));
         return;
      }

      ruby_pause_watchdog("search on a freq");
      createSearchPopup();
      g_bSearching = true;
      render_all(get_current_timestamp_ms(), true);
      pairing_stop();
      render_all(get_current_timestamp_ms(), true);

      if ( ! m_bDidConnectToAVehicle )
         m_bMustSwitchBack = true;
      g_pCurrentModel = NULL;
      m_CurrentSearchFrequencyKhz = 0;
      g_iSearchFrequency = 0;
      m_pSearchChannels = &(m_Channels[m_pItemsSelectFreq->getSelectedIndex()]);
      m_SearchChannelsCount = 1;

      log_line("Searching only on %s", str_format_frequency(m_pSearchChannels[0]));
      
      if ( m_bHasSiKRadio )
      {
         enableMenuItem(m_IndexSiKInfo, false);
         enableMenuItem(m_IndexSiKAirRate, false);
         enableMenuItem(m_IndexSiKECC, false);
         enableMenuItem(m_IndexSiKLBT, false);
         enableMenuItem(m_IndexSiKMCSTR, false);
      }
      enableMenuItem(m_IndexBand, false);
      m_pItemsSelectFreq->setEnabled(false);
      m_pMenuItems[m_IndexStartSearch]->setTitle(L("Stop Search"));

      handle_commands_abandon_command();
      reset_vehicle_runtime_info(&g_SearchVehicleRuntimeInfo);

      m_bIsSearchingManual = true;
      m_bIsSearchingAuto = false;
      m_bIsSearchPaused = false;
      m_bIsSearchingSiK = false;
      int iBand = getBand(m_pSearchChannels[0]);
      if ( m_bHasSiKRadio )
      if ( iBand == RADIO_HW_SUPPORTED_BAND_433 || iBand == RADIO_HW_SUPPORTED_BAND_868 || iBand == RADIO_HW_SUPPORTED_BAND_915 )
         m_bIsSearchingSiK = true;


      if ( NULL != m_pPopupSearch )
      {
         popups_remove(m_pPopupSearch);
         m_pPopupSearch = NULL;
      }
      createSearchPopup();

      render_all(get_current_timestamp_ms(), true);

      g_bSearching = true;
      g_bSearchFoundVehicle = false;
      g_RouterIsReadyTimestamp = 0;
      render_search_step = -1;
      search_finished_with_no_results = false;
      return;
   }

   if ( m_bHasSiKRadio && (m_SelectedIndex > m_IndexManualSearch) )
   {
      ControllerSettings* pCS = get_ControllerSettings();
   
      if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
         return;

      if ( m_IndexSiKAirRate == m_SelectedIndex )
      {
         if ( 0 == m_pItemsSelect[2]->getSelectedIndex() )
            pCS->iSearchSiKAirRate = DEFAULT_RADIO_DATARATE_SIK_AIR;
         else
            pCS->iSearchSiKAirRate = getSiKAirDataRates()[m_pItemsSelect[2]->getSelectedIndex()-1];
         save_ControllerSettings();
      }

      if ( m_IndexSiKECC == m_SelectedIndex )
      {
         pCS->iSearchSiKECC = m_pItemsSelect[3]->getSelectedIndex();
         save_ControllerSettings();
      }
      if ( m_IndexSiKLBT == m_SelectedIndex )
      {
         pCS->iSearchSiKLBT = m_pItemsSelect[4]->getSelectedIndex();
         save_ControllerSettings();
      }
      if ( m_IndexSiKMCSTR == m_SelectedIndex )
      {
         pCS->iSearchSiKMCSTR = m_pItemsSelect[5]->getSelectedIndex();
         save_ControllerSettings();
      }
      return;
   }
}

void MenuSearch::createSearchPopup()
{
   link_watch_remove_popups();
   if ( NULL != m_pPopupSearch )
   {
      popups_remove(m_pPopupSearch);
      m_pPopupSearch = NULL;
   }

   m_pPopupSearch = new Popup(true, L("Searching ..."), 500);
   m_pPopupSearch->setFixedWidth(0.28);
   m_pPopupSearch->setFont(g_idFontMenuLarge);
   m_pPopupSearch->useSmallLines(true);
   m_pPopupSearch->setCenteredTexts();
   m_pPopupSearch->disableAutoRemove();

   if ( m_bHasSiKRadio && m_bIsSearchingSiK )
   {
      int iAirDataRate = DEFAULT_RADIO_DATARATE_SIK_AIR;
      if ( m_pItemsSelect[2]->getSelectedIndex() > 0 )
         iAirDataRate = getSiKAirDataRates()[m_pItemsSelect[2]->getSelectedIndex()-1];
      int iECC = m_pItemsSelect[3]->getSelectedIndex();
      int iLBT = m_pItemsSelect[4]->getSelectedIndex();
      int iMCSTR = m_pItemsSelect[5]->getSelectedIndex();
      char szBuff[256];
      sprintf(szBuff, "Searching using SiK radios (%d kbps air rate, %s, %s, %s)...",
          iAirDataRate/1000,
          iECC?"ECC":"No ECC",
          iLBT?"LBT":"No LBT",
          iMCSTR?"MCSTR":"No MCSTR");
      m_pPopupSearch->addLine(szBuff);
   }     

   m_pPopupSearch->addLine(L("Long press on [Back] to stop the search."));

   popups_add_topmost(m_pPopupSearch);
}
