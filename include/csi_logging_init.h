#ifndef CSI_LOGGING_INIT_H
#define CSI_LOGGING_INIT_H

int csi_logging_init(const char *output_file);
void csi_logging_cleanup(void);
int csi_logging_init_from_env(void);

#endif /* CSI_LOGGING_INIT_H */

