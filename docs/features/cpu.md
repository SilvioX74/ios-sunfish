# CPU
**State:** ✅ 8 cores; A55 1804 MHz, A76 2208 MHz (read from OSM registers), temperature and real MHz logged every 5 s (xnu 0255); runs hot, idle 79-85 C with two stuck daemons, 63 C with them paused. **Updated:** 2026-10-31.

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

### 2026-10-31 18:05 idle1, sigstop test: two daemons burn 105 of 126 CPU s/min at idle
- kernel136, everyday args. `psx` CPU s per 60 s, screen on / off: aggregated 53.6 / 55.4, coreduetd 52.4 / 52.3, biomed 9.3 / 9.3, backboardd 0.9 / 20.1, adspboot 2.4 / 3.3, kernel_task 1.3 / 3.3. Totals 126 / 149. Boot logs 20261031-173536 (idle1b), 20261031-174422 (idle1c).
- `kill -STOP` of aggregated and coreduetd at 17:53, nothing else changed: 4 min later a55 82.2 -> 62.8 C, ddr 76.4 -> 62.5 C, busy `71 41 30 21 17 7 | 25 21` -> `2 2 1 1 4 1 | 1 2` %; biomed's CPU stopped growing too. No input during the test, so client safety isn't shown. A reboot restores them.
- Clock A/B at idle, 8 min each, daemons running: `flamecpufreq=1200` gives a55 row 3 = 1017 MHz, a76 row 4 = 1094 MHz, steady 68-70 C; default 1804 and 2208 MHz, steady 79-85 C. So about 10-12 C on the CPU sensors, 8-10 C on ddr and aoss. `lmh` 0 in every sample.
- `cyc` wraps at 32 bits: real count = delta + N x 2^32 (N = 2 at default rows, 1 at the 1200 rows). Cause of the spin not known: research/notes/idle-spin-2026-10-31.md. Guesses: coreduetd on bad stored dates (burn starts right after two boots with a 1970 clock), aggregated (really PowerLog) on its database. Waking took 3 key presses in the screen-off test, possibly a double toggle: not confirmed. backboardd's 20 s/min not seen with the daemons paused.

### 2026-10-31 17:30 th1: first temperatures on kernel136 (xnu 0255): idle 66-72 C, scrolling 95.4 C, A76 throttled about 10 %
- Boot log 20261031-080759 (no split: uptime kept counting through a long fastboot wait; tools/sunfish-livelog.sh fixed, needs a listener restart). Battery 4472 mV, 0 resets, 0 `--` readings. Sensors idle 66-72 C, right after boot 80-84 C.
- Settings long swipes, ~215 s: hottest 95.4 C (cpu6), ddr 87.0, gpu 88.3. Stopped by the main session: the agent's 85 C stop used `sysctl`, which the phone lacks. Above 85 C in 5-10 s.
- A76 real clock, `cyc` with wrap corrected: 2208 MHz up to cpu6 94 C, then 2112 -> 1974 MHz over 150 s (first read as a 48 % fall: wrong). Back to 2208 MHz within 30 s of the load ending. A55 stayed 1804 MHz. `lmh` +0x704 read 0 in all 432 reads, the row didn't change: so a hardware limit exists, and neither register shows it. Swaps/s fell with it: 52-54 -> 47-48.
- `l3 7 0 0 0` never changed. 0 GPU hangs, 0 submit errors.

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

