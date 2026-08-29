/********************************** (C) COPYRIGHT *******************************
 * File Name          : shed.h
 * Description        : Minimal cooperative scheduler. Time base is
 *                       milliseconds from TMR2 (see systick.[ch]). Tasks
 *                       are named slots, either one-shot or periodic;
 *                       shed_update() must be called regularly from the
 *                       main loop. Identical to ../../ch32l103/User/shed.[ch].
 *******************************************************************************/
#ifndef APP_SHED_H_
#define APP_SHED_H_

#include <stdint.h>

/* Schedules cb() to run after period_ms milliseconds; if is_repeat is
 * non-zero, cb() is re-armed automatically every period_ms afterwards.
 * Returns 0 on success, -1 if there is no free slot or cb is NULL.        */
int shed_add(const char *name, void (*cb)(void), uint32_t period_ms, uint8_t is_repeat);

/* Cancels the task previously scheduled under 'name'. Returns 0 if found
 * and removed, 1 if no matching task exists.                             */
int shed_remove(const char *name);

/* Must be called periodically (e.g. once per main-loop iteration); runs
 * any due callbacks. The current tick count is passed in by the caller so
 * the scheduler itself has no dependency on any particular time source
 * (see systick.[ch] for this project's millisecond time base).           */
void shed_update(uint32_t now_ms);

#endif /* APP_SHED_H_ */
