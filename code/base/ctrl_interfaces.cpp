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

#include "base.h"
#include "config.h"
#include "hardware.h"
#include "hardware_procs.h"
#include "hardware_radio.h"
#include "ctrl_interfaces.h"
#include "ctrl_settings.h"
#include "tx_powers.h"
#include "../common/string_utils.h"

#if defined(HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA)

ControllerInterfacesSettings s_CIS;
bool s_bAddedNewRadioInterfaces = false;
int  s_iNewCardRadioInterfaceIndex = -1;

// returns true if the power levels where updated
bool controllerInterfacesCheckPowerLevels()
{
   return false;
}

// Returns the index of the card
int controllerAddedNewRadioInterfaces()
{
   if ( s_bAddedNewRadioInterfaces )
   {
      if ( s_iNewCardRadioInterfaceIndex >= 0 )
         return s_iNewCardRadioInterfaceIndex;
      return MAX_RADIO_INTERFACES;
   }
   return -1;
}

void _controller_interfaces_add_card(const char* szMAC)
{
   if ( (NULL == szMAC) || (0 == szMAC[0]) )
      return;

   radio_hw_info_t* pRadioInfo = hardware_get_radio_info_from_mac(szMAC);

   strncpy(s_CIS.listRadioInterfaces[s_CIS.radioInterfacesCount].szMAC, szMAC, MAX_MAC_LENGTH-1);
   s_CIS.listRadioInterfaces[s_CIS.radioInterfacesCount].szMAC[MAX_MAC_LENGTH-1] = 0;
   s_CIS.listRadioInterfaces[s_CIS.radioInterfacesCount].cardModel = 0;
   if ( NULL != pRadioInfo )
      s_CIS.listRadioInterfaces[s_CIS.radioInterfacesCount].cardModel = pRadioInfo->iCardModel;
   s_CIS.listRadioInterfaces[s_CIS.radioInterfacesCount].capabilities_flags = RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA | RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
   
   if ( hardware_radio_is_sik_radio(pRadioInfo) )
      s_CIS.listRadioInterfaces[s_CIS.radioInterfacesCount].capabilities_flags &= ~(RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO);

   s_CIS.listRadioInterfaces[s_CIS.radioInterfacesCount].iRawPowerLevel = DEFAULT_RADIO_TX_POWER_CONTROLLER;
   s_CIS.listRadioInterfaces[s_CIS.radioInterfacesCount].szUserDefinedName = (char*)malloc(2);
   s_CIS.listRadioInterfaces[s_CIS.radioInterfacesCount].szUserDefinedName[0] = 0;
   s_CIS.listRadioInterfaces[s_CIS.radioInterfacesCount].iInternal = false;
   s_CIS.radioInterfacesCount++;
   s_bAddedNewRadioInterfaces = true;

   s_iNewCardRadioInterfaceIndex = -1;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      pRadioInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioInfo )
         continue;
      if ( 0 != strcmp(pRadioInfo->szMAC, szMAC) )
         continue;
      s_iNewCardRadioInterfaceIndex = i;
      break;
   }
}

int _controller_interfaces_get_card_index(const char* szMAC)
{
   if ( (NULL == szMAC) || (0 == szMAC[0]) )
      return -1;
   int index = -1;
   for( int i=0; i<s_CIS.radioInterfacesCount; i++ )
      if ( 0 == strncmp(szMAC, s_CIS.listRadioInterfaces[i].szMAC, MAX_MAC_LENGTH) )
      {
         index = i;
         break;
      }
   if ( index == -1 )
   if ( s_CIS.radioInterfacesCount < CONTROLLER_INTERFACES_MAX_LIST_SIZE-1 )
   {
      index = s_CIS.radioInterfacesCount;
      _controller_interfaces_add_card(szMAC);
   }
   return index;
}

void reset_ControllerInterfacesSettings()
{
   memset(&s_CIS, 0, sizeof(s_CIS));
   s_CIS.radioInterfacesCount = 0;
   s_CIS.listMACTXPreferredCount = 0;
   s_CIS.inputInterfacesCount = 0;
}

int save_ControllerInterfacesSettings()
{
   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CONTROLLER_INTERFACES);
   FILE* fd = fopen(szFile, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("CtrlInterfaces: Failed to save controller interfaces settings to file: %s", szFile);
      return 0;
   }

   fprintf(fd, "%s\n", CONTROLLER_INTERFACES_STAMP_ID);
   
   fprintf(fd, "radio_cards_settings: %d\n", s_CIS.radioInterfacesCount );
   for( int i=0; i<s_CIS.radioInterfacesCount; i++ )
   {
      char szBuff[1024];
      strcpy(szBuff, s_CIS.listRadioInterfaces[i].szUserDefinedName);
      int kSize = (int)strlen(szBuff);
      for( int k=0; k<kSize; k++ )
         if ( szBuff[k] == ' ' || szBuff[k] == 10 || szBuff[k] == 13 )
            szBuff[k] = '~';
      if ( szBuff[0] == '~' && szBuff[1] == 0 )
         szBuff[0] = 0;
      if (  0 == szBuff[0] )
         fprintf(fd, "~ %s %d %u %d\n", s_CIS.listRadioInterfaces[i].szMAC, s_CIS.listRadioInterfaces[i].cardModel, s_CIS.listRadioInterfaces[i].capabilities_flags, s_CIS.listRadioInterfaces[i].iRawPowerLevel);
      else
         fprintf(fd, "%s %s %d %u %d\n", szBuff, s_CIS.listRadioInterfaces[i].szMAC, s_CIS.listRadioInterfaces[i].cardModel, s_CIS.listRadioInterfaces[i].capabilities_flags, s_CIS.listRadioInterfaces[i].iRawPowerLevel);
      fprintf(fd, "%d\n", s_CIS.listRadioInterfaces[i].iInternal);
      log_line("CtrlInterfaces: Saved raw tx power for card %d (MAC: %s, card model: %s): %d", i, s_CIS.listRadioInterfaces[i].szMAC, str_get_radio_card_model_string(s_CIS.listRadioInterfaces[i].cardModel), s_CIS.listRadioInterfaces[i].iRawPowerLevel);
   }

   fprintf(fd, "TX_Preferred: %d\n", s_CIS.listMACTXPreferredCount );
   for( int i=0; i<s_CIS.listMACTXPreferredCount; i++ )
      fprintf(fd, " %s\n", s_CIS.listMACTXPreferredOrdered[i]);

   fprintf(fd, "Input_Interfaces: %d\n", s_CIS.inputInterfacesCount );
   for( int i=0; i<s_CIS.inputInterfacesCount; i++ )
   {
      char szName[MAX_JOYSTICK_INTERFACE_NAME];
      strcpy(szName, s_CIS.inputInterfaces[i].szInterfaceName );
      int len = strlen(szName);
      for( int j=0; j<len; j++ )
         if ( szName[j] == ' ' )
            szName[j] = '~';
      fprintf(fd, "%d %s\n", i+1, szName );
      fprintf(fd, "%u %d %d %d\n", s_CIS.inputInterfaces[i].uId, s_CIS.inputInterfaces[i].bCalibrated, s_CIS.inputInterfaces[i].countAxes, s_CIS.inputInterfaces[i].countButtons );
      for( int k=0; k<MAX_JOYSTICK_AXES; k++ )
         fprintf(fd, "%d %d %d %d\n", s_CIS.inputInterfaces[i].axesMinValue[k], s_CIS.inputInterfaces[i].axesMaxValue[k], s_CIS.inputInterfaces[i].axesCenterValue[k], s_CIS.inputInterfaces[i].axesCenterZone[k] );

      for( int k=0; k<MAX_JOYSTICK_BUTTONS; k++ )
         fprintf(fd, "%d ", s_CIS.inputInterfaces[i].buttonsReleasedValue[k] );
      fprintf(fd, "\n");

      for( int k=0; k<MAX_JOYSTICK_BUTTONS; k++ )
         fprintf(fd, "%d ", s_CIS.inputInterfaces[i].buttonsMinValue[k] );
      fprintf(fd, "\n");

      for( int k=0; k<MAX_JOYSTICK_BUTTONS; k++ )
         fprintf(fd, "%d ", s_CIS.inputInterfaces[i].buttonsMaxValue[k] );
      fprintf(fd, "\n");
   }

   // Extra params

   if ( NULL != fd )
      fclose(fd);

   log_line("CtrlInterfaces: Saved controller interfaces settings to file: %s", szFile);
   log_line("CtrlInterfaces: Saved controller interfaces settings for %d radio interfaces, %d input devices.", s_CIS.radioInterfacesCount, s_CIS.inputInterfacesCount);
   return 1;
}

int load_ControllerInterfacesSettings()
{
   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CONTROLLER_INTERFACES);
   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("CtrlInterfaces: Failed to load controller interfaces settings from file: %s (missing file). Reset and create the file.", szFile);
      reset_ControllerInterfacesSettings();
      save_ControllerInterfacesSettings();
      return 0;
   }

   int failed = 0;
   char szBuff[256];
   szBuff[0] = 0;
   if ( 1 != fscanf(fd, "%s", szBuff) )
      failed = 1;
   if ( 0 != strcmp(szBuff, CONTROLLER_INTERFACES_STAMP_ID) )
      failed = 2;

   if ( failed )
   {
      reset_ControllerInterfacesSettings();
      log_softerror_and_alarm("CtrlInterfaces: Failed to load controller interfaces settings from file: %s (invalid config file stamp)", szFile);
      fclose(fd);
      return 0;
   }

   // Radio interfaces

   s_CIS.radioInterfacesCount = 0;
   if ( 1 != fscanf(fd, "%*s %d", &s_CIS.radioInterfacesCount) )
   {
      failed = 4;
      s_CIS.radioInterfacesCount = 0;
      log_softerror_and_alarm("CtrlInterfaces: Failed to load controller interfaces settings from file: %s (invalid ri count)", szFile);
   }

   if ( ! failed )
   {
      log_line("CtrlInterfaces: Loading %d controller interfaces settings...", s_CIS.radioInterfacesCount);
      for( int i=0; i<s_CIS.radioInterfacesCount; i++ )
      {
         u32 tmp = 0;
         int tmp2 = 0;
         s_CIS.listRadioInterfaces[i].szUserDefinedName = (char*)malloc(64);
         if ( 5 != fscanf(fd, "%s %s %d %u %d", s_CIS.listRadioInterfaces[i].szUserDefinedName, s_CIS.listRadioInterfaces[i].szMAC, &(s_CIS.listRadioInterfaces[i].cardModel), &tmp, &tmp2) )
         {
            failed = 5;
            log_softerror_and_alarm("CtrlInterfaces: Failed to load controller interfaces settings from file: %s (invalid ri data)", szFile);
         }
         if ( failed )
         {
            s_CIS.radioInterfacesCount = 0;
            break;
         }

         if ( 1 != fscanf(fd, "%d", &s_CIS.listRadioInterfaces[i].iInternal) )
         {
            failed = 1;
            s_CIS.radioInterfacesCount = 0;
            break;
         }

         s_CIS.listRadioInterfaces[i].capabilities_flags = tmp;
         s_CIS.listRadioInterfaces[i].iRawPowerLevel = tmp2;

         int kSize = (int)strlen(s_CIS.listRadioInterfaces[i].szUserDefinedName);
         for( int k=0; k<kSize; k++ )
            if ( s_CIS.listRadioInterfaces[i].szUserDefinedName[k] == '~' )
               s_CIS.listRadioInterfaces[i].szUserDefinedName[k] = ' ';

         if ( 0 == s_CIS.listRadioInterfaces[i].szUserDefinedName[0] || (s_CIS.listRadioInterfaces[i].szUserDefinedName[0] == ' ' && 0 == s_CIS.listRadioInterfaces[i].szUserDefinedName[1]) )
         {
             s_CIS.listRadioInterfaces[i].szUserDefinedName[0] = 0;
         }
         char szFlags[128];
         str_get_radio_capabilities_description(s_CIS.listRadioInterfaces[i].capabilities_flags, szFlags);
         log_line("CtrlInterfaces: Loaded controller interface %d settings: MAC: [%s], name: [%s], card type: %s, flags: %s, raw tx power: %d",
            i, s_CIS.listRadioInterfaces[i].szMAC, s_CIS.listRadioInterfaces[i].szUserDefinedName,
            str_get_radio_card_model_string(s_CIS.listRadioInterfaces[i].cardModel),
            szFlags, s_CIS.listRadioInterfaces[i].iRawPowerLevel );
      }
   }
   s_CIS.listMACTXPreferredCount = 0;
   if ( 1 != fscanf(fd, "%*s %d", &s_CIS.listMACTXPreferredCount) )
   {
      failed = 6;
      s_CIS.listMACTXPreferredCount = 0;
      log_softerror_and_alarm("CtrlInterfaces: Failed to load controller interfaces settings from file: %s (invalid txp count)", szFile);
   }

   if ( ! failed )
   for( int i=0; i<s_CIS.listMACTXPreferredCount; i++ )
   {
      if ( 1 != fscanf(fd, "%s", s_CIS.listMACTXPreferredOrdered[i]) )
      {
         failed = 7;
         log_softerror_and_alarm("CtrlInterfaces: Failed to load controller interfaces settings from file: %s (invalid txp data)", szFile);
      }
      if ( failed )
      {
         s_CIS.listMACTXPreferredCount = 0;
         break;
      }
   }

   // Input interfaces

   if ( (!failed) && (1 != fscanf(fd, "%*s %d", &s_CIS.inputInterfacesCount)) )
   {
      failed = 8;
      s_CIS.inputInterfacesCount = 0;
      log_softerror_and_alarm("CtrlInterfaces: Failed to load controller interfaces settings from file: %s (invalid input icount)", szFile);
   }
   if ( ! failed )
   for( int i=0; i<s_CIS.inputInterfacesCount; i++ )
   {
      int index = 0;
      if ( 2 != fscanf(fd, "%d %s", &index, s_CIS.inputInterfaces[i].szInterfaceName) )
      {
         failed = 9;
         log_softerror_and_alarm("CtrlInterfaces: Failed to load controller interfaces settings from file: %s (invalid ii data)", szFile);
      }
      if ( index != i+1 )
         failed = 10;
      if ( failed == 1 )
         break;
      int len = strlen(s_CIS.inputInterfaces[i].szInterfaceName);
      for( int j=0; j<len; j++ )
         if ( s_CIS.inputInterfaces[i].szInterfaceName[j] == '~' )
            s_CIS.inputInterfaces[i].szInterfaceName[j] = ' ';
      int tmp1, tmp2, tmp3;
      if ( 4 != fscanf(fd, "%u %d %d %d", &s_CIS.inputInterfaces[i].uId, &tmp1, &tmp2, &tmp3) )
         failed = 11;
      s_CIS.inputInterfaces[i].bCalibrated = tmp1;
      s_CIS.inputInterfaces[i].countAxes = tmp2;
      s_CIS.inputInterfaces[i].countButtons = tmp3;
      s_CIS.inputInterfaces[i].currentHardwareIndex = -1;

      for( int k=0; k<MAX_JOYSTICK_AXES; k++ )
         if ( 4 != fscanf(fd, "%d %d %d %d", &s_CIS.inputInterfaces[i].axesMinValue[k], &s_CIS.inputInterfaces[i].axesMaxValue[k], &s_CIS.inputInterfaces[i].axesCenterValue[k], &s_CIS.inputInterfaces[i].axesCenterZone[k]) )
            failed = 12;

      for( int k=0; k<MAX_JOYSTICK_BUTTONS; k++ )
         if ( 1 != fscanf(fd, "%d", &s_CIS.inputInterfaces[i].buttonsReleasedValue[k]) )
            failed = 13;

      for( int k=0; k<MAX_JOYSTICK_BUTTONS; k++ )
         if ( 1 != fscanf(fd, "%d", &s_CIS.inputInterfaces[i].buttonsMinValue[k]) )
            failed = 14;

      for( int k=0; k<MAX_JOYSTICK_BUTTONS; k++ )
         if ( 1 != fscanf(fd, "%d", &s_CIS.inputInterfaces[i].buttonsMaxValue[k]) )
            failed = 15;
   }

   // Extra params
   if ( ! failed )
   {
   }

   if ( failed )
   {
      log_softerror_and_alarm("CtrlInterfaces: Failed to load controller interfaces settings from file: %s (invalid config file, error code: %d)", szFile, failed);
      fclose(fd);
      return 0;
   }
   fclose(fd);
   log_line("CtrlInterfaces: Loaded controller interfaces settings from file: %s", szFile);
   log_line("CtrlInterfaces: Loaded controller interfaces settings for %d radio interfaces, %d preferred Tx cards, %d input devices.", s_CIS.radioInterfacesCount, s_CIS.listMACTXPreferredCount, s_CIS.inputInterfacesCount);
   controllerRadioInterfacesLogInfo();
   return 1;
}

ControllerInterfacesSettings* get_ControllerInterfacesSettings()
{
   return &s_CIS;
}

void controllerRadioInterfacesLogInfo()
{
   char szBuff[1024];

   log_line("=================================================================");
   log_line("Controller radio interfaces flags and info:");
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioInfo )
      {
         log_softerror_and_alarm("CtrlInterfaces: Failed to get radio hardware info for radio interface %d.", i+1);
         continue;
      }
      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioInfo->szMAC);
      szBuff[0] = 0;
      if ( controllerIsCardRXOnly(pRadioInfo->szMAC) )
         strcat(szBuff, "[RX ONLY]");
      if ( controllerIsCardTXOnly(pRadioInfo->szMAC) )
         strcat(szBuff, "[TX ONLY]");
      if ( ! controllerIsCardRXOnly(pRadioInfo->szMAC) )
      if ( ! controllerIsCardTXOnly(pRadioInfo->szMAC) )
         strcat(szBuff, "[RX/TX]");

      int iPreferredTxPriority = controllerIsCardTXPreferred(pRadioInfo->szMAC);

      char szBands[128];
      str_get_supported_bands_string(pRadioInfo->supportedBands, szBands);
      
      if ( NULL != pCardInfo )
         log_line("CtrlInterfaces: * RadioInterface %d: card type: %s, %s MAC:%s phy#%d, %s %s, %s, raw_tx_power: %d, preferred for Tx: %d", i+1, str_get_radio_card_model_string(pCardInfo->cardModel), pRadioInfo->szName, pRadioInfo->szMAC, pRadioInfo->phy_index, (controllerIsCardDisabled(pRadioInfo->szMAC)?"[DISABLED]":"[ENABLED]"), szBuff, szBands, pCardInfo->iRawPowerLevel, iPreferredTxPriority);
      else
         log_line("CtrlInterfaces: * RadioInterface %d: card type: %s, %s MAC:%s phy#%d, %s %s, %s, preferred for Tx: %d", i+1, "Unknown Type", pRadioInfo->szName, pRadioInfo->szMAC, pRadioInfo->phy_index, (controllerIsCardDisabled(pRadioInfo->szMAC)?"[DISABLED]":"[ENABLED]"), szBuff, szBands, iPreferredTxPriority);
      u32 uFlags = controllerGetCardFlags(pRadioInfo->szMAC);
      szBuff[0] = 0;
      str_get_radio_capabilities_description(uFlags, szBuff);
      log_line("CtrlInterfaces:       Controller set flags: %s", szBuff);
   }
   log_line("=================================================================");
}

t_ControllerRadioInterfaceInfo* controllerGetRadioCardInfo(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return NULL;
   return &(s_CIS.listRadioInterfaces[index]);
}

int controllerIsCardDisabled(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return 0;

   if ( s_CIS.listRadioInterfaces[index].capabilities_flags & RADIO_HW_CAPABILITY_FLAG_DISABLED )   
      return 1;
   return 0;
}

int controllerIsCardRXOnly(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return 0;
   if ( !(s_CIS.listRadioInterfaces[index].capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
      return 1;
   return 0;
}

int controllerIsCardTXOnly(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return 0;
   if ( !(s_CIS.listRadioInterfaces[index].capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
      return 1;
   return 0;
}

void controllerSetCardDisabled(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return;

   s_CIS.listRadioInterfaces[index].capabilities_flags = s_CIS.listRadioInterfaces[index].capabilities_flags | RADIO_HW_CAPABILITY_FLAG_DISABLED;

   log_line("Controller interfaces: Added radio interface %s to disabled list", szMAC);
   save_ControllerInterfacesSettings();
}

void controllerRemoveCardDisabled(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return;

   s_CIS.listRadioInterfaces[index].capabilities_flags = s_CIS.listRadioInterfaces[index].capabilities_flags & (~RADIO_HW_CAPABILITY_FLAG_DISABLED);

   log_line("Controller interfaces: Removed radio interface %s from disabled list", szMAC);
   save_ControllerInterfacesSettings();
}


void controllerSetCardTXRX(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return;

   s_CIS.listRadioInterfaces[index].capabilities_flags = s_CIS.listRadioInterfaces[index].capabilities_flags | RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;

   log_line("Controller interfaces: Set radio interface %s as TX/RX", szMAC);

   save_ControllerInterfacesSettings();
}

void controllerSetCardRXOnly(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return;

   s_CIS.listRadioInterfaces[index].capabilities_flags = (s_CIS.listRadioInterfaces[index].capabilities_flags | RADIO_HW_CAPABILITY_FLAG_CAN_RX) & (~RADIO_HW_CAPABILITY_FLAG_CAN_TX);

   log_line("Controller interfaces: Set radio interface %s as RX only", szMAC);

   save_ControllerInterfacesSettings();
}

void controllerSetCardTXOnly(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return;

   s_CIS.listRadioInterfaces[index].capabilities_flags = (s_CIS.listRadioInterfaces[index].capabilities_flags | RADIO_HW_CAPABILITY_FLAG_CAN_TX) & (~RADIO_HW_CAPABILITY_FLAG_CAN_RX);

   log_line("Controller interfaces: Set radio interface %s as TX only", szMAC);

   save_ControllerInterfacesSettings();
}


// Returns the priority index (lower = higher priority )
int controllerIsCardTXPreferred(const char* szMAC)
{
   if ( (NULL == szMAC) || (0 == szMAC[0]) )
      return 0;
   for( int i=0; i<s_CIS.listMACTXPreferredCount; i++ )
   {
      if ( 0 == strcmp(szMAC, s_CIS.listMACTXPreferredOrdered[i]) )
         return i+1;
   }
   return 0;
}

int controllerGetTXPreferredIndexForCard(const char* szMAC)
{
   return controllerIsCardTXPreferred(szMAC);
}

void controllerSetCardTXPreferred(const char* szMAC)
{
   if ( (NULL == szMAC) || (0 == szMAC[0]) )
      return;
   int iIndexControllerCard = _controller_interfaces_get_card_index(szMAC);
   if ( iIndexControllerCard < 0 )
   {
      log_softerror_and_alarm("CtrlInterfaces: Can't find MAC in the list of current cards.", szMAC);
      controllerRadioInterfacesLogInfo();
      return;
   }
   // Is already on top of the list?
   int iCurrentPriority = controllerIsCardTXPreferred(szMAC);
   if ( 1 == iCurrentPriority )
   {
      log_line("CtrlInterfaces: Radio interface %s (card type: %s) is already on top of preferred Tx  list. Do nothing.", szMAC, str_get_radio_card_model_string(s_CIS.listRadioInterfaces[iIndexControllerCard].cardModel));
      return;
   }
   // Is already in the list? Move it to first position
   if ( iCurrentPriority > 0 )
   {
      char szTmp[MAX_MAC_LENGTH+1];
      strcpy(szTmp, s_CIS.listMACTXPreferredOrdered[iCurrentPriority-1]);

      for( int i=iCurrentPriority-1; i>0; i-- )
         strcpy(s_CIS.listMACTXPreferredOrdered[i], s_CIS.listMACTXPreferredOrdered[i-1]);

      strcpy(s_CIS.listMACTXPreferredOrdered[0], szTmp);
      log_line("CtrlInterfaces: Moved radio interface %s (card type: %s) to top of preferred Tx list.", s_CIS.listRadioInterfaces[iIndexControllerCard].szMAC, str_get_radio_card_model_string(s_CIS.listRadioInterfaces[iIndexControllerCard].cardModel));
      save_ControllerInterfacesSettings();
      return;
   }

   if ( s_CIS.listMACTXPreferredCount >= CONTROLLER_INTERFACES_MAX_LIST_SIZE )
      return;

   log_line("CtrlInterfaces: Adding radio interface %s (card type: %s) to preferred Tx list on top.", szMAC, str_get_radio_card_model_string(s_CIS.listRadioInterfaces[iIndexControllerCard].cardModel));

   for( int i=s_CIS.listMACTXPreferredCount-1; i>0; i--)
      strcpy(s_CIS.listMACTXPreferredOrdered[i], s_CIS.listMACTXPreferredOrdered[i-1]);
   strncpy(s_CIS.listMACTXPreferredOrdered[0], szMAC, MAX_MAC_LENGTH-1);
   s_CIS.listMACTXPreferredOrdered[0][MAX_MAC_LENGTH-1] = 0;
   s_CIS.listMACTXPreferredCount++;
   
   save_ControllerInterfacesSettings();
}

void controllerRemoveCardTXPreferred(const char* szMAC)
{
   if ( (NULL == szMAC) || (0 == szMAC[0]) )
      return;
   int iCurrentPriority = controllerIsCardTXPreferred(szMAC);
   int iIndexControllerCard = _controller_interfaces_get_card_index(szMAC);
   if ( iCurrentPriority <= 0 )
   {
      if ( iIndexControllerCard < 0 )
         log_line("CtrlInterfaces: Radio card %s (unknown card) is not currently a preferred Tx card.", szMAC);
      else
         log_line("CtrlInterfaces: Radio card %s (card type: %s) is not currently a preferred Tx card.", s_CIS.listRadioInterfaces[iIndexControllerCard].szMAC, str_get_radio_card_model_string(s_CIS.listRadioInterfaces[iIndexControllerCard].cardModel));
      return;
   }
   for( int i=iCurrentPriority-1; i<s_CIS.listMACTXPreferredCount-1; i++ )
      strcpy(s_CIS.listMACTXPreferredOrdered[i], s_CIS.listMACTXPreferredOrdered[i+1]);

   s_CIS.listMACTXPreferredCount--;
   
   if ( iIndexControllerCard < 0 )
      log_line("CtrlInterfaces: Removed radio interface %s (unknown card) from preferred Tx list.", szMAC);
   else
      log_line("CtrlInterfaces: Removed radio interface %s (card type: %s) from preferred Tx list.", s_CIS.listRadioInterfaces[iIndexControllerCard].szMAC, str_get_radio_card_model_string(s_CIS.listRadioInterfaces[iIndexControllerCard].cardModel));
   save_ControllerInterfacesSettings();
}

void controllerSetCardInternal(const char* szMAC, bool bInternal)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return;
   s_CIS.listRadioInterfaces[index].iInternal = (int)bInternal;
}

bool controllerIsCardInternal(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return false;
   return (bool) s_CIS.listRadioInterfaces[index].iInternal;
}

u32 controllerGetCardFlags(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return 0;

   return s_CIS.listRadioInterfaces[index].capabilities_flags;
}

void controllerSetCardFlags(const char* szMAC, u32 flags)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return;

   s_CIS.listRadioInterfaces[index].capabilities_flags = flags;
}


void controllerGetCardUserDefinedNameOrType(radio_hw_info_t* pRadioHWInfo, char* szOutput)
{
   if ( NULL != szOutput )
      szOutput[0] = 0;
   if ( (NULL == pRadioHWInfo) || (NULL == szOutput) )
      return;
   char* szN = controllerGetCardUserDefinedName(pRadioHWInfo->szMAC);
   if ( (NULL != szN) && (0 != szN[0]) )
   {
      strcpy(szOutput, szN);
      return;
   }
   
   t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
   if ( NULL != pCardInfo )
   {
      const char* szCardModel = str_get_radio_card_model_string(pCardInfo->cardModel);
      if ( NULL != szCardModel && 0 != szCardModel[0] )
      {
         strcpy(szOutput, szCardModel);
         return;
      }
   }
         
   strcpy(szOutput, "Generic");
}


void controllerGetCardUserDefinedNameOrShortType(radio_hw_info_t* pRadioHWInfo, char* szOutput)
{
   if ( NULL != szOutput )
      szOutput[0] = 0;
   if ( (NULL == pRadioHWInfo) || (NULL == szOutput) )
      return;
   char* szN = controllerGetCardUserDefinedName(pRadioHWInfo->szMAC);
   if ( (NULL != szN) && (0 != szN[0]) )
   {
      strcpy(szOutput, szN);
      return;
   }
   
   t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
   if ( NULL != pCardInfo )
   {
      const char* szCardModel = str_get_radio_card_model_string_short(pCardInfo->cardModel);
      if ( (NULL != szCardModel) && (0 != szCardModel[0]) )
      {
         strcpy(szOutput, szCardModel);
         return;
      }
   }
         
   strcpy(szOutput, "Generic");
}

char* controllerGetCardUserDefinedName(const char* szMAC)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return NULL;
   return s_CIS.listRadioInterfaces[index].szUserDefinedName;
}

void controllerSetCardUserDefinedName(const char* szMAC, const char* szName)
{
   int index = _controller_interfaces_get_card_index(szMAC);
   if ( -1 == index )
      return;

   if ( NULL != s_CIS.listRadioInterfaces[index].szUserDefinedName )
      free(s_CIS.listRadioInterfaces[index].szUserDefinedName);

   if ( NULL == szName || 0 == szName[0] || (' ' == szName[0] && 0 == szName[1])  )
   {
      s_CIS.listRadioInterfaces[index].szUserDefinedName = (char*) malloc(2);
      s_CIS.listRadioInterfaces[index].szUserDefinedName[0] = 0;
      return;
   }
   s_CIS.listRadioInterfaces[index].szUserDefinedName = (char*) malloc(strlen(szName)+1);
   strcpy(s_CIS.listRadioInterfaces[index].szUserDefinedName, szName);
}

/*
int _controllerComputeRXTXCardsSingleFrequency(Model* pModel, int* pFrequencies, int* pRXCardIndexes, int& countCardsRX, int* pTXCardIndexes, int& countCardsTX)
{
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadio = hardware_get_radio_info(i);
      if ( NULL == pRadio )
      {
         *pFrequencies = 0;
         pFrequencies++;
         continue;
      }

      u32 flags = controllerGetCardFlags(pRadio->szMAC);
      if ( (flags & RADIO_HW_CAPABILITY_FLAG_DISABLED) || controllerIsCardDisabled(pRadio->szMAC) )
      {
         *pFrequencies = 0;
         pFrequencies++;
         continue;
      }

      *pFrequencies = pModel->radioLinksParams.link_frequency_khz[0];
      pFrequencies++;
   }

   int* pListTXCards = pTXCardIndexes;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadio = hardware_get_radio_info(i);
      if ( NULL == pRadio )
         continue;
      u32 flags = controllerGetCardFlags(pRadio->szMAC);
      if ( (flags & RADIO_HW_CAPABILITY_FLAG_DISABLED) || controllerIsCardDisabled(pRadio->szMAC) )
         continue;

      if ( 0 == hardware_radioindex_supports_frequency(i, pModel->radioLinksParams.link_frequency_khz[0]) )
         continue;

      if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) || (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
      {
         *pRXCardIndexes = i;
         pRXCardIndexes++;
         countCardsRX++;
         log_line("Added card %d: %s to RX list", i+1, pRadio->szUSBPort);
      }

      if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
      {
         *pTXCardIndexes = i;
         pTXCardIndexes++;
         countCardsTX++;
         log_line("Added card %d: %s to TX list", i+1, pRadio->szUSBPort);
      }
   }

   // Leave only one tx card in the list, the one that is most preffered;
   // If automatic selection, leave all of them (tx) in the list

   ControllerSettings* pCS = get_ControllerSettings();

   if ( 0 == pCS->nAutomaticTxCard )
   {
      int maxPrefferedValue = -1;
      int maxPrefferedIndex = -1;
      radio_hw_info_t* pRadioTx = NULL;
      for( int i=0; i<countCardsTX; i++ )
      {
         radio_hw_info_t* pRadio = hardware_get_radio_info(pListTXCards[i]);
         int prefValue = controllerGetTXPreferredIndexForCard(pRadio->szMAC);
         if ( prefValue >= maxPrefferedValue )
         {
            maxPrefferedValue = prefValue;
            maxPrefferedIndex = pListTXCards[i];
            pRadioTx = pRadio;
         }
      }

      if ( maxPrefferedIndex >= 0 )
      {
         pListTXCards[0] = maxPrefferedIndex;
         log_line("Setting card %d: %s as the one used for TX.", maxPrefferedIndex, pRadioTx->szUSBPort);
      }
      countCardsTX = 1;
   }
   log_line("Total cards used for vehicle radio links: %d cards will RX, %d cards will TX", countCardsRX, countCardsTX);

   if ( 0 == countCardsRX || 0 == countCardsTX )
   {
      log_softerror_and_alarm("Failed to find a Tx or Rx card for the vehicle link.");
      return 0;
   }
   return 1;
}

int _controllerComputeRXTXCardsDualFrequency(Model* pModel, int* pFrequencies, int* pRXCardIndexes, int& countCardsRX, int* pTXCardIndexes, int& countCardsTX)
{
   // First, detect which cards on the controller support which vehicle radio links frequencies

   bool bNicSupportsRxMain[MAX_RADIO_INTERFACES];
   bool bNicSupportsRxSecondary[MAX_RADIO_INTERFACES];
   bool bNicSupportsTxMain[MAX_RADIO_INTERFACES];
   bool bNicSupportsTxSecondary[MAX_RADIO_INTERFACES];

   int iCountSupportsRxMain = 0;
   int iCountSupportsRxSecondary = 0;
   int iCountSupportsTxMain = 0;
   int iCountSupportsTxSecondary = 0;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      bNicSupportsRxMain[i] = false;
      bNicSupportsRxSecondary[i] = false;
      bNicSupportsTxMain[i] = false;
      bNicSupportsTxSecondary[i] = false;
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadio = hardware_get_radio_info(i);
      if ( NULL == pRadio )
         continue;
      u32 flags = controllerGetCardFlags(pRadio->szMAC);
      if ( (flags & RADIO_HW_CAPABILITY_FLAG_DISABLED) || controllerIsCardDisabled(pRadio->szMAC) )
         continue;

      if ( hardware_radio_supports_frequency(pRadio, pModel->radioLinksParams.link_frequency_khz[0]) )
      {
         if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
         if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) || (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
         {
            bNicSupportsRxMain[i] = true;
            iCountSupportsRxMain++;
         }
         if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
         if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
         {
            bNicSupportsTxMain[i] = true;
            iCountSupportsTxMain++;
         }
      }
      if ( hardware_radio_supports_frequency(pRadio, pModel->radioLinksParams.link_frequency_khz[1]) )
      {
         if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
         if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) || (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
         {
            bNicSupportsRxSecondary[i] = true;
            iCountSupportsRxSecondary++;
         }
         if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
         if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
         {
            bNicSupportsTxSecondary[i] = true;
            iCountSupportsTxSecondary++;
         }
      }
      log_line("Card %d supports: RX main: %s, TX main: %s, RX secondary: %s, TX secondary: %s", i, bNicSupportsRxMain[i]?"yes":"no", bNicSupportsTxMain[i]?"yes":"no", bNicSupportsRxSecondary[i]?"yes":"no", bNicSupportsTxSecondary[i]?"yes":"no");
   }
   log_line("Total: %d cards supports RX main, %d cards supports TX main, %d cards supports RX secondary, %d cards supports TX secondary.", iCountSupportsRxMain, iCountSupportsTxMain, iCountSupportsRxSecondary, iCountSupportsTxSecondary);

   int countAllocatedForMain = 0;
   int countAllocatedForSecondary = 0;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadio = hardware_get_radio_info(i);
      if ( NULL == pRadio )
      {
         *pFrequencies = 0;
         pFrequencies++;
         continue;
      }
      u32 flags = controllerGetCardFlags(pRadio->szMAC);
      if ( (flags & RADIO_HW_CAPABILITY_FLAG_DISABLED) || controllerIsCardDisabled(pRadio->szMAC) )
      {
         *pFrequencies = 0;
         pFrequencies++;
         continue;
      }

      if ( ( 1 == iCountSupportsRxMain && bNicSupportsRxMain[i] ) ||
           ( 1 == iCountSupportsTxMain && bNicSupportsTxMain[i] ) )
      {
         *pFrequencies = pModel->radioLinksParams.link_frequency_khz[0];
         pFrequencies++;
         countAllocatedForMain++;
      }
      else if ( ( 1 == iCountSupportsRxSecondary && bNicSupportsRxSecondary[i] ) ||
           ( 1 == iCountSupportsTxSecondary && bNicSupportsTxSecondary[i] ) )
      {
         *pFrequencies = pModel->radioLinksParams.link_frequency_khz[1];
         pFrequencies++;
         countAllocatedForSecondary++;
      }
      else if ( bNicSupportsRxMain[i] || bNicSupportsTxMain[i] )
      {
         if ( countAllocatedForMain > 0 )
         if ( countAllocatedForSecondary == 0 )
         if ( bNicSupportsRxSecondary[i] || bNicSupportsTxSecondary[i] )
         {
            *pFrequencies = pModel->radioLinksParams.link_frequency_khz[1];
            pFrequencies++;
            countAllocatedForSecondary++;
            continue;
         }
         *pFrequencies = pModel->radioLinksParams.link_frequency_khz[0];
         pFrequencies++;
         countAllocatedForMain++;
      }
      else
      {
         *pFrequencies = pModel->radioLinksParams.link_frequency_khz[1];
         pFrequencies++;
         countAllocatedForSecondary++;
      }
   }

   bool bSetTxForMain = false;
   bool bSetTxForSecondary = false;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadio = hardware_get_radio_info(i);
      if ( NULL == pRadio )
         continue;

      u32 flags = controllerGetCardFlags(pRadio->szMAC);
      if ( (flags & RADIO_HW_CAPABILITY_FLAG_DISABLED) || controllerIsCardDisabled(pRadio->szMAC) )
         continue;

      if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) || (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) )
      {
         *pRXCardIndexes = i;
         pRXCardIndexes++;
         countCardsRX++;
      }
      if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) )
      {
         if ( 1 == iCountSupportsTxMain && bNicSupportsTxMain[i] && (!bSetTxForMain) )
         {
            *pTXCardIndexes = i;
            pTXCardIndexes++;
            countCardsTX++;
            bSetTxForMain = true;
         }
         else if ( 1 == iCountSupportsTxSecondary && bNicSupportsTxSecondary[i] && (!bSetTxForSecondary) )
         {
            *pTXCardIndexes = i;
            pTXCardIndexes++;
            countCardsTX++;
            bSetTxForSecondary = true;
         }
         else if ( bNicSupportsTxSecondary[i] && (!bSetTxForSecondary) )
         {
            *pTXCardIndexes = i;
            pTXCardIndexes++;
            countCardsTX++;
            bSetTxForSecondary = true;
         }
         else if ( ! bSetTxForMain )
         {
            *pTXCardIndexes = i;
            pTXCardIndexes++;
            countCardsTX++;
            bSetTxForMain = true;
         }
      }
   }

   log_line("Total cards used for main frequency: %d cards will RX, %d cards will TX", iCountSupportsRxMain, iCountSupportsTxMain);
   log_line("Total card used for secondary frequency: %d will RX, %d cards will TX", iCountSupportsRxSecondary, iCountSupportsTxSecondary);

   if ( (! bSetTxForMain) || (! bSetTxForSecondary) )
   {
      log_softerror_and_alarm("Failed to find a Tx card for main or secondary vehicle link.");
      return 0;
   }
   if ( iCountSupportsRxMain == 0 || iCountSupportsRxSecondary == 0 ||
        iCountSupportsTxMain == 0 || iCountSupportsTxSecondary == 0 )
   {
      log_softerror_and_alarm("Failed to find a Tx or Rx card for main or secondary vehicle link.");
      return 0;
   }
   return 1;
}

int controllerComputeRXTXCards(Model* pModel, int iSearchFreq, int* pFrequencies, int* pRXCardIndexes, int& countCardsRX, int* pTXCardIndexes, int& countCardsTX)
{
   // If model is null, it's default model connect wait
   // If search freq is not 0, it's searching for models on a frequency

   if ( NULL == pFrequencies || NULL == pRXCardIndexes || NULL == pTXCardIndexes )
   {
      log_line("ControllerComputeRXTXCards: Invalid params or default vehicle.");
      return 0;
   }

   countCardsRX = 0;
   countCardsTX = 0;
   int* pListTXCards = pTXCardIndexes;

   // Is searching for vehicle on a frequency ?

   if ( 0 < iSearchFreq )
   {
      log_line("ControllerComputeRXTXCards: Searching mode: use fixed frequency for all and default params.");
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadio = hardware_get_radio_info(i);
         if ( NULL == pRadio )
         {
            *pFrequencies = 0;
            pFrequencies++;
            continue;
         }
         u32 flags = controllerGetCardFlags(pRadio->szMAC);
         if ( (flags & RADIO_HW_CAPABILITY_FLAG_DISABLED) || controllerIsCardDisabled(pRadio->szMAC) )
         {
            *pFrequencies = 0;
            pFrequencies++;
            continue;
         }

         *pFrequencies = iSearchFreq;
         pFrequencies++;

         if ( 0 == hardware_radio_supports_frequency(pRadio, iSearchFreq ) )
            continue;

         if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
         if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) && (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
         {
            *pRXCardIndexes = i;
            pRXCardIndexes++;
            countCardsRX++;
         }
      }
      if ( 0 == countCardsRX )
         return 0;
      return 1;
   }

   // First pairing

   if ( NULL == pModel )
   {
      log_line("ControllerComputeRXTXCards: First pairing, use default frequencies and params.");
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadio = hardware_get_radio_info(i);
         if ( NULL == pRadio )
         {
            *pFrequencies = 0;
            pFrequencies++;
            continue;
         }
         u32 flags = controllerGetCardFlags(pRadio->szMAC);
         if ( (flags & RADIO_HW_CAPABILITY_FLAG_DISABLED) || controllerIsCardDisabled(pRadio->szMAC) )
         {
            *pFrequencies = 0;
            pFrequencies++;
            continue;
         }

         int freq = DEFAULT_FREQUENCY;
         if ( pRadio->supportedBands & RADIO_HW_SUPPORTED_BAND_58 )
            freq = DEFAULT_FREQUENCY58;

         *pFrequencies = freq;
         pFrequencies++;

         if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
         if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) || (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
         {
            *pRXCardIndexes = i;
            pRXCardIndexes++;
            countCardsRX++;
         }
         if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
         if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
         {
            *pTXCardIndexes = i;
            pTXCardIndexes++;
            countCardsTX++;
         }
      }

      // Leave only one tx card in the list, the one that is most preffered;
      int maxPrefferedValue = -1;
      int maxPrefferedIndex = -1;
      for( int i=0; i<countCardsTX; i++ )
      {
         radio_hw_info_t* pRadio = hardware_get_radio_info(pListTXCards[i]);
         int prefValue = controllerGetTXPreferredIndexForCard(pRadio->szMAC);
         if ( prefValue >= maxPrefferedValue )
         {
            maxPrefferedValue = prefValue;
            maxPrefferedIndex = i;
         }
      }
      if ( maxPrefferedIndex >= 0 )
         pListTXCards[0] = maxPrefferedIndex;

      countCardsTX = 1;
      if ( 0 == countCardsRX || 0 == countCardsTX )
         return 0;
      return 1;
   }


   // Model with a single radio link

   if ( 1 == pModel->radioInterfacesParams.interfaces_count || 0 == pModel->radioLinksParams.link_frequency_khz[1] )
   {
      log_line("ControllerComputeRXTXCards: Detected single radio link vehicle.");         
      return _controllerComputeRXTXCardsSingleFrequency(pModel, pFrequencies, pRXCardIndexes, countCardsRX, pTXCardIndexes, countCardsTX);
      
   }

   // Model with multiple links

   log_line("ControllerComputeRXTXCards: Detected single radio link vehicle.");         
   return _controllerComputeRXTXCardsDualFrequency(pModel, pFrequencies, pRXCardIndexes, countCardsRX, pTXCardIndexes, countCardsTX);
}
*/



void controllerInterfacesEnumJoysticks()
{
   log_line("ControllerInterfacesSettings: Enumerating joysticks interfaces...");
   bool bNewInterfacesDetected = false;
   bool bInterfacesRemoved = false;

   hardware_enum_joystick_interfaces();
   for( int i=0; i<hardware_get_joystick_interfaces_count(); i++ )
   {
      hw_joystick_info_t* pJoystick = hardware_get_joystick_info(i);
      if ( NULL == pJoystick )
         continue;

      bool bFound = false;
      for( int j=0; j<s_CIS.inputInterfacesCount; j++ )
      {
         if ( 0 != strcmp(pJoystick->szName, s_CIS.inputInterfaces[j].szInterfaceName) )
            continue;
         if ( pJoystick->uId != s_CIS.inputInterfaces[j].uId )
            continue;
         if ( pJoystick->countAxes != s_CIS.inputInterfaces[j].countAxes )
            continue;
         if ( pJoystick->countButtons != s_CIS.inputInterfaces[j].countButtons )
            continue;
         s_CIS.inputInterfaces[j].currentHardwareIndex = i;
         #if defined (HW_PLATFORM_RADXA)
         for( int k=0; k<MAX_JOYSTICK_AXES; k++ )
         {
            s_CIS.inputInterfaces[j].axesMinValue[k] = -32780;
            s_CIS.inputInterfaces[j].axesMaxValue[k] = 32780;
         }
         #endif
         bFound = true;
         break;
      }
      if ( (! bFound) && (s_CIS.inputInterfacesCount < CONTROLLER_MAX_INPUT_INTERFACES-1) )
      {
         bNewInterfacesDetected = true;
         strcpy(s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].szInterfaceName, pJoystick->szName);
         s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].uId = pJoystick->uId;
         s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].countAxes = pJoystick->countAxes;
         s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].countButtons = pJoystick->countButtons;
         s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].bCalibrated = false;
         s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].currentHardwareIndex = i;
         for( int k=0; k<MAX_JOYSTICK_AXES; k++ )
         {
            s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].axesCenterZone[k] = 10; // 1% center zone
            s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].axesCenterValue[k] = 0;
            s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].axesMinValue[k] = -99;
            s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].axesMaxValue[k] = 99;
            #if defined (HW_PLATFORM_RADXA)
            s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].axesMinValue[k] = -32780;
            s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].axesMaxValue[k] = 32780;
            #endif
         }
         for( int k=0; k<MAX_JOYSTICK_BUTTONS; k++ )
         {
            s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].buttonsReleasedValue[k] = 0;
            s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].buttonsMinValue[k] = 0;
            s_CIS.inputInterfaces[s_CIS.inputInterfacesCount].buttonsMaxValue[k] = 1;
         }
         s_CIS.inputInterfacesCount++;
      }
   }

   // Remove missing ones 
   for( int j=0; j<s_CIS.inputInterfacesCount; j++ )
   {
      bool bHardwareFound = false;

      for( int i=0; i<hardware_get_joystick_interfaces_count(); i++ )
      {
         hw_joystick_info_t* pJoystick = hardware_get_joystick_info(i);
         if ( NULL == pJoystick )
            continue;

         if ( 0 != strcmp(pJoystick->szName, s_CIS.inputInterfaces[j].szInterfaceName) )
            continue;
         if ( pJoystick->uId != s_CIS.inputInterfaces[j].uId )
            continue;
         if ( pJoystick->countAxes != s_CIS.inputInterfaces[j].countAxes )
            continue;
         if ( pJoystick->countButtons != s_CIS.inputInterfaces[j].countButtons )
            continue;
         s_CIS.inputInterfaces[j].currentHardwareIndex = i;
         bHardwareFound = true;
         break;
      }

      if ( ! bHardwareFound )
      {
         bInterfacesRemoved = true;
         for( int i=j; i<s_CIS.inputInterfacesCount-1; i++ )
         {
            memcpy( &s_CIS.inputInterfaces[i], &s_CIS.inputInterfaces[i+1], sizeof(t_ControllerInputInterface));
         }
         s_CIS.inputInterfacesCount--;
      }
   }

   log_line("ControllerInterfacesSettings: Finished enumerating joysticks interfaces. New found? %s, removed any? %s", bNewInterfacesDetected?"yes":"no", bInterfacesRemoved?"yes":"no");

   if ( bNewInterfacesDetected || bInterfacesRemoved )
      save_ControllerInterfacesSettings();
}

t_ControllerInputInterface* controllerInterfacesGetAt(int index)
{
   if ( (index < 0) || (index >= hardware_get_joystick_interfaces_count()) )
      return NULL;
   
   hw_joystick_info_t* pJoystick = hardware_get_joystick_info(index);
   if ( NULL == pJoystick )
      return NULL;

   for( int i=0; i<s_CIS.inputInterfacesCount; i++ )
   {
      if ( s_CIS.inputInterfaces[i].currentHardwareIndex == index )
         return &s_CIS.inputInterfaces[i];
   }
   return NULL;
}

#else

int save_ControllerInterfacesSettings() { return 0; }
int load_ControllerInterfacesSettings() { return 0; }
void reset_ControllerInterfacesSettings() {}
ControllerInterfacesSettings* get_ControllerInterfacesSettings() { return NULL; }
void controllerRadioInterfacesLogInfo() {}
#endif