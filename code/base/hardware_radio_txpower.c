#include "base.h"
#include "config.h"
#include "hardware_radio_txpower.h"
#include "hardware_radio.h"
#include "hardware_procs.h"

void hardware_radio_set_txpower_raw_rtl8812au(int iCardIndex, int iTxPower)
{
   log_line("Setting radio interface %d RTL8812AU raw tx power to %d using iw dev...", iCardIndex+1, iTxPower);
   if ( (iTxPower < 1) || (iTxPower > MAX_TX_POWER) )
      iTxPower = DEFAULT_RADIO_TX_POWER;

   char szComm[256];
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( (iCardIndex != -1) && (iCardIndex != i) )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( (hardware_radio_driver_is_rtl8812au_card(pRadioHWInfo->iRadioDriver)) ||
           (pRadioHWInfo->iRadioType == RADIO_TYPE_RALINK) )
      {
         sprintf(szComm, "iw dev %s set txpower fixed %d", pRadioHWInfo->szName, -100*iTxPower);
         //hw_execute_process_wait(szComm);
         hw_execute_bash_command(szComm, NULL);
      }
   }

   log_line("Radio interface %d RTL8812AU raw tx power changed to: %d", iCardIndex+1, iTxPower);
}

void hardware_radio_set_txpower_raw_rtl8812eu(int iCardIndex, int iTxPower)
{
   log_line("Setting radio interface %d RTL8812EU raw tx power to: %d", iCardIndex+1, iTxPower);
   if ( (iTxPower < 1) || (iTxPower > MAX_TX_POWER) )
      iTxPower = DEFAULT_RADIO_TX_POWER;

   char szComm[256];
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( (iCardIndex != -1) && (iCardIndex != i) )
         continue;
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( hardware_radio_driver_is_rtl8812eu_card(pRadioHWInfo->iRadioDriver) )
      {
         sprintf(szComm, "iw dev %s set txpower fixed %d", pRadioHWInfo->szName, iTxPower*40);
         hw_execute_bash_command(szComm, NULL);
      }
   }

   log_line("Radio interface %d RTL8812EU raw tx power changed to: %d", iCardIndex+1, iTxPower);
}

void hardware_radio_set_txpower_raw_rtl8733bu(int iCardIndex, int iTxPower)
{
   log_line("Setting radio interface %d RTL8733BU raw tx power to: %d", iCardIndex+1, iTxPower);
   if ( (iTxPower < 1) || (iTxPower > MAX_TX_POWER) )
      iTxPower = DEFAULT_RADIO_TX_POWER;

   char szComm[256];
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( (iCardIndex != -1) && (iCardIndex != i) )
         continue;
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( hardware_radio_driver_is_rtl8733bu_card(pRadioHWInfo->iRadioDriver) )
      {
         sprintf(szComm, "iw dev %s set txpower fixed %d", pRadioHWInfo->szName, iTxPower*40);
         hw_execute_bash_command(szComm, NULL);
      }
   }

   log_line("Radio interface %d RTL8733BU raw tx power changed to: %d", iCardIndex+1, iTxPower);
}

void hardware_radio_set_txpower_raw_atheros(int iCardIndex, int iTxPower)
{
   log_line("Setting radio interface %d Atheros raw tx power to: %d", iCardIndex+1, iTxPower);
   if ( (iTxPower < 1) || (iTxPower > MAX_TX_POWER) )
      iTxPower = DEFAULT_RADIO_TX_POWER;

   #ifdef HW_PLATFORM_RASPBERRY
   int iHasOrgFile = 0;
   if ( access( "/etc/modprobe.d/ath9k_hw.conf.org", R_OK ) != -1 )
      iHasOrgFile = 1;
   if ( iHasOrgFile )
   {
      FILE *pF = fopen("/etc/modprobe.d/ath9k_hw.conf.org", "rb");
      if ( NULL == pF )
         iHasOrgFile = 0;
      else
      {
         fseek(pF, 0, SEEK_END);
         long fsize = ftell(pF);
         fclose(pF);
         if ( fsize < 20 || fsize > 512 )
            iHasOrgFile = 0;
      }
   }

   char szBuff[256];
   szBuff[0] = 0;
   if ( iHasOrgFile )
      sprintf(szBuff, "cp /etc/modprobe.d/ath9k_hw.conf.org tmp/ath9k_hw.conf; sed -i 's/txpower=[0-9]*/txpower=%d/g' tmp/ath9k_hw.conf; cp tmp/ath9k_hw.conf /etc/modprobe.d/", iTxPower);
   else if ( access( "/etc/modprobe.d/ath9k_hw.conf", R_OK ) != -1 )
      sprintf(szBuff, "cp /etc/modprobe.d/ath9k_hw.conf tmp/ath9k_hw.conf; sed -i 's/txpower=[0-9]*/txpower=%d/g' tmp/ath9k_hw.conf; cp tmp/ath9k_hw.conf /etc/modprobe.d/", iTxPower);
   else
      log_softerror_and_alarm("Can't access/find Atheros power config files.");

   if ( 0 != szBuff[0] )
      hw_execute_bash_command(szBuff, NULL);

   // Change power for RALINK cards too. They are 2.4Ghz only cards
   if ( access( "/etc/modprobe.d/rt2800usb.conf", R_OK ) != -1 )
   {
      iTxPower = (iTxPower/10)-2;
      if ( iTxPower < 0 ) iTxPower = 0;
      if ( iTxPower > 5 ) iTxPower = 5;
      sprintf(szBuff, "cp /etc/modprobe.d/rt2800usb.conf tmp/; sed -i 's/txpower=[0-9]*/txpower=%d/g' tmp/rt2800usb.conf; cp tmp/rt2800usb.conf /etc/modprobe.d/", iTxPower);
      hw_execute_bash_command(szBuff, NULL);
   }
   #endif
   log_line("Radio interface %d Atheros raw tx power changed to: %d", iCardIndex+1, iTxPower);
}

