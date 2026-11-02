# Plan (2026-10-31): speed and heat

Rules: CLAUDE.md ("How to work"). Old plan: `docs/plans/2026-10-31-mixed-priorities.md`. One line per item;
details and history in the feature Logs (`docs/features/<name>.md`).

## Focus (the user, 2026-10-31)
Make the phone fast and smooth first, stability later, and stop it running hot all the time.
Targets (Claude's numbers, the user can change them):
- Settings scrolling: at least 58 swaps/s with p95 at or under 20 ms, across 5 boots (now 57.0, p95 20.2 ms, nearly there).
- Control Center: at least 45 swaps/s (now 36).
- Soak: no UI restarts in 30 min of hard scrolling (now about one per 6 min).

## Test schedule
- Normal test runs: 15 min at most. A/B comparisons: 5 boots per side once the bench tool exists.
- One 1 h soak per night, after `work/night` is set (9:30 pm or when the user says); results in [gpu-bugs.md](gpu-bugs.md).
- A run with a hang, a backboardd restart, the panel off or 0 swaps is void.
- Stop rule: a gain under 1 ms counts only if a 5-boot A/B confirms it.

## Priority (a couple at a time, dev speed on top; ▶ = being worked; ~half a day max each)
Only items Claude can move now. User tasks are listed below and never gate work; parked ones in docs/backlog.md.
1. ▶ **Heat**: idle fixed (53-59 C; aggregated and coreduetd data moved aside). A55 at 1497 MHz: 5 C cooler, +2.5 swaps/s, now everyday (`sweetrowa55=6`); 1248 MHz raises GPU hangs. Governor (kernel142, `sweetgov=1`) and screen-off clocks work, no heat gain shown: off. A76 at 1939 MHz is 5 C cooler for 0.4 ms. GPU clock gating (xnu 0262, `sweethwcg=1`): idle 3.4 C cooler on the GPU sensor, no speed loss shown (hw2, hw3); goes in the next soak, default if clean. Lowest GPU clock 3.5 C cooler at idle, top DDR vote no change (ddi1). (cpu.md)
2. **CPU cost per frame** (about 14 ms): phone-CPU build tried, no gain (cpu1); redfin knobs 0117-0120: Settings 57, p95 20.2 ms (cp1), on for the soak; low Settings runs (42-49 swaps/s, 1 in 6) are real: the app misses ticks; `sweetfgbig`, xnu 0265, A/B running (fg1). Then cache flush off the render thread (backlog O9, O11). (smoothness.md)
3. ▶ **GPU cost per frame**: at internal 5 nits (not 32) a second full-size pass costs 7 ms per frame, on all apps; slider minimum gives 5 nits now, luma boost test is plan Next 2; done so far: shadow copy cache, dead fill mode 3: Home 26 -> 52, Control Center 23 -> 37, Settings 50-56 swaps/s (env on the phone, not default). Next: Control Center trace without per-draw tracing (crashes backboardd); `SWEET_CULL=2`: off, more G1 and little gain; Control Center backdrop at quarter size: 33 swaps/s, p95 49 ms (cs1), on the phone under test; Home is redrawn at full size behind Control Center each frame (cc3), but pass hash shows no repeats (ph1): reuse parked; scratch trim no gain (hw2): off; low-draw full-size passes in sysmem next (guess 1-3 ms); widgets page draws at 26 swaps/s, not looked at; 650 MHz gave Home 48.6, Control Center 30.8 but a bad Settings boot (smoothness.md gd1); `sweetddr=1` no gain. Mode 3 and cache become defaults once tonight's soak is clean (U22 no longer gates this). (smoothness.md)
4. **Real-use hitches**: recheck the >100 ms gaps every ~3 s (Oct 30, before the recent fixes); time app opens and the keyboard. Then the screen-on CPU governor (redfin 0963, touch and animation at full speed), kept only if the A/B shows no speed loss. (smoothness.md, cpu.md)
5. **Upstream takeaways** (backlog U4-U7, U9, U10; after the Next list below): bridge thread QoS (one line, up to 1 ms, guess); `TU_FETCH_GMEM_MIN_DRAWS=5`/`20` on Control Center (0.5-2 ms, guess; may be worse), 5-boot A/B for both; `SETTLE=90` benches; trace filter env. Then UBWC census, spare image blocks timing.

After these: GPU governor (full and idle levels at most), once the hangs in gpu-bugs.md are under control: GPU clock changes have needed fixes before (xnu 0174). @2x stays spare.

## GPU bugs
Listed with exact log lines and rates in [gpu-bugs.md](gpu-bugs.md) (not auto-loaded). Fix one now only if it voids benches, stops phone testing, or doubles in rate after a change (then turn that change off first). The rest wait until the speed targets are met.

## GPU bug now due
G1 (no-fault hang, no recovery) voids benches today, 4 of about 40 boots: it qualifies for a fix. Stuck memory path, not our lock: GBIF never acks (24 of 24 failed, ack 3: 8 of 8 recovered). Flush hammer (hm1): flushes aren't the trigger; both SMMU syncs stuck, no fault. CX-cycle recovery `sweetcxcycle` (what Linux does and we don't): works on an injected hang, kernel149 (cx3); a real G1 not met yet (gpu.md).

## Phone
Claude's. `work/night` set. Last everyday setup: kernel144 sweetse9=2 sweetcampmic=2 sweetswapbig=6 sweetampreset=1 sweetrowa55=6 sweetgpufreq=6 sweethwcg=1, no flamereboot; env SWEET_DEADFILL=3 M2V_SHADOW_CACHE=1 SWEET_U0011..14=1; bundle work/metal2-cpu. Bundle for after a clean soak: work/metal2-next (mode 3, cache, knobs on by default), not installed.

## Next (in this order)
1. ▶ Soak n6 (22:43, same setup as n5, log work/nightsoak-n6.log): does a real G1 recover by CX cycle? Injected hang did: rc 0, 93 ms, UI restarts once (cx3, gpu.md). n5 ended in a silent reset at 11 min in Maps, 54 MB image churn at the GPU pool limit (gpu.md n5). While it runs: read-only work and builds only.
2. Luma boost A/B at 5 nits as soon as n6 ends: `CA_DISABLE_LUMA_BOOST=1`, the biggest lead so far (about 7 ms per frame, up to 6 C, guess about 60 %; smoothness.md 22:50). Bench baseline: boots now start at 5 nits, so Settings 57 won't reproduce until this is settled.
3. Clean (no silent reset, no failed recovery): defaults with tools/sunfish-install-speed.sh, sweetgpufreq, sweethwcg and sweetcxcycle=1 default on, webcam motion check of mode 3 (st3 correction). One G1 can't blame clock gating (1 in 31 k against 1 per about 100 k); if n5 also hangs early, count G1 per submits without `sweethwcg`.
4. fg1 again in full, kernel146, read `SWEET SCHED: fg` per window: do low Settings runs show more A55 time? (smoothness.md 20:10.)
5. Fence poll: kernel with xnu fence-poll patch, `sweetfencepoll=200`, Turnip 100 ms slice: 5-boot A/B, waiter tid's ms, A55 temperature, p95 (read-only: about 2 ms/frame of A55 polling, a heat item, guess under 0.5 C; backlog O11). Bridge QoS `SWEET_BRIDGE_QOS=1`, 5-boot A/B (guess 0-0.5 ms; backlog U7).

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
