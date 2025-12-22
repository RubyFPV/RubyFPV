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
#include "hardware_procs.h"
#include "../radio/radioflags.h"
#include "../radio/radiopackets2.h"
#include <ctype.h>


u32 channels433[] = {425000, 426000, 427000, 428000, 429000, 430000, 431000, 432000, 433000, 434000, 435000, 435000, 437000, 438000, 439000, 440000, 441000, 442000, 443000, 444000, 445000};
u32 channels868[] = {858000, 859000, 860000, 861000, 862000, 863000, 864000, 865000, 866000, 867000, 868000, 869000, 870000, 871000, 872000, 873000, 874000, 875000};
u32 channels915[] = {910000,911000,912000,913000,914000,915000,916000,917000,918000,919000,920000};

u32 channels23[] = { 2312000, 2317000, 2322000, 2327000, 2332000, 2337000, 2342000, 2347000, 2352000, 2357000, 2362000, 2367000, 2372000, 2377000, 2382000, 2387000, 2392000, 2397000, 2402000, 2407000 };
u32 channels24[] = { 2412000, 2417000, 2422000, 2427000, 2432000, 2437000, 2442000, 2447000, 2452000, 2457000, 2462000, 2467000, 2472000, 2484000 };
u32 channels25[] = { 2487000, 2489000, 2492000, 2494000, 2497000, 2499000, 2512000, 2532000, 2572000, 2592000, 2612000, 2632000, 2652000, 2672000, 2692000, 2712000 };
u32 channels58[] = { 5180000, 5200000, 5220000, 5240000, 5260000, 5280000, 5300000, 5320000, 5500000, 5520000, 5540000, 5560000, 5580000, 5600000, 5620000, 5640000, 5660000, 5680000, 5700000, 5745000, 5765000, 5785000, 5805000, 5825000, 5845000, 5865000, 5885000 };

// in 1 Mb increments, in bps
int s_LegacyIEEERates[] = {6000000, 9000000, 12000000, 18000000, 24000000, 36000000, 48000000, 54000000};
int s_SiKAirDataRates[] = {2000, 4000, 8000, 16000, 19000, 24000, 32000, 48000, 64000, 96000, 128000, 192000, 250000};

int s_ArrayTestRadioRatesLegacy[] = {6000000, 12000000, 18000000, 24000000, 36000000, 48000000, 54000000};
int s_ArrayTestRadioRatesMCS[] = {-1, -2, -3, -4, -5, -6, -7, -8, -9};

u32* getChannels433() { return channels433; }
int getChannels433Count() { return sizeof(channels433)/sizeof(channels433[0]); }
u32* getChannels868() { return channels868; }
int getChannels868Count() { return sizeof(channels868)/sizeof(channels868[0]); }
u32* getChannels915() { return channels915; }
int getChannels915Count() { return sizeof(channels915)/sizeof(channels915[0]); }

u32* getChannels24() { return channels24; }
int getChannels24Count() { return sizeof(channels24)/sizeof(channels24[0]); }
u32* getChannels23() { return channels23; }
int getChannels23Count() { return sizeof(channels23)/sizeof(channels23[0]); }
u32* getChannels25() { return channels25; }
int getChannels25Count() { return sizeof(channels25)/sizeof(channels25[0]); }
u32* getChannels58() { return channels58; }
int getChannels58Count() { return sizeof(channels58)/sizeof(channels58[0]); }

int _getChannelsAndCount(u32 nBand, u32** ppuChannels)
{
   if (ppuChannels == NULL)
      return -1;

   switch ( nBand )
   {
   case RADIO_HW_SUPPORTED_BAND_433:
      *ppuChannels = getChannels433();
      return getChannels433Count();

   case RADIO_HW_SUPPORTED_BAND_868:
      *ppuChannels = getChannels868();
      return getChannels868Count();

   case RADIO_HW_SUPPORTED_BAND_915:
      *ppuChannels = getChannels915();
      return getChannels915Count();

   case RADIO_HW_SUPPORTED_BAND_23:
      *ppuChannels = getChannels23();
      return getChannels23Count();

   case RADIO_HW_SUPPORTED_BAND_24:
      *ppuChannels = getChannels24();
      return getChannels24Count();

   case RADIO_HW_SUPPORTED_BAND_25:
      *ppuChannels = getChannels25();
      return getChannels25Count();

   case RADIO_HW_SUPPORTED_BAND_58:
      *ppuChannels = getChannels58();
      return getChannels58Count();

   default:
      break;
  }

  *ppuChannels = NULL;
  return -1;
}


int* getSiKAirDataRates()
{
   return s_SiKAirDataRates;
}

int getSiKAirDataRatesCount()
{
   return sizeof(s_SiKAirDataRates)/sizeof(s_SiKAirDataRates[0]);
}


int getBand(u32 freqKhz)
{
   if ( freqKhz < 10000 ) // Old format
   {
      if ( freqKhz < 2700 )
         return RADIO_HW_SUPPORTED_BAND_24;
      else
         return RADIO_HW_SUPPORTED_BAND_58;
   }
   if ( freqKhz < 500000 )
      return RADIO_HW_SUPPORTED_BAND_433;
   if ( freqKhz < 900000 )
      return RADIO_HW_SUPPORTED_BAND_868;
   if ( freqKhz < 950000 )
      return RADIO_HW_SUPPORTED_BAND_915;
   if ( freqKhz < 2412000 )
      return RADIO_HW_SUPPORTED_BAND_23;
   if ( freqKhz < 2487000 )
      return RADIO_HW_SUPPORTED_BAND_24;
   if ( freqKhz < 5000000 )
      return RADIO_HW_SUPPORTED_BAND_25;
   if ( freqKhz > 5000000 )
      return RADIO_HW_SUPPORTED_BAND_58;

   return 0;
}

int getChannelIndexForFrequency(u32 nBand, u32 freqKhz)
{
   u32* puChannels = NULL;
   int iChannelcount = _getChannelsAndCount(nBand, &puChannels);
   if( puChannels != NULL )
   {
      for( int i=0; i<iChannelcount; i++ )
      {
         if ( freqKhz == puChannels[i] )
            return i;
      }
   }
   return -1;
}

int isFrequencyInBands(u32 freqKhz, u8 bands)
{
   if ( freqKhz < 500000 )
   {
      if ( bands & RADIO_HW_SUPPORTED_BAND_433 )
         return 1;
      return 0;
   }
   if ( freqKhz < 890000 )
   {
      if ( bands & RADIO_HW_SUPPORTED_BAND_868 )
         return 1;
      return 0;
   }
   if ( freqKhz < 950000 )
   {
      if ( bands & RADIO_HW_SUPPORTED_BAND_915 )
         return 1;
      return 0;
   }
   if ( freqKhz < 2412000 )
   {
      if ( bands & RADIO_HW_SUPPORTED_BAND_23 )
         return 1;
      return 0;
   }
   if ( freqKhz < 2487000 )
   {
      if ( bands & RADIO_HW_SUPPORTED_BAND_24 )
         return 1;
      return 0;
   }
   if ( freqKhz < 5000000 )
   {
      if ( bands & RADIO_HW_SUPPORTED_BAND_25 )
         return 1;
      return 0;
   }
   if ( freqKhz > 5000000 )
   {
      if ( bands & RADIO_HW_SUPPORTED_BAND_58 )
         return 1;
      return 0;
   }
   return 0;
}

int getSupportedChannels(u32 supportedBands, int includeSeparator, u32* pOutChannels, int maxChannels)
{
   if ( NULL == pOutChannels || 0 == maxChannels )
      return 0;

   int radio_hw_supported_bands[] =
   {
      RADIO_HW_SUPPORTED_BAND_433,
      RADIO_HW_SUPPORTED_BAND_868,
      RADIO_HW_SUPPORTED_BAND_915,
      RADIO_HW_SUPPORTED_BAND_23,
      RADIO_HW_SUPPORTED_BAND_24,
      RADIO_HW_SUPPORTED_BAND_25,
      RADIO_HW_SUPPORTED_BAND_58
   };

   int iCountSupported = 0;
   for( int r=0; r<(int)(sizeof(radio_hw_supported_bands)/sizeof(radio_hw_supported_bands[0])); r++ )
   {
      u32* puChannels = NULL;
      int iChannelsCount = _getChannelsAndCount(supportedBands & radio_hw_supported_bands[r], &puChannels);
      if( puChannels != NULL )
      {
         for( int i=0; i<iChannelsCount; i++ )
         {
            *pOutChannels = puChannels[i];
            pOutChannels++;
            iCountSupported++;
            if ( iCountSupported >= maxChannels )
               return iCountSupported;
         }
         if ( includeSeparator )
         {
            *pOutChannels = 0;
            pOutChannels++;
            iCountSupported++;
            if ( iCountSupported >= maxChannels )
               return iCountSupported;
         }
      }
   }

   return iCountSupported;
}

int *getLegacyDataRatesBPS() { return s_LegacyIEEERates; }
int getLegacyDataRatesCount() { return sizeof(s_LegacyIEEERates)/sizeof(s_LegacyIEEERates[0]); }

int getTestDataRatesCountLegacy() { return sizeof(s_ArrayTestRadioRatesLegacy)/sizeof(s_ArrayTestRadioRatesLegacy[0]); }
int getTestDataRatesCountMCS() { return sizeof(s_ArrayTestRadioRatesMCS)/sizeof(s_ArrayTestRadioRatesMCS[0]); }
int* getTestDataRatesLegacy() { return s_ArrayTestRadioRatesLegacy; }
int* getTestDataRatesMCS() { return s_ArrayTestRadioRatesMCS; }

// Positive level shift -> increase datarate
// Negative level shift -> decrease datarate
int getDataRateShiftedByLevels(int iDatarateBSP, int iLevelsToShift)
{
   if ( 0 == iLevelsToShift )
      return iDatarateBSP;

   // MCS datarates
   if ( iDatarateBSP < 0 )
   {
      iDatarateBSP -= iLevelsToShift;
      if ( iDatarateBSP < -MAX_MCS_INDEX-1 )
         iDatarateBSP = -MAX_MCS_INDEX-1;
      if ( iDatarateBSP > -1 )
         iDatarateBSP = -1;
      return iDatarateBSP;
   }

   // Legacy datarates
   // Find the datarate index (lowest one) and then do the shifting
   int iCountRates = getLegacyDataRatesCount();
   int* pRates = getLegacyDataRatesBPS();
   for( int iRateIndex=0; iRateIndex<iCountRates; iRateIndex++ )
   {
      if ( pRates[iRateIndex] >= iDatarateBSP )
      {
         iRateIndex += iLevelsToShift;
         if ( iRateIndex < 0 )
            iRateIndex = 0;
         if ( iRateIndex >= iCountRates )
            iRateIndex = iCountRates - 1;
         iDatarateBSP = pRates[iRateIndex];
         break;
      }
   }

   return iDatarateBSP;
}

u32 getRealDataRateFromMCSRate(int mcsIndex, int iHT40)
{
   u32 uMul = 1;
   if ( iHT40 )
      uMul = 2;
   if ( 0 == mcsIndex )
      return 6000000*uMul;
   if ( 1 == mcsIndex )
      return 13000000*uMul;
   if ( 2 == mcsIndex )
      return 19000000*uMul;
   if ( 3 == mcsIndex )
      return 26000000*uMul;
   if ( 4 == mcsIndex )
      return 39000000*uMul;
   if ( 5 == mcsIndex )
      return 52000000*uMul;
   if ( 6 == mcsIndex )
      return 58000000*uMul;
   if ( 7 == mcsIndex )
      return 65000000*uMul;
   if ( 8 == mcsIndex )
      return 78000000*uMul;
   if ( 9 == mcsIndex )
      return 78000000*uMul;
   return 58000000*uMul;
}

u32 getRealDataRateFromRadioDataRate(int dataRateBPS, u32 uRadioFlags, int iIsDownlink)
{
   u32 iMultiply = 1;
   if ( uRadioFlags & RADIO_FLAG_HT40_VEHICLE )
   if ( iIsDownlink )
      iMultiply = 2;
   if ( uRadioFlags & RADIO_FLAG_HT40_CONTROLLER )
   if ( ! iIsDownlink )
      iMultiply = 2;
   
   if ( -100 == dataRateBPS )
   {
      if ( (uRadioFlags & RADIO_FLAGS_USE_MCS_DATARATES) )
         return iMultiply * getRealDataRateFromMCSRate(0, 0);
      return iMultiply * DEFAULT_RADIO_DATARATE_LOWEST;
   }
   else if ( 0 == dataRateBPS )
   {
      return iMultiply * DEFAULT_RADIO_DATARATE_LOWEST;
   }
   else if ( dataRateBPS < 0 )
      return iMultiply * getRealDataRateFromMCSRate(-dataRateBPS-1, 0);
   else if ( dataRateBPS <= 56 )
      return iMultiply * ((u32)(dataRateBPS))*1000*1000;
   else
      return iMultiply * (u32)dataRateBPS;
}

static int s_iTableRadioMCS_SNR[] = {2,5,9,11,15,18,20,25,29,31};
static int s_iTableRadioMCS_DBM[] = {-82,-79,-77,-74,-70,-66,-65,-64,-59,-57};

int getRadioMinimSNRForDataRate(int iDatarate)
{
   if ( (iDatarate < 0) && (iDatarate >= (-MAX_MCS_INDEX-1)) )
      return s_iTableRadioMCS_SNR[-iDatarate-1];
   if ( iDatarate > 0 )
   {
      for( int i=0; i<MAX_MCS_INDEX; i++ )
      {
         if ( getRealDataRateFromMCSRate(i, 0) >= (u32)iDatarate )
            return s_iTableRadioMCS_SNR[i];
      }
   }
   return 0;
}

int getRadioMinimDBMForDataRate(int iDatarate)
{
   if ( (iDatarate < 0) && (iDatarate >= (-MAX_MCS_INDEX-1)) )
      return s_iTableRadioMCS_DBM[-iDatarate-1];
   if ( iDatarate > 0 )
   {
      for( int i=0; i<MAX_MCS_INDEX; i++ )
      {
         if ( getRealDataRateFromMCSRate(i, 0) >= (u32)iDatarate )
            return s_iTableRadioMCS_DBM[i];
      }
   }
   return 0;
}
