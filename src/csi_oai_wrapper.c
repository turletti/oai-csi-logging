#include "csi_rb_logging_external.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>

typedef struct {
  int16_t r;
  int16_t i;
} c16_t_oai;

extern int csi_logging_push_measurement(uint32_t frame, uint32_t slot, uint32_t rb,
                                        const void *h_data, uint32_t num_subcarriers);

typedef struct {
  uint32_t gNB_id;
  uint32_t nr_slot_rx;
  uint32_t frame_rx;
} UE_nr_rxtx_proc_t_stub;

typedef struct {
  uint16_t start_rb;
  uint16_t nr_of_rbs;
  uint16_t freq_density;
} fapi_nr_dl_config_csirs_pdu_rel15_t_stub;

void csi_rb_logging_callback_impl(
    const void *ue,
    const void *proc,
    const c16_t_oai csi_rs_estimated_channel_freq[][2][1200],
    const void *csirs_config_pdu)
{
  if (!proc || !csirs_config_pdu)
    return;
  
  const UE_nr_rxtx_proc_t_stub *proc_stub = (const UE_nr_rxtx_proc_t_stub *)proc;
  const fapi_nr_dl_config_csirs_pdu_rel15_t_stub *config_stub = 
    (const fapi_nr_dl_config_csirs_pdu_rel15_t_stub *)csirs_config_pdu;
  
  uint32_t frame = proc_stub->frame_rx;
  uint32_t slot = proc_stub->nr_slot_rx;
  uint16_t start_rb = config_stub->start_rb;
  uint16_t nr_of_rbs = config_stub->nr_of_rbs;
  
  for (int rb = start_rb; rb < start_rb + nr_of_rbs; rb++) {
    c16_t h_per_rb[12];
    
    for (int k = 0; k < 12; k++) {
      int subcarrier = rb * 12 + k;
      h_per_rb[k].r = csi_rs_estimated_channel_freq[0][0][subcarrier].r;
      h_per_rb[k].i = csi_rs_estimated_channel_freq[0][0][subcarrier].i;
    }
    
    csi_logging_push_measurement(frame, slot, rb, h_per_rb, 12);
  }
}
