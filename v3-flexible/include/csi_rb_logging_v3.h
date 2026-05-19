/*
 * CSI Logging v3 - Flexible MIMO Support
 * 
 * Supports:
 * - Granularity: per-RB or per-subcarrier
 * - MIMO: SISO (1x1) to 4x4
 * - Antenna selection: all or specific indices
 * - Port selection: all or specific indices
 * - Subcarrier sampling: configurable (1/12, 1/6, 1/4, 1/2, 1)
 */

#ifndef CSI_RB_LOGGING_V3_H
#define CSI_RB_LOGGING_V3_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * Configuration Structure
 * ============================================================ */

typedef struct {
  // Granularity
  enum { CSI_GRAN_RB = 0, CSI_GRAN_SUBCARRIER = 1 } granularity;
  
  // Antenna selection: "all" or "0,1,2,3"
  uint8_t antenna_indices[16];  // Max 16 RX antennas
  uint8_t num_antenna_indices;
  
  // Port selection: "all" or "0,1"
  uint8_t port_indices[4];      // Max 4 TX ports
  uint8_t num_port_indices;
  
  // Subcarrier sampling (only for GRAN_SUBCARRIER)
  // 1 = all, 2 = every 2nd, 3 = every 3rd, 4 = every 4th, 6 = every 6th, 12 = every 12th
  uint8_t subcarrier_sampling;
  
  // Output
  char output_dir[256];
  bool include_header;  // Write JSON metadata
} csi_config_v3_t;

/* ============================================================
 * Measurement Structure (per measurement)
 * ============================================================ */

typedef struct {
  uint32_t frame;
  uint16_t slot;
  uint16_t rnti;
  
  // MIMO indices (optional, depends on config)
  uint8_t ant_rx;     // 0-15 (only if MIMO)
  uint8_t port_tx;    // 0-3  (only if MIMO)
  
  // RB and subcarrier
  uint16_t rb;
  uint8_t subcarrier;  // 0-11 (only if GRAN_SUBCARRIER)
  
  // I/Q values
  int16_t real;
  int16_t imag;
  
} csi_measurement_v3_t;

/* ============================================================
 * Metadata (written once at CSV start)
 * ============================================================ */

typedef struct {
  enum { CSI_GRAN_RB = 0, CSI_GRAN_SUBCARRIER = 1 } granularity;
  uint8_t nb_antenna_rx;     // Total RX antennas in gNB
  uint8_t nb_ports_tx;       // Total TX ports from UE
  uint8_t num_antenna_indices;  // How many selected
  uint8_t antenna_indices[16];
  uint8_t num_port_indices;
  uint8_t port_indices[4];
  uint8_t subcarrier_sampling;
} csi_csv_metadata_v3_t;

/* ============================================================
 * Ring Buffer & Logging
 * ============================================================ */

#define CSI_RING_BUFFER_SIZE 1000000  // Adjust based on memory

typedef struct {
  csi_measurement_v3_t *buffer;
  uint32_t write_idx;
  uint32_t read_idx;
  uint32_t count;
  
  // Metadata (constant for session)
  csi_csv_metadata_v3_t metadata;
  
  // File handle
  FILE *csv_file;
  bool header_written;
} csi_ring_buffer_v3_t;

/* ============================================================
 * Public API
 * ============================================================ */

// Initialize config from string: "antenna_selection=0,1,2,3 port_selection=0,1 ..."
int csi_config_parse_v3(const char *config_str, csi_config_v3_t *config);

// Initialize ring buffer with config
int csi_ring_buffer_init_v3(csi_ring_buffer_v3_t *rb, 
                             const csi_config_v3_t *config,
                             uint8_t nb_antenna_rx,
                             uint8_t nb_ports_tx,
                             const char *output_dir);

// Log a measurement
int csi_push_measurement_v3(csi_ring_buffer_v3_t *rb,
                             uint32_t frame, uint16_t slot, uint16_t rnti,
                             uint8_t ant_rx, uint8_t port_tx,
                             uint16_t rb_idx, uint8_t subcarrier_idx,
                             int16_t real, int16_t imag);

// Flush to CSV
int csi_ring_buffer_flush_v3(csi_ring_buffer_v3_t *rb);

// Cleanup
void csi_ring_buffer_free_v3(csi_ring_buffer_v3_t *rb);

// Check if antenna/port should be logged (based on selection)
bool csi_should_log_antenna_v3(const csi_ring_buffer_v3_t *rb, uint8_t ant_rx);
bool csi_should_log_port_v3(const csi_ring_buffer_v3_t *rb, uint8_t port_tx);
bool csi_should_log_subcarrier_v3(const csi_ring_buffer_v3_t *rb, uint8_t sc);

#endif  // CSI_RB_LOGGING_V3_H
