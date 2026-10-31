#define _POSIX_C_SOURCE 200809L

#include "../include/collector_manager.h"
#include "../include/config.h"
#include "../include/logger.h"

#include <signal.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;

static void handle_signal(int signal_number) {
  (void)signal_number;
  running = 0;
}

int main(void) {
  log_info("Sentinel starting");

  sentinel_config_t config = {0};

  if (config_load(SENTINEL_DEFAULT_CONFIG, &config) != 0) {
    log_warn("Failed to load configuration, using defaults");
  }

  log_info("Monitoring interval: %d seconds", config.interval);

  if (config.interval <= 0) {
    log_warn("Invalid monitoring interval, using 1 second");
    config.interval = 1;
  }

  if (collectors_init() != 0) {
    log_error("Failed to initialize collectors");
    return 1;
  }

  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  log_info("Sentinel monitoring started");

  if (collectors_run(config.state_path, 0) != 0) {
    log_error("Initial collector baseline failed");
  }

  struct timespec startup_delay = {.tv_sec = 0, .tv_nsec = 100000000L};

  nanosleep(&startup_delay, NULL);

  while (running) {
    if (collectors_run(config.state_path, 1) != 0) {
      log_error("Collector cycle failed");
    }

    if (!running) {
      break;
    }

    sleep((unsigned int)config.interval);
  }

  collectors_cleanup();

  log_info("Sentinel stopped");

  return 0;
}
