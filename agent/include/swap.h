#ifndef SENTINEL_SWAP_H
#define SENTINEL_SWAP_H

#include "metrics.h"

int swap_init(void);
int swap_collect(swap_metrics_t *metrics);
void swap_cleanup(void);

#endif
