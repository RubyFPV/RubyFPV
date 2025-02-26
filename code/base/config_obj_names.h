#pragma once

#define FIFO_RUBY_CAMERA1 "/tmp/ruby/fifocam1"
#define FIFO_RUBY_AUDIO1 "/tmp/ruby/fifoaudio1"
#define FIFO_RUBY_AUDIO_QUEUE "/tmp/ruby/fifoaudioqueue"
#define FIFO_RUBY_AUDIO_BUFF "/tmp/ruby/fifoaudiobuff"

// Must be same name used by video player when it's using fifo pipes
#define FIFO_RUBY_STATION_VIDEO_STREAM_DISPLAY "/tmp/ruby/fifovideo_disp"
#define FIFO_RUBY_STATION_VIDEO_STREAM_RECORDING "/tmp/ruby/fifovideo_rec"
#define FIFO_RUBY_STATION_VIDEO_STREAM_ETH "/tmp/ruby/fifovideo_eth"

#define SEMAPHORE_RESTART_VIDEO_STREAMER "RUBY_SEM_RESTART_VIDEO_STREAMER"
#define SEMAPHORE_START_VIDEO_RECORD "RUBY_SEM_START_VIDEO_REC"
#define SEMAPHORE_STOP_VIDEO_RECORD "RUBY_SEM_STOP_VIDEO_REC"
#define SEMAPHORE_VIDEO_STREAMER_OVERLOAD "RUBY_SEM_VIDEO_STREAMER_OVERLOAD"

#define SEMAPHORE_STOP_RX_RC "RUBY_SEM_STOP_RX_RC"

#define RUBY_SEM_RX_RADIO_HIGH_PRIORITY "RUBY_SEM_RX_RADIO_HIGH_PRIORITY"
#define RUBY_SEM_RX_RADIO_REG_PRIORITY "RUBY_SEM_RX_RADIO_REG_PRIORITY"

#define SEMAPHORE_SM_VIDEO_DATA_AVAILABLE "RUBY_SEM_SM_VIDEO_DATA_AVAILABLE"
#define SEMAPHORE_MPP_DISPLAY_FRAME_READY "RUBY_SEM_MPP_DISPLAY_FRAME_READY"
#define SEMAPHORE_MPP_DECODER_STALLED "RUBY_SEM_MPP_STALLED"
#define SEMAPHORE_MPP_DECODER_NO_NEW_OUTPUT "RUBY_SEM_MPP_NO_NEW_OUTPUT"