# Plan (2026-10-31): speed and heat

Rules: CLAUDE.md ("How to work"). Old plan: `docs/plans/2026-10-31-mixed-priorities.md`. One line per item;
details and history in the feature Logs (`docs/features/<name>.md`).

## Focus (the user, 2026-10-31)
Make the phone fast and smooth first, stability later, and stop it running hot all the time.
Targets (Claude's numbers, the user can change them):
- Settings scrolling: at least 58 swaps/s with p95 at or under 20 ms, across 5 boots (good boots 56-57, 20-30 ms; about 1 in 6 boots 42-49).
- Control Center: at least 45 swaps/s (now 38.7).
- Soak: no UI restarts in 30 min of hard scrolling (n7: 1 restart in 60 min).

## Test schedule
- Normal test runs: 15 min at most. A/B comparisons: 5 boots per side once the bench tool exists.
- One 1 h soak per night, after `work/night` is set (9:30 pm or when the user says); results in [gpu-bugs.md](gpu-bugs.md).
- A run with a hang, a backboardd restart, the panel off or 0 swaps is void.
- Stop rule: a gain under 1 ms counts only if a 5-boot A/B confirms it.

## Priority (a couple at a time, dev speed on top; ▶ = being worked; ~half a day max each)
Only items Claude can move now. User tasks are listed below and never gate work; parked ones in docs/backlog.md.
1. ▶ **Heat**: idle fixed (53-59 C; aggregated and coreduetd data moved aside). A55 at 1497 MHz: 5 C cooler, +2.5 swaps/s, now everyday (`sweetrowa55=6`); 1248 MHz raises GPU hangs. Governor (kernel142, `sweetgov=1`) and screen-off clocks work, no heat gain shown: off. A76 at 1939 MHz is 5 C cooler for 0.4 ms. GPU clock gating (xnu 0262, `sweethwcg=1`): idle 3.4 C cooler on the GPU sensor, no speed loss shown (hw2, hw3); goes in the next soak, default if clean. Lowest GPU clock 3.5 C cooler at idle, top DDR vote no change (ddi1). (cpu.md)
2. **CPU cost per frame** (about 14 ms): phone-CPU build tried, no gain (cpu1); redfin knobs 0117-0120: Settings 57, p95 20.2 ms (cp1), on for the soak; low Settings runs (42-49 swaps/s, 1 in 6) are real: the app misses ticks; `sweetfgbig`, xnu 0265, A/B running (fg1). Then cache flush off the render thread (backlog O9, O11). (smoothness.md)
3. ▶ **GPU cost per frame**: at internal 5 nits (not 32) a second full-size pass costs 7 ms per frame, on all apps; slider minimum gives 5 nits; luma boost off fixes it: Settings GPU busy 83 -> 55 %, Home 47.8 -> 53.3 (lb0-lb3, default now); done so far: shadow copy cache, dead fill mode 3: Home 26 -> 52, Control Center 23 -> 37, Settings 50-56 swaps/s (env on the phone, not default). Next: Control Center trace without per-draw tracing (crashes backboardd); `SWEET_CULL=2`: off, more G1 and little gain; Control Center backdrop at quarter size: 33 swaps/s, p95 49 ms (cs1), on the phone under test; Home is redrawn at full size behind Control Center each frame (cc3), but pass hash shows no repeats (ph1): reuse parked; scratch trim no gain (hw2): off; low-draw full-size passes in sysmem next (guess 1-3 ms); widgets page draws at 26 swaps/s, not looked at; 650 MHz gave Home 48.6, Control Center 30.8 but a bad Settings boot (smoothness.md gd1); `sweetddr=1` no gain. Mode 3 and cache: defaults after clean soak n7. (smoothness.md)
4. **Real-use hitches**: recheck the >100 ms gaps every ~3 s (Oct 30, before the recent fixes); time app opens and the keyboard. Then the screen-on CPU governor (redfin 0963, touch and animation at full speed), kept only if the A/B shows no speed loss. (smoothness.md, cpu.md)
5. **Upstream takeaways** (backlog U4-U7, U9, U10; after the Next list below): bridge thread QoS (one line, up to 1 ms, guess); `TU_FETCH_GMEM_MIN_DRAWS=5`/`20` on Control Center (0.5-2 ms, guess; may be worse), 5-boot A/B for both; `SETTLE=90` benches; trace filter env. Then UBWC census, spare image blocks timing.

After these: GPU governor (full and idle levels at most), once the hangs in gpu-bugs.md are under control: GPU clock changes have needed fixes before (xnu 0174). @2x stays spare.

## GPU bugs
Listed with exact log lines and rates in [gpu-bugs.md](gpu-bugs.md) (not auto-loaded). Fix one now only if it voids benches, stops phone testing, or doubles in rate after a change (then turn that change off first). The rest wait until the speed targets are met.

## GPU bug now due
G1 (no-fault hang, no recovery): n7 ran a full hour, 397 k submits, 0 G1, with `sweetsmmutcr=3` (stock SMMU walks and ACTLR), default in kernel153. Watch it in the next soaks. A CX cycle doesn't recover a real G1 (the GDSC refuses, n6), so off. Silent reset n1, n5: not seen in n7; pet crumb on in soaks (gpu.md).

## Phone
Claude's. `work/night` set until the morning session. Everyday setup: kernel154 (sweetgpufreq=6, sweethwcg=1, sweetsmmutcr=3, sweetfencepoll=200 by default) sweetse9=2 sweetcampmic=2 sweetswapbig=6 sweetampreset=1, bundle work/metal2-next (mode 3, cache, knobs on by default), luma boost off.

## Next (in this order)
1. Webcam motion check of mode 3 in the morning (st3 correction; dim screen at night). Defaults installed and checked: 2 boots, 15 min UI run clean (df, smoothness.md).
2. Heat under load: n7 peaked 95.7 C on cpu6, 60 of 730 readings at 90 C or more, 0 caps. When and in which app, and do caps ever fire? A76 at 1939 MHz is 5 C cooler for 0.4 ms (cpu.md hot1).
3. Bridge QoS on the fence bundle: `SWEET_BRIDGE_QOS=1`, 5-boot A/B (guess 0-0.5 ms; backlog U7). Fence poll done: default, 1.3 ms/frame off the A55s (fa, cpu.md).
4. ▶ Low Settings runs (about 1 in 3): `sweetfgbig` didn't help (fg1); 4 of 5 runs after a cooling wait were low: idle check running (smoothness.md).

## User tasks (details in user-tasks.md; nothing waits on these)
proximity by hand (U17), battery discharge (U3), headphone plug test (U15),
Files check (U13), touch while plugged in (U14).
Parked for later (the user, 2026-10-30): Compass accuracy (20-30 deg off, jumps; dm12, recorder on), camera far focus (lens range; backlog Later), auto-brightness (B42), headphone jack iOS side (audio.md jack10b).
Parked for the speed focus (the user, 2026-10-31): speaker volume (B16), garbled images (B44), stale clock (B49), battery + camera CPU (O14), camera polish (B48).
Finished items: [completed.md](completed.md) (not auto-loaded).

## Upstream, not CPU/GPU (the daily check adds here during the speed focus; details in backlog.md)
- Touch poll under 8 ms while touched (hakios-xnu 6d8f818; ~3.5 ms input lag, guess; needs a latency tool).
- RPMh try-lock: retry on EBUSY, a vote can fail silently now (0.5 h; found reading ours for e5b01a0).
- xpcproxy identity version (hakios-xnu 78b518b): first count the errors in one Safari oslog.
- Privilege table (hakios-xnu 502aee8, 2 h): may wake daemons quieted for heat, watch idle CPU.
- Speaker guard for louder sound (315e973, 3-5 h), iOS microphone (2bb073e, 1-2 days, B23 risk): with B16.
- Driver review reports (230762c, 5429d38, 1800069; 2-3 h read-only): SMEM padding underflow found in phone/wifi/smem.c.

## Questions for you (Claude took the recommended path meanwhile)
- PowerLog and CoreDuet databases on the phone were moved aside (not deleted) to /var/local/idlefix-db, to stop the idle CPU burn; `tools/sunfish-idlefix-db.sh --undo` restores them. Battery usage history and app suggestions start fresh. OK to delete them later? (Kept for now.)

## Your tasks at the phone
In [user-tasks.md](user-tasks.md) (not auto-loaded): 11 open. Claude doesn't wait on them; it moves to the next item.

## Dev speed (on top of the priority list when found; ~1 h max each)
- (none)

## Backlog
The Broken, Later and Optimize lists are in [backlog.md](backlog.md) (not auto-loaded).

## Verification
Each item: numbers in the feature Log, this plan and the README status table in the same commit, a journal line,
a webcam check for anything visible, battery ≥ 3600 mV before each boot.
