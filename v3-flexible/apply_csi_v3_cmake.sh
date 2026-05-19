#!/bin/bash
# Integrate CSI v3 into OAI CMakeLists.txt

set -e

OAI_CMAKE=${1:-/opt/oai-ran/CMakeLists.txt}
CSI_V3_DIR=${CSI_V3_DIR:-/home/gitlab-runner/oai-csi-logging/v3-flexible}

if [ ! -f "$OAI_CMAKE" ]; then
  echo "ERROR: $OAI_CMAKE not found"
  exit 1
fi

echo "Integrating CSI v3 into OAI CMakeLists.txt..."

# Add CSI v3 subdirectory if not present
if ! grep -q "add_subdirectory.*csi_logging_v3" "$OAI_CMAKE"; then
  sed -i '/add_subdirectory(openair1)/a add_subdirectory('"$CSI_V3_DIR"')' "$OAI_CMAKE"
  echo "✅ Added CSI v3 subdirectory"
fi

# Link CSI v3 to nr-softmodem target
if ! grep -q "target_link_libraries.*csi_logging_v3" "$OAI_CMAKE"; then
  sed -i '/target_link_libraries(nr-softmodem/a\  csi_logging_v3' "$OAI_CMAKE"
  echo "✅ Linked CSI v3 to nr-softmodem"
fi

echo "✅ CSI v3 integrated into CMakeLists.txt"
