#!/bin/sh
# Video stage 2 on the running phone (docs/features/video.md "Stage 2 spec"): one venusctl step, then always
# venusctl shutdown + power off (except probe, which stays read-only), printing each command's output.
#   tools/sunfish-venus-test.sh probe|power|pas|boot|ping
#     probe  venusctl probe (read-only)
#     power  probe, power on, probe
#     pas    power on, pas-check
#     boot   power on, boot (PAS load + auth + memory protect, undone by boot itself)
#     ping   power on, boot --keep, init, ping
# Env: VENUS_V=1 adds -v (every register write); VENUS_PHYS=ADDR passes init --phys ADDR (when PMEM_ALLOC32 fails);
# VENUS_T=SECONDS per-command ssh timeout (60). Needs /var/local/bin/venusctl and /var/local/fw/venus on the phone.
cd "$(dirname "$0")/.." || exit 1
step=$1
case "$step" in probe|power|pas|boot|ping) ;; *) echo "usage: $0 probe|power|pas|boot|ping" >&2; exit 2;; esac
v=${VENUS_V:+-v}
t=${VENUS_T:-60}
rc=0

run() {	# one venusctl command per ssh call, so a hang or reset shows its last line
	echo "=== venusctl $v $*"
	timeout "$t" tools/sunfish-ssh "venusctl $v $*" 2>&1
	r=$?
	echo "=== exit $r"
	return $r
}

cleanup() {
	run shutdown || rc=1
	run power off || rc=1
	run probe
}

if [ "$step" = probe ]; then
	run probe
	exit $?
fi
if [ "$step" = power ]; then
	run probe
fi
if run power on; then
	case "$step" in
	power) run probe || rc=1 ;;
	pas) run pas-check || rc=1 ;;
	boot) run boot || rc=1 ;;
	ping)
		if run boot --keep && run init ${VENUS_PHYS:+--phys $VENUS_PHYS}; then
			run ping || rc=1
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
