#include "../include/memory.h"
#include "../include/logger.h"

#include <stdio.h>
#include <string.h>

#define KB_PER_GB 1048576.0

int memory_init(void) {
  log_info("Memory collector initialized");

  return 0;
}

int memory_collect(memory_metrics_t *metrics) {
  if (metrics == NULL) {
    return -1;
  }

  FILE *file = fopen("/proc/meminfo", "r");

  if (file == NULL) {
    log_error("Failed to open /proc/meminfo");
    return -1;
  }

  unsigned long total_kb = 0;
  unsigned long available_kb = 0;

  char line[256];

  while (fgets(line, sizeof(line), file) != NULL) {
    if (strncmp(line, "MemTotal:", 9) == 0) {
      sscanf(line + 9, "%lu", &total_kb);
    } else if (strncmp(line, "MemAvailable:", 13) == 0) {
      sscanf(line + 13, "%lu", &available_kb);
    }
  }

  fclose(file);

  if (total_kb == 0 || available_kb > total_kb) {
    return -1;
  }

  unsigned long used_kb = total_kb - available_kb;

  metrics->total_gb = (double)total_kb / KB_PER_GB;
  metrics->available_gb = (double)available_kb / KB_PER_GB;
  metrics->used_gb = (double)used_kb / KB_PER_GB;

  metrics->usage = 100.0 * (double)used_kb / (double)total_kb;

  return 0;
}

void memory_cleanup(void) { log_info("Memory collector stopped"); }
