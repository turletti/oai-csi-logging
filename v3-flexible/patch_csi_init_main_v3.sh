#!/bin/bash
# Patch nr-softmodem.c to initialize CSI v3 logging from Helm environment variables
set -e

FILE="${OAI_DIR:-/oai-ran}/executables/nr-softmodem.c"

if [ ! -f "$FILE" ]; then
  echo "❌ File not found: $FILE"
  exit 1
fi

echo "Patching $FILE for CSI v3 initialization..."

# Add header include if not present
if ! grep -q "#include.*csi_rb_logging_v3.h" "$FILE"; then
  sed -i '/#include.*config.h/a #include "csi_rb_logging_v3.h"' "$FILE"
  echo "✅ Added #include csi_rb_logging_v3.h"
fi

# Add initialization call in main() if not present
if ! grep -q "nr_csi_logging_init_v3" "$FILE"; then
  sed -i '/^int main( int argc, char \*\*argv ) {/a \  /* Initialize CSI v3 logging from Helm environment variables */ \n  if (getenv("CSI_LOGGING_ENABLED")) { \n    char csi_config[512] = ""; \n    const char *granul = getenv("CSI_GRANULARITY"); \n    const char *ant_sel = getenv("CSI_ANTENNA_SELECTION"); \n    const char *port_sel = getenv("CSI_PORT_SELECTION"); \n    const char *samp = getenv("CSI_SUBCARRIER_SAMPLING"); \n    const char *out_dir = getenv("CSI_OUTPUT_DIR"); \n    if (granul) snprintf(csi_config + strlen(csi_config), sizeof(csi_config) - strlen(csi_config), "granularity=%s ", granul); \n    if (ant_sel) snprintf(csi_config + strlen(csi_config), sizeof(csi_config) - strlen(csi_config), "antenna_selection=%s ", ant_sel); \n    if (port_sel) snprintf(csi_config + strlen(csi_config), sizeof(csi_config) - strlen(csi_config), "port_selection=%s ", port_sel); \n    if (samp) snprintf(csi_config + strlen(csi_config), sizeof(csi_config) - strlen(csi_config), "subcarrier_sampling=%s ", samp); \n    nr_csi_logging_init_v3(csi_config, 64, 1, out_dir ? out_dir : "/data/csi"); \n  }' "$FILE"
  echo "✅ Added nr_csi_logging_init_v3() initialization call"
fi

echo "✅ nr-softmodem.c patched for CSI v3"
