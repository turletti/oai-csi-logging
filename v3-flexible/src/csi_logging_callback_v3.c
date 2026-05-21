/*
 * CSI Ring Buffer & CSV Writer v3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <json-c/json.h>
#include "csi_rb_logging_v3.h"

/* Initialize ring buffer */
int csi_ring_buffer_init_v3(csi_ring_buffer_v3_t *rb,
                             const csi_config_v3_t *config,
                             uint8_t nb_antenna_rx,
                             uint8_t nb_ports_tx,
                             const char *output_dir) {
  if (!rb || !config) return -1;
  
  // Allocate buffer
  rb->buffer = (csi_measurement_v3_t *)malloc(CSI_RING_BUFFER_SIZE * sizeof(csi_measurement_v3_t));
  if (!rb->buffer) {
    fprintf(stderr, "ERROR: malloc failed for CSI ring buffer\n");
    return -1;
  }
  
  rb->write_idx = 0;
  rb->read_idx = 0;
  rb->count = 0;
  rb->header_written = false;
  
  // Setup metadata
  rb->metadata.granularity = config->granularity;
  rb->metadata.nb_antenna_rx = nb_antenna_rx;
  fprintf(stderr, "[CSI-DEBUG] ring_buffer_init: nb_antenna_rx set to %u\n", nb_antenna_rx);
  rb->metadata.nb_ports_tx = nb_ports_tx;
  rb->metadata.num_antenna_indices = config->num_antenna_indices;
  rb->metadata.num_port_indices = config->num_port_indices;
  rb->metadata.subcarrier_sampling = config->subcarrier_sampling;
  rb->metadata.include_header = config->include_header;
  
  memcpy(rb->metadata.antenna_indices, config->antenna_indices, sizeof(config->antenna_indices));
  memcpy(rb->metadata.port_indices, config->port_indices, sizeof(config->port_indices));
  
  // Create output directory
  mkdir(output_dir, 0755);
  
  // Open CSV file
  char csv_path[512];
  snprintf(csv_path, sizeof(csv_path), "%s/csi_per_rb.csv", output_dir);
  
  rb->csv_file = fopen(csv_path, "w");
  rb->header_written = false;
  if (!rb->csv_file) {
    fprintf(stderr, "ERROR: Cannot open %s\n", csv_path);
    free(rb->buffer);
    return -1;
  }
  
  // Write JSON metadata header if enabled
//   if (config->include_header) {
//     json_object *metadata = json_object_new_object();
//     
//     json_object_object_add(metadata, "granularity",
//       json_object_new_string(config->granularity == CSI_GRAN_RB ? "rb" : "subcarrier"));
//     json_object_object_add(metadata, "nb_antenna_rx",
//       json_object_new_int(nb_antenna_rx));
//     json_object_object_add(metadata, "nb_ports_tx",
//       json_object_new_int(nb_ports_tx));
//     
//     json_object *ant_arr = json_object_new_array();
//     if (config->num_antenna_indices == 0) {
//       for (int i = 0; i < nb_antenna_rx; i++) {
//         json_object_array_add(ant_arr, json_object_new_int(i));
//       }
//     } else {
//       for (int i = 0; i < config->num_antenna_indices; i++) {
//         json_object_array_add(ant_arr, json_object_new_int(config->antenna_indices[i]));
//       }
//     }
//     json_object_object_add(metadata, "antenna_selection", ant_arr);
//     
//     json_object *port_arr = json_object_new_array();
//     if (config->num_port_indices == 0) {
//       for (int i = 0; i < nb_ports_tx; i++) {
//         json_object_array_add(port_arr, json_object_new_int(i));
//       }
//     } else {
//       for (int i = 0; i < config->num_port_indices; i++) {
//         json_object_array_add(port_arr, json_object_new_int(config->port_indices[i]));
//       }
//     }
//     json_object_object_add(metadata, "port_selection", port_arr);
//     json_object_object_add(metadata, "subcarrier_sampling",
//       json_object_new_int(config->subcarrier_sampling));
//     
//     fprintf(rb->csv_file, "# %s\n", json_object_to_json_string(metadata));
//     json_object_put(metadata);
//  }
  
  // Write CSV header based on granularity and MIMO config
  fprintf(rb->csv_file, "frame,slot,rnti");
  
  // Add MIMO columns if needed
  if (nb_antenna_rx > 1 || nb_ports_tx > 1) {
    fprintf(rb->csv_file, ",ant_rx,port_tx");
  }
  
  fprintf(rb->csv_file, ",rb");
  
  if (config->granularity == CSI_GRAN_SUBCARRIER) {
    fprintf(rb->csv_file, ",subcarrier");
  }
  
  fprintf(rb->csv_file, ",real,imag\n");
  
  fflush(rb->csv_file);
  rb->header_written = true;
  
  return 0;
}

/* Push measurement to ring buffer */
int csi_push_measurement_v3(csi_ring_buffer_v3_t *rb,
                             uint32_t frame, uint16_t slot, uint16_t rnti,
                             uint8_t ant_rx, uint8_t port_tx,
                             uint16_t rb_idx, uint8_t subcarrier_idx,
                             int16_t real, int16_t imag) {
  if (!rb || !rb->buffer) return -1;
  
  // Check if should be logged (based on antenna/port/subcarrier selection)
  if (!csi_should_log_antenna_v3(rb, ant_rx)) return 0;
  if (!csi_should_log_port_v3(rb, port_tx)) return 0;
  if (rb->metadata.granularity == CSI_GRAN_SUBCARRIER && 
      !csi_should_log_subcarrier_v3(rb, subcarrier_idx)) return 0;
  
  // Add to ring buffer
  uint32_t idx = rb->write_idx % CSI_RING_BUFFER_SIZE;
  
  rb->buffer[idx].frame = frame;
  rb->buffer[idx].slot = slot;
  rb->buffer[idx].rnti = rnti;
  rb->buffer[idx].ant_rx = ant_rx;
  rb->buffer[idx].port_tx = port_tx;
  rb->buffer[idx].rb = rb_idx;
  rb->buffer[idx].subcarrier = subcarrier_idx;
  rb->buffer[idx].real = real;
  rb->buffer[idx].imag = imag;
  
  rb->write_idx++;
  rb->count++;
  
  // Auto-flush if buffer getting full
  if (rb->count > CSI_RING_BUFFER_SIZE * 0.9) {
    csi_ring_buffer_flush_v3(rb);
  }
  
  return 0;
}

/* Flush ring buffer to CSV */
int csi_ring_buffer_flush_v3(csi_ring_buffer_v3_t *rb) {
  if (!rb || !rb->csv_file) return -1;
  // Write header on first flush with actual nb_antenna_rx
  if (!rb->header_written && rb->metadata.include_header) {
    json_object *metadata = json_object_new_object();
    json_object_object_add(metadata, "granularity",
      json_object_new_string(rb->metadata.granularity == CSI_GRAN_RB ? "rb" : "subcarrier"));
    json_object_object_add(metadata, "nb_antenna_rx",
      json_object_new_int(rb->metadata.nb_antenna_rx));
    json_object_object_add(metadata, "nb_ports_tx",
      json_object_new_int(rb->metadata.nb_ports_tx));
    json_object *ant_arr = json_object_new_array();
    if (rb->metadata.num_antenna_indices == 0) {
      for (int i = 0; i < rb->metadata.nb_antenna_rx; i++) {
        json_object_array_add(ant_arr, json_object_new_int(i));
      }
    } else {
      for (int i = 0; i < rb->metadata.num_antenna_indices; i++) {
        json_object_array_add(ant_arr, json_object_new_int(rb->metadata.antenna_indices[i]));
      }
    }
    json_object_object_add(metadata, "antenna_selection", ant_arr);
    json_object *port_arr = json_object_new_array();
    if (rb->metadata.num_port_indices == 0) {
      for (int i = 0; i < rb->metadata.nb_ports_tx; i++) {
        json_object_array_add(port_arr, json_object_new_int(i));
      }
    } else {
      for (int i = 0; i < rb->metadata.num_port_indices; i++) {
        json_object_array_add(port_arr, json_object_new_int(rb->metadata.port_indices[i]));
      }
    }
    json_object_object_add(metadata, "port_selection", port_arr);
    json_object_object_add(metadata, "subcarrier_sampling", json_object_new_int(rb->metadata.subcarrier_sampling));
    fprintf(rb->csv_file, "# %s\n", json_object_to_json_string(metadata));
    fprintf(rb->csv_file, "frame,slot,rnti,ant_rx,port_tx,rb,real,imag\n");
    json_object_put(metadata);
    rb->header_written = true;
  }
  
  while (rb->read_idx < rb->write_idx) {
    uint32_t idx = rb->read_idx % CSI_RING_BUFFER_SIZE;
    csi_measurement_v3_t *m = &rb->buffer[idx];
    
    fprintf(rb->csv_file, "%u,%u,0x%04x", m->frame, m->slot, m->rnti);
    
    // MIMO columns
    if (rb->metadata.nb_antenna_rx > 1 || rb->metadata.nb_ports_tx > 1) {
      fprintf(rb->csv_file, ",%u,%u", m->ant_rx, m->port_tx);
    }
    
    fprintf(rb->csv_file, ",%u", m->rb);
    
    if (rb->metadata.granularity == CSI_GRAN_SUBCARRIER) {
      fprintf(rb->csv_file, ",%u", m->subcarrier);
    }
    
    fprintf(rb->csv_file, ",%d,%d\n", m->real, m->imag);
    
    rb->read_idx++;
  }
  
  rb->count = 0;
  fflush(rb->csv_file);
  return 0;
}

/* Cleanup */
void csi_ring_buffer_free_v3(csi_ring_buffer_v3_t *rb) {
  if (!rb) return;
  
  if (rb->csv_file) {
    csi_ring_buffer_flush_v3(rb);
    fclose(rb->csv_file);
  }
  
  if (rb->buffer) {
    free(rb->buffer);
  }
  
  memset(rb, 0, sizeof(*rb));
}
