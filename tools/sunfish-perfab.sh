#!/bin/bash
# A/B bench: boots each config N times in turn (A B A B), runs fixed screens, prints one table per screen, marks void runs.
#   tools/sunfish-perfab.sh TAG N [--screens settings,home,cc,apps,safari] [--secs 15] CONFIG [CONFIG...]
# CONFIG is NAME=KERNEL[,bootarg...][,+ENVKEY=VAL...][,@BUNDLEDIR], e.g. base=135,sweetswapbig=6 noalloc=135,sweetswapbig=6,+M2V_FAST_ALLOC=0
# KERNEL: NN (work/kernelNN/kernel.development.sweet) or a path. flamereboot=1800 sweettrace=1 are always added. A config
# without + lines runs with an empty /var/local/mtl2-env; one without @ with the bundle that was on the phone at the start.
# Results: work/perfab/TAG/results.tsv (a row per screen run, kept if the run is killed), one screenshot per boot (Settings), one per screen on boot 1.
# Env SETTLE: seconds between SSH up and the first screen (default 30). Exit: 0 done, 2 usage, 3 stopped early.
# Env MAXSTART: each screen waits up to 3 min for the hottest CPU to read below this (default 75 C), then goes on and notes the temperature.
# A window stops and is void at 95 C.
# About 2 min per boot with settings only, 4 min with all five screens (estimates: boot to SSH 20-40 s, SETTLE 30 s, screens ~30 s each).
cd "$(dirname "$0")/.." || exit 1
L=../logs/usb-live.log
usage() { sed -n 3,4p "$0" | cut -c3- >&2; exit 2; }
die() { echo "perfab: $*" >&2; exit 2; }
[ $# -ge 3 ] || usage
TAG=$1 N=$2; shift 2
SECS=15 SCREENS=settings,home,cc,apps,safari MAXSTART=${MAXSTART:-75}
SPECS=()
while [ $# -gt 0 ]; do
	case $1 in
	--screens) SCREENS=$2; shift 2;;
	--secs) SECS=$2; shift 2;;
	--*) usage;;
	*) SPECS+=("$1"); shift;;
	esac
done
case $TAG in ''|*[!A-Za-z0-9_-]*) die "bad TAG '$TAG' (letters, digits, - and _)";; esac
case $N in ''|*[!0-9]*|0) die "bad N '$N'";; esac
case $SECS in ''|*[!0-9]*|0) die "bad --secs '$SECS'";; esac
case $MAXSTART in ''|*[!0-9]*) die "bad MAXSTART '$MAXSTART'";; esac
[ ${#SPECS[@]} -ge 1 ] || usage
IFS=, read -ra SCR <<< "$SCREENS"
for s in "${SCR[@]}"; do
	case $s in settings|home|cc|apps|safari) ;; *) die "unknown screen '$s'";; esac
done

NAMES=() KFILE=() KNUM=() ARGS=() ENVS=() BUNDLE=()
for spec in "${SPECS[@]}"; do
	case $spec in *=*) ;; *) die "bad config '$spec'";; esac
	name=${spec%%=*} a= e= b=
	case $name in ''|*[!A-Za-z0-9_-]*|prep) die "bad config name '$name'";; esac
	case " ${NAMES[*]} " in *" $name "*) die "config name '$name' used twice";; esac
	IFS=, read -ra parts <<< "${spec#*=}"
	k=${parts[0]}
	case $k in
	''|*[!0-9]*) kf=$k;;
	*) kf=work/kernel$k/kernel.development.sweet;;
	esac
	[ -f "$kf" ] || die "$name: no kernel at $kf"
	for p in "${parts[@]:1}"; do
		case $p in
		+*=*) e+="${p#+}"$'\n';;
		@*) b=${p#@}; [ -s "$b/SweetMetal2.bundle/SweetMetal2" ] || die "$name: no SweetMetal2.bundle in $b";;
		*) a+="$p"$'\n';;
		esac
	done
	NAMES+=("$name") KFILE+=("$kf") KNUM+=("$k") ARGS+=("${a}flamereboot=1800"$'\n'"sweettrace=1") ENVS+=("${e%$'\n'}") BUNDLE+=("$b")
done
OUT=work/perfab/$TAG
[ -e "$OUT/results.tsv" ] && die "$OUT/results.tsv exists: use a new TAG (markers must be unique in the log)"
pgrep -f '^[^ ]*sh ([^-].*)?sunfish-livelog\.sh' >/dev/null || die "the live-log listener is not running (tools/sunfish-livelog.sh): not starting it, not booting"
mkdir -p "$OUT"
R=$OUT/results.tsv
PY="python3 -I tools/sunfish-perfab.py"
ENVF=/var/local/mtl2-env CLIENTS=/var/local/mtl2-clients

S() { timeout "${SSHT:-15}" tools/sunfish-ssh "$@" 2>/dev/null; }
C() { timeout 5 python3 -I tools/flame-cmd.py --wait 0 "$@" >/dev/null 2>&1; }
mark() { echo "=== perfab $TAG $* $(date +%T)" | tee -a ../logs/boots/current.log >> "$L"; }
logsize() { stat -c %s "$L"; }
ssh_up() { timeout 3 tools/sunfish-ssh true 2>/dev/null; }
fb_up() { timeout 5 fastboot devices 2>/dev/null | grep -q fastboot; }
stop() { STOPPED=$*; exit 3; }

# Phone state over the run; both files are kept on the phone, so they outlive reboots and hangs.
KNOWN=0 PHONE_ENV= PHONE_BUNDLE= ORIG_ENV= ORIG_ENV_THERE=0 ORIG_CLIENTS= ORIG_CLIENTS_THERE=0 RUN_OFF= STOPPED=
BOOTS_RUN=0

# Sets RTEXT and RTHERE from a phone file; fails if SSH does.
read_remote() {
	local out
	out=$(S "if [ -e $1 ]; then echo there; cat $1; else echo absent; fi") || return 1
	case $out in there*) RTHERE=1;; absent*) RTHERE=0;; *) return 1;; esac
	RTEXT=
	case $out in *$'\n'*) RTEXT=${out#*$'\n'};; esac
}
write_remote() {	# FILE TEXT
	{ [ -n "$2" ] && printf '%s\n' "$2"; } | S "cat > $1"
}
snapshot() {
	[ "$KNOWN" = 1 ] && return 0
	read_remote $ENVF || return 1
	ORIG_ENV=$RTEXT ORIG_ENV_THERE=$RTHERE PHONE_ENV=$RTEXT
	read_remote $CLIENTS || return 1
	ORIG_CLIENTS=$RTEXT ORIG_CLIENTS_THERE=$RTHERE
	printf '%s\n' "$ORIG_ENV" > "$OUT/orig-env"
	KNOWN=1
}
# Undo first, so the bundle at the start stays the .prev of any bench bundle. install writes mtl2-clients: put it back for
# a config without a bundle, or the sides of an A/B differ by more than the bundle.
bundle_undo() {
	SSHT=30 S true || return 1
	timeout 60 tools/sunfish-metal2-install.sh --undo --no-restart >/dev/null 2>&1 || return 1
	PHONE_BUNDLE=
	if [ "$ORIG_CLIENTS_THERE" = 1 ]; then write_remote $CLIENTS "$ORIG_CLIENTS"; else S "rm -f $CLIENTS"; fi
}
apply_state() {	# config index
	if [ "$PHONE_BUNDLE" != "${BUNDLE[$1]}" ]; then
		if [ -n "$PHONE_BUNDLE" ]; then bundle_undo || return 1; fi
		if [ -n "${BUNDLE[$1]}" ]; then
			timeout 180 tools/sunfish-metal2-install.sh "${BUNDLE[$1]}" --no-restart >/dev/null 2>&1 || return 1
			PHONE_BUNDLE=${BUNDLE[$1]}
		fi
	fi
	if [ "$PHONE_ENV" != "${ENVS[$1]}" ]; then
		write_remote $ENVF "${ENVS[$1]}" || return 1
		PHONE_ENV=${ENVS[$1]}
	fi
}
state_matches() { [ "$KNOWN" = 1 ] && [ "$PHONE_ENV" = "${ENVS[$1]}" ] && [ "$PHONE_BUNDLE" = "${BUNDLE[$1]}" ]; }

# Where the phone is, waiting up to 3 min; a kernel that answers neither gets a reset (sync waits at most 3 s, xnu 0246).
where_phone() {
	local t0=$SECONDS nudge=0
	while [ $((SECONDS - t0)) -lt 180 ]; do
		fb_up && { echo fastboot; return; }
		ssh_up && { echo ssh; return; }
		if [ $nudge = 0 ] && [ $((SECONDS - t0)) -ge 30 ]; then C reboot; nudge=1; fi
		if [ $nudge = 1 ] && [ $((SECONDS - t0)) -ge 90 ]; then C 'reboot!'; nudge=2; fi
		sleep 2
	done
	echo none
}
to_fastboot() {
	local t0=$SECONDS nudge=0
	timeout 10 python3 -I tools/flame-cmd.py reboot >/dev/null 2>&1
	while [ $((SECONDS - t0)) -lt 120 ]; do
		fb_up && return 0
		if [ $nudge = 0 ] && [ $((SECONDS - t0)) -ge 60 ]; then C 'reboot!'; nudge=1; fi
		sleep 2
	done
	return 1
}
# Only with the phone listed in fastboot: getvar hangs while iOS is up.
battery() {
	local i v
	for i in 1 2 3; do
		v=$(timeout 10 fastboot getvar battery-voltage 2>&1 | sed -n 's/^battery-voltage: *\([0-9][0-9]*\).*/\1/p')
		[ -n "$v" ] && break
		sleep 2
	done
	BATT=$v
	[ -n "$v" ] || stop "battery voltage unreadable"
	[ "$v" -ge 3600 ] || stop "battery $v mV, under 3600"
}
boot_cfg() {	# config index
	local args
	mapfile -t args <<< "${ARGS[$1]}"
	timeout 150 tools/sunfish-boot.sh "perfab-${NAMES[$1]}" "${KFILE[$1]}" "${args[@]}" > "$OUT/boot.log" 2>&1 ||
		stop "boot of ${NAMES[$1]} failed: $(tail -1 "$OUT/boot.log" | cut -c1-120)"
}
wait_ssh() {
	local i
	for i in $(seq 180); do ssh_up && return 0; sleep 1; done
	return 1
}
# Leaves the phone in fastboot with config i's env and bundle set (before the reboot: a backboardd restart would bring G3).
ready() {	# config index
	local try w
	for try in 1 2 3 4; do
		w=$(where_phone)
		case $w in
		ssh)
			snapshot && apply_state "$1" || { sleep 5; continue; }
			to_fastboot && return 0
			;;
		fastboot)
			state_matches "$1" && return 0
			# env on the phone unknown or wrong (start, or the last boot died): boot once to read and set it
			battery
			[ -n "$RUN_OFF" ] || RUN_OFF=$(logsize)
			mark prep "${NAMES[$1]}"
			boot_cfg "$1"
			wait_ssh || stop "no SSH on the boot that sets the phone's env"
			;;
		*) stop "neither SSH nor fastboot for 3 min";;
		esac
	done
	stop "could not set the env or reboot for ${NAMES[$1]}"
}

# Hottest of a55, cpu6, cpu7 in whole C from this boot's last THERM line; empty on a kernel without these lines.
hot_now() {
	tac ../logs/boots/current.log 2>/dev/null | grep -a -m1 'SWEET THERM: a55 ' |
		awk '{ for (i = 1; i < NF; i++) if ($i ~ /^(a55|cpu6|cpu7)$/ && $(i + 1) + 0 > m) m = $(i + 1) + 0 } END { if (m) print int(m) }'
}
# No input until the hottest CPU is below MAXSTART, up to 3 min, then on regardless. Leaves progress text in COOLED if it waited.
wait_cool() {
	local t0=$SECONDS t
	COOLED=
	t=$(hot_now)
	[ "${t:-0}" -ge "$MAXSTART" ] || return 0
	while [ "$t" -ge "$MAXSTART" ] && [ $((SECONDS - t0)) -lt 180 ]; do
		sleep 5
		t=$(hot_now); t=${t:-0}
	done
	COOLED=" cool $((SECONDS - t0))s ${t}C"
}
# Motion goes on until SECONDS reaches $1; in the window, only while the hottest CPU is under 95 C.
more() {
	[ $SECONDS -lt "$1" ] || return 1
	[ "${INWIN:-0}" = 1 ] || return 0
	local t
	t=$(hot_now)
	[ "${t:-0}" -lt 95 ] && return 0
	TOO_HOT=1
	return 1
}

bb_pid() {
	local i out
	for i in 1 2; do
		out=$(SSHT=10 S "psx | grep -E '[ /]backboardd\$' | sed 's/^ *//' | cut -d' ' -f1 | head -1") && { echo "${out:-none}"; return 0; }
		sleep 1
	done
	return 1
}
open_app() {	# bundle id; openapp returns once FrontBoard answered. rc 255 = SSH failed
	SSHT=25 S "openapp $1" >/dev/null
	local rc=$?
	[ $rc = 255 ] && SSH_LOST=1
	[ $rc = 0 ] || OPEN_FAILED=1
	return $rc
}
# flame-cmd --wait 0 returns while the swipe runs on the phone: every sleep is the swipe + 50 ms, so they never queue up.
# H only wakes the screen; the bottom swipe unlocks, then goes home from whatever app unlocking returned to.
go_home() { C swipe 540 2330 540 1100 300; sleep 1; }
# Night (work/night): each boot comes up at DCS 128 whatever the slider was: drag the slider down in Control Center. Before any window.
night_dim() { [ -e work/night ] || return 0; C H; sleep 1; go_home; sleep 1; C swipe 1000 5 1000 900 300; sleep 2; C swipe 660 900 660 1600 500; sleep 2; go_home; sleep 1; go_home; }
unlock() { C H; sleep 1; go_home; sleep 1; go_home; }
UP=0
long_swipes() {	# until SECONDS reaches $1
	while more "$1"; do
		if [ $UP = 0 ]; then C swipe 540 2000 540 400 800; UP=1; else C swipe 540 400 540 2000 800; UP=0; fi
		sleep 0.85
	done
}
# Tight home pages as in fpsbench BENCH_TIGHT.
home_swipes() {
	while more "$1"; do
		C swipe 900 1200 150 1200 400; sleep 0.45
		C swipe 150 1200 900 1200 400; sleep 0.45
	done
}
# Down then up in the left margin, off the sliders (brightness!). The up swipe can dismiss Control Center: so reopen each round.
cc_swipes() {
	while more "$1"; do
		C swipe 1000 5 1000 900 300; sleep 0.35
		C swipe 30 1100 30 1900 800; sleep 0.85
		C swipe 30 1900 30 1100 800; sleep 0.85
	done
}
APPS=(com.apple.calculator com.apple.mobilenotes com.apple.mobiletimer) APPI=0
app_cycle() {
	local bid t0 ms short
	while more "$1"; do
		bid=${APPS[$APPI]} APPI=$(((APPI + 1) % ${#APPS[@]}))
		t0=$(date +%s%N)
		open_app "$bid"
		[ -n "$SSH_LOST" ] && return
		ms=$((($(date +%s%N) - t0) / 1000000))
		short=${bid##*.}
		[ "${INWIN:-0}" = 1 ] && OPENS+="${OPENS:+,}$short=$ms"
		sleep 3
		go_home
	done
}
lead_in() {	# setup, then ~5 s of motion outside the window
	case $1 in
	settings) unlock; open_app com.apple.Preferences; sleep 2;;
	safari) unlock; open_app com.apple.mobilesafari; sleep 2;;
	home|cc|apps) unlock;;
	esac
}
motion() {	# screen, until SECONDS
	case $1 in
	settings|safari) long_swipes "$2";;
	home) home_swipes "$2";;
	cc) cc_swipes "$2";;
	apps) app_cycle "$2";;
	esac
}

# One screen: leaves the parser's verdict in VERDICT.
run_screen() {	# config index, boot number, screen
	local name=${NAMES[$1]} b=$2 s=$3 pid0 pid1 flags=()
	SSH_LOST= OPEN_FAILED= OPENS= INWIN=0 TOO_HOT=
	pid0=$(bb_pid) || SSH_LOST=1
	if [ -z "$SSH_LOST" ]; then
		lead_in "$s"
		motion "$s" $((SECONDS + 5))
		mark "$name b$b $s start"
		INWIN=1
		motion "$s" $((SECONDS + SECS))
		mark "$name b$b $s end"
		[ "$s" = cc ] && [ "$b" = 1 ] && { C swipe 1000 5 1000 900 300; sleep 1.5; }	# the last up swipe closes it: reopen for the picture
		{ [ "$s" = settings ] || [ "$b" = 1 ]; } && timeout 30 tools/flame-shot "$OUT/$name-b$b-$s.png" >/dev/null 2>&1
		go_home
		pid1=$(bb_pid) || SSH_LOST=1
		[ -z "$SSH_LOST" ] && [ "$pid0" != "$pid1" ] && flags+=(--restart)
	fi
	[ -n "$SSH_LOST" ] && flags+=(--ssh-lost)
	[ -n "$OPEN_FAILED" ] && flags+=(--open-failed)
	[ -n "$TOO_HOT" ] && flags+=(--too-hot)
	VERDICT=$($PY screen --log "$L" --offset "$OFF" --out "$R" --tag "$TAG" --name "$name" --boot "$b" --screen "$s" --secs "$SECS" \
		--kernel "${KNUM[$1]}" --batt "$BATT" --bootlog "$BLOG" --opens "$OPENS" "${flags[@]}")
	OFF=$(logsize)
}
void_row() {	# config index, boot number, screen, reason
	$PY screen --out "$R" --tag "$TAG" --name "${NAMES[$1]}" --boot "$2" --screen "$3" --kernel "${KNUM[$1]}" --batt "$BATT" \
		--bootlog "$BLOG" --void "$4" >/dev/null
}

# On any exit: back to the env and bundle from the start, when SSH answers; counts, table.
finish() {
	local rc=$? changed= i
	trap - EXIT
	trap '' INT TERM
	if [ "$KNOWN" = 1 ] && { [ -n "$PHONE_BUNDLE" ] || [ "$PHONE_ENV" != "$ORIG_ENV" ]; }; then
		if ssh_up; then
			if [ -n "$PHONE_BUNDLE" ]; then
				bundle_undo && changed="bundle" || echo "perfab: bundle undo failed: still ${PHONE_BUNDLE}"
			fi
			if [ "$PHONE_ENV" != "$ORIG_ENV" ]; then
				if [ "$ORIG_ENV_THERE" = 1 ]; then write_remote $ENVF "$ORIG_ENV"; else S "rm -f $ENVF"; fi &&
					changed="${changed:+$changed and }env" || echo "perfab: env restore failed"
			fi
			[ -n "$changed" ] && echo "Restored $changed on the phone: a reboot is needed for it to apply."
		else
			echo "perfab: no SSH: phone still has the run's env or bundle; start env saved in $OUT/orig-env${PHONE_BUNDLE:+, then tools/sunfish-metal2-install.sh --undo}"
		fi
	fi
	[ -n "$STOPPED" ] && echo "Stopped: $STOPPED"
	if [ -e "$R" ]; then
		$PY table "$R"
		if [ "$BOOTS_RUN" -gt 0 ]; then
			$PY counts --log "$L" --offset "${RUN_OFF:-0}" --tag "$TAG" --results "$R" --doc docs/gpu-bugs.md --configs "$(printf '%s; ' "${SPECS[@]}" | sed 's/; $//')"
		fi
	fi
	exit $rc
}
trap finish EXIT
trap 'stop "killed"' INT TERM

for b in $(seq "$N"); do
	for i in "${!NAMES[@]}"; do
		name=${NAMES[$i]} BATT= BLOG=
		ready "$i"
		battery
		[ -n "$RUN_OFF" ] || RUN_OFF=$(logsize)
		mark "$name b$b boot kernel ${KNUM[$i]} $BATT mV"
		BOOTS_RUN=$((BOOTS_RUN + 1))
		boot_cfg "$i"
		line="b$b $name $BATT mV:"
		if ! wait_ssh; then
			for s in "${SCR[@]}"; do void_row "$i" "$b" "$s" "no SSH after boot"; done
			echo "$line no SSH after boot"
			continue
		fi
		BLOG=$(basename "$(readlink ../logs/boots/current.log)")
		OFF=$(logsize)
		sleep "${SETTLE:-30}"
		night_dim
		abort=
		for s in "${SCR[@]}"; do
			if [ -n "$abort" ]; then
				void_row "$i" "$b" "$s" "rest of boot after: $abort"
				line+=" $s void"
				continue
			fi
			wait_cool
			line+=$COOLED
			run_screen "$i" "$b" "$s"
			case $VERDICT in
			OK*) line+=" $s ${VERDICT#OK }";;
			ABORT*) abort=${VERDICT#ABORT }; line+=" $s VOID ($abort)";;
			*) line+=" $s VOID (${VERDICT#VOID })";;
			esac
		done
		echo "$line"
	done
done
exit 0
