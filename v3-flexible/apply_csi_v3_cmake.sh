#!/bin/bash
set -e

OAI_CMAKE=${OAI_DIR:-/oai-ran}/CMakeLists.txt

if [ ! -f "$OAI_CMAKE" ]; then
  echo "ERROR: $OAI_CMAKE not found"
  exit 1
fi

echo "Applying CSI v3 linking to $OAI_CMAKE..."

cat >> "$OAI_CMAKE" << 'CMAKELISTS'

# ============================================================================
# CSI v3 LOGGING INTEGRATION (V2-inspired methodology)
# ============================================================================
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSON_C REQUIRED json-c)

# KEY: Add library search directories for ninja/linker
link_directories(${JSON_C_LIBRARY_DIRS})

# Create an IMPORTED library target for CSI v3
add_library(csi_logging_v3 STATIC IMPORTED GLOBAL)
set_target_properties(csi_logging_v3 PROPERTIES
  IMPORTED_LOCATION /usr/local/lib/libcsi_logging_v3.a
  INTERFACE_INCLUDE_DIRECTORIES /usr/local/include
)

# Link CSI v3 library to main executables
foreach(target nr-softmodem nr-uesoftmodem nr-cuup nr-du)
  if(TARGET ${target})
    target_link_libraries(${target} PRIVATE csi_logging_v3 ${JSON_C_LIBRARIES})
    message(STATUS "CSI v3: Linked to ${target}")
  endif()
endforeach()

# ============================================================================
CMAKELISTS

echo "✅ CSI v3 linking configuration appended to CMakeLists.txt"

grep -q "CSI v3 LOGGING INTEGRATION" "$OAI_CMAKE" || \
  (echo "❌ VERIFICATION FAILED" && exit 1)

echo "✅ CSI v3 linking configuration verified"
