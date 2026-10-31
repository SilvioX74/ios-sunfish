#!/bin/bash
# Idle heat: drop the launchd jobs com.apple.aggregated and com.apple.coreduetd from the phone's job cache.
# Why: both used a full core each at idle (105 of 126 CPU s/min); paused with SIGSTOP the phone fell from 82 C to
# 63 C (2026-10-31). The cause isn't known, and the effect of dropping them on their clients is unchecked (aggregated
# is KeepAlive and also holds the com.apple.powerlog* Mach services). com.apple.aggregated.addaily, com.apple.biomed
# and everything else stay.
#   - launchd.plist (backup /System/Library/xpc/launchd.plist.pre-idlefix, first run only; local copy
#     work/userland/launchd.plist.pre-idlefix): the new plist must parse and differ only by the named jobs, or nothing is written
# Idempotent. Reboot to apply (this script doesn't reboot).
#   tools/sunfish-install-idlefix.sh [--undo] [--only LABEL]...
#     --only LABEL: drop just this job instead of the two above (repeatable)
#     --undo: put the .pre-idlefix launchd.plist back (edits made to the plist since the backup go with it)
set -euo pipefail
cd "$(dirname "$0")/.."
F() { python3 -I tools/flame-files.py "$@"; }
U=work/userland P=/System/Library/xpc/launchd.plist
B=$P.pre-idlefix L=$U/launchd.plist.pre-idlefix G=$U/launchd.plist.pre-idlefix.get
usage() { echo "usage: tools/sunfish-install-idlefix.sh [--undo] [--only LABEL]..." >&2; exit 2; }
undo=0 labels=()
while [ $# -gt 0 ]; do
	case "$1" in
	--undo) undo=1; shift ;;
	--only)
		case "${2:-}" in ""|-*) usage ;; esac
		labels+=("$2"); shift 2 ;;
	*) usage ;;
	esac
done
[ $undo = 1 ] && [ ${#labels[@]} -gt 0 ] && usage
[ ${#labels[@]} -gt 0 ] || labels=(com.apple.aggregated com.apple.coreduetd)
mkdir -p $U

# The phone's backup -> $G. Returns 1 only if the phone says there is none (ERR 2): an unreachable phone must not
# look like "no backup yet", or a first-run backup would overwrite the real one.
get_backup() {
	local e
	rm -f "$G"
	e=$(F get "$B" "$G" 2>&1) && return 0
	case "$e" in
	*"phone: ERR 2"*) return 1 ;;
	*) echo "$e" >&2; echo "can't read $B; nothing changed" >&2; exit 1 ;;
	esac
}
# LOCAL -> REMOTE, then read it back: a short or failed write stops here.
put_checked() {
	F put "$1" "$2" 644 >/dev/null
	F get "$2" "$U/launchd.plist.idlefix-rb" >/dev/null
	cmp "$1" "$U/launchd.plist.idlefix-rb" || { echo "$2: read-back differs from $1" >&2; exit 1; }
	rm -f "$U/launchd.plist.idlefix-rb"
}

F get "$P" "$U/launchd.plist.cur" >/dev/null
have_backup=1
get_backup || have_backup=0

if [ $undo = 1 ]; then
	[ $have_backup = 1 ] || { echo "no $B on the phone; nothing changed" >&2; exit 1; }
	if cmp -s "$G" "$U/launchd.plist.cur"; then
		rm -f "$G"
		echo "launchd.plist already matches .pre-idlefix; nothing to do"
		exit 0
	fi
fi

rc=0
python3 -I - "$undo" "$U/launchd.plist.cur" "$U/launchd.plist.new" "$([ $have_backup = 1 ] && echo "$G" || true)" "${labels[@]}" <<'PY' || rc=$?
import collections, plistlib, sys
undo, src, dst, backup, named = sys.argv[1] == '1', sys.argv[2], sys.argv[3], sys.argv[4], set(sys.argv[5:])

def load(path):
    try:
        p = plistlib.load(open(path, 'rb'))
    except Exception as e:
        sys.exit(f'{path}: does not parse ({e}); nothing changed')
    if not isinstance(p, dict) or not isinstance(p.get('LaunchDaemons'), dict):
        sys.exit(f'{path}: no LaunchDaemons; nothing changed')
    return p

def labels(p):
    return collections.Counter(v['Label'] for jobs in p.values() if isinstance(jobs, dict)
                               for v in jobs.values() if isinstance(v, dict) and 'Label' in v)

before = labels(load(src))
bak = labels(load(backup)) if backup else collections.Counter()
if undo:
    print(f'labels: {sum(before.values())} -> {sum(bak.values())}')
    print('restored:', ' '.join(sorted(set(bak) - set(before))) or '(none)')
    print('removed by the restore:', ' '.join(sorted(set(before) - set(bak))) or '(none)')
    sys.exit(0)

unknown = sorted(l for l in named if l not in before and l not in bak)
if unknown:
    sys.exit(f'no such job in launchd.plist or its backup: {" ".join(unknown)}; nothing changed')
gone = sorted(named - set(before))
if gone:
    print('already gone:', ' '.join(gone))
drop = named & set(before)
if not drop:
    print(f'labels: {sum(before.values())}; all named jobs already gone, nothing to do')
    sys.exit(3)

# The walk of sunfish-launchd-drop.sh: that script writes to the phone as it runs, so it can't be called before the checks.
p = load(src)
for sect, jobs in p.items():
    if isinstance(jobs, dict):
        for k in [k for k, v in jobs.items() if isinstance(v, dict) and v.get('Label') in drop]:
            del jobs[k]
data = plistlib.dumps(p, fmt=plistlib.FMT_BINARY)

# Check what would be written, parsed back, against the original minus the named jobs.
got = plistlib.loads(data)
after = labels(got)
want = load(src)
for sect, jobs in want.items():
    if isinstance(jobs, dict):
        want[sect] = {k: v for k, v in jobs.items() if not (isinstance(v, dict) and v.get('Label') in drop)}
removed = set(before) - set(after)
print(f'labels: {sum(before.values())} -> {sum(after.values())}')
print('removed:', ' '.join(sorted(removed)) or '(none)')
want_after = before - collections.Counter({l: before[l] for l in drop})
if removed != drop or after != want_after or got != want:
    sys.exit(f'refused: the new plist did not lose exactly {" ".join(sorted(drop))}; nothing changed')
open(dst, 'wb').write(data)
PY
if [ $rc = 3 ]; then
	rm -f "$G"
	exit 0
fi
[ $rc = 0 ] || { rm -f "$G"; exit $rc; }

if [ $undo = 1 ]; then
	put_checked "$G" "$P"
	rm -f "$G"
	python3 -I tools/flame-cmd.py sync >/dev/null 2>&1 || true
	echo "launchd.plist restored from .pre-idlefix; reboot to apply (not rebooting)"
	exit 0
fi

if [ $have_backup = 1 ]; then
	# Keep both backups as they are; --undo uses the phone's.
	if [ ! -e "$L" ]; then
		cp "$G" "$L"
	elif ! cmp -s "$G" "$L"; then
		echo "note: $L differs from the phone's backup; both kept, --undo uses the phone's"
	fi
	rm -f "$G"
	echo "backup kept: $B"
else
	[ ! -e "$L" ] || { echo "$L exists but the phone has no backup: move it away first; nothing changed" >&2; exit 1; }
	cp "$U/launchd.plist.cur" "$L"
	put_checked "$L" "$B"
	echo "backup: $B (local copy $L)"
fi
put_checked "$U/launchd.plist.new" "$P"
python3 -I tools/flame-cmd.py sync >/dev/null 2>&1 || true
echo "launchd.plist updated; reboot to apply (not rebooting)"
