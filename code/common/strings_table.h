#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
   const char* szEnglish;
   const char* szTranslatedCN;
   const char* szTranslatedFR;
   const char* szTranslatedDE;
   const char* szTranslatedHI;
   const char* szTranslatedRU;
   const char* szTranslatedSP;
   u32 uHash;
} type_localized_strings;

type_localized_strings* string_get_table();
int string_get_table_size();

#ifdef __cplusplus
}
#endif