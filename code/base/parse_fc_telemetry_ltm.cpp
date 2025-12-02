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

#include "../base/base.h"
#include "parse_fc_telemetry_ltm.h"
#include "parse_fc_telemetry.h"
#include "../../mavlink/common/mavlink.h"

#define LIGHTTELEMETRY_START1 0x24 //$ Header byte 1
#define LIGHTTELEMETRY_START2 0x54 //T Header byte 2

#define LIGHTTELEMETRY_AFRAME 0x41 //A Attitude frame: Attitude data (Roll, Pitch, Heading)
#define LIGHTTELEMETRY_GFRAME 0x47 //G GPS frame: GPS + Baro altitude data (Lat, Lon, Speed, Alt, Sats, Sat fix)
#define LIGHTTELEMETRY_NFRAME 0x4e //N Navigation frame
#define LIGHTTELEMETRY_OFRAME 0x4f //O Origin frame: (Lat, Lon, Alt, OSD on, home fix)
#define LIGHTTELEMETRY_SFRAME 0x53 //S Status frame: Sensors/Status data (VBat, Consumed current, Rssi, Airspeed, Arm status, Failsafe status, Flight mode)
#define LIGHTTELEMETRY_XFRAME 'X' //X GPS eXtra frame: (GPS HDOP value, hw_status (failed sensor))

// complete length including headers and checksum
#define LIGHTTELEMETRY_GFRAMELENGTH 18
#define LIGHTTELEMETRY_AFRAMELENGTH 10
#define LIGHTTELEMETRY_SFRAMELENGTH 11
#define LIGHTTELEMETRY_OFRAMELENGTH 18
#define LIGHTTELEMETRY_NFRAMELENGTH 10
#define LIGHTTELEMETRY_XFRAMELENGTH 10  

#define LTM_PARSE_STATE_IDLE 0
#define LTM_PARSE_STATE_HEADER_START1 1
#define LTM_PARSE_STATE_HEADER_START2 2
#define LTM_PARSE_STATE_HEADER_MSGTYPE 3
#define LTM_PARSE_STATE_HEADER_DATA 4


extern bool s_bHasReceivedGPSInfo;
extern bool s_bHasReceivedGPSPos;
extern bool s_bHasReceivedHeartbeat;

extern int s_iHeartbeatMsgCount;
extern int s_iSystemMsgCount;

static u8 s_StateLTMParser = LTM_PARSE_STATE_IDLE;
static u8 s_LTMExpectedFrameType = 0;
static u8 s_LTMExpectedPayloadLength = 0;
static u8 s_StateLTMChecksum = 0;
static u8 s_StateLTMPayloadAddIndex = 0;

static u8 s_LTMPayloadBuffer[1024];
static u8 s_LTMPayloadReadIndex = 0;

static u32 s_LTMLastCurrentComputeTime = 0;
static u16 s_LTMLastConsumedCurrent = 0;

static u32 s_TimeLastLTM_Altitude = 0;
static long s_LastLTM_Altitude = 0; // in 1/10 m

u8 parse_ltm_read_u8()
{
   return s_LTMPayloadBuffer[s_LTMPayloadReadIndex++];
}


u16 parse_ltm_read_u16()
{
   u16 t = parse_ltm_read_u8();
   t |= (u16)parse_ltm_read_u8()<<8;
   return t;
}

u32 parse_ltm_read_u32()
{
   u32 t = parse_ltm_read_u16();
   t |= (uint32_t)parse_ltm_read_u16()<<16;
   return t;
}

bool _parse_ltm_message(t_packet_header_fc_telemetry* pdpfct, t_packet_header_ruby_telemetry_extended_v6* pPHRTE, u8 vehicleType)
{
   s_LTMPayloadReadIndex = 0;
   u8 tmp8;
   //u16 tmp16;
   //u32 tmp32;
   //printf("\n%c", s_LTMExpectedFrameType);

   if ( s_LTMExpectedFrameType == LIGHTTELEMETRY_GFRAME )
   {
      pdpfct->latitude = parse_ltm_read_u32();
      pdpfct->longitude = parse_ltm_read_u32();

      tmp8 = parse_ltm_read_u8();
      pdpfct->hspeed = tmp8*100+100000;
      pdpfct->altitude = ((int)parse_ltm_read_u32()) + 100000;
      pdpfct->altitude_abs = pdpfct->altitude;
      if ( s_TimeLastLTM_Altitude == 0 )
      {
         s_TimeLastLTM_Altitude = get_current_timestamp_ms();
         s_LastLTM_Altitude = ((long)pdpfct->altitude) - 100000;
         pdpfct->vspeed = 100000;
      }
      else
      {
         long alt = ((long)pdpfct->altitude) - 100000;
         if ( get_current_timestamp_ms() > s_TimeLastLTM_Altitude )
         {
            long dTime = get_current_timestamp_ms() - s_TimeLastLTM_Altitude; 
            float vspeed = (float)(alt - s_LastLTM_Altitude)*1000.0/(float)dTime;
            //log_line("alt: %d - %d, %d, %f, dt: %d", alt, s_LastMAVLink_Altitude, (long)vspeed, vspeed, dTime);
            pdpfct->vspeed = (u32)(vspeed + 100000);
         }
         s_TimeLastLTM_Altitude = get_current_timestamp_ms();
         s_LastLTM_Altitude = alt;
      }
        
      tmp8 = parse_ltm_read_u8();
      pdpfct->satelites = (tmp8 >> 2) & 0xFF;
      pdpfct->gps_fix_type = 0;
      if ( tmp8 & 0b00000011 )
      {
         pdpfct->gps_fix_type = GPS_FIX_TYPE_3D_FIX;
      }
      s_bHasReceivedGPSPos = true;
      s_bHasReceivedGPSInfo = true;
      //printf("\nalt: %d, lat: %u\n", pdpfct->altitude, pdpfct->latitude);
      s_iSystemMsgCount++;
      return true;
   }
 
   if ( s_LTMExpectedFrameType == LIGHTTELEMETRY_AFRAME )
   {
      pdpfct->pitch = ((int16_t)parse_ltm_read_u16())*100 + 18000;
      pdpfct->roll =  ((int16_t)parse_ltm_read_u16())*100 + 18000;
      pdpfct->heading = (((int16_t)parse_ltm_read_u16()) + 360+180) % 360;
      return true;
   }
   if ( s_LTMExpectedFrameType == LIGHTTELEMETRY_OFRAME )
   {
      //printf("\nooo");
      //td->ltm_home_latitude = (double)((int32_t)ltmread_u32())/10000000;
      //td->ltm_home_longitude = (double)((int32_t)ltmread_u32())/10000000;
      //td->ltm_home_altitude = (float)((int32_t)ltmread_u32())/100.0f;
      //td->ltm_osdon = ltmread_u8();
      //td->ltm_homefix = ltmread_u8();
      return true;
   }
   if ( s_LTMExpectedFrameType == LIGHTTELEMETRY_NFRAME )
   {
      //printf("\nnnn");
      //uint8_t ltm_nav_mode = ltmread_u8();
      //uint8_t ltm_nav_state = ltmread_u8();
      //uint8_t ltm_nav_activeWpAction = ltmread_u8();
      //uint8_t ltm_nav_activeWpNumber = ltmread_u8();
      //uint8_t ltm_nav_error = ltmread_u8();
      //uint8_t ltm_nav_flags = ltmread_u8();
      //td->mission_current_seq = ltm_nav_activeWpNumber;
       
      return true;
   }
   if ( s_LTMExpectedFrameType == LIGHTTELEMETRY_XFRAME ) 
   {
      //printf("\nxxx");
      //HDOP 		uint16 HDOP * 100
      //hw status 	uint8
      //LTM_X_counter 	uint8
      //Disarm Reason 	uint8
      //(unused) 		1byte

      pdpfct->hdop = parse_ltm_read_u16();
      return true;
   }
   if ( s_LTMExpectedFrameType == LIGHTTELEMETRY_SFRAME )
   {
      //printf("\nsss");
      //Vbat 			uint16, mV
      //Battery Consumption 	uint16, mAh
      //RSSI 			uchar
      //Airspeed 			uchar, m/s
      //Status 			uchar
      
      pdpfct->voltage = parse_ltm_read_u16();
      pdpfct->mah = parse_ltm_read_u16();
      pdpfct->rc_rssi = parse_ltm_read_u8();

      if ( NULL != pPHRTE )
      {
         if ( ! (pPHRTE->uRubyFlags & FLAG_RUBY_TELEMETRY_HAS_MAVLINK_RC_RSSI) )
         {
            log_line("Received RC RSSI from FC through LTM, value: %d", pdpfct->rc_rssi);
            pPHRTE->uRubyFlags |= FLAG_RUBY_TELEMETRY_HAS_MAVLINK_RC_RSSI;
         }
         pPHRTE->uplink_mavlink_rc_rssi = pdpfct->rc_rssi;
      }

      //printf("\nvolt: %f, amps: %f, mah: %d", pdpfct->voltage/100.0f, pdpfct->current/100.0f, pdpfct->mah);
      
      if ( get_current_timestamp_ms() >= s_LTMLastCurrentComputeTime + 500 )
      {
         if ( 0 != s_LTMLastConsumedCurrent && 0 != s_LTMLastCurrentComputeTime )
         {
            pdpfct->current = 2*(pdpfct->mah-s_LTMLastConsumedCurrent);
            s_LTMLastConsumedCurrent = pdpfct->mah;
         }
         s_LTMLastConsumedCurrent = pdpfct->mah;
         s_LTMLastCurrentComputeTime = get_current_timestamp_ms();

      }
      tmp8 = parse_ltm_read_u8();

      pdpfct->flight_mode = 0;
      switch (( tmp8 >> 2) & 0b00111111 )
      {
         case 0: pdpfct->flight_mode = FLIGHT_MODE_MANUAL; break;
         case 1: pdpfct->flight_mode = FLIGHT_MODE_RATE; break;
         case 2: pdpfct->flight_mode = FLIGHT_MODE_ACRO; break;
         case 3: pdpfct->flight_mode = FLIGHT_MODE_HORZ; break;
         case 4: pdpfct->flight_mode = FLIGHT_MODE_ACRO; break;
         case 5: pdpfct->flight_mode = FLIGHT_MODE_STAB; break;
         case 6: pdpfct->flight_mode = FLIGHT_MODE_STAB; break;
         case 7: pdpfct->flight_mode = FLIGHT_MODE_STAB; break;
         case 8: pdpfct->flight_mode = FLIGHT_MODE_ALTH; break;
         case 9: pdpfct->flight_mode = FLIGHT_MODE_POSHOLD; break;
         //case 10: pdpfct->flight_mode = FLIGHT_MODE_HORZ; break;
         //case 11: pdpfct->flight_mode = FLIGHT_MODE_HORZ; break;
         case 12: pdpfct->flight_mode = FLIGHT_MODE_CIRCLE; break;
         case 13: pdpfct->flight_mode = FLIGHT_MODE_RTL; break;
         //case 14: pdpfct->flight_mode = FLIGHT_MODE_HORZ; break;
         case 15: pdpfct->flight_mode = FLIGHT_MODE_LAND; break;
         case 16: pdpfct->flight_mode = FLIGHT_MODE_FBWA; break;
         case 17: pdpfct->flight_mode = FLIGHT_MODE_FBWB; break;
         case 18: pdpfct->flight_mode = FLIGHT_MODE_CRUISE; break;
         //case 19: pdpfct->flight_mode = FLIGHT_MODE_HORZ; break;
         //case 20: pdpfct->flight_mode = FLIGHT_MODE_HORZ; break;
         case 21: pdpfct->flight_mode = FLIGHT_MODE_AUTOTUNE; break;
      }

      if ( tmp8 & 0b00000001 )
      {
         pdpfct->flight_mode |= FLIGHT_MODE_ARMED;
         pdpfct->uFCFlags |= FC_TELE_FLAGS_ARMED;
      }
      else
      {
         pdpfct->flight_mode &= ~FLIGHT_MODE_ARMED;
         pdpfct->uFCFlags &= ~FC_TELE_FLAGS_ARMED;
      }
      s_bHasReceivedHeartbeat = true;
      s_iHeartbeatMsgCount++;
      return true;
   }
   return false;
}



bool parse_telemetry_from_fc_ltm( u8* buffer, int length, t_packet_header_fc_telemetry* pphfct, t_packet_header_ruby_telemetry_extended_v6* pPHRTE, u8 vehicleType)
{
   bool ret = false;
   u8 c;
   for( int i=0; i<length; i++)
   {
      c = *buffer;
      buffer++;
      if ( s_StateLTMParser == LTM_PARSE_STATE_IDLE )
      {
         s_StateLTMParser = (c=='$') ? LTM_PARSE_STATE_HEADER_START1 : LTM_PARSE_STATE_IDLE;
         continue;
      }
      if ( s_StateLTMParser == LTM_PARSE_STATE_HEADER_START1 )
      {
         s_StateLTMParser = (c=='T') ? LTM_PARSE_STATE_HEADER_START2 : LTM_PARSE_STATE_IDLE;
         continue;
      }
      if ( s_StateLTMParser == LTM_PARSE_STATE_HEADER_START2 )
      {
         s_LTMExpectedPayloadLength = 0;
         switch (c)
         {
            case 'G':
               s_LTMExpectedPayloadLength = LIGHTTELEMETRY_GFRAMELENGTH;
               break;
            case 'A':
               s_LTMExpectedPayloadLength = LIGHTTELEMETRY_AFRAMELENGTH;
               break;
            case 'S':
               s_LTMExpectedPayloadLength = LIGHTTELEMETRY_SFRAMELENGTH;
               break;
            case 'O':
               s_LTMExpectedPayloadLength = LIGHTTELEMETRY_OFRAMELENGTH;
               break;
            case 'N':
               s_LTMExpectedPayloadLength = LIGHTTELEMETRY_NFRAMELENGTH;
               break;
            case 'X':
               s_LTMExpectedPayloadLength = LIGHTTELEMETRY_XFRAMELENGTH;
               break;
            default:
               s_StateLTMParser = LTM_PARSE_STATE_IDLE;
               break;
         }
         if ( s_LTMExpectedPayloadLength > 0 )
            s_StateLTMParser = LTM_PARSE_STATE_HEADER_MSGTYPE;
         s_LTMExpectedFrameType = c;
         s_StateLTMPayloadAddIndex = 0;
         continue;
      }

      if ( s_StateLTMParser == LTM_PARSE_STATE_HEADER_MSGTYPE )
      {
           if ( s_StateLTMPayloadAddIndex == 0 )
              s_StateLTMChecksum = c;
           else 
              s_StateLTMChecksum ^= c;

           if (s_StateLTMPayloadAddIndex == s_LTMExpectedPayloadLength-4)
           {
              // Received checksum byte
              s_LTMPayloadReadIndex = 0;
              if ( s_StateLTMChecksum == 0 )
              {
                 set_time_last_mavlink_message_from_fc(get_current_timestamp_ms());
                 ret = _parse_ltm_message( pphfct, pPHRTE, vehicleType );
                 //printf("\nrecv %c ltm frame, %d bytes", s_LTMExpectedFrameType, s_StateLTMPayloadAddIndex );
              }
              s_StateLTMParser = LTM_PARSE_STATE_IDLE;

           }
           else
              s_LTMPayloadBuffer[s_StateLTMPayloadAddIndex++] = c;
      }
   }
   return ret;
}
