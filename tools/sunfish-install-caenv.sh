#!/bin/bash
# GPU cost per frame: set CoreAnimation environment variables in the backboardd job.
# Why: at low internal brightness CoreAnimation draws needs_p3_clamp, a full-screen destination-read draw, 7 ms per
# frame. CA_DISPLAY_COLOR_ACCURACY_FIX_NITS sets the nits threshold, default 80 (research/re/QuartzCore.s). init_debug
# reads it once, through CADefaultsGetInt (getenv at 0x18837f568), so it has to come from launchd, not mtl2-env.
# The value goes through atoi and 0 means "use the default" (0x1882e69b8): use a whole number of at least 1.
#   - launchd.plist (backup /System/Library/xpc/launchd.plist.pre-caenv, first run only, never overwritten; local copy
#     work/userland/launchd.plist.pre-caenv): the new plist must parse and differ only in the EnvironmentVariables
#     of com.apple.backboardd, or nothing is written
# Idempotent. Reboot to apply (this script doesn't reboot).
#   tools/sunfish-install-caenv.sh KEY=VALUE [KEY=VALUE...]   add or replace these entries
#   tools/sunfish-install-caenv.sh --undo                     put the .pre-caenv launchd.plist back (edits made to the plist since the backup go with it)
set -euo pipefail
cd "$(dirname "$0")/.."
F() { python3 -I tools/flame-files.py "$@"; }
U=work/userland P=/System/Library/xpc/launchd.plist
B=$P.pre-caenv L=$U/launchd.plist.pre-caenv G=$U/launchd.plist.pre-caenv.get
usage() { echo "usage: tools/sunfish-install-caenv.sh KEY=VALUE [KEY=VALUE...] | --undo" >&2; exit 2; }
undo=0
[ $# -gt 0 ] || usage
if [ "$1" = --undo ]; then
	[ $# = 1 ] || usage
	undo=1
else
	for a in "$@"; do
		[[ "$a" =~ ^[A-Za-z_][A-Za-z0-9_]*=.+$ ]] || usage
	done
fi
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
	F get "$2" "$U/launchd.plist.caenv-rb" >/dev/null
	cmp "$1" "$U/launchd.plist.caenv-rb" || { echo "$2: read-back differs from $1" >&2; exit 1; }
	rm -f "$U/launchd.plist.caenv-rb"
}

F get "$P" "$U/launchd.plist.cur" >/dev/null
have_backup=1
get_backup || have_backup=0

if [ $undo = 1 ]; then
	[ $have_backup = 1 ] || { echo "no $B on the phone; nothing changed" >&2; exit 1; }
	if cmp -s "$G" "$U/launchd.plist.cur"; then
		rm -f "$G"
		echo "launchd.plist already matches .pre-caenv; nothing to do"
		exit 0
	fi
fi

rc=0
python3 -I - "$undo" "$U/launchd.plist.cur" "$U/launchd.plist.new" "$G" "$@" <<'PY' || rc=$?
import plistlib, sys
undo, src, dst, backup, pairs = sys.argv[1] == '1', sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5:]
LABEL, ENV = 'com.apple.backboardd', 'EnvironmentVariables'

def load(path):
    try:
        p = plistlib.load(open(path, 'rb'))
    except Exception as e:
        sys.exit(f'{path}: does not parse ({e}); nothing changed')
    if not isinstance(p, dict) or not isinstance(p.get('LaunchDaemons'), dict):
        sys.exit(f'{path}: no LaunchDaemons; nothing changed')
    return p

def job(p, path):
    found = [v for jobs in p.values() if isinstance(jobs, dict)
             for v in jobs.values() if isinstance(v, dict) and v.get('Label') == LABEL]
    if len(found) != 1:
        sys.exit(f'{path}: {len(found)} {LABEL} jobs, expected 1; nothing changed')
    return found[0]

def show(env):
    return ' '.join(f'{k}={v}' for k, v in sorted(env.items())) or '(none)'

def without_env(p, path):
    job(p, path).pop(ENV, None)
    return plistlib.dumps(p, fmt=plistlib.FMT_BINARY)

before = job(load(src), src).get(ENV, {})
if not isinstance(before, dict):
    sys.exit(f'{LABEL} {ENV} is not a dictionary; nothing changed')
print('backboardd env before:', show(before))
if undo:
    print('backboardd env after: ', show(job(load(backup), backup).get(ENV, {})))
    if without_env(load(src), src) != without_env(load(backup), backup):
        print('note: other edits made since the backup are undone too')
    sys.exit(0)

new = dict(p.split('=', 1) for p in pairs)
if len(new) != len(pairs):
    sys.exit('the same key is given twice; nothing changed')
if all(before.get(k) == v for k, v in new.items()):
    print('all keys already set, nothing to do')
    sys.exit(3)

p = load(src)
job(p, src).setdefault(ENV, {}).update(new)
data = plistlib.dumps(p, fmt=plistlib.FMT_BINARY)

# Check what would be written, parsed back, against the original: same plist outside this job's env, same env plus the keys.
got = plistlib.loads(data)
after = job(got, 'the new plist').get(ENV, {})
print('backboardd env after: ', show(after))
print('set:', ' '.join(sorted(new)))
if after != {**before, **new} or without_env(got, 'the new plist') != without_env(load(src), src):
    sys.exit(f'refused: the new plist changes more than the {ENV} of {LABEL}; nothing changed')
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
	echo "launchd.plist restored from .pre-caenv; reboot to apply (not rebooting)"
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
