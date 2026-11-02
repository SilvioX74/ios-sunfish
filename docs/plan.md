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
1. ▶ **Heat**: idle fixed (53-59 C; aggregated and coreduetd data moved aside). A55 at 1497 MHz: 5 C cooler, +2.5 swaps/s, now everyday (`sweetrowa55=6`); 1248 MHz raises GPU hangs. Governor (kernel142, `sweetgov=1`) and screen-off clocks work, no heat gain shown: off. A76 at 1939 MHz is 5 C cooler for 0.4 ms. GPU clock gating was never on: xnu 0262, kernel143, `sweethwcg=1`, first run going. Idle: lowest GPU clock 3.5 C cooler, top DDR vote no change (ddi1). (cpu.md)
2. **CPU cost per frame** (about 14 ms): phone-CPU build tried, no gain (cpu1); redfin knobs 0117-0120: Settings 57, p95 20.2 ms (cp1), on for the soak; unchanged-draw fast path, cache flush off the render thread, app and CA threads on the A76s (backlog O9, O11). (smoothness.md)
3. ▶ **GPU cost per frame**: at internal 5 nits (not 32) a second full-size pass costs 7 ms per frame: couldn't get back to that state on demand; done so far: shadow copy cache, dead fill mode 3: Home 26 -> 52, Control Center 23 -> 37, Settings 50-56 swaps/s (env on the phone, not default). Next: Control Center trace without per-draw tracing (crashes backboardd); `SWEET_CULL=2`: off, more G1 and little gain; Control Center backdrop at quarter size: 33 swaps/s, p95 49 ms (cs1), on the phone under test; next: one traced boot of the bench gesture (never traced; needs about 6 ms off heavy frames), redfin 0111 scratch trim being ported; 650 MHz gave Home 48.6, Control Center 30.8 but a bad Settings boot (smoothness.md gd1); `sweetddr=1` no gain. Mode 3 and cache become defaults once tonight's soak is clean (U22 no longer gates this). (smoothness.md)
4. **Real-use hitches**: recheck the >100 ms gaps every ~3 s (Oct 30, before the recent fixes); time app opens and the keyboard. Then the screen-on CPU governor (redfin 0963, touch and animation at full speed), kept only if the A/B shows no speed loss. (smoothness.md, cpu.md)

After these: GPU governor (full and idle levels at most), once the hangs in gpu-bugs.md are under control: GPU clock changes have needed fixes before (xnu 0174). @2x stays spare.

## GPU bugs
Listed with exact log lines and rates in [gpu-bugs.md](gpu-bugs.md) (not auto-loaded). Fix one now only if it voids benches, stops phone testing, or doubles in rate after a change (then turn that change off first). The rest wait until the speed targets are met.

## GPU bug now due
G1 (no-fault hang, no recovery) voids benches today, 4 of about 40 boots: it qualifies for a fix. Stuck memory path, not our lock: GBIF never acks (24 of 24 failed, ack 3: 8 of 8 recovered). Flush hammer (hm1): flushes aren't the trigger; both SMMU syncs stuck, no fault. Next: CX-cycle recovery `sweetcxcycle`, what Linux does and we don't (gpu.md).

## Tonight's soak (after `work/night`, 9:30 pm)
Already started and waiting (`tools/sunfish-nightsoak.sh n3`, detached, survives any session; don't kill or restart it, stay off the phone from 21:25 to about 22:35). It reboots the phone at 21:30. Result: work/nightsoak-n3.log, done at the "finished" line; nothing wakes a session, so check it after 22:35.
One 1 h soak with `tools/sunfish-uisoak.sh`: kernel139, sweetse9=2 sweetcampmic=2 sweetswapbig=6 sweetampreset=1 sweetrowa55=6 sweetgpufreq=6, no flamereboot; env SWEET_DEADFILL=3 M2V_SHADOW_CACHE=1 SWEET_U0011..14=1; bundle work/metal2-cpu; slider down first. Clean (no silent reset, no failed recovery): make these defaults with tools/sunfish-install-speed.sh, a rebuilt `METAL2_VARIANT=next` including 0117-0120 with knobs on by default, and kernel141 + sweetgpufreq default.

## User tasks (details in user-tasks.md; nothing waits on these)
proximity by hand (U17), battery discharge (U3), headphone plug test (U15),
Files check (U13), touch while plugged in (U14).
Parked for later (the user, 2026-10-30): Compass accuracy (20-30 deg off, jumps; dm12, recorder on), camera far focus (lens range; backlog Later), auto-brightness (B42), headphone jack iOS side (audio.md jack10b).
Parked for the speed focus (the user, 2026-10-31): speaker volume (B16), garbled images (B44), stale clock (B49), battery + camera CPU (O14), camera polish (B48).
Finished items: [completed.md](completed.md) (not auto-loaded).

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
