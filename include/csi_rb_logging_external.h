/* include/csi_rb_logging_external.h */
#ifndef CSI_RB_LOGGING_EXTERNAL_H
#define CSI_RB_LOGGING_EXTERNAL_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
  int16_t r;
  int16_t i;
} c16_t;

typedef struct {
  uint32_t frame;
  uint32_t slot;
  uint32_t rb;
  uint16_t num_subcarriers;
  c16_t h_per_rb[12];
} csi_rb_measurement_t;

typedef struct {
  uint32_t capacity;
  uint32_t write_idx;
  uint32_t read_idx;
  csi_rb_measurement_t *buffer;
} csi_ring_buffer_t;

int csi_ring_buffer_init(csi_ring_buffer_t *rb, uint32_t capacity);
int csi_ring_buffer_push(csi_ring_buffer_t *rb, const csi_rb_measurement_t *meas);
int csi_ring_buffer_pop(csi_ring_buffer_t *rb, csi_rb_measurement_t *meas);
void csi_ring_buffer_free(csi_ring_buffer_t *rb);

extern int csi_rb_logging_enabled;
extern void (*csi_rb_logging_callback)(const void *, const void *, const void *, const void *);

#endif
