/*
 * CSI Integration v3
 * Called from nr_srs_rx_procedures in phy_procedures_nr_gNB.c
 * 
 * Handles:
 * - Iterating over selected RX antennas and TX ports
 * - Extracting I/Q per RB or per subcarrier
 * - Pushing to ring buffer with filtering
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "csi_rb_logging_v3.h"

// Global ring buffer (initialized once)
static csi_ring_buffer_v3_t g_csi_rb = {0};
static bool g_csi_initialized = false;
static int g_csi_enabled = 0;

/* Called once at gNB startup (from main or init) */
int nr_csi_logging_init_v3(const char *config_str,
                            uint8_t nb_antenna_rx,
                            uint8_t nb_ports_tx,
                            const char *output_dir) {
  if (g_csi_initialized) {
    fprintf(stderr, "WARNING: CSI already initialized\n");
    return 0;
  }
  
  csi_config_v3_t config = {0};
  
  // Parse config from string
  if (csi_config_parse_v3(config_str, &config) < 0) {
    fprintf(stderr, "ERROR: Failed to parse CSI config\n");
    return -1;
  }
  
  // Initialize ring buffer
  if (csi_ring_buffer_init_v3(&g_csi_rb, &config, nb_antenna_rx, nb_ports_tx, output_dir) < 0) {
    fprintf(stderr, "ERROR: Failed to initialize CSI ring buffer\n");
    return -1;
  }
  
  g_csi_initialized = true;
  g_csi_enabled = 1;
  
  printf("[CSI] Initialized v3 logging\n");
  printf("  Granularity: %s\n", config.granularity == CSI_GRAN_RB ? "RB" : "Subcarrier");
  printf("  RX antennas: %u\n", nb_antenna_rx);
  printf("  TX ports: %u\n", nb_ports_tx);
  printf("  Output: %s\n", output_dir);
  
  return 0;
}

/* Called from nr_srs_rx_procedures() */
void nr_srs_csi_logging_invoke_v3(uint32_t frame_rx,
                                   uint16_t slot_rx,
                                   uint16_t rnti,
                                   uint8_t nb_antennas_rx,
                                   uint8_t N_ap,              // num_ant_ports (0=1, 1=2, 2=4)
                                   uint8_t N_symb_SRS,        // num_symbols
                                   uint16_t ofdm_symbol_size,
                                   uint16_t bwp_start,
                                   uint16_t bwp_size,
                                   const c16_t srs_estimated_channel_freq[][N_ap][ofdm_symbol_size * N_symb_SRS]) {
  if (!g_csi_enabled || !g_csi_initialized) {
    return;
  }
  
  // Determine actual number of TX ports from encoding
  // 0 = 1 port, 1 = 2 ports, 2 = 4 ports
  uint8_t nb_ports_tx = (1 << N_ap);
  
  // Iterate over selected RX antennas
  for (uint8_t ant_rx = 0; ant_rx < nb_antennas_rx; ant_rx++) {
    if (!csi_should_log_antenna_v3(&g_csi_rb, ant_rx)) {
      continue;
    }
    
    // Iterate over selected TX ports
    for (uint8_t port_tx = 0; port_tx < nb_ports_tx; port_tx++) {
      if (!csi_should_log_port_v3(&g_csi_rb, port_tx)) {
        continue;
      }
      
      // Iterate over RBs
      for (uint16_t rb = bwp_start; rb < bwp_start + bwp_size; rb++) {
        
        // Per-RB mode
        if (g_csi_rb.metadata.granularity == CSI_GRAN_RB) {
          // Average I/Q over 12 subcarriers
          int32_t sum_real = 0;
          int32_t sum_imag = 0;
          
          for (int sc = 0; sc < 12; sc++) {
            uint32_t freq_idx = (rb * 12 + sc) * N_symb_SRS;
            if (freq_idx + N_symb_SRS <= ofdm_symbol_size * N_symb_SRS) {
              c16_t val = srs_estimated_channel_freq[ant_rx][port_tx][freq_idx];
              sum_real += val.i;
              sum_imag += val.q;
            }
          }
          
          int16_t avg_real = (int16_t)(sum_real / 12);
          int16_t avg_imag = (int16_t)(sum_imag / 12);
          
          csi_push_measurement_v3(&g_csi_rb,
                                   frame_rx, slot_rx, rnti,
                                   ant_rx, port_tx,
                                   rb, 0,  // subcarrier not used in RB mode
                                   avg_real, avg_imag);
        }
        
        // Per-subcarrier mode
        else {
          for (uint8_t sc = 0; sc < 12; sc++) {
            // Check subcarrier sampling
            if (!csi_should_log_subcarrier_v3(&g_csi_rb, sc)) {
              continue;
            }
            
            uint32_t freq_idx = (rb * 12 + sc) * N_symb_SRS;
            if (freq_idx + N_symb_SRS <= ofdm_symbol_size * N_symb_SRS) {
              c16_t val = srs_estimated_channel_freq[ant_rx][port_tx][freq_idx];
              
              csi_push_measurement_v3(&g_csi_rb,
                                       frame_rx, slot_rx, rnti,
                                       ant_rx, port_tx,
                                       rb, sc,
                                       val.i, val.q);
            }
          }
        }
      }
    }
  }
}

/* Disable/enable logging */
void nr_csi_logging_enable_v3(int enable) {
  g_csi_enabled = enable;
  printf("[CSI] Logging %s\n", enable ? "enabled" : "disabled");
}

/* Cleanup (call at shutdown) */
void nr_csi_logging_shutdown_v3(void) {
  if (g_csi_initialized) {
    csi_ring_buffer_flush_v3(&g_csi_rb);
    csi_ring_buffer_free_v3(&g_csi_rb);
    g_csi_initialized = false;
  }
}
