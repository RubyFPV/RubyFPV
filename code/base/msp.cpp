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
#include "msp.h"

void parse_msp_reset_state(type_msp_parse_state* pMSPState)
{
   if ( NULL == pMSPState )
      return;

   memset(pMSPState, 0, sizeof(type_msp_parse_state));
   memset( &(pMSPState->headerTelemetryMSP), 0, sizeof(t_packet_header_telemetry_msp));

   pMSPState->headerTelemetryMSP.uMSPOSDCols = 60;
   pMSPState->headerTelemetryMSP.uMSPOSDRows = 22;
   pMSPState->headerTelemetryMSP.uMSPFlags = 0;

   pMSPState->iMSPState = MSP_STATE_WAIT_HEADER1;
   pMSPState->bEmptyBuffer = true;
   pMSPState->uMSPPreviousCommand = 0xFF;
   pMSPState->uMSPDisplayPortCommand = 0xFF;
   pMSPState->uMSPDisplayPortPreviousCommand = 0xFF;
   pMSPState->iCountDrawnStrings = 0;
   pMSPState->iLastDrawFrameNumber = 0;
   pMSPState->iMaxDrawX = pMSPState->iMaxDrawY = 0;
}


void _parse_msp_osd_add_string(type_msp_parse_state* pMSPState, u8* pData, int iDataLength)
{
   if ( (NULL == pMSPState) || (NULL == pData) || (iDataLength < 4) )
      return;

   if ( (pMSPState->headerTelemetryMSP.uMSPOSDRows == 0) ||
        (pMSPState->headerTelemetryMSP.uMSPOSDCols == 0) ||
        ((pMSPState->headerTelemetryMSP.uMSPFlags & MSP_FLAGS_FC_TYPE_MASK) == 0) )
      return;

   pMSPState->bEmptyBuffer = false;
   pMSPState->iCountDrawnStrings++;

   int y = pData[0];
   int x = pData[1];
   u8 uAttr = pData[2];
   if ( (x<0) || (x >= 64) || (y<0) || (y>=24) )
      return;

   if ( y > pMSPState->iMaxDrawY )
   {
      pMSPState->iMaxDrawY = y;
      log_line("MSPOSD: Max Y updated to %d", pMSPState->iMaxDrawY);
   }
   char szBuff[255];
   memset(szBuff, 0, 255);
   for( int i=3; i<iDataLength; i++ )
   {
      if ( pData[i] == 0 )
         break;

      if ( x > pMSPState->iMaxDrawX )
      {
         pMSPState->iMaxDrawX = x;
         log_line("MSPOSD: Max X updated to %d", pMSPState->iMaxDrawX);
      }
      pMSPState->uScreenCharsTmp[x + y*pMSPState->headerTelemetryMSP.uMSPOSDCols] = pData[i];
      if ( uAttr & 0x03 )
         pMSPState->uScreenCharsTmp[x + y*pMSPState->headerTelemetryMSP.uMSPOSDCols] |= (((u16)(uAttr & 0x03)) << 8);
      szBuff[i-3] = pData[i];
      x++;
   }
}

void _parse_msp_osd_command(type_msp_parse_state* pMSPState, bool bAllowScreenUpdate)
{
   if ( NULL == pMSPState )
      return;
   if ( (pMSPState->uMSPCommand != MSP_CMD_DISPLAYPORT) || (pMSPState->iMSPCommandPayloadSize < 1) || (pMSPState->iMSPDirection != MSP_DIR_FROM_FC) )
      return;

   pMSPState->uMSPDisplayPortPreviousCommand = pMSPState->uMSPDisplayPortCommand;
   pMSPState->uMSPDisplayPortCommand = pMSPState->uMSPCommandPayload[0];

   switch ( pMSPState->uMSPDisplayPortCommand )
   {
      case MSP_DISPLAYPORT_CLEAR:
         //memset(&(pMSPState->uScreenChars[0]), 0, MAX_MSP_CHARS_BUFFER*sizeof(u16));
         memset(&(pMSPState->uScreenCharsTmp[0]), 0, MAX_MSP_CHARS_BUFFER*sizeof(u16));
         pMSPState->bEmptyBuffer = true;
         pMSPState->iCountDrawnStrings = 0;
         break;

      case MSP_DISPLAYPORT_KEEPALIVE:
         if ( pMSPState->iCountDrawnStrings > 0 )
            pMSPState->iLastDrawFrameNumber++;
         pMSPState->iCountDrawnStrings = 0;
         //if ( ! pMSPState->bEmptyBuffer )
         if ( bAllowScreenUpdate )
            memcpy(&(pMSPState->uScreenChars[0]), &(pMSPState->uScreenCharsTmp[0]), MAX_MSP_CHARS_BUFFER*sizeof(u16));
         //pMSPState->bEmptyBuffer = true;
         pMSPState->iLastDrawFrameNumber++;
         break;

      case MSP_DISPLAYPORT_DRAW_SCREEN:
         pMSPState->iCountDrawnStrings = 0;
         //if ( ! pMSPState->bEmptyBuffer )
         if ( bAllowScreenUpdate )
            memcpy(&(pMSPState->uScreenChars[0]), &(pMSPState->uScreenCharsTmp[0]), MAX_MSP_CHARS_BUFFER*sizeof(u16));
         pMSPState->bEmptyBuffer = true;
         pMSPState->iLastDrawFrameNumber++;
         //memset(&(pMSPState->uScreenCharsTmp[0]), 0, MAX_MSP_CHARS_BUFFER*sizeof(u16));
         break;

      case MSP_DISPLAYPORT_DRAW_STRING:
         _parse_msp_osd_add_string(pMSPState, &pMSPState->uMSPCommandPayload[1], pMSPState->iMSPCommandPayloadSize-1);
         break;

      default: break;
   }
}

void parse_msp_incoming_data(type_msp_parse_state* pMSPState, u8* pData, int iDataLength, bool bAllowScreenUpdate)
{
   if ( (NULL == pMSPState) || (NULL == pData) || (iDataLength < 1) )
      return;

   for( int i=0; i<iDataLength; i++ )
   {
      pMSPState->uMSPRawCommand[pMSPState->iMSPRawCommandFilledBytes] = *pData;
      pMSPState->iMSPRawCommandFilledBytes++;
      if ( pMSPState->iMSPRawCommandFilledBytes >= 256 )
         pMSPState->iMSPRawCommandFilledBytes = 0;

      switch(pMSPState->iMSPState)
      {
         case MSP_STATE_ERROR:
         case MSP_STATE_WAIT_HEADER1:
            pMSPState->iMSPRawCommandFilledBytes = 0;
            if ( *pData == '$' )
            {
               pMSPState->uMSPRawCommand[0] = *pData;
               pMSPState->iMSPRawCommandFilledBytes = 1;
               pMSPState->iMSPState = MSP_STATE_WAIT_HEADER2;
            }
            break;

         case MSP_STATE_WAIT_HEADER2:
            if ( *pData == 'M' )
               pMSPState->iMSPState = MSP_STATE_WAIT_DIR;
            else
            {
               pMSPState->iMSPState = MSP_STATE_ERROR;
               pMSPState->iMSPRawCommandFilledBytes = 0;
            }
            break;

         case MSP_STATE_WAIT_DIR:
            if ( *pData == '<' )
            {
               pMSPState->iMSPState = MSP_STATE_WAIT_SIZE;
               pMSPState->iMSPDirection = MSP_DIR_TO_FC;
            }
            else if ( *pData == '>' )
            {
               pMSPState->iMSPState = MSP_STATE_WAIT_SIZE;
               pMSPState->iMSPDirection = MSP_DIR_FROM_FC;
            }
            else
            {
               pMSPState->iMSPState = MSP_STATE_WAIT_HEADER1;
               pMSPState->iMSPRawCommandFilledBytes = 0;
            }
            break;

         case MSP_STATE_WAIT_SIZE:
            pMSPState->iMSPCommandPayloadSize = (int) *pData;
            pMSPState->uMSPChecksum = *pData;
            pMSPState->iMSPState = MSP_STATE_WAIT_TYPE;
            break;

         case MSP_STATE_WAIT_TYPE:
            pMSPState->uMSPPreviousCommand = pMSPState->uMSPCommand;
            pMSPState->uMSPCommand = *pData;
            pMSPState->uMSPChecksum ^= *pData;
            pMSPState->iMSPParsedCommandPayloadBytes = 0;
            if ( pMSPState->iMSPCommandPayloadSize > 0 )
               pMSPState->iMSPState = MSP_STATE_PARSE_DATA;
            else
               pMSPState->iMSPState = MSP_STATE_WAIT_CHECKSUM;
            break;

         case MSP_STATE_PARSE_DATA:
            pMSPState->uMSPCommandPayload[pMSPState->iMSPParsedCommandPayloadBytes] = *pData;
            pMSPState->iMSPParsedCommandPayloadBytes++;
            pMSPState->uMSPChecksum ^= *pData;
            if ( pMSPState->iMSPParsedCommandPayloadBytes >= pMSPState->iMSPCommandPayloadSize )
               pMSPState->iMSPState = MSP_STATE_WAIT_CHECKSUM;
            break;

         case MSP_STATE_WAIT_CHECKSUM:
            if ( pMSPState->uMSPChecksum == *pData )
            {
               pMSPState->uLastMSPCommandReceivedTime = g_TimeNow;
               if ( pMSPState->uMSPCommand == MSP_CMD_DISPLAYPORT )
                  _parse_msp_osd_command(pMSPState, bAllowScreenUpdate);
            }
            pMSPState->iMSPState = MSP_STATE_WAIT_HEADER1;
            pMSPState->iMSPRawCommandFilledBytes = 0;
            break;

         default:
            break;
      }
      pData++;
   }
}
