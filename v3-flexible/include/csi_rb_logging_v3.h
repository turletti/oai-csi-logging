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
  csi_granularity_t granularity;
  uint8_t antenna_indices[16];
  uint8_t num_antenna_indices;
  uint8_t port_indices[4];
  uint8_t num_port_indices;
  uint8_t subcarrier_sampling;
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
  bool include_header;
  /* v3.1: SRS buffer geometry, written to the JSON header for traceability */
  uint16_t ofdm_symbol_size;
  uint16_t first_carrier_offset;
  uint16_t bwp_start;
  uint16_t bwp_size;
  uint8_t nb_symb_srs;
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
  uint32_t dropped;   /* v3.1: rows dropped because the ring was full (reported then reset at each flush) */
} csi_ring_buffer_v3_t;
/* ============================================================
 * Public API
 * ============================================================ */
int csi_config_parse_v3(const char *config_str, csi_config_v3_t *config);
int csi_config_parse_env_v3(csi_config_v3_t *config);
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
/* v3.1: copy pending rows to dst (capacity CSI_RING_BUFFER_SIZE), mark them consumed; call under the mutex */
uint32_t csi_ring_buffer_snapshot_v3(csi_ring_buffer_v3_t *rb, csi_measurement_v3_t *dst);
/* v3.1: write header (once) + rows to the CSV; only the flush thread (or shutdown) calls it */
int csi_write_rows_v3(csi_ring_buffer_v3_t *rb, const csi_measurement_v3_t *rows, uint32_t n);
void csi_ring_buffer_free_v3(csi_ring_buffer_v3_t *rb);
bool csi_should_log_antenna_v3(const csi_ring_buffer_v3_t *rb, uint8_t ant_rx);
bool csi_should_log_port_v3(const csi_ring_buffer_v3_t *rb, uint8_t port_tx);
bool csi_should_log_subcarrier_v3(const csi_ring_buffer_v3_t *rb, uint8_t sc);

/* v3.1 entry point, called from nr_srs_rx_procedures() after the SRS channel estimation.
 * srs_estimated_channel_freq points to OAI's c16_t [nb_antennas_rx][N_ap][ofdm_symbol_size * N_symb_SRS]
 * array. N_ap is the number of SRS ports (already 1 << num_ant_ports in OAI).
 * The buffer is DC-centred: subcarrier g of the carrier (g = 12*crb + sc) of SRS symbol s is at
 * s*ofdm_symbol_size + (first_carrier_offset - ofdm_symbol_size/2) + g. */
void nr_srs_csi_logging_invoke_v3(uint32_t frame_rx, uint16_t slot_rx, uint16_t rnti,
                                  uint8_t nb_antennas_rx, uint8_t N_ap, uint8_t N_symb_SRS,
                                  uint16_t ofdm_symbol_size, uint16_t first_carrier_offset,
                                  uint16_t bwp_start, uint16_t bwp_size,
                                  const void *srs_estimated_channel_freq);
#endif
