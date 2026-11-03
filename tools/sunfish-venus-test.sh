#!/bin/sh
# Video stages 2-3 on the running phone (docs/features/video.md "Stage 2/3 spec"): one venusctl step, then venusctl
# shutdown + power off (except probe and memtest, which don't power anything), printing each command's output.
#   tools/sunfish-venus-test.sh probe|power|pas|boot|memtest|ping|session|decode
#     probe    venusctl probe (read-only)
#     power    probe, power on, probe
#     pas      power on, pas-check
#     boot     power on, boot (PAS load + auth + memory protect, undone by boot itself)
#     memtest  the init --phys region: memtest write, 60 s under whatever load runs, memtest check (no power on)
#     ping     power on, boot --keep, init, ping (no PING_ACK is only a note: this firmware reads SYS_PING as a
#              session command)
#     session  power on, boot --keep, init, session (SYS_SESSION_INIT H.264 decoder -> 0x20006, SESSION_END -> 0x20007)
#     decode   stage 3: power on, boot --keep, init, decode $VENUS_IN $VENUS_OUT --frames $VENUS_FRAMES (defaults
#              /var/local/venus/in.h264, /var/local/venus/out.yuv, 5; Annex B in, cropped NV12 out). Buffers go in a
#              window right after the --phys queue block (VENUS_PHYS 0xa5100000 -> 0xa5500000, at most 64 MB), or at
#              VENUS_BUFPHYS=ADDR. Pull OUT and compare: tools/sunfish-venus-yuvcmp.py REF.nv12 OUT.yuv W H
# Every step that powers on ends with venusctl shutdown + power off, also after a failed step. The SMMU release counts
# as done once each SMR reads invalid (other register differences are printed as warnings, saved vs read).
# Pre-run review (research/notes/venus-stage1-2026-11-02.md 8.5): every power on/off gets --cvp (mvs1 domain on,
# as downstream); a timed-out command (exit 124) or a failed shutdown stops the run with no power off
# ("LEFT POWERED: reboot needed"): a GDSC/clock collapse under live firmware hangs the bus.
# VENUSCTL_* in the environment (the decoder switches) are passed to every venusctl on the phone.
# Env: VENUS_V=1 adds -v (every register write); VENUS_PHYS=ADDR passes init --phys ADDR (when PMEM_ALLOC32 fails);
# VENUS_T=SECONDS per-command ssh timeout (60); VENUS_MT="ADDR SIZE" memtest region (0xa5100000 0x301000).
# Needs /var/local/bin/venusctl and /var/local/fw/venus on the phone.
cd "$(dirname "$0")/.." || exit 1
step=$1
case "$step" in probe|power|pas|boot|memtest|ping|session|decode) ;; *) echo "usage: $0 probe|power|pas|boot|memtest|ping|session|decode" >&2; exit 2;; esac
v=${VENUS_V:+-v}
t=${VENUS_T:-60}
mt=${VENUS_MT:-0xa5100000 0x301000}
rc=0

run() {	# one venusctl command per ssh call, so a hang or reset shows its last line
	ve=$(env | grep -E '^VENUSCTL_[A-Z0-9_]+=[A-Za-z0-9_.x-]*$' | tr '\n' ' ')	# the decoder's switches, passed to the phone
	echo "=== ${ve}venusctl $v $*"
	timeout "$t" tools/sunfish-ssh "${ve}venusctl $v $*" 2>&1
	r=$?
	echo "=== exit $r"
	if [ $r = 124 ]; then	# a blocked SMC keeps the pmem lock: anything after it queues behind (review 8.5.2)
		echo "=== venusctl $* timed out after ${t}s: stopped, no shutdown or power off"
		echo "=== LEFT POWERED: reboot needed"
		echo "=== $step: FAILED"
		exit 124
	fi
	return $r
}

cleanup() {
	if ! run shutdown; then
		echo "=== shutdown failed: no power off (the firmware may still run)"
		echo "=== LEFT POWERED: reboot needed"
		echo "=== $step: FAILED"
		exit 1
	fi
	run power off --cvp || rc=1
	run probe
}

if [ "$step" = probe ]; then
	run probe
	exit $?
fi
if [ "$step" = memtest ]; then
	run memtest $mt write || exit 1
	echo "=== sleep 60 (load as it runs)"
	sleep 60
	run memtest $mt check
	r=$?
	echo "=== memtest: $([ $r = 0 ] && echo ok || echo FAILED)"
	exit $r
fi
if [ "$step" = power ]; then
	run probe
fi
if run power on --cvp; then
	case "$step" in
	power) run probe || rc=1 ;;
	pas) run pas-check || rc=1 ;;
	boot) run boot || rc=1 ;;
	ping|session)
		if run boot --keep && run init ${VENUS_PHYS:+--phys $VENUS_PHYS}; then
			run "$step" || rc=1
		else
			rc=1
		fi ;;
	decode)
		if run boot --keep && run init ${VENUS_PHYS:+--phys $VENUS_PHYS}; then
			run decode "${VENUS_IN:-/var/local/venus/in.h264}" "${VENUS_OUT:-/var/local/venus/out.yuv}" \
				--frames "${VENUS_FRAMES:-5}" ${VENUS_BUFPHYS:+--bufphys $VENUS_BUFPHYS} || rc=1
		else
			rc=1
		fi ;;
	esac
else
	rc=1
fi
cleanup
echo "=== $step: $([ $rc = 0 ] && echo ok || echo FAILED)"
exit $rc
