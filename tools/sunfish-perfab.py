#!/usr/bin/env python3
"""Parser behind tools/sunfish-perfab.sh: per-screen metrics and void rules, the tables, the bug counts.

  tools/sunfish-perfab.py screen --log L --offset N --out RESULTS.tsv --tag T --name A --boot 1 --screen S
                                 [--secs 15] [--kernel K] [--batt MV] [--bootlog F] [--opens a=1,b=2]
                                 [--restart] [--ssh-lost] [--open-failed] [--void REASON]
      appends one row, prints "OK swaps/s", "VOID reason" or "ABORT reason" (ABORT = the rest of the boot is void too)
  tools/sunfish-perfab.py table RESULTS.tsv
  tools/sunfish-perfab.py counts --log L --offset N --tag T --results RESULTS.tsv --doc docs/gpu-bugs.md --configs TEXT

--offset is the log's byte size taken before the screen's lead-in: the log holds every boot (hundreds of MB), and a hang
in the lead-in counts against the screen too. Swap stats come from sunfish-timeline.py's load() and pct().
"""
import argparse, bisect, importlib.util, os, re, sys, time

here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('timeline', os.path.join(here, 'sunfish-timeline.py'))
tl = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tl)

COLS = ('tag config boot screen status reason swaps_s p50 p95 le17 gaps100 busy_pct first_submit copies_per_swap '
        'tl_swaps fb_swaps opens batt_mv bootlog kernel').split()
FB = re.compile(r'SWEET FB: 5s: (\d+) swaps .*?, (\d+) panel copies')
GPU = re.compile(r'SWEET GPU: 5s: (\d+) submits .*?GPU busy (\d+) ms \((\d+)%\)')
HANG = re.compile(r'GPU hang or fault|fence wait|device refused')
G1 = re.compile(r'GPU hang or fault.*\bfsr 0 ')
G2 = 'submit failed: 0xe00002c2'
G4 = re.compile(r'GPU hang or fault.*\bfsr 40000002')
STOP_RULE = 'Stop rule: a gain under 1 ms counts only with 5 boots per side.'

def read_from(path, offset):
    """The log from a byte offset on (from the start if the log was rotated since)."""
    with open(path, 'rb') as f:
        if offset > os.path.getsize(path):
            offset = 0
        f.seek(offset)
        return f.read().decode(errors='replace')

def num(x, fmt='%.2f'):
    return '' if x is None else fmt % x

def timeline_stats(recs):
    """swaps/s, p50, p95, share <= 17 ms, gaps > 100 ms: same definitions as sunfish-timeline.py main()."""
    sw = sorted(r[1] for r in recs if r[0] == 1)
    if len(sw) < 2:
        return len(sw), None, None, None, None, None
    iv = sorted((y - x) * 1000.0 / tl.MACH_HZ for x, y in zip(sw, sw[1:]))
    secs = (sw[-1] - sw[0]) / tl.MACH_HZ
    return (len(sw), len(sw) / secs, tl.pct(iv, .5), tl.pct(iv, .95), 100.0 * sum(1 for x in iv if x <= 17.5) / len(iv),
            sum(1 for x in iv if x > 100))

def first_submit(recs):
    """GPU ms of each frame's first submit by the busiest client (backboardd), p50."""
    spans, own = {}, {}
    for r in recs:
        if r[0] == 7 and r[3]:
            spans[(r[2], r[3])] = (r[4] - r[1]) * 1000.0 / tl.CP_HZ
            own[r[3]] = own.get(r[3], 0) + spans[(r[2], r[3])]
    if not own:
        return None
    pid = max(own, key=own.get)
    subs = sorted((r[1], r[2]) for r in recs if r[0] == 6 and r[3] == pid)
    st = [s[0] for s in subs]
    sw = sorted(r[1] for r in recs if r[0] == 1)
    out = []
    for x, y in zip(sw, sw[1:]):
        i, j = bisect.bisect_right(st, x), bisect.bisect_right(st, y)
        if i < j and (subs[i][1], pid) in spans:
            out.append(spans[(subs[i][1], pid)])
    return tl.pct(sorted(out), .5) if out else None

def append_row(path, row):
    new = not os.path.exists(path)
    with open(path, 'a') as f:
        if new:
            f.write('\t'.join(COLS) + '\n')
        f.write('\t'.join(str(row.get(c, '')) for c in COLS) + '\n')

def screen(a):
    row = dict(tag=a.tag, config=a.name, boot=a.boot, screen=a.screen, batt_mv=a.batt, bootlog=a.bootlog, kernel=a.kernel,
               opens=a.opens)
    if a.void:
        row.update(status='VOID', reason=a.void)
        append_row(a.out, row)
        print('VOID', a.void)
        return
    text = read_from(a.log, a.offset)
    start, end = f'=== perfab {a.tag} {a.name} b{a.boot} {a.screen} start', f'=== perfab {a.tag} {a.name} b{a.boot} {a.screen} end'
    slice_path = a.out + '.slice'
    with open(slice_path, 'w') as f:
        f.write(text)
    try:
        recs, _ = tl.load(slice_path, start[4:], end[4:])
    finally:
        os.unlink(slice_path)
    swaps_tl, rate, p50, p95, le17, gaps = timeline_stats(recs)
    # 5 s stats lines inside the window; hang and restart lines from the lead-in on, to the end of what has arrived
    on = False
    fb, gpu, hang, failed = [], [], None, False
    for line in text.splitlines():
        if line.startswith(start):
            on = True
        elif line.startswith(end):
            on = False
        if HANG.search(line) and not hang:
            hang = HANG.search(line).group(0)
        failed = failed or 'submit failed' in line
        if on:
            m = FB.search(line)
            if m:
                fb.append((int(m[1]), int(m[2])))
            m = GPU.search(line)
            if m:
                gpu.append((int(m[2]), int(m[3])))
    swaps_fb, copies = sum(x[0] for x in fb), sum(x[1] for x in fb)
    swaps = max(swaps_tl, swaps_fb)
    if rate is None and swaps_fb:
        rate = swaps_fb / 5.0 / len(fb)     # no TL records: kernel without sweettrace
    ratio = copies / swaps_fb if swaps_fb else None
    busy = sum(x[1] for x in gpu) / len(gpu) if gpu else None
    # plan.md void rules; a restart also voids the rest of the boot: the GPU stays in the slower mode until reboot (G3)
    reason, abort = None, False
    if a.ssh_lost:
        reason, abort = 'SSH lost', True
    elif hang:
        reason, abort = hang, True
    elif failed:
        reason, abort = 'submit failed', True
    elif a.restart:
        reason, abort = 'backboardd restarted', True
    elif a.open_failed:
        reason = 'openapp failed'
    elif swaps == 0:
        reason = '0 swaps'
    elif a.secs >= 6 and not fb:
        reason = 'no SWEET FB lines'        # one is due every 5 s while swapping
    elif fb and (copies == 0 or (ratio is not None and ratio < 0.5)):
        reason = f'panel off ({copies} copies, {swaps_fb} swaps)'
    elif (gpu or a.secs >= 6) and not any(x[0] for x in gpu):
        reason = 'GPU busy 0 ms (CPU drawing)'  # the kernel drops the line altogether when nothing was submitted
    row.update(status='VOID' if reason else 'OK', reason=reason or '', swaps_s=num(rate), p50=num(p50), p95=num(p95),
               le17=num(le17, '%.1f'), gaps100='' if gaps is None else gaps, busy_pct=num(busy, '%.0f'),
               first_submit=num(first_submit(recs)), copies_per_swap=num(ratio), tl_swaps=swaps_tl, fb_swaps=swaps_fb)
    append_row(a.out, row)
    print(('ABORT ' if abort else 'VOID ') + reason if reason else 'OK ' + num(rate, '%.1f'))

def load_rows(path):
    if not os.path.exists(path):
        return []
    with open(path) as f:
        head = f.readline().rstrip('\n').split('\t')
        return [dict(zip(head, l.rstrip('\n').split('\t'))) for l in f]

def mean(rows, key):
    v = [float(r[key]) for r in rows if r.get(key)]
    return sum(v) / len(v) if v else None

def table(a):
    rows = load_rows(a.results)
    if not rows:
        print('no results')
        return
    screens, configs = [], []
    for r in rows:
        if r['screen'] not in screens:
            screens.append(r['screen'])
        if r['config'] not in configs:
            configs.append(r['config'])
    w = max(8, *(len(c) for c in configs))
    head = f'{"config":{w}s} {"ok/void":7s} {"swaps/s (min-max)":19s} {"frame":>6s} {"p50":>6s} {"p95":>6s} {"<=17%":>6s} {">100/run":>8s} {"GPU%":>5s} {"1st":>6s}'
    f2 = lambda x: '-' if x is None else f'{x:.1f}'
    for s in screens:
        print(f'\n{s} (ms)')
        print(head)
        vals = {}
        for c in configs:
            rr = [r for r in rows if r['screen'] == s and r['config'] == c]
            ok = [r for r in rr if r['status'] == 'OK']
            m = {k: mean(ok, k) for k in ('swaps_s', 'p50', 'p95', 'le17', 'gaps100', 'busy_pct', 'first_submit')}
            m['frame'] = 1000.0 / m['swaps_s'] if m['swaps_s'] else None
            vals[c] = m
            sv = [float(r['swaps_s']) for r in ok if r['swaps_s']]
            rng = f'{m["swaps_s"]:.1f} ({min(sv):.1f}-{max(sv):.1f})' if sv else '-'
            print(f'{c:{w}s} {f"{len(ok)}/{len(rr) - len(ok)}":7s} {rng:19s} {f2(m["frame"]):>6s} {f2(m["p50"]):>6s} {f2(m["p95"]):>6s} '
                  f'{f2(m["le17"]):>6s} {f2(m["gaps100"]):>8s} {f2(m["busy_pct"]):>5s} {f2(m["first_submit"]):>6s}')
        if len(configs) == 2:
            A, B = vals[configs[0]], vals[configs[1]]
            def d(k):
                return '-' if A[k] is None or B[k] is None else f'{B[k] - A[k]:+.1f}'
            print(f'{"B-A":{w}s} {"":7s} {d("swaps_s"):19s} {d("frame"):>6s} {d("p50"):>6s} {d("p95"):>6s} {d("le17"):>6s} {d("gaps100"):>8s} '
                  f'{d("busy_pct"):>5s} {d("first_submit"):>6s}')
        opens = {}
        for r in rows:
            if r['screen'] == s and r['status'] == 'OK' and r.get('opens'):
                for kv in r['opens'].split(','):
                    k, v = kv.split('=')
                    opens.setdefault((r['config'], k), []).append(int(v))
        if opens:
            print('open ms (openapp over SSH): ' + ', '.join(f'{c} {k} {sum(v) // len(v)}' for (c, k), v in opens.items()))
        for r in rows:
            if r['screen'] == s and r['status'] != 'OK':
                print(f'  void {r["config"]} b{r["boot"]}: {r["reason"]}')
    print('\n' + STOP_RULE)

def bug_counts(text, tag):
    """(boots, submits, G1, G2, G4) from this run's first marker on."""
    first = text.find(f'=== perfab {tag} ')
    if first < 0:
        return None
    boots = submits = g1 = g2 = g4 = 0
    bootm = re.compile(rf'^=== perfab {re.escape(tag)} \S+ b\d+ boot ')
    for line in text[first:].splitlines():
        if line.startswith('=== '):
            boots += bool(bootm.match(line))
            continue
        m = GPU.search(line)
        if m:
            submits += int(m[1])
        if 'GPU hang or fault' in line:
            g1 += bool(G1.search(line))
            g4 += bool(G4.search(line))
        g2 += G2 in line
    return boots, submits, g1, g2, g4

def add_row(doc, row):
    """Newest first, under the table's header; makes the section (before Daily soak) if missing."""
    with open(doc) as f:
        lines = f.read().split('\n')
    sep = None
    for i, l in enumerate(lines):
        if l.startswith('## Bench runs'):
            sep = next((j for j in range(i, len(lines)) if lines[j].startswith('|---')), None)
            break
    if sep is None:
        sect = ['## Bench runs', 'Written by `tools/sunfish-perfab.sh`, newest first. Void runs are screen runs, not boots.', '',
                '| Date | Tag | Kernels and configs | Boots | Submits | G1 | G2 | G4 | Void runs |', '|---|---|---|---|---|---|---|---|---|', '']
        at = next((i for i, l in enumerate(lines) if l.startswith('## Daily soak')), len(lines))
        lines[at:at] = sect
        sep = at + 4
    lines.insert(sep + 1, row)
    with open(doc, 'w') as f:
        f.write('\n'.join(lines))

def counts(a):
    c = bug_counts(read_from(a.log, a.offset), a.tag)
    if c is None:
        return
    boots, submits, g1, g2, g4 = c
    voids = sum(1 for r in load_rows(a.results) if r['status'] != 'OK')
    subs = f'{submits / 1000:.0f} k' if submits >= 1000 else str(submits)
    cfg = a.configs.replace('|', '/')
    add_row(a.doc, f'| {time.strftime("%Y-%m-%d")} | {a.tag} | {cfg} | {boots} | {subs} | {g1} | {g2} | {g4} | {voids} |')
    print(f'Bug counts: {boots} boots, {subs} submits, G1 {g1}, G2 {g2}, G4 {g4}, void runs {voids} (added to {a.doc})')

def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest='cmd', required=True)
    s = sub.add_parser('screen')
    for k in ('--log', '--out', '--tag', '--name', '--boot', '--screen'):
        s.add_argument(k, required=k != '--log')
    s.add_argument('--offset', type=int, default=0)
    s.add_argument('--secs', type=int, default=15)
    for k in ('--kernel', '--batt', '--bootlog', '--opens', '--void'):
        s.add_argument(k, default='')
    for k in ('--restart', '--ssh-lost', '--open-failed'):
        s.add_argument(k, action='store_true')
    t = sub.add_parser('table')
    t.add_argument('results')
    c = sub.add_parser('counts')
    for k in ('--log', '--tag', '--results', '--doc', '--configs'):
        c.add_argument(k, required=True)
    c.add_argument('--offset', type=int, default=0)
    a = ap.parse_args()
    {'screen': screen, 'table': table, 'counts': counts}[a.cmd](a)

if __name__ == '__main__':
    main()
