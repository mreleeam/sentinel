#include "../include/config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void trim(char *str) {
  char *start = str;

  while (*start != '\0' && isspace((unsigned char)*start)) {
    start++;
  }

  if (start != str) {
    memmove(str, start, strlen(start) + 1);
  }

  size_t length = strlen(str);

  while (length > 0 && isspace((unsigned char)str[length - 1])) {
    str[length - 1] = '\0';
    length--;
  }
}

static void set_defaults(sentinel_config_t *config) {
  config->interval = 5;

  snprintf(config->state_path, sizeof(config->state_path), "%s",
           SENTINEL_DEFAULT_STATE);
}

int config_load(const char *path, sentinel_config_t *config) {
  if (config == NULL) {
    return -1;
  }

  set_defaults(config);

  FILE *file = fopen(path, "r");

  if (file == NULL) {
    return -1;
  }

  char line[256];
  char section[64] = "";

  while (fgets(line, sizeof(line), file) != NULL) {
    trim(line);

    if (line[0] == '\0' || line[0] == '#') {
      continue;
    }

    if (line[0] == '[') {
      if (sscanf(line, "[%63[^]]", section) != 1) {
        section[0] = '\0';
      }

      trim(section);
      continue;
    }

    char key[64];
    char value[160];

    if (sscanf(line, "%63[^=]=%159[^\n]", key, value) != 2) {
      continue;
    }

    trim(key);
    trim(value);

    if (strcmp(section, "agent") == 0) {
      if (strcmp(key, "interval") == 0) {
        char *end = NULL;
        long value_number = strtol(value, &end, 10);

        if (end != value && *end == '\0' && value_number > 0 &&
            value_number <= 86400) {
          config->interval = (int)value_number;
        }
      }
    }

    if (strcmp(section, "state") == 0) {
      if (strcmp(key, "path") == 0) {
        snprintf(config->state_path, sizeof(config->state_path), "%s", value);
      }
    }
  }

  fclose(file);

  return 0;
}
