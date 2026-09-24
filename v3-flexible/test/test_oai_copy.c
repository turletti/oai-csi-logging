/* Build the SRS buffer with the exact statements of OAI w37/w38 nr_srs_channel_estimation()
 * ("Copy as DC in center": half_bw, neg_start, memset, memcpy), for arbitrary bwp_start and k_0,
 * then check that the v3.1 logger puts carrier RB r at CSV rb r. srs_est[j] is the estimate of
 * carrier subcarrier bwp_start*12 + k0 + j; we store real = (carrier subcarrier)/12 + 1. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef struct { int16_t i, q; } c16_t;
#include "csi_rb_logging_v3.h"
void nr_csi_logging_shutdown_v3(void);
int main(int argc, char **argv) {
  int N = atoi(argv[1]), NRB = atoi(argv[2]), bwp_start = atoi(argv[3]), bwp_size = atoi(argv[4]), k0 = atoi(argv[5]);
  int msrs_sc = atoi(argv[6]);                       /* K_TC * M_sc_b_SRS: subcarriers covered by the SRS */
  unsigned first_carrier_offset = N - NRB * 12 / 2;  /* nr_parms.c */
  unsigned subcarrier_offset = bwp_start * 12;
  c16_t *freq = calloc(N, sizeof(c16_t));
  c16_t *srs_est = calloc(N, sizeof(c16_t));
  for (int j = 0; j < msrs_sc; j++) { int g = bwp_start * 12 + k0 + j; srs_est[j].i = g / 12 + 1; srs_est[j].q = 7; }
  /* --- verbatim logic from OAI w37/w38 --- */
  const unsigned half_bw = N - first_carrier_offset;
  const unsigned neg_start = N / 2 - half_bw + subcarrier_offset + k0;
  memset(&freq[0], 0, sizeof(c16_t) * neg_start);
  memcpy(&freq[neg_start], srs_est, (N - neg_start) * sizeof(c16_t));
  /* --- */
  nr_srs_csi_logging_invoke_v3(1, 8, 0x4601, 1, 1, 1, N, first_carrier_offset, bwp_start, bwp_size, freq);
  nr_csi_logging_shutdown_v3();
  return 0;
}
