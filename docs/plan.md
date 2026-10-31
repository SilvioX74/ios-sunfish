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
1. ▶ **A/B bench tool** (dev speed, ~1 h): `tools/sunfish-perfab.sh`: boots each config N times, runs fixed screens (Settings, Home, Control Center, app open, Safari), prints one table, marks void runs, updates the counts in gpu-bugs.md. Boots with one bundle averaged 53 and 58 swaps/s, so 2 boots per side is too few. (smoothness.md)
2. **Heat and real clocks**: temperature and clock log done (xnu 0255, kernel136). Measured: idle 66-72 C, scrolling 95 C in 3.5 min; above ~94 C the chip cuts the A76 from 2208 to ~1974 MHz and swaps/s falls 10 %, so heat costs speed. Next: idle CPU load (A55 50-70 % busy with nothing running), `flamecpufreq=1200` idle A/B, low speed with the screen off (xnu 0242 flag), DDR vote. Keep 15-s benches below 75 C start. (cpu.md)
3. **CPU cost per frame** (about 14 ms): build bridge and Mesa for the phone's CPU (the bridge sets no target-cpu; untested), redfin 0130-0137, unchanged-draw fast path, cache flush off the render thread, app and CA threads on the A76s (backlog O9, O11). (smoothness.md)
4. **GPU cost per frame** (about 13 ms): blur-heavy screens first; re-measure the 5 draws that were 52 % of draw time (trace before dead fill); 650 MHz (O16, +11 % earlier; 800 fails); retest `sweetddr=1` (last tried Oct 24 on the old present path); fill as a clear (mode 2) only if clean. (smoothness.md)
5. **Real-use hitches**: recheck the >100 ms gaps every ~3 s (Oct 30, before the recent fixes); time app opens and the keyboard. Then the screen-on CPU governor (redfin 0963, touch and animation at full speed), kept only if the A/B shows no speed loss. (smoothness.md, cpu.md)

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
In [user-tasks.md](user-tasks.md) (not auto-loaded): 10 open. Claude doesn't wait on them; it moves to the next item.

## Dev speed (on top of the priority list when found; ~1 h max each)
- A/B bench tool: priority 1.

## Backlog
The Broken, Later and Optimize lists are in [backlog.md](backlog.md) (not auto-loaded).

## Verification
Each item: numbers in the feature Log, this plan and the README status table in the same commit, a journal line,
a webcam check for anything visible, battery ≥ 3600 mV before each boot.
