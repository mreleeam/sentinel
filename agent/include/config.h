#ifndef SENTINEL_CONFIG_H
#define SENTINEL_CONFIG_H

#define SENTINEL_DEFAULT_CONFIG "/etc/sentinel/config.ini"
#define SENTINEL_DEFAULT_STATE "/var/lib/sentinel/metrics.json"

typedef struct {
  int interval;
  char state_path[256];
} sentinel_config_t;

int config_load(const char *path, sentinel_config_t *config);

#endif
