# Plan (2026-10-31, updated 2026-11-02 09:10): speed and heat

Rules: CLAUDE.md ("How to work"). Old plan: `docs/plans/2026-10-31-mixed-priorities.md`. One line per item;
details and history in the feature Logs (`docs/features/<name>.md`).

## Focus (the user, 2026-10-31)
Make the phone fast and smooth first, stability later, and stop it running hot all the time.
Targets (Claude's numbers, the user can change them):
- Settings scrolling: at least 58 swaps/s with p95 at or under 20 ms, across 5 boots: p95 met (19.3, kernel157 sweetvsyncphase, 0 low launches in 25); 57.0 swaps/s is this bench's cap.
- Control Center: at least 45 swaps/s (now 40.3).
- Soak: no UI restarts in 30 min of hard scrolling (n7: 1 restart in 60 min).

## Test schedule
- Normal test runs: 15 min at most. A/B comparisons: 5 boots per side once the bench tool exists.
- One 1 h soak per night, after `work/night` is set (9:30 pm or when the user says); results in [gpu-bugs.md](gpu-bugs.md).
- A run with a hang, a backboardd restart, the panel off or 0 swaps is void.
- Stop rule: a gain under 1 ms counts only if a 5-boot A/B confirms it.

## Priority (a couple at a time, dev speed on top; ▶ = being worked; ~half a day max each)
Only items Claude can move now. User tasks are listed below and never gate work; parked ones in docs/backlog.md.
1. ▶ **Heat**: idle 53-59 C; load peaks 93-96 C. Hottest app: Photos (hs1: max 95.7, 9 of its 22 readings at 90+; Safari 92.2; others under 90). Defaults in: A55 1497 MHz, GPU clock gating, fence poll 200 us. Next: what Photos burns (per-thread CPU, GPU per frame), then A76 at 1939 MHz for that load (5 C cooler for 0.4 ms, cpu.md hot1). (cpu.md)
2. **Control Center** 39.1 -> 40.3 swaps/s with `TU_FETCH_GMEM_MIN_DRAWS=20` (default now, cs4; target 45); trace without per-draw tracing (it crashes backboardd); Home redrawn at full size behind it (cc3). (smoothness.md)
3. **Real-use hitches**: recheck the >100 ms gaps every ~3 s (Oct 30); time app opens and the keyboard. (smoothness.md, cpu.md)
4. **Render CPU**: backboardd's render_for_time takes 13.2 ms of each 16.7 ms tick on Settings (catrace lat); what it spends that on (profile tick+0..13 with tools/sunfish-pc-hist.py). (smoothness.md)
5. **Upstream takeaways**: trace filter env (U6: works, still distorts, parked); UBWC census (U9), spare image blocks timing (U10). QoS (U7) no gain, off.

After these: GPU governor (full and idle levels at most), once the hangs in gpu-bugs.md are under control: GPU clock changes have needed fixes before (xnu 0174). @2x stays spare.

## GPU bugs
Listed with exact log lines and rates in [gpu-bugs.md](gpu-bugs.md) (not auto-loaded). Fix one now only if it voids benches, stops phone testing, or doubles in rate after a change (then turn that change off first). The rest wait until the speed targets are met.

## GPU bug now due
G1 (no-fault hang, no recovery): n7 ran a full hour, 397 k submits, 0 G1, with `sweetsmmutcr=3` (stock SMMU walks and ACTLR), default in kernel153. Watch it in the next soaks. A CX cycle doesn't recover a real G1 (the GDSC refuses, n6), so off. Silent reset n1, n5: not seen in n7; pet crumb on in soaks (gpu.md).

## Phone
Claude's (the user is up; `work/night` removed 2026-11-02 06:24). Everyday setup: kernel157 (sweetgpufreq=6, sweethwcg=1, sweetsmmutcr=3, sweetfencepoll=200, sweetvsyncphase=5000 compiled in) sweetse9=2 sweetcampmic=2 sweetswapbig=6 sweetampreset=1, no flamereboot; bundle work/metal2-next (mode 3, cache, U0011..14, Turnip 0110 slice); luma boost off; /var/local/mtl2-env = SWEET_U0011..14=1, TU_FETCH_GMEM_MIN_DRAWS=20. kernel155 = 154 + `sweetvsyncgrid` (off; no gain); kernel156 = 0274 boot-arg only. Live-log listener restarted 2026-11-02 06:29 (usb0 link-state fix running). Phone booted hs1 (kernel157 everyday setup).

## Next (in this order)
1. Heat: per-thread CPU and GPU time in a Photos scroll window (kernel157 everyday).
2. Tonight's soak (n8): `tools/sunfish-nightsoak.sh n8` (boots the everyday setup, kernel157 + `sweetwdtcrumb=1`).

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
