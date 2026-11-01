#!/bin/bash
# Control Center's backdrop at quarter size: add backdropScale to CoreMaterial's modulesBackground recipe, which is the
# only large blur recipe without one (smoothness.md cc2: 2 full-size captures, about 10 ms each, in its slow frames).
#   tools/sunfish-install-ccscale.sh [SCALE]    default 0.25; keeps .orig on the phone, first run only
#   tools/sunfish-install-ccscale.sh --undo
# Needs a SpringBoard restart or reboot to apply; does neither.
set -e
cd "$(dirname "$0")/.."
F=/System/Library/PrivateFrameworks/CoreMaterial.framework/modulesBackground.materialrecipe
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
if [ "${1:-}" = --undo ]; then
	tools/sunfish-ssh "[ -f $F.orig ] && cp -p $F.orig $F && echo restored"
	exit 0
fi
tools/sunfish-ssh "[ -f $F.orig ] || cp -p $F $F.orig"
python3 -I tools/flame-files.py get "$F.orig" "$T/r" >/dev/null
python3 -I - "$T/r" "${1:-0.25}" <<'E'
import plistlib, sys
p, scale = sys.argv[1], float(sys.argv[2])
raw = open(p, 'rb').read()
d = plistlib.loads(raw)
d['baseMaterial']['materialFiltering']['backdropScale'] = scale
fmt = plistlib.FMT_BINARY if raw[:6] == b'bplist' else plistlib.FMT_XML
open(p, 'wb').write(plistlib.dumps(d, fmt=fmt))
print('format', 'binary' if fmt == plistlib.FMT_BINARY else 'xml', 'backdropScale', scale)
E
python3 -I tools/flame-files.py put "$T/r" "$F" 644 >/dev/null
tools/sunfish-ssh "ls -la $F | cut -c1-60"
