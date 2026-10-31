#ifndef SENTINEL_DISK_H
#define SENTINEL_DISK_H

#include "metrics.h"

int disk_init(void);
int disk_collect(disk_metrics_t *metrics);
void disk_cleanup(void);

#endif
