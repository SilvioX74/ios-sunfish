# WebGL (Safari)
**State:** 🟡 context created and frames run (20 fps, 15 min, 0 hangs), but draws fail (GL_INVALID_OPERATION): only
the clear colour shows. **Updated:** 2026-11-03.

## Facts
- WebKit's GPU process runs ANGLE on Metal (iOS 16: WebKit 615.8.1.10.3, Metal 306.6.6). ANGLE first asks
  `supportsFeatureSet:4` (iOS_GPUFamily3_v1); the bridge answers YES for iOS families 1-3 since 283012f
  (SWEET_FEATURESET=0: all NO, contexts lost again).
- ANGLE compiles every WebGL program from Metal shader *source* (newLibraryWithSource); the metal2vulkan runtime
  only takes AIR (library.rs:99-107). Most likely why draws fail (research/notes/webgl/).
- Test page: work/stage0/webgl.html (gitignored; beacons to the PC http.server on 192.168.7.1:8077, see video.md s0).

## Open
- Draws: the shader-source path (investigator, 2026-11-03).

## Log
(Newest first, append-only. When this Log passes about 150 lines, move its oldest entries to the archive.)

### 2026-11-03 06:55 wg2: feature-set fix: live context, draws fail
Bundle work/metal2-next 283012f (LC_UUID 4C4C448E-5555-3144-A107-29808E55734D, includes 0125 off), kernel158 boot
venusR; per-boot log ../logs/boots/20261103-062614.log, 06:34-06:55.
- Install: home screen fine, backboardd stayed pid 401, no restart.
- webgl.html?n=400: `r=WebKit WebGL v=WebGL 1.0 lost0=false e0=0`; LINK_STATUS true, empty info log, getError 0
  after link; getError 1282 (GL_INVALID_OPERATION) after each of the first frames. 181 fps beacons over 15 min:
  median 20.4, min 17.9, max 22.6 fps, worst frame 410 ms (first 5 s), ~80 ms steady, ~18,559 frames. 0 GPU hangs,
  0 backboardd restarts, max 69.9 C. Webcam: page near-black with a faint dark-blue glow = the clear colour
  (0.05, 0.05, 0.1); no cubes.
- GPU process log: device setup, "no device answer" for gpuCoreCount, maxVertexAmplificationCount,
  maxComputeThreadgroupMemory, isLargeMRTSupported, supportsComputeCompressedTextureWrite,
  supportsConditionalLoadStore; 0 MISSING, no library/pipeline errors.
- Guess: shader compile from source fails silently at draw time. Investigator on the exact failing point.

### 2026-11-03 06:25 wg1: context lost at creation (B55)
getContext('webgl') returned an already-lost context (getError 37442, RENDERER null). Cause from disassembly:
angle::IsMetalRendererAvailable (libANGLE-shared 0x1ce2b85a4) needs supportsFeatureSet:4; the bridge said NO for all
sets, so EGL_Initialize fails in GraphicsContextGLCocoa::platformInitializeContext with no log line
(research/notes/webgl/context-lost-2026-11-03.md). The GPU process lowMemoryHandler a second later is unrelated
(over its 300 MB soft limit: resident 486 MB; separate issue).
