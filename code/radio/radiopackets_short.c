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
#include "radiopackets_short.h"
#include "radiolink.h"

u8 s_uRadioPacketsShortIndexes[MAX_RADIO_INTERFACES];

void radio_packets_short_init()
{
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      s_uRadioPacketsShortIndexes[i] = 0;
}

void radio_packet_short_init(t_packet_header_short* pPHS)
{
   if ( NULL == pPHS )
      return;
   pPHS->start_header = SHORT_PACKET_START_BYTE_REG_PACKET;
   pPHS->crc = 0;
   pPHS->data_length = 0;
   pPHS->packet_id = 0;
   pPHS->last_ack_packet_id = 0;
}

u8 radio_packets_short_get_next_id_for_radio_interface(int iInterfaceIndex)
{
   u8 uIndex = s_uRadioPacketsShortIndexes[iInterfaceIndex];
   s_uRadioPacketsShortIndexes[iInterfaceIndex]++;
   return uIndex; 
}

int radio_buffer_is_valid_short_packet(u8* pBuffer, int iLength)
{
   if ( (NULL == pBuffer) || (iLength < (int)sizeof(t_packet_header_short)) )
      return 0;

   if ( ((*pBuffer) != SHORT_PACKET_START_BYTE_REG_PACKET) &&
        ((*pBuffer) != SHORT_PACKET_START_BYTE_START_PACKET) &&
        ((*pBuffer) != SHORT_PACKET_START_BYTE_END_PACKET) )
      return 0;

   t_packet_header_short* pPHS = (t_packet_header_short*)pBuffer;
      
   if ( pPHS->data_length > iLength - sizeof(t_packet_header_short) )
      return 0;

   u8 crc = base_compute_crc8(pBuffer+2, pPHS->data_length + sizeof(t_packet_header_short)-2);
   if ( crc != pPHS->crc )
   {
      //log_buffer1(pBuffer, pPHS->data_length + sizeof(t_packet_header_short), 6);
      return 0;
   }
   return 1;
}
