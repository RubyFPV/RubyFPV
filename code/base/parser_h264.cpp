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
#include "parser_h264.h"


ParserH264::ParserH264()
{
   init();
}

ParserH264::~ParserH264()
{
}

void ParserH264::init()
{
   m_szPrefix[0] = 0;
   m_iDetectedISlices = 1;
   m_iConsecutiveSlicesForCurrentNALU = 1;
   m_uTotalParsedBytes = 0;
   m_uStreamCurrentParsedToken = 0x11111111;
   m_uStreamPrevParsedToken = 0x11111111;
   m_uCurrentNALUType = 0;
   m_uLastNALUType = 0;
   m_uSizeCurrentFrame = 0;
   m_uSizeLastFrame = 0;
   m_iFramesSinceLastKeyframe = 0;
   m_iDetectedKeyframeIntervalInFrames = -1;

   m_uTimeLastNALStart = 0;
   m_uTimeLastFPSCompute = 0;
   m_iFramesSinceLastFPSCompute = 0;
   m_iDetectedFPS = 0;

   m_iLastParseDetectedNALStartPosition = -1;
   m_bDetectedFirstKeyframe = false;
   m_iReadH264ProfileAfterBytes = -1;
   m_iReadH264ProfileConstrainsAfterBytes = -1;
   m_iReadH264LevelAfterBytes = -1;
   m_iDetectedH264Profile = 0;
   m_iDetectedH264ProfileConstrains = -1;
   m_iDetectedH264Level = 0;
}

void ParserH264::setPrefix(const char* szPrefix)
{
   if ( (NULL == szPrefix) || (0 == szPrefix[0]) )
   {
      m_szPrefix[0] = 0;
      return;
   }
   strncpy(m_szPrefix, szPrefix, sizeof(m_szPrefix)/sizeof(m_szPrefix[0]));
}

// Returns current NAL type
// Raspivid and Majestic: A NAL starts with 0x000001, NAL type and flags and then data
//  NAL: [0x000001] [Type & Flags] [....]
u32 ParserH264::parseData(u8* pData, int iDataLength, u32 uTimeNow)
{
   if ( (NULL == pData) || (iDataLength <= 0) )
      return 0;

   m_iLastParseDetectedNALStartPosition = -1;
   int iBytesParsed = 0;
   while ( iDataLength > 0 )
   {
      m_uStreamPrevParsedToken = (m_uStreamPrevParsedToken << 8) | (m_uStreamCurrentParsedToken & 0xFF);
      m_uStreamCurrentParsedToken = (m_uStreamCurrentParsedToken<<8) | (*pData);
      m_uTotalParsedBytes++;
      pData++;
      iDataLength--;
      iBytesParsed++;
      m_uSizeCurrentFrame++;

      if ( (-1 != m_iReadH264ProfileAfterBytes) || (-1 != m_iReadH264ProfileConstrainsAfterBytes) || (-1 != m_iReadH264LevelAfterBytes) )
         _trydetectH264Info();

      if ( m_uStreamPrevParsedToken == 0x00000001 )
      {
         m_iLastParseDetectedNALStartPosition = iBytesParsed-1;
         _parseDetectedStartOfNALUnit(uTimeNow);
      }
   }

   return m_uCurrentNALUType;
}

// Returns the number of bytes parsed from input untill start of next NAL
// Raspivid and Majestic: A NAL starts with 0x000001, NAL type and flags and then data
//  NAL: [0x000001] [Type & Flags] [....]
int ParserH264::parseDataUntilStartOfNextNALOrLimit(u8* pData, int iDataLength, int iMaxToParse, u32 uTimeNow)
{
   if ( (NULL == pData) || (iDataLength <= 0) )
      return 0;

   m_iLastParseDetectedNALStartPosition = -1;
   int iBytesParsed = 0;
   while ( (iDataLength > 0) && (iBytesParsed < iMaxToParse) )
   {
      m_uStreamPrevParsedToken = (m_uStreamPrevParsedToken << 8) | (m_uStreamCurrentParsedToken & 0xFF);
      m_uStreamCurrentParsedToken = (m_uStreamCurrentParsedToken<<8) | (*pData);
      m_uTotalParsedBytes++;
      iBytesParsed++;
      pData++;
      iDataLength--;
      m_uSizeCurrentFrame++;

      if ( (-1 != m_iReadH264ProfileAfterBytes) || (-1 != m_iReadH264ProfileConstrainsAfterBytes) || (-1 != m_iReadH264LevelAfterBytes) )
         _trydetectH264Info();

      if ( m_uStreamCurrentParsedToken == 0x00000001 )
      {
         m_iLastParseDetectedNALStartPosition = iBytesParsed-1;
         return iBytesParsed;
      }
      if ( m_uStreamPrevParsedToken == 0x00000001 )
         _parseDetectedStartOfNALUnit(uTimeNow);
   }

   return iBytesParsed;
}

void ParserH264::_trydetectH264Info()
{
   if ( m_iReadH264ProfileAfterBytes >= 0 )
   {
      m_iReadH264ProfileAfterBytes--;
      if ( 0 == m_iReadH264ProfileAfterBytes )
      {
         m_iDetectedH264Profile = m_uStreamCurrentParsedToken & 0xFF;
         log_line("Detected H264 stream profile: %d (0x%02X)", m_iDetectedH264Profile, (u8)m_iDetectedH264Profile);
      }
   }
   if ( m_iReadH264ProfileConstrainsAfterBytes >= 0 )
   {
      m_iReadH264ProfileConstrainsAfterBytes--;
      if ( 0 == m_iReadH264ProfileConstrainsAfterBytes )
      {
         m_iDetectedH264ProfileConstrains = m_uStreamCurrentParsedToken & 0xFF;
         log_line("Detected H264 stream profile constrains: %d (0x%02X)", m_iDetectedH264ProfileConstrains, (u8)m_iDetectedH264ProfileConstrains);
      }
   }
   if ( m_iReadH264LevelAfterBytes >= 0 )
   {
      m_iReadH264LevelAfterBytes--;
      if ( 0 == m_iReadH264LevelAfterBytes )
      {
         m_iDetectedH264Level = m_uStreamCurrentParsedToken & 0xFF;
         log_line("Detected H264 stream level: %d (0x%02X)", m_iDetectedH264Level, (u8)m_iDetectedH264Level);
      }
   }
}

void ParserH264::_parseDetectedStartOfNALUnit(u32 uTimeNow)
{
   m_uLastNALUType = m_uCurrentNALUType;
   m_uCurrentNALUType = m_uStreamCurrentParsedToken & 0b11111;
   m_uSizeLastFrame = m_uSizeCurrentFrame;
   
   m_uTimeLastNALStart = uTimeNow;
   m_uSizeCurrentFrame = 0;

   // Begin: compute slices based on Iframe
   if ( m_uCurrentNALUType == m_uLastNALUType )
      m_iConsecutiveSlicesForCurrentNALU++;
   else
   {
      // Reached a keyframe frame
      if ( (m_uLastNALUType != 1) && (! parser_h264_is_signaling_nal(m_uLastNALUType)) )
      {
         if ( m_bDetectedFirstKeyframe )
         {
            m_iDetectedISlices = m_iConsecutiveSlicesForCurrentNALU;
            m_iDetectedKeyframeIntervalInFrames = m_iFramesSinceLastKeyframe/m_iDetectedISlices;
         }
         if ( ! m_bDetectedFirstKeyframe )
            m_iFramesSinceLastFPSCompute = 0;
  
         m_bDetectedFirstKeyframe = true;
         m_iFramesSinceLastKeyframe = 0;
      }
      m_iConsecutiveSlicesForCurrentNALU = 1;
   }
   // End: compute slices based on Iframe

   if ( m_bDetectedFirstKeyframe )
   if ( (m_uCurrentNALUType == 5) || (m_uCurrentNALUType == 1) )
   {
      m_iFramesSinceLastKeyframe++;
      m_iFramesSinceLastFPSCompute++;
   }

   if ( m_uCurrentNALUType == 7 )
   if ( (0 == m_iDetectedH264Level) || (0 == m_iDetectedH264Profile) || (-1 == m_iDetectedH264ProfileConstrains) )
   {
      m_iReadH264ProfileAfterBytes = 1;
      m_iReadH264ProfileConstrainsAfterBytes = 2;
      m_iReadH264LevelAfterBytes = 3;
   }


   if ( m_bDetectedFirstKeyframe && (100 == m_iFramesSinceLastFPSCompute) )
   {
      if ( uTimeNow != m_uTimeLastFPSCompute )
         m_iDetectedFPS = 100000/(uTimeNow - m_uTimeLastFPSCompute);
      if ( m_iDetectedISlices > 0 )
         m_iDetectedFPS /= m_iDetectedISlices;
      m_uTimeLastFPSCompute = uTimeNow;
      m_iFramesSinceLastFPSCompute = 0;
   }
}

int ParserH264::lastParseDetectedNALStart()
{
   return m_iLastParseDetectedNALStartPosition;
}

bool ParserH264::IsInsideIFrame()
{
   return (m_uCurrentNALUType == 5)?true:false;
}

u32 ParserH264::getCurrentNALType()
{
   return m_uCurrentNALUType;
}

u32 ParserH264::getPreviousNALType()
{
   return m_uLastNALUType;
}


u32 ParserH264::getSizeOfLastCompleteFrameInBytes()
{
   return m_uSizeLastFrame;
}

int ParserH264::getDetectedSlices()
{
   return m_iDetectedISlices;
}

int ParserH264::getCurrentFrameSlices()
{
   return m_iConsecutiveSlicesForCurrentNALU;
}

int ParserH264::getDetectedFPS()
{
   return m_iDetectedFPS;
}

int ParserH264::getDetectedKeyframeIntervalMs()
{
   if ( m_iDetectedFPS == 0 )
      return 0;
   return (m_iDetectedKeyframeIntervalInFrames*1000)/m_iDetectedFPS;
}

int ParserH264::getDetectedProfile()
{
   return m_iDetectedH264Profile;
}

int ParserH264::getDetectedProfileConstrains()
{
   return m_iDetectedH264ProfileConstrains;
}

int ParserH264::getDetectedLevel()
{
   return m_iDetectedH264Level;
}

void ParserH264::resetDetectedProfileAndLevel()
{
   m_iDetectedH264Profile = 0;
   m_iDetectedH264ProfileConstrains = -1;
   m_iDetectedH264Level = 0;
}

bool parser_h264_is_signaling_nal(u8 uNALId)
{
   // 7,8: present in h264
   // 0,2,4: present in h265

   if ( (uNALId == 0) || (uNALId == 2) || (uNALId == 4) || (uNALId == 7) || (uNALId == 8) )
      return true;
   return false;
}
