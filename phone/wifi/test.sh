#!/bin/sh
# Host tests: sp23 adspboot --audio-fifo ring and DC guard (af_ring.h), sp25 amp standby, smem.c bounds.
set -e
cd "$(dirname "$0")"
mkdir -p out
${CC:-cc} -O1 -g -Wall -Wextra -fsanitize=address,undefined -o out/test_af_ring test_af_ring.c -lm
./out/test_af_ring
# sp25: standby/resume against a fake amp
${CC:-cc} -O1 -g -Wall -Wextra -fsanitize=address,undefined -o out/test_cs_standby test_cs_standby.c
./out/test_cs_standby
# hakios review W: smem.c bounds on a fake SMEM
${CC:-cc} -O1 -g -Wall -Wextra -fsanitize=address,undefined -o out/test_smem test_smem.c
./out/test_smem
