/*
 * gcsv.c - Gyroflow .gcsv writer.
 */
#include "gcsv.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Gyroflow .gcsv scale-factor formulas (per docs.gyroflow.xyz/.../gcsv-format
 * and AdrianEddy/telemetry-parser src/gyroflow/gcsv.rs):
 *   raw * gscale = rad/s
 *   raw * ascale = g
 */
static double gscale_rad_per_s(int dps) { return ((double)dps * M_PI / 180.0) / 32768.0; }
static double ascale_g_per_lsb(int g)   { return (double)g / 32768.0; }

int gcsv_open(GcsvWriter *w, const char *path,
              int gyr_dps, int acc_g,
              const char *id, const char *orientation)
{
	memset(w, 0, sizeof(*w));
	w->fp = fopen(path, "w");
	if (!w->fp) return -errno;

	/* Line buffer: keeps disk writes coalesced but flushes on newline
	 * so a power-loss leaves a valid prefix. */
	setvbuf(w->fp, NULL, _IOLBF, 4096);

	w->gyro_range_dps = gyr_dps;
	w->accel_range_g  = acc_g;

	double gscale = gscale_rad_per_s(gyr_dps);
	double ascale = ascale_g_per_lsb(acc_g);
	time_t now = time(NULL);

	fprintf(w->fp,
		"GYROFLOW IMU LOG\n"
		"version,1.3\n"
		"id,%s\n"
		"orientation,%s\n"
		"tscale,0.000001\n"     /* t column = microseconds */
		"gscale,%.10f\n"
		"ascale,%.10f\n"
		"timestamp,%lld\n"
		"note,bmi270 +/- %d dps / +/- %d g\n"
		"t,gx,gy,gz,ax,ay,az\n",
		id ? id : "bmi270",
		orientation ? orientation : "YxZ",
		gscale, ascale,
		(long long)now,
		gyr_dps, acc_g);

	w->header_done = 1;
	return 0;
}

int gcsv_write(GcsvWriter *w, uint64_t t_us,
               int16_t gx, int16_t gy, int16_t gz,
               int16_t ax, int16_t ay, int16_t az)
{
	if (!w->fp) return -EBADF;

	if (w->t0_us == 0)
		w->t0_us = t_us;

	uint64_t dt = (t_us >= w->t0_us) ? (t_us - w->t0_us) : 0;

	if (fprintf(w->fp, "%llu,%d,%d,%d,%d,%d,%d\n",
	            (unsigned long long)dt,
	            gx, gy, gz, ax, ay, az) < 0)
		return -errno;
	return 0;
}

void gcsv_close(GcsvWriter *w)
{
	if (w->fp) {
		fflush(w->fp);
		fclose(w->fp);
		w->fp = NULL;
	}
}
