#!/bin/bash
# Video 4b (docs/features/video.md "Stage 4 spec"): vtvenus, the VideoToolbox H.264 decoder that sends frames to venusd
# (phone/video/vtvenus.c), as the free decoder slot /System/Library/VideoDecoders/ExampleIPB.videodecoder (the slot
# vtstub used; installing one replaces the other). Also puts vtdecode (--raw test mode) in /var/local/bin.
# VideoToolbox loads the slot when a process first builds its decoder list: new processes get it at once,
# mediaserverd (where Safari's and apps' sessions decode) after a restart (--restart: psx -k mediaserverd, launchd
# starts it again; audio stops for a moment).
# Needs venusd running (tools/sunfish-install-venusd.sh). Off without removing: touch /var/local/venus-off (the
# plugin refuses CreateInstance, VT uses the software decoder; venusd refuses OPEN too).
#   tools/sunfish-install-vtvenus.sh install [FILE] [--restart]   FILE default work/userland-linux/out/vtvenus.dylib
#   tools/sunfish-install-vtvenus.sh remove [--restart]          deletes the slot file (log /var/local/vtvenus.log stays)
#   tools/sunfish-install-vtvenus.sh check                       slot, switch, log tail; no writes
# Copied to /var/local/tmp first, then mv'd into place (a running process keeps its mapping of the old file).
set -euo pipefail
cd "$(dirname "$0")/.."
F() { python3 -I tools/flame-files.py "$@"; }
S() { tools/sunfish-ssh "$@"; }
VD=/System/Library/VideoDecoders
DST=$VD/ExampleIPB.videodecoder
MODE=${1:-}
shift || true
SRC= RESTART=0
for a in "$@"; do
	case $a in --restart) RESTART=1;; -*) echo "unknown option $a" >&2; exit 2;; *) SRC=$a;; esac
done
restart() {
	[ $RESTART = 1 ] || { echo "mediaserverd keeps its old decoder list until restarted (--restart)"; return; }
	S "/var/local/bin/psx -k mediaserverd" || true
	echo "mediaserverd restarted"
}
case $MODE in
install)
	SRC=${SRC:-work/userland-linux/out/vtvenus.dylib}
	[ -f "$SRC" ] || SRC=phone/video/out/vtvenus.dylib
	[ -f "$SRC" ] || { echo "no vtvenus.dylib: run phone/video/build.sh"; exit 1; }
	S "mkdir -p $VD /var/local/tmp /var/local/bin"
	F put "$SRC" /var/local/tmp/vtvenus.dylib 755 >/dev/null
	S "mv /var/local/tmp/vtvenus.dylib $DST && chmod 755 $DST && ls -la $DST"
	[ -f work/userland-linux/out/vtdecode ] && F put work/userland-linux/out/vtdecode /var/local/bin/vtdecode 755 >/dev/null &&
		echo "installed vtdecode -> /var/local/bin/vtdecode"
	echo "installed $SRC -> $DST (log: /var/local/vtvenus.log)"
	restart
	;;
remove)
	S "rm -f $DST; ls $VD"
	echo "removed $DST"
	restart
	;;
check)
	S "ls -la $DST 2>/dev/null || echo 'slot: empty'; [ -e /var/local/venus-off ] && echo 'venus-off: present (plugin refuses)' || echo 'venus-off: absent'; \
[ -S /var/run/venusd.sock ] && echo 'venusd socket: present' || echo 'venusd socket: absent'; tail -n 5 /var/local/vtvenus.log 2>/dev/null || true"
	;;
*)	echo "usage: $0 install [FILE] [--restart] | remove [--restart] | check" >&2; exit 2;;
esac
