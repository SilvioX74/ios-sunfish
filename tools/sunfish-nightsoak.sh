#!/bin/bash
# The nightly soak: waits for 9:30 pm, sets work/night, boots the everyday setup with the slider at minimum, then 1 h of
# tools/sunfish-uisoak.sh. Starts detached, so it outlives the session that started it; any session can read the log.
#   tools/sunfish-nightsoak.sh TAG           starts it and returns; log in work/nightsoak-TAG.log, last line "finished HH:MM:SS"
# Only one runs at a time. Results go in docs/gpu-bugs.md ("Daily soak").
cd "$(dirname "$0")/.." || exit 1
TAG=${1:?usage: tools/sunfish-nightsoak.sh TAG}
LOG=work/nightsoak-$TAG.log
if [ "${2:-}" != --run ]; then
	setsid nohup "$0" "$TAG" --run > "$LOG" 2>&1 < /dev/null &
	echo "started, log in $LOG"
	exit 0
fi
exec 9> work/nightsoak.lock
flock -n 9 || { echo "another night soak is running"; exit 1; }
C() { python3 -I tools/flame-cmd.py --wait 0 "$@" >/dev/null 2>&1; }
now=$(date +%s); t=$(date -d '21:30' +%s); [ "$t" -gt "$now" ] && sleep $((t - now))
touch work/night
timeout 5 tools/sunfish-ssh true 2>/dev/null || { echo "phone not up at $(date +%T): soak not started"; exit 1; }
python3 -I tools/flame-cmd.py reboot >/dev/null 2>&1
for i in $(seq 60); do fastboot devices | grep -q fastboot && break; sleep 3; done
fastboot getvar battery-voltage 2>&1 | head -1
timeout 150 tools/sunfish-boot.sh soakn work/kernel139/kernel.development.sweet sweetse9=2 sweetcampmic=2 sweetswapbig=6 sweetampreset=1 sweetrowa55=6 sweetgpufreq=6 >/dev/null 2>&1
for i in $(seq 120); do timeout 4 tools/sunfish-ssh true 2>/dev/null && break; sleep 1; done
sleep 30
C H; sleep 1; C swipe 540 2330 540 1100 300; sleep 2; C swipe 540 2330 540 1100 300; sleep 2
# Control Center, slider down, closed again.
C swipe 1000 5 1000 900 300; sleep 2
for i in 1 2 3 4; do C swipe 660 850 660 1650 500; sleep 1.5; done
C swipe 540 2330 540 1100 300; sleep 1; C swipe 540 2330 540 1100 300; sleep 2
grep -a 'SWEET BL: nits' ../logs/boots/current.log | tail -1 | cut -c1-40
tools/sunfish-uisoak.sh "$TAG" 3600
grep -a 'SWEET PANEL: dcs 51' ../logs/boots/current.log | tail -1 | cut -c1-44
echo finished $(date +%T)
