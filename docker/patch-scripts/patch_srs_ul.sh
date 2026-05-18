#!/bin/bash
FILE="${OAI_DIR:-/oai-ran}/openair1/SCHED_NR/phy_procedures_nr_gNB.c"
[ ! -f "$FILE" ] && echo "❌ File not found: $FILE" && exit 1

# Add includes
! grep -q "#include \"csi_rb_logging_external.h\"" "$FILE" && sed -i '/^#include ".*\.h"/a #include "csi_rb_logging_external.h"' "$FILE" && echo "✅ Added #include"

# Add extern declaration
! grep -q "extern void nr_srs_csi_logging_invoke" "$FILE" && sed -i '/^extern.*srs_channel/a extern void nr_srs_csi_logging_invoke(uint32_t frame, uint32_t slot, uint16_t start_rb, uint16_t nr_of_rbs, const c16_t srs_estimated_channel_freq[], uint16_t ofdm_symbol_size);' "$FILE" && echo "✅ Added extern"

# Add CSI logging call in nr_srs_rx_procedures - after timing advance stats
if ! grep -q "nr_srs_csi_logging_invoke.*frame_rx.*slot_rx" "$FILE"; then
  sed -i '/stop_meas(&gNB->srs_timing_advance_stats);/a\
\
    /* Log CSI for SRS channel estimation *\/\
    if (csi_rb_logging_enabled) {\
      uint16_t start_rb = srs_pdu->bwp_start;\
      uint16_t nr_of_rbs = srs_pdu->bwp_size;\
      for (int ant_rx_ind = 0; ant_rx_ind < nb_antennas_rx; ant_rx_ind++) {\
        nr_srs_csi_logging_invoke(frame_rx, slot_rx, start_rb, nr_of_rbs,\
                                  (c16_t*)srs_estimated_channel_freq[ant_rx_ind][0],\
                                  ofdm_symbol_size);\
      }\
    }' "$FILE" && echo "✅ Added CSI logging call"
fi

echo "✅ phy_procedures_nr_gNB.c patched for SRS CSI logging"
