#include "csi_rb_logging_external.h"
#include <stdio.h>

void csi_rb_logging_callback_impl(
    const void *channel_data,
    const void *proc,
    const void *config,
    const void *unused)
{
  fprintf(stderr, "[CSI] Callback called\n");
}
