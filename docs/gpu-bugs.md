# GPU bugs (started 2026-10-31)

Known GPU bugs and how often they happen, so a jump in rate shows which day and change brought it. Not loaded
automatically. Details stay in [features/gpu.md](features/gpu.md); the plan is in [plan.md](plan.md).

Fix a bug now only if it:
1. voids benches,
2. stops phone testing, or
3. doubles in rate after a change (turn that change off first).

Everything else waits until the speed targets in the plan are met. `tools/sunfish-perfab.sh` will update the
counts once it exists (plan priority 1); until then, add them by hand after each soak.

## Bugs
| ID | Bug | Log line to count | Latest rate or count | Recovers? | Fix now? |
|---|---|---|---|---|---|
| G1 | No-fault hang, stuck TLB sync | `GPU hang or fault` with `fsr 0`, `tlb sync -> timeout` | kernel134: 2 in ~113 k submits; kernel135: 0 in 151 k | No, 0 of 8; reboot needed | Only if it voids benches |
| G2 | Submit error, backboardd restarts | `submit failed: 0xe00002c2` | kernel135: 3 in ~34 min (151 k submits) | Restart, then G3 | Yes if it voids benches |
| G3 | Slower mode after a backboardd restart | 2nd submit about 6 ms slower | Every restart seen (m1, z1) | Reboot | Yes if it voids benches |
| G4 | SMMU-fault hang | `GPU hang or fault` with `fsr 40000002` | kernel135: 1 in 151 k | Yes, ~28 ms | No |
| G5 | Boot with no GPU | 0 `SWEET GPU` lines, backboardd's Metal bridge closed | Once (r1, kernel134, 20261031-035915) | Reboot | Only if it returns |
| G6 | DeviceLost after Camera launches | `commandBuffer refused: command buffer: DeviceLost` | Once (kernel127, 2026-10-30) | Killing backboardd | Only if it returns |
| G7 | Planar surface refused | `planar iosurface refused` | Seen with G6; effect unknown | Unknown | No |
| G8 | Reduce Transparency hang | Hang right after turning it on | Not reproduced on kernel52 | Cause unproven | No |

Failed recoveries leave `device refused: OutOfMemory(Device)` and CPU drawing until reboot (G1). CPU drawing can reach
60 swaps/s on some screens, so a good speed number alone doesn't prove the GPU ran. Silent resets are backlog B4.

## Daily soak (1 h, overnight)
Newest first. Earlier soaks were shorter or used other loads, so compare rates, not raw counts.

| Date | Kernel, bundle | Length | Submits | G1 | G2 | G4 | Other |
|---|---|---|---|---|---|---|---|
| 2026-10-31 z1 | kernel135, allocator | ~34 min, stopped early | 151 k | 0 | 3 | 1 | - |
| 2026-10-31 soak1 | kernel134, allocator | 2 boots | ~113 k | 2 | at least 1 | not counted | - |
