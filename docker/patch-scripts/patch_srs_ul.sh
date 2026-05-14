#!/bin/bash
FILE="${OAI_DIR:-/oai-ran}/openair1/PHY/NR_ESTIMATION/nr_ul_channel_estimation.c"
[ ! -f "$FILE" ] && echo "❌ File not found: $FILE" && exit 1
! grep -q "#include \"csi_rb_logging_external.h\"" "$FILE" && sed -i '/#include "nr_ul_estimation.h"/a #include "csi_rb_logging_external.h"' "$FILE" && echo "✅ Added #include"
! grep -q "extern void nr_srs_csi_logging_invoke" "$FILE" && sed -i '/#include "nr_ul_estimation.h"/a extern void nr_srs_csi_logging_invoke(uint32_t frame, uint32_t slot, uint16_t start_rb, uint16_t nr_of_rbs, const c16_t srs_estimated_channel_freq[], uint16_t ofdm_symbol_size);' "$FILE" && echo "✅ Added extern"
if ! grep -q "csi_rb_logging_enabled && srs_estimated_channel_freq" "$FILE"; then
  awk '/^int nr_srs_channel_interpolation/ {in_func=1} in_func && /[[:space:]]*return 0;/ && !done {print "  /* Log CSI for uplink SRS channel estimation at gNB */"; print "  if (csi_rb_logging_enabled && srs_estimated_channel_freq) {"; print "    uint16_t start_rb = 0;"; print "    uint16_t nr_of_rbs = ofdm_symbol_size / 12;"; print "    nr_srs_csi_logging_invoke(0, 0, start_rb, nr_of_rbs,"; print "                              srs_estimated_channel_freq, ofdm_symbol_size);"; print "  }"; print ""; done=1} {print}' "$FILE" > "$FILE.tmp" && mv "$FILE.tmp" "$FILE" && echo "✅ Added CSI logging call"
fi
echo "✅ nr_ul_channel_estimation.c patched"
