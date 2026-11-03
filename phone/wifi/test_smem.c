/* hakios review W: host test of smem.c's bounds on a fake SMEM (rd32/wr32 over a buffer, no /dev/pmem). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "smem.c"

#define CHECK(c) do { int ok_ = (c); printf("%s %s\n", ok_ ? "ok  " : "FAIL", #c); fails += !ok_; } while (0)
static uint8_t *mem;
static uint32_t mutex;
static int fails;

void pm_init(void) {}

uint32_t rd32(uint64_t pa)
{
	if (pa >= SMEM_BASE && pa + 4 <= SMEM_BASE + SMEM_SIZE) {
		uint32_t v;
		memcpy(&v, mem + (pa - SMEM_BASE), 4);
		return v;
	}
	if (pa == TCSR_MUTEX(SMEM_HWLOCK))
		return mutex;
	fprintf(stderr, "rd32 outside SMEM: %llx\n", (unsigned long long)pa);
	exit(2);
}

void wr32(uint64_t pa, uint32_t v)
{
	if (pa >= SMEM_BASE && pa + 4 <= SMEM_BASE + SMEM_SIZE) {
		memcpy(mem + (pa - SMEM_BASE), &v, 4);
		return;
	}
	if (pa == TCSR_MUTEX(SMEM_HWLOCK)) {
		mutex = v;
		return;
	}
	fprintf(stderr, "wr32 outside SMEM: %llx\n", (unsigned long long)pa);
	exit(2);
}

static void w32(uint64_t off, uint32_t v) { memcpy(mem + off, &v, 4); }
static void w16(uint64_t off, uint16_t v) { memcpy(mem + off, &v, 2); }

#define POFF	0x10000u	/* the apps-modem partition */
#define PSIZE	0x8000u

/* fresh SMEM: a $TOC with one apps-modem partition, no entries */
static void reset(void)
{
	memset(mem, 0, SMEM_SIZE);
	nparts = 0;
	global_part = -1;
	uint64_t t = SMEM_SIZE - 0x1000;
	w32(t, 0x434f5424);
	w32(t + 8, 1);
	w32(t + 32, POFF);
	w32(t + 36, PSIZE);
	w32(t + 44, SMEM_HOST_APPS | (uint32_t)SMEM_HOST_MODEM << 16);
	w32(t + 48, 64);
	w32(POFF, 0x54525024);
	w32(POFF + 4, SMEM_HOST_APPS | (uint32_t)SMEM_HOST_MODEM << 16);
	w32(POFF + 8, PSIZE);
	w32(POFF + 12, 32);
	w32(POFF + 16, PSIZE);
}

/* an uncached entry at the free offset */
static void add_unc(uint16_t item, uint32_t sz, uint16_t pad_data, uint16_t pad_hdr, uint32_t room)
{
	uint32_t fu;
	memcpy(&fu, mem + POFF + 12, 4);
	w16(POFF + fu, CANARY);
	w16(POFF + fu + 2, item);
	w32(POFF + fu + 4, sz);
	w16(POFF + fu + 8, pad_data);
	w16(POFF + fu + 10, pad_hdr);
	w32(POFF + 12, fu + 16 + room);
}

int main(void)
{
	uint64_t pa;
	uint32_t sz;
	mem = calloc(1, SMEM_SIZE);

	reset();
	add_unc(479, 64, 0, 0, 64);
	CHECK(smem_init() == 0 && nparts == 1);
	CHECK(smem_get(SMEM_HOST_MODEM, 479, &pa, &sz) == 0 && sz == 64 && pa == SMEM_BASE + POFF + 32 + 16);

	reset();	/* padding_data > size: was size = 64 - 128 = ~4 GB */
	add_unc(479, 64, 128, 0, 64);
	smem_init();
	CHECK(smem_get(SMEM_HOST_MODEM, 479, &pa, &sz) != 0);

	reset();	/* an entry running past free_unc */
	add_unc(479, 0x100000, 0, 0, 64);
	smem_init();
	CHECK(smem_get(SMEM_HOST_MODEM, 479, &pa, &sz) != 0);

	reset();	/* sz + 16 wrapping to 0 in 32 bits: used to loop for good */
	add_unc(1, 0xfffffff0u, 0, 0, 64);
	smem_init();
	CHECK(smem_get(SMEM_HOST_MODEM, 479, &pa, &sz) != 0);

	reset();	/* cached entry with padding > size, and one reaching below free_cac */
	w32(POFF + 16, PSIZE - 0x100);
	w16(POFF + PSIZE - 64, CANARY);
	w16(POFF + PSIZE - 64 + 2, 480);
	w32(POFF + PSIZE - 64 + 4, 64);
	w16(POFF + PSIZE - 64 + 8, 100);
	smem_init();
	CHECK(smem_get(SMEM_HOST_MODEM, 480, &pa, &sz) != 0);
	w16(POFF + PSIZE - 64 + 8, 0);
	CHECK(smem_get(SMEM_HOST_MODEM, 480, &pa, &sz) == 0 && sz == 64 && pa == SMEM_BASE + POFF + PSIZE - 128);
	w32(POFF + PSIZE - 64 + 4, 0x1000);
	CHECK(smem_get(SMEM_HOST_MODEM, 480, &pa, &sz) != 0);

	reset();	/* header free offsets inverted; alloc must refuse too */
	w32(POFF + 12, PSIZE + 0x1000);
	smem_init();
	CHECK(smem_get(SMEM_HOST_MODEM, 479, &pa, &sz) != 0);
	CHECK(smem_alloc(SMEM_HOST_MODEM, 479, 64, &pa) != 0);

	reset();	/* non-power-of-two cacheline */
	w32(SMEM_SIZE - 0x1000 + 48, 48);
	smem_init();
	CHECK(smem_get(SMEM_HOST_MODEM, 479, &pa, &sz) != 0);

	reset();	/* partition past the end of SMEM, wrong magic, size mismatch: skipped */
	w32(SMEM_SIZE - 0x1000 + 36, SMEM_SIZE);
	smem_init();
	CHECK(nparts == 0);
	reset();
	w32(POFF, 0);
	smem_init();
	CHECK(nparts == 0);
	reset();
	w32(POFF + 8, PSIZE / 2);
	smem_init();
	CHECK(nparts == 0);

	reset();	/* huge TOC count: clamped to the 4 KB table */
	w32(SMEM_SIZE - 0x1000 + 8, 0xffffffffu);
	CHECK(smem_init() == 0 && nparts == 1);

	reset();	/* legacy TOC entry pointing past SMEM */
	smem_init();
	w32(16 * 4 + 32 * 4 + 16 + 16 * 7, 1);
	w32(16 * 4 + 32 * 4 + 16 + 16 * 7 + 4, SMEM_SIZE - 16);
	w32(16 * 4 + 32 * 4 + 16 + 16 * 7 + 8, 64);
	CHECK(smem_get(SMEM_HOST_MODEM, 7, &pa, &sz) != 0);
	w32(16 * 4 + 32 * 4 + 16 + 16 * 7 + 4, 0x1000);
	CHECK(smem_get(SMEM_HOST_MODEM, 7, &pa, &sz) == 0 && pa == SMEM_BASE + 0x1000 && sz == 64);

	reset();	/* alloc still works on a sane partition */
	smem_init();
	CHECK(smem_alloc(SMEM_HOST_MODEM, 479, 60, &pa) == 0 && smem_get(SMEM_HOST_MODEM, 479, &pa, &sz) == 0 && sz == 60);

	printf("%s\n", fails ? "FAILED" : "all ok");
	return fails != 0;
}
