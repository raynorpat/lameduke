#include "task_man.h"

#include <stddef.h>

static void (*task)(void);
static unsigned taskrate, elapsed;

void TS_ScheduleTask(void (*function)(void), int rate)
{
    task = function;
    elapsed = 0;
    TS_SetTaskRate(rate);
}

void TS_SetTaskRate(int rate)
{
    taskrate = rate > 0 ? (unsigned)rate : 1;
}

void TS_Terminate(void)
{
    task = NULL;
}

/* Run the task as often as its rate fits in one frame at outrate. */
void TS_Advance(unsigned outrate)
{
    if (!task)
        return;
    elapsed += taskrate;
    while (task && elapsed >= outrate) {
        elapsed -= outrate;
        task();
    }
}
