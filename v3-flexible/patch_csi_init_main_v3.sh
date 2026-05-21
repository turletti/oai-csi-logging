#!/bin/bash
set -e
FILE="${OAI_DIR:-/oai-ran}/executables/nr-softmodem.c"
[ ! -f "$FILE" ] && echo "❌ File not found: $FILE" && exit 1
echo "Patching $FILE for CSI v3 initialization..."
if ! grep -q "#include.*csi_rb_logging_v3.h" "$FILE"; then
  sed -i '/#include.*config.h/a #include "csi_rb_logging_v3.h"' "$FILE"
  echo "✅ Added #include csi_rb_logging_v3.h"
fi
echo "✅ $FILE patched for CSI v3"
