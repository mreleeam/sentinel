#include "../include/swap.h"
#include "../include/logger.h"

#include <stdio.h>
#include <string.h>

#define BYTES_PER_GB 1073741824.0

int swap_init(void) {
  log_info("Swap collector initialized");

  return 0;
}

int swap_collect(swap_metrics_t *metrics) {
  if (metrics == NULL) {
    return -1;
  }

  FILE *file = fopen("/proc/meminfo", "r");

  if (file == NULL) {
    log_error("Failed to open /proc/meminfo");
    return -1;
  }

  unsigned long total_kb = 0;
  unsigned long free_kb = 0;

  char line[256];

  while (fgets(line, sizeof(line), file) != NULL) {
    if (strncmp(line, "SwapTotal:", 10) == 0) {
      sscanf(line + 10, "%lu", &total_kb);
    } else if (strncmp(line, "SwapFree:", 9) == 0) {
      sscanf(line + 9, "%lu", &free_kb);
    }
  }

  fclose(file);

  if (free_kb > total_kb) {
    return -1;
  }

  unsigned long used_kb = total_kb - free_kb;

  metrics->total_gb = ((double)total_kb * 1024.0) / BYTES_PER_GB;

  metrics->free_gb = ((double)free_kb * 1024.0) / BYTES_PER_GB;

  metrics->used_gb = ((double)used_kb * 1024.0) / BYTES_PER_GB;

  if (total_kb > 0) {
    metrics->usage = 100.0 * (double)used_kb / (double)total_kb;
  } else {
    metrics->usage = 0.0;
  }

  return 0;
}

void swap_cleanup(void) { log_info("Swap collector stopped"); }
