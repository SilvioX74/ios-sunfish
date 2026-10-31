# Smoothness: archived log (entries up to 2026-10-30 22:45)
Current file: [features/smoothness.md](../../features/smoothness.md). Same format, newest first.

## Log

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
