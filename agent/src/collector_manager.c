#define _POSIX_C_SOURCE 200809L

#include "../include/collector_manager.h"

#include "../include/cpu.h"
#include "../include/disk.h"
#include "../include/logger.h"
#include "../include/memory.h"
#include "../include/network.h"
#include "../include/process.h"
#include "../include/state.h"
#include "../include/swap.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int collect_hostname(char *hostname, size_t size) {

  if (hostname == NULL || size == 0) {
    return -1;
  }

  if (gethostname(hostname, size) != 0) {
    hostname[0] = '\0';
    return -1;
  }

  hostname[size - 1] = '\0';

  return 0;
}

static int collect_uptime(char *uptime_string, size_t size) {

  if (uptime_string == NULL || size == 0) {
    return -1;
  }

  FILE *file = fopen("/proc/uptime", "r");

  if (file == NULL) {
    log_error("Failed to open /proc/uptime");
    return -1;
  }

  double uptime_seconds = 0.0;

  if (fscanf(file, "%lf", &uptime_seconds) != 1) {

    fclose(file);

    log_error("Failed to read /proc/uptime");

    return -1;
  }

  fclose(file);

  if (uptime_seconds < 0.0) {
    return -1;
  }

  unsigned long long total_seconds = (unsigned long long)uptime_seconds;

  unsigned long long days = total_seconds / 86400ULL;

  total_seconds %= 86400ULL;

  unsigned long long hours = total_seconds / 3600ULL;

  total_seconds %= 3600ULL;

  unsigned long long minutes = total_seconds / 60ULL;

  unsigned long long seconds = total_seconds % 60ULL;

  if (days > 0) {

    snprintf(uptime_string, size, "%llud %lluh %llum", days, hours, minutes);

  } else if (hours > 0) {

    snprintf(uptime_string, size, "%lluh %llum %llus", hours, minutes, seconds);

  } else if (minutes > 0) {

    snprintf(uptime_string, size, "%llum %llus", minutes, seconds);

  } else {

    snprintf(uptime_string, size, "%llus", seconds);
  }

  return 0;
}

int collectors_init(void) {
  log_info("Initializing collectors");

  if (cpu_init() != 0) {
    log_error("CPU collector initialization failed");

    return -1;
  }

  if (memory_init() != 0) {
    log_error("Memory collector initialization failed");

    return -1;
  }

  if (swap_init() != 0) {
    log_error("Swap collector initialization failed");

    return -1;
  }

  if (disk_init() != 0) {
    log_error("Disk collector initialization failed");

    return -1;
  }

  if (network_init() != 0) {
    log_error("Network collector initialization failed");

    return -1;
  }

  if (process_init() != 0) {
    log_error("Process collector initialization failed");

    return -1;
  }

  log_info("All collectors initialized");

  return 0;
}

int collectors_run(const char *state_path, int report) {

  if (state_path == NULL) {
    return -1;
  }

  system_metrics_t metrics = {0};

  if (collect_hostname(metrics.hostname, sizeof(metrics.hostname)) != 0) {

    log_error("Hostname collection failed");
  }

  if (collect_uptime(metrics.uptime, sizeof(metrics.uptime)) != 0) {

    log_error("Uptime collection failed");
  }

  if (cpu_collect(&metrics.cpu) != 0) {
    log_error("CPU collection failed");
  }

  if (memory_collect(&metrics.memory) != 0) {
    log_error("Memory collection failed");
  }

  if (swap_collect(&metrics.swap) != 0) {
    log_error("Swap collection failed");
  }

  if (disk_collect(&metrics.disk) != 0) {
    log_error("Disk collection failed");
  }

  if (network_collect(&metrics.network) != 0) {
    log_error("Network collection failed");
  }

  if (process_collect(&metrics.processes) != 0) {
    log_error("Process collection failed");
  }

  if (!report) {
    return 0;
  }

  if (state_write(state_path, &metrics) != 0) {
    log_error("Failed to write state");

    return -1;
  }

  return 0;
}

void collectors_cleanup(void) {
  cpu_cleanup();
  memory_cleanup();
  swap_cleanup();
  disk_cleanup();
  network_cleanup();
  process_cleanup();

  log_info("Collectors stopped");
}
