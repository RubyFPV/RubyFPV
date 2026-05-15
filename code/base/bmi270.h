/*
 * bmi270.h - minimal BMI270 I2C driver for Gyroflow logging.
 *
 * Standalone: no pthread, no callbacks, single-threaded polled reads.
 * Designed to run alongside Majestic + Ruby on OpenIPC SigmaStar cameras
 * without competing for CPU.
 */
#ifndef BMI270_H
#define BMI270_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int      fd;              /* I2C fd (set by bmi270_open) */
	uint8_t  addr;            /* 0x68 or 0x69 */
	int      gyro_range_dps;  /* 125/250/500/1000/2000 */
	int      accel_range_g;   /* 2/4/8/16 */
	int      odr_hz;          /* 25/50/100/200/400/800/1600 */
} BmiCtx;

/* Six int16 raw samples (LSB units), 2's complement.  Apply scale on host. */
typedef struct {
	int16_t  ax, ay, az;
	int16_t  gx, gy, gz;
} BmiRaw;

/* Open /dev/i2c-N, verify chip-id, upload 8 KB config, configure ODR/range.
 * Polls INTERNAL_STATUS until init bit reports success.
 * Returns 0 on success, negative errno-like on failure. */
int bmi270_open(BmiCtx *ctx, const char *i2c_dev, uint8_t addr,
                int odr_hz, int gyro_range_dps, int accel_range_g);

/* One 12-byte burst read of ACC_X_LSB..GYR_Z_MSB (regs 0x0C..0x17).
 * Parses little-endian 2's complement int16.  ~1 ioctl per call. */
int bmi270_read(const BmiCtx *ctx, BmiRaw *out);

/* Power down sensor, close fd. */
void bmi270_close(BmiCtx *ctx);

#ifdef __cplusplus
}
#endif

#endif
