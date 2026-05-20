# CSI Logging v3 - Flexible MIMO
cmake_minimum_required(VERSION 3.10)
project(csi_logging_v3 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -fPIC")

# Find json-c (required for metadata)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSON_C json-c)

if(NOT JSON_C_FOUND)
  message(WARNING "json-c not found, CSI metadata will be disabled")
  set(JSON_C_INCLUDE_DIRS "")
  set(JSON_C_LIBRARIES "")
else()
  include_directories(${JSON_C_INCLUDE_DIRS})
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
set(CSI_V3_SOURCES
  src/csi_config_parser.c
  src/csi_logging_callback_v3.c
  src/csi_integration_v3.c
)

# Static library
add_library(csi_logging_v3 STATIC ${CSI_V3_SOURCES})

if(JSON_C_FOUND)
  # Export json-c as interface dependency (consumers must link it)
  # Do not link into the static library itself (V2-compatible approach)
  set_target_properties(csi_logging_v3 PROPERTIES
    INTERFACE_LINK_LIBRARIES "${JSON_C_LIBRARIES}"
  )
  target_include_directories(csi_logging_v3 PUBLIC ${JSON_C_INCLUDE_DIRS})
endif()

# Export for use in OAI
target_include_directories(csi_logging_v3 PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# Installation
install(TARGETS csi_logging_v3
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

install(FILES include/csi_rb_logging_v3.h DESTINATION include)

message(STATUS "CSI Logging v3 configured")
