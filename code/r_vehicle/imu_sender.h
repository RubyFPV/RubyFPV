/*
 * imu_sender.h - Vehicle-side BMI270 producer for Ruby telemetry uplink.
 *
 * Reads the BMI270 at the configured ODR on a background thread, batches
 * samples into PACKET_TYPE_RUBY_TELEMETRY_IMU_DOWNLOAD packets, and sends
 * them via the existing router IPC channel.
 *
 * If no BMI270 is present (chip-id mismatch or I2C open failure), all
 * functions become no-ops with a single log message at init time.
 * The same vehicle binary therefore works on hardware with or without
 * the IMU populated.
 *
 * Lifecycle (called from ruby_tx_telemetry main):
 *
 *   imu_sender_init(ipc_fd, vehicle_id);          // after IPC setup
 *   while (running) { imu_sender_drain_and_send(); }  // every ~40 ms
 *   imu_sender_shutdown();                        // on exit
 */
#ifndef IMU_SENDER_H
#define IMU_SENDER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize.  Best-effort; never fails the caller.  Returns 1 if the
 * IMU was opened successfully, 0 if absent (silently disabled). */
int imu_sender_init(int ipc_to_router_fd, uint32_t vehicle_id);

/* Drain pending samples (up to one packet's worth) and send.
 * Returns number of samples sent (0 if buffer empty or IMU disabled). */
int imu_sender_drain_and_send(void);

/* Stop reader thread, power down sensor. */
void imu_sender_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
