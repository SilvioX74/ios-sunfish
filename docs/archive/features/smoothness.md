# Smoothness: archived log (entries up to 2026-11-01 06:20)
Current file: [features/smoothness.md](../../features/smoothness.md). Same format, newest first.

## Log

### 2026-11-01 06:20 cs1: Control Center backdrop at quarter size: 28.9 -> 32.9 swaps/s, p95 72 -> 49 ms
- tools/sunfish-install-ccscale.sh (backdropScale 0.25 in modulesBackground.materialrecipe, .orig kept), `tools/sunfish-perfab.sh cs1 4 --secs 15 --screens cc,home,settings`, kernel139 sweetrowa55=6, SWEET_DEADFILL=3, M2V_SHADOW_CACHE=1, SWEET_CULL=2, minimum brightness, 4 boots, 36 k submits, 0 G1, 0 G2, 0 G4, 0 void. Control Center 32.9 (32.1-33.5), frame 30.4 ms, p95 49.3, >100 ms gaps 0.5, 1st submit 18.5. Before (cu1 c2, 3 boots, no interleave, cc after home): 28.9, 34.6 ms, p95 72.2, 5.7, 27.8. Home 49.6, Settings 49.1. Not an interleaved A/B; cc ran first here. Look not checked in motion. Why: recipe and traces in home-cc-gpu-2026-10-31.md (cc2) and research/re/CoreMaterial.s.

### 2026-11-01 05:50 cc2: Control Center's slow frames: two full-size backdrop captures, not the blur chain
- Light trace of 10 open and close rounds, kernel139, modes on, boot of 05:45, no crash, research/notes/cc2-trace.txt (numbers in home-cc-gpu-2026-10-31.md). 54 of 582 frames over 25 ms: 1.96 passes of 1127x2438 at about 9.9 ms each, main pass 11.2 ms, blur chain under 2 ms. 127 frames at 12-25 ms: a main pass of 18.6 ms alone. Traced times.
- Back on the everyday boot afterwards, env as before.

### 2026-11-01 05:20 st4: SWEET_CULL=2 on the phone: nothing wrong seen, short check
- kernel139 sweetrowa55=6, SWEET_DEADFILL=3, M2V_SHADOW_CACHE=1, SWEET_CULL=2, boot st4, 4454 mV. Stills fine: lock, Home x2, Control Center. Webcam frames during slow swipes: Home to Today mid-motion, Settings closing to Home: no black holes or bands. 0 hangs, 0 submit errors, 60.6 C at 5 min. The agent's run covered only lock and Home (its swipes didn't register; direct ones did), so App Library, Spotlight and the apps weren't rechecked with the cull. Left on the phone for U22.

### 2026-11-01 05:10 cu1: SWEET_CULL=2 on Home: 42.5 -> 49.2 swaps/s, steadier; 1 G1 on its side
- `tools/sunfish-perfab.sh cu1 4 --secs 15 --screens home,cc,settings`, kernel139 sweetrowa55=6, SWEET_DEADFILL=3, M2V_SHADOW_CACHE=1, minimum brightness, 8 boots, 67 k submits, G1 1 (c2 b3), G2 0, G4 0, 2 void from it. c0 then c2: Home 42.5 (37.1-44.5) / 49.2 (48.6-49.5), frame 23.5 / 20.3 ms, p95 45.2 / 38.9, 1st submit 20.9 / 17.4; Control Center 28.5 / 28.9; Settings 51.1 / 50.8. Stills fine (Home, Control Center, Settings at b1). Not checked for holes or rim errors in motion. Stays env-only and not on the everyday setup until checked. Whether the wallpaper draw can discard pixels is unverified (no metallib).

### 2026-11-01 04:30 gd1: GPU 650 MHz: Home 44.8 -> 48.6, Control Center 28.6 -> 30.8, Settings worse; sweetddr=1 no change
- `tools/sunfish-perfab.sh gd1 4 --secs 15 --screens home,cc,settings`, kernel139 sweetrowa55=6, SWEET_DEADFILL=3, M2V_SHADOW_CACHE=1, minimum brightness, 12 boots, 106 k submits, 0 G1, 0 G2, 0 G4, 2 void (base Home too hot). Base / sweetgpufreq=6 / sweetddr=1: Home 44.8 / 48.6 / 44.5 swaps/s, p95 43.4 / 39.8 / 45.2, max 89.8 / 93.9 / 93.4 C; Control Center 28.6 / 30.8 / 28.5, p95 61.6 / 61.8 / 63.0; Settings 49.2 / 44.6 (34.4-48.6) / 49.3, p95 31.8 / 41.7 / 31.8, >100 ms gaps 0 / 2.5 / 0.
- Reading: 650 MHz gains 1.7-2.4 ms where the GPU is the limit, but one Settings boot fell to 34.4 with gaps, cause unknown: not default. More heat. sweetddr=1: no gain now; off. Everyday setup unchanged.

### 2026-11-01 04:00 cc1: Control Center light trace: an open and still panel costs little; the bench measures its open and close
- kernel139, SWEET_DEADFILL=3, M2V_SHADOW_CACHE=1, MESA_GPU_TRACES=print only (no crash, backboardd pid 60), boot of 03:44:50. Slice research/notes/cc1-trace.txt, in a window with Control Center open and swipes inside it: 448 frames, about 9.2 ms per frame: 1125x2436 1 pass per frame, 44 bins, 7.95 ms (load 0.33, store 0.58, binning 0.14); 509x1099 0.29 per frame at 3.14 ms; 513x1103 0.07 at 2.6; copies 0.1 per frame. draw_ib_gmem 96 %.
- So Control Center's 35-46 ms frames in perfab come from opening and closing it (blur over a moving scene, re-capture), not a still panel. The cc screen in perfab is a mix of open, margin swipes and dismiss. No trace of open and close yet. Note: left-edge swipes with it closed open Spotlight.

### 2026-11-01 03:05 st3: mode 3 survey: no stale strip seen; motion not fully caught
- kernel139 sweetrowa55=6, SWEET_DEADFILL=3, M2V_SHADOW_CACHE=1, boot 20261101-024953, 4455 mV, DCS 33. Fine: Home x2, App Library, Spotlight, Control Center, Settings, Calculator, Notes, Safari, Mail, Calendar, Reminders, Files. Stopwatch digits garbled while running (8 s readout likely); Maps shot mid-transition, not rechecked; Photos black gaps (B44); lock step and Settings subpage not done. Wi-Fi icon absent at 60 s, present at 120 s: the d3 difference is timing.
- 10 webcam frames: no stale strip, wrong band or flash, but Home frames caught settled pages, not mid-swipe, so this doesn't clear motion: U22. 0 hangs, 0 submit errors, no new crash files. Control Center bench in this run read 13.2 swaps/s in one 5-s window: a bad test (margin swipes alone). Phone left on this setup as the everyday candidate.

### 2026-11-01 02:45 d4: mode 3 with the held clear: Home 31.4 -> 40.8, Settings unchanged
- `tools/sunfish-perfab.sh d4 5 --secs 15 --screens home,cc,settings`, kernel139 sweetrowa55=6, M2V_SHADOW_CACHE=1, minimum brightness, 10 boots, 83 k submits, 0 G1, 0 G2, 0 G4, 1 void (Home too hot, 95 C). Mode 2 then 3: Home 31.4 (28.2-33.3) / 40.8 (37.4-44.1), frame 31.9 / 24.5 ms, 1st submit 28.6 / 20.6; Settings 49.3 / 49.5, 1st 8.1 / 8.1; Control Center 26.4 / 23.1 including one run at 1.4 swaps/s (see below), 1st 28.2 / 25.9. Not default yet: stale-strip survey next.

### 2026-11-01 02:10 d3: SWEET_DEADFILL=3 on Home: 32.0 -> 43.8 swaps/s, Settings lost mode 2's clear
- `tools/sunfish-perfab.sh d3 3 --secs 15 --screens home,cc,settings`, kernel139 sweetrowa55=6, M2V_SHADOW_CACHE=1, minimum brightness, 6 boots, 49 k submits, 0 G1, 0 G2, 0 G4, 0 void. Mode 2 then 3: Home 32.0 (29.2-33.5) / 43.8 (43.1-44.2), frame 31.2 / 22.8 ms, 1st submit 28.5 / 21.2; Control Center 26.5 / 27.0, 1st 27.9 / 30.5; Settings 50.8 / 46.3, 1st 8.1 / 12.3.
- Mode 3 dropped fills covered by the next same-setup fill, but not the clear path of mode 2: Settings fell back to mode 1's cost. Being rebuilt as a superset. Stale strips not checked yet.

### 2026-11-01 01:30 nd1: Home and Control Center slower at minimum brightness? (mf1, mf2, nd1)
- Same bundle and env, kernel139, sweetrowa55=6. DCS 128 at boot (rows2, sy1): Home 38.6 and 36.5 swaps/s, 1st submit 24.8 ms; Control Center 28.8 and 29.3. DCS 36 at boot, slider at minimum since mf1 (mf1, mf2, nd1): Home 31.6 (27.5-33.7), 31.6, 30.6 (25.8-33.5), 1st submit 28.2-28.9 ms; Control Center 25.8, 26.2; Settings GPU 88 % against 56-62 %. nd1 skipped the dim step (no Control Center before the bench) and stayed slow, so the step isn't the cause.
- Guess: at low brightness iOS adds a full-screen colour step, about 4 ms of GPU per frame. Not tested: a bright A/B is against the night rule, so it's for the morning. Until then compare runs only within one brightness. Night benches understate speed.

### 2026-11-01 01:10 sy1: TU_DEBUG=sysmem: not worth it on partial data; run cut short when USB dropped
- `tools/sunfish-perfab.sh sy1 3 --secs 15 --screens home,cc,settings`, kernel139 sweetrowa55=6, SWEET_DEADFILL=2, M2V_SHADOW_CACHE=1, 1 usable boot a side: Home 36.5 -> 38.1 swaps/s, Control Center 29.3 -> 26.4, Settings 52.4 -> 51.5. gmem b2: 1 G4 hang voided 2 screens. sysmem b2 (boot 20261101-005041): `link up 00:52:56`, the live log stops at uptime 136 s, no SSH, no fastboot, phone gone from USB; the webcam at 01:08 shows the lock screen reading 1:02: UI alive after USB went. One boot a side doesn't count; sysmem is dropped for now, since Control Center lost 3.8 ms and it's expected to add DDR heat.
- Cause of the USB loss unknown; whether sysmem caused it isn't shown. Waiting for flamereboot=1800 (about 01:22).

### 2026-11-01 00:07 n2: UI soak: no silent reset in 60 min; one no-fault hang at 867 s, recovery failed, rest void
- tools/sunfish-uisoak.sh n2, same setup as n1, boot from ca1c (kernel139, sweetrowa55=6, SWEET_DEADFILL=2, M2V_SHADOW_CACHE=1), 23:06:52-00:06:52. No reset. `[1151.016] present: GPU hang or fault (fence 91789, done 91777, smmu fsr 0 far 0 fsynr0 0): reset`, recover2 FAILED after 2064 ms, tlbstatus 1 still at +30 s, backboardd killed: G1, then G3 and CPU drawing: backboardd 10164 CPU s, A76 and A55 hot (94 C). So everything after 867 s of the soak is void, including its 96.4 C max and 86 k submit total.
- The tool's first count said G1 0: its pattern missed `fsr 0 far`. Fixed; it now also stops at a failed recovery.
- Rates with mode 2: 2 G1 (df5, n2) in about 250 k submits; 0 with the old setup in 151 k (z1), 2 in 113 k (soak1). Doesn't show a doubling. Cache and A55 rows stay env and boot args. Silent reset: not repeated in 1 of 1 rerun.

### 2026-10-31 23:05 ca1: cache not the cause of the Calculator fragments; clock not clipped
- kernel139, everyday args + sweetrowa55=6, boots ca1b (SWEET_DEADFILL=2) and ca1c (+ M2V_SHADOW_CACHE=1). Tap AC, 7, 8, 9, shots after 3 s. Rows 800-880: 0 lit pixels in all 8 shots, cache on and off, so st1's fragments didn't come back (one-off, cause unknown). Cache off showed a faint dotted line above the digits once (rows 560-600, 8 px over 120); cache on none. research/ca1.
- Status bar clock fully visible in the framebuffer on still Home, Settings, Maps, Notes. The clipped clock and doubled back label in the 22:49 shot were an app switch caught by the 8 s readout (the user spotted it); the digits in the webcam frame sit under the camera hole (the user's point).

### 2026-10-31 22:55 n1: UI soak: 24.5 min, 0 GPU bugs, then a silent reset to fastboot
- tools/sunfish-uisoak.sh n1, kernel139 sweetrowa55=6 and everyday args, SWEET_DEADFILL=2, M2V_SHADOW_CACHE=1, boot 20261031-221630, started 22:30. 1470 s, 126.8 k submits, G1 0, G2 0, G4 0, backboardd restarts 0, max 96.1 C, caps 0. The log just stops at uptime 2258.298 at normal `MTL2` lines; 72.8 C at 2256 s; phone in fastboot at 22:54. No panic line, no hang line. Before: z1 3 G2 and 1-2 G4 in 34 min.
- Cause unknown; silent resets are backlog B4. Whether the new settings are involved isn't shown.

### 2026-10-31 22:30 st1: stale-content survey of M2V_SHADOW_CACHE=1: nothing stale from the cache
- kernel139 sweetrowa55=6 and everyday args, SWEET_DEADFILL=2, M2V_SHADOW_CACHE=1, bundle work/metal2-shadow, boot 20261031-221630, 4454 mV, 0 hangs, 0 submit errors. `SHADOW: 5s copies 2 skipped 639 hashed 94521 KB, hash 40966 us`. research/st1.
- Pass: lock clock, Notes typing, Calculator digits, Settings long scroll, Stopwatch, Rotation Lock tile, App Library and Spotlight, Safari page and scroll, wallpaper on lock and wake. Status bar clock stale for about 1 min at Home: same as the old B49 (seen without the cache). Thin dotted fragments under Calculator digits (calc2, calc3): being checked. Maps dark map, Photos grid fine, not compared with df5.

### 2026-10-31 22:15 sh1: shadow copy cache: Home 25.7 -> 38.3 swaps/s, Control Center 23.3 -> 28.7, Settings 56.1 with p95 20.5
- `tools/sunfish-perfab.sh sh1 3 --secs 15 --screens home,cc,settings`, kernel139 sweetswapbig=6 sweetse9=2 sweetcampmic=2 sweetampreset=1 SWEET_DEADFILL=2, bundle with bridge 0116, sweettrace=1, 9 boots, 4454 mV, 78 k submits, 0 G1, 0 G2, 0 G4, 0 void. M2V_SHADOW_CACHE 0 / 1 / 2 (2 = never recopy, ceiling):

| screen | swaps/s (min-max), 0 / 1 / 2 | frame ms | p95 ms | >100 ms gaps per run | 1st submit ms |
|---|---|---|---|---|---|
| Home | 25.7 (24.2-26.6) / 38.3 (37.8-38.6) / 38.1 | 38.9 / 26.1 / 26.3 | 64.6 / 45.8 / 49.4 | 1.7 / 0.3 / 0.3 | 37.2 / 24.8 / 24.8 |
| Control Center | 23.3 (22.7-24.1) / 28.7 (28.5-29.1) / 28.3 | 42.9 / 34.8 / 35.3 | 88.2 / 60.1 / 59.1 | 20.3 / 1.7 / 0.7 | 37.6 / 28.7 / 28.6 |
| Settings | 53.3 (46.7-56.7) / 56.1 (55.9-56.4) / 55.0 | 18.8 / 17.8 / 18.2 | 23.6 / 20.5 / 25.9 | 0 / 0 / 0.7 | 8.7 / 7.9 / 8.0 |

- Observed: mode 1 reaches the mode 2 ceiling; hash cost small (`SHADOW: 5s copies 0 skipped 891 hashed 13486 KB, hash 5852 us`). 3 boots a side, gains far over the 1 ms stop rule. Still GPU-bound on Home and Control Center (93-99 %).
- Not checked: stale content in mode 1. Default stays off until a survey.

### 2026-10-31 21:15 sv2, sv3, gh1: mode 2 survey; doubling mostly camera; traces void
- sv2, kernel139 SWEET_DEADFILL=2, boot 20261031-205524, research/df5: lock, Home x2, Today, Control Center, Settings x2, Calculator, Notes, Clock, Mail, Reminders, Calendar, Files fine. Maps: horizontal slice tears on the blue dot and a button, first try one torn frame. Photos: black gaps (B44). Safari: black tab thumbnail, not conclusive. 0 hangs, 0 restarts.
- gh1, boots 20261031-210714 (off) and the one from 21:08:52 (on), 8 webcam frames each: Settings doubling in both modes, along the scroll axis by about one glyph: exposure. Mode 2 had 2 strong frames in 4, off 1 heavy in 4: too few, and pages differed (Focus against root). Home frames static: inconclusive. No torn or stale region in any frame. Earlier "ghosted icons" (:141) not confirmed as real. Needs a look by eye: docs/user-tasks.md.
- sv3, boot 20261031-210239, MESA_GPU_TRACES=print, TU_SWEET_DRAW_TRACE=1: backboardd signal 11 at 0x8 once in the Home window and once in Control Center at about 12 MB/s of trace; both trace files were partial, void as timings; shares in research/notes/sv3-*-trace.txt. Cause not separated from mode 2, but df5 had no restart.
- Shares, Home full pass 1125x2436: 45.6 bins, 21.3 ms, 74 draws, 8.8 ms in draw spans (33 %), f8357eea3897 49 % of that; copy_buffer_to_image 6.0 ms per frame, 22.5 copies. Control Center: 45 bins, 19.3 ms, 6.6 ms draws.

### 2026-10-31 20:55 df5: SWEET_DEADFILL=2: Settings 47.2 -> 54.8 swaps/s, Home and Control Center nearly flat; 1 hang
- `tools/sunfish-perfab.sh df5 5 --secs 15 --screens home,cc,settings`, kernel138 sweetswapbig=6 sweetse9=2 sweetcampmic=2 sweetampreset=1, sweettrace=1, 10 boots. 69 k submits, G1 1 (df2 b5), G2 0, G4 0, 4 void runs (2 too hot, 1 hang + the screen after it). Base then df2:

| screen | ok/void | swaps/s (min-max) | frame ms | p95 ms | GPU % | 1st submit ms |
|---|---|---|---|---|---|---|
| Settings | 5/0, 4/1 | 47.2 (43.0-54.3) / 54.8 (53.7-55.9) | 21.2 / 18.2 | 32.6 / 27.3 | 75 / 65 | 12.9 / 8.8 |
| Home | 4/1, 4/1 | 26.1 (24.9-26.9) / 26.7 (26.6-26.8) | 38.3 / 37.5 | 53.1 / 55.2 | 99 / 100 | 36.2 / 36.0 |
| Control Center | 5/0, 4/1 | 21.6 (20.6-22.1) / 22.6 (21.7-23.3) | 46.3 / 44.2 | 104.0 / 97.4 | 99 / 98 | 41.6 / 38.7 |

- Observed: Settings gains 2.9 ms and is much steadier. Home's 36 ms isn't in these fills: the investigator's first guess is wrong for Home. Settings here ran last, after two hot screens, and base got 43-45 in 3 of 5 boots against 50-55 in cpu1; temps were below the throttle (A76 2208 MHz), so why the base dropped is open. Swap count and p95 in a run with a hang are void.
- A hang under df2 at 1 in about 34 k submits against 0 in about 35 k for base: too few to read. df2 stays env-only until a survey for ghosting and a soak.

### 2026-10-31 19:55 cpu1: bundle built for Cortex-A76: no gain; Home 27, Control Center 22 swaps/s; heat throttles Settings
- `tools/sunfish-perfab.sh cpu1 5 --secs 15 --screens settings,safari,cc,home`, kernel137 sweetswapbig=6 sweetse9=2 sweetcampmic=2 sweetampreset=1, sweettrace=1, idle fixes in, 10 boots, 4455-4456 mV. 100 k submits, 0 G1, 0 G2, 0 G4, 2 void (too hot, 95 C). Window means, default then A76 build:

| screen | swaps/s (min-max) | frame ms | p95 ms | GPU % | 1st submit ms | start / max C | A76 min MHz |
|---|---|---|---|---|---|---|---|
| Settings | 52.8 (50.5-55.2) / 51.0 (47.0-53.8) | 18.9 / 19.6 | 28.0 / 29.8 | 80 / 80 | 12.4 / 12.5 | 88 91 / 90 94 | 2052 / 2016 |
| Safari | 55.0 (53.3-57.3) / 54.8 (53.7-56.6) | 18.2 / 18.3 | 18.4 / 18.5 | 71 / 68 | 9.6 / 8.9 | 90 91 / 90 91 | 2208 / 2208 |
| Control Center | 21.9 (21.2-22.4) / 22.1 (21.3-22.5) | 45.7 / 45.3 | 97.2 / 91.0 | 98 / 98 | 41.2 / 41.0 | 86 88 / 87 88 | 2208 / 2208 |
| Home | 26.8 (26.0-27.1) / 27.0 (26.9-27.1) | 37.3 / 37.0 | 41.0 / 41.2 | 100 / 100 | 35.7 / 35.7 | 84 86 / 84 86 | 2208 / 2208 |

- Observed: every screen within noise, under 1 ms, 5 boots a side: no gain (9368 LSE atomics, A76 tuning). Default build stays; `PHONE_CPU=cortex-a76` stays in the build script, off.
- Observed: the hottest sensor goes from 71-74 C to 84-90 C in the ~5 s lead-in. Settings runs at 91-95 C and the A76 falls as low as 1800 MHz inside a 15-s window, without a cap. Per-boot Settings swaps/s tracks it only loosely (b5 base 55.2 at 1800 min). Home and Control Center are GPU-bound (98-100 %).
- Tool's startC is taken after the lead-in, so it's not the temperature at the end of the cool wait.

### 2026-10-31 16:45 Speed plan (the user's choice; no phone run)
- Targets, test schedule and priorities in plan.md. Evidence for the bench tool: boots with one bundle averaged 53 and 58 swaps/s (al1 A1, A2); hand-run scripts voided runs (f1/f2, the panel-off A/B). Older entries moved to the archive: [archive/features/smoothness.md](../archive/features/smoothness.md).

### 2026-10-31 06:35 al1: bridge allocator A/B: 50 -> 56 swaps/s average, up to 58.7; p50 at the tick in one boot
- kernel134 sweetswapbig=6 sweetampreset=1 sweettrace=1 sweetcpusample=1; 4 boots A B A B, 20261031-062315, 062721, 063013, 063312; 4 Settings long swipes each, window 5-15 s:

| boot | bundle | swaps/s | swap-to-swap p50 ms | p95 ms | 1st submit ms | CPU ms per swap |
|---|---|---|---|---|---|---|
| A1 | allocator | 55.5 54.4 52.4 50.3 | 17.4-18.1 | 25.3-31.7 | 13.0-13.1 | 14.0-15.6 |
| B1 | without | 49.5 51.6 51.6 52.1 | 18.3-18.8 | 30.3-31.1 | 12.9-13.1 | 11.7-15.4 |
| A2 | allocator | 58.2 57.4 58.4 58.7 | 16.8-17.0 | 19.9-21.5 | 13.0-13.2 | 14.5-15.2 |
| B2 | without | 49.0 49.5 49.3 50.2 | 19.4-20.0 | 27.8-30.8 | 13.0-13.2 | 12.4-16.6 |

- Observed: mean 55.7 against 50.3. Render thread CPU per swap isn't lower (14.7 vs 13.7, inside the spread), so how the allocator helps isn't explained: maybe other threads, or lock contention with the render thread. Boots differ a lot (A1 vs A2): 2 boots each is few. backboardd footprint +4-6 MB (72-73 against 68-69 MB). 0 hang lines in A2, B2 and the checked logs (two greps came back blank); no new crash files seen in A1.
- Allocator bundle stays. Everyday: kernel134 sweetse9=2 sweetcampmic=2 sweetswapbig=6 sweetampreset=1 + bundle-alloc build. 60 is close: p50 16.8 ms in A2 is about one tick.

### 2026-10-31 03:30 pc3: profile of the render thread with the everyday setup: flat
- kernel132 sweetswapbig=6 sweetcpusample=1, bundle with dead fill mode 1, boot 20261031-031919.log, 4454 mV, no reset. `tid 582 n 8000 user 7615 kernel 385 missed 0`. research/notes/pc3-hist.txt. Images: SweetMetal2 47.7 %, Turnip 11.7, QuartzCore 10.9, malloc 10.1, libsystem_platform 6.5, kernel 4.8, libobjc 4.0. Functions: sys_dcache_flush 4.5, draw_resident 3.4, memmove 2.4, settle_draw 2.1, unfair lock 1.7 + 1.0, tiny malloc 1.6, then all under 1.2.
- Reading: no single target left: at ~14 ms per frame the flush is ~0.6 ms, malloc ~1.4 ms. Candidates stay: flush off the thread, a Rust thread-local allocator, redfin 0130-0137, unchanged-draw fast path.

### 2026-10-31 03:20 h1: dead fill mode 1 is the bridge's default: 53 swaps/s with a clean survey
- Bundle with mode 1 as default (SWEET_DEADFILL unset = 1, 0 off) and the two audit fixes, kernel132 sweetswapbig=6, boot 20261031-023756.log, 4440 mV. Settings long swipe: 53.5 / 52.8 swaps/s, swap-to-swap p50 18.67 / 18.68, p95 22.90 / 23.66 ms, CPU per swap 11.0 / 16.5 ms on A76, GPU busy 77-87 %. DF mode1 dropped 92 -> 9246, 95-215 per s in scrolls; cleared and held 0. So the fixes don't hurt it. Home fpsbench with pauses: 35.2 swaps/s (not a steady load).
- Survey, research/df3: lock, Home, Today, Settings + subpage, Calculator, Notes, Clock, Mail, Files, Reminders, Calendar fine. Control Center fine, with a red blob at the bottom on the panel (probably the Music icon blurred). Safari top half black on a GitHub page: not told apart from content. Maps no tiles, Weather no data: probably network or location, unchecked. Open and close frames saved, mostly unread.
- 1 GPU hang in 3 min mixed use: 654.099 s "v2 fence wait: GPU hang or fault (fence 17537, done 17511, fsr 0)". This entry first said recovered; it did not (gpu.md 2026-10-31 04:00). 5 sleep/wake cycles fine.
- Everyday setup: kernel132 sweetswapbig=6 + this bundle. Limits now: CPU 11-16 and GPU ~13 ms per frame, 18.7 ms between swaps.

### 2026-10-31 02:35 m0/m1/m2: SWEET_DEADFILL=1 keeps the speed: 53 swaps/s; the stale clock isn't caused by it
- kernel132 sweetswapbig=6 sweettrace=1 sweetcpusample=1, env before a fresh boot, boots 20261031-015720.log (m0), 021047 (m1), 022306 (m2), 0 hangs. Settings long swipe:

| config | swaps/s | swap-to-swap p50 / p95 ms | 1st submit ms | 2nd submit ms | CPU ms per swap | GPU busy % |
|---|---|---|---|---|---|---|
| off | 33.3, 34.4 | 29.0 / 48.7, 29.0 / 31.4 | 26.0 | 0.57 | 10.1, 10.9 | 94-100 |
| SWEET_DEADFILL=1 | 53.8, 52.7 | 18.2 / 26.8, 18.4 / 30.0 | 13.0, 13.2 | 0.57 | 14.2, 15.6 | 78-86 |
| SWEET_DEADFILL=2 | no usable run: the bench got 1-118 swaps, the phone locked in some | - | - | - | - | - |

- Earlier p1b for mode 2: 52.3, 54.4, first submit 8.8 ms. So mode 2's clear saves ~4 ms of GPU that isn't the limit now; the CPU is. Mode 1 has no held passes or clears, so it's the simpler choice: making it the default in the bridge. GPU busy 78-86 %: both CPU and GPU sit close to the 16.7 ms mark.
- Clock: readings every ~36 s for ~8 min: all three configs alternate between the current time and older frames, including off: off showed 1:58 at real 2:01, 1:59 at 2:08, and webcam 2:06 at 2:09:41. m2 webcam :29 at 2:32. Phone `date` is right. Correction to the f1/f2 entry: mode 2 isn't the cause; f2's 3.5 min clean was too short. Alternating values suggest several surfaces in rotation for the status bar with old contents. Not tested: an older kernel, to see if 0250 or 0251 brought this.

### 2026-10-31 02:00 f1/f2: DEADFILL survey with a control: stale clock points at mode 2; Compass tears are capture artefacts
- kernel132 sweetswapbig=6; f1 SWEET_DEADFILL=2 (20261031-012907.log), f2 env empty (20261031-014637.log). PNGs research/df2 (a_ mode 2, b_ control).
- Compass: both modes: shots 1-2 clean, 3-5 torn; webcam clean. So flame-shot tears on moving content: the Compass "shear" in apps.md g1 was a capture artefact. Correction to that entry. Tips: black rectangles in the banner in both modes, webcam agrees: real, not mode 2. Photos: garbled thumbnails in both, webcam agrees: real or old files.
- Clock: mode 2 tracked 1:32-1:34, jumped back to 1:32 at 01:35:16 in Settings, still 1:32 at 01:36:05, webcam agrees; fine again after Home. Control: current for 3.5 min. With e1's 12:59 at 1:0x, that's 2 events with mode 2, 0 without; small sample.
- Hang: f1 mixed use, Maps: 905.879 s "GPU hang or fault (fence 23128, done 23111, fsr 0)", recover2 and recover3 failed, DeviceLost in Maps, then 10-20 fps. No panic. Control had no mixed run. Same shape as b1's (no mode 2). Hang rate is an open problem on its own, and a failed recover3 leaves CPU drawing until reboot.
- Other screens with mode 2: Home, Control Center, Settings, Camera UI, Maps UI clean; Maps no tiles; page swipe shows ghosted icons in motion on the webcam, clean after.
- Speed in this run: void: a script misuse set kern.sweet_ui_swapbig wrong: Settings 30.6 and ~40. Fixed fpsbench: mode 2 42.6, control 48.3 on Home (bench still alternates and pauses; not a steady load). Mode 2 stays off. Repeat with m0, m1, m2 and an 8 min clock watch running; code audit in research/notes/deadfill-audit.md.

### 2026-10-31 01:30 e1: first DEADFILL survey: 17 screens clean; three findings, likely test artefacts; Home 57 swaps/s
- kernel132 sweetswapbig=6 SWEET_DEADFILL=2 before a fresh boot, boot 20261031-005605.log, 20261031-012256.log, 4455 mV, no reset, no hang. Clean (PNG read, research/df): lock, Home x2, Control Center, Today, Settings, Calendar, Mail, Notes, Reminders, Clock, Safari, Files, Calculator, Weather, App Library, Utilities folder. 5 sleep/wake cycles fine. Env set back to empty.
- Not run: Tips, Maps retry, app switcher, Camera preview, mid-animation frames, 3 min of use. Maps showed only a spinner; Camera went to the lock screen.
- Findings: (1) Compass digits and lower dial sheared, twice, with mode 2; also in g1 without it, but clean in d2. (2) Status-bar clock 12:59 at real time 1:0x in Settings, Calendar and others. (3) After the Home bench, research/df/fps_end.png is badly sheared and the webcam shows a page overlaid.
- Reading (guess): (3) is my bug: with --wait 0 the bench sent swipes faster than the phone ran them, so it was still swiping; fixed with gaps that include the swipe, and that also invalidates any --wait 0 run's pacing. fps_end.png's stair steps look like a rolling shutter: flame-shot reading the live surface while it moves. So (1) may be a capture tear too: Compass moves constantly. Second survey with a control is running. (2) unexplained.
- Numbers: Settings df1 52.3 swaps/s, swap-to-swap p50 19.96 / p95 24.19 ms, first submit 8.77 / 10.63, CPU 17.3 ms per swap on A76. Home fpsbench 57.4 swaps/s, 57.2 panel updates/s (2 samples; bench queue overrun, so continuous motion). Tick bins in this tool look offset by one at this spacing: use the ms columns.

### 2026-10-31 00:55 p1b: SWEET_DEADFILL=2 at full canvas: 52-54 swaps/s, first submit 8.8 ms
- kernel132 sweetswapbig=6 sweettrace=1 sweetcpusample=1, env set before a fresh boot, /var/local/mtl2-telemetry, boot 20261031-004812.log, 4454 mV, no hang, no DeviceLost. Settings long swipe, window 5-15 s. Base: 34.4 swaps/s, first submit 25.9:

| run | swaps/s | 1st submit p50 / p95 ms | 2nd submit p50 / p95 ms | CPU ms per swap | GPU busy % | DF dropped / cleared / held per frame (estimate) |
|---|---|---|---|---|---|---|
| df1 | 52.3 | 8.77 / 10.63 | 0.57 / 3.54 | 14.7 | 61-66 | ~3.5 / ~0.87 / ~3.1 |
| dg2 | 54.4 | 8.95 / 9.21 | 0.57 / 1.24 | 11.7 | 62-65 | ~3.3 / ~0.8 / ~3.4 |

- Observed: the leading fills are dead: 3.4 drops per frame here, against 12 in ~720 frames in df1 (Oct 30 08:00), which was measured on other loads through a restart. Settings and Control Center look clean on the webcam; pixel-level not checked. So the DEADFILL scissor limit wasn't the blocker on this load, and the relaxed rule isn't needed yet.
- p1a drawlog: the 20-pass cap is spent during boot, so no Settings pass. Boot passes lead with 3 identical full-canvas fills: PBGRAXm_A2Xhf, blend 0, mask 15, sc 0,0,1125,2436, then PBGRAXm_TimgA2Xhfn_Isrc.
- A first run swiped over an open Control Center: 26.3 swaps/s, GPU 100 %, discarded; a blur-heavy screen stays costly.
- Not default yet: mode 2 can draw wrong pictures: survey of all screens running. The fix gives a real chance at 60: CPU 11.7-14.7 ms is now the part to watch.
- Part 2 of that run: with M2V_SHADOW_DUMP=all, Compass (130x90 BGRA row 544, 6 keys, kept) and Tips rendered clean in research/b44/d2-*.png, though g1 had them sheared on the same kernel: not explained; the survey opens each twice.

### 2026-10-31 00:40 gt3: the fixed GPU trace works: five draws with one shader are about half of the draw time
- turnip 0109, fresh boots, research/gt3-pass.log (18.8 MB: boot + 20 s Settings swipe), research/gt3-draw.log (122 MB, with TU_SWEET_DRAW_TRACE=1: boot, launch, 5 s swipe). No fopen failure, the file grows with flush. Traces include boot and app launch, and the draw trace inflates times (1125x2436 pass mean 22.46 ms against 9.98): shares only.
- Pass log: 1125x2436 passes n=420, mean 9.98 ms across idle and motion, 44 bins, load 0.39 ms, store 0.48, binning 0.13. draw_ib_gmem 4296.7 of 4455.9 ms of render-pass time. copy_buffer_to_image 7.2 per frame, 1.77 ms per frame (p50 0.18 ms in a frame buffer). So copies are a small part, not 31 % as the gt1 trace suggested on Oct 24.
- Draw log: draw spans 2156 ms over 325082 spans. fs f8357eea3897 + vs 227c3411bdc9: 40356 spans, 1130.7 ms (52 %), mean 28 us; in draw indices 0-4 of the pass (5 draws replayed in 44 bins). Next: fs 42c74821f542 199.4 ms, fs 666966cbf987 196.0 ms, fs 780adfdf3ada 102.0 ms. Draw spans cover ~9 of 22 ms per traced pass: the rest isn't explained. Analysis in research/notes/fps-gpu-draws.md.
- The turnip/shim bundle with the trace fix stays installed; off without the variables. Tool takes TOP as a plain number.

### 2026-10-31 00:30 m1: the slow mode comes from restarting backboardd; a fresh boot gets the fast one
- kernel131 sweetswapbig=6 sweettrace=1 sweetcpusample=1, boots 20261031-001444.log, 20261031-002157.log, 4455 and 4454 mV, no hang, no DeviceLost. Settings long swipe:

| step | swaps/s | swap-to-swap p50 / p95 ms | 1st / 2nd submit ms | CPU ms per swap |
|---|---|---|---|---|
| fresh boot | 32.8, 34.4 | 29.6 / 39.0, 29.1 / 31.5 | 25.9 / 0.57, 26.0 / 0.56 | 10.3, 9.1 |
| backboardd restarted | 28.3, 28.3 | 35.3 / 37.8, 35.4 / 37.8 | 25.9 / 6.75, 26.0 / 6.76 | 9.3, 9.4 |
| + TU_FETCH_GMEM_MIN_DRAWS=100000, restart | 29.3, 29.3 | 34.1 / 36.5, 34.1 / 36.3 | 26.0 / 5.50 | 6.6, 9.6 |
| + TUDBG=sysmem | 30.5, 30.5 | 32.7 / 37.8, 32.5 / 49.4 | 24.6 / 5.56 | 8.0, 13.5 |
| fresh boot + that setting | 33.1, 34.1 | 29.2 / 47.1, 29.3 / 32.1 | 25.9 / 0.77 | 14.0, 9.1 |

- Correction to k1: TU_FETCH_GMEM_MIN_DRAWS isn't the fix; its good row there came from a different state and it doesn't restore the fast mode here, so it doesn't go in as a default. Trigger: a backboardd restart adds ~5-6 ms to the second submit, unchanged by the knobs. Not known why (guess: after a restart something re-uploads each frame, or a surface stays in a slower path). Daily use only hits this after a backboardd crash or recovery.

### 2026-10-31 00:10 k1: Turnip knobs at full canvas: first submit stays ~26 ms; a second mode with 28 swaps/s found and avoided
- kernel131 sweetswapbig=6 sweettrace=1 sweetcpusample=1, boot 20261030-235443.log, 4440 mV, no hang, no DeviceLost. Settings long swipe, backboardd restarted per setting. GPU busy ~99 % in all:

| setting | swaps/s | swap-to-swap p50 / p95 ms | 1st submit ms | 2nd submit ms | CPU ms per swap | visual |
|---|---|---|---|---|---|---|
| base | 28.3, 28.3 | 35.3 / 37.8 | 25.9 | 6.75 | 9.5, 10.4 | OK |
| TUDBG=sysmem | 30.7, 30.4 | 32.0-32.7 / 36.5-59.5 | 24.5 | 5.57 | 11.3, 12.2 | OK |
| TU_SWEET_PRIM_ORDER=0 | 30.0, 29.9 | 33.4 / 35.6 | 25.3 | 5.45 | 9.5, 10.7 | Control Center broken: black, no blur, panels missing |
| PRIM_ORDER=0 + sysmem | 32.6, 32.6 | 30.8 / 34.2 | 22.6 | 5.53 | 11.3, 12.2 | Control Center broken |
| TUDBG=noubwc | 28.3, 28.3 | 35.3 / 37.8 | 25.9 | 6.75 | 10.0, 9.3 | OK |
| TU_FETCH_GMEM_MIN_DRAWS=100000 | 34.2, 34.1 | 29.3 / 31.3 | 25.9 | 0.77 | 10.3, 9.3 | OK, blur fine |
| base again | 28.4, 28.3 | 35.3 / 37.4 | 25.9 | 6.75 | 9.1, 7.0 | OK |

- Observed: this base (2nd submit 6.75 ms, 28 swaps/s) differs from a1's bound runs (2nd 0.56 ms, 34.4). So the same settings can land in two modes. Setting 6 reproduces 0, so it isn't drift within one boot. Guess: a1 ran on a fresh boot, these after a backboardd restart, or autotune picks a different mode for the small second pass. TU_FETCH_GMEM_MIN_DRAWS=100000 gives the good mode in every run so far.
- No knob moves the first submit by much without breaking the picture: draws are the cost. sysmem saves 1.4 ms, costs ~1.5 ms CPU. Next: per-draw trace, fixed (research/notes/fps-gpu-full.md).

### 2026-10-30 23:50 w1-w4: 32 sleep/wake cycles with no hang, bind on or off; bind goes in the everyday boot
- 8 cycles each, `key 0 150`, Settings open, ~5 s off, ~11 s on: w1 kernel128 (20261030-232041.log), w2 kernel131 bind off (232730), w3 and w4 kernel131 sweetswapbig=6 (233333, 233931). 0 hangs, 0 recover lines, SSH up. Frames of w2-w4 not viewed, so "lit on wake" is confirmed only for w1. Swaps/s not measured.
- b1's hang is in its own log (20261030-230323.log:8172). Tally with the bind on: 1 hang in 19 wakes, off: 0 in 16. Too few to blame it: fsr 0, a busy-type hang like the older display-change hangs in gpu.md (kernel78 etc., before the bind existed). b1 also had a poller running (sysq/dash every ~90 ms) and Wi-Fi scans right before. Unexplained; if it recurs, read "DEVLOST first" and recover3's "gfx rail TIMEOUT".
- Decision: kernel131 with `sweetswapbig=6` is the everyday boot; the pacing gain is large and the evidence against is one event. 650 MHz (`sweetgpufreq=6`) stays off pending a soak: backlog Later.

### 2026-10-30 23:20 b1/g6/g7: bind robustness: GPU hang after a wake (cause unknown); 650 MHz gives 38 swaps/s; 800 MHz fails bring-up
- b1 kernel131 sweetswapbig=6 sweetcpusample=1 sweettrace=1, boot 20261030-230323.log, 4453 mV. Boot: _binds 1, _pri 54, _a55 0. Sleep/wake with `key 0 150`: 5 s after wake 3 (122.078): "present: GPU hang or fault (fence 1019, done 1013)", recover3 "gfx rail TIMEOUT, sptprac TIMEOUT", "hfi ctrl init TIMEOUT", "recover3: FAILED after 1585511 us", "recover2: FAILED after 1711527 us", backboardd killed, "DEVLOST first (turnip) VK_ERROR_DEVICE_LOST", submits 461, faults 1. No panic, no reset. UI back, then a later bench only 15-17 swaps/s (no SWEET TL or GPU lines: guess: CPU fallback for the rest of the boot).
- Other items passed: backboardd kill -9 (new tid, _binds +1, UI back < 40 s); Calculator, Maps, Safari, Photos, Control Center, Camera open; 3 min in Camera, no bad lines. End counters a76 9629, a55 2, skip_rt 0, skip_cpu 0.
- So the bind isn't default yet: a control is running to tell whether it caused the hang. An unrecovered wake hang would be serious in daily use either way.
- GPU clock, kernel131 bind on, Settings long swipe:

| sweetgpufreq | MHz read | swaps/s | swap-to-swap p50 / p95 ms | GPU per frame ms | GPU busy % | thread CPU ms per swap |
|---|---|---|---|---|---|---|
| 5 (a1 r2-r5) | 564 | 34.3-34.4 | 29.0 / 31.3-31.5 | 26.5 | 97-100 | 6.8-11.0 |
| 6, boot 20261030-231240.log | 649-650 | 37.5, 38.2, 38.2 | 26.1-26.7 / 28.2-30.0 | 23.6-24.0 | 97-100 | 7.7-10.6 |
| 7, boot 20261030-231536.log | "dcvs 800 MHz acked ret 0" | - | - | - | - | - |

- Observed: 650 MHz: +11 % swaps/s, in proportion to the clock, so the full canvas is plainly GPU-capped; 0 hangs in ~1 min of use only. 800: "ring me_init: rptr 0 wptr 9 ... TIMEOUT", "bring-up FAILED in 1583 ms", CPU fallback at 16-17 swaps/s. Level 8 skipped. No temperature source found. 650 needs a soak before default: history: gpu.md "a real GPU hang once at 565 MHz".

### 2026-10-30 23:00 a1/a2: render thread bound to one A76 (xnu 0250, kernel131): @2x 51 swaps/s; full canvas rate unchanged but pacing even
- `kern.sweet_ui_swapbig` live, boots 20261030-225151.log (a1), 20261030-225632.log (a2), 4439 and 4454 mV, no reset, no DeviceLost, no visible hitch. Settings list, one 15 s swipe each, window 5-15 s. `_pri` 54 throughout (non-RT), `_skip_rt` 0, `_binds` +1 per bound run, `_a55` +0 or +1. Warm-up runs left out.

| canvas | run | mode | swaps/s | ticks 0 / 1 / 2 / 3+ % | swap-to-swap p50 / p95 ms | GPU per frame ms | thread CPU ms: A55 / A76 | GPU busy % |
|---|---|---|---|---|---|---|---|---|
| full | r1 | off | 34.1 | 6 / 66 / 17 / 11 | 22.8 / 64.2 | 26.6 | 4.9 / 9.7 | 97-99 |
| full | r2 | CPU 6 | 34.4 | 0 / 100 / 0 / 0 | 29.0 / 31.5 | 26.5 | 0.0 / 11.0 | 97-99 |
| full | r3 | off | 34.1 | 4 / 74 / 13 / 9 | 26.2 / 60.1 | 26.5 | 4.0 / 9.9 | 96-99 |
| full | r4 | CPU 6 | 34.4 | 0 / 100 / 0 / 0 | 29.0 / 31.3 | 26.5 | 0.2 / 6.6 | 98-100 |
| full | r5 | CPU 7 | 34.3 | 0 / 100 / 0 / 0 | 29.2 / 31.3 | 26.5 | 0.0 / 10.0 | 97-100 |
| full | r6 | off | 34.5 | 7 / 65 / 18 / 11 | 21.3 / 58.5 | 26.5 | 5.0 / 10.5 | 97-99 |
| @2x | r1 | off | 40.0 | 3 / 59 / 27 / 12 | 20.2 / 50.8 | 12.6 | 6.0 / 13.9 | 57-60 |
| @2x | r2 | CPU 6 | 50.0 | 7 / 65 / 27 / 0 | 18.1 / 32.0 | 12.6 | 0.4 / 14.6 | 65-73 |
| @2x | r3 | off | 44.6 | 5 / 63 / 24 / 8 | 18.5 / 41.5 | 12.5 | 5.0 / 12.6 | 63-65 |
| @2x | r4 | CPU 6 | 51.8 | 9 / 65 / 25 / 0 | 17.9 / 31.7 | 12.6 | 0.5 / 14.3 | 65-75 |
| @2x | r5 | CPU 7 | 51.5 | 7 / 70 / 23 / 0 | 17.5 / 32.2 | 12.5 | 0.0 / 11.3 | 73-74 |
| @2x | r6 | off | 45.2 | 8 / 61 / 24 / 8 | 18.5 / 41.1 | 12.5 | 2.5 / 9.8 | 61-66 |

- Observed: the guess held: A55 share goes to ~0, and 3+ tick gaps go from 8-12 % to 0 on @2x and from 9-11 % to 0 on full. @2x: 40-45 -> 50-52 swaps/s. Full: rate unchanged, but swap-to-swap p95 58-64 -> 31 ms: much even frame pacing. Binding didn't make the thread wait in these runs. Thread CPU numbers swing between windows; they're rough.
- Reading: the full canvas is now plainly GPU-capped: GPU busy 97-100 % and a flat 29 ms per swap. Earlier "no gain from GPU clocks" was measured when the CPU capped the rate, so worth retrying. @2x still has ~25 % 2-tick gaps. The tick column for bound full-canvas runs reads 1 tick at 29 ms apart; the tool's bins look off at that spacing, so trust the ms columns.
- Not tested: sleep/wake, backboardd restart, app opens, camera with the thread bound; if CA changes its policy to realtime after binding, an assert could panic.

### 2026-10-30 22:50 n1: @2x + fast bridge: ~41 swaps/s, CPU 14 ms per swap, the frame fits a tick on average but misses it often
- kernel130 sweetfb_2x=1 sweettrace=1 sweetcpusample=1, bundle 0114 + LTO, boot 20261030-223823.log, 4455 mV, no reset, no DeviceLost. Settings list, one 15 s swipe, window 5-15 s, no gaps > 100 ms:

| run | swaps/s | ticks between swaps 0 / 1 / 2 / 3+ % | swap-to-swap p50 / p95 ms | swap -> first submit p50 / p95 ms | GPU per frame ms | CPU ms per swap | GPU busy % |
|---|---|---|---|---|---|---|---|
| f1 | 39.9 | 2 / 54 / 37 / 7 | 20.87 / 42.27 | 20.61 / 41.99 | 12.47 | 13.4 | 57, 57 |
| f2 | 40.8 | 3 / 54 / 35 / 7 | 19.70 / 40.32 | 19.41 / 39.81 | 12.52 | 14.0 | 61, 55 |
| f3 | 41.4 | 2 / 62 / 29 / 9 | 18.69 / 42.02 | 18.37 / 41.35 | 12.53 | 14.5 | 59, 64 |

- Observed: compare cs2 (old bridge at @2x): 38 swaps/s, 22-23 ms CPU per swap. CPU + GPU p50 26.5-27.3 ms per frame. Swap -> first submit stays ~5 ms over the CPU time, p95 40+ ms. Present calls per tick 0.65-0.68. Lock screen still has the red strip, status bar missing.
- Guess: mean CPU is under the tick, so the misses are variance: frames landing on an A55 (A55 20-30 % of the thread's CPU) run slower and miss. If so, strict A76 placement could move many 2-tick frames to 1 tick. Patch being written. Not known: whether a miss comes from CPU, or from the wait for the GPU to finish. The full canvas stays capped by its 26.5 ms GPU per frame.

### 2026-10-30 22:45 ml1: bridge 0114 + LTO: CPU per swap ~24 -> ~18 ms, swaps/s unchanged: at full canvas the GPU now caps the rate
- kernel130 sweettrace=1 sweetcpusample=1, boot 20261030-222005.log, 4454 mV, bundle UUID 4C4C4425 confirmed on the phone. Settings list, one 15 s swipe, window 5-15 s:

| run | memo | swaps/s | swap -> first submit p50 / p95 ms | GPU per frame ms | busiest thread CPU ms per swap |
|---|---|---|---|---|---|
| m1 | on | 33.7 | 24.85 / 57.6 | 26.5 | 19.3, 18.5 |
| m2 | on | 34.0 | 22.6 / 60.7 | 26.6 | 17.6, 18.3 |
| m3 | on | 33.7 | 22.0 / 60.4 | 26.6 | 15.1, 18.6 |
| o1 | off | 34.4 | 22.7 / 58.1 | 26.5 | ~22, 16 (windows misaligned) |
| o2 | off | 34.5 | 21.8 / 56.7 | 26.5 | 18.7, 16.8 |
| baseline c1b2, c1c2 | - | 33.7-33.8 | 24.4-24.6 | 26.5 | 22.7-24.2 |

- Observed: memo on and off look the same, so the ~5 ms gain is LTO and the shared FormatAnswers (both in all five runs), not the table memo. supports_family 250-500 calls/s in motion (rough). PC capture pc2 (tid 6097, n 7721, kernel 254): SweetMetal2 48.1 %, QuartzCore 15.9, Turnip 12.0, malloc 9.2, memmove 5.3, kernel 3.3. No family function in the top 40. Top: draw_resident 4.4, sys_dcache_flush 2.9, settle_draw 2.6. No DeviceLost, no new garbling, no reset.
- Reading (inference): GPU 26.5 ms per frame alone gives ~38 frames/s, and GPU busy was 84-93 % on this canvas (tl1). So on 1125x2436 the GPU caps the rate and the CPU gain can't show. Correction to my "GPU is not the limit" (20:55, 21:35 entries): that held only at @2x; the two limits sit close together, so both must come down. The dropped copy and pass work is back on the list for the full canvas.
- fpsbench with --wait 0: unreliable this run (started on the lock screen, wrong screens). ml2s 24.9 on home pages. Not a reference.
- Side notes: kernel130 reboot once failed ("phone not in fastboot after 120 s", retry worked). Installer ran twice, so SweetMetal2.bundle.prev holds the new bundle: --undo can't return to 0113 (rebuildable from git).

### 2026-10-30 22:15 Bridge 0114 family memo + fat LTO built (not run)
- 0114: `supports_family` memo per device (lock-free, off with M2V_FAMILY_MEMO=0), FormatAnswers through Arc, counter `FM supports_family calls=N` in the 10 s telemetry (needs /var/local/mtl2-telemetry). A driver error on a format's first computation would stay frozen for that family.
- tools/build-metal2-linux.sh: `lto="fat"`, `codegen-units=1`; `M2V_LTO=off` while iterating. Full script: 142 s -> 365 s clean; 64 s -> 289 s after touching one file. SweetMetal2 22.5 MB -> 16.0 MB. CI's build-m2v-bridge.sh stays non-LTO. Next: same long-swipe bench and PC capture against cs1 (33.8 swaps/s, ~24 ms CPU per swap).

### 2026-10-30 22:05 pc1 named (research/notes/fps-bridge-profile.md): the bridge's cost is flat; one clear waste, the capability audit
- dSYM and out/ binary share UUID 4C4C44E7-5555-3144-A176-C9A72330C92D; phone copy not compared. 46 of 4234 PCs unnamed. Top: draw_resident 3.40 % of all samples, sys_dcache_flush 2.39, FormatAnswers::clone 2.11, format_answers 1.12, bind::graphics::instantiate 1.11, Unsupported::clone 1.10, Recording::record 0.90, pixel_format_info 0.88. Top 10 are 27 % of the bridge, top 30 are 43 %, across 705 functions.
- By group, ms per frame at 23 ms: encoder per-draw 2.95, capability audit 1.89, bind instantiate 1.38, resource tracking 1.09, pipeline lookup 1.06, descriptors 0.91, cache flush 0.55. Shader translation doesn't show as its own cost; logging isn't a cost.
- Observed: `supportsFamily:` has no memo and re-walks the family tables; samples spread over the whole capture. QuartzCore's own call sites are in new_metal_context, so the per-frame caller is something else (guess: MTLPixelFormatGetInfoForFeatureSetQuery).
- Ranked estimates: memo 2-3 ms, LTO 0.6-1.8, redfin's 0130-0137 1-2 (3-5 h), unchanged-draw fast path 1-2 (1 day), allocator 0.5-1, flush off the thread ~0.5. All together ~16-17 ms at best, not 14. Doing memo + LTO first.

### 2026-10-30 21:50 pc1: PC profile of the render thread: over half of its CPU is in our Metal bridge
- kernel130 (xnu 0249, `kern.sweet_pcsample`), `CAMERA=1 tools/sunfish-test.sh -k work/kernel130/kernel.development.sweet pc1 sweetse9=2 sweetcampmic=2 sweetcpusample=1`, boot 20261030-214521.log, no reset. Settings list, ~75 s of long swipes. `tid 596 n 8000 user 7535 kernel 465 missed 0 waited 9500 ms`, no line gaps; backboardd pid 60. Webcam: Settings on screen during the capture. Tool: tools/sunfish-pc-hist.py.

| image | samples | % |
|---|---|---|
| SweetMetal2 | 4234 | 52.9 |
| QuartzCore | 1013 | 12.7 |
| libvulkan_freedreno | 768 | 9.6 |
| libsystem_malloc | 652 | 8.2 |
| libsystem_platform | 479 | 6.0 |
| kernel | 465 | 5.8 |
| libobjc | 159 | 2.0 |
| libsystem_pthread | 126 | 1.6 |

- Top named: __platform_memmove 3.4 %, _tiny_malloc_from_free_list 1.2 %, CA::OGL::MetalContext::draw 0.9 %, CA::Render::Updater::prepare_layer0 0.8 %. SweetMetal2 pages show no names yet; spread flat, top pages 2.4 %.
- Correction to Facts ("Most backboardd CPU time is in QuartzCore; the bridge and Turnip are small"): no longer true. At ~23 ms per frame, by arithmetic SweetMetal2 is ~12 ms, Turnip ~2 ms, QuartzCore ~3 ms. Sampling skews toward the end of interrupt-masked kernel sections. So the lever is ours to fix without QuartzCore RE. Naming the bridge functions next (research/notes/fps-bridge-profile.md).

### 2026-10-30 21:35 cs1/cs2: the time before the first submit is CPU work, 22-24 ms per frame; the ~3 s stalls were the bench
- Boots with sweettrace=1 sweetcpusample=1: cs1 kernel128 (20261030-211909.log, 4454 mV), cs2 kernel129 sweetfb_2x=1 (20261030-212522.log, 4455 mV). Settings list: a = old bench, b and c = one long swipe `swipe 540 2100 540 300 15000` with no finger lift.

| run | swaps/s | swap-to-swap p50 / p95 ms | swap -> next first submit p50 / p95 ms | GPU per frame ms | gaps > 100 ms: n, ms | swaps/s without gaps | busiest thread CPU ms per swap |
|---|---|---|---|---|---|---|---|
| c1a | 28.6 | 23.8 / 58.3 | 23.5 / 56.7 | 26.3 | 6, 2997 | 33.9 | 17.1-23.5 |
| c1b2 | 32.6 | 24.8 / 58.7 | 24.4 / 58.4 | 26.5 | 1, 495 | 33.8 | 23.5 |
| c1c2 | 33.7 | 24.9 / 58.1 | 24.6 / 57.6 | 26.5 | 0 | 33.7 | 24.3 |
| c2a | 33.7 | 22.6 / 47.5 | 22.3 / 47.2 | 12.5 | 6, 2951 | 39.9 | 22.1 |
| c2b | 38.0 | 23.1 / 44.2 | 22.7 / 43.9 | 12.5 | 1, 382 | 39.1 | 22.1 |
| c2c | 37.0 | 23.1 / 49.0 | 22.8 / 48.7 | 12.5 | 1, 345 | 37.9 | 23.4 |

- Observed: one backboardd thread (tid 560 / 625, unnamed) takes 3.2-4.4 s CPU per 5 s; split A55 650-1370 ms, A76 2330-3580 ms. Its CPU per swap equals swap -> next first submit, on both canvases. Other backboardd threads 0.3-1.1 s, SpringBoard 280-421 ms; Preferences isn't listed. Swap-to-swap in ticks: 1 in 51-56 %, 2 in 27-40 %, 3+ in 5-13 %.
- So this is run, not wait: the render thread burns ~23 ms per frame. The bound to reach 60 is CPU, and older notes' 10-15 ms is out of date. @2x gains +4-5 swaps/s at steady state (33.8 -> ~38.5) from removing GPU queueing; CPU per swap is unchanged.
- Bench artefact: in tonight's bench each `flame-cmd swipe` took ~3.2 s (its default 3 s wait for results), so swipes came once per ~3.2 s and the 2-3 s gaps were idle time between flicks. Fpsbench runs on record likely understate motion too, wherever flame-cmd could bind the log port. Fixed: `--wait 0` in sunfish-fpsbench.sh, not yet run; new numbers may not compare with old ones. Long-swipe runs still show one 345-495 ms gap at +2.4-2.9 s in 3 of 4 runs, cause unknown (near when flame-cmd returned).
- No SWEET CPUFREQ line in these boot logs (printed at ~20 s, files start ~21 s). Next: sampled PCs of that thread (xnu patch being written, tools/sunfish-pc-hist.py).

### 2026-10-30 21:15 tl1/tl2: frame timing against the tick, both canvases: the wait is before the first submit
- Boots with sweettrace=1: tl1 kernel128 (20261030-205504.log, 4454 mV), tl2 kernel129 sweetfb_2x=1 (20261030-205952.log, 4455 mV). Settings vertical scroll, 10 s of full motion. Timeline tool has no clipping or tick output, so a new script read the SWEET TL records (mach clock 19.2 MHz, not 24). p50 / p95:

| metric | 1125x2436 | 750x1624 |
|---|---|---|
| swaps/s in window | 30.2 | 33.5 |
| swap-to-swap in ticks: 0 / 1 / 2 / 3 / 4+ | 4 / 56 / 26 / 8 / 5 % | 2 / 55 / 33 / 7 / 3 % |
| swap-to-swap ms | 24.2 / 56.4 | 23.1 / 51.3 |
| tick after previous swap -> first submit, ms | 17.1 / 51.0 | 15.0 / 43.1 |
| first submit -> swap call, ms | 0.27 / 0.58 | 0.31 / 0.59 |
| GPU per frame, sum of spans, ms | 26.3 / 27.1 | 12.6 / 13.2 |
| first submit -> last GPU end, ms | 59.4 / 91.1 | 12.6 / 13.2 |
| GPU end -> present call, ms | 12.8 / 46.6 | 13.9 / 40.0 |
| CPU + GPU, ms | 43.3 / 77.3 | 27.3 / 55.7 |
| frames with CPU + GPU <= 16.7 ms | 0 % | 6 % |
| median overshoot of the rest, ms | 26.6 | 11.1 |
| present within 2 ms of the first tick after GPU end | 65 % | 63 % |

- Observed: the swap fires ~0.3 ms after the first submit, before the GPU ends, so 'one serial frame' in older notes is not exact: at 1125x2436 GPU work queues up (59 ms latency, GPU busy 84-93 %) while swaps go on. Each frame has two backboardd submits. Tick records 511 and 582 per 10 s, under 600: ticks late or records lost, unknown.
- My tick-threshold guess at the @2x entry is refuted: at @2x the median frame is 11 ms over, not just above the line. Halving the GPU moved tick -> first submit only from 17.1 to 15.0 ms.
- Inference: the rate is now set by the part before the first submit (p50 ~15 ms, p95 43-51), which is CPU or a wait inside it. Investigating clocks and what that time holds (research/notes/fps-cpu.md).
- Part B of that run (B44 dump) didn't run: sizes 97x76, 92x64, 78x63 from kernel126's logs never appear tonight; lock-screen-scale shadows tonight are 86x86 row 352 off 32, 101x101, 103x103, A8 96x62 and 131x61. Bundle with 0113 installed (log only for now).

### 2026-10-30 20:55 @2x A/B (kernel129, xnu 0248, sweetfb_2x=1): GPU time more than halves, swaps/s stay: GPU is NOT the limit in Settings
- Boot: `CAMERA=1 tools/sunfish-test.sh -k work/kernel129/kernel.development.sweet fb2x sweetse9=2 sweetcampmic=2 sweetfb_2x=1`, 4456 mV, log 20261030-203804.log, MobileGestalt cache rebuilt under @2x (8013 B), then restored (8012 B). No reset, no DeviceLost, no fallback lines. "swap 1 layer 0 surface 2 750x1624 ... frames now scaled to fit by the GPU". Upscale works, full screen, touch right (Settings icon, Notes).
- Correction to the means in the 20:35 table: they include the partial first and last 5 s windows. Full-motion windows, swaps/s and backboardd GPU ms/submit, from the same runs:

| Screen | canvas | full-motion swaps/s | GPU ms/submit |
|---|---|---|---|
| Settings list | 1125x2436 | 34.6 31.8 33.0; 34.2 37.0 37.2 34.4 | 13.1-15.4 |
| Settings list, RT on | 1125x2436 | 35.8 34.8; 37.6 43.2 41.2 | 14.3-14.6 |
| Settings list | 750x1624 | 38.6 35.6 37.4; 37.2 36.0 37.2; 36.4 31.4 34.0 33.4 | 6.3-6.5 |
| Home | 1125x2436 | 25.8 29.8 26.6; 28.0 27.0 25.2 24.0 | 13.8-21.2 |
| Home, RT on | 1125x2436 | 27.6 26.2 25.8 25.8; 25.8 29.6 28.0 | 14.3-20.1 |
| Home | 750x1624 | 26.2 23.6 34.6 29.6 28.8; 30.0 28.4 28.4 | 5.1-8.1 |

- Observed: GPU ms/submit 14.4 -> 6.4 in Settings, 17 -> 6 on Home; GPU busy 65-75 -> 40 %, 36 -> 18 %. Settings swaps/s unchanged (~36); Home at most +2-3, inside the noise. By the stop rule @2x is a dead end for fps.
- Inference: the swap rate is not set by GPU cost. Left: CA CPU per frame, tick waits, or the bench itself (touch events feed the scroll; swipes have gaps while a new flame-cmd process starts, so 5 s windows hold idle time). Cheap next check: swap-to-swap histogram from sweettrace timeline and the injector's event rate.
- Visual at @2x: Home, Settings, Control Center, Notes layouts intact; text looks a little soft on the webcam. Lock screen broken: black wallpaper, a red strip over the status bar with digits "45025538", status-bar clock and Wi-Fi missing. "Sounds & Haptics" shows as "Sounds". So @2x stays opt-in and off. It still halves GPU busy, which may matter for battery (O14), not measured.
- Fresh GPU trace (gt2) failed: MESA_GPU_TRACES reached backboardd (mtl2.log) but the file stayed 0 bytes through ~6 restarts; only the first run wrote (9.4 MB at lock, 60 MB lock -> Camera; not Home, not kept). Guess: buffered output flushes only under load or first-run state. Not retried: copies vs passes matters less now the GPU isn't the limit.
- Phone restored: kernel128 everyday args, 1125x2436 read back, mtl2-env empty. /var/local/turnip-backboardd.log was deleted (recreate mode 666 for a trace). k128e booted once on the @2x plist before the restore (no `cmp` on the phone), k128f after.

### 2026-10-30 20:35 Ladder (a): per-screen table + Reduce Transparency A/B: no swap gain from no-blur; GPU is 15-20 ms/submit, not 25-30
- kernel128, no reboot, boot log 20261030-200443.log (markers h1, s1, r1, c1, H1, S3, C3...). 20 s of motion, H key before each run, 2 runs per row, swaps > 0 in all. RT toggled by taps: Settings > Accessibility > Display & Text Size > Reduce Transparency; restored off. 5-6 samples of 5 s per run, the first partial.

| Screen | RT | swaps/s | panel/s | GPU ms/submit | longest ms | GPU busy % |
|---|---|---|---|---|---|---|
| Home, horizontal swipes (one page, bounces) | off | 24.2 / 21.5 | 23.2 / 20.5 | 16.5 / 16.3 | 43 / 39 | 37 / 35 |
| Settings main list, vertical | off | 24.0 / 32.0 | 22.9 / 29.8 | 14.6 / 14.5 | 26 / 72 | 65 / 75 |
| Reminders, empty list | off | 12.4 / 13.3 | 11.7 / 12.2 | 19.8 / 20.5 | 72 / 72 | 30 / 31 |
| Control Center open/close loop | off | 12.5 / 12.3 | 11.6 / 11.9 | 20.9 / 22.6 | 79 / 83 | 32 / 32 |
| Home | on | 22.0 / 24.4 | 21.2 / 23.3 | 15.4 / 15.8 | 38.5 / 38.6 | 34 / 35 |
| Settings main list | on | 27.9 / 30.0 | 24.4 / 26.1 | 14.4 / 14.5 | 72 / 72 | 68 / 76 |
| Control Center loop | on | 12.9 / 13.3 | 12.4 / 12.3 | 20.1 / 22.0 | 52 / 52 | 32 / 35 |

- Observed: RT cut Control Center's longest submit (79-83 -> 52 ms) and nothing else; swaps/s inside run-to-run spread. No DeviceLost, no reset (B10's old GPU crash did not return). Garbled glyphs as before: lock-screen buttons, Control Center toggles, Accessibility icons (B29/B44). With RT on, a Control Center mid-open frame showed no backdrop, drawn over the home icons (mid-animation or a bug: unknown; one frame).
- Guess: Reminders' 12 is limited by how little an empty list redraws, not by cost. At ~23 swaps/s and 16 ms/submit the GPU accounts for ~37 % of the frame on Home: the other ~27 ms is CPU + tick waits, so GPU savings alone may buy less than research/notes/fps-ladder-bc.md's guess. Settings reaches 32 swaps/s with GPU 75 % busy, so its limit may be different from Home's.
- Scoping (research/notes/fps-ladder-bc.md, the Oct 24 trace gt1, home swipes): 1125x2436 passes are 98.2 % of render-pass time, no compute span, copy_buffer_to_image 3487 ms against 7708 ms of passes. So the "blur passes" in older notes are the composited full-screen pass; half-res blur is dropped. To confirm on a fresh trace.
- New patch: xnu 0248 `sweetfb_2x` + `mkadt --2x` (kernel129): opt-in 750x1624 canvas at scale 2, upscaled in the present blit. Built (fcbf44e), not yet booted.

### 2026-10-30 11:30 es1: CA_EMPTY_SHADERS=1 in backboardd's launchd env: no change, and the UI looked normal; smoothness PARKED
- launchd.plist com.apple.backboardd EnvironmentVariables CA_EMPTY_SHADERS=1 (backup .pre-emptyshaders, restored after): 22.4 / 24.8 swaps/s vs restored baseline 22.1 / 22.1; GPU busy 20-49 %, bb GPU 1.0-2.1 s per 5 s. The home screen looked normal, so CA probably ignores the variable in this build: the bound is still unmeasured.
- Today's ios-redfin line: DEADFILL +0.5, UI threads on the A76s +0.5, CA blur keys 0. GPU busy stays ~20-45 %: the limit is the serial CPU->GPU frame with one frame in flight (research/notes/fps-limit.md), not GPU throughput. A real fix (two frames in flight / CA pipelining) is days of QuartzCore RE: parked (backlog O7), after ~3 h today.

### 2026-10-30 09:45 hp5: CA blur defaults change nothing; the CA_EMPTY_SHADERS bound did not apply
- kernel116, sunfish-m2env-bench.sh, H before each run: base 21.9 / 23.4 swaps/s; CADisableAutoSeparateBackdropLayers=1 21.5 / 21.8; CALinearBlurs=1 23.0; CALinearBlurs=0 24.6 / 23.6; final base 22.5 (all within the 21-25 noise; keys set in com.apple.coreanimation as mobile, removed after).
- CA_EMPTY_SHADERS=1 via /var/local/mtl2-env: 24.0, but the home screen looked normal: mtl2-env is read by our Metal frontend, not put into backboardd's process environment, so the bound was never measured. Next: set it in backboardd's launchd job EnvironmentVariables for one boot (backup), or skip.

### 2026-10-30 09:15 ub2: xnu 0238 v2 (UI threads on the A76s) works but gains ~0.5 swaps/s
- kernel115 sweetblnew=0 (HAPTICS=1 CAMERA=1), fpsbench BENCH_TIGHT=1 30 s, H before each run, live `sysq kern.sweet_ui_bigcore N`:
  off 22.5 / 22.4 / 23.3 (mean 22.73 swaps/s, 21.37 panel/s, GPU 32.1 %); display only (1) 22.9 / 23.3 / 23.2 (23.13, 22.13, 34.7 %); display + render-server (3) 23.1 / 22.9 / 23.9 (23.30, 22.23, 33.9 %). Placements per run: mode 1 ~2100 idle + ~400 preempt, mode 3 ~5700 + ~1050. No reset.
- So the A55 render thread was not the limit (redfin's gain does not carry over, like DEADFILL). Left at 1 (default, harmless). The cost is the GPU blur/backdrop passes and the one-frame-in-flight serial CPU+GPU frame (research/notes/fps-limit.md): next target.

### 2026-10-30 08:30 k115: kernel115 (xnu 0238 v2, no printf under the sched lock) runs without a reset; placements happen
- Two boots (sweetuibig=0, then default), 3+ min each, no reset (kernel114 = v1 reset at 28 s). sysq: kern.sweet_ui_bigcore 1, kern.sweet_ui_idle 1342, kern.sweet_ui_preempt 2880, kern.sweet_ui_first "tid 1024 cpu 7 preempt".
- fps A/B void: the panel was off (auto-lock), 0 swaps in the bench windows. Redo with H before each run.

### 2026-10-30 08:15 ub1: kernel114 (xnu 0238, UI threads on the A76s) RESETS ~28 s after boot
- Boot `CAMERA=1 ... kernel114 sweetse9=2 sweetcampmic=2` (4455 mV): SSH up at 17 s; "SWEET UI: tagged thread 599 com.apple.coreanimation.render-server (0238 class 2, kern.sweet_ui_bigcore 1)" [25.639], "tagged thread 616 com.apple.coreanimation.display.primary (class 1)" [25.737], "SWEET UIWDT: armed, 30 s" [25.815]; last line "[27.901] exec duetexpertd"; no panic text; the phone came back in fastboot. "bigcore first use" never printed.
- Leading suspect: 0238 prints that first-use line with printf() from inside choose_processor() (pset locked, interrupts off): a printf there can deadlock the scheduler. Second suspect: returning a running big core for a real-time CA thread outside the normal RT path.
- Smaller step: kernel115 = 0238 without any printf under the lock (counters + a sysctl instead), and a boot-arg A/B starting with sweetuibig=0 (tag only, no placement change).

### 2026-10-30 08:00 df1: ios-redfin's SWEET_DEADFILL (b0491b0) finds almost nothing to drop here; no fps gain
- tools/sunfish-m2env-bench.sh, 3 rounds each, kernel113: swaps/s base 21.8 / 22.9 / 21.9 (mean 22.2), SWEET_DEADFILL=1 22.1 / 23.6 / 22.4 (22.7), =2 21.7 / 24.2 / 22.2 (22.7); GPU busy % samples overlap (noise). Settings with mode 2 drawn correctly (webcam). No crash, no reset.
- Telemetry (needs /var/local/mtl2-telemetry, every 10 s into /var/local/mtl2.log): "DF mode2 dropped 12 cleared 16 held 720" after ~720 frames (idle + 2 swipes), i.e. CoreAnimation here rarely paints full-target fills that are later overwritten; redfin's 13.7 -> 7.9 ms win does not carry over. Idle commit-to-complete mean 6.1 ms.
- Our cost is elsewhere (research/notes/fps-limit.md: blur/backdrop passes 35-58 ms in swipes, render thread 10-15 ms CPU often on an A55, one frame in flight). Next from the redfin line: xnu 1041 UI threads on the A76s (port in progress). DEADFILL stays in the build, default off; /var/local/mtl2-env left empty.

### 2026-10-30 02:55 auto flip (kernel104): garbled once, clean once; zero-copy parked
- zc6 (sweetfb_zerocopy=2): "auto flip on" at 32.9 s, 357 flips, 0 skipped, but every frame garbled (vertical stripes on the wallpaper, apps unreadable). No reset.
- zc7 (same boot args): auto flip at 34.4 s; lock screen clean (only the B29 lock buttons striped). Registers equal the working manual setup: QS OP_MODE 0x20034401, steps 0x215555/0x215015, SSPP SRC_SIZE 0x09840465, OUT 0x09240438, ystride 0x1200, REQ_PIXELS 0x09840465, SRC0 0x9cb00000; re-entering mode 2 only changed SRC0. zc6's garbage not explained (apps were open there; zc7 only looked at the lock screen).
- Parked (the user's rule: usable before fast): zero-copy gives no fps gain (A/B), manual flip works (zc4), auto flip is flaky. Default stays off. To resume: reproduce zc6 with apps open on kernel104 and dump the regs while garbled; then a heavy-app A/B where the freed GPU time could matter. The fps limit itself is O7.

### 2026-10-30 02:35 Where the ~21 fps comes from (read-only; research/notes/fps-limit.md)
- Facts (5 s "SWEET GPU" lines in the zc5 swipe windows): backboardd GPU per submit 25-30 ms average, longest 55-66 ms, GPU busy only 35-49 %, in both modes. Present blit adds 2.5 ms per present. Our code has no ~20 Hz pacing (60 Hz tick SweetFramebuffer.cpp:104/405-442; SwapWait returns at once outside flip mode).
- Inference: CA renders on a tick (10-15 ms CPU), submits, the GPU takes 25-30 ms (blur/backdrop passes up to 66 ms), one frame in flight, swap on the next tick -> 2-3 ticks per frame = ~21/s. CPU and GPU never overlap.
- Cheapest next checks: flip mode + sweettrace=1 timeline; a no-blur screen bench; pin backboardd's render thread to the A76 cores. The big lever (2 frames in flight) needs QuartzCore RE.

### 2026-10-30 02:20 A/B: flip mode saves the GPU copy but not frames (zc5a-d; BENCH_TIGHT=1 fpsbench 30 s)
| run | mode | fps lines | avg fps | other |
|---|---|---|---|---|
| zc5a | flip (k103) | 8 | 20.2 | ~21 flips/s, 0 panel copies, skipped 2 |
| zc5b | blit (k98) | 8 | 20.6 | avg copy ~7200 us (600-13000) |
| zc5c | flip (k103) | 7 | 21.5 | ~25 flips/s, skipped 1 |
| zc5d | blit (k98) | 7 | 22.5 | avg copy ~9200 us (2500-14200) |
- Observation: same ~21 fps in both modes; flip mode removes 7-9 ms of GPU copy per frame. So the ~21 fps limit is upstream (backboardd/CA render or GPU submission), not the panel copy. No reset; still frames sharp in both modes.
- Guess (untested): the GPU time freed may matter under app load (render + copy share the GPU); a heavy-app bench would show it.

### 2026-10-30 02:00 xnu 0225 auto flip mode built (kernel104 = 103 + 0225; untested)
- `sweetfb_zerocopy=2`: when both slots are owned, flip mode turns on by itself (retry from swap() once per second if the MDP isn't mapped yet); frames are dropped (not blitted over slot0) until then; a freed slot surface leaves flip mode and the next pair re-enters. `=1` unchanged.

### 2026-10-30 01:40 E3c FLIP MODE WORKS (zc3, zc4; kernel103 = 102 + 0224)
- zc3: with sweetfb_zerocopy=1 and mode 0, opening Calculator froze the screen on stripes (backboardd renders into slot0 = the old fb; "5s: 72 swaps, avg copy 16041 us"); `sctl kern.sweet_zc_scan=0` (forced redraw) recovered it. Expected from the slot0 overlap: mode 0 must not be used once slots are owned.
- zc4: slots at 33.9/34.1 s (slot1 id 2, slot0 id 3); `kern.sweet_zc_scan=2` right after -> lock screen, home, Calculator, Settings all normal; 5 taps -> "22,222" live; skipped 0, waits_to 0 throughout. Rates: idle 0-0.2 flips/s, taps ~22/s, Settings scroll load ~27 flips/s (641 in 24 s). One mid-scroll frame showed tearing/ghosting (e2; webcam blur vs slot mixing unknown). `=0` afterwards: clean (redraw worked). No reset, 4439 mV.
- Next: A/B with tools/sunfish-fpsbench.sh (kernel98 vs kernel103 mode 2, same load); enter flip mode automatically when both slots are owned (no mode-0 window); check the tearing (SwapWait/flush latch timing).

### 2026-10-30 01:15 xnu 0224 zero-copy E3c built (kernel103 = 102 + 0224; untested)
- Two slots (slot1 0x9cb00000 first, slot0 0x9c000000 over the old fb) for backboardd's two 1125x2436 surfaces. `kern.sweet_zc_scan=2` = flip mode (EBUSY unless both slots owned): swap() decodes the SwapEnd record (mask rec+0x104, id rec+0x54+4*layer), writes VIG0 SRC0 + CTL0 flush and skips present; frames without a slot are dropped (logged). SwapWait polls the flush bits (1 ms, cap 33 ms). `=0` reverts and forces a redraw. `kern.sweet_zc_stats` string.
- Open risks: with presents stopped, swap-complete/vsync pacing may throttle CA; leaving flip mode may show a wrong frame (slot0 = fb).

### 2026-10-30 00:55 E3b WORKS: VIG0 scans backboardd's own surface via the scaler (zc2, kernel102)
- `sweetfb_zerocopy=1`: "sweet zc: slot1 surface id 2 1125x2436 bpr 4608 size 0xab8000" at 32.8 s (backboardd's 1st display surface; the 2nd stays in normal memory).
- `sctl kern.sweet_zc_scan=1` -> "zc scan 1 src 0x9cb00000 bpr 4608": Calculator unchanged, sharp, full screen, not frozen; 10 s later same; a `T` tap changed "5" to "2" live while scanning. `=0` -> "src 0x9c000000 bpr 4352", normal. No reset, no underflow (full 1125x2436 downscale fetch is fine). 4455 mV.
- Caveat: only one of backboardd's two buffers is in the slot, so while scanning, every other frame may be stale; not visible by eye on the webcam.
- Next E3c: both display surfaces in slots (slot0 at carveout offset 0 over the old fb), swap() flips SRC0_ADDR to the presented surface, present() skipped while zero-copy is on; then SwapWait + fpsbench.

### 2026-10-30 00:40 zc1: no slot surface (kernel101); slot limit raised (kernel102)
- zc1 (kernel101, sweetfb_zerocopy=1): boot normal, no reset. backboardd creates two 1125x2436 BGRA surfaces at 32.5/32.7 s with bpr 4608, planes 0, size 0xab8000 (> the 0xA90000 slot limit), so no "slot1 surface" line; sweet_zc_scan stayed 0, step 4 not run. SpringBoard also makes one 1125x2436 at 43.9 s.
- 0223 revised: SWEET_ZC_SLOT_LEN 0xB00000 (slot 0x9cb00000..0x9d600000, inside the 0x1700000 carveout). kernel102 built (53 s).

### 2026-10-30 00:10 xnu 0223 zero-copy E3a/E3b built (kernel101 = 98 + 0222 + 0223; untested)
- E3a (boot-arg `sweetfb_zerocopy=1`): backboardd's first 1125x2436 BGRA surface (allocSize ≤ 0xA90000) is backed by the carveout slot PA 0x9cb00000 (withPhysicalAddress, write-combined, bzero_phys); logs "sweet zc: create ..." for every create with width ≥ 1000 (cap 20) and "sweet zc: slot1 surface id ...".
- E3b: `kern.sweet_zc_scan` 1 = VIG0 scans the slot with the full 1125x2436 -> 1080x2340 downscale (steps 0x215555/0x215015), 0 = ABL values. EPERM without the boot-arg, EBUSY without a slot surface. Registered late (~10 s), skipped with sweetmdpdump=0.
- Risk: the full-size fetch can underflow the MDP (reset). Design: research/notes/zero-copy-e3.md.

### 2026-10-29 23:00 E1 WORKS: QSEED3LITE scales VIG0 (e1a, e1b; kernel98, pmem from userland)
- Recipe from the Pixel 4/4a msm-4.14 SDE source: research/notes/qseed3lite.md; scripts research/notes/e1-on.sh / e1-off.sh (pmem writes, one CTL0 flush 0x20001).
- e1a read-only dump (ABL state): QS+0 = 0x2004 is the scaler VERSION (not "unused"); QS OP_MODE 0, steps 0x200000, preload 0x03040304; SSPP SRC_SIZE/OUT_SIZE 0x09240438, SRC0 0x9c000000, ystride 0x1100, fmt 0x237ff; REQ_PIXELS (+0x108/118/128) 0x09240438; CTL0 flush 0.
- e1b: crop 1037x2246 at (21,47), steps 0x1eb9d6/0x1eb6eb, OP_MODE written 0xa0034401 (reads 0x20034401: bit 31 not kept) -> image zoomed ~4%, edges cut, no garbage/stripes, stable 13 s; e1-off -> baseline. No reset, no MDP/underrun log lines.
- So a 1125x2436 surface can be scanned and scaled to the panel by hardware. Next: E3 (display surfaces in 2 carveout slots below 4 GB, swap() flips SRC0_ADDR, SwapWait), first the full-size 1125x2436 downscale check (more fetch per line: underflow risk).

### 2026-10-29 16:00 E2: no contiguous 9.7 MB below 4 GB (e2b, kernel100 = 98 + revised 0222)
- The 15:35 panic was `ml_io_map` of 10 MB (io_map submap is 8 MB, KMA_NOFAIL), not the contiguous allocation. Revised 0222: early non-panicking kmem_alloc_contig, copy via ml_phys_read.
- e2b: `e2 alloc below 4 GB failed (3)`, `e2 alloc any PA ok, PA 0x1101dc000`; write 2 -> ERANGE (VIG0 SRC0_ADDR is 32-bit), VIG0 untouched; no hang. So display buffers must be below 4 GB: either reserved at boot (shim/ADT reserved-memory like the splash carveout) or carved from the splash carveout (24.1 MB: 2 x 1125x2436 frames). E2's DRAM-outside-carveout question stays open (only answerable with a buffer < 4 GB).

### 2026-10-29 15:35 RESET: E2 allocation panics (e2a, kernel99 = 98 + xnu 0222)
- `sctl kern.sweet_mdp_e2=1` -> `panic(...): kmem_alloc(..., 10190848, 0x100021): failed with 3 @vm_kern.c:178` (kmem_alloc_contig of 9.7 MB below 4 GB with KMA_PERMANENT failed; the call panics instead of returning). No e2 log lines; phone to fastboot; rescued on kernel98 (e2r, ssh 18 s, 4439 mV). sctl 64-bit retry (11522a2) worked up to the write.
- No verdict on E2. Next: allocate the contiguous buffer early (at registration, when memory is not fragmented), with a non-panicking call, or reserve it at boot; fail with ENOMEM.

### 2026-10-29 15:15 zero-copy design (research/notes/zero-copy-design.md)
- Chosen: backboardd keeps 1125x2436; its display IOSurfaces allocated physically contiguous (SweetSurface.cpp:759, fallback normal); swap() (SweetFramebuffer.cpp:868) points VIG0 at the swapped surface (SRC_SIZE 1125x2436, OUT 1080x2340, QSEED3LITE scaling) + CTL0 flush; real SwapWait polls the flush bits (1 ms, 33 ms cap). Rejected: 1080x2340 canvas (360x780 pt: red rdar status bar, would need an iPhone 13 mini identity), carveout slots (2 x 11.1 MB fill the 24.1 MB carveout). Rounded-corner mattes lost at first.
- Experiments: E2 (MDP reads contiguous memory outside the carveout? SMMU stream 0x800) first, E1 (QSEED3LITE scale test), E3 (flip + SwapWait behind sweetfb_zerocopy, fpsbench).

### 2026-10-29 15:00 zero-copy step A: VIG0 source switch works (zc1, kernel98 = 97 + xnu 0221)
- 0221: `kern.sweet_mdp_src` (quad RW): write = VIG0 SRC0_ADDR (validated inside the splash carveout) + CTL0 flush 0x20001 (VIG0 | CTL bit 17, as ios-s9). sctl now writes hex / 64-bit values.
- zc1: src 0x9c000000 -> 0x9c06a400 (+100 rows): picture moved toward the top, garbage strip at the bottom (webcam zc1-b); back to 0x9c000000: identical to the baseline (zc1-c). Log: `VIG0 src 0x9c06a400 (flush 0x20001)`, `after 20 ms ... flush 0` (latched within 20 ms). No reset; ssh 17 s; 4438 mV.
- Next: the design for removing the blit: either backboardd renders 1080x2340 directly into carveout slots (canvas change) or 1125x2436 slots + QSEED3LITE scaling on VIG0; a real SwapWait.

### 2026-10-29 14:40 MDP dump read (md2, kernel97 = 95 + revised 0220; sctl buffer 256 B -> 16 KB)
- 0220 revised: nothing mapped at registration; the first read maps 11 single 4 KB MDSS pages (never 0x0ae44000, which SweetFramebuffer notes hangs the bus). kernel97: 90 s untouched, no hang; reads work. (kernel96's hang: whole-range mapping covering 0x44000 is the best guess, not proven.) `SWEET USB: busERR ... SMMU cb fsr 400` repeats every ~5.7 s on kernel95 too (12 per boot): unrelated.
- Layout ABL leaves: only VIG0 active: src_size = out_size 0x09240438 (1080x2340, no scaling), addr0 0x9c000000, ystride0 0x1100 (4352 = 1088 px x 4 B), fmt 0x000237ff, op 0x80000000; qseed words 00002004 00000000 0013b736 ... (scaler not used). CTL0 layer0 0x01000005 (VIG0 -> LM0), top 0x00020000, flush 0. LM0 out 0x09240438. INTF1 autorefresh 0x80000001. VIG1/DMA0-2 off.
- So zero-copy = VIG0 SRC0_ADDR + CTL0 flush. Baby step A next: a write sysctl, point VIG0 at 0x9c000000 + 100 rows (inside the splash carveout), webcam, restore.

### 2026-10-29 14:15 RESET: kernel96 (xnu 0220 kern.sweet_mdpdump) hangs ~60 s after boot (md1)
- kernel96 = kernel95 + 0220 (read-only MDP/SMMU dump sysctl, maps MDSS 0x0ae00000+0x84208 and apps SMMU 0x15000000+1 MB at registration, 10 s after SweetBattery start). "SWEET MDP: kern.sweet_mdpdump ready" at 30.0 s; the dump was never read. Webcam: black with white vertical streaks; log ends at 59.9 s with `SWEET USB: busERR addr 0; SMMU cb fsr 400 far 000144d675929c97 fsynr0 220c2d fsynr1 74`; phone in fastboot (4455 -> 4438 mV). ssh up 21 s before that.
- Cause unknown (the sysctl was not read; mapping alone should not touch hardware). Smaller step: map lazily on the first read, MDSS only (SMMU part dropped). kernel95 stays the default.

### 2026-10-29 13:45 zero-copy plan (research/notes/zero-copy-plan.md)
- SM7150 MDSS (0x0ae00000) SSPP/CTL/LM offsets match SDM845 (ios-s9 0096 carries over); scaler is QSEED3LITE (VIG+0xa00), not QSEED3. MDP stream 0x800/0x440 on the apps SMMU; splash carveout 0x9c000000+0x1700000. Our surfaces are page-granular (not contiguous).
- Steps: 0 read-only `kern.sweet_mdpdump` sysctl (pipes, CTL, LM, INTF, PP, SMMU S2CR for 0x800); 1 contiguous slots if the stream is in bypass; 2 VIG0 src = a copy of the framebuffer (webcam); 3 1125x2436 slot + scaler; 4 flip = SRC0_ADDR + CTL flush; 5 real SwapWait. Estimate 21 -> 40-60 swaps/s (guess).

### 2026-10-29 11:25 ios-s9 (Galaxy S9 port, MIT) display work vs ours (research/notes/ios-s9-display.md)
- Already ours: panel autorefresh on every TE (0x80000001), CPU frequencies, UI threads on big cores.
- Not ours: zero-copy scanout (their 0096/0097): an MDP VIG pipe reads backboardd's surface directly with the QSEED3 scaler, CTL flush latches at frame start, SwapWait (sel 6) returns once the swap is on screen. Ours: a GPU blit into the bootloader framebuffer (0x9c000000) per vsync, ~2.4 ms GPU but 10-17 ms wall queued behind backboardd on the single ring (= O2); our SwapWait is a no-op (SweetFramebuffer.cpp:1303).
- Plan: read the MDP pipe layout ABL leaves (read-only), then point a pipe at backboardd's 1125x2436 surface and scale/crop, latch via CTL, real SwapWait. 1-2 days; MDP writes risk a reset only. Estimate (unmeasured): 35-55 panel updates/s. Second: @2x canvas A/B (small gain, our cost is per draw).

### 2026-10-28 21:55 present mode 2 (xnu 0213, retry a put-off present from a 1 ms timer within 10 ms) is worse
- kernel94, fresh boots, `BENCH_TIGHT=1 sunfish-fpsbench.sh TAG 30`: sweetpresentwait=1: pw1a 23.0 swaps/s, 21.9 panel/s; pw1b 22.5 / 21.1. sweetpresentwait=2: pw2a 22.7 / 16.0; pw2b 20.7 / 14.7. Mode 1 (blocking present) stays the default; mode 2 drops ~6 panel updates/s (retries land late or miss the next tick).

### 2026-10-28 15:30 presentwait A/B on kernel83 (3 fresh boots each)
- off: kpw0a/b/c 22.3/16.1, 22.3/14.9, 20.2/14.5 (mean 15.2 panel/s); on: kpw1a/b/c 23.6/21.9, 22.4/21.6, 21.5/20.3 (mean 21.3). Confirms O1.

### 2026-10-28 17:45 the drop explained: sweetpresentwait=1 was not a default
- kernel83 fresh boots: kpw0c (no wait) 657 swaps / 459 panel copies per 30 s (70%), avg copy 1.2-1.9 ms, 383 of 600 ticks put off (sweet_gpu.c:1010-1020: the present's IB slot from two presents back is still queued behind backboardd on the single ring; the put-off frame is retried only on the next tick, and the next swap often replaces it). kpw1c (wait) 538 / 508 (94%), copy 5.3-15.6 ms wall vs ~2.5 ms GPU, 0 put off: ~20.3 vs ~15.3 panel updates/s. backboardd jobs 13-18 ms GPU each, longest 41 ms, GPU 33-54% busy: FIFO latency, not throughput. sweetpresentwait=1 is now a default.

### 2026-10-28 15:45 /var on root vs ufsroot1: the same; sweetpresentwait is not a default
- kernel68: /var on the root (sweetnofstab=1) knfa/b/c 21.8/14.6, 21.0/14.4, 20.7/14.4; /var on ufsroot1 kfsa/b 22.7/16.4, 21.6/15.0. Not the cause.
- So far ruled out: the Linux bundle build, kernel68 vs 81, dim 10 vs 50, /var location. But tools/sunfish-boot.sh's default args do NOT include `sweetpresentwait=1`, which the best measured config used ("kernel37 + sweetpresentwait=1"; the old status.md boot line too). Today's benches all ran without it. A/B on kernel83 next.

### 2026-10-28 15:20 kernel68 vs kernel81, fresh boots: the same
- kab68a/b/c 21.8/14.6, 22.3/14.3, 22.2/15.7; kab81a/b 21.7/14.8, 22.0/14.3 (swaps/s, panel/s). No kernel patch caused the drop. kernel68 is also when /var moved to the ufsroot1 HFS volume (fstab): next, kernel68 with sweetnofstab=1 (/var back on the root).

### 2026-10-28 15:20 dim 10% vs 50% on one boot (k81r): no difference
- d10a 22.4 / 14.6, d50a 19.3 / 13.9, d10b 22.2 / 14.0, d50b 20.8 / 13.9 (swaps/s / panel updates/s). The LM_0 alpha is not the cause. Next: kernel68 vs kernel81 fresh boots.

### 2026-10-28 14:50 CI vs local bundle, fresh boots on kernel79: no difference
| tag | bundle | swaps/s | panel/s |
|---|---|---|---|
| abci1 / abci2 / abci3 | CI (work/metal2-0106) | 20.7 / 19.8 / 22.4 | 14.6 / 12.4 / 14.4 |
| ablo1 / ablo2 / ablo3 | local (Linux build) | 19.9 / 21.5 / 21.9 | 13.8 / 15.5 / 15.6 |
- Means: CI 13.8, local 15.0 panel updates/s. The Linux build is not the cause. Both are well below the 21-24 of 10-24..10-27. Next suspect: those benches ran at full brightness; since the overnight run the dim tool keeps LM_0's constant alpha at 10% (a blended mixer stage). A/B dim 10 vs 50 on one boot next; then kernel68 vs kernel79.

### 2026-10-28 12:00 fresh-boot panel updates 15.6-16.2/s (sk70p) vs 21-24 before
- Build comparison (read-only): the Linux build (tools/build-metal2-linux.sh) and CI use the same flags: meson buildtype release, optimization 3, b_ndebug true; cargo --release; frontend -O2 -g; strip -x. Only the SDK differs (16.5 vs 17.5). Turnip 0106/0107 and xnu 0196 cost ~0 when off.
- The pool cap (bridge 0105) is not it either: on one boot cap 0 gave 17.1, cap 8 18.5, cap 32 18.85.
- Next (after the soak): a fresh boot with CI's bundle work/metal2-0106 vs the local bundle, two benches each; then kernel68 vs kernel70.

### 2026-10-27 experiments.md lines
- (see [gpu](gpu.md), experiments line for 2026-10-27) 2026-10-27 es1: kernel58 (0182 SMMU stall-on-fault by default) injected SMMU-stall hang: early resume fsr 40000402, recovered first path in 26.4 ms, sctlr e7 after reset (stall bit kept), 21.5 swaps/s at +60-120 s with swipes (so er4-er7's 6-8/s was the idle lock screen).

### 2026-10-25 experiments.md lines
- (see [gpu](gpu.md), experiments line for 2026-10-25) 2026-10-25 xnu 0174 (kernel50): Linux-style Fabia PLL0 restore after the DCVS vote locks; core 564 MHz after recovery (was 9-56). GMU DCVS is what zeroes PLL0 L. Post benches 9 swaps/s on a widget panel (not comparable); backboardd 20 vs 13 ms/submit.

Older entries (before 2026-10-25): [archive/features/smoothness.md](../archive/features/smoothness.md).

### 2026-10-24 22:30 GPU busy vs wait, Reduce Transparency, DDR vote (kernel42, sweetpresentwait=1 sweettrace=1)
- **Busy vs waiting (existing timeline, ctlA/ctlB/cs7/cs8):** no other span overlaps any backboardd CP span (0 of ~620 per bench), so the 13-40 ms is that submit's own work, not interleaving. Long submits are tightly clustered: ~11.5 ms (10-20 bucket) and 38.6 ms p50 / 40.4 p95 (>=20 bucket); they queue 61-74 ms (p50) before CP start. GPU busy 25-37%; only 39-90 of ~620 spans start back-to-back. The trace cannot tell busy from stalled inside a span; the near-constant 38.6 ms is suspicious (fixed cost or a wait quantum). Needs RBBM busy perfcounter or a CP timestamp per render pass.
- **Bus vote:** `tools/sunfish-boot.sh` passes `sweetddr=0` by default (set 10-23 when the DDR vote cost panel updates on the old present path), so the GPU trail had no "bus vote" lines. With `sweetddr=1`: "bus vote MC0 top -> 0", "bus vote SH0 top -> 0".
- **Baseline (rtbase1/2):** 21.0 / 21.4 swaps/s, 20.1 / 20.3 panel; backboardd ~16.5 / ~17.0 ms/submit, longest 41.9 / 40.7 ms; timeline <=17 ms 19.8 / 23.8%, <=34 ms 62.5 / 64.3%; bb p50/p95 5.8-5.9 / 39.9 ms; GPU end -> swap p50 27.8 / 29.3 ms.
- **Reduce Transparency** (`EnhancedBackgroundContrastEnabled=1` in com.apple.Accessibility, backboardd restarted): dock turned opaque grey, but backboardd logged "vertexFunction is not a MTLFunction" / "fragmentFunction is not a MTLFunction" (CA's RT shaders do not build through the bridge), then ~3 s later "present: GPU hang or fault (fence 4375, done 4367)" and recovery failed ("ring me_init ... TIMEOUT", "GBIF halt + soft reset + CP init -> -1"). rton1/rton2 (24.6 / 23.6 panel) are invalid (GPU dead, no GPU stats). Pref removed (cfprefsd had cached it; killed cfprefsd, deleted key), fresh boot rt-clean healthy: 19.1 swaps/s, 18.1 panel, ~15-17 ms/submit. Blur cost NOT measured this way.
- **sweetddr=1 (ddr1a/ddr1b):** 17.9 / 17.9 swaps/s, 17.3 / 16.9 panel; backboardd 13.2 / 13.0 ms/submit (more submits, mean drops) but p50/p95 unchanged 5.7 / 40.0 ms, longest 40.8 / 40.9 ms; <=17 ms 21.1 / 22.3%, <=34 ms 62.2 / 63.1%; blit 2.37 ms; GPU end -> swap 29.6 / 29.7 ms. The 40 ms submits do not move with DDR bandwidth -> not bandwidth-bound; panel rate ~3/s lower. Keep `sweetddr=0`.
- Battery 4432-4456 mV throughout. Phone left booted on ddr1 (sweetddr=1); next boot is back to default.

### 2026-10-24 21:55 Completion-path stamps (metal2 SWEET_CTL + timeline --ctl), fresh boot kernel42, sweetpresentwait=1 sweettrace=1
- ctlA / ctlB: 19.0 / 19.5 swaps/s (18.1 / 18.7 panel). 1607 CTL records for backboardd (pid 58).
| hop (per swap) | p50 | p95 | max |
|---|---|---|---|
| GPU end -> bridge completed() | 0.18-0.19 ms | 1.6-2.6 ms | 6.8-8.8 ms |
| completed() -> -finish | 0.00 | 0.01 | 0.02 |
| flushSurfaces | 0.65 | 1.55-1.68 | 2.4 |
| -finish -> CA completion queue block | 0.09 | 0.24-0.27 | 0.41 |
| CA completed handlers | 0.00 | 0.02 | 0.09 |
| handlers done -> swap | 28.2-28.5 ms | 93.9 ms | 96-100 ms |
| commit -> GPU start | 0.59 ms | 102-105 ms | 150-162 ms |
- The bridge and kernel completion path costs <1 ms. All of the 29 ms is after CA knows the frame is done: CA waits for its next usable vsync tick. With render on a tick (~6 ms CPU) plus 13-40 ms of GPU, the previous frame finishes after the following tick, so CA renders every 2nd-3rd tick (~20/s). Lever: per-frame GPU time under ~10 ms (step 2), or a deeper CA pipeline.
- mtl2-env cleared afterwards.

### 2026-10-24 21:30 xnu 0166 CPU sampler works (kernel42, clean CI run 40150300651)
- Earlier boots tested nothing: the patch re-added osfmk/kern/thread.c as a new file (untracked in work/src/xnu), dxb skipped it ("already exists"), and my check grepped "SWEET CPU:", which SweetCPU.cpp also prints. Fixed: helper in osfmk/arm64/cpu.c, sampler driven from sweet_gpu.c's 5 s stats tick, check for "sampler started". New files need -f clean=true.
- Fresh boot, sweetpresentwait=1 sweettrace=1 sweetcpusample=1, swipe benches cs7/cs8: 18.9 / 19.3 swaps/s (18.0 / 18.6 panel).
- backboardd tid 576 (the IOMFB vsync/render thread): 0.66-1.85 s CPU per 5 s window during swipes, ~10-15 ms per frame at 19-25 fps; 55-75% of it already on the A76s. Next threads: tid 934 0.17-0.43 s, tid 544 / 931 ~0.1 s. SpringBoard tid 256 (main) 0.22-0.48 s per 5 s.
- Per-CPU busy% during swipes: c0 7-18, c1-c5 0-12, c6/c7 8-24. The CPUs are mostly idle; the GPU is ~30% busy.
- Reading: neither CPU nor GPU is saturated and the render thread needs only ~10-15 ms per frame, so the ~29 ms GPU end -> swap gap and the ~20 fps are waiting/pacing, not work. A big-core scheduler would save at most a few ms per frame. Open: the completion path (bridge handler thread + dispatch hop to CA) is the one untested hop.
- Caution: later benches (ctc1/ctc2/notel1/notel2, 7-10 swaps/s) ended on the widgets page (blurred widget materials): the swipe position drifted, so those runs and the mtl2 telemetry ctc numbers are not comparable. Reboot before benching.

### 2026-10-24 20:30 Step 3: GPU end -> swap gap (29 ms)
- Path traced (investigator): kernel fence wait is a 20 us busy loop (Turnip waits in 1 ms slices), metal2vulkan's per-queue waiter uses vkWaitSemaphores; because our bridge always registers a completed handler, finishing goes through the runtime's handler thread plus a dispatch_async to CA's completion queue. IOMFB SwapEnd itself doesn't block (SwapWait sel 6 is a no-op).
- From the tl1 trace (no new code): swaps land 4-8 ms after a vsync notification (p10 3.6, p50 6.5, p90 17.6 ms; 768 swaps). So CA swaps on the tick after the GPU finishes, and at ~20 swaps/s it starts a new frame only every ~3rd tick: looks like one frame in flight.
- Our IOMFB only ever sends notification type 5 (vsync); no swap-complete / buffer-release notifications. backboardd calls sel 72 (Enable/DisableNotifications) very often. Next: find in QuartzCore what gates the next frame (research/re/QuartzCore.s, investigator running).
- [21:40] QuartzCore RE (investigator): CA subscribes only to vsync (type 5) for rendering; no swap-complete/buffer events are used; 2 swap slots freed via SwapWait (ours returns 0 at once, not a gate). Hypothesis: vsync_callback skips ticks earlier than a "vbl threshold" = end of last render + one period, capping ~30/s.
- xnu 0165 (kernel41, CI 40144904620): sweetvsyncahead=US reports each tick's time US later. Fresh-boot matrix, sweetpresentwait=1 sweettrace=1, 2 boots each:
| sweetvsyncahead | swaps/s (bench) | panel/s | swap-to-swap p50 / p95 | <=17 ms | GPU end -> swap p50 |
|---|---|---|---|---|---|
| 0 | 20.5 / 18.7 | 19.5 / 18.3 | 33.2 / 100.5 ms | 23 / 19% | 29.3 / 29.1 ms |
| 8000 | 19.4 / 19.6 | 18.5 / 18.7 | 33.2 / 100-114 ms | 23 / 24% | 29.2 / 27.6 ms |
| 16667 | 18.8 / 21.1 | 18.2 / 20.2 | 33.3 / 100.6 ms | 24 / 22% | 29.3 / 29.0 ms |
- No effect: the threshold hypothesis is wrong (or that branch isn't taken). Default stays 0. Next: measure CA's own per-frame CPU (backboardd render thread pstack under swipes; bridge commit stamps vs swaps) to see what fills the 29 ms.
- [22:30] CPU profile under swipes (pstack, 120 samples each): backboardd's CA::Render::Server thread idle in 100% of samples (mach_msg); rendering happens on the IOMFBServer vsync thread (vsync_callback -> render_for_time), running in 6.7% of samples. SpringBoard main thread idle 99%. But psx shows backboardd 69.6% CPU and SpringBoard 37.7% during the bench: pstack only catches threads at syscalls, so it hides running time. Reading: CA's per-frame CPU (render_for_time on one thread) is the likely filler of the 29 ms.
- The kernel scheduler is not big/little aware (no cluster types; 6x A55 1.8 GHz + 2x A76 2.2 GHz scheduled as equals), so the render thread often runs on an A55.
- Next candidates: (a) measure render_for_time CPU time per frame and which CPU it runs on (kernel per-thread stats or a sampling profiler that doesn't stop the process); (b) make the scheduler prefer the A76 cluster for UI/render threads (XNU AMP: cluster types from the device tree + the Edge scheduler, or a simpler bias).
- [21:00] xnu 0166 (CPU sampler): three silent boots. Cause: the xnu CI build is incremental; a new file added to bsd/conf/files is not compiled (the kernel had no "SWEET CPU:" strings). New source files need gh workflow run xnu.yml -f clean=true. Sampler now driven from sweet_gpu.c's 5 s stats tick.

### 2026-10-24 17:00-18:00 SWEET_CULL (bridge dead-draw culling), metal2 runs up to 40134633213 + debug
- See [metal](metal.md) for this entry; it also touches this feature.

### 2026-10-24 15:40 Bridge drawlog (metal2 run 40123962779, commit 0472fb8, SWEET_DRAWLOG)
- See [metal](metal.md) for this entry; it also touches this feature.

### 2026-10-24 12:30 Step 2: Turnip GPU trace under swipes (gt1, MESA_GPU_TRACES=print; kernel already running, 4184 mV charging)
- Bench with tracing: 19.3 swaps/s, 18.0 panel updates/s. Timeline: backboardd GPU/submit p50 9.0 ms, p95 39 ms; GPU end -> swap p50 26.7 ms.
- 1569 render passes: <5 ms 1212, 5-10 173, 10-20 18, 20-40 166, >40 0. All long passes are full-screen 1125x2436 GMEM (44 bins 288x224).
- 5 longest passes: 33-36 ms with only 21-92 draws; draw_ib_gmem is >95% of it, gmem load/store ~1 ms each, binning 0.1-0.3 ms. So a few expensive draws (fill/shader: likely blur or backdrop), not draw count or size.
- copy_buffer_to_image: 10018 copies, 3.5 s total (0.35 ms each); 44-50 copies (~10 ms) ride in the same submits as long passes.
- Follow-up (PC only): the trace has one draw_ib_gmem span per bin, no per-draw data. Slow passes are uniformly slow: nearly every one of the 44 bins costs ~0.7-1.3 ms (fast passes ~0.08 ms per bin), even with only 21 draws. So the cost is one or more full-screen expensive draws (blur/backdrop or large texture sampling), spread over the whole screen. Per-draw attribution needs per-draw timestamps in Turnip's trace.

### 2026-10-24 11:50 Present-mode comparison with the timeline (kernel39 / kernel40 = + xnu 0164 sweetpresentslots)
- See [display](display.md) for this entry; it also touches this feature.

### 2026-10-24 11:30 Step 1 done: per-frame timeline (xnu 0163 on kernel39, boot args sweetpresentwait=1 sweettrace=1)
- Wi-Fi install tool was run by the user (credentials stay out of git). Charger on the TB4 port, 4125 mV at boot.
- Bench tl1/tl2 (Settings swipes, 30 s): 23.4 / 22.5 swaps/s, 22.3 / 21.7 panel updates/s (bench script) with tracing on. Timeline (both runs agree; first version of the tool used 24 MHz and printed times 20% too short, fixed: mach time and CP counter are 19.2 MHz, 718M ticks in a 37 s bench):
| metric | p50 | p95 | max |
|---|---|---|---|
| swap-to-swap | 33.2 ms | 100 ms | 1210-1376 ms |
| backboardd GPU per submit | 5.7-5.9 ms | 40 ms | 41-44 ms |
| present blit GPU | 2.4 ms | 2.6 ms | 2.9 ms |
| backboardd GPU end -> swap | 29 ms | 95 ms | 96-100 ms |
| swap -> present call (tick wait) | 10.7 ms | 29-35 ms | 79-106 ms |
| present call duration | 0.65-0.68 ms | 43 ms | 83-122 ms |
| present submit -> blit GPU start | 0 | 114 ms | 162 ms |
- Rates over the swap span: 20.2-20.3 swaps/s, 19.4-19.5 presents/s, vsync ticks 51.6-52.0/s (nominal 60: after a blocked tick the timer fires late and in bursts).
- Frame intervals: 21-22% <= 17 ms, 64-65% <= 34 ms, ~21% > 50 ms.
- Reading: the GPU is not the main gap. 29 ms from backboardd's last GPU end to the swap (CA/CPU side), the bimodal 40 ms GPU frames, and a vsync tick that only delivers ~52/s dominate; the present wait (p95 43 ms) hits ~5% of ticks.

### 2026-10-24 11:30 Step 0 (video spike), partial; new plan written
- Plan archived to docs/sunfish/plans/2026-10-24-present-path.md; new docs/sunfish/plan.md (video spike, per-frame timeline xnu 0163, backboardd GPU time, ordering).
- New tool phone/tools/vtdecode (AVAssetReader + VideoToolbox probe; userland CI). On kernel38 + current bundle:
  - H.264 and HEVC 1080x1920: VTIsHardwareDecodeSupported = 0; VTDecompressionSession with RequireHardwareAcceleratedVideoDecoder -> OSStatus -12906 (no hardware decoder, as expected).
  - VTDecompressionSession without that requirement -> OSStatus 0, UsingHardwareAcceleratedVideoDecoder = no: a software decoder exists for both codecs.
  - Decode fps NOT measured: AVAssetReader fails "Cannot Access URL ... sandbox extension was not issued" (file in /var/local/tmp or /var/mobile/Media/vt, run as root over ssh; an entitlements plist did not help).
- Safari to http://192.168.7.1:8000 (local test page) is refused: "not connected to the internet" (USB ethernet is not an iOS network). A Safari test needs Wi-Fi: run tools/sunfish-install-wifi.sh yourself (credentials stay out of git and out of chat), or measure decode in-process by feeding VT sessions from a parsed mp4 (not done).
- Webcam note: phone sits rotated 90 deg (top toward the right of the image); flame-cmd taps are in portrait panel coordinates; flame-shot returned a garbled image today.
- Phone is on the 1800 s reboot cycle (k38d booted 11:00).

### 2026-10-24 08:40 CA vsync: backboardd was never on IOMFB vsync
- QuartzCore (research/dsc, `ipsw dyld disass --image QuartzCore`): `IOMFBServer::set_vsync_enabled` subscribes to
  IOMFB vsync (sel 72, type 5) only when `CADeviceUseVBL()`; else it runs its own CADisplayTimer.
  `CADeviceUseVBL` = getenv("CA_FORCE_TIMER") != 0 || MobileGestalt ChipID in {0x7000, 0x7001, 0x8000-0x8004, 0x8010}.
  Our ChipID 0x8015 is not, so CA never saw our vsync. (Setting it via /var/local/mtl2-env is too late: dispatch_once.)
- Fix: launchd.plist (`/System/Library/xpc/launchd.plist`), backboardd EnvironmentVariables CA_FORCE_TIMER=1
  (original: /var/local/launchd.plist.pre-vbl). backboardd now logs "notifications type 5 on/off".
- Bench (kernel36, thread off): 21.6 swaps/s = 20.8 panel updates/s (no dropped swaps; was 17-19 panel, 19-27 swaps).
  With sweetfb_thread=1: 14.7 panel/s. xnu 0161 present-on-swap (kernel37, sweetfb_onswap=1): 14.1 panel/s. Both off.
- Present inline on the vsync tick still takes 10-17 ms wall (blit waits behind backboardd's next job): next lever.

### 2026-10-24 00:40 Night run: where backboardd's GPU time goes
- Brightness: tools/sunfish-dim.sh 10 (LM_0 blend alpha); user confirmed "a lot lower", text readable.
- Experiments without rebuilds: tools/sunfish-m2env-bench.sh TAG [KEY=VALUE...] (writes
  /var/local/mtl2-env, TUDBG=... -> /var/local/mtl2-tu-debug, restarts backboardd, benches).
  Pre-create /var/local/turnip-<proc>.log mode 666 (clients run as mobile).
- No effect on fps or backboardd GPU ms: TU_AUTOTUNE_ALGO prefer_sysmem / prefer_gmem, TU_DEBUG=sysmem / gmem,
  turnip 0102 TU_SWEET_PRIM_ORDER=0 (no per-overlap flush for fetch draws). Not fill rate.
- Bridge telemetry (/var/local/mtl2-telemetry): 1 render pass per frame, 14-46 draws, ctc 7.5-16 ms.
- MESA_GPU_TRACES=print (per-op GPU timestamps) per backboardd frame (~11.5 ms):
  copy_buffer_to_image ~13x = 6.6 ms, copy_image_to_buffer 1.4 ms, the render pass itself 3.5 ms.
- Cause: Turnip reports requiresDedicatedAllocation for every external image, so metal2vulkan never
  aliases an IOSurface (host-pointer import) and keeps a private "shadow" image, copied in/out each frame.
  Also CA's 16/32-byte row pitches are refused for explicit DRM-linear layouts (turnip 0103 logs them).
- Fixes: xnu 0159 (kernel35) rounds single-plane IOSurface rows to 64 bytes (sweetsurfalign=0 off) — alone
  no change; turnip 0104: host-allocation images not dedicated (testing).
- Results (kernel35 + turnip 0104 + bridge 0102): the copy-out is gone and most copy-ins are gone.
  dr23 frame GPU 9.4 -> 6.4 ms, dr14 7.4 -> 4.4 ms. Remaining: ~3 ms/frame of small A8/RGBA8 buffer
  textures with 32-byte pitches (Turnip needs 64 for explicit linear).
- fps barely moved (17-19 panel updates/s): the GPU is only 35-60% busy, and backboardd is idle ~45%
  of the time (pstack samples + phone/tools/a2s; shared-cache symbols are redacted, but most CPU is
  in QuartzCore, little in the bridge). The limit is now pacing/latency, not GPU work.
- Matrix on kernel35 (tools/sunfish-cfgbench.sh): sweetfb_thread=0 vs 1 is the same (~17 panel/s).
- backboardd never calls IOMFB sel 72 (vsync notification enable) on this boot. Check how CA gets
  vsync now (0121 relied on it). This is the next lead for pacing.
- Idle 10 Hz full redraws = the Clock icon's second hand (normal).
- Next: (1) trace CA's vsync source / swap pacing; (2) Turnip: allow 16/32-byte linear pitch for
  sampled-only images (removes the last copies); (3) charger config (the phone loses about 0.5 A while benching).

### 2026-10-24 experiments.md lines
| date | patch / switch | hypothesis | predicted ms saved | measured fps and GPU ms | verdict | commit |
|---|---|---|---|---|---|---|
| 2026-10-24 | `TU_AUTOTUNE_ALGO`, `TU_DEBUG=gmem/sysmem` | render mode / tiling cost | few | no change | dead end | - |
| 2026-10-24 | backboardd `CA_FORCE_TIMER=1` (launchd.plist) | CA ignores our vsync (chip-id gate) | pacing | swaps == panel updates, 20-23/s | kept | 7058eca |
| 2026-10-24 | xnu 0163 timeline (`sweettrace=1`) | measurement | 0 | no overhead (22.3 / 21.7 fps) | kept (tool) | 3e72521 |
| 2026-10-24 | sysmem under swipes | tile overhead makes the 40 ms frames | 10+ | same 13-16 ms/submit, longest 40-45 ms | dead end | e9fd4fa |
| 2026-10-24 | xnu 0165 `sweetvsyncahead=8/16.7` | beat CA's vbl threshold | pacing | no effect | dead end | 244f7e7 |
| 2026-10-24 | xnu 0166 `sweetcpusample` | CA is CPU-bound | - | render thread 10-15 ms CPU/frame, 55-75% on A76; CPUs mostly idle | dead end (waiting, not CPU) | fe0675c |
| 2026-10-24 | SWEET_CTL completion stamps | completion path is slow | - | GPU end -> handlers < 1 ms | dead end | 98a324c |
| 2026-10-24 | timeline analysis: CP span overlap/busy (ctlA/B, cs7/8) | long submits include waiting or others' work | - | 0 overlaps; long spans fixed at 38.6 ms p50 / 40.4 p95; GPU 25-37% busy; trace can't split busy vs stall | inconclusive (needs RBBM busy counter / per-pass CP timestamps) | - |
| 2026-10-24 | `sweetddr=1` DDR/NoC top vote (off by default in sunfish-boot.sh) | 40 ms submits are bandwidth-bound | 5+ | 17.3/16.9 fps vs 20.1/20.3; bb p50/p95 5.7/40.0 ms unchanged (mean 13.1 vs ~16.7, more short submits) | dead end (keep sweetddr=0) | - |

### 2026-10-23 23:05 Smoothness measurements + memory work (stopped at 16% battery)
Bench (tools/sunfish-fpsbench.sh, BENCH_TIGHT=1, Settings swipes, 30 s):
| build | GPU MHz | swaps/s | panel updates/s |
|---|---|---|---|
| 0157 no WFI | 430 | 34.6 | 15.5 |
| 0157 + sweetwfi=1 | 430 | 34.3 | 15.4 |
| 0157 | 565 | 29.5 | 17.4 |
| 0157 | 650 | 28.5 | 17.8 |
| 0158 non-blocking present (+ TU census on) | 565 | 22.7 | 12.5 |
- Bottleneck: the present path, not GPU clock or WFI. present() ~23 ms wall (blit queued behind
  backboardd jobs of 10-42 ms on the single ring) for ~2.4 ms of GPU work; GPU only ~50% busy.
- 0158 (present returns busy instead of waiting) did not help (put off 701 of 600 frames). Not kept as
  default; next idea: present on a second ringbuffer / priority, or scan out without the blit.
- 0156 per-client pool reuse works (73 hits in 6 app switches, no 10 MB bzero each).
- Turnip TU_DEBUG=bos census (phone/turnip/patches/0101, /var/local/tubos): at the first print backboardd
  held ~128 MB but Turnip listed 5 BOs (1 MB); later prints were buffered (fflush added, re-run pending).
- Phone left on k91 (kernel33, 565 MHz, flamereboot=0) to charge, battery 3706 mV.

### 2026-10-23 13:15 Display/GPU tuning — **best: 23.4 panel updates/s** (Metal2, 565 MHz, defaults updated)
Tight bench (continuous swipes), kernel with 0147+0148+0149; panel updates/s (what the screen shows):
| config | swaps/s | panel/s |
|---|---|---|
| CPU render + GPU composite (k37) | 17.0 | 17.2 |
| Metal2 565 MHz, no big-core pref (k36) | 18.6 | 18.2 |
| Metal2 565 + 0147 big cores (k38) | 32 | 17.6 |
| + 0149 present thread, 650 MHz (k40) | 22.3 | 13.9 |
| + 0149 thread, DDR vote (k41) | 22.3 | 11.9 |
| **0149 thread off, 565 MHz, no DDR vote (k42)** | 25.6 | **23.4** |
| thread off + DDR vote (k43) | 31.5 | 17.7 |
- 825 MHz (0148 level 8): GMU acks, but CP me_init times out (GX not running; likely needs ACD/MX vote) → don't use.
  650 MHz works but is not faster: render jobs stay ~15 ms → not clock-bound.
- 0149 (present thread) hurts → off by default (`sweetfb_thread=0`, kept for later). DDR vote: more swaps, fewer panel
  updates (more GPU contention for the present).
- sunfish-boot.sh defaults now: `sweetmtl=2 sweetgpufreq=5 sweetgpudelay=0 sweetfb_thread=0` (Metal2 needs the
  bundle installed: tools/sunfish-metal2-install.sh).
- CI incremental build: build step **109 s** (full build 406 s), run wall time 4.7 min incl. queue.
- (Recovered from `git show 1808cf9^:docs/sunfish/workday.md`; that commit deleted it.)

### 2026-10-23 12:35 0147 big-core preference — **backboardd ~2x faster; panel path is now the cap**
- 0147: threads with priority ≥ foreground take an idle A76 (CPUs 6-7) first (ADT has one SMP cluster).
- Tight bench (k38, Metal2, 565 MHz): backboardd **30–34 swaps/s** (was ~18), panel **16.5–18.6 updates/s**.
- Cap: each panel copy waits ~25–34 ms because the present blit queues behind backboardd's render jobs
  (12–19 ms each, GPU 40–67% busy) on the single ring, and the copy is synchronous.
- Next: 0148 adds the A618's stock 650/800/825 MHz levels (shorter render jobs); later: present on a
  higher-priority path or asynchronous panel copy.
- CI: incremental xnu builds (cache of source/objects/fakeroot; `clean=true` input for a full build).
- (Recovered from `git show 1808cf9^:docs/sunfish/workday.md`; that commit deleted it.)

### 2026-10-23 12:10 FPS reality check — benchmark metric corrected
- The bench counted `swaps`; the screen shows `panel copies`. With Metal at 267 MHz many swaps never reached the
  panel (k35: 22.8 swaps/s but 13.2 panel updates/s). Bench now reports both; `BENCH_TIGHT=1` = continuous swipes.
- Panel updates/s, continuous swipes (k36/k37, GPU 565 MHz acked via DCVS):
  Metal2 + Turnip **18.2** · CPU render + GPU composite **16.7–17.6**. Old bench (with pauses): all paths ~16–17.
- Metal halves backboardd CPU (49 s → 22 s per bench). Neither CPU (~0.55 core) nor GPU (11–29% busy) is
  saturated → the ~18 fps ceiling is frame pacing/latency, not throughput. Next: where backboardd waits per frame.
- (Recovered from `git show 1808cf9^:docs/sunfish/workday.md`; that commit deleted it.)

### 2026-10-23 08:50 Fallback: CPU render path — **pass: 4.3 → 17.2 fps** (4×)
- New `port/tools/sunfish-fpsbench.sh TAG SECS`: unlock + scripted home-page swipes, averages the
  `SWEET FB: 5s` fps lines. Same workload for every measurement.
| Change | Swipe fps |
|---|---|
| baseline `sweetfb_hz=30` | 4.3 |
| `sweetfb_hz=60` | 4.9 |
| drop `com.apple.voicemail.vmd` (Visual Voicemail, spun ~1 core without cellular) | **16.7** |
| drop `com.apple.ReportCrash` (was busy with TTS-synth crash reports) | **17.2** |
- Jobs removed with new `port/tools/sunfish-launchd-drop.sh LABEL…` (backup on phone:
  /System/Library/xpc/launchd.plist.pre-drop). `sweetfb_hz=60` is now the helper default.
- Remaining CPU in the bench: backboardd (~1.1 cores) = the actual renderer.
- (Recovered from `git show 1808cf9^:docs/sunfish/workday.md`; that commit deleted it.)

### 2026-10-23 experiments.md lines
| date | patch / switch | hypothesis | predicted ms saved | measured fps and GPU ms | verdict | commit |
|---|---|---|---|---|---|---|
| 2026-10-23 | `sweetfb_hz=60` | 30 Hz vsync caps the UI | - | 4.3 -> 4.9 fps | kept | df05f86 |
| 2026-10-23 | GPU clock 430 / 565 / 650 MHz | GPU-bound | several | 15.5 / 17.4 / 17.8 fps; GPU ms unchanged | dead end | - |
| 2026-10-23 | xnu 0157 no wait-for-idle between submits (`sweetwfi=1` restores) | WFI serializes submits | few | 15.5 vs 15.4 fps at 430 MHz | kept, no gain | 095806b |

### 2026-10-23 10:00 status.md update
- UI 17 fps while swiping (was 4.3): dropped launchd jobs vmd, ReportCrash, axassetsd; sweetfb_hz=60.
