#ifndef SENTINEL_CPU_H
#define SENTINEL_CPU_H

#include "metrics.h"

int cpu_init(void);
int cpu_collect(cpu_metrics_t *metrics);
void cpu_cleanup(void);

#endif
