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

#include "../base/msp.h"
#include "parse_msp.h"
#include "timers.h"


void parse_msp_reset_state(type_msp_parse_state* pMSPState)
{
   if ( NULL == pMSPState )
      return;

   memset(pMSPState, 0, sizeof(type_msp_parse_state));

   memset( &(pMSPState->headerTelemetryMSP), 0, sizeof(t_packet_header_telemetry_msp));

   pMSPState->iMSPState = MSP_STATE_WAIT_HEADER1;
   pMSPState->bEmptyBuffer = true;
   pMSPState->uMSPPreviousCommand = 0xFF;
   pMSPState->uMSPDisplayPortCommand = 0xFF;
   pMSPState->uMSPDisplayPortPreviousCommand = 0xFF;
}

void _parse_msp_osd_add_string(t_structure_vehicle_info* pRuntimeInfo, u8* pData, int iDataLength)
{
   if ( (NULL == pRuntimeInfo) || (NULL == pData) || (iDataLength < 4) )
      return;

   if ( (pRuntimeInfo->mspState.headerTelemetryMSP.uRows == 0) ||
        (pRuntimeInfo->mspState.headerTelemetryMSP.uCols == 0) ||
        ((pRuntimeInfo->mspState.headerTelemetryMSP.uFlags & MSP_FLAGS_FC_TYPE_MASK) == 0) )
      return;

   pRuntimeInfo->mspState.bEmptyBuffer = false;

   int y = pData[0];
   int x = pData[1];
   u8 uAttr = pData[2];
   if ( (x<0) || (x >= 64) || (y<0) || (y>=24) )
      return;

   char szBuff[255];
   memset(szBuff, 0, 255);
   for( int i=3; i<iDataLength; i++ )
   {
      if ( pData[i] == 0 )
         break;
      pRuntimeInfo->mspState.uScreenCharsTmp[x + y*pRuntimeInfo->mspState.headerTelemetryMSP.uCols] = pData[i];
      if ( uAttr & 0x03 )
         pRuntimeInfo->mspState.uScreenCharsTmp[x + y*pRuntimeInfo->mspState.headerTelemetryMSP.uCols] |= (((u16)(uAttr & 0x03)) << 8);
      szBuff[i-3] = pData[i];
      x++;
   }
}

void _parse_msp_osd_command(t_structure_vehicle_info* pRuntimeInfo)
{
   if ( NULL == pRuntimeInfo )
      return;
   if ( (pRuntimeInfo->mspState.uMSPCommand != MSP_CMD_DISPLAYPORT) || (pRuntimeInfo->mspState.iMSPCommandPayloadSize < 1) || (pRuntimeInfo->mspState.iMSPDirection != MSP_DIR_FROM_FC) )
      return;

   pRuntimeInfo->mspState.uMSPDisplayPortPreviousCommand = pRuntimeInfo->mspState.uMSPDisplayPortCommand;
   pRuntimeInfo->mspState.uMSPDisplayPortCommand = pRuntimeInfo->mspState.uMSPCommandPayload[0];

   switch ( pRuntimeInfo->mspState.uMSPDisplayPortCommand )
   {
      case MSP_DISPLAYPORT_CLEAR:
         //memset(&(pRuntimeInfo->mspState.uScreenChars[0]), 0, MAX_MSP_CHARS_BUFFER*sizeof(u16));
         memset(&(pRuntimeInfo->mspState.uScreenCharsTmp[0]), 0, MAX_MSP_CHARS_BUFFER*sizeof(u16));
         pRuntimeInfo->mspState.bEmptyBuffer = true;
         break;

      case MSP_DISPLAYPORT_KEEPALIVE:
         //if ( ! pRuntimeInfo->mspState.bEmptyBuffer )
            memcpy(&(pRuntimeInfo->mspState.uScreenChars[0]), &(pRuntimeInfo->mspState.uScreenCharsTmp[0]), MAX_MSP_CHARS_BUFFER*sizeof(u16));
         pRuntimeInfo->mspState.bEmptyBuffer = true;
         break;

      case MSP_DISPLAYPORT_DRAW_SCREEN:
         //if ( ! pRuntimeInfo->mspState.bEmptyBuffer )
            memcpy(&(pRuntimeInfo->mspState.uScreenChars[0]), &(pRuntimeInfo->mspState.uScreenCharsTmp[0]), MAX_MSP_CHARS_BUFFER*sizeof(u16));
         pRuntimeInfo->mspState.bEmptyBuffer = true;
         //memset(&(pRuntimeInfo->mspState.uScreenCharsTmp[0]), 0, MAX_MSP_CHARS_BUFFER*sizeof(u16));
         break;

      case MSP_DISPLAYPORT_DRAW_STRING:
         _parse_msp_osd_add_string(pRuntimeInfo, &pRuntimeInfo->mspState.uMSPCommandPayload[1], pRuntimeInfo->mspState.iMSPCommandPayloadSize-1);
         break;

      default: break;
   }
}

void parse_msp_incoming_data(t_structure_vehicle_info* pRuntimeInfo, u8* pData, int iDataLength)
{
   if ( (NULL == pRuntimeInfo) || (NULL == pData) || (iDataLength < 1) )
      return;

   for( int i=0; i<iDataLength; i++ )
   {
      pRuntimeInfo->mspState.uMSPRawCommand[pRuntimeInfo->mspState.iMSPRawCommandFilledBytes] = *pData;
      pRuntimeInfo->mspState.iMSPRawCommandFilledBytes++;
      if ( pRuntimeInfo->mspState.iMSPRawCommandFilledBytes >= 256 )
         pRuntimeInfo->mspState.iMSPRawCommandFilledBytes = 0;

      switch(pRuntimeInfo->mspState.iMSPState)
      {
         case MSP_STATE_ERROR:
         case MSP_STATE_WAIT_HEADER1:
            pRuntimeInfo->mspState.iMSPRawCommandFilledBytes = 0;
            if ( *pData == '$' )
            {
               pRuntimeInfo->mspState.uMSPRawCommand[0] = *pData;
               pRuntimeInfo->mspState.iMSPRawCommandFilledBytes = 1;
               pRuntimeInfo->mspState.iMSPState = MSP_STATE_WAIT_HEADER2;
            }
            break;

         case MSP_STATE_WAIT_HEADER2:
            if ( *pData == 'M' )
               pRuntimeInfo->mspState.iMSPState = MSP_STATE_WAIT_DIR;
            else
            {
               pRuntimeInfo->mspState.iMSPState = MSP_STATE_ERROR;
               pRuntimeInfo->mspState.iMSPRawCommandFilledBytes = 0;
            }
            break;

         case MSP_STATE_WAIT_DIR:
            if ( *pData == '<' )
            {
               pRuntimeInfo->mspState.iMSPState = MSP_STATE_WAIT_SIZE;
               pRuntimeInfo->mspState.iMSPDirection = MSP_DIR_TO_FC;
            }
            else if ( *pData == '>' )
            {
               pRuntimeInfo->mspState.iMSPState = MSP_STATE_WAIT_SIZE;
               pRuntimeInfo->mspState.iMSPDirection = MSP_DIR_FROM_FC;
            }
            else
            {
               pRuntimeInfo->mspState.iMSPState = MSP_STATE_WAIT_HEADER1;
               pRuntimeInfo->mspState.iMSPRawCommandFilledBytes = 0;
            }
            break;

         case MSP_STATE_WAIT_SIZE:
            pRuntimeInfo->mspState.iMSPCommandPayloadSize = (int) *pData;
            pRuntimeInfo->mspState.uMSPChecksum = *pData;
            pRuntimeInfo->mspState.iMSPState = MSP_STATE_WAIT_TYPE;
            break;

         case MSP_STATE_WAIT_TYPE:
            pRuntimeInfo->mspState.uMSPPreviousCommand = pRuntimeInfo->mspState.uMSPCommand;
            pRuntimeInfo->mspState.uMSPCommand = *pData;
            pRuntimeInfo->mspState.uMSPChecksum ^= *pData;
            pRuntimeInfo->mspState.iMSPParsedCommandPayloadBytes = 0;
            if ( pRuntimeInfo->mspState.iMSPCommandPayloadSize > 0 )
               pRuntimeInfo->mspState.iMSPState = MSP_STATE_PARSE_DATA;
            else
               pRuntimeInfo->mspState.iMSPState = MSP_STATE_WAIT_CHECKSUM;
            break;

         case MSP_STATE_PARSE_DATA:
            pRuntimeInfo->mspState.uMSPCommandPayload[pRuntimeInfo->mspState.iMSPParsedCommandPayloadBytes] = *pData;
            pRuntimeInfo->mspState.iMSPParsedCommandPayloadBytes++;
            pRuntimeInfo->mspState.uMSPChecksum ^= *pData;
            if ( pRuntimeInfo->mspState.iMSPParsedCommandPayloadBytes >= pRuntimeInfo->mspState.iMSPCommandPayloadSize )
               pRuntimeInfo->mspState.iMSPState = MSP_STATE_WAIT_CHECKSUM;
            break;

         case MSP_STATE_WAIT_CHECKSUM:
            if ( pRuntimeInfo->mspState.uMSPChecksum == *pData )
            {
               pRuntimeInfo->mspState.uLastMSPCommandReceivedTime = g_TimeNow;
               if ( pRuntimeInfo->mspState.uMSPCommand == MSP_CMD_DISPLAYPORT )
                  _parse_msp_osd_command(pRuntimeInfo);
            }
            pRuntimeInfo->mspState.iMSPState = MSP_STATE_WAIT_HEADER1;
            pRuntimeInfo->mspState.iMSPRawCommandFilledBytes = 0;
            break;

         default:
            break;
      }
      pData++;
   }
}
