#include "../base/base.h"
#include "../base/config.h"
#include "../base/shared_mem.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"

#include "../radio/fec.h"

int packet_length = 32;
int packets_per_block = 6;
int fecs_per_block = 3;

#define MAX_PACKETS 64


u8* packetsArray[MAX_PACKETS];
u8* fecsArray[MAX_PACKETS];
u8* fecsForDecode[MAX_PACKETS];

void print_all()
{
   for( int i=0; i<packets_per_block; i++ )
   {
      printf("\nPacket %d: ", i);
      for( int j=0; j<packet_length; j++ )
         printf("%d,", packetsArray[i][j]);
   }
   printf("\n");
   for( int i=0; i<fecs_per_block; i++ )
   {
      printf("\nFEC %d: ", i);
      for( int j=0; j<packet_length; j++ )
         printf("%d,", fecsArray[i][j]);
   }
   printf("\n");
}

int main(int argc, char *argv[])
{
   printf("\nTesting FEC encode/decode\n");

   fec_init();

   for( int i=0; i<packets_per_block; i++ )
   {
      packetsArray[i] = (u8*)malloc(packet_length);
      for( int j=0; j<packet_length; j++ )
         packetsArray[i][j] = 32 + i + j;
   }
   for( int i=0; i<fecs_per_block; i++ )
   {
      fecsArray[i] = (u8*)malloc(packet_length);
   }

   print_all();

   fec_encode(packet_length, packetsArray, packets_per_block, fecsArray, fecs_per_block);

   printf("\nEncode result:\n");
   print_all();


   for( int j=0; j<packet_length; j++ )
    {
      packetsArray[2][j] = 0;
      packetsArray[4][j] = 0;
      fecsArray[1][j] = 0;
    }

   printf("\nRandomised data:\n");
   print_all();

   //the following three fields are infos for fec_decode
    unsigned int fec_block_nos[MAX_PACKETS];
    unsigned int erased_blocks[MAX_PACKETS];
    unsigned int nr_fec_blocks = 0;

    erased_blocks[0] = 2;
    fec_block_nos[0] = 0;
    fecsForDecode[0] = fecsArray[0];

    erased_blocks[1] = 4;
    fec_block_nos[1] = 2;
    fecsForDecode[1] = fecsArray[2];

    nr_fec_blocks = 2;

   fec_decode(packet_length, packetsArray, packets_per_block, fecsForDecode, fec_block_nos, erased_blocks, nr_fec_blocks );

   printf("\nDecoded data:\n");
   print_all();


   return (0);
}
