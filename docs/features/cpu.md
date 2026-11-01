# CPU
**State:** ✅ 8 cores; A55 1804 MHz, A76 2208 MHz (read from OSM registers), temperature and real MHz logged every 5 s (xnu 0255); idle was 79-85 C with two stuck daemons, 54 C after 1 h with their data moved aside. **Updated:** 2026-10-31.

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

### 2026-10-31 20:48 xnu 0258 sunfish-cpu-rows written, not built, not run
- `sweetrowa55=R`, `sweetrowa76=R` (decimal, each optional; absent: flamecpufreq picks as before): that cluster's boot row, so the thermal cap and the dark clock restore to it. Wins over flamecpufreq (also `flamecpufreq=0`), with a log line. A row past the table or a 1-core boost row is ignored with a log line. Log `SWEET CPUFREQ: rows a55 R=MHz a76 R=MHz (boot-arg)`. The table was already printed per cluster at init (rows count from 0); no second print. The `SWEET THERM:` layout is unchanged.
- Rows from the table in `../logs/boots/20261029-125039.log` (read from OSM at init, not in the source): A55 6=1497, 7=1612, 8=1708, 9=1804; A76 9=1843, 10=1939, 11=2169, 12=2208.

### 2026-10-31 19:46 xnu 0257 sunfish-dark-clk written, not built, not run
- `sweetdarkclk=1` (default 0; needs `sweetdark` on): while dark A55 row 4 (1248 MHz), A76 row 1 (652 MHz); effective row = lower of cap row and dark row; leaving dark restores the cap row. Log `SWEET THERM: dark a55 4=1248 a76 1=652`, `SWEET THERM: bright a55 9=1804 a76 12=2208`, `dark 0|1` after `cap N` on the busy line. Rows are lowered as soon as the off request arrives, while the panel thread's sleep-in (about 200-350 ms) is still running, not after it ends. Raised at the top of RequestPowerChange(on); the key press that leads there still runs at dark rows.

### 2026-11-01 13:10 hot1: A76 2208 against 1939 MHz over 4 minutes of Settings
- `SWIPE_MS=3000 tools/sunfish-perfab.sh hot1 2 --secs 240 --screens settings`, kernel139, everyday env with SWEET_U0011..14, 4 boots, 162 k submits, 0 GPU bugs. 2208: 55.7 swaps/s (55.0-56.3), frame 18.0, p95 26.8, max 92.0 C, A76 min 2197. 1939: 54.6 (53.3-55.9), 18.3, 28.9, max 86.9 C. So 5 C for 0.4 ms, under the 1 ms stop rule, 2 boots a side. 2208 stays while speed comes first; a governor that holds 2208 only for touch and animation is the way to get both.

### 2026-11-01 07:17 Long idle check: fixes hold for an hour
- Everyday boot from 06:18, kernel139, sweetrowa55=6, no touches, screen state as left. After about 1 h: a55 54.1 C, ddr 53.8, gpu 55.8; busy `5 3 2 3 1 1 | 2 3` %; CPU s used: coreduetd 18.0, aggregated 9.3, biomed 5.3, backboardd 40.7, SpringBoard 47.7. 0 GPU errors. Was 79-85 C and 53 CPU s per minute each.

### 2026-11-01 03:45 Correction to rows2
- A55 at 1248 MHz isn't a proven G1 reproducer: mf3 had 0 G1 in about 81 k submits on that row with mode 3 (gpu.md). Row 6 stays for its speed and heat results.

### 2026-11-01 00:45 rows2: A55 at 1248 MHz: no speed gain, 3 no-fault GPU hangs in 5 boots; 1497 MHz stays
- `tools/sunfish-perfab.sh rows2 5 --secs 15 --screens settings,home,cc,apps`, kernel139, SWEET_DEADFILL=2, M2V_SHADOW_CACHE=1, sweetrowa55=6 against 4, 10 boots, 86 k submits. G1 3, all on row 4 (boots 2, 3, 4), at uptime 104-212 s, recovery FAILED; 0 on row 6. Boots lost on row 4 voided 7 runs, row 6 2 (both `0 swaps`, b1 home and cc, unexplained).
- Home 38.6 / 38.8 swaps/s, Control Center 28.8 / 28.6, apps 22.7 / 24.5. Open ms: Notes 236 / 314, Clock 475 / 500, Calculator 520 / 521. No speed gain from row 4, so it's off.
- Observed: 3 G1 in about 43 k submits at 1248 MHz against 2 in about 380 k before: the lowest A55 clock seems to raise G1. A reproducer for G1 (gpu-bugs.md): cause not known. Everyday boot now adds sweetrowa55=6; xnu default unchanged.
- Correction to smoothness.md n2: mode 2 had 2 G1 in about 380 k submits, not 250 k (df5, rows1, sh1, n1, n2).

### 2026-10-31 21:40 rows1: A55 at 1497 MHz: cooler and faster on Settings
- `tools/sunfish-perfab.sh rows1 5 --secs 15 --screens settings,home`, kernel139 (xnu 0258) sweetswapbig=6 sweetse9=2 sweetcampmic=2 sweetampreset=1 SWEET_DEADFILL=2, sweettrace=1, base against `sweetrowa55=6`, 10 boots, 55 k submits, 0 G1, 0 G2, 0 G4.
- Settings: base 3 ok, 2 void (too hot), 49.6 (48.3-50.7) swaps/s, frame 20.2 ms, p50 19.0, <=17 ms 32 %, start / max 91.6 / 94.6 C, A76 min 2006 MHz. A55 1497: 5 ok, 0 void, 52.1 (49.1-55.0) swaps/s, frame 19.2 ms, p50 17.3, <=17 ms 55 %, 86.1 / 90.5 C, A76 min 2185 MHz. p95 30.1 and 30.2.
- Home: 26.7 and 26.9 swaps/s, GPU-bound; p95 47.3 and 59.0 (mean of a few gaps, not read as a change).
- Observed: about 5 C cooler on Settings, fewer throttled runs, +2.5 swaps/s, -1 ms. Base here is below df5's 54.8: Settings ran first here, right after boot, last in df5: order matters (cooler after the GPU-bound screens).
- Next: lower A55 rows (4 = 1248, 5 = 1324); A55 rows may slow app work: needs an app-open check before default.

### 2026-10-31 20:25 dk1: sweetdarkclk=1 works; heat gain not shown (kernel138, xnu 0257)
- Boots 20261031-195555 (sweetdarkclk=1), 20261031-200956 (control), 4455 mV, 0 resets, 0 hangs, 0 `submit failed`. Dark: real 1248 and 652 MHz (control 1804 and 2208), busy 1-6 %. Wake: `bright` line 6 ms after the key command. 5 of 5 dark and wake cycles, touch and Settings fine afterwards.
- Hottest sensor at dark +1 / +3 / +5 min: sweetdarkclk=1 61.9 / 59.3 / 58.3 C, control 56.4 / 54.8 / 52.8 C. Not a fair comparison: the first boot had been at 94 C and was still cooling, screen-on start 67.0 against 59.6 C. At 1-6 % busy clock choice may matter little (guess).
- Stays off (default). Needs a same-start test to show a gain: Optimize item.

### 2026-10-31 18:57 db1, db2: both idle burners stopped by moving their data aside
- db1, kernel137, boot 20261031-182504: PowerLog files moved. aggregated 4.75 CPU s in 10 min (was ~53 s per min). coreduetd unchanged at 0.88 core, biomed 0.14; hottest 66-70 C. coreduetd's stack: `_DKBiomeQuery`, BPS publishers, `-[NSDate descriptionWithLocale:]`, formatting a date per event. Old PowerLog archive was at powerlog_1985-04-10 with about 1950 files: walking day by day, dates from old 1970-clock boots is a guess.
- db2: /var/mobile/Library/Biome moved too. Boot 20261031-185506: at 90 s and at 120 s of uptime coreduetd 8.26 -> 8.28 CPU s, aggregated 2.26 -> 2.54, biomed 2.13 -> 2.14; 67-68 C, busy `40 33 31 11 6 4 | 8 10` % at 131 s (still settling).
- Not checked: long idle, apps after the Biome move (stopped for the show-off). Whether the burn returns as the stores refill is unknown. The moved files are in /var/local/idlefix-db.

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

