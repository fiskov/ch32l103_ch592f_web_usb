/********************************** (C) COPYRIGHT *******************************
 * File Name          : shed.c
 * Description        : Minimal cooperative scheduler (see sched.h).
 *
 *  Used slots are kept compacted at the front of the table ([0, s_used))
 *  so the per-iteration scan only touches live tasks: sched_remove() and
 *  expired one-shots just mark a slot free (cb == NULL), and the freed
 *  slots are compacted away at the start of the next sched_update(),
 *  outside callback context - callbacks may freely remove/re-add tasks
 *  (including themselves) while the scan is running.
 *******************************************************************************/
#include "sched.h"
#include <string.h>

#define SCHED_SIZE        8
#define SCHED_NAME_SIZE   12

typedef void (*sched_callback_t)(void);

typedef struct
{
    char            name[SCHED_NAME_SIZE];
    uint8_t         is_repeat;
    uint32_t        due_ms;      /* absolute ms deadline */
    uint32_t        period_ms;
    sched_callback_t cb;
} sched_task_t;

static sched_task_t s_tasks[SCHED_SIZE];

/* Live tasks occupy [0, s_used); freed slots inside that range are marked
 * cb == NULL until the next sched_update() compacts them away. */
static uint8_t s_used = 0;

/* Timestamp of the most recent sched_update() call; sched_add() arms new
 * tasks relative to it, so the module never reads a timer itself. */
static uint32_t s_now_ms = 0;

int sched_add(const char *name, void (*cb)(void), uint32_t period_ms, uint8_t is_repeat)
{
    sched_task_t *t = NULL;
    uint8_t i;

    if (cb == NULL)
    {
        return -1;
    }

    /* Prefer a slot freed earlier in this pass, then the table tail. */
    for (i = 0; i < s_used; i++)
    {
        if (s_tasks[i].cb == NULL)
        {
            t = &s_tasks[i];
            break;
        }
    }
    if (t == NULL)
    {
        if (s_used >= SCHED_SIZE)
        {
            return -1; /* no free slot */
        }
        t = &s_tasks[s_used++];
    }

    memset(t->name, 0, sizeof(t->name));
    if (name)
    {
        strncpy(t->name, name, SCHED_NAME_SIZE - 1);
    }
    t->period_ms = period_ms;
    t->due_ms    = s_now_ms + period_ms;
    t->is_repeat = is_repeat;
    t->cb        = cb;
    return 0;
}

int sched_remove(const char *name)
{
    uint8_t i;

    if (name == NULL)
    {
        return -1;
    }

    for (i = 0; i < s_used; i++)
    {
        if (s_tasks[i].cb == NULL)
        {
            continue;
        }
        if (strncmp(s_tasks[i].name, name, SCHED_NAME_SIZE - 1) == 0)
        {
            /* Mark-only: the slot is compacted away by the next
             * sched_update(), which also keeps this safe to call from
             * inside a scheduler callback. */
            s_tasks[i].cb = NULL;
            return 0;
        }
    }
    return 1; /* not found */
}

void sched_update(uint32_t now_ms)
{
    uint8_t i;

    s_now_ms = now_ms;

    /* Compact freed slots out of [0, s_used) so the scan below only
     * walks live tasks. The swap-from-tail order is not FIFO, but task
     * order within one update pass carries no meaning here. */
    for (i = 0; i < s_used; )
    {
        if (s_tasks[i].cb != NULL)
        {
            i++;
            continue;
        }
        s_tasks[i] = s_tasks[s_used - 1];
        s_used--;
    }

    for (i = 0; i < s_used; i++)
    {
        sched_task_t *t = &s_tasks[i];

        if (t->cb == NULL)
        {
            continue; /* removed by an earlier callback this pass */
        }
        /* Signed-difference comparison handles uint32_t wraparound safely. */
        if ((int32_t)(now_ms - t->due_ms) < 0)
        {
            continue;
        }

        if (t->is_repeat)
        {
            t->due_ms += t->period_ms;
            t->cb();
            /* t may have been removed/re-added by the callback; either way
             * the slot state is consistent for the next pass. */
        }
        else
        {
            sched_callback_t cb = t->cb;
            t->cb = NULL; /* free before calling: cb may reschedule itself */
            cb();
        }
    }
}
