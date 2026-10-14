/* Real-time behaviour: an "L1" thread calls the logger every PERIOD_US (default aw2s-like: 133 RB, 1 ant;
 * -DNRB=273 -DNANT=4 -DFFT=4096 for a Benetel-like 100 MHz 4T4R carrier) for ~12 s, so the flush thread
 * runs twice. Reports the worst call duration and the row count. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
typedef struct { int16_t i, q; } c16_t;
#include "csi_rb_logging_v3.h"
#ifndef NRB
#define NRB 133
#endif
#ifndef NANT
#define NANT 1
#endif
#ifndef FFT
#define FFT 2048
#endif
void nr_csi_logging_shutdown_v3(void);
static double now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }
int main(void) {
  const int N = FFT;
  c16_t *H = calloc((size_t)NANT * N, sizeof(c16_t));
  for (int a = 0; a < NANT; a++)
    for (int g = 0; g < 12 * NRB; g++) { H[a * N + N - 6 * NRB - N / 2 + g].i = 1000; H[a * N + N - 6 * NRB - N / 2 + g].q = 1; }
  double worst = 0, first = -1, t_end = now_ms() + 12000; long calls = 0;
  while (now_ms() < t_end) {
    double a = now_ms();
    nr_srs_csi_logging_invoke_v3(calls % 1024, 8, 0x4601, NANT, 1, 1, N, N - 6 * NRB, 0, NRB, H);
    double d = now_ms() - a; if (calls == 0) first = d; else if (d > worst) worst = d;
    calls++; usleep(PERIOD_US);
  }
  nr_csi_logging_shutdown_v3();
  printf("calls %ld, expected rows %ld, first call %.2f ms, worst other call %.2f ms\n", calls, (long)calls * NRB * NANT, first, worst);
  return 0;
}
