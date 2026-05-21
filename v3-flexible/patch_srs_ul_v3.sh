#!/bin/bash
set -e
OAI_DIR=${OAI_DIR:-/oai-ran}
FILE="$OAI_DIR/openair1/SCHED_NR/phy_procedures_nr_gNB.c"
[ ! -f "$FILE" ] && echo "ERROR: $FILE not found" && exit 1

echo "Patching $FILE for CSI logging v3..."

# Add extern declaration
if ! grep -q "extern void nr_srs_csi_logging_invoke_v3" "$FILE"; then
  LINE=$(grep -n "^nr_srs_rx_procedures" "$FILE" | head -1 | cut -d: -f1)
  if [ -n "$LINE" ]; then
    sed -i "$((LINE-1))i extern void nr_srs_csi_logging_invoke_v3(uint32_t frame_rx, uint16_t slot_rx, uint16_t rnti, uint8_t nb_antennas_rx, uint8_t N_ap, uint8_t N_symb_SRS, uint16_t ofdm_symbol_size, uint16_t bwp_start, uint16_t bwp_size, const c16_t srs_estimated_channel_freq[][N_ap][ofdm_symbol_size * N_symb_SRS]);" "$FILE"
    echo "✅ Added extern declaration"
  fi
fi

# Add CSI logging call (search for specific context to avoid matching extern)
if ! grep -q "nr_srs_csi_logging_invoke_v3(frame_rx, slot_rx" "$FILE"; then
  sed -i '/stop_meas(&gNB->srs_timing_advance_stats);/a\    nr_srs_csi_logging_invoke_v3(frame_rx, slot_rx, srs_pdu->rnti, nb_antennas_rx, N_ap, N_symb_SRS, ofdm_symbol_size, srs_pdu->bwp_start, srs_pdu->bwp_size, srs_estimated_channel_freq);' "$FILE"
  echo "✅ Added CSI logging call v3"
fi

echo "✅ $FILE patched for CSI logging v3"
