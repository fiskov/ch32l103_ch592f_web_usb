/********************************** (C) COPYRIGHT *******************************
 * File Name          : shed.c
 * Description        : Minimal cooperative scheduler (see shed.h).
 *
 * A one-shot slot is freed BEFORE its callback runs, so the callback is
 * allowed to reschedule itself under the same name from within itself.
 *******************************************************************************/
#include "shed.h"
#include "systick.h"
#include <string.h>

#define SHED_SIZE        8
#define SHED_NAME_SIZE   12

typedef void (*shed_callback_t)(void);

typedef struct
{
    char            name[SHED_NAME_SIZE];
    uint8_t         is_repeat;
    uint32_t        due_ms;      /* absolute SysTick_Millis() deadline */
    uint32_t        period_ms;
    shed_callback_t cb;
} shed_task_t;

static shed_task_t s_tasks[SHED_SIZE];

int shed_add(const char *name, void (*cb)(void), uint32_t period_ms, uint8_t is_repeat)
{
    if (cb == NULL)
    {
        return -1;
    }

    for (int i = 0; i < SHED_SIZE; i++)
    {
        shed_task_t *t = &s_tasks[i];
        if (t->cb == NULL)
        {
            memset(t->name, 0, sizeof(t->name));
            if (name)
            {
                strncpy(t->name, name, SHED_NAME_SIZE - 1);
            }
            t->period_ms = period_ms;
            t->due_ms    = SysTick_Millis() + period_ms;
            t->is_repeat = is_repeat;
            t->cb        = cb;
            return 0;
        }
    }
    return -1; /* no free slot */
}

int shed_remove(const char *name)
{
    if (name == NULL)
    {
        return -1;
    }

    for (int i = 0; i < SHED_SIZE; i++)
    {
        shed_task_t *t = &s_tasks[i];
        if (t->cb == NULL)
        {
            continue;
        }
        if (strncmp(t->name, name, SHED_NAME_SIZE - 1) == 0)
        {
            t->cb = NULL;
            return 0;
        }
    }
    return 1; /* not found */
}

void shed_update(void)
{
    uint32_t now = SysTick_Millis();

    for (int i = 0; i < SHED_SIZE; i++)
    {
        shed_task_t *t = &s_tasks[i];
        if (t->cb == NULL)
        {
            continue;
        }
        /* Signed-difference comparison handles uint32_t wraparound safely. */
        if ((int32_t)(now - t->due_ms) < 0)
        {
            continue;
        }

        if (t->is_repeat)
        {
            t->due_ms += t->period_ms;
            t->cb();
        }
        else
        {
            shed_callback_t cb = t->cb;
            t->cb = NULL; /* free before calling: cb may reschedule itself */
            cb();
        }
    }
}
