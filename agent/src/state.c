#define _POSIX_C_SOURCE 200809L

#include "../include/state.h"
#include "../include/logger.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int create_parent_directory(const char *path) {

  char directory[256];

  snprintf(directory, sizeof(directory), "%s", path);

  char *slash = strrchr(directory, '/');

  if (slash == NULL) {
    return 0;
  }

  *slash = '\0';

  if (directory[0] == '\0') {
    return 0;
  }

  struct stat info;

  if (stat(directory, &info) == 0) {

    if (S_ISDIR(info.st_mode)) {
      return 0;
    }

    log_error("%s exists but is not a directory", directory);

    return -1;
  }

  if (mkdir(directory, 0755) != 0 && errno != EEXIST) {

    log_error("Failed to create directory %s: %s", directory, strerror(errno));

    return -1;
  }

  return 0;
}

static void write_json_string(FILE *file, const char *value) {

  fputc('"', file);

  if (value != NULL) {

    for (const char *p = value; *p != '\0'; p++) {

      switch (*p) {

      case '"':
        fputs("\\\"", file);
        break;

      case '\\':
        fputs("\\\\", file);
        break;

      case '\n':
        fputs("\\n", file);
        break;

      case '\r':
        fputs("\\r", file);
        break;

      case '\t':
        fputs("\\t", file);
        break;

      default:
        fputc(*p, file);
        break;
      }
    }
  }

  fputc('"', file);
}

static void write_process(FILE *file, const process_info_t *process) {

  fprintf(file,
          "{"
          "\"pid\":%d,"
          "\"name\":",
          process->pid);

  write_json_string(file, process->name);

  fprintf(file, ",\"user\":");

  write_json_string(file, process->user);

  fprintf(file,
          ","
          "\"memory_mb\":%.2f,"
          "\"cpu_usage\":%.2f,"
          "\"exe_path\":",
          process->memory_mb, process->cpu_usage);

  write_json_string(file, process->exe_path);

  fputc('}', file);
}

int state_write(const char *path, const system_metrics_t *metrics) {

  if (path == NULL || metrics == NULL) {
    return -1;
  }

  if (create_parent_directory(path) != 0) {
    return -1;
  }

  char temporary_path[512];

  int written =
      snprintf(temporary_path, sizeof(temporary_path), "%s.tmp", path);

  if (written < 0 || (size_t)written >= sizeof(temporary_path)) {

    log_error("State path is too long");

    return -1;
  }

  FILE *file = fopen(temporary_path, "w");

  if (file == NULL) {

    log_error("Failed to open state file: %s", strerror(errno));

    return -1;
  }

  fprintf(file, "{\n"
                "  \"hostname\": ");

  write_json_string(file, metrics->hostname);

  fprintf(file, ",\n"
                "  \"uptime\": ");

  write_json_string(file, metrics->uptime);

  fprintf(file,
          ",\n"
          "  \"cpu\": {\n"
          "    \"usage\": %.2f\n"
          "  },\n"
          "  \"memory\": {\n"
          "    \"usage\": %.2f,\n"
          "    \"total_gb\": %.2f,\n"
          "    \"used_gb\": %.2f,\n"
          "    \"available_gb\": %.2f\n"
          "  },\n"
          "  \"swap\": {\n"
          "    \"usage\": %.2f,\n"
          "    \"total_gb\": %.2f,\n"
          "    \"used_gb\": %.2f,\n"
          "    \"free_gb\": %.2f\n"
          "  },\n"
          "  \"disk\": {\n"
          "    \"usage\": %.2f,\n"
          "    \"total_gb\": %.2f,\n"
          "    \"available_gb\": %.2f\n"
          "  },\n"
          "  \"network\": {\n"
          "    \"interface\": ",
          metrics->cpu.usage, metrics->memory.usage, metrics->memory.total_gb,
          metrics->memory.used_gb, metrics->memory.available_gb,
          metrics->swap.usage, metrics->swap.total_gb, metrics->swap.used_gb,
          metrics->swap.free_gb, metrics->disk.usage, metrics->disk.total_gb,
          metrics->disk.available_gb);

  write_json_string(file, metrics->network.interface);

  fprintf(file,
          ",\n"
          "    \"rx_mb\": %.2f,\n"
          "    \"tx_mb\": %.2f,\n"
          "    \"rx_mb_s\": %.2f,\n"
          "    \"tx_mb_s\": %.2f\n"
          "  },\n"
          "  \"processes\": [\n",
          metrics->network.rx_mb, metrics->network.tx_mb,
          metrics->network.rx_mb_s, metrics->network.tx_mb_s);

  for (int i = 0; i < metrics->processes.count; i++) {

    fprintf(file, "    ");

    write_process(file, &metrics->processes.processes[i]);

    if (i + 1 < metrics->processes.count) {
      fputc(',', file);
    }

    fputc('\n', file);
  }

  fprintf(file,
          "  ],\n"
          "  \"process_count\": %d\n"
          "}\n",
          metrics->processes.count);

  if (fflush(file) != 0) {

    log_error("Failed to flush state file: %s", strerror(errno));

    fclose(file);
    unlink(temporary_path);

    return -1;
  }

  if (fclose(file) != 0) {

    log_error("Failed to close state file: %s", strerror(errno));

    unlink(temporary_path);

    return -1;
  }

  if (rename(temporary_path, path) != 0) {

    log_error("Failed to update state file: %s", strerror(errno));

    unlink(temporary_path);

    return -1;
  }

  return 0;
}
