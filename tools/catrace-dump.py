#!/usr/bin/env python3
"""catrace-dump.py OUT.bin [--pair PID] [--list PID]: read a phone/tools/catrace file (Core Animation kdebug points).

  python3 -I tools/flame-files.py get /var/local/catrace.bin work/catrace.bin; python3 -I tools/catrace-dump.py work/catrace.bin
Default: per process (pid, name) the count, rate and median gap of each debugid. --list PID prints that pid's events
(time in us from the trace start, args). --pair PID (the client app): each of its commits (0x31ca0215, arg1 context id,
arg2 frame begin) is matched to the server's next 0x31ca0009 (commit receipt) with the same context id; then
  recv->next render   receipt to the next 0x31ca000d (render_for_time)
  prev render->recv   the render before the receipt to the receipt
  fb - last render    the commit's frame begin minus the latest render at or before the commit
with p10/p50/p90 and a histogram in 1 ms bins. Frame begin: a raw double (CACurrentMediaTime seconds) when the bits
read as one, else mach ticks. Threads born during the trace get their pid from the kernel's new-thread events.
"""
import bisect, collections, struct, sys

NAMES = {0x31ca0009: 'server commit recv', 0x31ca0215: 'client commit start', 0x31ca0216: 'client commit end',
         0x31ca0005: 'Transaction::commit', 0x31ca000d: 'render_for_time', 0x31ca00c9: 'displaylink dispatch start',
         0x31ca00ca: 'displaylink dispatch end', 0x31ca0104: 'signpost', 0x31ca00d4: 'set_frame_begin_time',
         0x07020008: 'LOST EVENTS', 0x07000004: 'new thread', 0x07000008: 'exec', 0x07010004: 'new thread name'}
HDR = struct.Struct('<8sIIIIQQQQ')
REC = struct.Struct('<QQQQQQIIQ')
THR = struct.Struct('<Qi20s')


def load(path):
    b = open(path, 'rb').read()
    magic, ver, numer, denom, recsize, nrec, nthr, lost, start = HDR.unpack_from(b)
    if magic[:7] != b'CATRACE' or recsize != REC.size:
        sys.exit('%s: not a catrace file (or kd_buf size %d)' % (path, recsize))
    off = HDR.size
    recs = [REC.unpack_from(b, off + i * REC.size) for i in range(nrec)]
    off += nrec * REC.size
    tid2pid, names = {}, {}
    for i in range(nthr):
        tid, pid, cmd = THR.unpack_from(b, off + i * THR.size)
        tid2pid[tid] = pid
        names.setdefault(pid, cmd.split(b'\0')[0].decode(errors='replace'))
    newpid = {}     # creating thread -> pid, to name it from the TRACE_STRING_NEWTHREAD that follows
    for r in recs:
        if r[6] == 0x07000004:      # arg1 new tid, arg2 pid
            tid2pid.setdefault(r[1], r[2])
            newpid[r[5]] = r[2]
        elif r[6] == 0x07010004 and r[5] in newpid:
            names.setdefault(newpid.pop(r[5]), struct.pack('<4Q', *r[1:5]).split(b'\0')[0].decode(errors='replace'))
    recs.sort(key=lambda r: r[0])
    us = lambda t: (t - start) * numer / denom / 1000.0
    return recs, tid2pid, names, us, lost, numer, denom


def pct(v, p):
    v = sorted(v)
    return v[min(len(v) - 1, int(p / 100.0 * len(v)))] if v else float('nan')


def hist(title, v):
    print('%s: n %d  p10 %.2f  p50 %.2f  p90 %.2f ms' % (title, len(v), pct(v, 10), pct(v, 50), pct(v, 90)))
    if not v:
        return
    c = collections.Counter(int(x // 1) for x in v)
    lo, hi = max(min(c), -50), min(max(c), 50)
    under, over = sum(n for k, n in c.items() if k < lo), sum(n for k, n in c.items() if k > hi)
    if under:
        print('   <%4d ms %5d' % (lo, under))
    for k in range(lo, hi + 1):
        print('  %4d ms %5d %s' % (k, c[k], '#' * min(c[k], 60)))
    if over:
        print('   >%4d ms %5d' % (hi, over))


def main():
    a = sys.argv[1:]
    if not a or a[0].startswith('-'):
        sys.exit(__doc__)
    recs, tid2pid, names, us, lost, numer, denom = load(a[0])
    pidof = lambda r: tid2pid.get(r[5], -1)
    print('%d records, %d lost-event markers, %.1f s, timebase %d/%d' % (
        len(recs), lost, (us(recs[-1][0]) - us(recs[0][0])) / 1e6 if recs else 0, numer, denom))
    if '--list' in a:
        want = int(a[a.index('--list') + 1])
        for r in recs:
            if pidof(r) == want:
                print('%12.1f  0x%08x %-26s tid %d  %x %x %x %x' % (us(r[0]), r[6], NAMES.get(r[6], ''), r[5], *r[1:5]))
        return
    if '--pair' not in a:
        per = collections.defaultdict(lambda: collections.defaultdict(list))
        for r in recs:
            per[pidof(r)][r[6]].append(us(r[0]))
        for pid in sorted(per):
            print('\npid %d %s' % (pid, names.get(pid, '?')))
            print('  debugid     name                        count    per s   median gap ms')
            for d, ts in sorted(per[pid].items()):
                span = (ts[-1] - ts[0]) / 1e6
                gaps = [y - x for x, y in zip(ts, ts[1:])]
                print('  0x%08x  %-26s %6d %8.1f %10.2f' % (d, NAMES.get(d, ''), len(ts), (len(ts) - 1) / span if span else 0,
                                                            pct(gaps, 50) / 1000 if gaps else float('nan')))
        return
    want = int(a[a.index('--pair') + 1])
    renders = [us(r[0]) for r in recs if r[6] == 0x31ca000d]
    recv = collections.defaultdict(list)    # context id -> receipt times
    for r in recs:
        if r[6] == 0x31ca0009:
            recv[r[1]].append(us(r[0]))
    tonext, fromprev, fbmr, unmatched, ncommit = [], [], [], 0, 0
    for r in recs:
        if r[6] != 0x31ca0215 or pidof(r) != want:
            continue
        ncommit += 1
        t = us(r[0])
        rl = recv.get(r[1], [])
        i = bisect.bisect_left(rl, t)
        if i < len(rl):
            s = rl[i]
            j = bisect.bisect_right(renders, s)
            if j < len(renders):
                tonext.append((renders[j] - s) / 1000)
            if j:
                fromprev.append((s - renders[j - 1]) / 1000)
        else:
            unmatched += 1
        j = bisect.bisect_right(renders, t)
        if j and r[2]:
            d = struct.unpack('<d', struct.pack('<Q', r[2]))[0]
            fb = d * 1e6 + us(0) if 1e-3 < d < 1e8 else us(r[2])
            fbmr.append((fb - renders[j - 1]) / 1000)
    print('pid %d %s: %d commits, %d with no server receipt, %d renders' % (
        want, names.get(want, '?'), ncommit, unmatched, len(renders)))
    hist('recv -> next render', tonext)
    hist('prev render -> recv', fromprev)
    hist('frame begin - last render', fbmr)


main()
