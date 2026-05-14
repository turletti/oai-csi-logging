#!/bin/bash
FILE="${OAI_DIR:-/oai-ran}/executables/nr-softmodem.c"
[ ! -f "$FILE" ] && echo "❌ File not found: $FILE" && exit 1

if ! grep -q "#include \"csi_logging_init.h\"" "$FILE"; then
  sed -i '/#include.*config.h/a #include "csi_logging_init.h"' "$FILE" && echo "✅ Added #include csi_logging_init.h"
fi

if ! grep -q "csi_logging_init_from_env" "$FILE"; then
  sed -i '/^int main( int argc, char \*\*argv ) {/a \  csi_logging_init_from_env();' "$FILE" && echo "✅ Added csi_logging_init_from_env() call"
fi

echo "✅ nr-softmodem.c patched"
