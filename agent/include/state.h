#ifndef SENTINEL_STATE_H
#define SENTINEL_STATE_H

#include "metrics.h"

int state_write(const char *path, const system_metrics_t *metrics);

#endif
