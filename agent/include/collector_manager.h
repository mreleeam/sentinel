#ifndef COLLECTOR_MANAGER_H
#define COLLECTOR_MANAGER_H

int collectors_init(void);
int collectors_run(const char *state_path, int report);
void collectors_cleanup(void);

#endif
