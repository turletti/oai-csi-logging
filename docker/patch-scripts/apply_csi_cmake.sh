#!/bin/bash
set -e

OAI_DIR="${1:-.}"
CMAKE_FILE="$OAI_DIR/CMakeLists.txt"

if [ ! -f "$CMAKE_FILE" ]; then
  echo "❌ CMakeLists.txt not found at $CMAKE_FILE"
  exit 1
fi

# Check if already included
if grep -q "csi_linking.cmake" "$CMAKE_FILE"; then
  echo "✅ csi_linking.cmake already included"
  exit 0
fi

# Add include() at the very end of CMakeLists.txt
# Find the last line and add our include before it
cat >> "$CMAKE_FILE" << 'EOF'

# CSI logging integration
include(csi_linking.cmake)
EOF

echo "✅ csi_linking.cmake include added to CMakeLists.txt"
