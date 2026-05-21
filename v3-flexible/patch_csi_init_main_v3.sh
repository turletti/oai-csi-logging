#!/bin/bash
set -e
FILE="${OAI_DIR:-/oai-ran}/executables/nr-softmodem.c"
[ ! -f "$FILE" ] && echo "❌ File not found: $FILE" && exit 1

echo "Patching $FILE for CSI v3 initialization..."

if ! grep -q "#include.*csi_rb_logging_v3.h" "$FILE"; then
  sed -i '/#include.*config.h/a #include "csi_rb_logging_v3.h"' "$FILE"
  echo "✅ Added #include csi_rb_logging_v3.h"
fi

if ! grep -q "nr_csi_logging_init_v3" "$FILE"; then
  cat > /tmp/csi_init_code.txt << 'INIT'
  /* Initialize CSI v3 logging from Helm environment variables */
  if (getenv("CSI_LOGGING_ENABLED")) {
    char csi_config[512] = "";
    size_t pos = 0;
    const char *env_val = NULL;
    #define APPEND_CONFIG(key, env_var) do { \
      env_val = getenv(env_var); \
      if (env_val) pos += snprintf(csi_config + pos, sizeof(csi_config) - pos, "%s=%s ", key, env_val); \
    } while(0)
    
    APPEND_CONFIG("granularity", "CSI_GRANULARITY");
    APPEND_CONFIG("antenna_selection", "CSI_ANTENNA_SELECTION");
    APPEND_CONFIG("port_selection", "CSI_PORT_SELECTION");
    APPEND_CONFIG("subcarrier_sampling", "CSI_SUBCARRIER_SAMPLING");
    
    const char *out_dir = getenv("CSI_OUTPUT_DIR");
    nr_csi_logging_init_v3(csi_config, gNB->frame_parms.nb_antennas_rx, 1, out_dir ? out_dir : "/data/csi");
  }
INIT

  awk '/^int main\( int argc, char \*\*argv \) \{/ {
    print
    while((getline line < "/tmp/csi_init_code.txt") > 0) {
      print line
    }
    next
  }
  {print}' "$FILE" > "${FILE}.tmp" && mv "${FILE}.tmp" "$FILE"
  
  echo "✅ Added nr_csi_logging_init_v3() initialization call"
fi

echo "✅ nr-softmodem.c patched for CSI v3"
