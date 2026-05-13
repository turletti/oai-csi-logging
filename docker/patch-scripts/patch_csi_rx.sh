#!/bin/bash
OAI_DIR="${OAI_DIR:-/oai-ran}"
FILE="$OAI_DIR/openair1/PHY/NR_UE_TRANSPORT/csi_rx.c"
[ ! -f "$FILE" ] && echo "❌ File not found: $FILE" && exit 1
! grep -q "#include \"csi_oai_wrapper.h\"" "$FILE" && sed -i '/#include "PHY\/NR_UE_ESTIMATION\/filt16a_32.h"/a #include "csi_oai_wrapper.h"' "$FILE" && echo "✅ Added csi_oai_wrapper.h"
! grep -q "#include \"csi_rb_logging_external.h\"" "$FILE" && sed -i '/#include "csi_oai_wrapper.h"/a #include "csi_rb_logging_external.h"' "$FILE" && echo "✅ Added csi_rb_logging_external.h"
! grep -q "extern void nr_csi_logging_invoke" "$FILE" && sed -i '/extern openair0_config_t openair0_cfg\[MAX_CARDS\];/a extern void nr_csi_logging_invoke(uint32_t frame, uint32_t slot, uint16_t start_rb, uint16_t nr_of_rbs, uint16_t nb_antennas_rx, uint16_t nb_ports, const void *csi_data, uint16_t ofdm_symbol_size);' "$FILE" && echo "✅ Added extern"
if ! grep -q "csirs_config_pdu && csi_rb_logging_enabled" "$FILE"; then
  awk '/[[:space:]]*&noise_power\);/ && !done {print $0; print ""; print "  /* Log CSI-RS channel estimation at UE */"; print "  if (csirs_config_pdu && csi_rb_logging_enabled) {"; print "    nr_csi_logging_invoke(proc->frame_rx, proc->nr_slot_rx,"; print "                          csirs_config_pdu->start_rb, csirs_config_pdu->nr_of_rbs,"; print "                          frame_parms->nb_antennas_rx, mapping_parms.ports,"; print "                          (const void*)&csi_rs_estimated_channel_freq[0][0][0],"; print "                          frame_parms->ofdm_symbol_size);"; print "  }"; done=1; next} {print}' "$FILE" > "$FILE.tmp" && mv "$FILE.tmp" "$FILE" && echo "✅ Added CSI logging call"
fi
echo "✅ csi_rx.c patched"
