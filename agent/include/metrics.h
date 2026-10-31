#ifndef SENTINEL_METRICS_H
#define SENTINEL_METRICS_H

#define MAX_PROCESSES 1024

typedef struct {
  double usage;
} cpu_metrics_t;

typedef struct {
  double usage;
  double total_gb;
  double available_gb;
  double used_gb;
} memory_metrics_t;

typedef struct {
  double usage;
  double total_gb;
  double free_gb;
  double used_gb;
} swap_metrics_t;

typedef struct {
  double usage;
  double total_gb;
  double available_gb;
} disk_metrics_t;

typedef struct {
  char interface[16];

  double rx_mb;
  double tx_mb;

  double rx_mb_s;
  double tx_mb_s;
} network_metrics_t;

typedef struct {
  int pid;
  char name[64];
  char user[64];

  double memory_mb;
  double cpu_usage;

  char exe_path[256];
} process_info_t;

typedef struct {
  process_info_t processes[MAX_PROCESSES];
  int count;
} process_metrics_t;

typedef struct {
  char hostname[256];

  char uptime[64];

  cpu_metrics_t cpu;
  memory_metrics_t memory;
  swap_metrics_t swap;
  disk_metrics_t disk;
  network_metrics_t network;
  process_metrics_t processes;
} system_metrics_t;

#endif
