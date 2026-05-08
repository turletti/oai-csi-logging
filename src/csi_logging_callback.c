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

__attribute__((constructor))
static void csi_logging_auto_init(void) {
  const char *enabled = getenv("CSI_LOGGING_ENABLED");
  const char *output_dir = getenv("CSI_OUTPUT_DIR");
  
  if (!enabled || atoi(enabled) != 1)
    return;
  
  char filepath[512];
  if (output_dir) {
    snprintf(filepath, sizeof(filepath), "%s/csi_per_rb.csv", output_dir);
  } else {
    snprintf(filepath, sizeof(filepath), "/tmp/csi_per_rb.csv");
  }
  
  csi_logging_init(filepath);
  
  extern void csi_rb_logging_callback_impl(const void *, const void *, const void *, const void *);
  csi_rb_logging_callback = csi_rb_logging_callback_impl;
  csi_rb_logging_enabled = 1;
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
                meas.h_per_rb[sc].r, meas.h_per_rb[sc].i);
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
  memcpy(meas.h_per_rb, h_data, num_subcarriers * sizeof(c16_t));

  return csi_ring_buffer_push(&csi_buffer, &meas);
}
