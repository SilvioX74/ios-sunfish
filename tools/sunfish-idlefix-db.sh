#!/bin/bash
# Move the PowerLog and CoreDuet databases aside, so their daemons start fresh (plan priority 2, cpu.md 2026-10-31).
#   tools/sunfish-idlefix-db.sh [--undo]
#   tools/sunfish-idlefix-db.sh --biome [--undo]    the same for /var/mobile/Library/Biome (coreduetd queries it)
# Why: rows dated by 1970-clock boots make aggregated's PowerLog archive job walk one day at a time (at 1985-04-10 with
# 1950 files after 2 days) and coreduetd's prediction queries run all boot: a full core each, 82 C against 63 C at idle.
# Nothing is deleted: everything moves to /var/local/idlefix-db/ (--undo moves it back). Reboot needed; the script reboots
# nothing itself. Daemons are paused first so they don't write during the move, and stay paused until that reboot.
cd "$(dirname "$0")/.." || exit 1
BIO=0; [ "${1:-}" = --biome ] && { BIO=1; shift; }
[ "${1:-}" = --undo ] && U=1 || U=0
tools/sunfish-ssh "UNDO=$U; BIO=$BIO; $(cat <<'E'
B=/var/local/idlefix-db
PL=$(ls -d /var/containers/Shared/SystemGroup/*/Library/BatteryLife 2>/dev/null | head -1)
KN=/var/mobile/Library/CoreDuet/Knowledge
if [ "$BIO" = 1 ]; then
	BI=/var/mobile/Library/Biome
	for n in coreduetd biomed; do p=$(psx | grep " $n\$" | sed 's/^ *//' | cut -d' ' -f1); [ -n "$p" ] && kill -STOP $p; done
	if [ "$UNDO" = 1 ]; then
		[ -d "$B/biome" ] || { echo "nothing to undo"; exit 1; }
		mkdir -p "$B/.new-biome" && mv "$BI" "$B/.new-biome/$(date +%s)" && mv "$B/biome" "$BI" && echo undone
		exit 0
	fi
	[ -e "$B/biome" ] && { echo "already moved: $B/biome exists"; exit 1; }
	mkdir -p "$B" && mv "$BI" "$B/biome" && mkdir "$BI" && chown 501:501 "$BI" && chmod 755 "$BI" || { echo "move failed"; exit 1; }
	du -sk "$B/biome"; ls -ld "$BI"; echo "moved; reboot to apply"
	exit 0
fi
for n in aggregated coreduetd; do p=$(psx | grep " $n\$" | sed 's/^ *//' | cut -d' ' -f1); [ -n "$p" ] && kill -STOP $p; done
if [ "$UNDO" = 1 ]; then
	[ -d "$B/powerlog" ] || { echo "nothing to undo"; exit 1; }
	mkdir -p "$B/.new-powerlog" && mv "$PL/Archives" "$PL/CurrentPowerlog.PLSQL"* "$KN/knowledgeC.db"* "$B/.new-powerlog/" 2>/dev/null
	mv "$B/powerlog/"* "$PL/" && mv "$B/knowledge/"* "$KN/" && rmdir "$B/powerlog" "$B/knowledge" && echo undone
	exit 0
fi
[ -e "$B/powerlog" ] && { echo "already moved: $B/powerlog exists"; exit 1; }
[ -n "$PL" ] && [ -d "$KN" ] || { echo "paths not found: $PL $KN"; exit 1; }
mkdir -p "$B/powerlog" "$B/knowledge"
mv "$PL/Archives" "$PL/CurrentPowerlog.PLSQL"* "$B/powerlog/" && mv "$KN/knowledgeC.db"* "$B/knowledge/" || { echo "move failed"; exit 1; }
du -sk "$B/powerlog" "$B/knowledge"; ls -ld "$PL" "$KN"; echo "moved; reboot to apply"
E
)"
