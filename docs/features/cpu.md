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

### 2026-11-02 15:55 dfs: 15-min soak on today's defaults: max 81.6 C, mean 69.6, none at 90+; 1 G4 with a backboardd restart
- kernel158 (vsync phase 5000, A76 1939) everyday args, mtl2-env SWEET_U0011..14=1 TU_FETCH_GMEM_MIN_DRAWS=20 SWEET_SURF_FLUSH=1, original adspboot (boot 20261102-130815 after the 13:06 restore; the runner read the previous boot's sensors.log line), 4455 mV. `tools/sunfish-uisoak.sh dfsa 450` then `dfsb 450`.
- dfsa void: 1 G4 and backboardd 60 -> 362 at 57 s (killed after recovery by design), 47.8 k submits. dfsb clean: 50.1 k submits, 0 G1/G2/G4.
- Per app pooled (max / mean C, swaps/s): Settings 76.1 / 71.7, 52.0; Safari 76.7 / 71.8, 51.8; Photos 81.6 / 73.9, 45.3; Maps 75.1 / 69.7, 30.2; Notes 72.8 / 65.3, 32.8; Calendar 70.3 / 66.5, 33.7 (Calendar shows its "What's New" sheet). Overall max 81.6, mean 69.6, 0 of 173 at 90+.
- Against hB (A76 1939 alone, kernel157): max 87.5 -> 81.6, mean 77.5 -> 69.6, swaps/s within ~1. The start temperature and room aren't controlled, so the extra 6-8 C isn't attributed to SWEET_SURF_FLUSH=1 or kernel158 alone.

### 2026-11-02 15:20 idN3: adspboot's CPU is per-pass work, not the poll rate: idle-poll change reverted, parked (O18)
- adspboot id3 in its idle mode, kernel158 everyday, boot 12:58:56, log fetched in time (work/heat/idN3-sensors.log). Its 60 s lines after setup: 641-711 passes, 10 ms sleeps 100 %, cpu 4173-4604 ms. Fast poll (F2): 1136-1244 passes at 2 ms, 3495-3810 ms. So a pass takes 50-90 ms of wall time and 3-6 ms of CPU whatever the sleep: the cost is inside the pass (blocking reads or per-pass work), not the loop rate. Sleeping longer only moved the same work into fewer, longer passes.
- Sensors start on the third try (`sns: motion: 0 sensors`, again in 10 s, twice, then `8 sensors`); accel, ALS and pressure stream, gyro and compass off at idle.
- Original binary restored on the phone (/var/local/bin/adspboot.id3 kept), flag removed; source changes reverted (5c59813, fbece50). Backlog O18: profile adspboot's pass (pstack) to find the 3-6 ms; worth ~3-4 % of a core screen on.

### 2026-11-02 15:00 id3: same-binary fast/slow poll A/B mostly lost; fast poll costs 3.7 s CPU per minute; motion sensors weren't streaming
- adspboot id3 (fbece50), kernel158 everyday, 4 boots N1 F1 N2 F2 (F = /var/local/adspboot.fastpoll), 300 s windows. sensors.log is rewritten every boot and was fetched only at the end: N1, F1, N2 stats lost. N2 and F2 windows weren't at Home (Settings open / lock screen), so only N1-F1 and N2-F2 pair up.
- psx adspboot CPU s: N1 22.9, F1 23.4, N2 17.8, F2 18.6 (side means 20.4 / 21.0): no difference. a55 mean 55.6 / 54.6 / 53.0 / 52.8 C.
- F2's own lines: 1136-1244 passes and 3495-3810 ms CPU per 60 s, all at 2 ms (fastpoll). `sensd: accel: the DSP has no "accel" sensor` (same for gyro): no motion stream was up. Guess: in N mode the "sensors not up" rule (2 ms until --sns-motion sensors start) never clears, so N polls at 2 ms too. One N boot with the log fetched in time is running.

### 2026-11-02 14:05 id2: adspboot idle2 made idle CPU worse (23.4 against 13.7 CPU s per 300 s): reverted on the phone
- kernel158 everyday + SWEET_SURF_FLUSH=1 + adspboot idle2 (5c59813), boot 20261102-120558, 4454 mV; same procedure as idle1 (Settings opened and closed, window from 3.5 min after boot, 300 s at Home screen on). Log: `adsp: poll: sensor rate, 2-10 ms, when idle; 2 ms while a reply is due; 20 ms blanked`; `SWEET SURF FLUSH: mode 1`.
- CPU s in 300 s (id2 / idle1): adspboot 23.4 / 13.7, backboardd 22.3 / 23.2, mediaserverd 12.4 / 12.2, PosterBoard 12.3 / 12.1, total 131.5 / 125.4. a55 mean 57.6 / 59.8 C (max 68.0); busy A55 8.4-3.0 %, A76 8.7, 6.8 (same as idle1). Sensors still stream (accel, ALS, pressure counts rising).
- Sound start not checked: the EM-01 mic card wasn't found (`pactl: No such entity`); `apr_tone` while adspboot runs gives `apr: no OPEN_ACK in 3 s` (one ADSP owner). One no-argument apr_tone run drove the amps for ~2 s by mistake.
- Old binary restored (/var/local/bin/adspboot.idle2 kept); the implementer is diagnosing (guess: the "in flight" test is always true, plus more work per pass). The flush default stays.

### 2026-11-02 13:05 io: idle-on CPU: adspboot polls every 2 ms; Home's 10 frames/s are the Clock icon's second hand (read-only)
- research/notes/idle-on-2026-11-02.md, boot 20261102-110735, idle1 on window.
- adspboot: edge_loop (phone/wifi/adspboot.c:3301-3350) polls glink rx, rpc, the audio FIFO, apr, amp and the sensors each pass, then `usleep(sns_dark() && !(audio active) ? 20000 : 2000)`: 500 passes/s with the screen on (45.6 ms/s CPU), 50/s off (8.7 ms/s). backboardd keeps gyro at 200 Hz and accel at 50 Hz after an app open. Fix being written: poll at the shortest active sensor interval clamped to 2-10 ms unless audio or a reply is in flight; off switch kept.
- Home's 10 swaps/s are real: `sweet_fb_swap_count` counts only client swaps (SweetFramebuffer.cpp:1040, :1060). backboardd turns vsync notifications on every 100.0 ms; SpringBoardHome's SBHClockApplicationIconImageView runs a CADisplayLink at CAFrameRateRangeMake(10, 120, 10) (0x1c5325118, 0x1c52b13b8) for the second hand. A real iPhone ticks too but redraws only the icon; ours costs ~7.7 ms of backboardd CPU and a full-screen GPU frame (2 submits) per tick. Backlog: dirty-rect presents (O17).
- Wallpaper posters at idle: PosterBoard 12.1, CollectionsPoster 4.5, EmojiPoster 3.2, WeatherPoster 1.5, AegirPoster 1.5 CPU s per 300 s, partly a refresh burst at 457-469 s. mediaserverd's 12.2 s unexplained.

### 2026-11-02 12:50 idle1: idle on kernel158: screen off 52.5 C as before; screen on 59.8 C, Home presents 10 frames/s, adspboot and PosterBoard busy
- Boot ev158 (kernel158 everyday, 11:07), 300 s each at Home: screen on from 3.3 min after boot (includes settling), screen off (power key `flame-cmd.py key 0 150`; `H` isn't a flame-cmd command).
- Screen on: a55 mean 59.8 C (max 72.8, single spikes; trend 60.9 -> 58.7), gpu 60.2, ddr 58.8; busy A55 8.2-3.0 %, A76 8.8, 7.2; 1497 / 1939 MHz flat; `SWEET FB: 5s` 50 swaps per 5 s (10 per s) with Home still; GPU ~101 submits per 5 s. CPU s in 300 s: backboardd 23.2, adspboot 13.7, mediaserverd 12.2, PosterBoard 12.1, kernel_task 6.2, coreduetd 5.9, SpringBoard 5.5, runningboardd 5.5, CollectionsPoster 4.5, bluetoothd 4.3 (total 125 s).
- Screen off: a55 52.5 C falling to 50.9; busy 1-2 %; no FB lines; top wland 2.8, adspboot 2.6, dasd 2.0, healthd 1.9 (total 22.7 s). Matches 2026-11-01 07:17 (54.1 C).
- Leads for idle-on heat: something presents Home at 10 frames/s (PosterBoard / CollectionsPoster wallpaper, guess); adspboot 2.7 % of a core screen on (our own tool); mediaserverd 4 %. backboardd 279 s/h against 40.7 on 2026-11-01.

### 2026-11-02 11:55 hA/hB: A76 at 1939 MHz: 6 C cooler at the peak, no 90 C readings, speed within noise; default in kernel158
- kernel157 everyday args, 4 boots alternating, `tools/sunfish-uisoak.sh TAG 450` 60 s after SSH; capped boots add `sweetrowa76=10` (A76 real MHz 1939 in the THERM lines; default 2207-2208, one dip to 2190). 4454 mV, 0 G1/G2/G4, 0 restarts, 0 caps, ~50.5 k submits each.
- Per boot max / mean C, readings at 90+ of 90: hA1 92.8 / 78.8, 3; hB1 86.1 / 76.2, 0; hA2 94.5 / 83.1, 7; hB2 89.0 / 78.7, 0. The two boots of a side differ by 2-5 C; capped was cooler in both pairs (-2.6 and -4.4 mean, -6.7 and -5.5 max).
- Per app (2-boot means), default -> capped: max C Photos 93.7 -> 87.5, Safari 90.1 -> 84.1, Maps 87.5 -> 82.0, Settings 86.4 -> 82.1, Notes 84.5 -> 81.5, Calendar 83.0 -> 79.2; swaps/s Settings 52.6 -> 52.5, Safari 57.0 -> 56.8, Photos 45.7 -> 45.0, Notes 33.6 -> 32.5, Calendar 35.8 -> 33.1, Maps 27.0 -> 27.8.
- Default: xnu 0276, A76 boot row 10 (1939 MHz) unless sweetrowa76 or flamecpufreq is given; off: `sweetrowa76=12` (kernel158). Calendar's -2.8 swaps/s is the one cost to watch (2 boots).

### 2026-11-02 11:20 pax: Photos' steady CPU is its own layout and render, no accessibility client (read-only)
- vt0 stacks symbolized (research/notes/photos-ax-2026-11-02.md), 51 running of 100: main thread layout from scrollViewDidScroll 18 %, PXGViewRenderer/texture streaming/MTKView draw 16 %, PXGBasicAXGroup upkeep (axContentInfoAtSpriteIndex) 8 %, UIKit touch/animation 16 %, unattributed objc/malloc 22 %; workers 18 %: PXBrowserSummaryController title requests, capitalising the date title builds a new ICU word tokenizer each time (px_firstLetterRange -> enumerateSubstringsInRange 0x403 -> NLLangid -> icu::RuleBasedTokenizer::init).
- The AX group is PhotosUICore's normal path (PXGLayout init creates it, visibleRectDidChange invalidates it every frame; 0x1a5084b14, 0x1a5046060, 0x1a501e760); accessibility on would only add leaf features. No AX image in any of 2131 thread stacks; com.apple.Accessibility.plist (2026-10-30 audit) has ApplicationAccessibilityEnabled 0, AutomationEnabled 0.
- No key turns these off. So Photos' heat is app work on the main thread and workers; the lever left is CPU clocks (A76 cap for the hot peaks: next, a soak A/B with sweetrowa76=10, 1939 MHz).

### 2026-11-02 11:00 vt: allowPixelTransferGraph crashes Photos; with cached thumbnails Photos' CPU is layout and accessibility, not scaling
- Running boot mlend (kernel157 everyday, 20261102-100118). vt0 (no keys): 60 s grid scroll: 37.1 swaps/s; CPU s MobileSlideShow 46.8, mediaserverd 16.3, backboardd 60.2 (trustd 36.3 and accessoryupdater 22.3 in the same delta: boot-time work, gone at idle 10 min later); A55 busy 47.5 %, cpu6 73.2, cpu7 40.0; max cpu6 89.6 C (start 83-85, Photos had just opened); GPU busy 48.2 %. Thumbnails drawn.
- vt0 stacks (100 samples, 51 running): 0 % vImage, 0 % Metal transfer. Running leaves: objc_msgSend, PhotosUICore PXGLayout accessibility (axContentInfoAtSpriteIndex), malloc, ICU tokenizer. So once thumbnails are cached, the vImage cost (phs, 57 % at the start of a scroll) is gone and Photos still burns ~47 CPU s/min elsewhere.
- vt1 (`tools/sunfish-vt-prefs.sh allowPixelTransferGraph=true allowPixelTransferFallbacks=true`): Photos crashed at launch, sig 11 in _lookUpImpOrForward from _VTPixelTransferGraphBuildChain+2264 <- _vtPixelTransferSession_BuildChain <- CMPhoto __convertAndScalePixelBuffer. Void; vt2 not run. Keys removed (--undo); backup .bak-vt on the phone.
- Idle 10 min later, 30 s: healthappd 4.9 s, adspboot 2.5, backboardd 2.1, coreduetd 1.6, dasd 1.0.
- Next: why PXGLayout computes accessibility info while scrolling (an AX client active? read-only).

### 2026-11-02 10:40 vts: VideoToolbox's transfer knobs: six booleans in com.apple.coremedia; the graph path may route scaling to Metal (read-only)
- research/notes/vt-scaling-2026-11-02.md. CMPhoto reads no preferences. VideoToolbox reads from com.apple.coremedia: allowSoftwareTransferSession, allowMetalTransferSession, allowHardwareTransferSession (default 1), allowPixelTransferChain / Graph / Fallbacks (default 0), asyncpixeltransfer.
- Default path: Metal (VTMetalTransferSession) is used only if the image isn't scaled or the session sets AllowLowQualityScaling (session property, CMPhoto never sets it; 0x194ec7834). Photos' thumbnails are scaled, so they stay in vImage: fits the stacks (0 Metal frames).
- The graph builder (VTPixelTransferGraphBuildChain, allowPixelTransferGraph) can pick a Metal step with high-quality scaling (0x194f61610, guess); allowPixelTransferFallbacks returns to the default path if the build fails (0x194ec6b24).
- Test: `tools/sunfish-vt-prefs.sh allowPixelTransferGraph=true allowPixelTransferFallbacks=true`, then Photos CPU s/min, pstack for VTMetalTransferSession frames, webcam for blank thumbnails. About 35 % that work moves to Metal; up to ~25 CPU s/min if it does.

### 2026-11-02 10:20 phs: Photos' CPU is software colour conversion and scaling (no M2 scaler driver), read-only
- 200 pstack samples each (work/heat/), MobileSlideShow over 4.3 s at the start of a grid scroll, mediaserverd 3.0 s two minutes later; symbolized (research/notes/photos-cpu-2026-11-02.md).
- MobileSlideShow, 368 running samples of 8032 (1.84 threads busy per sample, in bursts): VideoToolbox software transfer 57.1 % (vVertical_Shear_ARGB_8888 27.2, vConvert_420Yp8_CbCr8ToARGB8888 15.8, vHorizontal_Shear 6.2: VTPixelTransferSessionTransferImage -> vImageScale, spread with dispatch_apply), main thread UI 19.6, allocation/CF/objc 17.7, CGImageSourceCreateThumbnailAtIndex resample 4.9, HEIF parsing 0.8. No retries or failure paths.
- On the iPhone X that transfer runs on AppleM2ScalerCSCDriver (the "scaler" node in the device tree); we have no such driver, so VideoToolbox does it in vImage. HEVC tile decode goes over XPC to mediaserverd (VTDecompressionSessionRemote_DecodeTile).
- mediaserverd's sample had 0 running threads (taken outside a decode burst): its 17 CPU s/min is unexplained; guess (70 %) software HEVC (VCPHEVC; no AVD).
- Fix candidates (guesses): a cheaper software scaling setting in VideoToolbox/CMPhoto (AllowLowQualityScaling, HardwareAndSoftwareConversion, AccelerationMode; -10 to -15 CPU s/min, 40 %); thumbnail size A/B; a GPU-backed stand-in for the M2 scaler (days; ~25 CPU s/min); HEVC on GPU/DSP (too big). Next: read whether those names are preference keys (investigator); confirm mediaserverd with samples during a scroll.

### 2026-11-02 10:00 ph2: Photos' heat is CPU: the app +31 CPU s and mediaserverd +17 per minute against Settings
- Running boot hs1 (kernel157 everyday), 60 s of grid scrolling each (`flame-cmd --wait 0`, a swipe about every 1 s; the first try ph1/se1 was void: flame-cmd's default 3 s wait left 2 s of 3 idle). Start under 70 C both.
- Photos / Settings: swaps/s 40.8 / 55.5; GPU submits 8722 / 9444, GPU busy 46.7 / 53.5 %; A55 busy cpu0-5 39-24 / 21-10 % (+14-18 on every A55), cpu6 69.6 / 73.7, cpu7 31.6 / 22.0. Clocks fixed (A55 1497, A76 2208), no caps. Max: cpu6 85.8 / 83.2, cpu7 84.8 / 78.7, a55 83.2 / 79.9; GPU, DDR and the rest equal.
- CPU s in the 60 s: MobileSlideShow 47.1 against Preferences 16.5; mediaserverd 17.3 (not in Settings' top 8); backboardd 59.5 / 68.7; SpringBoard 6.8 / 6.2. Webcam: Photos thumbnails all drawn while scrolling.
- So Photos' extra heat is CPU on the A55s and cpu7, not GPU. Guess: thumbnail decode in software (mediaserverd, the app). Next: pstack samples of both (running), symbolized.

### 2026-11-02 09:30 hs1: which app runs hottest: Photos (15-min UI soak, kernel157)
- Everyday setup kernel157 (sweetvsyncphase=5000) sweetse9=2 sweetcampmic=2 sweetswapbig=6 sweetampreset=1, mtl2-env SWEET_U0011..14=1 + TU_FETCH_GMEM_MIN_DRAWS=20, boot 20261102-091254, on charge 4437-4438 mV. `tools/sunfish-uisoak.sh hs1a 450` then `hs1b 450`: 50.9 k + 50.4 k submits, 0 G1/G2/G4, 0 backboardd restarts, 0 caps. Max 95.7 C (hs1a), 93.2 (hs1b); 13 of 180 readings at 90+ (7.2 %).
- Per app (hottest of a55/cpu6/cpu7 per reading; max / mean C, readings at 90+): Photos 95.7 / 87.9, 9 of 22; Safari 92.2 / 83.9, 4 of 34; Settings 89.9 / 84.2, 0 of 35; Maps 86.7 / 82.1, 0; Calendar 85.4 / 78.9, 0; Notes 82.8 / 74.8, 0.
- So the 90+ C peaks are Photos (and some Safari). Next: what Photos burns (per-thread CPU and GPU per frame in a Photos window), then the A76 cap only for that load if it's CPU.
- Gotcha: the per-app markers `=== uisoak TAG app` land in logs/usb-live.log, not in the per-boot file.

### 2026-11-02 02:04 fa: fence poll and 100 ms slice: 1.3 ms/frame off the A55s, no speed change; default
- `SWIPE_MS=3000 SETTLE=90 MAXSTART=75 tools/sunfish-perfab.sh`, kernel153, 5 boots a side, settings,home, off = work/metal2-next, fence = work/metal2-fence + `sweetfencepoll=200` (xnu 0267, Turnip 0110), 0 hangs, 0 restarts, 0 void.
- Backboardd's waiter thread (inferred from its place among the tids, tracks GPU busy, not named): 515-578 -> 141-205 ms per 5 s, about 1.3 ms per frame, nearly all A55, all 5 boots. A55 busy on Settings 36.8 -> 34.2 %. The A76 render thread (about 3.7 s per 5 s) and the other A55 thread (about 700 ms) don't move.
- Settings median off 56.2, p95 29.3; fence 56.9, p95 20.5. Means 54.4 and 52.6, pulled down by low runs on both sides (off 50.5, 52.0; fence 49.3, 43.0). Home 52.4 and 52.8. Max C Settings 81.5 and 80.4, Home 84.5 and 82.7, but start temperatures also 1.7-1.9 C lower on fence and all above MAXSTART: no heat gain shown.
- Goes in as a default for the idle CPU it saves: xnu 0272 `sweetfencepoll=200` (kernel154), bundle work/metal2-next rebuilt with Turnip 0110. Off: `sweetfencepoll=20` on the boot line. Low Settings runs were 4 of 10 here: fg1 next.

### 2026-11-01 20:45 hw3: clock gating is 3.4 C cooler at idle
- kernel145, base args, 4 interleaved boots, same method as ddi1 (5 min with no touches, lit lock screen, minutes 2-5), 0 hangs, 4442-4455 mV. a55 / gpu / ddr C: `sweethwcg=1` 56.9 / 57.1 / 56.1 and 56.1 / 56.2 / 55.0; off 58.8 / 60.3 / 58.2 and 58.2 / 59.9 / 57.9. Means: on 56.5 / 56.7 / 55.6, off 58.5 / 60.1 / 58.1: 2.0, 3.4, 2.5 C cooler, about what the lowest GPU clock gave in ddi1 and with no loss of clock. Runs swing about 3 C inside themselves; ambient not read. The difference also shows gating is on at boot, which the log can't: no `hwcg on` line before the log starts.
- With hw2's 5 boots and hw1's 3: no speed gain or loss shown (Settings low boots hit all sides), G1 0 in 8 boots on this side, 0-2 C at peak. Goes into the next soak, not tonight's (already set up on kernel139). Default if that's clean, with `sweethwcg=0` as the off switch.

### 2026-11-01 18:45 hw1: clock gating runs clean; Settings 56.9 and steady, heat not shown yet
- kernel144 (xnu 0262, 0263), `sweethwcg=1`, base args, everyday env. Injected hang (`sweethangtest=0xC0000040`, boot 20261101-181401): `recover2 done` after 35.8 ms, `gbif halt ack 3 ok`, `hwcg on clock_cntl 8aa8aa82 (was 0)`, UI back. Boot-time line is in no log, since the live log starts at 21.5 s, before GPU init: on at boot is inferred from the call in `gpu_cp_init`, not seen. `core` still reads 648-650 MHz with gating on.
- `SWIPE_MS=3000 tools/sunfish-perfab.sh hw1, hw1c, hw1d`, off 4 boots, on 3 (first call cut off by a timeout, so not 3 and 3), about 69 k submits, G1 0, G2 0, G4 0, 0 void. Off / on: Home 52.3 (50.2-53.4) / 53.0 (51.4-54.2), max 81-88 / 82-89 C; Control Center 38.7 / 38.4, max 86-93 / 89-92 C; Settings 50.0 (43.5-55.0) / 56.9 (56.5-57.4), p95 32.4 / 20.0, max 83-90 / 83-86 C. The off side is noisy and cp1 got 57.0 without gating, so this isn't shown to be gating: 5-boot A/B next.
- Idle on one boot, gpu sensor minutes 2-5: 56.1, 55.8, 56.1, 59.6 C (last during a 28 % busy spike), against base 58.4 mean in ddi1; 1 boot, ambient unknown, not a result.

### 2026-11-01 18:10 ddi1: idle by GPU clock and DDR vote: lowest GPU clock 2-3.7 C cooler; top DDR vote no change
- kernel139, base args with sweetgpufreq=6, env and bundle as on the phone, 5 boots, each 5 min with no touches after SSH + 60 s, lit lock screen, readings once a minute from `SWEET THERM`, minutes 2-5 averaged, 4441-4442 mV, busy 1-6 % per core everywhere, 0 hangs. a55 / gpu / ddr C: base 56.5 / 58.4 / 56.4; sweetgpufreq=1 54.4 / 54.9 / 53.5, again 54.1 / 54.7 / 53.2; sweetgpufreq=5 56.0 / 57.2 / 55.4; sweetddr=1 55.9 / 57.8 / 55.8.
- Observed: the lowest GPU clock takes 3.5-3.7 C off the GPU sensor and 2.1-2.4 C off the A55 at idle, with the two runs within 0.3 C. Each run swings about 3 C, so 5 and 6 aren't told apart (0.5-1.2 C), and neither is the top DDR vote (0.6 C). Room temperature not read. So GPU clock power is real but gives about 3.5 C of the idle 56-58 C; the rest is elsewhere (panel, rails, no deep CPU idle: guess). `bus vote` logs no line in any boot, so the sweetddr=1 vote is inferred from the code, not seen.

### 2026-11-01 17:45 xnu 0262 sunfish-gpu-hwcg, kernel143: built, not run; idle GPU state read
- Read-only, boot 20261101-143340, 2.9 h idle: hardware clock gating was never on (RBBM_CLOCK_CNTL 0). Core clock, GX and CX GDSC and the rails stay up all the time; nothing lowers them. Present isn't the cause: about 1 swap and 1 panel copy per 60 s at idle. All sensors sit within about 3 C at idle and the GPU-minus-DDR gap is 2.3-2.6 C loaded or not, so the GPU reading hottest doesn't prove it's the source (guess: sensor position).
- Correction to gv1's "DDR vote" and to the comment at sweet_gpu.c:287: `sweetddr=0` is the baked-in default (bootargs.c, sunfish-boot.sh), so there's no top vote on normal boots; `sweetddr=1` only adds one. The level ABL leaves has never been read.
- 0262: `sweethwcg=1` (default 0: no writes) ports mainline's A618 clock gating in `a6_hwcg_on`, called from `gpu_cp_init`: 63 table entries from `a615_hwcg`, CGC mode 0x222, delay 0x111, hysteresis 0x555, RBBM_CLOCK_CNTL 0x8aa8aa82. Unlike mainline it rewrites the table every time, since a soft reset may clear it and leave CLOCK_CNTL. Log `hwcg on clock_cntl`. With it on, `core MHz` may read lower (guess: the counter stops when gated).
- Open worry: mainline's second 0x06010800 catalog entry has no `.hwcg`, reason not given, so compare the G1 rate.

### 2026-11-01 13:50 xnu 0261 sunfish-governor written, not built, not run
- `sweetgov=1` (default 0: no thread, busy line and rows as before). Thread tick 20 ms display on, 100 ms dark. Per domain: boot row at once on touch (`sweet_hid_touch`), GPU submit burst (4 in 33 ms, `gpu_note_submit_locked`) or display wake (`sweet_cpufreq_dark(0)`), held 300 ms; else busiest core over the tick: over 70 % boot row, under 30 % for 100 ms a row down. Effective row = lowest of cap, dark, governor, written only by `osm_rows_apply`. Busy line adds `gov up N dn M` after `dark N`; `SWEET THERM: gov on ...` at start; perfab regexes checked against a sample line.
- Floors `sweetgovfloor55=R` (default 6, 1497 MHz), `sweetgovfloor76=R` (default 5, 1209 MHz), kept within cap floor..boot row. With the 0260 boot row 6 the A55 doesn't move by default; only the A76 is governed, unless `sweetrowa55` is higher.
- Guesses, to measure: burst threshold from the 5 s counts in the last 60 boots (scrolling about 160 submits/s, a band at 20-30/s); touch-to-write about 8 ms poll plus thread wake, up to about 28 ms if a kick misses the wake, plus OSM settling (not measured). Risks: first-touch hitch at low rows, wake-from-dark ordering with the panel thread, up to about 20 row writes/s per domain under load, and any link to GPU hangs (A55 at 1248 MHz raised G1 in rows2, cause not known). A/B against sweetgov=0 is needed before it stays on.

### 2026-10-31 20:48 xnu 0258 sunfish-cpu-rows written, not built, not run
- `sweetrowa55=R`, `sweetrowa76=R` (decimal, each optional; absent: flamecpufreq picks as before): that cluster's boot row, so the thermal cap and the dark clock restore to it. Wins over flamecpufreq (also `flamecpufreq=0`), with a log line. A row past the table or a 1-core boost row is ignored with a log line. Log `SWEET CPUFREQ: rows a55 R=MHz a76 R=MHz (boot-arg)`. The table was already printed per cluster at init (rows count from 0); no second print. The `SWEET THERM:` layout is unchanged.
- Rows from the table in `../logs/boots/20261029-125039.log` (read from OSM at init, not in the source): A55 6=1497, 7=1612, 8=1708, 9=1804; A76 9=1843, 10=1939, 11=2169, 12=2208.

### 2026-10-31 19:46 xnu 0257 sunfish-dark-clk written, not built, not run
- `sweetdarkclk=1` (default 0; needs `sweetdark` on): while dark A55 row 4 (1248 MHz), A76 row 1 (652 MHz); effective row = lower of cap row and dark row; leaving dark restores the cap row. Log `SWEET THERM: dark a55 4=1248 a76 1=652`, `SWEET THERM: bright a55 9=1804 a76 12=2208`, `dark 0|1` after `cap N` on the busy line. Rows are lowered as soon as the off request arrives, while the panel thread's sleep-in (about 200-350 ms) is still running, not after it ends. Raised at the top of RequestPowerChange(on); the key press that leads there still runs at dark rows.

### 2026-11-01 14:35 gv1: governor costs no speed and shows no heat gain so far; off
- `SWIPE_MS=3000 tools/sunfish-perfab.sh gv1 4 --secs 15 --screens settings,home,cc,apps`, kernel142 (xnu 0260, 0261), sweetgpufreq=6, everyday env, 8 boots, 94 k submits, 0 GPU bugs, 2 void (`0 swaps`, g0 b2). Off / on: Settings 53.4 / 55.2 swaps/s, p95 30.1 / 23.3; Home 53.1 / 52.9; Control Center 38.4 / 38.4; opens Clock 514 / 492, Calculator 527 / 479, Notes 262 / 261 ms; peak temp the same. Idle 5 min: 59.6-60.9 C with A76 at 2208 MHz, 59.9-62 C with A76 at 1209 MHz: no gain at idle, 1-5 % busy so clocks hardly matter; GPU sensor is the hottest there (62 C). Off by default; light use not measured.
- Idle now: DDR vote and GPU power state are the next suspects (sweet_gpu.c:286; GX collapse unread).

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

