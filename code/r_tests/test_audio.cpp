#include <stdlib.h>
#include "../base/base.h"
#include "../base/config.h"
#include "../base/shared_mem.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../common/string_utils.h"

int iCountErrors = 0;


void _find_block(u8* pPacket, char* szMatchFile)
{
   if ( NULL == pPacket || NULL == szMatchFile || 0 == szMatchFile[0] )
   {
      printf("Invalid data\n");
      iCountErrors++;
      return;
   }

   t_packet_header* pPHSrc = (t_packet_header*) pPacket;
   u8* pDataSrc = pPacket + sizeof(t_packet_header);

   u32 uSrcAudioIndex;
   memcpy(&uSrcAudioIndex, pDataSrc, sizeof(u32));
   u32 uSrcBlockIndex = uSrcAudioIndex >> 8;
   u32 uSrcPacketIndex = uSrcAudioIndex & 0xFF;

   FILE* fpInput = fopen(szMatchFile, "rb");

   if ( NULL == fpInput )
   {
      printf("Invalid match file.\n");
      iCountErrors++;
      return;
   }

   u8 buffer[2000];
   bool bFound = false;

   while ( true )
   {
      int iRead = fread(buffer, 1, sizeof(t_packet_header), fpInput);
      if ( iRead <= 0 )
      {
         //printf("End of file. Read %d bytes.\n", iTotalRead);
         break;
      }

      t_packet_header* pPH = (t_packet_header*)buffer;
      if ( pPH->packet_type != PACKET_TYPE_AUDIO_SEGMENT )
      {
         printf("Different packet type found: %s\n", str_get_packet_type(pPH->packet_type));
         iCountErrors++;
         continue;
      }

      u8* pData = &(buffer[sizeof(t_packet_header)]);

      // Read rest of the packet
      iRead = fread( pData, 1, pPH->total_length - sizeof(t_packet_header), fpInput);
      if ( iRead <= 0 )
      {
         break;
      }

      u32 uAudioIndex;
      memcpy(&uAudioIndex, pData, sizeof(u32));
      u32 uBlockIndex = uAudioIndex >> 8;
      u32 uPacketIndex = uAudioIndex & 0xFF;

      if ( uBlockIndex != uSrcBlockIndex )
         continue;
      if ( uPacketIndex != uSrcPacketIndex )
         continue;

      printf("found match at %u/%u, %d bytes, ", uBlockIndex, uPacketIndex, pPH->total_length - sizeof(u32) - sizeof(t_packet_header));
      bool bIdentical = true;
      if ( pPHSrc->total_length != pPH->total_length )
      {
         printf("Packets have different size!\n");
         iCountErrors++;
      }
      for( int i=4; i<pPHSrc->total_length; i++ )
      {
         if ( (*(pPacket+i)) != buffer[i] )
         {
            bIdentical = false;
            printf("Packets differ at byte %d.\n", i);
            iCountErrors++;
            break;
         }
      }
      if ( bIdentical )
         printf("Identical.\n");
      bFound = true;
      break;
   }

   fclose(fpInput);

   if ( ! bFound )
   {
      printf("Packet %u/%u not found!\n", uSrcBlockIndex, uSrcPacketIndex);
      iCountErrors++;
   }
}


void _parse_block(u8* pPacket, char* szMatchFile)
{
   if ( NULL == pPacket || NULL == szMatchFile || 0 == szMatchFile[0] )
   {
      printf("Invalid data\n");
      iCountErrors++;
      return;
   }
   t_packet_header* pPH = (t_packet_header*) pPacket;
   u8* pData = pPacket + sizeof(t_packet_header);

   u32 uAudioIndex;
   memcpy(&uAudioIndex, pData, sizeof(u32));
   u32 uBlockIndex = uAudioIndex >> 8;
   u32 uPacketIndex = uAudioIndex & 0xFF;

   printf("Parsing packet %u/%u: ", uBlockIndex, uPacketIndex);

   _find_block(pPacket, szMatchFile);
}

int main(int argc, char *argv[])
{
   printf("\nTesting raw audio stream decode...\n");

   if ( argc < 2 )
   {
      printf("\nSpecify input raw file name.\n");
      return 0;

   }
   FILE* fpInput = fopen(argv[1], "rb");
   if ( NULL == fpInput )
   {
      printf("Failed to open input file %s\n", argv[1]);
      return 0;
   }

   char szKey[24];
   strcpy(szKey, "0123456789");
   szKey[10] = 10;
   szKey[11] = 0;
   int iKeyPositionToCheck = 0;

   char szComm[256];
   char szOutput[256];
   int iPipeOutput = -1;
   int iSegment = 0;
   FILE* fpOutput = NULL;
   char szBuff[256];

   u32 uAudioSize = 0;
   u32 uNextExpectedBlock = MAX_U32;
   u32 uNextExpectedPacket = MAX_U32;

   u32 uStartBlockInput = MAX_U32;
   u32 uBlockIndex = MAX_U32;
   u32 uPacketIndex = MAX_U32;

   u8 buffer[2000];
   int iTotalRead = 0;
   while ( true )
   {
      int iRead = fread(buffer, 1, sizeof(t_packet_header), fpInput);
      if ( iRead <= 0 )
      {
         printf("End of file. Read %d bytes.\n", iTotalRead);
         break;
      }
      iTotalRead += iRead;

      t_packet_header* pPH = (t_packet_header*)buffer;
      if ( pPH->packet_type != PACKET_TYPE_AUDIO_SEGMENT )
      {
         printf("Different packet type found: %s\n", str_get_packet_type(pPH->packet_type));
         iCountErrors++;
         continue;
      }

      u8* pData = &(buffer[sizeof(t_packet_header)]);

      // Read rest of the packet
      iRead = fread( pData, 1, pPH->total_length - sizeof(t_packet_header), fpInput);
      if ( iRead <= 0 )
      {
         printf("End of file. Read %d bytes.\n", iTotalRead);
         break;
      }
      iTotalRead += iRead;

      u32 uAudioIndex;
      memcpy(&uAudioIndex, pData, sizeof(u32));
      uBlockIndex = uAudioIndex >> 8;
      uPacketIndex = uAudioIndex & 0xFF;

      if ( uNextExpectedBlock == MAX_U32 )
      {
          uNextExpectedBlock = uBlockIndex;
          uNextExpectedPacket = uPacketIndex;
          uStartBlockInput = uBlockIndex;
          uAudioSize = pPH->total_length - sizeof(u32) - sizeof(t_packet_header);

          printf("Start decoding at block %u/%u, audio size: %u\n", uNextExpectedBlock, uNextExpectedPacket, uAudioSize);
      }

      u32 uSize = pPH->total_length - sizeof(u32) - sizeof(t_packet_header);
      if ( uSize != uAudioSize )
      {
         printf("Invalid packet size: %u bytes, expected %u bytes.\n", uSize, uAudioSize);
         iCountErrors++;
      }

      if ( (uBlockIndex != uNextExpectedBlock) || (uPacketIndex != uNextExpectedPacket) )
      {
         printf("Missing packets at %u/%u, expected %u/%u\n",
            uBlockIndex, uPacketIndex, uNextExpectedBlock, uNextExpectedPacket);
         uNextExpectedBlock = uBlockIndex;
         uNextExpectedPacket = uPacketIndex;
         iCountErrors++;
      }

      uNextExpectedPacket++;
      if ( uNextExpectedPacket >= 6 )
      {
         uNextExpectedPacket = 0;
         uNextExpectedBlock++;
      }

      if ( argc > 2 )
         _parse_block(buffer, argv[2]);
   }

   fclose(fpInput);

   printf("Input stream: from block %u to block %u, %u bytes each.\n%d errors\n",
      uStartBlockInput, uBlockIndex, uAudioSize, iCountErrors);
   printf("Done\n");
   return (0);
}
