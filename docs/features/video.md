# Video playback and hardware decode
**State:** ⬜ not tested; hardware decode not started. **Updated:** 2026-11-02.

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
  our plugin goes there and registers 'avc1'/'hvc1' as hardware. Code signing of a new dylib: unchecked.

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
- Stages 0, 2-5.

## Log
(Newest first, append-only. When this Log passes about 150 lines, move its oldest entries to the archive.)

### 2026-11-02 22:00 Stage 1 research done (read-only)
- No blocker found: firmware is on the phone's vendor partition, TrustZone PAS ID 9, register map, clocks, GDSCs, SMMU IDs
  and memory region come from the phone's own live DT (../recon/live.dts) and the downstream sunfish 4.14 driver.
  VideoToolbox entry point found (a free name on its decoder load list). Facts and the stage 2 spec above. Confidence:
  high on DT/firmware/PAS ID/packet IDs/load list (source or binary), medium on the order of power steps and bus votes.

### 2026-11-02 20:10 Plan adopted: hardware video decode is priority 1
- The user's goals: video-heavy apps like Instagram, then games (Minecraft). Video playback was never tested on the port.
  Stage 1 research can run without the phone; stage 0 needs it back.
