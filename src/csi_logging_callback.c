/* src/csi_logging_callback.c */
#include <stddef.h>
#include "csi_rb_logging_external.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
int csi_rb_logging_enabled = 0;
void (*csi_rb_logging_callback)(const void *, const void *, const void *, const void *) = NULL;
static csi_ring_buffer_t csi_buffer;
static pthread_t logging_thread;
static int logging_active = 0;
static char csi_output_file_buf[512] = "/tmp/csi_per_rb.csv";
static int csi_init_done = 0;
int csi_logging_init_from_env(void) {
  if (csi_init_done) {
    fprintf(stderr, "[CSI] Already initialized\n");
    return 0;
  }
  const char *enabled = getenv("CSI_LOGGING_ENABLED");
  const char *output_dir = getenv("CSI_OUTPUT_DIR");
  fprintf(stderr, "[CSI] Environment check: CSI_LOGGING_ENABLED=%s\n",
          enabled ? enabled : "NOT SET");
  if (!enabled || atoi(enabled) != 1) {
    fprintf(stderr, "[CSI] CSI logging disabled\n");
    return 0;
  }
  fprintf(stderr, "[CSI] Initializing CSI logging...\n");
  char filepath[512];
  if (output_dir) {
    snprintf(filepath, sizeof(filepath), "%s/csi_per_rb.csv", output_dir);
  } else {
    snprintf(filepath, sizeof(filepath), "/tmp/csi_per_rb.csv");
  }
  fprintf(stderr, "[CSI] Output file: %s\n", filepath);
  if (csi_logging_init(filepath) < 0) {
    fprintf(stderr, "[CSI] ERROR: csi_logging_init failed\n");
    return -1;
  }
  extern void csi_rb_logging_callback_impl(const void *, const void *, const void *, const void *);
  csi_rb_logging_callback = csi_rb_logging_callback_impl;
  csi_rb_logging_enabled = 1;
  csi_init_done = 1;
  fprintf(stderr, "[CSI] Initialization complete\n");
  return 0;
}
static void *csi_logging_thread_func(void *arg) {
  FILE *fp = fopen(csi_output_file_buf, "w");
  if (!fp) {
    fprintf(stderr, "Failed to open %s\n", csi_output_file_buf);
    return NULL;
  }
  fprintf(fp, "frame,slot,rb,subcarrier,real,imag\n");
  fflush(fp);
  csi_rb_measurement_t meas;
  while (logging_active) {
    if (csi_ring_buffer_pop(&csi_buffer, &meas) == 0) {
      for (int sc = 0; sc < meas.num_subcarriers; sc++) {
        fprintf(fp, "%u,%u,%u,%d,%d,%d\n",
                meas.frame, meas.slot, meas.rb, sc,
                meas.h_per_rb_r[sc], meas.h_per_rb_i[sc]);
      }
      fflush(fp);
    } else {
      usleep(1000);
    }
  }
  fclose(fp);
  return NULL;
}
int csi_logging_init(const char *output_file) {
  static int _initialized = 0;
  if (_initialized) return 0;
  _initialized = 1;
  if (output_file) {
    snprintf(csi_output_file_buf, sizeof(csi_output_file_buf), "%s", output_file);
  }
  if (csi_ring_buffer_init(&csi_buffer, 1000) < 0)
    return -1;
  logging_active = 1;
  // THREAD NOT CREATED HERE - deferred to csi_logging_start_thread()
  return 0;
}
int csi_logging_start_thread(void) {
  if (logging_thread) return 0;
  if (pthread_create(&logging_thread, NULL, csi_logging_thread_func, NULL) < 0) {
    logging_active = 0;
    return -1;
  }
  return 0;
}
void csi_logging_cleanup(void) {
  logging_active = 0;
  if (logging_thread)
    pthread_join(logging_thread, NULL);
  csi_ring_buffer_free(&csi_buffer);
}
int csi_logging_push_measurement(uint32_t frame, uint32_t slot, uint32_t rb,
                                  const void *h_data, uint32_t num_subcarriers) {
  if (!h_data || num_subcarriers > 12)
    return -1;
  csi_rb_measurement_t meas;
  meas.frame = frame;
  meas.slot = slot;
  meas.rb = rb;
  meas.num_subcarriers = num_subcarriers;
  const int16_t *p = (const int16_t *)h_data;
  for (uint32_t i = 0; i < num_subcarriers; i++) {
    meas.h_per_rb_r[i] = p[2*i];
    meas.h_per_rb_i[i] = p[2*i+1];
  }
  return csi_ring_buffer_push(&csi_buffer, &meas);
}
