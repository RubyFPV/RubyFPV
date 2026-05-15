/*
 * imu_receiver.h - GS-side consumer for PACKET_TYPE_RUBY_TELEMETRY_IMU_DOWNLOAD.
 *
 * Writes samples to a rolling <media_dir>/_current.gcsv.  Polls media_dir
 * once per ~1s for new video-*.{h265,h264,mp4} files: when one appears,
 * _current.gcsv is renamed to <basename>.gcsv and a fresh _current.gcsv
 * is opened.  This way samples that arrived during recording are paired
 * with the video file Ruby ultimately produces (Ruby remuxes with a
 * different timestamped filename at post-process time).
 *
 * Lifecycle (called from ruby_rx_telemetry main):
 *
 *   imu_receiver_init("/home/radxa/ruby/media");      // at startup
 *   on each incoming packet:
 *     if (pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_IMU_DOWNLOAD)
 *        imu_receiver_on_packet(buf, len);
 *   in the periodic loop (every ~1 s):
 *     imu_receiver_periodic();
 *   imu_receiver_shutdown();
 */
#ifndef IMU_RECEIVER_H
#define IMU_RECEIVER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int  imu_receiver_init(const char *media_dir);
void imu_receiver_on_packet(const uint8_t *buf, int total_length);
void imu_receiver_periodic(void);
void imu_receiver_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
