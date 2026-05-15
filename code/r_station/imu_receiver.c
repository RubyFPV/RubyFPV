/*
 * imu_receiver.c - see imu_receiver.h.
 *
 * Strategy:
 *  - Open a rolling <media_dir>/_current.gcsv at init using fallback scale
 *    codes; reopen with proper scales as soon as the first IMU packet
 *    arrives carrying real scale info.
 *  - Each PACKET_TYPE_RUBY_TELEMETRY_IMU_DOWNLOAD carries:
 *      t_packet_header
 *      t_packet_header_imu_download (segment, t_us_base, scale, n_samples)
 *      n_samples * t_imu_sample_compact
 *    We append rows to the gcsv with absolute-microsecond t built from
 *    t_us_base + accumulated dt_us.
 *  - imu_receiver_periodic() scans media_dir once.  When a new
 *    video-*.{h265,h264,mp4} appears that we haven't seen, _current.gcsv
 *    is renamed to <basename>.gcsv and a fresh _current.gcsv opens.
 */
#define _GNU_SOURCE
#include "imu_receiver.h"

#include "../base/base.h"
#include "../base/gcsv.h"
#include "../radio/radiopackets2.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* --- module state ------------------------------------------------------ */

#define IMU_RECV_MAX_SEEN 128       /* track up to N recent video basenames */
#define IMU_RECENT_MAX_SAMPLES 48000 /* 120 s at 400 Hz */

static char        s_media_dir[512];
static GcsvWriter  s_gcsv;
static int         s_gcsv_open;
static char        s_current_path[640];
static int         s_gyr_dps = 2000;
static int         s_acc_g   = 4;
static int         s_scale_latched;
static uint64_t    s_samples_written;
static uint32_t    s_last_segment = UINT32_MAX;
static uint64_t    s_drops;

typedef struct {
	uint64_t t_us;
	int16_t gx, gy, gz;
	int16_t ax, ay, az;
} ImuRecentSample;

static ImuRecentSample s_recent[IMU_RECENT_MAX_SAMPLES];
static unsigned s_recent_head;
static unsigned s_recent_count;

/* seen basenames (kept in a tiny FIFO ring) */
static char  s_seen[IMU_RECV_MAX_SEEN][96];
static int   s_seen_count;
static int   s_seen_head;

static int  s_initialized;

/* --- helpers ----------------------------------------------------------- */

static int gyr_decode(uint8_t c)
{
	static const int t[5] = { 125, 250, 500, 1000, 2000 };
	return (c < 5) ? t[c] : 2000;
}
static int acc_decode(uint8_t c)
{
	static const int t[4] = { 2, 4, 8, 16 };
	return (c < 4) ? t[c] : 16;
}

static int is_video_name(const char *name, char *base, size_t cap)
{
	if (strncmp(name, "video-", 6) != 0) return 0;
	size_t n = strlen(name);
	const char *exts[] = { ".h265", ".h264", ".mp4", NULL };
	for (int i = 0; exts[i]; i++) {
		size_t el = strlen(exts[i]);
		if (n > el && !strcasecmp(name + n - el, exts[i])) {
			size_t bl = n - el;
			if (bl + 1 > cap) return 0;
			memcpy(base, name, bl);
			base[bl] = '\0';
			return 1;
		}
	}
	return 0;
}

static int seen_basename(const char *base)
{
	for (int i = 0; i < s_seen_count; i++)
		if (!strcmp(s_seen[i], base)) return 1;
	return 0;
}

static void remember_basename(const char *base)
{
	int slot = (s_seen_head + s_seen_count) % IMU_RECV_MAX_SEEN;
	if (s_seen_count < IMU_RECV_MAX_SEEN) {
		s_seen_count++;
	} else {
		s_seen_head = (s_seen_head + 1) % IMU_RECV_MAX_SEEN;
	}
	strncpy(s_seen[slot], base, sizeof(s_seen[slot]) - 1);
	s_seen[slot][sizeof(s_seen[slot]) - 1] = '\0';
}

static int open_current_gcsv(void)
{
	if (s_gcsv_open) { gcsv_close(&s_gcsv); s_gcsv_open = 0; }
	snprintf(s_current_path, sizeof(s_current_path),
	         "%s/_current.gcsv", s_media_dir);
	if (gcsv_open(&s_gcsv, s_current_path, s_gyr_dps, s_acc_g,
	              "bmi270-rubyfpv", "XYZ") < 0) {
		log_softerror_and_alarm("[IMU] gcsv_open(%s) failed: %s",
		                        s_current_path, strerror(errno));
		return -1;
	}
	s_gcsv_open = 1;
	s_samples_written = 0;
	log_line("[IMU] writing %s (gyro=+/-%d dps, accel=+/-%d g)",
	         s_current_path, s_gyr_dps, s_acc_g);
	return 0;
}

static int parse_srt_time_ms(const char *p)
{
	int h = 0, m = 0, s = 0, ms = 0;
	if (4 != sscanf(p, "%d:%d:%d,%d", &h, &m, &s, &ms))
		return -1;
	return ((h * 60 + m) * 60 + s) * 1000 + ms;
}

static uint32_t read_clip_duration_ms(const char *basename)
{
	char path[640];
	char line[256];
	uint32_t duration_ms = 0;

	snprintf(path, sizeof(path), "%s/%s.srt", s_media_dir, basename);
	FILE *fd = fopen(path, "r");
	if (fd) {
		while (fgets(line, sizeof(line), fd)) {
			char *arrow = strstr(line, "-->");
			if (!arrow) continue;
			int ms = parse_srt_time_ms(arrow + 3);
			if (ms > 0 && (uint32_t)ms > duration_ms)
				duration_ms = (uint32_t)ms;
		}
		fclose(fd);
	}
	if (duration_ms > 0)
		return duration_ms;

	snprintf(path, sizeof(path), "%s/%s.info", s_media_dir, basename);
	fd = fopen(path, "r");
	if (fd) {
		char src[256];
		int fps = 0, duration_s = 0;
		if (fscanf(fd, "%255s %d %d", src, &fps, &duration_s) == 3 && duration_s > 0)
			duration_ms = (uint32_t)duration_s * 1000U;
		fclose(fd);
	}
	return duration_ms;
}

static void remember_sample(uint64_t t_us, const t_imu_sample_compact *sample)
{
	s_recent[s_recent_head].t_us = t_us;
	s_recent[s_recent_head].gx = sample->gx;
	s_recent[s_recent_head].gy = sample->gy;
	s_recent[s_recent_head].gz = sample->gz;
	s_recent[s_recent_head].ax = sample->ax;
	s_recent[s_recent_head].ay = sample->ay;
	s_recent[s_recent_head].az = sample->az;
	s_recent_head = (s_recent_head + 1) % IMU_RECENT_MAX_SAMPLES;
	if (s_recent_count < IMU_RECENT_MAX_SAMPLES)
		s_recent_count++;
}

static int write_recent_gcsv(const char *path, uint32_t duration_ms)
{
	if (s_recent_count == 0 || duration_ms == 0)
		return -1;

	unsigned last = (s_recent_head + IMU_RECENT_MAX_SAMPLES - 1) % IMU_RECENT_MAX_SAMPLES;
	uint64_t end_us = s_recent[last].t_us;
	uint64_t start_us = (end_us > (uint64_t)duration_ms * 1000ULL) ?
		end_us - (uint64_t)duration_ms * 1000ULL : 0;

	GcsvWriter out;
	if (gcsv_open(&out, path, s_gyr_dps, s_acc_g, "bmi270-rubyfpv", "XYZ") < 0)
		return -1;

	unsigned written = 0;
	unsigned first = (s_recent_head + IMU_RECENT_MAX_SAMPLES - s_recent_count) % IMU_RECENT_MAX_SAMPLES;
	for (unsigned i = 0; i < s_recent_count; i++) {
		unsigned idx = (first + i) % IMU_RECENT_MAX_SAMPLES;
		if (s_recent[idx].t_us < start_us)
			continue;
		gcsv_write(&out, s_recent[idx].t_us,
		           s_recent[idx].gx, s_recent[idx].gy, s_recent[idx].gz,
		           s_recent[idx].ax, s_recent[idx].ay, s_recent[idx].az);
		written++;
	}
	gcsv_close(&out);
	log_line("[IMU] paired recent window -> %s (%u samples, %u ms)",
	         path, written, duration_ms);
	return written > 0 ? 0 : -1;
}

static void rotate_to_basename(const char *basename)
{
	if (!s_gcsv_open) return;
	char old_path[640];
	strncpy(old_path, s_current_path, sizeof(old_path));
	old_path[sizeof(old_path) - 1] = '\0';

	gcsv_close(&s_gcsv);
	s_gcsv_open = 0;

	char new_path[640];
	snprintf(new_path, sizeof(new_path), "%s/%s.gcsv", s_media_dir, basename);
	uint32_t duration_ms = read_clip_duration_ms(basename);
	if (duration_ms > 0 && write_recent_gcsv(new_path, duration_ms) == 0) {
		unlink(old_path);
	} else if (rename(old_path, new_path) == 0) {
		log_line("[IMU] paired fallback -> %s (%llu samples)",
		         new_path, (unsigned long long)s_samples_written);
	} else {
		log_softerror_and_alarm("[IMU] rename %s -> %s failed: %s",
		                        old_path, new_path, strerror(errno));
	}
	open_current_gcsv();
}

/* --- public API -------------------------------------------------------- */

int imu_receiver_init(const char *media_dir)
{
	if (!media_dir || !*media_dir) return -1;
	memset(s_seen, 0, sizeof(s_seen));
	s_seen_count = s_seen_head = 0;
	s_scale_latched = 0;
	s_last_segment = UINT32_MAX;
	s_drops = 0;
	s_recent_head = 0;
	s_recent_count = 0;
	strncpy(s_media_dir, media_dir, sizeof(s_media_dir) - 1);
	s_media_dir[sizeof(s_media_dir) - 1] = '\0';

	mkdir(s_media_dir, 0755); /* harmless if exists */

	if (open_current_gcsv() < 0) return -1;

	/* Pre-populate seen-set with anything already present so we don't
	 * spuriously rotate when the first scan happens. */
	DIR *d = opendir(s_media_dir);
	if (d) {
		struct dirent *de;
		while ((de = readdir(d)) != NULL) {
			char base[96];
			if (is_video_name(de->d_name, base, sizeof base))
				remember_basename(base);
		}
		closedir(d);
	}

	s_initialized = 1;
	log_line("[IMU] receiver watching %s", s_media_dir);
	return 0;
}

void imu_receiver_on_packet(const uint8_t *buf, int total_length)
{
	if (!s_initialized) return;
	if (total_length < (int)(sizeof(t_packet_header) + sizeof(t_packet_header_imu_download)))
		return;

	const t_packet_header_imu_download *h =
		(const t_packet_header_imu_download *)(buf + sizeof(t_packet_header));
	int n = h->n_samples;
	if (n < 1 || n > 16) return;

	int expected = (int)(sizeof(t_packet_header)
	                   + sizeof(t_packet_header_imu_download)
	                   + (size_t)n * sizeof(t_imu_sample_compact));
	if (total_length < expected) return;

	/* Latch scales on first packet, ignore mid-stream changes. */
	int gyr = gyr_decode((h->scale_codes >> 4) & 0x0F);
	int acc = acc_decode(h->scale_codes & 0x0F);
	if (!s_scale_latched) {
		s_scale_latched = 1;
		if ((gyr != s_gyr_dps || acc != s_acc_g) && s_gcsv_open && s_samples_written == 0) {
			s_gyr_dps = gyr;
			s_acc_g   = acc;
			open_current_gcsv();
		}
	} else if (gyr != s_gyr_dps || acc != s_acc_g) {
		log_softerror_and_alarm("[IMU] mid-stream scale code change ignored "
		                        "(have %d/%d, got %d/%d)",
		                        s_gyr_dps, s_acc_g, gyr, acc);
	}

	/* Drop detection. */
	if (s_last_segment != UINT32_MAX) {
		uint32_t expect = s_last_segment + 1;
		if (h->imu_segment_index != expect)
			s_drops += (uint32_t)(h->imu_segment_index - expect);
	}
	s_last_segment = h->imu_segment_index;

	if (!s_gcsv_open) return;

	const t_imu_sample_compact *samples =
		(const t_imu_sample_compact *)(buf + sizeof(t_packet_header)
		                              + sizeof(t_packet_header_imu_download));
	uint64_t t_us = h->t_us_base;
	for (int i = 0; i < n; i++) {
		if (i > 0) t_us += samples[i].dt_us;
		remember_sample(t_us, &samples[i]);
		gcsv_write(&s_gcsv, t_us,
		           samples[i].gx, samples[i].gy, samples[i].gz,
		           samples[i].ax, samples[i].ay, samples[i].az);
		s_samples_written++;
	}
}

void imu_receiver_periodic(void)
{
	if (!s_initialized) return;

	/* Self-throttle to ~1 Hz: caller is in a fast main loop (50 ms). */
	static uint64_t s_last_scan_us;
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	uint64_t now_us = (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)(ts.tv_nsec / 1000);
	if (now_us - s_last_scan_us < 1000000ULL) return;
	s_last_scan_us = now_us;

	DIR *d = opendir(s_media_dir);
	if (!d) return;

	struct dirent *de;
	while ((de = readdir(d)) != NULL) {
		char base[96];
		if (!is_video_name(de->d_name, base, sizeof base)) continue;
		if (seen_basename(base)) continue;
		remember_basename(base);
		rotate_to_basename(base);
	}
	closedir(d);
}

void imu_receiver_shutdown(void)
{
	if (!s_initialized) return;
	if (s_gcsv_open) {
		gcsv_close(&s_gcsv);
		s_gcsv_open = 0;
		log_line("[IMU] receiver shutdown, %llu samples written, %llu drops",
		         (unsigned long long)s_samples_written,
		         (unsigned long long)s_drops);
	}
	s_initialized = 0;
}
