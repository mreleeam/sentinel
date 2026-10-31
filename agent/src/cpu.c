#include "../include/cpu.h"
#include "../include/logger.h"

#include <stdio.h>

static unsigned long long previous_total;
static unsigned long long previous_idle;
static int have_previous = 0;

static int read_cpu(unsigned long long *total, unsigned long long *idle) {
  FILE *file = fopen("/proc/stat", "r");

  if (file == NULL) {
    return -1;
  }

  char line[256];

  if (fgets(line, sizeof(line), file) == NULL) {
    fclose(file);
    return -1;
  }

  fclose(file);

  char cpu[16];

  unsigned long long user = 0;
  unsigned long long nice = 0;
  unsigned long long system = 0;
  unsigned long long idle_time = 0;
  unsigned long long iowait = 0;
  unsigned long long irq = 0;
  unsigned long long softirq = 0;
  unsigned long long steal = 0;

  int fields =
      sscanf(line, "%15s %llu %llu %llu %llu %llu %llu %llu %llu", cpu, &user,
             &nice, &system, &idle_time, &iowait, &irq, &softirq, &steal);

  if (fields < 5) {
    return -1;
  }

  *total = user + nice + system + idle_time + iowait + irq + softirq + steal;

  *idle = idle_time + iowait;

  return 0;
}

int cpu_init(void) {
  previous_total = 0;
  previous_idle = 0;
  have_previous = 0;

  log_info("CPU collector initialized");

  return 0;
}

int cpu_collect(cpu_metrics_t *metrics) {
  if (metrics == NULL) {
    return -1;
  }

  unsigned long long total;
  unsigned long long idle;

  if (read_cpu(&total, &idle) != 0) {
    log_error("Failed to read /proc/stat");
    return -1;
  }

  metrics->usage = 0.0;

  if (have_previous) {
    unsigned long long total_diff = total - previous_total;
    unsigned long long idle_diff = idle - previous_idle;

    if (total_diff > 0 && idle_diff <= total_diff) {
      metrics->usage =
          100.0 * (double)(total_diff - idle_diff) / (double)total_diff;
    }
  }

  previous_total = total;
  previous_idle = idle;
  have_previous = 1;

  return 0;
}

void cpu_cleanup(void) { log_info("CPU collector stopped"); }
