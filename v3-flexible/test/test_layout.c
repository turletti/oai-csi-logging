/* Test: logged RB values must match an OAI-layout (DC-centred) SRS buffer. */
/* OAI-layout SRS buffer (DC-centred, per-symbol blocks) fed to the v3.1 logger.
 * Carrier subcarrier g of symbol s, antenna a, port p holds:
 *   real = g/12 + 1 (+ s*2)      imag = 100*a + 10*p + 1 (+ s*2)
 * so the averaged RB value must be exactly (rb+1+(nsym-1)) and imag tag known. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
typedef struct { int16_t i, q; } c16_t;
#include "csi_rb_logging_v3.h"
void nr_csi_logging_shutdown_v3(void);
int main(int argc, char **argv) {
  int N = atoi(argv[1]), NRB = atoi(argv[2]), nant = atoi(argv[3]), nap = atoi(argv[4]), nsym = atoi(argv[5]);
  int bwp_start = argc > 6 ? atoi(argv[6]) : 0, bwp_size = argc > 7 ? atoi(argv[7]) : NRB;
  int fco = N - 6 * NRB, base = fco - N / 2;
  c16_t *H = calloc((size_t)nant * nap * N * nsym, sizeof(c16_t));
  for (int a = 0; a < nant; a++) for (int p = 0; p < nap; p++) for (int s = 0; s < nsym; s++)
    for (int g = 0; g < 12 * NRB; g++) {
      c16_t *v = &H[((size_t)a * nap + p) * N * nsym + (size_t)s * N + base + g];
      v->i = (int16_t)(g / 12 + 1 + 2 * s); v->q = (int16_t)(100 * a + 10 * p + 1 + 2 * s);
    }
  for (int f = 0; f < 3; f++)
    nr_srs_csi_logging_invoke_v3(100 + f, 8, 0x4601, nant, nap, nsym, N, fco, bwp_start, bwp_size, H);
  nr_csi_logging_shutdown_v3();
  free(H);
  return 0;
}
