#include <stddef.h>
#include "csi_rb_logging_external.h"

int csi_rb_logging_enabled = 0;
void (*csi_rb_logging_callback)(const void *, const void *, const void *, const void *) = NULL;
