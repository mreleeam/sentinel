#include "../include/disk.h"
#include "../include/logger.h"

#include <stddef.h>
#include <sys/statvfs.h>

#define BYTES_PER_GB 1073741824.0

int disk_init(void) {
  log_info("Disk collector initialized");

  return 0;
}

int disk_collect(disk_metrics_t *metrics) {
  if (metrics == NULL) {
    return -1;
  }

  struct statvfs fs;

  if (statvfs("/", &fs) != 0) {
    log_error("Failed to read filesystem statistics");
    return -1;
  }

  unsigned long long total_bytes =
      (unsigned long long)fs.f_blocks * (unsigned long long)fs.f_frsize;

  unsigned long long available_bytes =
      (unsigned long long)fs.f_bavail * (unsigned long long)fs.f_frsize;

  if (total_bytes == 0 || available_bytes > total_bytes) {
    return -1;
  }

  unsigned long long used_bytes = total_bytes - available_bytes;

  metrics->total_gb = (double)total_bytes / BYTES_PER_GB;

  metrics->available_gb = (double)available_bytes / BYTES_PER_GB;

  metrics->usage = 100.0 * (double)used_bytes / (double)total_bytes;

  return 0;
}

void disk_cleanup(void) { log_info("Disk collector stopped"); }
