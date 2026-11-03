#!/bin/bash
# Video 4a (docs/features/video.md "Stage 4 spec"): venusd, the root daemon owning the Venus video engine, and its
# test client venusc. Installs:
#  - venusd, venusc, venusctl (work/userland-linux/out, tools/build-userland-linux.sh) -> /var/local/bin
#  - the launchd job org.iosonpixel.venusd from tools/venusd.plist into the launchd.plist cache (takes effect at the
#    next boot); backup /System/Library/xpc/launchd.plist.pre-venusd (first run only)
# Needs the firmware in /var/local/fw/venus (tools/sunfish-venus-fw.sh) and kernel160+ booted with sweetpmemwc=2
# (fast copy-out). Off: touch /var/local/venus-off (venusd refuses OPEN, Venus stays off).
#   tools/sunfish-install-venusd.sh [--tools | --undo | --check]
#   --tools: only the three tools (the 4a hand test: tools/sunfish-venusd-test.sh)
#   --undo: job removed (tools left); --check: prints the state, no writes
set -euo pipefail
cd "$(dirname "$0")/.."
F() { python3 -I tools/flame-files.py "$@"; }
S() { tools/sunfish-ssh "$@"; }
U=work/userland O=work/userland-linux/out
LP=/System/Library/xpc/launchd.plist
KEY=/System/Library/LaunchDaemons/org.iosonpixel.venusd.plist
TOOLS="venusd venusc venusctl"
MODE=${1:-}
mkdir -p $U/venusd

# job in/out of the launchd.plist cache (cur -> new), the job dict from tools/venusd.plist; --check: print it
job() {
	python3 -I - "$U/venusd/launchd.plist.cur" "$U/venusd/launchd.plist.new" "$KEY" "$1" tools/venusd.plist <<'PY'
import plistlib, sys
p = plistlib.load(open(sys.argv[1], 'rb'))
jobs = p['LaunchDaemons']
if sys.argv[4] == '--check':
    print('launchd job:', jobs.get(sys.argv[3], 'absent'))
    sys.exit(0)
if sys.argv[4] == '--undo':
    jobs.pop(sys.argv[3], None)
else:
    jobs[sys.argv[3]] = plistlib.load(open(sys.argv[5], 'rb'))
plistlib.dump(p, open(sys.argv[2], 'wb'), fmt=plistlib.FMT_BINARY)
print('launchd jobs:', len(jobs))
PY
}

case "$MODE" in ""|--tools|--undo|--check) ;; *) echo "usage: $0 [--tools | --undo | --check]"; exit 1;; esac

if [ "$MODE" != --tools ]; then
	F get $LP $U/venusd/launchd.plist.cur >/dev/null
fi

if [ "$MODE" = --check ]; then
	job --check
	S "for t in $TOOLS; do [ -x /var/local/bin/\$t ] && echo \"tool \$t: ok\" || echo \"tool \$t: MISSING\"; done; \
for f in /var/local/fw/venus/venus.mdt /var/local/venus-off /var/local/venusd.log /var/run/venusd.sock /var/tmp/venusctl.state $LP.pre-venusd; do \
[ -e \$f ] && echo \"\$f: present\" || echo \"\$f: absent\"; done; \
/bin/ps ax | grep '[v]enusd' || echo 'venusd: not running'; tail -n 3 /var/local/venusd.log 2>/dev/null || true"
	exit 0
fi

if [ "$MODE" = --undo ]; then
	job --undo
	F put $U/venusd/launchd.plist.new $LP 644 >/dev/null
	echo "org.iosonpixel.venusd removed (from the next boot; a running venusd: kill -TERM it, it shuts Venus down)"
	exit 0
fi

# 1. tools
for t in $TOOLS; do [ -s $O/$t ] || { echo "missing $O/$t: run tools/build-userland-linux.sh"; exit 1; }; done
S "mkdir -p /var/local/bin /var/local/tmp"
# Put to a temp file and rename: writing over a running venusd in place silently kept the old binary (vdstop, 2026-11-03).
for t in $TOOLS; do
	F put $O/$t /var/local/tmp/$t.new 755 >/dev/null && S "mv -f /var/local/tmp/$t.new /var/local/bin/$t" || { echo "install of $t failed"; exit 1; }
done
want=$(sha256sum $O/venusd | cut -c1-16); have=$(S "sha256sum /var/local/bin/venusd" | cut -c1-16)
[ "$want" = "$have" ] || { echo "venusd on the phone is $have, not $want"; exit 1; }
echo "installed: $TOOLS (a running venusd keeps the old binary until it restarts)"
[ "$MODE" = --tools ] && exit 0

# 2. launchd job
if ! F get $LP.pre-venusd $U/venusd/launchd.plist.pre-venusd >/dev/null 2>&1; then
	cp $U/venusd/launchd.plist.cur $U/venusd/launchd.plist.pre-venusd
	F put $U/venusd/launchd.plist.pre-venusd $LP.pre-venusd 644 >/dev/null
	echo "backup: $LP.pre-venusd"
fi
job install
F put $U/venusd/launchd.plist.new $LP 644 >/dev/null
echo "installed: org.iosonpixel.venusd (starts at the next boot)"
