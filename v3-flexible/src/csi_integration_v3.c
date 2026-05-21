/*
 * CSI Integration v3
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "csi_rb_logging_v3.h"

// c16_t struct from OAI (will be included via OAI headers at link time)
// For standalone compilation, define it locally
#ifndef c16_t
typedef struct {
  int16_t i;  // Real (I)
  int16_t q;  // Imaginary (Q)
} c16_t;
#endif

static csi_ring_buffer_v3_t g_csi_rb = {0};
static bool g_csi_initialized = false;
static int g_csi_enabled = 0;

int nr_csi_logging_init_v3(const char *config_str,
                            uint8_t nb_antenna_rx,
                            uint8_t nb_ports_tx,
                            const char *output_dir) {
  if (g_csi_initialized) {
    fprintf(stderr, "WARNING: CSI already initialized\n");
    return 0;
  }
  
  csi_config_v3_t config = {0};
  
  if (csi_config_parse_v3(config_str, &config) < 0) {
    fprintf(stderr, "ERROR: Failed to parse CSI config\n");
    return -1;
  }
  
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

void nr_srs_csi_logging_invoke_v3(uint32_t frame_rx,
                                   uint16_t slot_rx,
                                   uint16_t rnti,
                                   uint8_t nb_antennas_rx,
                                   uint8_t N_ap,
                                   uint8_t N_symb_SRS,
                                   uint16_t ofdm_symbol_size,
                                   uint16_t bwp_start,
                                   uint16_t bwp_size,
                                   const c16_t srs_estimated_channel_freq[][N_ap][ofdm_symbol_size * N_symb_SRS]) {
  static unsigned int call_count = 0;
  if (++call_count % 100 == 0 && g_csi_initialized) {
    csi_ring_buffer_flush_v3(&g_csi_rb);
  }
  static int g_csi_lazy_init_done = 0;
  if (!g_csi_lazy_init_done) {
    const char *config_str = getenv("CSI_CONFIG") ?: "";
    const char *out_dir = getenv("CSI_OUTPUT_DIR") ?: "/data/csi";
    nr_csi_logging_init_v3(config_str, nb_antennas_rx, 1, out_dir);
    g_csi_lazy_init_done = 1;
  if (g_csi_enabled) fprintf(stderr, "[CSI-DEBUG] invoke: frame=%u slot=%u rnti=%u nb_ant=%u enabled=%d initialized=%d\n", frame_rx, slot_rx, rnti, nb_antennas_rx, g_csi_enabled, g_csi_initialized);
  fprintf(stderr, "[CSI-DEBUG] bwp: start=%u size=%u N_ap=%u N_symb=%u ofdm_size=%u\n", bwp_start, bwp_size, N_ap, N_symb_SRS, ofdm_symbol_size);
  if (!g_csi_enabled || !g_csi_initialized) {
    return;
  }
  
  uint8_t nb_ports_tx = (1 << N_ap);
  
  for (uint8_t ant_rx = 0; ant_rx < nb_antennas_rx; ant_rx++) {
    if (!csi_should_log_antenna_v3(&g_csi_rb, ant_rx)) {
      continue;
    }
    
    for (uint8_t port_tx = 0; port_tx < nb_ports_tx; port_tx++) {
      if (!csi_should_log_port_v3(&g_csi_rb, port_tx)) {
        continue;
      }
      
      for (uint16_t rb = bwp_start; rb < bwp_start + bwp_size; rb++) {
        
        if (g_csi_rb.metadata.granularity == CSI_GRAN_RB) {
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
                                   rb, 0,
                                   avg_real, avg_imag);
        }
        else {
          for (uint8_t sc = 0; sc < 12; sc++) {
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

void nr_csi_logging_enable_v3(int enable) {
  g_csi_enabled = enable;
  printf("[CSI] Logging %s\n", enable ? "enabled" : "disabled");
}

void nr_csi_logging_shutdown_v3(void) {
  if (g_csi_initialized) {
    csi_ring_buffer_flush_v3(&g_csi_rb);
    csi_ring_buffer_free_v3(&g_csi_rb);
    g_csi_initialized = false;
  }
}
