#!/bin/bash
FILE="/oai-ran/executables/nr-softmodem.c"
if [ ! -f "$FILE" ]; then
  echo "ERROR: $FILE not found"
  exit 1
fi
sed -i '/#include "nr-softmodem.h"/a #include "csi_logging_init.h"' "$FILE"
# REMOVED: sed -i '/logInit();/a \  csi_logging_init_from_env();' "$FILE"
echo "Patch applied"
