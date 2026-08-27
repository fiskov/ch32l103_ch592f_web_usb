/********************************** (C) COPYRIGHT *******************************
 * File Name          : systick.h
 * Description        : Free-running 1ms SysTick counter, used as the time
 *                       base for the cooperative scheduler (shed.[ch]).
 *******************************************************************************/
#ifndef USER_SYSTICK_H_
#define USER_SYSTICK_H_

#include "debug.h"

/* Configures SysTick to interrupt every 1ms and starts the millisecond
 * counter returned by SysTick_Millis(). Must be called once at startup. */
void SysTick_InitMillis(void);

/* Milliseconds elapsed since SysTick_InitMillis() was called. Wraps around
 * approximately every 49.7 days (uint32_t), which is not a concern for a
 * cooperative scheduler using 16/32-bit delta arithmetic.                */
uint32_t SysTick_Millis(void);

#endif /* USER_SYSTICK_H_ */
