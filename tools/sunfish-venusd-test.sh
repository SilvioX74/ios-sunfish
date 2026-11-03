#!/bin/sh
# Video 4a on the running phone (docs/features/video.md "Stage 4 spec"): venusd started by hand (detached with &, in the
# background on the phone), one venusc run through it, venusd stopped (SIGTERM: it shuts Venus down, PAS shutdown
# first, then power off), then venusd's log lines of this run.
#   tools/sunfish-venusd-test.sh [IN [OUT]] [venusc options: --frames N, --null]
#     IN default /var/local/venus/in.h264, OUT /var/local/venus/out.nv12 (cropped NV12, as venusctl decode's: compare
#     with tools/sunfish-venus-yuvcmp.py REF.nv12 OUT W H)
#   e.g. tools/sunfish-venusd-test.sh /var/local/venus/in-1080p-300.h264 --null
# Env: VENUSD_T per-venusc timeout in s (120); VENUSD_ENV extra environment for venusd (e.g. "VENUSD_VERBOSE=1
# VENUSD_IDLE_S=10"); VENUSD_RUNS how many venusc runs in a row (1; > 1 checks that Venus stays up between sessions).
# Refuses while a venusd runs (the launchd job) or a venusctl STATE exists (a venusctl run is active). LEFT POWERED:
# venusd didn't stop within 20 s, or it exited with STATE left (its shutdown failed): reboot needed.
# Needs /var/local/bin/{venusd,venusc} (tools/sunfish-install-venusd.sh --tools) and /var/local/fw/venus.
cd "$(dirname "$0")/.." || exit 1
in=/var/local/venus/in.h264
out=/var/local/venus/out.nv12
case "${1:-}" in -*|"") ;; *) in=$1; shift; case "${1:-}" in -*|"") ;; *) out=$1; shift;; esac;; esac
t=${VENUSD_T:-120}
runs=${VENUSD_RUNS:-1}
S() { tools/sunfish-ssh "$@"; }
LOG=/var/local/venusd.log

if S "/bin/ps ax | grep -q '[/]var/local/bin/venusd'"; then
	echo "=== venusd already runs on the phone (launchd job?): refused"; exit 2
fi
if S "[ -e /var/tmp/venusctl.state ]"; then
	echo "=== /var/tmp/venusctl.state exists (a venusctl run is active): refused"; exit 2
fi
size=$(S "[ -e $LOG ] && wc -c <$LOG || echo 0" | tr -d ' \r')
echo "=== start: ${VENUSD_ENV:+$VENUSD_ENV }venusd --log $LOG"
S "env ${VENUSD_ENV:-} /var/local/bin/venusd --log $LOG >/dev/null 2>&1 </dev/null &"
i=0
while ! S "[ -S /var/run/venusd.sock ]"; do
	i=$((i + 1))
	[ $i -ge 10 ] && { echo "=== no /var/run/venusd.sock after 10 s"; S "tail -c +$((size + 1)) $LOG"; exit 1; }
	sleep 1
done
rc=0
n=1
while [ $n -le "$runs" ]; do
	echo "=== venusc $in $out $* (run $n of $runs)"
	timeout "$t" tools/sunfish-ssh "venusc $in $out $*" 2>&1
	r=$?
	echo "=== exit $r"
	[ $r = 0 ] || rc=1
	n=$((n + 1))
done
echo "=== stop: SIGTERM"
S "kill -TERM \$(cat /var/run/venusd.pid)"
i=0
while S "/bin/ps ax | grep -q '[/]var/local/bin/venusd'"; do
	i=$((i + 1))
	if [ $i -ge 20 ]; then
		echo "=== venusd still running 20 s after SIGTERM: LEFT POWERED (maybe): check $LOG, reboot needed"
		S "tail -c +$((size + 1)) $LOG | grep -v -e '^venus: set ' -e '^venus: cmd ' -e '^venus:   '"
		echo "=== venusd: FAILED"; exit 1
	fi
	sleep 1
done
echo "=== venusd log (this run, without the property/buffer lines):"
S "tail -c +$((size + 1)) $LOG | grep -v -e '^venus: set ' -e '^venus: cmd ' -e '^venus:   '"
if S "[ -e /var/tmp/venusctl.state ]"; then
	echo "=== venusd exited with /var/tmp/venusctl.state left: its shutdown failed: LEFT POWERED, reboot needed"
	echo "=== venusd: FAILED"; exit 1
fi
echo "=== venusd: $([ $rc = 0 ] && echo ok || echo FAILED)"
exit $rc
