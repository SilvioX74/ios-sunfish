# WebGL (Safari)
**State:** ✅ WebGL 1 and 2 draw in Safari: 400 cubes 24 fps (18 with MSAA), three.js games_fps (WebGL 2) 15 min with 0
hangs but only 7.4 fps (GPU-bound); HexGL parked (its loader stalls on iOS Safari). **Updated:** 2026-11-03.

## Facts
- WebKit's GPU process runs ANGLE on Metal (iOS 16: WebKit 615.8.1.10.3, Metal 306.6.6). ANGLE first asks
  `supportsFeatureSet:4` (iOS_GPUFamily3_v1); the bridge answers YES for iOS families 1-3 since 283012f
  (SWEET_FEATURESET=0: all NO, contexts lost again).
- ANGLE compiles every WebGL program from Metal shader *source* (newLibraryWithSource); the metal2vulkan runtime
  only takes AIR (library.rs:99-107). Most likely why draws fail (research/notes/webgl/).
- Shader source (B55 route A, frontend): `newLibraryWithSource:` runs Apple's compile (MTLCompilerService), serializes the
  `_MTLLibrary` to a temp metallib and loads it as the bridge's NBLibrary; boot log `SWEET SRC: <proc> #N lib .. -> NBLibrary ok|fail (..)`,
  refusals of other devices' functions log `SWEET MTL: foreign function <class> refused`. SWEET_SRCLIB=0: off (Apple's library, as before).
- Ported from UniXNU/AdrenoMetal 243d63c (MIT, 2026-11-03): swizzled texture views (bridge `nmb_texture_new_view_swizzle`), real
  sampler LOD clamps (a sampler kept per clamp pair; SWEET_LODCLAMP=0 drops them), `presentDrawable:` (+ atTime:,
  afterMinimumDuration:), and a missing selector answered nil/0 after its one `MISSING` line (SWEET_MISSING_ABORT=1: abort as before).
- Test page: work/stage0/webgl.html (gitignored; beacons to the PC http.server on 192.168.7.1:8077, see video.md s0).

## Open
- WebGL 2 (argument encoders); a real WebGL game; fps (24 at 400 cubes: profile); shader compile cost on page load.

## Log
(Newest first, append-only. When this Log passes about 150 lines, move its oldest entries to the archive.)

### 2026-11-03 16:00 trace: games_fps is one 57 ms fragment shader on the screen pass
Bundle 2278758 (SWEET_GPU_TRACE_PROC=com.apple.WebKit.GPU + MESA_GPU_TRACES=print), traces in research/notes/
webgl-trace-2026-11-03.log (80 s) and -draw- (45 s, TU_SWEET_DRAW_TRACE=1); fps 7.2-7.8 during both.
- render_pass 60.6 s of GPU in 80 s; the 1125x1905 target 50.2 s of it (83 %): 3244 passes, mean 15.5 ms, ~1 draw per
  pass (1948 1-sample passes, 1296 4-sample MSAA passes with 0-1 draws). 1024x1024 shadow map: 2588 passes, 3.9 ms each,
  up to 101 draws.
- Draw trace: `fs 4b132e36ec68 vs c9b15857bd08` = 70.7 % of draw time, 57 ms per draw (n=353); `1125x1905 draw 0`
  17.2 ms mean. Next shaders 3.8-5.1 ms.
- tiledRender=false on 7335 of 7400 passes (`Autotune selected sysmem`); lrz disabled on all (lrzDisableReason empty);
  sysmem_clear 5.7 s, sysmem_resolve 2.2 s; slow_clear_lrz 3250x.
- Reading: per-pixel shader cost, not draw count. Next: dump shader 4b132e36ec68 (ANGLE MSL -> AIR -> SPIR-V -> ir3),
  instruction count, loops/branches, texture/compare sampling; then sysmem vs GMEM for the screen pass.

### 2026-11-03 14:40 tfps: three.js games_fps (WebGL 2) runs 15 min clean — the 3D target
three.js examples/games_fps.html (MIT, sparse clone in work/webgl-games/threejs, gitignored; error/fps hook added),
bundle fcdf6de, kernel160 boot lat1, 11:28:54-11:44 (15.4 min).
- WebGL 2 scene drawn (3 canvases, 1125x1905): the textured grid level with walls and stairs, shadows; tapped spheres
  appear (webcam f15m.jpg, checked by Claude). All modules + collision-world.glb loaded.
- 0 hangs, 0 backboardd restarts, 0 CRASH, no context loss; 10 shader compiles via SWEET SRC (0.7-0.8 s each, 4.4 s
  total, 0 fails); 0 MISSING (no argument encoder needed here), 0 foreign-function, 0 refusals.
- fps median 7.4 (6.1-7.8): GPU-bound, busy 91-99 %, ~240 submits per 5 s, mean ~20 ms, longest ~104 ms. Max 67.4 C,
  no THERM cap. Page errors: requestPointerLock missing on touch (the demo's), .ogg audio decode (Safari).
- flame-shot came out white (the shot tool doesn't capture this layer); the webcam shows the scene.
- Next: why 7 fps (profile one frame: shadow maps? MSAA? draw count), WebGL 2 features with argument buffers.

### 2026-11-03 14:15 HexGL parked
?v=10 with the fixed hook: index.html loaded (subresources from Safari's cache, so no GETs), the hook's ticks report
`0 canvases` for 2.5 min, no error beacons: the game never creates its canvases (launch.js stalls before the menu). Not a
GPU/bridge problem (no WebGL context is made). The same page runs in a desktop browser. Parked (half-day rule): for the
3D target take a simpler WebGL game next time; antialiased WebGL itself works (wgaa2).

### 2026-11-03 14:05 HexGL: stale Safari cache, then my own error hook broke the menu
Safari caches moved to /var/local/safari-cache-bak/20261103/ (test only): the scripts load again (38 GETs). The page
stayed white because the error hook's 5 s tick called getContext('webgl') on the first canvas, which locks HexGL's 2D
menu canvas to WebGL (the game's getContext('2d') then gets null). Earlier "white at the title page" was the same. Hook
fixed (counts canvases only); retest running.

### 2026-11-03 11:55 HexGL retry: page stays white with no scripts loading on the phone
After the .m4a change, Safari requested only index.html (+ the start beacon), no JS/CSS/audio, no ticks, twice (?v=2,
?v=3). The same page loads in the desktop browser here (only a favicon 404). Phone-side Safari state (cache?); parked.

### 2026-11-03 11:30 HexGL white = its loader waits forever on .ogg audio (page bug, not ours)
Error hook in work/webgl-games/hexgl/index.html (beacons to :8078). WebGL context healthy (ticks `lost=false err=0`
every 5 s after one initial 1282), 2 programs compiled (SWEET SRC ok, 835 ms each on a cold MTLCompilerService), ~23
WebKit submits then none. Beacons: `rej EncodingError: Decoding failed` x4 and `error Audio decode failed!` x4: Safari 16
can't decode HexGL's .ogg files, and bkcore/Audio.js never calls its loader callback on a decode failure, so the race
never starts (the canvas shows its undrawn white). Local copy changed (gitignored): .ogg -> .m4a (ffmpeg AAC 96k),
Cityscape.js points at .m4a, Audio.js calls back on failure. Retest running.

### 2026-11-03 09:40 wgaa2: resolve textures passed through (4da54f0): antialiased WebGL works; HexGL white
Bundle 4da54f0 (LC_UUID 4C4C44B9): the frontend used to drop every resolveTexture (NmbPassAttachment had no resolve
fields), so all colour MSAA resolves did nothing and depth resolves were refused. ANGLE always sends a resolve texture
(finalizeLoadStoreAction; no late store-action setters). Ported from AdrenoMetal 243d63c; SWEET_RESOLVE_FIX=0 off.
- webgl.html?n=400&aa=1, 5 min: no context loss, getError 0, 0 ResolveWithoutTexture/CRASH/MISSING; 5535 frames in 301 s,
  median 18.4 fps (16.1-18.7; 24.4 without MSAA); webcam: cubes with sharp edges.
- HexGL: menu and controls screen draw; after the continue tap all textures and the skybox load, then the canvas is
  solid white; WebKit GPU submits 2-3 per 5 s; no refusal, hang, crash or MISSING line. Cause unknown (JS stall? a
  blown-out render?). Next: get the page's console/errors (beacon a window.onerror into HexGL's index.html).
- UI (Settings, CC, Photos, Notes, Safari) unchanged; Maps still black (B50), plus overlapping "192.168.7" text in the
  Favorites cells (new, once).

### 2026-11-03 09:15 hexgl / wgaa: antialiased WebGL is lost on the first frame (depth resolve without texture)
Bundle a1f3952, kernel160 boot maps512. HexGL (MIT three.js racing game, work/webgl-games/hexgl served on
192.168.7.1:8078; gitignored): menu and controls-help screen draw correctly; after the continue tap the canvas goes
black (2 tries, HIGH and LOW quality); 38 requests, no 404, no model/texture requests after START. One line per try:
`command buffer refused: renderCommandEncoder: render encoder: Invalid(... RenderPass(ResolveWithoutTexture(Depth)))`.
Reproduced with webgl.html?aa=1 (antialias:true, the WebGL default; wg1-wg5 used antialias:false): same line, then
`err=contextlost`. Also seen: first shader compiles after a GPU-process restart took 766 / 1014 ms (vs 3-5 ms in wg4).
Cause (from the runtime source): metal2vulkan rejects a resolving depth store action without a resolve texture, as
Metal's validation layer does; ANGLE relies on the driver tolerating it. Fix being written in the frontend: downgrade
to Store/DontCare (SWEET_RESOLVE_FIX=0 off).

### 2026-11-03 07:58 wg5: AdrenoMetal ports (4ca1221) installed: no change in WebGL, UI fine
Bundle 4ca1221 (LC_UUID 4C4C4484; swizzle views, missing-selector fallback, presentDrawable:, real LOD clamps), kernel159.
UI smoke: Settings, Control Center, Photos (one blue thumbnail: B44), Maps (map area black: B50), Safari ok; 0 MISSING,
0 CRASH after the install, 1 backboardd restart (the install), 0 GPU hangs. webgl.html n=400, 5 min: median 24.3 fps
(min 18.2, max 25.5), 0 err beacons, cubes correct on the webcam.

### 2026-11-03 07:45 wg4: WebGL draws: 400 textured cubes, 15 min, 24 fps, 0 hangs
Bundle work/metal2-next 5a76e18 (LC_UUID 4C4C4448-5555-3144-A1E8-B17B63C5AD50: 14ebff1 newLibraryWithSource override +
sampler-LOD setters), kernel158 boot venusR; per-boot log ../logs/boots/20261103-062614.log, 07:28:04-07:43:15.
- wg3 before it (14ebff1 alone, 07:21): `SWEET SRC: ... lib 5019 bytes source -> 7996 bytes metallib -> NBLibrary ok
  (1 functions) compile 2708 us load 83 us` (and #2, 10562 -> 9988 bytes), then `MISSING NBRenderEncoder
  -setFragmentSamplerState:lodMinClamp:lodMaxClamp:atIndex:` and `CRASH com.apple.WebKit[1614] sig 6` -> context lost.
- wg4: both programs `NBLibrary ok` (compile 4518 / 3776 us, load 250 / 318 us); frame beacons getError 0; 180 fps
  beacons: median 24.4, min 21.8 (first 5 s), max 25.4; worst frame 286 ms (first), 64-73 ms steady. 0 context loss,
  0 MISSING/CRASH/foreign-function lines, 0 GPU hangs, 0 backboardd restarts after the install; max 76.1 C. Webcam
  (scratchpad wg4-*.jpg, checked by Claude): coloured checker-textured cubes with correct depth, animating to the end.
- Not measured: CPU/GPU load. The sampler LOD clamps are still dropped here (AdrenoMetal port of real clamps under way).

### 2026-11-03 07:45 Why draws fail (read-only, disassembly): Apple's library, our pipeline refuses it
- Our device has no `newLibraryWithSource:`, so `_MTLDevice`'s runs: MTLLibraryBuilder -> MTLCompilerService (the
  boot log shows `exec MTLCompilerService` twice right after the GPU process opened its device) -> an Apple
  `_MTLLibrary`. ANGLE's link passes on any library (ProgramMtl 0x1ce455804-810), hence LINK_STATUS true.
- At draw: CreateMslShader 0x1ce412b08 `newFunctionWithName:@"main0" constantValues:error:`, then
  createRenderPipelineState 0x1ce40e580; our renderDescriptor refuses a non-NBFunction ("foreign function",
  NativeBridgeMetalFrontend.mm:1881-1883) silently -> nil -> GL_INVALID_OPERATION (0x1ce40e5cc). Inferred, not
  seen on the phone: that the on-device source compile succeeds.
- No SPIR-V route in this ANGLE or the shared cache. Route chosen: override newLibraryWithSource:, serialize Apple's
  library to a metallib, load it through our metallib -> SPIR-V path (implementer, switch SWEET_SRCLIB=0).
  Fallback: compile ANGLE's source ahead of time in the macOS VM.

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
