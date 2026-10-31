#ifndef SENTINEL_MEMORY_H
#define SENTINEL_MEMORY_H

#include "metrics.h"

int memory_init(void);
int memory_collect(memory_metrics_t *metrics);
void memory_cleanup(void);

#endif
