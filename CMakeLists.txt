cmake_minimum_required(VERSION 3.10)
project(oai-csi-logging C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -fPIC")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(csi_logging SHARED
  src/csi_ring_buffer.c
  src/csi_logging_callback.c
)

target_include_directories(csi_logging PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(csi_logging PRIVATE pthread)

install(TARGETS csi_logging
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

install(FILES include/csi_rb_logging_external.h
  DESTINATION include
)
