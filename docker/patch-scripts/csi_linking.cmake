# CSI logging library linking
# This file is included at the end of OAI's main CMakeLists.txt
# It defines the imported library target and links it to executables

add_library(csi_logging STATIC IMPORTED GLOBAL)
set_target_properties(csi_logging PROPERTIES
  IMPORTED_LOCATION /usr/local/lib/libcsi_logging.a
  INTERFACE_INCLUDE_DIRECTORIES /usr/local/include
)

# Link to all main executables
foreach(target nr-softmodem nr-uesoftmodem nr-cuup nr-du)
  if(TARGET ${target})
    target_link_libraries(${target} PRIVATE csi_logging)
  endif()
endforeach()
