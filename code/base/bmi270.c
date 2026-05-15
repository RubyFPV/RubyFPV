/*
 * bmi270.c - Minimal BMI260/BMI270 driver.  No threads, no callbacks.
 *
 * Init sequence follows Bosch BMI2 reference and uploads the firmware blob
 * matching the chip ID read from register 0x00.
 */
#include "bmi270.h"
#include "bmi260_config_file.h"
#include "bmi270_config_file.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

/* --- registers (Bosch datasheet bst-bmi270-ds000) ---------------------- */
#define R_CHIP_ID         0x00
#define R_INTERNAL_STATUS 0x21
#define R_ACC_X_LSB       0x0C   /* 12 contiguous bytes: 6 accel + 6 gyro */
#define R_ACC_CONF        0x40
#define R_ACC_RANGE       0x41
#define R_GYR_CONF        0x42
#define R_GYR_RANGE       0x43
#define R_INIT_CTRL       0x59
#define R_INIT_ADDR_0     0x5B
#define R_INIT_ADDR_1     0x5C
#define R_INIT_DATA       0x5E
#define R_PWR_CONF        0x7C
#define R_PWR_CTRL        0x7D
#define R_CMD             0x7E

#define CHIP_ID_BMI270 0x24
#define CHIP_ID_BMI260 0x27
#define INTERNAL_STATUS_INIT_OK 0x01

/* --- I2C primitives (linux/i2c-dev I2C_RDWR ioctl) --------------------- */

static int i2c_w8(int fd, uint8_t a, uint8_t reg, uint8_t v)
{
	uint8_t buf[2] = { reg, v };
	struct i2c_msg m = { .addr = a, .flags = 0, .len = 2, .buf = buf };
	struct i2c_rdwr_ioctl_data x = { .msgs = &m, .nmsgs = 1 };
	return ioctl(fd, I2C_RDWR, &x) < 0 ? -errno : 0;
}

static int i2c_r8(int fd, uint8_t a, uint8_t reg, uint8_t *v)
{
	struct i2c_msg m[2] = {
		{ .addr = a, .flags = 0,        .len = 1, .buf = &reg },
		{ .addr = a, .flags = I2C_M_RD, .len = 1, .buf = v    },
	};
	struct i2c_rdwr_ioctl_data x = { .msgs = m, .nmsgs = 2 };
	return ioctl(fd, I2C_RDWR, &x) < 0 ? -errno : 0;
}

static int i2c_rN(int fd, uint8_t a, uint8_t reg, uint8_t *buf, uint16_t n)
{
	struct i2c_msg m[2] = {
		{ .addr = a, .flags = 0,        .len = 1, .buf = &reg },
		{ .addr = a, .flags = I2C_M_RD, .len = n, .buf = buf  },
	};
	struct i2c_rdwr_ioctl_data x = { .msgs = m, .nmsgs = 2 };
	return ioctl(fd, I2C_RDWR, &x) < 0 ? -errno : 0;
}

/* Block write: [reg, data...] in a single I2C transaction. */
static int i2c_wN(int fd, uint8_t a, uint8_t reg, const uint8_t *data, int n)
{
	uint8_t tmp[1 + 32];
	if (n > 32) return -EINVAL;
	tmp[0] = reg;
	memcpy(tmp + 1, data, n);
	struct i2c_msg m = { .addr = a, .flags = 0, .len = 1 + n, .buf = tmp };
	struct i2c_rdwr_ioctl_data x = { .msgs = &m, .nmsgs = 1 };
	return ioctl(fd, I2C_RDWR, &x) < 0 ? -errno : 0;
}

/* --- ODR / range encoding (datasheet tables 17/19) --------------------- */

static uint8_t odr_bits(int hz)
{
	if (hz <=  25) return 0x06;
	if (hz <=  50) return 0x07;
	if (hz <= 100) return 0x08;
	if (hz <= 200) return 0x09;
	if (hz <= 400) return 0x0A;
	if (hz <= 800) return 0x0B;
	return 0x0C; /* 1600 Hz (accel max is 1.6 kHz, gyro 6.4 kHz) */
}

static uint8_t gyr_range_bits(int dps)
{
	if (dps <=  125) return 0x04;
	if (dps <=  250) return 0x03;
	if (dps <=  500) return 0x02;
	if (dps <= 1000) return 0x01;
	return 0x00; /* 2000 dps */
}

static uint8_t acc_range_bits(int g)
{
	if (g <=  2) return 0x00;
	if (g <=  4) return 0x01;
	if (g <=  8) return 0x02;
	return 0x03;
}

/* --- firmware upload --------------------------------------------------- */

static int upload_firmware_once(int fd, uint8_t a,
                                const uint8_t *config_file, int config_size)
{
	uint8_t s = 0;
	if (i2c_r8(fd, a, R_INTERNAL_STATUS, &s) < 0) return -EIO;
	if ((s & 0x0F) == INTERNAL_STATUS_INIT_OK) return 0; /* already initialized */

	if (i2c_w8(fd, a, R_PWR_CONF,  0x00) < 0) return -EIO;
	usleep(450);
	if (i2c_w8(fd, a, R_INIT_CTRL, 0x00) < 0) return -EIO;
	if (i2c_w8(fd, a, R_INIT_ADDR_0, 0x00) < 0) return -EIO;
	if (i2c_w8(fd, a, R_INIT_ADDR_1, 0x00) < 0) return -EIO;

	if ((config_file == NULL) || (config_size <= 0) || ((config_size % 32) != 0))
		return -EINVAL;

	/* INIT_ADDR is in 16-bit words. When the firmware is split into I2C
	 * chunks, Bosch requires advancing it by bytes_written / 2. */
	for (int i = 0; i < (config_size / 32); i++) {
		uint16_t init_addr = (uint16_t)(i * 16);
		if (i2c_w8(fd, a, R_INIT_ADDR_0, (uint8_t)(init_addr & 0x0F)) < 0)
			return -EIO;
		if (i2c_w8(fd, a, R_INIT_ADDR_1, (uint8_t)(init_addr >> 4)) < 0)
			return -EIO;
		if (i2c_wN(fd, a, R_INIT_DATA, &config_file[i * 32], 32) < 0)
			return -EIO;
	}

	if (i2c_w8(fd, a, R_INIT_CTRL, 0x01) < 0) return -EIO;
	usleep(150000);

	for (int i = 0; i < 10; i++) {
		if (i2c_r8(fd, a, R_INTERNAL_STATUS, &s) < 0) return -EIO;
		if ((s & 0x0F) == INTERNAL_STATUS_INIT_OK) return 0;
		usleep(5000);
	}
	return -EIO;
}

static int upload_firmware(int fd, uint8_t a,
                           const uint8_t *config_file, int config_size)
{
	int rc = upload_firmware_once(fd, a, config_file, config_size);
	if (rc == 0)
		return 0;

	/* Some boards leave BMI2 init in a failed state after a previous owner.
	 * Retry once from a clean device reset before giving up. */
	if (i2c_w8(fd, a, R_CMD, 0xB6) < 0)
		return rc;
	usleep(50000);
	return upload_firmware_once(fd, a, config_file, config_size);
}

/* --- public API -------------------------------------------------------- */

int bmi270_open(BmiCtx *ctx, const char *i2c_dev, uint8_t addr,
                int odr_hz, int gyr_dps, int acc_g)
{
	memset(ctx, 0, sizeof(*ctx));
	int fd = open(i2c_dev, O_RDWR);
	if (fd < 0) return -errno;

	if (ioctl(fd, I2C_SLAVE, addr) < 0) { close(fd); return -errno; }

	uint8_t id = 0;
	if (i2c_r8(fd, addr, R_CHIP_ID, &id) < 0) { close(fd); return -EIO; }

	const uint8_t *config_file = NULL;
	int config_size = 0;
	if (id == CHIP_ID_BMI270) {
		config_file = bmi270_config_file;
		config_size = BMI270_CONFIG_FILE_SIZE;
	}
	else if (id == CHIP_ID_BMI260) {
		config_file = bmi260_config_file;
		config_size = BMI260_CONFIG_FILE_SIZE;
	}
	else {
		fprintf(stderr, "bmi2: chip id 0x%02x != BMI270 0x%02x or BMI260 0x%02x\n",
		        id, CHIP_ID_BMI270, CHIP_ID_BMI260);
		close(fd);
		return -ENODEV;
	}

	int rc = upload_firmware(fd, addr, config_file, config_size);
	if (rc < 0) { close(fd); return rc; }

	uint8_t odr = odr_bits(odr_hz);

	/* Performance mode for both; bandwidth = OSR4 (low-noise).
	 * ACC_CONF: filter_perf=1, BW=OSR4 (0x0A high nibble), odr in low nibble
	 * GYR_CONF: filter_perf=1, noise_perf=1, BW=OSR4, odr in low nibble */
	if (i2c_w8(fd, addr, R_PWR_CTRL, 0x0E) < 0) goto fail; /* acc+gyr+temp */
	if (i2c_w8(fd, addr, R_ACC_CONF,  0xA0 | odr) < 0) goto fail;
	if (i2c_w8(fd, addr, R_ACC_RANGE, acc_range_bits(acc_g)) < 0) goto fail;
	if (i2c_w8(fd, addr, R_GYR_CONF,  0xE0 | odr) < 0) goto fail;
	if (i2c_w8(fd, addr, R_GYR_RANGE, gyr_range_bits(gyr_dps)) < 0) goto fail;
	if (i2c_w8(fd, addr, R_PWR_CONF,  0x02) < 0) goto fail; /* APS off */

	usleep(450);

	ctx->fd = fd;
	ctx->addr = addr;
	ctx->odr_hz = odr_hz;
	ctx->gyro_range_dps = gyr_dps;
	ctx->accel_range_g = acc_g;
	return 0;

fail:
	close(fd);
	return -EIO;
}

int bmi270_read(const BmiCtx *ctx, BmiRaw *out)
{
	uint8_t b[12];
	if (i2c_rN(ctx->fd, ctx->addr, R_ACC_X_LSB, b, 12) < 0) return -EIO;

	/* int16 little-endian, sign-extended by the C cast. */
	out->ax = (int16_t)((uint16_t)b[1]  << 8 | b[0]);
	out->ay = (int16_t)((uint16_t)b[3]  << 8 | b[2]);
	out->az = (int16_t)((uint16_t)b[5]  << 8 | b[4]);
	out->gx = (int16_t)((uint16_t)b[7]  << 8 | b[6]);
	out->gy = (int16_t)((uint16_t)b[9]  << 8 | b[8]);
	out->gz = (int16_t)((uint16_t)b[11] << 8 | b[10]);
	return 0;
}

void bmi270_close(BmiCtx *ctx)
{
	if (ctx->fd <= 0) return;
	i2c_w8(ctx->fd, ctx->addr, R_PWR_CTRL, 0x00);
	i2c_w8(ctx->fd, ctx->addr, R_PWR_CONF, 0x03);
	close(ctx->fd);
	ctx->fd = -1;
}
