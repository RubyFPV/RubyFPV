#pragma once

#define MAX_VIDEO_BITRATE_HISTORY_VALUES 30
// About 1.5 seconds of history at an update rate of 20 times/sec (50 ms)

typedef struct
{
   u32 uTimeStampTaken;
   u32 uTotalBitrateBPS;
   u32 uVideoBitrateBPS;
}
type_processor_tx_video_bitrate_sample;


class ProcessorTxVideo
{
   public:
      ProcessorTxVideo(int iVideoStreamIndex, int iCameraIndex);
      virtual ~ProcessorTxVideo();

      bool init();
      bool uninit();
      
      void periodicLoop();

      u32 getCurrentVideoBitrate();
      u32 getCurrentVideoBitrateAverage();
      u32 getCurrentVideoBitrateAverageLastMs(u32 uMilisec);
      u32 getCurrentTotalVideoBitrate();
      u32 getCurrentTotalVideoBitrateAverage();
      u32 getCurrentTotalVideoBitrateAverageLastMs(u32 uMilisec);
      static int m_siInstancesCount;

   protected:
      bool m_bInitialized;
      int m_iInstanceIndex;
      int m_iVideoStreamIndex;
      int m_iCameraIndex;
      
      u32 m_uIntervalMSComputeVideoBitrateSample;
      u32 m_uTimeLastVideoBitrateSampleTaken;

      type_processor_tx_video_bitrate_sample m_BitrateHistorySamples[MAX_VIDEO_BITRATE_HISTORY_VALUES];
      int m_iVideoBitrateSampleIndex;

      u32 m_uVideoBitrateKbAverageSum;
      u32 m_uTotalVideoBitrateKbAverageSum;
      u32 m_uVideoBitrateAverage;
      u32 m_uTotalVideoBitrateAverage;
};

bool process_data_tx_video_command(int iRadioInterface, u8* pPacketBuffer);
bool process_data_tx_video_loop();
void process_data_tx_video_on_new_data(u8* pData, int iDataSize);
void process_data_tx_video_on_data_sent(int iDataSize);

