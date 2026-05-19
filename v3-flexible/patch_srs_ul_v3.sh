#!/bin/bash
# Patch nr_srs_rx_procedures to call CSI logging v3

set -e

OAI_DIR=${OAI_DIR:-/oai-ran}
FILE="$OAI_DIR/openair1/SCHED_NR/phy_procedures_nr_gNB.c"

if [ ! -f "$FILE" ]; then
  echo "ERROR: $FILE not found"
  exit 1
fi

echo "Patching $FILE for CSI logging v3..."

# Add extern declaration ONCE (only if not already present)
if ! grep -q "extern void nr_srs_csi_logging_invoke_v3" "$FILE"; then
  # Add after last #include, before any function definitions
  sed -i '/^#include.*$/!b; $!{N;ba}; s/\n/\n\nextern void nr_srs_csi_logging_invoke_v3(uint32_t frame_rx, uint16_t slot_rx, uint16_t rnti, uint8_t nb_antennas_rx, uint8_t N_ap, uint8_t N_symb_SRS, uint16_t ofdm_symbol_size, uint16_t bwp_start, uint16_t bwp_size, const c16_t srs_estimated_channel_freq[][N_ap][ofdm_symbol_size * N_symb_SRS]);\n/' "$FILE"
  echo "✅ Added extern declaration"
fi

# Add CSI logging call after timing advance stats (EXACT location from code inspection)
if ! grep -q "nr_srs_csi_logging_invoke_v3" "$FILE"; then
  sed -i '/stop_meas(&gNB->srs_timing_advance_stats);/a\
\
    /* Log CSI for all RX antennas and TX ports (v3) */\
    if (csi_rb_logging_enabled) {\
      nr_srs_csi_logging_invoke_v3(frame_rx, slot_rx, srs_pdu->rnti,\
                                    nb_antennas_rx, N_ap, N_symb_SRS,\
                                    ofdm_symbol_size,\
                                    srs_pdu->bwp_start, srs_pdu->bwp_size,\
                                    srs_estimated_channel_freq);\
    }' "$FILE"
  echo "✅ Added CSI logging call v3"
fi

echo "✅ $FILE patched for CSI logging v3"
