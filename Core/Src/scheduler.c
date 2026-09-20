#include "scheduler.h"

void Scheduler_Run(void)
{
    uint32_t now = HAL_GetTick();
    for (uint32_t i = 0; i < TASK_COUNT; i++) {
        if (now - s_tasks[i].last_run >= s_tasks[i].period_ms) {
            s_tasks[i].last_run = now;
            s_tasks[i].func();
        }
    }
}
