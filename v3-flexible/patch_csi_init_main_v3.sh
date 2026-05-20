#!/bin/bash
# Patch nr-softmodem.c to initialize CSI v3 logging from environment variables
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
  # Find the main function and add init right after opening brace
  sed -i '/^int main( int argc, char \*\*argv ) {/a \  /* Initialize CSI v3 logging from environment */ \n  const char *csi_config = getenv("OAI_CSI_V3_CONFIG"); \n  const char *csi_output = getenv("OAI_CSI_OUTPUT_DIR"); \n  if (csi_config || csi_output) { \n    nr_csi_logging_init_v3(csi_config, 64, 1, csi_output ? csi_output : "/data/csi"); \n  }' "$FILE"
  echo "✅ Added nr_csi_logging_init_v3() initialization call"
fi

echo "✅ nr-softmodem.c patched for CSI v3"
