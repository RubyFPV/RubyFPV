/*
 * imu_sender.c - see imu_sender.h.
 *
 * Threading model:
 *   - One pthread reads BMI260/BMI270 at the configured ODR via clock_nanosleep
 *     TIMER_ABSTIME (low jitter).
 *   - Producer pushes each sample into a fixed-size ring buffer under a
 *     mutex.
 *   - Consumer (drain_and_send, called from ruby_tx_telemetry main loop)
 *     pops up to IMU_PKT_MAX_SAMPLES at a time, packs them into a
 *     PACKET_TYPE_RUBY_TELEMETRY_IMU_DOWNLOAD packet, sends via IPC.
 *
 * Defaults:
 *   - I2C bus  : /dev/i2c-1     (overridden by env RUBY_IMU_I2C)
 *   - Address  : 0x68           (overridden by env RUBY_IMU_ADDR)
 *   - ODR      : 400 Hz         (overridden by env RUBY_IMU_RATE)
 *   - Gyro     : +/- 2000 dps   (overridden by env RUBY_IMU_GYR_DPS)
 *   - Accel    : +/- 4 g        (overridden by env RUBY_IMU_ACC_G)
 *
 * Environment variables let advanced users tune without rebuilding;
 * defaults are sensible for Gyroflow stabilization of FPV footage.
 */
#define _GNU_SOURCE
#include "imu_sender.h"

#include "../base/bmi270.h"
#include "../base/base.h"
#include "../base/ruby_ipc.h"
#include "../radio/radiopackets2.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* --- configuration ----------------------------------------------------- */

#define IMU_RING_CAP        2048   /* > 5 s of headroom @ 400 Hz */
#define IMU_PKT_MAX_SAMPLES 16     /* per t_packet_header_imu_download.n_samples u8 */

/* --- ring buffer ------------------------------------------------------- */

typedef struct {
	uint64_t t_us;
	int16_t  gx, gy, gz;
	int16_t  ax, ay, az;
} ImuSlot;

static ImuSlot          s_ring[IMU_RING_CAP];
static unsigned         s_head;
static unsigned         s_tail;
static pthread_mutex_t  s_lock = PTHREAD_MUTEX_INITIALIZER;

/* --- module state ------------------------------------------------------ */

static int        s_enabled;          /* 1 if BMI2 IMU opened, 0 otherwise */
static BmiCtx     s_bmi;
static pthread_t  s_thread;
static int        s_thread_started;
static volatile int s_running;

static int        s_ipc_fd;
static uint32_t   s_vehicle_id;

static uint32_t   s_segment_index;
static int        s_odr_hz;
static int        s_gyr_dps;
static int        s_acc_g;
static uint8_t    s_scale_codes;

/* --- helpers ----------------------------------------------------------- */

static uint64_t mono_us(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)(ts.tv_nsec / 1000);
}

static uint8_t gyr_code(int dps)
{
	if (dps <= 125)  return 0;
	if (dps <= 250)  return 1;
	if (dps <= 500)  return 2;
	if (dps <= 1000) return 3;
	return 4;
}

static uint8_t acc_code(int g)
{
	if (g <= 2) return 0;
	if (g <= 4) return 1;
	if (g <= 8) return 2;
	return 3;
}

static int env_int(const char *name, int def)
{
	const char *v = getenv(name);
	if (!v || !*v) return def;
	int n = atoi(v);
	return n > 0 ? n : def;
}

/* --- reader thread ----------------------------------------------------- */

static void *imu_reader(void *arg)
{
	(void)arg;
	long period_ns = 1000000000L / s_odr_hz;
	struct timespec next;
	clock_gettime(CLOCK_MONOTONIC, &next);

	while (s_running) {
		BmiRaw r;
		if (bmi270_read(&s_bmi, &r) == 0) {
			uint64_t t = mono_us();
			pthread_mutex_lock(&s_lock);
			s_ring[s_head].t_us = t;
			s_ring[s_head].gx = r.gx; s_ring[s_head].gy = r.gy; s_ring[s_head].gz = r.gz;
			s_ring[s_head].ax = r.ax; s_ring[s_head].ay = r.ay; s_ring[s_head].az = r.az;
			unsigned next_head = (s_head + 1) % IMU_RING_CAP;
			if (next_head == s_tail)
				s_tail = (s_tail + 1) % IMU_RING_CAP;  /* drop oldest */
			s_head = next_head;
			pthread_mutex_unlock(&s_lock);
		}

		next.tv_nsec += period_ns;
		while (next.tv_nsec >= 1000000000L) {
			next.tv_nsec -= 1000000000L;
			next.tv_sec++;
		}
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
	}
	return NULL;
}

/* --- public API -------------------------------------------------------- */

int imu_sender_init(int ipc_fd, uint32_t vehicle_id)
{
	s_enabled = 0;
	s_ipc_fd = ipc_fd;
	s_vehicle_id = vehicle_id;
	s_head = s_tail = 0;
	s_segment_index = 0;

	const char *i2c   = getenv("RUBY_IMU_I2C");   if (!i2c || !*i2c)   i2c = "/dev/i2c-1";
	const char *addrs = getenv("RUBY_IMU_ADDR");
	uint8_t addr = (addrs && *addrs) ? (uint8_t)strtol(addrs, NULL, 0) : 0x68;
	s_odr_hz  = env_int("RUBY_IMU_RATE", 400);
	s_gyr_dps = env_int("RUBY_IMU_GYR_DPS", 2000);
	s_acc_g   = env_int("RUBY_IMU_ACC_G", 4);

	int rc = bmi270_open(&s_bmi, i2c, addr, s_odr_hz, s_gyr_dps, s_acc_g);
	if (rc < 0) {
		log_line("[IMU] BMI2 IMU not detected on %s @ 0x%02x (err %d); gyro logging disabled",
		         i2c, addr, -rc);
		return 0;
	}

	s_scale_codes = (uint8_t)((gyr_code(s_gyr_dps) << 4) | acc_code(s_acc_g));

	s_running = 1;
	if (pthread_create(&s_thread, NULL, imu_reader, NULL) != 0) {
		log_softerror_and_alarm("[IMU] failed to start reader thread: %s",
		                        strerror(errno));
		s_running = 0;
		bmi270_close(&s_bmi);
		return 0;
	}
	s_thread_started = 1;

	s_enabled = 1;
	log_line("[IMU] BMI2 IMU active: %s @ 0x%02x, ODR=%d Hz, gyro=+/-%d dps, accel=+/-%d g",
	         i2c, addr, s_bmi.odr_hz, s_bmi.gyro_range_dps, s_bmi.accel_range_g);
	return 1;
}

int imu_sender_drain_and_send(void)
{
	if (!s_enabled) return 0;

	/* Pop up to IMU_PKT_MAX_SAMPLES samples into a local array. */
	ImuSlot local[IMU_PKT_MAX_SAMPLES];
	int n = 0;
	pthread_mutex_lock(&s_lock);
	while (n < IMU_PKT_MAX_SAMPLES && s_tail != s_head) {
		local[n++] = s_ring[s_tail];
		s_tail = (s_tail + 1) % IMU_RING_CAP;
	}
	pthread_mutex_unlock(&s_lock);

	if (n == 0) return 0;

	/* Build packet: t_packet_header + t_packet_header_imu_download + samples. */
	uint8_t buf[MAX_PACKET_TOTAL_SIZE];
	t_packet_header PH;
	radio_packet_init(&PH, PACKET_COMPONENT_TELEMETRY,
	                  PACKET_TYPE_RUBY_TELEMETRY_IMU_DOWNLOAD, STREAM_ID_DATA2);
	PH.vehicle_id_src  = s_vehicle_id;
	PH.vehicle_id_dest = 0;
	PH.total_length = (u16)(sizeof(t_packet_header)
	                       + sizeof(t_packet_header_imu_download)
	                       + (size_t)n * sizeof(t_imu_sample_compact));

	t_packet_header_imu_download imuH;
	imuH.imu_segment_index = s_segment_index++;
	imuH.t_us_base         = (uint32_t)(local[0].t_us & 0xFFFFFFFFu);
	imuH.scale_codes       = s_scale_codes;
	imuH.n_samples         = (uint8_t)n;
	imuH.reserved          = 0;

	uint8_t *p = buf;
	memcpy(p, &PH, sizeof(PH));                          p += sizeof(PH);
	memcpy(p, &imuH, sizeof(imuH));                      p += sizeof(imuH);

	uint64_t prev_t = local[0].t_us;
	for (int i = 0; i < n; i++) {
		t_imu_sample_compact s;
		uint64_t dt = (i == 0) ? 0 : (local[i].t_us - prev_t);
		if (dt > 65535) dt = 65535;
		s.dt_us = (uint16_t)dt;
		s.gx = local[i].gx; s.gy = local[i].gy; s.gz = local[i].gz;
		s.ax = local[i].ax; s.ay = local[i].ay; s.az = local[i].az;
		memcpy(p, &s, sizeof(s));
		p += sizeof(s);
		prev_t = local[i].t_us;
	}

	int sent = ruby_ipc_channel_send_message(s_ipc_fd, buf, PH.total_length);
	if (sent != (int)PH.total_length)
		log_softerror_and_alarm("[IMU] short IPC send to router: %d/%d",
		                        sent, (int)PH.total_length);
	return n;
}

void imu_sender_shutdown(void)
{
	if (!s_enabled) return;
	s_running = 0;
	if (s_thread_started) {
		pthread_join(s_thread, NULL);
		s_thread_started = 0;
	}
	bmi270_close(&s_bmi);
	s_enabled = 0;
	log_line("[IMU] sender shutdown");
}
