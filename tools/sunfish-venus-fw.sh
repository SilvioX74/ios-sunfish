#!/bin/bash
# Venus (video engine) firmware on the running phone, for venusctl / stage 2 (docs/features/video.md).
# Extracts /firmware/venus.{mdt,b00..b04} from the factory vendor image (../restore/factory/vendor.img, ext4) with
# debugfs into research/fw/venus/ (Qualcomm firmware: gitignored; kept if already there with the same sizes), then puts
# each file at /var/local/fw/venus/ on the phone.
#   tools/sunfish-venus-fw.sh [--extract-only]
set -euo pipefail
cd "$(dirname "$0")/.."
IMG=${VENDOR_IMG:-../restore/factory/vendor.img}
L=research/fw/venus R=/var/local/fw/venus
FILES="venus.mdt venus.b00 venus.b01 venus.b02 venus.b03 venus.b04"
mkdir -p "$L"
for f in $FILES; do
	want=$(debugfs -R "stat /firmware/$f" "$IMG" 2>/dev/null | sed -n 's/.*Size: \([0-9]*\).*/\1/p' | head -1)
	[ -n "$want" ] || { echo "no /firmware/$f in $IMG" >&2; exit 1; }
	if [ "$(stat -c %s "$L/$f" 2>/dev/null)" = "$want" ]; then
		echo "have $L/$f ($want B)"
	else
		rm -f "$L/$f"; debugfs -R "dump /firmware/$f $L/$f" "$IMG" >/dev/null 2>&1
		[ "$(stat -c %s "$L/$f")" = "$want" ] || { echo "extract of $f failed" >&2; exit 1; }
		echo "extracted $L/$f ($want B)"
	fi
done
[ "${1:-}" = --extract-only ] && exit 0
tools/sunfish-ssh "mkdir -p $R"
for f in $FILES; do
	python3 -I tools/flame-files.py put "$L/$f" "$R/$f" 644 >/dev/null
	echo "put $L/$f -> $R/$f"
done
tools/sunfish-ssh "ls -l $R"
