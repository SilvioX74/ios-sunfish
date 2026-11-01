#!/bin/bash
# UI soak on the running boot: opens stock apps in turn and scrolls inside each (8 s swipes), for the nightly GPU bug
# counts in docs/gpu-bugs.md. Sends no reboot; prints one line of counts at the end, and stops early if the phone goes.
#   tools/sunfish-uisoak.sh TAG [SECONDS]    default 3600
cd "$(dirname "$0")/.." || exit 1
tag=${1:?tag} secs=${2:-3600} L=../logs/usb-live.log
apps="com.apple.Preferences com.apple.mobilesafari com.apple.mobilenotes com.apple.mobilecal com.apple.Maps com.apple.mobileslideshow"
C() { python3 -I tools/flame-cmd.py --wait 0 "$@" >/dev/null 2>&1; }
pid() { timeout 15 tools/sunfish-ssh "psx | grep ' backboardd\$' | sed 's/^ *//' | cut -d' ' -f1" 2>/dev/null | tr -d '\r\n'; }
echo "=== uisoak $tag start $(date +%T)" | tee -a ../logs/boots/current.log >> "$L"
off=$(wc -c < "$L") t0=$SECONDS end=$((SECONDS + secs)) i=0 restarts=0 stop=done
p0=$(pid); [ -n "$p0" ] || { echo "$tag: phone not answering"; exit 3; }
while [ $SECONDS -lt $end ]; do
	for a in $apps; do
		[ $SECONDS -ge $end ] && break 2
		p=$(pid)
		[ -n "$p" ] || { stop="ssh lost at $((SECONDS - t0)) s"; break 2; }
		tail -c +"$off" "$L" | grep -a -q 'recover2: FAILED' && { stop="failed GPU recovery at $((SECONDS - t0)) s: rest would be CPU drawing"; break 2; }
		[ "$p" != "$p0" ] && { restarts=$((restarts + 1)); echo "=== uisoak $tag backboardd $p0 -> $p at $((SECONDS - t0)) s" >> "$L"; p0=$p; }
		timeout 25 tools/sunfish-ssh "/var/local/bin/openapp $a" >/dev/null 2>&1; sleep 2
		for d in 0 1 2; do
			[ $SECONDS -ge $end ] && break 3
			if [ $(((i + d) % 2)) = 0 ]; then C swipe 540 2100 540 300 8000; else C swipe 540 300 540 2100 8000; fi
			sleep 8.5
		done
		i=$((i + 1))
	done
done
echo "=== uisoak $tag end $stop $(date +%T)" | tee -a ../logs/boots/current.log >> "$L"
tail -c +"$off" "$L" | grep -a -E 'GPU hang or fault|submit failed|recover2: FAILED|SWEET GPU: 5s|SWEET THERM: (a55|cap|CRITICAL)' | awk -v t="$tag" -v m=$((SECONDS - t0)) -v r="$restarts" -v st="$stop" '
	/SWEET GPU: 5s/ { for (k = 1; k < NF; k++) if ($(k + 1) ~ /^submits/) s += $k }
	/GPU hang or fault/ && /fsr 0[^0-9a-f]/ { g1++ }
	/recover2: FAILED/ { failed++ }
	/GPU hang or fault/ && /fsr 40000002/ { g4++ }
	/submit failed/ { g2++ }
	/SWEET THERM: cap/ { caps++ }
	/SWEET THERM: CRITICAL/ { crit++ }
	/SWEET THERM: a55/ { for (k = 1; k < NF; k++) if ($k ~ /^(a55|cpu6|cpu7)$/ && $(k + 1) + 0 > mx) mx = $(k + 1) + 0 }
	END { printf "%s: %d s (%s), %d submits, G1 %d, G2 %d, G4 %d, backboardd restarts %d, max %.1f C, caps %d, critical %d, failed recoveries %d (CPU drawing after that: void)\n", t, m, st, s, g1, g2, g4, r, mx, caps, crit, failed }'
