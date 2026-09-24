#!/bin/bash
# Host-side tests of the CSI v3.1 logger (no OAI build needed).
#  - test_layout: OAI-layout (DC-centred) SRS buffer with RB-coded values; every logged row must hold
#    the value of its RB/antenna/port; run under ASan/UBSan.
#  - test_latency: worst logger call duration while the flush thread writes (realistic aw2s rate).
set -e
cd "$(dirname "$0")"
SRC="../src/csi_config_parser.c ../src/csi_logging_callback_v3.c ../src/csi_integration_v3.c"
JC="$(pkg-config --cflags --libs json-c)"
gcc -O1 -g -fsanitize=address,undefined -I ../include test_layout.c $SRC $JC -lpthread -o test_layout
run() {  # granularity N N_RB n_ant n_ap n_symb [bwp_start bwp_size]
  g=$1; shift; d=$(mktemp -d)
  CSI_OUTPUT_DIR=$d CSI_GRANULARITY=$g ./test_layout "$@" > $d/log 2>&1
  ! grep -q "AddressSanitizer\|runtime error" $d/log || { echo "SANITIZER: $*"; exit 1; }
  echo -n "[$g $*] "; python3 check_layout.py $d/csi_per_rb.csv $2 $3 $4 $5 $g ${6:-0} ${7:-$2}
}
run rb 1536 106 1 1 1; run rb 2048 133 1 1 1; run rb 2048 106 2 1 1; run rb 4096 273 4 2 1
run rb 1536 106 2 2 2; run rb 2048 106 1 4 4; run subcarrier 1536 106 2 2 1; run subcarrier 2048 133 1 1 2
run rb 1536 106 1 1 1 10 50
gcc -O2 -DPERIOD_US=7000 -I ../include test_latency.c $SRC $JC -lpthread -o test_latency
d=$(mktemp -d); CSI_OUTPUT_DIR=$d CSI_FLUSH_CORE=0 ./test_latency 2>/dev/null | tail -1
echo "rows in CSV: $(grep -c '^[0-9]' $d/csi_per_rb.csv)"
