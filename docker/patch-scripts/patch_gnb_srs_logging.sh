#!/bin/bash
FILE="${OAI_DIR:-/oai-ran}/openair1/SCHED_NR/phy_procedures_nr_gNB.c"
[ ! -f "$FILE" ] && echo "❌ File not found: $FILE" && exit 1

# Add include ONCE - check first
if ! grep -q "#include \"csi_rb_logging_external.h\"" "$FILE"; then
  sed -i '1i #include "csi_rb_logging_external.h"' "$FILE"
  echo "✅ Added #include"
fi

# Add extern ONCE - check first
if ! grep -q "extern void nr_srs_csi_logging_invoke" "$FILE"; then
  sed -i '/^#include "csi_rb_logging_external.h"/a extern void nr_srs_csi_logging_invoke(uint32_t frame, uint32_t slot, uint16_t start_rb, uint16_t nr_of_rbs, const void *srs_estimated_channel_freq, uint16_t ofdm_symbol_size);' "$FILE"
  echo "✅ Added extern declaration"
fi

# Add logging call ONCE - check first
if ! grep -q "nr_srs_csi_logging_invoke.*frame_rx.*slot_rx" "$FILE"; then
  sed -i '/frame_parms->delay_table);$/a\        if (csi_rb_logging_enabled) {\n          nr_srs_csi_logging_invoke(frame_rx, slot_rx, srs_pdu->bwp_start, srs_pdu->bwp_size,\n                                    (const void *)srs_estimated_channel_freq[ant_rx_ind][p_ind],\n                                    ofdm_symbol_size);\n        }' "$FILE"
  echo "✅ Added logging call"
fi

echo "✅ Patch applied successfully"
