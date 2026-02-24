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

#include "../base/base.h"
#include <ctype.h>
#include "strings_loc.h"
#include "strings_table.h"

#define MAX_DYNAMIC_LOC_STRINGS 2000
static char* s_szDynamicLocStringsList[MAX_DYNAMIC_LOC_STRINGS];
static int s_iCountDynamicLocStrings = 0;

#define STRINGS_HASH_SIZE 7137
static u16 s_HashTableDynamicStrings[STRINGS_HASH_SIZE];
static u16 s_HashTableLocStrings[STRINGS_HASH_SIZE];


static const char* s_szLanguages[] = { "Chinese", "English", "French", "German", "Hindi", "Russian", "Spanish" };
static const char* s_szStringTableEmptyText = "";
static const char* s_szStringTableMissingText = "missing text";
static int s_iActiveLanguage = 1;
static int s_iLocalizationInited = 0;

int getLanguagesCount()
{
   return sizeof(s_szLanguages)/sizeof(s_szLanguages[0]);
}

const char* getLanguageName(int iIndex)
{
   if ( (iIndex < 0) || (iIndex >= getLanguagesCount()) )
      return s_szStringTableEmptyText;
   return s_szLanguages[iIndex];
}

void setActiveLanguage(int iLanguage)
{
   s_iActiveLanguage = iLanguage;
}

int getActiveLanguage()
{
   return s_iActiveLanguage;
}

u16 _loc_string_compute_hash(const char* szString)
{
   if ( (NULL == szString) || (0 == szString[0]) )
      return 0xFFFF;
   int iLen = strlen(szString);
   u16 uHash = iLen + (szString[iLen/2] - ' ')*2;
   uHash += 7*(szString[0] - ' ') + 9*(szString[iLen-1] - ' ');
   if ( iLen > 1 )
      uHash += (szString[1] - ' ')*51 + (szString[iLen-2] - ' ')*27;
   if ( iLen > 2 )
      uHash += (szString[2] - ' ')*21*23 + (szString[iLen-3] - ' ')*17*(iLen+7);
   return uHash % STRINGS_HASH_SIZE;
}

const char* _check_add_dynamic_loc_string(const char* szString)
{
   if ( (NULL == szString) || (0 == szString[0]) )
      return s_szStringTableEmptyText;

   u16 uHashIndex = _loc_string_compute_hash(szString);
   int iColCount = 0;
   while ( (s_HashTableDynamicStrings[uHashIndex] != 0xFFFF) && (iColCount < STRINGS_HASH_SIZE/2) )
   {
       if ( s_HashTableDynamicStrings[uHashIndex] < s_iCountDynamicLocStrings )
       if ( (NULL != s_szDynamicLocStringsList[s_HashTableDynamicStrings[uHashIndex]]) && (0 == strcmp(s_szDynamicLocStringsList[s_HashTableDynamicStrings[uHashIndex]], szString)) )
          return s_szDynamicLocStringsList[s_HashTableDynamicStrings[uHashIndex]];
       uHashIndex++;
       uHashIndex = uHashIndex % STRINGS_HASH_SIZE;
       iColCount++;
   }

   if ( (s_iCountDynamicLocStrings >= MAX_DYNAMIC_LOC_STRINGS) || (iColCount >= STRINGS_HASH_SIZE/2) )
      return s_szStringTableMissingText;

   s_szDynamicLocStringsList[s_iCountDynamicLocStrings] = (char*)malloc(strlen(szString)+2);
   if ( NULL == s_szDynamicLocStringsList[s_iCountDynamicLocStrings] )
      return s_szStringTableMissingText;
   memset(s_szDynamicLocStringsList[s_iCountDynamicLocStrings], 0, strlen(szString)+2);
   strcpy(s_szDynamicLocStringsList[s_iCountDynamicLocStrings], szString);
   log_line("Added dynamic loc string at pos %d of %d: (%s)", s_iCountDynamicLocStrings, MAX_DYNAMIC_LOC_STRINGS, s_szDynamicLocStringsList[s_iCountDynamicLocStrings]);

   uHashIndex = _loc_string_compute_hash(szString);
   iColCount = 0;
   while ( (s_HashTableDynamicStrings[uHashIndex] != 0xFFFF) && (iColCount < STRINGS_HASH_SIZE/2) )
   {
       uHashIndex++;
       uHashIndex = uHashIndex % STRINGS_HASH_SIZE;
       iColCount++;
   }
   if ( iColCount >= STRINGS_HASH_SIZE/2 )
      return s_szStringTableMissingText;
   s_HashTableDynamicStrings[uHashIndex] = (u16)s_iCountDynamicLocStrings;
   s_iCountDynamicLocStrings++;
   return s_szDynamicLocStringsList[s_iCountDynamicLocStrings-1];
}

void initLocalizationData()
{
   for( int i=0; i<STRINGS_HASH_SIZE; i++ )
   {
      s_HashTableDynamicStrings[i] = 0xFFFF;
      s_HashTableLocStrings[i] = 0xFFFF;
   }

   type_localized_strings* pStringsTable = string_get_table();
   for( int i=0; i<string_get_table_size(); i++ )
   {
      if ( 0 == pStringsTable[i].szEnglish[0] )
         continue;
      u16 uHashIndex = _loc_string_compute_hash(pStringsTable[i].szEnglish);

      int iColCount = 0;
      while ( (s_HashTableLocStrings[uHashIndex] != 0xFFFF) && (iColCount < STRINGS_HASH_SIZE/2) )
      {
          uHashIndex++;
          uHashIndex = uHashIndex % STRINGS_HASH_SIZE;
          iColCount++;
      }
      if ( iColCount < STRINGS_HASH_SIZE/2 )
         s_HashTableLocStrings[uHashIndex] = (u16)i;
   }
   s_iLocalizationInited = 1;
   log_line("Initializing localization data for %d strings for %d languages", string_get_table_size(), getLanguagesCount());
}

const char* L(const char* szString)
{
   if ( !s_iLocalizationInited )
      initLocalizationData();

   if ( (NULL == szString) || (0 == szString[0] ) )
      return s_szStringTableEmptyText;

   if ( (0 == szString[1]) || (0 == szString[2]) )
   //   return _check_add_dynamic_loc_string(szString);
      return szString;

   type_localized_strings* pStringsTable = string_get_table();
   int iStringsTableSize = string_get_table_size();
   const char* pLocalized = NULL;
   const char* pEnglish = NULL;

   u16 uHashIndex = _loc_string_compute_hash(szString);
   int iColCount = 0;
   int iStringTableIndex = -1;

   while ( (s_HashTableLocStrings[uHashIndex] != 0xFFFF) && (iColCount < STRINGS_HASH_SIZE/2) )
   {
       if ( s_HashTableLocStrings[uHashIndex] < iStringsTableSize )
       if ( 0 == strcmp(pStringsTable[s_HashTableLocStrings[uHashIndex]].szEnglish, szString) )
       {
          iStringTableIndex = s_HashTableLocStrings[uHashIndex];
          pEnglish = pStringsTable[s_HashTableLocStrings[uHashIndex]].szEnglish;
          if (0 == s_iActiveLanguage )
             pLocalized = pStringsTable[s_HashTableLocStrings[uHashIndex]].szTranslatedCN;
          if (1 == s_iActiveLanguage )
             pLocalized = pStringsTable[s_HashTableLocStrings[uHashIndex]].szEnglish;
          if (2 == s_iActiveLanguage )
             pLocalized = pStringsTable[s_HashTableLocStrings[uHashIndex]].szTranslatedFR;
          if (3 == s_iActiveLanguage )
             pLocalized = pStringsTable[s_HashTableLocStrings[uHashIndex]].szTranslatedDE;
          if (4 == s_iActiveLanguage )
             pLocalized = pStringsTable[s_HashTableLocStrings[uHashIndex]].szTranslatedHI;
          if (5 == s_iActiveLanguage )
             pLocalized = pStringsTable[s_HashTableLocStrings[uHashIndex]].szTranslatedRU;
          if (6 == s_iActiveLanguage )
             pLocalized = pStringsTable[s_HashTableLocStrings[uHashIndex]].szTranslatedSP;
          break;
       }
       uHashIndex++;
       uHashIndex = uHashIndex % STRINGS_HASH_SIZE;
       iColCount++;
   }

   if ( (NULL == pLocalized) || (-1 == iStringTableIndex) || ( 0 != strcmp(pStringsTable[iStringTableIndex].szEnglish, szString)) ||
      (strlen(szString) != strlen(pStringsTable[iStringTableIndex].szEnglish)) )
   {
      return szString;
   }

   if ( iColCount >= STRINGS_HASH_SIZE/2 )
      return s_szStringTableMissingText;

   if ( (NULL != pLocalized) && (NULL != pEnglish) )
   {
      if ( 0 != *pLocalized )
      {
         return pLocalized;
      }
      if ( 0 != *pEnglish )
      {
         return pEnglish;
      }
      return s_szStringTableMissingText;
   }
   return _check_add_dynamic_loc_string(szString);
   //return szString;
}
