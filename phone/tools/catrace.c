/*
 * catrace: record Core Animation kdebug trace points (class 0x31 subclass 0xca) with mach timestamps, through the
 * KERN_KDEBUG sysctls (root). Low Settings launches: app commits against backboardd's renders.
 *   catrace SECONDS OUT.bin       raw records + thread map (tools/catrace-dump.py reads it)
 *   catrace -t SECONDS [OUT.bin]  also print per-code, per-pid and lost-event counts
 * File: header (struct hdr), nrec raw 64-byte kd_buf, nthr 32-byte kd_threadmap (tid, pid, command[20]).
 * Only class 0x31/0xca is let through; the kernel always adds class 7 (DBG_TRACE: lost events, new threads, exec).
 * Takes ktrace over from a background owner; KERN_KDREMOVE on exit and on SIGINT/SIGTERM.
 */
#include <mach/mach_time.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <unistd.h>

/* bsd/sys/sysctl.h, bsd/sys/kdebug_private.h (not all in the SDK) */
#ifndef KERN_KDEBUG
#define KERN_KDEBUG 24
#endif
#define KD_ENABLE 3
#define KD_SETBUF 4
#define KD_GETBUF 5
#define KD_SETUP 6
#define KD_REMOVE 7
#define KD_READTR 10
#define KD_THRMAP 12
#define KD_READCURTHRMAP 21
#define KD_SET_TYPEFILTER 22
#define KDEBUG_ENABLE_TRACE 1
#define TYPEFILTER_SIZE (256 * 256 / 8)
#define TRACE_LOST_EVENTS 0x07020008u
#define TRACE_DATA_NEWTHREAD 0x07000004u
#define NBUFS 131072	/* events in the kernel buffer (8 MB); read every 50 ms */

typedef struct {
	uint64_t timestamp, arg1, arg2, arg3, arg4, arg5;	/* arg5 = thread id */
	uint32_t debugid, cpuid;
	uint64_t unused;
} kd_buf;	/* 64 bytes on arm64 */

typedef struct {
	uint64_t thread;
	int valid;	/* pid */
	char command[20];
} kd_threadmap;	/* 32 bytes */

typedef struct {
	int nkdbufs, nolog;
	unsigned int flags;
	int nkdthreads, bufid;
} kbufinfo_t;

struct hdr {
	char magic[8];	/* "CATRACE\0" */
	uint32_t version, numer, denom, recsize;
	uint64_t nrec, nthr, lost, start;	/* lost = TRACE_LOST_EVENTS records; start = mach_absolute_time */
};

static volatile sig_atomic_t stop;
static int armed;

static int kd(int op, int val, int nval, void *buf, size_t *len)
{
	int mib[4] = { CTL_KERN, KERN_KDEBUG, op, val };
	return sysctl(mib, nval ? 4 : 3, buf, len, NULL, 0);
}

static void cleanup(void)
{
	size_t z = 0;
	if (armed)
		kd(KD_REMOVE, 0, 0, NULL, &z);
	armed = 0;
}

static void onsig(int s)
{
	(void)s;
	stop = 1;
}

static void die(const char *what)
{
	perror(what);
	exit(1);
}

int main(int argc, char **argv)
{
	int text = 0;
	if (argc > 1 && !strcmp(argv[1], "-t"))
		text = 1, argc--, argv++;
	if (argc < 2 || (!text && argc < 3)) {
		fprintf(stderr, "usage: catrace SECONDS OUT.bin | catrace -t SECONDS [OUT.bin]\n");
		return 2;
	}
	double secs = atof(argv[1]);
	const char *out = argc > 2 ? argv[2] : NULL;
	FILE *f = NULL;
	if (out && !(f = fopen(out, "wb")))
		die(out);

	size_t z = 0;
	atexit(cleanup);
	signal(SIGINT, onsig);
	signal(SIGTERM, onsig);
	armed = 1;
	kd(KD_REMOVE, 0, 0, NULL, &z);
	if (kd(KD_SETBUF, NBUFS, 1, NULL, &z) || kd(KD_SETUP, 0, 0, NULL, &z))
		die("kdebug setbuf/setup");
	static uint8_t tf[TYPEFILTER_SIZE];
	tf[0x31ca / 8] |= 1 << (0x31ca % 8);	/* the kernel adds class 7 itself */
	z = sizeof(tf);
	if (kd(KD_SET_TYPEFILTER, 0, 0, tf, &z))
		die("kdebug typefilter");
	z = 0;
	if (kd(KD_ENABLE, KDEBUG_ENABLE_TRACE, 1, NULL, &z))
		die("kdebug enable");

	mach_timebase_info_data_t tb;
	mach_timebase_info(&tb);
	struct hdr h = { "CATRACE", 1, tb.numer, tb.denom, sizeof(kd_buf), 0, 0, 0, mach_absolute_time() };
	uint64_t end = h.start + (uint64_t)(secs * 1e9 * tb.denom / tb.numer);
	kd_buf *rd = malloc(NBUFS * sizeof(kd_buf)), *all = NULL;
	size_t nall = 0, cap = 0;
	if (!rd)
		die("malloc");
	if (f)
		fwrite(&h, sizeof(h), 1, f);
	for (int last = 0; !last;) {
		usleep(50000);
		last = stop || mach_absolute_time() >= end;
		size_t len = NBUFS * sizeof(kd_buf);
		if (kd(KD_READTR, 0, 0, rd, &len))
			die("kdebug read");
		/* len is now the number of events */
		for (size_t i = 0; i < len; i++)
			h.lost += rd[i].debugid == TRACE_LOST_EVENTS;
		if (f)
			fwrite(rd, sizeof(kd_buf), len, f);
		h.nrec += len;
		if (text) {
			if (nall + len > cap) {
				cap = (nall + len) * 2;
				if (!(all = realloc(all, cap * sizeof(kd_buf))))
					die("realloc");
			}
			memcpy(all + nall, rd, len * sizeof(kd_buf));
			nall += len;
		}
	}
	z = 0;
	kd(KD_ENABLE, 0, 1, NULL, &z);

	/* thread map: the kernel's snapshot from enable time, then threads alive now that it lacks */
	kbufinfo_t bi = { 0 };
	z = sizeof(bi);
	kd(KD_GETBUF, 0, 0, &bi, &z);
	size_t n0 = bi.nkdthreads > 0 ? bi.nkdthreads : 0, ncur = 65536;
	kd_threadmap *map = calloc(n0 + ncur, sizeof(kd_threadmap));
	if (!map)
		die("calloc");
	z = n0 * sizeof(kd_threadmap);
	if (n0 && kd(KD_THRMAP, 0, 0, map, &z))
		perror("kdebug thrmap"), n0 = 0;
	z = ncur * sizeof(kd_threadmap);
	if (kd(KD_READCURTHRMAP, 0, 0, map + n0, &z))
		perror("kdebug readcurthrmap"), z = 0;
	ncur = z / sizeof(kd_threadmap);
	size_t nthr = n0;
	for (size_t i = n0; i < n0 + ncur; i++) {
		size_t j;
		if (!map[i].thread)
			continue;
		for (j = 0; j < n0 && map[j].thread != map[i].thread; j++)
			;
		if (j == n0)
			map[nthr++] = map[i];
	}
	cleanup();
	h.nthr = nthr;
	if (f) {
		fwrite(map, sizeof(kd_threadmap), nthr, f);
		fseek(f, 0, SEEK_SET);
		fwrite(&h, sizeof(h), 1, f);
		if (fclose(f))
			die(out);
	}
	fprintf(stderr, "catrace: %llu records, %zu threads, %llu lost-event markers, %.1f s%s%s\n",
		(unsigned long long)h.nrec, nthr, (unsigned long long)h.lost,
		(mach_absolute_time() - h.start) * 1e-9 * tb.numer / tb.denom, out ? " -> " : "", out ? out : "");
	if (!text)
		return 0;

	/* per-code counts */
	uint32_t codes[512];
	uint64_t ccount[512];
	int nc = 0;
	for (size_t i = 0; i < nall; i++) {
		int k;
		for (k = 0; k < nc && codes[k] != all[i].debugid; k++)
			;
		if (k == nc) {
			if (nc == 512)
				continue;
			codes[nc] = all[i].debugid, ccount[nc++] = 0;
		}
		ccount[k]++;
	}
	printf("code        count\n");
	for (int k = 0; k < nc; k++)
		printf("0x%08x  %llu\n", codes[k], (unsigned long long)ccount[k]);

	/* per-pid counts of class 0x31 events; threads born during the trace from TRACE_DATA_NEWTHREAD (arg1 tid, arg2 pid) */
	int pids[256];
	uint64_t pcount[256];
	int np = 0, pid = -1;
	uint64_t lasttid = 0;
	for (size_t i = 0; i < nall; i++) {
		kd_buf *e = &all[i];
		if (e->debugid >> 24 != 0x31)
			continue;
		if (e->arg5 != lasttid)
			pid = -1;
		lasttid = e->arg5;
		for (size_t j = 0; j < nthr && pid < 0; j++)
			if (map[j].thread == e->arg5)
				pid = map[j].valid;
		for (size_t j = 0; j < nall && pid < 0; j++)
			if (all[j].debugid == TRACE_DATA_NEWTHREAD && all[j].arg1 == e->arg5)
				pid = (int)all[j].arg2;
		int k;
		for (k = 0; k < np && pids[k] != pid; k++)
			;
		if (k == np) {
			if (np == 256)
				continue;
			pids[np] = pid, pcount[np++] = 0;
		}
		pcount[k]++;
	}
	printf("pid     count    command\n");
	for (int k = 0; k < np; k++) {
		const char *cmd = "?";
		for (size_t j = 0; j < nthr; j++)
			if (map[j].valid == pids[k]) {
				cmd = map[j].command;
				break;
			}
		printf("%-7d %-8llu %.20s\n", pids[k], (unsigned long long)pcount[k], cmd);
	}
	printf("lost-event markers %llu\n", (unsigned long long)h.lost);
	return 0;
}
