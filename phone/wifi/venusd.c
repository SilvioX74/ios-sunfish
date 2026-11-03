/*
 * venusd: video stage 4a (docs/features/video.md "Stage 4 spec", research/notes/venus-stage4-design-2026-11-03.md):
 * the root daemon that owns the Venus video engine for clients without /dev/pmem. Protocol: venus_proto.h.
 *   venusd [--log FILE]    default /var/local/venusd.log (cut to 0 past 1 MB; all of venus_core's output), one line
 *                          per bring-up, shutdown and session also to /dev/console (the live log)
 * Venus code: venus_core.c, venusctl's steps in tools/sunfish-venus-test.sh decode's order: power on (with the CVP
 * domain, as the script's --cvp), boot --keep, init --phys VENUSD_PHYS, then per session the decoder steps with the
 * buffer window right after the queue block (VENUSD_BUF_MB, 160: the v4a0 memtest window 0xa5500000+160 MB). Down
 * (idle VENUSD_IDLE_S = 60 s, SIGTERM, a firmware sys error) = venusctl shutdown (PAS shutdown first), then power
 * off; a failed shutdown is LEFT POWERED: no power off (a collapse under live firmware hangs the bus), venusd stays
 * up refusing OPEN with -EIO until a reboot. venus_core's STATE file is the record: a STATE left by a venusd that
 * died (a STOP in venus_core exits) is undone at the next start. Don't run venusctl while venusd runs (same STATE).
 * Polling, no IRQ: the message queue every 1 ms while a session runs, nothing while idle. One session at a time
 * (a second OPEN: -EBUSY). Frames go back over the socket (copy-out through the kernel160 sweetpmemwc=2 mapping);
 * IOSurface hand-off is 4b.
 * Switches: /var/local/venus-off (OPEN -> -ENODEV), VENUSD_VERBOSE=1 (venusctl's per-buffer lines in the log),
 * VENUSD_IDLE_S, VENUSD_PHYS (0xa5100000), VENUSD_BUF_MB (160), VENUSD_IN_KB (input buffer floor, 1024),
 * VENUSD_DEBUG (firmware debug mask, 0x18); the VENUSCTL_* decoder switches work as in venusctl.
 * Cold start (vdcold, video.md vdboot: the first decode after boot stalls up to ~13 s): until a session's first
 * picture, socket reads/writes that would block and the 3 s no-progress rule get VENUSD_COLD_S (20) instead of 2/3 s
 * (0: the old rule). VENUSD_WARMUP=1 (default): right after the socket is up, power Venus, decode one built-in
 * 320x240 IDR, power off again, so the first-use stall happens at boot, not in Safari (0: off).
 */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include "pmem.h"
#include "venus_core.h"
#include "venus_proto.h"

#define LOG_MAX		(1 << 20)
#define MAXCL		4
#ifndef LOCKF	/* -D for a host test build */
#define LOCKF		"/var/run/venusd.pid"
#define OFF_FILE	"/var/local/venus-off"
#endif

enum { S_NONE, S_OPEN, S_RUN, S_EOS, S_DONE, S_FAILED };	/* the session (owner connection) */
static const char *const sname[] = { "none", "open", "running", "eos sent", "eos done", "failed" };

static struct client { int fd, gone; } cl[MAXCL];	/* gone: a write failed, dropped by the main loop */
static int owner = -1, sess = S_NONE, hfi, up, fault, logfd = -1, confd = -1;
static volatile sig_atomic_t term;
static const char *fw;
static uint64_t phys = 0xa5100000ull, buf_mb = 160;
static uint32_t debug = 0x18, in_min = 1024u << 10;
static unsigned idle_s = 60, nsess;
static double idle_since, t_open, t_first, t_last_frame, t_eos, t_prog;
static uint32_t sent, hint_w, hint_h;
static unsigned last_acts;
static uint8_t *au;
static unsigned cold_s = 20;	/* vdcold: VENUSD_COLD_S */

/* vdcold: the warm-up stream, one 320x240 grey IDR (SPS, PPS, slice; ffmpeg color=gray, libx264 constrained baseline,
 * SEI removed; 320x240 is a size stage 3 decoded bit-exact) */
static const uint8_t warm_au[] = {
	0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xc0, 0x0d, 0xdc, 0x14, 0x1f, 0xb0, 0x11, 0x00, 0x00, 0x03, 0x00, 0x01, 0x00,
	0x00, 0x03, 0x00, 0x3c, 0x0f, 0x14, 0x2b, 0x80, 0x00, 0x00, 0x00, 0x01, 0x68, 0xce, 0x0f, 0x2c, 0x80, 0x00, 0x00,
	0x01, 0x65, 0x88, 0x84, 0x04, 0xbc, 0x98, 0xa0, 0x00, 0x20, 0x2f, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27,
	0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75,
	0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7,
	0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d,
	0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75,
	0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7,
	0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d,
	0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75,
	0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7,
	0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d,
	0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75,
	0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7,
	0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x80
};

static double mono(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* one line to the log (stdout) and the live log (/dev/console, as the bridge's sm2 lines) */
static void note(const char *fmt, ...)
{
	char s[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(s, sizeof s, fmt, ap);
	va_end(ap);
	printf("%s\n", s);
	if (confd < 0)
		confd = open("/dev/console", O_WRONLY | O_NOCTTY | O_NONBLOCK);
	if (confd >= 0)
		dprintf(confd, "%s\n", s);
}

/* vdcold: how long a socket read/write may wait in all: the cold-start allowance until the session's first picture
 * (the first decode after boot stalls), else the 2 s of SO_SNDTIMEO/SO_RCVTIMEO */
static double io_limit(void)
{
	int cold = owner >= 0 && (sess == S_OPEN || sess == S_RUN || sess == S_EOS) && !t_first;
	return cold && cold_s > 2 ? cold_s : 2;
}

/* partial transfers and EAGAIN (the 2 s socket timeouts) retried until io_limit() has passed */
static int wr_full(int fd, const void *b, size_t n)
{
	const uint8_t *p = b;
	double t0 = mono(), lim = io_limit();
	while (n) {
		ssize_t k = write(fd, p, n);
		if (k < 0 && errno == EINTR)
			continue;
		if (k < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) && mono() - t0 < lim) {
			struct pollfd q = { .fd = fd, .events = POLLOUT };
			poll(&q, 1, 100);
			continue;
		}
		if (k <= 0)
			return -1;
		p += k, n -= (size_t)k;
	}
	return 0;
}

static int rd_full(int fd, void *b, size_t n)
{
	uint8_t *p = b;
	double t0 = mono(), lim = io_limit();
	while (n) {
		ssize_t k = read(fd, p, n);
		if (k < 0 && errno == EINTR)
			continue;
		if (k < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) && mono() - t0 < lim) {
			struct pollfd q = { .fd = fd, .events = POLLIN };
			poll(&q, 1, 100);
			continue;
		}
		if (k <= 0)
			return -1;
		p += k, n -= (size_t)k;
	}
	return 0;
}

static void send_msg(int i, struct venusd_msg *m, const void *payload)
{
	if (i < 0 || cl[i].fd < 0 || cl[i].gone)
		return;
	m->magic = VENUSD_MAGIC;
	m->version = VENUSD_VERSION;
	/* sink() runs inside venus_core's queue loop: no session teardown from here, the main loop drops it */
	if (wr_full(cl[i].fd, m, sizeof *m) || (m->len && wr_full(cl[i].fd, payload, m->len))) {
		printf("venusd: client fd %d: write failed (%s): dropping it\n", cl[i].fd, strerror(errno));
		cl[i].gone = 1;
	}
}

static void reply(int i, int type, int32_t status, uint32_t seq)
{
	struct vdec_status st;
	vdec_status(&st);
	struct venusd_msg m = { .type = (uint16_t)type, .status = status, .seq = seq,
		.frames = i == owner ? (uint32_t)st.frames : 0 };
	send_msg(i, &m, NULL);
}

/* the decoder's frames (venus_core frame_out) -> DECODED to the session's client */
static void sink(const struct vdec_frame *f)
{
	struct venusd_msg m = { .type = VD_DECODED, .len = (uint32_t)f->len, .pts = f->ts, .flags = f->flags,
		.width = (uint32_t)f->cw, .height = (uint32_t)f->ch, .crop_x = (uint32_t)f->cx, .crop_y = (uint32_t)f->cy,
		.stride = f->stride, .scanlines = f->scanlines, .filled = f->filled, .pic_type = f->pic_type,
		.seq = (uint32_t)f->index, .fbd_w = f->fbd_w, .fbd_h = f->fbd_h, .fbd_x = f->fbd_x, .fbd_y = f->fbd_y,
		.copy_us = (uint32_t)(f->copy_ms * 1e3) };
	if (!f->index)
		t_first = mono();
	t_last_frame = mono();
	send_msg(owner, &m, f->buf);
}

/* ---- Venus up/down: tools/sunfish-venus-test.sh decode's steps and its LEFT POWERED rule ---- */

static void down(const char *why)
{
	if (fault)
		return;
	note("venusd: Venus down (%s)", why);
	if (shutdown_cmd()) {
		fault = 1;
		note("venusd: shutdown FAILED: no power off (the firmware may still run): LEFT POWERED, reboot needed");
		return;
	}
	if (power_off())
		note("venusd: power off reported a failure (see %s)", "the venusd log");
	up = 0;
	vdec_fw_reset();
}

static int bring_up(void)
{
	double t0 = mono();
	struct stat sb;
	cvp = 1;	/* the test script's --cvp on every power on/off (pre-run review 8.5) */
	if (power_on()) {	/* undoes itself */
		note("venusd: power on failed");
		return -1;
	}
	if (boot(fw, 1) || init(phys)) {
		note("venusd: boot/init failed: undoing");
		down("bring-up failed");
		return -1;
	}
	if (stat(STATE, &sb)) {
		note("venusd: no %s after init: refusing to go on", STATE);
		return -1;
	}
	up = 1;
	note("venusd: Venus up in %.0f ms (firmware %s, queues %#llx)", 1e3 * (mono() - t0), fw, (unsigned long long)phys);
	return 0;
}

/* ---- the session ---- */

static void sess_log(const char *how)
{
	struct vdec_status st;
	vdec_status(&st);
	double span = t_last_frame > t_first ? t_last_frame - t_first : 0;
	note("venusd: session %u %s: %u AUs in, %d frames, %.2f s from OPEN, first frame %.0f ms after it, %.1f fps"
	     " (frame 0 -> %d), copy-out mean %.2f ms max %.2f ms, error %#x%s", nsess, how, sent, st.frames,
	     mono() - t_open, st.frames ? 1e3 * (t_first - t_open) : 0.0, st.frames > 1 && span > 0 ? (st.frames - 1) / span : 0.0,
	     st.frames ? st.frames - 1 : 0, st.frames ? 1e3 * st.cp_sum / st.frames : 0.0, 1e3 * st.cp_max, st.err,
	     st.fw_dead ? ", firmware sys error" : "");
}

/* the HFI session down (if it started), the firmware too after a sys error */
static void sess_stop(void)
{
	struct vdec_status st;
	int bad = hfi && vdec_close();
	hfi = 0;
	vdec_status(&st);
	if (st.fw_dead)
		down("firmware sys error");
	else if (bad)	/* a half-closed firmware session would refuse the next SESSION_INIT: start clean */
		down("session close incomplete");
}

static void sess_fail(int32_t status, uint32_t seq)
{
	printf("venusd: session %u failed: status %d (%#x) at frame %u\n", nsess, status, (unsigned)status, seq);
	if (owner >= 0)
		reply(owner, VD_ERROR, status, seq);
	sess_stop();
	sess_log("failed");
	sess = S_FAILED;
}

static void sess_end(const char *how)
{
	if (sess != S_FAILED) {
		sess_stop();
		sess_log(how);
	}
	sess = S_NONE;
	owner = -1;
	idle_since = mono();
}

static void drop_client(int i)
{
	if (cl[i].fd < 0)
		return;
	close(cl[i].fd);
	cl[i].fd = -1;
	cl[i].gone = 0;
	if (i == owner)
		sess_end("closed (client gone)");
}

static void do_open(int i, const struct venusd_msg *m)
{
	struct stat sb;
	int32_t rc = 0;
	if (owner >= 0 && owner != i)
		rc = -EBUSY, printf("venusd: OPEN from fd %d refused: one session at a time (fd %d has it)\n", cl[i].fd,
				  cl[owner].fd);
	else if (owner == i)
		rc = -EINVAL, printf("venusd: second OPEN on fd %d refused (session %s)\n", cl[i].fd, sname[sess]);
	else if (m->codec != VD_CODEC_H264)
		rc = -EINVAL, printf("venusd: OPEN codec %u refused: H.264 (%u) only\n", m->codec, VD_CODEC_H264);
	else if (!stat(OFF_FILE, &sb))
		rc = -ENODEV, printf("venusd: OPEN refused: %s exists\n", OFF_FILE);
	else if (fault)
		rc = -EIO, printf("venusd: OPEN refused: Venus was left powered after a failed shutdown (reboot)\n");
	else if (!up && bring_up())
		rc = -EIO;
	if (!rc) {
		owner = i;
		sess = S_OPEN;
		nsess++;
		sent = 0;
		hint_w = m->width, hint_h = m->height;
		t_open = mono();
		t_first = t_last_frame = 0;
		note("venusd: session %u opened (fd %d, hint %ux%u)", nsess, cl[i].fd, hint_w, hint_h);
	}
	reply(i, VD_OPEN, rc, 0);
}

/* the first FRAME: the size from its SPS (else the OPEN hint, else 1920x1088 as venusctl), then the decoder start */
static int sess_start(const uint8_t *p, uint32_t len)
{
	uint32_t off[4], alen[4];
	struct h264_sps sps = { 0, 0, 0, 0, 0, 0 };
	if (split_aus(p, len, off, alen, 4, &sps) < 0) {
		uint32_t w = hint_w ? hint_w : 1920, h = hint_h ? hint_h : 1080;
		sps = (struct h264_sps){ .w = (int)((w + 15) & ~15u), .h = (int)((h + 15) & ~15u), .cw = (int)w, .ch = (int)h };
		printf("venusd: no SPS in the first access unit: %dx%d (visible %dx%d), the firmware's SEQUENCE_CHANGED decides\n",
		       sps.w, sps.h, sps.cw, sps.ch);
	}
	if (vdec_prepare(debug, 0, buf_mb, INT_MAX, &sps))
		return -EIO;
	int r = vdec_start(in_min);
	if (r) {
		struct vdec_status st;
		vdec_status(&st);
		hfi = r == 2;	/* a session to close (sess_stop) */
		return st.err ? st.err : -EIO;
	}
	sess = S_RUN;
	hfi = 1;
	t_prog = mono();
	return 0;
}

/* one message from connection i (it is readable) */
static void command(int i)
{
	struct venusd_msg m;
	if (rd_full(cl[i].fd, &m, sizeof m)) {
		printf("venusd: client fd %d gone\n", cl[i].fd);
		drop_client(i);
		return;
	}
	if (m.magic != VENUSD_MAGIC || m.len > VENUSD_MAX_AU) {
		printf("venusd: client fd %d: bad message (magic %#x, len %u): dropped\n", cl[i].fd, m.magic, m.len);
		drop_client(i);
		return;
	}
	if (m.len && rd_full(cl[i].fd, au, m.len)) {
		drop_client(i);
		return;
	}
	if (m.type == VD_OPEN) {
		do_open(i, &m);
		return;
	}
	if (i != owner) {
		reply(i, VD_ERROR, -EINVAL, m.seq);
		return;
	}
	if (m.type == VD_CLOSE) {
		reply(i, VD_CLOSE, 0, 0);
		sess_end("closed");
		return;
	}
	if (sess == S_FAILED) {
		reply(i, VD_ERROR, -EIO, m.seq);
		return;
	}
	if (m.type == VD_FRAME) {
		if (sess == S_EOS || sess == S_DONE) {
			reply(i, VD_ERROR, -EINVAL, m.seq);
			return;
		}
		if (sess == S_OPEN) {
			int r = sess_start(au, m.len);
			if (r) {
				sess_fail(r, m.seq);
				return;
			}
		}
		int r = vdec_etb(au, m.len, m.pts, 0, (int)m.seq);
		if (r == -2) {
			struct vdec_status st;
			vdec_status(&st);
			printf("venusd: frame %u: %u bytes, input buffers are %u: dropped\n", m.seq, m.len, st.in_size);
			reply(i, VD_ERROR, -E2BIG, m.seq);
		} else if (r) {
			sess_fail(-EIO, m.seq);
		} else {
			sent++;
		}
		return;
	}
	if (m.type == VD_EOS) {
		if (sess == S_OPEN) {	/* nothing decoded: nothing to drain */
			reply(i, VD_EOS_DONE, 0, 0);
			sess = S_DONE;
			return;
		}
		if (sess != S_RUN || vdec_etb(NULL, 0, 0, 1, (int)sent)) {
			sess_fail(sess == S_RUN ? -EIO : -EINVAL, m.seq);
			return;
		}
		sess = S_EOS;
		t_eos = t_prog = mono();
		return;
	}
	reply(i, VD_ERROR, -EINVAL, m.seq);
}

/* while a session runs: the queues, then what came of them */
static void sess_poll(void)
{
	if (sess != S_RUN && sess != S_EOS)
		return;
	if (vdec_poll(0)) {
		sess_fail(-EIO, sent);
		return;
	}
	struct vdec_status st;
	vdec_status(&st);
	double t = mono();
	if (st.acts != last_acts)
		last_acts = st.acts, t_prog = t;
	if (st.err || st.fw_dead) {
		sess_fail(st.err ? st.err : -EIO, sent);
		return;
	}
	if (sess == S_EOS && st.eos) {
		reply(owner, VD_EOS_DONE, 0, 0);
		sess = S_DONE;
		printf("venusd: EOS done %.0f ms after EOS, %d frames\n", 1e3 * (t - t_eos), st.frames);
		return;
	}
	/* venusctl decode's 3 s without a message; while the inputs are all queued too (the client can't send).
	 * vdcold: VENUSD_COLD_S before the session's first picture */
	double quiet = !t_first && cold_s > 3 ? cold_s : 3;
	if (t - t_prog > quiet && (sess == S_EOS || !vdec_can_etb())) {
		printf("venusd: no firmware progress for %.0f s (%s)\n", quiet, sname[sess]);
		if (sess == S_EOS) {
			reply(owner, VD_EOS_DONE, -ETIMEDOUT, 0);
			sess = S_DONE;
		} else {
			sess_fail(-ETIMEDOUT, sent);
		}
	}
}

/* vdcold: one tiny decode at start (VENUSD_WARMUP), then Venus down again: the first-use stall happens here */
static void warm_up(void)
{
	struct vdec_status st = { 0 };
	double t0 = mono(), t_up, lim = cold_s > 30 ? cold_s : 30;
	int r = 0, eos = 0;
	note("venusd: warm-up: Venus up, one %zu-byte 320x240 IDR, Venus down", sizeof warm_au);
	if (bring_up()) {
		note("venusd: warm-up: bring-up failed (OPEN brings it up again)");
		return;
	}
	t_up = mono();
	t_open = t_up, t_first = t_last_frame = 0;
	memcpy(au, warm_au, sizeof warm_au);
	r = sess_start(au, sizeof warm_au);
	if (!r && vdec_etb(au, sizeof warm_au, 0, 0, 0))
		r = -EIO;
	while (!r && !term) {
		if (!eos && vdec_can_etb()) {	/* the EOS once an input is free (a reconfig may hold them) */
			if (vdec_etb(NULL, 0, 0, 1, 1)) {
				r = -EIO;
				break;
			}
			eos = 1;
		}
		if (vdec_poll(1)) {
			r = -EIO;
			break;
		}
		vdec_status(&st);
		if (st.err || st.fw_dead)
			r = st.err ? st.err : -EIO;
		else if (st.eos)
			break;
		else if (mono() - t_up > lim)
			r = -ETIMEDOUT;
	}
	vdec_status(&st);
	note("venusd: warm-up %s (status %d): %d frame(s), first frame %.0f ms after the decoder start, %.0f ms in all"
	     " (bring-up %.0f ms)", r ? "FAILED" : "done", r, st.frames, t_first ? 1e3 * (t_first - t_up) : 0.0,
	     1e3 * (mono() - t0), 1e3 * (t_up - t0));
	sess_stop();
	sess = S_NONE;
	t_first = t_last_frame = 0;
	if (up)
		down("warm-up done");
}

static void on_term(int s)
{
	(void)s;
	term = 1;
}

static int sock_create(void)	/* camd's sock_create */
{
	struct sockaddr_un a = { .sun_family = AF_UNIX };
	snprintf(a.sun_path, sizeof a.sun_path, "%s", VENUSD_SOCK);
	unlink(VENUSD_SOCK);
	int s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0 || bind(s, (struct sockaddr *)&a, sizeof a) || chmod(VENUSD_SOCK, 0666) || listen(s, 4)) {
		perror(VENUSD_SOCK);
		return -1;
	}
	return s;
}

int main(int argc, char **argv)
{
	const char *logp = "/var/local/venusd.log";
	if (argc == 3 && !strcmp(argv[1], "--log"))
		logp = argv[2];
	else if (argc != 1) {
		fprintf(stderr, "usage: venusd [--log FILE]\n");
		return 2;
	}
	int lk = open(LOCKF, O_RDWR | O_CREAT, 0644);
	if (lk < 0 || flock(lk, LOCK_EX | LOCK_NB)) {
		fprintf(stderr, "venusd: already running (%s locked)\n", LOCKF);
		return 0;	/* not a crash: launchd leaves it */
	}
	if (!ftruncate(lk, 0))
		dprintf(lk, "%d\n", getpid());
	logfd = open(logp, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (logfd < 0) {
		perror(logp);
		return 1;
	}
	dup2(logfd, 1);
	dup2(logfd, 2);
	setvbuf(stdout, NULL, _IOLBF, 0);
	vc_quiet = !(getenv("VENUSD_VERBOSE") && atoi(getenv("VENUSD_VERBOSE")));
	if (getenv("VENUSD_IDLE_S"))
		idle_s = (unsigned)atoi(getenv("VENUSD_IDLE_S"));
	if (getenv("VENUSD_PHYS"))
		phys = strtoull(getenv("VENUSD_PHYS"), NULL, 0);
	if (getenv("VENUSD_BUF_MB"))
		buf_mb = strtoull(getenv("VENUSD_BUF_MB"), NULL, 0);
	if (getenv("VENUSD_IN_KB"))
		in_min = (uint32_t)strtoul(getenv("VENUSD_IN_KB"), NULL, 0) << 10;
	if (getenv("VENUSD_DEBUG"))
		debug = (uint32_t)strtoul(getenv("VENUSD_DEBUG"), NULL, 0);
	if (getenv("VENUSD_COLD_S"))
		cold_s = (unsigned)atoi(getenv("VENUSD_COLD_S"));
	int warm = !getenv("VENUSD_WARMUP") || atoi(getenv("VENUSD_WARMUP"));
	struct stat sb;
	fw = stat("/vendor/firmware/venus.mdt", &sb) == 0 ? "/vendor/firmware" : "/var/local/fw/venus";	/* venusctl's */
	au = malloc(VENUSD_MAX_AU);
	if (!au)
		return 1;
	note("venusd: start, pid %d, idle %u s, queues %#llx, window %llu MB, inputs >= %u KB, quiet %d, cold %u s, warm-up %d",
	     getpid(), idle_s, (unsigned long long)phys, (unsigned long long)buf_mb, in_min >> 10, vc_quiet, cold_s, warm);
	pm_init();
	stale_drop(STATE);
	stale_drop(AHB_STATE);
	if (!stat(STATE, &sb))	/* a venusd (or venusctl) of this boot died with Venus up */
		down("STATE left from an earlier run");
	else if (!stat(AHB_STATE, &sb)) {
		note("venusd: %s without %s: powered, no firmware: power off", AHB_STATE, STATE);
		cvp = 1;
		power_off();
	}
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, on_term);
	signal(SIGINT, on_term);
	vdec_sink = sink;
	int ls = sock_create();
	if (ls < 0)
		return 1;
	for (int i = 0; i < MAXCL; i++)
		cl[i].fd = -1, cl[i].gone = 0;
	if (warm && !fault)	/* clients that connect meanwhile wait in the listen backlog */
		warm_up();
	idle_since = mono();
	while (!term) {
		struct pollfd p[MAXCL + 1];
		int n = 0, idx[MAXCL];
		p[n].fd = ls, p[n].events = POLLIN, p[n++].revents = 0;
		for (int i = 0; i < MAXCL; i++) {
			/* the owner's next message only once it can be taken: an input buffer free (backpressure) */
			if (cl[i].fd < 0 || cl[i].gone || (i == owner && sess == S_RUN && !vdec_can_etb()))
				continue;
			idx[n - 1] = i, p[n].fd = cl[i].fd, p[n].events = POLLIN, p[n++].revents = 0;
		}
		int active = sess == S_RUN || sess == S_EOS;
		if (poll(p, (nfds_t)n, active ? 1 : 1000) > 0) {
			if (p[0].revents & POLLIN) {
				int fd = accept(ls, NULL, NULL), k;
				for (k = 0; fd >= 0 && k < MAXCL && cl[k].fd >= 0; k++)
					;
				if (fd >= 0 && k == MAXCL) {
					printf("venusd: too many clients\n");
					close(fd);
				} else if (fd >= 0) {
					struct timeval tv = { 2, 0 };	/* a stuck client can't hold the daemon for long */
					int sz = 4 << 20;
					setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
					setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
					setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sz, sizeof sz);
					cl[k].fd = fd, cl[k].gone = 0;
					printf("venusd: client fd %d connected\n", fd);
				}
			}
			for (int j = 1; j < n; j++)
				if (p[j].revents & (POLLIN | POLLHUP | POLLERR) && cl[idx[j - 1]].fd >= 0)
					command(idx[j - 1]);
		}
		sess_poll();
		for (int i = 0; i < MAXCL; i++)
			if (cl[i].fd >= 0 && cl[i].gone)
				drop_client(i);
		if (up && owner < 0 && mono() - idle_since > idle_s)
			down("idle");
		if (!fstat(logfd, &sb) && sb.st_size > LOG_MAX && !ftruncate(logfd, 0))
			printf("venusd: log cut at %d B\n", LOG_MAX);
	}
	if (owner >= 0)
		sess_end("closed (venusd stopping)");
	if (up)
		down("venusd stopping");
	note("venusd: exit (signal)%s", fault ? ": LEFT POWERED" : "");
	unlink(VENUSD_SOCK);
	return 1;	/* as camd: launchd (KeepAlive SuccessfulExit false) starts it again after a kill */
}
