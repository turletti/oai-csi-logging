#include "csi_rb_logging_external.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int csi_ring_buffer_init(csi_ring_buffer_t *rb, uint32_t capacity) {
  if (!rb || capacity == 0)
    return -1;
  
  rb->buffer = (csi_rb_measurement_t *)malloc(capacity * sizeof(csi_rb_measurement_t));
  if (!rb->buffer)
    return -1;
  
  rb->capacity = capacity;
  rb->write_idx = 0;
  rb->read_idx = 0;
  
  memset(rb->buffer, 0, capacity * sizeof(csi_rb_measurement_t));
  return 0;
}

int csi_ring_buffer_push(csi_ring_buffer_t *rb, const csi_rb_measurement_t *meas) {
  if (!rb || !meas)
    return -1;
  
  rb->buffer[rb->write_idx] = *meas;
  rb->write_idx = (rb->write_idx + 1) % rb->capacity;
  
  if (rb->write_idx == rb->read_idx)
    rb->read_idx = (rb->read_idx + 1) % rb->capacity;
  
  return 0;
}

int csi_ring_buffer_pop(csi_ring_buffer_t *rb, csi_rb_measurement_t *meas) {
  if (!rb || !meas)
    return -1;
  
  if (rb->write_idx == rb->read_idx)
    return -2;
  
  *meas = rb->buffer[rb->read_idx];
  rb->read_idx = (rb->read_idx + 1) % rb->capacity;
  
  return 0;
}

void csi_ring_buffer_free(csi_ring_buffer_t *rb) {
  if (!rb)
    return;
  
  if (rb->buffer)
    free(rb->buffer);
  
  rb->buffer = NULL;
  rb->capacity = 0;
  rb->write_idx = 0;
  rb->read_idx = 0;
}
