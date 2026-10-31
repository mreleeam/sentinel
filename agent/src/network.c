#define _POSIX_C_SOURCE 200809L

#include "../include/network.h"
#include "../include/logger.h"

#include <limits.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define BYTES_PER_MB 1048576.0

static unsigned long long previous_rx_bytes = 0;
static unsigned long long previous_tx_bytes = 0;

static struct timespec previous_time;
static int have_previous_sample = 0;

static char previous_interface[IF_NAMESIZE] = "";

static int get_default_interface(char *interface, size_t size) {
  if (interface == NULL || size == 0) {
    return -1;
  }

  FILE *file = fopen("/proc/net/route", "r");

  if (file == NULL) {
    return -1;
  }

  char line[512];

  if (fgets(line, sizeof(line), file) == NULL) {
    fclose(file);
    return -1;
  }

  int best_metric = INT_MAX;
  char best_interface[IF_NAMESIZE] = "";

  while (fgets(line, sizeof(line), file) != NULL) {
    char iface[IF_NAMESIZE];

    unsigned long destination = 0;
    unsigned long gateway = 0;

    unsigned int flags = 0;
    unsigned int metric = 0;

    int parsed = sscanf(line, "%15s %lx %lx %x %*u %*u %u", iface, &destination,
                        &gateway, &flags, &metric);

    if (parsed != 5) {
      continue;
    }

    if (destination != 0) {
      continue;
    }

    if ((flags & 0x1) == 0) {
      continue;
    }

    if ((int)metric < best_metric) {
      best_metric = (int)metric;

      snprintf(best_interface, sizeof(best_interface), "%s", iface);
    }
  }

  fclose(file);

  if (best_interface[0] == '\0') {
    return -1;
  }

  snprintf(interface, size, "%s", best_interface);

  return 0;
}

static int read_interface_bytes(const char *interface,
                                unsigned long long *rx_bytes,
                                unsigned long long *tx_bytes) {

  if (interface == NULL || rx_bytes == NULL || tx_bytes == NULL) {
    return -1;
  }

  FILE *file = fopen("/proc/net/dev", "r");

  if (file == NULL) {
    return -1;
  }

  char line[512];

  while (fgets(line, sizeof(line), file) != NULL) {
    char name[IF_NAMESIZE];

    unsigned long long rx = 0;
    unsigned long long tx = 0;

    int parsed = sscanf(line, " %15[^:]: %llu %*u %*u %*u %*u %*u %*u %*u %llu",
                        name, &rx, &tx);

    if (parsed != 3) {
      continue;
    }

    if (strcmp(name, interface) != 0) {
      continue;
    }

    *rx_bytes = rx;
    *tx_bytes = tx;

    fclose(file);

    return 0;
  }

  fclose(file);

  return -1;
}

static double get_elapsed_seconds(void) {
  struct timespec now;

  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
    return 0.0;
  }

  if (!have_previous_sample) {
    previous_time = now;
    have_previous_sample = 1;

    return 0.0;
  }

  double elapsed = (double)(now.tv_sec - previous_time.tv_sec) +
                   (double)(now.tv_nsec - previous_time.tv_nsec) / 1000000000.0;

  previous_time = now;

  return elapsed;
}

int network_init(void) {
  previous_rx_bytes = 0;
  previous_tx_bytes = 0;

  have_previous_sample = 0;

  previous_interface[0] = '\0';

  log_info("Network collector initialized");

  return 0;
}

int network_collect(network_metrics_t *metrics) {
  if (metrics == NULL) {
    return -1;
  }

  memset(metrics, 0, sizeof(*metrics));

  char interface[sizeof(metrics->interface)];

  if (get_default_interface(interface, sizeof(interface)) != 0) {
    log_error("Failed to determine default network interface");
    return -1;
  }

  unsigned long long rx_bytes = 0;
  unsigned long long tx_bytes = 0;

  if (read_interface_bytes(interface, &rx_bytes, &tx_bytes) != 0) {
    log_error("Failed to read network statistics for %s", interface);

    return -1;
  }

  snprintf(metrics->interface, sizeof(metrics->interface), "%s", interface);

  /*
   * If the interface changed, reset the rate baseline.
   */
  if (strcmp(previous_interface, interface) != 0) {
    snprintf(previous_interface, sizeof(previous_interface), "%s", interface);

    previous_rx_bytes = rx_bytes;
    previous_tx_bytes = tx_bytes;

    have_previous_sample = 0;

    metrics->rx_mb = (double)rx_bytes / BYTES_PER_MB;

    metrics->tx_mb = (double)tx_bytes / BYTES_PER_MB;

    metrics->rx_mb_s = 0.0;
    metrics->tx_mb_s = 0.0;

    return 0;
  }

  double elapsed = get_elapsed_seconds();

  metrics->rx_mb = (double)rx_bytes / BYTES_PER_MB;

  metrics->tx_mb = (double)tx_bytes / BYTES_PER_MB;

  metrics->rx_mb_s = 0.0;
  metrics->tx_mb_s = 0.0;

  if (elapsed > 0.0 && rx_bytes >= previous_rx_bytes &&
      tx_bytes >= previous_tx_bytes) {

    unsigned long long rx_delta = rx_bytes - previous_rx_bytes;

    unsigned long long tx_delta = tx_bytes - previous_tx_bytes;

    metrics->rx_mb_s = ((double)rx_delta / BYTES_PER_MB) / elapsed;

    metrics->tx_mb_s = ((double)tx_delta / BYTES_PER_MB) / elapsed;
  }

  previous_rx_bytes = rx_bytes;
  previous_tx_bytes = tx_bytes;

  return 0;
}

void network_cleanup(void) {
  previous_rx_bytes = 0;
  previous_tx_bytes = 0;

  have_previous_sample = 0;

  previous_interface[0] = '\0';

  log_info("Network collector stopped");
}
