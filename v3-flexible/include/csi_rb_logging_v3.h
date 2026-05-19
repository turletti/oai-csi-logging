/*
 * CSI Logging v3 - Flexible MIMO Support
 */

#ifndef CSI_RB_LOGGING_V3_H
#define CSI_RB_LOGGING_V3_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/* ============================================================
 * Granularity Enum (DEFINED ONCE)
 * ============================================================ */

typedef enum {
  CSI_GRAN_RB = 0,
  CSI_GRAN_SUBCARRIER = 1
} csi_granularity_t;

/* ============================================================
 * Configuration Structure
 * ============================================================ */

typedef struct {
  // Granularity
  csi_granularity_t granularity;
  
  // Antenna selection: "all" or "0,1,2,3"
  uint8_t antenna_indices[16];
  uint8_t num_antenna_indices;
  
  // Port selection: "all" or "0,1"
  uint8_t port_indices[4];
  uint8_t num_port_indices;
  
  // Subcarrier sampling
  uint8_t subcarrier_sampling;
  
  // Output
  char output_dir[256];
  bool include_header;
} csi_config_v3_t;

/* ============================================================
 * Measurement Structure
 * ============================================================ */

typedef struct {
  uint32_t frame;
  uint16_t slot;
  uint16_t rnti;
  uint8_t ant_rx;
  uint8_t port_tx;
  uint16_t rb;
  uint8_t subcarrier;
  int16_t real;
  int16_t imag;
} csi_measurement_v3_t;

/* ============================================================
 * Metadata
 * ============================================================ */

typedef struct {
  csi_granularity_t granularity;
  uint8_t nb_antenna_rx;
  uint8_t nb_ports_tx;
  uint8_t num_antenna_indices;
  uint8_t antenna_indices[16];
  uint8_t num_port_indices;
  uint8_t port_indices[4];
  uint8_t subcarrier_sampling;
} csi_csv_metadata_v3_t;

/* ============================================================
 * Ring Buffer
 * ============================================================ */

#define CSI_RING_BUFFER_SIZE 1000000

typedef struct {
  csi_measurement_v3_t *buffer;
  uint32_t write_idx;
  uint32_t read_idx;
  uint32_t count;
  csi_csv_metadata_v3_t metadata;
  FILE *csv_file;
  bool header_written;
} csi_ring_buffer_v3_t;

/* ============================================================
 * Public API
 * ============================================================ */

int csi_config_parse_v3(const char *config_str, csi_config_v3_t *config);

int csi_ring_buffer_init_v3(csi_ring_buffer_v3_t *rb,
                             const csi_config_v3_t *config,
                             uint8_t nb_antenna_rx,
                             uint8_t nb_ports_tx,
                             const char *output_dir);

int csi_push_measurement_v3(csi_ring_buffer_v3_t *rb,
                             uint32_t frame, uint16_t slot, uint16_t rnti,
                             uint8_t ant_rx, uint8_t port_tx,
                             uint16_t rb_idx, uint8_t subcarrier_idx,
                             int16_t real, int16_t imag);

int csi_ring_buffer_flush_v3(csi_ring_buffer_v3_t *rb);

void csi_ring_buffer_free_v3(csi_ring_buffer_v3_t *rb);

bool csi_should_log_antenna_v3(const csi_ring_buffer_v3_t *rb, uint8_t ant_rx);
bool csi_should_log_port_v3(const csi_ring_buffer_v3_t *rb, uint8_t port_tx);
bool csi_should_log_subcarrier_v3(const csi_ring_buffer_v3_t *rb, uint8_t sc);

#endif
