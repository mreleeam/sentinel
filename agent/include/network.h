#ifndef SENTINEL_NETWORK_H
#define SENTINEL_NETWORK_H

#include "metrics.h"

int network_init(void);
int network_collect(network_metrics_t *metrics);
void network_cleanup(void);

#endif
