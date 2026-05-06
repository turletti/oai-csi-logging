#include "csi_rb_logging_external.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static csi_ring_buffer_t csi_buffer;
static pthread_t logging_thread;
static int logging_active = 0;
static const char *csi_output_file = "/tmp/csi_per_rb.csv";

static void *csi_logging_thread_func(void *arg) {
  FILE *fp = fopen(csi_output_file, "w");
  if (!fp) {
    fprintf(stderr, "Failed to open %s\n", csi_output_file);
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
  if (output_file)
    csi_output_file = output_file;
  
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
  if (!h_data || num_subcarriers != 12)
    return -1;
  
  csi_rb_measurement_t meas;
  meas.frame = frame;
  meas.slot = slot;
  meas.rb = rb;
  meas.num_subcarriers = num_subcarriers;
  meas.h_per_rb = (c16_t *)h_data;
  
  return csi_ring_buffer_push(&csi_buffer, &meas);
}
