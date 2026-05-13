#ifndef CSI_LOGGING_INIT_H
#define CSI_LOGGING_INIT_H

int csi_logging_init(const char *output_file);
int csi_logging_init_from_env(void);
int csi_logging_start_thread(void);
void csi_logging_cleanup(void);

#endif
