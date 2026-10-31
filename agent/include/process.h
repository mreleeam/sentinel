#ifndef SENTINEL_PROCESS_H
#define SENTINEL_PROCESS_H

#include "metrics.h"

int process_init(void);
int process_collect(process_metrics_t *metrics);
void process_cleanup(void);

#endif
