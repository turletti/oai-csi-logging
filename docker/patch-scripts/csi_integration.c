#include <stdio.h>
#include "common/platform_types.h"
#include <stdint.h>
#include "csi_oai_wrapper.h"
#include "csi_rb_logging_external.h"

extern int csi_rb_logging_enabled;

void nr_csi_logging_invoke(uint32_t frame, uint32_t slot, uint16_t start_rb,
    uint16_t nr_of_rbs, uint16_t nb_antennas_rx, uint16_t nb_ports,
    const void *csi_data_void, uint16_t ofdm_symbol_size)
{
  if (!csi_rb_logging_enabled) return;
  if (!csi_data_void) return;
  
  const c16_t *csi_data = (const c16_t *)csi_data_void;
  
  for (uint16_t rb = start_rb; rb < start_rb + nr_of_rbs; rb++) {
    for (int ant_rx = 0; ant_rx < nb_antennas_rx; ant_rx++) {
      for (uint16_t port_tx = 0; port_tx < nb_ports; port_tx++) {
        uint16_t k_start = rb * 12;
        uint32_t offset = (ant_rx * nb_ports * ofdm_symbol_size) + 
                          (port_tx * ofdm_symbol_size) + k_start;
        if (offset + 12 <= nb_antennas_rx * nb_ports * ofdm_symbol_size) {
          csi_logging_push_measurement(frame, slot, rb,
              &csi_data[offset], 12);
        }
      }
    }
  }
}

void nr_srs_csi_logging_invoke(uint32_t frame, uint32_t slot,
    uint16_t start_rb, uint16_t nr_of_rbs,
    const c16_t srs_estimated_channel_freq[], uint16_t ofdm_symbol_size)
{
  if (!csi_rb_logging_enabled) return;
  if (!srs_estimated_channel_freq) return;
  
  for (uint16_t rb = start_rb; rb < start_rb + nr_of_rbs; rb++) {
    uint16_t k_start = rb * 12;
    uint32_t offset = k_start;
    if (offset + 12 <= ofdm_symbol_size) {
      csi_logging_push_measurement(frame, slot, rb,
          &srs_estimated_channel_freq[offset], 12);
    }
  }
}
