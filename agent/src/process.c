#define _GNU_SOURCE

#include "../include/process.h"
#include "../include/logger.h"

#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_CPU_CACHE 4096
#define KB_PER_MB 1024.0

typedef struct {
  int pid;
  unsigned long long cpu_time;
} cpu_cache_entry_t;

static cpu_cache_entry_t cpu_cache[MAX_CPU_CACHE];
static int cpu_cache_count = 0;

static struct timespec previous_collect_time;
static int have_previous_time = 0;

static int cpu_count = 1;

static unsigned long long get_process_cpu_time(int pid) {
  char path[64];

  snprintf(path, sizeof(path), "/proc/%d/stat", pid);

  FILE *file = fopen(path, "r");

  if (file == NULL) {
    return 0;
  }

  char line[4096];

  if (fgets(line, sizeof(line), file) == NULL) {
    fclose(file);
    return 0;
  }

  fclose(file);

  char *closing_paren = strrchr(line, ')');

  if (closing_paren == NULL) {
    return 0;
  }

  char state = '\0';

  unsigned long long utime = 0;
  unsigned long long stime = 0;

  int fields = sscanf(closing_paren + 2,
                      "%c "
                      "%*d %*d %*d %*d %*d "
                      "%*u %*u %*u %*u %*u "
                      "%llu %llu",
                      &state, &utime, &stime);

  if (fields != 3) {
    return 0;
  }

  return utime + stime;
}

static unsigned long long get_previous_cpu(int pid) {
  for (int i = 0; i < cpu_cache_count; i++) {
    if (cpu_cache[i].pid == pid) {
      return cpu_cache[i].cpu_time;
    }
  }

  return 0;
}

static void update_cpu_cache(int pid, unsigned long long cpu_time) {

  for (int i = 0; i < cpu_cache_count; i++) {
    if (cpu_cache[i].pid == pid) {
      cpu_cache[i].cpu_time = cpu_time;
      return;
    }
  }

  if (cpu_cache_count >= MAX_CPU_CACHE) {
    return;
  }

  cpu_cache[cpu_cache_count].pid = pid;
  cpu_cache[cpu_cache_count].cpu_time = cpu_time;

  cpu_cache_count++;
}

static double get_elapsed_seconds(void) {
  struct timespec now;

  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
    return 0.0;
  }

  if (!have_previous_time) {
    previous_collect_time = now;
    have_previous_time = 1;

    return 0.0;
  }

  double elapsed =
      (double)(now.tv_sec - previous_collect_time.tv_sec) +
      (double)(now.tv_nsec - previous_collect_time.tv_nsec) / 1000000000.0;

  previous_collect_time = now;

  return elapsed;
}

static void collect_process_details(int pid, process_info_t *process) {

  char path[64];

  snprintf(path, sizeof(path), "/proc/%d/status", pid);

  FILE *file = fopen(path, "r");

  if (file != NULL) {
    char line[256];

    while (fgets(line, sizeof(line), file) != NULL) {

      if (strncmp(line, "Name:", 5) == 0) {

        sscanf(line, "Name:\t%63[^\n]", process->name);

      } else if (strncmp(line, "Uid:", 4) == 0) {

        unsigned long uid = 0;

        if (sscanf(line, "Uid:\t%lu", &uid) == 1) {

          struct passwd *user = getpwuid((uid_t)uid);

          if (user != NULL && user->pw_name != NULL) {

            snprintf(process->user, sizeof(process->user), "%s", user->pw_name);

          } else {

            snprintf(process->user, sizeof(process->user), "%lu", uid);
          }
        }

      } else if (strncmp(line, "VmRSS:", 6) == 0) {

        unsigned long memory_kb = 0;

        if (sscanf(line, "VmRSS:\t%lu", &memory_kb) == 1) {

          process->memory_mb = (double)memory_kb / KB_PER_MB;
        }
      }
    }

    fclose(file);
  }

  snprintf(path, sizeof(path), "/proc/%d/exe", pid);

  ssize_t length =
      readlink(path, process->exe_path, sizeof(process->exe_path) - 1);

  if (length > 0) {
    process->exe_path[length] = '\0';
  }
}

static int compare_processes(const void *left, const void *right) {

  const process_info_t *a = (const process_info_t *)left;

  const process_info_t *b = (const process_info_t *)right;

  if (a->cpu_usage < b->cpu_usage) {
    return 1;
  }

  if (a->cpu_usage > b->cpu_usage) {
    return -1;
  }

  if (a->memory_mb < b->memory_mb) {
    return 1;
  }

  if (a->memory_mb > b->memory_mb) {
    return -1;
  }

  return 0;
}

int process_init(void) {
  log_info("Process collector initialized");

  memset(cpu_cache, 0, sizeof(cpu_cache));

  cpu_cache_count = 0;
  have_previous_time = 0;

  long detected_cpu_count = sysconf(_SC_NPROCESSORS_ONLN);

  if (detected_cpu_count > 0) {
    cpu_count = (int)detected_cpu_count;
  } else {
    cpu_count = 1;
  }

  log_info("Process CPU normalization: %d logical CPUs", cpu_count);

  return 0;
}

int process_collect(process_metrics_t *metrics) {

  if (metrics == NULL) {
    return -1;
  }

  DIR *proc_directory = opendir("/proc");

  if (proc_directory == NULL) {
    log_error("Failed to open /proc: %s", strerror(errno));

    return -1;
  }

  double elapsed = get_elapsed_seconds();

  long clock_ticks = sysconf(_SC_CLK_TCK);

  if (clock_ticks <= 0) {
    clock_ticks = 100;
  }

  metrics->count = 0;

  struct dirent *entry;

  while ((entry = readdir(proc_directory)) != NULL) {

    if (metrics->count >= MAX_PROCESSES) {
      break;
    }

    char *end = NULL;

    long pid_value = strtol(entry->d_name, &end, 10);

    if (end == entry->d_name || *end != '\0' || pid_value <= 0) {
      continue;
    }

    int pid = (int)pid_value;

    process_info_t *process = &metrics->processes[metrics->count];

    memset(process, 0, sizeof(*process));

    process->pid = pid;

    collect_process_details(pid, process);

    unsigned long long current_cpu = get_process_cpu_time(pid);

    unsigned long long previous_cpu = get_previous_cpu(pid);

    process->cpu_usage = 0.0;

    if (elapsed > 0.0 && previous_cpu != 0 && current_cpu >= previous_cpu) {

      unsigned long long delta = current_cpu - previous_cpu;

      double cpu_seconds = (double)delta / (double)clock_ticks;

      process->cpu_usage = (cpu_seconds / elapsed) * 100.0 / (double)cpu_count;
    }

    update_cpu_cache(pid, current_cpu);

    metrics->count++;
  }

  closedir(proc_directory);

  qsort(metrics->processes, (size_t)metrics->count,
        sizeof(metrics->processes[0]), compare_processes);

  return 0;
}

void process_cleanup(void) {
  log_info("Process collector stopped");

  memset(cpu_cache, 0, sizeof(cpu_cache));

  cpu_cache_count = 0;
  have_previous_time = 0;
}
