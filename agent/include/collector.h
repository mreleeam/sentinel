#ifndef SENTINEL_COLLECTOR_H
#define SENTINEL_COLLECTOR_H

#include "metrics.h"

typedef int (*collector_init_fn)(void);
typedef int (*collector_collect_fn)(system_metrics_t *metrics);
typedef void (*collector_cleanup_fn)(void);

typedef struct {
  const char *name;
  collector_init_fn init;
  collector_collect_fn collect;
  collector_cleanup_fn cleanup;
} collector_t;

#endif
