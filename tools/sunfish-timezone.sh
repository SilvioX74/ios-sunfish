#!/bin/sh
# Time zone of the iOS on the phone: /var/db/timezone/localtime is a symlink into the zoneinfo database,
# and iOS picks a new target up at once (the lock-screen clock changes without a reboot).
#   tools/sunfish-timezone.sh [ZONE]      ZONE default Europe/Rome, e.g. US/Pacific to go back
# The root ships US/Pacific; this phone lives in Italy (the user, 2026-11-19).
set -eu
cd "$(dirname "$0")/.."
Z=${1:-Europe/Rome}
tools/sunfish-ssh "test -f /var/db/timezone/zoneinfo/$Z" || { echo "no zone $Z on the phone" >&2; exit 1; }
tools/sunfish-ssh "rm -f /var/db/timezone/localtime && ln -s /var/db/timezone/zoneinfo/$Z /var/db/timezone/localtime"
tools/sunfish-ssh 'ls -l /var/db/timezone/localtime; date'
