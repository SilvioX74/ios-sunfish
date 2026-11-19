#!/bin/sh
# The 32 stock apps iOS keeps in /var/staged_system_apps (Mail, Calendar, Clock, Notes, Maps, Weather,
# Calculator, Contacts, Files, Health, Home, Music, News, Podcasts, Reminders, Shortcuts, Stocks, Tips,
# Voice Memos, FaceTime, Find My, Fitness, Freeform, Measure, Magnifier, Compass, Books, TV, Wallet,
# App Store helper, Watch, Translate) never got installed on this root: our /var never went through
# MobileInstallation, so installd never staged them and SpringBoard showed six apps.
#
#   tools/sunfish-install-stagedapps.sh          copy them in and invalidate the LaunchServices database
#   tools/sunfish-install-stagedapps.sh --undo   take them out again and invalidate it the same way
#
# Then REBOOT: lsd rebuilds its database by scanning /Applications and the apps appear on the Home screen.
#
# What does NOT work, tried 2026-11-19 (apps.md): copying the bundles in alone (SpringBoard ignores them);
# `lsreg APP` and `lsreg -r`, which fail with LaunchServices -9499 at _LSRegisterBundleNode; and giving
# installd a RunAtLoad job plus a changed LastBuildInfo.plist, which starts installd but stages nothing.
# Invalidating lsd's store is what works, because that is how the first 143 apps got registered.
set -eu
cd "$(dirname "$0")/.."
S() { tools/sunfish-ssh "$@"; }
if [ "${1:-}" = "--undo" ]; then
	S 'cd /var/staged_system_apps && for a in *.app; do rm -rf "/Applications/$a"; done; echo "in /Applications: $(ls /Applications | wc -l)"'
else
	S 'cd /var/staged_system_apps && for a in *.app; do [ -d "/Applications/$a" ] || cp -a "$a" "/Applications/$a"; done; echo "in /Applications: $(ls /Applications | wc -l)"'
fi
# lsd keeps its store in its own data container; the UUID differs per root, so look it up.
S 'mkdir -p /var/local/lsdb-backup; n=0; for f in $(find /var/mobile/Containers/Data/InternalDaemon -name "*LaunchServices*.csstore" 2>/dev/null); do mv "$f" /var/local/lsdb-backup/ && n=$((n+1)); done; echo "store LaunchServices messi da parte: $n (copia in /var/local/lsdb-backup)"'
python3 -I tools/flame-cmd.py sync >/dev/null 2>&1 || true
echo "fatto: riavvia (lsd ricostruisce il database e le app compaiono)"
