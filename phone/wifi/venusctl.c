/*
 * venusctl: video stage 2 (docs/features/video.md "Stage 2 spec"), the Venus/IRIS1 video engine from userland
 * through /dev/pmem, as adspboot does for the ADSP. Facts: research/notes/venus-stage1-2026-11-02.md (DT lines,
 * downstream videocc-sdmmagpie.c in research/ext/msm-vidc-sunfish/), PLL/RCG/branch sequences as phone/tools/camclk.c.
 *   venusctl probe          read-only: GCC video CBCRs, TBU sf vote/status, apps SMMU SMRs for SIDs 0x1080-0x10e0,
 *                           cmd-db MM3/MM2; videocc (efuse, PLL0, RCG, GDSCs, CBCRs) only with gcc_video_ahb on.
 *                           Never reads the core (0x0aa00000).
 *   venusctl power on [--cvp]   BCM MM3 + MM2 minimal votes (no MX/CX), gcc_video_ahb/axi, PLL0 480 MHz, iris RCG /2
 *                           = 240 MHz, GDSC mvsc then mvs0 (--cvp: + mvs1), branches; then WRAPPER_HW_VERSION
 *   venusctl power off [--cvp]  the reverse; gcc_video_ahb stays on (critical in mainline gcc-sm7150), votes last
 *   venusctl pas-check [--fw DIR] [--no-shutdown]  needs power on: PAS is_supported(9), init_image(9, venus.mdt
 *                           metadata), shutdown(9). No mem_setup, no auth_and_reset.
 *   -v                      print every register write (address, old, new, read-back)
 * Writes only: GCC video_ahb/video_axi CBCRs, the videocc PLL0/RCG/GDSC/CBCR registers below, RPMh BCMs MM3/MM2.
 * No SMMU writes. Every videocc access is gated on gcc_video_ahb CLK_OFF = 0, the core read on the GDSCs + clocks.
 * Polls time out after 100 ms and say where they stopped.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "pmem.h"

#define GCC		0x00100000u
#define GCC_VIDEO_AHB	(GCC + 0xb004)	/* gcc_video_ahb_clk, GDSC parent (notes table "GDSC parent clock") */
#define GCC_VIDEO_AXI	(GCC + 0xb01c)
#define GCC_VIDEO_XO	(GCC + 0xb028)	/* critical in mainline: read only */
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

static int verbose, cvp;

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

static int allowed(uint32_t pa)
{
	if (pa == GCC_VIDEO_AHB || pa == GCC_VIDEO_AXI)
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
		printf("%-14s %#x = %#010x: %s, en %u\n", g[i].n, g[i].pa, v, onoff(v), v & 1);
	}
	uint32_t vote = rd32(GCC_TBU_SF_VOTE);
	printf("tbu sf vote %#x = %#010x (bit0 sw_collapse %u, PWR_ON %u); status %#x = %#010x %#010x\n",
	       GCC_TBU_SF_VOTE, vote, vote & 1, vote >> 31, TBU_SF_STATUS, rd32(TBU_SF_STATUS), rd32(TBU_SF_STATUS + 4));
	probe_smmu();
	probe_bcm();
	if (!ahb_on()) {
		printf("videocc skipped: AHB off\n");
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
	if (v & HW_CONTROL)
		printf("venus: note: gdsc %s HW_CONTROL set (%#010x)\n", name, v);
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

static int power_on(void)
{
	for (unsigned i = 0; i < 2; i++)
		if (bcm(bcms[i], BCM_ON)) {
			printf("venus: STOP: bcm %s vote failed\n", bcms[i]);
			return power_off(), 1;
		}
	if (gcc_on(GCC_VIDEO_AHB, "gcc_video_ahb", 1))
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
	int ok = ahb_on() && !(rd32(GCC_VIDEO_AXI) & CLK_OFF) && (vrd(GDSC_MVSC) & PWR_ON) && (vrd(GDSC_MVS0) & PWR_ON);
	for (unsigned i = 0; i < NBR; i++)
		if (!branches[i].cvp || cvp)
			ok = ok && !(vrd(branches[i].pa) & CLK_OFF);
	if (!ok) {
		printf("venus: STOP: a clock or GDSC dropped before the HW version read\n");
		return power_off(), 1;
	}
	printf("venus: WRAPPER_HW_VERSION %#x = %#010x\n", WRAP_HW_VERSION, rd32(WRAP_HW_VERSION));
	return 0;
}

static int power_off(void)
{
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
	/* gcc_video_ahb stays as it is: critical in mainline gcc-sm7150, and the GDSC parent */
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

static int pas_check(const char *fw, int no_shutdown)
{
	if (!ahb_on() || !(vrd(GDSC_MVSC) & PWR_ON) || !(vrd(GDSC_MVS0) & PWR_ON)) {
		printf("venus: refused: power is off (gcc_video_ahb %#010x%s): run 'venusctl power on' first\n",
		       rd32(GCC_VIDEO_AHB), ahb_on() ? ", or gdsc mvsc/mvs0 off" : "");
		return 1;
	}
	char path[256];
	size_t mlen;
	snprintf(path, sizeof(path), "%s/venus.mdt", fw);
	uint8_t *mdt = slurp(path, &mlen);
	if (!mdt) {
		fprintf(stderr, "venus: no %s\n", path);
		return 1;
	}
	uint32_t phoff, phnum = 0;
	memcpy(&phoff, mdt + 0x1c, 4);
	memcpy(&phnum, mdt + 0x2c, 2);
	if (memcmp(mdt, "\177ELF", 4) || phoff + 32ull * phnum > mlen) {
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
	if (ph[hash].offset + ph[hash].filesz <= mlen) {
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

int main(int argc, char **argv)
{
	const char *fw = NULL, *cmd = NULL, *arg = NULL;
	int no_shutdown = 0;
	setvbuf(stdout, NULL, _IOLBF, 0);	/* a hang shows the last line */
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v"))
			verbose = 1;
		else if (!strcmp(argv[i], "--cvp"))
			cvp = 1;
		else if (!strcmp(argv[i], "--no-shutdown"))
			no_shutdown = 1;
		else if (!strcmp(argv[i], "--fw") && i + 1 < argc)
			fw = argv[++i];
		else if (!cmd)
			cmd = argv[i];
		else if (!arg)
			arg = argv[i];
		else
			goto usage;
	}
	if (!cmd)
		goto usage;
	pm_init();
	if (!strcmp(cmd, "probe") && !arg)
		return probe();
	if (!strcmp(cmd, "power") && arg && !strcmp(arg, "on"))
		return power_on();
	if (!strcmp(cmd, "power") && arg && !strcmp(arg, "off"))
		return power_off();
	if (!strcmp(cmd, "pas-check") && !arg) {
		struct stat st;
		if (!fw)	/* the vendor partition's copy, else a copy under /var/local (out of git) */
			fw = stat("/vendor/firmware/venus.mdt", &st) == 0 ? "/vendor/firmware" : "/var/local/fw/venus";
		return pas_check(fw, no_shutdown);
	}
usage:
	fprintf(stderr, "usage: venusctl [-v] probe | power on|off [--cvp] | pas-check [--fw DIR] [--no-shutdown]\n");
	return 2;
}
