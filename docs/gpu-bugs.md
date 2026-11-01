# GPU bugs (started 2026-10-31)

Known GPU bugs and how often they happen, so a jump in rate shows which day and change brought it. Not loaded
automatically. Details stay in [features/gpu.md](features/gpu.md); the plan is in [plan.md](plan.md).

Fix a bug now only if it:
1. voids benches,
2. stops phone testing, or
3. doubles in rate after a change (turn that change off first).

Everything else waits until the speed targets in the plan are met. `tools/sunfish-perfab.sh` adds the counts of
each of its runs to "Bench runs" below; add the counts by hand after each soak.

## Bugs
| ID | Bug | Log line to count | Latest rate or count | Recovers? | Fix now? |
|---|---|---|---|---|---|
| G1 | No-fault hang, stuck TLB sync | `GPU hang or fault` with `fsr 0`, `tlb sync -> timeout` | kernel134: 2 in ~113 k submits; kernel135: 0 in 151 k; kernel138-139: 2 in ~380 k; sweetrowa55=4: 3 in ~43 k with mode 2 (rows2), 0 in ~81 k with mode 3 (mf3) | No, 0 of 8; reboot needed | Only if it voids benches |
| G2 | Submit error, backboardd restarts | `submit failed: 0xe00002c2` | kernel135: 3 in ~34 min (151 k submits) | Restart, then G3 | Yes if it voids benches |
| G3 | Slower mode after a backboardd restart | 2nd submit about 6 ms slower | Every restart seen (m1, z1) | Reboot | Yes if it voids benches |
| G4 | SMMU-fault hang | `GPU hang or fault` with `fsr 40000002` | kernel135: 2 in 151 k (z1 boot 20261031-073304; first listed as 1) | Yes, ~28 ms | No |
| G5 | Boot with no GPU | 0 `SWEET GPU` lines, backboardd's Metal bridge closed | Once (r1, kernel134, 20261031-035915) | Reboot | Only if it returns |
| G6 | DeviceLost after Camera launches | `commandBuffer refused: command buffer: DeviceLost` | Once (kernel127, 2026-10-30) | Killing backboardd | Only if it returns |
| G7 | Planar surface refused | `planar iosurface refused` | Seen with G6; effect unknown | Unknown | No |
| G8 | Reduce Transparency hang | Hang right after turning it on | Not reproduced on kernel52 | Cause unproven | No |

Failed recoveries leave `device refused: OutOfMemory(Device)` and CPU drawing until reboot (G1). CPU drawing can reach
60 swaps/s on some screens, so a good speed number alone doesn't prove the GPU ran. Silent resets are backlog B4.

## Bench runs
Written by `tools/sunfish-perfab.sh`, newest first. Void runs are screen runs, not boots.

| Date | Tag | Kernels and configs | Boots | Submits | G1 | G2 | G4 | Void runs |
|---|---|---|---|---|---|---|---|---|
| 2026-11-01 | cs1 | cs=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,sweetrowa55=6,+SWEET_DEADFILL=3,+M2V_SHADOW_CACHE=1,+SWEET_CULL=2 | 4 | 36 k | 0 | 0 | 0 | 0 |
| 2026-11-01 | cu1 | c0=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,sweetrowa55=6,+SWEET_DEADFILL=3,+M2V_SHADOW_CACHE=1,@work/metal2-shadow/out/phone/metal2/out; c2=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,sweetrowa55=6,+SWEET_DEADFILL=3,+M2V_SHADOW_CACHE=1,@work/metal2-shadow/out/phone/metal2/out,+SWEET_CULL=2 | 8 | 67 k | 1 | 0 | 0 | 2 |
| 2026-11-01 | gd1 | base=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,sweetrowa55=6,+SWEET_DEADFILL=3,+M2V_SHADOW_CACHE=1; gpu6=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,sweetrowa55=6,+SWEET_DEADFILL=3,+M2V_SHADOW_CACHE=1,sweetgpufreq=6; ddr1=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,sweetrowa55=6,+SWEET_DEADFILL=3,+M2V_SHADOW_CACHE=1,sweetddr=1 | 12 | 106 k | 0 | 0 | 0 | 2 |
| 2026-11-01 | mf3 | fl1=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,sweetrowa55=4,+SWEET_DEADFILL=3,+M2V_SHADOW_CACHE=1; fl0=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,sweetrowa55=4,+SWEET_DEADFILL=3,+M2V_SHADOW_CACHE=1,sweetmapflush=0 | 16 | 81 k | 0 | 0 | 0 | 1 |
| 2026-11-01 | d4 | df2=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,sweetrowa55=6,+M2V_SHADOW_CACHE=1,@work/metal2-shadow/out/phone/metal2/out,+SWEET_DEADFILL=2; df3=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,sweetrowa55=6,+M2V_SHADOW_CACHE=1,@work/metal2-shadow/out/phone/metal2/out,+SWEET_DEADFILL=3 | 10 | 83 k | 0 | 0 | 0 | 1 |
| 2026-11-01 | d3 | df2=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,sweetrowa55=6,+M2V_SHADOW_CACHE=1,@work/metal2-shadow/out/phone/metal2/out,+SWEET_DEADFILL=2; df3=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,sweetrowa55=6,+M2V_SHADOW_CACHE=1,@work/metal2-shadow/out/phone/metal2/out,+SWEET_DEADFILL=3 | 6 | 49 k | 0 | 0 | 0 | 0 |
| 2026-11-01 | dw1 | dw=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,sweetrowa55=6,+SWEET_DEADFILL=2,+M2V_SHADOW_CACHE=1,+SWEET_DFWHY=1 | 1 | 4 k | 0 | 0 | 0 | 0 |
| 2026-11-01 | nd1 | nd=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,sweetrowa55=6,+SWEET_DEADFILL=2,+M2V_SHADOW_CACHE=1 | 3 | 11 k | 0 | 0 | 0 | 0 |
| 2026-11-01 | mf2 | fl1=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,sweetrowa55=6,+SWEET_DEADFILL=2,+M2V_SHADOW_CACHE=1; fl0=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,sweetrowa55=6,+SWEET_DEADFILL=2,+M2V_SHADOW_CACHE=1,sweetmapflush=0 | 6 | 49 k | 0 | 0 | 0 | 0 |
| 2026-11-01 | mf1 | m0=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,sweetrowa55=4,sweetmapflush=0,+SWEET_DEADFILL=2,+M2V_SHADOW_CACHE=1 | 8 | 35 k | 0 | 0 | 0 | 0 |
| 2026-11-01 | sy1 | gmem=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,sweetrowa55=6,+SWEET_DEADFILL=2,+M2V_SHADOW_CACHE=1; sysmem=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,sweetrowa55=6,+SWEET_DEADFILL=2,+M2V_SHADOW_CACHE=1,+TU_DEBUG=sysmem | 4 | 20 k | 0 | 0 | 1 | 5 |
| 2026-11-01 | rows2 | r6=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,+SWEET_DEADFILL=2,+M2V_SHADOW_CACHE=1,sweetrowa55=6; r4=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,+SWEET_DEADFILL=2,+M2V_SHADOW_CACHE=1,sweetrowa55=4 | 10 | 86 k | 3 | 0 | 0 | 9 |
| 2026-10-31 | sh1 | s0=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,+SWEET_DEADFILL=2,@work/metal2-shadow/out/phone/metal2/out; s1=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,+SWEET_DEADFILL=2,@work/metal2-shadow/out/phone/metal2/out,+M2V_SHADOW_CACHE=1; s2=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,+SWEET_DEADFILL=2,@work/metal2-shadow/out/phone/metal2/out,+M2V_SHADOW_CACHE=2 | 9 | 78 k | 0 | 0 | 0 | 0 |
| 2026-10-31 | rows1 | base=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,+SWEET_DEADFILL=2; a55l=139,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,+SWEET_DEADFILL=2,sweetrowa55=6 | 10 | 55 k | 0 | 0 | 0 | 2 |
| 2026-10-31 | df5 | base=138,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1; df2=138,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,+SWEET_DEADFILL=2 | 10 | 69 k | 1 | 0 | 0 | 4 |
| 2026-10-31 | cpu1 | base=137,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1; a76=137,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1,@work/metal2-a76/out/phone/metal2/out | 10 | 100 k | 0 | 0 | 0 | 2 |
| 2026-10-31 | smoke3 | base=137,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1 | 1 | 10 k | 0 | 0 | 0 | 0 |
| 2026-10-31 | smoke1 | base=137,sweetswapbig=6,sweetse9=2,sweetcampmic=2,sweetampreset=1 | 1 | 9 k | 0 | 0 | 0 | 0 |

## Daily soak (1 h, overnight)
Newest first. Earlier soaks were shorter or used other loads, so compare rates, not raw counts.

| Date | Kernel, bundle | Length | Submits | G1 | G2 | G4 | Other |
|---|---|---|---|---|---|---|---|
| 2026-11-01 n2 | kernel139, shadow cache, mode 2, A55 1497 MHz | 14.5 min valid of 60; G1, failed recovery | about 86 k total | 1 | 0 | 0 | CPU drawing for the rest; no silent reset |
| 2026-10-31 n1 | kernel139, shadow cache, mode 2, A55 1497 MHz | 24.5 min, silent reset | 127 k | 0 | 0 | 0 | Reset to fastboot, no panic line |
| 2026-10-31 z1 | kernel135, allocator | ~34 min, stopped early | 151 k | 0 | 3 | 2 | - |
| 2026-10-31 soak1 | kernel134, allocator | 2 boots | ~113 k | 2 | at least 1 | not counted | - |
