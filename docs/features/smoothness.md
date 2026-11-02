# Smoothness
**State:** 🟡 Safari 60, Settings 56-57 (p95 20-30 ms), Home about 53, Control Center 38.7 swaps/s at full canvas, defaults since 2026-11-02 (luma boost off, mode 3, shadow cache, SWEET_U0011..14) and the quarter-size Control Center backdrop (2026-11-01; not default yet); 60 not reached. **Updated:** 2026-10-31.

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

### 2026-11-02 18:01 dl6b: Control Center held still renders nothing; all cost is the open/close animation (read-only)
- work/cc/drawlog-dl6.txt passes 993-1330 (pid 60; screen = ios 2/3 double buffer), research/notes/cc-draws-dl6-2026-11-02.md. t is CPU encode time; drawlog writing slows frames (open ~33 ms apart, close 44-59 ms), so rates are inflated.
- Open (181.50-182.69 s, 33 frames): 5 passes each: A screen 80-91 draws (fills, wallpaper, Home icons, then draw 79 an opaque full-screen 284x611 blur image over the changed region; cull 3 drops draws 4-78), B 207x460 volume slider offscreen, C screen 63-79 draws (~2-3 Mpx, module backgrounds read the framebuffer: GMEM), D 207x460 brightness slider, E screen 17-21 draws. No blur computed in open frames (reused). 182.2-182.69: spring tail, full cost for 2.5 -> 0.35 px of movement.
- Held 182.78-185.05: not one pass (MIN=1). Close (185.05-185.51, 10 frames, 9-13 passes): Home in full (S93 ~36 Mpx pre-cull), two 284x611 blur passes reading it (cull 3 can't drop Home here: the blur reads it), a one-draw full-screen opaque blur pass, the sliders, the home-indicator pass (402x15 drawn in a full-screen 2-draw pass).
- Remaining cuts (guesses): render area per pass + load elision when the first opaque draw covers it (only pays for GMEM passes; ra alone gave nothing), quarter-size Home in close frames (days), blur size already 0.25 (ccscale). Next check: which passes Turnip runs in GMEM vs sysmem during open/close (one GPU trace).

### 2026-11-02 17:56 dl6: timestamped drawlog with one real Control Center open/close; analysis running
- Bundle fe75b25 (headers end ` t=<mach> tex=ios=<id> pid=<pid>`), kernel158 everyday + SWEET_CULL=3, SWEET_DRAWLOG_MIN=1 MINPX=20000 COUNT=1500: work/cc/drawlog-dl6.txt (24 MB) filled by uptime 192.4 s, so it holds boot, idle Home and cycle 1 only (open 181.682 s, held, close 185.816 s; work/cc/dl6-swipes.txt). Pass t/19.2e6 lines up with kernel uptime (passes at ~181.5/182.3 s and ~185.2/185.8 s around the swipes). Webcam: Control Center open in cycle 1.
- Gotcha: SSH answers at ~18 s but iOS isn't up until the lock screen; unlock swipes before it are lost.

### 2026-11-02 18:40 ra: render area = union of draw scissors (bridge 0124): no gain; stays off
- Bundle 9314dc0 (SweetMetal2 4C4C4422), kernel158 everyday + TU_FETCH_GMEM_MIN_DRAWS=20 + SWEET_SURF_FLUSH=1 + SWEET_CULL=3, 5 boots a side alternating (raa def, rab +SWEET_M2V_RENDER_AREA=1), home,cc,settings, 4453-4455 mV, 0 G1/G2/G4, 0 void.
- Means def / ra: home 52.70 / 52.82, cc 43.49 / 43.36, settings 56.34 / 56.35; p95 the same. Pairs won by ra: home 3, cc 2, settings 1 of 5. `RA shrunk 3059 passes of 28672, 3929.5 Mpx, 21.4 Mpx/s` (rab1): only ~11 % of passes shrink, so too little load/store is saved to show.
- Screenshots and webcam with ra on: clean. raa1's Home (def) had a garbled Music dock icon: the known B44 bug, not this change. Off by default; bridge 0124 stays in the bundle.

### 2026-11-02 17:25 dl5b: correction to dl4/dl5: those passes were Home, not Control Center (read-only)
- research/notes/cc-draws-dl5-2026-11-02.md. The 83-draw and 14-draw passes are two halves of one Home frame, split where the dock's blurred backdrop is re-captured: the 14 draws are the Search pill (269x107 blurred backdrop, 9-slice), magnifier and text, the dock platter (269x107 blur), dock icons, a badge and the status bar; the 97-draw pass is both halves merged. No scaling (icon width 180.000 in every pass), so dl4's "Home offscreen for Control Center's blur" was wrong: the bench window hadn't reached Control Center (SKIP counts passes since backboardd started).
- Home frames: passes 29-39 have scissor 0,192 1120x1472 (only the Clock icon's hands move; grid icons drift 0.001-0.01 px with a 3D transform), yet the render area is the full 1125x2436 with c0 load+store: ~8.7 MB/frame of GMEM load/store more than the draws need. Change being written: render area = union of draw scissors (SWEET_M2V_RENDER_AREA). The 14-draw pass runs in sysmem already (TU_FETCH_GMEM_MIN_DRAWS=20).
- To catch Control Center itself: SWEET_DRAWLOG_MIN=1 MINPX=20000 COUNT~200, better with a timestamp and the c0 texture id in the header (not done).

### 2026-11-02 17:10 dl5: c1 is memoryless (no load, no store): no traffic to save; Control Center alternates an 83-draw Home pass and a 14-draw composite
- Bundle 228e2cc (drawlog logs every attachment), kernel158 everyday + SWEET_CULL=3, SWEET_DRAWLOG_MIN=1 MINPX=100000 SKIP=200 COUNT=40, fixed cc motion, webcam: Control Center. work/cc/drawlog-dl5.txt, 40 passes, all 1125x2436.
- Every pass: c0 ld=1 st=1 mem=0 (load, store), c1 ld=0 st=0 mem=3 (don't-care, memoryless). So CA's second attachment costs no memory traffic, only GMEM space (two attachments, smaller bins). The c1 lead is closed.
- Pattern: 83-draw passes (Home offscreen) alternate with 14-draw full-size passes (likely Control Center's final composite) for matches 200-227, one 97-draw pass, then 83-draw passes. No blur pass (284x611 and smaller) appeared despite MINPX=100000; why isn't checked.

### 2026-11-02 16:45 dl4: Control Center's steady frames redraw Home offscreen every frame; no cover, nothing more to cull (read-only)
- work/cc/drawlog-dl4.txt, 8 passes of 83 draws (research/notes/cc-draws-dl4-2026-11-02.md): fills 0-3, wallpaper 4-17 (8 clears c1, 9-12 draw into c1, 13 composites c1 onto c0), Bdso 18, icons, labels, widget (65 writes c1, 73 a backdrop), dock in one. No status bar, no blur, no module: Home rendered into an offscreen target (icons drift sub-pixel every pass: scaled and redrawn), most likely Control Center's blur source. Dirty rect 0,192 1120x2240 then 1120x1472; the Vulkan render area stays the full 1125x2436 (metal2vulkan-runtime render.rs:3905).
- Per pass ~22.6 Mpx (~16.4 after deadfill) on a 2.5 Mpx dirty rect: fills 8.3, wallpaper 6.2, c1 offscreen 3.3, Bdso 2.1, c1 composite 1.2, icons and labels 0.7. Cull 3's ~20 draws per frame come from an unlogged pass (the final composite, under the drawlog's 30-draw / 1000x2000 limits).
- Leads: c1 (second full-size BGRA8 attachment) load/store unknown: if Load/Store, ~22 MB per frame (guess ~1.5 ms); drawlog extension to log each attachment's load/store/storage mode and smaller passes being written. Then: render area = union of draw boxes (0.1-0.7 ms, guess); Home offscreen at reduced size (biggest, hard).

### 2026-11-02 16:35 Correction: clock times of today's later entries
- Entries stamped 2026-11-02 13:05 to 20:50 in this Log were given estimated times that ran ahead of the clock; they were written between about 12:00 and 16:30 EST, in the order shown. Their content stands.

### 2026-11-02 20:50 mk: SWEET_CULL_MASK0 no gain (2 boots a side, as expected after b54); steady Control Center frames captured
- Bundle 0167c75 (mask-0 skip), kernel158 everyday + SWEET_CULL=3, 2 boots a side (cut short): home def 53.1, 53.1 / m0 53.8, 52.1; cc 43.7, 42.6 / 43.8, 42.2; settings 56.9, 55.0 / 56.4, 54.8. All within noise, screenshots identical, 0 G1/G2/G4. Stays off.
- dl4: SWEET_DRAWLOG_MIN=30 SKIP=40 COUNT=8, fixed cc motion, webcam: Control Center. 8 passes 1125x2436, 83 draws each (one 94): work/cc/drawlog-dl4.txt. Analysis running.

### 2026-11-02 20:20 b54: correction to dl3: the "S1" draws write CA's second colour attachment; they are not empty (read-only)
- The drawlog logs only colorAttachments[0].writeMask (NativeBridgeMetalFrontend.mm:2348), so "mask=0" hid that S1 pipelines write c1 (pipeline dump `PBGRAXm_TatcA2S1Xhf`, boot 20261030-214341: c0 writeMask 0, c1 all; stencil off). Later `..._IsrcM1` draws read c1 onto c0. "S" + digit in CA's labels comes from an index table (research/re/QuartzCore.s 0x18837b898). No stencil, nothing dropped (research/notes/b54-stencil-2026-11-02.md).
- So dl3's "draws 9-12, 27 write nothing (2.65 Mpx)" was wrong. SWEET_CULL_MASK0 requires all 8 attachments' masks to be 0, so it leaves S1 draws alone and should skip about nothing; its A/B is cut to 2 boots a side. Open idea: c1 is a second full-size BGRA8 attachment in every Home pass; whether Turnip stores it out (cost) isn't known: log its storeAction/storage mode.

### 2026-11-02 20:00 cs3: 15-min soak with SWEET_CULL=3 default: 0 GPU bugs in 100 k submits
- Boot ev3 (kernel158 everyday, mtl2-env with SWEET_CULL=3). `tools/sunfish-uisoak.sh cs3a 450`, `cs3b 450`: 50.7 k + 49.7 k submits, 0 G1/G2/G4, 0 backboardd restarts, 0 caps; max 84.8 / 82.8 C. Per app swaps/s (a / b): Settings 53.6 / 51.2, Safari 56.9 / 56.7, Notes 33.8 / 34.8, Calendar 33.9 / 35.6, Maps 30.6 / 30.9, Photos 44.4 / 43.0 (dfs without cull 3: Settings 52.0, Safari 51.8, Photos 45.3). No sign of cull mode 2's G1 doubling so far; tonight's 1 h soak is the real check.

### 2026-11-02 19:45 dl3: real Control Center caught only at its rare 105-draw open frames; steady frames are under 100 draws (read-only)
- kernel158 everyday + SWEET_CULL=3, fixed cc motion, webcam: Control Center open; SWEET_DRAWLOG_MIN=100 logged 3 passes of 105 draws, seconds apart (Clock icon hands move between them): Home draws 0-91, then a blended (not opaque) 284x611 blur `PBGRABsovXm_TimgA2Xhf_IsrcCcl` (draw 93) and 3 small modules. Under a blended blur Home is live, so cull 3 correctly keeps it. DF3's 790-947 draws/s at 43.8 swaps/s means ~20 draws culled in each steady frame, which has fewer than 100 draws.
- The drawlog records draws before the cull (DrawLog::draw at encode, :1400/:1421; cullReplay at endEncoding :1290).
- Found on the way: draws 9-12 and 27 (S1 wallpaper draws) have colour mask 0 in passes without depth/stencil: they write nothing (~2.65 Mpx per pass in every pass that draws Home). A skip rule behind an env is being written (SWEET_CULL_MASK0). Next: capture the steady frame (MIN=30, SKIP~40, COUNT=8).

### 2026-11-02 19:10 cv: cover cull SWEET_CULL=3: Control Center 41.9 -> 43.8 swaps/s in 5 of 5 pairs, picture clean; default now
- Bundle metal2-next 6826fcb (cull 3), kernel158 everyday args + TU_FETCH_GMEM_MIN_DRAWS=20 + SWEET_SURF_FLUSH=1, fixed cc motion (cc2), 5 boots a side alternating, `SWIPE_MS=3000 SETTLE=90 MAXSTART=99 tools/sunfish-perfab.sh TAG 1 --screens cc,home,settings`, 4453-4454 mV, 0 G1/G2/G4, 0 caps, 0 void. Webcam: the new cc motion opens and closes Control Center, no Spotlight.
- Control Center def 41.70, 41.87, 41.64, 42.30, 42.21 (p95 mean 39.9) / c3 43.50, 43.75, 43.72, 43.80, 44.10 (p95 34.5): +1.83, ranges don't overlap. Home 52.55 / 53.36 (+0.8, noise), Settings 56.99 / 56.88. Max C 78.3 / 77.7.
- Telemetry (extra c3 boot with /var/local/mtl2-telemetry): `DF3 covered` 790-947 draws/s, 133-157 Mpx/s skipped during the cc bench; Spotlight/Control Center by hand 118-428 draws/s.
- Webcam on c3: Spotlight over Home, Control Center open and closed: clean. perfab screenshots cc/settings same as def.
- Default now: SWEET_CULL=3 in /var/local/mtl2-env via tools/sunfish-install-speed.sh (off: delete the line). Watch G1 in tonight's soak (cull mode 2 once doubled it). Control Center 43.8 against the 45 target.

### 2026-11-02 18:40 cc2: the second draw log was Spotlight too; the Control Center bench could drift into Spotlight: fixed
- work/cc/drawlog-dl2.txt (MIN=120): 12 passes of 147-157 draws = Spotlight with the keyboard up (no module grid; webcam frames agree), not Control Center. Draw 87 `PBGRAXm_TimgA2Xhfn_IsrcCcl` (blend 0, mask 15, the 509x1099 blur over the full screen) covers draws 0-86 (Home, wallpaper, dock; 19 framebuffer readers): ~25 Mpx hidden of 36 per pass, ~15 Mpx more than mode 3 already drops; SWEET_CULL=3 would catch them (research/notes/cc-draws-dl2-2026-11-02.md). Risk: if the cover's quad ends at 2429 instead of 2436, the bottom draws stay (the DF3 row will tell).
- The bench's cc motion (open 1000,5->1000,900; down drag 30,1100->30,1900; up drag 30,1900->30,1100) opens Spotlight whenever a reopen misses, and the rest of the window measures Spotlight. ata5's 31.1 swaps/s (p95 64.7, 10 gaps) has a grey CC backdrop without dock colours, like CC over Spotlight on the webcam: about 1 in 60 cc runs today, -9 swaps/s each.
- Fix in tools/sunfish-perfab.sh cc_swipes: a slow pull from the top-right edge (1000,5 -> 1000,1500, 800 ms) opens, the up drag in the left margin closes; no swipe starts mid-screen. New cc baseline needed; earlier cc numbers are "old sequence".

### 2026-11-02 18:10 dl1: draw log caught Spotlight, not Control Center: 63 of ~110 draws (18 Mpx) are hidden under an opaque blurred backdrop
- Bundle with drawlog options (972ef6a), kernel158 everyday, SWEET_DRAWLOG_MIN=100 COUNT=12, boot 20261102-144348; work/cc/drawlog-dl1.txt, 12 passes 1125x2436 of 108-110 draws (research/notes/cc-draws-2026-11-02.md). The runner's close swipe pulled Spotlight down; the scene is Spotlight over Home (Siri Suggestions platter, search bar, blinking caret = 108-110 draws).
- Per pass ~23.7 Mpx on a 2.74 Mpx target: wallpaper 7.6, 4 identical opaque fills of the dirty rect 5.9, Home plates 1.9, Home icons/labels/widget ~1.5. Draw 71 (`PBGRAXm_TimgA2Xhfn_IsrcCcl`, blend off, full mask, the 509x1099 blurred backdrop stretched to full width) overwrites the whole dirty rect; draws 0-70 (~18 Mpx, 15 of them framebuffer-reading) lie inside it in all 12 passes: dead work. Same pipeline sequence every frame.
- The bridge's cull catches only draws 43-70: IsrcCcl isn't whitelisted as covering, and it uses the scissor as each draw's clip. Being written: cover cull behind an env (implementer). Re-capture of the real Control Center pass (MIN=120, webcam-checked) running (dl2).

### 2026-11-02 17:40 at: TU_AUTOTUNE_ALGO=profiled: no gain over 5 boots; one Control Center outlier at 31 swaps/s
- kernel158 everyday args + TU_FETCH_GMEM_MIN_DRAWS=20 + SWEET_SURF_FLUSH=1, 5 boots a side alternating (ata = def, atb = profiled), cc,home,settings, 4440-4455 mV, 0 G1/G2/G4, 0 void.
- cc swaps/s def 40.2, 40.4, 40.2, 40.2, 31.1 / atp 39.9, 40.5, 40.0, 39.5, 40.3 (median 40.2 / 40.0; atp won 2 of 5 pairs). home def 53.3, 53.7, 51.6, 52.4, 52.3 / atp 47.7, 55.5, 53.3, 53.6, 53.5. settings def 55.5-57.2 / atp 56.5-57.5 (+0.6 mean, within noise). Screenshots normal. Profiled stays off.
- ata5's Control Center run: 31.1 swaps/s, p95 64.7 ms, 10 gaps over 100 ms, GPU busy 82 %: a one-off stall on the default (1 in 15 Control Center runs today); not followed up yet.

### 2026-11-02 16:50 ts: Turnip env sweep on Control Center: TU_AUTOTUNE_ALGO=profiled about +1 swap/s (2 boots), GMEM_MIN_DRAWS=100 no gain
- kernel158 everyday args + SWEET_SURF_FLUSH=1, new bundle, 2 boots per config, cc,home, 4453-4455 mV, 0 G1/G2/G4; one void run (lock screen, ts2b) rerun.
- cc swaps/s (p95) / home: def (GMEM_MIN_DRAWS=20) 39.4 (36.9), 39.2 (35.9) / 53.1, 52.9; d100 38.8 (40.4), 39.1 (37.8) / 51.9, 52.7; atp (TU_AUTOTUNE_ALGO=profiled) 40.2 (36.6), 40.5 (34.7) / 52.7, 51.9. atp minus def: cc +1.05, home -0.7. Screenshots normal.
- Under the 1 ms stop rule: a 5-boot A/B with Settings added is running (ata/atb).

### 2026-11-02 16:20 po0: per-overlap flush off gives nothing on Control Center: Turnip 0111 dropped
- Bundle metal2-next with Turnip 0111 (4bba3bf, off by default), kernel158 everyday args + TU_FETCH_GMEM_MIN_DRAWS=20 + SWEET_SURF_FLUSH=1, 2 boots a side (po0a, po0b), cc,home, 4441-4455 mV, 0 G1/G2/G4, 0 void.
- def: cc 40.4, 40.3 (p95 39.7, 37.2), home 52.4, 53.4. TU_SWEET_PRIM_ORDER=0 (flush fully off, picture broken: cyan background, no blur, garbage at slider edges): cc 39.0, 38.7 (p95 35.0, 37.3), home 53.9, 52.0. cc median frame 24.7 -> 25.8 ms, GPU busy 88 -> 86-87 %.
- So the upper bound of 0111 on Control Center is about zero (slower, if anything); 0111 stays in the bundle off and isn't tested further here. The new bundle on defaults: Control Center and Home screenshots normal. Next: Turnip env sweep (ts).

### 2026-11-02 15:40 ccp: Control Center plan: per-overlap flush only for framebuffer readers is the best lead (read-only)
- research/notes/cc-plan-2026-11-02.md; cc3 trace re-read (kernel139, older than today's defaults; traced times ~2.4x real). Drag frame, real ms (estimates): main pass 134 draws ~11 (spread evenly over all bins: the draws, not one area), one-draw composite ~3.6, module tiles 79 draws ~3.2, overlay ~2.
- metal2vulkan marks every pipeline of a pass with any framebuffer-fetch draw for rasterization order (pipeline/graphics.rs:811-824, render_target.rs:1209-1212), so Turnip flushes per overlap on all of them (tu_pipeline.cc:3790-3797, :4424-4438). ios-redfin's Turnip 0107 (SWEET_TU_ORDER_FETCHERS_ONLY=1) keeps it only for shaders that read an input attachment. On Settings, TU_SWEET_PRIM_ORDER=0 once gave -1.9 ms/frame (k1). Being ported; upper bound run (po0, TU_SWEET_PRIM_ORDER=0, picture broken) queued.
- Ruled out or small: UBWC (U9) ~0 (targets linear, blur images already compressed, noubwc no change); no CA/CoreMaterial knob caches the Home redraw behind Control Center (reusing it is days of bridge work); TU_DEBUG=sysmem worse; scratch trim no change. Smaller: TU_FETCH_GMEM_MIN_DRAWS=100 / TU_AUTOTUNE_ALGO=profiled (0-1 ms), COLOR_WRITES=fetch (0-0.5 ms), SWEET_DRAWLOG_MIN to see the 134 draws.

### 2026-11-02 13:40 sf: SWEET_SURF_FLUSH=1 (no flush at completion): speed unchanged, picture fine; default now for heat
- Bundle metal2-next de4fcfd (UUID 4C4C44C6), kernel158 everyday args + TU_FETCH_GMEM_MIN_DRAWS=20, 5 boots a side alternating (sfa = default 3, sfb = 1), `SWIPE_MS=3000 SETTLE=90 MAXSTART=99 tools/sunfish-perfab.sh TAG 1 --screens settings,home,cc`, 4453-4455 mV, 0 G1/G2/G4, 0 void. mtl2.log: `SWEET SURF FLUSH: mode 3` / `mode 1` for backboardd.
- Settings 56.60 / 56.54 swaps/s (p95 median 19.6 / 20.2), Home 53.39 / 52.85, Control Center 40.18 / 40.35; GPU busy the same; max C rises with run order, no side difference (79.9 / 79.0). Screenshots (Settings, Control Center) and the app switcher look normal.
- The flush at completion cost ~1.17 ms per frame on the completion worker (rc): CPU time, not on the frame's path, so no speed change was expected; the heat saving isn't measured separately. Default now in mtl2-env (tools/sunfish-install-speed.sh; off: delete the line). Risk to watch: the kernel's CPU-copy present fallback (used around GPU recoveries) could show stale strips.

### 2026-11-02 12:30 rc: backboardd's render CPU on Settings: mostly bridge per-draw work; the surface cache flush is the one hot spot (read-only)
- 300 pstack samples of backboardd over 5.4 s of Settings scrolling (kernel158 everyday, metal2-next; work/heat/bbd-settings-raw.txt), symbolized (research/notes/bbd-cpu-2026-11-02.md). No thread spins: threads 25-28 are parked workqueue threads (pc 0, never in user space).
- Render thread (thread 3): busy 13.4 ms per 16.7 ms frame (catrace: 13.2). Per frame: bridge Rust runtime 4.90, bridge allocation and collections 1.73, bridge ObjC mode-3 cull list (dfNote, dfBindHash, CullEntry copies) 1.28, CA's own code 2.00, Turnip 1.95, sys_dcache_flush at commit 0.72, other kernel calls ~0.6, idle 2.45. Inclusive: draw_resident 3.56, capture_backdrop 3.06 (reaches the deferred draws through cullReplay 2.56), malloc+free+FastAlloc ~2. Top leaf: sys_dcache_flush 5.4 %; the rest is a flat tail.
- Completion worker (thread 6, contexts::run): 1.56 ms per frame, 21 of its 28 busy samples in sys_dcache_flush (flushSurfaces < -[NBCommandBuffer finish]).
- Cut candidates: skip the IOSurface flushes the CPU doesn't need (0.72 ms on the critical path + 1.17 ms heat; env knob SWEET_SURF_FLUSH being written); redundant pipeline binds and a new VkRenderPass per frame in Turnip (0.3-0.5 ms, guess); allocation churn (0.5-1 ms, guess); mode 3 replay overhead ~0.9 ms against its GPU savings.

### 2026-11-02 12:10 k158: kernel158 everyday check: speed unchanged with the A76 at 1939
- `SWIPE_MS=3000 SETTLE=90 MAXSTART=99 tools/sunfish-perfab.sh k158 1 --screens settings,home,cc`, kernel158 everyday args + TU_FETCH_GMEM_MIN_DRAWS=20, 4454 mV, 0 G1/G2/G4 in 10 k submits. Settings 56.6 (p95 19.9), Home 53.0 (32.6), Control Center 40.3 (41.5); A76 1939 MHz on all three; max 78.0-81.6 C (k157: 83.5-86.7). Everyday boot ev158 after, webcam: lock and Home normal.

### 2026-11-02 11:30 la: cold app launches: first frame ~0.3 s for most apps, Safari 1.0 s, Maps 2.9 s
- Running boot (kernel157 everyday), 3 cold launches per app (killed, from Home, `openapp`), catrace 8 s each (work/launch/). t0 = the app's exec (class 7 record), t1 = its first CA commit, t2 = last render of the launch burst (ends at a 200 ms render pause; for Settings, Calendar and some Photos runs that falls around 3.5 s, idle or animation renders, so read t2 loosely).
- Median t1-t0 / t2-t0 ms: Settings 290 / 3593; Calendar 301 / 3518; Photos 307 / 3653; Notes 530 / 1629 (2 runs); Safari 1004 / 1738; Maps 2880 / 7528 (2327-3899 first frame).
- Notes launch 1 crashed: `CRASH MobileNotes[783] sig 10` (SIGBUS) far 0x233ac7a68 inside a mapped 0x232000000-0x234000000 prot 3/3 tag 35 (B52). 0 GPU errors, 0 backboardd restarts, 0 lost trace events.
- Webcam after each app's 3rd launch: Settings, Calendar OK; Safari tab overview with black tab thumbnails; Notes "Hello there" with a dark rounded rectangle in the body (an attachment not drawn, guess; B44 family); Photos a black area and one solid blue tile (maybe still loading); Maps black map (B50) with a small magenta glitch on the Work button.
- Next for launches: Maps' 2.9 s first frame and Safari's 1.0 s are the long ones (what they wait on: catrace + pstack at launch).

### 2026-11-02 11:15 gp: Settings' >100 ms gaps are the bench's swipe reversal, not a stall (read-only)
- vpa1-5 and vpb1-5 per-boot logs + catrace (research/notes/settings-gaps-2026-11-02.md). All 28 vpa gaps and 28 of 30 vpb gaps sit at the bottom reversal: `SWEET HID: touch 9 up` and `down` at 540,2000 in the same ms, gap starting 7-40 ms later, lasting 100-130 ms, every ~6.45 s (one swipe cycle).
- Inside each gap: Preferences' display link fires every 16.7 ms with ~1 ms of work, its 2-4 commits reach backboardd within 1 ms, backboardd's per-vsync event fires 4-8 times but render_for_time runs once (nothing changed), the kernel ticks every frame. The gap ends when Preferences sets a frame begin time and commits a moved list. Longest interval near each touch-down: bottom p50 98, max 130 ms; top p50 64, max 98.
- The other 2 vpb gaps (248 and 150 ms) are lost SWEET TL records: catrace shows 14 commits and 15 renders at 60 Hz there; TL kept GPU spans and kernel blits but no swap/tick records while its clock line says "dropped 0". 2 in 50 windows: a trace tool bug that can fake gaps.
- Cost of the reversal to the bench: about 1.3 swaps/s of the ~57 ceiling (guess). Not a user hitch. methods.md notes it.

### 2026-11-02 10:45 ml: CA_ENABLE_MEMORYLESS=0 is much worse: Control Center 40.5 -> 26.7, Home 54.2 -> 40.5
- `SWIPE_MS=3000 SETTLE=90 MAXSTART=99 tools/sunfish-perfab.sh mlo1/mlx1/mlo2/mlx2 1 --screens cc,home`, kernel157 everyday args + TU_FETCH_GMEM_MIN_DRAWS=20; backboardd env via tools/sunfish-install-caenv.sh: OFF = CA_DISABLE_LUMA_BOOST=1 CA_FORCE_TIMER=1 (the everyday plist), ON adds CA_ENABLE_MEMORYLESS=0. 4453-4454 mV, 0 G1/G2/G4, 0 void.
- OFF: cc 40.3, 40.7 (p95 47.7, 47.8), home 54.6, 53.7. ON: cc 27.5, 25.9 (p95 60.0, 63.2), home 40.2, 40.8 (p95 48.5). Screenshots the same. Stopped at 2 boots a side: the gap is 13.7 swaps/s. Memoryless stays on (default).
- Correction to the plan's note: the everyday backboardd env also has CA_FORCE_TIMER=1 (needed: CA renders on our kernel tick). Restored and rebooted (mlend, lock screen OK).

### 2026-11-02 09:30 k157: kernel157 everyday check
- `SWIPE_MS=3000 SETTLE=90 MAXSTART=99 tools/sunfish-perfab.sh k157 1 --screens settings,home,cc`, kernel157 (sweetvsyncphase=5000 built in), everyday args + TU_FETCH_GMEM_MIN_DRAWS=20, 0 void, 0 G1/G2/G4 in 10 k submits. Settings 56.4 (p95 20.9), Home 52.8 (31.8), Control Center 40.3 (39.2); max 83.5-86.7 C. One launch per screen, so the Settings number is one sample. Webcam after the hs1 boot: lock screen normal.

### 2026-11-02 09:30 lat: Settings commits wait for touches, not the display link; the hold to tick+9.6 costs about 0 ms (read-only, parked)
- vpa1-5 traces (phase 5000), 24 981 frames, p50 ms after our tick: display link scheduled 4.13, fires 4.27; touch timestamp 5.63 (p10 1.1, p90 10.0); start_frame 0x31ca00d1 (CA::Transaction::start_frame 0x1882e1f40) 9.79; finish_frame 11.42; commit 11.55-12.40; next render 16.98. backboardd's render_for_time runs tick+0.05 to tick+13.43 (13.2 ms). Notes research/notes/latency-2026-11-02.md.
- During a drag a touch starts each frame (start_frame reason 0x20 from __processEventQueue 0x188eb2388: UIKit's update cycle is off, _UIUpdateCycleEnabled 0x189192744), not the display link. Touches stamped tick+1..8 start their frame no earlier than tick+9.6; tick+9..17 start at once (+1.4-2.5 ms). SpringBoard shows the same floor (start_frame at 9.83). The floor follows backboardd's finish_update in the same tick (r 0.40), not the display link. Bench touches come from the kernel's ud_touch_cmd, about 93 Hz.
- Guess (low): touch routing in backboardd or the HID queue starves while the render thread's CPU phase runs (tick+0.1..9.5).
- Estimated gain of removing the hold: about 0 ms input-to-photon (the commit still lands before the same render tick); slack before the tick 4.3 -> 8-10 ms. Ungated commits could reach the touch rate (more CPU, heat). The bigger term is the 13.2 ms render. Parked (backlog Later); the confirm test is one catrace window with MACH_SCHED and UIKit 0x2b87 codes.

### 2026-11-02 09:05 vpa/vpb: sweetvsyncphase=5000 removes the low Settings launches (0 in 25); default in kernel157
- kernel156, everyday args + TU_FETCH_GMEM_MIN_DRAWS=20, `CATRACE=1 RELAUNCH=5` Settings, 5 boots a side alternating, 4453-4455 mV, 0 void, 0 G1/G2/G4 (22-23 k submits a boot), webcam after: Home normal.
- `sweetvsyncphase=5000` (vpa1-5): 0 low of 25, none under 55.9; mean 57.0 swaps/s, median p95 19.26 ms, mean skip2 3.07, 22 of 25 with p95 <= 20 ms. Display-link phase 4.07-4.19 ms in all 10 launches checked (p10 3.98, p90 4.8).
- Off (vpb1-5): 2 low (41.9, 44.7) plus 3 at 52.6-54.4 with p95 32-33 ms; mean 55.23, median p95 19.31, skip2 6.82, 17 of 25 with p95 <= 20. Their phases 11.3-14.9 ms, different each launch.
- Default: xnu 0275, `sweetvsyncphase=5000` unless the boot-arg says otherwise; any value over 16000 turns it off (kernel157). Settings target: p95 met (19.3); 58 swaps/s not reachable by this bench (its swipe gaps cap it near 57).
- Still open: the full fix is sending IOMFB frame info (Presentation_time) so CA anchors its display links itself (backlog Later); commits never start before tick + ~11 ms even with the display link at +4 ms (input or render handoff wait?), a latency lead.

### 2026-11-02 08:45 vp13: proof: kernel156 sweetvsyncphase=13500 locks the phase and makes every launch low
- kernel156 (xnu 0274) everyday args + `sweetvsyncphase=13500`, TU_FETCH_GMEM_MIN_DRAWS=20, `CATRACE=1 RELAUNCH=5` Settings, boot 20261102-080939, 4453 mV, 0 G1/G2/G4.
- Swaps/s L1-L5: 42.9 43.5 41.7 42.2 42.4 (p95 about 34 ms, skip2 35-37 %): 5 of 5 low, as predicted. Display-link phase (dl.py) 12.49-12.52 ms in all 5 launches (SpringBoard 12.49-12.57): locked, where it normally jumps between launches. It reads 1 ms under 13500 because the trace measures from the tick's wake time, about 1 ms after its deadline (guess for the offset).
- The setup line `SWEET FB: vsync phase ...` isn't in the per-boot log: that log starts at about 23 s (USB), after the framebuffer's setup. The kernel has the string.
- Cause confirmed. Next: sweetvsyncphase=5000 against off, 5 boots each (vpa/vpb, running).

### 2026-11-02 08:35 u6: TU_GPU_TRACEPOINT filter works but doesn't make Control Center traces cheap
- `SWIPE_MS=3000 SETTLE=90 MAXSTART=99 tools/sunfish-perfab.sh u6a/u6b/u6c 1 --screens cc,home`, kernel154 everyday args + TU_FETCH_GMEM_MIN_DRAWS=20, 4454 mV, 0 G1/G2/G4.
- Untraced (u6a): cc 39.3 (p95 46.2), home 53.9 (32.5). `MESA_GPU_TRACES=print` (u6b): void, 0 swaps on both screens, 234 submits in all, no backboardd restart seen, 3.3 MB of trace. With `TU_GPU_TRACEPOINT=-draw_ib_gmem -gmem_load -gmem_store -binning_ib` (u6c; spaces as separators reach the env intact, the four names are gone from the file): cc 22.4 (p95 90.1), home 33.1, 55.5 MB of trace.
- u6c's file is dominated by sysmem_clear and gmem_clear (117 k lines each), then cmd_buffer 21 k, render_pass 12.7 k. Full-size 1125x2436 pass 7.56 ms traced (u6b 8.61 ms with per-bin stamps), 44 bins.
- Reading: per-bin stamps weren't the main cost; the clear tracepoints are. A next try would add `-sysmem_clear -gmem_clear -sysmem_clear_all` (untested). U6 parked: traces aren't needed for the current Priority 1 work.

### 2026-11-02 08:25 Low Settings launches found: display-link phase against our vsync tick (catrace, read-only)
- Analysis of the 30 ct/st traces (research/notes/catrace-2026-11-02.md, scripts in catrace-2026-11-02/). Observed: apps' CADisplayLink timers (Preferences, SpringBoard) fire at exact multiples of 320000 ticks (1/60 s from mach time 0) in all 30 launches; no 0x31ca002c events, so CA's frame_info_callback/add_timing never ran (we send no IOMFB Presentation_time), and the display-link anchor never moves. Our tick (SweetFramebuffer.cpp:110, 16667 us = 320006 ticks) has its own phase, re-anchored after any late tick (:456-461); within a launch it drifts +0.08 ms per 4 s, between launches it jumps several ms.
- DL = display-link fire minus our tick. All 7 low launches have DL 12.5-14.8 ms (42.7-50.8 swaps/s, 16-41 % of renders 33 ms); all 23 good ones are outside (0.8-11.3 ms, or 15.4-15.5 mild). Commits start at about max(tick + 11 ms, DL + 2-3 ms), so in the band a commit ends right at the next tick, within its +-1 ms wake jitter, and two commits share one render.
- Same in low and good: commit rate 61-62/s, commit 0.65-0.74 ms, Preferences main thread CPU (mostly cpu7), backboardd renders on cpu6.
- Guess: the phase alone causes the lows (90 %); the band is about 3 ms of 16.7, so ~18 % of launches by chance (70 %). Launch 2's excess earlier isn't explained.
- Evidence in QuartzCore: display_timer_callback 0x1882bacc8 -> get_phase 0x1882bab34 with IOMFBDisplay::timing_reference 0x1882bc40c (shmem +0x28/+0x30/+0x38), written only by add_timing 0x18853d33c from frame_info_callback 0x1882bc6d4.
- Fix being written: xnu 0274 `sweetvsyncphase=US` locks our tick to the grid at k*P - US (P exactly 1/60 s). Tests: US=13500 one boot (all launches should be low: proof), then US=5000 against off, 5 boots each. The full fix later: send frame info so CA anchors the display links itself. Open: commits never start before tick + ~11 ms even when the display link fires at +1-4 ms (input or render handoff wait?).

### 2026-11-02 08:10 ct/st: catrace over 30 Settings launches; settle after kill doesn't remove the lows; d20 on Settings unsettled
- `CATRACE=1 RELAUNCH=5 SWIPE_MS=3000 SETTLE=90 MAXSTART=99 tools/sunfish-perfab.sh TAG 1 --screens settings`, kernel154 everyday args, 4454-4455 mV, 0 G1/G2/G4, 0 void. catrace works (sanity run: 6905 records in 6 s, 0 lost); a trace file per launch.
- Swaps/s L1-L5: cta base 56.8 56.8 56.8 56.8 55.9; ctb d20 47.8 44.4 55.3 57.9 50.8; ctc base 56.7 55.4 56.8 57.4 42.7; ctd d20 57.3 57.1 57.3 57.2 44.3; sta base + RELAUNCH_SETTLE=10 49.7 45.4 57.3 57.3 57.0; stb same 56.4 56.1 57.1 56.2 56.8. skip2 15-38 % in low launches, 1.6-6 in good.
- Settle check: 10 s after the kill, sta L2 was still low (45.4): a busy start right after the kill isn't needed for a low launch. Lows today: L1 2, L2 2, L5 3 (+1 mild).
- `TU_FETCH_GMEM_MIN_DRAWS=20` on Settings: 3 clearly low of 10 (ctb L1, L2, ctd L5) against 3 of 20 base (ctc L5, sta L1, L2). Good launches the same (57.1-57.9). Too few to tell; d20 stays default, watched in the next Settings runs (if the rate doubles, it goes off).
- Found: Preferences emits Transaction::commit (0x31ca0005, ~62/s), not commit_transaction 0x31ca0215 (only SpringBoard does), so catrace-dump --pair finds nothing; analysis on 0x0005 + backboardd's 0x0009/0x000d in progress (investigator). perfab started catrace with nohup, which the phone lacks: fixed (e049067).

### 2026-11-02 07:35 cs4: TU_FETCH_GMEM_MIN_DRAWS=20 on Control Center: +1.1 swaps/s over 5 boots, default now
- `SWIPE_MS=3000 SETTLE=90 MAXSTART=99 tools/sunfish-perfab.sh cs4a..cs4d 1 --screens cc,home`, kernel154 everyday args, metal2-next, empty env except the threshold. 4453-4455 mV, 0 G1/G2/G4 in 62 k submits.
- Control Center swaps/s (p95 ms): base 39.2 (49.1), 39.0 (47.0), 39.1 (45.3), 39.1 (45.9): mean 39.10, p95 46.8. d5 39.9 (43.1), 40.5 (46.6), 39.8 (44.7), one void (lock screen, bench setup): mean 40.07, p95 44.8. d20 40.2 (44.3), 40.8 (46.8), 40.3 (40.6), 39.8 (44.5): mean 40.28, p95 44.1. With cs/csr: d20 5 boots mean 40.5 against base 8 boots mean 39.4; d20 above base in every boot pair.
- Home unchanged (base 53.3, d5 52.9, d20 53.6; p95 32.5-32.7). Control Center screenshots base/d5/d20 (cs4a) look the same: blur, colours, no black tiles.
- Default: `TU_FETCH_GMEM_MIN_DRAWS=20` in /var/local/mtl2-env via tools/sunfish-install-speed.sh (off: delete the line). Not yet benched on Settings or apps: next Settings run carries a d20 side. Still far from the 45 target.

### 2026-11-02 07:00 ll: low against good Settings launches from the app's side (read-only): nothing logged tells them apart
- 12 low launches (under 50.5 swaps/s) against 46 good (56+), rl1-4 (kernel154), vg1-4 and va1-4 (kernel155), boot logs 20261102-040919 to -055936. Notes: research/notes/low-launch-2026-11-02.md.
- Observed: the live log has no runningboard, jetsam, assertion, frame-rate, display-link or app oslog lines. Preferences main-thread CPU is the same in low and good launches (rl4 L4 1072-1120 ms per 5 s, good 964-1155); in low ones backboardd's render thread works less (2443 against 3584 ms per 5 s) and cpu6 is less busy (54-69 % against 75-82 %).
- Launch 2 differs from launch 1: a fresh start (launch 1 is the process started at boot), it follows the launch-1 screenshot, and the one remotemanagementd burst per boot (158-168 s uptime) lands next to it in all 12 boots. These lead-in lines are also there before the 4 good launch 2s, so they don't explain it alone.
- Found in QuartzCore (research/re/QuartzCore.s): kdebug codes, not gated: 0x31ca0215/0216 client commit, 0x31ca0009 server receives a commit (context id), 0x31ca0005 Transaction::commit with frame-begin time, 0x31ca000d each render, 0x31ca00c9/00ca the app's display link. No env var logs commits (CA_LOG_* knobs are only image queue and class descriptions). The mtl2 K lines and SWEET_CTL never see commits.
- Guess (about 30 %): the app's frame clock picks up a phase against our tick at process start and keeps it; a start in a busy moment anchors it late and the commits sit at the render gate's edge, so about 1 tick in 3 merges two commits.
- Next: `catrace` (userland kdebug reader, implementer writing it) over 2 boots with RELAUNCH=5, commit-to-render offsets per launch. Cheaper side check: a 10 s settle between the kill and the reopen; if launch 2 stops being low, start-up timing is involved.

### 2026-11-02 06:45 mo3: mode 3 webcam motion check: no stale strip in 900 frames; doubling is the camera
- kernel154 everyday setup (sweetse9=2 sweetcampmic=2 sweetswapbig=6 sweetampreset=1), bundle metal2-next (mode 3), boot 20261102-062911, 4455 mV. Iriun webcam 3840x2160 at 30 fps; a page swipe lasts about 16 frames, so mid-motion is caught.
- A: 6 Home page swipes, 420 frames (research/mo3/home/). B2: open Settings, 2 scrolls, Control Center open and close, 480 frames (research/mo3/apps/). B1 void: G4 hang at 210.020 s (fsr 40000002), recover2 done in 45.5 ms, then backboardd killed after recovery (by design), back on the Lock screen.
- Observed: no stale strip, black band, flash or old content left behind, in motion or settled. Mid-motion frames show every moving thing twice (Home icons, the whole Control Center), both copies sharp. The hard edges where a ghost ends (mid_138) sit at a different screen row in each frame (f135-f141), along the panel's scan direction: the 30 fps exposure spans two 60 Hz panel frames, cut wherever the scan is. A tile edge from mode 3 would stay put.
- Not covered: a stale strip that lasts a single 60 Hz frame can't be seen at 30 fps. U22 stays open for that; it doesn't gate anything.
- Side notes: an injected swipe up from the bottom edge (`W 0.5 0.99 0.5 0.4 200`) scrolled Settings instead of going Home (injection, not checked by hand). Settings looks dark and low-contrast in dark mode on the webcam (exposure, not checked).
- Live-log listener restarted before this boot (usb0 link-state fix now running).

### 2026-11-02 06:24 vg/va: neither sweetvsyncgrid nor sweetvsyncahead removes the low Settings launches; launch 2 often low
- `RELAUNCH=5 SWIPE_MS=3000 SETTLE=90 MAXSTART=99 tools/sunfish-perfab.sh`, kernel155 (xnu 0273), empty env, settings only, 4 boots each, 0 hangs.
- `sweetvsyncgrid=1`: vg1 56.8 50.0 56.7 56.0 57.3; vg2 56.9 44.1 56.9 56.8 53.9; vg3 56.5 43.1 57.7 58.3 56.1; vg4 41.9 44.4 57.1 42.5 56.9: 6 clearly low (under 50) of 20, 1 mild.
- `sweetvsyncahead=4000`: va1 55.9 28.4 57.6 43.8 57.2; va2 56.5 42.1 56.5 57.1 56.9; va3 56.5 57.0 57.1 57.4 57.0; va4 48.1 56.9 57.2 57.4 55.1: 4 clearly low (one at 28.4, p95 98 ms), 3 mild.
- Base (rl, kernel154): 2 clearly low, 4 mild of 20. So both are no better, maybe worse; neither goes in. The gate/jitter guess isn't supported.
- Launch 2 (the first fresh launch after the bench kills Settings) was low in 6 of 8 boots here, 2 of 4 in rl; launch 1 low 2 of 8. Not explained. Next: read-only, compare a low and a good launch from the app's side (Preferences main thread, commits per frame, its display link), and whether a killed app's relaunch differs (caches, `lsd`/`runningboard` lines).

### 2026-11-02 05:24 cs: TU_FETCH_GMEM_MIN_DRAWS on Control Center: about +1 swap/s, not enough boots; stopped on a lost USB link
- `SWIPE_MS=3000 SETTLE=90 MAXSTART=99 tools/sunfish-perfab.sh`, kernel154, metal2-next, cc,home. Control Center swaps/s (p95): default 39.6 (44.7), 40.3 (44.5), 39.0 (45.9); `TU_FETCH_GMEM_MIN_DRAWS=5` 40.5 (41.7), 40.4 (43.9); `=20` 41.2 (40.8), one void (0 swaps, cause not read). Home 53.2-53.5 all. First submit about 1 ms lower with the env. Too few boots for the stop rule; screenshots not read. Far from 45 either way.
- Stopped at csr3 d5: no SSH for 15 min while iOS ran (lock screen on the webcam): the host's usb0 interface had its address but was set down, and the listener only checked the address. Brought up by hand; listener fixed to check the link state too (applies when it restarts).

### 2026-11-02 04:48 Correction to rl: "handlers done -> swap" restates the skipped ticks; CA's render gate (read-only)
- tools/sunfish-timeline.py:81-94 pairs each swap with the last command buffer whose handlers finished before it: the previous frame's. So that stage is "wait for the tick plus the next render": low launches' p95 24-25 ms is a good p50 (7-8 ms) plus one tick. It doesn't locate the skip.
- Read in QuartzCore (research/re/QuartzCore.s): each tick runs vsync_callback, which renders only if tick + period >= the next-update time (server+0x2b0; 0x1882ba354-3a4); a client commit sets next-update = t + period x (N - 0.25) (0x1882cf960-9ec, 0x188501014). SwapEnd doesn't wait for the GPU. From the TL lines: swap 11.9-14.0 ms after the tick, the same in low and good launches; tick interval p1/p99 15.7/17.6 ms, mean 16666.95 us, in every launch: our tick is reported as the wake time (SweetFramebuffer.cpp:494), about +-1 ms of jitter.
- `CA_FORCE_TIMER=1` already turns on the VBL path (`CADeviceUseVBL` = atoi(getenv) != 0): backboardd runs on our kernel tick. `sweetpresentwait=1` does nothing (only 2 is read).
- Guess (about 50 %): each launch's commit time lands at its own offset to the gate's edge, and the tick's jitter turns that into a partial skip rate. No log has the app's commit times.
- Next: RELAUNCH=5, 4 boots each: `sweetvsyncahead=4000` (moves the edge 4 ms, boot arg only); xnu 0273 `sweetvsyncgrid=1` (the tick's deadline instead of the wake time, kernel155). Base tonight: 2 clearly low + 4 mild in 20.

### 2026-11-02 04:39 rl: low Settings runs are per launch; the extra time is Core Animation's wait after the GPU (handlers done -> swap)
- `RELAUNCH=5 SWIPE_MS=3000 SETTLE=90 MAXSTART=99 tools/sunfish-perfab.sh`, kernel154, no QoS, settings only, 4 boots (logs 20261102-040337, -041725, -042245, -043010), Preferences killed and relaunched between launches (pid 277 -> 414 checked), 0 hangs.
- Per boot (swaps/s): rl1 57.0 56.3 55.0 57.5 57.0; rl2 55.9 53.9 57.1 46.8 57.0; rl3 56.6 54.6 56.9 57.4 57.3; rl4 57.4 57.0 57.7 41.5 57.2. Low launches mixed with good ones in 4 of 4 boots; clearly low 2 of 20 (skip2 25, 42 %), milder 4 (skip2 5-7 %, p95 27-31 ms). No pattern in launch order or start temperature (75-80 C both).
- `tools/sunfish-timeline.py --ctl`, p50 / p95 ms, low against good in the same boot: GPU end -> completed 0.64 / 0.82 both; flushSurfaces 1.5-1.7 / 1.8-1.9 both; commit -> GPU start 7.8 / 8.1 both; handlers done -> swap 8.2-9.9 / 23.8-24.8 low, 6.8-6.9 / 11.3-12.9 good. All the extra time is there.
- Gotcha: `SWEET_CTL=/var/local/ctl.txt` writes nothing (backboardd runs as mobile, /var/local is root's); use /var/local/turnip/ctl.txt.
- Guess: the phase between backboardd's render and the display tick is set per launch (backboardd env has CA_FORCE_TIMER=1, a timer display link). Next: read-only, what CA does between handlers and swap and what sets that phase.

### 2026-11-02 04:02 qs: bridge QoS no gain, stays off; without QoS 5 of 5 Settings boots at 56.6-57.2, p95 19.4-21.1
- `SWIPE_MS=3000 SETTLE=90 MAXSTART=99 tools/sunfish-perfab.sh`, kernel154, metal2-next, 5 boots a side, settings,home, 0 hangs, 0 restarts, 0 void (qs0a redone as qs0a_r after the runner's own timeout).
- Settings off 57.2, 56.8, 56.8, 56.8, 56.6, p95 19.4, 19.4, 20.1, 19.6, 21.1: median 56.8, p95 19.6, no low run. `SWEET_BRIDGE_QOS=1`: 57.0, 53.1, 57.0, 41.2, 51.4, p95 median 32.4: 2 low (41.2 with skip2 42 %). Home 53.7 and 52.9. The metal2vulkan threads didn't move to A76 (names not in the log). QoS stays off; whether it causes low runs isn't separable from the base rate in 5 boots.
- Settings without QoS: p95 target met on 4 of 5 boots; swaps/s about 57 against 58 (60 Hz minus the 2-tick share, 3-4 %).

### 2026-11-02 03:22 Low Settings runs: not idle (correction to fg1's lead); fixed per launch, render thread waits (read-only)
- 43 OK Settings runs tonight (kernels 146-154), low under 54 swaps/s: waits were 5-10 s when they happened (wait from `SWEET CMD` timestamps; COOLED only goes to perfab's stdout, line 389); 10 runs waited: 2 low, 8 good; 33 didn't: 14 low, 19 good. The worst runs had no wait (fg1e 42.6, tm3c 43.8, fg0d 45.6, df2 46.0). Idle gap to the window 0 s in all; uptime 108-143 s both. So the start temperature lead was wrong: cpu6 was less busy in low lead-ins, so cooler (guess).
- Fixed for the launch: per-5 s swaps stay low in all three blocks (fg1e 214/217/201) and in the 5 s before; Home right after a low Settings run is normal (lb1b 54.1, tm3c 54.8). Clocks the same (A55 1497, A76 2208, lmh 0). Render thread 12.4-13.4 ms per swap in both, GPU per submit the same.
- backboardd's GPU end -> swap p50 11.6, p95 27 ms in low runs against 9.3, 13.7 in good (fg1e, tm3c vs fg1f, tm3a); submit -> GPU start 2.7 vs 3.9 ms. The render thread waits; cpu6 60-70 % busy (good 76-82 %). Guess (low confidence): a launch locks into a bad phase between the app's commits and backboardd's deadline; app about 4 + render 13 + GPU about 8 ms in a row is longer than 16.7 ms.
- Next: per launch or per boot: 3 boots, Settings 4-5 times each with Preferences killed and reopened (perfab change); `SWEET_CTL` in the same boots for `tools/sunfish-timeline.py --ctl`, to split GPU end -> swap into stages.

### 2026-11-02 03:16 fg1: sweetfgbig doesn't remove the low Settings runs; the low runs may follow an idle wait
- `SWIPE_MS=3000 SETTLE=90 MAXSTART=75 tools/sunfish-perfab.sh`, kernel154, metal2-next (md5 080f97ab, Turnip 0110, bridge 0123), no env, settings only, 6 boots a side, `sweetcpusample=1`. One void (fg0b, G2 `submit failed`, 21.5 swaps/s), redone.
- off 57.4, 54.3, 56.5, 45.6, 50.6, 56.7 (mean 53.5, median 55.4, 2 low); `sweetfgbig=1` 57.0, 56.6, 48.8, 56.8, 42.6, 56.8 (53.1, 56.7, 2 low). The patch was active (29-38 `SWEET SCHED: fgbig` lines per boot), yet Preferences' main thread stayed 80-88 % on A55 in good runs; low runs had a lower A55 share (68-81 %), so A55 time isn't the cause. Good runs p95 19-21 ms: the target on good boots.
- Lead: tonight's 40 OK Settings runs: of 5 with a start temperature under 76 C, 4 were low; of the other 35, 11. Under 76 C means perfab's wait_cool held input to cool. Guess: low runs follow an idle wait, not heat. Read-only check running.
- `tools/sunfish-uisoak.sh d2 900` on kernel154 everyday: 906 s, 96.8 k submits, 0 hangs, 0 restarts, max 91.2 C, 650 MHz.

### 2026-11-02 01:23 df: speed settings are the real defaults now
- tools/sunfish-install-speed.sh: bundle work/metal2-next, Control Center backdrop 0.25, luma boost off, env lines out. Kernel153 (xnu 0271) has sweetgpufreq=6, sweethwcg=1, sweetsmmutcr=3 compiled in.
- `SWIPE_MS=3000 SETTLE=60 tools/sunfish-perfab.sh`, empty mtl2-env, args only sweetse9=2 sweetcampmic=2 sweetswapbig=6 sweetampreset=1, 2 boots, 5 nits, 649-650 MHz, `tcr 802019 actlr 70000000` and `hwcg on` in the log, 0 hangs, 0 void. Settings 55.6 (p95 30.3), 46.0 (33.4, 45 % busy: low run); Home 53.3 (32.6), 52.1 (32.8); Control Center 38.5 (37.6), 38.8 (38.4). Max C 87-92. Screenshots normal.
- `tools/sunfish-uisoak.sh d1 900`, everyday boot: 906 s, 97.9 k submits, G1 0, G2 0, G4 0, 0 restarts, max 93.2 C, 0 caps. With n7: 495 k submits, 0 G1.
- Open: Control Center 38.7 against the target 45; Settings low runs (42-49 swaps/s, about 1 in 6) now the main miss against 58, p95 20 ms; webcam motion check of mode 3 in the morning. Off: `tools/sunfish-install-speed.sh --undo`, `sweetsmmutcr=0 sweethwcg=0 sweetgpufreq=5` on the boot line.

### 2026-11-01 23:19 lb0-lb3: luma boost is it: CA_DISABLE_LUMA_BOOST=1 now the default
- `SWIPE_MS=3000 SETTLE=60 tools/sunfish-perfab.sh`, 2 boots per side, settings,home, 15 s, kernel150 base args, metal2-cpu, mode 3, cache, knobs on, all 8 boots at `SWEET BL: nits 5`, 0 hangs, 0 void, 22:51-23:18. Boots 20261101-225145 to -231505.
- Settings / Home swaps/s and p95, GPU busy on Settings: none 52.4, 29.4 ms / 47.8, 40.0 ms, 83 %. `CA_DISABLE_LUMA_BOOST=1` 51.5 (56.4, 46.7), 27.4 ms / 53.4, 33.0 ms, 52 %. `CA_DISPLAY_COLOR_ACCURACY_FIX_NITS=1` 52.5, 30.3 ms / 48.5, 39.6 ms, 84 %: nothing. Both 56.5, 23.9 ms / 53.2, 33.1 ms, 58 %. lb1b's 46.7 is the known low Settings run (47 % busy, CPU-bound).
- GPU busy per submit, busiest window: about 5.6 -> 3.5 ms, backboardd per submit about 5 -> 3 ms; first Home submit 17.2 -> 14.3 ms. So about 2-3 ms per frame, less than the 7 ms traced. Settings with it matches the 32-nit numbers (57.0, 20.2 ms). All 4 Settings screenshots normal; whether minimum brightness looks different is not checked (user task).
- Heat: hottest CPU 81-88 C on all sides, start temperatures 81-86 C, no difference shown in 15 s windows. A soak is the fair test.
- 2 boots per side, but the gain is over 1 ms and GPU busy moves 30 points, well outside the noise: shipped with an off switch. In tools/sunfish-install-speed.sh; `tools/sunfish-install-caenv.sh --undo` turns it off.

### 2026-11-01 22:41 CoreAnimation's internal nits explain the n4/n5 gap (read-only); luma boost test next
- n4 (kernel144, internal nits 133) against n5 (kernel149, nits 5), first 10 app opens, same order, 650 MHz, no caps, panel DCS 43 in both. Mean over 5 s windows, n4 / n5: backboardd ms per present Settings 8.5 / 14.9, Safari 11.7 / 13.3, Notes 8.7 / 11.7, Calendar 15.5 / 21.8, Photos 9.0 / 12.0; all apps busy 58 / 72 %, GPU sensor mean 63.1 / 69.4 C (n5 started 8 C hotter, so at most about 6 C is the pass).
- The panel isn't it: DCS 43 fast at 133 nits, DCS 239 slow at 5 nits (ni1). The extra pass (st5, 1125x2436, about 7-9 ms) is in the band 5 to 32 nits: slider position 0x1 gives 5.0 nits, 0x1a62 gives 32.8. Nothing logged in between.
- Correction to dd1 (threshold 50-92): at `sweetblmin=50` the kernel saw 50 but CoreBrightness still said `NitsPhysical` 5.0, and Settings stayed 15.4-17.1 ms. So an xnu floor doesn't reach CoreAnimation.
- Why it wouldn't come back on demand: a drag changes `brightness` and DCS only; nits and position are written later (at a backboardd restart, or about 0.2 s after, next to an ALS report, link a guess). Boots since n4 start at position 1, 5 nits, so the 32-nit numbers (Settings 57) won't reproduce until the slider is raised and written.
- Guess, about 60 %: luma boost, QuartzCore begin_update's nits gate (0x1882e6d40; note reads 2-20 nits). iPhone10,3 in boost_devices not confirmed. Test: `tools/sunfish-install-caenv.sh CA_DISABLE_LUMA_BOOST=1`, 2 boots with and 2 without at 5 nits: `SWIPE_MS=3000 tools/sunfish-perfab.sh TAG 2 --secs 15 --screens settings`; backboardd ms per present about 16 -> 8 confirms. If not: `CA_DISPLAY_COLOR_ACCURACY_FIX_NITS=1`.

### 2026-11-01 22:02 Bridge 0117-0120: on by default in source, for after soak n4
- SWEET_U0011..14 now on unless set to `0` (patches edited). `METAL2_VARIANT=next` built 22:01 into work/metal2-next (SweetMetal2 16,408,608 bytes); also has mode 3 and shadow cache as defaults. Not installed or run: goes on the phone only after a clean n4, with tools/sunfish-install-speed.sh.

### 2026-11-01 20:55 fg1: stopped early, too few boots: the user took the phone
- kernel146 (xnu 0265), `sweetcpusample=1` both sides, everyday env, 3 boots usable of the 4 run, stopped by hand after b2 base when the user asked for the phone, 22 k submits, G1 0, void 0. Off / `sweetfgbig=1`: Home 51.6 (49.4-53.8) / 53.1; Settings 55.8 (54.3-57.4) / 56.1, p95 25.6 / 21.1, late % 5.5 / 4.3. 2 boots against 1: says nothing. No low Settings run came up. `SWEET SCHED: fg` lines not read yet: they're in the boot logs named in work/perfab/fg1/results.tsv. Rerun in full.

### 2026-11-01 20:45 ph1: pass hash: nothing repeats, so pass reuse is parked
- kernel139, bundle work/metal2-passhash (bridge 0121, 0122), `SWEET_PASSHASH=1`, 1 boot, `SWIPE_MS=3000`, 0 void: Control Center 38.1, Home 53.0, Settings 54.5 swaps/s with hashing on. research/notes/ph1-passhash.txt; screens split by line order and pass rate, so an estimate: only 3 windows for Control Center.
- 1125x2436: Control Center 32-99 draws: 229 passes, same-as-prev 0, same-last8 0, same-content-last8 0, same-without-buffers 6; loads 221; first-diff pso 94, state 108, buffer contents 23, counts 4; hash 237 us, 29 KB. No 100+ class there, though cc3's trace counts 133-144 draws: the two count differently, not understood. Home 32-99: 1055 passes, same-as-prev 0, same-content-last8 2. Settings 100+: 1230 passes, same-as-prev 0, same-content-last8 4.
- Limits: there's no frame signal, so "previous" means the previous pass of that size and draw class, and first-diff compares different passes in a frame (hence pso and state); same-last8 covers that and is still 0. Ring-buffer ranges could cause false misses. So the 10-15 ms guess in cc3 has no support; a full-pass reuse is parked. Only a per-draw prefix compare could say more, and it's not worth doing while Control Center's frames can't be matched to a pass.

### 2026-11-01 20:10 Low Settings runs are real: the app misses ticks (read-only)
- 48 windows at `SWIPE_MS=3000` in hw2, hw1, hw1c, gd2, cp1, mf4: 8 below 50 swaps/s, 5 at 50-54, 35 at 54+: a continuum. Tick stays 60.1/s. 2-tick intervals: 37-42 % at 42-43.5 swaps/s, 22-26 % at 47-49, 4-6 % normally. No stalls (0-1 gaps over 100 ms either way), through the whole window, and already low in the swipes before it. Up and down the same.
- Not the list or swipes: 9 low and 9 normal screenshots follow the same positions, none at the end of the list; swipe cycles and swap counts per swipe alike (swipes are seen only at kernel injection). Not backboardd: it submits nothing on a skipped tick, tick-to-submit and GPU end are the same, render CPU per swap the same. Not boot leftovers: uptimes overlap, and a line-by-line log diff found nothing that replicates. Home and Control Center run first in the same boots and look normal.
- Guess: Settings' commit arrives late, e.g. main thread on an A55; per launch or per boot unknown. No log has that thread's data. Best window in 6 tags is 57.8, so the 58 target is at this gesture's limit: normal runs dip at each swipe start (about 53, then 59).
- Next: xnu 0265 `sweetfgbig`, frontmost app's main thread on the A76s with per-cluster CPU in the log; compare skip share, not the mean. Bench: add a 2-tick % column, report median and low count, reset `UP=0` each boot (never reset at perfab.sh:242, so with two configs one side always starts the same way; pooled no effect, 54.0 / 54.3, still a confound). Scripts: research/notes/runs.py, h.py, corr.py.

### 2026-11-01 20:00 hw2: clock gating and scratch trim: no speed gain, both stay off
- `SWIPE_MS=3000 tools/sunfish-perfab.sh hw2 5 --secs 15 --screens home,cc,settings`, kernel144, bundle work/metal2-scratch (bridge 0121), 650 MHz, everyday env, 15 boots, 143 k submits, G1 1 (scr b4, boot 20261101-193946), G2 0, G4 0, 4 void rows (off b4 Control Center too hot; the 3 of scr b4). off / `sweethwcg=1` / `SWEET_M2V_SCRATCH=1`: Home 47.0 (26.0-54.6) / 52.6 (50.4-53.7) / 52.5 (49.8-54.0), and off without its one low boot (b1) is 48.5-54.6; Control Center 38.6 / 38.1 / 38.9, p95 36.2 / 36.9 / 37.1; Settings 55.0 (51.1-57.1) / 52.2 (42.3-56.8) / 53.2 (41.9-57.0), p95 26.6 / 25.6 / 23.1. Max C Home 91.4 / 89.4 / 91.6, Control Center 94.2 / 92.7 / 93.8, Settings 88.3 / 88.2 / 87.9.
- hw1's Settings 56.9 was noise, as feared: `on` got 2 low boots here. Clock gating: no speed gain, maybe 0-2 C at peak, not shown. Idle check next, since it costs nothing while on, if it stays clean.
- Scratch trim: about 75 % of backboardd's passes trimmed (25330 of 32768), kept-read 0, drawn correctly (webcam frame and screenshots), yet no speed change. So halving the bins doesn't save the 1-3 ms guessed: per-bin cost was a tracing artefact, as cc3 warned. Off; 0121 stays as a knob. The one G1 was on this side: too few to read.
- Low boots show on every side and cost more than any knob here: Settings 42.3, 41.9, 49.0, Home 26.0, with no config in common. Being analysed.
- Home's low boots are the bench, not the phone: both 26-swap Home runs with a screenshot (gd2 g6 b1, hw2 off b1) are on the widgets page; a normal one (hw2 hwcg b1, 52) is on the icon page. Screenshots exist only for b1, so cp1 b4's 32 and 36 aren't checked. So off's real Home here is about 52, the same as the other two, and gd1 and gd2's "low Home" notes should be read the same way. Widgets page itself draws at 26 swaps/s: a real slow screen, not yet looked at. The bench's Home lead-in has to go to the icon page first (fix once the running bench is done: bash reads a script as it runs).

### 2026-11-01 19:20 cc3: Home is redrawn at full size behind Control Center on every moving frame
- One boot, kernel139, bundle metal2-cpu, 650 MHz, `MESA_GPU_TRACES=print`, boot 20261101-185417, 14.1 swaps/s with tracing, 5 cycles, 168 frames, parsed by research/notes/cc3*.py. Full-size passes are 93 % of pass time, 1.9-4.8 per frame, all 44 bins of 288x224. Traced ms per frame: open 54.0, drag down 45.4, drag up 51.5, close 30.7. Main pass (133-144 draws) costs 25-28 ms traced in every moving phase: about 18 ms untraced by a fitted x0.75 (guess). The blur chain is small: 284x611 pair 0.4-3 ms. No full-size capture pass remains after quarter size. At rest the frame is only 2 full-size passes, 12 ms: CA can skip the main pass there.
- Roles from bin maps and a screenshot, guesses: main = Home; full-size 1-draw pass = blur composite; 59-79 draws = module tiles; 14-16 draws = overlay; 2 draws = home indicator (2 bins of real work). No-op passes (0 draws, sysmem) 4.6 per open frame, 20.6 ms in all.
- Colour 1 is in tile memory on all 599 full-size passes (cleared, never loaded or stored). Scratch trim would make 22 bins of 576x224 if it qualifies, but tracing costs about 61 us per bin here, so the gain can't be read from this trace.
- Next, guesses: only reusing the main pass reaches 6 ms alone (10-15 ms net of a copy). First a logging-only bridge patch, `SWEET_PASSHASH=1`, to see whether its draws repeat from frame to frame. Then low-draw full-size passes in sysmem (1-3 ms, could be negative), no load on the 1-draw pass if opaque (about 0.7 ms). Drag down is 38 % of traced pass time and rare in real use: add an open-and-close bench screen.

### 2026-11-01 18:20 Control Center: the bench needs 6 ms off each heavy frame, and its biggest cost was never traced (read-only)
- From gd2 g6 (kernel139, 650 MHz, 32 nits, after the quarter-size backdrop), kernel spans, untraced: 72 % of frames carry 93 % of the GPU time. ms per average frame: spans of 21-27 ms 9.8, spans of 12-18 ms 8.8, present blit 2.5, the rest about 5. Over a gesture, GPU ms: start of open 16, opening 29, margin drag 26, closing 29, Home 7 (phases are guesses from roughly aligned clocks). 45 swaps/s needs about 22.9 ms where the bench now spends 29.6.
- No trace covers the bench's margin drag: cc1 and cc2 touched elsewhere, which is why cc1 read 9 ms. Quarter-size removed the 51 ms frames but not the single-pass (30,) frames, so it helped open and close, not the drag. About 40 % of bench time is that drag, which real use rarely has: guess that a separate open-and-close screen judges feel better.
- `SWEET_MTLOW` was never built: a proposal only, in no source or binary. Correction to the plan. Not worth a run unless a trace shows radius-driven passes remain.
- Next, savings are guesses: one traced boot of the bench gesture with `+MESA_GPU_TRACES=print` alone; `blurRadius` 0 in modulesBackground (3-6 ms, loses the blur, so only if nothing else does it); `CA_ENABLE_MEMORYLESS=0` (1-3 ms, could be worse); port redfin 0111 scratch trim, `SWEET_M2V_SCRATCH=1` (1-3 ms, all screens; being ported); redfin 0109+0110 `SWEET_M2V_COLOR_WRITES=fetch` (0.5-2 ms). Analysis scripts: research/notes/cccycle.py, shapes.py.

### 2026-11-01 17:40 Correction to st3: its screen checks were framebuffer reads, not the panel
- Late correction from the agent that ran st3, checked against boot 20261101-024953 only. The 13 "fine" screens come from flame-shot, which reads the scanout memory (0x9c000000), so they show what the GPU wrote, not what the panel showed, and would look the same with the panel off.
- Only 2 webcam montages exist, research/st3/motion.png and motion2.png. motion.png's first row is black, most likely panel off (`SWEET FB: display on` at 39.460 and 676.262 s, timing inferred): void. Of the 10 frames st3 counted, those are not evidence.
- Not supported: "lock step not done" (the panel did go off and wake at 676 s, cause not read); "no new crash files" (the newest folder, Assistant, wasn't opened). Not checked: backboardd restarts, so st3 isn't shown to be a clean run; 0 hangs and 0 submit errors stand. Control Center's 13.2 swaps/s: void, as st3 already said. No webcam frame after boot, against the rule. The st3 agent also ran background jobs against its spec; none left running.
- Effect: mode 3's survey is weaker than st3 says. Stale strips on real panel output aren't cleared by it beyond the remaining webcam frames; U22 stays open and doesn't gate the default. Tonight's soak is still the check for hangs. A webcam motion check of mode 3 goes in before it becomes default.

### 2026-11-01 07:30 xnu 0259 sunfish-ddmin: written, not built, not run
- `iokit/Kernel/arm/SweetBacklight.cpp`, to test whether the published minimums change the 7 ms second pass at 2-5 nits. Boot-args, absent = 2 as before: `sweetddmin=N` (`digital-dimming-min-nits`), `sweetblmin=N` (MinNits, HardwareAccessibleMinNits, DisplayPanelLuminanceMin, DisplayProductLuminanceMin, lMinPanel, lMinProduct). DCS mapping unchanged. Guess, unverified: CoreBrightness's flag at 0x1af0d985c compares [0x55c] (maybe the hardware minimum) > digital-dimming-min-nits.

### 2026-11-01 13:45 gd2: GPU 650 MHz on the full setup: Control Center 36.1 -> 38.4 swaps/s, p95 42.7 -> 36.0
- `SWIPE_MS=3000 tools/sunfish-perfab.sh gd2 5 --secs 15 --screens home,cc,settings`, kernel139 sweetrowa55=6, SWEET_DEADFILL=3, M2V_SHADOW_CACHE=1, SWEET_U0011..14, 10 boots, 92 k submits, G1 1 (565 MHz side), 3 void. 565 / 650 MHz: Home 46.3 (43.0-52.7) / 47.3 (25.9-53.4), Home ran first after boot, one boot low on each side; Control Center 36.1 / 38.4, frame 27.7 / 26.0, p95 42.7 / 36.0, max 93.8 / 94.1 C; Settings 54.6 (47.2-57.8) / 55.7 (52.1-57.2). No Settings penalty this time: gd1's bad boot looks like a fluke, and low boots show on both sides. Going into tonight's soak as sweetgpufreq=6; if the soak shows more hangs, it comes out first.

### 2026-11-01 13:00 Correction: those gaps look like a tracing artefact
- With tracing on, a kept (reusable) command buffer gets a timestamp-copy stream after it in `tu_queue.cc`: WFI, WAIT_FOR_ME, 4 KB CP_MEMCPY per chunk, WFI. In the same trace 167 of 290 work-to-barrier gaps with a one-time barrier are at most 0.26 us; the kept barrier never goes under 100 us (0 of 1113). So the "12 ms per frame in gaps" isn't confirmed for untraced runs. The barrier (`submit.rs` after_completed, to_host, tracker-planned) can't fold: encoder buffers end before the submit plan. Boundary emits CCU clean, tu_init_hw (invalidate, WFI), and the kernel CCU invalidates and CACHE_FLUSH_TS: a barrier buffer takes 3 us with all that, nothing redundant. Nothing changed.

### 2026-11-01 12:40 Control Center gaps between submits: not CPU, merging won't help
- Read-only: a slow frame has 2 boundaries per buffer (work, 3 us barrier command buffer, next work), each 0.29-1.05 ms, constant within a frame, whatever the pass size. Kernel stamps show 1 tick between submits, and in one run 1081 of 1229 submits queued before the prior one ended: the stall is at the GPU boundary. Work-to-barrier and barrier-to-work both about 738 us p50, though only the second crosses kernel packets, so merging keeps the gap. Cause unknown. Idea: record the barrier in the work command buffer: could halve it, about 6 ms per slow frame; untested.

### 2026-11-01 12:10 cp1: bridge CPU knobs (0117-0120): Settings 53.9 -> 57.0 swaps/s, p95 29.0 -> 20.2 ms
- `SWIPE_MS=3000 tools/sunfish-perfab.sh cp1 5 --secs 15 --screens home,settings`, kernel139 sweetrowa55=6, SWEET_DEADFILL=3, M2V_SHADOW_CACHE=1, bundle work/metal2-cpu, SWEET_U0011..14=1 on one side, nits 32, 10 boots, 68 k submits, 0 GPU bugs, 0 void. Settings 53.9 (47.6-56.6) / 57.0 (56.5-57.7), frame 18.5 / 17.6, p95 29.0 / 20.2, <=17 ms 65.8 / 81.7 %, 85 / 84.5 C. Home 47.9 (32.5-53.0) / 47.7 (36.0-53.0), flat, GPU-bound; a low boot on each side unexplained. Steadier as well as faster. Knobs on for the soak.

### 2026-11-01 11:20 sw3: Safari holds 60 swaps/s with long swipes; Settings 51 and hot when first after boot
- `SWIPE_MS=3000 tools/sunfish-perfab.sh sw3 4 --secs 15 --screens settings,safari`, kernel139 sweetrowa55=6, SWEET_DEADFILL=3, M2V_SHADOW_CACHE=1, nits 32, 4 boots, 31 k submits, 0 GPU bugs. Safari 60.1 (60.1-60.1), p50 16.7, p95 17.8, <=17 ms 88 %, GPU 52 %, 85 C. Settings 51.3 (46.3-54.6), p50 17.7, p95 29.8, <=17 ms 49 %, GPU 58 %, max 88.7-94.1 C, A76 min 2171. Settings ran first, about 30 s after boot, when boot load has already heated the phone; heat and CPU, not GPU, limit it now.

### 2026-11-01 10:15 lo0-lo3, a76r: low-brightness pass not reproduced; cull adds little at normal brightness
- a76r, kernel139, SWEET_CULL=2 on, 12 boots, 109 k submits, G1 1: A76 2208 / 2169 / 1939 MHz: Home 52.5 / 53.2 / 52.3, Control Center 36.8 / 36.9 / 36.7, Settings 49.2 / 50.9 / 48.2, Home max 85.3 / 87.8 / 82.7 C, Settings <=17 ms 55 / 56 / 43 %. A76 stays 2208 MHz; 1939 is about 4 C cooler if heat comes first.
- lo0-lo3, no cull: slider dragged down, but `SWEET BL: nits` stays 32 after boot (DCS 3x-6x), never 5, so the slow state didn't come back; how it came about is open (possibly the DCS 128 to 33 path overnight). At 32 nits: Settings 47.3-52.2, GPU 56-59 %; Home 51.5-52.2; Control Center 35.3-36.1. CA_DISPLAY_COLOR_ACCURACY_FIX_NITS=1 and CA_DISABLE_LUMA_BOOST=1 show no effect here, not a test of the 5-nit state. Both undone. So the switch point is between 5 and 32 nits by these runs; p3 clamp at 80 nits (research note) is not it. dd1's 50 nits stayed slow but had its range changed.
- Cull costs 0.6 swaps/s on Home here (51.9 against 52.5), off the everyday setup: env is now SWEET_DEADFILL=3 and M2V_SHADOW_CACHE=1. Its 42.5 -> 49.2 was in the 5-nit state only.

### 2026-11-01 08:25 ni1, br1: speed follows iOS's internal nits, not the panel: Home 52.3, Control Center 36.8 at 119-148 nits
- ni1, kernel139, same boot, slider dragged in steps: panel DCS rose to 239 but `SWEET BL: nits` stayed 5 and Settings stayed 46.7-51.9 swaps/s, GPU 2 submits of 7.7 ms per swap. So the panel brightness isn't the trigger. The last step had a backboardd restart at 245 s (then 16 swaps/s, avg copy 5961 us): void; cause not read. After it nits 119.
- br1: `tools/sunfish-perfab.sh br1 3 --secs 15 --screens home,cc,settings`, reboots picked up nits 119-148, DCS 146-173. Home 52.3 (50.7-53.1), frame 19.1 ms, p50 17.4, p95 32.8, 1st 14.2, GPU 87 %; Control Center 36.8 (36.7-36.8), 27.2 ms, p95 41.0, 1st 18.5; Settings 49.7 (49.0-50.3), p50 17.2, p95 33.3, GPU 54 %. 26 k submits, G1 1 (b2), 1 void. Against minimum: Home 49.6, Control Center 32.9, Settings GPU 84 %.
- Settings now has GPU to spare (54 %), p50 at the tick: its limit is elsewhere, and the bench's swipe gaps cap it (about 57). Cause of the low-nits pass still unknown.

### 2026-11-01 07:47 dd1: nits properties don't remove the second pass
- kernel140 (xnu 0259), Settings first after boot, everyday env, 2 boots each, 28 k submits, 0 GPU bugs: base 44.8 swaps/s, p50 21.3; sweetddmin=0 45.6, 20.8; sweetddmin=500 46.8, 20.5 (1 boot, the other 0 swaps); sweetblmin=50 46.1, 21.0 (brightness went to 50 nits). All within noise, so these don't control it, and at 50 nits it's still slow. Threshold between 50 and 92 nits if nits is the trigger. Needs slider tests above 50 nits, against the night rule: morning.

### 2026-11-01 07:35 st5: at minimum brightness each frame has two full-size passes
- MESA_GPU_TRACES=print, Settings long swipes, kernel139, modes on, 5 nits, boot of 07:27, no crash, research/notes/st5-settings-trace.txt. 648 passes of 1125x2436, 2 per moving frame: 9.5 then 8.3 ms traced, 44 bins, about 7.91 mean with idle frames. Copies 0.01 ms per frame. So the 7 ms second submit is a whole second full-screen pass that iOS adds at low brightness. Which code adds it isn't known yet.

### 2026-11-01 07:40 Settings' limit at low brightness: a second submit of 7 ms; G3 may be this
- Read-only, logs since Oct 30 22:00. Second-submit p50 by nits: 2 nits 6.66 ms, 5 nits 7.02 (82 k pairs), 92 nits 0.57, 101 nits 0.60, 160 nits 0.59. Same boot, 92 then 2 nits (20261031-001444): 0.57 then 5.59. So below some brightness iOS or the bridge adds a pass; this fits Home 38 against 31 (nd1). Mechanism unknown.
- Stages, Settings, cs1 b1 (5 nits): 48.9 swaps/s, p50 19.45, 1st + 2nd submit 8.1 + 7.0, present 2.65, GPU 17.7 per frame, so GPU-bound. sh1 s1 b1 (101 nits): 56.4, p50 16.70, p95 18.7, 7.9 + 0.56, 2.56, GPU 11.1, idle 5.6.
- G3 ("slower mode after a backboardd restart", 2nd submit 6 ms slower) lines up with the same drop: possibly not a restart effect at all. Unverified.
- Bench caveat: 16 swipes in 15 s leave 16 gaps of about 65 ms, so Settings tops out near 57 swaps/s on this bench (estimate): the 58 target can't show there. Judge by p50 and p95, or lengthen swipes.
- Tail: 17 % of cs1 frames exceed 22 ms with the same GPU time and 13 ms idle before submit: cause not observed; p95 23-32 at 101 nits too in rows2.
