/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga
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
#include "tx_powers.h"
#include "../utils/utils_vehicle.h"
#include <math.h>


// mW in -> mW out
static int s_iTxBoosterGainTable4W[][2] =
{
   {1, 150},
   {5, 300},
   {10, 500},
   {20, 1000},
   {30, 1500},
   {40, 1900},
   {50, 2200},
   {100, 4000}
}; // measured 06 feb 2025, ruby 10.4, 5800 mhz, 18mb legacy datarates


static int s_iRawTxRadioValues[] = {1,5,10,15,20,23,26,30,35,40,45,50,53,56,60,63,65,68,70};

static int s_iTxUIPowerLevelsMw[] =
{ 1, 5, 10, 25, 50, 75, 100,
  150, 200, 250, 300, 350, 400, 450, 500, 600, 700, 800, 900, 1000};

//--------------------------------------------------------------
// Power measurements should be done at 5700 Mhz, MCS-2 datarate,
// on this raw power levels:

static int s_iTxRawPowerLevelMeasurementsValues[] =
   { 1,  10,  20,   30,   40,   45,   50,   53,   56,   60,   63,   68,   70};
//------------------------------------------------------------------------
// Results:

static int s_iTxInfo722N[] =
   { 1,   1,   2,    3,   10,   25,   35,   60,   80,   90,    0,    0,    0};
static int s_iTxInfoBlueStick[]  =
   { 1,   2,   4,    8,   28,   50,   80,  110,  280, 1000,   0,     0,    0};
static int s_iTxInfoAWUS036NH[] =
   { 1,  10,  20,   30,   40,   50,   60,    0,    0,    0,    0,    0,    0};
static int s_iTxInfoAWUS036NHA[] =
   { 1,   1,   2,    6,   17,   70,  120,  180,  215,  310,  460,    0,    0};

// { 1,  10,  20,   30,   40,   45,   50,   53,   56,   60,   63,   68,   70};
//------------------------------------------------------------------------
static int s_iTxInfo58Generic[] =
   { 1,   5,  17,   50,  150,  170,  190,  210,  261,  310,    0,    0,    0};
static int s_iTxInfoAWUS036ACH[] =
   { 1,   2,   5,   20,   50,   90,  160,  250,  300,  420,  500,    0,    0};
static int s_iTxInfoASUSUSB56[]  =
   { 1,   4,  13,   42,   116, 190,  280,  360,  420,  490,  540,    0,    0}; // measured 08.dec.2024, ruby 10.1
static int s_iTxInfoRTLDualAnt[] =
   { 1,   1,   2,    7,   25,   40,   78,   95,  125,  175,  220,    0,    0}; // measured 14.feb.2025, ruby 10.4, 18mb datarate, 5700mhz
static int s_iTxInfoAli1W[]  =
   { 1,   1,   2,    5,   10,   20,   30,   50,  100,  300,  450,    0,    0};
static int s_iTxInfoA6100[] =
   { 1,   1,   3,   10,   17,   19,   22,   23,   25,    0,    0,    0,    0}; // measured 08.dec.2024, ruby 10.1
static int s_iTxInfoAWUS036ACS[] =
   { 1,   1,   2,    3,   10,   25,   35,   50,   60,   90,  110,    0,    0};
static int s_iTxInfoArcherT2UP[] =
   { 1,   2,   7,   25,   65,  100,  135,  150,  170,  190,    0,    0,    0}; // measured 05.feb.2025, ruby 10.3, 5800mhz, 18mb legacy rates
static int s_iTxInfoArcherRTL8812AU_AF1[] =
   { 1,   2,   5,   15,   40,   70,   95,  110,  130,  150,    0,    0,    0}; // measured 16.jan.2025

// { 1,  10,  20,   30,   40,   45,   50,   53,   56,   60,   63,   68,   70};
//------------------------------------------------------------------------
static int s_iTxInfoRTL8812EU[] =
   { 6,   7,  15,   45,  110,  160,  230,  270,  320,  380,  430,  500,  550}; // measured 08.dec.2024, ruby 10.1

static int s_iTxInfoRTL8812EURunCamV2[] =
   {9,    9,  12,   15,   40,   65,  100,  120,  150,  200,  230,  300,  320}; // measured 21.mar.2025, ruby 10.8, 5700mhz, 18mb legacy rates, power/freq factor: 5805/5700: 72/130
//   {8,    8,  10,   15,   45,   50,   70,   90,  120,  150,  180,  250,  270}; // measured 21.mar.2025, ruby 10.8, 5805mhz, 18mb legacy rates, power/freq factor: 5805/5700: 72/130

static int s_iTxInfoRTL8733BU[] =
   { 1,   1,   1,    4,    8,    12,  20,   22,   30,   60,    0,    0,    0}; // measured 20.jan.2025, ruby 10.3
   
static int s_iTxInfoRTL8812AUBonnetLow[] =
   { 1,   1,   2,    8,   20,   25,   35,   40,   45,    0,    0,    0,    0}; // measured 20.jan.2025, Ruby 11.2, Legacy 18Mb/MCS2 on 5700 Mhz
static int s_iTxInfoRTL8812AUBonnetHigh[] =
   { 1,   1,   3,    8,   20,   35,   55,   70,   90,  120,   150,   0,    0}; // measured 28.aug.2025, Ruby 11.2, Legacy 18Mb/MCS2 on 5700 Mhz

// { 1,  10,  20,   30,   40,   45,   50,   53,   56,   60,   63,   68,   70};
//------------------------------------------------------------------------
static int s_iTxInfoOIPCUSight[] = 
 { 550, 600, 650,  670,  0,  0,  0,  0,  0,  0,  0,  0,  0}; // measured 23.dec.2024, ruby 10.3



bool _tx_powers_is_card_serial_radio(u32 uBoardType, int iCardModel)
{
   if ( iCardModel < 0 )
      iCardModel = -iCardModel;
   if ( (iCardModel == CARD_MODEL_SIK_RADIO) || (iCardModel == CARD_MODEL_SERIAL_RADIO) || (iCardModel == CARD_MODEL_SERIAL_RADIO_ELRS) )
      return true;
   return false;
}

const int* _tx_powers_get_mw_table_for_card(u32 uBoardType, int iCardModel)
{
   if ( iCardModel < 0 )
      iCardModel = -iCardModel;
   int* piMwPowers = s_iTxInfo58Generic;
   if ( (uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_SIGMASTAR_338Q )
   if ( ((uBoardType & BOARD_SUBTYPE_MASK) >> BOARD_SUBTYPE_SHIFT) == BOARD_SUBTYPE_OPENIPC_AIO_EMAX_MINI )
   {
      piMwPowers = s_iTxInfoArcherRTL8812AU_AF1;
      return piMwPowers;
   }

   if ( (uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_SIGMASTAR_338Q )
   if ( ((uBoardType & BOARD_SUBTYPE_MASK) >> BOARD_SUBTYPE_SHIFT) == BOARD_SUBTYPE_OPENIPC_AIO_RUNCAM_V2 )
   if ( iCardModel == CARD_MODEL_BLUE_8812EU )
   {
      piMwPowers = s_iTxInfoRTL8812EURunCamV2;
      return piMwPowers;
   }
   if ( (uBoardType & BOARD_TYPE_MASK) == BOARD_TYPE_RADXA_RUNCAM_VRX )
   if ( iCardModel == CARD_MODEL_BLUE_8812EU )
   {
      piMwPowers = s_iTxInfoRTL8812EURunCamV2;
      return piMwPowers;
   }

   switch (iCardModel)
   {
      case CARD_MODEL_TPLINK722N: piMwPowers = s_iTxInfo722N; break;
      case CARD_MODEL_ATHEROS_GENERIC: piMwPowers = s_iTxInfo722N; break;
      case CARD_MODEL_BLUE_STICK: piMwPowers = s_iTxInfoBlueStick; break;
      case CARD_MODEL_ALFA_AWUS036NH: piMwPowers = s_iTxInfoAWUS036NH; break;
      case CARD_MODEL_ALFA_AWUS036NHA: piMwPowers = s_iTxInfoAWUS036NHA; break;

      case CARD_MODEL_RTL8812AU_GENERIC: piMwPowers = s_iTxInfoRTLDualAnt; break;
      case CARD_MODEL_RTL8812AU_DUAL_ANTENNA: piMwPowers = s_iTxInfoRTLDualAnt; break;
      case CARD_MODEL_ASUS_AC56: piMwPowers = s_iTxInfoASUSUSB56; break;
      case CARD_MODEL_ALFA_AWUS036ACH: piMwPowers = s_iTxInfoAWUS036ACH; break;
      case CARD_MODEL_ALFA_AWUS036ACS: piMwPowers = s_iTxInfoAWUS036ACS; break;
      case CARD_MODEL_ZIPRAY: piMwPowers = s_iTxInfoAli1W; break;
      case CARD_MODEL_NETGEAR_A6100: piMwPowers = s_iTxInfoA6100; break;
      case CARD_MODEL_TENDA_U12: piMwPowers = s_iTxInfo58Generic; break;
      case CARD_MODEL_ARCHER_T2UPLUS: piMwPowers = s_iTxInfoArcherT2UP; break;
      case CARD_MODEL_RTL8814AU: piMwPowers = s_iTxInfo58Generic; break;
      case CARD_MODEL_RTL8812AU_OIPC_USIGHT: piMwPowers = s_iTxInfoOIPCUSight; break;
      case CARD_MODEL_RTL8812AU_AF1: piMwPowers = s_iTxInfoArcherRTL8812AU_AF1; break;
      case CARD_MODEL_RTL8733BU: piMwPowers = s_iTxInfoRTL8733BU; break;
      case CARD_MODEL_BLUE_8812EU: piMwPowers = s_iTxInfoRTL8812EU; break;
      case CARD_MODEL_BONNET_LOW_POWER: piMwPowers = s_iTxInfoRTL8812AUBonnetLow; break;
      case CARD_MODEL_BONNET_HIGH_POWER: piMwPowers = s_iTxInfoRTL8812AUBonnetHigh; break;
   }
   return piMwPowers;
}

const int* tx_powers_get_raw_radio_power_values(int* piOutputCount)
{
   if ( NULL != piOutputCount )
      *piOutputCount = sizeof(s_iRawTxRadioValues)/sizeof(s_iRawTxRadioValues[0]);
   return s_iRawTxRadioValues;
}

const int* tx_powers_get_ui_levels_mw(int* piOutputCount)
{
   if ( NULL != piOutputCount )
      *piOutputCount = sizeof(s_iTxUIPowerLevelsMw)/sizeof(s_iTxUIPowerLevelsMw[0]);
   return s_iTxUIPowerLevelsMw;
}

const int* tx_powers_get_raw_measurement_intervals(int* piOutputCount)
{
   if ( NULL != piOutputCount )
      *piOutputCount = sizeof(s_iTxRawPowerLevelMeasurementsValues)/sizeof(s_iTxRawPowerLevelMeasurementsValues[0]);
   return s_iTxRawPowerLevelMeasurementsValues;
}

int tx_powers_get_mw_boosted_value_from_mw(int imWValue, bool bBoost2W, bool bBoost4W)
{
   if ( (!bBoost2W) && (!bBoost4W) )
      return imWValue;
   int iCountValues = sizeof(s_iTxBoosterGainTable4W)/sizeof(s_iTxBoosterGainTable4W[0][0])/2;
   for( int i=iCountValues-1; i>=0; i-- )
   {
      if ( imWValue >= s_iTxBoosterGainTable4W[i][0] )
      {
         if ( i == (iCountValues-1) )
            return s_iTxBoosterGainTable4W[i][1];

         int iBoostDelta = (s_iTxBoosterGainTable4W[i+1][1] - s_iTxBoosterGainTable4W[i][1]);

         int iRet = s_iTxBoosterGainTable4W[i][1] + (int)( (float)iBoostDelta * (float)(imWValue - s_iTxBoosterGainTable4W[i][0])/(float)(s_iTxBoosterGainTable4W[i+1][0] - s_iTxBoosterGainTable4W[i][0]) );
         if ( bBoost2W )
            iRet /= 2;
         return iRet;
      }
   }
   return imWValue;
}

int tx_powers_get_max_usable_power_mw_for_card(u32 uBoardType, int iCardModel)
{
   if ( iCardModel < 0 )
      iCardModel = -iCardModel;
   if ( _tx_powers_is_card_serial_radio(uBoardType, iCardModel) )
      return 100;
   const int* piMwPowers = _tx_powers_get_mw_table_for_card(uBoardType, iCardModel);
   int iCount = sizeof(s_iTxRawPowerLevelMeasurementsValues)/sizeof(s_iTxRawPowerLevelMeasurementsValues[0]);
   for( int i=0; i<iCount; i++ )
   {
      if ( piMwPowers[i] <= 0 )
        return piMwPowers[i-1];
   }
   return piMwPowers[iCount-1];
}

int tx_powers_get_max_usable_power_raw_for_card(u32 uBoardType, int iCardModel)
{
   if ( iCardModel < 0 )
      iCardModel = -iCardModel;
   if ( _tx_powers_is_card_serial_radio(uBoardType, iCardModel) )
      return 20;
   const int* piMwPowers = _tx_powers_get_mw_table_for_card(uBoardType, iCardModel);
   int iCount = sizeof(s_iTxRawPowerLevelMeasurementsValues)/sizeof(s_iTxRawPowerLevelMeasurementsValues[0]);
   for( int i=0; i<iCount; i++ )
   {
      if ( piMwPowers[i] <= 0 )
        return s_iTxRawPowerLevelMeasurementsValues[i-1];
   }
   return s_iTxRawPowerLevelMeasurementsValues[iCount-1];
}

int tx_powers_convert_raw_to_mw(u32 uBoardType, int iCardModel, int iRawPower)
{
   if ( iCardModel < 0 )
      iCardModel = -iCardModel;
   if ( iRawPower < 0 )
      iRawPower = -iRawPower;
   if ( _tx_powers_is_card_serial_radio(uBoardType, iCardModel) )
      return pow(10.0, ((float)iRawPower)/10.0);
   const int* piMwPowers = _tx_powers_get_mw_table_for_card(uBoardType, iCardModel);
   int iCount = sizeof(s_iTxRawPowerLevelMeasurementsValues)/sizeof(s_iTxRawPowerLevelMeasurementsValues[0]);
   
   if ( iRawPower <= s_iTxRawPowerLevelMeasurementsValues[0] )
      return (piMwPowers[0]*iRawPower)/s_iTxRawPowerLevelMeasurementsValues[0];

   for( int i=0; i<iCount-1; i++ )
   {
      if ( piMwPowers[i+1] <= 0 )
         return piMwPowers[i];
      if ( iRawPower >= s_iTxRawPowerLevelMeasurementsValues[i] )
      if ( iRawPower <= s_iTxRawPowerLevelMeasurementsValues[i+1] )
      {
         return piMwPowers[i] + ((piMwPowers[i+1] - piMwPowers[i]) * (iRawPower - s_iTxRawPowerLevelMeasurementsValues[i])) / (s_iTxRawPowerLevelMeasurementsValues[i+1] - s_iTxRawPowerLevelMeasurementsValues[i]);
      }
   }

   return piMwPowers[0];
}

int tx_powers_convert_mw_to_raw(u32 uBoardType, int iCardModel, int imWPower)
{
   if ( iCardModel < 0 )
      iCardModel = -iCardModel;
   if ( _tx_powers_is_card_serial_radio(uBoardType, iCardModel) )
      return 10*log10((float)imWPower);
   const int* piMwPowers = _tx_powers_get_mw_table_for_card(uBoardType, iCardModel);
   int iCount = sizeof(s_iTxRawPowerLevelMeasurementsValues)/sizeof(s_iTxRawPowerLevelMeasurementsValues[0]);
   
   if ( imWPower <= piMwPowers[0] )
   {
      int iRaw = (s_iTxRawPowerLevelMeasurementsValues[0]*imWPower)/piMwPowers[0];
      if ( iRaw < 1 )
         iRaw = 1;
      return iRaw;
   }
   for( int i=0; i<iCount-1; i++ )
   {
      if ( piMwPowers[i+1] <= 0 )
         return s_iTxRawPowerLevelMeasurementsValues[i];
      if ( imWPower >= piMwPowers[i] )
      if ( imWPower <= piMwPowers[i+1] )
      {
         return s_iTxRawPowerLevelMeasurementsValues[i] + ((s_iTxRawPowerLevelMeasurementsValues[i+1] - s_iTxRawPowerLevelMeasurementsValues[i]) * (imWPower - piMwPowers[i])) / (piMwPowers[i+1] - piMwPowers[i]);
      }
   }

   return s_iTxRawPowerLevelMeasurementsValues[0];
}

void tx_power_get_current_mw_powers_for_model(Model* pModel, int* piOutputArray)
{
   if ( (NULL == pModel) || (NULL == piOutputArray) )
      return;

   for( int i=0; i<pModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( ! hardware_radio_type_is_ieee(pModel->radioInterfacesParams.interface_radiotype_and_driver[i] & 0xFF) )
         continue;
      int iCardModel = pModel->radioInterfacesParams.interface_card_model[i];
      if ( iCardModel < 0 )
         iCardModel = -iCardModel;
      piOutputArray[i] = tx_powers_convert_raw_to_mw(pModel->hwCapabilities.uBoardType, iCardModel, pModel->radioInterfacesParams.interface_raw_power[i]);
   }
}

int tx_power_compute_uplink_power_for_model_link(Model* pModel, int iVehicleRadioLink, int iLocalRadioInterfaceIndex, int iCardModel)
{
   if ( (NULL == pModel) || (! hardware_radio_index_is_wifi_radio(iLocalRadioInterfaceIndex)) )
      return 0;

   if ( iCardModel < 0 )
      iCardModel = -iCardModel;

   u32 uBoardType = hardware_getBoardType();      
   int iVehicleLinkMwPower = get_vehicle_radio_link_current_tx_power_mw(pModel, iVehicleRadioLink);
   iVehicleLinkMwPower *= 4;

   int iTxPowerMw = iVehicleLinkMwPower;
   int iCardMaxPowerMw = tx_powers_get_max_usable_power_mw_for_card(uBoardType, iCardModel);
   if ( iTxPowerMw > iCardMaxPowerMw )
      iTxPowerMw = iCardMaxPowerMw;
   return iTxPowerMw;
}


int get_vehicle_radio_link_current_tx_power_mw(Model* pModel, int iRadioLinkIndex)
{
   if ( (NULL == pModel) || (iRadioLinkIndex < 0) || (iRadioLinkIndex >= pModel->radioLinksParams.links_count) )
      return 1;

   int iMaxCardPowerMw = 1;
   for( int i=0; i<pModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( ! hardware_radio_type_is_ieee(pModel->radioInterfacesParams.interface_radiotype_and_driver[i] & 0xFF) )
         continue;
      if ( pModel->radioInterfacesParams.interface_link_id[i] != iRadioLinkIndex )
         continue;

      int iCardModel = pModel->radioInterfacesParams.interface_card_model[i];
      if ( iCardModel < 0 )
         iCardModel = -iCardModel;
      int iCardRawPower = pModel->radioInterfacesParams.interface_raw_power[i];
      int iCardPowerMw = tx_powers_convert_raw_to_mw(pModel->hwCapabilities.uBoardType, iCardModel, iCardRawPower);
      if ( pModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_2W )
         iCardPowerMw = tx_powers_get_mw_boosted_value_from_mw(iCardPowerMw, true, false);
      if ( pModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_4W )
         iCardPowerMw = tx_powers_get_mw_boosted_value_from_mw(iCardPowerMw, false, true);
      if ( iCardPowerMw > iMaxCardPowerMw )
         iMaxCardPowerMw = iCardPowerMw;
   }

   return iMaxCardPowerMw;
}

