#!/bin/bash
# CSI logging v3.1: insert the logger call in nr_srs_rx_procedures(), right after the SRS channel
# estimation / TA block (inside "if (*srs_est >= 0)"), with a block-scope prototype so that no
# file-level anchor is needed (the v3 "^nr_srs_rx_procedures" anchor never matched -> implicit
# declaration).
set -e
OAI_DIR=${OAI_DIR:-/oai-ran}
FILE="$OAI_DIR/openair1/SCHED_NR/phy_procedures_nr_gNB.c"
[ ! -f "$FILE" ] && echo "ERROR: $FILE not found" && exit 1
ANCHOR='stop_meas(&gNB->srs_timing_advance_stats);'
n=$(grep -cF "$ANCHOR" "$FILE" || true)
[ "$n" != "1" ] && echo "ERROR: anchor found $n times (expected 1)" && exit 1
if ! grep -q "nr_srs_csi_logging_invoke_v3(frame_rx, slot_rx" "$FILE"; then
  sed -i "/stop_meas(&gNB->srs_timing_advance_stats);/a\\
    { /* CSI logging v3.1 */\\
      extern void nr_srs_csi_logging_invoke_v3(uint32_t, uint16_t, uint16_t, uint8_t, uint8_t, uint8_t, uint16_t, uint16_t, uint16_t, uint16_t, const void *);\\
      nr_srs_csi_logging_invoke_v3(frame_rx, slot_rx, srs_pdu->rnti, nb_antennas_rx, N_ap, N_symb_SRS, ofdm_symbol_size, frame_parms->first_carrier_offset, srs_pdu->bwp_start, srs_pdu->bwp_size, (const void *)srs_estimated_channel_freq);\\
    }" "$FILE"
  echo "✅ Added CSI logging call v3.1"
fi
grep -q "frame_parms->first_carrier_offset, srs_pdu->bwp_start" "$FILE" || { echo "ERROR: patch verification failed"; exit 1; }
echo "✅ $FILE patched for CSI logging v3.1"
