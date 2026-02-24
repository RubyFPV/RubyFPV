#include <stdlib.h>
#include <pthread.h>
#include "../base/base.h"
#include "../base/config.h"
#include "../base/shared_mem.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"

bool g_bQuit = false;
u32 g_TimeNow = 0;

char g_szFormat[64];
char g_szSpeed[64];
char g_szFlags[64];
char g_szFile[128];
bool g_bSkip = false;
bool g_bPauses = false;
int  g_iBufferPacketsCount = -1;

pthread_t s_ThreadAudioBuffering;
bool s_bThreadAudioBufferingStarted = false;
bool s_bStopThreadAudioBuffering = false;

pthread_t s_ThreadAudioQueueing;
bool s_bThreadAudioQueueingStarted = false;
bool s_bStopThreadAudioQueueing = false;

int s_fPipeAudioPlayerOutput = -1;
int s_fPipeAudioPlayerQueueWrite = -1;
int s_fPipeAudioPlayerQueueRead = -1;

int s_fPipeAudioBufferWrite = -1;
int s_fPipeAudioBufferRead = -1;

int s_iAudioPacketSize = 1024;
u8  s_uAudioBufferedPackets[MAX_BUFFERED_AUDIO_PACKETS][MAX_PACKET_PAYLOAD];
int s_iAudioBufferedPacketsSizes[MAX_BUFFERED_AUDIO_PACKETS];
int s_iAudioBufferWritePos = 0;
int s_iAudioBufferReadPos = 0;
int s_iAudioBuffersFilledInPackets = 0;
int s_iAudioBuffersTotalWrote = 0;
int s_iAudioQueueTotalWrote = 0;


void* _thread_audio_queueing_playback(void *argument)
{
   s_bThreadAudioQueueingStarted = true;
   log_line("[Thd-Que] Created audio queue thread.");
   int iCountReads = 0;
   fd_set readSet;
   fd_set exceSet;
   struct timeval timePipeInput;
   u8 uBuffer[2048];

   while ( ! g_bQuit )
   {
      if ( s_fPipeAudioPlayerQueueRead <= 0 )
      {
         hardware_sleep_ms(10);
         continue;
      }
      int iReadSize = 0;
      u32 uTimeStart = get_current_timestamp_ms();
      u32 uTimeEnd = 0;

      FD_ZERO(&readSet);
      FD_SET(s_fPipeAudioPlayerQueueRead, &readSet);
      FD_ZERO(&exceSet);
      FD_SET(s_fPipeAudioPlayerQueueRead, &exceSet);
      timePipeInput.tv_sec = 0;
      timePipeInput.tv_usec = 20*1000; // 20 miliseconds timeout

      int iSelResult = select(s_fPipeAudioPlayerQueueRead+1, &readSet, NULL, &exceSet, &timePipeInput);
      uTimeEnd = get_current_timestamp_ms();
      if ( iSelResult <= 0 )
      {
         log_line("[Thd-Que] Wait timeout in %u ms", uTimeEnd - uTimeStart);
         continue;
      }
      if( FD_ISSET(s_fPipeAudioPlayerQueueRead, &exceSet) || g_bQuit )
      {
         log_line("[Thd-Que] Exception on read. Exit");
         break;
      }
      log_line("[Thd-Que] Signaled read in %u ms", uTimeEnd-uTimeStart);
      uTimeStart = get_current_timestamp_ms();
      iReadSize = read(s_fPipeAudioPlayerQueueRead, uBuffer, s_iAudioPacketSize);

      uTimeEnd = get_current_timestamp_ms();
      //if ( 0 == iCountReads )
      //   log_line("[Thd-Que] Start reading input data (%d bytes)", iReadSize);
      iCountReads++;

      log_line("[Thd-Que] Read (%d) input %d bytes in %u ms", iCountReads, iReadSize, uTimeEnd - uTimeStart);
      if ( iReadSize <= 0 )
      {
         if ( s_bStopThreadAudioQueueing )
            break;
         hardware_sleep_ms(10);
         continue;
      }
      if ( s_bStopThreadAudioQueueing )
         break;


      if ( s_fPipeAudioPlayerOutput > 0 )
      {
         uTimeStart = get_current_timestamp_ms();
         int iRes = write(s_fPipeAudioPlayerOutput, uBuffer, iReadSize);
         uTimeEnd = get_current_timestamp_ms();
         log_line("[Thd-Que] Wrote %d bytes (total %d bytes) in %u ms", iRes, s_iAudioQueueTotalWrote, uTimeEnd-uTimeStart);
         if ( iRes < 0 )
         {
            log_line("[Thd-Que] Failed to write to audio player pipe.");
            break;
         }
         s_iAudioQueueTotalWrote += iRes;
      }
      else
         hardware_sleep_ms(1);
   }
   log_line("[Thd-Que] Finished audio playback thread.");
   s_bThreadAudioQueueingStarted = false;
   return NULL;
}


void* _thread_audio_buffering_playback(void *argument)
{
   s_bThreadAudioBufferingStarted = true;
   log_line("[Thd-Buff] Created audio playback thread.");
   int iCountReads = 0;
   fd_set readSet;
   fd_set exceSet;
   struct timeval timePipeInput;
   while ( ! g_bQuit )
   {
      if ( s_fPipeAudioBufferRead <= 0 )
      {
         hardware_sleep_ms(10);
         continue;
      }
      int iReadSize = 0;
      u32 uTimeStart = get_current_timestamp_ms();
      u32 uTimeEnd = 0;

      FD_ZERO(&readSet);
      FD_SET(s_fPipeAudioBufferRead, &readSet);
      FD_ZERO(&exceSet);
      FD_SET(s_fPipeAudioBufferRead, &exceSet);
      timePipeInput.tv_sec = 0;
      timePipeInput.tv_usec = 20*1000; // 20 miliseconds timeout

      int iSelResult = select(s_fPipeAudioBufferRead+1, &readSet, NULL, &exceSet, &timePipeInput);
      uTimeEnd = get_current_timestamp_ms();
      if ( iSelResult <= 0 )
      {
         log_line("[Thd-Buff] Wait timeout in %u ms", uTimeEnd - uTimeStart);
         continue;
      }
      if( FD_ISSET(s_fPipeAudioBufferRead, &exceSet) || g_bQuit )
      {
         log_line("[Thd-Buff] Exception on read. Exit");
         break;
      }
      log_line("[Thd-Buff] Signaled read in %u ms", uTimeEnd-uTimeStart);
      uTimeStart = get_current_timestamp_ms();
      iReadSize = read(s_fPipeAudioBufferRead, &s_uAudioBufferedPackets[s_iAudioBufferWritePos][0], s_iAudioPacketSize);

      uTimeEnd = get_current_timestamp_ms();
      //if ( 0 == iCountReads )
      //   log_line("[Thd-Buff] Start reading input data (%d bytes)", iReadSize);
      iCountReads++;

      log_line("[Thd-Buff] Read (%d) input %d bytes in %u ms", iCountReads, iReadSize, uTimeEnd - uTimeStart);
      if ( iReadSize <= 0 )
      {
         if ( s_bStopThreadAudioBuffering )
            break;
         hardware_sleep_ms(10);
         continue;
      }
      if ( s_bStopThreadAudioBuffering )
         break;

      s_iAudioBufferedPacketsSizes[s_iAudioBufferWritePos] = iReadSize;
      s_iAudioBufferWritePos++;
      if ( s_iAudioBufferWritePos >= MAX_BUFFERED_AUDIO_PACKETS )
         s_iAudioBufferWritePos = 0;
      s_iAudioBufferedPacketsSizes[s_iAudioBufferWritePos] = 0;

      if ( s_iAudioBufferReadPos == s_iAudioBufferWritePos )
         continue;

      s_iAudioBuffersFilledInPackets = s_iAudioBufferWritePos - s_iAudioBufferReadPos;
      if ( s_iAudioBufferWritePos < s_iAudioBufferReadPos )
         s_iAudioBuffersFilledInPackets = s_iAudioBufferWritePos + MAX_BUFFERED_AUDIO_PACKETS - s_iAudioBufferReadPos;

      if ( s_iAudioBuffersFilledInPackets < g_iBufferPacketsCount )
         continue;

      int iDiff = s_iAudioBuffersFilledInPackets;
      while ( iDiff > g_iBufferPacketsCount )
      {
         if ( s_fPipeAudioPlayerQueueWrite > 0 )
         {
            uTimeStart = get_current_timestamp_ms();
            int iRes = write(s_fPipeAudioPlayerQueueWrite, &s_uAudioBufferedPackets[s_iAudioBufferReadPos][0], s_iAudioBufferedPacketsSizes[s_iAudioBufferReadPos]);
            uTimeEnd = get_current_timestamp_ms();
            log_line("[Thd-Buff] Wrote one buff out of %d, %d bytes (total %d bytes) in %u ms", s_iAudioBuffersFilledInPackets, iRes, s_iAudioBuffersTotalWrote, uTimeEnd-uTimeStart);
            if ( iRes < 0 )
            {
               log_line("[Thd-Buff] Failed to write to audio queue pipe.");
               break;
            }
            s_iAudioBuffersTotalWrote += iRes;
         }
         else
            hardware_sleep_ms(1);

         s_iAudioBufferReadPos++;
         if ( s_iAudioBufferReadPos >= MAX_BUFFERED_AUDIO_PACKETS )
            s_iAudioBufferReadPos = 0;
         iDiff--;
      }
   }
   log_line("[Thd-Buff] Finished audio playback thread.");
   s_bThreadAudioBufferingStarted = false;
   return NULL;
}


void handle_sigint(int sig)
{
   log_line("Caught signal to stop: %d\n", sig);
   g_bQuit = true;
}


int main(int argc, char *argv[])
{
   if ( argc < 6 )
   {
      printf("\nUsage: test_audio2  [speed] [format] [flags] [usebuffers] [file]\n");
      printf("  speed:  8000, 16000, etc\n");
      printf("  format: S16_LE, S16_BE, etc\n");
      printf("  flags: 0: normal, 1: skips, 2: pauses\n");
      printf("  usebuffers: -1 no buff, x how much buffer\n\n");
      return -1;
   }

   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   log_init("TA");
   log_enable_stdout();

   strcpy(g_szSpeed, argv[1]);
   strcpy(g_szFormat, argv[2]);
   strcpy(g_szFlags, argv[3]);
   if ( NULL != strstr(g_szFlags, "1") )
      g_bSkip = true;
   if ( NULL != strstr(g_szFlags, "2") )
      g_bPauses = true;
   g_iBufferPacketsCount = atoi(argv[4]);
   strcpy(g_szFile, argv[5]);

   log_line("Skips: %s", g_bSkip?"yes":"no");
   log_line("Buffers: %d", g_iBufferPacketsCount);
   char szComm[256];
   //sprintf(szComm, "aplay -q --disable-resample -N -R 10000 -c 1 --rate %s --format %s %s 2>/dev/null &", g_szSpeed, g_szFormat, FIFO_RUBY_AUDIO1);
   sprintf(szComm, "aplay -q -N -R 10000 -c 1 --rate %s --format %s %s 2>/dev/null &", g_szSpeed, g_szFormat, FIFO_RUBY_AUDIO1);
   log_line("Executing player cmd: [%s]", szComm);
   hw_execute_bash_command_nonblock(szComm, NULL);

   int iFileSize = get_filesize(g_szFile);

   log_line("Opening input file (%s)...", g_szFile);
   FILE* fp = fopen(g_szFile, "rb");
   if ( NULL == fp )
   {
      log_error_and_alarm("Failed to open audio file: (%s)", g_szFile);
      hw_stop_process("aplay");
      return 0;
   }

   log_line("Opening output pipe (%s)...", FIFO_RUBY_AUDIO1);
   int iRetries = 20;
   while ( (s_fPipeAudioPlayerOutput <= 0) && (iRetries > 0) )
   {
      iRetries--;
      s_fPipeAudioPlayerOutput = open(FIFO_RUBY_AUDIO1, O_WRONLY);
      //s_fPipeAudioPlayerOutput = open(FIFO_RUBY_AUDIO1, O_WRONLY | O_NONBLOCK);
      if ( s_fPipeAudioPlayerOutput > 0 )
         break;
      log_error_and_alarm("Failed to open audio pipe write endpoint: %s, retry...",FIFO_RUBY_AUDIO1);
      hardware_sleep_ms(20);
   }
   if ( s_fPipeAudioPlayerOutput > 0 )
      log_line("Opened input file (%s) and output pipe (%s)", g_szFile, FIFO_RUBY_AUDIO1);
   else
   {
      log_error_and_alarm("Failed to open audio pipe write endpoint: %s",FIFO_RUBY_AUDIO1);
      fclose(fp);
      hw_stop_process("aplay");
      return 0;
   }

   log_line("Player pipe FIFO default size: %d bytes", fcntl(s_fPipeAudioPlayerOutput, F_GETPIPE_SZ));
   //fcntl(s_fPipeAudioPlayerOutput, F_SETPIPE_SZ, 250000);
   //log_line("Player pipe FIFO new size: %d bytes", fcntl(s_fPipeAudioPlayerOutput, F_GETPIPE_SZ));


   log_line("Prepare to streaming data...");
   for( int i=0; i<4; i++ )
      hardware_sleep_ms(500);


   if ( g_iBufferPacketsCount >= 0 )
   {
      s_fPipeAudioPlayerQueueRead = open(FIFO_RUBY_AUDIO_QUEUE, O_CREAT | O_RDONLY | O_NONBLOCK);
      if ( s_fPipeAudioPlayerQueueRead <= 0 )
      {
         log_error_and_alarm("Failed to open audio pipe queue read endpoint: %s",FIFO_RUBY_AUDIO_QUEUE);
         return 0;
      }
      log_line("Opened successfully audio pipe queue read endpoint: %s", FIFO_RUBY_AUDIO_QUEUE);

      s_fPipeAudioPlayerQueueWrite = open(FIFO_RUBY_AUDIO_QUEUE, O_CREAT | O_WRONLY | O_NONBLOCK);
      if ( s_fPipeAudioPlayerQueueWrite <= 0 )
      {
         log_error_and_alarm("[AudioRx] Failed to open audio pipe queue write endpoint: %s",FIFO_RUBY_AUDIO_QUEUE);
         return 0;
      }
      log_line("Opened successfully audio pipe queue write endpoint: %s", FIFO_RUBY_AUDIO_QUEUE);

      s_fPipeAudioBufferRead = open(FIFO_RUBY_AUDIO_BUFF, O_CREAT | O_RDONLY | O_NONBLOCK);
      if ( s_fPipeAudioBufferRead <= 0 )
      {
         log_error_and_alarm("Failed to open audio pipe buffer read endpoint: %s",FIFO_RUBY_AUDIO_BUFF);
         return 0;
      }
      log_line("Opened successfully audio pipe buffer read endpoint: %s", FIFO_RUBY_AUDIO_BUFF);

      s_fPipeAudioBufferWrite = open(FIFO_RUBY_AUDIO_BUFF, O_CREAT | O_WRONLY | O_NONBLOCK);
      if ( s_fPipeAudioBufferWrite <= 0 )
      {
         log_error_and_alarm("[AudioRx] Failed to open audio pipe buffer write endpoint: %s",FIFO_RUBY_AUDIO_BUFF);
         return 0;
      }
      log_line("Opened successfully audio pipe buffer write endpoint: %s", FIFO_RUBY_AUDIO_BUFF);

      log_line("Player queue FIFO default size: %d bytes", fcntl(s_fPipeAudioPlayerQueueWrite, F_GETPIPE_SZ));
      fcntl(s_fPipeAudioPlayerQueueWrite, F_SETPIPE_SZ, 250000);
      log_line("Player queue FIFO new size: %d bytes", fcntl(s_fPipeAudioPlayerQueueWrite, F_GETPIPE_SZ));
      log_line("Player buff FIFO default size: %d bytes", fcntl(s_fPipeAudioBufferWrite, F_GETPIPE_SZ));
      fcntl(s_fPipeAudioBufferWrite, F_SETPIPE_SZ, 250000);
      log_line("Player buff FIFO new size: %d bytes", fcntl(s_fPipeAudioBufferWrite, F_GETPIPE_SZ));

      pthread_attr_t attr;
      struct sched_param params;

      pthread_attr_init(&attr);
      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
      pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
      pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
      params.sched_priority = 0;
      pthread_attr_setschedparam(&attr, &params);
      s_bThreadAudioQueueingStarted = false;
      s_bStopThreadAudioQueueing = false;
      if ( 0 != pthread_create(&s_ThreadAudioQueueing, &attr, &_thread_audio_queueing_playback, NULL) )
         log_softerror_and_alarm("Failed to create queueing thread.");
      else
         s_bThreadAudioQueueingStarted = true;
      pthread_attr_destroy(&attr);

      pthread_attr_init(&attr);
      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
      pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
      pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
      params.sched_priority = 0;
      pthread_attr_setschedparam(&attr, &params);
      s_bThreadAudioBufferingStarted = false;
      s_bStopThreadAudioBuffering = false;
      if ( 0 != pthread_create(&s_ThreadAudioBuffering, &attr, &_thread_audio_buffering_playback, NULL) )
         log_softerror_and_alarm("Failed to create playback thread.");
      else
         s_bThreadAudioBufferingStarted = true;
      pthread_attr_destroy(&attr);
   }

   log_line("Start streaming data...");
   u8 uBuffer[2048];
   int iTotalRead = 0;
   int iReadSize = 0;
   int iCountReads = 0;
   int iCountSkips = 0;
   int iCountNextSkip = 0;

   int iNextPause = 0;

   while ( ! g_bQuit )
   {
      g_TimeNow = get_current_timestamp_ms();

      if ( s_bThreadAudioBufferingStarted )
      if ( s_iAudioBuffersFilledInPackets >= MAX_BUFFERED_AUDIO_PACKETS/4 )
      {
         log_line("Buffers are 1/4 full (%d of %d). Wating at file in %d of %d ...", s_iAudioBuffersFilledInPackets, MAX_BUFFERED_AUDIO_PACKETS, iTotalRead, iFileSize);
         hardware_sleep_ms(10);
         continue;
      }
      iReadSize = fread(uBuffer, 1, 1024, fp);
      if ( iReadSize <= 0 )
      {
         log_line("Reached end of input file. Exit.");
         break;
      }
      iCountReads++;
      iTotalRead += iReadSize;

      if ( g_bPauses )
      {
         if ( iNextPause == 0 )
             iNextPause = iCountReads + 10 + (rand()%30);
         if ( iCountReads == iNextPause )
         {
            iNextPause = 0;
            u32 uTime = 500 + (rand()%5000);
            log_line("Pause for %u ms", uTime);
            for( int i=0; i<10; i++ )
               hardware_sleep_ms(uTime/10);
            log_line("Resumed");
         }
      }

      bool bSkip = false;
      if ( g_bSkip )
      {
         if ( iCountReads < 8 )
            bSkip = true;

         if ( NULL != strstr(g_szFlags, "1") )
         {
            if ( 0 == iCountSkips )
            if ( 0 == iCountNextSkip )
            {
               iCountNextSkip = 10 + rand() % 40;
               log_line("Skip after %d packets", iCountNextSkip);
               iCountNextSkip += iCountReads;
            }

            if ( 0 == iCountSkips )
            if ( 0 != iCountNextSkip )
            if ( iCountReads >= iCountNextSkip )
            {
               iCountNextSkip = 0;
               bSkip = true;
               iCountSkips = 7 + (rand()%15);
               log_line("Skip %d counts", iCountSkips);
            }

            if ( iCountSkips > 0 )
            {
               bSkip = true;
               iCountSkips--;
               if ( 0 == iCountSkips )
                  iCountNextSkip = 0;
            }
         }
      }
      if ( bSkip )
         log_line("Skipped read %d, %d bytes", iCountReads, iReadSize);
      else
      {
         u32 uTimeStart = get_current_timestamp_ms();

         if ( s_bThreadAudioBufferingStarted )
            write(s_fPipeAudioBufferWrite, uBuffer, iReadSize);
         else
            write(s_fPipeAudioPlayerOutput, uBuffer, iReadSize);
         hardware_sleep_ms(10);
         u32 uTimeEnd = get_current_timestamp_ms();
         log_line("Wrote %d bytes at %d of %d (buffers has %d packets) in %u ms", iReadSize, iTotalRead, iFileSize, s_iAudioBuffersFilledInPackets, uTimeEnd-uTimeStart);
      }
      //log_line("Piped %d bytes...", iReadSize);
   }

   if ( s_bThreadAudioQueueingStarted )
   {
      log_line("Waiting for player to play.");
      while ( ! g_bQuit )
      {
         hardware_sleep_ms(5);
      }
   }

   if ( s_bThreadAudioBufferingStarted )
   {
      log_line("Waiting for player to play.");
      while ( ! g_bQuit )
      {
         hardware_sleep_ms(5);
      }
   }

   hw_stop_process("aplay");
   fclose(fp);

   if ( -1 != s_fPipeAudioPlayerOutput )
      close(s_fPipeAudioPlayerOutput);
   s_fPipeAudioPlayerOutput = -1;

   if ( -1 != s_fPipeAudioPlayerQueueWrite )
      close(s_fPipeAudioPlayerQueueWrite);
   s_fPipeAudioPlayerQueueWrite = -1;

   if ( -1 != s_fPipeAudioPlayerQueueRead )
      close(s_fPipeAudioPlayerQueueRead);
   s_fPipeAudioPlayerQueueRead = -1;

   if ( s_fPipeAudioBufferWrite > 0 )
      close(s_fPipeAudioBufferWrite);
   s_fPipeAudioBufferWrite = -1;

   if ( s_fPipeAudioBufferRead > 0 )
      close(s_fPipeAudioBufferRead);
   s_fPipeAudioBufferRead = -1;

   s_bStopThreadAudioBuffering = true;
   if ( s_bThreadAudioBufferingStarted )
   {
      pthread_cancel(s_ThreadAudioBuffering);
   }
   s_bThreadAudioBufferingStarted = false;

   s_bStopThreadAudioQueueing = true;
   if ( s_bThreadAudioQueueingStarted )
   {
      pthread_cancel(s_ThreadAudioQueueing);
   }
   s_bThreadAudioQueueingStarted = false;

   return 0;
}
