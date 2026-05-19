/*
 * CSI Config Parser v3
 * Parse configuration strings like:
 *   "granularity=rb antenna_selection=0,1,2,3 port_selection=0,1 subcarrier_sampling=1"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csi_rb_logging_v3.h"

#define MAX_CONFIG_TOKENS 32

/* Helper: parse comma-separated indices */
static int parse_indices(const char *str, uint8_t *indices, uint8_t max_count) {
  if (!str || !indices) return 0;
  
  // Handle "all"
  if (strcmp(str, "all") == 0) {
    return 0;  // Special marker: 0 means "all"
  }
  
  int count = 0;
  char *copy = strdup(str);
  char *token = strtok(copy, ",");
  
  while (token && count < max_count) {
    indices[count] = (uint8_t)atoi(token);
    count++;
    token = strtok(NULL, ",");
  }
  
  free(copy);
  return count;
}

/* Parse config string */
int csi_config_parse_v3(const char *config_str, csi_config_v3_t *config) {
  if (!config) return -1;
  
  // Defaults
  config->granularity = CSI_GRAN_RB;
  config->num_antenna_indices = 0;  // 0 = all
  config->num_port_indices = 0;     // 0 = all
  config->subcarrier_sampling = 1;
  config->include_header = true;
  strcpy(config->output_dir, "/data/csi");
  
  if (!config_str) return 0;  // Use defaults
  
  char *copy = strdup(config_str);
  char *saveptr = NULL;
  char *token = strtok_r(copy, " ", &saveptr);
  
  while (token) {
    char *eq = strchr(token, '=');
    if (!eq) {
      token = strtok_r(NULL, " ", &saveptr);
      continue;
    }
    
    *eq = '\0';
    char *key = token;
    char *value = eq + 1;
    
    if (strcmp(key, "granularity") == 0) {
      if (strcmp(value, "subcarrier") == 0) {
        config->granularity = CSI_GRAN_SUBCARRIER;
      } else {
        config->granularity = CSI_GRAN_RB;
      }
    }
    else if (strcmp(key, "antenna_selection") == 0) {
      config->num_antenna_indices = parse_indices(value, config->antenna_indices, 16);
    }
    else if (strcmp(key, "port_selection") == 0) {
      config->num_port_indices = parse_indices(value, config->port_indices, 4);
    }
    else if (strcmp(key, "subcarrier_sampling") == 0) {
      config->subcarrier_sampling = (uint8_t)atoi(value);
      if (config->subcarrier_sampling < 1 || config->subcarrier_sampling > 12) {
        fprintf(stderr, "ERROR: subcarrier_sampling must be 1-12\n");
        config->subcarrier_sampling = 1;
      }
    }
    else if (strcmp(key, "output_dir") == 0) {
      strncpy(config->output_dir, value, 255);
    }
    else if (strcmp(key, "include_header") == 0) {
      config->include_header = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
    }
    
    token = strtok_r(NULL, " ", &saveptr);
  }
  
  free(copy);
  return 0;
}

/* Check if antenna should be logged */
bool csi_should_log_antenna_v3(const csi_ring_buffer_v3_t *rb, uint8_t ant_rx) {
  if (!rb) return false;
  
  // If num_antenna_indices == 0, means "all"
  if (rb->metadata.num_antenna_indices == 0) {
    return ant_rx < rb->metadata.nb_antenna_rx;
  }
  
  // Check if in selection list
  for (int i = 0; i < rb->metadata.num_antenna_indices; i++) {
    if (rb->metadata.antenna_indices[i] == ant_rx) {
      return true;
    }
  }
  return false;
}

/* Check if port should be logged */
bool csi_should_log_port_v3(const csi_ring_buffer_v3_t *rb, uint8_t port_tx) {
  if (!rb) return false;
  
  if (rb->metadata.num_port_indices == 0) {
    return port_tx < rb->metadata.nb_ports_tx;
  }
  
  for (int i = 0; i < rb->metadata.num_port_indices; i++) {
    if (rb->metadata.port_indices[i] == port_tx) {
      return true;
    }
  }
  return false;
}

/* Check if subcarrier should be logged (based on sampling) */
bool csi_should_log_subcarrier_v3(const csi_ring_buffer_v3_t *rb, uint8_t sc) {
  if (!rb || sc >= 12) return false;
  
  // sampling=1: all, sampling=2: 0,2,4,6,8,10, sampling=3: 0,3,6,9, etc.
  return (sc % rb->metadata.subcarrier_sampling) == 0;
}
