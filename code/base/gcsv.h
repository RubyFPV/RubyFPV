/*
 * gcsv.h - Gyroflow .gcsv v1.3 writer.
 *
 * Gyroflow format reference:
 *   https://docs.gyroflow.xyz/app/getting-started/supported-cameras
 *
 * Header carries scale factors so the file stores raw int16 samples
 * (compact and lossless).  Gyroflow applies tscale/gscale/ascale on import.
 *
 *   tscale = 1e-6  -> t column is microseconds
 *   gscale = (range_dps * pi/180) / 32768   -> raw -> rad/s
 *   ascale = range_g / 32768                -> raw -> g
 */
#ifndef BMI270_GCSV_H
#define BMI270_GCSV_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	FILE *fp;
	uint64_t t0_us;        /* timestamp of first sample (anchor for t column) */
	int      header_done;
	int      gyro_range_dps;
	int      accel_range_g;
} GcsvWriter;

/* Open path, write Gyroflow header (without t0_us; that is set on first
 * sample so the t column starts at 0). */
int gcsv_open(GcsvWriter *w, const char *path,
              int gyro_range_dps, int accel_range_g,
              const char *id, const char *orientation);

/* Append one sample row.  Pass monotonic microseconds; the first call
 * latches t0 so subsequent rows are relative. */
int gcsv_write(GcsvWriter *w, uint64_t t_us,
               int16_t gx, int16_t gy, int16_t gz,
               int16_t ax, int16_t ay, int16_t az);

void gcsv_close(GcsvWriter *w);

#ifdef __cplusplus
}
#endif

#endif
