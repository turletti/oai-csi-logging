#include "csi_rb_logging_external.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern int csi_rb_logging_enabled;

void csi_rb_logging_callback_impl(
    const void *channel_data,
    const void *proc,
    const void *config,
    const void *unused)
{
  if (!csi_rb_logging_enabled || !channel_data) return;
  
  /* For CSI-RS: channel_data is the estimated channel
   * proc contains frame/slot info
   * config contains RB and port info
   */
  
  uint32_t frame = 0;
  uint32_t slot = 0;
  uint16_t start_rb = 0;
  uint16_t num_rbs = 1;
  
  /* Push measurement for each RB */
  for (uint16_t rb = start_rb; rb < start_rb + num_rbs; rb++) {
    csi_logging_push_measurement(frame, slot, rb, channel_data, 12);
  }
}
