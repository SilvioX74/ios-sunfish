#!/bin/bash
# Wi-Fi at boot on the Pixel 4a: install wland + firmware and the launchd job
# org.iosonpixel.wland (`wland auto`: once per phone boot, log /var/local/wland.log).
# Firmware: modem from backup/modem_b.img (restore/fw/modempart), wlanmdsp/bdwlan from
# the factory vendor.img (restore/fw/vendor), rmtfs = backup/{modemst1,modemst2,fsg,fsc}.
# Reboot afterwards. Off: touch /var/local/wland.off or boot-arg sweetwifi=0.
# Binaries from tools/build-userland-linux.sh (work/userland-linux/out; work/userland/stage is the old macOS build).
#   --job    only (re)write the launchd job, no binaries or firmware
#   --wland  only replace /var/local/bin/wland (temp file + rename; the running wland keeps its copy until reboot)
# HT20 association (B57) is on by default; legacy 11g: a line "ht=0" in /var/local/wland.conf, then reboot.
set -euo pipefail
cd "$(dirname "$0")/.."
F() { python3 -I tools/flame-files.py "$@"; }
S() { tools/sunfish-ssh "$@"; }
U=work/userland O=work/userland-linux/out R=../restore/fw
MODE=${1:-}
JOB_ONLY=0; [ "$MODE" = --job ] && JOB_ONLY=1
put_bin() {	# temp + rename: writing over a running binary in place can silently keep the old one (vdstop, 2026-11-03)
	[ -s $O/$1 ] || { echo "missing $O/$1: run tools/build-userland-linux.sh"; exit 1; }
	F put $O/$1 /var/local/tmp/$1.new 755 >/dev/null && S "mv -f /var/local/tmp/$1.new /var/local/bin/$1" ||
		{ echo "install of $1 failed"; exit 1; }
}
if [ "$MODE" = --wland ]; then
	S "mkdir -p /var/local/bin /var/local/tmp"
	put_bin wland
	want=$(sha256sum $O/wland | cut -c1-16); have=$(S "sha256sum /var/local/bin/wland" | cut -c1-16)
	[ "$want" = "$have" ] || { echo "wland on the phone is $have, not $want"; exit 1; }
	echo "wland $want installed; it runs from the next boot"
	exit 0
fi
if [ $JOB_ONLY = 0 ]; then
for d in /var/local/fw /var/local/fw/modem /var/local/fw/rw; do F mkdir $d 755 >/dev/null 2>&1 || true; done
F mkdir /var/local/fw/rmtfs 700 >/dev/null 2>&1 || true
S "mkdir -p /var/local/bin /var/local/tmp"
put_bin wland
put_bin netsvc
if [ -d $R/modempart/image ]; then
	for f in $R/modempart/image/modem.* $R/modempart/image/modemr.jsn $R/vendor/wlanmdsp.mbn \
		$R/vendor/modemuw.jsn $R/vendor/bdwlan-sunfish.bin $R/vendor/bdwlan-sunfish-EVT1.0.bin; do
		F put "$f" /var/local/fw/modem/$(basename "$f") 644 >/dev/null
	done
	for p in modemst1 modemst2 fsg fsc; do F put ../backup/$p.img /var/local/fw/rmtfs/$p.img 600 >/dev/null; done
fi
fi
F get /System/Library/xpc/launchd.plist $U/launchd.plist.cur
python3 -I - "$U/launchd.plist.cur" "$U/launchd.plist.new" <<'PY'
import plistlib, sys
p = plistlib.load(open(sys.argv[1], 'rb'))
p['LaunchDaemons']['/System/Library/LaunchDaemons/org.iosonpixel.wland.plist'] = {
    'Label': 'org.iosonpixel.wland',
    'ProgramArguments': ['/var/local/bin/wland', 'auto'],
    'RunAtLoad': True, 'UserName': 'root',
    # Interactive like venusd/dropbear: launchd's default class let timer coalescing stretch wland's 1 ms naps ~10x
    # (venusd lat1, 2026-11-03); Wi-Fi downloads ran at 0.5 MB/s (B57)
    'ProcessType': 'Interactive', 'POSIXSpawnType': 'Interactive',
    'EnvironmentVariables': {'PATH': '/var/local/bin:/usr/bin:/bin:/usr/sbin:/sbin'},
}
plistlib.dump(p, open(sys.argv[2], 'wb'), fmt=plistlib.FMT_BINARY)
print('launchd jobs:', len(p['LaunchDaemons']))
PY
F put $U/launchd.plist.new /System/Library/xpc/launchd.plist 644
python3 -I tools/flame-cmd.py sync >/dev/null || true
echo "installed; reboot: wland starts ~at boot, Settings > Wi-Fi to join"
