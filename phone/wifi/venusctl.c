/*
 * venusctl: video stage 2 (docs/features/video.md "Stage 2 spec"), the Venus/IRIS1 video engine from userland
 * through /dev/pmem, as adspboot does for the ADSP. Facts: research/notes/venus-stage1-2026-11-02.md (DT lines,
 * downstream videocc-sdmmagpie.c in research/ext/msm-vidc-sunfish/), PLL/RCG/branch sequences as phone/tools/camclk.c.
 *   venusctl probe          read-only: GCC video CBCRs, TBU sf vote/status, apps SMMU SMRs for SIDs 0x1080-0x10e0,
 *                           cmd-db MM3/MM2; videocc (efuse, PLL0, RCG, GDSCs, CBCRs) only with gcc_video_ahb on.
 *                           Never reads the core (0x0aa00000).
 *   venusctl power on [--cvp]   BCM MM3 + MM2 minimal votes (no MX/CX), gcc_video_ahb/axi, PLL0 480 MHz, iris RCG /2
 *                           = 240 MHz, GDSC mvsc then mvs0 (--cvp: + mvs1), branches; then WRAPPER_HW_VERSION
 *   venusctl power off [--cvp] [--force]  the reverse; gcc_video_ahb stays on (critical in mainline gcc-sm7150),
 *                           votes last; its HW_CTL bit goes back if power on cleared it (state in AHB_STATE).
 *                           Refuses while STATE says pas 1 (firmware may run: venusctl shutdown first) unless --force.
 *   venusctl pas-check [--fw DIR] [--no-shutdown]  needs power on: PAS is_supported(9), init_image(9, venus.mdt
 *                           metadata), shutdown(9). No mem_setup, no auth_and_reset. --no-shutdown: STATE pas 1.
 *   venusctl boot [--fw DIR] [--keep]  needs power on: exact SMRs for SIDs 0x1080/0x10a0/0x10c0/0x10e0 (smmu.c
 *                           smmu_identity, mask 0), CPU_CGC_DIS/CPU_CLOCK_CONFIG = 0, PAS init_image(9), mem_setup,
 *                           segments to 0x94800000, auth_and_reset(9), memory protect 0x42000c08, status reads.
 *                           Without --keep: PAS shutdown(9) and the SMRs go again. What it did: STATE.
 *   venusctl init [--phys ADDR]  after boot --keep: 3 MB HFI queue block (PMEM_ALLOC32, else only with --phys, 1 MB
 *                           aligned in 0xa5100000-0xbcc00000) with the queue table, 3 queues and the SFR page, the
 *                           UC/QTBL/SFR/VMIMSG registers, CTRL_INIT = 1, poll CTRL_STATUS (1 s)
 *   venusctl ping [--debug MASK]  after init: SYS_INIT -> INIT_DONE, debug to the debug queue, IMAGE_VERSION,
 *                           SYS_PING -> PING_ACK; polls the message queue (no IRQ), prints every packet. No
 *                           PING_ACK is a note, not a failure (v2c; VENUSCTL_PING_FATAL=1: failure)
 *   venusctl session [--debug MASK]  after init: as ping up to IMAGE_VERSION, then SYS_SESSION_INIT (H.264 decoder)
 *                           -> SESSION_INIT_DONE, SYS_SESSION_END -> SESSION_END_DONE (v2c)
 *   venusctl shutdown       undoes boot --keep/init from STATE: PAS shutdown(9), queue memory, SMRs. Then power off.
 *   venusctl memtest ADDR SIZE write|check  the --phys region check (pre-run review 8.1): fill ADDR..ADDR+SIZE through
 *                           /dev/pmem (as init) with addr ^ 0x5a5a5a5a per word, or count the words that changed
 * Pre-run review fixes (notes section 8.5): STATE and AHB_STATE carry the boot session id; a file from an earlier
 * boot is dropped unread (VENUSCTL_NO_BOOTID=1: keep the old behaviour). smmu release only of a slot that still reads
 * our exact SMR and S2CR; it counts as done once the SMR reads invalid, other differences are warnings (v2c;
 * VENUSCTL_STRICT_RELEASE=1: any difference fails). init/ping re-check the mnoc_sf_0_tbu vote before CTRL_INIT and each doorbell.
 *   -v                      print every register write (address, old, new, read-back)
 * Writes only: GCC video_ahb/video_axi CBCRs, the videocc PLL0/RCG/GDSC/CBCR registers below, RPMh BCMs MM3/MM2,
 * the core registers in allowed() (boot/init/ping), the firmware carveout, the queue block; SMMU writes only through
 * smmu_identity/smmu_release (one exact SMR + S2CR + translation-off bank per SID, never masked: masked SMRs reset
 * the phone in camera tests). Every videocc access is gated on gcc_video_ahb CLK_OFF = 0, every core access on the
 * GDSCs + clocks (core_ok). Polls time out (100 ms; 1 s for the firmware) and say where they stopped.
 * Off switches: VENUSCTL_SID1=1 (only 0x1080), VENUSCTL_NO_CLKCFG=1 (no CPU_CGC_DIS/CLOCK_CONFIG writes),
 * VENUSCTL_NO_MEMPROT=1 (no memory protect call).
 * gcc_video_ahb boots enabled but hardware-gated (0x80000003: HW_CTL set, CLK_OFF reads 1 while idle; Linux
 * clk-branch.c skips the halt check in that mode). power on clears HW_CTL first so CLK_OFF can be polled;
 * VENUSCTL_KEEP_HWCTL=1 skips that (old behaviour).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include "pmem.h"
#include "smmu.h"

#define GCC		0x00100000u
#define GCC_VIDEO_AHB	(GCC + 0xb004)	/* gcc_video_ahb_clk, GDSC parent (notes table "GDSC parent clock") */
#define GCC_VIDEO_AXI	(GCC + 0xb01c)
#define GCC_VIDEO_XO	(GCC + 0xb028)	/* critical in mainline: read only (no downstream video consumer) */
#define GCC_HW_CTL	(1u << 1)	/* CBCR hwcg bit, as gcc-sm7150.c hwcg_bit = 1 */
#define AHB_STATE	"/var/tmp/venusctl.ahb"	/* gcc_video_ahb CBCR before power on cleared HW_CTL */
#define GCC_TBU_SF_VOTE	0x0017d044u	/* hlos1 vote mnoc_sf_0_tbu (SIDs 0x1000-0x13ff), camera-sid-cc0.md: read only */
#define TBU_SF_STATUS	0x15182220u	/* 2 words (camera-sid-cc0.md section 4) */
#define EFUSE_VIDEO	0x00786018u	/* bit 10 = multipipe disabled: PLL L 0x14, 200 MHz only (videocc-sdmmagpie.c:350) */
#define EFUSE_NO_MPIPE	(1u << 10)

#define VCC		0x0ab00000u
/* video_pll0, Fabia, .offset 0x42c (videocc-sdmmagpie.c:91); Fabia layout as camclk.c (clk-alpha-pll.c:120-131) */
#define PLL0		(VCC + 0x42c)
#define PLL_MODE	(PLL0 + 0x00)
#define PLL_L		(PLL0 + 0x04)
#define PLL_USER	(PLL0 + 0x0c)
#define PLL_USER_U	(PLL0 + 0x10)
#define PLL_CONFIG	(PLL0 + 0x14)
#define PLL_CONFIG_U	(PLL0 + 0x18)
#define PLL_TEST	(PLL0 + 0x1c)
#define PLL_TEST_U	(PLL0 + 0x20)
#define PLL_STATUS	(PLL0 + 0x24)
#define PLL_OPMODE	(PLL0 + 0x2c)
#define PLL_FRAC	(PLL0 + 0x38)
#define OUTCTRL		(1u << 0)
#define RESET_N		(1u << 2)
#define VOTE_FSM_ENA	(1u << 20)
#define UPDATE_BYPASS	(1u << 23)
#define LOCK_DET	(1u << 31)
#define PLL_RUN		1u
#define PLL_OUT_MASK	7u
/* video_cc_iris_clk_src: cmd_rcgr 0x7f0, hid_width 5, no MND, PLL0 main = sel 1 (videocc-sdmmagpie.c:51-56,138) */
#define IRIS_RCG	(VCC + 0x7f0)
#define IRIS_CFG	(IRIS_RCG + 4)
/* GDSCRs (live.dts 6995-7023); SW_COLLAPSE bit 0, HW_CONTROL bit 1, PWR_ON bit 31 (Linux gdsc.c) */
#define GDSC_MVSC	(VCC + 0x814)
#define GDSC_MVS0	(VCC + 0x874)
#define GDSC_MVS1	(VCC + 0x8b4)
#define SW_COLLAPSE	(1u << 0)
#define HW_CONTROL	(1u << 1)
#define PWR_ON		(1u << 31)
#define CBCR_EN		(1u << 0)
#define CLK_OFF		(1u << 31)
#define VCC_END		(VCC + 0xb94)	/* regmap max_register (videocc-sdmmagpie.c:330) */

#define WRAP_HW_VERSION	0x0aae0000u	/* WRAPPER 0xE0000 + 0 (vidc_hfi_io.h) */

/* video stage 2 steps 4-6: core registers (vidc_hfi_io.h, absolute = 0x0aa00000 + offset; notes section 1) */
#define CORE		0x0aa00000u
#define CORE_END	0x0aae4000u	/* up to the end of the wrapper (PIL reg 0x0aae0000 + 0x4000); videocc lies above */
#define CS		(CORE + 0xd2000)	/* VIDC_CPU_CS_BASE_OFFS */
#define CS_VMIMSG	(CS + 0x34)	/* HFI_DSP_QTBL_ADDR (setup_dsp_uc_memmap_vpu5, venus_hfi.c:4688) */
#define CS_VMIMSGAG0	(CS + 0x38)	/* HFI_DSP_UC_REGION_ADDR (venus_hfi.c:4690) */
#define CS_VMIMSGAG1	(CS + 0x3c)	/* HFI_DSP_UC_REGION_SIZE (venus_hfi.c:4692) */
#define CTRL_INIT	(CS + 0x48)	/* SCIACMD (__boot_firmware, venus_hfi.c:1274) */
#define CTRL_STATUS	(CS + 0x4c)	/* SCIACMDARG0: read only */
#define QTBL_INFO	(CS + 0x50)	/* SCIACMDARG1 (__setup_ucregion_memory_map, venus_hfi.c:1888) */
#define QTBL_ADDR	(CS + 0x54)	/* SCIACMDARG2 (venus_hfi.c:1886) */
#define VERSION_INFO	(CS + 0x58)	/* SCIACMDARG3: read only */
#define SFR_ADDR	(CS + 0x5c)	/* SCIBCMD (venus_hfi.c:1890) */
#define UC_ADDR		(CS + 0x64)	/* SCIBARG1 (venus_hfi.c:1883) */
#define UC_SIZE		(CS + 0x68)	/* SCIBARG2 (venus_hfi.c:1885) */
#define IC_SOFTINT	(CORE + 0xdf018)	/* VIDC_CPU_IC_SOFTINT, bit 15 = H2A (__iface_cmdq_write doorbell) */
#define SOFTINT_H2A	(1u << 15)
#define WRAP_INTR_STATUS (CORE + 0xe000c)	/* read only */
#define WRAP_CPU_CLKCFG	(CORE + 0xe2000)	/* clock_config_on_enable_vpu5 (venus_hfi.c:4706) */
#define WRAP_CPU_CGCDIS	(CORE + 0xe2010)	/* clock_config_on_enable_vpu5 (venus_hfi.c:4705) */
#define WRAP_CPU_STATUS	(CORE + 0xe2014)	/* read only */
#define ST_INIT		1u		/* CTRL_STATUS bit 0 */
#define ST_ERROR	0xfeu		/* CTRL_STATUS 7:1, 4 = bad UC region (venus_hfi.c:1277) */

#define FW_PA		0x94800000ull	/* pil_video_region (live.dts 23729-23733) */
#define FW_SIZE		0x00500000ull
#define SCM_MP_VIDEO_VAR 0x42000c08ull	/* SCM_SIP_FNID(SCM_SVC_MP 0xc, TZBSP_MEM_PROTECT_VIDEO_VAR 8) */
/* __protect_cp_mem (venus_hfi.c:4347-4390): cp_size = venus_ns start, non-pixel = venus_sec_non_pixel range */
#define CP_SIZE		0x25800000u
#define CP_NP_START	0x01000000u
#define CP_NP_SIZE	0x24800000u
#define STATE		"/var/tmp/venusctl.state"
static const uint16_t venus_sids[] = { 0x1080, 0x10a0, 0x10c0, 0x10e0 };	/* venus_ns 0x1080 mask 0x60 (live.dts 9974) */

/* HFI shared memory (venus_hfi.h:36-113): 3 MB block = table 0x1c8 + 3 queues of 0xc8000; queues get
 * SHARED_QSIZE - SFR - QDSS = 0x2fe000 (venus_hfi.c:1916), the SFR page follows them, inside the UC region */
#define QSHARED		0x300000u
#define QTBL_SIZE	0x1c8u
#define QHDR0		0x120u		/* sizeof(struct hfi_queue_table_header) */
#define QHDR_SIZE	0x38u
#define QSIZE		0xc8000u	/* 1024 x 50 x 16 */
#define QWORDS		(QSIZE / 4)
#define SFR_OFF		0x2fe000u
#define SFR_SIZE	0x1000u
#define FIXED_LO	0xa5030000ull	/* free below 4 GB: after ramdumplog to the DRAM hole (notes 8.1; unproven) */
#define FIXED_HI	0xbcc00000ull
#define POOL_END	0xe0000000ull	/* venus_ns IOVA pool 0x25800000-0xe0000000 (notes 8.4) */

#define PAS_VENUS	9
#define SCM_PIL(cmd)	(0x42000000ull | (2u << 8) | (cmd))
#define MDT_RELOC	(1u << 27)

/* videocc branches (videocc-sdmmagpie.c:170-300); voted = BRANCH_VOTED there: no halt poll on disable (10 us) */
struct br { const char *name; uint32_t pa; int voted, cvp; };
static const struct br branches[] = {	/* enable order (spec); disable runs it backwards */
	{ "mvsc_ctl_axi", VCC + 0x9cc, 0, 0 },
	{ "mvs0_axi", VCC + 0x9ec, 0, 0 },
	{ "mvs1_axi", VCC + 0xa0c, 0, 1 },
	{ "mvsc_core", VCC + 0x850, 0, 0 },
	{ "mvs0_core", VCC + 0x890, 1, 0 },
	{ "mvs1_core", VCC + 0x8d0, 1, 1 },
	{ "venus_ahb", VCC + 0xa6c, 0, 0 },
	{ "iris_ahb", VCC + 0x8f4, 1, 0 },
};
#define NBR (sizeof(branches) / sizeof(branches[0]))
#define VIDEO_CC_XO	(VCC + 0x984)	/* CLK_IS_CRITICAL: read only */

static int verbose, cvp, force;

/* ---- boot session id (pre-run review 8.5.1): state files from an earlier boot are dropped unread ---- */

static char bootid[64];

static const char *boot_id(void)
{
	if (bootid[0])
		return bootid;
	size_t n = sizeof(bootid) - 1;
	if (sysctlbyname("kern.bootsessionuuid", bootid, &n, NULL, 0) || !bootid[0]) {
		struct timeval bt = { 0, 0 };
		n = sizeof(bt);
		sysctlbyname("kern.boottime", &bt, &n, NULL, 0);
		snprintf(bootid, sizeof(bootid), "boottime-%ld.%06d", (long)bt.tv_sec, (int)bt.tv_usec);
	}
	return bootid;
}

/* a state file without our "boot ID" line is from an earlier boot (or older venusctl): delete it, act on nothing */
static void stale_drop(const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return;
	char line[256], id[128] = "";
	while (fgets(line, sizeof(line), f))
		if (sscanf(line, "boot %127s", id) == 1)
			break;
	fclose(f);
	if (!strcmp(id, boot_id()))
		return;
	if (getenv("VENUSCTL_NO_BOOTID")) {
		printf("venus: %s is from boot %s, not %s: kept (VENUSCTL_NO_BOOTID)\n", path, id[0] ? id : "?", boot_id());
		return;
	}
	printf("venus: %s: stale state from an earlier boot: dropped (boot %s, now %s)\n", path, id[0] ? id : "?",
	       boot_id());
	unlink(path);
}

static int ahb_on(void)
{
	return !(rd32(GCC_VIDEO_AHB) & CLK_OFF);
}

/* every videocc access: gcc_video_ahb must be on, else stop here (an access with it off hangs the bus) */
static void vcc_gate(uint32_t pa, const char *what)
{
	if (pa >= VCC && pa <= VCC_END && !ahb_on()) {
		printf("venus: STOP at %s (%#x): gcc_video_ahb %#010x is off, videocc not touched\n", what, pa,
		       rd32(GCC_VIDEO_AHB));
		exit(1);
	}
}

static uint32_t vrd(uint32_t pa)
{
	vcc_gate(pa, "read");
	return rd32(pa);
}

/* every core access (0x0aa00000-0x0aae4000): AHB, gcc_video_axi, GDSCs mvsc + mvs0 and the branches all on */
static int core_ok(void)
{
	int ok = ahb_on() && !(rd32(GCC_VIDEO_AXI) & CLK_OFF) && (vrd(GDSC_MVSC) & PWR_ON) && (vrd(GDSC_MVS0) & PWR_ON);
	for (unsigned i = 0; i < NBR && ok; i++)
		if (!branches[i].cvp || cvp)
			ok = !(vrd(branches[i].pa) & CLK_OFF);
	return ok;
}

static void core_gate(uint32_t pa, const char *what)
{
	if (pa >= CORE && pa < CORE_END && !core_ok()) {
		printf("venus: STOP at %s (%#x): a video clock or GDSC is off, core not touched"
		       " (venusctl shutdown, then power off)\n", what, pa);
		exit(1);
	}
}

static uint32_t crd(uint32_t pa)
{
	core_gate(pa, "read");
	return rd32(pa);
}

static int allowed(uint32_t pa)
{
	if (pa == GCC_VIDEO_AHB || pa == GCC_VIDEO_AXI)
		return 1;
	/* stage 2 steps 4-6, each from its downstream line (defines above) */
	if (pa == WRAP_CPU_CGCDIS || pa == WRAP_CPU_CLKCFG)	/* boot: clock_config_on_enable_vpu5 */
		return 1;
	if (pa == UC_ADDR || pa == UC_SIZE || pa == QTBL_ADDR || pa == QTBL_INFO || pa == SFR_ADDR)	/* init */
		return 1;
	if (pa == CS_VMIMSG || pa == CS_VMIMSGAG0 || pa == CS_VMIMSGAG1 || pa == CTRL_INIT)	/* init */
		return 1;
	if (pa == IC_SOFTINT)	/* ping: doorbell */
		return 1;
	if ((pa >= PLL0 && pa <= PLL_FRAC) || pa == IRIS_RCG || pa == IRIS_CFG)
		return 1;
	if (pa == GDSC_MVSC || pa == GDSC_MVS0 || pa == GDSC_MVS1)
		return 1;
	for (unsigned i = 0; i < NBR; i++)
		if (pa == branches[i].pa)
			return 1;
	return 0;
}

static void wr(uint32_t pa, uint32_t v, const char *why)
{
	if (!allowed(pa)) {
		printf("venus: BLOCKED wr %#x = %#x (%s)\n", pa, v, why);
		exit(1);
	}
	vcc_gate(pa, why);
	core_gate(pa, why);
	uint32_t old = rd32(pa);
	wr32(pa, v);
	if (verbose)
		printf("venus: wr %#x %#010x -> %#010x, reads %#010x  [%s]\n", pa, old, v, rd32(pa), why);
}

static void upd(uint32_t pa, uint32_t mask, uint32_t val, const char *why)
{
	wr(pa, (vrd(pa) & ~mask) | (val & mask), why);
}

/* (reg & mask) == want within ms (1 ms steps); 0 = ok */
static int poll(uint32_t pa, uint32_t mask, uint32_t want, unsigned ms)
{
	for (unsigned i = 0; i <= ms; i++) {
		if ((vrd(pa) & mask) == want)
			return 0;
		if (i < ms)
			delay_ms(1);
	}
	return -1;
}

static const char *onoff(uint32_t cbcr)
{
	return (cbcr & CLK_OFF) ? "off" : "on";
}

/* GCC CBCRs: CLK_OFF with HW_CTL set means hardware-gated (idle), not off */
static const char *gcc_state(uint32_t cbcr)
{
	return (cbcr & CLK_OFF) && (cbcr & GCC_HW_CTL) ? "hw-gated (HW_CTL)" : onoff(cbcr);
}

/* ---- RPMh BCMs (cmd-db as adspboot.c cmddb_aux; command = commit | valid | x << 14 | y, sweet_gpu.c gpu_bus_vote) ---- */

static int cmddb_aux(const char *id, uint64_t *aux, uint32_t *len)
{
	uint64_t db = rd32(0x0c3f000cull);
	if (rd32(db + 4) != 0x0c0330db)
		return -1;
	uint64_t data = db + 4 + 4 + 8 * 16 + 4 + 4;
	for (uint32_t r = 0; r < 8; r++) {
		uint64_t rh = db + 8 + 16 * r;
		uint32_t w0 = rd32(rh), w1 = rd32(rh + 4);
		uint32_t hoff = w0 >> 16, doff = w1 & 0xffff, cnt = w1 >> 16;
		for (uint32_t e = 0; e < cnt; e++) {
			uint64_t ent = data + hoff + 24 * e;
			char name[9];
			for (uint32_t i = 0; i < 8; i++)
				name[i] = (char)(rd32(ent + (i & ~3u)) >> (8 * (i & 3)));
			name[8] = 0;
			if (strncmp(name, id, 8))
				continue;
			uint32_t w5 = rd32(ent + 20);
			*len = w5 & 0xffff;
			*aux = data + doff + (w5 >> 16);
			return 0;
		}
	}
	return -1;
}

/* Minimal nonzero vote x = y = 1 (BCM units): the notes give KBps only (vidc 1000 KBps), not the BCM unit. */
#define BCM_ON		(1u << 30 | 1u << 29 | 1u << 14 | 1u)
#define BCM_OFF		(1u << 30)	/* commit, not valid: x = y = 0 (Linux tcs_cmd_gen) */
static const char *const bcms[] = { "MM3", "MM2" };	/* vote order (notes table "RPMh BCMs") */

static int bcm(const char *res, uint32_t v)
{
	uint32_t off[1] = { 0 }, val[1] = { v };
	int rc = pm_rpmh(res, 1, off, val);
	printf("venus: bcm %s = %#010x -> %d\n", res, v, rc);
	return rc;
}

/* ---- probe (read-only) ---- */

static int sid_match(uint32_t smr, uint32_t sid)
{
	uint32_t id = smr & 0x7fff, m = (smr >> 16) & 0x7fff;
	return ((id ^ sid) & ~m & 0x7fff) == 0;
}

static void probe_smmu(void)
{
	uint64_t s = 0x15000000ull;	/* apps SMMU (smmu.c), register reads only */
	uint32_t id0 = rd32(s + 0x20), nsmr = id0 & 0xff, valid = 0, hits = 0;
	printf("smmu: sCR0 %08x ID0 %08x: %u SMRs\n", rd32(s), id0, nsmr);
	for (uint32_t i = 0; i < nsmr && i < 128; i++) {
		uint32_t smr = rd32(s + 0x800 + 4 * i);
		if (!(smr >> 31))
			continue;
		valid++;
		uint32_t first = 0;
		for (uint32_t sid = 0x1080; sid <= 0x10e0; sid++)
			if (sid_match(smr, sid)) {
				first = sid;
				break;
			}
		if (!first)
			continue;
		uint32_t s2cr = rd32(s + 0xc00 + 4 * i);
		printf("smmu: SMR%u %08x (id %#x mask %#x, first venus SID %#x) S2CR %08x type %u cb %u\n", i, smr,
		       smr & 0x7fff, (smr >> 16) & 0x7fff, first, s2cr, (s2cr >> 16) & 3, s2cr & 0xff);
		hits++;
	}
	printf("smmu: %u valid SMRs, %u match SIDs 0x1080-0x10e0\n", valid, hits);
}

static void probe_bcm(void)
{
	for (unsigned i = 0; i < 2; i++) {
		uint64_t aux;
		uint32_t len;
		if (cmddb_aux(bcms[i], &aux, &len)) {
			printf("cmd-db: %s not found\n", bcms[i]);
			continue;
		}
		printf("cmd-db: %s aux %u bytes:", bcms[i], len);
		for (uint32_t o = 0; o < len && o < 16; o += 4)
			printf(" %08x", rd32(aux + o));
		printf("\n");
	}
}

static void probe_vcc(void)
{
	uint32_t ef = rd32(EFUSE_VIDEO);
	printf("efuse %#x = %#010x: multipipe %s\n", EFUSE_VIDEO, ef, (ef & EFUSE_NO_MPIPE) ? "disabled (L 0x14)" : "ok");
	static const struct { const char *n; uint32_t off; } r[] = {
		{ "MODE", 0x00 }, { "L", 0x04 }, { "USER_CTL", 0x0c }, { "USER_CTL_U", 0x10 }, { "CONFIG_CTL", 0x14 },
		{ "CONFIG_CTL_U", 0x18 }, { "TEST_CTL", 0x1c }, { "TEST_CTL_U", 0x20 }, { "STATUS", 0x24 },
		{ "OPMODE", 0x2c }, { "FRAC", 0x38 },
	};
	for (unsigned i = 0; i < sizeof r / sizeof r[0]; i++)
		printf("pll0 %-12s %#x = %#010x\n", r[i].n, PLL0 + r[i].off, vrd(PLL0 + r[i].off));
	uint32_t m = vrd(PLL_MODE);
	printf("pll0: lock %u outctrl %u reset_n %u fsm %u opmode %u, VCO %.1f MHz\n", m >> 31, m & 1, (m >> 2) & 1,
	       (m >> 20) & 1, vrd(PLL_OPMODE), 19.2 * ((vrd(PLL_L) & 0xffff) + (vrd(PLL_FRAC) & 0xffff) / 65536.0));
	uint32_t cmd = vrd(IRIS_RCG), cfg = vrd(IRIS_CFG);
	printf("iris rcg %#x CMD %#010x (root %s) CFG %#010x (src %u, div field %u)\n", IRIS_RCG, cmd,
	       (cmd >> 31) ? "off" : "on", cfg, (cfg >> 8) & 7, cfg & 0x1f);
	static const struct { const char *n; uint32_t pa; } g[] = {
		{ "mvsc", GDSC_MVSC }, { "mvs0", GDSC_MVS0 }, { "mvs1", GDSC_MVS1 } };
	for (unsigned i = 0; i < 3; i++) {
		uint32_t v = vrd(g[i].pa);
		printf("gdsc %-4s %#x = %#010x: %s, sw_collapse %u, hw_control %u\n", g[i].n, g[i].pa, v,
		       (v & PWR_ON) ? "PWR_ON" : "off", v & 1, (v >> 1) & 1);
	}
	for (unsigned i = 0; i < NBR; i++) {
		uint32_t v = vrd(branches[i].pa);
		printf("cbcr %-12s %#x = %#010x: %s, en %u\n", branches[i].name, branches[i].pa, v, onoff(v), v & 1);
	}
	uint32_t xo = vrd(VIDEO_CC_XO);
	printf("cbcr %-12s %#x = %#010x: %s\n", "xo", VIDEO_CC_XO, xo, onoff(xo));
}

static int probe(void)
{
	static const struct { const char *n; uint32_t pa; } g[] = {
		{ "gcc_video_ahb", GCC_VIDEO_AHB }, { "gcc_video_axi", GCC_VIDEO_AXI }, { "gcc_video_xo", GCC_VIDEO_XO } };
	for (unsigned i = 0; i < 3; i++) {
		uint32_t v = rd32(g[i].pa);
		printf("%-14s %#x = %#010x: %s, en %u\n", g[i].n, g[i].pa, v, gcc_state(v), v & 1);
	}
	uint32_t vote = rd32(GCC_TBU_SF_VOTE);
	printf("tbu sf vote %#x = %#010x (bit0 sw_collapse %u, PWR_ON %u); status %#x = %#010x %#010x\n",
	       GCC_TBU_SF_VOTE, vote, vote & 1, vote >> 31, TBU_SF_STATUS, rd32(TBU_SF_STATUS), rd32(TBU_SF_STATUS + 4));
	probe_smmu();
	probe_bcm();
	if (!ahb_on()) {
		printf("videocc skipped: AHB %s\n", gcc_state(rd32(GCC_VIDEO_AHB)));
		return 0;
	}
	probe_vcc();
	return 0;
}

/* ---- power ---- */

static int branch_on(uint32_t pa, const char *name)
{
	if (!(vrd(pa) & CBCR_EN))
		upd(pa, CBCR_EN, CBCR_EN, name);
	if (poll(pa, CLK_OFF, 0, 100)) {
		printf("venus: STOP: %s %#x still CLK_OFF after 100 ms (%#010x)\n", name, pa, vrd(pa));
		return -1;
	}
	printf("venus: %s on (%#010x)\n", name, vrd(pa));
	return 0;
}

static void branch_off(uint32_t pa, const char *name, int voted)
{
	if (!(vrd(pa) & CBCR_EN)) {
		printf("venus: %s already disabled (%#010x)\n", name, vrd(pa));
		return;
	}
	upd(pa, CBCR_EN, 0, name);
	if (voted)
		usleep(10);	/* clk-branch.c: voted branches only delay on disable */
	else if (poll(pa, CLK_OFF, CLK_OFF, 100))
		printf("venus: %s %#x CLK_OFF not set after 100 ms (%#010x), going on\n", name, pa, vrd(pa));
	printf("venus: %s off (%#010x)\n", name, vrd(pa));
}

static int gdsc_on(uint32_t pa, const char *name)
{
	uint32_t v = vrd(pa);
	if (v & HW_CONTROL) {	/* review 8.5.5: in HW mode the domain can collapse under us; downstream SW mode first */
		upd(pa, HW_CONTROL, 0, "gdsc SW mode");
		uint32_t n = vrd(pa);
		printf("venus: gdsc %s HW_CONTROL was set (%#010x): cleared (%#010x)\n", name, v, n);
		if (n & HW_CONTROL) {
			printf("venus: STOP: gdsc %s HW_CONTROL won't clear (%#010x)\n", name, n);
			return -1;
		}
		v = n;
	}
	if (!(v & PWR_ON) || (v & SW_COLLAPSE))
		upd(pa, SW_COLLAPSE, 0, name);
	if (poll(pa, PWR_ON, PWR_ON, 100)) {
		printf("venus: STOP: gdsc %s %#x no PWR_ON after 100 ms (%#010x)\n", name, pa, vrd(pa));
		return -1;
	}
	printf("venus: gdsc %s on (%#010x)\n", name, vrd(pa));
	return 0;
}

static void gdsc_off(uint32_t pa, const char *name)
{
	if (vrd(pa) & SW_COLLAPSE && !(vrd(pa) & PWR_ON)) {
		printf("venus: gdsc %s already off (%#010x)\n", name, vrd(pa));
		return;
	}
	upd(pa, SW_COLLAPSE, SW_COLLAPSE, name);
	if (poll(pa, PWR_ON, 0, 100))
		printf("venus: gdsc %s %#x still PWR_ON after 100 ms (%#010x), going on\n", name, pa, vrd(pa));
	else
		printf("venus: gdsc %s off (%#010x)\n", name, vrd(pa));
}

/* clk_fabia_pll_configure + alpha_pll_fabia_enable as camclk.c pll0_on, video_pll0_config (videocc-sdmmagpie.c:82-89) */
static int pll_on(uint32_t l)
{
	uint32_t m = vrd(PLL_MODE), op = vrd(PLL_OPMODE);
	if (m & VOTE_FSM_ENA) {
		printf("venus: STOP: pll0 MODE %#x in FSM vote mode\n", m);
		return -1;
	}
	if ((op & PLL_RUN) && (m & OUTCTRL)) {
		if ((vrd(PLL_L) & 0xffff) == l) {
			printf("venus: pll0 already running, L %#x (MODE %#x)\n", l, m);
			return 0;
		}
		printf("venus: STOP: pll0 running at L %#x, not %#x (MODE %#x)\n", vrd(PLL_L), l, m);
		return -1;
	}
	if (m & ~(RESET_N | UPDATE_BYPASS | LOCK_DET)) {	/* reset state 0 or our own 'off' state (as camclk) */
		printf("venus: STOP: pll0 MODE %#x not reset/standby\n", m);
		return -1;
	}
	/* configure; zero fields (frac, test_ctl) skipped as clk_alpha_pll_write_config */
	wr(PLL_L, l, "pll0 L");
	wr(PLL_CONFIG, 0x20485699, "pll0 CONFIG_CTL");
	wr(PLL_CONFIG_U, 0x00002067, "pll0 CONFIG_CTL_U");
	wr(PLL_USER, 0x00000001, "pll0 USER_CTL");
	wr(PLL_USER_U, 0x00004805, "pll0 USER_CTL_U");
	wr(PLL_TEST_U, 0x40000000, "pll0 TEST_CTL_U");
	upd(PLL_MODE, UPDATE_BYPASS, UPDATE_BYPASS, "pll0 UPDATE_BYPASS");
	upd(PLL_MODE, RESET_N, RESET_N, "pll0 RESET_N");
	/* enable */
	upd(PLL_MODE, OUTCTRL, 0, "pll0 OUTCTRL off");
	wr(PLL_OPMODE, 0, "pll0 OPMODE STANDBY");
	upd(PLL_MODE, RESET_N, RESET_N, "pll0 RESET_N");
	wr(PLL_OPMODE, PLL_RUN, "pll0 OPMODE RUN");
	if (poll(PLL_MODE, LOCK_DET, LOCK_DET, 100)) {
		printf("venus: STOP: pll0 no lock after 100 ms (MODE %#x), back to STANDBY\n", vrd(PLL_MODE));
		wr(PLL_OPMODE, 0, "pll0 OPMODE STANDBY");
		return -1;
	}
	upd(PLL_USER, PLL_OUT_MASK, PLL_OUT_MASK, "pll0 outputs");
	upd(PLL_MODE, OUTCTRL, OUTCTRL, "pll0 OUTCTRL");
	printf("venus: pll0 locked at %.1f MHz (MODE %#010x)\n", 19.2 * l, vrd(PLL_MODE));
	return 0;
}

static void pll_off(void)	/* alpha_pll_fabia_disable, as camclk.c pll0_off */
{
	uint32_t m = vrd(PLL_MODE);
	if (m & VOTE_FSM_ENA) {
		printf("venus: pll0 in FSM vote mode, left alone\n");
		return;
	}
	if (!(m & OUTCTRL) && !(vrd(PLL_OPMODE) & PLL_RUN)) {
		printf("venus: pll0 already off (MODE %#x)\n", m);
		return;
	}
	upd(PLL_MODE, OUTCTRL, 0, "pll0 OUTCTRL off");
	upd(PLL_USER, PLL_OUT_MASK, 0, "pll0 outputs off");
	wr(PLL_OPMODE, 0, "pll0 OPMODE STANDBY");
	printf("venus: pll0 off (MODE %#010x)\n", vrd(PLL_MODE));
}

/* clk_rcg2_configure (camclk.c mclk_24): src SEL, div field 2h-1, mode 0 (no MND), then CMD UPDATE */
static int rcg_set(uint32_t src, uint32_t divf, const char *why)
{
	uint32_t cfg = vrd(IRIS_CFG);
	cfg &= ~((0x7u << 8) | 0x1fu | (3u << 12) | (1u << 20));
	cfg |= src << 8 | divf;
	wr(IRIS_CFG, cfg, why);
	upd(IRIS_RCG, 1, 1, "iris rcg UPDATE");
	if (poll(IRIS_RCG, 1, 0, 100)) {
		printf("venus: STOP: iris rcg didn't update after 100 ms (CMD %#x, CFG %#x)\n", vrd(IRIS_RCG), vrd(IRIS_CFG));
		return -1;
	}
	printf("venus: iris rcg src %u div field %u (CMD %#010x CFG %#010x)\n", src, divf, vrd(IRIS_RCG), vrd(IRIS_CFG));
	return 0;
}

static int power_off(void);

static int gcc_on(uint32_t pa, const char *name, int must)
{
	if (!(rd32(pa) & CBCR_EN))
		upd(pa, CBCR_EN, CBCR_EN, name);
	for (int i = 0; i <= 100; i++) {
		if (!(rd32(pa) & CLK_OFF)) {
			printf("venus: %s on (%#010x)\n", name, rd32(pa));
			return 0;
		}
		delay_ms(1);
	}
	printf("venus: %s %#x still CLK_OFF after 100 ms (%#010x)%s\n", name, pa, rd32(pa),
	       must ? "" : ", checked again after the GDSCs");
	return -1;
}

/* gcc_video_ahb: clear HW_CTL (keep CLK_ENABLE) so CLK_OFF reflects the clock; the old value goes to AHB_STATE.
 * gcc_video_xo (0x10b028, en 0) stays untouched: no video node in sdmmagpie(-vidc).dtsi uses GCC_VIDEO_XO_CLK
 * (videocc runs from RPMh bi_tcxo, videocc-sdmmagpie.c:60,100; the venus PIL node takes VIDEO_CC_XO_CLK, dtsi:3408) */
static void ahb_hwctl_clear(void)
{
	uint32_t v = rd32(GCC_VIDEO_AHB);
	if (!(v & GCC_HW_CTL))
		return;
	if (getenv("VENUSCTL_KEEP_HWCTL")) {
		printf("venus: gcc_video_ahb HW_CTL set (%#010x), kept (VENUSCTL_KEEP_HWCTL)\n", v);
		return;
	}
	FILE *f = fopen(AHB_STATE, "w");
	if (f) {
		fprintf(f, "%#x\nboot %s\n", v, boot_id());
		fclose(f);
	} else {
		printf("venus: can't write %s, power off won't restore HW_CTL\n", AHB_STATE);
	}
	upd(GCC_VIDEO_AHB, GCC_HW_CTL | CBCR_EN, CBCR_EN, "gcc_video_ahb clear HW_CTL");
	printf("venus: gcc_video_ahb HW_CTL cleared (%#010x -> %#010x)\n", v, rd32(GCC_VIDEO_AHB));
}

/* power off: put HW_CTL back if power on cleared it */
static void ahb_hwctl_restore(void)
{
	unsigned v = 0;
	FILE *f = fopen(AHB_STATE, "r");
	if (!f)
		return;
	int n = fscanf(f, "%x", &v);
	fclose(f);
	unlink(AHB_STATE);
	if (n == 1 && (v & GCC_HW_CTL)) {
		upd(GCC_VIDEO_AHB, GCC_HW_CTL, GCC_HW_CTL, "gcc_video_ahb restore HW_CTL");
		printf("venus: gcc_video_ahb HW_CTL restored (%#010x)\n", rd32(GCC_VIDEO_AHB));
	}
}

static int power_on(void)
{
	for (unsigned i = 0; i < 2; i++)
		if (bcm(bcms[i], BCM_ON)) {
			printf("venus: STOP: bcm %s vote failed\n", bcms[i]);
			return power_off(), 1;
		}
	ahb_hwctl_clear();
	if (gcc_on(GCC_VIDEO_AHB, "gcc_video_ahb", 1))	/* BRANCH_HALT-style poll, as gcc_video_axi */
		return power_off(), 1;
	int axi_late = gcc_on(GCC_VIDEO_AXI, "gcc_video_axi", 0);	/* may need the video GDSCs to report on */

	uint32_t l = (rd32(EFUSE_VIDEO) & EFUSE_NO_MPIPE) ? 0x14 : 0x19;	/* videocc-sdmmagpie.c:356-358 */
	if (l != 0x19)
		printf("venus: efuse: multipipe disabled, PLL L %#x\n", l);
	if (pll_on(l) || rcg_set(1, 3, "iris rcg PLL0 main /2"))	/* F(240 MHz, PLL0_OUT_MAIN, 2): field 2*2-1 */
		return power_off(), 1;
	if (gdsc_on(GDSC_MVSC, "mvsc") || gdsc_on(GDSC_MVS0, "mvs0") || (cvp && gdsc_on(GDSC_MVS1, "mvs1")))
		return power_off(), 1;
	if (axi_late && gcc_on(GCC_VIDEO_AXI, "gcc_video_axi", 1))
		return power_off(), 1;
	for (unsigned i = 0; i < NBR; i++)
		if ((!branches[i].cvp || cvp) && branch_on(branches[i].pa, branches[i].name))
			return power_off(), 1;
	/* the core read: only with everything above on, checked once more */
	if (!core_ok()) {
		printf("venus: STOP: a clock or GDSC dropped before the HW version read\n");
		return power_off(), 1;
	}
	printf("venus: WRAPPER_HW_VERSION %#x = %#010x\n", WRAP_HW_VERSION, rd32(WRAP_HW_VERSION));
	return 0;
}

/* STATE's pas line: 1 = TZ holds an image (pas-check --no-shutdown, boot --keep), the firmware may run */
static int state_pas(void)
{
	int p = 0;
	char line[256];
	FILE *f = fopen(STATE, "r");
	if (!f)
		return 0;
	while (fgets(line, sizeof(line), f))
		if (sscanf(line, "pas %d", &p) == 1)
			break;
	fclose(f);
	return p;
}

static int power_off(void)
{
	/* review 8.5.2: collapsing GDSCs/clocks/BCMs under a live ARM9 hangs the bus */
	if (state_pas()) {
		if (!force) {
			printf("venus: refused: %s says pas 1 (TZ image loaded, firmware may run): venusctl shutdown first,"
			       " or power off --force\n", STATE);
			return 1;
		}
		printf("venus: --force: powering off with %s pas 1\n", STATE);
	}
	if (ahb_on()) {
		for (unsigned i = NBR; i-- > 0;)
			if (!branches[i].cvp || cvp || (vrd(branches[i].pa) & CBCR_EN))
				branch_off(branches[i].pa, branches[i].name, branches[i].voted);
		if (cvp || (vrd(GDSC_MVS1) & PWR_ON))
			gdsc_off(GDSC_MVS1, "mvs1");
		gdsc_off(GDSC_MVS0, "mvs0");
		gdsc_off(GDSC_MVSC, "mvsc");
		if (!(vrd(PLL_MODE) & VOTE_FSM_ENA) && ((vrd(IRIS_CFG) >> 8) & 7) == 1)
			rcg_set(0, 1, "iris rcg park on XO");	/* downstream enable_safe_config: off the PLL before it stops */
		pll_off();
	} else {
		printf("venus: videocc skipped: AHB off\n");
	}
	if (rd32(GCC_VIDEO_AXI) & CBCR_EN) {
		upd(GCC_VIDEO_AXI, CBCR_EN, 0, "gcc_video_axi");
		printf("venus: gcc_video_axi off (%#010x)\n", rd32(GCC_VIDEO_AXI));
	}
	/* gcc_video_ahb stays enabled: critical in mainline gcc-sm7150, and the GDSC parent; only HW_CTL goes back */
	ahb_hwctl_restore();
	int rc = 0;
	for (unsigned i = 2; i-- > 0;)
		rc |= bcm(bcms[i], BCM_OFF);
	return rc ? 1 : 0;
}

/* ---- PAS check (adspboot.c boot(): metadata = ELF header + phdrs + hash segment) ---- */

struct phdr { uint32_t type, offset, vaddr, paddr, filesz, memsz, flags, align; };

static int64_t scm(uint32_t cmd, uint64_t arginfo, uint64_t a, uint64_t b, uint64_t c, uint64_t *r1)
{
	uint64_t x[8] = { SCM_PIL(cmd), arginfo, a, b, c, 0, 0, 0 };
	pm_smc(x);
	if (r1)
		*r1 = x[1];
	return (int64_t)x[0];
}

static uint8_t *slurp(const char *path, size_t *len)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	*len = (size_t)ftell(f);
	fseek(f, 0, SEEK_SET);
	uint8_t *b = calloc(1, *len + 4);
	if (b && fread(b, 1, *len, f) != *len) {
		free(b);
		b = NULL;
	}
	fclose(f);
	return b;
}

/* venus.mdt and its metadata (ELF header + phdrs + hash segment), shared by pas-check and boot */
struct fwimg {
	uint8_t *mdt, *meta;
	size_t mlen, meta_len;
	struct phdr *ph;
	uint32_t phnum;
	int hash;
};

static int fw_meta(const char *fw, struct fwimg *f)
{
	char path[256];
	snprintf(path, sizeof(path), "%s/venus.mdt", fw);
	uint8_t *mdt = slurp(path, &f->mlen);
	if (!mdt) {
		fprintf(stderr, "venus: no %s\n", path);
		return 1;
	}
	uint32_t phoff, phnum = 0;
	memcpy(&phoff, mdt + 0x1c, 4);
	memcpy(&phnum, mdt + 0x2c, 2);
	if (memcmp(mdt, "\177ELF", 4) || phoff + 32ull * phnum > f->mlen) {
		fprintf(stderr, "venus: %s is not an ELF32 image\n", path);
		return 1;
	}
	struct phdr *ph = (struct phdr *)(mdt + phoff);
	int hash = -1;
	for (uint32_t i = 0; i < phnum; i++)
		if (((ph[i].flags >> 24) & 7) == 2) {
			hash = (int)i;
			break;
		}
	if (hash < 0) {
		fprintf(stderr, "venus: no hash segment in %s\n", path);
		return 1;
	}
	size_t ehsz = phoff + 32ull * phnum, meta_len = ehsz + ph[hash].filesz;
	uint8_t *meta = calloc(1, meta_len + 4);
	memcpy(meta, mdt, ehsz);
	if (ph[hash].offset + ph[hash].filesz <= f->mlen) {
		memcpy(meta + ehsz, mdt + ph[hash].offset, ph[hash].filesz);
	} else {
		size_t hl;
		snprintf(path, sizeof(path), "%s/venus.b%02d", fw, hash);
		uint8_t *h = slurp(path, &hl);
		if (!h || hl < ph[hash].filesz) {
			fprintf(stderr, "venus: hash segment %s missing\n", path);
			return 1;
		}
		memcpy(meta + ehsz, h, ph[hash].filesz);
		free(h);
	}
	printf("venus: %s/venus.mdt: %u phdrs, hash seg %d (%u bytes), metadata %zu bytes\n", fw, phnum, hash,
	       ph[hash].filesz, meta_len);
	*f = (struct fwimg){ .mdt = mdt, .meta = meta, .mlen = f->mlen, .meta_len = meta_len, .ph = ph,
		.phnum = phnum, .hash = hash };
	return 0;
}

static int pas_check(const char *fw, int no_shutdown)
{
	if (!ahb_on() || !(vrd(GDSC_MVSC) & PWR_ON) || !(vrd(GDSC_MVS0) & PWR_ON)) {
		printf("venus: refused: power is off (gcc_video_ahb %#010x%s): run 'venusctl power on' first\n",
		       rd32(GCC_VIDEO_AHB), ahb_on() ? ", or gdsc mvsc/mvs0 off" : "");
		return 1;
	}
	struct fwimg f;
	if (fw_meta(fw, &f))
		return 1;
	size_t meta_len = f.meta_len;
	uint8_t *meta = f.meta;

	uint64_t r1 = 0;
	int64_t rc = scm(7, 1, PAS_VENUS, 0, 0, &r1);
	printf("venus: pas supported(%d) -> %lld/%llx\n", PAS_VENUS, (long long)rc, (unsigned long long)r1);

	uint64_t meta_pa = pm_alloc(meta_len, 0x1000);
	if (!meta_pa)
		return 1;
	pm_wr(meta_pa, meta, (meta_len + 3) & ~(size_t)3);
	printf("venus: pas init_image(%d, %llx)...\n", PAS_VENUS, (unsigned long long)meta_pa);
	rc = scm(1, 2 | (2u << 6), PAS_VENUS, meta_pa, 0, &r1);
	printf("venus: pas init_image -> %lld/%llx\n", (long long)rc, (unsigned long long)r1);
	pm_free(meta_pa);
	int init_ok = !rc && !r1;
	if (no_shutdown && init_ok) {	/* review 8.5.8: record it, so shutdown can undo it */
		FILE *sf = fopen(STATE, "w");
		if (sf) {
			fprintf(sf, "boot %s\npas 1\nq 0 0\ninit 0\nsysinit 0\n", boot_id());
			fclose(sf);
			printf("venus: %s: pas 1 (venusctl shutdown undoes it)\n", STATE);
		} else {
			printf("venus: can't write %s: a PAS shutdown(9) is owed, nothing records it\n", STATE);
		}
	}
	/* adspboot adsp3: a shutdown with nothing in TZ blocked forever, so only after an accepted init */
	if (no_shutdown || !init_ok) {
		printf("venus: pas shutdown skipped (%s)\n", no_shutdown ? "--no-shutdown" : "init failed");
		return init_ok ? 0 : 1;
	}
	printf("venus: pas shutdown(%d)...\n", PAS_VENUS);
	rc = scm(6, 1, PAS_VENUS, 0, 0, &r1);
	printf("venus: pas shutdown -> %lld/%llx\n", (long long)rc, (unsigned long long)r1);
	return rc || r1 ? 1 : 0;
}

/* ---- stage 2 steps 4-6: state shared by boot --keep, init, ping and shutdown (separate runs) ---- */

struct vstate {
	int pas;		/* init_image accepted: PAS shutdown(9) owed */
	int nsmr;
	struct smmu_saved smr[4];	/* what smmu_identity wrote, for smmu_release */
	uint32_t sid[4];		/* the SID each smr[] entry was written for */
	uint64_t q;		/* HFI block (QSHARED) */
	int qkind;		/* 1 PMEM_ALLOC32 (pm_free), 2 --phys region */
	int init, sysinit;
};

static void state_save(const struct vstate *v)
{
	FILE *f = fopen(STATE ".new", "w");
	if (!f) {
		printf("venus: can't write %s: shutdown won't know what to undo\n", STATE);
		return;
	}
	fprintf(f, "boot %s\npas %d\nq %#llx %d\ninit %d\nsysinit %d\n", boot_id(), v->pas, (unsigned long long)v->q, v->qkind, v->init,
		v->sysinit);
	for (int i = 0; i < v->nsmr; i++) {
		const struct smmu_saved *m = &v->smr[i];
		fprintf(f, "smr %d %d %#llx %#llx %#x %#x %#x %#x %#x %#x\n", m->slot, m->cb, (unsigned long long)m->cbp,
			(unsigned long long)m->gr1, m->smr, m->s2cr, m->cbar, m->cba2r, m->sctlr, v->sid[i]);
	}
	fclose(f);
	rename(STATE ".new", STATE);
}

static int state_load(struct vstate *v)
{
	memset(v, 0, sizeof(*v));
	FILE *f = fopen(STATE, "r");
	if (!f)
		return -1;
	char line[256];
	while (fgets(line, sizeof(line), f)) {
		unsigned long long a, b;
		struct smmu_saved m;
		if (sscanf(line, "pas %d", &v->pas) == 1 || sscanf(line, "init %d", &v->init) == 1 ||
		    sscanf(line, "sysinit %d", &v->sysinit) == 1)
			continue;
		if (sscanf(line, "q %llx %d", &a, &v->qkind) == 2) {
			v->q = a;
			continue;
		}
		unsigned sid;
		if (v->nsmr < 4 && sscanf(line, "smr %d %d %llx %llx %x %x %x %x %x %x", &m.slot, &m.cb, &a, &b, &m.smr,
					  &m.s2cr, &m.cbar, &m.cba2r, &m.sctlr, &sid) == 10) {
			m.cbp = a;
			m.gr1 = b;
			v->sid[v->nsmr] = sid;
			v->smr[v->nsmr++] = m;
		}
	}
	fclose(f);
	return 0;
}

static int64_t pas(uint32_t cmd, const char *what)
{
	uint64_t r1 = 0;
	printf("venus: pas %s(%d)...\n", what, PAS_VENUS);
	int64_t rc = scm(cmd, 1, PAS_VENUS, 0, 0, &r1);
	printf("venus: pas %s -> %lld/%llx\n", what, (long long)rc, (unsigned long long)r1);
	return rc ? rc : (int64_t)r1;
}

static void print_sfr(uint64_t q)
{
	char s[SFR_SIZE + 1];
	uint32_t sz = rd32(q + SFR_OFF);
	memset(s, 0, sizeof(s));
	pm_rd(q + SFR_OFF + 4, s, SFR_SIZE - 4);
	for (unsigned i = 0; i < SFR_SIZE - 4 && s[i]; i++)
		if (s[i] < 0x20 || s[i] > 0x7e)
			s[i] = '.';
	printf("venus: SFR %#llx bufSize %u: \"%s\"\n", (unsigned long long)(q + SFR_OFF), sz, s);
}

/* the undo for boot/init, in reverse: PAS shutdown (only powered: TZ touches the core), queue memory, SMRs */
static int teardown(struct vstate *v)
{
	if (v->pas) {
		if (!core_ok()) {
			printf("venus: STOP: PAS shutdown needs the power on (a clock or GDSC is off): nothing undone,"
			       " state kept in %s\n", STATE);
			return 1;
		}
		if (pas(6, "shutdown")) {
			printf("venus: STOP: PAS shutdown failed: queue memory and SMRs kept (the firmware may still run),"
			       " state kept in %s\n", STATE);
			return 1;
		}
		v->pas = 0;
		v->init = v->sysinit = 0;
		state_save(v);
	}
	if (v->q) {
		if (v->qkind == 1 && pm_free(v->q))
			printf("venus: pm_free %#llx failed\n", (unsigned long long)v->q);
		else
			printf("venus: queue block %#llx %s\n", (unsigned long long)v->q, v->qkind == 1 ? "freed" :
			       "(--phys region) given up");
		v->q = 0;
		state_save(v);
	}
	int rc = 0;
	while (v->nsmr > 0) {
		/* review 8.5.1: only a slot that still holds exactly what we wrote (WLAN/ADSP may own it now) */
		const struct smmu_saved *m = &v->smr[--v->nsmr];
		uint32_t sid = v->sid[v->nsmr], smr = rd32(0x15000800ull + 4 * m->slot), s2cr = rd32(0x15000c00ull + 4 * m->slot);
		if (smr != ((1u << 31) | sid) || ((s2cr >> 16) & 3) || (s2cr & 0xff) != (uint32_t)m->cb) {
			printf("venus: SMR%d left alone: reads SMR %08x S2CR %08x, we wrote SMR %08x S2CR -> CB%d (not ours now)\n",
			       m->slot, smr, s2cr, (1u << 31) | sid, m->cb);
		} else {
			smmu_last = *m;
			int r = smmu_release("venus");
			/* v2c: SMR invalid (r 1) = released, the other differences only warned (after PAS shutdown
			 * the bank registers read changed every run); a still-valid SMR stays a failure.
			 * VENUSCTL_STRICT_RELEASE=1: any difference fails (old behaviour). */
			if (r > 0 && !getenv("VENUSCTL_STRICT_RELEASE")) {
				printf("venus: warning: SMR%d released (reads invalid), other registers differ (above)\n",
				       m->slot);
				r = 0;
			}
			rc |= r != 0;
		}
		state_save(v);
	}
	unlink(STATE);
	return rc ? 1 : 0;
}

/* review 8.5.4: SIDs 0x1000-0x13ff go through mnoc_sf_0_tbu, powered only by someone else's vote (ours = collapse).
 * Re-read right before the firmware starts DMA; no vote of our own. The status words are printed only (their bits
 * are not known). VENUSCTL_NO_TBUCHECK=1: skip the check. 0 = on. */
static int tbu_ok(const char *when)
{
	uint32_t vote = rd32(GCC_TBU_SF_VOTE);
	if (vote & PWR_ON)
		return 0;
	printf("venus: %s mnoc_sf_0_tbu vote %#x = %#010x has no PWR_ON (status %#010x %#010x) before %s\n",
	       getenv("VENUSCTL_NO_TBUCHECK") ? "note (VENUSCTL_NO_TBUCHECK):" : "STOP:", GCC_TBU_SF_VOTE, vote,
	       rd32(TBU_SF_STATUS), rd32(TBU_SF_STATUS + 4), when);
	return getenv("VENUSCTL_NO_TBUCHECK") ? 0 : -1;
}

/* review 8.5.6: a block the firmware may DMA into: 1 MB aligned, in the free gap 0xa5030000-0xbcc00000 and inside
 * the venus_ns pool [0x25800000, 0xe0000000). 0 = ok. */
static int range_ok(uint64_t a, uint64_t len, const char *what)
{
	if (!(a & 0xfffff) && len && a >= CP_SIZE && a + len <= POOL_END && a >= FIXED_LO && a + len <= FIXED_HI)
		return 0;
	printf("venus: refused: %s %#llx+%#llx: needs 1 MB alignment, inside %#llx-%#llx (and %#x-%#llx)\n", what,
	       (unsigned long long)a, (unsigned long long)len, FIXED_LO, FIXED_HI, CP_SIZE, POOL_END);
	return -1;
}

static void core_status(const char *when)
{
	uint32_t st = crd(CTRL_STATUS);
	printf("venus: %s: CTRL_STATUS %#010x (init %u error %#x pc_ready %u) VERSION_INFO %#010x HW_VERSION %#010x"
	       " CPU_STATUS %#010x INTR_STATUS %#010x\n", when, st, st & ST_INIT, st & ST_ERROR, (st >> 8) & 1,
	       crd(VERSION_INFO), crd(WRAP_HW_VERSION), crd(WRAP_CPU_STATUS), crd(WRAP_INTR_STATUS));
}

static int boot_fail(struct vstate *v)
{
	printf("venus: boot failed, undoing\n");
	teardown(v);
	return 1;
}

static int boot(const char *fw, int keep)
{
	struct vstate v;
	if (state_load(&v) == 0) {
		printf("venus: refused: %s exists (a boot --keep is active): venusctl shutdown first\n", STATE);
		return 1;
	}
	if (!core_ok()) {
		printf("venus: refused: power is off (a video clock or GDSC is off): run 'venusctl power on' first\n");
		return 1;
	}
	uint32_t vote = rd32(GCC_TBU_SF_VOTE);
	if (!(vote & PWR_ON)) {	/* SIDs 0x1000-0x13ff go through mnoc_sf_0_tbu (camera-sid-cc0.md) */
		printf("venus: refused: mnoc_sf_0_tbu vote %#x = %#010x has no PWR_ON\n", GCC_TBU_SF_VOTE, vote);
		return 1;
	}
	struct fwimg f;
	if (fw_meta(fw, &f))
		return 1;
	/* load segments: span and relocation as adspboot boot() */
	uint64_t lo = ~0ull, hi = 0;
	int reloc = 0;
	for (uint32_t i = 0; i < f.phnum; i++) {
		if ((int)i == f.hash || f.ph[i].type != 1 || !f.ph[i].memsz)
			continue;
		reloc |= !!(f.ph[i].flags & MDT_RELOC);
		if (f.ph[i].paddr < lo)
			lo = f.ph[i].paddr;
		if (f.ph[i].paddr + f.ph[i].memsz > hi)
			hi = f.ph[i].paddr + f.ph[i].memsz;
	}
	uint64_t base = reloc ? FW_PA : lo;
	if (lo > hi || base < FW_PA || base + (hi - lo) > FW_PA + FW_SIZE) {
		printf("venus: refused: image %llx-%llx (reloc %d) does not fit %llx+%llx\n", (unsigned long long)lo,
		       (unsigned long long)hi, reloc, FW_PA, FW_SIZE);
		return 1;
	}
	printf("venus: image linked %llx-%llx, %s to %llx\n", (unsigned long long)lo, (unsigned long long)hi,
	       reloc ? "relocated" : "fixed", (unsigned long long)base);

	/* exact SMRs only; refuse if any SMR already matches one of the SIDs (not ours to change) */
	unsigned nsid = getenv("VENUSCTL_SID1") ? 1 : 4;
	uint64_t sm = 0x15000000ull;
	uint32_t nsmr = rd32(sm + 0x20) & 0xff;
	for (uint32_t i = 0; i < nsmr && i < 128; i++) {
		uint32_t smr = rd32(sm + 0x800 + 4 * i);
		for (unsigned k = 0; k < nsid && (smr >> 31); k++)
			if (sid_match(smr, venus_sids[k])) {
				printf("venus: refused: SMR%u %08x already matches SID %#x\n", i, smr, venus_sids[k]);
				return 1;
			}
	}
	for (unsigned k = 0; k < nsid; k++) {
		smmu_last.slot = -1;	/* a failed call leaves smmu_last alone: don't record the previous SID twice */
		int bad = smmu_identity("venus", venus_sids[k], 0);
		if (smmu_last.slot >= 0) {
			v.sid[v.nsmr] = venus_sids[k];
			v.smr[v.nsmr++] = smmu_last;
			state_save(&v);
			printf("venus: SID %#x: SMR%d + CB%d added\n", venus_sids[k], smmu_last.slot, smmu_last.cb);
		}
		if (bad)
			return boot_fail(&v);
	}

	/* clock_config_on_enable_vpu5 (venus_hfi.c:4703-4707), part of downstream __venus_power_on before the PIL load */
	if (!getenv("VENUSCTL_NO_CLKCFG")) {
		wr(WRAP_CPU_CGCDIS, 0, "CPU_CGC_DIS");
		wr(WRAP_CPU_CLKCFG, 0, "CPU_CLOCK_CONFIG");
		printf("venus: CPU_CGC_DIS %#010x CPU_CLOCK_CONFIG %#010x\n", crd(WRAP_CPU_CGCDIS), crd(WRAP_CPU_CLKCFG));
	}

	uint64_t r1 = 0;
	int64_t rc = scm(7, 1, PAS_VENUS, 0, 0, &r1);
	printf("venus: pas supported(%d) -> %lld/%llx\n", PAS_VENUS, (long long)rc, (unsigned long long)r1);
	uint64_t meta_pa = pm_alloc(f.meta_len, 0x1000);
	if (!meta_pa)
		return boot_fail(&v);
	pm_wr(meta_pa, f.meta, (f.meta_len + 3) & ~(size_t)3);
	printf("venus: pas init_image(%d, %llx)...\n", PAS_VENUS, (unsigned long long)meta_pa);
	rc = scm(1, 2 | (2u << 6), PAS_VENUS, meta_pa, 0, &r1);
	printf("venus: pas init_image -> %lld/%llx\n", (long long)rc, (unsigned long long)r1);
	pm_free(meta_pa);
	if (rc || r1)
		return boot_fail(&v);	/* nothing in TZ: no shutdown (adspboot adsp3) */
	v.pas = 1;
	state_save(&v);
	if (reloc) {
		rc = scm(2, 3, PAS_VENUS, FW_PA, FW_SIZE, &r1);
		printf("venus: pas mem_setup %llx+%llx -> %lld/%llx\n", FW_PA, FW_SIZE, (long long)rc,
		       (unsigned long long)r1);
		if (rc || r1)
			return boot_fail(&v);
	}
	static uint8_t zeros[0x10000];
	uint64_t total = 0;
	for (uint32_t i = 0; i < f.phnum; i++) {
		if ((int)i == f.hash || f.ph[i].type != 1 || !f.ph[i].memsz)
			continue;
		uint64_t pa = base + (f.ph[i].paddr - lo);
		if (f.ph[i].filesz) {
			char path[256];
			size_t sl;
			snprintf(path, sizeof(path), "%s/venus.b%02u", fw, i);
			uint8_t *seg = slurp(path, &sl);
			if (!seg || sl < f.ph[i].filesz) {
				printf("venus: %s missing or short\n", path);
				return boot_fail(&v);
			}
			if (pm_wr(pa, seg, (f.ph[i].filesz + 3) & ~3u))
				return boot_fail(&v);
			free(seg);
			total += f.ph[i].filesz;
		}
		for (uint64_t o = (f.ph[i].filesz + 3) & ~3ull; o < f.ph[i].memsz; o += sizeof(zeros)) {
			uint64_t n = f.ph[i].memsz - o < sizeof(zeros) ? f.ph[i].memsz - o : sizeof(zeros);
			if (pm_wr(pa + o, zeros, (n + 3) & ~3ull))
				return boot_fail(&v);
		}
		printf("venus: seg %u -> %llx (%u file, %u mem)\n", i, (unsigned long long)pa, f.ph[i].filesz,
		       f.ph[i].memsz);
	}
	printf("venus: loaded %llu bytes\n", (unsigned long long)total);
	if (!core_ok()) {	/* auth_and_reset unpowered resets the phone (notes section 5) */
		printf("venus: STOP: a clock or GDSC dropped before auth_and_reset\n");
		return boot_fail(&v);
	}
	if (pas(5, "auth_and_reset"))
		return boot_fail(&v);
	if (!getenv("VENUSCTL_NO_MEMPROT")) {
		uint64_t x[8] = { SCM_MP_VIDEO_VAR, 4, 0, CP_SIZE, CP_NP_START, CP_NP_SIZE, 0, 0 };
		printf("venus: mem protect video var (0, %#x, %#x, %#x)...\n", CP_SIZE, CP_NP_START, CP_NP_SIZE);
		pm_smc(x);
		printf("venus: mem protect -> %lld/%llx\n", (long long)x[0], (unsigned long long)x[1]);
		if (x[0])
			return boot_fail(&v);
	}
	core_status("after auth");
	if (keep) {
		printf("venus: boot ok, kept (state %s): next venusctl init, or venusctl shutdown\n", STATE);
		return 0;
	}
	int r = teardown(&v);
	printf("venus: boot ok, undone -> %s\n", r ? "teardown FAILED" : "ok");
	return r;
}

/* ---- HFI queues (downstream __write_queue / __read_queue on the uncached block) ---- */

static uint64_t qb;	/* the block, from STATE */

static uint64_t qhdr(unsigned i)
{
	return qb + QHDR0 + QHDR_SIZE * i;
}

static uint64_t qstart(unsigned i)
{
	return qb + QTBL_SIZE + (uint64_t)QSIZE * i;
}

/* qhdr fields (struct hfi_queue_header) */
#define QH_RX_REQ	0x20
#define QH_TX_REQ	0x24
#define QH_READ		0x30
#define QH_WRITE	0x34

static int q_write(const uint32_t *w)
{
	uint32_t n = w[0] / 4, rd = rd32(qhdr(0) + QH_READ), wi = rd32(qhdr(0) + QH_WRITE);
	if (!n || rd >= QWORDS || wi >= QWORDS) {
		printf("venus: STOP: cmdq bad (size %u, read %u, write %u)\n", w[0], rd, wi);
		return -1;
	}
	uint32_t empty = wi >= rd ? QWORDS - (wi - rd) : rd - wi;
	if (empty <= n) {
		wr32(qhdr(0) + QH_TX_REQ, 1);
		printf("venus: STOP: cmdq full (%u words free, %u needed)\n", empty, n);
		return -1;
	}
	wr32(qhdr(0) + QH_TX_REQ, 0);
	uint32_t first = n < QWORDS - wi ? n : QWORDS - wi;
	pm_wr(qstart(0) + 4ull * wi, w, 4 * first);
	if (first < n)
		pm_wr(qstart(0), w + first, 4 * (n - first));
	wr32(qhdr(0) + QH_WRITE, (wi + n) % QWORDS);
	uint32_t rx = rd32(qhdr(0) + QH_RX_REQ);
	printf("venus: cmd %#x size %u at %u, rx_req %u%s\n", w[1], w[0], wi, rx, rx == 1 ? ", doorbell" : "");
	if (rx == 1) {	/* __iface_cmdq_write: interrupt only if the firmware asked for one */
		if (tbu_ok("doorbell"))
			return -1;
		wr(IC_SOFTINT, SOFTINT_H2A, "doorbell");
	}
	return 0;
}

static uint32_t pkt[3072];	/* VIDC_IFACEQ_VAR_HUGE_PKT_SIZE */

/* one packet from queue i (1 msg, 2 debug) into pkt: words, 0 = empty, -1 = bad */
static int q_read(unsigned i)
{
	uint32_t rd = rd32(qhdr(i) + QH_READ), wi = rd32(qhdr(i) + QH_WRITE), want_rx = i == 1;
	if (rd == wi) {
		wr32(qhdr(i) + QH_RX_REQ, want_rx);
		return 0;
	}
	if (rd >= QWORDS || wi >= QWORDS) {
		printf("venus: STOP: queue %u bad indexes (read %u, write %u)\n", i, rd, wi);
		return -1;
	}
	uint32_t size = rd32(qstart(i) + 4ull * rd), n = size / 4, nr;
	int got = (int)n;
	if (!n) {
		printf("venus: queue %u: zero packet size at %u\n", i, rd);
		return -1;
	}
	if (size <= sizeof(pkt)) {
		uint32_t first = n < QWORDS - rd ? n : QWORDS - rd;
		pm_rd(qstart(i) + 4ull * rd, pkt, 4 * first);
		if (first < n)
			pm_rd(qstart(i), pkt + first, 4 * (n - first));
		nr = (rd + n) % QWORDS;
	} else {
		printf("venus: queue %u: bad packet size %u at %u, dropped to the write index\n", i, size, rd);
		nr = wi;
		got = -1;
	}
	wr32(qhdr(i) + QH_RX_REQ, nr != wi ? 0 : want_rx);
	wr32(qhdr(i) + QH_READ, nr);
	return got;
}

static void print_pkt(unsigned q, int n)
{
	printf("venus: %s pkt type %#x size %u:", q == 1 ? "msgq" : "dbgq", pkt[1], pkt[0]);
	for (int k = 0; k < n && k < 8; k++)
		printf(" %08x", pkt[k]);
	printf("\n");
	if (pkt[1] == 0x20004 && n > 6) {	/* HFI_MSG_SYS_DEBUG: msg_type, msg_size, timestamp, text */
		char s[sizeof(pkt)];
		size_t len = 4u * (unsigned)n - 24;
		if (pkt[3] < len)
			len = pkt[3];
		memcpy(s, pkt + 6, len);
		s[len] = 0;
		for (size_t k = 0; k < len && s[k]; k++)
			if (s[k] < 0x20 || s[k] > 0x7e)
				s[k] = ' ';
		printf("venus: fw [%#x]: %s\n", pkt[2], s);
	}
}

static int wait_soft;	/* v2c: a timeout is reported as a note, not a STOP (ping) */
static uint32_t lastmsg[8];	/* v2c: first words of the last msgq packet, for a non-fatal ping */
static int lastmsg_n;

/* drain the debug queue, then the message queue until a packet of type want (0 = just drain); 0 = got it */
static int wait_msg(uint32_t want, unsigned ms)
{
	for (unsigned t = 0; t <= ms; t++) {
		int n;
		while ((n = q_read(2)) > 0)
			print_pkt(2, n);
		while ((n = q_read(1)) > 0) {
			print_pkt(1, n);
			lastmsg_n = n < 8 ? n : 8;
			memcpy(lastmsg, pkt, 4u * (unsigned)lastmsg_n);
			if (pkt[1] == 0x21001 && n > 3 && pkt[3] == 1) {	/* EVENT_NOTIFY, HFI_EVENT_SYS_ERROR */
				printf("venus: STOP: firmware sys error (data %#x %#x)\n", pkt[4], pkt[5]);
				print_sfr(qb);
				return -1;
			}
			if (want && pkt[1] == want)
				return 0;
		}
		if (n < 0)
			return -1;
		if (t < ms)
			delay_ms(1);
	}
	if (!want)
		return 0;
	printf("venus: %s no %#x after %u ms\n", wait_soft ? "note:" : "STOP:", want, ms);
	core_status("timeout");
	print_sfr(qb);
	return -1;
}

static int init(uint64_t phys)
{
	struct vstate v;
	if (state_load(&v) || !v.pas) {
		printf("venus: refused: no running boot (venusctl boot --keep first)\n");
		return 1;
	}
	if (v.q) {
		printf("venus: refused: queues already set up at %#llx (venusctl shutdown first)\n",
		       (unsigned long long)v.q);
		return 1;
	}
	if (!core_ok()) {
		printf("venus: refused: a video clock or GDSC is off\n");
		return 1;
	}
	core_status("before init");
	uint64_t q;
	if (phys) {
		if (range_ok(phys, QSHARED + SFR_SIZE, "--phys"))	/* 3 MB + the SFR page, as the review asks */
			return 1;
		q = phys;
		v.qkind = 2;
	} else {
		q = pm_alloc32(QSHARED, 0x100000);	/* UC region address is bits 31:20 (vidc_hfi_io.h HFI_DSP_UC_REGION_ADDR) */
		if (!q) {
			printf("venus: STOP: PMEM_ALLOC32 failed: rerun with --phys ADDR (1 MB aligned, %#llx-%#llx)\n",
			       FIXED_LO, FIXED_HI);
			return 1;
		}
		v.qkind = 1;
	}
	v.q = q;
	state_save(&v);
	/* device addresses are physical (identity bank) and 32-bit; below CP_SIZE is the content-protected range */
	if (q < CP_SIZE || q + QSHARED > POOL_END) {
		printf("venus: STOP: queue block %#llx outside %#x-%#llx\n", (unsigned long long)q, CP_SIZE, POOL_END);
		return 1;
	}
	qb = q;
	static uint8_t zeros[0x10000];
	for (uint32_t o = 0; o < QSHARED; o += sizeof(zeros))
		if (pm_wr(q + o, zeros, sizeof(zeros)))
			return 1;
	/* __interface_queues_init (venus_hfi.c:1898-2052) */
	uint32_t t[QTBL_SIZE / 4];
	memset(t, 0, sizeof(t));
	t[0] = 0;		/* qtbl_version */
	t[1] = QTBL_SIZE;
	t[2] = QHDR0;
	t[3] = QHDR_SIZE;
	t[4] = 3;		/* num_q */
	t[5] = 3;		/* num_active_q */
	/* t[6..7] device_addr: a host pointer downstream, 0 here */
	memcpy(&t[8], "msm_v4l2_vidc", 14);
	for (unsigned i = 0; i < 3; i++) {
		uint32_t *h = &t[(QHDR0 + QHDR_SIZE * i) / 4];	/* __set_queue_hdr_defaults */
		h[0] = 1;				/* status */
		h[1] = (uint32_t)qstart(i);		/* start_addr = device address */
		h[2] = 0x01010000u | i;			/* VIDC_IFACEQ_DFLT_QHDR | queue id */
		h[3] = QSIZE / 4;
		h[6] = 1;				/* rx_wm */
		h[7] = 1;				/* tx_wm */
		h[8] = i == 2 ? 0 : 1;			/* rx_req: 0 on the debug queue */
	}
	pm_wr(q, t, sizeof(t));
	wr32(q + SFR_OFF, SFR_SIZE);	/* hfi_sfr_struct bufSize */
	printf("venus: queue block %#llx (%s): table %#x, cmdq %#llx msgq %#llx dbgq %#llx, SFR %#llx\n",
	       (unsigned long long)q, v.qkind == 1 ? "PMEM_ALLOC32" : "--phys", QTBL_SIZE, (unsigned long long)qstart(0),
	       (unsigned long long)qstart(1), (unsigned long long)qstart(2), (unsigned long long)(q + SFR_OFF));
	/* __setup_ucregion_memory_map (venus_hfi.c:1881-1896) + setup_dsp_uc_memmap_vpu5 (4685-4692); no QDSS */
	wr(UC_ADDR, (uint32_t)q, "UC_REGION_ADDR");
	wr(UC_SIZE, QSHARED, "UC_REGION_SIZE");
	wr(QTBL_ADDR, (uint32_t)q, "QTBL_ADDR");
	wr(QTBL_INFO, 1, "QTBL_INFO");
	wr(SFR_ADDR, (uint32_t)(q + SFR_OFF), "SFR_ADDR");
	wr(CS_VMIMSG, (uint32_t)q, "DSP_QTBL_ADDR");
	wr(CS_VMIMSGAG0, (uint32_t)q, "DSP_UC_REGION_ADDR");
	wr(CS_VMIMSGAG1, QSHARED, "DSP_UC_REGION_SIZE");
	/* __boot_firmware (venus_hfi.c:1265-1290): CTRL_INIT = 1 (no CVP domain in the DT), wait for a status */
	if (tbu_ok("CTRL_INIT"))
		return 1;
	wr(CTRL_INIT, 1, "CTRL_INIT");
	uint32_t st = 0;
	unsigned ms;
	for (ms = 0; ms <= 1000; ms++) {
		st = crd(CTRL_STATUS);
		if (st & (ST_INIT | ST_ERROR))
			break;
		delay_ms(1);
	}
	v.init = (st & ST_INIT) && !(st & ST_ERROR);
	state_save(&v);
	printf("venus: CTRL_STATUS %#010x after %u ms: init %u, error %#x%s\n", st, ms, st & ST_INIT, st & ST_ERROR,
	       (st & ST_ERROR) == 4 ? " (invalid UC_REGION)" : "");
	core_status("after init");
	if (!v.init) {
		printf("venus: STOP: %s\n", st & ST_ERROR ? "firmware reports an error" : "no init status after 1 s");
		print_sfr(q);
		return 1;
	}
	printf("venus: init ok: next venusctl ping, or venusctl shutdown\n");
	return 0;
}

/* ping and session: state check, then SYS_INIT (once per boot), debug config and IMAGE_VERSION. 0 = ok. */
static int sys_up(struct vstate *vp, uint32_t debug, const char *what)
{
	struct vstate v;
	if (state_load(&v) || !v.init || !v.q) {
		printf("venus: refused: no initialised firmware (venusctl boot --keep, then init)\n");
		return 1;
	}
	if (!core_ok()) {
		printf("venus: refused: a video clock or GDSC is off\n");
		return 1;
	}
	qb = v.q;
	core_status(what);
	if (wait_msg(0, 0))
		return 1;
	if (v.sysinit) {
		printf("venus: SYS_INIT already sent in an earlier run, skipped\n");
	} else {
		uint32_t sys_init[3] = { 12, 0x10001, 1 };	/* HFI_CMD_SYS_INIT, HFI_VIDEO_ARCH_OX */
		if (q_write(sys_init) || wait_msg(0x20001, 1000))
			return 1;
		printf("venus: SYS_INIT_DONE error %#x, %u properties\n", pkt[2], pkt[3]);
		if (pkt[2]) {
			printf("venus: STOP: SYS_INIT_DONE with an error\n");
			return 1;
		}
		v.sysinit = 1;
		state_save(&v);
	}
	/* create_pkt_cmd_sys_debug_config: SYS_SET_PROPERTY, SYS_DEBUG_CONFIG {mask, HFI_DEBUG_MODE_QUEUE} + pad */
	uint32_t dbg[7] = { 28, 0x10005, 1, 0x1, debug, 1, 0 };
	if (q_write(dbg) || wait_msg(0, 100))
		return 1;
	/* create_pkt_cmd_sys_image_version: SYS_GET_PROPERTY IMAGE_VERSION -> SYS_PROPERTY_INFO 0x2000a */
	uint32_t ver[4] = { 16, 0x10006, 1, 0x6 };
	if (q_write(ver) || wait_msg(0x2000a, 1000))
		return 1;
	if (pkt[0] >= 16 + 128 && pkt[3] == 0x6) {	/* hfi_process_sys_get_prop_image_version: 128 bytes at word 4 */
		char s[129];
		memcpy(s, pkt + 4, 128);
		for (int k = 0; k < 128; k++)
			if (s[k] < 0x20 || s[k] > 0x7e)
				s[k] = ' ';
		s[128] = 0;
		printf("venus: firmware image version: %s\n", s);
	} else {
		printf("venus: property info without an image version (size %u, property %#x)\n", pkt[0], pkt[3]);
	}
	*vp = v;
	return 0;
}

static int ping(uint32_t debug)
{
	struct vstate v;
	if (sys_up(&v, debug, "before ping"))
		return 1;
	uint32_t pg[3] = { 12, 0x210002, 0x76656e75 };	/* HFI_CMD_SYS_PING; client_data: our own cookie */
	if (q_write(pg))
		return 1;
	/* v2c: VIDEO.IR.1.2-00053 reads word 2 as a session ID (HostDriver_GetSessionIdx fail) and answers EVENT_NOTIFY
	 * SESSION_ERROR 2, data 0x1004 (HFI_ERR_SESSION_INVALID_SESSION_ID): no PING_ACK. Not fatal: the firmware is
	 * alive, session is the real round trip. VENUSCTL_PING_FATAL=1: a missing PING_ACK fails (old behaviour). */
	int fatal = getenv("VENUSCTL_PING_FATAL") != NULL;
	lastmsg_n = 0;
	wait_soft = !fatal;
	int r = wait_msg(0x220002, 1000);
	wait_soft = 0;
	if (r && fatal)
		return 1;
	if (r) {
		printf("venus: ping not acked (non-fatal); last msgq packet:");
		for (int k = 0; k < lastmsg_n; k++)
			printf(" %08x", lastmsg[k]);
		printf("%s\n", lastmsg_n ? "" : " none");
	} else {
		printf("venus: PING_ACK client_data %#x%s\n", pkt[2], pkt[2] == pg[2] ? "" : " (not our cookie)");
	}
	wait_msg(0, 50);
	core_status("after ping");
	printf("venus: ping %s: venusctl session, or venusctl shutdown, then power off\n",
	       r ? "done without PING_ACK" : "ok (stage 2 go)");
	return 0;
}

/*
 * v2c: the first real HFI round trip, an H.264 decoder session opened and closed again (no buffers, no LOAD_RESOURCES).
 * Constants from the downstream driver (research/ext/msm-vidc-sunfish/, vidc_hfi_helper.h; same values in
 * research/notes/venus-stage1-2026-11-02.md "Packet IDs"):
 *   HFI_CMD_SYS_SESSION_INIT 0x10007 = HFI_CMD_START_OFFSET 0x10000 + 0x7, struct hfi_cmd_sys_session_init_packet
 *     {size 20, packet_type, session_id, session_domain, session_codec}; downstream session_id = hash32_ptr(session),
 *     any 32-bit cookie: ours 0x76656e75 ("venu")
 *   HFI_VIDEO_DOMAIN_DECODER 2 (HFI_COMMON_BASE + 0x2), HFI_VIDEO_CODEC_H264 2
 *   HFI_MSG_SYS_SESSION_INIT_DONE 0x20006 = HFI_MSG_START_OFFSET 0x20000 + 0x6 {size, type, session_id, error_type,
 *     num_properties, ...}; HFI_CMD_SYS_SESSION_END 0x10008 {size 12, type, session_id};
 *   HFI_MSG_SYS_SESSION_END_DONE 0x20007 {size 16, type, session_id, error_type}
 * A missing or failed reply is a STOP (exit 1); the caller's shutdown still undoes everything (PAS shutdown first).
 */
#define SESSION_ID	0x76656e75u
static int session(uint32_t debug)
{
	struct vstate v;
	if (sys_up(&v, debug, "before session"))
		return 1;
	uint32_t si[5] = { 20, 0x10007, SESSION_ID, 2, 2 };	/* SYS_SESSION_INIT, decoder, H.264 */
	if (q_write(si) || wait_msg(0x20006, 1000))
		return 1;
	printf("venus: SESSION_INIT_DONE session %#x%s error %#x, %u properties\n", pkt[2],
	       pkt[2] == SESSION_ID ? "" : " (not ours)", pkt[3], pkt[0] >= 20 ? pkt[4] : 0);
	if (pkt[3]) {
		printf("venus: STOP: SESSION_INIT_DONE with an error (no session to end)\n");
		wait_msg(0, 50);
		return 1;
	}
	uint32_t se[3] = { 12, 0x10008, SESSION_ID };	/* SYS_SESSION_END */
	if (q_write(se) || wait_msg(0x20007, 1000))
		return 1;
	printf("venus: SESSION_END_DONE session %#x error %#x\n", pkt[2], pkt[0] >= 16 ? pkt[3] : 0);
	int bad = pkt[0] >= 16 && pkt[3];
	wait_msg(0, 50);
	core_status("after session");
	if (bad) {
		printf("venus: STOP: SESSION_END_DONE with an error\n");
		return 1;
	}
	printf("venus: session ok (H.264 decoder opened and closed): venusctl shutdown, then power off\n");
	return 0;
}

/* review 8.1: is the --phys region really unused? write a pattern, let the load run, check it (/dev/pmem as init) */
static int memtest(uint64_t a, uint64_t len, int write)
{
	if (range_ok(a, len, "memtest") || (len & 3))
		return 1;
	static uint32_t buf[0x4000];
	uint64_t bad = 0;
	unsigned shown = 0;
	for (uint64_t o = 0; o < len; o += sizeof(buf)) {
		uint64_t n = len - o < sizeof(buf) ? len - o : sizeof(buf);
		if (!write && pm_rd(a + o, buf, n))
			return 1;
		for (uint64_t k = 0; k < n / 4; k++) {
			uint64_t pa = a + o + 4 * k;
			uint32_t want = (uint32_t)pa ^ 0x5a5a5a5au;
			if (write) {
				buf[k] = want;
			} else if (buf[k] != want) {
				if (shown++ < 5)
					printf("venus: memtest mismatch at %#llx: %#010x, want %#010x\n", (unsigned long long)pa,
					       buf[k], want);
				bad++;
			}
		}
		if (write && pm_wr(a + o, buf, n))
			return 1;
	}
	if (write)
		printf("venus: memtest wrote %#llx+%#llx (addr ^ 0x5a5a5a5a): next memtest ... check\n",
		       (unsigned long long)a, (unsigned long long)len);
	else
		printf("venus: memtest %#llx+%#llx: %llu of %llu words changed -> %s\n", (unsigned long long)a,
		       (unsigned long long)len, (unsigned long long)bad, (unsigned long long)(len / 4), bad ? "IN USE" : "ok");
	return bad ? 1 : 0;
}

static int shutdown_cmd(void)
{
	struct vstate v;
	if (state_load(&v)) {
		printf("venus: no %s: nothing to undo\n", STATE);
		return 0;
	}
	printf("venus: state: pas %d, %d SMRs, queues %#llx (kind %d), init %d, sysinit %d\n", v.pas, v.nsmr,
	       (unsigned long long)v.q, v.qkind, v.init, v.sysinit);
	if (v.q && v.pas && core_ok()) {
		qb = v.q;
		wait_msg(0, 0);	/* last firmware words */
		core_status("before shutdown");
	}
	int r = teardown(&v);
	printf("venus: shutdown %s: next venusctl power off\n", r ? "FAILED" : "ok");
	return r;
}

int main(int argc, char **argv)
{
	const char *fw = NULL, *cmd = NULL, *arg = NULL, *arg2 = NULL, *arg3 = NULL;
	int no_shutdown = 0, keep = 0;
	uint64_t phys = 0;
	uint32_t debug = 0x18;	/* HFI_DEBUG_MSG_ERROR | FATAL (guess: msm_vidc_debug.c's default is not in our source copy) */
	setvbuf(stdout, NULL, _IOLBF, 0);	/* a hang shows the last line */
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v"))
			verbose = 1;
		else if (!strcmp(argv[i], "--cvp"))
			cvp = 1;
		else if (!strcmp(argv[i], "--force"))
			force = 1;
		else if (!strcmp(argv[i], "--no-shutdown"))
			no_shutdown = 1;
		else if (!strcmp(argv[i], "--keep"))
			keep = 1;
		else if (!strcmp(argv[i], "--fw") && i + 1 < argc)
			fw = argv[++i];
		else if (!strcmp(argv[i], "--phys") && i + 1 < argc)
			phys = strtoull(argv[++i], NULL, 0);
		else if (!strcmp(argv[i], "--debug") && i + 1 < argc)
			debug = (uint32_t)strtoul(argv[++i], NULL, 0);
		else if (!cmd)
			cmd = argv[i];
		else if (!arg)
			arg = argv[i];
		else if (!arg2)
			arg2 = argv[i];
		else if (!arg3)
			arg3 = argv[i];
		else
			goto usage;
	}
	if (!cmd)
		goto usage;
	if (!fw) {	/* the vendor partition's copy, else a copy under /var/local (out of git) */
		struct stat st;
		fw = stat("/vendor/firmware/venus.mdt", &st) == 0 ? "/vendor/firmware" : "/var/local/fw/venus";
	}
	pm_init();
	stale_drop(STATE);
	stale_drop(AHB_STATE);
	if (arg2 && strcmp(cmd, "memtest"))
		goto usage;
	if (!strcmp(cmd, "memtest") && arg && arg2 && arg3 && (!strcmp(arg3, "write") || !strcmp(arg3, "check")))
		return memtest(strtoull(arg, NULL, 0), strtoull(arg2, NULL, 0), !strcmp(arg3, "write"));
	if (!strcmp(cmd, "probe") && !arg)
		return probe();
	if (!strcmp(cmd, "power") && arg && !strcmp(arg, "on"))
		return power_on();
	if (!strcmp(cmd, "power") && arg && !strcmp(arg, "off"))
		return power_off();
	if (!strcmp(cmd, "pas-check") && !arg)
		return pas_check(fw, no_shutdown);
	if (!strcmp(cmd, "boot") && !arg)
		return boot(fw, keep);
	if (!strcmp(cmd, "init") && !arg)
		return init(phys);
	if (!strcmp(cmd, "ping") && !arg)
		return ping(debug);
	if (!strcmp(cmd, "session") && !arg)
		return session(debug);
	if (!strcmp(cmd, "shutdown") && !arg)
		return shutdown_cmd();
usage:
	fprintf(stderr, "usage: venusctl [-v] probe | power on|off [--cvp] [--force] | pas-check [--fw DIR] [--no-shutdown]\n"
		"       | boot [--fw DIR] [--keep] | init [--phys ADDR] | ping [--debug MASK]\n"
		"       | session [--debug MASK] | shutdown\n"
		"       | memtest ADDR SIZE write|check\n");
	return 2;
}
