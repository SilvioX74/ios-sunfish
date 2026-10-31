# CPU
**State:** ✅ 8 cores; A55 1804 MHz, A76 2208 MHz (read from OSM registers). **Updated:** 2026-10-24.

## Facts
- 8 cores; A55 1804 MHz, A76 2208 MHz (read from OSM registers) (status.md).
- xnu 0147 UI threads prefer A76 (`sweetbig=0` turns it off).
- xnu 0166 `sweetcpusample`: render thread 10-15 ms CPU/frame, 55-75% on A76; CPUs mostly idle. The scheduler is not big/little aware (068a681).
- xnu 0255 (2026-10-31, not run) `sweettherm=0` turns it off: 5 s `SWEET THERM:` lines, `kern.sweet_temp_{a55,a76,gpu,ddr,max}`; TSENS, OSM, L3 and LMh, reads only.

## Open
- Heat (the user, 2026-10-31: the phone is hot all the time): plan priorities 2 and 5.

## Log
(Newest first, append-only. When this Log passes about 150 lines, move its oldest entries to the archive.)

Older entries (before 2026-10-25): [archive/features/cpu.md](../archive/features/cpu.md).

### 2026-10-31 17:30 xnu 0256 sunfish-thermal-cap written, not built, not run
- Finding (kernel136 logs): the `cyc` counter is 32 bits and wraps within 5 s; real A76 cycles per 5 s = delta + 2 x 2^32. So 2452M = 2208 MHz, the "fall" to 1279M = about 1974 MHz, A55 436M = 1805 MHz. The A76 runs about 10 % slow at cpu6 94-95 C while the row and `+0x704` don't show it, and swaps/s fell by the same 10 %. Guess: a hardware throttle; what drives it is not known.
- Cap as a last resort above that, `sweetthermcap=0` off: hottest of a55, cpu6, cpu7, cpuss, gpu at or above 98 C lowers both domains a row per second, floors A76 row 1 (652 MHz), A55 row 4 (1248 MHz); at or below 92 C raises a row per 5 s, up to the boot rows. At or above 100 C for 5 samples: `SWEET THERM: CRITICAL` and straight to the floors (no reset: the only reset path is inline in the USB command loop). Samples every 1 s from 90 C. Log `SWEET THERM: cap a55 R=MHz a76 R=MHz at T C (rising|falling)`, `kern.sweet_therm_caps`, `cap N` on the busy line. The GPU clock is not capped.
- The busy line's `cyc N` becomes `real N`: MHz measured over about 100 ms, `kern.sweet_mhz_{a55,a76}`.

### 2026-10-31 17:10 xnu 0255 sunfish-thermal-clocks written, not built, not run
- Temperatures, busy %, OSM index, raw cycle delta, raw LMh and L3 registers every 5 s (redfin 0960, 0961, without PMIC parts or "w" commands). Unverified on sunfish: whether TSENS, L3 and LMh reads can fault (watch the first boot for a reset), 0x9c0, 0x704, the L3 offsets.

### 2026-10-31 16:45 Governor plan (the user's ask; no phone run)
- Stages: log temperature, real MHz and LMh limit (redfin 0960, 0961); low CPU speed with the screen off (xnu 0242 flag); screen-on governor (redfin 0963, 2 domains), kept only if the A/B shows no speed loss. Observed: CPUs pinned at max from ~20 s after boot; no temperature driver. Guesses for the heat: pinned clocks, charging current, polling, no deep idle (found no record of it). Bridge build sets no target-cpu (untested; Mesa flags not read).

### 2026-10-30 01:40 CPU governor question (read-only, research/notes/cpu-governor.md)
- The CPUs already run at their top all-core LUT entries after xnu 0086 (A55 index 9 = 1804 MHz, A76 index 12 = 2208 MHz, read back; ABL leaves A55 at 1497 MHz and A76 at 300 MHz until ~20 s after boot). A governor (ios-redfin 0963, needs their 0961 + 0960, 2-cluster port) would save power, not add speed. Not started. Unchecked: LMh capping (needs 0961's real-MHz probe) and the ~20 s at 300 MHz on the A76s early in boot.

