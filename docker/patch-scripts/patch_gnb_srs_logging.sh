#!/bin/bash
FILE="${OAI_DIR:-/oai-ran}/openair1/SCHED_NR/phy_procedures_nr_gNB.c"
[ ! -f "$FILE" ] && echo "❌ File not found: $FILE" && exit 1

# Add include
! grep -q "#include.*csi_rb_logging_external.h" "$FILE" && \
  sed -i '1s/^/#include "csi_rb_logging_external.h"\n/' "$FILE" && \
  echo "✅ Added #include csi_rb_logging_external.h"

# Add extern if not present
! grep -q "extern void nr_srs_csi_logging_invoke" "$FILE" && \
  sed -i '/^#include/a extern void nr_srs_csi_logging_invoke(uint32_t frame, uint32_t slot, uint16_t start_rb, uint16_t nr_of_rbs, const c16_t srs_estimated_channel_freq[], uint16_t ofdm_symbol_size);' "$FILE" && \
  echo "✅ Added extern nr_srs_csi_logging_invoke"

# Add logging call after nr_srs_channel_interpolation - find the line with frame_parms->delay_table);
if ! grep -q "nr_srs_csi_logging_invoke.*frame_rx.*slot_rx" "$FILE"; then
  sed -i '/frame_parms->delay_table);$/a\        if (csi_rb_logging_enabled) {\n          nr_srs_csi_logging_invoke(frame_rx, slot_rx, srs_pdu->bwp_start, srs_pdu->bwp_size,\n                                    (const c16_t *)srs_estimated_channel_freq[ant_rx_ind][p_ind],\n                                    ofdm_symbol_size);\n        }' "$FILE" && \
  echo "✅ Added CSI logging call after nr_srs_channel_interpolation"
fi

echo "✅ phy_procedures_nr_gNB.c patched"
