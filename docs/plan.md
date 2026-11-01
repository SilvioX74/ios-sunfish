# Plan (2026-10-31): speed and heat

Rules: CLAUDE.md ("How to work"). Old plan: `docs/plans/2026-10-31-mixed-priorities.md`. One line per item;
details and history in the feature Logs (`docs/features/<name>.md`).

## Focus (the user, 2026-10-31)
Make the phone fast and smooth first, stability later, and stop it running hot all the time.
Targets (Claude's numbers, the user can change them):
- Settings scrolling: at least 58 swaps/s with p95 at or under 20 ms, across 5 boots (now 50-58, p95 20-31 ms).
- Control Center: at least 45 swaps/s (one run: 26).
- Soak: no UI restarts in 30 min of hard scrolling (now about one per 6 min).

## Test schedule
- Normal test runs: 15 min at most. A/B comparisons: 5 boots per side once the bench tool exists.
- One 1 h soak per night, after `work/night` is set (9:30 pm or when the user says); results in [gpu-bugs.md](gpu-bugs.md).
- A run with a hang, a backboardd restart, the panel off or 0 swaps is void.
- Stop rule: a gain under 1 ms counts only if a 5-boot A/B confirms it.

## Priority (a couple at a time, dev speed on top; ▶ = being worked; ~half a day max each)
Only items Claude can move now. Items waiting on the user are in "Waiting on you" below; parked ones in docs/backlog.md.
1. ▶ **Heat**: idle fixed (53-59 C; aggregated and coreduetd data moved aside). A55 at 1497 MHz: 5 C cooler, +2.5 swaps/s, now everyday (`sweetrowa55=6`); 1248 MHz raises GPU hangs. Next: DDR vote, A76 rows, screen-on governor. Screen-off clocks work, no gain shown. (cpu.md)
2. **CPU cost per frame** (about 14 ms): phone-CPU build tried, no gain (cpu1); redfin 0130-0137, unchanged-draw fast path, cache flush off the render thread, app and CA threads on the A76s (backlog O9, O11). (smoothness.md)
3. ▶ **GPU cost per frame**: done so far: shadow copy cache, dead fill mode 3: Home 26 -> 41-44, Control Center 23 -> 29, Settings 50-56 swaps/s (env on the phone, not default). Next: Control Center trace without per-draw tracing (crashes backboardd); bright against minimum A/B (Home 38 against 31; drag the slider live with `SWEET_DFWHY=1`; research/notes/home-cc-gpu-2026-10-31.md); then `SWEET_CULL=2` for the fill under the wallpaper; a trace of Control Center opening and closing; 650 MHz gave Home 48.6, Control Center 30.8 but a bad Settings boot (smoothness.md gd1); `sweetddr=1` no gain. Mode 3 and cache become defaults after U22. (smoothness.md)
4. **Real-use hitches**: recheck the >100 ms gaps every ~3 s (Oct 30, before the recent fixes); time app opens and the keyboard. Then the screen-on CPU governor (redfin 0963, touch and animation at full speed), kept only if the A/B shows no speed loss. (smoothness.md, cpu.md)

After these: GPU governor (full and idle levels at most), once the hangs in gpu-bugs.md are under control: GPU clock changes have needed fixes before (xnu 0174). @2x stays spare.

## GPU bugs
Listed with exact log lines and rates in [gpu-bugs.md](gpu-bugs.md) (not auto-loaded). Fix one now only if it voids benches, stops phone testing, or doubles in rate after a change (then turn that change off first). The rest wait until the speed targets are met.

## Waiting on you (details in user-tasks.md; Claude moves on meanwhile)
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
