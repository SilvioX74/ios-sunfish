#!/bin/bash
# VideoToolbox pixel-transfer preferences (com.apple.coremedia, read by every mobile process that uses
# VTPixelTransferSession). Photos scales its thumbnails in vImage because the port has no M2 scaler driver
# (docs/features/cpu.md phs); allowPixelTransferGraph lets VideoToolbox search for another path (Metal), and
# allowPixelTransferFallbacks falls back to the default path when that search fails (research/notes/vt-scaling-2026-11-02.md).
#   tools/sunfish-vt-prefs.sh KEY=true|false [KEY=...]   set booleans, e.g. allowPixelTransferGraph=true allowPixelTransferFallbacks=true
#   tools/sunfish-vt-prefs.sh --undo                     remove the keys this script knows about
# Kills Photos and cfprefsd so the next launch reads the new values; doesn't reboot.
set -euo pipefail
cd "$(dirname "$0")/.."
F() { python3 -I tools/flame-files.py "$@"; }
P=/var/mobile/Library/Preferences/com.apple.coremedia.plist
T=work/vt/com.apple.coremedia.plist
KEYS="allowPixelTransferGraph allowPixelTransferFallbacks allowPixelTransferChain allowSoftwareTransferSession allowMetalTransferSession allowHardwareTransferSession"
[ $# -ge 1 ] || { echo "usage: tools/sunfish-vt-prefs.sh KEY=true|false ... | --undo" >&2; exit 2; }
mkdir -p work/vt
tools/sunfish-ssh "c=\$(/bin/ps ax | grep '[/]MobileSlideShow.app/MobileSlideShow' | sed 's/^ *//' | cut -d' ' -f1); [ -n \"\$c\" ] && kill -9 \$c; p=\$(/bin/ps ax | grep '[c]fprefsd daemon' | sed 's/^ *//' | cut -d' ' -f1); [ -n \"\$p\" ] && kill \$p; sleep 1; [ -e $P ] || exit 0; [ -e $P.bak-vt ] || cp $P $P.bak-vt"
rm -f "$T"
F get $P $T >/dev/null 2>&1 || true
python3 -I - "$T" "$KEYS" "$@" <<'PY'
import os, plistlib, sys
path, known, args = sys.argv[1], sys.argv[2].split(), sys.argv[3:]
p = plistlib.load(open(path, 'rb')) if os.path.exists(path) and os.path.getsize(path) else {}
if args == ['--undo']:
    for k in known:
        p.pop(k, None)
else:
    for a in args:
        k, _, v = a.partition('=')
        if k not in known or v not in ('true', 'false'):
            sys.exit('bad argument %r: KEY=true|false with KEY one of %s' % (a, ' '.join(known)))
        p[k] = v == 'true'
plistlib.dump(p, open(path, 'wb'), fmt=plistlib.FMT_BINARY)
print('com.apple.coremedia:', {k: p[k] for k in known if k in p} or 'none of the transfer keys')
PY
F put $T $P 600 >/dev/null
tools/sunfish-ssh "chown mobile:mobile $P; p=\$(/bin/ps ax | grep '[c]fprefsd' | sed 's/^ *//' | cut -d' ' -f1); for x in \$p; do kill \$x; done; true"
echo "written; Photos and cfprefsd restarted on next use"
