#!/usr/bin/env python3
"""Video stage 3 (docs/features/video.md "Stage 3 spec"): compare `venusctl decode` output against a reference decode.

  tools/sunfish-venus-yuvcmp.py REF.nv12 OUT.yuv WIDTH HEIGHT [--frames N] [--min-psnr DB]

Both files are plain NV12 (Y WIDTH*HEIGHT, then interleaved CbCr WIDTH*HEIGHT/2 per frame), as venusctl writes and
`ffmpeg -i IN.h264 -f rawvideo -pix_fmt nv12 REF.nv12` makes. Per frame: exact luma bytes, PSNR of Y and of CbCr,
the luma CRC32 of both (zlib crc32, the value venusctl prints). Exit 0 when every frame OUT has is in REF and its
Y PSNR is at least --min-psnr (default 40 dB; identical = inf), 1 otherwise. Stdlib only.
"""
import argparse
import math
import sys
import zlib


def psnr(a, b):
    if a == b:
        return math.inf
    se = sum((x - y) * (x - y) for x, y in zip(a, b))
    return 10 * math.log10(255 * 255 * len(a) / se)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("ref")
    ap.add_argument("out")
    ap.add_argument("width", type=int)
    ap.add_argument("height", type=int)
    ap.add_argument("--frames", type=int, default=0, help="compare at most N frames (0: all in OUT)")
    ap.add_argument("--min-psnr", type=float, default=40.0)
    a = ap.parse_args()
    ylen = a.width * a.height
    flen = ylen * 3 // 2
    with open(a.ref, "rb") as f:
        ref = f.read()
    with open(a.out, "rb") as f:
        out = f.read()
    n = len(out) // flen
    if len(out) % flen:
        print(f"note: OUT has {len(out) % flen} trailing bytes (not a whole {a.width}x{a.height} NV12 frame)")
    if a.frames:
        n = min(n, a.frames)
    if not n:
        print("FAIL: no whole frame in OUT")
        return 1
    ok = True
    print("frame  luma exact        Y PSNR   CbCr PSNR  crc32 ref crc32 out")
    for i in range(n):
        o = out[i * flen:(i + 1) * flen]
        r = ref[i * flen:(i + 1) * flen]
        if len(r) < flen:
            print(f"{i:5d}  not in REF ({len(ref) // flen} frames)")
            ok = False
            continue
        exact = sum(1 for x, y in zip(o[:ylen], r[:ylen]) if x == y)
        py, pc = psnr(o[:ylen], r[:ylen]), psnr(o[ylen:], r[ylen:])
        good = py >= a.min_psnr
        ok &= good
        print(f"{i:5d}  {exact:7d}/{ylen:<7d} {py:7.2f} dB {pc:7.2f} dB  {zlib.crc32(r[:ylen]):08x}  "
              f"{zlib.crc32(o[:ylen]):08x}{'' if good else '  LOW'}")
    print(f"{'ok' if ok else 'FAIL'}: {n} frames, Y PSNR >= {a.min_psnr} dB {'everywhere' if ok else 'not everywhere'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
