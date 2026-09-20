#ifndef __SCHEDULER_H
#define __SCHEDULER_H
#include "main.h"

typedef struct {
    uint32_t last_run;
    uint32_t period_ms;
    void (*func)(void);
} task_t;

#define TASK_COUNT 6

extern task_t s_tasks[];

void Scheduler_Run(void);

#endif
