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

### 2026-10-31 17:10 xnu 0255 sunfish-thermal-clocks written, not built, not run
- Temperatures, busy %, OSM index, raw cycle delta, raw LMh and L3 registers every 5 s (redfin 0960, 0961, without PMIC parts or "w" commands). Unverified on sunfish: whether TSENS, L3 and LMh reads can fault (watch the first boot for a reset), 0x9c0, 0x704, the L3 offsets.

### 2026-10-31 16:45 Governor plan (the user's ask; no phone run)
- Stages: log temperature, real MHz and LMh limit (redfin 0960, 0961); low CPU speed with the screen off (xnu 0242 flag); screen-on governor (redfin 0963, 2 domains), kept only if the A/B shows no speed loss. Observed: CPUs pinned at max from ~20 s after boot; no temperature driver. Guesses for the heat: pinned clocks, charging current, polling, no deep idle (found no record of it). Bridge build sets no target-cpu (untested; Mesa flags not read).

### 2026-10-30 01:40 CPU governor question (read-only, research/notes/cpu-governor.md)
- The CPUs already run at their top all-core LUT entries after xnu 0086 (A55 index 9 = 1804 MHz, A76 index 12 = 2208 MHz, read back; ABL leaves A55 at 1497 MHz and A76 at 300 MHz until ~20 s after boot). A governor (ios-redfin 0963, needs their 0961 + 0960, 2-cluster port) would save power, not add speed. Not started. Unchecked: LMh capping (needs 0961's real-MHz probe) and the ~20 s at 300 MHz on the A76s early in boot.

