#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <time.h>
#include "csi_rb_logging_v3.h"
#include <stdlib.h>

#ifndef c16_t
typedef struct {
  int16_t i;
  int16_t q;
} c16_t;
#endif

static pthread_t g_csi_flush_thread;
static pthread_t g_csi_timestamp_thread;
static pthread_mutex_t g_csi_flush_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_csi_flush_stop = 0;
static int g_csi_timestamp_stop = 0;

void* csi_flush_thread_func(void *arg) {
  int core_id = 32;
  const char *core_env = getenv("CSI_FLUSH_CORE");
  if (core_env) {
    core_id = atoi(core_env);
  }

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
  fprintf(stderr, "[CSI] Flush thread pinned to core %d\n", core_id);

  csi_ring_buffer_v3_t *rb = (csi_ring_buffer_v3_t *)arg;
  while (!g_csi_flush_stop) {
    usleep(5000000);
    pthread_mutex_lock(&g_csi_flush_mutex);
    if (rb->count > 0) {
      csi_ring_buffer_flush_v3(rb);
    }
    pthread_mutex_unlock(&g_csi_flush_mutex);
  }
  return NULL;
}

void* csi_timestamp_thread_func(void *arg) {
  csi_ring_buffer_v3_t *rb = (csi_ring_buffer_v3_t *)arg;
  
  while (!g_csi_timestamp_stop) {
    sleep(1);
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    pthread_mutex_lock(&g_csi_flush_mutex);
    if (rb->csv_file) {
      fprintf(rb->csv_file, "# TIMESTAMP: %s\n", timestamp);
      fflush(rb->csv_file);
    }
    pthread_mutex_unlock(&g_csi_flush_mutex);
  }
  return NULL;
}

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

  if (csi_config_parse_env_v3(&config) < 0) {
    fprintf(stderr, "ERROR: Failed to parse CSI config\n");
    return -1;
  }

  if (csi_ring_buffer_init_v3(&g_csi_rb, &config, nb_antenna_rx, nb_ports_tx, output_dir) < 0) {
    fprintf(stderr, "ERROR: Failed to initialize CSI ring buffer\n");
    return -1;
  }

  g_csi_initialized = true;
  g_csi_enabled = 1;

  g_csi_flush_stop = 0;
  pthread_create(&g_csi_flush_thread, NULL, csi_flush_thread_func, &g_csi_rb);

  g_csi_timestamp_stop = 0;
  pthread_create(&g_csi_timestamp_thread, NULL, csi_timestamp_thread_func, &g_csi_rb);

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
  static int g_csi_lazy_init_done = 0;
  if (!g_csi_lazy_init_done) {
    const char *out_dir = getenv("CSI_OUTPUT_DIR") ?: "/data/csi";
    nr_csi_logging_init_v3(NULL, nb_antennas_rx, 1, out_dir);
    g_csi_lazy_init_done = 1;
  }

  if (!g_csi_enabled || !g_csi_initialized) {
    return;
  }

  uint8_t nb_ports_tx = (1 << N_ap);

  pthread_mutex_lock(&g_csi_flush_mutex);

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

  pthread_mutex_unlock(&g_csi_flush_mutex);
}

void nr_csi_logging_enable_v3(int enable) {
  g_csi_enabled = enable;
  printf("[CSI] Logging %s\n", enable ? "enabled" : "disabled");
}

void nr_csi_logging_shutdown_v3(void) {
  if (g_csi_initialized) {
    g_csi_flush_stop = 1;
    g_csi_timestamp_stop = 1;
    pthread_join(g_csi_flush_thread, NULL);
    pthread_join(g_csi_timestamp_thread, NULL);
    pthread_mutex_lock(&g_csi_flush_mutex);
    csi_ring_buffer_flush_v3(&g_csi_rb);
    csi_ring_buffer_free_v3(&g_csi_rb);
    pthread_mutex_unlock(&g_csi_flush_mutex);
    g_csi_initialized = false;
  }
}
