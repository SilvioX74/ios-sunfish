#!/bin/bash
# vtstub (phone/video/vtstub.c, video stage 4 probe, docs/features/video.md) as VideoToolbox's free decoder slot
# /System/Library/VideoDecoders/ExampleIPB.videodecoder on the running phone. Its register call only appends
# "pid NAME time" to /var/local/vtstub.log. A process loads it when its VideoToolbox builds the decoder list
# (mediaserverd: restart it, psx -k mediaserverd).
#   tools/sunfish-vtstub.sh install [FILE]   FILE default work/userland-linux/out/vtstub.dylib, else phone/video/out
#   tools/sunfish-vtstub.sh remove           deletes the slot file (the log stays)
# Copied to /var/local/tmp first, then mv'd into place (a running process keeps its mapping of the old file).
set -euo pipefail
cd "$(dirname "$0")/.."
F() { python3 -I tools/flame-files.py "$@"; }
S() { tools/sunfish-ssh "$@"; }
VD=/System/Library/VideoDecoders
DST=$VD/ExampleIPB.videodecoder
case ${1:-} in
install)
	SRC=${2:-work/userland-linux/out/vtstub.dylib}
	[ -f "$SRC" ] || SRC=phone/video/out/vtstub.dylib
	[ -f "$SRC" ] || { echo "no vtstub.dylib: run phone/video/build.sh"; exit 1; }
	S "mkdir -p $VD /var/local/tmp"
	F put "$SRC" /var/local/tmp/vtstub.dylib 755 >/dev/null
	S "mv /var/local/tmp/vtstub.dylib $DST && chmod 755 $DST && ls -la $DST"
	echo "installed $SRC -> $DST (log: /var/local/vtstub.log)"
	;;
remove)
	S "rm -f $DST; ls $VD"
	echo "removed $DST"
	;;
*)	echo "usage: $0 install [FILE] | remove" >&2; exit 2;;
esac
