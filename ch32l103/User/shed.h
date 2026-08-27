/********************************** (C) COPYRIGHT *******************************
 * File Name          : shed.h
 * Description        : Minimal cooperative scheduler, adapted from
 *                       ch592f/app/shed.c. Time base is milliseconds from
 *                       SysTick (see systick.[ch]). Tasks are named slots,
 *                       either one-shot or periodic; shed_update() must be
 *                       called regularly from the main loop.
 *******************************************************************************/
#ifndef USER_SHED_H_
#define USER_SHED_H_

#include "debug.h"

/* Schedules cb() to run after period_ms milliseconds; if is_repeat is
 * non-zero, cb() is re-armed automatically every period_ms afterwards.
 * Returns 0 on success, -1 if there is no free slot or cb is NULL.        */
int shed_add(const char *name, void (*cb)(void), uint32_t period_ms, uint8_t is_repeat);

/* Cancels the task previously scheduled under 'name'. Returns 0 if found
 * and removed, 1 if no matching task exists.                             */
int shed_remove(const char *name);

/* Must be called periodically (e.g. once per main-loop iteration); runs
 * any due callbacks. */
void shed_update(void);

#endif /* USER_SHED_H_ */
