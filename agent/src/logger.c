#define _POSIX_C_SOURCE 200809L

#include "../include/logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static void log_message(const char *level, const char *format, va_list args) {
  time_t now = time(NULL);
  struct tm time_info;

  if (localtime_r(&now, &time_info) == NULL) {
    fprintf(stderr, "[%s] ", level);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    return;
  }

  char timestamp[32];

  if (strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &time_info) ==
      0) {
    fprintf(stderr, "[%s] ", level);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    return;
  }

  fprintf(stderr, "[%s] [%s] ", timestamp, level);
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
  fflush(stderr);
}

void log_info(const char *format, ...) {
  va_list args;

  va_start(args, format);
  log_message("INFO", format, args);
  va_end(args);
}

void log_warn(const char *format, ...) {
  va_list args;

  va_start(args, format);
  log_message("WARN", format, args);
  va_end(args);
}

void log_error(const char *format, ...) {
  va_list args;

  va_start(args, format);
  log_message("ERROR", format, args);
  va_end(args);
}
