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

## Open
- Everything above.

## Log
(Newest first, append-only. When this Log passes about 150 lines, move its oldest entries to the archive.)

### 2026-11-02 20:10 Plan adopted: hardware video decode is priority 1
- The user's goals: video-heavy apps like Instagram, then games (Minecraft). Video playback was never tested on the port.
  Stage 1 research can run without the phone; stage 0 needs it back.
