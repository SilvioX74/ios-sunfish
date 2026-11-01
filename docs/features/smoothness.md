# Smoothness
**State:** 🟡 Settings 50-56 swaps/s, Home 41-44, Control Center 29 at full canvas, at minimum brightness, with env SWEET_DEADFILL=3 M2V_SHADOW_CACHE=1 (2026-11-01; not default yet); 60 not reached. **Updated:** 2026-10-31.

## How to use
- Bench: `tools/sunfish-fpsbench.sh TAG SECONDS` (BENCH_TIGHT=1); config matrix `tools/sunfish-cfgbench.sh`; timeline `sweettrace=1` + `tools/sunfish-timeline.py`. Details in [methods.md](../methods.md).
- backboardd needs `CA_FORCE_TIMER=1` in `/System/Library/xpc/launchd.plist` (backup `/var/local/launchd.plist.pre-vbl`).
- Experiments rule (experiments.md): fps = panel updates/s from `tools/sunfish-fpsbench.sh` (Settings swipes) unless noted; GPU ms = backboardd GPU ms per frame/submit. Stop rule (the user's plan, 2026-10-31): a gain under 1 ms counts only if a 5-boot A/B confirms it; the profile is flat, so small wins are most of what's left.

## Facts
### Frame path
1. **The app or SpringBoard draws its layers.** These are IOSurfaces from SweetSurface; Metal goes through the
   SweetMetal2 bundle (metal2vulkan + Turnip).
2. **backboardd (CoreAnimation's render server) composites one render pass per frame**, 14–46 draws at
   1125x2436, into a 1125x2436 IOSurface.
3. **It swaps through IOMFB SwapEnd (sel 5), which records only the frame**
   (`iokit/Kernel/arm/SweetFramebuffer.cpp`).
4. **The vsync tick (a 60 Hz software timer) presents it**: a GPU blit that scales the frame into the ABL panel
   buffer, scanned out by the MDP.
5. **The vsync tick also notifies clients (type 5).**

#### 1. IOSurface shadow copies (fixed)
- **Symptom:**
  - backboardd's GPU time is linear in its draw count: ~3 ms + 0.33 ms per draw.
  - Neither the GPU clock nor the render mode changes it: GMEM, sysmem and `TU_SWEET_PRIM_ORDER=0` give the same
    numbers.
- **How it was found:** `MESA_GPU_TRACES=print` showed `copy_buffer_to_image` taking ~6.6 ms and
  `copy_image_to_buffer` ~1.4 ms per frame, against a 3.5 ms render pass.
- **Causes:**
  - Turnip reported `requiresDedicatedAllocation` for every external image, so the bridge never aliased an
    IOSurface. Fixed by `phone/turnip/patches/0104`.
  - Turnip rounds linear image rows up to a multiple of 4, so textures whose height isn't a multiple of 4
    were refused. Fixed by `phone/metal2/bridge/patches/0102` (read-only textures may alias past their last row).
  - Core Animation asks for 16/32-byte row pitches, while Turnip needs 64 for explicit DRM-linear layouts.
    xnu 0159 rounds single-plane IOSurface rows to 64 (`sweetsurfalign`).
- **Result:** a dr23 frame went from 9.4 to 6.4 ms of GPU time.
- **Left over:** about 3 ms/frame of small A8/RGBA8 buffer textures with 32-byte pitch. These are not
  IOSurfaces, and Turnip refuses them (turnip 0103 logs each refusal).

#### 2. CA never used our vsync (fixed)
- **The gate:** `CA::WindowServer::IOMFBServer::set_vsync_enabled` subscribes to IOMFB vsync (sel 72, type 5) only
  when `CADeviceUseVBL()` is true. Otherwise it runs its own CADisplayTimer.
- **The check:** `CADeviceUseVBL` = `getenv("CA_FORCE_TIMER")` is non-zero, or MobileGestalt ChipID is in
  {0x7000, 0x7001, 0x8000–0x8004, 0x8010}. Our synthetic chip-id is 0x8015, which is not in the list.
- **Fix:** backboardd `EnvironmentVariables` gets `CA_FORCE_TIMER=1` in `/System/Library/xpc/launchd.plist`
  (the original is backed up at `/var/local/launchd.plist.pre-vbl`). Setting it from `/var/local/mtl2-env` is
  too late, because CA reads it in a `dispatch_once` before the Metal plugin loads.
- **Result:** 20–23 swaps/s with swaps equal to panel updates. Before: 17–19 panel updates/s against 19–27 swaps.

#### 3. The present (no longer the limit)
- **2026-10-29 zc5:** the panel copy isn't on the limiting path (research/notes/fps-limit.md). Kept for the notes below.
- The present blit takes 10–17 ms of wall time but only ~2.4 ms of GPU work. It waits on the single ring
  behind backboardd's next job, and because it runs inline on the vsync tick, the vsync notification goes out
  late too.
- **Tried and worse; keep these off:**
  - present thread (`sweetfb_thread=1`): ~14.7 panel updates/s
  - present-on-swap (xnu 0161, `sweetfb_onswap=1`): ~14 panel updates/s
  - non-blocking present (0158): put frames off
  - DDR vote while presenting
- **Ideas:**
  - notify before presenting
  - a second, higher-priority ringbuffer for the present
  - scanout without the blit (risky: MDP registers)

#### 4. Other facts
- **CPU and GPU are both close to the frame limit** (2026-10-31, dead fill mode 1): render thread 11-16 ms per swap on
  an A76, GPU ~13 ms per frame, GPU busy 77-87 %. The older 35-60 % busy figure was before these fixes.
- **Render thread profile** (pc3, 2026-10-31): SweetMetal2 47.7 %, Turnip 11.7, QuartzCore 10.9, malloc 10.1. Flat: the top
  function is 4.5 %.
- **Idle redraws:** 10 full redraws/s when idle come from the Clock icon's second hand. This is normal.
- **GPU clock:** no gain on Oct 23-24, when pacing was the limit, but 650 MHz gave +11 % swaps/s on the GPU-capped full
  canvas (2026-10-30, before dead fill). 800 MHz fails bring-up. No gain from `TU_AUTOTUNE_ALGO` or `TU_DEBUG=gmem/sysmem`.
- **Battery drains on the charger:** about -0.5 A while benching with the screen on. The kernel never configures
  the charger.

### Patches from this work
| Patch | What it does |
|---|---|
| xnu 0159 | 64-byte IOSurface rows |
| xnu 0160 | Log IOMFB async calls |
| xnu 0161 | Present on swap (off by default) |
| turnip 0102 | Prim-order switch |
| turnip 0103 | Log refused explicit layouts |
| turnip 0104 | Host-pointer images don't need a dedicated allocation |
| bridge 0102 | Read-only textures may alias past their last row |

Kernels:
- **kernel35:** up to 0159
- **kernel36:** up to 0160
- **kernel37:** up to 0161. **Best: kernel37 with `sweetpresentwait=1`.**

## Open
- Plan priorities 1 and 3-5 ([plan.md](../plan.md)): bench tool, CPU and GPU cost per frame, real-use hitches.
- About 3 ms/frame of small A8/RGBA8 buffer textures with 32-byte pitch (2026-10-24 figure; not re-measured).

## Log
(Newest first, append-only. When this Log passes about 150 lines, move its oldest entries to the archive.)

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
