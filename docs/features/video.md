# Video playback and hardware decode
**State:** 🟡 Safari plays H.264 and HEVC 720p/1080p in software (250-590 % of one core, 1080p H.264 drops 5.6 %); hardware decode: research done, decoder slot loads, engine bring-up under way. **Updated:** 2026-11-03.

## Facts
- The iPhone X decodes video in hardware (AppleAVD); the port has no AVD, so iOS falls back to software decoders.
  mediaserverd's CPU in Photos is probably software HEVC (VCPHEVC; a guess, cpu.md 2026-11-02 phs).
- Colour conversion and scaling (VideoToolbox pixel transfer) run in software on every core: no AppleM2ScalerCSCDriver
  (cpu.md phs, vts). The graph-path preference crashes Photos (cpu.md vt).
- Showing video frames needs YUV (biplanar 420) IOSurfaces in the Metal bridge: GPU bug G7 "planar iosurface refused"
  (gpu-bugs.md).
- The Snapdragon 730G (SM7150) has Qualcomm's video engine ("Venus"); its firmware is Qualcomm's and stays on the phone,
  out of git. Linux's open `venus` driver (HFI protocol) is the reference.
- Sound for video: speaker quiet (B16); AirPods connect but play nothing (audio.md).

## Plan (the user, 2026-11-02: hardware decode first)
Stages, each a go/no-go: 0 Safari baseline on the software path; 1 read-only research and spec; 2 kernel bring-up
(power, firmware via TrustZone, HFI ping); 3 first decoded frame from a phone tool; 4 VideoToolbox decoder plugin
(H.264, then HEVC); 5 tuning (heat, 4K, several streams). Rough size: 2-6 weeks of sessions.

## Hardware (stage 1 research, 2026-11-02; full notes research/notes/venus-stage1-2026-11-02.md)
- SM7150 = sdmmagpie, Venus VPU5/IRIS1, HFI gen1 (Linux venus HFI_VERSION_4XX; no mainline SM7150 entry, the
  downstream 4.14 msm_vidc driver is the reference, research/ext/msm-vidc-sunfish/).
- Core 0x0aa00000 +0x200000, wrapper 0xaae0000, IRQ SPI 174 level. videocc 0x0ab00000: GDSCs mvsc +0x814, mvs0 +0x874,
  mvs1 (CVP) +0x8b4; all need gcc_video_ahb (GCC+0xb004). Clocks videocc mvsc_ctl_axi 0x9cc, mvs0_axi 0x9ec, mvsc_core
  0x850, mvs0_core 0x890, venus_ahb 0xa6c, iris_ahb 0x8f4; gcc_video_axi 0xb01c. PLL0 (Fabia, +0x42c) 480 MHz /2 =
  240 MHz, the lowest step (enough for 1080p30, estimate).
- Bus: RPMh BCMs MM3 and MM2, unvoted by anyone else. SMMU: apps SMMU, stream ID 0x1080 mask 0x60 (non-secure); never
  touch 0x1081/0x1083/0x1084. Device VA 0x25800000-0xe0000000.
- Firmware: /vendor/firmware/venus.mdt + .b00-.b04 (VIDEO.IR.1.2-00053), loads at 0x94800000 size 0x500000. TrustZone
  PAS ID 9 (SMC 0x42000200|cmd), then memory protect 0x42000c08 (0, 0x25800000, 0x1000000, 0x24800000).
- Reusable code: PAS loader phone/wifi/adspboot.c (via /dev/pmem PMEM_SMC), RPMh votes by BCM name (sweet_gpu.c),
  Fabia PLL phone/tools/camclk.c, phone/wifi/smmu.c.
- Output: reference frames are UBWC; linear NV12 comes only on OUTPUT2 with the multi-stream property.
- iOS: _VTLoadVideoDecoders dlopens a fixed list; AVD.videodecoder refuses without AppleAVD. Names on the list missing
  from our cache (e.g. /System/Library/VideoDecoders/ExampleIPB.videodecoder, _ExampleIPBDecoderRegister) would load:
  our plugin goes there and registers 'avc1'/'hvc1' as hardware. The path is the flat Mach-O itself (dlopen RTLD_LOCAL,
  then the register symbol is called). Library validation is in effect off (sweet_amfi.c has no LV hook), root FS is
  writable, so an ldid-signed dylib should load (not yet tested). Safari <video> decodes in mediaserverd (AVPlayer remote;
  medium-high confidence), WebCodecs/WebRTC maybe in the GPU process: the plugin must be cheap to load anywhere.
- Firmware source: restore/factory/vendor.img /firmware/venus.{mdt,b00-b04} (1.1 MB), extracted locally to research/fw/venus.

## Stage 2 spec: `venusctl` (userland, /dev/pmem like adspboot; failed tries need no reboot)
1. `probe` (read-only): GCC video clocks, TBU vote/status, SMR scan; videocc only with AHB on.
2. `power on/off`: MM3+MM2 votes, PLL 240 MHz, mvsc then mvs0 GDSC, branch clocks; read HW version at 0xaae0000.
3. `pas-check`: is-supported, init_image(9) with the signature, shutdown. Errors come back as codes.
4. `boot`: exact SMRs, segments to 0x94800000, mem-setup, auth_and_reset, memory protect, read status, shutdown
   (most likely step to reset).
5. `init`: queues (3 MB uncached: header 0x1c8, 3 queues of 0xc8000) + SFR page, registers 0xd2064/68, 0xd2054,
   0xd2050, 0xd205c, 0xd2034-3c; 1 -> 0xd2048, poll 0xd204c bit0.
6. `ping`: SYS_INIT 0x10001 -> 0x20001, SYS_PING 0x210002 -> 0x220002 (stage 2 go).
7. Steps 4-6 ten times in one boot, then 15 min under GPU load (clean teardown).
Risks: register access with a clock or GDSC off hangs the bus (gate on CBCR/GDSC bit31); auth_and_reset unpowered
resets; masked SMRs reset (camera tests), use exact ones; no MX/CX votes (reset redfin); PAS shutdown before power off.
Open: free memory for queues (0xa5100000-0xbcc00000 looks free, unproven), PMEM_ALLOC32 below 4 GB likely fails,
whether CVP (mvs1) must be on, which of the four stream IDs carry traffic.

## Open
- Stages 2-5. Stage 0 done 2026-11-03 (s0): plays H.264/HEVC 720p/1080p, 250-590 % CPU.

## Log
(Newest first, append-only. When this Log passes about 150 lines, move its oldest entries to the archive.)

### 2026-11-03 06:25 Stage 0: Safari baseline on the software path (s0), WebGL lost at creation
Kernel158 everyday boot (2026-11-02 21:31, no reboot), bundle 82f6b41, external power 4427 mV, iOS slider minimum.
Per-boot log ../logs/boots/20261103-004005.log, 06:10-06:22. Page work/stage0/index.html (gitignored; clips 60 s,
30 fps testsrc2, muted autoplay, beacons to the PC's http.server on 192.168.7.1:8077). CPU = psx CPU-seconds delta
over 30 s (10-40 s into the clip), % of one core; temp = max SWEET THERM a55.
| clip | plays | frames | dropped | CPU total | top processes | max temp |
|---|---|---|---|---|---|---|
| H.264 720p | yes, t=60 | 1800 | 3 (17 in a first run) | ~400 % (sum of top 3; total lost) | WebContent 127, mediaserverd 124, backboardd 53 | 59.9 C |
| H.264 1080p | yes, t=60 | 1800 | 100 (5.6 %) | 590 % | mediaserverd 257, WebContent 138, backboardd 96 | 64.1 C |
| HEVC 720p | yes, t=60 | 1800 | 5 | 247 % | mediaserverd 70, WebContent 64, backboardd 50 | 62.2 C |
| HEVC 1080p | yes, t=60 | 1800 | 49 (2.7 %) | 346 % | mediaserverd 115, backboardd 93, WebContent 63 | 63.2 C |
- Observed: all four play to the end with no error beacon, no GPU hang, no backboardd restart. The live log has no
  decoder line at all (VideoToolbox, vtdecode, ExampleIPB, "planar iosurface", AppleAVD, H264/HEVC: 0 each), so it
  doesn't show which path decodes. Webcam frames at 10 s showed the colour bars for H.264 and a blank area for HEVC
  (first paint late; the beacons show full playback).
- Guess: mediaserverd is the software decoder; HEVC costing less CPU than H.264 at the same size is unexpected (maybe a
  different, better-threaded decoder, or the HEVC files' lower bitrate: 31 MB vs 46 MB). Target (under 15 % of one core)
  is 20-40x away.
- WebGL (webgl.html, 400 cubes): `getContext('webgl')` returns a context that is already lost: isContextLost true,
  getError 37442 (CONTEXT_LOST_WEBGL), RENDERER null, createShader null. No frames. Nothing about GL in oslog
  (research/notes/webgl/wgl-2026-11-03.oslog); the GPU process logs `lowMemoryHandler: critical=1` a second later.
  New backlog B55; investigator on it.

### 2026-11-02 23:10 venusctl: pre-run review fixes
(research/notes/venus-stage1-2026-11-02.md 8.5; built with phone/wifi/build.sh, not run on the phone yet)
- 1 Stale state: STATE and AHB_STATE carry `boot <kern.bootsessionuuid>` (fallback kern.boottime); a file from another boot (or without the line) is dropped unread ("stale state from an earlier boot: dropped"; VENUSCTL_NO_BOOTID=1 keeps it). Teardown releases an SMR only if it still reads exactly 1<<31|sid with S2CR translate -> our CB, else prints why and leaves it.
- 2 `power off` refuses while STATE says pas 1 (`--force` overrides). Script: an exit 124 or a failed shutdown stops the run with no power off and prints "LEFT POWERED: reboot needed".
- 3 Script passes `--cvp` to every power on/off.
- 4 init re-reads the mnoc_sf_0_tbu vote (0x17d044 PWR_ON) right before CTRL_INIT, q_write before each doorbell; STOP if off (status 0x15182220 printed, bits unknown; VENUSCTL_NO_TBUCHECK=1 skips). No vote of our own.
- 5 gdsc_on clears a set HW_CONTROL (SW mode, as downstream) and prints it; STOP if it stays set.
- 6 `init --phys` (and the queue check): 1 MB aligned, ADDR+3 MB+4 KB within 0xa5030000-0xbcc00000 and 0x25800000-0xe0000000.
- 7 `pas-check --no-shutdown` writes STATE pas 1 (with the boot id), so `shutdown` undoes it.
- 8 New `venusctl memtest ADDR SIZE write|check` (pattern addr ^ 0x5a5a5a5a, /dev/pmem as init; count + first 5 mismatches); script step `memtest` (0xa5100000+0x301000, 60 s, check).

### 2026-11-02 23:00 venusctl: boot, init, ping, shutdown written (stage 2 steps 4-6), not run yet
- phone/wifi/venusctl.c: `boot [--fw DIR] [--keep]` (exact SMRs via smmu.c smmu_identity mask 0 for 0x1080/0x10a0/
  0x10c0/0x10e0, one translation-off bank each; refuses if any SMR already matches; CPU_CGC_DIS/CPU_CLOCK_CONFIG = 0;
  PAS init_image/mem_setup/load to 0x94800000/auth_and_reset(9); memory protect 0x42000c08 (0, 0x25800000,
  0x1000000, 0x24800000); status reads), `init [--phys ADDR]` (3 MB block from PMEM_ALLOC32, else only an explicit
  1 MB-aligned address in 0xa5100000-0xbcc00000; table/queues as downstream, SFR at +0x2fe000; UC/QTBL/SFR/VMIMSG
  registers, CTRL_INIT = 1, 1 s poll), `ping [--debug MASK]` (SYS_INIT, debug config, IMAGE_VERSION, SYS_PING, message
  queue polled), `shutdown` (PAS shutdown, queue memory, SMRs from /var/tmp/venusctl.state). Every core access gated on
  the clocks/GDSCs. Off switches: VENUSCTL_SID1=1, VENUSCTL_NO_CLKCFG=1, VENUSCTL_NO_MEMPROT=1.
- Guesses: SFR inside the UC region at +0x2fe000 (downstream allocates it right after the 0x2fe000 queue block);
  debug mask 0x18 (ERROR|FATAL; downstream default not in our source copy); doorbell only when cmdq rx_req = 1 (as
  downstream); ping client_data is our cookie; interrupt_init_vpu5 (INTR_MASK) left out (we poll).
- tools/sunfish-venus-test.sh probe|power|pas|boot|ping: the venusctl sequence per step over sunfish-ssh, always ending
  with shutdown + power off (probe excepted: read-only).

### 2026-11-02 22:50 venusctl: AHB HW_CTL handling
- Seen in vd1 `venusctl probe`: gcc_video_ahb 0x10b004 = 0x80000003 (CLK_ENABLE 1, HW_CTL 1, CLK_OFF 1), gcc_video_axi
  0x10b01c = 0x80000000, gcc_video_xo 0x10b028 = 0x80000000. The "AHB on" gate (CLK_OFF = 0) never passed.
- Source: no downstream gcc-sdmmagpie.c in research/ext/msm-vidc-sunfish/ (only the dt-bindings header). Mainline
  research/re/gcc-sm7150.c: gcc_video_ahb and gcc_video_xo are not clk_branches at all, only
  `qcom_branch_set_clk_en` (CLK_ENABLE bit 0) in probe, "keep some clocks always-on" (:3007 0x0b004, :3011 0x0b028);
  no hwcg_reg, no halt_check. gcc_video_axi (:2618-2629): BRANCH_HALT on 0xb01c, no hwcg; its BCR at :2921.
  Linux clk-branch.c:72 skips every halt check while a branch is in HW_CTL mode, so CLK_OFF = 1 under HW_CTL is
  normal idle gating, not "off". Downstream DT: GCC_VIDEO_AHB_CLK is the ahb_clk of mvsc/mvs0/mvs1 GDSCs
  (sdmmagpie.dtsi:4013,4019,4026); no video node uses GCC_VIDEO_XO_CLK (videocc parent is bi_tcxo,
  videocc-sdmmagpie.c:60,100; venus PIL node takes VIDEO_CC_XO_CLK, dtsi:3408).
- Change (phone/wifi/venusctl.c): power on clears HW_CTL on gcc_video_ahb (CLK_ENABLE kept), saves the old CBCR in
  /var/tmp/venusctl.ahb, then polls CLK_OFF = 0 for 100 ms (BRANCH_HALT style, as gcc_video_axi: no source gives a
  halt_check for this branch); power off restores HW_CTL from that file after the videocc teardown. probe prints
  "hw-gated (HW_CTL)" for a GCC CBCR with CLK_OFF + HW_CTL and still skips videocc then (read-only kept).
  gcc_video_xo is not added to the allow-list (no video consumer). Off switch: VENUSCTL_KEEP_HWCTL=1. Not run yet.

### 2026-11-02 22:45 vd1: decoder slot loads in mediaserverd; venusctl probe; software baseline (night, read-only)
- Run vd1, kernel158 everyday boot of soak n8 (20261102-213020), markers `=== vd1 *`.
- vtstub at /System/Library/VideoDecoders/ExampleIPB.videodecoder: loaded by vtdecode (pids 1478, 1498, 1500) and by
  mediaserverd right after `psx -k mediaserverd` (pid 1485). No crash lines; stub removed, mediaserverd back (pid 1512).
  One of four vtdecode runs left no log line (unexplained). Stage 4 entry point works.
- vtdecode, synthetic testsrc2 H.264 10 s / 300 frames, software path (`using hardware decoder: no`,
  VTIsHardwareDecodeSupported 0, hardware-required session -12906): 720p 212.6 fps (1.41 s), 1080p 112.2 fps (2.67 s);
  same with the stub (218.8, 213.8, 110.5). Every run prints `IOServiceMatchingfailed for: AppleM2ScalerCSCDriver`.
  CPU share not measured (fps is decode-as-fast-as-possible; a CPU % at 30 fps playback is stage 0's job).
- venusctl probe: gcc_video_ahb 0x10b004 = 0x80000003 (enable and HW_CTL set, CLK_OFF 1: hardware-gated, so the
  probe's AHB-on check skipped videocc); gcc_video_axi 0x80000000, gcc_video_xo 0x10b028 0x80000000 (off); TBU vote
  0x17d044 = 0x80000001; SMMU 79 SMRs, 15 valid, none for SIDs 0x1080-0x10e0 (boot step must add exact SMRs); cmd-db
  has MM3 and MM2 (aux 00197b70 00040020).
- For `power on`: with HW_CTL set the AHB branch reads off until something uses it; clear HW_CTL (bit1) before polling
  CLK_OFF, or the gate check never passes (guess, check against downstream gcc-sdmmagpie).

### 2026-11-02 22:00 Plugin loading checked (read-only)
- Free slot /System/Library/VideoDecoders/ExampleIPB.videodecoder, symbol _ExampleIPBDecoderRegister (0x194fe0c5f/c95).
  No library validation hook in sweet_amfi.c; SunfishAudio.driver already runs inside mediaserverd. Phone test next: a
  stub that only logs which processes load it.

### 2026-11-02 21:50 venusctl written (stage 2 steps 1-3), not run yet
- phone/wifi/venusctl.c (3851cad): `probe` (read-only), `power on/off [--cvp]`, `pas-check [--fw DIR] [--no-shutdown]`.
  Writes only to a fixed allow-list (GCC video branches, videocc PLL0/RCG/GDSCs/branches, RPMh MM3+MM2); no SMR writes;
  videocc access gated on gcc_video_ahb bit31; 100 ms poll timeouts; a failed `power on` runs `power off`.
- Guesses in it: MM3/MM2 vote x=1,y=1 (probe prints whether cmd-db has them); GDSC SW_COLLAPSE bit0 from Linux;
  gcc_video_ahb left on at power off; efuse 0x786018 bit10 picks PLL L=0x14 vs 0x19 (downstream). PAS shutdown only
  after a successful init_image (adspboot adsp3: shutdown with nothing loaded blocked forever).
- Phone step needed first: copy venus.mdt + .b00-.b04 to /var/local/fw/venus (iOS has no /vendor), log in device-changes.

### 2026-11-02 22:00 Stage 1 research done (read-only)
- No blocker found: firmware is on the phone's vendor partition, TrustZone PAS ID 9, register map, clocks, GDSCs, SMMU IDs
  and memory region come from the phone's own live DT (../recon/live.dts) and the downstream sunfish 4.14 driver.
  VideoToolbox entry point found (a free name on its decoder load list). Facts and the stage 2 spec above. Confidence:
  high on DT/firmware/PAS ID/packet IDs/load list (source or binary), medium on the order of power steps and bus votes.

### 2026-11-02 20:10 Plan adopted: hardware video decode is priority 1
- The user's goals: video-heavy apps like Instagram, then games (Minecraft). Video playback was never tested on the port.
  Stage 1 research can run without the phone; stage 0 needs it back.
