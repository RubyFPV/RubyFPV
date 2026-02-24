#pragma once
#include <stdio.h>
#include <stdlib.h>
#include "base.h"


#define MAX_PASS_LENGTH 64


#ifdef __cplusplus
extern "C" {
#endif

// Load and saves pass phrases
int lpp(char* szOutputBuffer, int maxLength);
int spp(char* szBuffer);

void rpp();
u8* gpp(int* pLen);
int hpp();

int epp(u8* pData, int len);
int dpp(u8* pData, int len);

#ifdef __cplusplus
}
#endif